// Copyright Foundry, 2017. All rights reserved.
 
#include "ModoBridgeCommands.h"
#include "ModoBridge.h"

#define LOCTEXT_NAMESPACE "ModoBridge"
 
PRAGMA_DISABLE_OPTIMIZATION
void ModoBridgeCommands::RegisterCommands()
{
	UI_COMMAND(Settings, "Settings", "Settings for MODO bridge", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PushAllToModo, "Push All", "Push all static meshes to MODO", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(PushSelectedToModo, "Push Selected", "Push selected static meshes to MODO", EUserInterfaceActionType::Button, FInputGesture());
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE