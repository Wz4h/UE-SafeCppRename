#pragma once

#include "CoreMinimal.h"
#include "RenameHistoryTypes.h"


/**
 * Rename 历史链持久化层
 *
 * 职责：
 * 1. 从 SafeCppRename.ini 读取所有历史链
 * 2. 将所有历史链写回 SafeCppRename.ini
 *
 * 注意：
 * - 不负责业务规则
 * - 不负责链追加逻辑
 * - 不负责生成 redirect
 * - 只做“磁盘 <-> 内存”转换
 */
class FRenameHistoryStore
{
public:
	/**
	 * 从历史文件中加载所有链
	 *
	 * @param FilePath 历史文件路径
	 * @param OutChains 输出的历史链列表
	 * @param OutReport 输出报告
	 * @return true 表示读取成功
	 */
	static bool LoadChains(
		const FString& FilePath,
		TArray<FRenameHistoryChainRecord>& OutChains,
		FString& OutReport);

	/**
	 * 将所有历史链保存到文件
	 *
	 * @param FilePath 历史文件路径
	 * @param InChains 要保存的历史链列表
	 * @param OutReport 输出报告
	 * @return true 表示保存成功
	 */
	static bool SaveChains(
		const FString& FilePath,
		const TArray<FRenameHistoryChainRecord>& InChains,
		FString& OutReport);

private:
	/**
	 * 解析单行 Chain 配置
	 *
	 * 例如：
	 * Chain0=/Script/Test.A,/Script/Test.B,/Script/Test.C
	 */
	static bool TryParseChainLine(
		const FString& Line,
		FRenameHistoryChainRecord& OutChain);

	/**
	 * 将单条链格式化为 ini 行
	 *
	 * 例如：
	 * Chain0=/Script/Test.A,/Script/Test.B,/Script/Test.C
	 */
	static FString FormatChainLine(
		int32 ChainIndex,
		const FRenameHistoryChainRecord& Chain);

	/** 生成 section 名 */
	static FString GetSectionName();

	/** 判断一行是否是目标 section */
	static bool IsSectionLine(const FString& Line, const FString& SectionName);

	/** 生成 section 行文本 */
	static FString MakeSectionLine(const FString& SectionName);

	/** 追加一行文本 */
	static void AppendLine(FString& InOutText, const FString& Line);
};