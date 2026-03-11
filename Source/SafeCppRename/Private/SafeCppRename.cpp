#include "SafeCppRename.h"
#include "SSafeCppRenamePanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

// 这个头用于从 ToolMenuContext 里拿到 SelectedAssets


#include "ContentBrowserMenuContexts.h"

#define LOCTEXT_NAMESPACE "SafeCppRename"

const FName FSafeCppRenameModule::RenameTabName(TEXT("SafeCppRenameTab"));

void FSafeCppRenameModule::StartupModule()
{
	// 注册 Tab
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		                        RenameTabName,
		                        FOnSpawnTab::CreateRaw(this, &FSafeCppRenameModule::SpawnRenameTab)
	                        )
	                        .SetDisplayName(LOCTEXT("TabTitle", "Safe C++ Rename"))
	                        .SetMenuType(ETabSpawnerMenuType::Hidden);

	// 注册菜单（确保 ToolMenus 可用）
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSafeCppRenameModule::RegisterMenus)
	);
}

void FSafeCppRenameModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RenameTabName);
}

TSharedRef<SDockTab> FSafeCppRenameModule::SpawnRenameTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SSafeCppRenamePanel> Panel = SNew(SSafeCppRenamePanel);
	PanelWidgetWeak = Panel;

	// 如果打开 Tab 时已经有 PendingTargetClass，则直接设置
	if (PendingTargetClass.IsValid())
	{
		Panel->SetTargetClass(PendingTargetClass.Get());
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Panel
		];
}

void FSafeCppRenameModule::OpenRenameTab()
{
	// 唤起 Tab（如果已存在则激活，不会重复创建）
	FGlobalTabmanager::Get()->TryInvokeTab(RenameTabName);

	// Tab 已经存在的情况下，需要把目标类更新到现有面板
	if (PanelWidgetWeak.IsValid() && PendingTargetClass.IsValid())
	{
		PanelWidgetWeak.Pin()->SetTargetClass(PendingTargetClass.Get());
	}
}


UClass* FSafeCppRenameModule::ResolveSelectedCppClassFromMenuContext(const FToolMenuContext& Context) const
{
	auto TryResolveFromAssetData = [](const FAssetData& AD) -> UClass*
	{
		// 1) 有时能直接拿到
		if (UObject* Obj = AD.GetAsset())
		{
			if (UClass* AsClass = Cast<UClass>(Obj))
			{
				return AsClass;
			}
		}

		// 2) C++ 类常见情况：GetAsset 拿不到，但 ObjectPath 是 /Script/Module.Class
		const FString ObjPath = AD.GetObjectPathString();   // 例如 "/Script/SafeCppRenameTest.MyCharacter"
		if (!ObjPath.IsEmpty() && ObjPath.StartsWith(TEXT("/Script/")))
		{
			if (UClass* Loaded = LoadObject<UClass>(nullptr, *ObjPath))
			{
				return Loaded;
			}
			// 有时不需要 Load，用 FindObject 也能拿到（可选）
			if (UClass* Found = FindObject<UClass>(nullptr, *ObjPath))
			{
				return Found;
			}
		}

		return nullptr;
	};

	// A) 优先：右键上下文（最符合“右键哪个就是哪个”）
	if (const UContentBrowserAssetContextMenuContext* AssetCtx = Context.FindContext<UContentBrowserAssetContextMenuContext>())
	{
		for (const FAssetData& AD : AssetCtx->SelectedAssets)
		{
			if (UClass* C = TryResolveFromAssetData(AD))
			{
				return C;
			}
		}
	}

	// B) 兜底：Content Browser 当前选择
	{
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssets;
		CB.Get().GetSelectedAssets(SelectedAssets);

		for (const FAssetData& AD : SelectedAssets)
		{
			if (UClass* C = TryResolveFromAssetData(AD))
			{
				return C;
			}
		}
	}

	return nullptr;
}


void FSafeCppRenameModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// -------- Tools 菜单（保底入口，基本稳定）--------
	if (UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = ToolsMenu->FindOrAddSection("SafeCppRename");
		Section.AddMenuEntry(
			"SafeCppRename_Tools",
			LOCTEXT("SafeCppRename_Tools_Label", "Safe C++ Rename"),
			LOCTEXT("SafeCppRename_Tools_Tip", "Open Safe C++ Rename tool."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FSafeCppRenameModule::OpenRenameTab))
		);
	}

	// -------- 右键菜单（你截图里的 C++ 类右键）--------
	// 不同 UE 版本菜单名可能不同：这里同时挂多个候选，保证“能出现”
	auto AddContextEntry = [&](const TCHAR* MenuName)
	{
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName))
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("SafeCppRename");

			FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
				"SafeCppRename_Context",
				LOCTEXT("SafeCppRename_Context_Label", "Safe C++ Rename"),
				LOCTEXT("SafeCppRename_Context_Tip", "Safely rename this C++ class (Blueprint-safe)."),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& Context)
				{
					if (UClass* Picked = ResolveSelectedCppClassFromMenuContext(Context))
					{
						PendingTargetClass = Picked;
					}
					else
					{
						PendingTargetClass = nullptr;
					}

					OpenRenameTab(); // 继续调用你现在这个无参函数
				})
			);

			// 这里必须用 ExecuteAction(带Context) 才能拿到“右键选中的那个类”
			FToolMenuEntry Entry1 = FToolMenuEntry::InitMenuEntry(
				"SafeCppRename_Context",
				LOCTEXT("SafeCppRename_Context_Label", "Safe C++ Rename"),
				LOCTEXT("SafeCppRename_Context_Tip", "Safely rename this C++ class (Blueprint-safe)."),
				FSlateIcon(),
				FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& Context)
				{
					if (UClass* Picked = ResolveSelectedCppClassFromMenuContext(Context))
					{
						PendingTargetClass = Picked;
					}
					else
					{
						PendingTargetClass = nullptr;
					}

					OpenRenameTab();
				})
			);

			Section.AddEntry(Entry1);
		}
	};

	// 你截图里的常见位置（不同版本不同名字）
	//AddContextEntry(TEXT("ContentBrowser.AssetContextMenu.CppClass"));
	AddContextEntry(TEXT("ContentBrowser.AssetContextMenu.Class"));
	//AddContextEntry(TEXT("ContentBrowser.AssetContextMenu"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSafeCppRenameModule, SafeCppRename)
