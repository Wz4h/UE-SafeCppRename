#pragma once

#include "CoreMinimal.h"

class UClass;

/** 单个文本替换操作（仅预演） */
struct FRenameTextReplaceOp
{
	/** 用于人类阅读的描述 */
	FString Desc;

	/** 查找的文本 */
	FString Find;

	/** 替换后的文本 */
	FString Replace;

	/** 是否大小写敏感 */
	bool bCaseSensitive = true;
	
	bool bMustHit = true;
};

/** 单个文件的改动计划 */
struct FRenameFilePlan
{
	/** 文件绝对路径 */
	FString FilePath;

	/** 是否需要重命名文件（例如 MyCharacter.h -> MyNewCharacter.h） */
	bool bRenameFile = false;

	/** 新文件名（绝对路径），仅在 bRenameFile=true 时有效 */
	FString NewFilePath;

	/** 文件内部的替换列表 */
	TArray<FRenameTextReplaceOp> Replaces;
};

/** CoreRedirects 计划（仅预演） */
struct FRenameRedirectPlan
{
	/** 旧类路径，例如 /Script/Mod.MyCharacter */
	FString OldClassPath;

	/** 新类路径，例如 /Script/Mod.MyNewCharacter */
	FString NewClassPath;

	/** 用于写入 ini 的一行文本（建议） */
	FString IniLine;
};

/** 预演输出：完整改动计划 */
struct FRenamePlan
{
	/** 是否成功生成计划 */
	bool bOk = false;

	/** 错误信息（bOk=false 时） */
	FString Error;

	/** 目标类名：AMyCharacter */
	FString OldClassName;

	/** 新类名：AMyNewCharacter */
	FString NewClassName;

	/** 目标模块名（用于生成 API 宏、生成路径等，可选） */
	FString ModuleName;

	/** 将修改的文件（通常 2 个：.h/.cpp） */
	TArray<FRenameFilePlan> Files;

	/** Redirects 计划（通常 1 条） */
	FRenameRedirectPlan Redirect;

	/** 生成一段可读报告（用于 Slate 输出） */
	FString ToReportString(bool bVerbose = false) const;
};

/** Plan 构建器：只生成计划，不做任何写入 */
class FSafeCppRenamePlanBuilder
{
public:
	/**
	 * 生成改名预演计划
	 * @param TargetClass  目标 C++ 类（必须是 Native）
	 * @param NewClassName 新类名（例如 AMyNewCharacter）
	 * @param bRenameFiles 是否计划重命名文件名（MyCharacter.h -> MyNewCharacter.h）
	 */
	static FRenamePlan BuildPlan(UClass* TargetClass, const FString& NewClassName, bool bRenameFiles);

private:
	
	static bool LocateClassHeaderAndCpp(
	UClass* TargetClass,
	FString& OutHeaderAbsPath,
	FString& OutCppAbsPath,
	FString& OutError
);
	/** 读取文件内容 */
	static bool LoadTextFile(const FString& AbsPath, FString& OutText);

	static void BuildReplacesForHeader(
	const FString& OldScriptName,
	const FString& NewScriptName,
	const FString& OldCppName,
	const FString& NewCppName,
	TArray<FRenameTextReplaceOp>& OutOps
);

	static void BuildReplacesForCpp(
		const FString& OldScriptName,
		const FString& NewScriptName,
		const FString& OldCppName,
		const FString& NewCppName,
		TArray<FRenameTextReplaceOp>& OutOps
	);

	static FRenameRedirectPlan BuildRedirectPlan(UClass* TargetClass, const FString& NewScriptName);

	/** 从文件名推导新文件名（只改基名，不改目录） */
	static FString MakeRenamedFilePath(const FString& OldAbsPath, const FString& OldClassName, const FString& NewClassName);
};