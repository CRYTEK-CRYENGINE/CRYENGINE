// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CrySignIn.cpp
//  Version:     v1.00
//  Created:     14/11/2011 by Paul Mikell.
//  Description: CCrySignIn member definitions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CrySignIn.h"

CCrySignIn::CCrySignIn(CCryLobby* pLobby, CCryLobbyService* pService)
	: m_pLobby(pLobby)
	, m_pService(pService)
{
}

CCrySignIn::~CCrySignIn()
{
}

void CCrySignIn::CancelTask(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (CrySignInTaskID siTaskID = 0; siTaskID.IsValid(); ++siTaskID)
		{
			STask* pTask = m_pTask[siTaskID];

			CRY_ASSERT_MESSAGE(pTask, "CCrySignIn: Task base pointers not setup");

			if (pTask && pTask->used && (pTask->lTaskID == lTaskID))
			{
				NetLog("[Lobby] SignIn task %u cancelled", lTaskID);
				pTask->pCb = NULL;
				pTask->cancelled = true;
				break;
			}
		}
	}
}

ECryLobbyError CCrySignIn::Initialise()
{
	for (CrySignInTaskID siTaskID = 0; siTaskID.IsValid(); ++siTaskID)
	{
		ClearTask(siTaskID);
	}

	return eCLE_Success;
}

ECryLobbyError CCrySignIn::Terminate()
{
	for (CrySignInTaskID siTaskID = 0; siTaskID.IsValid(); ++siTaskID)
	{
		FreeTask(siTaskID);
	}

	return eCLE_Success;
}

void CCrySignIn::Tick(CTimeValue tv)
{
}

void CCrySignIn::StartTaskRunning(CrySignInTaskID siTaskID)
{
	STask* pTask = m_pTask[siTaskID];

	if (pTask->used)
	{
		pTask->running = true;
	}
}

void CCrySignIn::StopTaskRunning(CrySignInTaskID siTaskID)
{
	STask* pTask = m_pTask[siTaskID];

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCrySignIn::EndTask, this, siTaskID);
	}
}

void CCrySignIn::EndTask(CrySignInTaskID siTaskID)
{
	LOBBY_AUTO_LOCK;

	FreeTask(siTaskID);
}

ECryLobbyError CCrySignIn::StartTask(uint32 eTask, CrySignInTaskID* pSITaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg)
{
	ECryLobbyError result = eCLE_TooManyTasks;
	CryLobbyTaskID lTaskID = m_pLobby->CreateTask();

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (CrySignInTaskID siTaskID = 0; siTaskID < MAX_SIGNIN_TASKS; ++siTaskID)
		{
			STask* pTask = m_pTask[siTaskID];

			CRY_ASSERT_MESSAGE(pTask, "CCrySignIn: Task base pointers not setup");

			if (!pTask->used)
			{
				CreateTask(siTaskID, lTaskID, eTask, pCb, pCbArg);

				if (pSITaskID)
				{
					*pSITaskID = siTaskID;
				}

				if (pLTaskID)
				{
					*pLTaskID = lTaskID;
				}

				result = eCLE_Success;
			}
		}

		if (result != eCLE_Success)
		{
			m_pLobby->ReleaseTask(lTaskID);
		}
	}

	return result;
}

void CCrySignIn::FreeTask(CrySignInTaskID siTaskID)
{
	STask* pTask = m_pTask[siTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCrySignIn: Task base pointers not setup");

	if (pTask && pTask->used)
	{
		for (uint32 i = 0; i < MAX_SIGNIN_PARAMS; ++i)
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
}

ECryLobbyError CCrySignIn::CreateTaskParamMem(CrySignInTaskID siTaskID, uint32 param, const void* pParamData, size_t paramDataSize)
{
	ECryLobbyError result;
	STask* pTask = m_pTask[siTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCrySignIn: Task base pointers not setup");

	if (paramDataSize > 0)
	{
		pTask->paramsMem[param] = m_pLobby->MemAlloc(paramDataSize);

		if (pTask->paramsMem[param] != TMemInvalidHdl)
		{
			if (pParamData)
			{
				memcpy(m_pLobby->MemGetPtr(pTask->paramsMem[param]), pParamData, paramDataSize);
			}

			result = eCLE_Success;
		}
		else
		{
			result = eCLE_OutOfMemory;
		}
	}
	else
	{
		result = eCLE_InvalidParam;
	}

	return result;
}

void CCrySignIn::UpdateTaskError(CrySignInTaskID siTaskID, ECryLobbyError error)
{
	STask* pTask = m_pTask[siTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCrySignIn: Task base pointers not setup");

	if (pTask->error == eCLE_Success)
	{
		pTask->error = error;
	}
}

void CCrySignIn::CreateTask(CrySignInTaskID siTaskID, CryLobbyTaskID lTaskID, uint32 eTask, void* pCb, void* pCbArg)
{
	STask* pTask = m_pTask[siTaskID];

	pTask->lTaskID = lTaskID;
	pTask->error = eCLE_Success;
	pTask->startedTask = eTask;
	pTask->subTask = eTask;
	pTask->pCb = pCb;
	pTask->pCbArg = pCbArg;
	pTask->used = true;
	pTask->running = false;
	pTask->cancelled = false;

	for (uint32 j = 0; j < MAX_SIGNIN_PARAMS; ++j)
	{
		pTask->paramsMem[j] = TMemInvalidHdl;
		pTask->paramsNum[j] = 0;
	}
}

void CCrySignIn::ClearTask(CrySignInTaskID siTaskID)
{
	STask* pTask = m_pTask[siTaskID];

	pTask->lTaskID = CryLobbyInvalidTaskID;
	pTask->error = eCLE_Success;
	pTask->startedTask = eT_Invalid;
	pTask->subTask = eT_Invalid;
	pTask->pCb = NULL;
	pTask->pCbArg = NULL;
	pTask->used = false;
	pTask->running = false;
	pTask->cancelled = false;

	for (uint32 j = 0; j < MAX_SIGNIN_PARAMS; ++j)
	{
		pTask->paramsMem[j] = TMemInvalidHdl;
		pTask->paramsNum[j] = 0;
	}
}
