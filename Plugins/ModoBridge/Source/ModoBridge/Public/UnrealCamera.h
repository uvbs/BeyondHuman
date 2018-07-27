// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

#include "common/CommonTypes.h"

class UnrealCamera
{
public:
	UnrealCamera();

	const FName&	name() const;
	const FString&	displayName() const;
	void			parse(const uint8* msgData);

	const Foundry::CameraData* getData();

private:
	FName	_uniqueName;
	FString	_displayName;

	const Foundry::CameraData	*_neutralData;

};