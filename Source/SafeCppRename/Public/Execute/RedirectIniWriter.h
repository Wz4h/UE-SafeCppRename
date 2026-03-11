#pragma once

#include "CoreMinimal.h"
#include "RenameHistoryTypes.h"

/**
 * Redirect ini 输出工具
 *
 * 职责：
 * 1. 将 RedirectEntry 格式化为 ini 行
 * 2. 读取并扫描现有 ini
 * 3. 合并到 [CoreRedirects] 段
 * 4. 覆盖写回整个 ini 文件
 */
class FRedirectIniWriter
{
public:
	/**
	 * 将单条 Redirect 格式化为 ini 行
	 *
	 * 例如：
	 * +ClassRedirects=(OldName="/Script/MyModule.A",NewName="/Script/MyModule.C")
	 */
	static FString FormatRedirectLine(const FString& RedirectKey, const FRedirectEntry& Entry);

	/**
	 * 批量生成 ini 文本片段
	 */
	static FString BuildRedirectText(const FString& RedirectKey, const TArray<FRedirectEntry>& Entries, bool bAppendTrailingNewLine = true);

	/**
	 * 将文本直接写入文件
	 *
	 * 注意：
	 * 这是底层写文件接口，不负责 section 合并逻辑。
	 */
	static bool WriteTextToFile(const FString& FilePath, const FString& Text, bool bAppend = false);

	/**
	 * 读取 ini、合并 Redirect、覆盖写回
	 *
	 * 合并规则：
	 * - 若无 [CoreRedirects]，则创建
	 * - 若无同 OldName，则添加
	 * - 若有同 OldName，则替换为最新 NewName
	 *
	 * @param FilePath ini 文件路径
	 * @param RedirectKey 例如 ClassRedirects
	 * @param DesiredEntries 希望写入/更新的目标 Redirect
	 * @param OutReport 输出过程报告
	 */
	static bool MergeRedirectsIntoIniFile(
		const FString& FilePath,
		const FString& RedirectKey,
		const TArray<FRedirectEntry>& DesiredEntries,
		FString& OutReport);
	/**
	 * 从 ini 文件中读取指定类型的 Redirect
	 *
	 * @param FilePath ini 文件路径
	 * @param RedirectKey 例如 ClassRedirects
	 * @param OutEntries 解析出的 Redirect 列表
	 * @param OutReport 输出报告
	 */
	static bool LoadRedirectsFromIniFile(
		const FString& FilePath,
		const FString& RedirectKey,
		TArray<FRedirectEntry>& OutEntries,
		FString& OutReport);
private:
	/** 对名字做 ini 输出安全处理 */
	static FString EscapeValue(const FString& InValue);

	/** 从一行 Redirect 文本中提取 OldName / NewName */
	static bool TryParseRedirectLine(
		const FString& Line,
		const FString& RedirectKey,
		FRedirectEntry& OutEntry);

	/** 从一行文本中提取形如 Key="Value" 的字符串值 */
	static bool TryExtractQuotedValue(
		const FString& Source,
		const FString& Key,
		FString& OutValue);

	/** 判断一行是否是指定 section，例如 [CoreRedirects] */
	static bool IsSectionLine(const FString& Line, const FString& SectionName);

	/** 生成 section 行，例如 [CoreRedirects] */
	static FString MakeSectionLine(const FString& SectionName);

	/** 追加一行文本 */
	static void AppendLine(FString& InOutText, const FString& Line);
};