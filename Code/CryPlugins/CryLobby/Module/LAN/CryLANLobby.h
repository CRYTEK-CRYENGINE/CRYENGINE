// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYLANLOBBY_H__
#define __CRYLANLOBBY_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryLobby.h"
#include "CryTCPServiceFactory.h"
#include "LAN/CryLANMatchMaking.h"
#include "CryReward.h"

#if USE_LAN

class CCryDedicatedServerArbitrator;
class CCryDedicatedServer;

enum CryLANLobbyPacketType
{
	eLANPT_SessionRequestJoinResult = eLobbyPT_SessionRequestJoinResult,

	eLANPT_MM_RequestServerData     = CRYLANLOBBY_PACKET_START,
	eLANPT_MM_ServerData,
	eLANPT_SessionRequestJoin,
	eLANPT_SessionAddRemoteConnections,
	eLANPT_HostMigrationStart,
	eLANPT_HostMigrationServer,
	eLANPT_HostMigrationClient,
	eLANPT_UserData,

	eLANPT_EndType
};

struct SCryLANUserID : public SCrySharedUserID
{
	virtual bool operator==(const SCryUserID& other) const
	{
		return id == ((SCryLANUserID&)other).id;
	}

	virtual bool operator<(const SCryUserID& other) const
	{
		return id < ((SCryLANUserID&)other).id;
	}

	virtual CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> GetGUIDAsString() const
	{
		CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> temp;

		temp.Format("%016llx", id);

		return temp;
	}

	SCryLANUserID& operator=(const SCryLANUserID& other)
	{
		id = other.id;
		return *this;
	}

	uint64 id;
};

class CCryLANLobbyService : public CCryLobbyService
{
public:
	CCryLANLobbyService(CCryLobby* pLobby, ECryLobbyService service);

	virtual ECryLobbyError         Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual ECryLobbyError         Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual void                   Tick(CTimeValue tv);
	virtual ICryMatchMaking*       GetMatchMaking()       { return m_pMatchmaking; }
	virtual ICryVoice*             GetVoice()             { return NULL; }
	virtual ICryReward*            GetReward()            { return NULL; }
	virtual ICryStats*             GetStats()             { return NULL; }
	virtual ICryLobbyUI*           GetLobbyUI()           { return NULL; }
	virtual ICryOnlineStorage*     GetOnlineStorage()     { return NULL; }
	virtual ICryFriends*           GetFriends()           { return NULL; }
	virtual ICryFriendsManagement* GetFriendsManagement() { return NULL; }
	virtual CryUserID              GetUserID(uint32 user) { return CryUserInvalidID; }
	virtual ECryLobbyError         GetSystemTime(uint32 user, SCrySystemTime* pSystemTime);

	virtual void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	virtual void                   OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	virtual void                   OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID);

	virtual void                   GetSocketPorts(uint16& connectPort, uint16& listenPort);

	#if USE_CRY_DEDICATED_SERVER_ARBITRATOR
	CCryDedicatedServerArbitrator* GetDedicatedServerArbitrator() { return m_pDedicatedServerArbitrator; }
	#endif
	#if USE_CRY_DEDICATED_SERVER
	CCryDedicatedServer* GetDedicatedServer() { return m_pDedicatedServer; }
	#endif
private:
	_smart_ptr<CCryLANMatchMaking> m_pMatchmaking;
	#if USE_CRY_DEDICATED_SERVER_ARBITRATOR
	CCryDedicatedServerArbitrator* m_pDedicatedServerArbitrator;
	#endif
	#if USE_CRY_DEDICATED_SERVER
	_smart_ptr<CCryDedicatedServer> m_pDedicatedServer;
	#endif
};

#endif // USE_LAN
#endif // __CRYLANLOBBY_H__
