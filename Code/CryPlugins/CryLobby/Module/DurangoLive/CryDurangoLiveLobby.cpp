// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryDurangoLiveLobby.h"
#include <CryCore/TypeInfo_impl.h>

#if USE_DURANGOLIVE

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using namespace Microsoft::WRL;
using namespace ABI::Windows::Xbox::Networking;
using namespace ABI::Windows::Xbox::System;

	#define DURANGO_DEFAULT_PORT           49150

	#define TDN_GET_USER_PRIVILEGES_RESULT 0

ECryLobbyError CryDurangoLiveLobbyGetErrorFromDurangoLive(HRESULT error)
{
	NetLog("[Lobby] eCLE_InternalError: Durango Live Error %d %x", error, error);
	return eCLE_InternalError;
}

CCryDurangoLiveLobbyService::CCryDurangoLiveLobbyService(CCryLobby* pLobby, ECryLobbyService service)
	: CCryLobbyService(pLobby, service)
	, m_titleID(0)
{
}

CCryDurangoLiveLobbyService::~CCryDurangoLiveLobbyService()
{
}

ECryLobbyError CCryDurangoLiveLobbyService::Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	ECryLobbyError ret = CCryLobbyService::Initialise(features, pCB);

	if ((ret == eCLE_Success) && (m_pTCPServiceFactory == NULL) && (features & eCLSO_TCPService))
	{
		m_pTCPServiceFactory = new CCryTCPServiceFactory();

		if (m_pTCPServiceFactory)
		{
			ret = m_pTCPServiceFactory->Initialise("Scripts/Network/TCPServices.xml", CCryTCPServiceFactory::k_defaultMaxSockets);
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	if (features & eCLSO_Base)
	{
		SConfigurationParams neededInfo[2] = {
			{ CLCC_LIVE_TITLE_ID,          { NULL }
			},{ CLCC_LIVE_SERVICE_CONFIG_ID, { NULL }
			}
		};

		m_pLobby->GetConfigurationInformation(neededInfo, CRY_ARRAY_COUNT(neededInfo));

		m_titleID = neededInfo[0].m_32;
		m_serviceConfigId.Set(reinterpret_cast<LPCWSTR>(neededInfo[1].m_pData));
	}

	#if USE_CRY_MATCHMAKING
	if ((ret == eCLE_Success) && (m_pMatchmaking == NULL) && (features & eCLSO_Matchmaking))
	{
		m_pMatchmaking = new CCryDurangoLiveMatchMaking(m_pLobby, this, m_service);

		if (m_pMatchmaking)
		{
			ret = m_pMatchmaking->Initialise();
		}
		else
		{
			ret = eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_ONLINE_STORAGE
	if ((ret == eCLE_Success) && (m_pOnlineStorage == NULL) && (features & eCLSO_OnlineStorage))
	{
		m_pOnlineStorage = new CCryDurangoLiveOnlineStorage(m_pLobby, this);
		if (m_pOnlineStorage)
		{
			ret = m_pOnlineStorage->Initialise();
		}
		else
		{
			ret = eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_ONLINE_STORAGE

	if ((ret == eCLE_Success) && (m_pLobbyUI == NULL) && (features & eCLSO_LobbyUI))
	{
		m_pLobbyUI = new CCryDurangoLiveLobbyUI(m_pLobby, this);

		if (m_pLobbyUI)
		{
			ret = m_pLobbyUI->Initialise();
		}
		else
		{
			ret = eCLE_OutOfMemory;
		}
	}

	if (ret == eCLE_Success)
	{
		if (features & eCLSO_Base)
		{
			m_pLobby->SetServicePacketEnd(m_service, eDurangoLivePT_EndType);
		}
	}
	else
	{
		Terminate(features, NULL);
	}

	if (pCB)
	{
		pCB(ret, m_pLobby, m_service);
	}

	NetLog("[Lobby] Durango Live Initialise error %d", ret);

	return ret;
}

ECryLobbyError CCryDurangoLiveLobbyService::Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	ECryLobbyError ret = CCryLobbyService::Terminate(features, pCB);

	if (m_pTCPServiceFactory && (features & eCLSO_TCPService))
	{
		ECryLobbyError error = m_pTCPServiceFactory->Terminate(false);
		m_pTCPServiceFactory = NULL;

		if (ret == eCLE_Success)
		{
			ret = error;
		}
	}

	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking && (features & eCLSO_Matchmaking))
	{
		ECryLobbyError error = m_pMatchmaking->Terminate();
		m_pMatchmaking = NULL;

		if (ret == eCLE_Success)
		{
			ret = error;
		}
	}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_ONLINE_STORAGE
	if (m_pOnlineStorage && (features & eCLSO_OnlineStorage))
	{
		ECryLobbyError error = m_pOnlineStorage->Terminate();
		m_pOnlineStorage = NULL;

		if (ret == eCLE_Success)
		{
			ret = error;
		}
	}
	#endif // USE_CRY_ONLINE_STORAGE

	if (m_pLobbyUI && (features & eCLSO_LobbyUI))
	{
		ECryLobbyError error = m_pLobbyUI->Terminate();
		m_pLobbyUI = NULL;

		if (ret == eCLE_Success)
		{
			ret = error;
		}
	}

	if (pCB)
	{
		pCB(ret, m_pLobby, m_service);
	}

	return ret;
}

void CCryDurangoLiveLobbyService::Tick(CTimeValue tv)
{
	if (m_pLobby->MutexTryLock())
	{
	#if USE_CRY_MATCHMAKING
		if (m_pMatchmaking)
		{
			m_pMatchmaking->Tick(tv);
		}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_ONLINE_STORAGE
		if (m_pOnlineStorage)
		{
			m_pOnlineStorage->Tick(tv);
		}
	#endif // USE_CRY_ONLINE_STORAGE

		if (m_pLobbyUI)
		{
			m_pLobbyUI->Tick(tv);
		}

		m_pLobby->MutexUnlock();
	}

	if (m_pTCPServiceFactory)
	{
		m_pTCPServiceFactory->Tick(tv);
	}
}

void CCryDurangoLiveLobbyService::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnPacket(addr, pPacket);
	}
	#endif // USE_CRY_MATCHMAKING
}

void CCryDurangoLiveLobbyService::OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnError(addr, error, sendID);
	}
	#endif // USE_CRY_MATCHMAKING
}

void CCryDurangoLiveLobbyService::OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnSendComplete(addr, sendID);
	}
	#endif // USE_CRY_MATCHMAKING
}

CryUserID CCryDurangoLiveLobbyService::GetUserID(uint32 user)
{
	return CryUserInvalidID;
}

void CCryDurangoLiveLobbyService::StartTaskRunning(CryLobbyServiceTaskID lsTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = GetTask(lsTaskID);

	if (pTask->used)
	{
		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_GetUserPrivileges:
			StartGetUserPrivileges(lsTaskID);
			break;
		}
	}
}

void CCryDurangoLiveLobbyService::StopTaskRunning(CryLobbyServiceTaskID lsTaskID)
{
	STask* pTask = GetTask(lsTaskID);

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCryDurangoLiveLobbyService::EndTask, this, lsTaskID);
	}
}

void CCryDurangoLiveLobbyService::EndTask(CryLobbyServiceTaskID lsTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = GetTask(lsTaskID);

	if (pTask->used)
	{
		if (pTask->pCB)
		{
			switch (pTask->startedTask)
			{
			case eT_GetUserPrivileges:
				((CryLobbyPrivilegeCallback)pTask->pCB)(pTask->lTaskID, pTask->error, pTask->dataNum[TDN_GET_USER_PRIVILEGES_RESULT], pTask->pCBArg);
				break;
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Lobby] Lobby Service EndTask %d Result %d", pTask->startedTask, pTask->error);
		}

		FreeTask(lsTaskID);
	}
}

ECryLobbyError CCryDurangoLiveLobbyService::GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg)
{
	ECryLobbyError error;
	CryLobbyServiceTaskID tid;

	LOBBY_AUTO_LOCK;

	error = StartTask(eT_GetUserPrivileges, user, false, &tid, pTaskID, (void*)pCB, pCBArg);

	if (error == eCLE_Success)
	{
		FROM_GAME_TO_LOBBY(&CCryDurangoLiveLobbyService::StartTaskRunning, this, tid);
	}

	return error;
}

void CCryDurangoLiveLobbyService::StartGetUserPrivileges(CryLobbyServiceTaskID lsTaskID)
{
	STask* pTask = GetTask(lsTaskID);
	uint32 result = 0;

	if (m_pLobby->GetInternalSocket(m_service))
	{
	}
	else
	{
		UpdateTaskError(lsTaskID, eCLE_InternalError);
	}

	pTask->dataNum[TDN_GET_USER_PRIVILEGES_RESULT] = result;

	StopTaskRunning(lsTaskID);
}

ECryLobbyError CCryDurangoLiveLobbyService::GetSystemTime(uint32 user, SCrySystemTime* pSystemTime)
{
	memset(pSystemTime, 0, sizeof(SCrySystemTime));

	ECryLobbyError error = eCLE_Success;
	SYSTEMTIME systemTime;
	::GetSystemTime(&systemTime);

	pSystemTime->m_Year = systemTime.wYear;
	pSystemTime->m_Month = static_cast<uint8>(systemTime.wMonth);
	pSystemTime->m_Day = static_cast<uint8>(systemTime.wDay);
	pSystemTime->m_Hour = static_cast<uint8>(systemTime.wHour);
	pSystemTime->m_Minute = static_cast<uint8>(systemTime.wMinute);
	pSystemTime->m_Second = static_cast<uint8>(systemTime.wSecond);

	return error;
}

void CCryDurangoLiveLobbyService::GetSocketPorts(uint16& connectPort, uint16& listenPort)
{
	connectPort = DURANGO_DEFAULT_PORT;
	listenPort = DURANGO_DEFAULT_PORT;
}

#endif // USE_DURANGOLIVE
