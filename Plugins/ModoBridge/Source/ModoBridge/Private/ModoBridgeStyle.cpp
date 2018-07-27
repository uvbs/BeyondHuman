// Copyright Foundry, 2017. All rights reserved.

#include "ModoBridgeStyle.h"
#include "SlateStyle.h"
#include "IPluginManager.h"
#include "EditorStyleSet.h"
#include "Styling/SlateStyleRegistry.h"

#include "ModoBridge.h"

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( ModoBridgeStyle::InContent( RelativePath, ".png" ), __VA_ARGS__ )

FString ModoBridgeStyle::InContent(const FString& RelativePath, const ANSICHAR* Extension)
{
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("ModoBridge"))->GetContentDir();

	return ( ContentDir / RelativePath ) + Extension;
}

TSharedPtr< FSlateStyleSet > ModoBridgeStyle::StyleSet;
TSharedPtr< class ISlateStyle > ModoBridgeStyle::Get() { return StyleSet; }

void ModoBridgeStyle::Initialize()
{
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);

	// Only register once
	if (StyleSet.IsValid() )
	{
		return;
	}

	StyleSet = MakeShareable( new FSlateStyleSet("ModoBridgeStyle") );
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Plugins Manager
	{
		const FTextBlockStyle NormalText = FEditorStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");

		StyleSet->Set("Plugins.ModoBridge", new IMAGE_BRUSH("modo", Icon40x40));
		StyleSet->Set("Plugins.ModoBridgeSettings", new IMAGE_BRUSH("settings", Icon16x16));
		StyleSet->Set("Plugins.ModoBridgePushSelected", new IMAGE_BRUSH("pushselected", Icon16x16));
		StyleSet->Set("Plugins.ModoBridgePushAll", new IMAGE_BRUSH("pushall", Icon16x16));
	}

	FSlateStyleRegistry::RegisterSlateStyle( *StyleSet.Get() );
};

void ModoBridgeStyle::Shutdown()
{
	if (StyleSet.IsValid() )
	{
		FSlateStyleRegistry::UnRegisterSlateStyle( *StyleSet.Get() );
		ensure( StyleSet.IsUnique() );
		StyleSet.Reset();
	}
}