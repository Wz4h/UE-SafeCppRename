#include "Execute/SafeCppRedirectService.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Execute/RenameHistoryStore.h"
#include "Execute/RenameHistoryManager.h"
#include "Execute/RedirectRepository.h"
#include "Execute/RedirectIniWriter.h"

#define LOCTEXT_NAMESPACE "FSafeCppRedirectService"

bool FSafeCppRedirectService::ApplyRedirects(
	const FSafeCppRedirectRequest& Request, FString& OutReport)
{
	OutReport.Reset();

	// 基本检查
	if (Request.SelectedClass == nullptr)
	{
		AppendLine(OutReport, TEXT("Error: SelectedClass is null."));
		return false;
	}

	if (Request.Plan == nullptr)
	{
		AppendLine(OutReport, TEXT("Error: Rename plan is null."));
		return false;
	}

	if (Request.NewClassName.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: NewClassName is empty."));
		return false;
	}

	if (Request.RedirectKey.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: RedirectKey is empty."));
		return false;
	}

	// 构建旧名 / 新名完整路径
	FString OldFullName;
	FString NewFullName;
	FString BuildNameError;

	if (!BuildFullClassNames(Request.SelectedClass, Request.NewClassName, OldFullName, NewFullName, BuildNameError))
	{
		AppendLine(OutReport, TEXT("Build full redirect names failed."));
		AppendLine(OutReport, BuildNameError);
		return false;
	}

	AppendLine(OutReport, FString::Printf(TEXT("OldName: %s"), *OldFullName));
	AppendLine(OutReport, FString::Printf(TEXT("NewName: %s"), *NewFullName));

	// 1. 读取已有 redirect
	TArray<FRedirectEntry> ExistingRedirects;
	if (Request.bWriteToFile && !Request.OutputIniFilePath.IsEmpty())
	{
		FString LoadReport;
		if (!FRedirectIniWriter::LoadRedirectsFromIniFile(
			Request.OutputIniFilePath,
			Request.RedirectKey,
			ExistingRedirects,
			LoadReport))
		{
			AppendLine(OutReport, TEXT("LoadRedirectsFromIniFile failed."));
			AppendLine(OutReport, LoadReport);
			return false;
		}

		AppendLine(OutReport, LoadReport);
	}

	// 2. 读取历史链文件
	TArray<FRenameHistoryChainRecord> ExistingChains;
	if (!Request.HistoryIniFilePath.IsEmpty())
	{
		FString HistoryLoadReport;
		if (!FRenameHistoryStore::LoadChains(
			Request.HistoryIniFilePath,
			ExistingChains,
			HistoryLoadReport))
		{
			AppendLine(OutReport, TEXT("LoadChains failed."));
			AppendLine(OutReport, HistoryLoadReport);
			return false;
		}

		AppendLine(OutReport, HistoryLoadReport);
	}

	// 3. 构建 Rename 历史链管理器
	FRenameHistoryManager HistoryManager;
	HistoryManager.SetChains(ExistingChains);


	// 3. 注册本次 rename
	if (!HistoryManager.RegisterRename(OldFullName, NewFullName))
	{
		AppendLine(OutReport, TEXT("Error: RegisterRename failed for current rename."));
		return false;
	}

	// 4. 生成整条链的终态 Redirect
	TArray<FRedirectEntry> DesiredRedirects;
	if (!HistoryManager.GenerateRedirectsForClass(OldFullName, DesiredRedirects))
	{
		AppendLine(OutReport, TEXT("No redirect generated for current class chain."));
		return true;
	}

	AppendLine(OutReport, FString::Printf(TEXT("GeneratedRedirectCount: %d"), DesiredRedirects.Num()));

	// 5. 合并到仓库（内存层）
	FRedirectRepository Repository;

	// 先导入已有 redirect
	for (const FRedirectEntry& ExistingEntry : ExistingRedirects)
	{
		Repository.AddExistingRedirect(ExistingEntry);
	}

	// 再合并本次生成的终态 redirect
	const int32 ChangedCount = Repository.MergeDesiredRedirects(DesiredRedirects);

	AppendLine(OutReport, FString::Printf(TEXT("RepositoryChangedCount: %d"), ChangedCount));
	AppendLine(OutReport, FString::Printf(TEXT("RepositoryTotalCount: %d"), Repository.Num()));

	// 6. 输出生成文本（方便面板查看）
	const FString RedirectText = FRedirectIniWriter::BuildRedirectText(Request.RedirectKey, Repository.GetAllRedirects(), true);

	AppendLine(OutReport, TEXT("GeneratedIniText:"));
	AppendLine(OutReport, RedirectText.IsEmpty() ? TEXT("<Empty>") : RedirectText);

	// 7. 可选写入文件：扫描 + 合并 + 覆盖回写
	if (Request.bWriteToFile)
	{
		if (Request.OutputIniFilePath.IsEmpty())
		{
			AppendLine(OutReport, TEXT("Error: bWriteToFile is true, but OutputIniFilePath is empty."));
			return false;
		}

		FString MergeReport;
		if (!FRedirectIniWriter::MergeRedirectsIntoIniFile(
			Request.OutputIniFilePath,
			Request.RedirectKey,
			Repository.GetAllRedirects(),
			MergeReport))
		{
			AppendLine(OutReport, TEXT("MergeRedirectsIntoIniFile failed."));
			AppendLine(OutReport, MergeReport);
			return false;
		}

		AppendLine(OutReport, MergeReport);
	}
	
	// 8. 保存历史链到 SafeCppRename.ini
	if (!Request.HistoryIniFilePath.IsEmpty())
	{
		FString HistorySaveReport;
		if (!FRenameHistoryStore::SaveChains(
			Request.HistoryIniFilePath,
			HistoryManager.GetChains(),
			HistorySaveReport))
		{
			AppendLine(OutReport, TEXT("SaveChains failed."));
			AppendLine(OutReport, HistorySaveReport);
			return false;
		}

		AppendLine(OutReport, HistorySaveReport);
	}
	return true;
}
bool FSafeCppRedirectService::BuildFullClassNames(
	const UClass* SelectedClass,
	const FString& NewClassName,
	FString& OutOldName,
	FString& OutNewName,
	FString& OutError)
{
	OutOldName.Reset();
	OutNewName.Reset();
	OutError.Reset();

	if (SelectedClass == nullptr)
	{
		OutError = TEXT("SelectedClass is null.");
		return false;
	}

	if (NewClassName.IsEmpty())
	{
		OutError = TEXT("NewClassName is empty.");
		return false;
	}

	FString ScriptPackagePath;
	if (!GetScriptPackagePath(SelectedClass, ScriptPackagePath, OutError))
	{
		return false;
	}

	// UE 反射名不带 A/U/F 前缀，因此这里直接使用 GetName()
	const FString OldObjectName = SelectedClass->GetName();

	if (OldObjectName.IsEmpty())
	{
		OutError = TEXT("SelectedClass->GetName() returned empty.");
		return false;
	}

	OutOldName = FString::Printf(TEXT("%s.%s"), *ScriptPackagePath, *OldObjectName);
	OutNewName = FString::Printf(TEXT("%s.%s"), *ScriptPackagePath, *NewClassName);

	return true;
}

bool FSafeCppRedirectService::GetScriptPackagePath(
	const UClass* SelectedClass,
	FString& OutScriptPackagePath,
	FString& OutError)
{
	OutScriptPackagePath.Reset();
	OutError.Reset();

	if (SelectedClass == nullptr)
	{
		OutError = TEXT("SelectedClass is null.");
		return false;
	}

	const UPackage* OuterPackage = SelectedClass->GetOutermost();
	if (OuterPackage == nullptr)
	{
		OutError = TEXT("SelectedClass->GetOutermost() returned null.");
		return false;
	}

	const FString PackageName = OuterPackage->GetName();
	if (PackageName.IsEmpty())
	{
		OutError = TEXT("Outer package name is empty.");
		return false;
	}

	// 原生类通常就是 /Script/ModuleName
	OutScriptPackagePath = PackageName;
	return true;
}



void FSafeCppRedirectService::AppendLine(FString& InOutText, const FString& Line)
{
	InOutText += Line;
	InOutText += LINE_TERMINATOR;
}

#undef LOCTEXT_NAMESPACE