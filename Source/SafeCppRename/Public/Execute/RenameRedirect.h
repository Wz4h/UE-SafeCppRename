#pragma once

#include "CoreMinimal.h"

class UClass;

/**
 * 负责生成 CoreRedirect 并写入 DefaultEngine.ini
 */
class FSafeCppRenameRedirect
{
public:

	/**
	 * 生成并写入 redirect
	 *
	 * @param OldClass      原始类
	 * @param NewClassName  新类名（带前缀，例如 AMyNewActor）
	 * @param OutReport     输出执行日志
	 */
	static bool GenerateRedirect(
		UClass* OldClass,
		const FString& NewClassName,
		FString& OutReport
	);
};