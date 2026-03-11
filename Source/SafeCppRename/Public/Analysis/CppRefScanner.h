#pragma once

#include "CoreMinimal.h"

class UClass;

/** 一条 C++ 引用命中信息 */
struct FSafeCppRenameCppRefHit
{
	FString FilePath;   // 文件路径（绝对或项目相对都行）
	int32   LineNumber; // 1-based
	FString LineText;   // 命中行内容（截断后的预览）
};

/**
 * 扫描：Source 目录下对某个类名的 C++ 引用（纯文本）
 * v1：只做提示，不做替换。
 */
class FSafeCppRenameCppRefScanner
{
public:
	/**
	 * 扫描项目 Source 目录中所有 .h/.cpp/.inl，找 TargetClassName 的引用
	 * @param TargetClass       目标类
	 * @param OutHits           命中列表
	 * @param OutError          错误信息（失败时）
	 * @param MaxHits           最大命中条数（防止输出过多）
	 */
	static bool ScanSourceReferences(
		UClass* TargetClass,
		TArray<FSafeCppRenameCppRefHit>& OutHits,
		FString& OutError,
		int32 MaxHits = 500
	);

private:
	static void ScanOneFile(
		const FString& FilePath,
		const FString& Needle,
		TArray<FSafeCppRenameCppRefHit>& InOutHits,
		int32 MaxHits
	);

	static bool ShouldScanFile(const FString& FilePath);
	static FString MakePreviewLine(const FString& Line, int32 MaxChars = 200);
};