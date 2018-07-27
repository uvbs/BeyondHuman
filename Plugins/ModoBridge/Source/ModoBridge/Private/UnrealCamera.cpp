// Copyright Foundry, 2017. All rights reserved.

#include "UnrealCamera.h"
#include <cassert>

#include "ModoBridge.h"
#include "UnrealNode.h"

UnrealCamera::UnrealCamera() :
	_uniqueName(),
	_displayName(),
	_neutralData(nullptr)
{
	// Empty
}

const FName& UnrealCamera::name() const
{
	return _uniqueName;
}

const FString& UnrealCamera::displayName() const
{
	return _displayName;
}

const Foundry::CameraData* UnrealCamera::getData()
{
	return _neutralData;
}

void UnrealCamera::parse(const uint8* msgData)
{
	_neutralData = flatbuffers::GetRoot<Foundry::CameraData>(msgData);
	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());
}