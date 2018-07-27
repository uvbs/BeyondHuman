// Copyright Foundry, 2017. All rights reserved.

#include "UnrealClient.h"
#include <cassert>
#include <iostream>

#ifdef _WIN32
#include <Winuser.h>
#endif

#include "Editor.h"
#include "RunnableThread.h"
#include "Engine/Selection.h"
#include "Materials/Material.h"
#include "Async.h"

#include "ModoBridge.h"
#include "UnrealHelper.h"
#include "ModoBridgeMeshAssetData.h"
#include "common/NeutralPeer.h"

#define LOCTEXT_NAMESPACE "ModoBridge"

static PeerController *gPeerController = NULL;

PeerController::PeerController() :
	_thread(nullptr),
	_peer(nullptr),
	_pushed(false)
{

}

void PeerController::pushAll()
{
	if (_peer && _pushed == false)
	{
		_pushed = true;
		_peer->pushRequestPrepare(true, false, false);
		_peer->pushRequest(false);
		_peer->pushRequestFinish();
	}
}

void PeerController::pushSelected()
{
	if (_peer && _pushed == false)
	{
		_pushed = true;
		_peer->pushRequestPrepare(true, false, false);
		_peer->pushRequest(true);
		_peer->pushRequestFinish();
	}
}

bool PeerController::isPushing()
{
	if (_peer)
	{
		_pushed = _peer->isPushing();
	}
	
	return _pushed;
}

void PeerController::endProgressBar()
{
	if (_peer)
	{
		_peer->endProgressBar();
	}
}

void PeerController::startUp(ModoBridgeHandler* bridgeHandler, const std::string& server, const uint16 port)
{
	_pushed = false;

	if (_thread == nullptr && _peer == nullptr)
	{
		_peer = new Client(server, port);
		_peer->begin(bridgeHandler);

		FString ThreadName = "ModoBridgeUnrealClient";
		_thread = FRunnableThread::Create(_peer, *ThreadName, 0, TPri_BelowNormal);
	}
}

void PeerController::setUEWindow(void* hwnd)
{
	if (_peer)
	{
		_peer->setUEWindow(hwnd);
	}
}

void PeerController::shutDown()
{
	if (_peer && _thread)
	{
		_peer->Stop();
		_thread->WaitForCompletion();
		_thread->Kill();
		delete _thread;
		_thread = nullptr;

		delete _peer;
		_peer = nullptr;
	}
}

PeerController::~PeerController()
{

	shutDown();
}

bool PeerController::isRunning()
{
	return (_peer && _thread);
}

PeerController* PeerController::activeInstance()
{
	if (gPeerController == nullptr)
	{
		gPeerController = new PeerController();
	}

	return gPeerController;
}

void PeerController::destroyInstance()
{
	if (gPeerController)
	{
		delete gPeerController;
		gPeerController = nullptr;
	}
}

Client::Client() :
	NeutralPeer("127.0.0.1", 12000),
	FRunnable(),
	_updateMutex(),
	_bridgeController(nullptr),
	_hwnd(nullptr),
	_sendSelected(false),
	_sendSelectedRequest(false),
	_nodeBuilders(),
	_meshBuilders(),
	_nodeBuilderQueue(),
	_meshBuilderQueue(),
	_meshes(),
	_selectedMeshes(),
	_msgLazyDeleteList(),
	_ProgressBarForPushing(nullptr),
	_progressBarAmount(0)
{ 

}

Client::Client(const std::string& server, const uint16 port) :
	NeutralPeer(server, port),
	FRunnable(),
	_updateMutex(),
	_bridgeController(nullptr),
	_hwnd(nullptr),
	_sendSelected(false),
	_sendSelectedRequest(false),
	_ProgressBarForPushing(nullptr),
	_progressBarAmount(0)
{

}

Client::~Client()
{
	endProgressBar();
	_bridgeController = nullptr;

	Stop();
	Exit();

	if (_ctx)
	{
		zmq_ctx_destroy(_ctx);
		_ctx = nullptr;
	}

	for (auto& builder : _nodeBuilders)
	{
		builder->Clear();
		delete builder;
	}

	for (auto& builder : _meshBuilders)
	{
		builder->Clear();
		delete builder;
	}
}

void Client::endProgressBar()
{
	_updateMutex.lock();
	if (_ProgressBarForPushing != nullptr)
	{
		delete _ProgressBarForPushing;
		_ProgressBarForPushing = 0;
	}
	_updateMutex.unlock();
}

bool Client::isPushing()
{
	return _states.size() != 0 || _lazyStates.size() != 0;
}

void Client::begin(ModoBridgeHandler* bridgeHandler)
{
	_bridgeController = bridgeHandler;
}

bool Client::reconnect()
{
	if (_socket)
	{
		zmq_close(_socket);
		_socket = nullptr;
	}

	_socket = zmq_socket(_ctx, ZMQ_REQ);
	int rc = zmq_connect(_socket, _address.c_str());
	int zero = 0;
	zmq_setsockopt(_socket, ZMQ_LINGER, &zero, sizeof(zero));

	assert(rc == 0);

	if (rc != 0)
	{
		UE_LOG(ModoBridge, Warning, TEXT("Error occurred connecting to server: %s."), UTF8_TO_TCHAR(zmq_strerror(errno)));
		return false;
	}
	else
	{
		UE_LOG(ModoBridge, Log, TEXT("Successfully connected to %s."), UTF8_TO_TCHAR(_address.c_str()));
	}

	// Displaying progress bar blocks any user input, we need to kill it if time out.
	// Any better logic is welcome
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		PeerController::activeInstance()->endProgressBar();
	});

	return true;
}

void Client::doProgressBarForPushing()
{
	AsyncTask(ENamedThreads::GameThread, [&]()
	{
		if (_ProgressBarForPushing == nullptr)
		{
			_ProgressBarForPushing = new FScopedSlowTask(_progressBarAmount, LOCTEXT("PushingToModo", "Pushing To Modo"));
			_ProgressBarForPushing->MakeDialog();
		}
		else
		{
			_ProgressBarForPushing->EnterProgressFrame();
		}
	});
}

void Client::pushRequest(bool selected)
{
	// [TODO] Move to pushRequestPrepare?
	_bridgeController->updateSharedRegistry();
	validateSelection();

	_sendSelectedRequest = selected;
	_updateMutex.lock();

	rebuildNode(selected);
	rebuildMesh(selected);
	
	reset(_nodeBuilderQueue);
	reset(_meshBuilderQueue);

	_lazyStates.push(Foundry::PeerState_eClear);

	for (auto& meshBuilder : _meshBuilders)
	{
		_lazyStates.push(Foundry::PeerState_eSendMesh);
		_meshBuilderQueue.push(meshBuilder);
	}

	for (auto& nodeBuilder : _nodeBuilders)
	{
		_lazyStates.push(Foundry::PeerState_eSendNode);
		_nodeBuilderQueue.push(nodeBuilder);
	}

	_progressBarAmount = _meshBuilders.size() + _nodeBuilders.size();
}

void Client::pushRequestFinish()
{
	Foundry::NeutralPeer::pushRequestFinish();
	_updateMutex.unlock();
}

void Client::processState(Foundry::PeerState state)
{
	if (state == Foundry::PeerState_eClear)
	{
		_sendSelected = _sendSelectedRequest;
	}
	else if (state == Foundry::PeerState_eUpdate)
	{
		// When we sending update, it means we have pushing all the data, so we can terminate the progressBar

		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			PeerController::activeInstance()->endProgressBar();
		});

	}
}

const FlatBufferBuilder* Client::dataNode()
{
	if (!_nodeBuilderQueue.empty())
	{
		doProgressBarForPushing();
		auto data = _nodeBuilderQueue.front();
		_nodeBuilderQueue.pop();
		return data;
	}

	return nullptr;
}

const FlatBufferBuilder* Client::dataMesh()
{
	if (!_meshBuilderQueue.empty())
	{
		doProgressBarForPushing();
		auto data = _meshBuilderQueue.front();
		_meshBuilderQueue.pop();
		return data;
	}

	return nullptr;
}

void Client::rebuildNode(bool selected)
{
	const std::vector<AStaticMeshActor*> *meshesPtr;
	if (selected)
	{
		meshesPtr = &_selectedMeshes;
	}
	else
	{
		meshesPtr = &_meshes;
	}

	reset(_nodeBuilders, meshesPtr->size());
	
	FScopedSlowTask SlowTaskActor(meshesPtr->size(), LOCTEXT("ParsingActors", "Parsing Actors"));
	SlowTaskActor.MakeDialog();

	uint32 index = 0;
	for (auto it = meshesPtr->begin(); it != meshesPtr->end(); ++it)
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		AStaticMeshActor *Mesh = *it;

		// Always export MAX LOD
		std::string globalName;
		globalName = _bridgeController->itemGlobalName(UnrealHelper::GetLocalUniqueName(Mesh));

		auto builder = _nodeBuilders[index++];
		const char* uniqueName = globalName.c_str();
		const char* displayName = TCHAR_TO_UTF8(*Mesh->GetActorLabel());

		std::vector<float> transformation;
		std::vector<flatbuffers::Offset<flatbuffers::String>> meshes;
		std::vector<flatbuffers::Offset<flatbuffers::String>> cameras;
		std::vector<flatbuffers::Offset<flatbuffers::String>> lights;
		std::vector<flatbuffers::Offset<flatbuffers::String>> matOverrides;
		std::vector<flatbuffers::Offset<flatbuffers::String>> children;

		bool assetReset = false;
		if (Mesh->GetClass() == AStaticMeshActor::StaticClass())
		{
			AStaticMeshActor* staticMeshActor = Cast<AStaticMeshActor>(Mesh);

			if (staticMeshActor != nullptr)
			{
				UStaticMesh* mesh = staticMeshActor->GetStaticMeshComponent()->GetStaticMesh();

				if (mesh != nullptr)
				{
					auto localNameA = UnrealHelper::GetLocalUniqueName(mesh);
					auto uniqueNameA = _bridgeController->assetGlobalName(localNameA);
					meshes.push_back(builder->CreateString(uniqueNameA));
					assetReset = CoordinateSytemConverter::Get()->ResetMeshAsset(mesh);
				}
			}

			UMeshComponent* meshCompo = staticMeshActor->GetStaticMeshComponent();

			if (meshCompo != nullptr)
			{
				for (int i = 0; i < meshCompo->GetNumMaterials(); i++)
				{
					auto matInterface = meshCompo->GetMaterial(i);
					if (matInterface == nullptr)
					{
						continue;
					}

					auto material = matInterface->GetMaterial();
					//auto materialName = UnrealHelper::GetMaterialGlobalName(material);
					auto materialName = material->GetOuter()->GetName() + FString(".") + material->GetName();
					matOverrides.push_back (builder->CreateString(TCHAR_TO_UTF8(*materialName)));
				}
			}
		}

		FTransform rootXfrm = Mesh->GetActorTransform();
		FMatrix mat;
		// [TODO] Try to make it transparent.
		// If an asset mesh is reset in creation. We have to compensate mesh reset transformation to actor transformation before sending back to neutral
		if (assetReset)
		{
			FRotator myRotationValue = FRotator(0.0, 0.0, -90.0);
			Mesh->AddActorLocalRotation(myRotationValue);
			mat = Mesh->GetActorTransform().ToMatrixWithScale();
			Mesh->SetActorTransform(rootXfrm);
		}
		else
		{
			mat = rootXfrm.ToMatrixWithScale();
		}

		FMatrix convertedMatrix = mat.GetTransposed();
		convertedMatrix = CoordinateSytemConverter::Get()->ConvertTransfromUnrealToNeutral(convertedMatrix, assetReset);
		const float *data = (const float*)convertedMatrix.M;

		for (int i = 0; i < 16; i++)
		{
			transformation.push_back(data[i]);
		}

		// Add child
		TArray<AActor* > attachedActors;
		Mesh->GetAttachedActors(attachedActors);

		for (int i = 0; i < attachedActors.Num(); i++)
		{
			if (attachedActors[i]->GetClass() != AStaticMeshActor::StaticClass())
			{
				continue;
			}

			AStaticMeshActor* childMeshActor = Cast<AStaticMeshActor>(attachedActors[i]);

			if (childMeshActor)
			{
				auto childGlobalName = _bridgeController->itemGlobalName(UnrealHelper::GetLocalUniqueName(childMeshActor));
				children.push_back(builder->CreateString(childGlobalName));
			}
		}

		auto nodeData = Foundry::CreateNodeDataDirect(*builder, uniqueName, displayName,
			&transformation, &meshes, &cameras, &lights, &children, &matOverrides);

		builder->Finish(nodeData);

		SlowTaskActor.EnterProgressFrame();
	}
}

void Client::rebuildMesh(bool selected)
{
	const std::vector<AStaticMeshActor*> *meshesPtr;
	if (selected)
	{
		meshesPtr = &_selectedMeshes;
	}
	else
	{
		meshesPtr = &_meshes;
	}

	std::set<UStaticMesh*> staticMeshes;

	// Scan all static mesh actors and find their static meshes for parsing
	{
		FScopedSlowTask SlowTaskActor(meshesPtr->size(), LOCTEXT("SearchingMeshes", "Searching Meshes"));
		SlowTaskActor.MakeDialog();

		for (auto it = meshesPtr->begin(); it != meshesPtr->end(); ++it)
		{
			AStaticMeshActor *Mesh = *it;
			UStaticMesh* staticMeshPtr = Mesh->GetStaticMeshComponent()->GetStaticMesh();
			if (staticMeshPtr == nullptr)
			{
				continue;
			}

			const FStaticMeshLODResources& mesh = staticMeshPtr->GetLODForExport(0);
			// Verify the integrity of the static mesh.
			if (mesh.VertexBuffer.GetNumVertices() == 0 || mesh.Sections.Num() == 0)
			{
				continue;
			}

			staticMeshes.insert(staticMeshPtr);
			SlowTaskActor.EnterProgressFrame();
		}
	}

	reset(_meshBuilders, staticMeshes.size());

	FScopedSlowTask SlowTaskMesh(staticMeshes.size(), LOCTEXT("ParsingMeshes", "Parsing Meshes"));
	SlowTaskMesh.MakeDialog();

	uint32 index = 0;
	for (auto staticMeshPtr : staticMeshes)
	{
		const FStaticMeshLODResources& mesh = staticMeshPtr->GetLODForExport(0);
		bool assetReset = CoordinateSytemConverter::Get()->ResetMeshAsset(staticMeshPtr);

		// Remaps an Unreal vert to final reduced vertex list
		TArray<int32> VertRemap;
		TArray<int32> UniqueVerts;

		// [TODO] control Weld and UnWeld
		if (false)
		{
			// Weld verts
			UnrealHelper::DetermineVertsToWeld(VertRemap, UniqueVerts, mesh);
		}
		else
		{
			// Do not weld verts
			VertRemap.SetNum(mesh.VertexBuffer.GetNumVertices());
			for (int32 i = 0; i < VertRemap.Num(); i++)
			{
				VertRemap[i] = i;
			}
			UniqueVerts = VertRemap;
		}

		const uint32 VertexCount = VertRemap.Num();
		const uint32 PolygonsCount = mesh.Sections.Num();

		auto builder = _meshBuilders[index++];

		std::string globalName = _bridgeController->assetGlobalName(UnrealHelper::GetLocalUniqueName(staticMeshPtr));
		const char* uniqueName = globalName.c_str();
		const char* displayName = TCHAR_TO_UTF8(*staticMeshPtr->GetName());

		// TODO use CreateUninitializedVector, but make sure it works firstly
		std::vector<float> vertices;
		vertices.reserve(Foundry::DataStrideVertex * VertexCount);

		std::vector<uint32> indices;
		indices.reserve(Foundry::DataStrideIndex * PolygonsCount);

		std::vector<float> normals;
		normals.reserve(Foundry::DataStrideNormal * VertexCount);

		std::vector<float> uvs;
		uvs.reserve(Foundry::DataStrideUV * VertexCount);

		std::vector<float> colors;
		colors.reserve(Foundry::DataStrideColor * VertexCount);

		std::vector<float> tangents;
		tangents.reserve(Foundry::DataStrideTangent * VertexCount);

		std::vector<uint32> matIdices;
		matIdices.reserve(Foundry::DataStrideMatIndex * PolygonsCount);

		std::vector<flatbuffers::Offset<flatbuffers::String>> materialNames;

		for (uint32 PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
		{
			FIndexArrayView RawIndices = mesh.IndexBuffer.GetArrayView();
			const FStaticMeshSection& Polygons = mesh.Sections[PolygonsIndex];
			const uint32 TriangleCount = Polygons.NumTriangles;
			for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			{
				for (uint32 PointIndex = 0; PointIndex < 3; PointIndex++)
				{
					uint32 UnrealVertIndex = RawIndices[Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)];
					indices.push_back(UnrealVertIndex);
					matIdices.push_back(Polygons.MaterialIndex);
				}
			}
		}

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			FVector pos = mesh.PositionVertexBuffer.VertexPosition(VertexIndex);
			CoordinateSytemConverter::Get()->ConvertPositionUnrealToNeutral(pos, assetReset);

			vertices.push_back(pos.X);
			vertices.push_back(pos.Y);
			vertices.push_back(pos.Z);
		}

		for (uint32 NormalIndex = 0; NormalIndex < VertexCount; NormalIndex++)
		{
			FVector Normal = mesh.VertexBuffer.VertexTangentZ(NormalIndex);
			CoordinateSytemConverter::Get()->ConvertNormalUnrealToNeutral(Normal, assetReset);

			normals.push_back(Normal.X);
			normals.push_back(Normal.Y);
			normals.push_back(Normal.Z);
		}

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			for (uint32 UVSetIndex = 0; UVSetIndex < mesh.VertexBuffer.GetNumTexCoords(); UVSetIndex++)
			{
				FVector2D UV = mesh.VertexBuffer.GetVertexUV(VertexIndex, UVSetIndex);
				CoordinateSytemConverter::Get()->ConvertUVUnrealToNeutral(UV);

				uvs.push_back(UV.X);
				uvs.push_back(UV.Y);
			}
		}

		auto colorVertexCount = mesh.ColorVertexBuffer.GetNumVertices();
		for (uint32 VertexIndex = 0; VertexIndex < colorVertexCount; VertexIndex++)
		{
			const FColor& Color = mesh.ColorVertexBuffer.VertexColor(VertexIndex);
			colors.push_back(Color.R / 255.0);
			colors.push_back(Color.G / 255.0);
			colors.push_back(Color.B / 255.0);
		}

		for (uint32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			FVector Tangent = mesh.VertexBuffer.VertexTangentX(VertexIndex);
			CoordinateSytemConverter::Get()->ConvertNormalUnrealToNeutral(Tangent, assetReset);

			tangents.push_back(Tangent.X);
			tangents.push_back(Tangent.Y);
			tangents.push_back(Tangent.Z);

			FVector BiNormal = mesh.VertexBuffer.VertexTangentY(VertexIndex);
			CoordinateSytemConverter::Get()->ConvertNormalUnrealToNeutral(BiNormal, assetReset);

			tangents.push_back(BiNormal.X);
			tangents.push_back(BiNormal.Y);
			tangents.push_back(BiNormal.Z);
		}

		auto staticMaterials = staticMeshPtr->StaticMaterials;

		for (FStaticMaterial& staticMat : staticMaterials)
		{
			auto matInterface = staticMat.MaterialInterface;
			if (matInterface == nullptr)
			{
				continue;
			}

			auto mat = matInterface->GetMaterial();
			FString assetName = UnrealHelper::GetMaterialGlobalName(mat);
			materialNames.push_back(builder->CreateString(TCHAR_TO_UTF8(*assetName)));
		}

		auto meshData = Foundry::CreateMeshDataDirect(*builder, uniqueName, displayName,
			&indices, &vertices, &normals, &uvs, &colors, &tangents, &matIdices, &materialNames);

		builder->Finish(meshData);

		SlowTaskMesh.EnterProgressFrame();
	}
}

void Client::validateSelection()
{
	_meshes.clear();
	for (TActorIterator<AStaticMeshActor> ActorItr(_bridgeController->getWorld()); ActorItr; ++ActorItr)
	{
		AStaticMeshActor *Mesh = *ActorItr;
		_meshes.push_back(Mesh);
	}

	_selectedMeshes.clear();
	USelection* selection = GEditor->GetSelectedActors();
	for (int32 i = 0; i < selection->Num(); i++)
	{
		UObject* obj = selection->GetSelectedObject(i);

		if (obj == nullptr)
		{
			continue;
		}

		if (obj->GetClass() == AStaticMeshActor::StaticClass())
		{
			AStaticMeshActor *mesh = dynamic_cast<AStaticMeshActor*>(obj);
			if (mesh != nullptr)
			{
				_selectedMeshes.push_back(mesh);
			}
		}
	}
}

void Client::processMeshData(uint8_t* data)
{
	_bridgeController->parseObject(data);
}

void Client::processTextureData(uint8_t* data)
{
	_bridgeController->parseTexture(data);
}

void Client::processNodeData(uint8_t* data)
{
	_bridgeController->parseNode(data);
}

void Client::processCameraData(uint8_t* data)
{
	_bridgeController->parseCamera(data);
}

void Client::processLightData(uint8_t* data)
{
	_bridgeController->parseLight(data);
}

void Client::processMaterialData(uint8_t* data)
{
	_bridgeController->parseMaterial(data);
}

void Client::processWhenIdle(uint8_t* data)
{
	while (!_lazyStates.empty())
	{
		auto lazy = _lazyStates.front();
		_lazyStates.pop();
		_states.push(lazy);
	}

	// When server Idle, we only sleep when we have no pending things to do
	if (_states.size() == 0)
	{
		FPlatformProcess::Sleep(0.5);
	}
}

void Client::processWhenClear(uint8_t* data)
{
	_bridgeController->clearScene();
	flushLazyDeleteList();

	if (data != nullptr)
	{
		_bridgeController->parseConfig(data);
	}
}

void Client::flushLazyDeleteList()
{
	for (size_t i = 0; i < _msgLazyDeleteList.size(); i++)
	{
		zmq_msg_t* msg = _msgLazyDeleteList[i];
		zmq_msg_close(msg);
		delete msg;
	}

	_msgLazyDeleteList.clear();
}

void Client::processWhenUpdate(uint8_t* data)
{
		// active unreal4
#ifdef _WIN32
		HWND hwnd = (HWND)_hwnd;
		SwitchToThisWindow(hwnd, false);
		BringWindowToTop(hwnd);
		SetForegroundWindow(hwnd);
		ShowWindow(hwnd, SW_RESTORE);
#endif
		AsyncTask(ENamedThreads::GameThread, [&]()
		{
			_bridgeController->updateScene();
		});
}

uint32 Client::Run()
{
	assert(_socket);

	static int REQUEST_TIMEOUT = 2500;

	int timeout = REQUEST_TIMEOUT;
	int triedNum = 1;

	_running = true;

	while (_running)
	{
		flatbuffers::FlatBufferBuilder builder(64);
		const flatbuffers::FlatBufferBuilder* builderPtr = nullptr;
		Foundry::PeerState state;
		int32 rc;
		const std::string* instruction = nullptr;

		builderPtr = getSendData(&builder, &instruction, state);

		if (builderPtr == nullptr)
		{
			rc = zmq_send(_socket, instruction->data(), instruction->size() + 1, 0);
		}
		else
		{
			rc = zmq_send(_socket, instruction->data(), instruction->size() + 1, ZMQ_SNDMORE);
			rc = zmq_send(_socket, builderPtr->GetBufferPointer(), builderPtr->GetSize() + 1, 0);
		}

		processState(state);

		timeout = REQUEST_TIMEOUT * triedNum;

		zmq_pollitem_t items[] = { { _socket, 0, ZMQ_POLLIN, 0 } };
		zmq_poll(items, 1, timeout);

		if (items[0].revents & ZMQ_POLLIN)
		{
			zmq_msg_t msgHeader;
			zmq_msg_init(&msgHeader);

			state = Foundry::PeerState_eIdle;
			rc = zmq_msg_recv(&msgHeader, _socket, 0);
			if (rc != -1)
			{
				state = processHeader((uint8_t*)zmq_msg_data(&msgHeader));
			}

			int64 more;
			size_t more_size = sizeof(more);
			zmq_getsockopt(_socket, ZMQ_RCVMORE, &more, &more_size);

			if (more)
			{
				zmq_msg_t	*msgData = new zmq_msg_t();
				zmq_msg_init(msgData);
				rc = zmq_msg_recv(msgData, _socket, 0);

				if (rc != -1)
				{
					processData((uint8_t*)zmq_msg_data(msgData), state);
					// we use zero copy, since the data will be used in main thread
					// for creating items, we keep the data until importing is compeletly finished.
					_msgLazyDeleteList.push_back(msgData);
				}
				else
				{
					zmq_msg_close(msgData);
					delete msgData;
				}
			}
			else
			{
				processNoData(state);
			}

			triedNum = 1;
			zmq_msg_close(&msgHeader);
		}
		else
		{
			FString time = FString::FromInt(timeout);
			UE_LOG(ModoBridge, Log, TEXT("Request time out[%s], has the server stopped?"), *time);
			// [todo] if time out, we add request message back to the queue
			// 1, we don't pop?
			// 2, dequeue?
			// 3, free feature from zmq?
			if (triedNum < 3)
			{
				triedNum ++;
			}
			else
			{
				triedNum = 1;
				UE_LOG(ModoBridge, Log, TEXT("Try to reconnect to server"));
				reconnect();
			}
		}
	}

	return 0;
}

void Client::Stop()
{

	_running = false;
}

bool Client::Init()
{	
	if (_socket)
	{
		return true;
	}

	reconnect();
	return true;
}

void Client::Exit()
{
	if (_socket)
	{
		zmq_close(_socket);
		_socket = nullptr;
	}
}

	void
Client::setUEWindow(void* hwnd)
{
	_hwnd = hwnd;
}