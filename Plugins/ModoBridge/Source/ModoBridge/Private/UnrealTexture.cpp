// Copyright Foundry, 2017. All rights reserved.

#include "UnrealTexture.h"
#include <cassert>

#include "Factories/TextureFactory.h"
#include "AssetRegistryModule.h"

#include "ModoBridge.h"
#include "UnrealNode.h"
#include "UnrealHelper.h"

UnrealTexture::UnrealTexture() :
	_uniqueName(),
	_displayName(),
	_neutralData(nullptr)
{
}

UnrealTexture::~UnrealTexture()
{
	_neutralData = nullptr;
}

const FName& UnrealTexture::name() const
{
	return _uniqueName;
}

const FString& UnrealTexture::displayName() const
{
	return _displayName;
}

void UnrealTexture::parse(const uint8* msgData)
{
	_neutralData  = flatbuffers::GetRoot<Foundry::TextureData>(msgData);

	_uniqueName = UTF8_TO_TCHAR(_neutralData->uniqueName()->c_str());
	_displayName = FString(_neutralData->displayName()->c_str());
}

void FillDataTSF_G8(uint8** data, const float** chan, int dim)
{
	(*data)[0] = (uint8)((*chan)[0] * 255);
	(*data) ++;

	(*chan) += dim;
}

void FillDataTSF_BGRA8(uint8** data, const float** chan, int dim)
{
	float R = (*chan)[0];
	float G = (dim > 1) ? (*chan)[1] : 0.0;
	float B = (dim > 2) ? (*chan)[2] : 0.0;
	float A = (dim > 3) ? (*chan)[3] : 1.0;

	(*data)[0] = (uint8)(B * 255);
	(*data)[1] = (uint8)(G * 255);
	(*data)[2] = (uint8)(R * 255);
	(*data)[3] = (uint8)(A * 255);

	(*data) += 4;
	(*chan) += dim;
}

void FillDataTSF_RGBA16F(uint8** data, const float** chan, int dim)
{
	FFloat16 R((*chan)[0]);
	FFloat16 G((dim > 1) ? (*chan)[1] : 0.0);
	FFloat16 B((dim > 2) ? (*chan)[2] : 0.0);
	FFloat16 A((dim > 3) ? (*chan)[3] : 1.0); 

	uint16 *fData = (uint16*)*data;

	fData[0] = R.Encoded;
	fData[1] = G.Encoded;
	fData[2] = B.Encoded;
	fData[3] = A.Encoded;

	(*data) += 8;
	(*chan) += dim;
}

void FillDataTSF_G8_UCHAR(uint8** data, const uint8_t** chan, int dim)
{
	(*data)[0] = (uint8)((*chan)[0]);
	(*data)++;

	(*chan) += dim;
}

void FillDataTSF_BGRA8_UCHAR(uint8** data, const uint8_t** chan, int dim)
{
	uint8_t R = (*chan)[0];
	uint8_t G = (dim > 1) ? (*chan)[1] : 0;
	uint8_t B = (dim > 2) ? (*chan)[2] : 0;
	uint8_t A = (dim > 3) ? (*chan)[3] : 255;

	(*data)[0] = (uint8)B;
	(*data)[1] = (uint8)G;
	(*data)[2] = (uint8)R;
	(*data)[3] = (uint8)A;

	(*data) += 4;
	(*chan) += dim;
}

void FillDataTSF_RGBA16F_UCHAR(uint8** data, const uint8_t** chan, int dim)
{
	float r = (*chan)[0] / 255.0;
	float g = (dim > 1) ? (*chan)[1] / 255.0 : 0.0;
	float b = (dim > 2) ? (*chan)[2] / 255.0 : 0.0;
	float a = (dim > 3) ? (*chan)[3] / 255.0 : 1.0;

	FFloat16 R(r);
	FFloat16 G(g);
	FFloat16 B(b);
	FFloat16 A(a);

	uint16 *fData = (uint16*)*data;

	fData[0] = R.Encoded;
	fData[1] = G.Encoded;
	fData[2] = B.Encoded;
	fData[3] = A.Encoded;

	(*data) += 8;
	(*chan) += dim;
}

void UnrealTexture::instantiate(UWorld* world, const TMap<FString, FString >& textureLookUp)
{
	FString localName = _displayName;
	UnrealHelper::UnrealTextureNameConvention(localName);
	auto packageName = TextureAssetPath + localName;

	UnrealHelper::GetValidPackageName(packageName);
	const FString* existingPackage = textureLookUp.Find(localName);
	if (existingPackage != nullptr)
	{
		packageName = *existingPackage;
	}

	UPackage* AssetPackage = CreatePackage(NULL, *packageName);
	UTextureFactory* texFactory = NewObject<UTextureFactory>();
	texFactory->AddToRoot();

	if (_neutralData->colorCorrection() == Foundry::ETextureColorCorrection_eSRGB)
	{
		texFactory->LODGroup = TEXTUREGROUP_World;
		texFactory->CompressionSettings = TC_Default;
	}
	else
	{
		texFactory->LODGroup = TEXTUREGROUP_WorldNormalMap;
		texFactory->CompressionSettings = TC_Normalmap;
	}

	texFactory->Blending = BLEND_Opaque;
	texFactory->ShadingModel = MSM_DefaultLit;
	texFactory->MipGenSettings = TMGS_NoMipmaps;

	int dim = _neutralData->components();
	texFactory->NoAlpha = (dim == 4) ? false : true;

	EObjectFlags Flags = RF_Public | RF_Standalone;

	UTexture2D* Texture = texFactory->CreateTexture2D(AssetPackage, *localName, Flags);

	bool isHDR = _neutralData->format() == Foundry::ETextureFormat_eFloat;
	ETextureSourceFormat TextureFormat;

	auto FillData = FillDataTSF_G8;
	auto FillDataUChar = FillDataTSF_G8_UCHAR;

	if (dim == 1)
	{
		FillData = FillDataTSF_G8;
		FillDataUChar = FillDataTSF_G8_UCHAR;
		TextureFormat = TSF_G8;
	}
	else if (!isHDR)
	{
		FillData = FillDataTSF_BGRA8;
		FillDataUChar = FillDataTSF_BGRA8_UCHAR;
		TextureFormat = TSF_BGRA8;
	}
	else
	{
		FillData = FillDataTSF_RGBA16F;
		FillDataUChar = FillDataTSF_RGBA16F_UCHAR;
		TextureFormat = TSF_RGBA16F;
	}

	if (Texture)
	{
		Texture->Filter = TextureFilter::TF_Trilinear;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->Source.Init(_neutralData->width(), _neutralData->height(), 1, 1, TextureFormat);
		Texture->SRGB = _neutralData->colorCorrection() == Foundry::ETextureColorCorrection_eSRGB;
		if (isHDR)
		{
			Texture->SRGB = false;
			Texture->CompressionSettings = TC_HDR;
		}

		uint8* MipData = Texture->Source.LockMip(0);
		if (_neutralData->format() == Foundry::ETextureFormat_eFloat)
		{
			auto fcolors = _neutralData->fcolors();
			const float *chan = fcolors->data();
			uint32 size = fcolors->size() / dim;
			for (uint32 i = 0; i < size; i++)
			{
				FillData(&MipData, &chan, dim);
			}
		}
		else
		{
			auto colors = _neutralData->colors();
			const uint8_t *chan = colors->data();
			uint32 size = colors->size() / dim;
			for (uint32 i = 0; i < size; i++)
			{
				FillDataUChar(&MipData, &chan, dim);
			}
		}

		Texture->Source.UnlockMip(0);
		Texture->UpdateResource();
		AssetPackage->MarkPackageDirty();

		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Texture);
	}
}