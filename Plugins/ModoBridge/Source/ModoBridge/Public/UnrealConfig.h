// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

#include "common/CommonTypes.h"

class UnrealConfig
{
public:
	UnrealConfig();
	~UnrealConfig();

	void			parse(const uint8* msgData);
	const Foundry::ConfigData* getData();

	bool useSubfolder;
	bool actorUniqueNameIdent;
	bool assetUniqueNameIdent;
	bool resetMeshAsset;

private:
	const Foundry::ConfigData *_neutralData;
};