// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "common/CommonTypes.h"
#include "Camera/CameraActor.h"
#include "GameFrameWork/Actor.h"
#include "Engine/Light.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

class UnrealMesh;
class UnrealCamera;
class UnrealLight;
class ModoBridgeHandler;

class UnrealNode
{
public:
	UnrealNode();
	~UnrealNode();

	const FName&		name() const;
	const FString&		displayName() const;
	const float*		transformation() const;
	uint32				numMeshes() const;
	const FName&		mesh(const uint32 idx) const;
	uint32				numLights() const;
	const FName&		light(const uint32 idx) const;
	uint32				numCameras() const;
	const FName&		camera(const uint32 idx) const;
	uint32				numChildren() const;
	const FName&		child(const uint32 idx) const;

	void			parse(const uint8* msgData);

	// [TODO] separate this function to one for dreamobject, the other for staticmesh actor
	AActor*			instantiateMeshes(AStaticMeshActor*,UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix);
	AActor*			instantiateCameras(ACameraActor*, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix);
	AActor*			instantiateLights(ALight* pLight, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix);
	AActor*			instantiateAActors(AActor* pActor, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix);

	void			instantiateTextures(UWorld* world, ModoBridgeHandler* scene);
	UStaticMesh*	buildMeshes(UnrealMesh* unrealMesh, ModoBridgeHandler* scene, const TMap<FString, FString >& meshLookUp);

	const Foundry::NodeData* getData();

private:
	FName			_uniqueName;
	FString			_displayName;
	TArray<FName>	_meshes;
	TArray<FName>	_lights;
	TArray<FName>	_cameras;
	TArray<FName>	_children;
	TArray<FName>	_materialOverrideNames;
	float			_transformation[16];
	const Foundry::NodeData *_neutralData;
};

