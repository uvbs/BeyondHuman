// Copyright Foundry, 2017. All rights reserved.

#include "ModoBridge.h"
#include "AssetEditorToolkit.h"
#include "LevelEditor.h"
#include "IPluginManager.h"

#include "ModoBridgeCommands.h"
#include "ModoBridgeStyle.h"
#include "ModoBridgeSettings.h"
#include "ModoBridgeHandler.h"

#define LOCTEXT_NAMESPACE "ModoBridge"

namespace ESettingsWindowMode
{
	enum Type
	{
		Modal,
		Modeless
	};
};

namespace EOnSettingsWindowStartup
{
	enum Type
	{
		ResetProviderToNone,
		PreserveProvider
	};
};

DEFINE_LOG_CATEGORY(ModoBridge);

class ModoBridgeModule : public IModoBridge
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void SettingsClicked();
	void PushSelectedClicked();
	void PushAllClicked();

	void AddToolbarExtension(FToolBarBuilder &);
	void ShowSettingsDialog(ESettingsWindowMode::Type, EOnSettingsWindowStartup::Type);
	void OnSettingsDialogClosed(const TSharedRef<SWindow>& InWindow);

	TSharedPtr<const FExtensionBase>	_ToolbarExtension;
	TSharedPtr<FExtensibilityManager>	_ExtensionManager;
	TSharedPtr<FUICommandList>			_PluginCommands;
	TSharedPtr<FExtender>				_ToolbarExtender;

	TSharedRef<SWidget>					GenerateModoBridgeMenu(TSharedPtr<FUICommandList> InCommandList);
	TSharedPtr<SWindow>					SettingsWindowPtr;
	TSharedPtr<SModoBridgeSettings>		SettingsPtr;
	void*								DllHandle;
};

IMPLEMENT_MODULE(ModoBridgeModule, ModoBridge)

void ModoBridgeModule::OnSettingsDialogClosed(const TSharedRef<SWindow>& InWindow)
{
	SettingsWindowPtr = NULL;
}

void ModoBridgeModule::ShowSettingsDialog(ESettingsWindowMode::Type InLoginWindowMode, EOnSettingsWindowStartup::Type InOnLoginWindowStartup = EOnSettingsWindowStartup::ResetProviderToNone)
{
	if (InLoginWindowMode == ESettingsWindowMode::Modal && SettingsWindowPtr.IsValid())
	{
		// unhook the delegate so it doesn't fire in this case
		SettingsWindowPtr->SetOnWindowClosed(FOnWindowClosed());
		SettingsWindowPtr->RequestDestroyWindow();
		SettingsWindowPtr = NULL;
	}

	if (SettingsWindowPtr.IsValid())
	{
		SettingsWindowPtr->BringToFront();
	}
	else
	{
		// Create the window
		SettingsWindowPtr = SNew(SWindow)
			.Title(LOCTEXT("ModoBridgeSettings", "Modo Bridge Settings"))
			.HasCloseButton(true)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.SizingRule(ESizingRule::Autosized);

		// Set the closed callback
		SettingsWindowPtr->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &ModoBridgeModule::OnSettingsDialogClosed));


		// Setup the content for the created login window.
		SettingsWindowPtr->SetContent(
			SNew(SBox)
			.WidthOverride(256.0f)
			[
				SAssignNew(SettingsPtr, SModoBridgeSettings)
				.ParentWindow(SettingsWindowPtr)
			]
		);


		TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
		if (RootWindow.IsValid())
		{
			if (InLoginWindowMode == ESettingsWindowMode::Modal)
			{
				FSlateApplication::Get().AddModalWindow(SettingsWindowPtr.ToSharedRef(), RootWindow);
			}
			else
			{
				FSlateApplication::Get().AddWindowAsNativeChild(SettingsWindowPtr.ToSharedRef(), RootWindow.ToSharedRef());
			}
		}
		else
		{
			if (InLoginWindowMode == ESettingsWindowMode::Modal)
			{
				FSlateApplication::Get().AddModalWindow(SettingsWindowPtr.ToSharedRef(), RootWindow);
			}
			else
			{
				FSlateApplication::Get().AddWindow(SettingsWindowPtr.ToSharedRef());
			}
		}
	}
}

void ModoBridgeModule::SettingsClicked()
{
	ShowSettingsDialog(ESettingsWindowMode::Modeless);
}

void ModoBridgeModule::PushSelectedClicked()
{
	auto handler = ModoBridgeHandler::activeInstance();
	if (handler->isServerEnabled())
	{
		handler->pushSelected();
	}
}

void ModoBridgeModule::PushAllClicked()
{
	auto handler = ModoBridgeHandler::activeInstance();
	if (handler->isServerEnabled())
	{
		handler->pushAll();
	}
}

void ModoBridgeModule::StartupModule()
{
	auto path = IPluginManager::Get().FindPlugin("ModoBridge")->GetBaseDir();
	path += "/Source/ThirdParty/ZeroMQ/Libraries/";

#if PLATFORM_WINDOWS
	path += "x64/libzmq-v140-mt-4_2_1.dll";
#endif

 	DllHandle = FPlatformProcess::GetDllHandle(*path);

	ModoBridgeCommands::Register();
	ModoBridgeStyle::Initialize();

	_PluginCommands = MakeShareable(new FUICommandList);

	_PluginCommands->MapAction(
		ModoBridgeCommands::Get().Settings,
		FExecuteAction::CreateRaw(this, &ModoBridgeModule::SettingsClicked),
		FCanExecuteAction());

	_PluginCommands->MapAction(
		ModoBridgeCommands::Get().PushSelectedToModo,
		FExecuteAction::CreateRaw(this, &ModoBridgeModule::PushSelectedClicked),
		FCanExecuteAction());

	_PluginCommands->MapAction(
		ModoBridgeCommands::Get().PushAllToModo,
		FExecuteAction::CreateRaw(this, &ModoBridgeModule::PushAllClicked),
		FCanExecuteAction());


	_ToolbarExtender = MakeShareable(new FExtender);
	_ToolbarExtension = _ToolbarExtender->AddToolBarExtension("Content", EExtensionHook::Before, _PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &ModoBridgeModule::AddToolbarExtension));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(_ToolbarExtender);
	
	_ExtensionManager = LevelEditorModule.GetToolBarExtensibilityManager();
}

void ModoBridgeModule::ShutdownModule()
{
	ModoBridgeHandler::destroyInstances();

	ModoBridgeStyle::Shutdown();

	if (_ExtensionManager.IsValid())
	{
		ModoBridgeCommands::Unregister();

		_ToolbarExtender->RemoveExtension(_ToolbarExtension.ToSharedRef());

		_ExtensionManager->RemoveExtender(_ToolbarExtender);
	}
	else
	{
		_ExtensionManager.Reset();
	}

	if (DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
}

void clicked()
{

}

void extension(FToolBarBuilder &builder)
{

}

TSharedRef< SWidget > ModoBridgeModule::GenerateModoBridgeMenu(TSharedPtr<FUICommandList> InCommandList)
{
	ModoBridgeCommands::Register();
	ModoBridgeStyle::Initialize();

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension("Content", EExtensionHook::Before, InCommandList, FToolBarExtensionDelegate::CreateStatic(&extension));
	// Get all menu extenders for this context menu from the level editor module
	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < 1; ++i)
	{
			Extenders.Add(ToolbarExtender);
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList, MenuExtender);

	MenuBuilder.AddMenuEntry(
			ModoBridgeCommands::Get().Settings,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(ModoBridgeStyle::Get()->GetStyleSetName(), "Plugins.ModoBridgeSettings")
			);

	MenuBuilder.AddMenuEntry(
		ModoBridgeCommands::Get().PushAllToModo,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(ModoBridgeStyle::Get()->GetStyleSetName(), "Plugins.ModoBridgePushAll")
		);

	MenuBuilder.AddMenuEntry(
		ModoBridgeCommands::Get().PushSelectedToModo,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(ModoBridgeStyle::Get()->GetStyleSetName(), "Plugins.ModoBridgePushSelected")
		);

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void ModoBridgeModule::AddToolbarExtension(FToolBarBuilder &builder)
{
	UE_LOG(ModoBridge, Log, TEXT("Starting Extension logic"));

	builder.BeginSection("MODO");
	{
		builder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateRaw(this, &ModoBridgeModule::GenerateModoBridgeMenu, _PluginCommands),
			LOCTEXT("GroupOptions_Label", "Modo Bridge"),
			LOCTEXT("GroupOptionsToolTip", "Modo Bridge Options"),
			FSlateIcon(ModoBridgeStyle::Get()->GetStyleSetName(), "Plugins.ModoBridge"));
	}
	builder.EndSection();
}

#undef LOCTEXT_NAMESPACE