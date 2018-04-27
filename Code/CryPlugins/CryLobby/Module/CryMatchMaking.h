// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYMATCHMAKING_H__
#define __CRYMATCHMAKING_H__

#pragma once

#include <CryLobby/ICryMatchMaking.h>
#include "CryLobby.h"
#include <CryNetwork/NetAddress.h>
#include "CryHostMigration.h"

#if USE_CRY_MATCHMAKING

	#define NET_HOST_HINT_DEBUG     0
	#define USE_DEFAULT_BAN_TIMEOUT (0.0f)

///////////////////////////////////////////////////////////////////////////
// When all references to SCrySessionUserData and ECrySessionUserDataType have been removed from the engine this can go.
typedef SCryLobbyUserData     SCrySessionUserData;
typedef ECryLobbyUserDataType ECrySessionUserDataType;
const ECrySessionUserDataType eCSUDT_Int64 = eCLUDT_Int64;
const ECrySessionUserDataType eCSUDT_Int32 = eCLUDT_Int32;
const ECrySessionUserDataType eCSUDT_Int16 = eCLUDT_Int16;
const ECrySessionUserDataType eCSUDT_Int8 = eCLUDT_Int8;
const ECrySessionUserDataType eCSUDT_Float64 = eCLUDT_Float64;
const ECrySessionUserDataType eCSUDT_Float32 = eCLUDT_Float32;
const ECrySessionUserDataType eCSUDT_Int64NoEndianSwap = eCLUDT_Int64NoEndianSwap;
///////////////////////////////////////////////////////////////////////////

class CCryLobbyPacket;
class CCrySharedLobbyPacket;

const uint32 CryMatchMakingConnectionTimeOut = 10000;
const uint32 CryMatchMakingHostMigratedConnectionConfirmationWindow = 3000; // 3 second window to receive client confirmations before pruning

	#define MAX_MATCHMAKING_SESSION_USER_DATA 64
	#define MAX_MATCHMAKING_TASKS             8
	#define MAX_MATCHMAKING_PARAMS            7
	#define GAME_RECV_QUEUE_SIZE              16

	#define HOST_MIGRATION_TASK_ID_FLAG       (0x10000000)

	#define INVALID_USER_ID                   (0)

// Dedicated servers allocated with CryDedicatedServerArbitrator have this as their m_uid in SCryMatchMakingConnectionUID
	#define DEDICATED_SERVER_CONNECTION_UID 0

struct SCryLobbySessionHandle
{
};

typedef CryLobbyID<SCryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> CryLobbySessionHandle;
	#define CryLobbyInvalidSessionHandle CryLobbySessionHandle()

typedef uint32 CryMatchMakingTaskID;
const CryMatchMakingTaskID CryMatchMakingInvalidTaskID = 0xffffffff;

struct SCryMatchMakingConnectionID
{
};

typedef CryLobbyID<SCryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> CryMatchMakingConnectionID;
	#define CryMatchMakingInvalidConnectionID CryMatchMakingConnectionID()

struct SCrySharedSessionID : public SCrySessionID
{
	void* operator new(size_t size) throw()
	{
		CCryLobby* pLobby = (CCryLobby*)CCryLobby::GetLobby();

		if (pLobby)
		{
			TMemHdl hdl = pLobby->MemAlloc(size);

			if (hdl != TMemInvalidHdl)
			{
				SCrySharedSessionID* id = (SCrySharedSessionID*)pLobby->MemGetPtr(hdl);

				id->hdl = hdl;

				return id;
			}
		}

		return NULL;
	}

	void operator delete(void* p)
	{
		if (p)
		{
			CCryLobby* pLobby = (CCryLobby*)CCryLobby::GetLobby();

			if (pLobby)
			{
				pLobby->MemFree(((SCrySharedSessionID*)p)->hdl);
			}
		}
	}

	TMemHdl hdl;
};

	#if NETWORK_HOST_MIGRATION

#include "Context/INetContextState.h"

		#if ENABLE_HOST_MIGRATION_STATE_CHECK
#define HMSCD_MAX_ENTITY_NAME_LENGTH 16

enum EHostMigrationStateCheckDataType
{
	eHMSCDT_NID
};

struct SHostMigrationStateCheckDataNID
{
	uint32 aspectsEnabled;
	uint16 id;
	uint16 salt;
	char   name[HMSCD_MAX_ENTITY_NAME_LENGTH];
	bool   allocated;
	bool   controlled;
	bool   owned;
	uint8  spawnType;
};

struct SHostMigrationStateCheckData
{
	EHostMigrationStateCheckDataType type;

	union
	{
		SHostMigrationStateCheckDataNID nid;
	};
};
		#endif

struct SHostMigrationInfo_Private : public SHostMigrationInfo
{
	SHostMigrationInfo_Private(void)
		: m_startTime(0.0f)
		, m_logTime(0.0f)
		, m_sessionMigrated(false)
		, m_matchMakingFinished(false)
		, m_newHostAddressValid(false)
		, m_lobbyHMStatus(0)
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
		, m_stateCheckDone(false)
		#endif
	{
	}

	void Reset(void)
	{
		m_sessionMigrated = false;
		m_matchMakingFinished = false;
		m_newHostAddressValid = false;
		m_lobbyHMStatus = 0;
		m_taskID = CryMatchMakingInvalidTaskID;
	}

	CTimeValue                   m_startTime;
	CTimeValue                   m_logTime;
	TNetAddress                  m_newHostAddress;

	_smart_ptr<INetContextState> m_storedContextState;

	uint32                       m_lobbyHMStatus;

	bool                         m_sessionMigrated;
	bool                         m_matchMakingFinished;
	bool                         m_newHostAddressValid;
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	bool                         m_stateCheckDone;
		#endif
};

	#endif

struct SCrySessionInfoRequester
{
	TNetAddress          addr;
	uint32               cookie;
	CryMatchMakingTaskID mmTaskID;
};

struct SBannedUser
{
	SBannedUser() : m_userID(CryUserInvalidID), m_userName(""), m_bannedUntil(), m_permanent(false), m_resolving(false), m_duplicate(false) {};

	CryUserID        m_userID;
	CryStringT<char> m_userName;
	CTimeValue       m_bannedUntil;
	bool             m_permanent;
	bool             m_resolving;
	bool             m_duplicate;
};

typedef std::deque<SBannedUser> TBannedUser;

class CCryMatchMaking : public CMultiThreadRefCount, public ICryMatchMaking
{
public:
	CCryMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType);

	ECryLobbyError         Initialise();

	ECryLobbyError         Terminate();
	void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	void                   OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID);
	void                   OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID);
	virtual void           CancelTask(CryLobbyTaskID lTaskID);
	virtual void           SessionDisconnectRemoteConnectionFromNub(CrySessionHandle gh, EDisconnectionCause cause);
	virtual void           SessionJoinFromConsole(void);
	virtual CrySessionID   GetSessionIDFromConsole(void);
	virtual ECryLobbyError GetSessionAddressFromSessionID(CrySessionID sessionID, uint32& hostIP, uint16& hostPort) { return eCLE_ServiceNotSupported; };
	virtual ECryLobbyError SessionTerminateHostHintingForGroup(CrySessionHandle gh, SCryMatchMakingConnectionUID* pConnections, uint32 numConnections, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionSetLocalFlags(CrySessionHandle gh, uint32 flags, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionClearLocalFlags(CrySessionHandle gh, uint32 flags, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionGetLocalFlags(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingFlagsCallback pCB, void* pCBArg);
	virtual bool           IsDead() const { return false; }

	virtual ECryLobbyError SendToServer(CCryLobbyPacket* pPacket, CrySessionHandle gh);
	virtual ECryLobbyError SendToAllClients(CCryLobbyPacket* pPacket, CrySessionHandle gh);
	virtual ECryLobbyError SendToClient(CCryLobbyPacket* pPacket, CrySessionHandle gh, SCryMatchMakingConnectionUID uid);
	virtual ECryLobbyError SessionSendRequestInfo(CCryLobbyPacket* pPacket, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSendRequestInfoCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionSendRequestInfoResponse(CCryLobbyPacket* pPacket, CrySessionRequesterID requester);
	virtual ECryLobbyError SessionSetupDedicatedServer(CrySessionHandle gh, CCryLobbyPacket* pPacket, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSetupDedicatedServerCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionReleaseDedicatedServer(CrySessionHandle h, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);

	// The CrySessionHandle passed to the game consists of
	// the local session handle used to identify the session in other session calls
	// a uid generated by the host to identify the connection used.
	// This id is passed around when the host receives a connection so any peer can identify a connection from this.
	CrySessionHandle                     CreateGameSessionHandle(CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid);
	SCryMatchMakingConnectionUID         CreateConnectionUID(CryLobbySessionHandle h);
	CryLobbySessionHandle                GetSessionHandleFromGameSessionHandle(CrySessionHandle gh);
	SCryMatchMakingConnectionUID         GetConnectionUIDFromGameSessionHandle(CrySessionHandle gh);
	virtual SCryMatchMakingConnectionUID GetConnectionUIDFromGameSessionHandleAndChannelID(CrySessionHandle gh, uint16 channelID);
	virtual uint16                       GetChannelIDFromGameSessionHandle(CrySessionHandle gh);
	SCryMatchMakingConnectionUID         GetConnectionUIDFromNubAndGameSessionHandle(CrySessionHandle gh);

	virtual ECryLobbyService             GetLobbyServiceTypeForNubSession();
	CryLobbySessionHandle                GetCurrentNetNubSessionHandle(void) const;
	CryLobbySessionHandle                GetCurrentHostedNetNubSessionHandle(void) const;
	virtual SCryMatchMakingConnectionUID GetHostConnectionUID(CrySessionHandle gh);
	virtual ECryLobbyError               SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg) = 0;

	virtual void                         TerminateHostMigration(CrySessionHandle gh);
	virtual eHostMigrationState          GetSessionHostMigrationState(CrySessionHandle gh);
	virtual ECryLobbyError               GetSessionPlayerPing(SCryMatchMakingConnectionUID uid, CryPing* const pPing);
	virtual ECryLobbyError               GetSessionIDFromSessionURL(const char* const pSessionURL, CrySessionID* const pSessionID) { return eCLE_ServiceNotSupported; };
	virtual ECryLobbyError               GetSessionIDFromIP(const char* const pAddr, CrySessionID* const pSessionID)               { return eCLE_ServiceNotSupported; };

	virtual uint32                       GetTimeSincePacketFromServerMS(CrySessionHandle gh);
	virtual void                         DisconnectFromServer(CrySessionHandle gh);
	virtual void                         StatusCmd(eStatusCmdMode mode);
	virtual void                         KickCmd(uint32 cxID, uint64 userID, const char* pName, EDisconnectionCause cause);
	virtual void                         KickCmd(CryLobbyConnectionID cxID, uint64 userID, const char* pName, EDisconnectionCause cause);
	virtual void                         Kick(const CryUserID* pUserID, EDisconnectionCause cause)                                                                                          {};
	virtual void                         BanCmd(uint64 userID, float timeout)                                                                                                               {};
	virtual void                         BanCmd(const char* pUserName, float timeout)                                                                                                       {};
	virtual void                         UnbanCmd(uint64 userID)                                                                                                                            {};
	virtual void                         UnbanCmd(const char* pUserName)                                                                                                                    {};
	virtual void                         BanStatus(void)                                                                                                                                    {};
	virtual void                         Ban(const CryUserID* pUserID, float timeout)                                                                                                       {};
	virtual ECryLobbyError               SessionSetAdvertisementData(CrySessionHandle gh, uint8* pData, uint32 dataSize, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCb, void* pCbArg) { return eCLE_ServiceNotSupported; }
	virtual ECryLobbyError               SessionGetAdvertisementData(CrySessionID sessionId, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetAdvertisementDataCallback pCb, void* pCbArg)  { return eCLE_ServiceNotSupported; }
	virtual void                         OnBannedUserListChanged()                                                                                                                          {};

	virtual const char*                  GetNameFromGameSessionHandle(CrySessionHandle gh)                                                                                                  { return ""; }

	#if RESET_CONNECTED_CONNECTION
	void ResetConnection(CryLobbyConnectionID id);
	#endif

	#if NETWORK_HOST_MIGRATION
	virtual bool              HostMigrationInitiate(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual void              HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause) = 0;
	virtual void              HostMigrationReset(CryLobbySessionHandle h);
	virtual void              HostMigrationFinalise(CryLobbySessionHandle h);
	virtual void              HostMigrationWaitForNewServer(CryLobbySessionHandle h);
	virtual ECryLobbyError    HostMigrationServer(SHostMigrationInfo* pInfo) = 0;
	virtual bool              IsSessionMigrated(SHostMigrationInfo* pInfo);
	virtual bool              GetNewHostAddress(char* address, SHostMigrationInfo* pInfo) = 0;
	virtual uint32            GetUpstreamBPS(void);
	virtual bool              AreAnySessionsMigrating(void);
	virtual bool              IsSessionMigrating(CrySessionHandle gh);
	virtual bool              IsNubSessionMigrating() const;
	bool                      IsNubSessionMigratable() const;
	virtual void              HostMigrationStoreNetContextState(INetContextState* pNetContextState);
	virtual INetContextState* HostMigrationGetNetContextState();
	virtual void              HostMigrationResetNetContextState();
	void                      OnReconnectClient(SHostMigrationInfo& hostMigrationInfo);
	bool                      IsHostMigrationFinished(CryLobbySessionHandle h);
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	void                      HostMigrationStateCheck(CryLobbySessionHandle h);
	virtual void              HostMigrationStateCheckAddDataItem(SHostMigrationStateCheckData* pData);
		#endif

protected:
	void                   MarkHostHintInformationDirty(CryLobbySessionHandle h)         { m_sessions[h]->localFlags |= CRYSESSION_LOCAL_FLAG_HOST_HINT_INFO_DIRTY; }
	void                   MarkHostHintExternalInformationDirty(CryLobbySessionHandle h) { m_sessions[h]->localFlags |= CRYSESSION_LOCAL_FLAG_HOST_HINT_EXTERNAL_INFO_DIRTY; }
	void                   SendHostHintInformation(CryLobbySessionHandle h);
	virtual ECryLobbyError SendHostHintExternal(CryLobbySessionHandle h)                 { return eCLE_Success; }
	static void            HostHintingOverrideChanged(ICVar* pCVar);
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	void                   HostMigrationStateCheckInitialise(CryLobbySessionHandle h);
	void                   HostMigrationStateCheckNT(CryLobbySessionHandle h);
	void                   HostMigrationStateCheckLogDataItem(SHostMigrationStateCheckData* pData);
	bool                   HostMigrationStateCheckCompareDataItems(SHostMigrationStateCheckData* pData1, SHostMigrationStateCheckData* pData2);
	bool                   HostMigrationStateCheckWriteDataItemToPacket(CCryLobbyPacket* pPacket, SHostMigrationStateCheckData* pData);
	bool                   HostMigrationStateCheckReadDataItemFromPacket(CCryLobbyPacket* pPacket, SHostMigrationStateCheckData* pData);
	void                   ProcessHostMigrationStateCheckData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
		#endif
public:
	#endif

protected:
	enum EMMTask
	{
		eT_SessionTerminateHostHintingForGroup,
		eT_SessionModifyLocalFlags,
		eT_SessionSetupDedicatedServer,
		eT_SessionSetupDedicatedServerNameResolve,
		eT_SessionConnectToServer,
		eT_SessionReleaseDedicatedServer,

		eT_MatchMakingPlatformSpecificTask
	};

	#if NETWORK_HOST_MIGRATION
	struct SHostHintInformation
	{
		SHostHintInformation(void) { Reset(); }
		CTimeValue lastSendInternal;
		CTimeValue lastSendExternal;
		uint32     upstreamBPS;
		CryPing    ping;
		uint8      natType;
		uint8      numActiveConnections;

		enum eConstants
		{
			// PAYLOAD_SIZE should be the sum of the sizes of the bits of the host hint we send to other peers
			PAYLOAD_SIZE = CryLobbyPacketUINT32Size /* upstreamBPS */ + CryLobbyPacketUINT16Size /* ping */ + CryLobbyPacketUINT8Size /* natType */ + CryLobbyPacketUINT8Size /* numActiveConnections */
		};

		bool operator==(const SHostHintInformation& other) const;
		bool operator!=(const SHostHintInformation& other) const;

		void Reset(void);
		bool SufficientlyDifferent(const SHostHintInformation& other, CTimeValue currentTime);
	};

		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	typedef std::vector<SHostMigrationStateCheckData> THostMigrationStateCheckDatas;
		#endif
	#endif

	struct  SSession
	{
		virtual ~SSession() {};
		virtual const char* GetLocalUserName(uint32 localUserIndex) const = 0;
		virtual void        Reset() = 0;

		struct SLConnection
		{
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
			THostMigrationStateCheckDatas hostMigrationStateCheckDatas;
	#endif
			SCryMatchMakingConnectionUID  uid;
			CryPing                       pingToServer;
			uint8                         numUsers;
			bool                          used;
			// CCryDedicatedServer::SSession uses the same member names for the non-pointer versions of localConnection and remoteConnection. See CCryDedicatedServer::CCryDedicatedServer()
			// cppcheck-suppress duplInheritedMember
		}* localConnection;

	#define CMMRC_FLAG_PLATFORM_DATA_VALID 0x00000001       // Flag set if the platform data in a remote connection is valid.

		struct SRConnection
		{
			bool  TimerStarted() { return timerStarted; }
			void  StartTimer()   { timerStarted = true; timer = g_time; }
			void  StopTimer()    { timerStarted = false; }
			int64 GetTimer()     { return g_time.GetMilliSecondsAsInt64() - timer.GetMilliSecondsAsInt64(); }

			CTimeValue                    timer;
			CryLobbyConnectionID          connectionID;
			SCryMatchMakingConnectionUID  uid;
	#if NETWORK_HOST_MIGRATION
			SHostHintInformation          hostHintInfo;
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
			THostMigrationStateCheckDatas hostMigrationStateCheckDatas;
		#endif
	#endif

			struct SData
			{
				TMemHdl data;
				uint32  dataSize;
			};

			MiniQueue<SData, GAME_RECV_QUEUE_SIZE> gameRecvQueue;

			CryPing                                pingToServer;
			uint32 flags;
			uint8  numUsers;
			bool   used;
			bool   timerStarted;
			bool   gameInformedConnectionEstablished;
		};
		// cppcheck-suppress duplInheritedMember
		CryLobbyIDArray<SRConnection*, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;

	#if NETWORK_HOST_MIGRATION
		SHostMigrationInfo_Private   hostMigrationInfo;
		SRConnection*                newHostPriority[MAX_LOBBY_CONNECTIONS];
		uint32                       newHostPriorityCount;
		SCryMatchMakingConnectionUID newHostUID;
		SCryMatchMakingConnectionUID desiredHostUID;
		SHostHintInformation         hostHintInfo;
		SHostHintInformation         outboundHostHintInfo;
	#endif
		CTimeValue                   pingToServerTimer;
		CryMatchMakingConnectionID   hostConnectionID;
		CryMatchMakingConnectionID   serverConnectionID;
		uint32                       createFlags;
		uint32                       localFlags;
	};

	struct  STask
	{
		bool  TimerStarted() { return timerStarted; }
		void  StartTimer()   { timerStarted = true; timer = g_time; }
		int64 GetTimer()     { return g_time.GetMilliSecondsAsInt64() - timer.GetMilliSecondsAsInt64(); }

		CTimeValue            timer;
		CryLobbyTaskID        lTaskID;
		ECryLobbyError        error;
		CryMatchMakingTaskID  returnTaskID;
		uint32                startedTask;
		uint32                subTask;
		CryLobbySessionHandle session;
		void*                 cb;
		void*                 cbArg;
		CryLobbySendID        sendID;
		ECryLobbyError        sendStatus;
		TMemHdl               params[MAX_MATCHMAKING_PARAMS];
		uint32                numParams[MAX_MATCHMAKING_PARAMS];
		bool                  used;
		bool                  running;
		bool                  timerStarted;
		bool                  canceled;
	};

	enum eDisconnectSource
	{
		eDS_Local,    // The disconnect has come from this machine.
		eDS_Remote,   // The disconnect has come from the other end of the connection.
		eDS_FromID    // The disconnect has come from the connection given in fromID.
	};

	void                       StartTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       TickBaseTask(CryMatchMakingTaskID mmTaskID);
	void                       EndTask(CryMatchMakingTaskID mmTaskID);
	void                       StopTaskRunning(CryMatchMakingTaskID mmTaskID);

	void                       Tick(CTimeValue tv);
	ECryLobbyError             StartTask(uint32 etask, bool startRunning, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg);
	void                       StartSubTask(uint32 etask, CryMatchMakingTaskID mmTaskID);
	void                       FreeTask(CryMatchMakingTaskID mmTaskID);
	ECryLobbyError             CreateTaskParamMem(CryMatchMakingTaskID mmTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	ECryLobbyError             CreateTaskParam(CryMatchMakingTaskID mmTaskID, uint32 param, const void* paramData, uint32 numParams, size_t paramSize);
	ECryLobbyError             CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers);
	virtual void               FreeSessionHandle(CryLobbySessionHandle h);
	CryMatchMakingConnectionID AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, uint32 numUsers);
	virtual void               FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	virtual uint64             GetSIDFromSessionHandle(CryLobbySessionHandle h) = 0;
	bool                       FindLocalConnectionFromUID(SCryMatchMakingConnectionUID uid, CryLobbySessionHandle* h);
	bool                       FindConnectionFromUID(SCryMatchMakingConnectionUID uid, CryLobbySessionHandle* pH, CryMatchMakingConnectionID* pID);
	bool                       FindConnectionFromSessionUID(CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid, CryMatchMakingConnectionID* pID);
	CryMatchMakingTaskID       FindTaskFromSendID(CryLobbySendID sendID);
	CryMatchMakingTaskID       FindTaskFromTaskTaskID(uint32 task, CryMatchMakingTaskID mmTaskID);
	CryMatchMakingTaskID       FindTaskFromTaskSessionHandle(uint32 task, CryLobbySessionHandle h);
	CryMatchMakingTaskID       FindTaskFromLobbyTaskID(CryLobbyTaskID lTaskID);
	void                       UpdateTaskError(CryMatchMakingTaskID mmTaskID, ECryLobbyError error);
	void                       SessionDisconnectRemoteConnectionViaNub(CryLobbySessionHandle h, CryMatchMakingConnectionID id, eDisconnectSource source, CryMatchMakingConnectionID fromID, EDisconnectionCause cause, const char* reason);
	void                       SessionDisconnectRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id, eDisconnectSource source, CryMatchMakingConnectionID fromID, EDisconnectionCause cause);
	void                       StartSessionPendingGroupDisconnect(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionPendingGroupDisconnect(CryMatchMakingTaskID mmTaskID);
	void                       StartSessionModifyLocalFlags(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionModifyLocalFlags(CryMatchMakingTaskID mmTaskID);
	void                       StartSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionSetupDedicatedServerNameResolve(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionSetupDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       ProcessRequestSetupDedicatedServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       StartSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       OnSendCompleteSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID, CryLobbySendID sendID);
	void                       TickSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionReleaseDedicatedServer(CryMatchMakingTaskID mmTaskID);
	void                       ProcessRequestReleaseDedicatedServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       SendServerInfo(CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID, CryMatchMakingTaskID mmTaskID, CryLobbySendID sendIDs[MAX_LOBBY_CONNECTIONS]);
	void                       ProcessSessionServerInfo(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       SessionConnectToServer(CryLobbySessionHandle h, const TNetAddress& addr, SCryMatchMakingConnectionUID uid);
	void                       ProcessSessionRequestJoinServerResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       TickSessionConnectToServer(CryMatchMakingTaskID mmTaskID);
	void                       ProcessD2CHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);

	ESocketError               Send(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, const TNetAddress& to);
	ESocketError               Send(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID);
	void                       SendToAll(CryMatchMakingTaskID mmTaskID, CCryLobbyPacket* pPacket, CryLobbySessionHandle h, CryMatchMakingConnectionID skipID);
	void                       SendToAllClientsNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h);
	void                       SendToServerNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h);
	void                       SendToClientNT(TMemHdl mh, uint32 length, CryLobbySessionHandle h, SCryMatchMakingConnectionUID uid);
	void                       SendServerPing(CryLobbySessionHandle h);
	void                       ProcessServerPing(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void                       DispatchUserPacket(TMemHdl mh, uint32 length, CrySessionHandle gh);
	void                       InitialDispatchUserPackets(CryLobbySessionHandle h, CryMatchMakingConnectionID connectionID);
	void                       DispatchSessionUserDataEvent(ECryLobbySystemEvent event, TMemHdl mh);
	void                       SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, SCryUserInfoResult* pUserInfo);
	TMemHdl                    PacketToMemoryBlock(CCryLobbyPacket* pPacket, uint32& length);
	CCryLobbyPacket*           MemoryBlockToPacket(TMemHdl mh, uint32 length);

	void                       DispatchSessionEvent(CrySessionHandle h, ECryLobbySystemEvent event);

	void                       ProcessSessionDeleteRemoteConnection(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	static void                MatchmakingSessionJoinFromConsoleCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 ip, uint16 port, void* pArg);
	virtual const char*        GetConnectionName(SSession::SRConnection* pConnection, uint32 localUserIndex) const   { return NULL; }
	virtual uint64             GetConnectionUserID(SSession::SRConnection* pConnection, uint32 localUserIndex) const { return INVALID_USER_ID; }
	#ifndef _RELEASE
	void                       DrawDebugText();
	#endif
	#if NETWORK_HOST_MIGRATION
	void         ProcessHostHinting(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	bool         ProcessSingleHostHint(CryLobbySessionHandle h, uint32 uid, SHostHintInformation& hintToUpdate, SHostHintInformation& incomingHint);
	void         SortNewHostPriorities(CryLobbySessionHandle handle);
	static int   SortNewHostPriorities_Comparator(const void* pLHS, const void* pRHS);
	virtual int  SortNewHostPriorities_ComparatorHelper(const SHostHintInformation& LHS, const SHostHintInformation& RHS);
	virtual void OnHostMigrationFailedToStart(CryLobbySessionHandle h) {}
		#if HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION
	void         DetectHostMigrationTaskBreak(CryLobbySessionHandle h, char* pLocation);
		#endif

	CHostMigration        m_hostMigration;
		#if ENABLE_HOST_MIGRATION_STATE_CHECK
	CryLobbySessionHandle m_hostMigrationStateCheckSession;
		#endif
	#endif

	bool ExpireBans(TBannedUser& bannedUsers, const CTimeValue& tv);

	#if MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
	TNetAddress m_arbitratorAddr;
	#endif

	TBannedUser       m_resolvedBannedUser;
	TBannedUser       m_unresolvedBannedUser;
	uint32            m_connectionUIDCounter;
	// CCryDedicatedServer uses the same member names for the non-pointer versions of m_sessions and m_task. See CCryDedicatedServer::CCryDedicatedServer()
	// cppcheck-suppress duplInheritedMember
	CryLobbyIDArray<SSession*, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;
	// cppcheck-suppress duplInheritedMember
	STask*            m_task[MAX_MATCHMAKING_TASKS];
	CCryLobby*        m_lobby;
	CCryLobbyService* m_pService;
	ECryLobbyService  m_serviceType;
};

#endif // USE_CRY_MATCHMAKING
#endif // __CRYMATCHMAKING_H__
