// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYDEDICATEDSERVER_H__
#define __CRYDEDICATEDSERVER_H__

#pragma once

#include "CryLobby.h"
#include "CryMatchMaking.h"

#define DEDICATED_SERVER_IS_FREE_SEND_INTERVAL 30000

#if USE_CRY_DEDICATED_SERVER

class CCrySharedLobbyPacket;

class CCryDedicatedServer : public CCryMatchMaking
{
public:
	CCryDedicatedServer(CCryLobby* pLobby, CCryLobbyService* pService, ECryLobbyService serviceType);

	virtual ECryLobbyError SessionRegisterUserData(SCryLobbyUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)                                                       { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionCreate(uint32* users, int numUsers, uint32 flags, SCrySessionData* data, CryLobbyTaskID* taskID, CryMatchmakingSessionCreateCallback cb, void* cbArg)                           { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionMigrate(CrySessionHandle h, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg) { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionUpdate(CrySessionHandle h, SCryLobbyUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)                                             { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionUpdateSlots(CrySessionHandle h, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)                                         { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionQuery(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingSessionQueryCallback cb, void* cbArg)                                                                           { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionGetUsers(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingSessionGetUsersCallback cb, void* cbArg)                                                                     { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionEnd(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)                                                                                         { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionStart(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)                                                                                       { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionSetLocalUserData(CrySessionHandle h, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg)                             { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionDelete(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg)                                                                                      { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionSearch(uint32 user, SCrySessionSearchParam* param, CryLobbyTaskID* taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg)                                                 { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionJoin(uint32* users, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* taskID, CryMatchmakingSessionJoinCallback cb, void* cbArg)                                     { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg)                                                                          { return eCLE_ServiceNotSupported; }
	virtual CrySessionID   SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h)                                                                                                                         { return CrySessionInvalidID; }
	virtual uint32         GetSessionIDSizeInPacket() const                                                                                                                                                       { return 0; }
	virtual ECryLobbyError WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const                                                                                                         { return eCLE_Success; }
	virtual CrySessionID   ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const                                                                                                                                { return CrySessionInvalidID; }
	virtual TNetAddress    GetHostAddressFromSessionHandle(CrySessionHandle h);

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();
	void                   Tick(CTimeValue tv);
	void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);

	#if NETWORK_HOST_MIGRATION
	virtual void           HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual void           HostMigrationFinalise(CryLobbySessionHandle h)              {}
	virtual ECryLobbyError HostMigrationServer(SHostMigrationInfo* pInfo)              { return eCLE_ServiceNotSupported; }
	virtual bool           GetNewHostAddress(char* address, SHostMigrationInfo* pInfo) { return false; }
	#endif

protected:
	virtual uint64             GetSIDFromSessionHandle(CryLobbySessionHandle h);
	CryMatchMakingConnectionID AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 numUsers);
	void                       StartArbitratorNameLookup();
	void                       TickArbitratorNameLookup(CTimeValue tv);
	void                       TickFree(CTimeValue tv);
	void                       TickAllocated(CTimeValue tv);
	void                       DispatchUserPacket(TMemHdl mh, uint32 length);
	void                       ProcessDedicatedServerIsFreeAck(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ProcessAllocateDedicatedServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       DispatchFreeDedicatedServer();
	void                       FreeDedicatedServer();
	void                       ProcessFreeDedicatedServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ProcessSessionRequestJoinServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ProcessUpdateSessionID(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);

	struct  SSession : public CCryMatchMaking::SSession
	{
		typedef CCryMatchMaking::SSession PARENT;
		virtual const char* GetLocalUserName(uint32 localUserIndex) const { return NULL; }
		virtual void        Reset();

		struct SLConnection : public CCryMatchMaking::SSession::SLConnection
		{
		};

		struct SRConnection : public CCryMatchMaking::SSession::SRConnection
		{
		};

		uint64       sid;
		TNetAddress  allocToAddr;
		CTimeValue   checkFreeStartTime;
		uint32       cookie;
		SLConnection localConnection;
		CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;
	};

	struct  STask : public CCryMatchMaking::STask
	{
	};

	CryLobbyIDArray<SSession, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;
	STask                 m_task[MAX_MATCHMAKING_TASKS];

	CTimeValue            m_arbitratorLastRecvTime;
	CTimeValue            m_arbitratorNextSendTime;

	CryLobbySessionHandle m_allocatedSession;
	CNameRequestPtr       m_pNameReq;

	CCryLobby*            m_pLobby;
};

#endif // USE_CRY_DEDICATED_SERVER
#endif // __CRYDEDICATEDSERVER_H__
