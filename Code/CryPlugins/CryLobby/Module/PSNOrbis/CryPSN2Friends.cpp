// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"
	#include "CryPSN2Response.h"
	#include "CryPSN2Friends.h"
	#include "CryPSN2WebApi.h"
	#include "CryPSN2WebApi_FriendsList.h"

	#if USE_PSN
		#if USE_CRY_FRIENDS

			#define FRIENDS_RESULTS_LIST        0     // mem + count num
			#define FRIENDS_WEBAPI_REQUEST_SLOT 1     // mem + jobid num

void CCryPSNFriends::ProcessWebApiEvent(SCryPSNSupportCallbackEventData& data)
{
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		STask* pTask = &m_task[i];
		if (pTask->used && pTask->running)
		{
			switch (pTask->subTask)
			{
			case eST_WaitingForFriendsListWebApiCallback:
				{
					if (pTask->paramsNum[FRIENDS_WEBAPI_REQUEST_SLOT] == data.m_webApiEvent.m_id)
					{
						EventWebApiGetFriends(i, data);
						data.m_webApiEvent.m_returnFlags |= SCryPSNSupportCallbackEventData::SWebApiEvent::WEBAPI_EVENT_HANDLED;
					}
				}
				break;
			}
		}
	}
}

void CCryPSNFriends::SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg)
{
	CCryPSNFriends* _this = static_cast<CCryPSNFriends*>(pArg);

	switch (ecb)
	{
	case eCE_ErrorEvent:
		{
			for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
			{
				STask* pTask = &_this->m_task[i];

				if (pTask->used && pTask->running)
				{
					_this->UpdateTaskError(i, MapSupportErrorToLobbyError(data.m_errorEvent.m_error));
				}
			}
		}
		break;
	case eCE_WebApiEvent:
		{
			_this->ProcessWebApiEvent(data);
		}
		break;
	}
}

CCryPSNFriends::CCryPSNFriends(CCryLobby* pLobby, CCryLobbyService* pService, CCryPSNSupport* pSupport) : CCryFriends(pLobby, pService)
{
	m_pPSNSupport = pSupport;

	// Make the CCryLobbyUI base pointers point to our data so we can use the common code in CCryLobbyUI
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		CCryFriends::m_pTask[i] = &m_task[i];
	}
}

ECryLobbyError CCryPSNFriends::StartTask(ETask etask, bool startRunning, uint32 user, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryFriendsTaskID tmpFTaskID;
	CryFriendsTaskID* pUseFTaskID = pFTaskID ? pFTaskID : &tmpFTaskID;
	ECryLobbyError error = CCryFriends::StartTask(etask, startRunning, pUseFTaskID, pLTaskID, h, pCb, pCbArg);
	return error;
}

void CCryPSNFriends::StartSubTask(ETask etask, CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];
	pTask->subTask = etask;
}

void CCryPSNFriends::StartTaskRunning(CryFriendsTaskID fTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_FriendsGetFriendsList:
			StartFriendsGetFriendsList(fTaskID);
			break;

		case eT_FriendsSendGameInvite:
			StopTaskRunning(fTaskID);
			break;
		}
	}
}

void CCryPSNFriends::EndTask(CryFriendsTaskID fTaskID)
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

void CCryPSNFriends::StopTaskRunning(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->used)
	{
		pTask->running = false;
		TO_GAME_FROM_LOBBY(&CCryPSNFriends::EndTask, this, fTaskID);
	}
}

ECryLobbyError CCryPSNFriends::FriendsGetFriendsList(uint32 user, uint32 start, uint32 num, CryLobbyTaskID* pTaskID, CryFriendsGetFriendsListCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;
	CryFriendsTaskID fTaskID;

	ECryLobbyError error = StartTask(eT_FriendsGetFriendsList, false, user, &fTaskID, pTaskID, CryLobbyInvalidSessionHandle, (void*)cb, pCbArg);
	if (error == eCLE_Success)
	{
		UpdateTaskError(fTaskID, CreateTaskParamMem(fTaskID, FRIENDS_WEBAPI_REQUEST_SLOT, NULL, sizeof(SCryPSNOrbisWebApiGetFriendsListInput)));
		if (error == eCLE_Success)
		{
			STask* pTask = &m_task[fTaskID];
			SCryPSNOrbisWebApiGetFriendsListInput* pFriendsInput = (SCryPSNOrbisWebApiGetFriendsListInput*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_WEBAPI_REQUEST_SLOT]);
			pFriendsInput->startIndex = start;
			pFriendsInput->count = num;

			FROM_GAME_TO_LOBBY(&CCryPSNFriends::StartTaskRunning, this, fTaskID);
		}
		else
		{
			FreeTask(fTaskID);
		}
	}
	return error;
}

void CCryPSNFriends::StartFriendsGetFriendsList(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	SCryPSNOrbisWebApiGetFriendsListInput* pFriendsInput = (SCryPSNOrbisWebApiGetFriendsListInput*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_WEBAPI_REQUEST_SLOT]);
	if (pFriendsInput)
	{
		memcpy(&pFriendsInput->id, &m_pPSNSupport->GetLocalNpId()->handle, sizeof(SceNpOnlineId));

		UpdateTaskError(fTaskID, CreateTaskParamMem(fTaskID, FRIENDS_RESULTS_LIST, NULL, pFriendsInput->count * sizeof(SFriendInfo)));
		if (pTask->error == eCLE_Success)
		{
			StartSubTask(eST_WaitingForFriendsListWebApiCallback, fTaskID);

			SFriendInfo* pFriends = (SFriendInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_RESULTS_LIST]);
			memset(pFriends, 0, pFriendsInput->count * sizeof(SFriendInfo));
			pTask->paramsNum[FRIENDS_RESULTS_LIST] = 0;

			pTask->paramsNum[FRIENDS_WEBAPI_REQUEST_SLOT] = m_pPSNSupport->GetWebApiInterface().AddJob(CCryPSNOrbisWebApiThread::GetFriendsList, pTask->paramsMem[FRIENDS_WEBAPI_REQUEST_SLOT]);
			if (pTask->paramsNum[FRIENDS_WEBAPI_REQUEST_SLOT] == INVALID_WEBAPI_JOB_ID)
			{
				// failed
				UpdateTaskError(fTaskID, eCLE_InternalError);
			}
		}
		else
		{
			//-- Out of memory
			UpdateTaskError(fTaskID, eCLE_OutOfMemory);
		}
	}
	else
	{
		//-- Out of memory
		UpdateTaskError(fTaskID, eCLE_OutOfMemory);
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(fTaskID);
	}
}

void CCryPSNFriends::EventWebApiGetFriends(CryFriendsTaskID fTaskID, SCryPSNSupportCallbackEventData& data)
{
	// parse json response to array
	if (data.m_webApiEvent.m_error == PSN_OK)
	{
		if (data.m_webApiEvent.m_pResponseBody)
		{
			if (data.m_webApiEvent.m_pResponseBody->eType == SCryPSNWebApiResponseBody::E_JSON)
			{
				// parse json to get details
				// CCryPSNWebApiJobController::PrintResponseJSONTree(data.m_webApiEvent.m_pResponseBody);

				STask* pTask = &m_task[fTaskID];
				SCryPSNOrbisWebApiGetFriendsListInput* pFriendsInput = (SCryPSNOrbisWebApiGetFriendsListInput*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_WEBAPI_REQUEST_SLOT]);
				SFriendInfo* pFriends = (SFriendInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_RESULTS_LIST]);

				const sce::Json::Value& numTotalFriends = data.m_webApiEvent.m_pResponseBody->jsonTreeRoot["totalResults"];
				if (numTotalFriends.getType() == sce::Json::kValueTypeUInteger)
				{
					uint32 nTotal = numTotalFriends.getUInteger();
					const sce::Json::Value& numReturnedFriends = data.m_webApiEvent.m_pResponseBody->jsonTreeRoot["size"];
					if (numReturnedFriends.getType() == sce::Json::kValueTypeUInteger)
					{
						const sce::Json::Value& returnedFriendsArray = data.m_webApiEvent.m_pResponseBody->jsonTreeRoot["friendList"];
						if (returnedFriendsArray.getType() == sce::Json::kValueTypeArray)
						{
							uint32 nReturned = numTotalFriends.getUInteger();
							const sce::Json::Value& numReturnedOffset = data.m_webApiEvent.m_pResponseBody->jsonTreeRoot["start"];
							if (numReturnedOffset.getType() == sce::Json::kValueTypeUInteger)
							{
								uint32 nOffset = numReturnedOffset.getUInteger();

								// sanity tests.
								// 1. The offset returned should be within the range of our initial request
								if ((nOffset < pFriendsInput->startIndex) || (nOffset >= (pFriendsInput->startIndex + pFriendsInput->count)))
								{
									// offset out of bounds
									UpdateTaskError(fTaskID, eCLE_InvalidRequest);
									StopTaskRunning(fTaskID);
									return;
								}

								// 2. size returned should be <= size requested.
								if (nReturned > pFriendsInput->count)
								{
									// returned count is more than the requested range. This shouldn't happen!
									UpdateTaskError(fTaskID, eCLE_InvalidRequest);
									StopTaskRunning(fTaskID);
									return;
								}

								// if size returned is 0, we're done!
								if (nReturned == 0)
								{
									UpdateTaskError(fTaskID, eCLE_Success);
									StopTaskRunning(fTaskID);
									return;
								}

								// copy friends
								for (uint32 i = 0; i < nReturned; i++)
								{
									SFriendInfo& friendr = pFriends[pTask->paramsNum[FRIENDS_RESULTS_LIST]];
									const sce::Json::Value& friendObj = returnedFriendsArray[i];
									if (friendObj.getType() == sce::Json::kValueTypeObject)
									{
										SCryPSNUserID* pId = new SCryPSNUserID;
										if (pId)
										{
											friendr.status = eLFS_Offline;

											sceNpWebApiUtilityParseNpId(friendObj["npId"].toString().c_str(), &pId->npId);
											friendr.userID = pId;

											cry_strcpy(friendr.name, friendObj["onlineId"].toString().c_str());

											const sce::Json::Value& pres = friendObj["presence"];
											if (pres.getType() == sce::Json::kValueTypeObject)
											{
												const sce::Json::Value& pripres = pres["primaryInfo"];
												if (pripres.getType() == sce::Json::kValueTypeObject)
												{
													const sce::Json::Value& status = pripres["gameStatus"];
													if (status.getType() == sce::Json::kValueTypeString)
													{
														cry_strcpy(friendr.presence, status.toString().c_str());
													}

													const sce::Json::Value& onlineStatus = pripres["onlineStatus"];
													if (!strcmp(onlineStatus.toString().c_str(), "offline"))
													{
														friendr.status = eLFS_Offline;
													}
													else if (!strcmp(onlineStatus.toString().c_str(), "standby"))
													{
														// treating standby mode as offline too
														friendr.status = eLFS_Offline;
													}
													else if (!strcmp(onlineStatus.toString().c_str(), "online"))
													{
														// there doesn't seem to be a way to distinguish between online-in-context and online-out-of-context.
														// The gameStatus is supposed to be invalid if not in the same NP communication id, so we'll try that.
														if (status == sce::Json::kValueTypeNull)
														{
															// friend not in same game
															friendr.status = eLFS_Online;
														}
														else
														{
															// friend is in same game.
															friendr.status = eLFS_OnlineSameTitle;
														}
													}
												}
											}

											pTask->paramsNum[FRIENDS_RESULTS_LIST]++;
										}
									}
									else
									{
										UpdateTaskError(fTaskID, eCLE_OutOfMemory);
										StopTaskRunning(fTaskID);
										return;
									}
								}

								// Are we finished?
								if ((pTask->paramsNum[FRIENDS_RESULTS_LIST] == pFriendsInput->count)
								    || ((nTotal < pFriendsInput->count) && (nReturned == nTotal))
								    || ((pFriendsInput->startIndex + pTask->paramsNum[FRIENDS_RESULTS_LIST]) >= nTotal))
								{
									// got all the friends we requested, so finished
									UpdateTaskError(fTaskID, eCLE_Success);
									StopTaskRunning(fTaskID);
									return;
								}

								// if we're here, we are still expecting more results to come back in a future event, so task is still running.
								// return now
								return;
							}
						}
					}
				}
			}
		}
	}

	// if we reach here, the json is bad, or we have failed for some uncaught reason
	UpdateTaskError(fTaskID, eCLE_InternalError);
	StopTaskRunning(fTaskID);
}

void CCryPSNFriends::EndFriendsGetFriendsList(CryFriendsTaskID fTaskID)
{
	STask* pTask = &m_task[fTaskID];

	if (pTask->paramsMem[FRIENDS_RESULTS_LIST] != TMemInvalidHdl)
	{
		SFriendInfo* pFriends = (SFriendInfo*)m_pLobby->MemGetPtr(pTask->paramsMem[FRIENDS_RESULTS_LIST]);

		if (pTask->error == eCLE_Success)
		{
			((CryFriendsGetFriendsListCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pFriends, pTask->paramsNum[FRIENDS_RESULTS_LIST], pTask->pCbArg);
		}
		else
		{
			((CryFriendsGetFriendsListCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
		}

		//-- remove our references to the user Ids
		for (uint32 i = 0; i < pTask->paramsNum[FRIENDS_RESULTS_LIST]; i++)
		{
			pFriends[i].userID = NULL;
		}
	}
	else
	{
		((CryFriendsGetFriendsListCallback)pTask->pCb)(pTask->lTaskID, pTask->error, NULL, 0, pTask->pCbArg);
	}
}

void CCryPSNFriends::LobbyUIGameInviteCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg)
{
	/*
	   CCryPSNFriends* _this = static_cast<CCryPSNFriends*>(pArg);

	   for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	   {
	    STask* pTask = &_this->m_task[i];
	    if (pTask->used && pTask->running && (pTask->subTask == eT_FriendsSendGameInvite))
	    {
	      if (pTask->lTaskID == taskID)
	      {
	        //-- us?
	        _this->UpdateTaskError(i, error);
	        _this->StopTaskRunning(i);
	        return;
	      }
	    }
	   }
	 */
}

ECryLobbyError CCryPSNFriends::FriendsSendGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;
	/*
	   ECryLobbyError error = eCLE_Success;

	   if (pUserIDs && (numUserIDs > 0))
	   {
	    CryFriendsTaskID fTaskID;

	    error = StartTask(eT_FriendsSendGameInvite, false, user, &fTaskID, pTaskID, CryLobbyInvalidSessionHandle, (void*)cb, pCbArg);
	    if (error == eCLE_Success)
	    {
	      error = m_pLobby->GetLobbyUI()->ShowGameInvite(user, h, pUserIDs, numUserIDs, pTaskID, LobbyUIGameInviteCallback, this);
	      if (error == eCLE_Success)
	      {
	        FROM_GAME_TO_LOBBY(&CCryPSNFriends::StartTaskRunning, this, fTaskID);
	      }
	      else
	      {
	        FreeTask(fTaskID);
	      }
	    }
	   }
	   else
	   {
	    error = eCLE_InvalidUser;
	   }

	   return error;
	 */
	return eCLE_ServiceNotSupported;
}

		#endif // USE_CRY_FRIENDS
	#endif   // USE_PSN
#endif     // CRY_PLATFORM_ORBIS
