// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if CRY_PLATFORM_ORBIS

	#include "CryPSN2Lobby.h"

	#if USE_PSN
		#include <net.h>
		#include <libnetctl.h>

void CCryPSNLobbyService::SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg)
{
	CCryPSNLobbyService* _this = static_cast<CCryPSNLobbyService*>(pArg);

	if (ecb == eCE_ErrorEvent)
	{
		for (uint32 i = 0; i < MAX_LOBBY_TASKS; i++)
		{
			STask* pTask = _this->GetTask(i);

			if (pTask->used && pTask->running)
			{
				_this->UpdateTaskError(i, MapSupportErrorToLobbyError(data.m_errorEvent.m_error));
				_this->StopTaskRunning(i);
			}
		}
	}
}

CCryPSNLobbyService::CCryPSNLobbyService(CCryLobby* pLobby, ECryLobbyService service) : CCryLobbyService(pLobby, service)
{
		#if USE_PSN_VOICE
	m_pVoice = NULL;
		#endif//USE_PSN_VOICE
	m_pLobbySupport = NULL;
		#if USE_CRY_MATCHMAKING
	m_pMatchmaking = NULL;
		#endif // USE_CRY_MATCHMAKING
	m_NPInited = false;
		#if USE_CRY_TCPSERVICE
	m_pTCPServiceFactory = NULL;
		#endif // USE_CRY_TCPSERVICE
		#if USE_CRY_STATS
	m_pStats = NULL;
		#endif // USE_CRY_STATS
	m_pLobbyUI = NULL;
		#if USE_CRY_FRIENDS
	m_pFriends = NULL;
		#endif // USE_CRY_FRIENDS
	m_pReward = NULL;
}

ECryLobbyError CCryPSNLobbyService::Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	ECryLobbyError error = CCryLobbyService::Initialise(features, pCB);

	if (!m_NPInited)
	{
		int ret;

		ret = sceNetCtlInit();
		if ((ret < PSN_OK) && (ret != SCE_NET_CTL_ERROR_NOT_INITIALIZED))
		{
			NetLog("sceNetCtlInit() failed. ret = 0x%x\n", ret);
			return eCLE_InternalError;
		}

		SConfigurationParams titleInfo[3] = {
			{ CLCC_PSN_TITLE_ID,     { NULL }
			},{ CLCC_PSN_TITLE_SECRET, { NULL }
			},{ CLCC_PSN_AGE_LIMIT,    { NULL }
			}
		};
		GetConfigurationInformation(titleInfo, 3);

		#if !USE_NPTITLE_DAT
		ret = sceNpSetNpTitleId((const SceNpTitleId*)titleInfo[0].m_pData, (const SceNpTitleSecret*)titleInfo[1].m_pData);
		if (ret < PSN_OK)
		{
			NetLog("sceNpSetNpTitleId() failed. ret = 0x%x\n", ret);
			return eCLE_InternalError;
		}
		#endif // !USE_NPTITLE_DAT

		SCryLobbyAgeRestrictionSetup* pAgeSetup = (SCryLobbyAgeRestrictionSetup*)titleInfo[2].m_pData;
		SceNpAgeRestriction* pAges = NULL;

		SceNpContentRestriction contentRestriction;
		memset(&contentRestriction, 0, sizeof(contentRestriction));
		contentRestriction.size = sizeof(contentRestriction);
		contentRestriction.defaultAgeRestriction = SCE_NP_NO_AGE_RESTRICTION;

		if (pAgeSetup && pAgeSetup->m_numCountries && pAgeSetup->m_pCountries)
		{
			pAges = new SceNpAgeRestriction[pAgeSetup->m_numCountries];
			if (pAges)
			{
				memset(pAges, 0, pAgeSetup->m_numCountries * sizeof(SceNpAgeRestriction));

				for (int i = 0; i < pAgeSetup->m_numCountries; ++i)
				{
					strncpy(pAges[i].countryCode.data, pAgeSetup->m_pCountries[i].id, 2);
					pAges[i].age = pAgeSetup->m_pCountries[i].age;
				}

				contentRestriction.ageRestrictionCount = pAgeSetup->m_numCountries;
				contentRestriction.ageRestriction = pAges;
			}
		}

		ret = sceNpSetContentRestriction(&contentRestriction);
		if (ret < PSN_OK)
		{
			NetLog("sceNpSetContentRestriction() failed. ret = 0x%x\n", ret);
			SAFE_DELETE_ARRAY(pAges);
			return eCLE_InternalError;
		}

		SAFE_DELETE_ARRAY(pAges);

		m_NPInited = true;
	}

		#if USE_CRY_TCPSERVICE
	if ((m_pTCPServiceFactory == NULL) && (features & eCLSO_TCPService))
	{
		m_pTCPServiceFactory = new CCryTCPServiceFactory();

		if (m_pTCPServiceFactory)
		{
			error = m_pTCPServiceFactory->Initialise("Scripts/Network/TCPServices.xml", CCryTCPServiceFactory::k_defaultMaxSockets);
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
		#endif // USE_CRY_TCPSERVICE

	if ((error == eCLE_Success) && (m_pLobbySupport == NULL) && (features & eCLSO_Base))
	{
		m_pLobbySupport = new CCryPSNSupport(m_pLobby, this, features);
		if (m_pLobbySupport)
		{
			error = m_pLobbySupport->Initialise();
			if (!m_pLobbySupport->RegisterEventHandler(eCE_ErrorEvent, CCryPSNLobbyService::SupportCallback, this))
			{
				return eCLE_InternalError;
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

		#if USE_PSN_VOICE
	if ((error == eCLE_Success) && (m_pVoice == NULL) && (features & eCLSO_Voice))
	{
		m_pVoice = new CCryPSNVoice(m_pLobby, this);

		if (m_pVoice)
		{
			error = m_pVoice->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
		#endif

		#if USE_CRY_MATCHMAKING
	if ((error == eCLE_Success) && (m_pMatchmaking == NULL) && (features & eCLSO_Matchmaking))
	{
		m_pMatchmaking = new CCryPSNMatchMaking(m_pLobby, this, m_pLobbySupport, m_service);

		if (m_pMatchmaking)
		{
			error = m_pMatchmaking->Initialise();
			if (!m_pLobbySupport->RegisterEventHandler(eCE_ErrorEvent | eCE_RequestEvent | eCE_RoomEvent | eCE_SignalEvent | eCE_WebApiEvent, CCryPSNMatchMaking::SupportCallback, m_pMatchmaking))
			{
				return eCLE_InternalError;
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
		#endif // USE_CRY_MATCHMAKING

		#if USE_CRY_STATS
	if ((error == eCLE_Success) && (m_pStats == NULL) && (features & eCLSO_Stats))
	{
		m_pStats = new CCryPSNStats(m_pLobby, this, m_pLobbySupport);

		if (m_pStats)
		{
			error = m_pStats->Initialise();
			if (!m_pLobbySupport->RegisterEventHandler(eCE_ErrorEvent, CCryPSNStats::SupportCallback, m_pStats))
			{
				return eCLE_InternalError;
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
		#endif // USE_CRY_STATS

	if ((error == eCLE_Success) && (m_pLobbyUI == NULL) && (features & eCLSO_LobbyUI))
	{
		m_pLobbyUI = new CCryPSNLobbyUI(m_pLobby, this, m_pLobbySupport);

		if (m_pLobbyUI)
		{
			error = m_pLobbyUI->Initialise();
			if (!m_pLobbySupport->RegisterEventHandler(eCE_ErrorEvent | eCE_WebApiEvent, CCryPSNLobbyUI::SupportCallback, m_pLobbyUI))
			{
				return eCLE_InternalError;
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

		#if USE_CRY_FRIENDS
	if ((error == eCLE_Success) && (m_pFriends == NULL) && (features & eCLSO_Friends))
	{
		m_pFriends = new CCryPSNFriends(m_pLobby, this, m_pLobbySupport);

		if (m_pFriends)
		{
			error = m_pFriends->Initialise();
			if (!m_pLobbySupport->RegisterEventHandler(eCE_ErrorEvent | eCE_WebApiEvent, CCryPSNFriends::SupportCallback, m_pFriends))
			{
				return eCLE_InternalError;
			}
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
		#endif // USE_CRY_FRIENDS

	if ((error == eCLE_Success) && (m_pReward == NULL) && (features & eCLSO_Reward))
	{
		m_pReward = new CCryPSNReward(m_pLobby);
		if (m_pReward)
		{
			error = m_pReward->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	if (error == eCLE_Success)
	{
		if (features & eCLSO_Base)
		{
			m_pLobby->SetServicePacketEnd(m_service, ePSNPT_EndType);
		}
	}
	else
	{
		Terminate(features, NULL);
	}

	if (pCB)
	{
		pCB(error, m_pLobby, m_service);
	}

	return error;
}

ECryLobbyError CCryPSNLobbyService::Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	if (
		#if USE_CRY_MATCHMAKING
	  m_pMatchmaking ||
		#endif // USE_CRY_MATCHMAKING
		#if USE_CRY_TCPSERVICE
	  m_pTCPServiceFactory ||
		#endif // USE_CRY_TCPSERVICE
	  m_pLobbySupport ||
		#if USE_CRY_STATS
	  m_pStats ||
		#endif // USE_CRY_STATS
	  m_pLobbyUI ||
		#if USE_CRY_FRIENDS
	  m_pFriends ||
		#endif // USE_CRY_FRIENDS
	  m_pReward ||
		#if USE_PSN_VOICE
	  m_pVoice ||
		#endif
	  0
	  )
	{
		ECryLobbyError ret = CCryLobbyService::Terminate(features, pCB);

		#if USE_CRY_FRIENDS
		if (m_pFriends && (features & eCLSO_Friends))
		{
			ECryLobbyError error = m_pFriends->Terminate();
			m_pFriends = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
		#endif // USE_CRY_FRIENDS

		if (m_pLobbyUI && (features & eCLSO_LobbyUI))
		{
			ECryLobbyError error = m_pLobbyUI->Terminate();
			m_pLobbyUI = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}

		#if USE_CRY_STATS
		if (m_pStats && (features & eCLSO_Stats))
		{
			ECryLobbyError error = m_pStats->Terminate();
			m_pStats = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
		#endif // USE_CRY_STATS

		#if USE_CRY_TCPSERVICE
		if (m_pTCPServiceFactory && (features & eCLSO_TCPService))
		{
			ECryLobbyError error = m_pTCPServiceFactory->Terminate(false);
			m_pTCPServiceFactory = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
		#endif // USE_CRY_TCPSERVICE

		#if USE_PSN_VOICE
		if (m_pVoice && (features & eCLSO_Voice))
		{
			ECryLobbyError error = m_pVoice->Terminate();
			m_pVoice = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
		#endif

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

		if (m_pReward && (features & eCLSO_Reward))
		{
			ECryLobbyError error = m_pReward->Terminate();
			m_pReward = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}

		if (m_pLobbySupport && (features & eCLSO_Base))
		{
			ECryLobbyError error = m_pLobbySupport->Terminate();
			m_pLobbySupport = NULL;

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
	else
	{
		return eCLE_NotInitialised;
	}
}

void CCryPSNLobbyService::Tick(CTimeValue tv)
{
	if (m_pLobby->MutexTryLock())
	{
		if (m_pLobbySupport)
		{
			m_pLobbySupport->Tick();
		}

		for (uint32 i = 0; i < MAX_LOBBY_TASKS; i++)
		{
			STask* pTask = GetTask(i);

			if (pTask->used && pTask->running)
			{
				if (pTask->error == eCLE_Success)
				{
					switch (pTask->startedTask)
					{
					case eT_GetUserPrivileges:
						TickGetUserPrivileges(i);
						break;
					}
				}
			}
		}

		#if USE_CRY_MATCHMAKING
		if (m_pMatchmaking)
		{
			m_pMatchmaking->Tick(tv);
		}
		#endif // USE_CRY_MATCHMAKING

		#if USE_PSN_VOICE
		if (m_pVoice)
		{
			m_pVoice->Tick(tv);
		}
		#endif

		#if USE_CRY_STATS
		if (m_pStats)
		{
			m_pStats->Tick(tv);
		}
		#endif // USE_CRY_STATS

		if (m_pReward)
		{
			m_pReward->Tick(tv);
		}

		if (m_pLobbyUI)
		{
			m_pLobbyUI->Tick(tv);
		}

		m_pLobby->MutexUnlock();
	}

		#if USE_CRY_TCPSERVICE
	if (m_pTCPServiceFactory)
	{
		m_pTCPServiceFactory->Tick(tv);
	}
		#endif // USE_CRY_TCPSERVICE
}

void CCryPSNLobbyService::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
		#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnPacket(addr, pPacket);
	}
		#endif // USE_CRY_MATCHMAKING

		#if USE_PSN_VOICE
	if (m_pVoice)
	{
		m_pVoice->OnPacket(addr, pPacket);
	}
		#endif // USE_PSN_VOICE
}

void CCryPSNLobbyService::OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID)
{
		#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnError(addr, error, sendID);
	}
		#endif // USE_CRY_MATCHMAKING
}

void CCryPSNLobbyService::OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID)
{
		#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnSendComplete(addr, sendID);
	}
		#endif // USE_CRY_MATCHMAKING
}

void CCryPSNLobbyService::GetConfigurationInformation(SConfigurationParams* infos, uint32 cnt)
{
	m_pLobby->GetConfigurationInformation(infos, cnt);
}

CryUserID CCryPSNLobbyService::GetUserID(uint32 user)
{
	if (m_pLobbySupport && (m_pLobbySupport->GetOnlineState() >= ePSNOS_Online))
	{
		//-- the npId should be valid
		SCryPSNUserID* pID = new SCryPSNUserID;
		if (pID)
		{
			pID->npId = *(m_pLobbySupport->GetLocalNpId());
			return pID;
		}
	}

	return CryUserInvalidID;
}

void CCryPSNLobbyService::StartTaskRunning(CryLobbyServiceTaskID lsTaskID)
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

void CCryPSNLobbyService::StopTaskRunning(CryLobbyServiceTaskID lsTaskID)
{
	STask* pTask = GetTask(lsTaskID);

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCryPSNLobbyService::EndTask, this, lsTaskID);
	}
}

void CCryPSNLobbyService::EndTask(CryLobbyServiceTaskID lsTaskID)
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
				{
					uint32 result = 0;
					if (!m_pLobbySupport || (m_pLobbySupport->GetOnlineState() < ePSNOS_Online) || (pTask->error == eCLE_InsufficientPrivileges) || (pTask->error == eCLE_AgeRestricted))
					{
						result |= CLPF_BlockMultiplayerSessons;
					}

					((CryLobbyPrivilegeCallback)pTask->pCB)(pTask->lTaskID, pTask->error, result, pTask->pCBArg);
				}
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

ECryLobbyError CCryPSNLobbyService::GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg)
{
	ECryLobbyError error;
	CryLobbyServiceTaskID tid;

	LOBBY_AUTO_LOCK;

	error = StartTask(eT_GetUserPrivileges, user, false, &tid, pTaskID, (void*)pCB, pCBArg);

	if (error == eCLE_Success)
	{
		FROM_GAME_TO_LOBBY(&CCryPSNLobbyService::StartTaskRunning, this, tid);
	}

	return error;
}

ECryLobbyError CCryPSNLobbyService::GetSystemTime(uint32 user, SCrySystemTime* pSystemTime)
{
	SceRtcTick tick;
	ECryLobbyError error = eCLE_Success;

	memset(pSystemTime, 0, sizeof(SCrySystemTime));

	// There's no Network Clock as far as I can see. We'll have to use the system clock for now, which isn't as accurate.
	int rc = sceRtcGetCurrentTick(&tick);
	switch (rc)
	{
	case 0:
		// success
		break;
	case SCE_NP_ERROR_NOT_INITIALIZED:
		error = eCLE_NotInitialised;
		break;
	case SCE_NP_ERROR_INVALID_ARGUMENT:
		error = eCLE_InvalidParam;
		break;
	default:
		error = eCLE_InternalError;
		break;
	}

	if (error == eCLE_Success)
	{
		SceRtcDateTime time;
		if (sceRtcSetTick(&time, &tick) == SCE_OK)
		{
			pSystemTime->m_Year = time.year;
			pSystemTime->m_Month = static_cast<uint8>(time.month);
			pSystemTime->m_Day = static_cast<uint8>(time.day);
			pSystemTime->m_Hour = static_cast<uint8>(time.hour);
			pSystemTime->m_Minute = static_cast<uint8>(time.minute);
			pSystemTime->m_Second = static_cast<uint8>(time.second);
		}
		else
		{
			error = eCLE_InternalError;
		}
	}

	return error;
}

void CCryPSNLobbyService::StartGetUserPrivileges(CryLobbyServiceTaskID lsTaskID)
{
	if (m_pLobbySupport && m_pLobby->GetInternalSocket(m_service))
	{
		// change this to ePSNOS_Matchmaking_Available if you want GetUserPrivileges to include PS+ test, otherwise it will test user age/PSN account privileges only
		m_pLobbySupport->ResumeTransitioning(ePSNOS_Online);
	}
	else
	{
		UpdateTaskError(lsTaskID, eCLE_InternalError);
		StopTaskRunning(lsTaskID);
	}
}

void CCryPSNLobbyService::TickGetUserPrivileges(CryLobbyServiceTaskID lsTaskID)
{
	// change this to ePSNOS_Matchmaking_Available if you want GetUserPrivileges to include PS+ test, otherwise it will test user age/PSN account privileges only
	m_pLobbySupport->ResumeTransitioning(ePSNOS_Online);
	if (m_pLobbySupport->HasTransitioningReachedState(ePSNOS_Online))
	{
		StopTaskRunning(lsTaskID);
	}
}

void CCryPSNLobbyService::GetSocketPorts(uint16& connectPort, uint16& listenPort)
{
	connectPort = SCE_NP_PORT;
	listenPort = SCE_NP_PORT;
}

		#if defined(WARFACE_CONSOLE_VERSION)
			#if defined(GUID_STYLE_DB)

CryUserID CCryPSNLobbyService::GetUserIDFromGUID(PlatformGUID64 xuid)
{
	SCryPSNUserID* pID = new SCryPSNUserID;

	if (pID)
	{
		pID->guid = xuid;
		return pID;
	}

	return CryUserInvalidID;
}

PlatformGUID64 CCryPSNLobbyService::GetGUIDFromUserID(CryUserID id)
{
	return ((SCryPSNUserID*)id.get())->guid;
}

			#endif
		#endif

void CCryPSNLobbyService::MakeAddrPCCompatible(TNetAddress& addr)
{
	//SIPv4Addr* pIPv4Addr = addr.GetPtr<SIPv4Addr>();

	//if (pIPv4Addr)
	//{
	//	pIPv4Addr->lobbyService = eCLS_LAN;
	//}
}

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS
