// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <string>
#include <map>

#include "common/CommonTypes.h"
#include "Engine/StaticMeshActor.h"

class UnrealNode;
class UnrealMesh;
class UnrealLight;
class UnrealCamera;
class UnrealTexture;
class UnrealMaterial;
class UnrealConfig;

// Main class for the ModoBridge, it controls the current shared scene
// [TODO] This class needs to be decoupled to SharedScene + Action Controller
// We only support sharing one scene for the moment, so it may be bugged if sharing more.
class ModoBridgeHandler
{
public:
	// Get current instance of the handler
	static ModoBridgeHandler*		activeInstance();

	// Destroy the instance
	static void						destroyInstances();

	// Start the server
	void				enableServer(FString address, int32 port);

	// Check if the server is enabled
	bool				isServerEnabled();

	// Shutdown the server
	void				disableServer();

	// Set/Change current World
	void				setWorld(UWorld* world);

	// Action Push All
	void				pushAll();

	// Action Push Selected
	void				pushSelected();

	// Return current World
	UWorld*				getWorld();

	// Find the Unreal mesh from what has been received
	UnrealMesh*			findMesh(const FName& name) const;

	// Find the Unreal camera from what has been received
	UnrealCamera* 		findCamera(const FName& name) const;

	// Find the Unreal light from what has been received
	UnrealLight*		findLight(const FName& name) const;

	// Find the Unreal texture from what has been received
	UnrealTexture*		findTexture(const FName& name) const;

	// Find the Unreal material from what has been received
	UnrealMaterial*		findMaterial(const FName& name) const;

	// Create a static mesh actor from the current UnrealNode
	AStaticMeshActor*	instantiateMeshes(UnrealNode pNode, UWorld* world, const FMatrix& convertedMatrix);

	// Update registry that is used by sharing the neutral scene
	void				updateSharedRegistry();

	// Register the actor into shared scene, so we can find it next time from local name or global name
	void 	registerItem(std::string globalName, std::string localName);

	// Register the actor into shared scene, make local name as global name (for Unreal created actors)
	void	registerItem(std::string localName);

	// Register the asset into shared scene, so we can find it next time from local name or global name
	void 	registerAsset(std::string globalName, std::string localName);

	// Register the asset into shared scene, make local name as global name (for Unreal created assets)
	void	registerAsset(std::string localName);

	// Check if an actor with such as globalName exists in the shared scene (registry)
	bool 	itemGlobalExists(std::string globalName);

	// Check if an actor with such as localName exists in the shared scene (registry)
	bool 	itemLocalExists(std::string localName);

	// Check if an asset with such as globalName exists in the shared scene (registry)
	bool 	assetGlobalExists(std::string globalName);

	// Check if an asset with such as localName exists in the shared scene (registry)
	bool 	assetLocalExists(std::string localName);

	// Get actor global name from local name
	std::string itemGlobalName(std::string localName);

	// Get actor local name from global name
	std::string itemLocalName(std::string globalName);

	// Get asset global name from local name
	std::string assetGlobalName(std::string localName);

	// Get asset local name from global name
	std::string assetLocalName(std::string globalName);
	
	// Update unreal scene according to what has been received
	void	updateScene();

	// Clear what has been received and get config
	// Different config for one push is illegal
	void	clearScene();

	// Parse node from message data
	void	parseNode(const uint8* msgData);

	// Parse texture from message data
	void	parseTexture(const uint8* msgData);

	// Parse mesh from message data
	void	parseObject(const uint8* msgData);

	// Parse camera from message data
	void	parseCamera(const uint8* msgData);

	// Parse light from message data
	void	parseLight(const uint8* msgData);

	// Parse material from message data
	void	parseMaterial(const uint8* msgData);

	// Parse material from config data
	void	parseConfig(const uint8* msgData);

	// Update lookup table for searching assets into subfolders
	void	updateSubFolderLookup();

	// Convert an actor to neutral Node
	Foundry::NodePtr Convert2NeutralNode(AActor* actor, const FString& name, const FString& display);

	// Get Local ID from a neutral actor
	void getNeutralActorLocalIdentName(const UnrealNode* node, std::string& name);
	
	// Get Local ID from a unreal actor
	void getUnrealActorLocalIdentName(const AActor* actor, std::string& name);
	
	// Get local ID from a unreal asset
	FName getUnrealAssetLocalIdentName(const UnrealMesh* mesh);

	// Get correct asset path for creation
	FString getAssetPathForCreation(const FString& defaultPath, const FString& name);

	// Get lookup tables for searching mesh assets in sub folders
	const TMap<FString, FString >&	getSubFolderMeshLookup();

	// Get lookup tables for searching material assets in sub folders
	const TMap<FString, FString >&	getSubFolderMaterialLookup();

	// Get lookup tables for searching texture assets in sub folders
	const TMap<FString, FString >&	getSubFolderTextureLookup();

private:
	// Sets default values for this actor's properties
	ModoBridgeHandler();
	~ModoBridgeHandler();

	void	cleanUpRegistedAssets(std::set<std::string>& validAssets);
	void	cleanUpRegistedItems(std::set<std::string>& validItems);

	void	InstantiateScene(bool meshMode);
	AActor*	buildScenegraph(UnrealNode* pNode, const FMatrix& parentTransform, AActor* parent);
	uint32	getTransmittedCount(const uint8*& msgData, const uint8* msgEnd);

	void	retrieveScene();
	void	flushClearScene();

	TMap<FName, UnrealNode*>			_nodes;
	TMap<FName, UnrealMesh*>			_meshes;
	TMap<FName, UnrealLight*>			_lights;
	TMap<FName, UnrealCamera*>			_cameras;
	TMap<FName, UnrealTexture*>			_textures;
	TMap<FName, UnrealMaterial*>		_materials;

	std::map<std::string, std::string>	_itemLocalGlobalNameMapper;
	std::map<std::string, std::string>	_itemGlobalLocalNameMapper;
	std::map<std::string, std::string>	_assetLocalGlobalNameMapper;
	std::map<std::string, std::string>	_assetGlobalLocalNameMapper;

	// Server State
	FString								_serverAddress = "127.0.0.1";
	int32 								_serverPort = 12000;
	UWorld*								_currentWorld;
	UnrealConfig*						_currentConfig;

	TMap<FString, FString >				_subFolderMeshLookup;
	TMap<FString, FString >				_subFolderMaterialLookup;
	TMap<FString, FString >				_subFolderTextureLookup;

	enum EChangeType
	{
		eAddition, eUpdate, eDeletion
	};
	
	enum EChangeEntity
	{
		eNodeUpdate, eMeshUpdate, eTextureUpdate, eCameraUpdate, eLightUpdate, eMaterialUpdate
	};

	enum EInvalidateType
	{
		eMeshInvalidate, eActorInvalidate
	};
};
