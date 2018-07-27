// Copyright Foundry, 2017. All rights reserved.

#pragma once

#include <vector>
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>

#include "Runnable.h"
#include "Misc/ScopedSlowTask.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#include "zmq.h"

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif

#include "common/NeutralPeer.h"
#include "ModoBridgeHandler.h"

class Client : public Foundry::NeutralPeer, public FRunnable
{
public:
	Client();
	Client(const std::string& server, const uint16 port);

	~Client();

	void				begin(ModoBridgeHandler* bridgeHandler);

	// FRunnable Interface.
	virtual bool		Init();
	virtual uint32		Run();
	virtual void		Stop();
	virtual void		Exit();

	void				setUEWindow(void* hwnd);
	bool				reconnect();
	void				validateSelection();
	void				pushRequest(bool selected) override;
	void				pushRequestFinish() override;
	bool				isPushing();
	void				endProgressBar();

private:
	const FlatBufferBuilder* dataNode() override;
	const FlatBufferBuilder* dataMesh() override;

	// Process functions must be very cheap, anything expensive should issue an async thread.
	// These founctions used as first tier just above network communication, any expensive will
	// result in network timeout.
	void processMeshData(uint8_t* data) override;
	void processNodeData(uint8_t* data) override;
	void processCameraData(uint8_t* data) override;
	void processLightData(uint8_t* data) override;
	void processTextureData(uint8_t* data) override;
	void processMaterialData(uint8_t* data) override;
	void processWhenUpdate(uint8_t* data) override;
	void processWhenClear(uint8_t* data) override;
	void processWhenIdle(uint8_t* data) override;
	void processState(Foundry::PeerState state);

	std::mutex						_updateMutex;
	ModoBridgeHandler*				_bridgeController;
	void*							_hwnd;
	bool							_sendSelected;
	bool							_sendSelectedRequest;

private:
	void rebuildNode(bool selected);
	void rebuildMesh(bool selected);
	void flushLazyDeleteList();
	void doProgressBarForPushing();

	std::vector<flatbuffers::FlatBufferBuilder* >	_nodeBuilders;
	std::vector<flatbuffers::FlatBufferBuilder* >	_meshBuilders;
	std::queue<flatbuffers::FlatBufferBuilder* >	_nodeBuilderQueue;
	std::queue<flatbuffers::FlatBufferBuilder* >	_meshBuilderQueue;

	std::vector<AStaticMeshActor*>	_meshes;
	std::vector<AStaticMeshActor*>	_selectedMeshes;
	std::vector<zmq_msg_t*>			_msgLazyDeleteList;

	FScopedSlowTask					*_ProgressBarForPushing;
	int								_progressBarAmount;
};


class PeerController
{
public:
	static PeerController*	activeInstance();
	static void				destroyInstance();
	bool					isRunning();
	void					startUp(ModoBridgeHandler* bridgeHandler, const std::string& server, const uint16 port);
	void					shutDown();
	void					setUEWindow(void* hwnd);
	void					pushAll();
	void					pushSelected();
	bool					isPushing();
	void					endProgressBar();

private:
	PeerController();
	~PeerController();

	FRunnableThread*		_thread;
	Client*					_peer;
	bool					_pushed;
};