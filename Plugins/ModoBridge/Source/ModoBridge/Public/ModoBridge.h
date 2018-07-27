// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "EngineUtils.h"

#if defined(WIN32)
#define SNPRINTF _snprintf_s
#else
#define SNPRINTF snprintf
#endif

const FString MeshAssetPath = FString("/Game/Meshes/");
const FString MaterialAssetPath = FString("/Game/Materials/");
const FString TextureAssetPath = FString("/Game/Textures/");

DECLARE_LOG_CATEGORY_EXTERN(ModoBridge, Log, All);

class IModoBridge : public IModuleInterface
{
	//...
};

