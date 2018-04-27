// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamLobby.h
//  Created:     11/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryLobby
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSTEAMLOBBY_H__
#define __CRYSTEAMLOBBY_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryLobby.h"

#if USE_STEAM

////////////////////////////////////////////////////////////////////////////////

	#include "CrySteamFriends.h"
	#include "CrySteamFriendsManagement.h"
	#include "CrySteamMatchMaking.h"
	#include "CrySteamStats.h"
	#include "CrySteamReward.h"
	#include "CrySteamVoice.h"

////////////////////////////////////////////////////////////////////////////////

enum CrySteamLobbyPacketType
{
	eSteamPT_SessionRequestJoinResult = eLobbyPT_SessionRequestJoinResult,

	eSteamPT_SessionRequestJoin       = CRYONLINELOBBY_PACKET_START,
	eSteamPT_SessionAddRemoteConnections,
	eSteamPT_UserData,
	eSteamPT_VoiceData,

	eSteamPT_EndType
};

////////////////////////////////////////////////////////////////////////////////

class CCrySteamLobbyService : public CCryLobbyService
{
public:
	CCrySteamLobbyService(CCryLobby* pLobby, ECryLobbyService service);
	virtual ~CCrySteamLobbyService(void);

	virtual ECryLobbyError   Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual ECryLobbyError   Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual void             Tick(CTimeValue tv);
	virtual ICryMatchMaking* GetMatchMaking()
	{
	#if USE_CRY_MATCHMAKING
		return m_pMatchmaking;
	#else
		return NULL;
	#endif // USE_CRY_MATCHMAKING
	}
	#if USE_CRY_VOICE && USE_STEAM_VOICE
	virtual ICryVoice*  GetVoice()  { return m_pVoice; }
	#else
	virtual ICryVoice*  GetVoice()  { return NULL; }
	#endif // USE_CRY_VOICE
	virtual ICryReward* GetReward() { return m_pReward; }
	virtual ICryStats*  GetStats()
	{
	#if USE_CRY_STATS
		return m_pStats;
	#else
		return NULL;
	#endif // USE_CRY_STATS
	}
	virtual ICryLobbyUI*       GetLobbyUI()       { return NULL; }
	virtual ICryOnlineStorage* GetOnlineStorage() { return NULL; }
	virtual ICryFriends*       GetFriends()
	{
	#if USE_CRY_FRIENDS
		return m_pFriends;
	#else
		return NULL;
	#endif // USE_CRY_FRIENDS
	}
	virtual ICryFriendsManagement* GetFriendsManagement()
	{
	#if USE_CRY_FRIENDS
		return m_pFriendsManagement;
	#else
		return NULL;
	#endif // USE_CRY_FRIENDS
	}
	virtual CryUserID      GetUserID(uint32 user) { return CryUserInvalidID; }
	virtual ECryLobbyError GetSystemTime(uint32 user, SCrySystemTime* pSystemTime);

	virtual void           OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	virtual void           OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	virtual void           OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID);

	virtual void           GetSocketPorts(uint16& connectPort, uint16& listenPort);

	void                   InviteAccepted(uint32 user, CrySessionID id);

private:
	#if USE_CRY_MATCHMAKING
	_smart_ptr<CCrySteamMatchMaking> m_pMatchmaking;
	#endif // USE_CRY_MATCHMAKING
	#if USE_CRY_STATS
	_smart_ptr<CCrySteamStats> m_pStats;
	#endif // USE_CRY_STATS

	_smart_ptr<CCrySteamReward> m_pReward;

	#if USE_CRY_VOICE && USE_STEAM_VOICE
	_smart_ptr<CCrySteamVoice> m_pVoice;
	#endif

	#if USE_CRY_FRIENDS
	_smart_ptr<CCrySteamFriends>           m_pFriends;
	_smart_ptr<CCrySteamFriendsManagement> m_pFriendsManagement;
	#endif // USE_CRY_FRIENDS

	#if !defined(RELEASE)
	AppId_t m_SteamAppID;
	#endif // !defined(RELEASE)
};

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM
#endif // __CRYSTEAMLOBBY_H__
