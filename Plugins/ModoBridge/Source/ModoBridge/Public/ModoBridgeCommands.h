// Copyright Foundry, 2017. All rights reserved.
 
#pragma once

#include "Framework/Commands/Commands.h"
#include "EditorStyle.h"

class ModoBridgeCommands : public TCommands<ModoBridgeCommands>
{
public:

	ModoBridgeCommands()
		: TCommands<ModoBridgeCommands>(TEXT("ModoBridge"), NSLOCTEXT("Contexts", "ModoBridge", "ModoBridge Plugin"), NAME_None, FEditorStyle::GetStyleSetName()) {}

	virtual void RegisterCommands() override;
	TSharedPtr< FUICommandInfo > PushSelectedToModo;
	TSharedPtr< FUICommandInfo > PushAllToModo;
	TSharedPtr< FUICommandInfo > Settings;
};