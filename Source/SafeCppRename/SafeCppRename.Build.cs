using UnrealBuildTool;

public class SafeCppRename : ModuleRules
{
	public SafeCppRename(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 纯编辑器模块
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ToolMenus",
			"AssetRegistry",
			"ContentBrowser",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",

			"Slate",
			"SlateCore",

			"ClassViewer",
			"AssetRegistry",
			"Projects"
		});
	}
}