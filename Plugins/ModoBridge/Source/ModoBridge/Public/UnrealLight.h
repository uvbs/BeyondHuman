// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "common/CommonTypes.h"

#include "Containers/UnrealString.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "UObject/NameTypes.h"

#include <vector>
#include <string>

class UnrealLight
{
public:
	UnrealLight();
	~UnrealLight();

	const FName&	name() const;
	const FString&	displayName() const;
	void			parse(const uint8* msgData);
	const FRotator&	rotation() const;
	const FVector&	location() const;

	const Foundry::LightData* getData();

private:
	FName		_uniqueName;
	FString		_displayName;
	FVector		_location;
	FRotator	_rotation;
	const Foundry::LightData *_neutralData;
};