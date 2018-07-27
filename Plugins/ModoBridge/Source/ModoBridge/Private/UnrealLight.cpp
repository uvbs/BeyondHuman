// Copyright Foundry, 2017. All rights reserved.

#include "UnrealLight.h"
#include <cassert>

#include "ModoBridge.h"
#include "UnrealNode.h"

UnrealLight::UnrealLight() :
	_uniqueName(),
	_displayName(),
	_location(),
	_rotation(),
	_neutralData(nullptr)
{

}

UnrealLight::~UnrealLight()
{
	_neutralData = nullptr;
}

const FName& UnrealLight::name() const
{
	return _uniqueName;
}

const FString&	UnrealLight::displayName() const
{
	return _displayName;
}

const FRotator& UnrealLight::rotation() const
{
	return _rotation;
}

const FVector&	UnrealLight::location() const
{
	return _location;
}

const Foundry::LightData* UnrealLight::getData()
{
	return _neutralData;
}

void UnrealLight::parse(const uint8* msgData)
{
	_neutralData = flatbuffers::GetRoot<Foundry::LightData>(msgData);
	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());

	const Foundry::Vec3 *pos = _neutralData->position();
	const Foundry::Vec3 *dir = _neutralData->direction();
	_location = FVector(pos->x(), pos->y(), pos->z());
	FVector direction(dir->x(), dir->y(), dir->z());
	_rotation = direction.Rotation();
}