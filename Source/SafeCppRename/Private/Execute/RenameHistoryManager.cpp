#include "Execute/RenameHistoryManager.h"



void FRenameHistoryManager::Reset()
{
	Chains.Reset();
}

void FRenameHistoryManager::SetChains(const TArray<FRenameHistoryChainRecord>& InChains)
{
	Chains = InChains;
}

bool FRenameHistoryManager::ContainsName(const FString& InName) const
{
	return FindChainContaining(InName) != nullptr;
}

const FRenameHistoryChainRecord* FRenameHistoryManager::FindChainContaining(const FString& InName) const
{
	if (InName.IsEmpty())
	{
		return nullptr;
	}

	for (const FRenameHistoryChainRecord& Chain : Chains)
	{
		if (Chain.ContainsName(InName))
		{
			return &Chain;
		}
	}

	return nullptr;
}

bool FRenameHistoryManager::RegisterRename(const FString& OldName, const FString& NewName)
{
	if (OldName.IsEmpty() || NewName.IsEmpty())
	{
		return false;
	}

	if (OldName == NewName)
	{
		return false;
	}

	// 查找 OldName 所在链
	for (FRenameHistoryChainRecord& Chain : Chains)
	{
		if (!Chain.ContainsName(OldName))
		{
			continue;
		}

		// 只允许链尾 rename
		const FString* LatestName = Chain.GetLatestName();
		if (LatestName == nullptr)
		{
			return false;
		}

		if (*LatestName != OldName)
		{
			// OldName 在链中，但不是最新名，拒绝
			return false;
		}

		// 防御性检查：不允许重复插入已有名字
		if (Chain.ContainsName(NewName))
		{
			return false;
		}

		Chain.Names.Add(NewName);
		return true;
	}

	// 没找到旧名所在链，说明这是第一次 rename，创建新链
	FRenameHistoryChainRecord NewChain;
	NewChain.Names.Add(OldName);
	NewChain.Names.Add(NewName);

	Chains.Add(MoveTemp(NewChain));
	return true;
}

bool FRenameHistoryManager::GenerateRedirectsForClass(
	const FString& AnyClassName,
	TArray<FRedirectEntry>& OutRedirects) const
{
	OutRedirects.Reset();

	const FRenameHistoryChainRecord* Chain = FindChainContaining(AnyClassName);
	if (Chain == nullptr)
	{
		return false;
	}

	if (!Chain->IsValid())
	{
		return false;
	}

	const FString* LatestName = Chain->GetLatestName();
	if (LatestName == nullptr || LatestName->IsEmpty())
	{
		return false;
	}

	// 除最后一个最新名字外，其余都指向最新名字
	for (int32 Index = 0; Index < Chain->Names.Num() - 1; ++Index)
	{
		const FString& OldName = Chain->Names[Index];

		if (OldName.IsEmpty() || OldName == *LatestName)
		{
			continue;
		}

		OutRedirects.Emplace(OldName, *LatestName);
	}

	return OutRedirects.Num() > 0;
}

TArray<FRedirectEntry> FRenameHistoryManager::GenerateAllRedirects() const
{
	TArray<FRedirectEntry> Result;
	TSet<FString> SeenOldNames;

	for (const FRenameHistoryChainRecord& Chain : Chains)
	{
		if (!Chain.IsValid())
		{
			continue;
		}

		const FString* LatestName = Chain.GetLatestName();
		if (LatestName == nullptr || LatestName->IsEmpty())
		{
			continue;
		}

		for (int32 Index = 0; Index < Chain.Names.Num() - 1; ++Index)
		{
			const FString& OldName = Chain.Names[Index];

			if (OldName.IsEmpty() || OldName == *LatestName)
			{
				continue;
			}

			// 防止异常情况下重复输出同一个 OldName
			if (SeenOldNames.Contains(OldName))
			{
				continue;
			}

			Result.Emplace(OldName, *LatestName);
			SeenOldNames.Add(OldName);
		}
	}

	return Result;
}