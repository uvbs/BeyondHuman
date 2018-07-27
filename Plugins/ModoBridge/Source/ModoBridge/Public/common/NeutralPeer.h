#ifndef NEUTRALPEER_H
#define NEUTRALPEER_H

#include <string>
#include <queue>
#include "CommonTypes.h"

#if PLATFORM_WINDOWS
	#include "AllowWindowsPlatformTypes.h"
#endif

#include "zmq.h"

using flatbuffers::FlatBufferBuilder;

namespace Foundry
{

enum PushType
{
	PushType_eItems = 0,
	PushType_eTextures,
	PushType_eMaterials
};

enum PeerState
{
	PeerState_eIdle = 0,
	PeerState_eClear,
	PeerState_eUpdate, //eUpdate causes async function to be called, never assumes it is processed before the state underneath
	PeerState_eGetNode,
	PeerState_eSendNode,
	PeerState_eGetMesh,
	PeerState_eSendMesh,
	PeerState_eGetCamera,
	PeerState_eSendCamera,
	PeerState_eGetLight,
	PeerState_eSendLight,
	PeerState_eSendTexture,
	PeerState_eSendMaterial
};

// Neutral peer is very performance critical, make sure no expensive computation in any functions.
// Otherwise, reaching larger scale will give you time out from the network connection.
// Any expensive things should be executed via an async thread. Your message system should be
// aware, which part can be done asynchronously
class NeutralPeer
{
public:
	NeutralPeer(const std::string& server, int port);
	virtual ~NeutralPeer();

	virtual void setup(const std::string& server, int port);
	virtual void pushRequest(bool selected);
	virtual void pushRequestPrepare(bool item, bool material, bool texture);
	virtual void pushRequestFinish();

protected:
	virtual void processState(PeerState state);
	virtual const FlatBufferBuilder* getSendData(FlatBufferBuilder *builder, const std::string** instruction, PeerState& state);
	virtual PeerState processHeader(uint8_t* data);
	virtual void processData(uint8_t* data, PeerState state);
	virtual void processNoData(PeerState state);
	virtual void update();

	virtual const FlatBufferBuilder* dataNode();
	virtual const FlatBufferBuilder* dataMesh();
	virtual const FlatBufferBuilder* dataCamera();
	virtual const FlatBufferBuilder* dataLight();
	virtual const FlatBufferBuilder* dataTexture();
	virtual const FlatBufferBuilder* dataMaterial();
	virtual const FlatBufferBuilder* dataConfig();

	// Process functions must be very cheap, anything expensive should issue an async thread.
	// These founctions used as first tier just above network communication, any expensive will
	// result in network timeout.
	virtual void processMeshData(uint8_t* data);
	virtual void processNodeData(uint8_t* data);
	virtual void processCameraData(uint8_t* data);
	virtual void processLightData(uint8_t* data);
	virtual void processTextureData(uint8_t* data);
	virtual void processMaterialData(uint8_t* data);
	virtual void processWhenIdle(uint8_t* data);
	virtual void processWhenUpdate(uint8_t* data);
	virtual void processWhenClear(uint8_t* data);

	virtual void reset(std::vector<flatbuffers::FlatBufferBuilder* >& builders, unsigned int size);
	virtual void reset(std::queue<flatbuffers::FlatBufferBuilder* >& builders);

	int			_port;
	bool		_running;
	void*		_socket;
	void*		_ctx;
	std::string	_server;
	std::string _address;

	std::queue<PeerState>	_lazyStates;
	std::queue<PeerState>	_states;

	// used for push prepare, the states of these will alter when really pushing
	bool		_pushItems;
	bool		_pushTextures;
	bool		_pushMaterials;
};

}

#if PLATFORM_WINDOWS
	#include "HideWindowsPlatformTypes.h"
#endif

#endif