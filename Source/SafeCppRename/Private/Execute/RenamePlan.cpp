#include "Execute/RenamePlan.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"

// ---------------- FRenamePlan ----------------

FString FRenamePlan::ToReportString(bool bVerbose) const
{
	FString Out;

	if (!bOk)
	{
		Out += TEXT("[Plan] FAILED\n");
		Out += TEXT("Reason: ") + Error + TEXT("\n");
		return Out;
	}

	// =========================
	// Summary
	// =========================
	Out += TEXT("[Plan] OK\n\n");
	Out += TEXT("Summary\n");
	Out += TEXT(" - Old name (Script): ") + OldClassName + TEXT("\n");
	Out += TEXT(" - New name (Script): ") + NewClassName + TEXT("\n");
	if (!ModuleName.IsEmpty())
	{
		Out += TEXT(" - Module: ") + ModuleName + TEXT("\n");
	}
	Out += TEXT("\n");

	// =========================
	// Files
	// =========================
	Out += TEXT("Files\n");
	Out += FString::Printf(TEXT(" - Count: %d\n"), Files.Num());

	for (const FRenameFilePlan& F : Files)
	{
		const FString FileName = FPaths::GetCleanFilename(F.FilePath);
		Out += TEXT(" - ") + FileName + TEXT("\n");
		Out += TEXT("   Path: ") + F.FilePath + TEXT("\n");

		if (F.bRenameFile)
		{
			Out += TEXT("   Rename to: ") + FPaths::GetCleanFilename(F.NewFilePath) + TEXT("\n");
		}

		Out += FString::Printf(TEXT("   Replacements: %d\n"), F.Replaces.Num());

		// 详细模式才展开
		if (bVerbose && F.Replaces.Num() > 0)
		{
			for (int32 i = 0; i < F.Replaces.Num(); ++i)
			{
				const FRenameTextReplaceOp& Op = F.Replaces[i];
				Out += FString::Printf(TEXT("    [%d] %s\n"), i + 1, *Op.Desc);
				Out += TEXT("         Find: ") + Op.Find + TEXT("\n");
				Out += TEXT("         Repl: ") + Op.Replace + TEXT("\n");
			}
		}
	}

	Out += TEXT("\n");

	// =========================
	// Redirect
	// =========================
	Out += TEXT("Core Redirect (recommended)\n");
	Out += TEXT(" - Old: ") + Redirect.OldClassPath + TEXT("\n");
	Out += TEXT(" - New: ") + Redirect.NewClassPath + TEXT("\n");
	Out += TEXT(" - Ini line: ") + Redirect.IniLine + TEXT("\n");
	Out += TEXT("\n");


	return Out;
}

// ---------------- helpers (local) ----------------

/**
 * 中文注释：Normalize 用户输入的新类名为 ScriptName（不带前缀）
 * - 规则：UI 理论上不带前缀
 * - 兼容：用户手滑输入了 AMyNewCharacter / UMyNewObject，则自动剥离
 */


/**
 * 中文注释：由 ScriptName 生成对应的 CppName（带前缀）
 */
static FString MakeCppName(UClass* TargetClass, const FString& ScriptName)
{
	if (!IsValid(TargetClass))
	{
		return ScriptName;
	}
	return TargetClass->GetPrefixCPP() + ScriptName;
}

// ---------------- FSafeCppRenamePlanBuilder ----------------

FRenamePlan FSafeCppRenamePlanBuilder::BuildPlan(UClass* TargetClass, const FString& NewClassName, bool bRenameFiles)
{
	FRenamePlan Plan;

	// ---------- 基本校验（v1：先做最必要的） ----------
	if (!IsValid(TargetClass))
	{
		Plan.bOk = false;
		Plan.Error = TEXT("TargetClass is invalid.");
		return Plan;
	}

	// 中文注释：UE 反射名（ScriptName）通常不带 A/U 前缀
	const FString OldScriptName = TargetClass->GetName(); // e.g. "MyCharacter"
	if (OldScriptName.IsEmpty())
	{
		Plan.bOk = false;
		Plan.Error = TEXT("Old class name (script) is empty.");
		return Plan;
	}

	// 中文注释：用户输入理论上是 ScriptName（不带前缀）
	const FString NewScriptName = NewClassName.TrimStartAndEnd();
	if (NewScriptName.IsEmpty())
	{
		Plan.bOk = false;
		Plan.Error = TEXT("New class name (script) is empty.");
		return Plan;
	}

	// 中文注释：内部 C++ 名（带前缀），用于 scope/ctor/class 声明等替换
	const FString OldCppName = MakeCppName(TargetClass, OldScriptName);   // e.g. "AMyCharacter"
	const FString NewCppName = MakeCppName(TargetClass, NewScriptName);   // e.g. "AMyNewCharacter"

	// Plan 展示保持 ScriptName（符合 UE/UI 习惯）
	Plan.OldClassName = OldScriptName;
	Plan.NewClassName = NewScriptName;

	// 目标模块名（粗略：用包名推断，v1足够展示）
	// 例：/Script/SafeCppRenameTest.MyCharacter -> SafeCppRenameTest
	const FString OldPathName = TargetClass->GetPathName();
	{
		// 从 "/Script/Module.Class" 抽 Module
		FString Left, Right;
		if (OldPathName.Split(TEXT("."), &Left, &Right))
		{
			Left = Left.Replace(TEXT("/Script/"), TEXT(""));
			Plan.ModuleName = Left;
		}
	}

	// ---------- 定位 .h/.cpp ----------
	FString HeaderAbs, CppAbs, Err;
	if (!LocateClassHeaderAndCpp(TargetClass, HeaderAbs, CppAbs, Err))
	{
		Plan.bOk = false;
		Plan.Error = TEXT("Failed to locate .h/.cpp files: ") + Err;
		return Plan;
	}

	// ---------- 生成头文件计划 ----------
	{
		FRenameFilePlan FP;
		FP.FilePath = HeaderAbs;

		BuildReplacesForHeader(OldScriptName, NewScriptName, OldCppName, NewCppName, FP.Replaces);

		if (bRenameFiles)
		{
			FP.bRenameFile = true;
			// 中文注释：文件名通常是 ScriptName（不带前缀）
			FP.NewFilePath = MakeRenamedFilePath(HeaderAbs, OldScriptName, NewScriptName);
		}

		Plan.Files.Add(MoveTemp(FP));
	}

	// ---------- 生成 cpp 文件计划 ----------
	if (!CppAbs.IsEmpty())
	{
		FRenameFilePlan FP;
		FP.FilePath = CppAbs;

		BuildReplacesForCpp(OldScriptName, NewScriptName, OldCppName, NewCppName, FP.Replaces);

		if (bRenameFiles)
		{
			FP.bRenameFile = true;
			// 中文注释：文件名通常是 ScriptName（不带前缀）
			FP.NewFilePath = MakeRenamedFilePath(CppAbs, OldScriptName, NewScriptName);
		}

		Plan.Files.Add(MoveTemp(FP));
	}

	// ---------- 生成 Redirect 计划 ----------
	Plan.Redirect = BuildRedirectPlan(TargetClass, NewScriptName);

	Plan.bOk = true;
	return Plan;
}

bool FSafeCppRenamePlanBuilder::LocateClassHeaderAndCpp(
	UClass* TargetClass,
	FString& OutHeaderAbsPath,
	FString& OutCppAbsPath,
	FString& OutError
)
{
	OutHeaderAbsPath.Reset();
	OutCppAbsPath.Reset();
	OutError.Reset();

	if (!IsValid(TargetClass))
	{
		OutError = TEXT("TargetClass invalid.");
		return false;
	}

	// =========================
	// 1) 优先用 UHT 写入的元数据定位头文件
	// =========================
	const FString ModuleRelativePath = TargetClass->GetMetaData(TEXT("ModuleRelativePath"));
	if (ModuleRelativePath.IsEmpty())
	{
		OutError = TEXT("No UHT metadata: ModuleRelativePath is empty. (Maybe not a UCLASS/UHT type?)");
		return false;
	}

	// 从类路径推断模块名：/Script/Module.Class
	FString ModuleName;
	{
		const FString Path = TargetClass->GetPathName(); // e.g. "/Script/SafeCppRenameTest.MyCharacter"
		FString Left, Right;
		if (Path.Split(TEXT("."), &Left, &Right))
		{
			ModuleName = Left.Replace(TEXT("/Script/"), TEXT(""));
		}
	}

	if (ModuleName.IsEmpty())
	{
		OutError = TEXT("Failed to infer module name from class path.");
		return false;
	}

	// 工程模块目录：<Project>/Source/<ModuleName>/
	const FString ModuleRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source") / ModuleName);
	if (!IFileManager::Get().DirectoryExists(*ModuleRoot))
	{
		OutError = FString::Printf(TEXT("Module folder not found: %s"), *ModuleRoot);
		return false;
	}

	TArray<FString> HeaderCandidates;
	HeaderCandidates.Add(FPaths::Combine(ModuleRoot, ModuleRelativePath));

	const FString IncludePath = TargetClass->GetMetaData(TEXT("IncludePath")); // e.g. "Characters/MyCharacter.h"
	if (!IncludePath.IsEmpty())
	{
		HeaderCandidates.Add(FPaths::Combine(ModuleRoot, TEXT("Public"), IncludePath));
		HeaderCandidates.Add(FPaths::Combine(ModuleRoot, TEXT("Private"), IncludePath));
	}

	for (const FString& H : HeaderCandidates)
	{
		if (IFileManager::Get().FileExists(*H))
		{
			OutHeaderAbsPath = H;
			break;
		}
	}

	if (OutHeaderAbsPath.IsEmpty())
	{
		const FString HeaderFileName = FPaths::GetCleanFilename(ModuleRelativePath);
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *ModuleRoot, *HeaderFileName, true, false);
		if (Found.Num() > 0)
		{
			OutHeaderAbsPath = Found[0];
		}
	}

	if (OutHeaderAbsPath.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Header not found. ModuleRelativePath=%s"), *ModuleRelativePath);
		return false;
	}

	// =========================
	// 2) 推导 cpp：同目录/Private 目录/递归搜同名 cpp
	// =========================
	const FString HeaderBase = FPaths::GetBaseFilename(OutHeaderAbsPath);
	const FString HeaderDir = FPaths::GetPath(OutHeaderAbsPath);

	const FString SameDirCpp = FPaths::Combine(HeaderDir, HeaderBase + TEXT(".cpp"));
	if (IFileManager::Get().FileExists(*SameDirCpp))
	{
		OutCppAbsPath = SameDirCpp;
		return true;
	}

	{
		FString RelToPublic;
		if (OutHeaderAbsPath.Split(FPaths::Combine(ModuleRoot, TEXT("Public")) + TEXT("/"), nullptr, &RelToPublic))
		{
			const FString PrivateCpp = FPaths::Combine(ModuleRoot, TEXT("Private"), FPaths::GetPath(RelToPublic), HeaderBase + TEXT(".cpp"));
			if (IFileManager::Get().FileExists(*PrivateCpp))
			{
				OutCppAbsPath = PrivateCpp;
				return true;
			}
		}
	}

	{
		TArray<FString> FoundCpp;
		IFileManager::Get().FindFilesRecursive(FoundCpp, *ModuleRoot, *(HeaderBase + TEXT(".cpp")), true, false);
		if (FoundCpp.Num() > 0)
		{
			OutCppAbsPath = FoundCpp[0];
		}
	}

	// cpp 找不到不算失败（v1）
	return true;
}

bool FSafeCppRenamePlanBuilder::LoadTextFile(const FString& AbsPath, FString& OutText)
{
	return FFileHelper::LoadFileToString(OutText, *AbsPath);
}

void FSafeCppRenamePlanBuilder::BuildReplacesForHeader(
	const FString& OldScriptName,
	const FString& NewScriptName,
	const FString& OldCppName,
	const FString& NewCppName,
	TArray<FRenameTextReplaceOp>& OutOps
)
{
	OutOps.Reset();

	// 1) generated.h include：使用 ScriptName（不带前缀）
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Update generated.h include");
		Op.Find = FString::Printf(TEXT("\"%s.generated.h\""), *OldScriptName);
		Op.Replace = FString::Printf(TEXT("\"%s.generated.h\""), *NewScriptName);
		OutOps.Add(MoveTemp(Op));
	}

	// 2) C++ 类名替换：使用 CppName（带前缀）
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Replace C++ class name (in header)");
		Op.Find = OldCppName;
		Op.Replace = NewCppName;
		OutOps.Add(MoveTemp(Op));
	}

	// 3) 构造函数声明：AMyCharacter( -> AMyNewCharacter(
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Replace constructor declaration");
		Op.Find = FString::Printf(TEXT("%s("), *OldCppName);
		Op.Replace = FString::Printf(TEXT("%s("), *NewCppName);
		OutOps.Add(MoveTemp(Op));
	}

	// 4) 兜底：若某些地方用到了 ScriptName（极少），可选替换（v1保持保守：不做）
	// 中文注释：为了避免误伤（例如注释/字符串/路径），v1 不在 header 做 ScriptName 全局替换。
}

void FSafeCppRenamePlanBuilder::BuildReplacesForCpp(
	const FString& OldScriptName,
	const FString& NewScriptName,
	const FString& OldCppName,
	const FString& NewCppName,
	TArray<FRenameTextReplaceOp>& OutOps
)
{
	OutOps.Reset();

	// 1) include 自身头：#include "MyCharacter.h" -> "MyNewCharacter.h"（ScriptName）
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Update cpp include header");
		Op.Find = FString::Printf(TEXT("\"%s.h\""), *OldScriptName);
		Op.Replace = FString::Printf(TEXT("\"%s.h\""), *NewScriptName);
		OutOps.Add(MoveTemp(Op));
	}

	// 2) 作用域替换：AMyCharacter:: -> AMyNewCharacter::（CppName）
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Replace scope (Old:: -> New::)");
		Op.Find = FString::Printf(TEXT("%s::"), *OldCppName);
		Op.Replace = FString::Printf(TEXT("%s::"), *NewCppName);
		OutOps.Add(MoveTemp(Op));
	}

	// 3) 类名裸引用（cpp 内部）：AMyCharacter -> AMyNewCharacter（CppName）
	{
		FRenameTextReplaceOp Op;
		Op.Desc = TEXT("Replace C++ class name inside target cpp");
		Op.Find = OldCppName;
		Op.Replace = NewCppName;
		OutOps.Add(MoveTemp(Op));
	}
}

FRenameRedirectPlan FSafeCppRenamePlanBuilder::BuildRedirectPlan(UClass* TargetClass, const FString& NewScriptName)
{
	FRenameRedirectPlan RP;

	// 中文注释：旧类路径（反射路径）
	FString OldPath = TargetClass->GetPathName();
	OldPath.ReplaceInline(TEXT("Class'"), TEXT(""));
	OldPath.ReplaceInline(TEXT("'"), TEXT(""));
	OldPath.ReplaceInline(TEXT("/Script/CoreUObject.Class"), TEXT(""));

	// 中文注释：新类路径：/Script/Mod.NewScriptName（不带前缀）
	FString Prefix, Suffix;
	if (OldPath.Split(TEXT("."), &Prefix, &Suffix))
	{
		RP.OldClassPath = OldPath;
		RP.NewClassPath = Prefix + TEXT(".") + NewScriptName;
	}
	else
	{
		RP.OldClassPath = OldPath;
		RP.NewClassPath = NewScriptName;
	}

	RP.IniLine = FString::Printf(TEXT("+ClassRedirects=(OldName=\"%s\",NewName=\"%s\")"), *RP.OldClassPath, *RP.NewClassPath);
	return RP;
}

FString FSafeCppRenamePlanBuilder::MakeRenamedFilePath(
	const FString& OldAbsPath,
	const FString& OldToken,
	const FString& NewToken
)
{
	const FString Dir = FPaths::GetPath(OldAbsPath);
	const FString File = FPaths::GetCleanFilename(OldAbsPath);

	// 中文注释：只改文件名中出现的 Token（v1：按大小写敏感）
	FString NewFile = File;
	NewFile.ReplaceInline(*OldToken, *NewToken, ESearchCase::CaseSensitive);

	return FPaths::Combine(Dir, NewFile);
}