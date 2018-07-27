// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "StaticMeshResources.h"
#include "CommonTypes.h"

// Convert between two bridged applications: Modo - Unreal
// The constructor is for selecting different ways of conversion.
// resetMeshAsset = false: legacy conversion that, swap axis in mesh asset and rotate mesh actor.
// resetMeshAsset = true: set mesh asset in Unreal coordinate. All transformations are converted
class CoordinateSytemConverter
{
public:
	// Get current converter, which has been set before doing the workflow
	static CoordinateSytemConverter* Get();
	static void Set(bool resetMeshAsset);

	// Convert Matrix to MODO system
	FMatrix ConvertTransfromUnrealToNeutral(FMatrix in_matrix, bool assetReset);
	// Convert Matrix to Unreal system
	FMatrix ConvertTransfromNeutralToUnreal(FMatrix in_matrix);
	// Convert UV from Unreal system to Neutral scene system
	void ConvertUVUnrealToNeutral(FVector2D &vector);
	// Convert UV from Neutral scene system to Unreal system
	void ConvertUVNeutralToUnreal(FVector2D &vector);
	// Convert Position from Unreal system to Neutral scene system
	void ConvertPositionUnrealToNeutral(FVector &vector, bool assetReset);
	// Convert Position from Neutral scene system to Unreal system
	void ConvertPositionNeutralToUnreal(FVector &vector);
	// Convert Normal from Unreal system to Neutral scene system
	void ConvertNormalUnrealToNeutral(FVector &vector, bool assetReset);
	// Convert Normal from Neutral scene system to Unreal system
	void ConvertNormalNeutralToUnreal(FVector &vector);
	// Return if resetMeshAsset is enabled for creation Mesh Asset
	bool ResetMeshAsset();
	// Check if the static mesh is created with "ResetMeshAsset"
	bool ResetMeshAsset(UStaticMesh *mesh);

private:
	CoordinateSytemConverter();

	// This var is current and only used when converting from Neutral to Unreal.
	// Once we convert back from Unreal to Neutral, the action must be paired.
	// For example, if reset is used when Neutral to Unreal, then we have to reset back
	// when Unreal to Neutral
	bool		_resetMeshAsset;
};

class UnrealHelper
{
public:
	// Get the unqiue name of Obj in the local system(Unreal)
	static std::string GetLocalUniqueName(const UObject* obj);

	// Helper functions for baking meshes
	static void DetermineUVsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex);
	static void DetermineVertsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh);

	// Get the global unique name of a UMaterial
	static FString GetMaterialGlobalName(const UMaterial* mat);

	// Helper functions for getting valide package name
	static bool GetValidPackageName(FString& packageName);
	static bool RemoveInvalidCharacters(FString& name);
	static bool RemoveMaterialSlotSuffix(FString& name);
	
	// Helper function for correcting invalid path
	static bool CorrectInvalidPath(FString& name);

	// Helper functions for add prefix for material and texture names
	static bool UnrealMaterialNameConvention(FString& name);
	static bool UnrealTextureNameConvention(FString& name);

	// Check if the left path is deeper than right path
	static bool isDeeper(const FString& left, const FString& right);

	// Create Object is better than NewObject for the following purpose
	// 1, it detects Object Redirectors and try to update the "source" Object
	// 2, it is assert crash free, updating an exisiting object with different type causes problems
	template<typename T>
	static UObject* CreateObject(UPackage* assetPackage, FName objName, EObjectFlags Flags)
	{
		UPackage* newAssetPackage = assetPackage;
		UObject* Obj = StaticFindObjectFast( /*Class=*/ NULL, assetPackage, objName, true);

		while (Obj && Obj->GetClass()->IsChildOf(UObjectRedirector::StaticClass()))
		{
			UObjectRedirector *objRedirector = Cast<UObjectRedirector>(Obj);
			if (objRedirector)
			{
				Obj = objRedirector->DestinationObject;
				if (Obj->GetOuter()->GetClass()->IsChildOf(UPackage::StaticClass()))
				{
					newAssetPackage = Cast<UPackage>(Obj->GetOuter());
				}
			}
			else
			{
				break;
			}
		}
		// Writing an exisiting obj with different type causes assert crash in UE4.
		if (Obj && !Obj->GetClass()->IsChildOf(T::StaticClass()))
		{
			return nullptr;
		}

		return 	NewObject<T>(newAssetPackage, objName, Flags);
	}
};