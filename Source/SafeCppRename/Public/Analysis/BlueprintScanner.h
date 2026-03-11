#pragma once

#include "CoreMinimal.h"

struct FAssetData;
class UClass;

/**
 * 扫描：继承指定 UClass 的所有蓝图（Child Blueprints）
 * v1 策略：遍历所有 UBlueprint 资产，加载后用 GeneratedClass->IsChildOf 判定
 */
class FSafeCppRenameBlueprintScanner
{
public:
	/** 扫描结果：返回所有子蓝图资产 */
	static bool ScanChildBlueprints(
		UClass* TargetClass,
		TArray<FAssetData>& OutChildBlueprintAssets,
		FString& OutError
	);

private:
	/** 判断某个蓝图资产是否为 TargetClass 的子类蓝图（加载蓝图后判定） */
	static bool IsBlueprintChildOf(UClass* TargetClass, const FAssetData& BlueprintAssetData);
};