// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Cryptography/rijndael.h"

#include "CryStats.h"

#if USE_CRY_STATS

CCryStats::CCryStats(CCryLobby* pLobby, CCryLobbyService* pService)
{
	CRY_ASSERT_MESSAGE(pLobby, "CCryStats::CCryStats: Lobby not specified");
	CRY_ASSERT_MESSAGE(pService, "CCryStats::CCryStats: Service not specified");

	m_pLobby = pLobby;
	m_pService = pService;

	for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
	{
		m_pTask[i] = NULL;
	}
}

ECryLobbyError CCryStats::Initialise()
{
	for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryStats: Task base pointers not setup");
		m_pTask[i]->used = false;
	}

	return eCLE_Success;
}

ECryLobbyError CCryStats::Terminate()
{
	for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
	{
		CRY_ASSERT_MESSAGE(m_pTask[i], "CCryStats: Task base pointers not setup");

		if (m_pTask[i]->used)
		{
			FreeTask(i);
		}
	}

	return eCLE_Success;
}

ECryLobbyError CCryStats::StartTask(uint32 eTask, bool startRunning, CryStatsTaskID* pSTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryLobbyTaskID lobbyTaskID = m_pLobby->CreateTask();

	if (lobbyTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryStats: Task base pointers not setup");

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

				for (uint32 j = 0; j < MAX_STATS_PARAMS; j++)
				{
					pTask->paramsMem[j] = TMemInvalidHdl;
					pTask->paramsNum[j] = 0;
				}

				if (pSTaskID)
				{
					*pSTaskID = i;
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

void CCryStats::FreeTask(CryStatsTaskID sTaskID)
{
	STask* pTask = m_pTask[sTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryStats: Task base pointers not setup");

	for (uint32 i = 0; i < MAX_STATS_PARAMS; i++)
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

void CCryStats::CancelTask(CryLobbyTaskID lTaskID)
{
	LOBBY_AUTO_LOCK;

	NetLog("[Lobby]Try cancel task %u", lTaskID);

	if (lTaskID != CryLobbyInvalidTaskID)
	{
		for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
		{
			STask* pTask = m_pTask[i];

			CRY_ASSERT_MESSAGE(pTask, "CCryStats: Task base pointers not setup");

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

ECryLobbyError CCryStats::CreateTaskParamMem(CryStatsTaskID sTaskID, uint32 param, const void* pParamData, size_t paramDataSize)
{
	STask* pTask = m_pTask[sTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryStats: Task base pointers not setup");

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

void CCryStats::UpdateTaskError(CryStatsTaskID sTaskID, ECryLobbyError error)
{
	STask* pTask = m_pTask[sTaskID];

	CRY_ASSERT_MESSAGE(pTask, "CCryStats: Task base pointers not setup");

	if (pTask->error == eCLE_Success)
	{
		pTask->error = error;
	}
}

ECryLobbyError CCryStats::EncryptUserData(uint32 nBufferSize, const uint8* pUnencrypted, uint8* pEncrypted)
{
	if (gEnv->pNetwork)
	{
		//-- get key
		SConfigurationParams param = {
			CLCC_CRYSTATS_ENCRYPTION_KEY, { 0 }
		};
		m_pLobby->GetConfigurationInformation(&param, 1);

		uint32 sizeEncrypted = gEnv->pNetwork->RijndaelEncryptBuffer(pEncrypted, pUnencrypted, nBufferSize, (uint8*)param.m_pData, 32);

		NetLog("[Stats] Unencrypted userdata: First few bytes:%02x%02x%02x%02x - %u bytes total",
		       pUnencrypted[0],
		       pUnencrypted[1],
		       pUnencrypted[2],
		       pUnencrypted[3],
		       nBufferSize
		       );
		NetLog("[Stats] Encrypted userdata: First few bytes:%02x%02x%02x%02x - %u bytes total",
		       pEncrypted[0],
		       pEncrypted[1],
		       pEncrypted[2],
		       pEncrypted[3],
		       sizeEncrypted
		       );

		if (sizeEncrypted == nBufferSize)
		{
			return eCLE_Success;
		}
		else
		{
			NetLog("[Stats] Encryption size mismatch; expected %u, got %u bytes.", nBufferSize, sizeEncrypted);
			return eCLE_InternalError;
		}
	}
	else
	{
		return eCLE_InternalError;
	}
}

ECryLobbyError CCryStats::DecryptUserData(uint32 nBufferSize, const uint8* pEncrypted, uint8* pDecrypted)
{
	//-- get key
	SConfigurationParams param = {
		CLCC_CRYSTATS_ENCRYPTION_KEY, { 0 }
	};
	m_pLobby->GetConfigurationInformation(&param, 1);

	uint32 sizeDecrypted = gEnv->pNetwork->RijndaelDecryptBuffer(pDecrypted, pEncrypted, nBufferSize, (uint8*)param.m_pData, 32);

	NetLog("[Stats] Encrypted userdata: First few bytes:%02x%02x%02x%02x - %u bytes total",
	       pEncrypted[0],
	       pEncrypted[1],
	       pEncrypted[2],
	       pEncrypted[3],
	       nBufferSize
	       );
	NetLog("[Stats] Decrypted userdata: First few bytes:%02x%02x%02x%02x - %u bytes total",
	       pDecrypted[0],
	       pDecrypted[1],
	       pDecrypted[2],
	       pDecrypted[3],
	       sizeDecrypted
	       );

	if (sizeDecrypted == nBufferSize)
	{
		return eCLE_Success;
	}
	else
	{
		NetLog("[Stats] Decryption size mismatch; expected %u, got %u bytes.", nBufferSize, sizeDecrypted);
		return eCLE_InternalError;
	}
}

#endif // USE_CRY_STATS
