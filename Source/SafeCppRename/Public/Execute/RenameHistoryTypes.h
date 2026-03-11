#pragma once

#include "CoreMinimal.h"

/**
 * Redirect 数据结构
 *
 * 用于表示一条 CoreRedirect：
 *
 * OldName -> NewName
 *
 * 例如：
 * /Script/Test.A -> /Script/Test.D
 */
struct FRedirectEntry
{
	FString OldName;
	FString NewName;

	FRedirectEntry()
	{
	}

	FRedirectEntry(const FString& InOldName, const FString& InNewName)
		: OldName(InOldName)
		, NewName(InNewName)
	{
	}

	bool IsValid() const
	{
		return !OldName.IsEmpty() && !NewName.IsEmpty();
	}

	bool operator==(const FRedirectEntry& Other) const
	{
		return OldName == Other.OldName && NewName == Other.NewName;
	}
};


/**
 * Rename 历史链
 *
 * 表示一个类从最早名字到最新名字的完整 rename 过程
 *
 * 例如：
 *
 * A -> B -> C -> D
 *
 * 存储为：
 *
 * [A, B, C, D]
 */
struct FRenameHistoryChainRecord
{
	/** 按时间顺序存储的名字链 */
	TArray<FString> Names;

	bool IsValid() const
	{
		return Names.Num() >= 2;
	}

	bool ContainsName(const FString& InName) const
	{
		return Names.Contains(InName);
	}

	const FString* GetLatestName() const
	{
		return Names.Num() > 0 ? &Names.Last() : nullptr;
	}

	void Append(const FString& NewName)
	{
		Names.Add(NewName);
	}
};