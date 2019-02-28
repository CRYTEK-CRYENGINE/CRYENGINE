// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryDurangoLiveLobby.h"

#if USE_DURANGOLIVE && USE_CRY_MATCHMAKING
	#using <windows.winmd>
	#using <platform.winmd>
	#using <Microsoft.Xbox.Services.winmd>
	#using <Microsoft.Xbox.GameChat.winmd>

	#include <CryCore/TypeInfo_impl.h>

	#include "CryDurangoLiveMatchMaking.h"
	#include "CryDurangoLiveLobbyPacket.h"

	#include <CryCore/Platform/IPlatformOS.h>

	#define TDM_SESSION_CREATE_SESSION_DATA     0
	#define TDM_SESSION_CREATE_USERDATA         1
	#define TDN_SESSION_CREATE_NUMUSERDATA      1

	#define TDM_SESSION_UPDATE_USERDATA         0
	#define TDN_SESSION_UPDATE_NUMUSERDATA      0

	#define TDM_SESSION_SEARCH_PARAM            0
	#define TDM_SESSION_SEARCH_PARAMDATA        1
	#define TDN_SESSION_SEARCH_PARAMNUMDATA     1

	#define TDN_UPDATE_SLOTS_PUBLIC             0
	#define TDN_UPDATE_SLOTS_PRIVATE            1

	#define TDM_SESSION_JOIN_CONNECTION_COUNTER 0
	#define CONNECTION_JOIN_SEND_INTERVAL       1000
	#define CONNECTION_JOIN_MAX_SEND_TIMES      30

	#define GET_REMOTE_SDA_TIMEOUT              20000

	#define SESSION_ID_ZERO                     0

	#pragma warning(push)
	#pragma warning(disable:6011 6102 6246)

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using namespace Microsoft::WRL;
using namespace ABI::Windows::Xbox::Networking;
using namespace ABI::Windows::Xbox::System;

// XXX - ATG
	#include "DurangoChat\ChatIntegrationLayer.h"

static const GUID SERVICECONFIG_GUID =
{
	0xe1039253, 0x2550, 0x49c7, { 0xb7, 0x85, 0x49, 0x34, 0xf0, 0x78, 0xc6, 0x85 }
};

CCryDurangoLiveMatchMaking::CCryDurangoLiveMatchMaking(CCryLobby* pLobby, CCryLobbyService* pService, ECryLobbyService serviceType) : CCryMatchMaking(pLobby, pService, serviceType), m_user(nullptr)
{
	m_AssociationIncoming.value = 0;

	// Make the CCryMatchMaking base pointers point to our data so we can use the common code in CCryMatchMaking
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		CCryMatchMaking::m_sessions[i] = &m_sessions[i];
		CCryMatchMaking::m_sessions[i]->localConnection = &m_sessions[i].localConnection;

		for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
		{
			CCryMatchMaking::m_sessions[i]->remoteConnection[j] = &m_sessions[i].remoteConnection[j];
		}
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		CCryMatchMaking::m_task[i] = &m_task[i];
	}

}

CryLobbySessionHandle CCryDurangoLiveMatchMaking::FindSessionFromID(GUID const& sessionID)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		if (m_sessions[i].localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			if (IsEqualGUID(m_sessions[i].sessionID, sessionID))
			{
				return i;
			}
		}
	}

	return CryLobbyInvalidSessionHandle;
}

void CCryDurangoLiveMatchMaking::StartAsyncActionTask(CryMatchMakingTaskID mmTaskID, IAsyncAction* pAsyncAction)
{
	STask* pTask = &m_task[mmTaskID];

	pTask->asyncTaskState = STask::eATS_Pending;

	StartConcurrencyTaskTask(mmTaskID, ABI::Concurrency::task_from_async(pAsyncAction));
}

void CCryDurangoLiveMatchMaking::StartConcurrencyTaskTask(CryMatchMakingTaskID mmTaskID, concurrency::task<HRESULT> pConcTask)
{
	STask* pTask = &m_task[mmTaskID];

	pTask->asyncTaskState = STask::eATS_Pending;

	concurrency::cancellation_token_source src;
	pConcTask.then([this, mmTaskID](HRESULT res)
	{
		STask* pTask = &m_task[mmTaskID];
		if (SUCCEEDED(res))
		{
		  pTask->asyncTaskState = STask::eATS_Success;
		  return;
		}

		pTask->asyncTaskState = STask::eATS_Failed;
		UpdateTaskError(mmTaskID, CryDurangoLiveLobbyGetErrorFromDurangoLive(res));
	}, src.get_token(), concurrency::task_continuation_context::use_current());
}

ECryLobbyError CCryDurangoLiveMatchMaking::StartTask(ETask etask, bool startRunning, CryMatchMakingTaskID* pMMTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCB, void* pCBArg)
{
	CryMatchMakingTaskID tmpMMTaskID;
	CryMatchMakingTaskID* pUseMMTaskID = pMMTaskID ? pMMTaskID : &tmpMMTaskID;
	ECryLobbyError error = CCryMatchMaking::StartTask(etask, startRunning, pUseMMTaskID, pLTaskID, h, pCB, pCBArg);

	if (error == eCLE_Success)
	{
		ResetTask(*pUseMMTaskID);
	}

	return error;
}

void CCryDurangoLiveMatchMaking::ResetTask(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::StartSubTask(ETask etask, CryMatchMakingTaskID mmTaskID)
{
	CCryMatchMaking::StartSubTask(etask, mmTaskID);
}

void CCryDurangoLiveMatchMaking::StartTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		ECryLobbyError error;

		if (!m_lobby->DebugOKToStartTaskRunning(pTask->lTaskID))
		{
			return;
		}

		if (m_lobby->DebugGenerateError(pTask->lTaskID, error))
		{
			UpdateTaskError(mmTaskID, error);
			StopTaskRunning(mmTaskID);

			if ((pTask->startedTask == eT_SessionCreate) || (pTask->startedTask == eT_SessionJoin))
			{
				FreeSessionHandle(pTask->session);
			}

			return;
		}
	#endif

		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_SessionRegisterUserData:
		case eT_SessionMigrate:
			StopTaskRunning(mmTaskID);
			break;

		case eT_SessionGetUsers:
			StartSessionGetUsers(mmTaskID);
			break;

		case eT_SessionCreate:
			StartSessionCreate(mmTaskID);
			break;

		case eT_SessionUpdate:
			StartSessionUpdate(mmTaskID);
			break;

		case eT_SessionUpdateSlots:
			StartSessionUpdateSlots(mmTaskID);
			break;

		case eT_SessionStart:
			StartSessionStart(mmTaskID);
			break;

		case eT_SessionEnd:
			StartSessionEnd(mmTaskID);
			break;

		case eT_SessionDelete:
			StartSessionDelete(mmTaskID);
			break;

		case eT_SessionSearch:
			StartSessionSearch(mmTaskID);
			break;

		case eT_SessionJoin:
			StartSessionJoin(mmTaskID);
			break;

	#if NETWORK_HOST_MIGRATION
		case eT_SessionMigrateHostStart:
			HostMigrationStartNT(mmTaskID);
			break;

		case eT_SessionMigrateHostServer:
			HostMigrationServerNT(mmTaskID);
			break;
	#endif

		case eT_SessionSetLocalUserData:
			StartSessionSetLocalUserData(mmTaskID);
			break;

		case eT_SessionQuery:
			StartSessionQuery(mmTaskID);
			break;
		}
	}
}

void CCryDurangoLiveMatchMaking::EndTask(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
	#if NETWORK_HOST_MIGRATION
		if (pTask->startedTask & HOST_MIGRATION_TASK_ID_FLAG)
		{
			m_sessions[pTask->session].hostMigrationInfo.m_taskID = CryLobbyInvalidTaskID;
		}
	#endif

		if (pTask->cb)
		{
			switch (pTask->startedTask)
			{
			case eT_SessionRegisterUserData:
			case eT_SessionUpdate:
			case eT_SessionUpdateSlots:
			case eT_SessionStart:
			case eT_SessionEnd:
			case eT_SessionSetLocalUserData:
			case eT_SessionDelete:
	#if NETWORK_HOST_MIGRATION
			case eT_SessionMigrateHostStart:
	#endif
				((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
				break;

			case eT_SessionGetUsers:
				EndSessionGetUsers(mmTaskID);
				break;

			case eT_SessionMigrate:                                                           // From the games point of view a migrate is the same as a create
				((CryMatchmakingSessionCreateCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, m_sessions[pTask->session].localConnection.uid), pTask->cbArg);
				break;

			case eT_SessionCreate:
				EndSessionCreate(mmTaskID);
				break;

			case eT_SessionSearch:
				EndSessionSearch(mmTaskID);
				break;

			case eT_SessionJoin:
				EndSessionJoin(mmTaskID);
				break;

			case eT_SessionQuery:
				EndSessionQuery(mmTaskID);
				break;
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Lobby] EndTask %d (%d) error %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		// Clear LIVE specific task state so that base class tasks can use this slot
		ResetTask(mmTaskID);

		FreeTask(mmTaskID);
	}
}

void CCryDurangoLiveMatchMaking::StopTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
		pTask->running = false;
		TO_GAME_FROM_LOBBY(&CCryDurangoLiveMatchMaking::EndTask, this, mmTaskID);
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::CreateSessionHandle(CryLobbySessionHandle* pH, bool host, uint32 createFlags, int numUsers)
{
	ECryLobbyError error = CCryMatchMaking::CreateSessionHandle(pH, host, createFlags, numUsers);

	if (error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[*pH];

		pSession->localConnection.name[0] = 0;
		memset(pSession->localConnection.userData, 0, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
		memset(&pSession->sessionID, 0, sizeof(pSession->sessionID));

		if (!m_user)
		{
			return eCLE_UserNotSignedIn;
		}

		pSession->localConnection.xuid = m_user->Xuid();
		pSession->localConnection.uid.m_uid = static_cast<uint16>(m_user->Xuid());
	}

	return error;
}

void CCryDurangoLiveMatchMaking::CreateSecureDeviceAssociationHandler(ISecureDeviceAssociation* pSecureDeviceAssociation, CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

	pConnection->pSecureDeviceAssociation = pSecureDeviceAssociation;
	if (!pConnection->pSecureDeviceAssociation)
	{
		return;
	}

	pConnection->state = SSession::SRConnection::eRCS_CreateAssociationSuccess;

	SOCKADDR_STORAGE remoteSocketAddress;
	pConnection->pSecureDeviceAssociation->GetRemoteSocketAddressBytes(sizeof(remoteSocketAddress), reinterpret_cast<BYTE*>(&remoteSocketAddress));

	TNetAddress addr = TNetAddress(SSockAddrStorageAddr(remoteSocketAddress));
	NetLog("[Lobby] Create Secure Device Association Handler from: " PRFORMAT_ADDR, PRARG_ADDR(addr));

	CryLobbyConnectionID connectionID = m_lobby->FindConnection(addr);

	if (connectionID == CryLobbyInvalidConnectionID)
	{
		connectionID = m_lobby->CreateConnection(addr);
	}

	if (connectionID != CryLobbyInvalidConnectionID)
	{
		m_lobby->ConnectionAddRef(connectionID);

		pConnection->connectionID = connectionID;
	}
}

void CCryDurangoLiveMatchMaking::StartCreateSecureDeviceAssociation(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

	ComPtr<IAsyncOperation<SecureDeviceAssociation*>> pCreateAssociationOperation;
	if (!m_pSecureDeviceAssociationTemplate)
	{
		pConnection->state = SSession::SRConnection::eRCS_CreateAssociationFailed;
		return;
	}

	NetLog("[Lobby] StartCreateSecureDeviceAssociation creating association");
	m_pSecureDeviceAssociationTemplate->CreateAssociationAsync(pSession->remoteConnection[id].pSecureDeviceAddress.Get(), CreateSecureDeviceAssociationBehavior_Default, &pCreateAssociationOperation);

	pConnection->state = SSession::SRConnection::eRCS_CreatingAssociation;

	enum
	{
		Result = 0,
		Association
	};

	ABI::Concurrency::task_from_async(pCreateAssociationOperation.Get()).then(
	  [this, h, id](std::tuple<HRESULT, ComPtr<ISecureDeviceAssociation>> res)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
		NetLog("[Lobby] StartCreateSecureDeviceAssociation create association result 0x%08x", std::get<Result>(res));

		if (FAILED(std::get<Result>(res)))
		{
		  pConnection->state = SSession::SRConnection::eRCS_CreateAssociationFailed;
		  return;
		}
		CreateSecureDeviceAssociationHandler(std::get<Association>(res).Get(), h, id);
	});
}

CryMatchMakingConnectionID CCryDurangoLiveMatchMaking::AddRemoteConnection(CryLobbySessionHandle h, ISecureDeviceAddress* pSecureDeviceAddress, ISecureDeviceAssociation* pSecureDeviceAssociation, Live::Xuid xuid)
{
	SSession* pSession = &m_sessions[h];

	SCryMatchMakingConnectionUID remoteUID;
	{
		remoteUID.m_sid = GetSIDFromSessionHandle(h);
		remoteUID.m_uid = static_cast<uint16>(xuid);
	}

	CryMatchMakingConnectionID id = CCryMatchMaking::AddRemoteConnection(h, CryLobbyInvalidConnectionID, remoteUID, 1);

	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

		pConnection->flags |= CMMRC_FLAG_PLATFORM_DATA_VALID;
		pConnection->xuid = xuid;

		if (pSecureDeviceAssociation)
		{
			SOCKADDR_STORAGE remoteSocketAddress;

			pConnection->state = SSession::SRConnection::eRCS_CreateAssociationSuccess;
			pSecureDeviceAssociation->get_RemoteSecureDeviceAddress(&pConnection->pSecureDeviceAddress);
			pConnection->pSecureDeviceAssociation = pSecureDeviceAssociation;
			pConnection->pSecureDeviceAssociation->GetRemoteSocketAddressBytes(sizeof(remoteSocketAddress), reinterpret_cast<BYTE*>(&remoteSocketAddress));

			TNetAddress addr = TNetAddress(SSockAddrStorageAddr(remoteSocketAddress));
			NetLog("[Lobby] Adding Remote connection address: " PRFORMAT_ADDR, PRARG_ADDR(addr));

			CryLobbyConnectionID connectionID = m_lobby->FindConnection(addr);

			if (connectionID == CryLobbyInvalidConnectionID)
			{
				connectionID = m_lobby->CreateConnection(addr);
			}

			if (connectionID != CryLobbyInvalidConnectionID)
			{
				m_lobby->ConnectionAddRef(connectionID);

				pConnection->connectionID = connectionID;

				return id;
			}
		}
		else
		{
			if (pSecureDeviceAddress)
			{
				pConnection->pSecureDeviceAddress = pSecureDeviceAddress;

				return id;
			}
		}
	}

	if (id != CryMatchMakingInvalidConnectionID)
	{
		FreeRemoteConnection(h, id);
	}

	return CryMatchMakingInvalidConnectionID;
}

void CCryDurangoLiveMatchMaking::FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserLeave, h, id);

			if (pConnection->pSecureDeviceAssociation)
			{
				ComPtr<IAsyncAction> action;
				pConnection->pSecureDeviceAssociation->DestroyAsync(&action);
			}

			pConnection->pSecureDeviceAddress = nullptr;
			pConnection->pSecureDeviceAssociation = nullptr;

			pConnection->xuid = 0;
			pConnection->name[0] = 0;

			CCryMatchMaking::FreeRemoteConnection(h, id);
		}
	}
}

concurrency::task<HRESULT> CCryDurangoLiveMatchMaking::FreeRemoteConnectionAsync(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	if (id == CryMatchMakingInvalidConnectionID)
		return concurrency::create_task([](){ return S_OK; });

	SSession* pSession = &m_sessions[h];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

	if (!pConnection->used)
		return concurrency::create_task([](){ return S_OK; });

	// XXX - ATG
	GetChatIntegrationLayer()->RemoveRemoteConnection(h, id, pConnection->uid.m_uid);

	SessionUserDataEvent(eCLSE_SessionUserLeave, h, id);

	ComPtr<IAsyncAction> action;
	if (pConnection->pSecureDeviceAssociation)
	{
		pConnection->pSecureDeviceAssociation->DestroyAsync(&action);
	}

	pConnection->pSecureDeviceAddress = nullptr;
	pConnection->pSecureDeviceAssociation = nullptr;

	pConnection->xuid = 0;
	pConnection->name[0] = 0;

	CCryMatchMaking::FreeRemoteConnection(h, id);
	if (action.Get())
	{
		return ABI::Concurrency::task_from_async(action);
	}

	return concurrency::create_task([](){ return S_OK; });
}

uint64 CCryDurangoLiveMatchMaking::GetSIDFromSessionHandle(CryLobbySessionHandle h)
{

	CRY_ASSERT_MESSAGE((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED), "CCryDurangoLiveMatchMaking::GetSIDFromSessionHandle: invalid session handle");

	uint64 temp;

	memcpy(&temp, &m_sessions[h].sessionID.Data4, sizeof(temp));

	return temp;

}

void CCryDurangoLiveMatchMaking::Tick(CTimeValue tv)
{
	CCryMatchMaking::Tick(tv);

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; ++i)
	{
		STask* pTask = &m_task[i];

	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		if (pTask->used)
		{
			if (m_lobby->DebugTickCallStartTaskRunning(pTask->lTaskID))
			{
				StartTaskRunning(i);
				continue;
			}
			if (!m_lobby->DebugOKToTickTask(pTask->lTaskID, pTask->running))
			{
				continue;
			}
		}
	#endif

		if (pTask->used)
		{
			if (pTask->running)
			{
				switch (pTask->subTask)
				{
				case eT_SessionCreate:
					TickSessionCreate(i);
					break;

				case eT_SessionUpdate:
					TickSessionUpdate(i);
					break;

				case eT_SessionUpdateSlots:
					TickSessionUpdateSlots(i);
					break;

				case eT_SessionStart:
					TickSessionStart(i);
					break;

				case eT_SessionEnd:
					TickSessionEnd(i);
					break;

				case eT_SessionDelete:
					TickSessionDelete(i);
					break;

				case eT_SessionSearch:
					TickSessionSearch(i);
					break;

				case eT_SessionJoin:
					TickSessionJoin(i);
					break;

				case eT_SessionJoinCreateAssociation:
					TickSessionJoinCreateAssociation(i);
					break;

				case eT_SessionRequestJoin:
					TickSessionRequestJoin(i);
					break;

	#if NETWORK_HOST_MIGRATION
				case eT_SessionMigrateHostStart:
					TickHostMigrationStartNT(i);
					break;

				case eT_SessionMigrateHostServer:
					TickHostMigrationServerNT(i);
					break;
	#endif
				default:
					TickBaseTask(i);
					break;
				}
			}
		}
	}

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* session = &m_sessions[i];

		if (session->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			// Check what Live thinks the status of our connections is
			for (uint32 j = 0; j < MAX_LOBBY_CONNECTIONS; j++)
			{
				SSession::SRConnection* pConnection = &session->remoteConnection[j];

				if (pConnection->used)
				{
					if (m_lobby->ConnectionGetState(pConnection->connectionID) != eCLCS_NotConnected)
					{
						SecureDeviceAssociationState state = SecureDeviceAssociationState_Invalid;
						pConnection->pSecureDeviceAssociation->get_State(&state);
						if ((state == SecureDeviceAssociationState_DestroyingLocal) ||
						    (state == SecureDeviceAssociationState_DestroyingRemote) ||
						    (state == SecureDeviceAssociationState_Destroyed))
						{
							m_lobby->ConnectionSetState(pConnection->connectionID, eCLCS_NotConnected);
						}
					}
				}
			}
		}
	}

	if (CLobbyCVars::Get().p2pShowConnectionStatus)
	{
		LogConnectionStatus();
	}
}

void CCryDurangoLiveMatchMaking::LogConnectionStatus()
{
	static float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float xpos = 50;
	CryFixedStringT<128> buffer;

	for (uint32 sessionIndex = 0; sessionIndex < MAX_MATCHMAKING_SESSIONS; ++sessionIndex)
	{
		SSession* pSession = &m_sessions[sessionIndex];
		float ypos = 50;

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			for (uint32 connectionIndex = 0; connectionIndex < MAX_LOBBY_CONNECTIONS; connectionIndex++)
			{
				SSession::SRConnection* pConnection = &pSession->remoteConnection[connectionIndex];

				if (pConnection->used)
				{
					SecureDeviceAssociationState state = SecureDeviceAssociationState_Invalid;
					pConnection->pSecureDeviceAssociation->get_State(&state);
					switch (state)
					{
					case SecureDeviceAssociationState_Invalid:
						buffer.Format(PRFORMAT_UID " : Invalid", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_CreatingOutbound:
						buffer.Format(PRFORMAT_UID " : CreatingOutbound", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_CreatingInbound:
						buffer.Format(PRFORMAT_UID " : CreatingInbound", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_Ready:
						buffer.Format(PRFORMAT_UID " : Ready", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_DestroyingLocal:
						buffer.Format(PRFORMAT_UID " : DestroyingLocal", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_DestroyingRemote:
						buffer.Format(PRFORMAT_UID " : DestroyingRemote", PRARG_UID(pConnection->uid));
						break;

					case SecureDeviceAssociationState_Destroyed:
						buffer.Format(PRFORMAT_UID " : Destroyed", PRARG_UID(pConnection->uid));
						break;
					}

					if (gEnv->pRenderer)
					{
						gEnv->pRenderer->Draw2dLabel(xpos, ypos, 1.25f, white, false, buffer.c_str());
					}

					ypos += 20.0f;
				}
			}
		}

		xpos += 250.0f;
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::Initialise()
{
	ECryLobbyError error = CCryMatchMaking::Initialise();

	if (error == eCLE_Success)
	{
		m_registeredUserData.num = 0;

		HRESULT hr = Interface::Statics<ISecureDeviceAssociationTemplate>()->GetTemplateByName(Microsoft::WRL::Wrappers::HStringReference(L"PeerTraffic").Get(), &m_pSecureDeviceAssociationTemplate);

		// XXX - ATG
		error = GetChatIntegrationLayer()->Initialize(true, this);
	}

	return error;
}

ECryLobbyError CCryDurangoLiveMatchMaking::Terminate()
{
	// XXX - ATG
	ShutdownChatIntegrationLayer();

	ECryLobbyError error = CCryMatchMaking::Terminate();

	return error;
}

ECryLobbyError CCryDurangoLiveMatchMaking::SetINetworkingUser(Live::State::INetworkingUser* user)
{
	m_user = user;

	if (user == nullptr)
	{
		return eCLE_InvalidParam;
	}

	return eCLE_Success;
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionRegisterUserData(SCrySessionUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	if (numData < MAX_MATCHMAKING_SESSION_USER_DATA)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionRegisterUserData, false, &mmTaskID, pTaskID, CryLobbyInvalidSessionHandle, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			memcpy(m_registeredUserData.data, pData, numData * sizeof(pData[0]));
			m_registeredUserData.num = numData;

			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_OutOfSessionUserData;
	}

	NetLog("[Lobby] Start SessionRegisterUserData error %d", error);

	return error;
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionMigrate(CrySessionHandle gh, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	// Because we simply want to re-use the session that is already available, we don't need to do much here

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionMigrate, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionMigrate error %d", error);

	return error;
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionQuery(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionQuery, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionQuery error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionQuery(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

void CCryDurangoLiveMatchMaking::EndSessionQuery(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	((CryMatchmakingSessionQueryCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionGetUsers(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionGetUsers, false, &tid, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Lobby] Start SessionGetUsers error %d", error);
	}

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionGetUsers(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::EndSessionGetUsers(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SCryUserInfoResult temp;
		int a;

		// Glue in local user
		SCryDurangoLiveUserID* pID = new SCryDurangoLiveUserID(m_sessions[pTask->session].localConnection.xuid);

		if (pID)
		{
			temp.m_userID = pID;
			temp.m_conID = m_sessions[pTask->session].localConnection.uid;
			temp.m_isDedicated = false;           // Live does not support dedicated servers
			cry_strcpy(temp.m_userName, m_sessions[pTask->session].localConnection.name);
			memcpy(temp.m_userData, m_sessions[pTask->session].localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
			((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &temp, pTask->cbArg);
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}

		for (a = 0; (a < MAX_LOBBY_CONNECTIONS) && (pTask->error == eCLE_Success); a++)
		{
			if (m_sessions[pTask->session].remoteConnection[a].used)
			{
				pID = new SCryDurangoLiveUserID(m_sessions[pTask->session].remoteConnection[a].xuid);

				if (pID)
				{
					temp.m_userID = pID;
					temp.m_conID = m_sessions[pTask->session].remoteConnection[a].uid;
					cry_strcpy(temp.m_userName, m_sessions[pTask->session].remoteConnection[a].name);
					memcpy(temp.m_userData, m_sessions[pTask->session].remoteConnection[a].userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
					((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &temp, pTask->cbArg);
				}
				else
				{
					UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
				}
			}
		}
	}

	((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionSetLocalUserData(CrySessionHandle gh, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		if (dataSize <= CRYLOBBY_USER_DATA_SIZE_IN_BYTES)
		{
			SSession* pSession = &m_sessions[h];
			SSession::SLConnection* pLConnection = &pSession->localConnection;
			CryMatchMakingTaskID mmTaskID;

			memcpy(pLConnection->userData, pData, dataSize);

			error = StartTask(eT_SessionSetLocalUserData, false, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
			}
		}
		else
		{
			error = eCLE_OutOfUserData;
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionCreate(uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, true, flags, numUsers);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionCreate, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[mmTaskID];

			error = CreateTaskParamMem(mmTaskID, TDM_SESSION_CREATE_SESSION_DATA, pData, sizeof(SCrySessionData));

			if (error == eCLE_Success)
			{
				error = CreateTaskParamMem(mmTaskID, TDM_SESSION_CREATE_USERDATA, pData->m_data, pData->m_numData * sizeof(pData->m_data[0]));

				if (error == eCLE_Success)
				{
					pTask->numParams[TDN_SESSION_CREATE_NUMUSERDATA] = pData->m_numData;
					FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
				}
			}

			if (error != eCLE_Success)
			{
				FreeTask(mmTaskID);
				FreeSessionHandle(h);
			}
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionCreate error %d", error);

	return error;
}

// ~^~^~TODO: We should set up the Incoming SDA listener here.
void CCryDurangoLiveMatchMaking::StartSessionCreate(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionCreate(CryMatchMakingTaskID mmTaskID)
{
	using namespace ABI::Microsoft::Xbox::Services::Matchmaking;
	STask* pTask = &m_task[mmTaskID];

	// If we don't have an async running... {
	// If we have not looked in 5 seconds:
	if (m_user)
	{
		Live::Xuid xuids;
		HRESULT hr = m_user->GetRemoteXuids(1, &xuids, nullptr);

		SSession* pSession = &m_sessions[pTask->session];

		pSession->sessionID = m_user->GetSessionName();

		pSession->localConnection.uid.m_sid = GetSIDFromSessionHandle(pTask->session);
		pSession->localConnection.uid.m_uid = static_cast<uint16>(m_user->Xuid());

		if (m_AssociationIncoming.value == 0)
		{
			m_pSecureDeviceAssociationTemplate->add_AssociationIncoming(Callback<ITypedEventHandler<SecureDeviceAssociationTemplate*, SecureDeviceAssociationIncomingEventArgs*>>(
			                                                              [](ISecureDeviceAssociationTemplate* sender, ISecureDeviceAssociationIncomingEventArgs* args) -> HRESULT
			{
				UNREFERENCED_PARAMETER(sender);

				ComPtr<ISecureDeviceAssociation> SDAssoc;
				args->get_Association(&SDAssoc);

				SOCKADDR_STORAGE remoteSocketAddress;
				SDAssoc->GetRemoteSocketAddressBytes(sizeof(remoteSocketAddress), reinterpret_cast<BYTE*>(&remoteSocketAddress));
				TNetAddress addr = TNetAddress(SSockAddrStorageAddr(remoteSocketAddress));

				NetLog("[Lobby] Incoming Secure Device Association from: " PRFORMAT_ADDR, PRARG_ADDR(addr));

				return S_OK;
			}).Get(), &m_AssociationIncoming);
		}

		StopTaskRunning(mmTaskID);
	}
}

void CCryDurangoLiveMatchMaking::EndSessionCreate(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	((CryMatchmakingSessionCreateCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, m_sessions[pTask->session].localConnection.uid), pTask->cbArg);

	if (pTask->error == eCLE_Success)
	{
		InitialUserDataEvent(pTask->session);
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionUpdate(CrySessionHandle gh, SCrySessionUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryMatchMakingTaskID mmTaskID;

			error = StartTask(eT_SessionUpdate, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

			if (error == eCLE_Success)
			{
				STask* pTask = &m_task[mmTaskID];

				error = CreateTaskParamMem(mmTaskID, TDM_SESSION_UPDATE_USERDATA, pData, numData * sizeof(pData[0]));

				if (error == eCLE_Success)
				{
					pTask->numParams[TDN_SESSION_UPDATE_NUMUSERDATA] = numData;
					FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
				}
				else
				{
					FreeTask(mmTaskID);
				}
			}
		}
		else
		{
			error = eCLE_InvalidRequest;
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionUpdate error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionUpdate(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionUpdate(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionUpdateSlots(CrySessionHandle gh, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionUpdateSlots, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[mmTaskID];

			pTask->numParams[TDN_UPDATE_SLOTS_PUBLIC] = numPublic;
			pTask->numParams[TDN_UPDATE_SLOTS_PRIVATE] = numPrivate;

			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionUpdateSlots return %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionUpdateSlots(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionUpdateSlots(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionStart(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionStart, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionStart error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionStart(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionStart(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionEnd(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionEnd, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionEnd error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionEnd(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionEnd(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionDelete(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionDelete, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionDelete error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	// Disconnect our local connection
	SessionDisconnectRemoteConnectionViaNub(pTask->session, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_UserRequested, "Session deleted");
	SessionUserDataEvent(eCLSE_SessionUserLeave, pTask->session, CryMatchMakingInvalidConnectionID);

	auto tasks = std::make_shared<std::vector<concurrency::task<HRESULT>>>();

	// Free any remaining remote connections
	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			tasks->emplace_back(FreeRemoteConnectionAsync(pTask->session, i));
		}
	}

	FreeSessionHandle(pTask->session);

	// XXX - ATG
	GetChatIntegrationLayer()->Reset();

	if (m_user)
	{
		ComPtr<IAsyncAction> sessionDelete;
		HRESULT res = m_user->DeleteSessionAsync(&sessionDelete);
		if (SUCCEEDED(res))
		{
			tasks->emplace_back(ABI::Concurrency::task_from_async(sessionDelete.Get()).then(
			                      [sessionDelete](HRESULT res)
			{
				return res;
			}));
		}
	}

	m_pSecureDeviceAssociationTemplate->remove_AssociationIncoming(m_AssociationIncoming);
	m_AssociationIncoming.value = 0;

	StartConcurrencyTaskTask(mmTaskID, concurrency::when_all(tasks->begin(), tasks->end()).then(
	                           [tasks](std::vector<HRESULT> results)
	{
		for (auto const& result : results)
		{
		  if (FAILED(result))
				return result;
		}

		return S_OK;
	}));
}

void CCryDurangoLiveMatchMaking::TickSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	if (pTask->asyncTaskState != STask::eATS_Pending)
	{
		StopTaskRunning(mmTaskID);
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionSearch(uint32 user, SCrySessionSearchParam* pParam, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSearchCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	CryMatchMakingTaskID mmTaskID;

	ECryLobbyError error = StartTask(eT_SessionSearch, false, &mmTaskID, pTaskID, CryLobbyInvalidSessionHandle, pCB, pCBArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[mmTaskID];

		error = CreateTaskParamMem(mmTaskID, TDM_SESSION_SEARCH_PARAM, pParam, sizeof(SCrySessionSearchParam));

		if (error == eCLE_Success)
		{
			error = CreateTaskParamMem(mmTaskID, TDM_SESSION_SEARCH_PARAMDATA, pParam->m_data, pParam->m_numData * sizeof(pParam->m_data[0]));

			if (error == eCLE_Success)
			{
				pTask->numParams[TDN_SESSION_SEARCH_PARAMNUMDATA] = pParam->m_numData;
				FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
			}
		}

		if (error != eCLE_Success)
		{
			FreeTask(mmTaskID);
		}
	}

	NetLog("[Lobby] Start SessionSearch error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

void CCryDurangoLiveMatchMaking::TickSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}

void CCryDurangoLiveMatchMaking::EndSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	SCryDurangoLiveSessionID* id = nullptr;
	{
		if (m_user)
		{
			id = new SCryDurangoLiveSessionID(m_user->GetSessionName());
		}
	}

	if (id)
	{
		SCrySessionSearchResult result;
		SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];

		result.m_id = id;

		result.m_numFilledSlots = 0;
		result.m_data.m_numPublicSlots = 0;
		result.m_data.m_numPrivateSlots = 0;
		cry_strcpy(result.m_data.m_name, "Test");
		result.m_data.m_ranked = false;

		result.m_data.m_data = userData;
		result.m_data.m_numData = m_registeredUserData.num;

		result.m_ping = 0;
		result.m_numFriends = 0;
		result.m_flags = 0;

		for (uint32 i = 0; i < m_registeredUserData.num; i++)
		{
			result.m_data.m_data[i] = m_registeredUserData.data[i];
		}

		if (pTask->cb)
		{
			((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &result, pTask->cbArg);
		}
	}

	// Need to check again as task is cancelled if result from previous call is automatically accepted,
	// in which case cb will be nulled out by the previous callback.
	if (pTask->cb)
	{
		((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::SessionJoin(uint32* pUsers, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionJoinCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, false, flags, numUsers);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionJoin, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			SCryDurangoLiveSessionID* pDurangoLiveID = (SCryDurangoLiveSessionID*)id.get();
			SSession* pSession = &m_sessions[h];

			pSession->sessionID = pDurangoLiveID->m_sessionID;
			OLECHAR* bstrGuid;
			StringFromCLSID(pSession->sessionID, &bstrGuid);
			CryLogAlways("SessionJoin: SID: %s", bstrGuid);
			::CoTaskMemFree(bstrGuid);

			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionJoin error %d", error);

	return error;
}

void CCryDurangoLiveMatchMaking::StartSessionJoin(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	using namespace ABI::Microsoft::Xbox::Services::Matchmaking;
	STask* pTask = &m_task[mmTaskID];

	// If we don't have an async running... {
	// If we have not looked in 5 seconds:
	if (m_user)
	{
		Live::Xuid xuids;
		HRESULT hr = m_user->GetHostXuid(xuids);

		unsigned int addresses = 1;
		ComPtr<ISecureDeviceAddress> sdaddr;
		hr = m_user->GetHostSecureDeviceAddress(&sdaddr, &addresses);

		SSession* pSession = &m_sessions[pTask->session];
		pSession->localConnection.uid.m_sid = GetSIDFromSessionHandle(pTask->session);

		CryMatchMakingConnectionID const remoteId =
		  [&]()
			{
				pSession->hostConnectionID = AddRemoteConnection(pTask->session, sdaddr.Get(), nullptr, xuids);
				return pSession->hostConnectionID;
		  } ();

		if (remoteId != CryMatchMakingInvalidConnectionID)
		{
			StartSubTask(eT_SessionJoinCreateAssociation, mmTaskID);
			StartCreateSecureDeviceAssociation(pTask->session, remoteId);
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			StopTaskRunning(mmTaskID);
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
		StopTaskRunning(mmTaskID);
	}
}

void CCryDurangoLiveMatchMaking::TickSessionJoinCreateAssociation(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[pSession->hostConnectionID];

	if (pConnection->state == SSession::SRConnection::eRCS_CreateAssociationSuccess)
	{
		StartSubTask(eT_SessionRequestJoin, mmTaskID);
		pTask->timerStarted = false;
		pTask->numParams[TDM_SESSION_JOIN_CONNECTION_COUNTER] = 0;
	}

	if (pConnection->state == SSession::SRConnection::eRCS_CreateAssociationFailed)
	{
		UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
		StopTaskRunning(mmTaskID);
	}
}

void CCryDurangoLiveMatchMaking::TickSessionRequestJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (!pTask->TimerStarted() || (pTask->GetTimer() > CONNECTION_JOIN_SEND_INTERVAL))
	{
		if (pTask->numParams[TDM_SESSION_JOIN_CONNECTION_COUNTER] < CONNECTION_JOIN_MAX_SEND_TIMES)
		{
			SSession* pSession = &m_sessions[pTask->session];
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT64Size;
			uint8 buffer[MaxBufferSize];
			CCryDurangoLiveLobbyPacket packet;

			pTask->StartTimer();
			pTask->numParams[TDM_SESSION_JOIN_CONNECTION_COUNTER]++;

			Live::Xuid xuid = m_user->Xuid();

			packet.SetWriteBuffer(buffer, MaxBufferSize);
			packet.StartWrite(eDurangoLivePT_SessionRequestJoin, false);
			packet.WriteUINT8(mmTaskID);
			packet.WriteUINT64(xuid);

			TNetAddress addr;
			m_lobby->AddressFromConnection(addr, pSession->remoteConnection[pSession->hostConnectionID].connectionID);
			NetLog("[Lobby] TickSessionRequestJoin sending to " PRFORMAT_ADDR, PRARG_ADDR(addr));

			if (Send(mmTaskID, &packet, pTask->session, pSession->hostConnectionID) != eSE_Ok)
			{
				UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			StopTaskRunning(mmTaskID);
		}
	}
}

void CCryDurangoLiveMatchMaking::EndSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[pTask->session];

		((CryMatchmakingSessionJoinCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, pSession->localConnection.uid), 0, 0, pTask->cbArg);
		InitialUserDataEvent(pTask->session);

		return;
	}

	((CryMatchmakingSessionJoinCallback)pTask->cb)(pTask->lTaskID, pTask->error, CrySessionInvalidHandle, 0, 0, pTask->cbArg);
}

void CCryDurangoLiveMatchMaking::ProcessSessionRequestJoin(const TNetAddress& addr, CCryDurangoLiveLobbyPacket* pPacket)
{
	NetLog("[Lobby] ProcessSessionRequestJoin from " PRFORMAT_ADDR, PRARG_ADDR(addr));

	CryMatchMakingTaskID returnTaskID;

	returnTaskID = pPacket->ReadUINT8();
	Live::Xuid xuid = pPacket->ReadUINT64();

	if (!m_user)
		return;

	SSockAddrStorageAddr const* pSockAddrStorageAddr = boost::get<SSockAddrStorageAddr>(&addr);
	if (!pSockAddrStorageAddr)
		return;

	IDatagramSocketPtr pSocket = m_lobby->GetInternalSocket(m_serviceType);
	if (!pSocket)
		return;

	ComPtr<ISecureDeviceAssociation> pSecureDeviceAssociation;
	{
		sockaddr_storage localAddr;
		int localAddrLen = sizeof(localAddr);

		if (getsockname(pSocket->GetSysSocket(), (CRYSOCKADDR*)&localAddr, &localAddrLen) == 0)
		{
			HRESULT hr = Interface::Statics<ISecureDeviceAssociationStatics>()->GetAssociationBySocketAddressBytes(
			  sizeof(pSockAddrStorageAddr->addr),
			  reinterpret_cast<uint8*>(&const_cast<SSockAddrStorageAddr*>(pSockAddrStorageAddr)->addr),
			  localAddrLen,
			  reinterpret_cast<uint8*>(&localAddr),
			  &pSecureDeviceAssociation);
			CryLogAlways("CCryDurangoLiveMatchMaking::ProcessSessionRequestJoin - GetAssociationBySocketAddressBytes returned 0x%08x", hr);
		}
	}

	if (!pSecureDeviceAssociation)
		return;

	CryLobbySessionHandle h = FindSessionFromID(m_user->GetSessionName());
	if (h == CryLobbyInvalidSessionHandle)
		return;

	//Live::Xuid xuid;
	//m_user->GetRemoteXuids(1, &xuid, nullptr);

	SSession* pSession = &m_sessions[h];
	CryMatchMakingConnectionID id = AddRemoteConnection(h, nullptr, pSecureDeviceAssociation.Get(), xuid);

	if (id == CryMatchMakingInvalidConnectionID)
		return;

	SSession::SLConnection* pLocalConnection = &pSession->localConnection;
	SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + CryLobbyPacketErrorSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketConnectionUIDSize;
	uint8 buffer[MaxBufferSize];
	CCryDurangoLiveLobbyPacket packet;

	packet.SetWriteBuffer(buffer, MaxBufferSize);
	packet.StartWrite(eDurangoLivePT_SessionRequestJoinResult, true);
	packet.WriteUINT8(returnTaskID);
	packet.WriteError(eCLE_Success);

	// Add clients connection info.
	packet.WriteConnectionUID(pConnection->uid);

	// Add servers connection info.
	packet.WriteConnectionUID(pLocalConnection->uid);

	Send(CryMatchMakingInvalidTaskID, &packet, h, id);

	ConnectionEstablishedUserDataEvent(h, id);

	return;
}

void CCryDurangoLiveMatchMaking::ProcessSessionRequestJoinResult(const TNetAddress& addr, CCryDurangoLiveLobbyPacket* pPacket)
{
	CryMatchMakingTaskID mmTaskID = pPacket->ReadUINT8();

	mmTaskID = FindTaskFromTaskTaskID(eT_SessionRequestJoin, mmTaskID);

	if (mmTaskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[mmTaskID];
		ECryLobbyError error = pPacket->ReadError();

		UpdateTaskError(mmTaskID, error);
		StopTaskRunning(mmTaskID);
	}
}

uint32 CCryDurangoLiveMatchMaking::GetSessionIDSizeInPacket() const
{
	return 0;
}

ECryLobbyError CCryDurangoLiveMatchMaking::WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const
{
	return eCLE_Success;
}

CrySessionID CCryDurangoLiveMatchMaking::ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const
{
	return CrySessionInvalidID;
}

CrySessionID CCryDurangoLiveMatchMaking::SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle gh)
{
	return CrySessionInvalidID;
}

void CCryDurangoLiveMatchMaking::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCryDurangoLiveLobbyPacket* pDLPacket = (CCryDurangoLiveLobbyPacket*)pPacket;

	switch (pDLPacket->StartRead())
	{
	case eDurangoLivePT_SessionRequestJoin:
		ProcessSessionRequestJoin(addr, pDLPacket);
		break;

	case eDurangoLivePT_SessionRequestJoinResult:
		ProcessSessionRequestJoinResult(addr, pDLPacket);
		break;

	// XXX - ATG
	case eDurangoLivePT_Chat:
		GetChatIntegrationLayer()->ReceivePacket(pDLPacket);
		break;

	default:
		CCryMatchMaking::OnPacket(addr, pDLPacket);
		break;
	}
}

	#if NETWORK_HOST_MIGRATION
void CCryDurangoLiveMatchMaking::HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	SSession* pSession = &m_sessions[h];

	pSession->hostMigrationInfo.Reset();
}

void CCryDurangoLiveMatchMaking::HostMigrationFinalise(CryLobbySessionHandle h)
{
	SSession* pSession = &m_sessions[h];

	HostMigrationReset(h);

	if ((pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST) == 0)
	{
		MarkHostHintInformationDirty(h);
	}
}

ECryLobbyError CCryDurangoLiveMatchMaking::HostMigrationServer(SHostMigrationInfo* pInfo)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(pInfo->m_session);

	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[h];

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			CryMatchMakingTaskID mmTaskID;

			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST;
			pSession->hostConnectionID = CryMatchMakingInvalidConnectionID;
			error = StartTask(eT_SessionMigrateHostServer, false, &mmTaskID, NULL, h, NULL, NULL);

			if (error == eCLE_Success)
			{
				STask* pTask = &m_task[mmTaskID];
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
				DetectHostMigrationTaskBreak(h, "HostMigrationServer()");
		#endif
				pSession->hostMigrationInfo.m_taskID = pTask->lTaskID;

				FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
			}
		}
		else
		{
			error = eCLE_InvalidSession;
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		m_hostMigration.Terminate((SHostMigrationInfo_Private*)pInfo);
		NetLog("[Host Migration]: CCryLiveMatchMaking::HostMigrationServer(): " PRFORMAT_SH ", error %d", PRARG_SH(h), error);
	}

	return error;
}

void CCryDurangoLiveMatchMaking::HostMigrationServerNT(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickHostMigrationServerNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	pSession->hostMigrationInfo.m_matchMakingFinished = true;
	StopTaskRunning(mmTaskID);
}

bool CCryDurangoLiveMatchMaking::GetNewHostAddress(char* pAddress, SHostMigrationInfo* pInfo)
{
	return false;
}

void CCryDurangoLiveMatchMaking::HostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
}

void CCryDurangoLiveMatchMaking::TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
	StopTaskRunning(mmTaskID);
}
	#endif

ECryLobbyError CCryDurangoLiveMatchMaking::SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError rc = eCLE_Success;

	#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (h != CryLobbyInvalidSessionHandle)
	{
		CryMatchMakingTaskID mmTaskID;
		SSession* pSession = &m_sessions[h];
		rc = StartTask(eT_SessionMigrateHostStart, false, &mmTaskID, pTaskID, h, pCB, pCBArg);

		if (rc == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryDurangoLiveMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	#endif

	return rc;
}

void CCryDurangoLiveMatchMaking::SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED)
	{
		ComPtr<IUser> pUser;
		ISystem* pSystem = GetISystem();

		if (pSystem)
		{
			int userIndex = pSystem->GetPlatformOS()->GetFirstSignedInUser();
			if (userIndex != IPlatformOS::Unknown_User)
			{
				int userId = pSystem->GetPlatformOS()->UserGetId(userIndex);
				Interface::Statics<IUser>()->GetUserById(userId, &pUser);
			}
		}

		if (id == CryMatchMakingInvalidConnectionID)
		{
			SSession::SLConnection* pConnection = &pSession->localConnection;
			SCryDurangoLiveUserID* pID = new SCryDurangoLiveUserID(pConnection->xuid);

			if (pID)
			{
				SCryUserInfoResult userInfo;

				userInfo.m_conID = pConnection->uid;
				userInfo.m_isDedicated = false;           // Live does not support dedicated servers
				userInfo.m_userID = pID;
				cry_strcpy(userInfo.m_userName, pConnection->name);
				memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

				CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
			}

			if (event == eCLSE_SessionUserJoin)
			{
				if (pUser.Get())
				{
					// XXX - ATG
					auto user = reinterpret_cast<Windows::Xbox::System::User^>(pUser.Get());
					GetChatIntegrationLayer()->AddLocalUserToChatChannel(0, user);
				}
			}
			else if (event == eCLSE_SessionUserLeave)
			{
				if (pUser.Get())
				{
					// XXX - ATG
					auto user = reinterpret_cast<Windows::Xbox::System::User^>(pUser.Get());
					GetChatIntegrationLayer()->RemoveUserFromChatChannel(0, user);
				}
			}
		}
		else
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
			SCryDurangoLiveUserID* pID = new SCryDurangoLiveUserID(pConnection->xuid);

			if (pID)
			{
				SCryUserInfoResult userInfo;

				userInfo.m_conID = pConnection->uid;
				userInfo.m_isDedicated = false;           // Live does not support dedicated servers
				userInfo.m_userID = pID;
				cry_strcpy(userInfo.m_userName, pConnection->name);
				memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

				CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
			}

			if (event == eCLSE_SessionUserJoin)
			{
				// XXX - ATG
				GetChatIntegrationLayer()->AddRemoteConnection(h, id, pConnection->uid.m_uid);
			}
			else if (event == eCLSE_SessionUserLeave)
			{
				// XXX - ATG
				GetChatIntegrationLayer()->RemoveRemoteConnection(h, id, pConnection->uid.m_uid);
			}
		}
	}
}

void CCryDurangoLiveMatchMaking::ConnectionEstablishedUserDataEvent(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

	SessionUserDataEvent(eCLSE_SessionUserJoin, h, id);
}

void CCryDurangoLiveMatchMaking::InitialUserDataEvent(CryLobbySessionHandle h)
{
	SSession* pSession = &m_sessions[h];

	pSession->localFlags |= CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED;

	SessionUserDataEvent(eCLSE_SessionUserJoin, h, CryMatchMakingInvalidConnectionID);

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; ++i)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserJoin, h, i);
		}
	}
}

const char* CCryDurangoLiveMatchMaking::SSession::GetLocalUserName(uint32 localUserIndex) const
{
	return NULL;
}

void CCryDurangoLiveMatchMaking::SSession::Reset()
{
	CCryMatchMaking::SSession::Reset();

	sessionID = NULL_GUID;
	hostConnectionID = CryMatchMakingInvalidConnectionID;
}

const char* CCryDurangoLiveMatchMaking::GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const
{
	SSession::SRConnection* pPlatformConnection = reinterpret_cast<SSession::SRConnection*>(pConnection);

	if (pPlatformConnection->uid.m_uid == DEDICATED_SERVER_CONNECTION_UID)
	{
		return "DedicatedServer";
	}

	if (pPlatformConnection->used)
	{
		return pPlatformConnection->name;
	}

	return NULL;
}

uint64 CCryDurangoLiveMatchMaking::GetConnectionUserID(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const
{
	return INVALID_USER_ID;
}

TNetAddress CCryDurangoLiveMatchMaking::GetHostAddressFromSessionHandle(CrySessionHandle gh)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);
	SSession* pSession = &m_sessions[h];

	if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
	{
		const TNetAddress* pDedicatedServerAddr = m_lobby->GetNetAddress(pSession->remoteConnection[pSession->serverConnectionID].connectionID);

		if (pDedicatedServerAddr)
		{
			return *pDedicatedServerAddr;
		}
	}

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
	{
		return TNetAddress(TLocalNetAddress(m_lobby->GetInternalSocketPort(eCLS_Online)));
	}

	if (pSession->hostConnectionID != CryMatchMakingInvalidConnectionID)
	{
		const TNetAddress* pHostAddr = m_lobby->GetNetAddress(pSession->remoteConnection[pSession->hostConnectionID].connectionID);

		if (pHostAddr)
		{
			return *pHostAddr;
		}
	}

	return TNetAddress(SNullAddr());
}

	#pragma warning(pop)

#endif // USE_DURANGOLIVE && USE_CRY_MATCHMAKING
