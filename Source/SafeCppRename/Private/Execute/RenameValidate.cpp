
#include "Execute/RenameValidate.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// 你 Plan 里已经有定位函数的话，也可以直接复用。
// 这里为了独立性，Validate 只做最小检查：ModuleRelativePath 是否存在并且文件存在。
static bool TryGetHeaderAbsPathFromUHT(UClass* TargetClass, FString& OutHeaderAbs, FString& OutError)
{
	OutHeaderAbs.Reset();
	OutError.Reset();

	if (!IsValid(TargetClass))
	{
		OutError = TEXT("TargetClass is invalid.");
		return false;
	}

	// UHT 写入的相对路径：Public/.../MyCharacter.h
	const FString ModuleRelativePath = TargetClass->GetMetaData(TEXT("ModuleRelativePath"));
	if (ModuleRelativePath.IsEmpty())
	{
		OutError = TEXT("ModuleRelativePath is empty. This class may not be a UHT type (UCLASS/USTRUCT/UENUM).");
		return false;
	}

	// 从类路径推断模块名：/Script/Module.Class
	FString ModuleName;
	{
		const FString Path = TargetClass->GetPathName(); // 你工程里是 /Script/Mod.Class
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

	// 先按 Project/Source/<ModuleName> 拼（v1够用；插件类后面再扩展）
	const FString ModuleRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source") / ModuleName);
	if (!IFileManager::Get().DirectoryExists(*ModuleRoot))
	{
		OutError = FString::Printf(TEXT("Module folder not found: %s"), *ModuleRoot);
		return false;
	}

	const FString HeaderCandidate = FPaths::Combine(ModuleRoot, ModuleRelativePath);
	if (!IFileManager::Get().FileExists(*HeaderCandidate))
	{
		OutError = FString::Printf(TEXT("Header not found: %s"), *HeaderCandidate);
		return false;
	}

	OutHeaderAbs = HeaderCandidate;
	return true;
}

FRenameValidateResult FSafeCppRenameValidator::Validate(UClass* TargetClass, const FString& NewClassName)
{
	FRenameValidateResult R;

	// 1) TargetClass 校验
	if (!ValidateTargetClass(TargetClass, R.Error))
	{
		R.bOk = false;
		return R;
	}

	// 2) NewName 基础合法性
	if (!ValidateNewNameBasic(TargetClass, NewClassName, R.Error))
	{
		R.bOk = false;
		return R;
	}

	// 3) 名称冲突校验
	if (!ValidateNoNameCollision(TargetClass, NewClassName, R.Error))
	{
		R.bOk = false;
		return R;
	}

	// 4) 源文件存在性校验（至少要找到头文件）
	if (!ValidateSourceFilesExist(TargetClass, R.Error))
	{
		R.bOk = false;
		return R;
	}

	// 警告（不阻断）
	CollectWarnings(TargetClass, NewClassName, R);

	R.bOk = true;
	return R;
}

bool FSafeCppRenameValidator::ValidateTargetClass(UClass* TargetClass, FString& OutError)
{
	// 中文注释：必须是有效类
	if (!IsValid(TargetClass))
	{
		OutError = TEXT("No target class selected.");
		return false;
	}

	// 中文注释：必须是 Native 类（C++ 类）。BlueprintGeneratedClass 不允许。
	if (!TargetClass->HasAnyClassFlags(CLASS_Native))
	{
		OutError = TEXT("TargetClass is not native (C++). Blueprint-generated classes are not supported.");
		return false;
	}

	// 中文注释：如果是骨架类/编译中类，可能不稳定（这里先简单挡一下）
	if (TargetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		OutError = TEXT("TargetClass appears to be compiled from Blueprint. Only native C++ classes are supported.");
		return false;
	}

	return true;
}



bool FSafeCppRenameValidator::ValidateNewNameBasic(UClass* TargetClass, const FString& NewClassName, FString& OutError)
{
	// 中文注释：先做输入卫生处理（去掉首尾空白），不做任何前缀处理
	const FString Name = NewClassName.TrimStartAndEnd();

	// 中文注释：不能为空
	if (Name.IsEmpty())
	{
		OutError = TEXT("New class name is empty.");
		return false;
	}

	// 中文注释：必须是合法 C++ 标识符：字母/数字/下划线，且不能以数字开头
	auto IsValidIdent = [](const FString& S) -> bool
	{
		if (S.IsEmpty())
		{
			return false;
		}
		if (!FChar::IsAlpha(S[0]) && S[0] != TEXT('_'))
		{
			return false;
		}
		for (TCHAR C : S)
		{
			if (!FChar::IsAlnum(C) && C != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	};

	if (!IsValidIdent(Name))
	{
		OutError = TEXT("New class name is not a valid C++ identifier. Use letters/digits/underscore, and do not start with a digit.");
		return false;
	}

	// 中文注释：禁止跟旧名一致（旧名是 ScriptName）
	if (IsValid(TargetClass) && Name == TargetClass->GetName())
	{
		OutError = TEXT("New class name is the same as the old class name.");
		return false;
	}

	return true;
} 

bool FSafeCppRenameValidator::ValidateNoNameCollision(UClass* TargetClass, const FString& NewClassName, FString& OutError)
{
	// 中文注释：最直接的冲突：UClass 同名是否已存在（同一个包/不同包都算风险）
	// FindObject 的 Outer 设为 ANY_PACKAGE 可能会找出任何地方的同名类。
	if (FindObject<UClass>(ANY_PACKAGE, *NewClassName) != nullptr)
	{
		OutError = TEXT("A class with the new name already exists in the current editor session.");
		return false;
	}

	return true;
}

bool FSafeCppRenameValidator::ValidateSourceFilesExist(UClass* TargetClass, FString& OutError)
{
	FString HeaderAbs;
	if (!TryGetHeaderAbsPathFromUHT(TargetClass, HeaderAbs, OutError))
	{
		return false;
	}

	// 中文注释：cpp 可选，这里不强制；真正 Execute 再按 Plan 推导/搜索
	return true;
}

void FSafeCppRenameValidator::CollectWarnings(UClass* TargetClass, const FString& NewClassName, FRenameValidateResult& InOutResult)
{
	// 中文注释：如果项目里已有蓝图继承该类，强烈建议写 CoreRedirects（你 Analyze 已经能扫到子蓝图）
	// 这里 Validate 不访问 AssetRegistry（避免卡），只给一个通用提醒。
	InOutResult.Warnings.Add(TEXT("If there are Blueprint children, Core Redirects (ClassRedirects) are strongly recommended."));

	// 中文注释：提醒用户命名规范（可选）
	InOutResult.Warnings.Add(TEXT("After Execute, close the Editor and rebuild the project (UHT + C++)."));
}