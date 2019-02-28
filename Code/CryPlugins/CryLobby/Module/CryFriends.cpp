// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryFriends.h"

#if USE_CRY_FRIENDS

CCryFriends::CCryFriends(CCryLobby* pLobby, CCryLobbyService* pService)
{
	CRY_ASSERT_MESSAGE(pLobby, "CCryFriends::CCryFriends: Lobby not specified");
	CRY_ASSERT_MESSAGE(pService, "CCryFriends::CCryFriends: Service not specified");

	m_pLobby = pLobby;
	m_pService = pService;

	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		m_pTask[i] = NULL;
	}
}

ECryLobbyError CCryFriends::Initialise()
{
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryFriends: Task base pointers not setup");
		m_pTask[i]->used = false;
	}

	return eCLE_Success;
}

ECryLobbyError CCryFriends::Terminate()
{
	for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryFriends: Task base pointers not setup");

		if (m_pTask[i]->used)
		{
			FreeTask(i);
		}
	}

	return eCLE_Success;
}

ECryLobbyError CCryFriends::StartTask(uint32 eTask, bool startRunning, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryLobbyTaskID lobbyTaskID = m_pLobby->CreateTask();

	if (lobbyTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryFriends: Task base pointers not setup");

			if (!pTask->used)
			{
				pTask->lTaskID = lobbyTaskID;
				pTask->error = eCLE_Success;
				pTask->startedTask = eTask;
				pTask->subTask = eTask;
				pTask->session = h;
				pTask->pCb = pCb;
				pTask->pCbArg = pCbArg;
				pTask->used = true;
				pTask->running = startRunning;
				pTask->canceled = false;

				for (uint32 j = 0; j < MAX_FRIENDS_PARAMS; j++)
				{
					pTask->paramsMem[j] = TMemInvalidHdl;
					pTask->paramsNum[j] = 0;
				}

				if (pFTaskID)
				{
					*pFTaskID = i;
				}

				if (pLTaskID)
				{
					*pLTaskID = lobbyTaskID;
				}

				return eCLE_Success;
			}
		}

		m_pLobby->ReleaseTask(lobbyTaskID);
	}

	return eCLE_TooManyTasks;
}

void CCryFriends::FreeTask(CryFriendsTaskID fTaskID)
{
	STask* pTask = m_pTask[fTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryFriends: Task base pointers not setup");

	for (uint32 i = 0; i < MAX_FRIENDS_PARAMS; i++)
	{
		if (pTask->paramsMem[i] != TMemInvalidHdl)
		{
			m_pLobby->MemFree(pTask->paramsMem[i]);
			pTask->paramsMem[i] = TMemInvalidHdl;
		}
		pTask->paramsNum[i] = 0;
	}

	m_pLobby->ReleaseTask(pTask->lTaskID);

	pTask->used = false;
}

void CCryFriends::CancelTask(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	NetLog("[Lobby]Try cancel task %u", lTaskID);

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_FRIENDS_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryFriends: Task base pointers not setup");

			if (pTask->used && (pTask->lTaskID == lTaskID))
			{
				NetLog("[Lobby] Task %u canceled", lTaskID);
				pTask->pCb = NULL;
				pTask->canceled = true;

				break;
			}
		}
	}
}

ECryLobbyError CCryFriends::CreateTaskParamMem(CryFriendsTaskID fTaskID, uint32 param, const void* pParamData, size_t paramDataSize)
{
	STask* pTask = m_pTask[fTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryFriends: Task base pointers not setup");

	if (paramDataSize > 0)
	{
		pTask->paramsMem[param] = m_pLobby->MemAlloc(paramDataSize);
		void* p = m_pLobby->MemGetPtr(pTask->paramsMem[param]);

		if (p)
		{
			if (pParamData)
			{
				memcpy(p, pParamData, paramDataSize);
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	return eCLE_Success;
}

void CCryFriends::UpdateTaskError(CryFriendsTaskID fTaskID, ECryLobbyError error)
{
	STask* pTask = m_pTask[fTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryFriends: Task base pointers not setup");

	if (pTask->error == eCLE_Success)
	{
		pTask->error = error;
	}
}

#endif // USE_CRY_FRIENDS
