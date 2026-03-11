
#include "Execute/RedirectIniWriter.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
bool FRedirectIniWriter::LoadRedirectsFromIniFile(
	const FString& FilePath,
	const FString& RedirectKey,
	TArray<FRedirectEntry>& OutEntries,
	FString& OutReport)
{
	OutEntries.Reset();
	OutReport.Reset();

	if (FilePath.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: FilePath is empty."));
		return false;
	}

	if (RedirectKey.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: RedirectKey is empty."));
		return false;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(FilePath);

	if (!FPaths::FileExists(NormalizedPath))
	{
		AppendLine(OutReport, FString::Printf(TEXT("Ini file does not exist, treat as empty: %s"), *NormalizedPath));
		return true;
	}

	FString FileText;
	if (!FFileHelper::LoadFileToString(FileText, *NormalizedPath))
	{
		AppendLine(OutReport, FString::Printf(TEXT("Error: Failed to read ini file: %s"), *NormalizedPath));
		return false;
	}

	TArray<FString> Lines;
	FileText.ParseIntoArrayLines(Lines, false);

	const FString TargetSectionName = TEXT("CoreRedirects");

	int32 SectionStartIndex = INDEX_NONE;
	int32 SectionEndIndex = INDEX_NONE;

	// 查找 [CoreRedirects]
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (IsSectionLine(Lines[Index], TargetSectionName))
		{
			SectionStartIndex = Index;
			SectionEndIndex = Lines.Num();

			for (int32 NextIndex = Index + 1; NextIndex < Lines.Num(); ++NextIndex)
			{
				const FString Trimmed = Lines[NextIndex].TrimStartAndEnd();
				if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
				{
					SectionEndIndex = NextIndex;
					break;
				}
			}
			break;
		}
	}

	if (SectionStartIndex == INDEX_NONE)
	{
		AppendLine(OutReport, TEXT("No [CoreRedirects] section found, treat as empty."));
		return true;
	}

	TMap<FString, FRedirectEntry> UniqueEntriesByOldName;

	for (int32 Index = SectionStartIndex + 1; Index < SectionEndIndex; ++Index)
	{
		FRedirectEntry ParsedEntry;
		if (TryParseRedirectLine(Lines[Index], RedirectKey, ParsedEntry))
		{
			// 同 OldName 只保留最后一条，和 merge 逻辑保持一致
			UniqueEntriesByOldName.Add(ParsedEntry.OldName, ParsedEntry);
		}
	}

	TArray<FString> SortedOldNames;
	UniqueEntriesByOldName.GetKeys(SortedOldNames);
	SortedOldNames.Sort();

	for (const FString& OldName : SortedOldNames)
	{
		if (const FRedirectEntry* Entry = UniqueEntriesByOldName.Find(OldName))
		{
			OutEntries.Add(*Entry);
		}
	}

	AppendLine(OutReport, FString::Printf(TEXT("Loaded redirect count: %d"), OutEntries.Num()));
	return true;
}
FString FRedirectIniWriter::FormatRedirectLine(const FString& RedirectKey, const FRedirectEntry& Entry)
{
	if (RedirectKey.IsEmpty() || !Entry.IsValid())
	{
		return FString();
	}

	const FString EscapedOldName = EscapeValue(Entry.OldName);
	const FString EscapedNewName = EscapeValue(Entry.NewName);

	return FString::Printf(
		TEXT("+%s=(OldName=\"%s\",NewName=\"%s\")"),
		*RedirectKey,
		*EscapedOldName,
		*EscapedNewName
	);
}

FString FRedirectIniWriter::BuildRedirectText(const FString& RedirectKey, const TArray<FRedirectEntry>& Entries, bool bAppendTrailingNewLine)
{
	if (RedirectKey.IsEmpty() || Entries.Num() == 0)
	{
		return FString();
	}

	FString Result;
	Result.Reserve(Entries.Num() * 96);

	for (const FRedirectEntry& Entry : Entries)
	{
		if (!Entry.IsValid())
		{
			continue;
		}

		const FString Line = FormatRedirectLine(RedirectKey, Entry);
		if (Line.IsEmpty())
		{
			continue;
		}

		Result += Line;
		Result += LINE_TERMINATOR;
	}

	if (!bAppendTrailingNewLine && Result.Len() > 0)
	{
		const FString LineTerminator = LINE_TERMINATOR;
		if (Result.EndsWith(LineTerminator))
		{
			Result.LeftChopInline(LineTerminator.Len(), false);
		}
	}

	return Result;
}

bool FRedirectIniWriter::WriteTextToFile(const FString& FilePath, const FString& Text, bool bAppend)
{
	if (FilePath.IsEmpty() || Text.IsEmpty())
	{
		return false;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(FilePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(NormalizedPath), true);

	if (bAppend)
	{
		return FFileHelper::SaveStringToFile(
			Text,
			*NormalizedPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
			&IFileManager::Get(),
			FILEWRITE_Append
		);
	}

	return FFileHelper::SaveStringToFile(
		Text,
		*NormalizedPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
	);
}

bool FRedirectIniWriter::MergeRedirectsIntoIniFile(
	const FString& FilePath,
	const FString& RedirectKey,
	const TArray<FRedirectEntry>& DesiredEntries,
	FString& OutReport)
{
	OutReport.Reset();

	if (FilePath.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: FilePath is empty."));
		return false;
	}

	if (RedirectKey.IsEmpty())
	{
		AppendLine(OutReport, TEXT("Error: RedirectKey is empty."));
		return false;
	}

	if (DesiredEntries.Num() == 0)
	{
		AppendLine(OutReport, TEXT("No desired redirects to merge."));
		return true;
	}

	const FString NormalizedPath = FPaths::ConvertRelativePathToFull(FilePath);

	FString FileText;
	if (FPaths::FileExists(NormalizedPath))
	{
		if (!FFileHelper::LoadFileToString(FileText, *NormalizedPath))
		{
			AppendLine(OutReport, FString::Printf(TEXT("Error: Failed to read ini file: %s"), *NormalizedPath));
			return false;
		}
	}
	else
	{
		AppendLine(OutReport, FString::Printf(TEXT("Ini file does not exist. A new file will be created: %s"), *NormalizedPath));
		FileText.Reset();
	}

	TArray<FString> Lines;
	FileText.ParseIntoArrayLines(Lines, false);

	const FString TargetSectionName = TEXT("CoreRedirects");

	int32 SectionStartIndex = INDEX_NONE;
	int32 SectionEndIndex = INDEX_NONE;

	// 查找 [CoreRedirects] 段
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (IsSectionLine(Lines[Index], TargetSectionName))
		{
			SectionStartIndex = Index;
			SectionEndIndex = Lines.Num();

			for (int32 NextIndex = Index + 1; NextIndex < Lines.Num(); ++NextIndex)
			{
				const FString Trimmed = Lines[NextIndex].TrimStartAndEnd();
				if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
				{
					SectionEndIndex = NextIndex;
					break;
				}
			}
			break;
		}
	}

	// 先收集现有 section 内的所有行，并解析出已有 redirect
	TMap<FString, FRedirectEntry> OldNameToMergedEntry;
	TArray<FString> PreservedNonTargetLines;

	if (SectionStartIndex != INDEX_NONE)
	{
		for (int32 Index = SectionStartIndex + 1; Index < SectionEndIndex; ++Index)
		{
			const FString& Line = Lines[Index];

			FRedirectEntry ParsedEntry;
			if (TryParseRedirectLine(Line, RedirectKey, ParsedEntry))
			{
				// 已有同 OldName 时，以最后一次扫描到的为准
				OldNameToMergedEntry.Add(ParsedEntry.OldName, ParsedEntry);
			}
			else
			{
				PreservedNonTargetLines.Add(Line);
			}
		}
	}

	// 再合并新的目标 redirect
	int32 AddedCount = 0;
	int32 ReplacedCount = 0;
	int32 UnchangedCount = 0;

	for (const FRedirectEntry& DesiredEntry : DesiredEntries)
	{
		if (!DesiredEntry.IsValid() || DesiredEntry.OldName == DesiredEntry.NewName)
		{
			continue;
		}

		if (FRedirectEntry* ExistingEntry = OldNameToMergedEntry.Find(DesiredEntry.OldName))
		{
			if (*ExistingEntry == DesiredEntry)
			{
				++UnchangedCount;
			}
			else
			{
				*ExistingEntry = DesiredEntry;
				++ReplacedCount;
			}
		}
		else
		{
			OldNameToMergedEntry.Add(DesiredEntry.OldName, DesiredEntry);
			++AddedCount;
		}
	}

	// 排序输出，保证稳定性
	TArray<FString> SortedOldNames;
	OldNameToMergedEntry.GetKeys(SortedOldNames);
	SortedOldNames.Sort();

	TArray<FString> RebuiltSectionLines;
	RebuiltSectionLines.Reserve(PreservedNonTargetLines.Num() + SortedOldNames.Num() + 1);

	// 保留原 section 中非目标 key 的行
	for (const FString& Line : PreservedNonTargetLines)
	{
		RebuiltSectionLines.Add(Line);
	}

	// 追加重建后的目标 Redirect 行
	for (const FString& OldName : SortedOldNames)
	{
		const FRedirectEntry* Entry = OldNameToMergedEntry.Find(OldName);
		if (!Entry)
		{
			continue;
		}

		RebuiltSectionLines.Add(FormatRedirectLine(RedirectKey, *Entry));
	}

	// 重新组装整个 ini 文件
	TArray<FString> OutputLines;

	if (SectionStartIndex == INDEX_NONE)
	{
		// 原文件没有 [CoreRedirects]，直接在尾部创建
		OutputLines = Lines;

		// 如果原文件非空，且末尾不是空行，补一个空行增强可读性
		if (OutputLines.Num() > 0)
		{
			const FString LastTrimmed = OutputLines.Last().TrimStartAndEnd();
			if (!LastTrimmed.IsEmpty())
			{
				OutputLines.Add(FString());
			}
		}

		OutputLines.Add(MakeSectionLine(TargetSectionName));
		for (const FString& Line : RebuiltSectionLines)
		{
			OutputLines.Add(Line);
		}
	}
	else
	{
		// 原文件存在 [CoreRedirects]，替换该段内容
		for (int32 Index = 0; Index < SectionStartIndex; ++Index)
		{
			OutputLines.Add(Lines[Index]);
		}

		OutputLines.Add(Lines[SectionStartIndex]);

		for (const FString& Line : RebuiltSectionLines)
		{
			OutputLines.Add(Line);
		}

		for (int32 Index = SectionEndIndex; Index < Lines.Num(); ++Index)
		{
			OutputLines.Add(Lines[Index]);
		}
	}

	FString FinalText;
	for (const FString& Line : OutputLines)
	{
		FinalText += Line;
		FinalText += LINE_TERMINATOR;
	}

	if (!WriteTextToFile(NormalizedPath, FinalText, false))
	{
		AppendLine(OutReport, FString::Printf(TEXT("Error: Failed to write merged ini file: %s"), *NormalizedPath));
		return false;
	}

	AppendLine(OutReport, FString::Printf(TEXT("Merge ini file success: %s"), *NormalizedPath));
	AppendLine(OutReport, FString::Printf(TEXT("Added redirect count: %d"), AddedCount));
	AppendLine(OutReport, FString::Printf(TEXT("Replaced redirect count: %d"), ReplacedCount));
	AppendLine(OutReport, FString::Printf(TEXT("Unchanged redirect count: %d"), UnchangedCount));

	return true;
}

FString FRedirectIniWriter::EscapeValue(const FString& InValue)
{
	FString Result = InValue;
	Result.ReplaceInline(TEXT("\""), TEXT("\\\""));
	return Result;
}

bool FRedirectIniWriter::TryParseRedirectLine(
	const FString& Line,
	const FString& RedirectKey,
	FRedirectEntry& OutEntry)
{
	OutEntry = FRedirectEntry();

	if (Line.IsEmpty() || RedirectKey.IsEmpty())
	{
		return false;
	}

	const FString Trimmed = Line.TrimStartAndEnd();
	const FString Prefix = FString::Printf(TEXT("+%s="), *RedirectKey);

	if (!Trimmed.StartsWith(Prefix))
	{
		return false;
	}

	FString OldName;
	FString NewName;

	if (!TryExtractQuotedValue(Trimmed, TEXT("OldName"), OldName))
	{
		return false;
	}

	if (!TryExtractQuotedValue(Trimmed, TEXT("NewName"), NewName))
	{
		return false;
	}

	OutEntry = FRedirectEntry(OldName, NewName);
	return OutEntry.IsValid();
}

bool FRedirectIniWriter::TryExtractQuotedValue(
	const FString& Source,
	const FString& Key,
	FString& OutValue)
{
	OutValue.Reset();

	if (Source.IsEmpty() || Key.IsEmpty())
	{
		return false;
	}

	const FString Pattern = Key + TEXT("=\"");
	int32 StartIndex = Source.Find(Pattern, ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (StartIndex == INDEX_NONE)
	{
		return false;
	}

	StartIndex += Pattern.Len();

	int32 EndIndex = INDEX_NONE;
	if (!Source.FindChar(TEXT('"'), EndIndex))
	{
		// 这只是找第一个 "，不对，下面重算
	}

	EndIndex = Source.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex);
	if (EndIndex == INDEX_NONE || EndIndex < StartIndex)
	{
		return false;
	}

	OutValue = Source.Mid(StartIndex, EndIndex - StartIndex);
	return !OutValue.IsEmpty();
}

bool FRedirectIniWriter::IsSectionLine(const FString& Line, const FString& SectionName)
{
	if (SectionName.IsEmpty())
	{
		return false;
	}

	const FString Trimmed = Line.TrimStartAndEnd();
	return Trimmed.Equals(MakeSectionLine(SectionName), ESearchCase::CaseSensitive);
}

FString FRedirectIniWriter::MakeSectionLine(const FString& SectionName)
{
	return FString::Printf(TEXT("[%s]"), *SectionName);
}

void FRedirectIniWriter::AppendLine(FString& InOutText, const FString& Line)
{
	InOutText += Line;
	InOutText += LINE_TERMINATOR;
}