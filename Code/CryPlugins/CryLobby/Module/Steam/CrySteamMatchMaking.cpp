// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamMatchMaking.cpp
//  Created:     11/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryMatchMaking
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Steam/CrySteamLobby.h" // CrySteamMatchMaking.h included here

#if USE_STEAM && USE_CRY_MATCHMAKING

	#include "CrySharedLobbyPacket.h"

////////////////////////////////////////////////////////////////////////////////

// Session create task params
	#define SESSION_CREATE_PARAM_SESSION_DATA 0       // ptr
	#define SESSION_CREATE_PARAM_USER_DATA    1       // ptr

// Session update task params
	#define SESSION_UPDATE_PARAM_USER_DATA     0      // ptr
	#define SESSION_UPDATE_PARAM_NUM_USER_DATA 0      // int

// Session update slots task params
	#define SESSION_UPDATE_SLOTS_PARAM_NUM_PUBLIC  0  // int
	#define SESSION_UPDATE_SLOTS_PARAM_NUM_PRIVATE 1  // int

// Session search task params
	#define SESSION_SEARCH_PARAM_GAME_PARAM     0     // ptr
	#define SESSION_SEARCH_PARAM_GAME_USER_DATA 1     // ptr

////////////////////////////////////////////////////////////////////////////////

	#define SESSION_SEARCH_TIMEOUT (10000)
	#define SESSION_JOIN_TIMEOUT   (10000)

////////////////////////////////////////////////////////////////////////////////

	#define STEAM_GAME_TYPE_RANKED   (1)
	#define STEAM_GAME_TYPE_UNRANKED (0)

// keys
	#define MAX_STEAM_KEY_VALUE_SIZE        (256)
	#define STEAM_KEY_SESSION_NAME          "session_name"
	#define STEAM_KEY_SESSION_GAME_TYPE     "game_type"
	#define STEAM_KEY_SESSION_PUBLIC_SLOTS  "public_slots"
	#define STEAM_KEY_SESSION_PRIVATE_SLOTS "private_slots"
	#define STEAM_KEY_SESSION_USER_DATA     "user_key_"

////////////////////////////////////////////////////////////////////////////////

CCrySteamMatchMaking::CCrySteamMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType)
	: CCryMatchMaking(lobby, service, serviceType)
	//////////////////////////////////////////////////////////////////////////////
	// Steam callback registration
	, m_SteamOnShutdown(this, &CCrySteamMatchMaking::OnSteamShutdown)
	, m_SteamOnLobbyDataUpdated(this, &CCrySteamMatchMaking::OnLobbyDataUpdated)
	, m_SteamOnP2PSessionRequest(this, &CCrySteamMatchMaking::OnP2PSessionRequest)
	, m_SteamOnLobbyChatUpdate(this, &CCrySteamMatchMaking::OnLobbyChatUpdate)
	//////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::Initialise()
{
	ECryLobbyError error = CCryMatchMaking::Initialise();

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::Tick(CTimeValue tv)
{
	CCryMatchMaking::Tick(tv);

	for (uint32 i = 0; i < MAX_MATCHMAKING_TASKS; i++)
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

		if ((pTask->used) && (pTask->running) && (CheckTaskStillValid(i)))
		{
			switch (pTask->subTask)
			{
			case eT_SessionMigrate:
				TickSessionMigrate(i);
				break;

			case eT_SessionSearch:
				TickSessionSearch(i);
				break;

			case eT_SessionJoin:
				TickSessionJoin(i);
				break;

			//#if NETWORK_HOST_MIGRATION
			//			case eT_SessionMigrateHostStart:
			//				TickHostMigrationStartNT(i);
			//				break;
			//
			//			case eT_SessionMigrateHostServerWaitStart:
			//				TickHostMigrationServerWaitStartNT(i);
			//				break;
			//
			//			case eT_SessionMigrateHostServer:
			//				TickHostMigrationServerNT(i);
			//				break;
			//
			//			case eT_SessionMigrateHostClientWaitStart:
			//				TickHostMigrationClientWaitStartNT(i);
			//				break;
			//
			//			case eT_SessionMigrateHostClient:
			//				TickHostMigrationClientNT(i);
			//				break;
			//
			//			case eT_SessionMigrateHostFinish:
			//				TickHostMigrationFinishNT(i);
			//				break;
			//#endif

			default:
				TickBaseTask(i);
				break;
			}
		}

		if ((pTask->canceled) || (pTask->error != eCLE_Success))
		{
			StopTaskRunning(i);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	CCrySharedLobbyPacket* pSharedPacket = (CCrySharedLobbyPacket*)pPacket;

	uint32 type = pSharedPacket->StartRead();
	switch (type)
	{
	case eSteamPT_SessionRequestJoinResult:
		ProcessSessionRequestJoinResult(addr, pSharedPacket);
		break;

	case eSteamPT_UserData:
		ProcessLocalUserData(addr, pSharedPacket);
		break;

	default:
		CCryMatchMaking::OnPacket(addr, pSharedPacket);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////

/*
   void						ProfileSettingsChanged(ULONG_PTR param);
   void						UserSignedOut(uint32 user);
 */

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionRegisterUserData(SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	if (numData < MAX_MATCHMAKING_SESSION_USER_DATA)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionRegisterUserData, false, &mmTaskID, taskID, CryLobbyInvalidSessionHandle, (void*)cb, cbArg);

		if (error == eCLE_Success)
		{
			for (int i = 0; i < numData; i++)
			{
				m_registeredUserData.data[i] = data[i];
			}
			m_registeredUserData.num = numData;

			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_OutOfSessionUserData;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionRegisterUserData error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionCreate(uint32* users, int numUsers, uint32 flags, SCrySessionData* data, CryLobbyTaskID* taskID, CryMatchmakingSessionCreateCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle h;
	ECryLobbyError error = CreateSessionHandle(&h, true, flags, numUsers);

	if (error == eCLE_Success)
	{
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionCreate, false, &mmTaskID, taskID, h, cb, cbArg);

		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[mmTaskID];
			SSession* pSession = &m_sessions[h];

			if (data->m_ranked)
			{
				pSession->gameType = STEAM_GAME_TYPE_RANKED;
			}
			else
			{
				pSession->gameType = STEAM_GAME_TYPE_UNRANKED;
			}
			pSession->gameMode = 0;

			pSession->data.m_numPublicSlots = data->m_numPublicSlots;
			pSession->data.m_numPrivateSlots = data->m_numPrivateSlots;
			pSession->data.m_ranked = data->m_ranked;

			NetLog("[MatchMaking] Created local connection " PRFORMAT_SH " " PRFORMAT_UID, PRARG_SH(h), PRARG_UID(pSession->localConnection.uid));

			error = CreateTaskParam(mmTaskID, SESSION_CREATE_PARAM_SESSION_DATA, data, 1, sizeof(SCrySessionData));

			if (error == eCLE_Success)
			{
				error = CreateTaskParam(mmTaskID, SESSION_CREATE_PARAM_USER_DATA, data->m_data, data->m_numData, data->m_numData * sizeof(data->m_data[0]));

				if (error == eCLE_Success)
				{
					FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
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

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionCreate error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionMigrate(CrySessionHandle h, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	NetLog("[TODO] CCrySteamMatchMaking::SessionMigrate()");
	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionMigrate error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionUpdate(CrySessionHandle h, SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryMatchMakingTaskID mmTaskID;
			error = StartTask(eT_SessionUpdate, false, &mmTaskID, taskID, lsh, cb, cbArg);

			if (error == eCLE_Success)
			{
				STask* task = &m_task[mmTaskID];
				error = CreateTaskParam(mmTaskID, SESSION_UPDATE_PARAM_USER_DATA, data, numData, numData * sizeof(data[0]));

				if (error == eCLE_Success)
				{
					FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
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

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionUpdate error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionUpdateSlots(CrySessionHandle h, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryMatchMakingTaskID mmTaskID;
			error = StartTask(eT_SessionUpdateSlots, false, &mmTaskID, pTaskID, lsh, pCB, pCBArg);

			if (error == eCLE_Success)
			{
				STask* task = &m_task[mmTaskID];

				CreateTaskParam(mmTaskID, SESSION_UPDATE_SLOTS_PARAM_NUM_PUBLIC, NULL, numPublic, 0);
				CreateTaskParam(mmTaskID, SESSION_UPDATE_SLOTS_PARAM_NUM_PRIVATE, NULL, numPrivate, 0);
				FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
			}
		}
		else
		{
			error = eCLE_InvalidRequest;
		}
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionUpdateSlots return %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionQuery(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionQuery, false, &mmTaskID, pTaskID, lsh, (void*)pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionQuery error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionGetUsers(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		SSession* pSession = &m_sessions[lsh];
		CryMatchMakingTaskID tid;

		error = StartTask(eT_SessionGetUsers, false, &tid, pTaskID, lsh, pCB, pCBArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, tid);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionGetUsers error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionStart(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionStart, false, &mmTaskID, taskID, lsh, cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_InvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionStart error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionEnd(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionEnd, false, &mmTaskID, taskID, lsh, cb, cbArg);

		if (error == eCLE_Success)
		{
			pSession->localFlags &= ~CRYSESSION_LOCAL_FLAG_STARTED;
			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionEnd error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionDelete(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SSession* pSession = &m_sessions[lsh];
		CryMatchMakingTaskID mmTaskID;

		error = StartTask(eT_SessionDelete, false, &mmTaskID, taskID, lsh, cb, cbArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
	}
	else
	{
		error = eCLE_SuccessInvalidSession;
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionDelete error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionSearch(uint32 user, SCrySessionSearchParam* param, CryLobbyTaskID* taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg)
{
	LOBBY_AUTO_LOCK;

	CryMatchMakingTaskID mmTaskID;

	ECryLobbyError error = StartTask(eT_SessionSearch, false, &mmTaskID, taskID, CryLobbyInvalidSessionHandle, cb, cbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[mmTaskID];

		error = CreateTaskParam(mmTaskID, SESSION_SEARCH_PARAM_GAME_PARAM, param, 1, sizeof(SCrySessionSearchParam));

		if (error == eCLE_Success)
		{
			error = CreateTaskParam(mmTaskID, SESSION_SEARCH_PARAM_GAME_USER_DATA, param->m_data, param->m_numData, param->m_numData * sizeof(param->m_data[0]));

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
			}
		}

		if (error != eCLE_Success)
		{
			FreeTask(mmTaskID);
		}
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionSearch error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionJoin(uint32* pUsers, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionJoinCallback pCB, void* pCBArg)
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
			SCrySteamSessionID* pSteamID = (SCrySteamSessionID*)id.get();
			SSession* pSession = &m_sessions[h];

			pSession->m_id.m_steamID = pSteamID->m_steamID;

			FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
		}
		else
		{
			FreeSessionHandle(h);
		}
	}

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionJoin error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionSetLocalUserData(CrySessionHandle h, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		if (dataSize <= CRYLOBBY_USER_DATA_SIZE_IN_BYTES)
		{
			SSession* pSession = &m_sessions[lsh];
			SSession::SLConnection* pLConnection = &pSession->localConnection;
			CryMatchMakingTaskID mmTaskID;

			memcpy(pLConnection->userData, pData, dataSize);

			error = StartTask(eT_SessionSetLocalUserData, false, &mmTaskID, pTaskID, lsh, (void*)pCB, pCBArg);

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCrySteamMatchMaking::StartTaskRunning, this, mmTaskID);
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

	if (error != eCLE_Success)
	{
		NetLog("[MatchMaking] Start SessionSetLocalUserData error %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

CrySessionID CCrySteamMatchMaking::SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h)
{
	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (m_sessions[lsh].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		SCrySteamSessionID* pID = new SCrySteamSessionID(m_sessions[lsh].m_id.m_steamID, false);

		if (pID)
		{
			return pID;
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

uint32 CCrySteamMatchMaking::GetSessionIDSizeInPacket() const
{
	return SCrySteamSessionID::GetSizeInPacket();
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const
{
	ECryLobbyError result = eCLE_InternalError;

	if (pPacket != NULL)
	{
		SCrySteamSessionID blank;
		SCrySteamSessionID* pID = &blank;

		if (sessionId != CrySessionInvalidID)
		{
			pID = (SCrySteamSessionID*)sessionId.get();
			if (pID == NULL)
			{
				pID = &blank;
			}
		}

		pID->WriteToPacket(pPacket);
		result = eCLE_Success;
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////

CrySessionID CCrySteamMatchMaking::ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const
{
	CrySessionID resultID = CrySessionInvalidID;

	if (pPacket != NULL)
	{
		SCrySteamSessionID* pID = new SCrySteamSessionID();
		if (pID != NULL)
		{
			pID->ReadFromPacket(pPacket);
			resultID = pID;
		}
	}

	return resultID;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)
{
	ECryLobbyError rc = eCLE_Success;

	#if NETWORK_HOST_MIGRATION
	LOBBY_AUTO_LOCK;
	#endif // NETWORK_HOST_MIGRATION

	NetLog("[TODO] CCrySteamMatchMaking::SessionEnsureBestHost()");
	return rc;
}

////////////////////////////////////////////////////////////////////////////////

	#if NETWORK_HOST_MIGRATION
void CCrySteamMatchMaking::HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause)
{
	NetLog("[TODO] CCrySteamMatchMaking::HostMigrationInitialise()");
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::HostMigrationServer(SHostMigrationInfo* pInfo)
{
	NetLog("[TODO] CCrySteamMatchMaking::HostMigrationServer()");
	return eCLE_InvalidSession;
}

////////////////////////////////////////////////////////////////////////////////

bool CCrySteamMatchMaking::GetNewHostAddress(char* address, SHostMigrationInfo* pInfo)
{
	NetLog("[TODO] CCrySteamMatchMaking::GetNewHostAddress()");
	return true;
}

	#endif // NETWORK_HOST_MIGRATION

////////////////////////////////////////////////////////////////////////////////

SCrySteamSessionID CCrySteamMatchMaking::GetSteamSessionIDFromSession(CryLobbySessionHandle lsh)
{
	if (lsh != CryLobbyInvalidSessionHandle)
	{
		SSession* pSession = &m_sessions[lsh];
		return pSession->m_id;
	}

	return SCrySteamSessionID();
}

////////////////////////////////////////////////////////////////////////////////

TNetAddress CCrySteamMatchMaking::GetHostAddressFromSessionHandle(CrySessionHandle h)
{
	LOBBY_AUTO_LOCK;

	CryLobbySessionHandle lsh = GetSessionHandleFromGameSessionHandle(h);
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (lsh != CryLobbyInvalidSessionHandle && pSteamMatchmaking)
	{
		SSession* pSession = &m_sessions[lsh];
		SCrySteamUserID host;

		// Is there a game server?
		if (!(pSteamMatchmaking->GetLobbyGameServer(pSession->m_id.m_steamID, NULL, NULL, &host.m_steamID)))
		{
			// If not, use the lobby owner as host...
			host = pSteamMatchmaking->GetLobbyOwner(pSession->m_id.m_steamID);
		}

		if (!(host == SCrySteamUserID()))
		{
			if (host == pSession->localConnection.userID)
			{
				NetLog("[STEAM]: GetHostAddressFromSessionHandle(): I am the host");
				return TNetAddress(TLocalNetAddress(m_lobby->GetInternalSocketPort(eCLS_Online)));
			}

			NetLog("[STEAM]: GetHostAddressFromSessionHandle(): host is [%s]", host.GetGUIDAsString().c_str());
			return TNetAddress(LobbyIdAddr(host.m_steamID.ConvertToUint64()));
		}

		NetLog("[STEAM]: Unable to retrieve host from Steam lobby (session id [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());
	}

	return TNetAddress(SNullAddr());
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::LobbyAddrIDTick()
{
	// Nothing to do here
}

////////////////////////////////////////////////////////////////////////////////

bool CCrySteamMatchMaking::LobbyAddrIDHasPendingData()
{
	bool pendingData = false;

	if (ISteamNetworking* pSteamNetworking = SteamNetworking())
	{
		// We don't use size since we only send/receive up to MAX_UDP_PACKET_SIZE
		uint32 size = 0;
		pendingData = pSteamNetworking->IsP2PPacketAvailable(&size);
	}

	return pendingData;
}

////////////////////////////////////////////////////////////////////////////////

ESocketError CCrySteamMatchMaking::LobbyAddrIDSend(const uint8* buffer, uint32 size, const TNetAddress& addr)
{
	auto lobbyIdAddr = stl::get<LobbyIdAddr>(addr);
	CSteamID steamID(lobbyIdAddr.id);
	ISteamNetworking* pSteamNetworking = SteamNetworking();
	if (!pSteamNetworking)
	{
		return eSE_MiscFatalError;
	}

	bool sent = pSteamNetworking->SendP2PPacket(steamID, buffer, size, k_EP2PSendUnreliable);
	if (sent)
	{
		return eSE_Ok;
	}

	NetLog("[STEAM]: LobbyAddrIDSend() unreachable address");
	return eSE_UnreachableAddress;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::LobbyAddrIDRecv(void (* cb)(void*, uint8* buffer, uint32 size, CRYSOCKET sock, TNetAddress& addr), void* cbData)
{
	static uint8 s_recvBuffer[MAX_UDP_PACKET_SIZE];
	static TNetAddress s_recvAddr;
	static uint32 s_recvSize;
	static CSteamID sender;
	ISteamNetworking* pSteamNetworking = SteamNetworking();
	if (!pSteamNetworking)
	{
		return;
	}

	while (LobbyAddrIDHasPendingData())
	{
		if (pSteamNetworking->ReadP2PPacket(s_recvBuffer, MAX_UDP_PACKET_SIZE, &s_recvSize, &sender))
		{
			CCryLobby::SSocketService* pService = m_lobby->GetCorrectSocketService(eCLS_Online);
			if ((pService != NULL) && (pService->m_socket != NULL))
			{
				s_recvAddr = TNetAddress(LobbyIdAddr(sender.ConvertToUint64()));
				cb(cbData, s_recvBuffer, s_recvSize, pService->m_socket->GetSysSocket(), s_recvAddr);
			}
			else
			{
				NetLog("[STEAM]: receiving Steam traffic before online service initialised");
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

const char* CCrySteamMatchMaking::SSession::GetLocalUserName(uint32 localUserIndex) const
{
	if (localFlags & CRYSESSION_LOCAL_FLAG_USED)
	{
		return localConnection.name;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::SSession::Reset()
{
	PARENT::Reset();

	hostConnectionID = CryMatchMakingInvalidConnectionID;
	m_id = SCrySteamSessionID();
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::SSession::InitialiseLocalConnection(SCryMatchMakingConnectionUID uid)
{
	localConnection.pingToServer = CRYLOBBY_INVALID_PING;
	localConnection.used = true;
	ISteamUser* pSteamUser = SteamUser();
	ISteamFriends* pSteamFriends = SteamFriends();
	if (pSteamUser && pSteamFriends)
	{
		localConnection.userID = pSteamUser->GetSteamID();
		cry_strcpy(localConnection.name, pSteamFriends->GetPersonaName());
	}
	else
	{
		localConnection.userID = k_steamIDNil;
		cry_strcpy(localConnection.name, "");
	}
	localConnection.uid = uid;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers)
{
	ECryLobbyError error = CCryMatchMaking::CreateSessionHandle(h, host, createFlags, numUsers);

	if (error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[*h];

		memset(pSession->localConnection.userData, 0, sizeof(pSession->localConnection.userData));
		pSession->data.m_data = pSession->userData;
		pSession->data.m_numData = m_registeredUserData.num;

		for (uint32 i = 0; i < m_registeredUserData.num; i++)
		{
			pSession->data.m_data[i] = m_registeredUserData.data[i];
		}
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

uint64 CCrySteamMatchMaking::GetSIDFromSessionHandle(CryLobbySessionHandle h)
{
	CRY_ASSERT_MESSAGE((h < MAX_MATCHMAKING_SESSIONS) && (m_sessions[h].localFlags & CRYSESSION_LOCAL_FLAG_USED), "CCrySteamMatchMaking::GetSIDFromSessionHandle: invalid session handle");
	return m_sessions[h].m_id.m_steamID.ConvertToUint64();
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::SetSessionUserData(CryMatchMakingTaskID mmTaskID, SCrySessionUserData* pData, uint32 numData)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED && pSteamMatchmaking)
	{
		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> key;
			CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> value;

			for (uint32 index = 0; index < numData; ++index)
			{
				key.Format(STEAM_KEY_SESSION_USER_DATA "%d", index);

				switch (pData[index].m_type)
				{
				case eCLUDT_Int64:
					value.Format("%x:%x:%" PRIx64, pData[index].m_type, pData[index].m_id, pData[index].m_int64);
					break;
				case eCLUDT_Int32:
					value.Format("%x:%x:%x", pData[index].m_type, pData[index].m_id, pData[index].m_int32);
					break;
				case eCLUDT_Int16:
					value.Format("%x:%x:%x", pData[index].m_type, pData[index].m_id, pData[index].m_int16);
					break;
				case eCLUDT_Int8:
					value.Format("%x:%x:%x", pData[index].m_type, pData[index].m_id, pData[index].m_int8);
					break;
				case eCLUDT_Float64:
					value.Format("%x:%x:%f", pData[index].m_type, pData[index].m_id, pData[index].m_f64);
					break;
				case eCLUDT_Float32:
					value.Format("%x:%x:%f", pData[index].m_type, pData[index].m_id, pData[index].m_f32);
					break;
				}

				pSteamMatchmaking->SetLobbyData(pSession->m_id.m_steamID, key.c_str(), value.c_str());
			}
		}
		else
		{
			UpdateTaskError(mmTaskID, eCLE_InvalidRequest);
		}
	}
	else
	{
		UpdateTaskError(mmTaskID, eCLE_InvalidSession);
	}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::StartTask(ETask etask, bool startRunning, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg)
{
	CryMatchMakingTaskID tmpMMTaskID;
	CryMatchMakingTaskID* useMMTaskID = mmTaskID ? mmTaskID : &tmpMMTaskID;
	ECryLobbyError error = CCryMatchMaking::StartTask(etask, startRunning, useMMTaskID, lTaskID, h, cb, cbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[*useMMTaskID];
		pTask->Reset();
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSubTask(ETask etask, CryMatchMakingTaskID mmTaskID)
{
	CCryMatchMaking::StartSubTask(etask, mmTaskID);

	STask* pTask = &m_task[mmTaskID];
	pTask->Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
		if (CheckTaskStillValid(mmTaskID))
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
			case eT_SessionGetUsers:
			case eT_SessionQuery:
			case eT_SessionStart:
			case eT_SessionEnd:
				StopTaskRunning(mmTaskID);
				break;

			case eT_SessionCreate:
				StartSessionCreate(mmTaskID);
				break;

			case eT_SessionMigrate:
				StartSessionMigrate(mmTaskID);
				break;

			case eT_SessionUpdate:
				StartSessionUpdate(mmTaskID);
				break;

			case eT_SessionUpdateSlots:
				StartSessionUpdateSlots(mmTaskID);
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

			//#if NETWORK_HOST_MIGRATION
			//			case eT_SessionMigrateHostStart:
			//				HostMigrationStartNT(mmTaskID);
			//				break;
			//
			//			case eT_SessionMigrateHostServer:
			//				HostMigrationServerNT(mmTaskID);
			//				break;
			//
			//			case eT_SessionMigrateHostClient:
			//				TickHostMigrationClientNT(mmTaskID);
			//				break;
			//#endif
			//
			case eT_SessionSetLocalUserData:
				StartSessionSetLocalUserData(mmTaskID);
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::EndTask(CryMatchMakingTaskID mmTaskID)
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
				((CryMatchmakingCallback)pTask->cb)(pTask->lTaskID, pTask->error, pTask->cbArg);
				break;

			case eT_SessionUpdate:
			case eT_SessionUpdateSlots:
			case eT_SessionStart:
			case eT_SessionEnd:
			case eT_SessionDelete:
			case eT_SessionSetLocalUserData:
	#if NETWORK_HOST_MIGRATION
				//case eT_SessionMigrateHostStart:
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
				((CryMatchmakingSessionCreateCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, m_sessions[pTask->session].localConnection.uid), pTask->cbArg);

				if (pTask->error == eCLE_Success)
				{
					InitialUserDataEvent(pTask->session);
				}
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
			NetLog("[MatchMaking] EndTask %d (%d) error %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		// Clear Steam specific task state so that base class tasks can use this slot
		pTask->Reset();

		FreeTask(mmTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StopTaskRunning(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->used)
	{
		pTask->running = false;
		TO_GAME_FROM_LOBBY(&CCrySteamMatchMaking::EndTask, this, mmTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CCrySteamMatchMaking::CheckTaskStillValid(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->session == CryLobbyInvalidSessionHandle)
	{
		// If the task isn't using a session continue task.
		return true;
	}

	if ((pTask->session < MAX_MATCHMAKING_SESSIONS) && (m_sessions[pTask->session].localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		// If the task is using a session and the session is still in use continue task.
		return true;
	}
	else
	{
		// If task is using a session that has since been deleted terminate task.
		UpdateTaskError(mmTaskID, eCLE_InvalidSession);
		StopTaskRunning(mmTaskID);

		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

CryMatchMakingConnectionID CCrySteamMatchMaking::AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, CSteamID steamID, uint32 numUsers, const char* name, uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES], bool isDedicated)
{
	SSession* pSession = &m_sessions[h];
	const bool bNewPlayerIsHost = (pSession->localConnection.userID.m_steamID == steamID);

	if (connectionID == CryLobbyInvalidConnectionID)
	{
		TNetAddress addr(LobbyIdAddr(steamID.ConvertToUint64()));

		connectionID = m_lobby->FindConnection(addr);
		if (connectionID == CryLobbyInvalidConnectionID)
		{
			connectionID = m_lobby->CreateConnection(addr);
		}
	}

	#if USE_CRY_VOICE && USE_STEAM_VOICE
	// Register the connection to CryVoice
	SCrySteamUserID* pUser = new SCrySteamUserID(steamID);
	CryUserID userId(pUser);
	CCrySteamVoice* voice = (CCrySteamVoice*)m_pService->GetVoice();
	NetLog("[Steam] Registering user to voice from AddRemoteConnection");
	voice->RegisterRemoteUser(connectionID, userId);
	#endif // USE_CRY_VOICE

	CryMatchMakingConnectionID id = CCryMatchMaking::AddRemoteConnection(h, connectionID, uid, numUsers);
	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

		pConnection->userID = SCrySteamUserID(steamID);
		cry_strcpy(pConnection->name, name);
		memcpy(pConnection->userData, userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
		pConnection->m_isDedicated = isDedicated;
	#if NETWORK_HOST_MIGRATION
		pConnection->m_migrated = (pSession->hostMigrationInfo.m_state != eHMS_Idle);
	#endif // NETWORK_HOST_MIGRATION

		if (bNewPlayerIsHost)
		{
			pSession->hostConnectionID = id;
		}
	}

	NetLog("[STEAM]: CCrySteamMatchMaking::AddRemoteConnection(), session %d, connection %d", h, id);
	return id;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	if (id != CryMatchMakingInvalidConnectionID)
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pConnection = &pSession->remoteConnection[id];
		ISteamNetworking* pSteamNetworking = SteamNetworking();

		if (pSteamNetworking && pConnection->used)
		{
			pSteamNetworking->CloseP2PSessionWithUser(pConnection->userID.m_steamID);
			CCryMatchMaking::FreeRemoteConnection(h, id);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionCreate(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	SCrySessionData* pData = (SCrySessionData*)m_lobby->MemGetPtr(pTask->params[SESSION_CREATE_PARAM_SESSION_DATA]);
	pData->m_data = (SCrySessionUserData*)m_lobby->MemGetPtr(pTask->params[SESSION_CREATE_PARAM_USER_DATA]);

	if (m_SteamOnLobbyCreated.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_SteamOnLobbyCreated.m_callResult.Cancel();
		StopTaskRunning(m_SteamOnLobbyCreated.m_taskID);
		NetLog("[MatchMaking]: StartSessionCreate() - cancelled previous session create");
	}

	ELobbyType lobbyType = k_ELobbyTypePrivate;
	if (pSession->createFlags & CRYSESSION_CREATE_FLAG_SEARCHABLE)
	{
		lobbyType = k_ELobbyTypePublic;
	}

	ISteamMatchmaking* matchMaking = SteamMatchmaking();
	SteamAPICall_t hSteamAPICall = k_uAPICallInvalid;
	if (matchMaking)
	{
		hSteamAPICall = matchMaking->CreateLobby(lobbyType, pSession->data.m_numPublicSlots + pSession->data.m_numPrivateSlots);
	}
	if (hSteamAPICall == k_uAPICallInvalid)
	{
		UpdateTaskError(mmTaskID, eCLE_InternalError);
		StopTaskRunning(mmTaskID);
	}
	else
	{
		m_SteamOnLobbyCreated.m_callResult.Set(hSteamAPICall, this, &CCrySteamMatchMaking::OnLobbyCreated);
		m_SteamOnLobbyCreated.m_taskID = mmTaskID;
		NetLog("[MatchMaking]: StartSessionCreate()");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionMigrate(CryMatchMakingTaskID mmTaskID)
{
	NetLog("[TODO] CCrySteamMatchMaking::StartSessionMigrate()");
	NetLog("[MatchMaking]: StartSessionMigrate()");
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::TickSessionMigrate(CryMatchMakingTaskID mmTaskID)
{
	NetLog("[TODO] CCrySteamMatchMaking::TickSessionMigrate()");
	NetLog("[MatchMaking]: TickSessionMigrate()");
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SCrySessionSearchParam* pSessionSearchParam = (SCrySessionSearchParam*)m_lobby->MemGetPtr(pTask->params[SESSION_SEARCH_PARAM_GAME_PARAM]);
	pSessionSearchParam->m_data = (SCrySessionSearchData*)m_lobby->MemGetPtr(pTask->params[SESSION_SEARCH_PARAM_GAME_USER_DATA]);

	if (m_SteamOnLobbySearchResults.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_SteamOnLobbySearchResults.m_callResult.Cancel();
		StopTaskRunning(m_SteamOnLobbySearchResults.m_taskID);
		NetLog("[MatchMaking]: StartSessionSearch() - cancelled previous session search");
	}

	// Set filters here using:
	// AddRequestLobbyStringFilter()
	// AddRequestLobbyNumericalFilter()
	// AddRequestNearValueFilter()
	// AddRequestLobbyListFilterSlotsAvailable())

	
	SteamAPICall_t hSteamAPICall = k_uAPICallInvalid;
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (pSteamMatchmaking)
	{
		hSteamAPICall = pSteamMatchmaking->RequestLobbyList();
	}
	if (hSteamAPICall == k_uAPICallInvalid)
	{
		UpdateTaskError(mmTaskID, eCLE_InternalError);
		StopTaskRunning(mmTaskID);
	}
	else
	{
		pTask->StartTimer();
		m_SteamOnLobbySearchResults.m_callResult.Set(hSteamAPICall, this, &CCrySteamMatchMaking::OnLobbySearchResults);
		m_SteamOnLobbySearchResults.m_taskID = mmTaskID;
		NetLog("[MatchMaking]: StartSessionSearch()");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::TickSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	int64 elapsed = pTask->GetTimer();
	if (elapsed >= SESSION_SEARCH_TIMEOUT)
	{
		NetLog("[STEAM]: session search has been running for %" PRId64 "ms - stopping", elapsed);
		((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
		UpdateTaskError(mmTaskID, eCLE_TimeOut);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::EndSessionSearch(CryMatchMakingTaskID mmTaskID)
{
	m_SteamOnLobbyDataUpdated.m_taskID = CryMatchMakingInvalidTaskID;
	m_SteamOnLobbySearchResults.m_taskID = CryMatchMakingInvalidTaskID;

	SendSessionSearchResultsToGame(mmTaskID, TMemInvalidHdl, TMemInvalidHdl);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionUpdate(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SCrySessionUserData* pData = (SCrySessionUserData*)m_lobby->MemGetPtr(pTask->params[SESSION_UPDATE_PARAM_USER_DATA]);

	SetSessionUserData(mmTaskID, pData, pTask->numParams[SESSION_UPDATE_PARAM_NUM_USER_DATA]);
	NetLog("[MatchMaking]: StartSessionUpdate()");
	StopTaskRunning(mmTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionUpdateSlots(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();

	if (pSteamMatchmaking && (pSession->localFlags & (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST)) == (CRYSESSION_LOCAL_FLAG_USED | CRYSESSION_LOCAL_FLAG_HOST))
	{
		CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> value;

		value.Format("%x", pTask->numParams[SESSION_UPDATE_SLOTS_PARAM_NUM_PUBLIC]);
		pSteamMatchmaking->SetLobbyData(pSession->m_id.m_steamID, STEAM_KEY_SESSION_PUBLIC_SLOTS, value.c_str());
		value.Format("%x", pTask->numParams[SESSION_UPDATE_SLOTS_PARAM_NUM_PRIVATE]);
		pSteamMatchmaking->SetLobbyData(pSession->m_id.m_steamID, STEAM_KEY_SESSION_PRIVATE_SLOTS, value.c_str());

		NetLog("[MatchMaking]: StartSessionUpdateSlots()");
	}

	StopTaskRunning(mmTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::EndSessionQuery(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[pTask->session];
		SCrySessionSearchResult result;
		SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];
		SCrySteamSessionID* pID = new SCrySteamSessionID(pSession->m_id.m_steamID, false);

		result.m_id = pID;
		result.m_numFilledSlots = pSession->localConnection.numUsers;

		for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
		{
			if (pSession->remoteConnection[i].used)
			{
				result.m_numFilledSlots += pSession->remoteConnection[i].numUsers;
			}
		}

		result.m_numFriends = 0;
		result.m_flags = 0;

		result.m_data = pSession->data;
		result.m_data.m_data = userData;
		for (int i = 0; i < MAX_MATCHMAKING_SESSION_USER_DATA; i++)
		{
			userData[i] = pSession->data.m_data[i];
		}

		((CryMatchmakingSessionQueryCallback)pTask->cb)(pTask->lTaskID, pTask->error, &result, pTask->cbArg);
	}
	else
	{
		((CryMatchmakingSessionQueryCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::EndSessionGetUsers(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];

	if (pTask->error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[pTask->session];
		SCryUserInfoResult result;

		SCrySteamUserID* pID = new SCrySteamUserID();
		if (pID != NULL)
		{
			pID->m_steamID = pSession->localConnection.userID.m_steamID;

			result.m_conID = pSession->localConnection.uid;
			result.m_userID = pID;
			cry_strcpy(result.m_userName, pSession->localConnection.name);
			memcpy(result.m_userData, pSession->localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
			result.m_isDedicated = false;

			((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &result, pTask->cbArg);
		}

		for (uint32 index = 0; index < MAX_LOBBY_CONNECTIONS; ++index)
		{
			if (pSession->remoteConnection[index].used)
			{
				pID = new SCrySteamUserID();
				if (pID != NULL)
				{
					pID->m_steamID = pSession->remoteConnection[index].userID.m_steamID;

					result.m_conID = pSession->remoteConnection[index].uid;
					result.m_userID = pID;
					cry_strcpy(result.m_userName, pSession->remoteConnection[index].name);
					memcpy(result.m_userData, pSession->remoteConnection[index].userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
					result.m_isDedicated = pSession->remoteConnection[index].m_isDedicated;

					((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, &result, pTask->cbArg);
				}
			}
		}
	}

	((CryMatchmakingSessionGetUsersCallback)pTask->cb)(pTask->lTaskID, pTask->error, NULL, pTask->cbArg);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	if (m_SteamOnLobbyEntered.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_SteamOnLobbyEntered.m_callResult.Cancel();
		StopTaskRunning(m_SteamOnLobbyEntered.m_taskID);
		NetLog("[MatchMaking]: StartSessionJoin() - cancelled previous session join");
	}

	SteamAPICall_t hSteamAPICall = k_uAPICallInvalid;
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (pSteamMatchmaking)
	{
		hSteamAPICall = pSteamMatchmaking->JoinLobby(pSession->m_id.m_steamID);
	}
	if (hSteamAPICall == k_uAPICallInvalid)
	{
		UpdateTaskError(mmTaskID, eCLE_InternalError);
		StopTaskRunning(mmTaskID);
	}
	else
	{
		pTask->StartTimer();
		m_SteamOnLobbyEntered.m_callResult.Set(hSteamAPICall, this, &CCrySteamMatchMaking::OnLobbyEntered);
		m_SteamOnLobbyEntered.m_taskID = mmTaskID;
		NetLog("[MatchMaking]: StartSessionJoin()");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::TickSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	int64 elapsed = pTask->GetTimer();
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (pSteamMatchmaking && elapsed >= SESSION_JOIN_TIMEOUT)
	{
		NetLog("[STEAM]: session join has been running for %" PRId64 "ms - stopping", elapsed);
		UpdateTaskError(mmTaskID, eCLE_TimeOut);

		// Make sure we leave the Steam lobby
		pSteamMatchmaking->LeaveLobby(pSession->m_id.m_steamID);
		NetLog("[STEAM]: Leave Steam lobby [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());
		FreeSessionHandle(pTask->session);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::EndSessionJoin(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	m_SteamOnLobbyEntered.m_taskID = CryMatchMakingInvalidTaskID;

	if (pTask->error == eCLE_Success)
	{
		SSession* pSession = &m_sessions[pTask->session];

		((CryMatchmakingSessionJoinCallback)pTask->cb)(pTask->lTaskID, pTask->error, CreateGameSessionHandle(pTask->session, pSession->localConnection.uid), 0, 0, pTask->cbArg);
		InitialUserDataEvent(pTask->session);

		return;
	}

	((CryMatchmakingSessionJoinCallback)pTask->cb)(pTask->lTaskID, pTask->error, CrySessionInvalidHandle, 0, 0, pTask->cbArg);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionDelete(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	SSession* pSession = &m_sessions[pTask->session];

	#if NETWORK_HOST_MIGRATION
	// Since we're deleting this session, terminate any host migration
	if (pSession->hostMigrationInfo.m_state != eHMS_Idle)
	{
		m_hostMigration.Terminate(&pSession->hostMigrationInfo);
	}
	#endif
	pSession->createFlags &= ~CRYSESSION_CREATE_FLAG_MIGRATABLE;

	// Disconnect our local connection
	SessionDisconnectRemoteConnectionViaNub(pTask->session, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, eDC_UserRequested, "Session deleted");
	SessionUserDataEvent(eCLSE_SessionUserLeave, pTask->session, CryMatchMakingInvalidConnectionID);
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (pSteamMatchmaking)
	{
		pSteamMatchmaking->LeaveLobby(pSession->m_id.m_steamID);
	}
	NetLog("[STEAM]: Leave Steam lobby [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; ++i)
	{
		SSession::SRConnection* pConnection = &pSession->remoteConnection[i];
		if (pConnection->used)
		{
			FreeRemoteConnection(pTask->session, i);
		}
	}
	FreeSessionHandle(pTask->session);

	NetLog("[MatchMaking]: StartSessionDelete()");
	StopTaskRunning(mmTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID)
{
	STask* pTask = &m_task[mmTaskID];
	const uint32 MaxBufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT16Size + CRYLOBBY_USER_DATA_SIZE_IN_BYTES;
	CCrySharedLobbyPacket packet;

	SessionUserDataEvent(eCLSE_SessionUserUpdate, pTask->session, CryMatchMakingInvalidConnectionID);

	if (packet.CreateWriteBuffer(MaxBufferSize))
	{
		SSession* pSession = &m_sessions[pTask->session];
		SSession::SLConnection* pLConnection = &pSession->localConnection;

		packet.StartWrite(eSteamPT_UserData, true);
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

	NetLog("[MatchMaking]: StartSessionSetLocalUserData()");
	StopTaskRunning(mmTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::InitialUserDataEvent(CryLobbySessionHandle h)
{
	SSession* pSession = &m_sessions[h];
	pSession->localFlags |= CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED;

	if (pSession->localConnection.used)
	{
		SessionUserDataEvent(eCLSE_SessionUserJoin, h, CryMatchMakingInvalidConnectionID);
	}

	for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
	{
		if (pSession->remoteConnection[i].used)
		{
			SessionUserDataEvent(eCLSE_SessionUserJoin, h, i);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id)
{
	SSession* pSession = &m_sessions[h];

	if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USER_DATA_EVENTS_STARTED)
	{
		SCryUserInfoResult userInfo;
		SCrySteamUserID* pUserID = new SCrySteamUserID();
		userInfo.m_userID = pUserID;

		if (id == CryMatchMakingInvalidConnectionID)
		{
			SSession::SLConnection* pConnection = &pSession->localConnection;

			userInfo.m_conID = pConnection->uid;
			userInfo.m_isDedicated = false;
			cry_strcpy(userInfo.m_userName, pConnection->name);
			memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
			pUserID->m_steamID = pConnection->userID.m_steamID;

			NetLog("[STEAM]: CCrySteamMatchMaking::SessionUserDataEvent() sending event %d on session %d for local user [%s][%s]", event, h, userInfo.m_userName, pConnection->userID.GetGUIDAsString().c_str());
			CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
		}
		else
		{
			SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

			userInfo.m_conID = pConnection->uid;
			userInfo.m_isDedicated = pConnection->m_isDedicated;
			cry_strcpy(userInfo.m_userName, pConnection->name);
			memcpy(userInfo.m_userData, pConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
			pUserID->m_steamID = pConnection->userID.m_steamID;

			NetLog("[STEAM]: CCrySteamMatchMaking::SessionUserDataEvent() sending event %d on session %d for remote user [%s][%s]", event, h, userInfo.m_userName, pConnection->userID.GetGUIDAsString().c_str());
			CCryMatchMaking::SessionUserDataEvent(event, h, &userInfo);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Steam Lobby Packet handlers
//
////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::ProcessSessionRequestJoinResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	if (m_SteamOnLobbyEntered.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnLobbyEntered.m_taskID];
		SSession* pSession = &m_sessions[pTask->session];

		uint8 numUsers;
		bool isDedicated;
		SCryMatchMakingConnectionUID hostConnectionUID;
		SCrySteamUserID steamID;
		char name[CRYLOBBY_USER_NAME_LENGTH];
		uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];

		pPacket->StartRead();
		pSession->InitialiseLocalConnection(pPacket->ReadConnectionUID());
		uint32 sessionCreateGameFlags = pPacket->ReadUINT16();
		pSession->createFlags = (pSession->createFlags & CRYSESSION_CREATE_FLAG_SYSTEM_MASK) | (sessionCreateGameFlags << CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT);

		bool hostMigrationSupported = m_lobby->GetLobbyServiceFlag(m_serviceType, eCLSF_SupportHostMigration);
		if ((pSession->createFlags & CRYSESSION_CREATE_FLAG_MIGRATABLE) && hostMigrationSupported)
		{
			pSession->localFlags |= CRYSESSION_LOCAL_FLAG_CAN_SEND_HOST_HINTS;
		}

		NetLog("[Lobby] Created local connection " PRFORMAT_SH " " PRFORMAT_UID, PRARG_SH(pTask->session), PRARG_UID(pSession->localConnection.uid));
		if ((!(pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)) && (pSession->localConnection.uid.m_uid > m_connectionUIDCounter))
		{
			// Keep all remote clients in sync with the host (in case they're chosen to be host during host migration)
			m_connectionUIDCounter = pSession->localConnection.uid.m_uid;
		}

		numUsers = pPacket->ReadUINT8();
		isDedicated = pPacket->ReadBool();
		hostConnectionUID = pPacket->ReadConnectionUID();
		steamID.ReadFromPacket(pPacket);
		pPacket->ReadString(name, CRYLOBBY_USER_NAME_LENGTH);
		pPacket->ReadData(userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

		CryMatchMakingConnectionID id = AddRemoteConnection(pTask->session, CryLobbyInvalidConnectionID, hostConnectionUID, steamID.m_steamID, numUsers, name, userData, isDedicated);

		if (id != CryMatchMakingInvalidConnectionID)
		{
			NetLog("[MatchMaking] Created server connection " PRFORMAT_MMCINFO, PRARG_MMCINFO(id, pSession->remoteConnection[id].connectionID, hostConnectionUID));
			StopTaskRunning(m_SteamOnLobbyEntered.m_taskID);
			pSession->hostConnectionID = id;
			SessionUserDataEvent(eCLSE_SessionUserJoin, pTask->session, id);
		}
		else
		{
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_ConnectionFailed);
		}
	}
	else
	{
		NetLog("[STEAM]: could not find session join task when processing session join result");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::ProcessLocalUserData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket)
{
	CryLobbySessionHandle h;
	SCryMatchMakingConnectionUID uid;
	CryMatchMakingConnectionID id;

	pPacket->StartRead();
	uid = pPacket->GetFromConnectionUID();
	uid.m_uid = pPacket->ReadUINT16();

	NetLog("[STEAM]: CCrySteamMatchMaking::ProcessLocalUserData() received user data packet");
	if (FindConnectionFromUID(uid, &h, &id))
	{
		SSession* pSession = &m_sessions[h];
		SSession::SRConnection* pRConnection = &pSession->remoteConnection[id];

		pPacket->ReadData(pRConnection->userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
		SessionUserDataEvent(eCLSE_SessionUserUpdate, h, id);
		NetLog("[STEAM]: CCrySteamMatchMaking::ProcessLocalUserData() processed user data packet");

		if (pSession->localFlags & CRYSESSION_LOCAL_FLAG_HOST)
		{
			NetLog("[STEAM]: CCrySteamMatchMaking::ProcessLocalUserData() host sent user data packet on to all");
			SendToAll(CryMatchMakingInvalidTaskID, pPacket, h, id);
		}
	}
	else
	{
		NetLog("[STEAM]: CCrySteamMatchMaking::ProcessLocalUserData() unable to find connection");
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Steam callbacks
//
////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnSteamShutdown(SteamShutdown_t* pParam)
{
	static bool exiting = false;
	if (!exiting)
	{
		NetLog("[STEAM]: Steam shutdown request; exiting");
		exiting = true;
		IConsole* pConsole = gEnv->pConsole;
		if (pConsole != NULL)
		{
			pConsole->ExecuteString("quit", true, true);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnLobbyDataUpdated(LobbyDataUpdate_t* pParam)
{

	SCrySteamSessionID lobbyID(CSteamID(pParam->m_ulSteamIDLobby), false);
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();

	// Find the session
	SSession* pSession = NULL;
	uint32 session;
	for (session = 0; session < MAX_MATCHMAKING_SESSIONS; ++session)
	{
		if (m_sessions[session].m_id.m_steamID == lobbyID.m_steamID)
		{
			pSession = &m_sessions[session];
			break;
		}
	}

	#if USE_CRY_VOICE && USE_STEAM_VOICE
	if (pSession && pSteamMatchmaking)
	{
		int numMembers = pSteamMatchmaking->GetNumLobbyMembers(pParam->m_ulSteamIDMember);
		for (int i = 0; i < numMembers; i++)
		{
			CSteamID user = pSteamMatchmaking->GetLobbyMemberByIndex(pParam->m_ulSteamIDMember, i);

			SCrySteamUserID* pUser = new SCrySteamUserID(user);
			CryUserID userId(pUser);
			CCrySteamVoice* voice = (CCrySteamVoice*)m_pService->GetVoice();
			ISteamUser* pSteamUser = SteamUser();
			if (pSteamUser)
			{
				for (int i = 0; i < pSession->remoteConnection.Size(); i++)
				{
					// check if connectionId is valid and user is a remote user
					if (pSession->remoteConnection[i].connectionID != CryLobbyInvalidConnectionID && user != pSteamUser->GetSteamID())
					{
						NetLog("[Steam] Registering user to voice from OnLobbyDataUpdated");
						voice->RegisterRemoteUser(pSession->remoteConnection[i].connectionID, userId);
					}
				}
			}

		}
	}
	#endif // USE_CRY_VOICE

	if (pSteamMatchmaking && m_SteamOnLobbyDataUpdated.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnLobbyDataUpdated.m_taskID];
		SCrySessionSearchParam* pSessionSearchParam = (SCrySessionSearchParam*)m_lobby->MemGetPtr(pTask->params[SESSION_SEARCH_PARAM_GAME_PARAM]);

		if ((pParam->m_bSuccess) && (pSessionSearchParam->m_maxNumReturn > 0))
		{
			CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> key;
			CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> value;
			uint32 count = pSteamMatchmaking->GetLobbyDataCount(pParam->m_ulSteamIDLobby);
			uint32 start = 0;

			// Extract the user data from the lobby
			TMemHdl resultsHdl = m_lobby->MemAlloc(sizeof(SCrySessionSearchResult));
			SCrySessionSearchResult* pResult = (SCrySessionSearchResult*)m_lobby->MemGetPtr(resultsHdl);
			if (pResult != NULL)
			{
				// Placement new the struct to correctly initialise m_id, which is a smart pointer
				new(pResult) SCrySessionSearchResult;

				SCrySteamSessionID* pID = new SCrySteamSessionID();
				if (pID != NULL)
				{
					pID->m_steamID.SetFromUint64(pParam->m_ulSteamIDLobby);

					pResult->m_id = pID;
					pResult->m_numFilledSlots = 0;
					pResult->m_numFriends = 0;
					pResult->m_ping = 0;
					pResult->m_flags = 0;

					key = STEAM_KEY_SESSION_NAME;
					value = pSteamMatchmaking->GetLobbyData(pParam->m_ulSteamIDLobby, key.c_str());
					cry_strcpy(pResult->m_data.m_name, value.c_str());
					++start;

					key = STEAM_KEY_SESSION_PUBLIC_SLOTS;
					value = pSteamMatchmaking->GetLobbyData(pParam->m_ulSteamIDLobby, key.c_str());
					sscanf(value.c_str(), "%x", &pResult->m_data.m_numPublicSlots);
					++start;

					key = STEAM_KEY_SESSION_PRIVATE_SLOTS;
					value = pSteamMatchmaking->GetLobbyData(pParam->m_ulSteamIDLobby, key.c_str());
					sscanf(value.c_str(), "%x", &pResult->m_data.m_numPrivateSlots);
					++start;

					key = STEAM_KEY_SESSION_GAME_TYPE;
					value = pSteamMatchmaking->GetLobbyData(pParam->m_ulSteamIDLobby, key.c_str());
					unsigned int ranked;
					sscanf(value.c_str(), "%x", &ranked);
					pResult->m_data.m_ranked = ranked != 0;
					++start;

					pResult->m_data.m_numData = count < start ? 0 : count - start;

					TMemHdl userHdl = 0;
					SCrySessionUserData* pUserData = NULL;

					if (pResult->m_data.m_numData)
					{
						userHdl = m_lobby->MemAlloc(sizeof(SCrySessionUserData) * (pResult->m_data.m_numData));
						pUserData = (SCrySessionUserData*)m_lobby->MemGetPtr(userHdl);
					}

					if (pUserData != NULL)
					{
						pResult->m_data.m_data = pUserData;

						for (uint32 index = 0; index < pResult->m_data.m_numData; ++index)
						{
							// Placement new the struct to correctly initialise m_id
							new(&pUserData[index])SCrySessionUserData;

							key.Format(STEAM_KEY_SESSION_USER_DATA "%d", index);
							value = pSteamMatchmaking->GetLobbyData(pParam->m_ulSteamIDLobby, key.c_str());

							sscanf(value.c_str(), "%x", &(pUserData[index].m_type));
							unsigned int id = 0;
							unsigned int val;
							switch (pUserData[index].m_type)
							{
							case eCLUDT_Int64:
								sscanf(value.c_str(), "%x:%x:%" PRIx64, &(pUserData[index].m_type), &id, &(pUserData[index].m_int64));
								break;
							case eCLUDT_Int32:
								sscanf(value.c_str(), "%x:%x:%x", &(pUserData[index].m_type), &id, &(pUserData[index].m_int32));
								break;
							case eCLUDT_Int16:
								sscanf(value.c_str(), "%x:%x:%x", &(pUserData[index].m_type), &id, &val);
								pUserData[index].m_int16 = val;
								break;
							case eCLUDT_Int8:
								sscanf(value.c_str(), "%x:%x:%x", &(pUserData[index].m_type), &id, &val);
								pUserData[index].m_int8 = val;
								break;
							case eCLUDT_Float64:
								sscanf(value.c_str(), "%x:%x:%lf", &(pUserData[index].m_type), &id, &(pUserData[index].m_f64));
								break;
							case eCLUDT_Float32:
								sscanf(value.c_str(), "%x:%x:%f", &(pUserData[index].m_type), &id, &(pUserData[index].m_f32));
								break;
							}
							pUserData[index].m_id = id;
						}

						TO_GAME_FROM_LOBBY(&CCrySteamMatchMaking::SendSessionSearchResultsToGame, this, m_SteamOnLobbyDataUpdated.m_taskID, resultsHdl, userHdl);
						--pSessionSearchParam->m_maxNumReturn;

						if (pSessionSearchParam->m_maxNumReturn == 0)
						{
							StopTaskRunning(m_SteamOnLobbyDataUpdated.m_taskID);
							m_SteamOnLobbyDataUpdated.m_taskID = CryMatchMakingInvalidTaskID;
						}
					}
					else
					{
						m_lobby->MemFree(resultsHdl);
						UpdateTaskError(m_SteamOnLobbyDataUpdated.m_taskID, eCLE_OutOfMemory);
					}
				}
				else
				{
					m_lobby->MemFree(resultsHdl);
					UpdateTaskError(m_SteamOnLobbyDataUpdated.m_taskID, eCLE_OutOfMemory);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnP2PSessionRequest(P2PSessionRequest_t* pParam)
{
	// Inform Steam that we're happy to accept traffic from this user
	// Note: might have to add tests here later, but for now just accept anyone
	
	ISteamNetworking* pSteamNetworking = SteamNetworking();
	if (pSteamNetworking)
	{
		NetLog("[STEAM]: Accepting P2P connection request for Steam ID [%s]", SCrySteamUserID(pParam->m_steamIDRemote).GetGUIDAsString().c_str());
		pSteamNetworking->AcceptP2PSessionWithUser(pParam->m_steamIDRemote);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam networking service not avaialble");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnLobbyChatUpdate(LobbyChatUpdate_t* pParam)
{
	SCrySteamSessionID lobbyID(CSteamID(pParam->m_ulSteamIDLobby), false);
	SCrySteamUserID userChanged(CSteamID(pParam->m_ulSteamIDUserChanged));
	SCrySteamUserID userMakingChange(CSteamID(pParam->m_ulSteamIDMakingChange));

	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
		return;
	}

	// Find the session
	SSession* pSession = NULL;
	uint32 session;
	for (session = 0; session < MAX_MATCHMAKING_SESSIONS; ++session)
	{
		if (m_sessions[session].m_id.m_steamID == lobbyID.m_steamID)
		{
			pSession = &m_sessions[session];
			break;
		}
	}

	if (pSession != NULL)
	{
		if (BChatMemberStateChangeRemoved(pParam->m_rgfChatMemberStateChange))
		{
			// Defaults for k_EChatMemberStateChangeLeft and k_EChatMemberStateChangeDisconnected
			ECryLobbySystemEvent clse = eCLSE_SessionUserLeave;
			EDisconnectionCause dc = eDC_UserRequested;
			const char* description = "User requested";

			if (pParam->m_rgfChatMemberStateChange & k_EChatMemberStateChangeKicked)
			{
				clse = eCLSE_KickedFromSession;
				dc = eDC_Kicked;
				description = "User kicked";
			}

			if (pParam->m_rgfChatMemberStateChange & k_EChatMemberStateChangeBanned)
			{
				clse = eCLSE_KickedFromSession;
				dc = eDC_Banned;
				description = "User banned";
			}

			NetLog("[STEAM]: user [%s][%s] removed : [%s]", pSteamFriends->GetFriendPersonaName(userChanged.m_steamID), userChanged.GetGUIDAsString().c_str(), description);

			// Search for the player in the session
			if (pSession->localConnection.userID.m_steamID == userChanged.m_steamID)
			{
				SessionUserDataEvent(clse, session, CryMatchMakingInvalidConnectionID);

				// End session
				SessionDisconnectRemoteConnectionViaNub(session, CryMatchMakingInvalidConnectionID, eDS_Local, CryMatchMakingInvalidConnectionID, dc, description);
				ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
				if (pSteamMatchmaking)
				{
					pSteamMatchmaking->LeaveLobby(pSession->m_id.m_steamID);
				}
				NetLog("[STEAM]: Leave Steam lobby [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());

				for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; ++i)
				{
					SSession::SRConnection* pConnection = &pSession->remoteConnection[i];
					if (pConnection->used)
					{
						FreeRemoteConnection(session, i);
					}
				}
				FreeSessionHandle(session);
			}
			else
			{
				for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; ++i)
				{
					if (pSession->remoteConnection[i].userID.m_steamID == userChanged.m_steamID)
					{
						SessionUserDataEvent(clse, session, i);
						FreeRemoteConnection(session, i);
						break;
					}
				}
			}
		}
		else
		{
			if (pSession->localConnection.userID == userChanged)
			{
				SessionUserDataEvent(eCLSE_SessionUserJoin, session, CryMatchMakingInvalidConnectionID);
				NetLog("[STEAM]: local user [%s][%s] added", pSteamFriends->GetFriendPersonaName(userChanged.m_steamID), userChanged.GetGUIDAsString().c_str());
			}
			else
			{
				// Add the player to the session (with no user data - that comes later from lobby packets)
				uint8 data[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
				memset(data, 0, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);
				CryMatchMakingConnectionID id = AddRemoteConnection(session, CryLobbyInvalidConnectionID, CreateConnectionUID(session), userChanged.m_steamID, 1, pSteamFriends->GetFriendPersonaName(userChanged.m_steamID), data, false);
				if (id != CryMatchMakingInvalidConnectionID)
				{
					// Send a response to the joining player
					CCrySharedLobbyPacket packet;
					if (packet.CreateWriteBuffer(MAX_LOBBY_PACKET_SIZE))
					{
						SSession::SRConnection* pConnection = &pSession->remoteConnection[id];

						packet.StartWrite(eSteamPT_SessionRequestJoinResult, true);
						packet.WriteConnectionUID(pConnection->uid);
						packet.WriteUINT16((pSession->createFlags & CRYSESSION_CREATE_FLAG_GAME_MASK) >> CRYSESSION_CREATE_FLAG_GAME_FLAGS_SHIFT);
						packet.WriteUINT8(pSession->localConnection.numUsers);
						if (gEnv->IsDedicated())                          // Server should flag if its dedicated or not
						{
							packet.WriteBool(true);
						}
						else
						{
							packet.WriteBool(false);
						}
						packet.WriteConnectionUID(pSession->localConnection.uid);
						pSession->localConnection.userID.WriteToPacket(&packet);
						packet.WriteString(pSession->localConnection.name, CRYLOBBY_USER_NAME_LENGTH);
						packet.WriteData(pSession->localConnection.userData, CRYLOBBY_USER_DATA_SIZE_IN_BYTES);

						Send(CryMatchMakingInvalidTaskID, &packet, session, id);
						SessionUserDataEvent(eCLSE_SessionUserJoin, session, id);

						NetLog("[STEAM]: remote user [%s][%s] added", pSteamFriends->GetFriendPersonaName(userChanged.m_steamID), userChanged.GetGUIDAsString().c_str());
					}
				}
				else
				{
					NetLog("[STEAM]: remote user [%s][%s] UNABLE TO ADD", pSteamFriends->GetFriendPersonaName(userChanged.m_steamID), userChanged.GetGUIDAsString().c_str());
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Steam callresults
//
////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnLobbyCreated(LobbyCreated_t* pParam, bool ioFailure)
{
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	if (m_SteamOnLobbyCreated.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnLobbyCreated.m_taskID];

		if (!ioFailure)
		{
			SSession* pSession = &m_sessions[pTask->session];
			SCrySessionData* pData = (SCrySessionData*)m_lobby->MemGetPtr(pTask->params[SESSION_CREATE_PARAM_SESSION_DATA]);

			UpdateTaskError(m_SteamOnLobbyCreated.m_taskID, ConvertSteamErrorToCryLobbyError(pParam->m_eResult));

			if (pTask->error == eCLE_Success)
			{
				CryFixedStringT<MAX_STEAM_KEY_VALUE_SIZE> value;

				// Set session details
				pSession->m_id.m_steamID = pParam->m_ulSteamIDLobby;
				pSession->InitialiseLocalConnection(CreateConnectionUID(pTask->session));
				cry_strcpy(pSession->data.m_name, pSession->localConnection.name);

				NetLog("[STEAM]: Steam lobby created by [%s], id [%s]", pSession->localConnection.name, CSteamIDAsString(pSession->m_id.m_steamID).c_str());

				// Set lobby keys
				pSteamMatchmaking->SetLobbyData(pParam->m_ulSteamIDLobby, STEAM_KEY_SESSION_NAME, pData->m_name);
				value.Format("%x", pData->m_numPublicSlots);
				pSteamMatchmaking->SetLobbyData(pParam->m_ulSteamIDLobby, STEAM_KEY_SESSION_PUBLIC_SLOTS, value.c_str());
				value.Format("%x", pData->m_numPrivateSlots);
				pSteamMatchmaking->SetLobbyData(pParam->m_ulSteamIDLobby, STEAM_KEY_SESSION_PRIVATE_SLOTS, value.c_str());
				value.Format("%x", pData->m_ranked);
				pSteamMatchmaking->SetLobbyData(pParam->m_ulSteamIDLobby, STEAM_KEY_SESSION_GAME_TYPE, value.c_str());

				SetSessionUserData(m_SteamOnLobbyCreated.m_taskID, pData->m_data, pData->m_numData);
				NetLog("[STEAM]: created lobby successfully");
			}
		}
		else
		{
			UpdateTaskError(m_SteamOnLobbyCreated.m_taskID, eCLE_InternalError);
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[STEAM]: failed to create lobby");
		}

		StopTaskRunning(m_SteamOnLobbyCreated.m_taskID);
		m_SteamOnLobbyCreated.m_taskID = CryMatchMakingInvalidTaskID;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnLobbySearchResults(LobbyMatchList_t* pParam, bool ioFailure)
{
	if (m_SteamOnLobbySearchResults.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnLobbySearchResults.m_taskID];
		SCrySessionSearchParam* pSessionSearchParam = (SCrySessionSearchParam*)m_lobby->MemGetPtr(pTask->params[SESSION_SEARCH_PARAM_GAME_PARAM]);
		ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();

		if (pSteamMatchmaking && pParam->m_nLobbiesMatching > 0)
		{
			if (pParam->m_nLobbiesMatching < pSessionSearchParam->m_maxNumReturn)
			{
				pSessionSearchParam->m_maxNumReturn = pParam->m_nLobbiesMatching;
			}

			for (uint32 index = 0; index < pSessionSearchParam->m_maxNumReturn; ++index)
			{
				CSteamID lobbyID = pSteamMatchmaking->GetLobbyByIndex(index);
				pSteamMatchmaking->RequestLobbyData(lobbyID);
			}

			m_SteamOnLobbyDataUpdated.m_taskID = m_SteamOnLobbySearchResults.m_taskID;
			m_SteamOnLobbySearchResults.m_taskID = CryMatchMakingInvalidTaskID;

			NetLog("[STEAM]: found %d lobbies - retrieving data", pParam->m_nLobbiesMatching);
		}
		else
		{
			NetLog("[STEAM]: no lobbies found");
			StopTaskRunning(m_SteamOnLobbySearchResults.m_taskID);
			m_SteamOnLobbySearchResults.m_taskID = CryMatchMakingInvalidTaskID;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::OnLobbyEntered(LobbyEnter_t* pParam, bool ioFailure)
{
	if (m_SteamOnLobbyEntered.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnLobbyEntered.m_taskID];
		SSession* pSession = &m_sessions[pTask->session];

		switch (pParam->m_EChatRoomEnterResponse)
		{
		case k_EChatRoomEnterResponseSuccess:             // Success
			break;
		case k_EChatRoomEnterResponseDoesntExist:         // Chat doesn't exist (probably closed)
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_InvalidSession);
			break;
		case k_EChatRoomEnterResponseLimited:             // Joining this chat is not allowed because you are a limited user (no value on account)
		case k_EChatRoomEnterResponseNotAllowed:          // General Denied - You don't have the permissions needed to join the chat
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_InsufficientPrivileges);
			break;
		case k_EChatRoomEnterResponseFull:                // Chat room has reached its maximum size
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_SessionFull);
			break;
		case k_EChatRoomEnterResponseError:               // Unexpected Error
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_InternalError);
			break;
		case k_EChatRoomEnterResponseCommunityBan:        // Attempt to join a chat when the user has a community lock on their account
		case k_EChatRoomEnterResponseBanned:              // You are banned from this chat room and may not join
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_Banned);
			break;
		case k_EChatRoomEnterResponseClanDisabled:        // Attempt to join a clan chat when the clan is locked or disabled
		case k_EChatRoomEnterResponseMemberBlockedYou:    // Join failed - some member in the chat has blocked you from joining
		case k_EChatRoomEnterResponseYouBlockedMember:    // Join failed - you have blocked some member already in the chat
			UpdateTaskError(m_SteamOnLobbyEntered.m_taskID, eCLE_SteamBlocked);
			break;
		}

		if (pTask->error == eCLE_Success)
		{
			NetLog("[STEAM]: joined Steam lobby [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());
		}
		else
		{
			NetLog("[STEAM]: could not join Steam lobby [%s]", CSteamIDAsString(pSession->m_id.m_steamID).c_str());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// End of Steam callbacks/callresults
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// Utility functions to send data back to the game on the game thread
//
////////////////////////////////////////////////////////////////////////////////

void CCrySteamMatchMaking::SendSessionSearchResultsToGame(CryMatchMakingTaskID mmTaskID, TMemHdl resultsHdl, TMemHdl userHdl)
{
	STask* pTask = &m_task[mmTaskID];

	if (resultsHdl != TMemInvalidHdl)
	{
		SCrySessionSearchResult* pResult = (SCrySessionSearchResult*)m_lobby->MemGetPtr(resultsHdl);
		pResult->m_data.m_data = (SCryLobbyUserData*)m_lobby->MemGetPtr(userHdl);

		((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, eCLE_SuccessContinue, pResult, pTask->cbArg);

		m_lobby->MemFree(userHdl);
		m_lobby->MemFree(resultsHdl);
	}
	else
	{
		// No associated memory means we can tell the game the search is finished
		((CryMatchmakingSessionSearchCallback)pTask->cb)(pTask->lTaskID, eCLE_Success, NULL, pTask->cbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// End of utility functions
//
////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamMatchMaking::ConvertSteamErrorToCryLobbyError(uint32 steamErrorCode)
{
	switch (steamErrorCode)
	{
	case k_EResultOK:                             // success
		return eCLE_Success;
	case k_EResultFail:                           // generic failure
	case k_EResultNoConnection:                   // no/failed network connection
	case k_EResultAccessDenied:                   // access is denied
	case k_EResultLimitExceeded:                  // Too much of a good thing
		return eCLE_InternalError;
	case k_EResultTimeout:                        // operation timed out
		return eCLE_TimeOut;
	case k_EResultInvalidPassword:                // password/ticket is invalid
	case k_EResultLoggedInElsewhere:              // same user logged in elsewhere
	case k_EResultInvalidProtocolVer:             // protocol version is incorrect
	case k_EResultInvalidParam:                   // a parameter is incorrect
	case k_EResultFileNotFound:                   // file was not found
	case k_EResultBusy:                           // called method busy - action not taken
	case k_EResultInvalidState:                   // called object was in an invalid state
	case k_EResultInvalidName:                    // name is invalid
	case k_EResultInvalidEmail:                   // email is invalid
	case k_EResultDuplicateName:                  // name is not unique
	case k_EResultBanned:                         // VAC2 banned
	case k_EResultAccountNotFound:                // account not found
	case k_EResultInvalidSteamID:                 // steamID is invalid
	case k_EResultServiceUnavailable:             // The requested service is currently unavailable
	case k_EResultNotLoggedOn:                    // The user is not logged on
	case k_EResultPending:                        // Request is pending (may be in process, or waiting on third party)
	case k_EResultEncryptionFailure:              // Encryption or Decryption failed
	case k_EResultInsufficientPrivilege:          // Insufficient privilege
	case k_EResultRevoked:                        // Access has been revoked (used for revoked guest passes)
	case k_EResultExpired:                        // License/Guest pass the user is trying to access is expired
	case k_EResultAlreadyRedeemed:                // Guest pass has already been redeemed by account, cannot be acked again
	case k_EResultDuplicateRequest:               // The request is a duplicate and the action has already occurred in the past, ignored this time
	case k_EResultAlreadyOwned:                   // All the games in this guest pass redemption request are already owned by the user
	case k_EResultIPNotFound:                     // IP address not found
	case k_EResultPersistFailed:                  // failed to write change to the data store
	case k_EResultLockingFailed:                  // failed to acquire access lock for this operation
	case k_EResultLogonSessionReplaced:
	case k_EResultConnectFailed:
	case k_EResultHandshakeFailed:
	case k_EResultIOFailure:
	case k_EResultRemoteDisconnect:
	case k_EResultShoppingCartNotFound:           // failed to find the shopping cart requested
	case k_EResultBlocked:                        // a user didn't allow it
	case k_EResultIgnored:                        // target is ignoring sender
	case k_EResultNoMatch:                        // nothing matching the request found
	case k_EResultAccountDisabled:
	case k_EResultServiceReadOnly:                // this service is not accepting content changes right now
	case k_EResultAccountNotFeatured:             // account doesn't have value, so this feature isn't available
	case k_EResultAdministratorOK:                // allowed to take this action, but only because requester is admin
	case k_EResultContentVersion:                 // A Version mismatch in content transmitted within the Steam protocol.
	case k_EResultTryAnotherCM:                   // The current CM can't service the user making a request, user should try another.
	case k_EResultPasswordRequiredToKickSession:  // You are already logged in elsewhere, this cached credential login has failed.
	case k_EResultAlreadyLoggedInElsewhere:       // You are already logged in elsewhere, you must wait
	case k_EResultSuspended:                      // Long running operation (content download) suspended/paused
	case k_EResultCancelled:                      // Operation canceled (typically by user: content download)
	case k_EResultDataCorruption:                 // Operation canceled because data is ill formed or unrecoverable
	case k_EResultDiskFull:                       // Operation canceled - not enough disk space.
	case k_EResultRemoteCallFailed:               // an remote call or IPC call failed
	case k_EResultPasswordUnset:                  // Password could not be verified as it's unset server side
	case k_EResultExternalAccountUnlinked:        // External account (PSN, Facebook...) is not linked to a Steam account
	case k_EResultPSNTicketInvalid:               // PSN ticket was invalid
	case k_EResultExternalAccountAlreadyLinked:   // External account (PSN, Facebook...) is already linked to some other account, must explicitly request to replace/delete the link first
	case k_EResultRemoteFileConflict:             // The sync cannot resume due to a conflict between the local and remote files
	case k_EResultIllegalPassword:                // The requested new password is not legal
	case k_EResultSameAsPreviousValue:            // new value is the same as the old one ( secret question and answer )
	case k_EResultAccountLogonDenied:             // account login denied due to 2nd factor authentication failure
	case k_EResultCannotUseOldPassword:           // The requested new password is not legal
	case k_EResultInvalidLoginAuthCode:           // account login denied due to auth code invalid
	case k_EResultAccountLogonDeniedNoMail:       // account login denied due to 2nd factor auth failure - and no mail has been sent
	case k_EResultHardwareNotCapableOfIPT:        //
	case k_EResultIPTInitError:                   //
	case k_EResultParentalControlRestricted:      // operation failed due to parental control restrictions for current user
	case k_EResultFacebookQueryError:             // Facebook query returned an error
	case k_EResultExpiredLoginAuthCode:           // account login denied due to auth code expired
	case k_EResultIPLoginRestrictionFailed:
	case k_EResultAccountLockedDown:
	case k_EResultAccountLogonDeniedVerifiedEmailRequired:
	case k_EResultNoMatchingURL:
	default:
	#if !defined(RELEASE)
		gEnv->pSystem->debug_LogCallStack();
	#endif // !defined(RELEASE)
		CryLog("[STEAM]: Unhandled error code %d", steamErrorCode);
		return eCLE_InternalError;
	}
}

////////////////////////////////////////////////////////////////////////////////

	#if !defined(_RELEASE)
void CCrySteamMatchMaking::Debug_DumpLobbyMembers(CryLobbySessionHandle lsh)
{
	SSession* pSession = &m_sessions[lsh];
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	ISteamFriends* pSteamFriends = SteamFriends();

	if ((lsh < MAX_MATCHMAKING_SESSIONS) && (pSession->localFlags & CRYSESSION_LOCAL_FLAG_USED))
	{
		NetLog("[STEAM]: *** Dumping session members ***");
		NetLog("[STEAM]: local player is [%s][%s]", pSession->localConnection.name, pSession->localConnection.userID.GetGUIDAsString().c_str());
		uint32 remotePlayerCount = 0;
		for (uint32 i = 0; i < MAX_LOBBY_CONNECTIONS; i++)
		{
			if (pSession->remoteConnection[i].used)
			{
				NetLog("[STEAM]: remote player [%d] is [%s][%s]", ++remotePlayerCount, pSession->remoteConnection[i].name, pSession->remoteConnection[i].userID.GetGUIDAsString().c_str());
			}
		}

		uint32 numLobbyMembers = (pSteamMatchmaking && pSteamFriends) ? pSteamMatchmaking->GetNumLobbyMembers(pSession->m_id.m_steamID) : 0;
		NetLog("[STEAM]: *** Dumping Steam lobby members (lobby id [%s], %d members) ***", CSteamIDAsString(pSession->m_id.m_steamID).c_str(), numLobbyMembers);
		if (numLobbyMembers > 0 && pSteamMatchmaking && pSteamFriends)
		{
			SCrySteamUserID id;
			if (pSteamMatchmaking->GetLobbyGameServer(pSession->m_id.m_steamID, NULL, NULL, &id.m_steamID))
			{
				NetLog("[STEAM]: lobby game server is [%s]", id.GetGUIDAsString().c_str());
			}
			else
			{
				NetLog("[STEAM]: lobby game server is [unknown]");
			}

			id = pSteamMatchmaking->GetLobbyOwner(pSession->m_id.m_steamID);
			NetLog("[STEAM]: lobby host is [%s][%s]", pSteamFriends->GetFriendPersonaName(id.m_steamID), id.GetGUIDAsString().c_str());

			for (uint32 index = 0; index < numLobbyMembers; ++index)
			{
				id = pSteamMatchmaking->GetLobbyMemberByIndex(pSession->m_id.m_steamID, index);
				NetLog("[STEAM]: lobby member [%d] is [%s][%s]", index, pSteamFriends->GetFriendPersonaName(id.m_steamID), id.GetGUIDAsString().c_str());
			}
		}
		NetLog("[STEAM]: *** Finished dumping session members ***");
	}
}
	#endif // !defined(_RELEASE)

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_MATCHMAKING
