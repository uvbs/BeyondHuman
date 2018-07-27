// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "ModoBridgeMeshAssetData.generated.h"

// [TODO] We should desgin this class in a more general why to store any asset data we need.
// One UCLASS for each file
// This class is for storing additional information needed for bridging the mesh asset
UCLASS()
class UModoBridgeMeshAssetData : public UAssetUserData
{
	GENERATED_BODY()
public:
	// If Asset is reset in creation
	bool AssetReset;
};