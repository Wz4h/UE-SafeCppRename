#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class SSafeCppRenamePanel;

/**
 * 纯 Editor 模块：注册 Tab + Tools 菜单 + 右键菜单入口
 * 右键菜单会把当前选中的 C++ UClass 传给面板，作为默认目标类。
 */
class FSafeCppRenameModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// 注册菜单入口（Tools + 右键）
	void RegisterMenus();

	// 打开工具 Tab（并把 PendingTargetClass 传给面板）
	void OpenRenameTab();

	// 创建 Tab
	TSharedRef<SDockTab> SpawnRenameTab(const class FSpawnTabArgs& Args);

	// 从右键/内容浏览器上下文解析出选中的 UClass
	UClass* ResolveSelectedCppClassFromMenuContext(const struct FToolMenuContext& Context) const;

private:
	// Tab 名
	static const FName RenameTabName;

	// 右键传入的“待处理类”
	TWeakObjectPtr<UClass> PendingTargetClass;

	// Tab 面板实例（用于 Tab 已经打开时更新目标类）
	TWeakPtr<SSafeCppRenamePanel> PanelWidgetWeak;
};