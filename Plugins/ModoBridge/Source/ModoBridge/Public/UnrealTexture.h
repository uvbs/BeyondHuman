// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

#include "common/CommonTypes.h"

class UnrealTexture
{
public:
	UnrealTexture();
	~UnrealTexture();

	const FName&	name() const;
	const FString&	displayName() const;

	void			instantiate(UWorld* world, const TMap<FString, FString >& materialLookUp);
	void			parse(const uint8* msgData);
private:
	FName	_uniqueName;
	FString	_displayName;

	const Foundry::TextureData *_neutralData;
};