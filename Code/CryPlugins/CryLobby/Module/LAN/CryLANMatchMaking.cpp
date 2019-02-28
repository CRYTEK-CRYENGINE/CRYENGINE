// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryLANLobby.h"
#include "CryLANMatchMaking.h"
#include "CrySharedLobbyPacket.h"

#if USE_LAN

	#define LAN_SEARCH_INTERVAL          1000
	#define MAX_SEARCH_RETRIES           5

	#define SEARCH_PARAM_GAME_PARAM      0      // ptr
	#define SEARCH_PARAM_GAME_PARAM_DATA 1      // ptr

static uint64 GetLANUserID(SCryMatchMakingConnectionUID uid)
{
	return uid.m_uid;
}

CCryLANMatchMaking::CCryLANMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType) : CCryMatchMaking(lobby, service, serviceType)
{
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

ECryLobbyError CCryLANMatchMaking::StartTask(ETask etask, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg)
{
	CryMatchMakingTaskID tmpMMTaskID;
	CryMatchMakingTaskID* useMMTaskID = mmTaskID ? mmTaskID : &tmpMMTaskID;
	ECryLobbyError error = CCryMatchMaking::StartTask(etask, false, useMMTaskID, lTaskID, h, cb, cbArg);

	if (error == eCLE_Success)
	{
		ResetTask(*useMMTaskID);
	}

	return error;
}

void CCryLANMatchMaking::ResetTask(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	pTask->search = CryLANInvalidSearchHandle;
	pTask->ticks = 0;
}

void CCryLANMatchMaking::StartTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* task = &m_task[mmTaskID];

	if (task->used)
	{
	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		ECryLobbyError error;

		if (!m_lobby->DebugOKToStartTaskRunning(task->lTaskID))
		{
			return;
		}

		if (m_lobby->DebugGenerateError(task->lTaskID, error))
		{
			UpdateTaskError(mmTaskID, error);
			StopTaskRunning(mmTaskID);

			if ((task->startedTask == eT_SessionCreate) || (task->startedTask == eT_SessionJoin))
			{
				FreeSessionHandle(task->session);
			}

			if (task->startedTask == eT_SessionSearch)
			{
				m_search[task->search].used = false;
			}

			return;
		}
	#endif

		task->running = true;

		switch (task->startedTask)
		{
		case eT_SessionCreate:
		case eT_SessionMigrate:
		case eT_SessionRegisterUserData:
		case eT_SessionUpdate:
		case eT_SessionUpdateSlots:
		case eT_SessionStart:
		case eT_SessionEnd:
		case eT_SessionGetUsers:
			// All of these tasks are either synchronous or not needed for LAN but forcing them to wait a tick before succeeding
			// should ensure less surprises when the asynchronous online versions come along
			StopTaskRunning(mmTaskID);
			break;

		case eT_SessionDelete:
			StartSessionDelete(mmTaskID);
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

		case eT_SessionMigrateHostClient:
			TickHostMigrationClientNT(mmTaskID);
			break;
	#endif

		case eT_SessionSetLocalUserData:
			StartSessionSetLocalUserData(mmTaskID);
			break;
		}
	}
}

void CCryLANMatchMaking::EndTask(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* task = &m_task[mmTaskID];

	if (task->used)
	{
	#if NETWORK_HOST_MIGRATION
		//-- For host migration tasks, ensure we clear the lobby task handle in the host migration info when the task is ended.
		if (task->startedTask & HOST_MIGRATION_TASK_ID_FLAG)
		{
			m_sessions[task->session].hostMigrationInfo.m_taskID = CryLobbyInvalidTaskID;
		}
	#endif

		if (task->cb)
		{
			switch (task->startedTask)
			{
			case eT_SessionRegisterUserData:
			case eT_SessionUpdate:
			case eT_SessionUpdateSlots:
			case eT_SessionStart:
			case eT_SessionEnd:
			case eT_SessionDelete:
			case eT_SessionSetLocalUserData:
	#if NETWORK_HOST_MIGRATION
			case eT_SessionMigrateHostStart:
	#endif
				((CryMatchmakingCallback)task->cb)(task->lTaskID, task->error, task->cbArg);
				break;

			case eT_SessionQuery:
				((CryMatchmakingSessionQueryCallback)task->cb)(task->lTaskID, task->error, NULL, task->cbArg);
				break;

			case eT_SessionGetUsers:
				EndSessionGetUsers(mmTaskID);
				break;

			case eT_SessionCreate:
				((CryMatchmakingSessionCreateCallback)task->cb)(task->lTaskID, task->error, CreateGameSessionHandle(task->session, m_sessions[task->session].localConnection.uid), task->cbArg);

				if (task->error == eCLE_Success)
				{
					InitialUserDataEvent(task->session);
				}

				break;

			case eT_SessionMigrate:
				((CryMatchmakingSessionCreateCallback)task->cb)(task->lTaskID, task->error, CreateGameSessionHandle(task->session, m_sessions[task->session].localConnection.uid), task->cbArg);
				break;

			case eT_SessionSearch:
				((CryMatchmakingSessionSearchCallback)task->cb)(task->lTaskID, task->error, NULL, task->cbArg);
				break;

			case eT_SessionJoin:
				((CryMatchmakingSessionJoinCallback)task->cb)(task->lTaskID, task->error, CreateGameSessionHandle(task->session, m_sessions[task->session].localConnection.uid), m_sessions[task->session].id.m_ip, m_sessions[task->session].id.m_port, task->cbArg);

				if (task->error == eCLE_Success)
				{
					InitialUserDataEvent(task->session);
				}

				break;
			}
		}

		// Clear LAN specific task state so that base class tasks can use this slot
		ResetTask(mmTaskID);

		FreeTask(mmTaskID);

		if (task->error != eCLE_Success)
		{
			NetLog("[Lobby] EndTask %u (%u) error %d", task->startedTask, task->subTask, task->error);
		}
	}
}

void CCryLANMatchMaking::StopTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	STask* task = &m_task[mmTaskID];

	if (task->used)
	{
		task->running = false;
		TO_GAME_FROM_LOBBY(&CCryLANMatchMaking::EndTask, this, mmTaskID);
	}
}

ECryLobbyError CCryLANMatchMaking::CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers)
{
	ECryLobbyError error = CCryMatchMaking::CreateSessionHandle(h, host, createFlags, numUsers);

	if (error == eCLE_Success)
	{
		SSession* session = &m_sessions[*h];

		session->data.m_data = session->userData;
		session->numFilledSlots = numUsers;
		session->data.m_numData = m_registeredUserData.num;

		for (uint32 j = 0; j < m_registeredUserData.num; j++)
		{
			session->data.m_data[j] = m_registeredUserData.data[j];
		}
	}

	return error;
}

CryMatchMakingConnectionID CCryLANMatchMaking::AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 ip, uint16 port, uint32 numUsers, const char* name, uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES], bool isDedicated)
{
	if (connectionID == CryLobbyInvalidConnectionID)
	{
		TNetAddress netAddr = TNetAddress(SIPv4Addr(ntohl(ip), ntohs(port)));

		connectionID = m_lobby->FindConnection(netAddr);

		if (connectionID == CryLobbyInvalidConnectionID)
		{
			connectionID = m_lobby->CreateConnection(netAddr);
		}
	}

	CryMatchMakingConnectionID id = CCryMatchMaking::AddRemoteConnection(h, connectionID, uid, numUsers);

	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* connection = &pSession->remoteConnection[id];

		connection->flags |= CMMRC_FLAG_PLATFORM_DATA_VALID;
		connection->ip = ip;
		connection->port = port;
	#if NETWORK_HOST_MIGRATION
		connection->m_migrated = (pSession->hostMigrationInfo.m_state != eHMS_Idle) ? true : false;
	#endif
		connection->m_isDedicated = isDedicated;
		cry_strcpy(connection->name, name);
		memcpy(connection->userData, userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
		pSession->numFilledSlots += numUsers;
	}

	return id;
}

void CCryLANMatchMaking::FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserLeave, h, id);
			pSession->numFilledSlots -= pConnection->numUsers;
			CCryMatchMaking::FreeRemoteConnection(h, id);
		}
	}
}

uint64 CCryLANMatchMaking::GetSIDFromSessionHandle(CryLobbySessionHandle h)
{
	CRY_ASSERT_MESSAGE((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED), "CCryLANMatchMaking::GetSIDFromSessionHandle: invalid session handle");

	return m_sessions[h].sid;
}

CryLobbySessionHandle CCryLANMatchMaking::FindSessionFromServerID(CryLobbySessionHandle h)
{
	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		if (m_sessions[i].localFlags & CRYSESSION_LOCAL_FLAG_USED)
		{
			if (m_sessions[i].id.m_h == h)
			{
				return i;
			}
		}
	}

	return CryLobbyInvalidSessionHandle;
}

ECryLobbyError CCryLANMatchMaking::CreateSessionSearchHandle(CryLANSearchHandle* h)
{
	for (uint32 i = 0; i < MAX_LAN_SEARCHES; i++)
	{
		if (!m_search[i].used)
		{
			m_search[i].used = true;
			m_search[i].numServers = 0;

			*h = i;

			return eCLE_Success;
		}
	}

	return eCLE_TooManyTasks;
};

ECryLobbyError CCryLANMatchMaking::SetSessionUserData(CryLobbySessionHandle h, SCrySessionUserData* data, uint32 numData)
{
	if (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		for (uint32 i = 0; i < numData; i++)
		{
			uint32 j;

			for (j = 0; j < m_registeredUserData.num; j++)
			{
				if (data[i].m_id == m_registeredUserData.data[j].m_id)
				{
					if (data[i].m_type == m_registeredUserData.data[j].m_type)
					{
						m_sessions[h].data.m_data[j] = data[i];
						break;
					}
					else
					{
						return eCLE_UserDataTypeMissMatch;
					}
				}
			}

			if (j == m_registeredUserData.num)
			{
				return eCLE_UserDataNotRegistered;
			}
		}

		return eCLE_Success;
	}

	return eCLE_InvalidSession;
}

ECryLobbyError CCryLANMatchMaking::SetSessionData(CryLobbySessionHandle h, SCrySessionData* data)
{
	if (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		m_sessions[h].data.m_numPublicSlots = data->m_numPublicSlots;
		m_sessions[h].data.m_numPrivateSlots = data->m_numPrivateSlots;
		cry_strcpy(m_sessions[h].data.m_name, data->m_name);
		m_sessions[h].data.m_ranked = data->m_ranked;

		return SetSessionUserData(h, data->m_data, data->m_numData);
	}

	return eCLE_InvalidSession;
}

void CCryLANMatchMaking::SendSessionQuery(CTimeValue tv, uint32 index, bool broadcast)
{
	STask* pTask = &m_task[index];

	// Request session information from server
	if (pTask->GetTimer() > LAN_SEARCH_INTERVAL)
	{
		pTask->StartTimer();

		if ((pTask->ticks == MAX_SEARCH_RETRIES) || pTask->canceled)
		{
			NetLog("[Lobby] Stop LAN search ticks %u canceled %d", pTask->ticks, pTask->canceled);

			if (pTask->search != CryLANInvalidSearchHandle)
			{
				m_search[pTask->search].used = false;
			}

			StopTaskRunning(index);
		}
		else
		{
			const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size;
			CCrySharedLobbyPacket packet;

			if (packet.CreateWriteBuffer(MaxBufferSize))
			{
				const SCryLobbyParameters& lobbyParams = m_lobby->GetLobbyParameters();
				packet.StartWrite(eLANPT_MM_RequestServerData, false);
				packet.WriteUINT8(index);

				if (broadcast)
				{
					for (uint32 port = lobbyParams.m_connectPort; port < lobbyParams.m_connectPort + MAX_SOCKET_PORTS_TRY; ++port)
					{
						Send(CryMatchMakingInvalidTaskID, &packet, pTask->session, TNetAddress(SIPv4Addr(BROADCAST_ADDRESS, port)));
					}
				}
				else
				{
					if (pTask->session != CryLobbyInvalidSessionHandle)
					{
						if ((m_sessions[pTask->session].localFlags & CRYSESSION_LOCAL_FLAG_HOST))
						{
							Send(CryMatchMakingInvalidTaskID, &packet, pTask->session, TNetAddress(SIPv4Addr(LOOPBACK_ADDRESS, lobbyParams.m_listenPort)));
						}
						else
						{
							Send(CryMatchMakingInvalidTaskID, &packet, pTask->session, m_sessions[pTask->session].hostConnectionID);
						}
					}
				}
			}
		}

		pTask->ticks++;
	}
}

void CCryLANMatchMaking::Tick(CTimeValue tv)
{
	CCryMatchMaking::Tick(tv);

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
	{
		STask* task = &m_task[i];

	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		if (task->used)
		{
			if (m_lobby->DebugTickCallStartTaskRunning(task->lTaskID))
			{
				StartTaskRunning(i);
				continue;
			}
			if (!m_lobby->DebugOKToTickTask(task->lTaskID, task->running))
			{
				continue;
			}
		}
	#endif

		if (task->used && task->running)
		{
			switch (task->subTask)
			{
			case eT_SessionQuery:

				SendSessionQuery(tv, i, false);

				break;

			case eT_SessionSearch:

				SendSessionQuery(tv, i, true);

				break;

			case eT_SessionJoin:
				TickSessionJoin(i);
				break;

	#if NETWORK_HOST_MIGRATION
			case eT_SessionMigrateHostStart:
				TickHostMigrationStartNT(i);
				break;

			case eT_SessionMigrateHostClient:
				TickHostMigrationClientNT(i);
				break;

			case eT_SessionMigrateHostFinish:
				TickHostMigrationFinishNT(i);
				break;
	#endif
			default:
				TickBaseTask(i);
				break;
			}
		}
	}
}

ECryLobbyError CCryLANMatchMaking::Initialise()
{
	ECryLobbyError error = CCryMatchMaking::Initialise();

	if (error == eCLE_Success)
	{
		m_registeredUserData.num = 0;

		for (uint32 i = 0; i < MAX_LAN_SEARCHES; i++)
		{
			m_search[i].used = false;
		}
	}

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionRegisterUserData(SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	if (numData < MAX_MATCHMAKING_SESSION_USER_DATA)
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionRegisterUserData, &tid, taskID, 0, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			for (int i = 0; i < numData; i++)
			{
				m_registeredUserData.data[i] = data[i];
			}
			m_registeredUserData.num = numData;

			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_OutOfSessionUserData;
	}

	NetLog("[Lobby] Start SessionRegisterUserData error %d", error);

	return error;
}

const char* CCryLANMatchMaking::SSession::GetLocalUserName(uint32 localUserIndex) const
{
	if (localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		return localConnection.name;
	}

	return NULL;
}

void CCryLANMatchMaking::SSession::Reset()
{
	PARENT::Reset();

	sid = gEnv->pTimer->GetAsyncTime().GetValue();
	hostConnectionID = CryMatchMakingInvalidConnectionID;
	memset(localConnection.userData, 0, sizeof(localConnection.userData));
}

void CCryLANMatchMaking::SetLocalUserName(CryLobbySessionHandle h, uint32 localUserIndex)
{
	SSession* pSession = &m_sessions[h];
	SConfigurationParams neededInfo[1] = {
		{ CLCC_LAN_USER_NAME, { 0 }
		}
	};

	m_lobby->GetConfigurationInformation(neededInfo, 1);

	cry_strcpy(pSession->localConnection.name, (const char*)neededInfo[0].m_pData);
}

const char* CCryLANMatchMaking::GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const
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

ECryLobbyError CCryLANMatchMaking::SessionCreate(uint32* users, int numUsers, uint32 flags, SCrySessionData* data, CryLobbyTaskID* taskID, CryMatchmakingSessionCreateCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;

	if (gEnv->IsDedicated())
	{
		CRY_ASSERT_MESSAGE(numUsers == 1, "Dedicated Server, but number of users on create != 1 - Being forced to 0");
		numUsers = 0;
	}

	ECryLobbyError error = CreateSessionHandle(&h, true, flags, numUsers);

	if (error == eCLE_Success)
	{
		SSession* session = &m_sessions[h];

		error = SetSessionData(h, data);

		if (error == eCLE_Success)
		{
			CryMatchMakingTaskID tid;

			error = StartTask(eT_SessionCreate, &tid, taskID, h, (void*)cb, cbArg);

			if (error == eCLE_Success)
			{
				SetLocalUserName(h, 0);

				session->localConnection.uid = CreateConnectionUID(h);
				session->localConnection.pingToServer = CRYLOBBY_INVALID_PING;
				session->localConnection.used = true;
	#if NETWORK_HOST_MIGRATION
				session->newHostPriorityCount = 0;
				session->newHostUID = CryMatchMakingInvalidConnectionUID;
	#endif
				NetLog("[Lobby] Created local connection " PRFORMAT_SH " " PRFORMAT_UID, PRARG_SH(h), PRARG_UID(session->localConnection.uid));
				FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
			}
			else
			{
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

ECryLobbyError CCryLANMatchMaking::SessionMigrate(CrySessionHandle gh, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* session = &m_sessions[h];

		error = SetSessionData(h, pData);

		if (error == eCLE_Success)
		{
			CryMatchMakingTaskID tid;

			error = StartTask(eT_SessionMigrate, &tid, pTaskID, h, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				session->localFlags |= CRYSESSION_LOCAL_FLAG_HOST;
				session->createFlags = flags;
				FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
			}
			else
			{
				FreeSessionHandle(h);
			}
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionMigrate error %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionQuery(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionQuery, &tid, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionQuery error %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionGetUsers(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionGetUsers, &tid, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Lobby] Start SessionQuery error %d", error);
	}

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionSetLocalUserData(CrySessionHandle gh, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		if (dataSize <= CRYLOBBY_USER_DATA_SIZE_IN_BYTES)
		{
			SSession* pSession = &m_sessions[h];
			SSession::SLConnection* pLConnection = &pSession->localConnection;
			CryMatchMakingTaskID mmTaskID;

			memcpy(pLConnection->userData, pData, dataSize);

			error = StartTask(eT_SessionSetLocalUserData, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, mmTaskID);
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

void CCryLANMatchMaking::StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT16Size + CRYLOBBY_USER_DATA_SIZE_IN_BYTES;
	CCrySharedLobbyPacket packet;

	SessionUserDataEvent(eCLSE_SessionUserUpdate, pTask->session, CryMatchMakingInvalidConnectionID);

	if (packet.CreateWriteBuffer(MaxBufferSize))
	{
		SSession* pSession = &m_sessions[pTask->session];
		SSession::SLConnection* pLConnection = &pSession->localConnection;

		packet.StartWrite(eLANPT_UserData, true);
		packet.WriteUINT16(pLConnection->uid.m_uid);
		packet.WriteData(pLConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			SendToAll(CryMatchMakingInvalidTaskID, &packet, pTask->session, CryMatchMakingInvalidConnectionID);
		}
		else
		{
			Send(CryMatchMakingInvalidTaskID, &packet, pTask->session, pSession->hostConnectionID);
		}

		packet.FreeWriteBuffer();
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
	}

	StopTaskRunning(mmTaskID);
}

void CCryLANMatchMaking::ProcessLocalUserData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h;
	SCryMatchMakingConnectionUID uid;
	CryMatchMakingConnectionID id;

	pPacket->StartRead();
	uid = pPacket->GetFromConnectionUID();
	uid.m_uid = pPacket->ReadUINT16();

	if (FindConnectionFromUID(uid, &h, &id))
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pRConnection = &pSession->remoteConnection[id];

		pPacket->ReadData(pRConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
		SessionUserDataEvent(eCLSE_SessionUserUpdate, h, id);

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			SendToAll(CryMatchMakingInvalidTaskID, pPacket, h, id);
		}
	}
}

ECryLobbyError CCryLANMatchMaking::SessionUpdate(CrySessionHandle gh, SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		if (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			error = SetSessionUserData(h, data, numData);

			if (error == eCLE_Success)
			{
				CryMatchMakingTaskID tid;

				error = StartTask(eT_SessionUpdate, &tid, taskID, h, (void*)cb, cbArg);

				if (error == eCLE_Success)
				{
					FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
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

ECryLobbyError CCryLANMatchMaking::SessionUpdateSlots(CrySessionHandle gh, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionUpdateSlots, &tid, pTaskID, h, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionUpdateSlots return %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionStart(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionStart, &tid, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	NetLog("[Lobby] Start SessionStart error %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionEnd(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionEnd, &tid, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionEnd error %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionDelete(CrySessionHandle gh, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionDelete, &tid, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);

	#if NETWORK_HOST_MIGRATION
			// Since we're deleting this session, terminate any host migration
			if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
			{
				m_hostMigration.Terminate(&pSession->hostMigrationInfo);
			}
	#endif
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	NetLog("[Lobby] Start SessionDelete error %d", error);

	return error;
}

void CCryLANMatchMaking::StartSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	STask* task = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[task->session];

	#if NETWORK_HOST_MIGRATION
	// Since we're deleting this session, terminate any host migration
	if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
	{
		m_hostMigration.Terminate(&pSession->hostMigrationInfo);
	}
	#endif
	pSession->createFlags &= ~CRYSESSION_CREATE_FLAG_MIGRATABLE;

	task->subTask = eT_SessionDelete;

	// Disconnect our local connection
	SessionDisconnectRemoteConnectionViaNub(task->session, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_UserRequested, "Session deleted");
	SessionUserDataEvent(eCLSE_SessionUserLeave, task->session, CryMatchMakingInvalidConnectionID);

	// Free any remaining remote connections
	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* connection = &pSession->remoteConnection[i];

		if (connection->used)
		{
			FreeRemoteConnection(task->session, i);
		}
	}

	FreeSessionHandle(task->session);
	StopTaskRunning(mmTaskID);
}

ECryLobbyError CCryLANMatchMaking::SessionSearch(uint32 user, SCrySessionSearchParam* param, CryLobbyTaskID* taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLANSearchHandle h;
	ECryLobbyError error = CreateSessionSearchHandle(&h);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionSearch, &tid, taskID, CryLobbyInvalidSessionHandle, (void*)cb, cbArg);

		if ((error == eCLE_Success) && param && param->m_data)
		{
			error = CreateTaskParam(tid, SEARCH_PARAM_GAME_PARAM, param, 1, sizeof(SCrySessionSearchParam));

			if (error == eCLE_Success)
			{
				error = CreateTaskParam(tid, SEARCH_PARAM_GAME_PARAM_DATA, param->m_data, param->m_numData, param->m_numData * sizeof(param->m_data[0]));
			}
		}

		if (error == eCLE_Success)
		{
			m_task[tid].search = h;
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
		else
		{
			m_search[h].used = false;
		}
	}

	NetLog("[Lobby] Start SessionSearch error %d", error);

	return error;
}

ECryLobbyError CCryLANMatchMaking::SessionJoin(uint32* users, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* taskID, CryMatchmakingSessionJoinCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, false, flags, numUsers);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionJoin, &tid, taskID, h, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			m_sessions[h].id = *((SCryLANSessionID*)id.get());
			SetLocalUserName(h, 0);
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, tid);
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	NetLog("[Lobby] Start SessionJoin error %d", error);

	return error;
}

uint32 CCryLANMatchMaking::GetSessionIDSizeInPacket() const
{
	return SCryLANSessionID::GetSizeInPacket();
}

ECryLobbyError CCryLANMatchMaking::WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const
{
	if (pPacket)
	{
		SCryLANSessionID blank;

		if (sessionId != CrySessionInvalidID)
		{
			SCryLANSessionID* pLanID = (SCryLANSessionID*)sessionId.get();
			if (pLanID)
			{
				pLanID->WriteToPacket(pPacket);
			}
			else
			{
				blank.WriteToPacket(pPacket);
			}
		}
		else
		{
			blank.WriteToPacket(pPacket);
		}

		return eCLE_Success;
	}

	return eCLE_InternalError;
}

CrySessionID CCryLANMatchMaking::ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const
{
	if (pPacket)
	{
		SCryLANSessionID* pLanID = new SCryLANSessionID();
		CrySessionID returnId = pLanID;

		if (pLanID)
		{
			pLanID->ReadFromPacket(pPacket);

			SCryLANSessionID blank;

			//-- Safe to use the equals operator here, since the fields tested in the operator are the same ones
			//-- written to the packet.
			if (!(*pLanID == blank))
			{
				//-- assume it's a valid session id
				return returnId;
			}
		}
	}

	return CrySessionInvalidID;
}

CrySessionID CCryLANMatchMaking::GetSessionIDFromConsole(void)
{
	if (gEnv->pNetwork)
	{
		IConsole* pConsole = gEnv->pConsole;
		CryFixedStringT<32> host;
		uint16 port = CLobbyCVars::Get().lobbyDefaultPort;

		if (pConsole)
		{
			ICVar* pCVar = pConsole->GetCVar("cl_serveraddr");
			if (pCVar)
			{
				host = pCVar->GetString();
			}

			pCVar = pConsole->GetCVar("cl_serverport");
			if (pCVar)
			{
				port = pCVar->GetIVal();
			}
			else
			{
				uint16 conPort, lisPort;
				m_pService->GetSocketPorts(conPort, lisPort);
				port = conPort;
			}
		}

		if (host.find("<session>") == CryStringT<char>::npos)
		{
			CNetAddressResolver* pResolver = m_lobby->GetResolver();

			if (pResolver)
			{
				CNameRequestPtr pReq = pResolver->RequestNameLookup(host.c_str());
				pReq->Wait();
				if (pReq->GetResult() != eNRR_Succeeded)
				{
					CryLogAlways("Name resolution for '%s' failed", host.c_str());
					return CrySessionInvalidID;
				}

				SIPv4Addr* pAddr = stl::get_if<SIPv4Addr>(&pReq->GetAddrs()[0]);
				if (pAddr)
				{
					SCryLANSessionID* pId = new SCryLANSessionID;
					PREFAST_ASSUME(pId);
					pId->m_ip = htonl(pAddr->addr);
					pId->m_port = port;
					pId->m_h = CryLobbyInvalidSessionHandle;

					return pId;
				}
			}
		}
	}

	return CrySessionInvalidID;
}

ECryLobbyError CCryLANMatchMaking::GetSessionIDFromIP(const char* const pAddress, CrySessionID* const pSessionID)
{
	ECryLobbyError error = eCLE_InvalidParam;

	if (pSessionID)
	{
		CrySessionID sessionID = CrySessionInvalidID;

		if (pAddress)
		{
			CryFixedStringT<32> host = pAddress;
			uint16 port = CLobbyCVars::Get().lobbyDefaultPort;

			CNetAddressResolver* pResolver = m_lobby->GetResolver();
			if (pResolver)
			{
				CNameRequestPtr pReq = pResolver->RequestNameLookup(host.c_str());
				pReq->Wait();
				if (pReq->GetResult() == eNRR_Succeeded)
				{
					SIPv4Addr* pAddr = stl::get_if<SIPv4Addr>(&pReq->GetAddrs()[0]);
					if (pAddr)
					{
						SCryLANSessionID* pId = new SCryLANSessionID;
						if (pId)
						{
							pId->m_ip = htonl(pAddr->addr);
							pId->m_port = port;
							pId->m_h = CryLobbyInvalidSessionHandle;

							sessionID = pId;

							error = eCLE_Success;
						}
						else
						{
							error = eCLE_OutOfMemory;
						}
					}
				}
				else
				{
					CryLogAlways("Name resolution for '%s' failed", host.c_str());
					error = eCLE_InternalError;
				}
			}
		}

		*pSessionID = sessionID;
	}

	return error;
}

void CCryLANMatchMaking::StartSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketLobbySessionHandleSize + CryLobbyPacketUINT8Size + CryLobbyPacketUINT8Size + CryLobbyPacketBoolSize + CRYLOBBY_USER_NAME_LENGTH + CRYLOBBY_USER_DATA_SIZE_IN_BYTES;
	CCrySharedLobbyPacket packet;
	if (packet.CreateWriteBuffer(MaxBufferSize))
	{
		packet.StartWrite(eLANPT_SessionRequestJoin, true);
		packet.WriteLobbySessionHandle(pSession->id.m_h);
		packet.WriteUINT8(mmTaskID);
		packet.WriteUINT8(pSession->localConnection.numUsers);
		packet.WriteString(pSession->localConnection.name, CRYLOBBY_USER_NAME_LENGTH);
		packet.WriteData(pSession->localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

		if (Send(mmTaskID, &packet, pTask->session, TNetAddress(SIPv4Addr(ntohl(pSession->id.m_ip), pSession->id.m_port))) != eSE_Ok)
		{
			UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(mmTaskID);
	}
}

void CCryLANMatchMaking::TickSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* task = &m_task[mmTaskID];

	if (task->sendStatus != eCLE_Pending)
	{
		if (task->sendStatus == eCLE_Success)
		{
			// The request to join has been sent so wait for result
			if (task->TimerStarted())
			{
				if (task->GetTimer() > CryMatchMakingConnectionTimeOut)
				{
					// No response so fail connection attempt
					NetLog("[Lobby] SessionJoin request session join packet sent no response received");
					UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
					StartSessionDelete(mmTaskID);
				}
			}
			else
			{
				task->StartTimer();
				NetLog("[Lobby] SessionJoin request session join packet sent waiting for response");
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			NetLog("[Lobby] SessionJoin error sending request session join packet error %d", task->error);
			StartSessionDelete(mmTaskID);
		}
	}
}

void CCryLANMatchMaking::ProcessSessionRequestJoin(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	ECryLobbyError error = eCLE_Success;
	uint32 bufferPos = 0;
	CryLobbySessionHandle h;
	CryLobbyConnectionID c;
	CryMatchMakingTaskID returnTaskID;

	pPacket->StartRead();
	h = pPacket->ReadLobbySessionHandle();
	returnTaskID = pPacket->ReadUINT8();

	if (m_lobby->ConnectionFromAddress(&c, addr))
	{
		CRYSOCKADDR_IN saddr;

		if (m_lobby->ConvertAddr(addr, &saddr))
		{
			// If the 'connect' command is used from the console with no session then we'll have an invalid
			// session handle here.  In this case, search for the first searchable host session and use that.
			if (h == CryLobbyInvalidSessionHandle)
			{
				for (int32 index = 0; index < MAX_MATCHMAKING_SESSIONS; ++index)
				{
					SSession* pSession = &m_sessions[index];
					if (((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST)) == (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST)) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_SEARCHABLE))
					{
						h = index;
						break;
					}
				}
			}

			if (h != CryLobbyInvalidSessionHandle && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
			{
				SSession* session = &m_sessions[h];
				uint8 numUsers = pPacket->ReadUINT8();

				if (session->numFilledSlots + numUsers <= session->data.m_numPublicSlots + session->data.m_numPrivateSlots)
				{
					char name[CRYLOBBY_USER_NAME_LENGTH];
					uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];

					pPacket->ReadString(name, CRYLOBBY_USER_NAME_LENGTH);
					pPacket->ReadData(userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

					CryMatchMakingConnectionID id = AddRemoteConnection(h, c, CreateConnectionUID(h), saddr.sin_addr.s_addr, saddr.sin_port, numUsers, name, userData, false);

					if (id != CryMatchMakingInvalidConnectionID)
					{
						// Added to session
						CCrySharedLobbyPacket packet;

						if (packet.CreateWriteBuffer(MAX_LOBBY_PACKET_SIZE))
						{
							SSession::SRConnection* connection = &session->remoteConnection[id];

							packet.StartWrite(eLANPT_SessionRequestJoinResult, true);
							packet.WriteUINT8(returnTaskID);
							packet.WriteError(error);
							packet.WriteLobbySessionHandle(h);
							packet.WriteConnectionUID(connection->uid);
							packet.WriteUINT16((session->createFlags & CRYSESSION_CREATE_FLAG_GAME_MASK) >> CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT);
							packet.WriteUINT8(session->localConnection.numUsers);
							if (gEnv->IsDedicated())                          // Server should flag if its dedicated or not
							{
								packet.WriteBool(true);
							}
							else
							{
								packet.WriteBool(false);
							}
							packet.WriteConnectionUID(session->localConnection.uid);
							packet.WriteString(session->localConnection.name, CRYLOBBY_USER_NAME_LENGTH);
							packet.WriteData(session->localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

							Send(CryMatchMakingInvalidTaskID, &packet, h, id);

							// Now connect success has been sent to new client it is safe to inform game of the connection
							SessionUserDataEvent(eCLSE_SessionUserJoin, h, id);

							// Send the new clients connection to the old clients
							packet.StartWrite(eLANPT_SessionAddRemoteConnections, true);
							packet.WriteLobbySessionHandle(h);
							packet.WriteUINT8(1);
							packet.WriteUINT32(htonl(connection->ip));
							packet.WriteUINT16(htons(connection->port));
							packet.WriteConnectionUID(connection->uid);
							packet.WriteUINT8(connection->numUsers);
							packet.WriteBool(connection->m_isDedicated);
							packet.WriteString(connection->name, CRYLOBBY_USER_NAME_LENGTH);
							packet.WriteData(connection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

							NetLog("[Lobby] Send new users to peers");
							SendToAll(CryMatchMakingInvalidTaskID, &packet, h, id);

							// Send the remote connections to the new client
							uint32 connectionToAdd = 0;

							while (true)
							{
								uint8 numConnections = 0;
								uint8 numAdded = 0;
								uint32 testSize;
								uint32 i;

								packet.StartWrite(eLANPT_SessionAddRemoteConnections, true);
								packet.WriteLobbySessionHandle(h);

								// Find out how many of the remote connections can fit in the packet

								testSize = packet.GetWriteBufferPos() + CryLobbyPacketUINT8Size;

								for (i = connectionToAdd; i < MAX_LOBBY_CONNECTIONS; i++)
								{
									SSession::SRConnection* connectionAdd = &session->remoteConnection[i];

									if (connectionAdd->used && (connectionAdd != connection) && (connectionAdd->flags & CMMRC_FLAG_PLATFORM_DATA_VALID))
									{
										uint32 add = CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size + CryLobbyPacketConnectionUIDSize +
										             CryLobbyPacketUINT8Size + CryLobbyPacketBoolSize + CRYLOBBY_USER_NAME_LENGTH + CRYLOBBY_USER_DATA_SIZE_IN_BYTES;

										if (testSize + add < MAX_LOBBY_PACKET_SIZE)
										{
											testSize += add;
											numConnections++;
										}
										else
										{
											break;
										}
									}
								}

								if (numConnections > 0)
								{
									// Add and send the connections
									packet.WriteUINT8(numConnections);

									for (i = connectionToAdd, numAdded = 0; (i < MAX_LOBBY_CONNECTIONS) && (numAdded < numConnections); i++, connectionToAdd++)
									{
										SSession::SRConnection* connectionAdd = &session->remoteConnection[i];

										if (connectionAdd->used && (connectionAdd != connection) && (connectionAdd->flags & CMMRC_FLAG_PLATFORM_DATA_VALID))
										{
											packet.WriteUINT32(htonl(connectionAdd->ip));
											packet.WriteUINT16(htons(connectionAdd->port));
											packet.WriteConnectionUID(connectionAdd->uid);
											packet.WriteUINT8(connectionAdd->numUsers);
											packet.WriteBool(connectionAdd->m_isDedicated);
											packet.WriteString(connectionAdd->name, CRYLOBBY_USER_NAME_LENGTH);
											packet.WriteData(connectionAdd->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
											numAdded++;

											NetLog("[Lobby] Send connection " PRFORMAT_MMCINFO " users to new connection " PRFORMAT_MMCINFO,
											       PRARG_MMCINFO(CryMatchMakingConnectionID(i), connectionAdd->connectionID, connectionAdd->uid), PRARG_MMCINFO(id, connection->connectionID, connection->uid));
										}
									}

									Send(CryMatchMakingInvalidTaskID, &packet, h, id);
								}
								else
								{
									// No more connections to send
									break;
								}
							}

							packet.FreeWriteBuffer();
						}
						else
						{
							FreeRemoteConnection(h, id);
							error = eCLE_ConnectionFailed;
						}
					}
					else
					{
						error = eCLE_SessionFull;
					}
				}
				else
				{
					error = eCLE_SessionFull;
				}
			}
			else
			{
				error = eCLE_ConnectionFailed;
			}
		}
		else
		{
			error = eCLE_ConnectionFailed;
		}
	}
	else
	{
		error = eCLE_ConnectionFailed;
	}

	if (error != eCLE_Success)
	{
		// Can't add to session so send back error
		const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT8Size + CryLobbyPacketErrorSize;
		CCrySharedLobbyPacket packet;

		if (packet.CreateWriteBuffer(MaxBufferSize))
		{
			packet.StartWrite(eLANPT_SessionRequestJoinResult, true);
			packet.WriteUINT8(returnTaskID);
			packet.WriteError(error);

			Send(CryMatchMakingInvalidTaskID, &packet, h, addr);

			packet.FreeWriteBuffer();
		}
	}

	NetLog("[Lobby] Processed Session request join packet error %d", error);
}

void CCryLANMatchMaking::ProcessSessionRequestJoinResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	ECryLobbyError error = eCLE_Success;
	CryLobbyConnectionID c;

	if (m_lobby->ConnectionFromAddress(&c, addr))
	{
		CryMatchMakingTaskID mmTaskID;

		pPacket->StartRead();
		mmTaskID = pPacket->ReadUINT8();
		mmTaskID = FindTaskFromTaskTaskID(eT_SessionJoin, mmTaskID);

		if (mmTaskID != CryMatchMakingInvalidTaskID)
		{
			STask* task = &m_task[mmTaskID];
			CRYSOCKADDR_IN saddr;

			if (m_lobby->ConvertAddr(addr, &saddr))
			{
				error = pPacket->ReadError();

				NetLog("[Lobby] Received SessionRequestJoinResult error %d", error);

				UpdateTaskError(mmTaskID, error);

				if (task->error == eCLE_Success)
				{
					SSession* pSession = &m_sessions[task->session];
					uint8 numUsers;
					bool isDedicated;
					SCryMatchMakingConnectionUID hostConnectionUID;
					char name[CRYLOBBY_USER_NAME_LENGTH];
					uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];

					CryLobbySessionHandle h = pPacket->ReadLobbySessionHandle();
					CRY_ASSERT((pSession->id.m_h == CryLobbyInvalidSessionHandle) || (pSession->id.m_h == h));
					pSession->id.m_h = h;
					pSession->localConnection.uid = pPacket->ReadConnectionUID();
					uint32 sessionCreateGameFlags = pPacket->ReadUINT16();
					pSession->createFlags = (pSession->createFlags & CRYSESSION_CREATE_FLAG_SYSTEM_MASK) | (sessionCreateGameFlags << CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT);
					pSession->sid = pSession->localConnection.uid.m_sid;

					bool hostMigrationSupported = m_lobby->GetLobbyServiceFlag(m_serviceType, eCLSF_SupportHostMigration);
					if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) && hostMigrationSupported)
					{
						pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
					}

					NetLog("[Lobby] Created local connection " PRFORMAT_SH " " PRFORMAT_UID, PRARG_SH(task->session), PRARG_UID(pSession->localConnection.uid));
					if ((!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)) && (pSession->localConnection.uid.m_uid > m_connectionUIDCounter))
					{
						// Keep all remote clients in sync with the host (in case they're chosen to be host during host migration)
						m_connectionUIDCounter = pSession->localConnection.uid.m_uid;
					}

					numUsers = pPacket->ReadUINT8();
					isDedicated = pPacket->ReadBool();
					hostConnectionUID = pPacket->ReadConnectionUID();
					pPacket->ReadString(name, CRYLOBBY_USER_NAME_LENGTH);
					pPacket->ReadData(userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

					CryMatchMakingConnectionID id = AddRemoteConnection(task->session, c, hostConnectionUID, saddr.sin_addr.s_addr, saddr.sin_port, numUsers, name, userData, isDedicated);

					if (id != CryMatchMakingInvalidConnectionID)
					{
						NetLog("[Lobby] Created server connection " PRFORMAT_MMCINFO, PRARG_MMCINFO(id, c, hostConnectionUID));
						StopTaskRunning(mmTaskID);
						pSession->hostConnectionID = id;
						SessionUserDataEvent(eCLSE_SessionUserJoin, h, id);
					}
					else
					{
						UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
					}
				}
			}
			else
			{
				UpdateTaskError(mmTaskID, eCLE_ConnectionFailed);
			}

			if (task->error != eCLE_Success)
			{
				StartSessionDelete(mmTaskID);
			}

			NetLog("[Lobby] Processed session request join result error %d", task->error);
		}
	}
}

void CCryLANMatchMaking::ProcessSessionAddRemoteConnections(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	uint32 bufferPos = 0;
	CryLobbySessionHandle h;

	pPacket->StartRead();
	h = pPacket->ReadLobbySessionHandle();

	h = FindSessionFromServerID(h);

	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* session = &m_sessions[h];
		if ((session->localFlags & CRYSESSION_LOCAL_FLAG_HOST) == 0)
		{
			uint8 numConnections = pPacket->ReadUINT8();

			for (uint32 i = 0; i < numConnections; i++)
			{
				char name[CRYLOBBY_USER_NAME_LENGTH];
				uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
				uint32 ip;
				uint16 port;
				uint8 numUsers;
				bool isDedicated;
				SCryMatchMakingConnectionUID connectionUID;

				ip = pPacket->ReadUINT32();
				port = pPacket->ReadUINT16();
				connectionUID = pPacket->ReadConnectionUID();
				numUsers = pPacket->ReadUINT8();
				isDedicated = pPacket->ReadBool();
				pPacket->ReadString(name, CRYLOBBY_USER_NAME_LENGTH);
				pPacket->ReadData(userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

				CryMatchMakingConnectionID id = AddRemoteConnection(h, CryLobbyInvalidConnectionID, connectionUID, ntohl(ip), ntohs(port), numUsers, name, userData, isDedicated);

				if (id != CryMatchMakingInvalidConnectionID)
				{
					NetLog("[Lobby] Add new connection " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, id, session->remoteConnection[id].connectionID, connectionUID));
					SessionUserDataEvent(eCLSE_SessionUserJoin, h, id);
				}
			}
		}
		else
		{
			NetLog("[Lobby] CCryLANMatchMaking::ProcessSessionAddRemoteConnections() packet received on " PRFORMAT_SH " but we're the host!  Ignoring", PRARG_SH(h));
		}
	}
}

size_t CCryLANMatchMaking::CalculateServerDataSize(CryLobbySessionHandle h)
{
	size_t size =
	  CryLobbyPacketUnReliableHeaderSize +
	  CryLobbyPacketUINT8Size +              // Requesters task id
	  CryLobbyPacketLobbySessionHandleSize + // Session handle
	  CryLobbyPacketUINT32Size +             // Num filled slots
	  CryLobbyPacketUINT32Size +             // Num public slots
	  CryLobbyPacketUINT32Size +             // Num private slots
	  MAX_SESSION_NAME_LENGTH +              // Session name
	  CryLobbyPacketBoolSize;                // Ranked

	for (uint32 i = 0; i < m_sessions[h].data.m_numData; i++)
	{
		switch (m_sessions[h].data.m_data[i].m_type)
		{
		case eCLUDT_Int64:
		case eCLUDT_Float64:
		case eCLUDT_Int64NoEndianSwap:
			size += CryLobbyPacketUINT64Size;
			break;

		case eCLUDT_Int32:
		case eCLUDT_Float32:
			size += CryLobbyPacketUINT32Size;
			break;

		case eCLUDT_Int16:
			size += CryLobbyPacketUINT16Size;
			break;

		case eCLUDT_Int8:
			size += CryLobbyPacketUINT8Size;
			break;
		}
	}

	return size;
}

size_t CCryLANMatchMaking::CalculateServerDataSize() const
{
	size_t size =
	  CryLobbyPacketUnReliableHeaderSize +
	  CryLobbyPacketUINT8Size +              // Requesters task id
	  CryLobbyPacketLobbySessionHandleSize + // Session handle
	  CryLobbyPacketUINT32Size +             // Num filled slots
	  CryLobbyPacketUINT32Size +             // Num public slots
	  CryLobbyPacketUINT32Size +             // Num private slots
	  MAX_SESSION_NAME_LENGTH +              // Session name
	  CryLobbyPacketBoolSize;                // Ranked

	for (uint32 i = 0; i < m_registeredUserData.num; i++)
	{
		switch (m_registeredUserData.data[i].m_type)
		{
		case eCLUDT_Int64:
		case eCLUDT_Float64:
		case eCLUDT_Int64NoEndianSwap:
			size += CryLobbyPacketUINT64Size;
			break;

		case eCLUDT_Int32:
		case eCLUDT_Float32:
			size += CryLobbyPacketUINT32Size;
			break;

		case eCLUDT_Int16:
			size += CryLobbyPacketUINT16Size;
			break;

		case eCLUDT_Int8:
			size += CryLobbyPacketUINT8Size;
			break;
		}
	}

	return size;
}

void CCryLANMatchMaking::SendServerData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	LOBBY_AUTO_LOCK;
	uint8 requestersTaskID;

	pPacket->StartRead();
	requestersTaskID = pPacket->ReadUINT8();

	for (uint32 i = 0; i < MAX_MATCHMAKING_SESSIONS; i++)
	{
		SSession* pSession = &m_sessions[i];
		if (((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST)) == (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST)) && (pSession->createFlags & CRYSESSION_CREATE_FLAG_SEARCHABLE))
		{
			uint32 bufferSize = CalculateServerDataSize(i);
			assert(bufferSize == CalculateServerDataSize() && "Expected data size not identical between sender and receiver, server will not be discoverable");
			CCrySharedLobbyPacket packet;

			if (packet.CreateWriteBuffer(bufferSize))
			{
				packet.StartWrite(eLANPT_MM_ServerData, false);
				packet.WriteUINT8(requestersTaskID);
				packet.WriteLobbySessionHandle(i);
				packet.WriteUINT32(pSession->numFilledSlots);
				packet.WriteUINT32(pSession->data.m_numPublicSlots);
				packet.WriteUINT32(pSession->data.m_numPrivateSlots);
				packet.WriteData(pSession->data.m_name, MAX_SESSION_NAME_LENGTH);
				packet.WriteBool(pSession->data.m_ranked);

				for (uint32 j = 0; j < pSession->data.m_numData; j++)
				{
					packet.WriteCryLobbyUserData(&pSession->data.m_data[j]);
				}

				assert(packet.GetWriteBufferPos() == bufferSize && "Written data size doesn't match expected data size");

				Send(CryMatchMakingInvalidTaskID, &packet, i, addr);
				packet.FreeWriteBuffer();
			}
		}
	}
}

void CCryLANMatchMaking::ServerDataToGame(CryMatchMakingTaskID mmTaskID, uint32 ip, uint16 port, TMemHdl params, uint32 length)
{
	LOBBY_AUTO_LOCK;

	SCryLANSessionID* id = NULL;
	const uint32 bufferSize = CalculateServerDataSize();
	if (length == bufferSize) // Guard against incompatible discovery response (likely different engine version)
	{
		id = new SCryLANSessionID;
	}
	else
	{
		CNetAddressResolver* pResolver = m_lobby->GetResolver();
		if (pResolver)
		{
			SIPv4Addr addr(uint32(htonl(ip)), port);
			TAddressString ipStr = pResolver->ToNumericString(addr);
			CryLogAlways("[Lobby] Session at %s is incompatible, not returned", ipStr.c_str());
		}
	}
	if (id)
	{
		STask* pTask = &m_task[mmTaskID];
		SCrySessionSearchResult result;
		SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];
		CCrySharedLobbyPacket packet;
		uint8 taskID8;

		packet.SetReadBuffer((uint8*)m_lobby->MemGetPtr(params), length);
		packet.ReadPacketHeader();
		packet.StartRead();
		taskID8 = packet.ReadUINT8();

		id->m_ip = ip;
		id->m_port = port;
		id->m_h = packet.ReadLobbySessionHandle();
		result.m_id = id;

		result.m_numFilledSlots = packet.ReadUINT32();
		result.m_data.m_numPublicSlots = packet.ReadUINT32();
		result.m_data.m_numPrivateSlots = packet.ReadUINT32();
		packet.ReadData(result.m_data.m_name, MAX_SESSION_NAME_LENGTH);
		result.m_data.m_ranked = packet.ReadBool();

		result.m_data.m_data = userData;
		result.m_data.m_numData = m_registeredUserData.num;

		result.m_ping = 0;
		result.m_numFriends = 0;
		result.m_flags = 0;

		for (uint32 i = 0; i < m_registeredUserData.num; i++)
		{
			result.m_data.m_data[i].m_id = m_registeredUserData.data[i].m_id;
			result.m_data.m_data[i].m_type = m_registeredUserData.data[i].m_type;
			packet.ReadCryLobbyUserData(&result.m_data.m_data[i]);
		}

		assert(packet.GetReadBufferPos() == bufferSize && "Read data size doesn't match expected data size");

		bool filterOk = true;
		SCrySessionSearchParam* pParam = static_cast<SCrySessionSearchParam*>(m_lobby->MemGetPtr(pTask->params[SEARCH_PARAM_GAME_PARAM]));
		SCrySessionSearchData* pData = static_cast<SCrySessionSearchData*>(m_lobby->MemGetPtr(pTask->params[SEARCH_PARAM_GAME_PARAM_DATA]));
		if (pParam && pData)
		{
			filterOk = ParamFilter(pParam->m_numData, pData, &result);
		}

		if (pTask->cb && filterOk)
		{
			((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &result, pTask->cbArg);
		}
	}

	m_lobby->MemFree(params);
}

bool CCryLANMatchMaking::ParamFilter(uint32 nParams, const SCrySessionSearchData* pParams, const SCrySessionSearchResult* pResult)
{
	for (uint32 i = 0; i < nParams; i++)
	{
		const SCrySessionSearchData* pParam = &pParams[i];

		for (uint32 j = 0; j < pResult->m_data.m_numData; j++)
		{
			const SCryLobbyUserData* pData = &pResult->m_data.m_data[j];

			if (pData->m_id == pParam->m_data.m_id)
			{
				bool bOk = true;

				if (pData->m_type != pParam->m_data.m_type)
				{
					return false;
				}

				switch (pParam->m_operator)
				{
				case eCSSO_Equal:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 == pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 == pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 == pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 == pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 == pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 == pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_NotEqual:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 != pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 != pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 != pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 != pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 != pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 != pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_LessThan:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 < pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 < pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 < pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 < pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 < pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 < pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_LessThanEqual:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 <= pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 <= pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 <= pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 <= pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 <= pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 <= pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_GreaterThan:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 > pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 > pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 > pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 > pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 > pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 > pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_GreaterThanEqual:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = (pData->m_int64 >= pParam->m_data.m_int64);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = (pData->m_int32 >= pParam->m_data.m_int32);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = (pData->m_int16 >= pParam->m_data.m_int16);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = (pData->m_int8 >= pParam->m_data.m_int8);
							}
							break;
						case eCLUDT_Float64:
							{
								bOk = (pData->m_f64 >= pParam->m_data.m_f64);
							}
							break;
						case eCLUDT_Float32:
							{
								bOk = (pData->m_f32 >= pParam->m_data.m_f32);
							}
							break;
						default:
							break;
						}
					}
					break;
				case eCSSO_BitwiseAndNotEqualZero:
					{
						switch (pParam->m_data.m_type)
						{
						case eCLUDT_Int64:
						case eCLUDT_Int64NoEndianSwap:
							{
								bOk = ((pData->m_int64 & pParam->m_data.m_int64) != 0);
							}
							break;
						case eCLUDT_Int32:
							{
								bOk = ((pData->m_int32 & pParam->m_data.m_int32) != 0);
							}
							break;
						case eCLUDT_Int16:
							{
								bOk = ((pData->m_int16 & pParam->m_data.m_int16) != 0);
							}
							break;
						case eCLUDT_Int8:
							{
								bOk = ((pData->m_int8 & pParam->m_data.m_int8) != 0);
							}
							break;
						case eCLUDT_Float64:
						case eCLUDT_Float32:
							{
								CRY_ASSERT_MESSAGE(0, "eCSSO_BitwiseAndNotEqualZero not supported on floating point numbers.");
								bOk = false;
							}
							break;
						default:
							break;
						}
					}
					break;
				default:
					break;
				}

				if (bOk == false)
				{
					return false;
				}
			}
		}
	}

	return true;
}

void CCryLANMatchMaking::ProcessServerData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CRYSOCKADDR_IN sockAddr;

	LOBBY_AUTO_LOCK;

	if (m_lobby->ConvertAddr(addr, &sockAddr))
	{
		pPacket->StartRead();
		CryMatchMakingTaskID taskID = pPacket->ReadUINT8();

		if ((taskID < MAX_MATCHMAKING_TASKS) && m_task[taskID].used && m_task[taskID].running && ((m_task[taskID].startedTask == eT_SessionQuery) || (m_task[taskID].startedTask == eT_SessionSearch)))
		{
			STask* pTask = &m_task[taskID];
			uint32 ip = sockAddr.sin_addr.s_addr;
			uint16 port = ntohs(sockAddr.sin_port);

			if (m_task[taskID].startedTask == eT_SessionSearch)
			{
				SSearch* pSearch = &m_search[pTask->search];

				if (pSearch->numServers < MAX_LAN_SEARCH_SERVERS)
				{
					for (uint32 i = 0; i < pSearch->numServers; i++)
					{
						if ((pSearch->servers[i].m_ip == ip) && (pSearch->servers[i].m_port == port))
						{
							return;
						}
					}

					pSearch->servers[pSearch->numServers].m_ip = ip;
					pSearch->servers[pSearch->numServers].m_port = port;
					pSearch->numServers++;

					NetLog("[Lobby] Found Session %u.%u.%u.%u:%u", ((uint8*)&ip)[0], ((uint8*)&ip)[1], ((uint8*)&ip)[2], ((uint8*)&ip)[3], port);
				}
			}

			TMemHdl params = m_lobby->MemAlloc(pPacket->GetReadBufferSize());
			memcpy(m_lobby->MemGetPtr(params), pPacket->GetReadBuffer(), pPacket->GetReadBufferSize());
			TO_GAME_FROM_LOBBY(&CCryLANMatchMaking::ServerDataToGame, this, taskID, ip, port, params, pPacket->GetReadBufferSize());
		}
	}
}

void CCryLANMatchMaking::EndSessionGetUsers(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SCryUserInfoResult temp;
		int a;

		// Glue in local user(s)
		SCryLANUserID* pID = new SCryLANUserID;

		if (pID)
		{
			pID->id = GetLANUserID(m_sessions[pTask->session].localConnection.uid);
			temp.m_userID = pID;
			temp.m_conID = m_sessions[pTask->session].localConnection.uid;
			temp.m_isDedicated = gEnv->IsDedicated() && (m_sessions[pTask->session].localFlags & CRYSESSION_LOCAL_FLAG_HOST);
			cry_strcpy(temp.m_userName, m_sessions[pTask->session].localConnection.name);
			memcpy(temp.m_userData, m_sessions[pTask->session].localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
			((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &temp, pTask->cbArg);
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_OutOfMemory);
		}

		for (a = 0; a < MAX_LOBBY_CONNECTIONS; a++)
		{
			if (m_sessions[pTask->session].remoteConnection[a].used)
			{
				pID = new SCryLANUserID;

				if (pID)
				{
					pID->id = GetLANUserID(m_sessions[pTask->session].remoteConnection[a].uid);
					temp.m_userID = pID;
					temp.m_conID = m_sessions[pTask->session].remoteConnection[a].uid;
					temp.m_isDedicated = m_sessions[pTask->session].remoteConnection[a].m_isDedicated;
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

		((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_Success, NULL, pTask->cbArg);
	}
	else
	{
		((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
	}
}

CrySessionID CCryLANMatchMaking::SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h)
{
	return NULL;
}

void CCryLANMatchMaking::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

	switch (pSPacket->StartRead())
	{
	case eLANPT_MM_RequestServerData:
		SendServerData(addr, pSPacket);
		break;

	case eLANPT_MM_ServerData:
		ProcessServerData(addr, pSPacket);
		break;

	case eLANPT_SessionRequestJoin:
		ProcessSessionRequestJoin(addr, pSPacket);
		break;

	case eLANPT_SessionRequestJoinResult:
		ProcessSessionRequestJoinResult(addr, pSPacket);
		break;

	case eLANPT_SessionAddRemoteConnections:
		ProcessSessionAddRemoteConnections(addr, pSPacket);
		break;

	#if NETWORK_HOST_MIGRATION
	case eLANPT_HostMigrationStart:
		ProcessHostMigrationStart(addr, pSPacket);
		break;

	case eLANPT_HostMigrationServer:
		ProcessHostMigrationFromServer(addr, pSPacket);
		break;

	case eLANPT_HostMigrationClient:
		ProcessHostMigrationFromClient(addr, pSPacket);
		break;
	#endif

	case eLANPT_UserData:
		ProcessLocalUserData(addr, pSPacket);
		break;

	default:
		CCryMatchMaking::OnPacket(addr, pSPacket);
		break;
	}
}

	#if NETWORK_HOST_MIGRATION
void CCryLANMatchMaking::HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	SSession* pSession = &m_sessions[h];

	pSession->hostMigrationInfo.Reset();

	if (pSession->newHostUID.m_uid == pSession->localConnection.uid.m_uid)
	{
		// New host is me so no need to be told the new host address on LAN
		pSession->hostMigrationInfo.m_newHostAddress = TNetAddress(TLocalNetAddress());
		pSession->hostMigrationInfo.m_newHostAddressValid = true;
	}
}

ECryLobbyError CCryLANMatchMaking::HostMigrationServer(SHostMigrationInfo* pInfo)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(pInfo->m_session);
	if (h != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		pSession->localFlags |= CRYSESSION_LOCAL_FLAG_HOST;
		pSession->hostConnectionID = CryMatchMakingInvalidConnectionID;
		error = StartTask(eT_SessionMigrateHostServer, &mmTaskID, NULL, h, NULL, NULL);

		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[mmTaskID];
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
			DetectHostMigrationTaskBreak(h, "HostMigrationServer()");
		#endif
			pSession->hostMigrationInfo.m_taskID = pTask->lTaskID;

			NetLog("[Host Migration]: matchmaking migrating " PRFORMAT_SH " on the SERVER", PRARG_SH(h));
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Host Migration]: CCryLANMatchMaking::HostMigrationServer(): " PRFORMAT_SH ", error %d", PRARG_SH(h), error);
	}

	return error;
}

void CCryLANMatchMaking::HostMigrationServerNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	if (pTask->canceled)
	{
		NetLog("[Host Migration]: HostMigrationServerNT() cancelled for " PRFORMAT_SH " - bailing", PRARG_SH(pTask->session));
		StopTaskRunning(mmTaskID);
		return;
	}

	NetLog("[Host Migration]: matchmaking SERVER migrated session successfully - informing CLIENTS");

	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT32Size;
	CCrySharedLobbyPacket packet;

	if (packet.CreateWriteBuffer(MaxBufferSize))
	{
		packet.StartWrite(eLANPT_HostMigrationServer, true);
		packet.WriteUINT32(mmTaskID);

		SendToAll(mmTaskID, &packet, pTask->session, pSession->serverConnectionID);

		pSession->hostMigrationInfo.m_sessionMigrated = true;
	}

	StartSubTask(eT_SessionMigrateHostFinish, mmTaskID);
	pTask->StartTimer();
}

ECryLobbyError CCryLANMatchMaking::HostMigrationClient(CryLobbySessionHandle h, CryMatchMakingTaskID hostTaskID)
{
	ECryLobbyError error = eCLE_Success;

	if ((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[h];
		CryMatchMakingTaskID mmTaskID;

		pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST;
		error = StartTask(eT_SessionMigrateHostClient, &mmTaskID, NULL, h, NULL, NULL);

		if (error == eCLE_Success)
		{
			NetLog("[Host Migration]: matchmaking migrating to the new session on the CLIENT");

			STask* pTask = &m_task[mmTaskID];
			pTask->returnTaskID = hostTaskID;
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
			DetectHostMigrationTaskBreak(h, "HostMigrationClient()");
		#endif
			pSession->hostMigrationInfo.m_taskID = pTask->lTaskID;
			StartTaskRunning(mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[Host Migration]: CCryLANMatchMaking::HostMigrationClient() error %d", error);
	}

	return error;

}

void CCryLANMatchMaking::TickHostMigrationClientNT(CryMatchMakingTaskID mmTaskID)
{
	NetLog("[Host Migration]: matchmaking CLIENT migrated to new session successfully");

	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	SSession::SRConnection* pConnection = &pSession->remoteConnection[pSession->hostConnectionID];

	if (m_lobby->ConnectionGetState(pConnection->connectionID) == eCLCS_Connected)
	{
		const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT32Size;
		CCrySharedLobbyPacket packet;

		if (packet.CreateWriteBuffer(MaxBufferSize))
		{
			packet.StartWrite(eLANPT_HostMigrationClient, true);
			packet.WriteUINT32(pTask->returnTaskID);

			Send(mmTaskID, &packet, pTask->session, pSession->hostConnectionID);
		}

		NetLog("[Host Migration]: matchmaking CLIENT migrated to new " PRFORMAT_SH " successfully", PRARG_SH(pTask->session));
		pSession->hostMigrationInfo.m_sessionMigrated = true;
		pSession->hostMigrationInfo.m_matchMakingFinished = true;
		pSession->hostMigrationInfo.m_newHostAddressValid = true;
	}
	else
	{
		NetLog("[Host Migration]: matchmaking CLIENT migrated to new " PRFORMAT_SH " successfully, but host disconnected", PRARG_SH(pTask->session));
	}

	StopTaskRunning(mmTaskID);
}

void CCryLANMatchMaking::ProcessHostMigrationFromServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	uint32 bufferPos = 0;
	CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;

	pPacket->StartRead();
	CryMatchMakingTaskID hostTaskID = pPacket->ReadUINT32();

	SCryMatchMakingConnectionUID mmCxUID = pPacket->GetFromConnectionUID();
	CryMatchMakingConnectionID mmCxID = CryMatchMakingInvalidConnectionID;

	if (FindConnectionFromUID(mmCxUID, &h, &mmCxID))
	{
		SSession* pSession = &m_sessions[h];
		pSession->hostConnectionID = mmCxID;

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			NetLog("[Host Migration]: ignoring session details from " PRFORMAT_SHMMCINFO " as I am already the server " PRFORMAT_UID,
			       PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, mmCxUID), PRARG_UID(pSession->localConnection.uid));
		}
		else
		{
			NetLog("[Host Migration]: matchmaking CLIENT received session details from " PRFORMAT_SHMMCINFO " - migrating to new LAN session",
			       PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, mmCxUID));
			HostMigrationClient(h, hostTaskID);

			if (m_lobby)
			{
				TMemHdl mh = m_lobby->MemAlloc(sizeof(SCryLobbyRoomOwnerChanged));
				if (mh != TMemInvalidHdl)
				{
					SCryLobbyRoomOwnerChanged* pOwnerData = (SCryLobbyRoomOwnerChanged*)m_lobby->MemGetPtr(mh);

					pOwnerData->m_session = CreateGameSessionHandle(h, pSession->localConnection.uid);
					m_lobby->DecodeAddress(addr, &pOwnerData->m_ip, &pOwnerData->m_port);
					pOwnerData->m_ip = htonl(pOwnerData->m_ip);

					TO_GAME_FROM_LOBBY(&CCryLANMatchMaking::DispatchRoomOwnerChangedEvent, this, mh);
				}
			}

			pSession->hostMigrationInfo.m_newHostAddress = addr;
			return;
		}
	}
	else
	{
		NetLog("[Host Migration]: matchmaking CLIENT received session details from unknown connection " PRFORMAT_ADDR " - ignoring", PRARG_ADDR(addr));
	}

	// If HostMigrationClient() was not called, this client will be pruned (after timeout) on the new host
	CRY_ASSERT_MESSAGE(false, "ProcessHostMigrationFromServer failed to call HostMigrationClient");
}

void CCryLANMatchMaking::ProcessHostMigrationFromClient(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	uint32 bufferPos = 0;

	pPacket->StartRead();
	CryMatchMakingTaskID hostTaskID = pPacket->ReadUINT32();

	CryLobbyConnectionID c;
	if (m_lobby->ConnectionFromAddress(&c, addr))
	{
		hostTaskID = FindTaskFromTaskTaskID(eT_SessionMigrateHostFinish, hostTaskID);
		if (hostTaskID != CryMatchMakingInvalidTaskID)
		{
			STask* pTask = &m_task[hostTaskID];
			SSession* pSession = &m_sessions[pTask->session];

			for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
			{
				SSession::SRConnection* connection = &pSession->remoteConnection[i];

				if (connection->used && (connection->connectionID == c))
				{
					connection->m_migrated = true;
					float timetaken = (g_time - pSession->hostMigrationInfo.m_startTime).GetSeconds();
					NetLog("[Host Migration]: matchmaking SERVER received CLIENT confirmation " PRFORMAT_SHMMCINFO " @ %fs", PRARG_SHMMCINFO(pTask->session, CryMatchMakingConnectionID(i), connection->connectionID, connection->uid), timetaken);
					return;
				}
			}
		}
	}

	SCryMatchMakingConnectionUID mmCxUID = pPacket->GetFromConnectionUID();
	NetLog("[Host Migration]: matchmaking SERVER received CLIENT confirmation for unknown connection " PRFORMAT_ADDR " " PRFORMAT_UID, PRARG_ADDR(addr), PRARG_UID(mmCxUID));
}

void CCryLANMatchMaking::TickHostMigrationFinishNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	bool finished = true;

	for (uint32 index = 0; finished && index < MAX_LOBBY_CONNECTIONS; ++index)
	{
		if (pSession->remoteConnection[index].used && !pSession->remoteConnection[index].m_migrated && !pSession->remoteConnection[index].uid.m_uid == DEDICATED_SERVER_CONNECTION_UID)
		{
			finished = false;
		}
	}

	float timeout = CLobbyCVars::Get().hostMigrationTimeout - (CryMatchMakingHostMigratedConnectionConfirmationWindow / 1000.0f);
	float timetaken = (g_time - pSession->hostMigrationInfo.m_startTime).GetSeconds();

	if (finished || (pTask->canceled) || (timetaken > timeout))
	{
		NetLog("[Host Migration]: matchmaking host migration finished for " PRFORMAT_SH, PRARG_SH(pTask->session));

		for (uint32 index = 0; index < MAX_LOBBY_CONNECTIONS; ++index)
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[index];

			if (pConnection->used && pConnection->uid.m_uid != DEDICATED_SERVER_CONNECTION_UID)
			{
				if (pConnection->m_migrated)
				{
					// This connection migrated so reset for next time
					NetLog("[Host Migration]: client " PRFORMAT_SHMMCINFO " migrated successfully @ %fs", PRARG_SHMMCINFO(pTask->session, CryMatchMakingConnectionID(index), pConnection->connectionID, pConnection->uid), timetaken);
					pConnection->m_migrated = false;
				}
				else
				{
					// Prune this connection
					if (CLobbyCVars::Get().netPruneConnectionsAfterHostMigration != 0)
					{
						NetLog("[Host Migration]: client " PRFORMAT_SHMMCINFO " failed to migrate - pruning @ %fs", PRARG_SHMMCINFO(pTask->session, CryMatchMakingConnectionID(index), pConnection->connectionID, pConnection->uid), timetaken);
						SessionDisconnectRemoteConnectionViaNub(pTask->session, index, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_FailedToMigrateToNewHost, "Failed to migrate player to new game");
					}
					else
					{
						NetLog("[Host Migration]: client " PRFORMAT_SHMMCINFO " failed to migrate but pruning disabled @ %fs", PRARG_SHMMCINFO(pTask->session, CryMatchMakingConnectionID(index), pConnection->connectionID, pConnection->uid), timetaken);
					}
				}
			}
		}

		pSession->hostMigrationInfo.m_matchMakingFinished = true;
		StopTaskRunning(mmTaskID);
	}
}

bool CCryLANMatchMaking::GetNewHostAddress(char* address, SHostMigrationInfo* pInfo)
{
	CryLobbySessionHandle sessionIndex = GetSessionHandleFromGameSessionHandle(pInfo->m_session);
	SSession* pSession = &m_sessions[sessionIndex];
	bool success = pSession->hostMigrationInfo.m_newHostAddressValid;

	if (success)
	{
		if (pSession->serverConnectionID != CryMatchMakingInvalidConnectionID)
		{
			success = (pSession->desiredHostUID != CryMatchMakingInvalidConnectionUID);

			if (success)
			{
				CryMatchMakingConnectionID connID = CryMatchMakingInvalidConnectionID;

				if (pSession->localConnection.uid == pSession->desiredHostUID)
				{
					pSession->hostMigrationInfo.SetIsNewHost(true);
					pSession->newHostUID = pSession->desiredHostUID;
					NetLog("[Host Migration]: " PRFORMAT_SH " becoming the new host...", PRARG_SH(sessionIndex));
				}
				else if (FindConnectionFromSessionUID(sessionIndex, pSession->desiredHostUID, &connID))
				{
					pSession->hostMigrationInfo.SetIsNewHost(false);
					pSession->newHostUID = pSession->desiredHostUID;
					NetLog("[Host Migration]: " PRFORMAT_SH " new host is " PRFORMAT_UID, PRARG_SH(sessionIndex), PRARG_UID(pSession->newHostUID));
				}
				else
				{
					success = false;
					NetLog("[Host Migration]: " PRFORMAT_SH " new host is " PRFORMAT_UID " but it's not here", PRARG_SH(sessionIndex), PRARG_UID(pSession->newHostUID));
				}
			}

			return success;
		}
	}

	if (success)
	{
		// Decode the address into an ip string
		if (stl::get_if<TLocalNetAddress>(&pSession->hostMigrationInfo.m_newHostAddress))
		{
			pSession->hostMigrationInfo.SetIsNewHost(true);
			NetLog("[Host Migration]: " PRFORMAT_SH " becoming the new host...", PRARG_SH(sessionIndex));

			const SCryLobbyParameters& lobbyParams = m_lobby->GetLobbyParameters();
			uint16 port = lobbyParams.m_listenPort;
			cry_sprintf(address, HOST_MIGRATION_MAX_SERVER_NAME_SIZE, "%s:%u", LOCAL_CONNECTION_STRING, port);
		}
		else
		{
			pSession->hostMigrationInfo.SetIsNewHost(false);
			NetLog("[Host Migration]: " PRFORMAT_SH " new host is " PRFORMAT_UID, PRARG_SH(sessionIndex), PRARG_UID(pSession->newHostUID));
			SIPv4Addr* pAddr = stl::get_if<SIPv4Addr>(&pSession->hostMigrationInfo.m_newHostAddress);
			if (pAddr)
			{
				pSession->id.m_ip = ntohl(pAddr->addr);
				pSession->id.m_port = pAddr->port;
			}
			m_lobby->DecodeAddress(pSession->hostMigrationInfo.m_newHostAddress, address, false);
		}
	}

	return success;
}

void CCryLANMatchMaking::HostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	CryLobbySessionHandle h = pTask->session;
	SSession* pSession = &m_sessions[h];

	// Ensure we're the host and we've not started the session
	if ((pSession->localFlags & (CRYSESSION_LOCAL_FLAG_HOST | CRYSESSION_LOCAL_FLAG_STARTED)) == CRYSESSION_LOCAL_FLAG_HOST)
	{
		// Determine if host hinting has a better host
		SHostHintInformation newHost;
		CryMatchMakingConnectionID newHostID;
		if (FindConnectionFromSessionUID(pTask->session, pSession->newHostUID, &newHostID))
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[newHostID];
			newHost = pConnection->hostHintInfo;
		}

		bool isBetterHost = false;
		if (SortNewHostPriorities_ComparatorHelper(newHost, pSession->hostHintInfo) < 0)
		{
			isBetterHost = true;
		}

		if (isBetterHost)
		{
			if (HostMigrationInitiate(h, eDC_Unknown))
			{
				// I'm currently the host but there's a better host out there...
				const uint32 maxBufferSize = CryLobbyPacketHeaderSize;
				uint8 buffer[maxBufferSize];
				CCryLobbyPacket packet;

				NetLog("[Host Migration]: local server is not best host for " PRFORMAT_SH " - instructing clients to choose new host", PRARG_SH(pTask->session));
				packet.SetWriteBuffer(buffer, maxBufferSize);
				packet.StartWrite(eLANPT_HostMigrationStart, true);

				SendToAll(mmTaskID, &packet, h, CryMatchMakingInvalidConnectionID);
				pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_HOST;
			}
			else
			{
				NetLog("[Host Migration]: local server is not best host for " PRFORMAT_SH ", but session is not migratable", PRARG_SH(pTask->session));
				UpdateTaskError(mmTaskID, eCLE_SessionNotMigratable);
				StopTaskRunning(mmTaskID);
			}
		}
		else
		{
			NetLog("[Host Migration]: local server is best host for " PRFORMAT_SH, PRARG_SH(pTask->session));
		}
	}
	else
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_STARTED)
		{
			NetLog("[Host Migration]: SessionEnsureBestHost() called after the session had started");
		}
	}
}

void CCryLANMatchMaking::TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	StopTaskRunning(mmTaskID);
}

void CCryLANMatchMaking::ProcessHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h = CryLobbyInvalidSessionHandle;
	CryMatchMakingConnectionID mmCxID = CryMatchMakingInvalidConnectionID;

	SCryMatchMakingConnectionUID mmCxUID = pPacket->GetFromConnectionUID();

	if (FindConnectionFromUID(mmCxUID, &h, &mmCxID))
	{
		SSession* pSession = &m_sessions[h];

		NetLog("[Host Migration]: received host migration push event for " PRFORMAT_SHMMCINFO, PRARG_SHMMCINFO(h, mmCxID, pSession->remoteConnection[mmCxID].connectionID, mmCxUID));

		if (!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST))
		{
			if (pSession->hostMigrationInfo.m_state == eHMS_Idle)
			{
				NetLog("[Host Migration]: remote server is not best host for " PRFORMAT_SH " - instructed to choose new host", PRARG_SH(h));
				HostMigrationInitiate(h, eDC_Unknown);
			}
			else
			{
				NetLog("[Host Migration]: ignoring pushed migration event - already migrating " PRFORMAT_SH, PRARG_SH(h));
			}
		}
	}
	else
	{
		NetLog("[Host Migration]: received push migration event from unknown host, " PRFORMAT_ADDR " " PRFORMAT_UID, PRARG_ADDR(addr), PRARG_UID(mmCxUID));
	}
}
	#endif

ECryLobbyError CCryLANMatchMaking::SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError rc = eCLE_Success;

	#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h = GetSessionHandleFromGameSessionHandle(gh);

	if (h != CryLobbyInvalidSessionHandle)
	{
		CryMatchMakingTaskID mmTaskID;
		SSession* pSession = &m_sessions[h];
		rc = StartTask(eT_SessionMigrateHostStart, &mmTaskID, pTaskID, h, (void*)pCB, pCBArg);

		if (rc == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCryLANMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	#endif

	return rc;
}

void CCryLANMatchMaking::SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED)
	{
		if (id == CryMatchMakingInvalidConnectionID)
		{
			SCryLANUserID* pID = new SCryLANUserID;

			if (pID)
			{
				SSession::SLConnection* pConnection = &pSession->localConnection;
				if (pConnection->numUsers > 0)
				{
					SCryUserInfoResult userInfo;

					pID->id = GetLANUserID(pConnection->uid);
					userInfo.m_userID = pID;
					userInfo.m_conID = pConnection->uid;
					userInfo.m_isDedicated = gEnv->IsDedicated() && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST);
					cry_strcpy(userInfo.m_userName, pConnection->name);
					memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

					CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
				}
			}
		}
		else
		{
			SCryLANUserID* pID = new SCryLANUserID;

			if (pID)
			{
				SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
				if (pConnection->numUsers > 0)
				{
					SCryUserInfoResult userInfo;

					pID->id = GetLANUserID(pConnection->uid);
					userInfo.m_userID = pID;
					userInfo.m_conID = pConnection->uid;
					userInfo.m_isDedicated = pConnection->m_isDedicated;
					cry_strcpy(userInfo.m_userName, pConnection->name);
					memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

					CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
				}
				else
				{
					InitialDispatchUserPackets(h, id);
					pConnection->gameInformedConnectionEstablished = true;
				}
			}
		}
	}
}

void CCryLANMatchMaking::InitialUserDataEvent(CryLobbySessionHandle h)
{
	SSession* pSession = &m_sessions[h];

	pSession->localFlags |= CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED;
	SessionUserDataEvent(eCLSE_SessionUserJoin, h, CryMatchMakingInvalidConnectionID);

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];

		if (pConnection->used)
		{
			SessionUserDataEvent(eCLSE_SessionUserJoin, h, i);
		}
	}
}

void CCryLANMatchMaking::DispatchRoomOwnerChangedEvent(TMemHdl mh)
{
	UCryLobbyEventData eventData;
	eventData.pRoomOwnerChanged = (SCryLobbyRoomOwnerChanged*)m_lobby->MemGetPtr(mh);

	m_lobby->DispatchEvent(eCLSE_RoomOwnerChanged, eventData);

	m_lobby->MemFree(mh);
}

TNetAddress CCryLANMatchMaking::GetHostAddressFromSessionHandle(CrySessionHandle gh)
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
		return TNetAddress(TLocalNetAddress(m_lobby->GetInternalSocketPort(eCLS_LAN)));
	}

	SIPv4Addr addr4;

	addr4.addr = ntohl(pSession->id.m_ip);
	addr4.port = pSession->id.m_port;

	return TNetAddress(addr4);
}

void CCryLANMatchMaking::Kick(const CryUserID* pUserID, EDisconnectionCause cause)
{
	if (pUserID != NULL)
	{
		const SCryLANUserID* pLANUserID = static_cast<const SCryLANUserID*>(pUserID->get());
		KickCmd(CryLobbyInvalidConnectionID, pLANUserID->id, NULL, cause);
	}
}

uint64 CCryLANMatchMaking::GetConnectionUserID(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const
{
	SSession::SRConnection* pPlatformConnection = reinterpret_cast<SSession::SRConnection*>(pConnection);

	if (pPlatformConnection->used)
	{
		return GetLANUserID(pPlatformConnection->uid);
	}

	return INVALID_USER_ID;
}

#endif // USE_LAN
