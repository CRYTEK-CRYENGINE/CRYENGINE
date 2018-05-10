// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_LOBBY_H__
#define __CRYPSN2_LOBBY_H__

#pragma once

#if CRY_PLATFORM_ORBIS

	#include "CryLobby.h"

	#if USE_PSN

		#include "PSNOrbis/CryPSN2Support.h"
		#include "PSNOrbis/CryPSN2MatchMaking.h"
		#include "PSNOrbis/CryPSN2Stats.h"
		#include "PSNOrbis/CryPSN2LobbyUI.h"
		#include "PSNOrbis/CryPSN2Friends.h"
		#include "PSNOrbis/CryPSN2Reward.h"
//#include "Lobby/PSN/CryPSNVoice.h"

		#include <np.h>

		#include "CryTCPServiceFactory.h"

struct SCryPSNUserID : public SCrySharedUserID
{
	virtual bool operator==(const SCryUserID& other) const
	{
		return (sceNpCmpNpId(&npId, &(((SCryPSNUserID&)other).npId)) == 0);
	}

	virtual bool operator<(const SCryUserID& other) const
	{
		int order = 0;
		sceNpCmpNpIdInOrder(&npId, &(((SCryPSNUserID&)other).npId), &order);
		return order < 0;
	}

	virtual CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> GetGUIDAsString() const
	{
		CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> temp;

		temp = CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH>(npId.handle.data);

		return temp;
	}

	SCryPSNUserID& operator=(const SCryPSNUserID& other)
	{
		npId = other.npId;
		return *this;
	}

	SceNpId npId;
};

enum CryPSNLobbyPacketType
{
	ePSNPT_HostMigrationStart = eLobbyPT_EndType,
	ePSNPT_HostMigrationServer,
	ePSNPT_JoinSessionAck,

	ePSNPT_EndType
};

class CCryPSNLobbyService : public CCryLobbyService
{
public:
	CCryPSNLobbyService(CCryLobby* pLobby, ECryLobbyService service);

	virtual ECryLobbyError   Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual ECryLobbyError   Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual void             Tick(CTimeValue tv);
	virtual ICryMatchMaking* GetMatchMaking()
	{
		#if USE_CRY_MATCHMAKING
		return m_pMatchmaking;
		#else // USE_CRY_MATCHMAKING
		return NULL;
		#endif //
	}
	virtual ICryVoice* GetVoice()
	{
		#if USE_PSN_VOICE
		return m_pVoice;
		#else//USE_PSN_VOICE
		return NULL;
		#endif//USE_PSN_VOICE
	}
	virtual ICryReward* GetReward() { return m_pReward; }
	virtual ICryStats*  GetStats()
	{
		#if USE_CRY_STATS
		return m_pStats;
		#else // USE_CRY_STATS
		return NULL;
		#endif // USE_CRY_STATS
	}
	virtual ICryOnlineStorage* GetOnlineStorage() { return NULL; }                        //No implementation yet
	virtual ICryLobbyUI*       GetLobbyUI()       { return m_pLobbyUI; }
	virtual ICryFriends*       GetFriends()
	{
		#if USE_CRY_FRIENDS
		return m_pFriends;
		#else // USE_CRY_FRIENDS
		return NULL;
		#endif // USE_CRY_FRIENDS
	}
	virtual ICryFriendsManagement* GetFriendsManagement() { return NULL; }

	CCryPSNSupport* const          GetPSNSupport() const  { return m_pLobbySupport; }

	virtual CryUserID              GetUserID(uint32 user);
	virtual ECryLobbyError         GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg);

	virtual ECryLobbyError         GetSystemTime(uint32 user, SCrySystemTime* pSystemTime);

	virtual void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	virtual void                   OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	virtual void                   OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID);

	void                           GetConfigurationInformation(SConfigurationParams* infos, uint32 cnt);

	virtual void                   GetSocketPorts(uint16& connectPort, uint16& listenPort);
	virtual void                   MakeAddrPCCompatible(TNetAddress& addr);

	static void                    SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);

		#if defined(WARFACE_CONSOLE_VERSION)
			#if defined(GUID_STYLE_DB)
	virtual CryUserID      GetUserIDFromGUID(PlatformGUID64 xuid);
	virtual CryUserID      GetUserIDFromGUIDAsString(const char* xuidAsString) { return GetUserIDFromGUID(_atoi64(xuidAsString)); }
	virtual PlatformGUID64 GetGUIDFromUserID(CryUserID id);
			#endif //GUID_STYLE_DB
		#endif//WARFACE_CONSOLE_VERSION

private:

	void StartTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void StopTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void EndTask(CryLobbyServiceTaskID lsTaskID);

	void StartGetUserPrivileges(CryLobbyServiceTaskID lsTaskID);
	void TickGetUserPrivileges(CryLobbyServiceTaskID lsTaskID);

	_smart_ptr<CCryPSNSupport>     m_pLobbySupport;
		#if USE_CRY_MATCHMAKING
	_smart_ptr<CCryPSNMatchMaking> m_pMatchmaking;
		#endif // USE_CRY_MATCHMAKING
		#if USE_PSN_VOICE
	//	_smart_ptr<CCryPSNVoice>				m_pVoice;
		#endif//USE_PSN_VOICE
		#if USE_CRY_STATS
	_smart_ptr<CCryPSNStats>   m_pStats;
		#endif // USE_CRY_STATS
	_smart_ptr<CCryPSNLobbyUI> m_pLobbyUI;
		#if USE_CRY_FRIENDS
	_smart_ptr<CCryPSNFriends> m_pFriends;
		#endif // USE_CRY_FRIENDS
	_smart_ptr<CCryPSNReward>  m_pReward;

	bool                       m_NPInited;
};

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif//__CRYPSN2_LOBBY_H__
