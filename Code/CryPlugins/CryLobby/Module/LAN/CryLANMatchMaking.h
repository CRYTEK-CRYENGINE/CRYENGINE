// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryMatchMaking.h"
#include "CrySharedLobbyPacket.h"

#if USE_LAN

	#define MAX_LAN_SEARCH_SERVERS 128
	#define MAX_LAN_SEARCHES       2

class CrySharedLobbyPacket;

// If the size and/or layout of SCryLANSessionID changes, its GetSizeInPacket,
// WriteToPacket, and ReadFromPacket must be changed to reflect this. Most
// importantly, these functions must not include size of SCryLANSessionID's vtab
// pointer, serialize the vtab pointer into a packet, or deserialize the vtab
// pointer out of a packet.

struct SCryLANSessionID : public SCrySharedSessionID
{
	CryLobbySessionHandle m_h;
	uint32                m_ip;
	uint16                m_port;

	SCryLANSessionID() :
		m_h(CryLobbyInvalidSessionHandle),
		m_ip(0),
		m_port(0)
	{
	}

	virtual bool operator==(const SCrySessionID& other)
	{
		return (m_h == ((const SCryLANSessionID&)other).m_h) &&
		       (m_ip == ((const SCryLANSessionID&)other).m_ip) &&
		       (m_port == ((const SCryLANSessionID&)other).m_port);
	};
	virtual bool operator<(const SCrySessionID& other)
	{
		return ((m_h < ((const SCryLANSessionID&)other).m_h) ||
		        ((m_h == ((const SCryLANSessionID&)other).m_h) && (m_ip < ((const SCryLANSessionID&)other).m_ip)) ||
		        ((m_h == ((const SCryLANSessionID&)other).m_h) && (m_ip == ((const SCryLANSessionID&)other).m_ip) && (m_port < ((const SCryLANSessionID&)other).m_port)));
	};
	SCryLANSessionID& operator=(const SCryLANSessionID& other)
	{
		m_h = other.m_h;
		m_ip = other.m_ip;
		m_port = other.m_port;
		return *this;
	}

	static uint32 GetSizeInPacket()
	{
		return CryLobbyPacketLobbySessionHandleSize + CryLobbyPacketUINT32Size + CryLobbyPacketUINT16Size;
	};

	void WriteToPacket(CCryLobbyPacket* pPacket) const
	{
		CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

		pSPacket->WriteLobbySessionHandle(m_h);
		pSPacket->WriteUINT32(m_ip);
		pSPacket->WriteUINT16(m_port);
	};

	void ReadFromPacket(CCryLobbyPacket* pPacket)
	{
		CCrySharedLobbyPacket* pSPacket = (CCrySharedLobbyPacket*)pPacket;

		m_h = pSPacket->ReadLobbySessionHandle();
		m_ip = pSPacket->ReadUINT32();
		m_port = pSPacket->ReadUINT16();
	};
	bool IsFromInvite() const
	{
		return true;
	}

	void AsCStr(char* pOutString, int inBufferSize) const
	{
		cry_sprintf(pOutString, inBufferSize, "%d :%d", m_ip, m_port);
	}
};

class CCryLANMatchMaking : public CCryMatchMaking
{
public:
	CCryLANMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType);

	ECryLobbyError         Initialise();
	void                   Tick(CTimeValue tv);

	void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);

	virtual ECryLobbyError SessionRegisterUserData(SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
	#if CRY_PLATFORM_DURANGO
	virtual ECryLobbyError SetINetworkingUser(Live::State::INetworkingUser* user) override { return eCLE_Success; }
	#endif
	virtual ECryLobbyError SessionCreate(uint32* users, int numUsers, uint32 flags, SCrySessionData* data, CryLobbyTaskID* taskID, CryMatchmakingSessionCreateCallback cb, void* cbArg);
	virtual ECryLobbyError SessionMigrate(CrySessionHandle h, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionUpdate(CrySessionHandle h, SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
	virtual ECryLobbyError SessionUpdateSlots(CrySessionHandle h, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionQuery(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionGetUsers(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionStart(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
	virtual ECryLobbyError SessionEnd(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
	virtual ECryLobbyError SessionDelete(CrySessionHandle h, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
	virtual ECryLobbyError SessionSearch(uint32 user, SCrySessionSearchParam* param, CryLobbyTaskID* taskID, CryMatchmakingSessionSearchCallback cb, void* cbArg);
	virtual ECryLobbyError SessionJoin(uint32* users, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* taskID, CryMatchmakingSessionJoinCallback cb, void* cbArg);
	virtual ECryLobbyError SessionSetLocalUserData(CrySessionHandle h, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg);
	virtual CrySessionID   SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h);
	virtual CrySessionID   GetSessionIDFromConsole(void);
	virtual ECryLobbyError GetSessionIDFromIP(const char* const pAddress, CrySessionID* const pSessionID);

	virtual uint32         GetSessionIDSizeInPacket() const;
	virtual ECryLobbyError WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const;
	virtual CrySessionID   ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const;
	virtual void           Kick(const CryUserID* pUserID, EDisconnectionCause cause);

	virtual ECryLobbyError SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	#if NETWORK_HOST_MIGRATION
	virtual void           HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual ECryLobbyError HostMigrationServer(SHostMigrationInfo* pInfo);
	virtual bool           GetNewHostAddress(char* address, SHostMigrationInfo* pInfo);
	#endif

private:
	enum ETask
	{
		eT_SessionRegisterUserData = eT_MatchMakingPlatformSpecificTask,
		eT_SessionCreate,
		eT_SessionMigrate,
		eT_SessionUpdate,
		eT_SessionUpdateSlots,
		eT_SessionQuery,
		eT_SessionGetUsers,
		eT_SessionStart,
		eT_SessionEnd,
		eT_SessionDelete,
		eT_SessionSearch,
		eT_SessionJoin,
		eT_SessionSetLocalUserData,

		// ^^New task IDs *must* go before host migration tasks^^
		eT_SessionMigrateHostStart = HOST_MIGRATION_TASK_ID_FLAG,
		eT_SessionMigrateHostServer,
		eT_SessionMigrateHostClient,
		eT_SessionMigrateHostFinish
	};

	struct SCryLANSearchHandle
	{
	};

	typedef CryLobbyID<SCryLANSearchHandle, MAX_LAN_SEARCHES> CryLANSearchHandle;
	#define CryLANInvalidSearchHandle CryLANSearchHandle()

	ECryLobbyError             StartTask(ETask task, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg);
	void                       StartTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       StopTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       EndTask(CryMatchMakingTaskID mmTaskID);
	ECryLobbyError             CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers);
	CryMatchMakingConnectionID AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 ip, uint16 port, uint32 numUsers, const char* name, uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES], bool isDedicated);
	virtual void               FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	virtual uint64             GetSIDFromSessionHandle(CryLobbySessionHandle h);
	ECryLobbyError             CreateSessionSearchHandle(CryLANSearchHandle* h);
	ECryLobbyError             SetSessionUserData(CryLobbySessionHandle h, SCrySessionUserData* data, uint32 numData);
	ECryLobbyError             SetSessionData(CryLobbySessionHandle h, SCrySessionData* data);
	size_t                     CalculateServerDataSize(CryLobbySessionHandle h);
	size_t                     CalculateServerDataSize() const;
	void                       SendServerData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ServerDataToGame(CryMatchMakingTaskID mmTaskID, uint32 ip, uint16 port, TMemHdl params, uint32 length);
	void                       ProcessServerData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	CryLobbySessionHandle      FindSessionFromServerID(CryLobbySessionHandle h);

	void                       StartSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       ProcessSessionRequestJoin(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ProcessSessionRequestJoinResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       ProcessSessionAddRemoteConnections(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);

	void                       StartSessionDelete(CryMatchMakingTaskID mmTaskID);

	void                       SendSessionQuery(CTimeValue tv, uint32 index, bool broadcast);
	void                       SetLocalUserName(CryLobbySessionHandle h, uint32 localUserIndex);
	virtual const char*        GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const;
	void                       StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID);
	void                       ProcessLocalUserData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       EndSessionGetUsers(CryMatchMakingTaskID mmTaskID);

	TNetAddress                GetHostAddressFromSessionHandle(CrySessionHandle h);

	#if NETWORK_HOST_MIGRATION
	void           HostMigrationServerNT(CryMatchMakingTaskID mmTaskID);
	ECryLobbyError HostMigrationClient(CryLobbySessionHandle h, CryMatchMakingTaskID hostTaskID);
	void           TickHostMigrationClientNT(CryMatchMakingTaskID mmTaskID);
	void           ProcessHostMigrationFromServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void           ProcessHostMigrationFromClient(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void           TickHostMigrationFinishNT(CryMatchMakingTaskID mmTaskID);
	void           HostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	void           TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	void           ProcessHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	#endif
	void           SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void           InitialUserDataEvent(CryLobbySessionHandle h);

	void           ResetTask(CryMatchMakingTaskID mmTaskID);
	void           DispatchRoomOwnerChangedEvent(TMemHdl mh);

	bool           ParamFilter(uint32 nParams, const SCrySessionSearchData* pParams, const SCrySessionSearchResult* pResult);

	virtual uint64 GetConnectionUserID(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const;

	struct  SRegisteredUserData
	{
		SCrySessionUserData data[MAX_MATCHMAKING_SESSION_USER_DATA];
		uint32              num;
	} m_registeredUserData;

	struct  SSession : public CCryMatchMaking::SSession
	{
		typedef CCryMatchMaking::SSession PARENT;

		virtual const char* GetLocalUserName(uint32 localUserIndex) const;
		virtual void        Reset();

		SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];
		SCrySessionData     data;
		SCryLANSessionID    id;
		uint64              sid;
		uint32              numFilledSlots;

		struct SLConnection : public CCryMatchMaking::SSession::SLConnection
		{
			char  name[CRYLOBBY_USER_NAME_LENGTH];
			uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
		}                     localConnection;

		struct SRConnection : public CCryMatchMaking::SSession::SRConnection
		{
			char   name[CRYLOBBY_USER_NAME_LENGTH];
			uint8  userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
			uint32 ip;
			uint16 port;
	#if NETWORK_HOST_MIGRATION
			bool   m_migrated;
	#endif
			bool   m_isDedicated;                                                 // Remote connection is a dedicated server
		};

		CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;
	};

	struct  SSearch
	{
		SCryLANSessionID servers[MAX_LAN_SEARCH_SERVERS];
		uint32           numServers;
		bool             used;
	};

	struct  STask : public CCryMatchMaking::STask
	{
		CryLANSearchHandle search;
		uint32             ticks;
	};

	CryLobbyIDArray<SSearch, CryLANSearchHandle, MAX_LAN_SEARCHES>             m_search;
	CryLobbyIDArray<SSession, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;
	STask m_task[MAX_MATCHMAKING_TASKS];
};

#endif // USE_LAN
