// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

#include "common/CommonTypes.h"

/**
 * 
 */
class UnrealMesh
{
public:
	UnrealMesh();
	~UnrealMesh();

	const FName&	name() const;
	const FString&	displayName() const;
	int32			matNameCount() const;
	FName			materialName(int idx) const;
	void			parse(const uint8* msgData);

	const Foundry::MeshData* getData();
private:
	FName			_uniqueName;
	FString			_displayName;
	TArray<FName>	_materialNames;

	const Foundry::MeshData	*_neutralData;
};
