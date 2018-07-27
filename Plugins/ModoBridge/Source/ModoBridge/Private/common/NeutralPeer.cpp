#include "NeutralPeer.h"

#if defined(WIN32)
#define SNPRINTF _snprintf_s
#else
#define SNPRINTF snprintf
#endif

namespace Foundry {

static const std::string M_Idle = "*IDLE*";
static const std::string M_Update = "*UPDATE*";
static const std::string M_Clear = "*CLEAR*";
static const std::string M_GetNode = "*NODE*";
static const std::string M_SendNode = "@NODE@";
static const std::string M_GetMesh = "*MESH*";
static const std::string M_SendMesh = "@MESH@";
static const std::string M_GetCamera = "*CAMERA*";
static const std::string M_SendCamera = "@CAMERA@";
static const std::string M_GetLight = "*LIGHT*";
static const std::string M_SendLight = "@LIGHT@";
static const std::string M_Undefined = "*UDEF*";
static const std::string M_GetTexture = "*TEXTURE*";
static const std::string M_SendTexture = "@TEXTURE@";
static const std::string M_SendMaterial = "@MATERIAL@";

bool isWrite(const char* header)
{
	if (header[0] == '@')
	{
		return true;
	}

	return false;
}

bool isRead(const char* header)
{
	if (header[0] == '*')
	{
		return true;
	}

	return false;
}

NeutralPeer::NeutralPeer(const std::string& server, int port) :
	_port(port),
	_running(false),
	_socket(nullptr),
	_ctx(nullptr),
	_server(server),
	_address(),
	_pushItems(false),
	_pushTextures(false),
	_pushMaterials(false)
{
	_ctx = zmq_ctx_new();

	char socketName[128];
	SNPRINTF(socketName, 128, "tcp://%s:%d", _server.c_str(), _port);
	_address = socketName;
}

void NeutralPeer::setup(const std::string& server, int port)
{
	_server = server;
	_port = port;

	char socketName[128];
	SNPRINTF(socketName, 128, "tcp://%s:%d", _server.c_str(), _port);
	_address = socketName;
}

NeutralPeer::~NeutralPeer()
{
	if (_socket)
	{
		zmq_close(_socket);
		_socket = nullptr;
	}

	if (_ctx)
	{
		zmq_ctx_destroy(_ctx);
		_ctx = nullptr;
	}
}

void NeutralPeer::pushRequest(bool selected)
{
	_lazyStates.push(PeerState_eClear);
	_lazyStates.push(PeerState_eSendCamera);
	_lazyStates.push(PeerState_eSendMesh);
	_lazyStates.push(PeerState_eSendLight);
	_lazyStates.push(PeerState_eSendNode);
	_lazyStates.push(PeerState_eUpdate);
}

void NeutralPeer::pushRequestPrepare(bool item, bool material, bool texture)
{
	_lazyStates.push(PeerState_eClear);
	_pushItems = item;
	_pushTextures = material;
	_pushMaterials = texture;
}

void NeutralPeer::pushRequestFinish()
{
	_lazyStates.push(PeerState_eUpdate);
	_pushItems = false;
	_pushTextures = false;
	_pushMaterials = false;
}

void NeutralPeer::processState(PeerState state)
{

}

const FlatBufferBuilder* NeutralPeer::dataMaterial()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataNode()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataConfig()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataMesh()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataCamera()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataLight()
{
	return nullptr;
}

const FlatBufferBuilder* NeutralPeer::dataTexture()
{
	return nullptr;
}

void NeutralPeer::reset(std::vector<flatbuffers::FlatBufferBuilder* >& builders, unsigned size)
{
	for (auto& builder : builders)
	{
		builder->Clear();
	}

	builders.clear();

	unsigned oldSize = builders.size();
	unsigned newSize = size;

	for (unsigned i = newSize; i < oldSize; i++)
	{
		delete builders[i];
	}

	builders.resize(newSize);

	for (unsigned i = oldSize; i < newSize; i++)
	{
		builders[i] = new flatbuffers::FlatBufferBuilder(256);
	}
}
void NeutralPeer::reset(std::queue<flatbuffers::FlatBufferBuilder* >& builders)
{
	std::queue<flatbuffers::FlatBufferBuilder*> empty;
	std::swap(builders, empty);
}

void NeutralPeer::update()
{

}

PeerState NeutralPeer::processHeader(uint8_t* data)
{
	char* message = (char*)data;
	PeerState state = PeerState_eIdle;

	if (isRead(message))
	{
		if (strncmp(M_Idle.c_str(), message, M_Idle.size()) == 0)
		{
			state = PeerState_eIdle;
		}
		else if (strncmp(M_GetNode.c_str(), message, M_GetNode.size()) == 0)
		{
			_states.push(PeerState_eGetNode);
			state = PeerState_eGetNode;
		}
		else if (strncmp(M_GetMesh.c_str(), message, M_GetMesh.size()) == 0)
		{
			_states.push(PeerState_eGetMesh);
			state = PeerState_eGetMesh;
		}
		else if (strncmp(M_GetCamera.c_str(), message, M_GetCamera.size()) == 0)
		{
			_states.push(PeerState_eGetCamera);
			state = PeerState_eGetCamera;
		}
		else if (strncmp(M_GetLight.c_str(), message, M_GetLight.size()) == 0)
		{
			_states.push(PeerState_eGetLight);
			state = PeerState_eGetLight;
		}
		else if (strncmp(M_Clear.c_str(), message, M_Clear.size()) == 0)
		{
			state = PeerState_eClear;
		}
		else if (strncmp(M_Update.c_str(), message, M_Update.size()) == 0)
		{
			state = PeerState_eUpdate;
		}
	}
	else if (isWrite(message))
	{
		if (strncmp(M_SendMesh.c_str(), message, M_SendMesh.size()) == 0)
		{
			state = PeerState_eSendMesh;
		}
		else if (strncmp(M_SendNode.c_str(), message, M_SendNode.size()) == 0)
		{
			state = PeerState_eSendNode;
		}
		else if (strncmp(M_SendCamera.c_str(), message, M_SendCamera.size()) == 0)
		{
			state = PeerState_eSendCamera;
		}
		else if (strncmp(M_SendLight.c_str(), message, M_SendLight.size()) == 0)
		{
			state = PeerState_eSendLight;
		}
		else if (strncmp(M_SendTexture.c_str(), message, M_SendTexture.size()) == 0)
		{
			state = PeerState_eSendTexture;
		}
		else if (strncmp(M_SendMaterial.c_str(), message, M_SendMaterial.size()) == 0)
		{
			state = PeerState_eSendMaterial;
		}
	}

	return state;
}

void NeutralPeer::processMeshData(uint8_t* data)
{

}

void NeutralPeer::processTextureData(uint8_t* data)
{

}

void NeutralPeer::processNodeData(uint8_t* data)
{

}

void NeutralPeer::processCameraData(uint8_t* data)
{

}

void NeutralPeer::processLightData(uint8_t* data)
{

}

void NeutralPeer::processWhenIdle(uint8_t* data)
{

}

void NeutralPeer::processWhenUpdate(uint8_t* data)
{

}

void NeutralPeer::processWhenClear(uint8_t* data)
{

}

void NeutralPeer::processMaterialData(uint8_t* data)
{

}

void NeutralPeer::processNoData(PeerState state)
{
	switch (state)
	{
	case PeerState_eIdle:
		processWhenIdle(nullptr);
		break;
	case PeerState_eUpdate:
		processWhenUpdate(nullptr);
		break;
	case PeerState_eClear:
		processWhenClear(nullptr);
		break;
	}
}

void NeutralPeer::processData(uint8_t* data, PeerState state)
{
	uint8_t *buffer_pointer = (uint8_t*)data;

	switch (state)
	{
	case PeerState_eSendMesh:
		processMeshData(buffer_pointer);
		break;
	case PeerState_eSendNode:
		processNodeData(buffer_pointer);
		break;
	case PeerState_eSendCamera:
		processCameraData(buffer_pointer);
		break;
	case PeerState_eSendLight:
		processLightData(buffer_pointer);
		break;
	case PeerState_eSendTexture:
		processTextureData(buffer_pointer);
		break;
	case PeerState_eSendMaterial:
		processMaterialData(buffer_pointer);
		break;
	case PeerState_eIdle:
		processWhenIdle(buffer_pointer);
		break;
	case PeerState_eUpdate:
		processWhenUpdate(buffer_pointer);
		break;
	case PeerState_eClear:
		processWhenClear(buffer_pointer);
		break;
	}
}

const FlatBufferBuilder* NeutralPeer::getSendData(FlatBufferBuilder *builder, const std::string** instruction, PeerState& state)
{
	const FlatBufferBuilder *result = nullptr;

	state = PeerState_eIdle;

	if (!_states.empty())
	{
		state = _states.front();
		_states.pop();
	}

	switch (state)
	{
	case PeerState_eIdle:
		*instruction = &M_Idle;
		break;
	case PeerState_eClear:
		*instruction = &M_Clear;
		result = dataConfig();
		break;
	case PeerState_eUpdate:
		*instruction = &M_Update;
		break;
	case PeerState_eGetNode:
		*instruction = &M_GetNode;
		break;
	case PeerState_eSendNode:
		*instruction = &M_SendNode;
		result = dataNode();
		break;
	case PeerState_eGetMesh:
		*instruction = &M_GetMesh;
		break;
	case PeerState_eSendMesh:
		*instruction = &M_SendMesh;
		result = dataMesh();
		break;
	case PeerState_eGetCamera:
		*instruction = &M_GetCamera;
		break;
	case PeerState_eSendCamera:
		*instruction = &M_SendCamera;
		result = dataCamera();
		break;
	case PeerState_eGetLight:
		*instruction = &M_GetLight;
		break;
	case PeerState_eSendLight:
		*instruction = &M_SendLight;
		result = dataLight();
		break;
	case PeerState_eSendTexture:
		*instruction = &M_SendTexture;
		result = dataTexture();
		break;
	case PeerState_eSendMaterial:
		*instruction = &M_SendMaterial;
		result = dataMaterial();
		break;
	default:
		*instruction = &M_Undefined;
	}

	return result;
}

}

#undef SNPRINTF