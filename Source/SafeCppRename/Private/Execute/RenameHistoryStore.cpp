#include "Execute/RenameHistoryStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FRenameHistoryStore::LoadChains(
	const FString& FilePath,
	TArray<FRenameHistoryChainRecord>& OutChains,
	FString& OutReport)
{
	OutChains.Reset();
	OutReport.Reset();

	if (FilePath.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: FilePath is empty."));
		return false;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(FilePath);

	if (!FPaths::FileExists(NormalizedPath))
	{
		AppendLine(OutReport, FString::Printf(TEXT("History file does not exist, treat as empty: %s"), *NormalizedPath));
		return true;
	}

	FString FileText;
	if (!FFileHelper::LoadFileToString(FileText, *NormalizedPath))
	{
		AppendLine(OutReport, FString::Printf(TEXT("Error: Failed to read history file: %s"), *NormalizedPath));
		return false;
	}

	TArray<FString> Lines;
	FileText.ParseIntoArrayLines(Lines, false);

	const FString TargetSectionName = GetSectionName();

	bool bInsideTargetSection = false;
	int32 LoadedCount = 0;

	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();

		if (Line.IsEmpty())
		{
			continue;
		}

		// 遇到新 section
		if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
		{
			bInsideTargetSection = IsSectionLine(Line, TargetSectionName);
			continue;
		}

		if (!bInsideTargetSection)
		{
			continue;
		}

		FRenameHistoryChainRecord ParsedChain;
		if (TryParseChainLine(Line, ParsedChain))
		{
			OutChains.Add(MoveTemp(ParsedChain));
			++LoadedCount;
		}
	}

	AppendLine(OutReport, FString::Printf(TEXT("Loaded chain count: %d"), LoadedCount));
	return true;
}

bool FRenameHistoryStore::SaveChains(
	const FString& FilePath,
	const TArray<FRenameHistoryChainRecord>& InChains,
	FString& OutReport)
{
	OutReport.Reset();

	if (FilePath.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: FilePath is empty."));
		return false;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(FilePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(NormalizedPath), true);

	FString OutputText;
	AppendLine(OutputText, MakeSectionLine(GetSectionName()));

	int32 SavedCount = 0;
	for (int32 ChainIndex = 0; ChainIndex < InChains.Num(); ++ChainIndex)
	{
		const FRenameHistoryChainRecord& Chain = InChains[ChainIndex];
		if (!Chain.IsValid())
		{
			continue;
		}

		AppendLine(OutputText, FormatChainLine(ChainIndex, Chain));
		++SavedCount;
	}

	if (!FFileHelper::SaveStringToFile(
		OutputText,
		*NormalizedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AppendLine(OutReport, FString::Printf(TEXT("Error: Failed to save history file: %s"), *NormalizedPath));
		return false;
	}

	AppendLine(OutReport, FString::Printf(TEXT("Saved chain count: %d"), SavedCount));
	AppendLine(OutReport, FString::Printf(TEXT("History file saved: %s"), *NormalizedPath));
	return true;
}

bool FRenameHistoryStore::TryParseChainLine(
	const FString& Line,
	FRenameHistoryChainRecord& OutChain)
{
	OutChain = FRenameHistoryChainRecord();

	if (Line.IsEmpty())
	{
		return false;
	}

	// 预期格式：
	// Chain0=/Script/Test.A,/Script/Test.B,/Script/Test.C
	const int32 EqualIndex = Line.Find(TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (EqualIndex == INDEX_NONE)
	{
		return false;
	}

	const FString Key = Line.Left(EqualIndex).TrimStartAndEnd();
	const FString Value = Line.Mid(EqualIndex + 1).TrimStartAndEnd();

	if (!Key.StartsWith(TEXT("Chain")))
	{
		return false;
	}

	if (Value.IsEmpty())
	{
		return false;
	}

	TArray<FString> ParsedNames;
	Value.ParseIntoArray(ParsedNames, TEXT(","), true);

	for (FString& Name : ParsedNames)
	{
		Name = Name.TrimStartAndEnd();
		if (!Name.IsEmpty())
		{
			OutChain.Names.Add(Name);
		}
	}

	return OutChain.IsValid();
}

FString FRenameHistoryStore::FormatChainLine(
	int32 ChainIndex,
	const FRenameHistoryChainRecord& Chain)
{
	FString JoinedNames;

	for (int32 Index = 0; Index < Chain.Names.Num(); ++Index)
	{
		if (Index > 0)
		{
			JoinedNames += TEXT(",");
		}

		JoinedNames += Chain.Names[Index];
	}

	return FString::Printf(TEXT("Chain%d=%s"), ChainIndex, *JoinedNames);
}

FString FRenameHistoryStore::GetSectionName()
{
	return TEXT("SafeCppRename");
}

bool FRenameHistoryStore::IsSectionLine(const FString& Line, const FString& SectionName)
{
	if (SectionName.IsEmpty())
	{
		return false;
	}

	return Line.TrimStartAndEnd().Equals(MakeSectionLine(SectionName), ESearchCase::CaseSensitive);
}

FString FRenameHistoryStore::MakeSectionLine(const FString& SectionName)
{
	return FString::Printf(TEXT("[%s]"), *SectionName);
}

void FRenameHistoryStore::AppendLine(FString& InOutText, const FString& Line)
{
	InOutText += Line;
	InOutText += LINE_TERMINATOR;
}