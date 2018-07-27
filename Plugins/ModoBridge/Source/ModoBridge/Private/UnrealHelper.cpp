// Copyright Foundry, 2017. All rights reserved.

#include "UnrealHelper.h"
#include "Misc/PackageName.h"
#include "Materials/Material.h"

#include "ModoBridge.h"

bool UnrealHelper::isDeeper(const FString& left, const FString& right)
{
	unsigned leftCount = 0;
	unsigned rightCount = 0;
	for (int32 i = 0; i < left.Len(); i++)
	{
		if (left[i] == TEXT('/'))
		{
			leftCount++;
		}
	}

	for (int32 i = 0; i < right.Len(); i++)
	{
		if (right[i] == TEXT('/'))
		{
			rightCount++;
		}
	}

	return leftCount > rightCount;
}

bool UnrealHelper::UnrealMaterialNameConvention(FString& name)
{
#ifdef NAMEFIX
	if (name.Len() >= 2 && name[0] == 'M' && name[1] == '_')
		return false;

	name = "M_" + name;
#endif
	return true;
}

bool UnrealHelper::UnrealTextureNameConvention(FString& name)
{
#ifdef NAMEFIX
	if (name.Len() >= 2 && name[0] == 'T' && name[1] == '_')
		return false;

	name = "T_" + name;
#endif
	return true;
}

bool UnrealHelper::GetValidPackageName(FString& PackageName)
{
	FText outReason;
	if (!FPackageName::IsValidLongPackageName(PackageName, false, &outReason))
	{

		UE_LOG(ModoBridge, Log, TEXT("PackageName is invalid because: %s"), *outReason.BuildSourceString());

		// try to reconver by replacing blank to underscodea
		// remove double slashes
		bool hasPreSlash = false;
		FString tempPackageName = "";
		tempPackageName.Reserve(PackageName.Len());

		for (int i = 0; i < PackageName.Len(); i++)
		{
			if (PackageName[i] == ' ')
			{
				PackageName[i] = '_';
				hasPreSlash = false;
				tempPackageName += PackageName[i];
			}
			else if (PackageName[i] == '/')
			{
				if (hasPreSlash)
				{
					PackageName[i] = '_';
					hasPreSlash = false;
				}
				else
				{
					hasPreSlash = true;
					tempPackageName += PackageName[i];
				}
			}
			else
			{
				hasPreSlash = false;
				tempPackageName += PackageName[i];
			}
		}

		if (tempPackageName.Len() != PackageName.Len())
		{
			PackageName = tempPackageName;
		}

		if (!FPackageName::IsValidLongPackageName(PackageName, false, &outReason))
		{
			UE_LOG(ModoBridge, Log, TEXT("Auto Resolve failed because: %s"), *outReason.BuildSourceString());
			return false;
		}
		else
		{
			UE_LOG(ModoBridge, Log, TEXT("Auto Resolve to: %s"), *PackageName);
			return true;
		}
	}

	return true;
}

// Strip invalid characters from the string, replacing spaces with underscores.

bool UnrealHelper::RemoveInvalidCharacters(FString& name)
{
	FString validName = "";
	bool bChanged = false;

	for (int i = 0; i < name.Len(); i++)
	{
		if (isalnum(name[i]) || name[i] == '_' || name[i] == '-')
		{
			validName.AppendChar(name[i]);
		}
		else
		{
			if (name[i] == ' ')
			{
				validName.AppendChar('_');
			}
			bChanged = true;
		}
	}

	if (bChanged)
	{
		name = validName;
	}

	return bChanged;
}

bool UnrealHelper::CorrectInvalidPath(FString& name)
{
	bool bChanged = false;
	FString validName = "";

	for (int i = 0; i < name.Len(); i++)
	{
		if (name[i] == '\\')
		{
			validName.AppendChar('/');
			bChanged = true;
		}
		else
		{
			validName.AppendChar(name[i]);
		}
	}

	if (bChanged)
		name = validName;

	return bChanged;
}

bool UnrealHelper::RemoveMaterialSlotSuffix(FString& name)
{
	bool bChanged = false;

	if (name.Len() > 6)
	{
		int32 Offset = name.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Offset != INDEX_NONE)
		{
			// Chop off the material name so we are left with the number in _SKINXX
			FString SkinXXNumber = name.Right(name.Len() - (Offset + 1)).RightChop(4);

			if (SkinXXNumber.IsNumeric())
			{
				// remove the '_skinXX' suffix from the material name
				name = name.Left(Offset);

				bChanged = true;
			}
		}
	}

	return bChanged;
}

void UnrealHelper::DetermineVertsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshLODResources& RenderMesh)
{
	const int32 VertexCount = RenderMesh.VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts 
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector, int32> HashedVerts;
	for (int32 a = 0; a < VertexCount; a++)
	{
		const FVector& PositionA = RenderMesh.PositionVertexBuffer.VertexPosition(a);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if (!FoundIndex)
		{
			int32 NewIndex = UniqueVerts.Add(a);
			VertRemap[a] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[a] = *FoundIndex;
		}
	}
}

void UnrealHelper::DetermineUVsToWeld(TArray<int32>& VertRemap, TArray<int32>& UniqueVerts, const FStaticMeshVertexBuffer& VertexBuffer, int32 TexCoordSourceIndex)
{
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// Maps unreal verts to reduced list of verts
	VertRemap.Empty(VertexCount);
	VertRemap.AddUninitialized(VertexCount);

	// List of Unreal Verts to keep
	UniqueVerts.Empty(VertexCount);

	// Combine matching verts using hashed search to maintain good performance
	TMap<FVector2D, int32> HashedVerts;
	for (int32 Vertex = 0; Vertex < VertexCount; Vertex++)
	{
		const FVector2D& PositionA = VertexBuffer.GetVertexUV(Vertex, TexCoordSourceIndex);
		const int32* FoundIndex = HashedVerts.Find(PositionA);
		if (!FoundIndex)
		{
			int32 NewIndex = UniqueVerts.Add(Vertex);
			VertRemap[Vertex] = NewIndex;
			HashedVerts.Add(PositionA, NewIndex);
		}
		else
		{
			VertRemap[Vertex] = *FoundIndex;
		}
	}
}

FString UnrealHelper::GetMaterialGlobalName(const UMaterial* mat)
{
	return mat->GetOuter()->GetName() + FString(".") + mat->GetName();
}

std::string UnrealHelper::GetLocalUniqueName(const UObject* obj)
{
	return std::to_string(obj->GetUniqueID());
}

static CoordinateSytemConverter* gCoordinateSystemConverter = nullptr;

void RotateX90(FVector &vector)
{
	float y = vector[1];
	vector[1] = vector[2];
	vector[2] = -y;
}

void RotateX270(FVector &vector)
{
	float x = vector[2];
	vector[2] = vector[1];
	vector[1] = -x;
}

CoordinateSytemConverter* CoordinateSytemConverter::Get()
{
	if (gCoordinateSystemConverter == nullptr)
		gCoordinateSystemConverter = new CoordinateSytemConverter();

	return gCoordinateSystemConverter;
}

// resetMeshAsset alters how we decompose transformation from neutral to asset + actor
void CoordinateSytemConverter::Set(bool resetMeshAsset)
{
	if (gCoordinateSystemConverter == nullptr)
		gCoordinateSystemConverter = new CoordinateSytemConverter();

	gCoordinateSystemConverter->_resetMeshAsset = resetMeshAsset;
}

void CoordinateSytemConverter::ConvertNormalUnrealToNeutral(FVector &vector, bool assetRest)
{
	if (assetRest)
	{
		RotateX270(vector);
	}
	vector[1] = -vector[1];
}


void CoordinateSytemConverter::ConvertUVUnrealToNeutral(FVector2D &vector)
{
	vector[1] = 1.0 - vector[1];
}

void CoordinateSytemConverter::ConvertPositionUnrealToNeutral(FVector &vector, bool assetRest)
{
	if (assetRest)
	{
		RotateX270(vector);
	}
	vector[1] = -vector[1];
#ifdef UNIT_MATCH_SCALE
#else
	vector[0] = vector[0] * 0.01;
	vector[1] = vector[1] * 0.01;
	vector[2] = vector[2] * 0.01;
#endif
}

FMatrix CoordinateSytemConverter::ConvertTransfromUnrealToNeutral(FMatrix in_matrix, bool assetRest)
{
	if (assetRest == false)
	{
		FMatrix UEMatrix;
		for (uint32 i = 0; i < 4; i++)
		{
			if (i == 1)
			{
				UEMatrix.M[i][0] = -in_matrix.M[i][0];
				UEMatrix.M[i][1] = in_matrix.M[i][1];
				UEMatrix.M[i][2] = -in_matrix.M[i][2];
				UEMatrix.M[i][3] = -in_matrix.M[i][3];
			}
			else
			{
				UEMatrix.M[i][0] = in_matrix.M[i][0];
				UEMatrix.M[i][1] = -in_matrix.M[i][1];
				UEMatrix.M[i][2] = in_matrix.M[i][2];
				UEMatrix.M[i][3] = in_matrix.M[i][3];
			}
		}
		return UEMatrix;
	}

	return in_matrix;
}

FMatrix CoordinateSytemConverter::ConvertTransfromNeutralToUnreal(FMatrix in_matrix)
{
	if (_resetMeshAsset == false)
	{
		FMatrix UEMatrix;
		for (uint32 i = 0; i < 4; i++)
		{
			if (i == 1)
			{
				UEMatrix.M[i][0] = -in_matrix.M[i][0];
				UEMatrix.M[i][1] = in_matrix.M[i][1];
				UEMatrix.M[i][2] = -in_matrix.M[i][2];
				UEMatrix.M[i][3] = -in_matrix.M[i][3];
			}
			else
			{
				UEMatrix.M[i][0] = in_matrix.M[i][0];
				UEMatrix.M[i][1] = -in_matrix.M[i][1];
				UEMatrix.M[i][2] = in_matrix.M[i][2];
				UEMatrix.M[i][3] = in_matrix.M[i][3];
			}
		}
		return UEMatrix;
	}

	return in_matrix;
}

void CoordinateSytemConverter::ConvertUVNeutralToUnreal(FVector2D &vector)
{
	vector[1] = 1.0 - vector[1];
}

void CoordinateSytemConverter::ConvertNormalNeutralToUnreal(FVector &vector)
{
	vector[1] = -vector[1];
	if (_resetMeshAsset)
	{
		RotateX90(vector);
	}
}

void CoordinateSytemConverter::ConvertPositionNeutralToUnreal(FVector &vector)
{
	vector[1] = -vector[1];
	if (_resetMeshAsset)
	{
		RotateX90(vector);
	}


#ifdef UNIT_MATCH_SCALE
#else
	vector[0] = vector[0] * 100.0;
	vector[1] = vector[1] * 100.0;
	vector[2] = vector[2] * 100.0;
#endif
}

CoordinateSytemConverter::CoordinateSytemConverter() : _resetMeshAsset(false)
{

}

bool CoordinateSytemConverter::ResetMeshAsset()
{
	return _resetMeshAsset;
}

bool CoordinateSytemConverter::ResetMeshAsset(UStaticMesh *mesh)
{
	// Look into user data array and find our modo mesh asset user data and get the values
	auto userDataArray = mesh->GetAssetUserDataArray();
	if (userDataArray != nullptr)
	{
		for (auto userData : *userDataArray)
		{
			auto modoUserData = Cast<UModoBridgeMeshAssetData>(userData);
			if (modoUserData != nullptr)
			{
				return modoUserData->AssetReset;
			}
		}
	}

	return false;
}