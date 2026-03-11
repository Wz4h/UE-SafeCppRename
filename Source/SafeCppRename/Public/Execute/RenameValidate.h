#pragma once

#include "CoreMinimal.h"

class UClass;

/** Validate 结果 */
struct FRenameValidateResult
{
	bool bOk = false;
	FString Error;             // 失败原因（英文）
	TArray<FString> Warnings;  // 警告（英文，可继续）
};

/** SafeCppRename：执行前校验 */
class FSafeCppRenameValidator
{
public:
	/**
	 * 校验 Rename 请求是否合法
	 * @param TargetClass   目标类（必须是 Native UClass）
	 * @param NewClassName  新类名（例如 AMyNewCharacter）
	 */
	static FRenameValidateResult Validate(UClass* TargetClass, const FString& NewClassName);

private:
	/** 校验类是否可重命名（Native/UCLASS 等） */
	static bool ValidateTargetClass(UClass* TargetClass, FString& OutError);

	/** 校验新名字是否合法（命名规则/前缀/字符集） */
	static bool ValidateNewNameBasic(UClass* TargetClass, const FString& NewClassName, FString& OutError);

	/** 校验是否同名冲突（已存在 UClass / 同名资产） */
	static bool ValidateNoNameCollision(UClass* TargetClass, const FString& NewClassName, FString& OutError);

	/** 校验目标类是否能定位到源文件（头文件必须存在，cpp 可选） */
	static bool ValidateSourceFilesExist(UClass* TargetClass, FString& OutError);

	/** 给出警告（不阻断） */
	static void CollectWarnings(UClass* TargetClass, const FString& NewClassName, FRenameValidateResult& InOutResult);
	
};