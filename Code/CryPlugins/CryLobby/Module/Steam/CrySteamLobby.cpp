// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamLobby.cpp
//  Created:     11/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryLobby
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Steam/CrySteamLobby.h"

#if USE_STEAM

#include <../../CryPlugins/CryGamePlatform/Interface/IGamePlatform.h>

////////////////////////////////////////////////////////////////////////////////

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#pragma comment( lib, "steam_api64.lib")
	#elif CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
		#pragma comment( lib, "steam_api.lib")
	#endif

////////////////////////////////////////////////////////////////////////////////

	#if !defined(RELEASE)
static void SteamShowOverlay_DevelopmentOnly(IConsoleCmdArgs* pArgs)
{
	ISteamUtils* pSteamUtils = SteamUtils();
	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamUtils || !pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Overlay cannot be shown; Steam is not initialized");
		return;
	}


	const char* overlay[] = { "Friends", "Community", "Players", "Settings", "OfficialGameGroup", "Stats", "Achievements", "LobbyInvite" };
	uint32 numOverlays = sizeof(overlay) / sizeof(char*);
	uint32 requestedOverlay = 0;

	if (pArgs->GetArgCount() > 1)
	{
		requestedOverlay = atoi(pArgs->GetArg(1));
		if ((requestedOverlay < 0) || (requestedOverlay > numOverlays))
		{
			NetLog("Illegal overlay index specified [%d]:", requestedOverlay);
			for (uint32 index = 0; index < numOverlays; ++index)
			{
				NetLog("  [%d] %s", index, overlay[index]);
			}
			return;
		}
	}

	if (pSteamUtils->IsOverlayEnabled() == false)
	{
		NetLog("[STEAM]: Attempting to show [%s] overlay", overlay[requestedOverlay]);
		pSteamFriends->ActivateGameOverlay(overlay[requestedOverlay]);
	}
	else
	{
		NetLog("[STEAM]: overlay already displayed");
	}
}

////////////////////////////////////////////////////////////////////////////////

static void SteamShowFriends_DevelopmentOnly(IConsoleCmdArgs* pArgs)
{
	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
		return;
	}
	uint32 friendCount = pSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
	NetLog("[STEAM]: Friends list (%d friends):", friendCount);
	for (uint32 index = 0; index < friendCount; ++index)
	{
		CSteamID friendID = pSteamFriends->GetFriendByIndex(index, k_EFriendFlagImmediate);
		NetLog("[STEAM]:  [%d] : [%s] : [%s]", index, CSteamIDAsString(friendID).c_str(), pSteamFriends->GetFriendPersonaName(friendID));
	}
}

////////////////////////////////////////////////////////////////////////////////

static void SteamInviteToGame_DevelopmentOnly(IConsoleCmdArgs* pArgs)
{
	ISteamFriends* pSteamFriends = SteamFriends();
	if (!pSteamFriends)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Steam friends service not available");
		return;
	}
	uint32 friendCount = pSteamFriends->GetFriendCount(k_EFriendFlagImmediate);
	uint32 argCount = pArgs->GetArgCount();

	if (argCount > 1)
	{
		uint32 argIndex = 1;
		CCrySteamLobbyService* pLobbyService = (CCrySteamLobbyService*)gEnv->pLobby->GetLobbyService(eCLS_Online);
		CryLobbySessionHandle lsh = ((CCryMatchMaking*)pLobbyService->GetMatchMaking())->GetCurrentHostedNetNubSessionHandle();

		if (lsh != CryLobbyInvalidSessionHandle)
		{
			while (argIndex < argCount)
			{
				uint32 friendIndex = atoi(pArgs->GetArg(argIndex++));
				if (friendIndex < friendCount)
				{
					SCrySteamUserID* pFriendID = new SCrySteamUserID(pSteamFriends->GetFriendByIndex(friendIndex, k_EFriendFlagImmediate));
					CryUserID id(pFriendID);
					CrySessionHandle sh = ((CCryMatchMaking*)pLobbyService->GetMatchMaking())->CreateGameSessionHandle(lsh, CryMatchMakingInvalidConnectionUID);
					NetLog("[STEAM]: Sending game invite to [%s]...", pSteamFriends->GetFriendPersonaName(pFriendID->m_steamID));
					pLobbyService->GetFriends()->FriendsSendGameInvite(0, sh, &id, 1, NULL, NULL, NULL);
				}
				else
				{
					NetLog("[STEAM]: Invalid friend index [%d] - ignoring... (see steam_show_friends)", friendIndex);
				}
			}
		}
		else
		{
			NetLog("[STEAM]: There doesn't appear to be a hosted nub session...");
		}
	}
	else
	{
		NetLog("[STEAM]: You need to tell me who to invite...");
	}
}

	#endif // !defined(RELEASE)

////////////////////////////////////////////////////////////////////////////////

CCrySteamLobbyService::CCrySteamLobbyService(CCryLobby* pLobby, ECryLobbyService service)
	: CCryLobbyService(pLobby, service)
	#if USE_CRY_MATCHMAKING
	, m_pMatchmaking(NULL)
	#endif // USE_CRY_MATCHMAKING
	#if USE_CRY_STATS
	, m_pStats(NULL)
	#endif // USE_CRY_STATS
	#if USE_CRY_FRIENDS
	, m_pFriends(NULL)
	, m_pFriendsManagement(NULL)
	#endif // USE_CRY_FRIENDS
	, m_pReward(NULL)
{
	#if !defined(RELEASE)
	// Development Steam cvars and console commands
	REGISTER_COMMAND_DEV_ONLY("steam_show_overlay", SteamShowOverlay_DevelopmentOnly, VF_NULL, "Shows the Steam overlay");
	REGISTER_COMMAND_DEV_ONLY("steam_show_friends", SteamShowFriends_DevelopmentOnly, VF_NULL, "Shows your Steam friends list");
	REGISTER_COMMAND_DEV_ONLY("steam_invite_to_game", SteamInviteToGame_DevelopmentOnly, VF_NULL, "Shows your Steam friends list");
	#endif
}

////////////////////////////////////////////////////////////////////////////////

CCrySteamLobbyService::~CCrySteamLobbyService(void)
{
	#if !defined(RELEASE)
	gEnv->pConsole->UnregisterVariable("steam_invite_to_game", true);
	gEnv->pConsole->UnregisterVariable("steam_show_friends", true);
	gEnv->pConsole->UnregisterVariable("steam_show_overlay", true);
	#endif // !defined(RELEASE)
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamLobbyService::Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	ECryLobbyError ret = eCLE_Success;

	if (SteamUser() == NULL)
	{
		Cry::GamePlatform::IPlugin* pPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<Cry::GamePlatform::IPlugin>();
		CRY_ASSERT(pPlugin != nullptr);
		CRY_ASSERT(pPlugin->GetType() == Cry::GamePlatform::EType::Steam);

		if (ret != eCLE_Success)
		{
			NetLog("[STEAM]: SteamAPI_Init() failed");
			return ret;
		}
		NetLog("[STEAM]: SteamAPI_Init() succeeded");
	}

	ret = CCryLobbyService::Initialise(features, pCB);

	#if USE_CRY_MATCHMAKING
	if ((ret == eCLE_Success) && (m_pMatchmaking == NULL) && (features & eCLSO_Matchmaking))
	{
		m_pMatchmaking = new CCrySteamMatchMaking(m_pLobby, this, m_service);

		if (m_pMatchmaking != NULL)
		{
			ret = m_pMatchmaking->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_VOICE && USE_STEAM_VOICE
	if ((ret == eCLE_Success) && (m_pVoice == NULL) && (features & eCLSO_Voice))
	{
		m_pVoice = new CCrySteamVoice(m_pLobby, this);

		if (m_pVoice)
		{
			ret = m_pVoice->Initialise();
		}
		else
		{
			ret = eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_VOICE

	if ((ret == eCLE_Success) && (m_pReward == NULL) && (features & eCLSO_Reward))
	{
		m_pReward = new CCrySteamReward(m_pLobby);
		if (m_pReward)
		{
			ret = m_pReward->Initialise();
		}
		else
		{
			ret = eCLE_OutOfMemory;
		}
	}

	#if USE_CRY_STATS
	if ((ret == eCLE_Success) && (m_pStats == NULL) && (features & eCLSO_Stats))
	{
		m_pStats = new CCrySteamStats(m_pLobby, this);

		if (m_pStats != NULL)
		{
			ret = m_pStats->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_STATS

	#if USE_CRY_FRIENDS
	if ((ret == eCLE_Success) && (m_pFriends == NULL) && (features & eCLSO_Friends))
	{
		m_pFriends = new CCrySteamFriends(m_pLobby, this);

		if (m_pFriends != NULL)
		{
			ret = m_pFriends->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}

	if ((ret == eCLE_Success) && (m_pFriendsManagement == NULL) && (features & eCLSO_Friends))
	{
		m_pFriendsManagement = new CCrySteamFriendsManagement(m_pLobby, this);

		if (m_pFriendsManagement != NULL)
		{
			ret = m_pFriendsManagement->Initialise();
		}
		else
		{
			return eCLE_OutOfMemory;
		}
	}
	#endif // USE_CRY_FRIENDS

	if (ret != eCLE_Success)
	{
		Terminate(features, NULL);
	}

	if (pCB)
	{
		pCB(ret, m_pLobby, m_service);
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamLobbyService::Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB)
{
	if ((SteamUser() != NULL)
	#if USE_CRY_MATCHMAKING
	    || (m_pMatchmaking != NULL)
	#endif // USE_CRY_MATCHMAKING
	#if USE_CRY_STATS
	    || (m_pStats != NULL)
	#endif // USE_CRY_STATS
	#if USE_CRY_FRIENDS
	    || (m_pFriends != NULL)
	    || (m_pFriendsManagement != NULL)
	#endif // USE_CRY_FRIENDS
	#if USE_CRY_VOICE && USE_STEAM_VOICE
	    || (m_pVoice != NULL)
	#endif
	    )
	{
		ECryLobbyError ret = CCryLobbyService::Terminate(features, pCB);

		if ((SteamUser() != NULL) && (features & eCLSO_Base))
		{
			SteamAPI_Shutdown();
			NetLog("[STEAM]: SteamAPI_Shutdown() called");
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

	#if USE_CRY_VOICE && USE_STEAM_VOICE
		if (m_pVoice && (features & eCLSO_Voice))
		{
			ECryLobbyError error = m_pVoice->Terminate();
			m_pVoice = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
	#endif // USE_CRY_VOICE

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

		if (m_pFriendsManagement && (features & eCLSO_Friends))
		{
			ECryLobbyError error = m_pFriendsManagement->Terminate();
			m_pFriendsManagement = NULL;

			if (ret == eCLE_Success)
			{
				ret = error;
			}
		}
	#endif // USE_CRY_FRIENDS

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

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::Tick(CTimeValue tv)
{
	if (m_pLobby->MutexTryLock())
	{
		if (SteamUser() != NULL)
		{
			SteamAPI_RunCallbacks();
		}

	#if USE_CRY_MATCHMAKING
		if (m_pMatchmaking != NULL)
		{
			m_pMatchmaking->Tick(tv);
		}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_STATS
		if (m_pStats != NULL)
		{
			m_pStats->Tick(tv);
		}
	#endif // USE_CRY_STATS

		if (m_pReward)
		{
			m_pReward->Tick(tv);
		}

	#if USE_CRY_VOICE && USE_STEAM_VOICE
		if (m_pVoice)
		{
			m_pVoice->Tick(tv);
		}
	#endif

	#if USE_CRY_FRIENDS
		if (m_pFriends != NULL)
		{
			m_pFriends->Tick(tv);
		}
	#endif // USE_CRY_FRIENDS

		m_pLobby->MutexUnlock();
	}
}

////////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamLobbyService::GetSystemTime(uint32 user, SCrySystemTime* pSystemTime)
{
	memset(pSystemTime, 0, sizeof(SCrySystemTime));
	ECryLobbyError error = eCLE_Success;

	if (SteamUser() != NULL)
	{
		SYSTEMTIME systemTime;
		::GetSystemTime(&systemTime);

		pSystemTime->m_Year = systemTime.wYear;
		pSystemTime->m_Month = static_cast<uint8>(systemTime.wMonth);
		pSystemTime->m_Day = static_cast<uint8>(systemTime.wDay);
		pSystemTime->m_Hour = static_cast<uint8>(systemTime.wHour);
		pSystemTime->m_Minute = static_cast<uint8>(systemTime.wMinute);
		pSystemTime->m_Second = static_cast<uint8>(systemTime.wSecond);
	}
	else
	{
		error = eCLE_InternalError;
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnPacket(addr, pPacket);
	}
	#endif // USE_CRY_MATCHMAKING

	#if USE_CRY_VOICE && USE_STEAM_VOICE
	if (m_pVoice)
	{
		m_pVoice->OnPacket(addr, pPacket);
	}
	#endif // USE_CRY_VOICE
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnError(addr, error, sendID);
	}
	#endif // USE_CRY_MATCHMAKING
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID)
{
	#if USE_CRY_MATCHMAKING
	if (m_pMatchmaking)
	{
		m_pMatchmaking->OnSendComplete(addr, sendID);
	}
	#endif // USE_CRY_MATCHMAKING
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::GetSocketPorts(uint16& connectPort, uint16& listenPort)
{
	connectPort = CLobbyCVars::Get().lobbySteamOnlinePort;
	listenPort = CLobbyCVars::Get().lobbySteamOnlinePort;
}

////////////////////////////////////////////////////////////////////////////////

void CCrySteamLobbyService::InviteAccepted(uint32 user, CrySessionID id)
{
	m_pLobby->InviteAccepted(m_service, user, id, eCLE_Success);
}

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM
