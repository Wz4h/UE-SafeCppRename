#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Execute/RenamePlan.h" // FRenamePlan

class SSafeCppRenamePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSafeCppRenamePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetTargetClass(UClass* InClass);

private:

	// 用于存储当前输出内容（分析或执行）
	FString CurrentOutputText;

	// UI 状态（v1：只保存用户输入）
	TWeakObjectPtr<UClass> SelectedClass;
	FString NewClassName;
	FText AnalyzeOutputText;

	// Show details checkbox 状态
	bool bShowDetails = false;

	// 缓存的 Plan（用于切换 Show details 时即时刷新）
	bool bHasCachedPlan = false;
	FRenamePlan CachedPlan;

	// Slate 绑定
	FText GetAnalyzeOutputText() const;

	// Show details checkbox 绑定
	ECheckBoxState GetShowDetailsCheckState() const;
	void OnShowDetailsChanged(ECheckBoxState NewState);
	
	FString BaseOutputText;
	
	// 事件处理
	FReply OnClickAnalyze();
	FReply OnClickExecute();
	FReply OnClickVerify();

	// 类选择器弹窗
	FReply OnPickClass();

	// 文本框回调
	void OnNewClassNameChanged(const FText& InText);

	// 显示当前选择的类
	FText GetSelectedClassText() const;
};