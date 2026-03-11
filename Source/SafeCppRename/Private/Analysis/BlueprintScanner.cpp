#include "Analysis/BlueprintScanner.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"


#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"


#include "UObject/SoftObjectPath.h" // FSoftClassPath
#include "UObject/TopLevelAssetPath.h"

static FString NormalizeClassPathString(const FString& In)
{
	// In 可能是：
	// 1) "/Script/SafeCppRenameTest.MyCharacter"
	// 2) "Class'/Script/SafeCppRenameTest.MyCharacter'"
	// 3) "/Script/CoreUObject.Class'/Script/SafeCppRenameTest.MyCharacter'"
	// 4) "BlueprintGeneratedClass'/Game/BP_A.BP_A_C'"

	// 只要带引号，大概率是 SoftClassPath/SoftObjectPath 格式
	if (In.Contains(TEXT("'")))
	{
		// FSoftClassPath 可以解析：Class'xxx' 或 /Script/CoreUObject.Class'xxx'
		const FSoftClassPath SCP(In);

		// UE5 推荐用 TopLevelAssetPath
		const FTopLevelAssetPath AssetPath = SCP.GetAssetPath();
		if (AssetPath.IsValid())
		{
			// 输出类似：/Script/SafeCppRenameTest.MyCharacter 或 /Game/BP_A.BP_A_C
			return AssetPath.ToString();
		}

		// 兜底：老接口
		const FString AssetPathStr = SCP.GetAssetPathString();
		if (!AssetPathStr.IsEmpty())
		{
			return AssetPathStr;
		}
	}

	// 已经是干净路径，直接返回
	return In;
}

bool FSafeCppRenameBlueprintScanner::ScanChildBlueprints(
	UClass* TargetClass,
	TArray<FAssetData>& OutChildBlueprintAssets,
	FString& OutError
)
{
	OutChildBlueprintAssets.Reset();
	OutError.Reset();

	if (!IsValid(TargetClass))
	{
		OutError = TEXT("TargetClass 无效");
		return false;
	}

	// 目标类完整路径，例如：
	// "/Script/SafeCppRenameTest.MyCharacter"
	const FString TargetClassPath =NormalizeClassPathString(TargetClass->GetPathName()) ;

	// 获取 AssetRegistry
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// 过滤 Blueprint 资产
	FARFilter Filter;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	UE_LOG(LogTemp, Warning, TEXT("ParentClassPath =%s, TargetClassPath=%s"),*TargetClassPath,*TargetClassPath);
	TArray<FAssetData> AllBlueprintAssets;
	AssetRegistry.GetAssets(Filter, AllBlueprintAssets);

	for (const FAssetData& AD : AllBlueprintAssets)
	{
	
		// 1️⃣ 直接父类路径
		FString ParentClassPath;
		if (AD.GetTagValue(TEXT("ParentClass"), ParentClassPath))
		{
			
			// 直接子类
			if (NormalizeClassPathString(ParentClassPath) == TargetClassPath)
			{
				OutChildBlueprintAssets.Add(AD);
				continue;
			}
		}

		// 2️⃣ NativeParentClass（间接继承情况）
		FString NativeParentPath;
		if (AD.GetTagValue(TEXT("NativeParentClass"), NativeParentPath))
		{
			// 如果蓝图最终继承自这个 C++ 类
			if (NativeParentPath == TargetClassPath)
			{
				OutChildBlueprintAssets.Add(AD);
				continue;
			}
		}
	}

	return true;
}

bool FSafeCppRenameBlueprintScanner::IsBlueprintChildOf(UClass* TargetClass, const FAssetData& BlueprintAssetData)
{
	if (!IsValid(TargetClass))
	{
		return false;
	}

	// 加载蓝图对象（v1 选择准确优先）
	UBlueprint* BP = Cast<UBlueprint>(BlueprintAssetData.GetAsset());
	if (!BP)
	{
		return false;
	}

	// GeneratedClass 才是运行时类
	UClass* GenClass = BP->GeneratedClass;
	if (!IsValid(GenClass))
	{
		return false;
	}

	// 是否为 TargetClass 的子类（包括间接继承）
	return GenClass->IsChildOf(TargetClass);
}