// Copyright Foundry, 2017. All rights reserved.

#include "UnrealMesh.h"
#include <vector>
#include <memory>

#include "ModoBridge.h"
#include "UnrealNode.h"

UnrealMesh::UnrealMesh() :
	_uniqueName(), 
	_displayName(),
	_neutralData(nullptr)
{

}

UnrealMesh::~UnrealMesh()
{
	_neutralData = nullptr;
}

const FName& UnrealMesh::name() const
{
	return _uniqueName;
}

const FString& UnrealMesh::displayName() const
{
	return _displayName;
}

int32 UnrealMesh::matNameCount() const
{
	return _materialNames.Num();
}

FName UnrealMesh::materialName(int idx) const
{	
	return _materialNames[idx];
}

const Foundry::MeshData* UnrealMesh::getData()
{
	return _neutralData;
}

void UnrealMesh::parse(const uint8* msgData)
{
	_neutralData = flatbuffers::GetRoot<Foundry::MeshData>(msgData);

	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());

	auto materialNames = _neutralData->materialNames();

	if (materialNames != nullptr)
	{
		int size = materialNames->size();
		_materialNames.Empty();
		for (int i = 0; i < size; i++)
		{
			const char* matName = (*materialNames)[i]->c_str();
			_materialNames.Add(UTF8_TO_TCHAR(matName));
		}
	}
}

