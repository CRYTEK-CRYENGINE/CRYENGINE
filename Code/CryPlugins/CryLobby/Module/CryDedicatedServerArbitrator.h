// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYDEDICATEDSERVERARBITRATOR_H__
#define __CRYDEDICATEDSERVERARBITRATOR_H__

#pragma once

#include "CryLobby.h"

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR

class CCrySharedLobbyPacket;

class CCryDedicatedServerArbitrator
{
public:
	CCryDedicatedServerArbitrator(CCryLobby* pLobby, CCryLobbyService* pService);

	ECryLobbyError Initialise();
	ECryLobbyError Terminate();
	void           Tick(CTimeValue tv);
	void           OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	void           OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	void           ListFreeServers();

protected:
	// There are 2 addresses for a dedicated server
	// serverPublicAddr is the address used by the external clients
	// serverPrivateAddr is the address used by the arbitrator
	// These addresses may or may not be the same depending on setup.

	struct SFreeServerData
	{
		TNetAddress serverPublicAddr;
		CTimeValue  lastRecvTime;
	};

	struct SAllocatedServerData
	{
		CTimeValue  allocTime;
		TNetAddress serverPublicAddr;
		TNetAddress serverPrivateAddr;
		uint32      clientCookie;
		uint8       clientTaskID;
		bool        serverReady;
	};

	struct SFreeingServerData
	{
		CTimeValue lastSendTime;
		uint32     sendCount;
	};

	typedef std::map<TNetAddress, SFreeServerData>    TFreeServersMap;          // TNetAddress is the dedicated servers private address
	typedef std::map<uint64, SAllocatedServerData>    TAllocatedServersMap;
	typedef std::map<TNetAddress, SFreeingServerData> TFreeingServersMap;       // TNetAddress is the dedicated servers private address

	TFreeServersMap::iterator      AddToFreeServers(const TNetAddress& serverPrivateAddr, const TNetAddress& serverPublicAddr);
	TAllocatedServersMap::iterator AddToAllocatedServers(uint64 sid, const TNetAddress& serverPrivateAddr, const TNetAddress& serverPublicAddr, uint32 clientCookie, uint8 clientTaskID);
	TFreeingServersMap::iterator   AddToFreeingServers(const TNetAddress& serverPrivateAddr);
	void                           ProcessDedicatedServerIsFree(const TNetAddress& serverPrivateAddr, CCrySharedLobbyPacket* pPacket);
	void                           SendRequestSetupDedicatedServerResult(bool success, uint32 clientTaskID, uint32 clientCookie, const TNetAddress& clientAddr, const TNetAddress& serverPublicAddr, SCryMatchMakingConnectionUID serverUID);
	void                           ProcessRequestSetupDedicatedServer(const TNetAddress& clientAddr, CCrySharedLobbyPacket* pPacket);
	void                           ProcessAllocateDedicatedServerAck(const TNetAddress& serverPrivateAddr, CCrySharedLobbyPacket* pPacket);
	void                           ProcessRequestReleaseDedicatedServer(const TNetAddress& clientAddr, CCrySharedLobbyPacket* pPacket);

	TFreeServersMap      m_freeServers;
	TAllocatedServersMap m_allocatedServers;
	TFreeingServersMap   m_freeingServers;
	CCryLobby*           m_pLobby;
	CCryLobbyService*    m_pService;
};

#endif // USE_CRY_DEDICATED_SERVER_ARBITRATOR
#endif // __CRYDEDICATEDSERVERARBITRATOR_H__
