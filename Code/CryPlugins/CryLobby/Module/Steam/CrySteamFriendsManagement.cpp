// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamFriendsManagement.h
//  Created:     26/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryFriends
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Steam/CrySteamLobby.h" // CrySteamFriends.h included here

#if USE_STEAM && USE_CRY_FRIENDS

////////////////////////////////////////////////////////////////////////////////

	#define FRIENDS_MANAGEMENT_PARAM_NUM_IDS         (0) // int
	#define FRIENDS_MANAGEMENT_PARAM_ID_LIST         (0) // ptr

	#define FRIENDS_MANAGEMENT_PARAM_MAX_RESULTS     (0) // int
	#define FRIENDS_MANAGEMENT_PARAM_SEARCH_INFO     (0) // ptr

	#define FRIENDS_MANAGEMENT_PARAM_RESULTS         (1) // ptr
	#define FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME (1) // ptr

////////////////////////////////////////////////////////////////////////////////

CCrySteamFriendsManagement::CCrySteamFriendsManagement(CCryLobby* pLobby, CCryLobbyService* pService)
	: CCryFriendsManagement(pLobby, pService)
{
	// Make the CCryFriendsManagement base pointers point to our data so we can use the common code in CCryFriendsManagement
	for (uint32 index = 0; index < MAX_FRIENDS_MANAGEMENT_TASKS; ++index)
	{
		CCryFriendsManagement::m_pTask[index] = &m_task[index];
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::Tick(CTimeValue tv)
{
	// Currently, no steam specific tasks need ticking, but leave this in case something crops up
	//for (uint32 i = 0; i < MAX_FRIENDS_MANAGEMENT_TASKS; i++)
	//{
	//	STask* pTask = &m_task[i];

	//	if (pTask->used && pTask->running)
	//	{
	//		switch (pTask->subTask)
	//		{
	//		default:
	//			break;
	//		}
	//	}
	//}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::InitialiseTask(ETask task, uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, void* pCb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryFriendsTaskID fTaskID;
	ECryLobbyError error = StartTask(task, false, user, &fTaskID, pTaskID, pCb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[fTaskID];
		pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS] = numUserIDs;

		if (numUserIDs > 0)
		{
			error = CreateTaskParamMem(fTaskID, FRIENDS_MANAGEMENT_PARAM_ID_LIST, NULL, numUserIDs * sizeof(CSteamID));

			if (error == eCLE_Success)
			{
				CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);

				for (uint32 index = 0; index < numUserIDs; index++)
				{
					if (pUserIDs[index].IsValid())
					{
						pSteamID[index] = ((const SCrySteamUserID*)pUserIDs[index].get())->m_steamID;
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
			FROM_GAME_TO_LOBBY(&CCrySteamFriendsManagement::StartTaskRunning, this, fTaskID);
		}
		else
		{
			FreeTask(fTaskID);
		}
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::StartTask(ETask task, bool startRunning, uint32 user, CryFriendsManagementTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg)
{
	CryFriendsTaskID tmpFTaskID;
	CryFriendsTaskID* pUseFTaskID = pFTaskID ? pFTaskID : &tmpFTaskID;
	ECryLobbyError error = CCryFriendsManagement::StartTask(task, startRunning, pUseFTaskID, pLTaskID, pCb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[*pUseFTaskID];
		pTask->m_timeStarted = g_time;
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartTaskRunning(CryFriendsManagementTaskID fTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_FriendsManagementIsUserFriend:
			StartFriendsManagementIsUserFriend(fTaskID);
			break;
		case eT_FriendsManagementIsUserBlocked:
			StartFriendsManagementIsUserBlocked(fTaskID);
			break;
		case eT_FriendsManagementAcceptInvite:
			StartFriendsManagementAcceptInvite(fTaskID);
			break;
		case eT_FriendsManagementGetName:
			StartFriendsManagementGetName(fTaskID);
			break;
		case eT_FriendsManagementGetStatus:
			StartFriendsManagementGetStatus(fTaskID);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::EndTask(CryFriendsManagementTaskID fTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		if (pTask->pCb)
		{
			switch (pTask->startedTask)
			{
			case eT_FriendsManagementIsUserFriend:
				EndFriendsManagementIsUserFriend(fTaskID);
				break;

			case eT_FriendsManagementIsUserBlocked:
				EndFriendsManagementIsUserBlocked(fTaskID);
				break;

			case eT_FriendsManagementAcceptInvite:        // intentional fall-through
				((CryFriendsManagementCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pTask->pCbArg);
				break;

			case eT_FriendsManagementGetName:
				EndFriendsManagementGetName(fTaskID);
				break;

			case eT_FriendsManagementGetStatus:
				EndFriendsManagementGetStatus(fTaskID);
				break;
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Lobby] CCrySteamFriendsManagement::EndTask() %d (%d) Result %d", pTask->startedTask, pTask->subTask, pTask->error);
		}

		FreeTask(fTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StopTaskRunning(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCrySteamFriendsManagement::EndTask, this, fTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementSendFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementAcceptFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementRejectFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementRevokeFriendStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementIsUserFriend(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg)
{
	ECryLobbyError error = InitialiseTask(eT_FriendsManagementIsUserFriend, user, pUserIDs, numUserIDs, pTaskID, (void*)pCb, pCbArg);
	NetLog("[STEAM] CCrySteamFriendsManagement::FriendsManagementIsUserFriend(), result %d", error);
	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementFindUser(uint32 user, SFriendManagementSearchInfo* pUserName, uint32 maxResults, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementBlockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementUnblockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	return eCLE_ServiceNotSupported;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementIsUserBlocked(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg)
{
	ECryLobbyError error = InitialiseTask(eT_FriendsManagementIsUserBlocked, user, pUserIDs, numUserIDs, pTaskID, (void*)pCb, pCbArg);
	NetLog("[STEAM] CCrySteamFriendsManagement::FriendsManagementIsBlocked(), result %d", error);
	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementAcceptInvite(uint32 user, CryUserID* pUserID, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg)
{
	ECryLobbyError error = InitialiseTask(eT_FriendsManagementAcceptInvite, user, pUserID, 1, pTaskID, (void*)pCb, pCbArg);
	NetLog("[STEAM] CCrySteamFriendsManagement::FriendsManagementAcceptInvite(), result %d", error);
	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementGetName(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg)
{
	ECryLobbyError error = InitialiseTask(eT_FriendsManagementGetName, user, pUserIDs, numUserIDs, pTaskID, (void*)pCb, pCbArg);
	NetLog("[STEAM] CCrySteamFriendsManagement::FriendsManagementGetName(), result %d", error);
	return error;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamFriendsManagement::FriendsManagementGetStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementStatusCallback pCb, void* pCbArg)
{
	ECryLobbyError error = InitialiseTask(eT_FriendsManagementGetStatus, user, pUserIDs, numUserIDs, pTaskID, (void*)pCb, pCbArg);
	NetLog("[STEAM] CCrySteamFriendsManagement::FriendsManagementGetStatus(), result %d", error);
	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartFriendsManagementIsUserFriend(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
	ECryLobbyError error;
	ISteamFriends* pSteamFriends = SteamFriends();
		
	if (pSteamFriends)
	{
		error = CreateTaskParamMem(fTaskID, FRIENDS_MANAGEMENT_PARAM_RESULTS, NULL, numUserIDs * sizeof(bool));
	}
	else
	{
		error = eCLE_SteamInitFailed;
	}

	if (error == eCLE_Success)
	{
		bool* pFriend = (bool*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS]);

		for (uint32 index = 0; index < numUserIDs; index++)
		{
			switch (pSteamFriends->GetFriendRelationship(pSteamID[index]))
			{
			case k_EFriendRelationshipFriend:
				pFriend[index] = true;
				break;

			case k_EFriendRelationshipNone:               // Intentional fall-through
			case k_EFriendRelationshipBlocked:            // Intentional fall-through
			case k_EFriendRelationshipRequestRecipient:   // Intentional fall-through
			case k_EFriendRelationshipRequestInitiator:   // Intentional fall-through
			case k_EFriendRelationshipIgnored:            // Intentional fall-through
			case k_EFriendRelationshipIgnoredFriend:      // Intentional fall-through
			default:
				pFriend[index] = false;
				break;
			}
		}
	}
	else
	{
		UpdateTaskError(fTaskID, error);
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::EndFriendsManagementIsUserFriend(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	bool* pFriend = (bool*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];

	TMemHdl memHdl = m_pLobby->MemAlloc(numUserIDs * sizeof(SFriendManagementInfo));
	SFriendManagementInfo* pResults = (SFriendManagementInfo*)m_pLobby->MemGetPtr(memHdl);

	if (pResults != NULL)
	{
		for (uint32 index = 0; index < numUserIDs; ++index)
		{
			pResults[index].result = pFriend[index];
			pResults[index].userID = new SCrySteamUserID(pSteamID[index]);

			if (pResults[index].userID == NULL)
			{
				pTask->error = eCLE_OutOfMemory;
				break;
			}
		}
	}
	else
	{
		pTask->error = eCLE_OutOfMemory;
	}

	if (pTask->error == eCLE_Success)
	{
		((CryFriendsManagementInfoCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pResults, numUserIDs, pTask->pCbArg);
	}
	else
	{
		((CryFriendsManagementInfoCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartFriendsManagementIsUserBlocked(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
	ECryLobbyError error;
	ISteamFriends* pSteamFriends = SteamFriends();

	if (pSteamFriends)
	{
		error = CreateTaskParamMem(fTaskID, FRIENDS_MANAGEMENT_PARAM_RESULTS, NULL, numUserIDs * sizeof(bool));
	}
	else
	{
		error = eCLE_SteamInitFailed;
	}

	if (error == eCLE_Success)
	{
		bool* pFriend = (bool*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS]);

		for (uint32 index = 0; index < numUserIDs; index++)
		{
			switch (pSteamFriends->GetFriendRelationship(pSteamID[index]))
			{
			case k_EFriendRelationshipBlocked:
				pFriend[index] = true;
				break;

			case k_EFriendRelationshipNone:               // Intentional fall-through
			case k_EFriendRelationshipRequestRecipient:   // Intentional fall-through
			case k_EFriendRelationshipFriend:             // Intentional fall-through
			case k_EFriendRelationshipRequestInitiator:   // Intentional fall-through
			case k_EFriendRelationshipIgnored:            // Intentional fall-through
			case k_EFriendRelationshipIgnoredFriend:      // Intentional fall-through
			default:
				pFriend[index] = false;
				break;
			}
		}
	}
	else
	{
		UpdateTaskError(fTaskID, error);
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::EndFriendsManagementIsUserBlocked(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	bool* pFriend = (bool*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];

	TMemHdl memHdl = m_pLobby->MemAlloc(numUserIDs * sizeof(SFriendManagementInfo));
	SFriendManagementInfo* pResults = (SFriendManagementInfo*)m_pLobby->MemGetPtr(memHdl);

	if (pResults != NULL)
	{
		for (uint32 index = 0; index < numUserIDs; ++index)
		{
			pResults[index].result = pFriend[index];
			pResults[index].userID = new SCrySteamUserID(pSteamID[index]);

			if (pResults[index].userID == NULL)
			{
				pTask->error = eCLE_OutOfMemory;
				break;
			}
		}
	}
	else
	{
		pTask->error = eCLE_OutOfMemory;
	}

	if (pTask->error == eCLE_Success)
	{
		((CryFriendsManagementInfoCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pResults, numUserIDs, pTask->pCbArg);
	}
	else
	{
		((CryFriendsManagementInfoCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartFriendsManagementAcceptInvite(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);

	SCrySteamSessionID* pSessionID = new SCrySteamSessionID(*pSteamID, true);
	CCrySteamLobbyService* pSteamLobbyService = static_cast<CCrySteamLobbyService*>(m_pLobby->GetLobbyService(eCLS_Online));

	TO_GAME_FROM_LOBBY(&CCrySteamLobbyService::InviteAccepted, pSteamLobbyService, uint32(0), CrySessionID(pSessionID));

	NetLog("[STEAM]: CCrySteamFriendsManagement::StartFriendsManagementAcceptInvite(): invite accepted");
	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartFriendsManagementGetName(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
	ISteamFriends* pSteamFriends = SteamFriends();
	if (pSteamFriends)
	{
		UpdateTaskError(fTaskID, CreateTaskParamMem(fTaskID, FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME, NULL, numUserIDs * sizeof(SFriendInfo)));
		SFriendInfo* pResults = (SFriendInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME]);

		if (pResults != NULL)
		{
			memset(pResults, 0, numUserIDs * sizeof(SFriendInfo));

			for (uint32 index = 0; index < numUserIDs; ++index)
			{
				pResults[index].userID = new SCrySteamUserID(pSteamID[index]);
				if (pResults[index].userID == NULL)
				{
					pTask->error = eCLE_OutOfMemory;
					break;
				}

				cry_strcpy(pResults[index].name, pSteamFriends->GetFriendPersonaName(pSteamID[index]));
			}
		}
	}
	else
	{
		UpdateTaskError(fTaskID, eCLE_SteamInitFailed);
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::EndFriendsManagementGetName(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->error == eCLE_Success)
	{
		uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
		SFriendInfo* pResults = (SFriendInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME]);

		((CryFriendsManagementSearchCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pResults, numUserIDs, pTask->pCbArg);
	}
	else
	{
		((CryFriendsManagementSearchCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::StartFriendsManagementGetStatus(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	CSteamID* pSteamID = (CSteamID*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_ID_LIST]);
	uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
	ISteamFriends* pSteamFriends = SteamFriends();
	if (pSteamFriends)
	{
		UpdateTaskError(fTaskID, CreateTaskParamMem(fTaskID, FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME, NULL, numUserIDs * sizeof(SFriendStatusInfo)));
		SFriendStatusInfo* pResults = (SFriendStatusInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME]);

		if (pResults != NULL)
		{
			memset(pResults, 0, numUserIDs * sizeof(SFriendStatusInfo));

			for (uint32 index = 0; index < numUserIDs; ++index)
			{
				pResults[index].userID = new SCrySteamUserID(pSteamID[index]);
				if (pResults[index].userID == NULL)
				{
					pTask->error = eCLE_OutOfMemory;
					break;
				}

				switch (pSteamFriends->GetFriendPersonaState(pSteamID[index]))
				{
				case k_EPersonaStateOffline:        // friend is not currently logged on
					pResults[index].status = eFMS_Offline;
					break;
				case k_EPersonaStateLookingToTrade: // Online, trading
				case k_EPersonaStateLookingToPlay:  // Online, wanting to play
				case k_EPersonaStateOnline:         // friend is logged on
					pResults[index].status = eFMS_Online;
					break;
				case k_EPersonaStateBusy:           // user is on, but busy
				case k_EPersonaStateAway:           // auto-away feature
				case k_EPersonaStateSnooze:         // auto-away for a long time
					pResults[index].status = eFMS_Away;
					break;
				}
			}
		}
	}
	else
	{
		UpdateTaskError(fTaskID, eCLE_SteamInitFailed);
	}

	StopTaskRunning(fTaskID);
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamFriendsManagement::EndFriendsManagementGetStatus(CryFriendsManagementTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->error == eCLE_Success)
	{
		uint32 numUserIDs = pTask->paramsNum[FRIENDS_MANAGEMENT_PARAM_NUM_IDS];
		SFriendStatusInfo* pResults = (SFriendStatusInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_MANAGEMENT_PARAM_RESULTS_TO_GAME]);

		((CryFriendsManagementStatusCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pResults, numUserIDs, pTask->pCbArg);
	}
	else
	{
		((CryFriendsManagementStatusCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_FRIENDS
