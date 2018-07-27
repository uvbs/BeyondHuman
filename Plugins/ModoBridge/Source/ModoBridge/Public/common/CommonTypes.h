// Copyright Foundry, 2017. All rights reserved.

#ifndef COMMONTYPES_H
#define COMMONTYPES_H

#include "NeutralData.h"

namespace Foundry {

#ifdef _WIN32
#define DoesNotRequireTR1 1
#endif

#ifdef __APPLE__
#define DoesNotRequireTR1 1
#endif

#ifndef DoesNotRequireTR1
#include <tr1/memory>
#endif

#ifdef DoesNotRequireTR1
	typedef std::shared_ptr<NodeData> NodePtr;
	typedef std::shared_ptr<MeshData> MeshPtr;
	typedef std::shared_ptr<TextureData> TexturePtr;
	typedef std::shared_ptr<CameraData> CameraPtr;
	typedef std::shared_ptr<LightData> LightPtr;
#else
	typedef std::tr1::shared_ptr<NodeData> NodePtr;
	typedef std::tr1::shared_ptr<MeshData> MeshPtr;
	typedef std::tr1::shared_ptr<TextureData> TexturePtr;
	typedef std::tr1::shared_ptr<CameraData> CameraPtr;
	typedef std::tr1::shared_ptr<LightData> LightPtr;
#endif

	static const int DataStrideIndex = 3;
	static const int DataStrideVertex = 3;
	static const int DataStrideNormal = 3;
	static const int DataStrideUV = 2;
	static const int DataStrideColor = 3;
	static const int DataStrideTangent = 6;
	static const int DataStrideMatIndex = 1;
	static const int DataStrideTransform = 16;
}

#endif