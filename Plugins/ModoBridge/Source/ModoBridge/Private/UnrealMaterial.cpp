// Copyright Foundry, 2017. All rights reserved.

#include "UnrealMaterial.h"
#include <cassert>

#include "AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Factories/TextureFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "ARFilter.h"

#include "ModoBridge.h"
#include "UnrealNode.h"
#include "ModoBridgeHandler.h"
#include "UnrealHelper.h"

struct SlotEffect
{
	const Foundry::TextureEffect *effect;
	const flatbuffers::String *textureName;
};

UnrealMaterial::UnrealMaterial() :
	_uniqueName(),
	_displayName(),
	_neutralData(nullptr)
{
}

UnrealMaterial::~UnrealMaterial()
{
	_neutralData = nullptr;
}

const FName& UnrealMaterial::name() const
{
	return _uniqueName;
}

const FString& UnrealMaterial::displayName() const
{
	return _displayName;
}

void UnrealMaterial::parse(const uint8* msgData)
{
	_neutralData  = flatbuffers::GetRoot<Foundry::MaterialData>(msgData);

	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());
}

int channelOutputIndex(Foundry::ETextureSwizzle swizzling)
{
	if (swizzling == Foundry::ETextureSwizzle_eRRR)
	{
		return 1;
	}

	if (swizzling == Foundry::ETextureSwizzle_eGGG)
	{
		return 2;
	}

	if (swizzling == Foundry::ETextureSwizzle_eBBB)
	{
		return 3;
	}

	if (swizzling == Foundry::ETextureSwizzle_eAAA)
	{
		return 4;
	}

	return 0;
}

template <typename T>
void LinkConstant(UMaterial* mat, const std::vector<float>& value, FMaterialInput<T>* matInput, int& position)
{
	if (mat != nullptr)
	{
		UMaterialExpression* Expression = NULL;

		switch (value.size())
		{
		case 0:
			return;
		case 1:
		{
			UMaterialExpressionConstant* expConst = NewObject<UMaterialExpressionConstant>(mat);
			expConst->R = value[0];
			Expression = expConst;
		}
		break;
		case 2:
		{
			UMaterialExpressionConstant2Vector* expConst = NewObject<UMaterialExpressionConstant2Vector>(mat);
			expConst->R = value[0];
			expConst->G = value[1];
			Expression = expConst;
		}
		break;
		case 3:
		{
			UMaterialExpressionConstant3Vector* expConst = NewObject<UMaterialExpressionConstant3Vector>(mat);
			expConst->Constant = FLinearColor(value[0], value[1], value[2]);
			Expression = expConst;
		}
		break;
		default:
		{
			UMaterialExpressionConstant4Vector* expConst = NewObject<UMaterialExpressionConstant4Vector>(mat);
			expConst->Constant = FLinearColor(value[0], value[1], value[2], value[3]);
			Expression = expConst;
		}
		break;
		}
		mat->Expressions.Add(Expression);

		Expression->MaterialExpressionEditorX = -200;
		Expression->MaterialExpressionEditorY = position;
		position += 64;

		TArray<FExpressionOutput> Outputs;
		Outputs = Expression->GetOutputs();
		FExpressionOutput* Output = Outputs.GetData();
		if (matInput)
		{
			matInput->Expression = Expression;
			matInput->Mask = Output->Mask;
			matInput->MaskR = Output->MaskR;
			matInput->MaskG = Output->MaskG;
			matInput->MaskB = Output->MaskB;
			matInput->MaskA = Output->MaskA;
		}

		mat->PostEditChange();
	}
}

template <typename T> 
void LinkTexture(
	UMaterial* mat,
	UTexture* tex,
	FMaterialInput<T>* matInput,
	int& position,
	float tiling[2],
	int uvIndex,
	int outIndex,
	EMaterialSamplerType samplerType = EMaterialSamplerType::SAMPLERTYPE_Color)
{
	if (mat != nullptr && tex != nullptr)
	{
		// An initial texture was specified, add it and assign it to the BaseColor
		UMaterialExpressionTextureSample* Expression = NULL;

		// Look into current material and get textureSampler if exists
		TArray<UMaterialExpressionTextureSample*> texSampleExpressions;
		mat->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionTextureSample>(texSampleExpressions);

		for (int i = 0; i < texSampleExpressions.Num(); i++)
		{
			UMaterialExpressionTextureSample* texSampleExpression = texSampleExpressions[i];

			// check texture
			if (tex == texSampleExpression->Texture)
			{
				// check tiling ... maybe check more in the future
				UMaterialExpressionTextureCoordinate* texCoordExpression = dynamic_cast<UMaterialExpressionTextureCoordinate*>(texSampleExpression->Coordinates.Expression);
				if (texCoordExpression != NULL)
				{
					if (texCoordExpression->UTiling == tiling[0] &&
						texCoordExpression->VTiling == tiling[1] &&
						texCoordExpression->CoordinateIndex == uvIndex
						)
					{
						Expression = texSampleExpression;
						break;
					}
				}
				else
				{
					if (tiling[0] == 1.0 && tiling[1] == 1.0 && uvIndex == 0)
					{
						Expression = texSampleExpression; // this line is not needed
						break;
					}
				}
			}
		}

		// If Expression can not be found, we create a new one
		if (Expression == NULL)
		{
			Expression = NewObject<UMaterialExpressionTextureSample>(mat);
			mat->Expressions.Add(Expression);
			Expression->MaterialExpressionEditorX = -380;
			Expression->MaterialExpressionEditorY = position;
			position += 64;

			Expression->Texture = tex;

			// Adjust samplerType as Unreal will assert if wrong type anyway
			if (tex->SRGB)
			{
				if (tex->CompressionSettings == TextureCompressionSettings::TC_Grayscale)
				{
					samplerType = EMaterialSamplerType::SAMPLERTYPE_Grayscale;
				}
				else
				{
					samplerType = EMaterialSamplerType::SAMPLERTYPE_Color;
				}
			}
			else
			{
				if (tex->CompressionSettings == TextureCompressionSettings::TC_Normalmap)
				{
					samplerType = EMaterialSamplerType::SAMPLERTYPE_Normal;
				}
				else if (tex->CompressionSettings == TextureCompressionSettings::TC_Grayscale)
				{
					samplerType = EMaterialSamplerType::SAMPLERTYPE_LinearGrayscale;
				}
				else
				{
					samplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				}
			}
			Expression->SamplerType = samplerType;

			if (tiling[0] != 1.0 || tiling[1] != 1.0 || uvIndex != 0)
			{
				TArray<UMaterialExpressionTextureCoordinate*> texCoordExpressions;

				mat->GetAllExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionTextureCoordinate>(texCoordExpressions);
				FExpressionInput texCoordInput;
				bool texCoordExists = false;

				for (int i = 0; i < texCoordExpressions.Num(); i++)
				{
					UMaterialExpressionTextureCoordinate* texCoordExpression = texCoordExpressions[i];
					if (texCoordExpression->UTiling == tiling[0] &&
						texCoordExpression->VTiling == tiling[1] &&
						texCoordExpression->CoordinateIndex == uvIndex)
					{
						texCoordInput.Expression = texCoordExpression;
						texCoordExists = true;
						break;
					}
				}

				if (texCoordExists == false)
				{
					UMaterialExpressionTextureCoordinate *texCoordExpression = NewObject<UMaterialExpressionTextureCoordinate>(mat);
					texCoordExpression->UTiling = tiling[0];
					texCoordExpression->VTiling = tiling[1];
					texCoordExpression->CoordinateIndex = uvIndex;
					mat->Expressions.Add(texCoordExpression);
					texCoordExpression->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX - 100;
					texCoordExpression->MaterialExpressionEditorY = position;
					texCoordInput.Expression = texCoordExpression;
				}

				Expression->Coordinates = texCoordInput;
			}
		}

		if (matInput != NULL)
			matInput->Connect(outIndex, Expression);

		mat->PostEditChange();
	}
}

UMaterial* CreateMaterial(const FString packageName, FString materialName)
{
	UMaterialFactoryNew* matFactory = NewObject<UMaterialFactoryNew>();
	matFactory->AddToRoot();
	UE_LOG(ModoBridge, Log, TEXT("Creating material package: %s"), *packageName);

	UPackage* assetPackage = CreatePackage(NULL, *packageName);
	EObjectFlags Flags = RF_Public | RF_Standalone;

	UObject* CreatedAsset = matFactory->FactoryCreateNew(UMaterial::StaticClass(), assetPackage, FName(*materialName), Flags, NULL, GWarn);

	if (CreatedAsset)
	{
		// Mark the package dirty...
		assetPackage->MarkPackageDirty();
	}
	matFactory->RemoveFromRoot();
	return Cast<UMaterial>(CreatedAsset);
}

bool AddFloatParam(const SlotEffect *data, UMaterial* mat, FMaterialInput<float>& matInput, int &graphOffset)
{
	bool anyTextureUsed = false;
	auto effect = data->effect;
	const char* imageName = data->textureName->c_str();
	auto tex = UnrealMaterial::getTexture(imageName);

	if (tex)
	{
		auto wrapU = effect->wrapU();
		auto wrapV = effect->wrapV();
		float tiling[2];
		tiling[0] = wrapU;
		tiling[1] = wrapV;

		auto swizzling = effect->swizzle();
		int outIndex = channelOutputIndex(swizzling);
		int uvChannelIndex = effect->uvIndex();
		EMaterialSamplerType type = SAMPLERTYPE_LinearColor;
		LinkTexture<float>(mat, tex, &matInput, graphOffset, tiling, uvChannelIndex, outIndex, type);
		anyTextureUsed = true;
		return true;
	}

	if (!anyTextureUsed)
	{
		if (effect->R() == matInput.Constant)
			return false;

		matInput.Constant = effect->R();
		std::vector<float> color = { effect->R() };
		LinkConstant<float>(mat, color, &matInput, graphOffset);
		return true;
	}

	return false;
}

bool AddVectorParam(const SlotEffect *data, UMaterial* mat, FMaterialInput<FVector>& matInput, int &graphOffset, EMaterialSamplerType type)
{
	bool anyTextureUsed = false;
	auto effect = data->effect;
	const char* imageName = data->textureName->c_str();
	auto tex = UnrealMaterial::getTexture(imageName);
	if (tex)
	{
		auto wrapU = effect->wrapU();
		auto wrapV = effect->wrapV();
		float tiling[2];
		tiling[0] = wrapU;
		tiling[1] = wrapV;

		auto swizzling = effect->swizzle();
		int outIndex = channelOutputIndex(swizzling);
		int uvChannelIndex = effect->uvIndex();
		LinkTexture<FVector>(mat, tex, &matInput, graphOffset, tiling, uvChannelIndex, outIndex, type);

		anyTextureUsed = true;
		return true;
	}

	if (!anyTextureUsed)
	{
		FVector vec(effect->R(), effect->G(), effect->B());

		if (matInput.Constant == vec)
			return false;

		matInput.Constant = vec;
		std::vector<float> color = { vec[0], vec[1], vec[2] };
		LinkConstant<FVector>(mat, color, &matInput, graphOffset);
		return true;
	}

	return false;
}

bool AddColorParam(const SlotEffect *data, UMaterial* mat, FMaterialInput<FColor>& matInput, int &graphOffset)
{
	bool anyTextureUsed = false;
	auto effect = data->effect;
	const char* imageName = data->textureName->c_str();
	auto tex = UnrealMaterial::getTexture(imageName);
	if (tex)
	{
		auto wrapU = effect->wrapU();
		auto wrapV = effect->wrapV();
		float tiling[2];
		tiling[0] = wrapU;
		tiling[1] = wrapV;

		auto swizzling = effect->swizzle();
		int outIndex = channelOutputIndex(swizzling);
		int uvChannelIndex = effect->uvIndex();

		EMaterialSamplerType type;
		type = (tex->SRGB == true) ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		LinkTexture<FColor>(mat, tex, &matInput, graphOffset, tiling, uvChannelIndex, outIndex, type);
		anyTextureUsed = true;
		return true;
	}

	if (!anyTextureUsed)
	{
		FVector4 vec(effect->R(), effect->G(), effect->B(), 1);

		uint8 R = FMath::Clamp((int)(vec[0] * 255), 0, 255);
		uint8 G = FMath::Clamp((int)(vec[1] * 255), 0, 255);
		uint8 B = FMath::Clamp((int)(vec[2] * 255), 0, 255);
		uint8 A = FMath::Clamp((int)(vec[3] * 255), 0, 255);

		if (matInput.Constant == FColor(R, G, B, A))
			return false;

		matInput.Constant = FColor(R, G, B, A);
		std::vector<float> color = { vec[0], vec[1], vec[2], vec[3] };
		LinkConstant<FColor>(mat, color, &matInput, graphOffset);
		return true;
	}

	return false;
}

bool AddUnkownParam(const SlotEffect *data, UMaterial* mat, int &graphOffset)
{
	bool anyTextureUsed = false;
	auto effect = data->effect;
	const char* imageName = data->textureName->c_str();
	auto tex = UnrealMaterial::getTexture(imageName);
	if (tex)
	{
		auto wrapU = effect->wrapU();
		auto wrapV = effect->wrapV();
		float tiling[2];
		tiling[0] = wrapU;
		tiling[1] = wrapV;

		auto swizzling = effect->swizzle();
		int outIndex = channelOutputIndex(swizzling);
		int uvChannelIndex = effect->uvIndex();

		EMaterialSamplerType type;
		type = (tex->SRGB == true) ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		LinkTexture<FColor>(mat, tex, NULL, graphOffset, tiling, uvChannelIndex, outIndex, type);
		anyTextureUsed = true;

		return true;
	}

	return false;
}

class TextureAssetInfo
{
public:
	TextureAssetInfo() : texturePtr(nullptr), name() {}
	UTexture*	texturePtr;
	FString		name;
};

static TMap<FString, TextureAssetInfo> g_textureAssets;

UTexture* UnrealMaterial::getTexture(const FString& name)
{
	auto correctedName = name;
	UnrealHelper::UnrealTextureNameConvention(correctedName);
	TextureAssetInfo* texInfoPtr = g_textureAssets.Find(correctedName);

	if (texInfoPtr == nullptr)
	{
		return nullptr;
	}
	else
	{
		return texInfoPtr->texturePtr;
	}
}

void UnrealMaterial::validateTextureAssets(bool subfolders)
{
	g_textureAssets.Empty();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.bRecursivePaths = subfolders;
	Filter.bRecursiveClasses = true;
	Filter.ClassNames.Add(UTexture::StaticClass()->GetFName());

	FString PackagePath = FPackageName::FilenameToLongPackageName(TextureAssetPath);
	if (PackagePath.Len() > 1 && PackagePath[PackagePath.Len() - 1] == TEXT('/'))
	{
		// The filter path can't end with a trailing slash
		PackagePath = PackagePath.LeftChop(1);
	}
	Filter.PackagePaths.Emplace(*PackagePath);

	TArray<FAssetData> selectedAssetList;
	AssetRegistryModule.Get().GetAssets(Filter, selectedAssetList);

	for (int i = 0; i < selectedAssetList.Num(); i++)
	{
		const FAssetData* adPtr = &selectedAssetList[i];
		FString assetName = adPtr->AssetName.ToString();
		FString packageName = adPtr->PackageName.ToString();
		TextureAssetInfo& assetInfo = g_textureAssets.FindOrAdd(assetName);

		if (assetInfo.name.IsEmpty() || UnrealHelper::isDeeper(assetInfo.name, packageName))
		{
			assetInfo.name = packageName;
			UObject* asset = adPtr->GetAsset();
			if (asset != NULL)
			{
				UTexture* texture = dynamic_cast<UTexture*> (asset);
				if (texture != NULL)
				{
					assetInfo.texturePtr = texture;
				}
			}
		}
	}
}

void UnrealMaterial::instantiate(UWorld* world, const TMap<FString, FString >& materialLookUp)
{
	FString localName = _displayName;
	UnrealHelper::UnrealMaterialNameConvention(localName);
	auto packageName = MaterialAssetPath + localName;
	UnrealHelper::GetValidPackageName(packageName);

	const FString* existingPackage = materialLookUp.Find(localName);
	if (existingPackage != nullptr)
	{
		packageName = *existingPackage;
	}

	UMaterial *mat = CreateMaterial(packageName, localName);
	bool useTransparent = false;
	bool useClearCoat = false;
	bool useSubsurface = false;
	int graphOffset = -64;

	SlotEffect base;
	base.effect = _neutralData->baseColor();
	base.textureName = _neutralData->baseColorImgName();
	AddColorParam(&base, mat, mat->BaseColor, graphOffset);

	SlotEffect metallic;
	metallic.effect = _neutralData->metallic();
	metallic.textureName = _neutralData->metallicImgName();
	AddFloatParam(&metallic, mat, mat->Metallic, graphOffset);

	SlotEffect opacity;
	opacity.effect = _neutralData->opacity();
	opacity.textureName = _neutralData->opacityImgName();
	useTransparent = AddFloatParam(&opacity, mat, mat->Opacity, graphOffset);

	SlotEffect emissive;
	emissive.effect = _neutralData->emissiveColor();
	emissive.textureName = _neutralData->emissiveColorImgName();
	AddColorParam(&emissive, mat, mat->EmissiveColor, graphOffset);

	SlotEffect normal;
	normal.effect = _neutralData->normal();
	normal.textureName = _neutralData->normalImgName();
	AddVectorParam(&normal, mat, mat->Normal, graphOffset, SAMPLERTYPE_Normal);

	SlotEffect specular;
	specular.effect = _neutralData->specular();
	specular.textureName = _neutralData->specularImgName();
	AddFloatParam(&specular, mat, mat->Specular, graphOffset);

	SlotEffect roughness;
	roughness.effect = _neutralData->roughness();
	roughness.textureName = _neutralData->roughnessImgName();
	AddFloatParam(&roughness, mat, mat->Roughness, graphOffset);

	SlotEffect clearCoatRough;
	clearCoatRough.effect = _neutralData->clearCoatRough();
	clearCoatRough.textureName = _neutralData->clearCoatRoughImgName();
	useClearCoat |= AddFloatParam(&clearCoatRough, mat, mat->ClearCoatRoughness, graphOffset);

	SlotEffect clearCoatAmt;
	clearCoatAmt.effect = _neutralData->clearCoatAmt();
	clearCoatAmt.textureName = _neutralData->clearCoatAmtImgName();
	useClearCoat |= AddFloatParam(&clearCoatAmt, mat, mat->ClearCoat, graphOffset);

	SlotEffect ao;
	ao.effect = _neutralData->ambientOcclusion();
	ao.textureName = _neutralData->ambientOcclusionImgName();
	AddFloatParam(&ao, mat, mat->AmbientOcclusion, graphOffset);

	SlotEffect ss;
	ss.effect = _neutralData->subsurfaceColor();
	ss.textureName = _neutralData->subsurfaceColorImgName();
	useSubsurface = AddColorParam(&ss, mat, mat->SubsurfaceColor, graphOffset);

	if (useTransparent)
	{
		mat->BlendMode = BLEND_Translucent;
	}

	if (useClearCoat)
	{
		mat->SetShadingModel(MSM_ClearCoat);
	}

	if (useSubsurface)
	{
		mat->SetShadingModel(MSM_Subsurface);
	}

	if (useTransparent || useClearCoat || useSubsurface)
	{
		mat->PostEditChange();
	}

	mat->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(mat);
}