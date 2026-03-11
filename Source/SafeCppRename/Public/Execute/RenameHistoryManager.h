#pragma once

#include "CoreMinimal.h"
#include "Execute/RenameHistoryStore.h"
#include "Execute/RenameHistoryTypes.h"

/**
 * Rename 历史链业务管理层
 *
 * 职责：
 * 1. 在内存中维护所有历史链
 * 2. 查找某个类所在的链
 * 3. 只允许链尾发生 rename
 * 4. 将新名字追加到链尾
 * 5. 根据链生成最终 Redirect
 */
class FRenameHistoryManager
{
public:
	FRenameHistoryManager() = default;

	/** 清空所有链 */
	void Reset();

	/** 设置全部链（通常用于 Store 读取后灌入） */
	void SetChains(const TArray<FRenameHistoryChainRecord>& InChains);

	/** 获取全部链 */
	const TArray<FRenameHistoryChainRecord>& GetChains() const
	{
		return Chains;
	}

	/** 是否包含指定名字 */
	bool ContainsName(const FString& InName) const;

	/**
	 * 查找某个类所在的链
	 *
	 * @return 找到则返回链指针，找不到返回 nullptr
	 */
	const FRenameHistoryChainRecord* FindChainContaining(const FString& InName) const;

	/**
	 * 注册一次 rename
	 *
	 * 规则：
	 * 1. 如果 OldName 不在任何链中，则新建链：OldName, NewName
	 * 2. 如果 OldName 在某条链中，则它必须是链尾，才允许 append NewName
	 * 3. 不允许对中间节点 rename
	 */
	bool RegisterRename(const FString& OldName, const FString& NewName);

	/**
	 * 根据某个类所在的历史链生成 redirect
	 *
	 * 例如链：
	 * A,B,C,D
	 *
	 * 生成：
	 * A -> D
	 * B -> D
	 * C -> D
	 */
	bool GenerateRedirectsForClass(
		const FString& AnyClassName,
		TArray<FRedirectEntry>& OutRedirects) const;

	/**
	 * 为所有链生成 redirect
	 *
	 * 例如：
	 * Chain0: A,B,C,D   -> A->D B->D C->D
	 * Chain1: X,Y,Z     -> X->Z Y->Z
	 */
	TArray<FRedirectEntry> GenerateAllRedirects() const;

private:
	/** 所有历史链 */
	TArray<FRenameHistoryChainRecord> Chains;
};