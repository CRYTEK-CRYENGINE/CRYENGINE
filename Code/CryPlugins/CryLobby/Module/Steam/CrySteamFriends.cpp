// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamFriends.cpp
//  Created:     26/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryFriends
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Steam/CrySteamLobby.h" // CrySteamFriends.h included here

#if USE_STEAM && USE_CRY_FRIENDS

////////////////////////////////////////////////////////////////////////////////

	#define FRIENDS_LIST_PARAM_MAX_FRIENDS       (0)  // int
	#define FRIENDS_LIST_PARAM_NUM_FRIENDS       (2)  // int
	#define FRIENDS_LIST_PARAM_FRIENDS_REMAINING (1)  // int
	#define FRIENDS_LIST_PARAM_FRIENDS_LIST      (0)  // ptr

	#define FRIENDS_INVITE_PARAM_NUM_FRIENDS     (0)  // int
	#define FRIENDS_INVITE_PARAM_FRIENDS_LIST    (0)  // ptr

	#define GET_FRIENDS_LIST_TIMEOUT             (10000)

	#define STEAM_RICH_PRESENCE_STATUS_KEY       "status"

////////////////////////////////////////////////////////////////////////////////

CCrySteamFriends::CCrySteamFriends(CCryLobby* pLobby, CCryLobbyService* pService)
	: CCryFriends(pLobby, pService)
	//////////////////////////////////////////////////////////////////////////////
	// Steam callback registration
	, m_SteamOnFriendRichPresenceUpdate(this, &CCrySteamFriends::OnFriendRichPresenceUpdate)
	, m_SteamOnGameLobbyJoinRequested(this, &CCrySteamFriends::OnGameLobbyJoinRequested)
	//////////////////////////////////////////////////////////////////////////////
{
	// Make the CCryFriends base pointers point to our data so we can use the common code in CCryFriends
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		CCryFriends::m_pTask[i] = &m_task[i];
	}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriends::FriendsGetFriendsList(uint32 user, uint32 start, uint32 num, CryLobbyTaskID* pTaskID, CryFriendsGetFriendsListCallback pCb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryFriendsTaskID fTaskID = CryFriendsInvalidTaskID;
	ECryLobbyError error = StartTask(eT_FriendsGetFriendsList, false, user, &fTaskID, pTaskID, CryLobbyInvalidSessionHandle, (void*)pCb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[fTaskID];

		pTask->paramsNum[FRIENDS_LIST_PARAM_MAX_FRIENDS] = num;
		pTask->paramsNum[FRIENDS_LIST_PARAM_FRIENDS_REMAINING] = num;

		FROM_GAME_TO_LOBBY(&CCrySteamFriends::StartTaskRunning, this, fTaskID);
	}
	else
	{
		NetLog("[STEAM]: CCrySteamFriends::FriendsGetFriendsList() result %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriends::FriendsSendGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsCallback pCb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_ServiceNotSupported;
	CCryMatchMaking* pMatchMaking = (CCryMatchMaking*)m_pLobby->GetMatchMaking();

	if (pMatchMaking != NULL)
	{
		if (numUserIDs > 0)
		{
			CryFriendsTaskID fTaskID = CryFriendsInvalidTaskID;
			CryLobbySessionHandle lsh = pMatchMaking->GetSessionHandleFromGameSessionHandle(h);

			error = StartTask(eT_FriendsSendGameInvite, false, user, &fTaskID, pTaskID, lsh, (void*)pCb, pCbArg);
			if (error == eCLE_Success)
			{
				STask* pTask = &m_task[fTaskID];
				pTask->paramsNum[FRIENDS_INVITE_PARAM_NUM_FRIENDS] = numUserIDs;
				error = CreateTaskParamMem(fTaskID, FRIENDS_INVITE_PARAM_FRIENDS_LIST, NULL, numUserIDs * sizeof(CSteamID));

				if (error == eCLE_Success)
				{
					CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_INVITE_PARAM_FRIENDS_LIST]);
					for (uint32 index = 0; index < numUserIDs; ++index)
					{
						if (pUserIDs[index].IsValid())
						{
							pSteamID[index] = ((SCrySteamUserID*)pUserIDs[index].get())->m_steamID;
						}
						else
						{
							error = eCLE_InvalidUser;
							break;
						}
					}
				}
			}

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCrySteamFriends::StartTaskRunning, this, fTaskID);
			}
			else
			{
				FreeTask(fTaskID);
			}
		}
		else
		{
			error = eCLE_InvalidUser;
		}
	}

	if (error != eCLE_Success)
	{
		NetLog("[STEAM]: CCrySteamFriends::FriendsSendGameInvite() result %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::Tick(CTimeValue tv)
{
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		STask* pTask = &m_task[i];

	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		if (pTask->used)
		{
			if (m_pLobby->DebugTickCallStartTaskRunning(pTask->lTaskID))
			{
				StartTaskRunning(i);
				continue;
			}
			if (!m_pLobby->DebugOKToTickTask(pTask->lTaskID, pTask->running))
			{
				continue;
			}
		}
	#endif

		if (pTask->used && pTask->running)
		{
			switch (pTask->subTask)
			{
			case eT_FriendsGetFriendsList:
				TickFriendsGetFriendsList(i);
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriends::StartTask(ETask etask, bool startRunning, uint32 user, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryFriendsTaskID tmpFTaskID;
	CryFriendsTaskID* pUseFTaskID = pFTaskID ? pFTaskID : &tmpFTaskID;
	ECryLobbyError error = CCryFriends::StartTask(etask, startRunning, pUseFTaskID, pLTaskID, h, pCb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[*pUseFTaskID];
		pTask->m_timeStarted = g_time;
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::StartTaskRunning(CryFriendsTaskID fTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		ECryLobbyError error;

		if (!m_pLobby->DebugOKToStartTaskRunning(pTask->lTaskID))
		{
			return;
		}

		if (m_pLobby->DebugGenerateError(pTask->lTaskID, error))
		{
			UpdateTaskError(fTaskID, error);
			StopTaskRunning(fTaskID);
			return;
		}
	#endif

		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_FriendsGetFriendsList:
			StartFriendsGetFriendsList(fTaskID);
			break;

		case eT_FriendsSendGameInvite:
			StartFriendsSendGameInvite(fTaskID);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::EndTask(CryFriendsTaskID fTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		if (pTask->pCb)
		{
			switch (pTask->startedTask)
			{
			case eT_FriendsGetFriendsList:
				EndFriendsGetFriendsList(fTaskID);
				break;

			case eT_FriendsSendGameInvite:
				((CryFriendsCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pTask->pCbArg);
				break;
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Lobby] Friends EndTask %d (%d) Result %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		FreeTask(fTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::StopTaskRunning(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCrySteamFriends::EndTask, this, fTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::StartFriendsGetFriendsList(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
		UpdateTaskError(fTaskID, eCLE_SteamInitFailed);
		StopTaskRunning(fTaskID);
		return;
	}

	// Get the list of actual (rather than pending) friends
	int friendCount = pSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
	if (friendCount > pTask->paramsNum[FRIENDS_LIST_PARAM_MAX_FRIENDS])
	{
		friendCount = pTask->paramsNum[FRIENDS_LIST_PARAM_MAX_FRIENDS];
	}

	UpdateTaskError(fTaskID, CreateTaskParamMem(fTaskID, FRIENDS_LIST_PARAM_FRIENDS_LIST, NULL, friendCount * sizeof(SFriendInfo)));

	if (pTask->error == eCLE_Success)
	{
		m_SteamOnFriendRichPresenceUpdate.m_taskID = fTaskID;

		TMemHdl memHdl = pTask->paramsMem[FRIENDS_LIST_PARAM_FRIENDS_LIST];
		pTask->paramsNum[FRIENDS_LIST_PARAM_NUM_FRIENDS] = friendCount;
		SFriendInfo* pFriendInfo = (SFriendInfo*)m_pLobby->MemGetPtr(memHdl);

		for (uint32 index = 0; index < friendCount; ++index)
		{
			CSteamID friendID = pSteamFriends->GetFriendByIndex(index, k_EFriendFlagImmediate);
			pFriendInfo[index].userID = new SCrySteamUserID(friendID);
			cry_strcpy(pFriendInfo[index].name, pSteamFriends->GetFriendPersonaName(friendID));
		}
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::TickFriendsGetFriendsList(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->running)
	{
		uint64 elapsedTime = (g_time - pTask->m_timeStarted).GetMilliSecondsAsInt64();
		if (elapsedTime >= GET_FRIENDS_LIST_TIMEOUT)
		{
			UpdateTaskError(fTaskID, eCLE_TimeOut);
			StopTaskRunning(fTaskID);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::EndFriendsGetFriendsList(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	m_SteamOnFriendRichPresenceUpdate.m_taskID = CryFriendsInvalidTaskID;

	if (pTask->error == eCLE_Success)
	{
		TMemHdl memHdl = pTask->paramsMem[FRIENDS_LIST_PARAM_FRIENDS_LIST];
		SFriendInfo* pFriendInfo = (SFriendInfo*)m_pLobby->MemGetPtr(memHdl);
		uint32 friendCount = pTask->paramsNum[FRIENDS_LIST_PARAM_NUM_FRIENDS];

		((CryFriendsGetFriendsListCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pFriendInfo, friendCount, pTask->pCbArg);
	}
	else
	{
		((CryFriendsGetFriendsListCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::StartFriendsSendGameInvite(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	CCrySteamMatchMaking* pMatchMaking = (CCrySteamMatchMaking*)m_pLobby->GetMatchMaking();
	ISteamMatchmaking* pSteamMatchmaking = SteamMatchmaking();
	ISteamFriends* pSteamFriends = SteamFriends();

	if (pMatchMaking != NULL && pSteamMatchmaking != NULL && pSteamFriends != NULL)
	{
		SCrySteamSessionID sessionID(pMatchMaking->GetSteamSessionIDFromSession(pTask->session));

		uint32 numUserIDs = pTask->paramsNum[FRIENDS_INVITE_PARAM_NUM_FRIENDS];
		CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_INVITE_PARAM_FRIENDS_LIST]);
		for (uint32 index = 0; index < numUserIDs; ++index)
		{
			bool inviteSent = pSteamMatchmaking->InviteUserToLobby(sessionID.m_steamID, pSteamID[index]);
			NetLog("[STEAM]: %s to [%s] : [%s]", (inviteSent) ? "Invite sent" : "Failed to send", CSteamIDAsString(pSteamID[index]).c_str(), pSteamFriends->GetFriendPersonaName(pSteamID[index]));
		}
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////
//
// Steam callbacks
//
////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::OnFriendRichPresenceUpdate(FriendRichPresenceUpdate_t* pParam)
{
	ISteamFriends* pSteamFriends = SteamFriends();
	if (pSteamFriends && m_SteamOnFriendRichPresenceUpdate.m_taskID != CryFriendsInvalidTaskID)
	{
		STask* pTask = &m_task[m_SteamOnFriendRichPresenceUpdate.m_taskID];

		if (pTask->error == eCLE_Success)
		{
			uint32 friendCount = pTask->paramsNum[FRIENDS_LIST_PARAM_MAX_FRIENDS];
			TMemHdl memHdl = pTask->paramsMem[FRIENDS_LIST_PARAM_FRIENDS_LIST];
			SFriendInfo* pFriendInfo = (SFriendInfo*)m_pLobby->MemGetPtr(memHdl);

			for (uint32 index = 0; index < friendCount; ++index)
			{
				const SCrySteamUserID* pID = static_cast<const SCrySteamUserID*>(pFriendInfo[index].userID.get());
				if (pID->m_steamID == pParam->m_steamIDFriend)
				{
					cry_strcpy(pFriendInfo[index].presence, pSteamFriends->GetFriendRichPresence(pParam->m_steamIDFriend, STEAM_RICH_PRESENCE_STATUS_KEY));

					if (--pTask->paramsNum[FRIENDS_LIST_PARAM_FRIENDS_REMAINING] == 0)
					{
						// All done
						StopTaskRunning(m_SteamOnFriendRichPresenceUpdate.m_taskID);
					}
					break;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriends::OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* pParam)
{
	UCryLobbyEventData data;
	SCryLobbyInviteAcceptedData inviteAcceptedData;
	data.pInviteAcceptedData = &inviteAcceptedData;

	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
		return;
	}

	CrySessionID sessionID = new SCrySteamSessionID(pParam->m_steamIDLobby, true);
	;
	inviteAcceptedData.m_user = 0;
	inviteAcceptedData.m_id = sessionID;
	inviteAcceptedData.m_service = eCLS_Online;

	NetLog("[STEAM]: received invite to [%s] from [%s]", CSteamIDAsString(pParam->m_steamIDLobby).c_str(), pSteamFriends->GetFriendPersonaName(pParam->m_steamIDFriend));

	m_pLobby->DispatchEvent(eCLSE_InviteAccepted, data);
}

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_FRIENDS
