#pragma once

#include "CoreMinimal.h"

/**
 * 重命名重定向请求对象
 *
 * 封装了执行重命名所需要的所有信息：
 * - 目标类、重命名后的类名
 * - 重定向的键值，用于将重定向写入 ini
 * - 是否写入文件等其他配置信息
 */
struct FSafeCppRedirectRequest
{
	/** 被重命名的类 */
	UClass* SelectedClass = nullptr;

	/** 新类名 */
	FString NewClassName;

	/** 重命名计划，通常是一个指向历史链的指针 */
	const void* Plan = nullptr; // 可能是指向某个计划对象的指针，具体看实际实现

	/** 重定向的键（在 ini 文件中的 key） */
	FString RedirectKey;

	/** 是否写入文件 */
	bool bWriteToFile = false;

	/** 输出的 ini 文件路径 */
	FString OutputIniFilePath;
	FString HistoryIniFilePath;
	FSafeCppRedirectRequest() = default;

	FSafeCppRedirectRequest(UClass* InClass, const FString& InNewClassName, const FString& InRedirectKey)
		: SelectedClass(InClass), NewClassName(InNewClassName), RedirectKey(InRedirectKey)
	{}
};
/**
 * SafeCppRename 中负责处理 C++ 类重命名的业务服务
 *
 * 职责：
 * - 校验请求
 * - 读取历史链
 * - 注册本次重命名
 * - 生成最终 redirect
 * - 写回 DefaultEngine.ini
 */
class FSafeCppRedirectService
{
public:
	/** 执行重命名和 redirect 更新 */
	static bool ApplyRedirects(
		const FSafeCppRedirectRequest& Request, FString& OutReport);

private:
	/** 构建完整的类名路径 */
	static bool BuildFullClassNames(
		const UClass* SelectedClass, 
		const FString& NewClassName, 
		FString& OutOldName, 
		FString& OutNewName, 
		FString& OutError);

	/** 获取脚本包路径（例如 /Script/ModuleName） */
	static bool GetScriptPackagePath(
		const UClass* SelectedClass, 
		FString& OutScriptPackagePath, 
		FString& OutError);

	
	/** 追加一行报告文本 */
	static void AppendLine(FString& InOutText, const FString& Line);
};