#pragma once

#include "CoreMinimal.h"
#include "RenameHistoryTypes.h"

/**
 * Redirect 仓库
 *
 * 职责：
 * 1. 保存当前已有的 Redirect 集合
 * 2. 按 OldName 建立索引
 * 3. 支持查找、新增、替换
 * 4. 合并“目标 Redirect”
 *
 * 注意：
 * - 本类不负责根据 Rename 历史链生成 Redirect
 * - 本类不负责写 ini 文件
 * - 本类只负责管理 Redirect 数据本身
 */
class FRedirectRepository
{
public:
	FRedirectRepository() = default;

	/** 清空所有 Redirect */
	void Reset();

	/**
	 * 添加一条“已有 Redirect”
	 *
	 * 用于初始化仓库，例如从现有配置中读出的 Redirect。
	 *
	 * 规则：
	 * - 如果 OldName 不存在，则新增
	 * - 如果 OldName 已存在且内容相同，则忽略
	 * - 如果 OldName 已存在但目标不同，则替换
	 *
	 * @return true 表示仓库发生变化
	 */
	bool AddExistingRedirect(const FRedirectEntry& Entry);

	/**
	 * 按 OldName 查找 Redirect
	 *
	 * @return 找到返回指针，找不到返回 nullptr
	 */
	const FRedirectEntry* FindByOldName(const FString& OldName) const;

	/**
	 * 合并一条“目标 Redirect”
	 *
	 * 合并策略：
	 * - 若不存在同 OldName，则新增
	 * - 若已存在且完全相同，则跳过
	 * - 若已存在但 NewName 不同，则替换为新的终态 Redirect
	 *
	 * @return true 表示仓库发生变化
	 */
	bool MergeDesiredRedirect(const FRedirectEntry& DesiredEntry);

	/**
	 * 批量合并目标 Redirect
	 *
	 * @return 实际发生变化的条数
	 */
	int32 MergeDesiredRedirects(const TArray<FRedirectEntry>& DesiredEntries);

	/**
	 * 是否包含指定 OldName
	 */
	bool ContainsOldName(const FString& OldName) const;

	/**
	 * 获取所有 Redirect
	 */
	const TArray<FRedirectEntry>& GetAllRedirects() const
	{
		return Redirects;
	}

	/**
	 * 获取 Redirect 数量
	 */
	int32 Num() const
	{
		return Redirects.Num();
	}

private:
	/**
	 * 重建 OldName -> RedirectIndex 索引
	 */
	void RebuildIndexMap();

	/**
	 * 判断输入记录是否合法
	 */
	bool IsEntryValid(const FRedirectEntry& Entry) const;

private:
	/** Redirect 顺序存储，便于最终输出 */
	TArray<FRedirectEntry> Redirects;

	/** OldName 到数组下标的索引 */
	TMap<FString, int32> OldNameToIndexMap;
};