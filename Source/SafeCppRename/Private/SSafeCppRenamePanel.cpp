#include "SSafeCppRenamePanel.h"

#include "ClassViewerModule.h"
#include "Analysis/BlueprintScanner.h"
#include "Analysis/CppRefScanner.h"
#include "Execute/RenameExecutor.h"
#include "Execute/RenamePlan.h"
#include "Execute/RenameRedirect.h"
#include "Execute/RenameValidate.h"
#include "Execute/SafeCppRedirectService.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SSafeCppRenamePanel"

// -------------- FSafeCppRenamePanel -------------- 

FText SSafeCppRenamePanel::GetAnalyzeOutputText() const
{
	return AnalyzeOutputText;
}

void SSafeCppRenamePanel::OnNewClassNameChanged(const FText& InText)
{
	NewClassName = InText.ToString();
}

FText SSafeCppRenamePanel::GetSelectedClassText() const
{
	if (SelectedClass.IsValid())
	{
		return FText::FromString(SelectedClass->GetName());
	}

	return LOCTEXT("NoClassSelected", "No class selected");
}

FReply SSafeCppRenamePanel::OnPickClass()
{
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::TreeView;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::ClassName;

	UClass* PickedClass = nullptr;

	TSharedPtr<SWindow> PickerWindow;

	FOnClassPicked OnPicked = FOnClassPicked::CreateLambda(
		[&PickedClass, &PickerWindow](UClass* InClass)
	{
		PickedClass = InClass;

		if (PickerWindow.IsValid())
		{
			PickerWindow->RequestDestroyWindow();
		}
	});

	FClassViewerModule& ClassViewerModule =
		FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	PickerWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Pick Target Class")))
		.ClientSize(FVector2D(600, 500))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			ClassViewerModule.CreateClassViewer(Options, OnPicked)
		];

	FSlateApplication::Get().AddModalWindow(
		PickerWindow.ToSharedRef(),
		FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr)
	);

	if (PickedClass)
	{
		SelectedClass = PickedClass;
	}

	return FReply::Handled();
}

void SSafeCppRenamePanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(STextBlock)
			.Text(LOCTEXT("Title", "Safe C++ Rename (Editor Only)"))
		]

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(STextBlock)
			.Text(LOCTEXT("Tip", "Flow: Analyze -> Execute -> (Restart) -> Verify. v1: UI + Plan only."))
		]

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(SUniformGridPanel)
			.SlotPadding(6.f)

			+ SUniformGridPanel::Slot(0, 0)
			[ 
				SNew(SButton)
				.Text(LOCTEXT("PickClass", "Pick Class..."))
				.OnClicked(this, &SSafeCppRenamePanel::OnPickClass)
			]

			+ SUniformGridPanel::Slot(1, 0)
			[ 
				SNew(STextBlock)
				.Text(this, &SSafeCppRenamePanel::GetSelectedClassText)
			]
		]

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(SEditableTextBox)
			.HintText(LOCTEXT("NewClassNameHint", "New Class Name (e.g. AMyNewActor)"))
			.OnTextChanged(this, &SSafeCppRenamePanel::OnNewClassNameChanged)
		]

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(SUniformGridPanel)
			.SlotPadding(6.f)

			+ SUniformGridPanel::Slot(0, 0)
			[ 
				SNew(SButton)
				.Text(LOCTEXT("Analyze", "Analyze"))
				.OnClicked(this, &SSafeCppRenamePanel::OnClickAnalyze)
			]

			+ SUniformGridPanel::Slot(1, 0)
			[ 
				SNew(SButton)
				.Text(LOCTEXT("Execute", "Execute"))
				.OnClicked(this, &SSafeCppRenamePanel::OnClickExecute)
			]
			
		]

		// =========================
		// Show details checkbox
		// =========================
		+ SScrollBox::Slot()
		.Padding(12.f)
		[   
			SNew(SCheckBox)
			.IsChecked(this, &SSafeCppRenamePanel::GetShowDetailsCheckState)
			.OnCheckStateChanged(this, &SSafeCppRenamePanel::OnShowDetailsChanged)
			[ 
				SNew(STextBlock)
				.Text(LOCTEXT("ShowDetails", "Show details"))
			]
		]

		// =========================
		// Analyze/Output Area
		// =========================
		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(STextBlock)
			.Text(LOCTEXT("AnalyzeResultLabel", "Output"))
		]

		+ SScrollBox::Slot()
		.Padding(12.f)
		[ 
			SNew(SMultiLineEditableTextBox)
			.Text(this, &SSafeCppRenamePanel::GetAnalyzeOutputText)
			.IsReadOnly(true)
			.AutoWrapText(true)
		]
	];
}

void SSafeCppRenamePanel::SetTargetClass(UClass* InClass)
{
	SelectedClass = InClass;
}

ECheckBoxState SSafeCppRenamePanel::GetShowDetailsCheckState() const
{
	return bShowDetails ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}
void SSafeCppRenamePanel::OnShowDetailsChanged(ECheckBoxState NewState)
{
	bShowDetails = (NewState == ECheckBoxState::Checked);

	// 如果有缓存的 Plan，更新 Plan 部分
	if (bHasCachedPlan)
	{
		// 只刷新 Plan 部分，不重新扫描
		FString PlanText = CachedPlan.ToReportString(bShowDetails);

		// 替换原有 Plan 部分
		CurrentOutputText = BaseOutputText + PlanText;

		// 刷新显示
		AnalyzeOutputText = FText::FromString(CurrentOutputText);
	}
}

// -------------- Analyze -------------- 
FReply SSafeCppRenamePanel::OnClickAnalyze()
{
    // 中文注释：开始执行分析时，先清空缓存
    bHasCachedPlan = false;

    // 清空执行报告（不再需要 ExecReport 了）
    CurrentOutputText.Reset();

    // 1) 检查目标类是否有效
    if (!SelectedClass.IsValid())
    {
        CurrentOutputText = TEXT("No target class selected.");
        AnalyzeOutputText = FText::FromString(CurrentOutputText);
        return FReply::Handled();
    }

    // 2) 检查新类名是否有效
    if (NewClassName.IsEmpty())
    {
        CurrentOutputText = TEXT("New class name is empty.");
        AnalyzeOutputText = FText::FromString(CurrentOutputText);
        return FReply::Handled();
    }

    // 3) 执行验证
    FRenameValidateResult V = FSafeCppRenameValidator::Validate(
        SelectedClass.Get(),
        NewClassName
    );

    if (!V.bOk)
    {
        FString Out;
        Out += TEXT("[Validate] FAILED\n");
        Out += TEXT("Reason: ") + V.Error + TEXT("\n");

        CurrentOutputText = Out;
        AnalyzeOutputText = FText::FromString(CurrentOutputText);
        return FReply::Handled();
    }

    // 4) 执行扫描（蓝图和C++引用）
    TArray<FAssetData> ChildBPs;
    FString Err;
    const double T0 = FPlatformTime::Seconds();

    const double T_BP0 = FPlatformTime::Seconds();
    FSafeCppRenameBlueprintScanner::ScanChildBlueprints(SelectedClass.Get(), ChildBPs, Err);
    const double T_BP1 = FPlatformTime::Seconds();

    // 扫描 C++ 引用
    TArray<FSafeCppRenameCppRefHit> CppHits;
    FString Err2;

    const double T_CPP0 = FPlatformTime::Seconds();
    FSafeCppRenameCppRefScanner::ScanSourceReferences(SelectedClass.Get(), CppHits, Err2, 300);
    const double T_CPP1 = FPlatformTime::Seconds();

    // 5) 生成报告
   
    BaseOutputText += FString::Printf(TEXT("Target: %s\n\n"), *SelectedClass->GetName());

    BaseOutputText += FString::Printf(TEXT("Child Blueprints: %d\n"), ChildBPs.Num());
    for (const FAssetData& AD : ChildBPs)
    {
        BaseOutputText += FString::Printf(TEXT(" - %s\n"), *AD.GetObjectPathString());
    }

    BaseOutputText += TEXT("\n");
    BaseOutputText += FString::Printf(TEXT("C++ References (hits=%d, max=%d)\n"), CppHits.Num(), 300);
    for (const auto& H : CppHits)
    {
        BaseOutputText += FString::Printf(TEXT(" - %s:%d: %s\n"), *H.FilePath, H.LineNumber, *H.LineText);
    }

    if (!Err.IsEmpty())
    {
        BaseOutputText += TEXT("\n[BlueprintScanner Error]\n") + Err + TEXT("\n");
    }
    if (!Err2.IsEmpty())
    {
        BaseOutputText += TEXT("\n[CppRefScanner Error]\n") + Err2 + TEXT("\n");
    }

    // 6) 生成重命名计划
    const bool bRenameFiles = true;
    CachedPlan = FSafeCppRenamePlanBuilder::BuildPlan(
        SelectedClass.Get(),
        NewClassName,
        bRenameFiles
    );

    bHasCachedPlan = CachedPlan.bOk;

    // 7) Plan 部分输出，基于 show details 控制是否展开
	FString PlanText;
    PlanText += TEXT("\n================ Plan ================\n");
    if (!CachedPlan.bOk)
    {
        PlanText += TEXT("[Plan] FAILED\n");
        PlanText += TEXT("Reason: ") + CachedPlan.Error + TEXT("\n");
    }
    else
    {
        PlanText = CachedPlan.ToReportString(bShowDetails); // 根据 Show details 控制是否展开
    }

    // 将 PlanText 和 BaseOutputText 合并为最终输出
    CurrentOutputText = BaseOutputText + PlanText;
    AnalyzeOutputText = FText::FromString(CurrentOutputText);

    // 8) 输出时间消耗
    const double T1 = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Warning, TEXT("[Analyze] Blueprint=%.3fs, Cpp=%.3fs, Total=%.3fs"),
        T_BP1 - T_BP0,
        T_CPP1 - T_CPP0,
        T1 - T0
    );

	
	// ==========================
	// Dump Blueprint ParentClassTag (raw + parsed)
	// ==========================

	// 中文注释：从 ParentClassTag 里提取 /Script/Module.Class 真实路径
	auto ExtractClassPathFromTag = [](const FString& Tag1) -> FString
	{
		FString Result = Tag1;

		int32 QuoteStart = INDEX_NONE;
		if (Tag1.FindChar('\'', QuoteStart))
		{
			int32 QuoteEnd = INDEX_NONE;
			if (Tag1.FindLastChar('\'', QuoteEnd) && QuoteEnd > QuoteStart)
			{
				Result = Tag1.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
			}
		}
		return Result;
	};

	UE_LOG(LogTemp, Warning, TEXT("======== [Analyze] ParentClassTag Dump BEGIN ========"));
	UE_LOG(LogTemp, Warning, TEXT("TargetClass=%s Path=%s"),
		SelectedClass.IsValid() ? *SelectedClass->GetName() : TEXT("None"),
		SelectedClass.IsValid() ? *SelectedClass->GetPathName() : TEXT("None"));

	for (const FAssetData& AD : ChildBPs)
	{
		// 中文注释：蓝图资产路径
		const FString BPPath = AD.GetObjectPathString();

		// 中文注释：取 ParentClass tag（原样）
		FString ParentTagRaw;
		const bool bHasParentTag = AD.GetTagValue(TEXT("ParentClass"), ParentTagRaw);

		// 中文注释：解析出真实 ClassPath（如果带 Class'...' 外壳）
		const FString ParentClassPathParsed = bHasParentTag ? ExtractClassPathFromTag(ParentTagRaw) : TEXT("");

		UE_LOG(LogTemp, Warning, TEXT("[BP] %s"), *BPPath);

		if (!bHasParentTag)
		{
			UE_LOG(LogTemp, Warning, TEXT("  ParentClassTagRaw=<MISSING TAG>"));
			UE_LOG(LogTemp, Warning, TEXT("  ParentClassPathParsed=<EMPTY>"));
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("  ParentClassTagRaw=%s"), *ParentTagRaw);
		UE_LOG(LogTemp, Warning, TEXT("  ParentClassPathParsed=%s"), *ParentClassPathParsed);
	}

	UE_LOG(LogTemp, Warning, TEXT("======== [Analyze] ParentClassTag Dump END ========"));
	
	
	
    return FReply::Handled();
}




// -------------- Execute --------------

FReply SSafeCppRenamePanel::OnClickExecute()
{
	if (!bHasCachedPlan || !CachedPlan.bOk)
	{
		CurrentOutputText = TEXT("No valid plan cached. Please click Analyze first.");
		AnalyzeOutputText = FText::FromString(CurrentOutputText);
		return FReply::Handled();
	}

	CurrentOutputText.Reset();

	FString Out;

	// Execute
	FString ExecReport;
	const bool bDryRun = false;
	const bool bStrict = true;

	if (!FSafeCppRenameExecutor::ExecutePlan(CachedPlan, bDryRun, bStrict, ExecReport))
	{
		Out += TEXT("\n\n================ Execute ================\n");
		Out += ExecReport;

		AnalyzeOutputText = FText::FromString(Out);
		return FReply::Handled();
	}

	Out += TEXT("\n\n================ Execute ================\n");
	Out += ExecReport;

	Out += TEXT("\n\n================ Redirect ================\n");

	FSafeCppRedirectRequest RedirectRequest;
	RedirectRequest.SelectedClass = SelectedClass.Get();
	RedirectRequest.NewClassName = NewClassName;
	RedirectRequest.Plan = &CachedPlan;
	RedirectRequest.bWriteToFile = true;
	RedirectRequest.OutputIniFilePath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");
	RedirectRequest.RedirectKey = TEXT("ClassRedirects");
	RedirectRequest.HistoryIniFilePath = FPaths::ProjectConfigDir() / TEXT("SafeCppRename.ini"); 
	FString RedirectReport;
	if (!FSafeCppRedirectService::ApplyRedirects(RedirectRequest, RedirectReport))
	{
		Out += RedirectReport;
		AnalyzeOutputText = FText::FromString(Out);
		return FReply::Handled();
	}

	Out += RedirectReport;

	AnalyzeOutputText = FText::FromString(Out);

	return FReply::Handled();
}

FReply SSafeCppRenamePanel::OnClickVerify()
{
	// v1：先占位
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("VerifyPlaceholder", "Verify: TODO (check Blueprint parent recovery after restart)"));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE