// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Engine/Texture.h"

#include "common/CommonTypes.h"

class UnrealMaterial
{
public:
	UnrealMaterial();
	~UnrealMaterial();

	static void validateTextureAssets(bool subfolders);
	static UTexture* getTexture(const FString& name);

	const FName&	name() const;
	const FString&	displayName() const;

	void			instantiate(UWorld* world, const TMap<FString, FString >& materialLookUp);
	void			parse(const uint8* msgData);
private:
	FName	_uniqueName;
	FString	_displayName;

	const Foundry::MaterialData *_neutralData;
};