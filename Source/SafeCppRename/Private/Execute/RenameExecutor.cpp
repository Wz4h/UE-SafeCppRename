#include "Execute/RenameExecutor.h"

#include "Execute/RenamePlan.h" // FRenamePlan / FRenameFilePlan / FRenameTextReplaceOp
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

static const TCHAR* GSafeCppRenameBackupRoot = TEXT("SafeCppRename/Backup");

bool FSafeCppRenameExecutor::ExecutePlan(const FRenamePlan& Plan, bool bDryRun, bool bStrict, FString& OutReport)
{
	OutReport.Reset();

	if (!Plan.bOk)
	{
		OutReport += TEXT("[Execute] FAILED\n");
		OutReport += TEXT("Reason: Plan is not OK.\n");
		if (!Plan.Error.IsEmpty())
		{
			OutReport += TEXT("PlanError: ") + Plan.Error + TEXT("\n");
		}
		return false;
	}

	OutReport += TEXT("[Execute] START\n");
	OutReport += FString::Printf(TEXT("DryRun: %s\n"), bDryRun ? TEXT("true") : TEXT("false"));
	OutReport += FString::Printf(TEXT("Strict: %s\n\n"), bStrict ? TEXT("true") : TEXT("false"));

	// 1) 创建备份目录（DryRun 下也创建/不创建都行；这里 DryRun 不创建）
	FString BackupDirAbs;
	FString Err;

	if (!bDryRun)
	{
		if (!CreateBackupDir(BackupDirAbs, Err))
		{
			OutReport += TEXT("[Execute] FAILED\n");
			OutReport += TEXT("Reason: Failed to create backup directory.\n");
			OutReport += TEXT("Error: ") + Err + TEXT("\n");
			return false;
		}

		// 2) 备份涉及文件
		if (!BackupFiles(Plan, BackupDirAbs, Err))
		{
			OutReport += TEXT("[Execute] FAILED\n");
			OutReport += TEXT("Reason: Failed to backup files.\n");
			OutReport += TEXT("Error: ") + Err + TEXT("\n");
			OutReport += TEXT("BackupDir: ") + BackupDirAbs + TEXT("\n");
			return false;
		}

		OutReport += TEXT("[Backup] OK\n");
		OutReport += TEXT("BackupDir: ") + BackupDirAbs + TEXT("\n\n");
	}
	else
	{
		OutReport += TEXT("[Backup] SKIPPED (DryRun)\n\n");
	}

	// 3) 对每个文件应用替换（写回时原子写入）
	int32 TotalFiles = 0;
	int32 TotalOps = 0;

	for (const FRenameFilePlan& FilePlan : Plan.Files)
	{
		++TotalFiles;

		OutReport += TEXT("------------------------------------------------------------\n");
		OutReport += TEXT("[File]\n");
		OutReport += TEXT("Path: ") + FilePlan.FilePath + TEXT("\n");
		OutReport += FString::Printf(TEXT("Ops: %d\n"), FilePlan.Replaces.Num());

		TArray<int32> HitCounts;
		HitCounts.Reserve(FilePlan.Replaces.Num());

		if (!ApplyReplacements(FilePlan.FilePath, FilePlan.Replaces, bStrict, bDryRun, HitCounts, Err))
		{
			OutReport += TEXT("[File] FAILED\n");
			OutReport += TEXT("Error: ") + Err + TEXT("\n");
			OutReport += TEXT("Tip: You can restore from backup if not DryRun.\n");

			// 中文注释：v1 不做自动回滚（因为我们已经备份）；失败即终止，避免部分成功
			OutReport += TEXT("\n[Execute] FAILED\n");
			return false;
		}

		// 输出命中情况
		for (int32 i = 0; i < FilePlan.Replaces.Num(); ++i)
		{
			++TotalOps;
			const FRenameTextReplaceOp& Op = FilePlan.Replaces[i];
			const int32 Hits = HitCounts.IsValidIndex(i) ? HitCounts[i] : 0;

			OutReport += FString::Printf(TEXT("  - Op[%d] Hits=%d Desc=%s\n"), i + 1, Hits, *Op.Desc);
		}

		OutReport += TEXT("[File] OK\n");

		// 4) 文件重命名（在内容写完后）
		if (FilePlan.bRenameFile)
		{
			OutReport += TEXT("[Rename]\n");
			OutReport += TEXT("From: ") + FilePlan.FilePath + TEXT("\n");
			OutReport += TEXT("To:   ") + FilePlan.NewFilePath + TEXT("\n");

			if (!bDryRun)
			{
				if (!RenameFile(FilePlan.FilePath, FilePlan.NewFilePath, Err))
				{
					OutReport += TEXT("[Rename] FAILED\n");
					OutReport += TEXT("Error: ") + Err + TEXT("\n");
					OutReport += TEXT("\n[Execute] FAILED\n");
					return false;
				}
				OutReport += TEXT("[Rename] OK\n");
			}
			else
			{
				OutReport += TEXT("[Rename] SKIPPED (DryRun)\n");
			}
		}

		OutReport += TEXT("\n");
	}

	OutReport += TEXT("------------------------------------------------------------\n");
	OutReport += TEXT("[Execute] OK\n");
	OutReport += FString::Printf(TEXT("FilesProcessed: %d\n"), TotalFiles);
	OutReport += FString::Printf(TEXT("OpsProcessed: %d\n"), TotalOps);
	OutReport += TEXT("\nNext steps:\n");
	OutReport += TEXT(" - Close the Editor\n");
	OutReport += TEXT(" - Rebuild the project (UHT + C++)\n");
	OutReport += TEXT(" - Reopen the Editor\n");
	OutReport += TEXT(" - If you added CoreRedirects, restart is required\n");

	return true;
}

bool FSafeCppRenameExecutor::CreateBackupDir(FString& OutBackupDirAbs, FString& OutError)
{
	OutBackupDirAbs.Reset();
	OutError.Reset();

	// 中文注释：备份根目录放在 Project/Saved 下
	const FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	const FString BackupRoot = FPaths::Combine(SavedDir, GSafeCppRenameBackupRoot);

	// 时间戳目录名
	const FDateTime Now = FDateTime::Now();
	const FString Stamp = Now.ToString(TEXT("%Y-%m-%d_%H-%M-%S"));
	OutBackupDirAbs = FPaths::Combine(BackupRoot, Stamp);

	IFileManager& FM = IFileManager::Get();
	if (!FM.MakeDirectory(*OutBackupDirAbs, /*Tree*/true))
	{
		OutError = FString::Printf(TEXT("MakeDirectory failed: %s"), *OutBackupDirAbs);
		return false;
	}

	return true;
}

bool FSafeCppRenameExecutor::BackupFiles(const FRenamePlan& Plan, const FString& BackupDirAbs, FString& OutError)
{
	OutError.Reset();

	IFileManager& FM = IFileManager::Get();

	for (const FRenameFilePlan& FilePlan : Plan.Files)
	{
		const FString Src = FilePlan.FilePath;
		if (!FM.FileExists(*Src))
		{
			OutError = FString::Printf(TEXT("Source file not found: %s"), *Src);
			return false;
		}

		// 中文注释：备份文件名用原文件名即可（同一个 Plan 内不会重复）
		const FString FileName = FPaths::GetCleanFilename(Src);
		const FString Dst = FPaths::Combine(BackupDirAbs, FileName);

		// Copy：返回值是 COPY_OK / COPY_Fail 等枚举，简单判断是否成功
		const uint32  CopyResult = FM.Copy(*Dst, *Src, /*bReplace*/true, /*bEvenIfReadOnly*/true);
		if (CopyResult != COPY_OK)
		{
			OutError = FString::Printf(TEXT("Copy failed. From=%s To=%s"), *Src, *Dst);
			return false;
		}
	}

	return true;
}

bool FSafeCppRenameExecutor::LoadTextFile(const FString& AbsPath, FString& OutText, FString& OutError)
{
	OutText.Reset();
	OutError.Reset();

	if (!FFileHelper::LoadFileToString(OutText, *AbsPath))
	{
		OutError = FString::Printf(TEXT("LoadFileToString failed: %s"), *AbsPath);
		return false;
	}
	return true;
}

FString FSafeCppRenameExecutor::MakeTempPath(const FString& TargetAbsPath)
{
	// 中文注释：同目录创建临时文件，方便 Move 原子替换（同分区）
	return TargetAbsPath + TEXT(".tmp");
}

bool FSafeCppRenameExecutor::AtomicWriteTextFile(const FString& TargetAbsPath, const FString& NewText, FString& OutError)
{
	OutError.Reset();

	IFileManager& FM = IFileManager::Get();

	const FString TempPath = MakeTempPath(TargetAbsPath);

	// 1) 写临时文件
	if (!FFileHelper::SaveStringToFile(NewText, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("SaveStringToFile failed: %s"), *TempPath);
		// 清理
		FM.Delete(*TempPath, /*RequireExists*/false, /*EvenReadOnly*/true);
		return false;
	}

	// 2) 用 Move 覆盖目标文件（尽量原子）
	// bReplace=true：目标存在则替换
	if (!FM.Move(*TargetAbsPath, *TempPath, /*bReplace*/true, /*bEvenIfReadOnly*/true, /*bAttributes*/true, /*bDoNotRetryOrError*/false))
	{
		OutError = FString::Printf(TEXT("Move failed. From=%s To=%s"), *TempPath, *TargetAbsPath);
		// 清理：Move 失败时临时文件可能还在
		FM.Delete(*TempPath, /*RequireExists*/false, /*EvenReadOnly*/true);
		return false;
	}

	return true;
}

bool FSafeCppRenameExecutor::ApplyReplacements(
	const FString& AbsPath,
	const TArray<FRenameTextReplaceOp>& Ops,
	bool bStrict,
	bool bDryRun,
	TArray<int32>& OutHitCounts,
	FString& OutError
)
{
	OutHitCounts.Reset();
	OutError.Reset();

	FString Text;
	if (!LoadTextFile(AbsPath, Text, OutError))
	{
		return false;
	}

	// 中文注释：依次 Replace，并记录命中次数
	for (const FRenameTextReplaceOp& Op : Ops)
	{
		// FString::ReplaceInline 返回替换次数
		const int32 Hits = Text.ReplaceInline(*Op.Find, *Op.Replace, ESearchCase::CaseSensitive);
		OutHitCounts.Add(Hits);

		const bool bNeedHit = bStrict && Op.Replace.IsEmpty();
		if (bNeedHit && Hits == 0)
		{
			OutError = FString::Printf(TEXT("Replacement hits=0 (strict). Desc=%s Find=%s File=%s"),
				*Op.Desc, *Op.Find, *AbsPath);
			return false;
		}
	}

	if (bDryRun)
	{
		return true;
	}

	// 原子写回
	if (!AtomicWriteTextFile(AbsPath, Text, OutError))
	{
		return false;
	}

	return true;
}

bool FSafeCppRenameExecutor::RenameFile(const FString& FromAbsPath, const FString& ToAbsPath, FString& OutError)
{
	OutError.Reset();

	IFileManager& FM = IFileManager::Get();

	if (FromAbsPath == ToAbsPath)
	{
		return true;
	}

	if (!FM.FileExists(*FromAbsPath))
	{
		OutError = FString::Printf(TEXT("Rename source not found: %s"), *FromAbsPath);
		return false;
	}

	// 确保目标目录存在
	const FString ToDir = FPaths::GetPath(ToAbsPath);
	if (!ToDir.IsEmpty())
	{
		FM.MakeDirectory(*ToDir, /*Tree*/true);
	}

	// Move 实现重命名
	if (!FM.Move(*ToAbsPath, *FromAbsPath, /*bReplace*/true, /*bEvenIfReadOnly*/true, /*bAttributes*/true, /*bDoNotRetryOrError*/false))
	{
		OutError = FString::Printf(TEXT("Rename (Move) failed. From=%s To=%s"), *FromAbsPath, *ToAbsPath);
		return false;
	}

	return true;
}