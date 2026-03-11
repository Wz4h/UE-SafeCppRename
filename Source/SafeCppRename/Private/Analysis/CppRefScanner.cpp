#include "Analysis/CppRefScanner.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

bool FSafeCppRenameCppRefScanner::ScanSourceReferences(
	UClass* TargetClass,
	TArray<FSafeCppRenameCppRefHit>& OutHits,
	FString& OutError,
	int32 MaxHits
)
{
	OutHits.Reset();
	OutError.Reset();

	if (!IsValid(TargetClass))
	{
		OutError = TEXT("TargetClass 无效（空指针或已销毁）");
		return false;
	}

	const FString Needle = TargetClass->GetName(); // 例如 AMyCharacter
	if (Needle.IsEmpty())
	{
		OutError = TEXT("TargetClass->GetName() 为空");
		return false;
	}

	// 扫描工程 Source 目录
	const FString SourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Source"));
	if (!IFileManager::Get().DirectoryExists(*SourceDir))
	{
		OutError = FString::Printf(TEXT("未找到 Source 目录：%s"), *SourceDir);
		return false;
	}

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *SourceDir, TEXT("*.*"), true, false);

	for (const FString& FilePath : Files)
	{
		if (!ShouldScanFile(FilePath))
		{
			continue;
		}

		ScanOneFile(FilePath, Needle, OutHits, MaxHits);

		if (OutHits.Num() >= MaxHits)
		{
			break;
		}
	}

	return true;
}

bool FSafeCppRenameCppRefScanner::ShouldScanFile(const FString& FilePath)
{
	const FString Ext = FPaths::GetExtension(FilePath).ToLower();
	return (Ext == TEXT("h") || Ext == TEXT("hpp") || Ext == TEXT("cpp") || Ext == TEXT("cc") || Ext == TEXT("inl"));
}

void FSafeCppRenameCppRefScanner::ScanOneFile(
	const FString& FilePath,
	const FString& Needle,
	TArray<FSafeCppRenameCppRefHit>& InOutHits,
	int32 MaxHits
)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		return; // 读不到就跳过
	}

	// 分行扫描（保留行号）
	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines, false);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FString& Line = Lines[i];
		if (Line.Contains(Needle, ESearchCase::CaseSensitive))
		{
			FSafeCppRenameCppRefHit Hit;
			Hit.FilePath = FilePath;
			Hit.LineNumber = i + 1;
			Hit.LineText = MakePreviewLine(Line);

			InOutHits.Add(MoveTemp(Hit));

			if (InOutHits.Num() >= MaxHits)
			{
				return;
			}
		}
	}
}

FString FSafeCppRenameCppRefScanner::MakePreviewLine(const FString& Line, int32 MaxChars)
{
	FString Trimmed = Line;
	Trimmed.TrimStartAndEndInline();

	if (Trimmed.Len() <= MaxChars)
	{
		return Trimmed;
	}

	return Trimmed.Left(MaxChars) + TEXT("...");
}