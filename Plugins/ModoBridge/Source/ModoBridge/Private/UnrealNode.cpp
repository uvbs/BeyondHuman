// Copyright Foundry, 2017. All rights reserved.

#include "UnrealNode.h"
#include <cassert>

#include "Camera/CameraActor.h"
#include "Engine/Light.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Components/LightComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "RawMesh.h"
#include "StaticMeshResources.h"
#include "AssetRegistryModule.h"

#include "ModoBridge.h"
#include "ModoBridgeHandler.h"
#include "UnrealMesh.h"
#include "UnrealCamera.h"
#include "UnrealHelper.h"
#include "UnrealLight.h"
#include "ModoBridgeMeshAssetData.h"

UnrealNode::UnrealNode() :
	_uniqueName(),
	_displayName(),
	_meshes(),
	_lights(),
	_cameras(),
	_children(),
	_materialOverrideNames(),
	_neutralData(nullptr)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			if (i == j)
			{
				_transformation[i * 4 + j] = 1;
			}
			else
			{
				_transformation[i * 4 + j] = 0;
			}
		}
	}
}

UnrealNode::~UnrealNode()
{
	_neutralData = nullptr;
}

const FName& UnrealNode::name() const
{
	return _uniqueName;
}

const FString&	UnrealNode::displayName() const
{
	return _displayName;
}

const Foundry::NodeData* UnrealNode::getData()
{
	return _neutralData;
}

void UnrealNode::parse(const uint8* msgData)
{
	_neutralData = flatbuffers::GetRoot<Foundry::NodeData>(msgData);
	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());

	auto trans = _neutralData->transformation();

	for (uint32 i = 0; i < trans->size() && i < 16; i++)
		_transformation[i] = (*trans)[i];

	auto meshNames = _neutralData->meshes();
	for (uint32 i = 0; i < meshNames->size(); i++)
		_meshes.Add(UTF8_TO_TCHAR((*meshNames)[i]->c_str()));

	auto lightNames = _neutralData->lights();
	for (uint32 i = 0; i < lightNames->size(); i++)
		_lights.Add(UTF8_TO_TCHAR((*lightNames)[i]->c_str()));

	auto cameraNames = _neutralData->cameras();
	for (uint32 i = 0; i < cameraNames->size(); i++)
		_cameras.Add(UTF8_TO_TCHAR((*cameraNames)[i]->c_str()));

	auto childrenNames = _neutralData->children();
	for (uint32 i = 0; i < childrenNames->size(); i++)
		_children.Add(UTF8_TO_TCHAR((*childrenNames)[i]->c_str()));

	auto matOverrides = _neutralData->materialOverrideNames();
	for (uint32 i = 0; i < matOverrides->size(); i++)
		_materialOverrideNames.Add(UTF8_TO_TCHAR((*matOverrides)[i]->c_str()));

}

uint32 UnrealNode::numChildren() const
{
	return _children.Num();
}

const FName& UnrealNode::child(const uint32 idx) const
{
	return _children[idx];
}

const float* UnrealNode::transformation() const
{
	return _transformation;
}

uint32 UnrealNode::numMeshes() const
{
	return _meshes.Num();
}

const FName& UnrealNode::mesh(const uint32 idx) const
{
	return _meshes[idx];
}

uint32 UnrealNode::numLights() const
{
	return _lights.Num();
}

const FName& UnrealNode::light(const uint32 idx) const
{
	return _lights[idx];
}

uint32 UnrealNode::numCameras() const
{
	return _cameras.Num();
}

const FName& UnrealNode::camera(const uint32 idx) const
{
	return _cameras[idx];
}

UStaticMesh * CreateStaticMesh(const FString packageName, FName meshName)
{
	UObject* meshAsset = nullptr;
	UPackage* existingPackage = FindPackage(nullptr, *packageName);
	if (existingPackage)
	{
		meshAsset = FindObject<UStaticMesh>(existingPackage, *meshName.ToString());
	}

	if (meshAsset == nullptr)
	{
		UPackage* assetPackage = CreatePackage(nullptr, *packageName);
		assetPackage->FullyLoad();
		EObjectFlags Flags = RF_Public | RF_Standalone;
		meshAsset = UnrealHelper::CreateObject<UStaticMesh>(assetPackage, meshName, Flags);
	}

	if (meshAsset)
	{
		// Mark the package dirty...
		meshAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(meshAsset);
	}

	return Cast<UStaticMesh>(meshAsset);
}

UMaterial* CreateMaterial(const FString packageName, FName materialName)
{
	UPackage* existingPackage = FindPackage(NULL, *packageName);
	UObject* materialAsset = nullptr;
	if (existingPackage)
	{
		materialAsset = FindObject<UObject>(existingPackage, *materialName.ToString());
	}

	if (materialAsset == nullptr)
	{
		UPackage* assetPackage = CreatePackage(NULL, *packageName);
		if (assetPackage != nullptr)
		{
			assetPackage->FullyLoad();
			EObjectFlags Flags = RF_Public | RF_Standalone;
			materialAsset = NewObject<UMaterial>(assetPackage, materialName, Flags);
			if (materialAsset)
			{
				// Mark the package dirty...
				assetPackage->MarkPackageDirty();
			}
		}
	}

	return Cast<UMaterial>(materialAsset);
}

void AdjustCameraNeutralToUnreal (ACameraActor* pCameraActor)
{
	FRotator myRotationValue = FRotator(-90.0, 0.0, -90.0);
	pCameraActor->AddActorLocalRotation(myRotationValue);
	pCameraActor->SetActorRelativeScale3D(FVector(1.0, 1.0, 1.0));
}

void AdjustLightNeutralToUnreal(ALight* pLight)
{
	FRotator myRotationValue = FRotator(-90.0, 0.0, -90.0);
	pLight->AddActorLocalRotation(myRotationValue);
	pLight->SetActorRelativeScale3D(FVector(1.0, 1.0, 1.0));
}

bool LoadMaterialsFromUnreal(TArray<FStaticMaterial>& materials, ModoBridgeHandler* scene, const FString& key)
{
	UMaterial* Material = (UMaterial*)StaticLoadObject(UMaterial::StaticClass(), nullptr, *key);
	if (Material != NULL)
	{
		materials.Add(Material);
		return true;
	}

	return false;
}

UStaticMesh* UnrealNode::buildMeshes(UnrealMesh* unrealMesh, ModoBridgeHandler* scene, const TMap<FString, FString >& meshLookUp)
{
	FRawMesh RawMesh;

	static const size_t materialIndex = 0; // no material yet
	static const size_t vertPerPoly = 3; // assuming 3 verts per poly

	const Foundry::MeshData* meshData = unrealMesh->getData();
	auto indices = meshData->indices();
	for (size_t i = 0; i < indices->size(); i++)
	{
		RawMesh.WedgeIndices.Add((*indices)[i]);
	}

	size_t polyCount = indices->size() / vertPerPoly;
	auto matIndicies = meshData->matIndices();
	for (size_t i = 0; i < polyCount; i++)
	{
		RawMesh.FaceSmoothingMasks.Add(1 << (i % 32));

		size_t idxIndex = i * vertPerPoly;
		RawMesh.FaceMaterialIndices.Add((*matIndicies)[idxIndex]);
	}

	auto vertices = meshData->vertices();
	size_t vertexCount = vertices->size() / Foundry::DataStrideVertex;
	for (size_t i = 0; i < vertexCount; i++)
	{
		FVector vector(
			(*vertices)[Foundry::DataStrideVertex * i], 
			(*vertices)[Foundry::DataStrideVertex * i + 1],
			(*vertices)[Foundry::DataStrideVertex * i + 2]);
		CoordinateSytemConverter::Get()->ConvertPositionNeutralToUnreal(vector);
		RawMesh.VertexPositions.Add(vector);
	}

	auto normals = meshData->normals();
	size_t normalCount = normals->size() / Foundry::DataStrideNormal;
	for (size_t i = 0; i < normalCount; i++)
	{
		FVector vector(
			(*normals)[Foundry::DataStrideNormal * i],
			(*normals)[Foundry::DataStrideNormal * i + 1],
			(*normals)[Foundry::DataStrideNormal * i + 2]);

		CoordinateSytemConverter::Get()->ConvertNormalNeutralToUnreal(vector);
		RawMesh.WedgeTangentZ.Add(vector);
	}

	auto tangents = meshData->tangents();
	size_t tangentCount = tangents->size() / Foundry::DataStrideTangent;
	for (size_t i = 0; i < tangentCount; i++)
	{
		FVector tangent(
			(*tangents)[Foundry::DataStrideTangent * i],
			(*tangents)[Foundry::DataStrideTangent * i + 1],
			(*tangents)[Foundry::DataStrideTangent * i + 2]);

		CoordinateSytemConverter::Get()->ConvertNormalNeutralToUnreal(tangent);
		RawMesh.WedgeTangentX.Add(tangent);

		FVector biNormal(
			(*tangents)[Foundry::DataStrideTangent * i + 3],
			(*tangents)[Foundry::DataStrideTangent * i + 4],
			(*tangents)[Foundry::DataStrideTangent * i + 5]);

		CoordinateSytemConverter::Get()->ConvertNormalNeutralToUnreal(biNormal);
		RawMesh.WedgeTangentY.Add(biNormal);
	}

	auto colors = meshData->colors();
	size_t colorCount = colors->size() / Foundry::DataStrideColor;
	if (colorCount >= vertexCount)
	{
		for (size_t i = 0; i < indices->size(); i++)
		{
			int idx = (*meshData->indices())[i]; // get vertex ID
			FColor rgba(
				(*colors)[Foundry::DataStrideVertex * idx + 0] * 255,
				(*colors)[Foundry::DataStrideVertex * idx + 1] * 255,
				(*colors)[Foundry::DataStrideVertex * idx + 2] * 255);

			RawMesh.WedgeColors.Add(rgba);
		}
	}

	int numUVSets = 1;
	auto uvs = meshData->uvs();
	size_t uvCounts = uvs->size() / Foundry::DataStrideUV;
	size_t uvOffset = uvCounts - vertexCount;
	while (uvOffset > 0)
	{
		uvOffset -= vertexCount;
		if (++numUVSets >= MAX_MESH_TEXTURE_COORDS)
		{
			break;
		}
	}

	FVector2D zeroVector(0.0, 0.0);
	for (size_t i = 0; i < indices->size(); i++)
	{
		uint32 idx = (*meshData->indices())[i]; // get vertex ID

		// for each uv set
		for (int setId = 0; setId < numUVSets; setId++)
		{
			uint32 uvId = (idx * numUVSets + setId) * Foundry::DataStrideUV;
			if (uvId + 1 >= uvs->size())
			{
				RawMesh.WedgeTexCoords[setId].Add(zeroVector);
			}
			else
			{
				FVector2D uv((*uvs)[uvId + 0], (*uvs)[uvId + 1]);

				CoordinateSytemConverter::Get()->ConvertUVNeutralToUnreal(uv);
				RawMesh.WedgeTexCoords[setId].Add(uv);
			}
		}
	}

	FName meshName = scene->getUnrealAssetLocalIdentName(unrealMesh);

	const FString* existingPackage = meshLookUp.Find(meshName.ToString());
	FString packageName;
	if (existingPackage != nullptr)
	{
		packageName = *existingPackage;
	}
	else
	{
		packageName = MeshAssetPath + meshName.ToString();
	}

	auto StaticMesh = CreateStaticMesh(packageName, meshName);

	FStaticMeshSourceModel* SrcModel;
	// Hard coded, we always update the first source model
	if (StaticMesh->SourceModels.Num() == 0)
	{
		SrcModel = new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	}
	else
	{
		SrcModel = &StaticMesh->SourceModels[0];
	}

	if (normalCount == indices->size())
	{
		SrcModel->BuildSettings.bRecomputeNormals = false;

		if (RawMesh.WedgeTangentY.Num() == RawMesh.WedgeTangentZ.Num())
		{
			SrcModel->BuildSettings.bRecomputeTangents = false;
		}
	}

	RawMesh.CompactMaterialIndices();
	SrcModel->RawMeshBulkData->SaveRawMesh(RawMesh);

	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 64;
	StaticMesh->LightMapCoordinateIndex = 1;

	bool bSilent = false;
	TArray < FText > OutErrors;
	StaticMesh->Build(bSilent, &OutErrors);

	static int lodIndex = 0;
	auto info = StaticMesh->SectionInfoMap.Get(lodIndex, materialIndex);

	StaticMesh->StaticMaterials.Empty();
	for (int i = 0; i < unrealMesh->matNameCount(); i++)
	{

		FName matName = unrealMesh->materialName(i);
		bool loaded = LoadMaterialsFromUnreal(StaticMesh->StaticMaterials, scene, matName.ToString());

		if (!loaded)
		{
			FString packageName = MaterialAssetPath + matName.ToString();
			UnrealHelper::GetValidPackageName(packageName);
			auto material = CreateMaterial(packageName, matName);
			StaticMesh->StaticMaterials.Add(material);
		}
	}

	if (StaticMesh->StaticMaterials.Num() == 0)
	{
		StaticMesh->StaticMaterials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
	}

	info.MaterialIndex = materialIndex;
	StaticMesh->SectionInfoMap.Remove(lodIndex, materialIndex);
	StaticMesh->SectionInfoMap.Set(lodIndex, materialIndex, info);

	// this is damage control. After build, we'd like to absolutely sure that 
	// all index is pointing correctly and they're all used. Otherwise we remove them
	FMeshSectionInfoMap OldSectionInfoMap = StaticMesh->SectionInfoMap;
	StaticMesh->SectionInfoMap.Clear();

	// fix up section data
	for (int32 LODResoureceIndex = 0; LODResoureceIndex < StaticMesh->RenderData->LODResources.Num(); ++LODResoureceIndex)
	{
		FStaticMeshLODResources& LOD = StaticMesh->RenderData->LODResources[LODResoureceIndex];
		int32 NumSections = LOD.Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			FMeshSectionInfo Info = OldSectionInfoMap.Get(LODResoureceIndex, SectionIndex);
			if (StaticMesh->StaticMaterials.IsValidIndex(Info.MaterialIndex))
			{
				StaticMesh->SectionInfoMap.Set(LODResoureceIndex, SectionIndex, Info);
			}
		}
	}

	StaticMesh->UpdateUVChannelData(false);
	StaticMesh->MarkPackageDirty();
	return StaticMesh;
}

AActor* UnrealNode::instantiateLights(ALight* pLight, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix)
{
	FName lightName			= _lights[0];
	UnrealLight* lightObj	= scene->findLight(lightName);
	UnrealLight	 dummyLight;

	if (lightObj == nullptr)
	{
		lightObj = &dummyLight;
	}
	
	auto lightData = lightObj->getData();

	if (pLight == nullptr)
	{
		switch (lightData->type())
		{
		case Foundry::ELightType_eSpot:
		{
			pLight = world->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), FTransform(convertedMatrix));
		}
		break;
		case Foundry::ELightType_eDirectional:
		{
			pLight = world->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(convertedMatrix));
		}
		break;
		case Foundry::ELightType_ePoint:
		{
			pLight = world->SpawnActor<APointLight>(APointLight::StaticClass(), FTransform(convertedMatrix));
		}
		break;
		case Foundry::ELightType_eArea:
		{
			pLight = world->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), FTransform(convertedMatrix));
		}
		break;
		default:
			UE_LOG(ModoBridge, Error, TEXT("Unknown Light type has been set."));
			pLight = world->SpawnActor<APointLight>(APointLight::StaticClass(), FTransform(convertedMatrix));
			break;
		}
	}
	
	auto ambient = lightData->ambient();
	auto diffuse = lightData->diffuse();

	FLinearColor ambientColor(ambient->x(), ambient->y(), ambient->z());
	FLinearColor diffuseColor(diffuse->x(), diffuse->y(), diffuse->z());

	ULightComponent* lightComponent = pLight->GetLightComponent();
	lightComponent->SetAffectDynamicIndirectLighting(false);
	lightComponent->SetAffectTranslucentLighting(false);
	lightComponent->SetCastShadows(false);
	lightComponent->SetIntensity(lightData->intensity());
	lightComponent->SetLightColor(diffuseColor);
	scene->registerItem(TCHAR_TO_UTF8(*this->name().ToString()), UnrealHelper::GetLocalUniqueName(pLight));

	pLight->SetActorTransform(FTransform(convertedMatrix));
	AdjustLightNeutralToUnreal(pLight);

	return pLight;
}

AActor* UnrealNode::instantiateAActors(AActor* pActor, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix)
{
	if (pActor == nullptr)
	{
		pActor = world->SpawnActor<AActor>(AActor::StaticClass(), FTransform(convertedMatrix));
		USceneComponent* RootComponent = NewObject<USceneComponent>(pActor, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
		RootComponent->Mobility = EComponentMobility::Static;

		pActor->AddInstanceComponent(RootComponent);
		RootComponent->RegisterComponent();

		pActor->Modify();
		pActor->SetRootComponent(RootComponent);
		
		scene->registerItem(TCHAR_TO_UTF8(*this->name().ToString()), UnrealHelper::GetLocalUniqueName(pActor));
	}
	else
	{
		pActor->SetActorTransform(FTransform(convertedMatrix));
	}

	return pActor;
}

AActor* UnrealNode::instantiateCameras(ACameraActor* pCameraActor, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix)
{
	if (pCameraActor == nullptr)
	{
		pCameraActor = world->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(convertedMatrix));
		scene->registerItem(TCHAR_TO_UTF8(*this->name().ToString()), UnrealHelper::GetLocalUniqueName(pCameraActor));
	}
	else
	{
		pCameraActor->SetActorTransform(FTransform(convertedMatrix));
	}

	FName cameraName = _cameras[0];
	UnrealCamera*	camObj = scene->findCamera(cameraName);

	if (camObj)
	{
		AdjustCameraNeutralToUnreal(pCameraActor);
		UCameraComponent* camera = pCameraActor->GetCameraComponent();

		auto cameraData = camObj->getData();
		camera->SetFieldOfView(cameraData->horizontalFOV());
		camera->SetAspectRatio(cameraData->aspect());
		camera->SetOrthoNearClipPlane(cameraData->clipPlaneNear());
		camera->SetOrthoFarClipPlane(cameraData->clipPlaneFar());
	}

	return pCameraActor;
}

AActor* UnrealNode::instantiateMeshes(AStaticMeshActor* pStaticMeshActor, UWorld* world, ModoBridgeHandler* scene, const FMatrix& convertedMatrix)
{
	if (pStaticMeshActor == nullptr)
	{
		pStaticMeshActor = world->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(convertedMatrix));
		scene->registerItem(TCHAR_TO_UTF8(*this->name().ToString()), UnrealHelper::GetLocalUniqueName(pStaticMeshActor));
	}
	else
	{
		pStaticMeshActor->SetActorTransform(FTransform(convertedMatrix));
	}

	FName meshName = _meshes[0];
	UnrealMesh* mesh = scene->findMesh(meshName);

	if (mesh == nullptr)
	{
		return pStaticMeshActor;
	}

	if (mesh->getData()->vertices()->size() != 0)
	{
		auto staticMesh = buildMeshes(mesh, scene, scene->getSubFolderMeshLookup());

		// We add or modify modo user data for this static mesh asset.
		UModoBridgeMeshAssetData* modoUserData = nullptr;
		auto assetUserDataArray = staticMesh->GetAssetUserDataArray();
		if (assetUserDataArray != nullptr)
		{
			for (auto assetUserData : *assetUserDataArray)
			{
				modoUserData = Cast<UModoBridgeMeshAssetData>(assetUserData);
				if (modoUserData != nullptr)
				{
					break;
				}
			}
		}
		// Add a new one if not find
		if (modoUserData == nullptr)
		{
			modoUserData = NewObject<UModoBridgeMeshAssetData>(staticMesh, UModoBridgeMeshAssetData::StaticClass());
			staticMesh->AddAssetUserData(modoUserData);
		}
		// Add the reset mesh asset value, so we can revert back before sending back to Modo
		modoUserData->AssetReset = CoordinateSytemConverter::Get()->ResetMeshAsset();
		scene->registerAsset(TCHAR_TO_UTF8(*meshName.ToString()), UnrealHelper::GetLocalUniqueName(staticMesh));
		UStaticMeshComponent *component = pStaticMeshActor->GetStaticMeshComponent();
		component->SetStaticMesh(staticMesh);
	}

	return pStaticMeshActor;
}