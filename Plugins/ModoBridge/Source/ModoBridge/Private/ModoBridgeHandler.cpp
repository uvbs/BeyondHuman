// Copyright Foundry, 2017. All rights reserved.

#include "ModoBridgeHandler.h"
#include <cassert>

#include "Editor.h"
#include "EngineGlobals.h"
#include "SceneViewport.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Light.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraActor.h"
#include "ARFilter.h"
#include "Misc/ScopedSlowTask.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "FileHelpers.h"
#include "AssetRegistryModule.h"
#endif

#include "ModoBridge.h"
#include "common/CommonTypes.h"
#include "UnrealNode.h"
#include "UnrealMesh.h"
#include "UnrealLight.h"
#include "UnrealCamera.h"
#include "UnrealHelper.h"
#include "UnrealTexture.h"
#include "UnrealMaterial.h"
#include "UnrealConfig.h"
#include "UnrealHelper.h"

#define LOCTEXT_NAMESPACE "ModoBridge"

static TMap<FString, ModoBridgeHandler*> bridgeHandlers;
static ModoBridgeHandler*	bridgeHandler = nullptr;

ModoBridgeHandler* ModoBridgeHandler::activeInstance()
{
	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (DesiredLevel != nullptr)
	{
		FString levelName = DesiredLevel->GetOutermost()->GetFName().GetPlainNameString();
		ModoBridgeHandler** ptr = bridgeHandlers.Find(levelName);
		if (ptr == nullptr)
		{
			ModoBridgeHandler* val = new ModoBridgeHandler();
			bridgeHandlers.Emplace(levelName, val);
			return val;
		}
		else
		{
			return *ptr;
		}
	}

	if (bridgeHandler == nullptr)
	{
		bridgeHandler = new ModoBridgeHandler();
	}

	return bridgeHandler;
}

void ModoBridgeHandler::destroyInstances()
{
	for (auto& elem : bridgeHandlers)
	{
		if (elem.Value != nullptr)
		{
			delete elem.Value;
			elem.Value = nullptr;
		}
	}

	if (bridgeHandler != nullptr)
	{
		delete bridgeHandler;
		bridgeHandler = nullptr;
	}
}

// Sets default values
ModoBridgeHandler::ModoBridgeHandler() :
	_nodes(),
	_meshes(),
	_lights(),
	_cameras(),
	_itemLocalGlobalNameMapper(),
	_itemGlobalLocalNameMapper(),
	_assetLocalGlobalNameMapper(),
	_assetGlobalLocalNameMapper(),
	_currentWorld(nullptr),
	_currentConfig(nullptr),
	_subFolderMeshLookup(),
	_subFolderMaterialLookup(),
	_subFolderTextureLookup()
{

}

ModoBridgeHandler::~ModoBridgeHandler()
{

	clearScene();
	PeerController::destroyInstance();
}


void ModoBridgeHandler::setWorld(UWorld* world)
{
	_currentWorld = world;
}

void ModoBridgeHandler::disableServer()
{
	PeerController::activeInstance()->shutDown();
}

bool ModoBridgeHandler::isServerEnabled()
{
	auto client = PeerController::activeInstance();
	return client->isRunning();
}

void ModoBridgeHandler::enableServer(FString address, int32 port)
{
	auto client = PeerController::activeInstance();
	if (client->isRunning() && address == _serverAddress && port == _serverPort)
		return;
	
	client->shutDown();
	client->startUp(this, TCHAR_TO_ANSI(*address), (uint16)port);

#ifdef _WIN32
	client->setUEWindow(GetActiveWindow());
#endif

	address = _serverAddress;
	port = _serverPort;
}

void ModoBridgeHandler::pushAll()
{
	auto client = PeerController::activeInstance();
	client->pushAll();
}

void ModoBridgeHandler::pushSelected()
{
	auto client = PeerController::activeInstance();
	client->pushSelected();
}

UnrealMesh* ModoBridgeHandler::findMesh(const FName& name) const
{
	if (_meshes.Contains(name))
	{
		return _meshes[name];
	}
	return nullptr;
}

UnrealCamera* ModoBridgeHandler::findCamera(const FName& name) const
{
	if (_cameras.Contains(name))
	{
		return _cameras[name];
	}
	return nullptr;
}

UnrealTexture* ModoBridgeHandler::findTexture(const FName& name) const
{
	if (_textures.Contains(name))
	{
		return _textures[name];
	}
	return nullptr;
}

UnrealMaterial* ModoBridgeHandler::findMaterial(const FName& name) const
{
	if (_materials.Contains(name))
	{
		return _materials[name];
	}
	return nullptr;
}

UnrealLight* ModoBridgeHandler::findLight(const FName& name) const
{
	if (_lights.Contains(name))
	{
		return _lights[name];
	}

	return nullptr;
}

void ModoBridgeHandler::parseNode(const uint8* msgData)
{
	UnrealNode* node = new UnrealNode();
	node->parse(msgData);
	_nodes.Emplace(node->name(), node);
}

void ModoBridgeHandler::parseTexture(const uint8* msgData)
{
	UnrealTexture* texture = new UnrealTexture();
	texture->parse(msgData);
	_textures.Emplace(texture->name(), texture);
}

void ModoBridgeHandler::parseObject(const uint8* msgData)
{
	UnrealMesh* mesh = new UnrealMesh();
	mesh->parse(msgData);
	_meshes.Emplace(mesh->name(), mesh);
}

void ModoBridgeHandler::parseCamera(const uint8* msgData)
{
	UnrealCamera* camera = new UnrealCamera();
	camera->parse(msgData);
	_cameras.Emplace(camera->name(), camera);
}

void ModoBridgeHandler::parseLight(const uint8* msgData)
{
	UnrealLight* light = new UnrealLight();
	light->parse(msgData);
	_lights.Emplace(light->name(), light);
}

void ModoBridgeHandler::parseMaterial(const uint8* msgData)
{
	UnrealMaterial* material = new UnrealMaterial();
	material->parse(msgData);
	_materials.Emplace(material->name(), material);
}

void ModoBridgeHandler::parseConfig(const uint8* msgData)
{
	if (_currentConfig)
		delete _currentConfig;

	_currentConfig = new UnrealConfig();
	_currentConfig->parse(msgData);
	CoordinateSytemConverter::Set(_currentConfig->resetMeshAsset);
}

// Cache all asset packages into lookup table, deeper path will be overwritten
void BuildAssetLookups(const FString& path, TMap<FString, FString >& lookUp)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.bRecursivePaths = true;

	FString PackagePath = FPackageName::FilenameToLongPackageName(path);
	if (PackagePath.Len() > 1 && PackagePath[PackagePath.Len() - 1] == TEXT('/'))
	{
		// The filter path can't end with a trailing slash
		PackagePath = PackagePath.LeftChop(1);
	}
	Filter.PackagePaths.Emplace(*PackagePath);

	TArray<FAssetData> selectedAssetList;
	AssetRegistryModule.Get().GetAssets(Filter, selectedAssetList);

	lookUp.Empty();
	for (int i = 0; i < selectedAssetList.Num(); i++)
	{
		const FAssetData* adPtr = &selectedAssetList[i];
		FString assetName = adPtr->AssetName.ToString();
		FString packageName = adPtr->PackageName.ToString();
		FString& assetPath = lookUp.FindOrAdd(assetName);

		if (assetPath.IsEmpty() || UnrealHelper::isDeeper(assetPath, packageName))
		{
			assetPath = packageName;
		}
	}
}

void ModoBridgeHandler::updateSubFolderLookup()
{
	BuildAssetLookups(MeshAssetPath, _subFolderMeshLookup);
	BuildAssetLookups(MaterialAssetPath, _subFolderMaterialLookup);
	BuildAssetLookups(TextureAssetPath, _subFolderTextureLookup);
}

void ModoBridgeHandler::InstantiateScene(bool meshMode)
{
	UE_LOG(ModoBridge, Log, TEXT("Creating scene in Unreal, handling %d nodes."), _nodes.Num());

	ULevel* DesiredLevel = GWorld->GetCurrentLevel();
	if (DesiredLevel == nullptr)
	{
		return;
	}

	_currentWorld = DesiredLevel->OwningWorld;

	if (_currentWorld == nullptr)
	{
		return;
	}

	updateSharedRegistry();

	if (_textures.Num() > 0)
	{
		FScopedSlowTask SlowTask(_textures.Num(), LOCTEXT("BuildingModoTextures", "Building Textures"));
		SlowTask.MakeDialog();

		for (auto& elem : _textures)
		{
			elem.Value->instantiate(_currentWorld, _subFolderTextureLookup);
			SlowTask.EnterProgressFrame();
		}
	}

	if (_materials.Num() > 0)
	{
		FScopedSlowTask SlowTask(_materials.Num(), LOCTEXT("BuildingModoMaterials", "Building Materials"));
		SlowTask.MakeDialog();

		bool recursive = false;

		if (_currentConfig)
		{
			const auto* configData = _currentConfig->getData();
			if (configData && configData->useSubFolder())
			{
				recursive = true;
			}
		}
		UnrealMaterial::validateTextureAssets(recursive);

		for (auto& elem : _materials)
		{
			elem.Value->instantiate(_currentWorld, _subFolderMaterialLookup);
			SlowTask.EnterProgressFrame();
		}
	}

	uint32 rootCount = 0;
	TSet<FName> notRoot;
	for (auto& elem : _nodes)
	{
		for (uint32 i = 0; i < elem.Value->numChildren(); ++i)
		{
			notRoot.Emplace(elem.Value->child(i));
		}
	}

	UWorld* world = _currentWorld;
	for (auto& elem : _nodes)
	{
		if (!notRoot.Contains(elem.Key))
		{
			// Build each "root" node, instantiating children
			// with combined transform matrices.
			FMatrix transform = FMatrix::Identity;
			buildScenegraph(elem.Value, transform, nullptr);
			rootCount++;
		}
	}

	UE_LOG(ModoBridge, Log, TEXT("Scene Instantiated, there were %d root nodes/scenegraphs."), _nodes.Num() - notRoot.Num());
	GEditor->GetActiveViewport()->Invalidate();
}

void ModoBridgeHandler::updateSharedRegistry()
{
	std::set<std::string> actorSet;
	std::set<std::string> assetSet;
	for (TActorIterator<AStaticMeshActor> ActorItr(getWorld()); ActorItr; ++ActorItr)
	{
		AStaticMeshActor *Mesh = *ActorItr;
		{
			auto uniqueName = UnrealHelper::GetLocalUniqueName(Mesh);
			registerItem(uniqueName);
			actorSet.insert(uniqueName);
		}
		{
			UStaticMesh* staticMeshPtr = Mesh->GetStaticMeshComponent()->GetStaticMesh();
			if (staticMeshPtr != nullptr)
			{
				auto uniqueName = UnrealHelper::GetLocalUniqueName(staticMeshPtr);
				registerAsset(uniqueName);
				assetSet.insert(uniqueName);
			}
		}
	}

	cleanUpRegistedAssets(assetSet);
	cleanUpRegistedItems(actorSet);
}


void ModoBridgeHandler::getNeutralActorLocalIdentName(const UnrealNode* node, std::string& name)
{
	bool useUniqueName = true;

	if (_currentConfig)
	{
		auto cfgData = _currentConfig->getData();
		useUniqueName = cfgData->actorUniqueNameIdent();
	}

	if (!useUniqueName)
	{
		name = TCHAR_TO_UTF8(*node->displayName());
	}
	else
	{
		name = _itemGlobalLocalNameMapper[TCHAR_TO_UTF8(*node->name().ToString())];
	}
}

void ModoBridgeHandler::getUnrealActorLocalIdentName(const AActor* actor, std::string& name)
{
	bool useUniqueName = true;

	if (_currentConfig)
	{
		auto cfgData = _currentConfig->getData();
		useUniqueName = cfgData->actorUniqueNameIdent();
	}

	if (!useUniqueName)
	{
		name = TCHAR_TO_UTF8(*actor->GetActorLabel());
	}
	else
	{
		name = UnrealHelper::GetLocalUniqueName(actor);
	}
}
FName ModoBridgeHandler::getUnrealAssetLocalIdentName(const UnrealMesh* mesh)
{
	uint32 useUniqueName = true;

	if (_currentConfig)
	{
		auto cfgData = _currentConfig->getData();
		useUniqueName = cfgData->assetUniqueNameIdent();
	}

	if (!useUniqueName)
	{
		return FName(*mesh->displayName());
	}

	return mesh->name();
}

FString ModoBridgeHandler::getAssetPathForCreation(const FString& defaultPath, const FString& name)
{
	bool searchSubFolder = false;

	if (_currentConfig)
	{
		auto cfgData = _currentConfig->getData();
		searchSubFolder = cfgData->useSubFolder();
	}

	if (searchSubFolder) 
	{
		
	}

	return defaultPath + name;

}
AActor* ModoBridgeHandler::buildScenegraph(UnrealNode* pNode, const FMatrix& parentTransform, AActor* parentItem)
{
	assert(pNode);

	AActor* actor = nullptr;
	bool hasEntity = false;
	FMatrix localMatrix;
	memcpy(localMatrix.M, pNode->transformation(), sizeof(float) * 16);

	FMatrix worldTransform = parentTransform * localMatrix;
	FMatrix convertedMatrix = CoordinateSytemConverter::Get()->ConvertTransfromNeutralToUnreal(worldTransform).GetTransposed();

	if (pNode->numMeshes())
	{
		AStaticMeshActor* pStaticMeshActor = nullptr;

		// [TODO] Refactor this to say that get item in the scene (one for unreal one for modo)
		std::string localName = "";
		getNeutralActorLocalIdentName(pNode, localName);

		if (localName != "")
		{
			for (TActorIterator<AStaticMeshActor> ActorItr(_currentWorld); ActorItr; ++ActorItr)
			{
				// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
				AStaticMeshActor *Mesh = *ActorItr;
				std::string stdName = "";
				getUnrealActorLocalIdentName(Mesh, stdName);
				if (stdName == localName)
				{
					pStaticMeshActor = Mesh;
					break;
				}
			}
		}
		hasEntity = true;
		actor = pNode->instantiateMeshes(pStaticMeshActor, _currentWorld, this, convertedMatrix);
	}

	if (pNode->numLights())
	{
		ALight* plight = nullptr;
		std::string localName = "";
		getNeutralActorLocalIdentName(pNode, localName);
		if (localName != "")
		{
			for (TActorIterator<ALight> ActorItr(_currentWorld); ActorItr; ++ActorItr)
			{
				// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
				ALight *Light = *ActorItr;
				std::string stdName = "";
				getUnrealActorLocalIdentName(Light, stdName);
				if (stdName == localName)
				{
					plight = Light;
					break;
				}
			}
		}
		hasEntity = true;
		actor = pNode->instantiateLights(plight, _currentWorld, this, convertedMatrix);
	}

	if (pNode->numCameras())
	{
		ACameraActor* pCameraActor = nullptr;
		// [TODO] Refactor this to say that get item in the scene (one for unreal one for modo)
		std::string localName = "";
		getNeutralActorLocalIdentName(pNode, localName);

		if (localName != "")
		{
			for (TActorIterator<ACameraActor> ActorItr(_currentWorld); ActorItr; ++ActorItr)
			{
				// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
				ACameraActor *Camera = *ActorItr;
				std::string stdName = "";
				getUnrealActorLocalIdentName(Camera, stdName);
				if (stdName == localName)
				{
					pCameraActor = Camera;
					break;
				}
			}
		}
		hasEntity = true;
		actor = pNode->instantiateCameras(pCameraActor, _currentWorld, this, convertedMatrix);
	}
	
	// We assume, the others are AActors !?
	if (!hasEntity)
	{
		AActor* pDummyActor = nullptr;
		// [TODO] Refactor this to say that get item in the scene (one for unreal one for modo)
		std::string localName = "";
		getNeutralActorLocalIdentName(pNode, localName);

		if (localName != "")
		{
			for (TActorIterator<AActor> ActorItr(_currentWorld); ActorItr; ++ActorItr)
			{
				AActor *dummy = *ActorItr;
				if (	dummy->GetClass() == AStaticMeshActor::StaticClass()
					||	dummy->GetClass() == ALight::StaticClass()
					||	dummy->GetClass() == ACameraActor::StaticClass())
				{
					continue;
				}

				// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
				std::string stdName = "";
				getUnrealActorLocalIdentName(dummy, stdName);
				if (stdName == localName)
				{
					pDummyActor = dummy;
					break;
				}
			}
		}

		actor = pNode->instantiateAActors(pDummyActor, _currentWorld, this, convertedMatrix);
	}

	if (actor)
	{
		actor->SetActorLabel(pNode->displayName());
		if (parentItem != nullptr)
		{
			actor->AttachToActor(parentItem, FAttachmentTransformRules::KeepWorldTransform);
		}
		else
		{
			actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
	
	if (pNode->numChildren())
	{
		for (uint32 i = 0; i < pNode->numChildren(); ++i)
		{
			FName childName = pNode->child(i);
			UnrealNode* child = _nodes.FindRef(childName);
			if (child != NULL) 
			{
				AActor* childActor = buildScenegraph(child, worldTransform, actor);

				if (childActor != nullptr)
				{
					childActor->AttachToActor(actor, FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
	}

	return actor;
}

uint32 ModoBridgeHandler::getTransmittedCount(const uint8*& msgData, const uint8* msgEnd)
{
	assert(msgData + sizeof(uint32) < msgEnd);
	uint32 count = 0;
	memcpy(&count, msgData, sizeof(uint32));
	msgData += sizeof(uint32);
	return count;
}


void ModoBridgeHandler::retrieveScene()
{

}

UWorld* ModoBridgeHandler::getWorld()
{
	return _currentWorld;
}

void ModoBridgeHandler::flushClearScene()
{

}

void ModoBridgeHandler::clearScene()
{
	for (auto& elem : _meshes)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_meshes.Empty();

	for (auto& elem : _lights)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_lights.Empty();

	for (auto& elem : _cameras)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_cameras.Empty();

	for (auto& elem : _nodes)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_nodes.Empty();

	for (auto& elem : _textures)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_textures.Empty();

	for (auto& elem : _materials)
	{
		delete elem.Value;
		elem.Value = nullptr;
	}
	_materials.Empty();

	if (_currentConfig)
	{
		delete _currentConfig;
		_currentConfig = nullptr;
	}
}

#if 0
const FString* ModoBridgeHandler::findMaterialFullName(const std::string& key)
{
	return _materialMapper.Find(key);
}
void ModoBridgeHandler::registerMaterialFullName(const std::string& key, const FString& fullname)
{
	_materialMapper[key] = fullname;
}

void ModoBridgeHandler::registerMaterialsToOrigin(UStaticMesh* mesh, std::string name)
{
	auto staticMaterials = mesh->StaticMaterials;
	TArray<UMaterial*> usedMaterials;

	for (FStaticMaterial& staticMat : staticMaterials)
	{
		UMaterial* mat = staticMat.MaterialInterface->GetMaterial();
		usedMaterials.Add(mat);
	}
	_protoMaterialMapper[name] = usedMaterials;
}

const TArray<UMaterial*>* ModoBridgeHandler::retreiveMaterialsFromOrigin(std::string name)
{
	return _protoMaterialMapper.Find(name);
}
#endif

void ModoBridgeHandler::registerItem(std::string globalName, std::string localName)
{
	_itemLocalGlobalNameMapper[localName] = globalName;
	_itemGlobalLocalNameMapper[globalName] = localName;
}

void ModoBridgeHandler::registerAsset(std::string globalName, std::string localName)
{
	_assetLocalGlobalNameMapper[localName] = globalName;
	_assetGlobalLocalNameMapper[globalName] = localName;
}

void ModoBridgeHandler::registerItem(std::string localName)
{
	std::string globalName = _itemLocalGlobalNameMapper[localName];
	if (globalName == "")
	{
		registerItem(localName, localName);
	}
}

void ModoBridgeHandler::registerAsset(std::string localName)
{
	std::string globalName = _assetLocalGlobalNameMapper[localName];
	if (globalName == "")
	{
		registerAsset(localName, localName);
	}
}

void ModoBridgeHandler::cleanUpRegistedAssets(std::set<std::string>& validAssets)
{
	for (auto it = _assetLocalGlobalNameMapper.begin(); it != _assetLocalGlobalNameMapper.end();)
	{
		auto& localName = it->first;
		auto& globalName = it->second;

		if (validAssets.find(localName) == validAssets.end())
		{
			_assetGlobalLocalNameMapper.erase(globalName);
			it = _assetLocalGlobalNameMapper.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void ModoBridgeHandler::cleanUpRegistedItems(std::set<std::string>& validItems)
{
	for (auto it = _itemLocalGlobalNameMapper.begin(); it != _itemLocalGlobalNameMapper.end();)
	{
		auto& localName = it->first;
		auto& globalName = it->second;

		if (validItems.find(localName) == validItems.end())
		{
			_itemGlobalLocalNameMapper.erase(globalName);
			it = _itemLocalGlobalNameMapper.erase(it);
		}
		else
		{
			it++;
		}
	}
}

bool ModoBridgeHandler::itemGlobalExists(std::string globalName)
{
	auto it = _itemGlobalLocalNameMapper.find(globalName);
	return it != _itemGlobalLocalNameMapper.end();
}

bool ModoBridgeHandler::itemLocalExists(std::string localName)
{
	auto it = _itemLocalGlobalNameMapper.find(localName);
	return it != _itemLocalGlobalNameMapper.end();
}

bool ModoBridgeHandler::assetGlobalExists(std::string globalName)
{
	auto it = _assetGlobalLocalNameMapper.find(globalName);
	return it != _assetGlobalLocalNameMapper.end();
}

bool ModoBridgeHandler::assetLocalExists(std::string localName)
{
	auto it = _assetLocalGlobalNameMapper.find(localName);
	return it != _assetLocalGlobalNameMapper.end();
}

std::string ModoBridgeHandler::assetGlobalName(std::string localName)
{
	auto it = _assetLocalGlobalNameMapper.find(localName);
	if (it != _assetLocalGlobalNameMapper.end())
	{
		return it->second;
	}
	else
	{
		return "";
	}
}

std::string ModoBridgeHandler::assetLocalName(std::string globalName)
{
	auto it = _assetGlobalLocalNameMapper.find(globalName);
	if (it != _assetGlobalLocalNameMapper.end())
	{
		return it->second;
	}
	else
	{
		return "";
	}
}

std::string ModoBridgeHandler::itemGlobalName(std::string localName)
{
	auto it = _itemLocalGlobalNameMapper.find(localName);
	if (it != _itemLocalGlobalNameMapper.end())
	{
		return it->second;
	}
	else
	{
		return "";
	}
}

std::string ModoBridgeHandler::itemLocalName(std::string globalName)
{
	auto it = _itemGlobalLocalNameMapper.find(globalName);
	if (it != _itemGlobalLocalNameMapper.end())
	{
		return it->second;
	}
	else
	{
		return "";
	}
}

void ModoBridgeHandler::updateScene()
{
	flushClearScene();

	if (_currentConfig)
	{
		const auto* configData = _currentConfig->getData();

		_subFolderMeshLookup.Empty();
		_subFolderMaterialLookup.Empty();
		_subFolderTextureLookup.Empty();

		if (configData && configData->useSubFolder())
		{
			updateSubFolderLookup();
		}
	}

	InstantiateScene(true);
}

const TMap<FString, FString >&	ModoBridgeHandler::getSubFolderMeshLookup()
{
	return _subFolderMeshLookup;
}

const TMap<FString, FString >&	ModoBridgeHandler::getSubFolderMaterialLookup()
{
	return _subFolderMaterialLookup;
}

const TMap<FString, FString >&	ModoBridgeHandler::getSubFolderTextureLookup()
{
	return _subFolderTextureLookup;
}

#undef LOCTEXT_NAMESPACE