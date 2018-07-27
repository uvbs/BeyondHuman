// Copyright Foundry, 2017. All rights reserved.

#include "UnrealConfig.h"
#include <cassert>

#include "ModoBridge.h"

UnrealConfig::UnrealConfig() :
	_neutralData(nullptr),
	useSubfolder(false),
	actorUniqueNameIdent(false),
	assetUniqueNameIdent(false),
	resetMeshAsset(false)
{

}

UnrealConfig::~UnrealConfig()
{
	_neutralData = nullptr;
}

const Foundry::ConfigData* UnrealConfig::getData()
{
	return _neutralData;
}

void UnrealConfig::parse(const uint8* msgData)
{
	_neutralData = flatbuffers::GetRoot<Foundry::ConfigData>(msgData);
	if (_neutralData != nullptr)
	{
		useSubfolder = _neutralData->useSubFolder();
		actorUniqueNameIdent = _neutralData->actorUniqueNameIdent();
		assetUniqueNameIdent = _neutralData->assetUniqueNameIdent();
		resetMeshAsset = _neutralData->resetMeshAsset();
	}
}