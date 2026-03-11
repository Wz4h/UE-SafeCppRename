
#include "Execute/RedirectRepository.h"

void FRedirectRepository::Reset()
{
	Redirects.Reset();
	OldNameToIndexMap.Reset();
}

bool FRedirectRepository::AddExistingRedirect(const FRedirectEntry& Entry)
{
	if (!IsEntryValid(Entry))
	{
		return false;
	}

	const int32* FoundIndex = OldNameToIndexMap.Find(Entry.OldName);
	if (!FoundIndex)
	{
		// 不存在则新增
		const int32 NewIndex = Redirects.Add(Entry);
		OldNameToIndexMap.Add(Entry.OldName, NewIndex);
		return true;
	}

	FRedirectEntry& ExistingEntry = Redirects[*FoundIndex];

	// 已存在且完全相同，跳过
	if (ExistingEntry == Entry)
	{
		return false;
	}

	// 已存在但目标不同，替换
	ExistingEntry = Entry;
	return true;
}

const FRedirectEntry* FRedirectRepository::FindByOldName(const FString& OldName) const
{
	if (OldName.IsEmpty())
	{
		return nullptr;
	}

	const int32* FoundIndex = OldNameToIndexMap.Find(OldName);
	if (!FoundIndex)
	{
		return nullptr;
	}

	if (!Redirects.IsValidIndex(*FoundIndex))
	{
		return nullptr;
	}

	return &Redirects[*FoundIndex];
}

bool FRedirectRepository::MergeDesiredRedirect(const FRedirectEntry& DesiredEntry)
{
	if (!IsEntryValid(DesiredEntry))
	{
		return false;
	}

	const int32* FoundIndex = OldNameToIndexMap.Find(DesiredEntry.OldName);
	if (!FoundIndex)
	{
		// 不存在同源 OldName，直接新增
		const int32 NewIndex = Redirects.Add(DesiredEntry);
		OldNameToIndexMap.Add(DesiredEntry.OldName, NewIndex);
		return true;
	}

	FRedirectEntry& ExistingEntry = Redirects[*FoundIndex];

	// 已存在且完全一致，不需要变化
	if (ExistingEntry == DesiredEntry)
	{
		return false;
	}

	// 已存在但 NewName 不同，则替换为终态目标
	ExistingEntry = DesiredEntry;
	return true;
}

int32 FRedirectRepository::MergeDesiredRedirects(const TArray<FRedirectEntry>& DesiredEntries)
{
	int32 ChangedCount = 0;

	for (const FRedirectEntry& Entry : DesiredEntries)
	{
		if (MergeDesiredRedirect(Entry))
		{
			++ChangedCount;
		}
	}

	return ChangedCount;
}

bool FRedirectRepository::ContainsOldName(const FString& OldName) const
{
	return FindByOldName(OldName) != nullptr;
}

void FRedirectRepository::RebuildIndexMap()
{
	OldNameToIndexMap.Reset();

	for (int32 Index = 0; Index < Redirects.Num(); ++Index)
	{
		const FRedirectEntry& Entry = Redirects[Index];
		if (!Entry.OldName.IsEmpty())
		{
			OldNameToIndexMap.Add(Entry.OldName, Index);
		}
	}
}

bool FRedirectRepository::IsEntryValid(const FRedirectEntry& Entry) const
{
	if (!Entry.IsValid())
	{
		return false;
	}

	// Old/New 不允许相同
	if (Entry.OldName == Entry.NewName)
	{
		return false;
	}

	return true;
}