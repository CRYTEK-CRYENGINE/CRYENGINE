// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryLobbyUI.h"

CCryLobbyUI::CCryLobbyUI(CCryLobby* pLobby, CCryLobbyService* pService)
{
	CRY_ASSERT_MESSAGE(pLobby, "CCryLobbyUI::CCryLobbyUI: Lobby not specified");
	CRY_ASSERT_MESSAGE(pService, "CCryLobbyUI::CCryLobbyUI: Service not specified");

	m_pLobby = pLobby;
	m_pService = pService;

	for (uint32 i = 0; i < MAX_LOBBYUI_TASKS; i++)
	{
		m_pTask[i] = NULL;
	}
}

ECryLobbyError CCryLobbyUI::Initialise()
{
	for (uint32 i = 0; i < MAX_LOBBYUI_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryLobbyUI: Task base pointers not setup");
		m_pTask[i]->used = false;
	}

	return eCLE_Success;
}

ECryLobbyError CCryLobbyUI::Terminate()
{
	for (uint32 i = 0; i < MAX_LOBBYUI_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryLobbyUI: Task base pointers not setup");

		if (m_pTask[i]->used)
		{
			FreeTask(i);
		}
	}

	return eCLE_Success;
}

ECryLobbyError CCryLobbyUI::StartTask(uint32 eTask, bool startRunning, CryLobbyUITaskID* pUITaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryLobbyTaskID lobbyTaskID = m_pLobby->CreateTask();

	if (lobbyTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_LOBBYUI_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryLobbyUI: Task base pointers not setup");

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

				for (uint32 j = 0; j < MAX_LOBBYUI_PARAMS; j++)
				{
					pTask->paramsMem[j] = TMemInvalidHdl;
					pTask->paramsNum[j] = 0;
				}

				if (pUITaskID)
				{
					*pUITaskID = i;
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

void CCryLobbyUI::FreeTask(CryLobbyUITaskID uiTaskID)
{
	STask* pTask = m_pTask[uiTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryLobbyUI: Task base pointers not setup");

	for (uint32 i = 0; i < MAX_LOBBYUI_PARAMS; i++)
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

void CCryLobbyUI::CancelTask(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	NetLog("[Lobby]Try cancel task %u", lTaskID);

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_LOBBYUI_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryLobbyUI: Task base pointers not setup");

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

ECryLobbyError CCryLobbyUI::CreateTaskParamMem(CryLobbyUITaskID uiTaskID, uint32 param, const void* pParamData, size_t paramDataSize)
{
	STask* pTask = m_pTask[uiTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryLobbyUI: Task base pointers not setup");

	if (paramDataSize > 0)
	{
		assert(param < MAX_LOBBYUI_PARAMS);

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

void CCryLobbyUI::UpdateTaskError(CryLobbyUITaskID uiTaskID, ECryLobbyError error)
{
	STask* pTask = m_pTask[uiTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryLobbyUI: Task base pointers not setup");

	if (pTask->error == eCLE_Success)
	{
		pTask->error = error;
	}
}
