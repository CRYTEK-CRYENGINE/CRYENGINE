// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYLOBBY_H__
#define __CRYLOBBY_H__

#pragma once

// Global lobby defines moved from CryNetwork/Config.h
#define USE_LAN                      1
#define USE_LOBBY_REMOTE_CONNECTIONS 1
#define USE_CRY_MATCHMAKING          1
#define USE_CRY_VOICE                1
#define USE_CRY_ONLINE_STORAGE       1
#define USE_CRY_STATS                1
#define USE_CRY_FRIENDS              1
#define USE_CRY_TCPSERVICE           1

#if PC_CONSOLE_NET_COMPATIBLE
	#define NETWORK_HOST_MIGRATION (1)
#else
	#define NETWORK_HOST_MIGRATION (0)
#endif

#if NETWORK_HOST_MIGRATION
	#if defined(PURE_CLIENT)
		#error Pure clients DO NOT support host migration
	#endif
#endif

#ifdef _RELEASE
	#define ENABLE_CRYLOBBY_DEBUG_TESTS              0
	#define HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION 0
	#define ENABLE_HOST_MIGRATION_STATE_CHECK        0
#else
	#define ENABLE_CRYLOBBY_DEBUG_TESTS              1
	#define HOST_MIGRATION_SOAK_TEST_BREAK_DETECTION (1 && NETWORK_HOST_MIGRATION)
	#define ENABLE_HOST_MIGRATION_STATE_CHECK        NETWORK_HOST_MIGRATION
#endif


#define TO_GAME_FROM_LOBBY(...) { CCryLobby* pLobby = static_cast<CCryLobby*>(CCryLobby::GetLobby()); pLobby->LockToGameMutex(); pLobby->GetToGameQueue()->Add(__VA_ARGS__); pLobby->UnlockToGameMutex(); }
#define FROM_GAME_TO_LOBBY         static_cast<CCryLobby*>(CCryLobby::GetLobby())->GetFromGameQueue()->Add

#define USE_GFWL                   0

#define USE_LIVE                   (USE_GFWL)

#define RESET_CONNECTED_CONNECTION (0)

#if !PC_CONSOLE_NET_COMPATIBLE
	#define ENCRYPT_LOBBY_PACKETS (0)
#else
	#define ENCRYPT_LOBBY_PACKETS 0
#endif

// USE_CRY_DEDICATED_SERVER_ARBITRATOR
// When set and game is started in dedicated arbitrator mode CryLobby will also have a CryDedicatedServerArbitrator service.
// This service keeps track of dedicated servers and gives them out to game clients when requested.
// USE_CRY_DEDICATED_SERVER
// When set and game is started in dedicated server mode CryLobby will also have a CryDedicatedServer service.
// This service will register the dedicated server with the dedicated server arbitrator.
// MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
// When set CryMatchMaking has the function SessionSetupDedicatedServer implemented.
// This function will request dedicated server connection information from the dedicated server arbitrator.
// The game host calls this to get a server for their session.
#if CRY_PLATFORM_DESKTOP
	#define USE_CRY_DEDICATED_SERVER_ARBITRATOR        1
	#define USE_CRY_DEDICATED_SERVER                   1
#else
	#define USE_CRY_DEDICATED_SERVER_ARBITRATOR        0
	#define USE_CRY_DEDICATED_SERVER                   0
#endif
#define MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR 1

#include <NetLog.h>
#include <Config.h>
#include <CryLobby/ICryLobbyPrivate.h>
#include <CryLobby/CryLobbyPacket.h>
#include "LobbyCVars.h"

#include "CryTCPServiceFactory.h"
#include <CryLobby/ICryTCPService.h>
#include <Socket/NetResolver.h>

#if CRY_PLATFORM_ORBIS
	#define USE_PSN         0
	#define USE_NPTITLE_DAT 1
#else
	#define USE_PSN         0
	#define USE_NPTITLE_DAT 0
#endif

#if USE_CRY_VOICE
	#if CRY_PLATFORM_ORBIS
		#define USE_PSN_VOICE 0
	#else
		#define USE_PSN_VOICE 0
	#endif
#endif

#include "INetworkPrivate.h"
#include "WorkQueue.h"
#include "Socket/IDatagramSocket.h"
#include <CryMemory/BucketAllocator.h>

const ECryLobbyError eCLE_Pending = eCLE_NumErrors;

enum CryLobbyPacketType
{
	eLobbyPT_Ack,
	eLobbyPT_SessionDeleteRemoteConnection,
#if NETWORK_HOST_MIGRATION
	eLobbyPT_HostHinting,
	#if ENABLE_HOST_MIGRATION_STATE_CHECK
	eLobbyPT_HostMigrationStateCheckData,
	#endif
#endif
	eLobbyPT_ServerPing,
	eLobbyPT_ConnectionRequest,
	eLobbyPT_ConnectionRequestResult,
	eLobbyPT_Ping,
	eLobbyPT_Pong,
	eLobbyPT_VoiceMicrophoneRequest,
	eLobbyPT_VoiceMicrophoneNotification,
	eLobbyPT_SessionRequestJoinResult,

#if USE_CRY_DEDICATED_SERVER_ARBITRATOR || USE_CRY_DEDICATED_SERVER || MATCHMAKING_USES_DEDICATED_SERVER_ARBITRATOR
	eLobbyPT_D2A_DedicatedServerIsFree,
	eLobbyPT_A2D_DedicatedServerIsFreeAck,
	eLobbyPT_C2A_RequestSetupDedicatedServer,
	eLobbyPT_A2C_RequestSetupDedicatedServerResult,
	eLobbyPT_A2D_AllocateDedicatedServer,
	eLobbyPT_D2A_AllocateDedicatedServerAck,
	eLobbyPT_A2D_FreeDedicatedServer,
	eLobbyPT_C2A_RequestReleaseDedicatedServer,
	eLobbyPT_A2C_RequestReleaseDedicatedServerResult,
	eLobbyPT_D2C_HostMigrationStart,
	eLobbyPT_C2D_UpdateSessionID,
	eLobbyPT_SessionServerInfo,
	eLobbyPT_SessionRequestJoinServer,
	eLobbyPT_SessionRequestJoinServerResult,
#endif

	eLobbyPT_InvalidPackeType,
	eLobbyPT_EndType
};

#define CRYLOBBY_PACKET_TYPE_ENCODE_SEED 0xc3ea1511

#define CRYSHAREDLOBBY_PACKET_START      0
#define CRYSHAREDLOBBY_PACKET_END        (eLobbyPT_EndType - 1)
#define CRYONLINELOBBY_PACKET_START      (CRYSHAREDLOBBY_PACKET_END + 1)
#define CRYONLINELOBBY_PACKET_MAX        63
#define CRYLANLOBBY_PACKET_START         (CRYONLINELOBBY_PACKET_MAX + 1)
#define CRYLANLOBBY_PACKET_MAX           127

const uint32 CryLobbyTimeOut            = 2000;
const uint32 CryLobbySendInterval       = 300;
const uint32 CryLobbyInGameSendInterval = 2000; //-- update ping every 2 seconds in-game.

typedef uint32 CryPingAccumulator;

#define MAX_LOBBY_PACKET_SIZE       1200 // Same as fragment size so lobby packets never fragment.
#define MAX_LOBBY_CONNECTIONS       64
#define MAX_LOBBY_TASKS             10
#define MAX_LOBBY_TASK_DATAS        10
#define SEND_DATA_QUEUE_SIZE        32
#define NUM_LOBBY_PINGS             3
#define MAX_SOCKET_PORTS_TRY        16u
#define LOOPBACK_ADDRESS            (0x7f000001)
#define BROADCAST_ADDRESS           (0xffffffffu)

#define LOBBY_BUCKET_ALLOCATOR_SIZE (2 * 1024 * 1024)
#define LOBBY_GENERAL_HEAP_SIZE     (256 * 1024)

#define TMemHdl                     LobbyTMemHdl

typedef void* TMemHdl;
#define TMemInvalidHdl NULL

class CCryLobby;

template<class ID, uint32 size>
class CryLobbyID
{
public:
	CryLobbyID()
	{
		m_id = size;
	}

	CryLobbyID(uint32 id)
	{
#if !defined(_RELEASE)
		if (id > size)
		{
			CryFatalError("CryLobbyID is being constructed with %u valid values 0 <= id <= %u", id, size);
		}
#endif

		m_id = id;
	}

	inline uint32 GetID() const
	{
		return m_id;
	}

	inline bool operator==(const CryLobbyID& other) const
	{
		return m_id == other.m_id;
	}

	inline friend bool operator==(const uint32& lhs, const CryLobbyID& rhs)
	{
		return CryLobbyID(lhs) == rhs;
	}

	inline bool operator!=(const CryLobbyID& other) const
	{
		return !(*this == other);
	}

	inline friend bool operator!=(const uint32& lhs, const CryLobbyID& rhs)
	{
		return CryLobbyID(lhs) != rhs;
	}

	inline bool operator<(const CryLobbyID& other) const
	{
		return m_id < other.m_id;
	}

	inline const CryLobbyID<ID, size>& operator++()
	{
#if !defined(_RELEASE)
		if (m_id >= size)
		{
			CryFatalError("CryLobbyID is being pre-incremented when already at maximum value %u", size);
		}
#endif

		++m_id;
		return *this;
	}

	inline CryLobbyID<ID, size> operator++(int)
	{
#if !defined(_RELEASE)
		if (m_id >= size)
		{
			CryFatalError("CryLobbyID is being post-incremented when already at maximum value %u", size);
		}
#endif

		CryLobbyID<ID, size> result = *this;
		++m_id;
		return result;
	}

	bool IsValid()
	{
		return m_id < size;
	}

private:
	uint32 m_id;
};

template<class Element, class ID, uint32 size>
class CryLobbyIDArray
{
public:
	inline Element& operator[](const ID& id)
	{
#if !defined(_RELEASE)
		if (id.GetID() >= size)
		{
			CryFatalError("CryLobbyIDArray is being indexed with an invalid CryLobbyID %u", id.GetID());
		}
#endif

		return m_array[id.GetID()];
	}

	inline const Element& operator[](const ID& id) const
	{
#if !defined(_RELEASE)
		if (id.GetID() >= size)
		{
			CryFatalError("CryLobbyIDArray is being indexed with an invalid CryLobbyID %u", id.GetID());
		}
#endif

		return m_array[id.GetID()];
	}

	inline uint32 Size() const
	{
		return size;
	}

private:
	Element m_array[size];
};

typedef uint32 CryLobbyServiceTaskID;
const CryLobbyServiceTaskID CryLobbyServiceInvalidTaskID = 0xffffffff;

struct SCryLobbyConnectionID
{
};

typedef CryLobbyID<SCryLobbyConnectionID, MAX_LOBBY_CONNECTIONS> CryLobbyConnectionID;
#define CryLobbyInvalidConnectionID CryLobbyConnectionID()

typedef uint32 CryLobbySendID;
const CryLobbySendID CryLobbyInvalidSendID = 0xffffffff;

typedef CryLockT<CRYLOCK_RECURSIVE> CryLobbyMutex;

typedef void (*                     CryLobbyServiceCallback)(ECryLobbyError error, CCryLobby* arg, ECryLobbyService service);

class ICryLobbyGFWLExtras
{
public:
	virtual ~ICryLobbyGFWLExtras(){}
	virtual void   OnCreateDevice(void* pD3D, void* pD3DPP) = 0;
	virtual void   OnDeviceReset(void* pD3DPP) = 0;
	virtual void   OnDeviceDestroyed() = 0;
	virtual void   Render() = 0;
	virtual uint32 PreTranslateMessage(void* msg) = 0;
};

enum ECryLobbyConnectionState
{
	eCLCS_NotConnected,
	eCLCS_Pending,
	eCLCS_Connected,
	eCLCS_Freeing
};

///////////////////////////////////////////////////////////////////////////////////////////////
// A set of macros to help keep lobby logging consistent and make life easier when types change
#define PRFORMAT_LCID      "lcid %u"
#define PRARG_LCID(lcid)                      (lcid).GetID()
#define PRFORMAT_ADDR      "addr %s"
#define PRARG_ADDR(address)                   CCryLobby::GetResolver()->ToNumericString(address).c_str()
#define PRFORMAT_UID       "uid %016" PRIx64 ":%u"
#define PRARG_UID(uid)                        (uid).m_sid, (uid).m_uid
#define PRFORMAT_SH        "session %u"
#define PRARG_SH(sh)                          (sh).GetID()
#define PRFORMAT_MMCID     "mmcid %u"
#define PRARG_MMCID(mmcid)                    (mmcid).GetID()
#define PRFORMAT_LCINFO    PRFORMAT_LCID " " PRFORMAT_ADDR
#define PRARG_LCINFO(lcid, address)           PRARG_LCID(lcid), PRARG_ADDR(address)
#define PRFORMAT_SHMMCINFO PRFORMAT_SH         " " PRFORMAT_MMCID " " PRFORMAT_LCID " " PRFORMAT_UID
#define PRARG_SHMMCINFO(sh, mmcid, lcid, uid) PRARG_SH(sh), PRARG_MMCID(mmcid), PRARG_LCID(lcid), PRARG_UID(uid)
#define PRFORMAT_MMCINFO   PRFORMAT_MMCID        " " PRFORMAT_LCID " " PRFORMAT_UID
#define PRARG_MMCINFO(mmcid, lcid, uid)       PRARG_MMCID(mmcid), PRARG_LCID(lcid), PRARG_UID(uid)
///////////////////////////////////////////////////////////////////////////////////////////////

class CCryTCPServiceFactory;
typedef _smart_ptr<CCryTCPServiceFactory> CCryTCPServiceFactoryPtr;

class CCryLobbyService : public CMultiThreadRefCount, public ICryLobbyService
{
public:
	CCryLobbyService(CCryLobby* pLobby, ECryLobbyService service);
	virtual ~CCryLobbyService();

	virtual ECryLobbyError    Initialise(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);
	virtual ECryLobbyError    Terminate(ECryLobbyServiceFeatures features, CryLobbyServiceCallback pCB);

	virtual ICryTCPServicePtr GetTCPService(const char* pService);
	virtual ICryTCPServicePtr GetTCPService(const char* pServer, uint16 port, const char* pUrlPrefix);
	virtual ECryLobbyError    GetUserPrivileges(uint32 user, CryLobbyTaskID* pTaskID, CryLobbyPrivilegeCallback pCB, void* pCBArg);
	virtual ECryLobbyError    CheckProfanity(const char* const pString, CryLobbyTaskID* pTaskID, CryLobbyCheckProfanityCallback pCb, void* pCbArg) { return eCLE_ServiceNotSupported; };

	virtual void              Tick(CTimeValue tv) = 0;

	virtual void              MakeAddrPCCompatible(TNetAddress& addr);
	virtual void              OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket) = 0;
	virtual void              OnError(const TNetAddress& addr, ESocketError error, CryLobbySendID sendID) = 0;
	virtual void              OnSendComplete(const TNetAddress& addr, CryLobbySendID sendID) = 0;

	virtual bool              IsDead() const { return false; };

	virtual bool              GetFlag(ECryLobbyServiceFlag flag);
	virtual void              OnInternalSocketChanged(IDatagramSocketPtr pSocket) {};

	virtual void              GetSocketPorts(uint16& connectPort, uint16& listenPort);
	virtual void              CancelTask(CryLobbyTaskID lTaskID);

	void                      CreateSocketNT(void);
	void                      FreeSocketNT(void);

	virtual ICrySignIn*       GetSignIn() { return NULL; }

protected:
	enum ETask
	{
		eT_GetUserPrivileges,
		eT_LobbyPlatformSpecificTask
	};

	struct  STask
	{
		uint32         user;
		CryLobbyTaskID lTaskID;
		ECryLobbyError error;
		uint32         startedTask;
		void*          pCB;
		void*          pCBArg;
		uint32         dataNum[MAX_LOBBY_TASK_DATAS];
		TMemHdl        dataMem[MAX_LOBBY_TASK_DATAS];
		bool           used;
		bool           running;
	};

	STask*         GetTask(CryLobbyServiceTaskID id) { return &m_tasks[id]; }
	ECryLobbyError StartTask(uint32 eTask, uint32 user, bool startRunning, CryLobbyServiceTaskID* pLSTaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg);
	void           FreeTask(CryLobbyServiceTaskID lsTaskID);
	void           UpdateTaskError(CryLobbyServiceTaskID lsTaskID, ECryLobbyError error);
	void           StartTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void           StopTaskRunning(CryLobbyServiceTaskID lsTaskID);
	void           EndTask(CryLobbyServiceTaskID lsTaskID);
	ECryLobbyError CreateTaskParamMem(CryLobbyServiceTaskID lsTaskID, uint32 param, const void* pParamData, size_t paramDataSize);

	// In the future it would be good to move the common task handling code into CCryLobbyService and the common platform task handling code into the platform lobby services.
	// Because of this tasks should always be accessed via GetTask() as m_tasks will be removed when the platform task data gets added.
	STask                    m_tasks[MAX_LOBBY_TASKS];
	CCryLobby*               m_pLobby;
	ECryLobbyService         m_service;
#if USE_CRY_TCPSERVICE
	CCryTCPServiceFactoryPtr m_pTCPServiceFactory;
#endif // USE_CRY_TCPSERVICE
	uint32                   m_lobbyServiceFlags;
};

typedef _smart_ptr<CCryLobbyService>                                          CCryLobbyServicePtr;
typedef std::multimap<EListenerPriorityType, SHostMigrationEventListenerInfo> THostMigrationEventListenerContainer;

class CCryLobby : public ICryLobbyPrivate, public IDatagramListener
#if NETWORK_HOST_MIGRATION
	                , public IHostMigrationEventListener
#endif
{
public:
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(ICryLobby)
		CRYINTERFACE_ADD(Cry::IEnginePlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CCryLobby, "Plugin_CryLobby", "31A1557A-0DBA-4CF8-AD79-86E97CD47A4B"_cry_guid);

	CCryLobby();
	virtual ~CCryLobby();

	// Cry::IEnginePlugin
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~Cry::IEnginePlugin

	static ICryLobby*                  GetLobby() { return m_pLobby; }

#if NETWORK_HOST_MIGRATION
	virtual CLobbyCVars& GetCVars() override { return m_cvars; }
#endif
	virtual void Tick(bool flush) override;

	virtual ECryLobbyError         Initialise(ECryLobbyService service, ECryLobbyServiceFeatures features, CryLobbyConfigurationCallback cfgCb, CryLobbyCallback cb, void* cbArg) override;
	virtual ECryLobbyError         Terminate(ECryLobbyService service, ECryLobbyServiceFeatures features, CryLobbyCallback cb, void* cbArg) override;
	virtual ECryLobbyError         ProcessEvents() override;
	virtual ECryLobbyService       SetLobbyService(ECryLobbyService service) override;
	virtual ECryLobbyService       GetLobbyServiceType() override
		{ return m_service; }
	virtual ICryLobbyService*      GetLobbyService() override
		{ return m_services[m_service]; }
	virtual ICryMatchMaking*       GetMatchMaking() override
		{ return m_services[m_service] ? m_services[m_service]->GetMatchMaking() : NULL; }
	virtual ICryMatchMakingPrivate* GetMatchMakingPrivate() override
		{ return static_cast<ICryMatchMakingPrivate *>(GetMatchMaking());	}
	virtual ICryMatchMakingConsoleCommands* GetMatchMakingConsoleCommands() override
		{ return static_cast<ICryMatchMakingConsoleCommands *>(GetMatchMaking());	}
	virtual ICryVoice*             GetVoice() override
		{ return m_services[m_service] ? m_services[m_service]->GetVoice() : NULL; }
	virtual ICryReward*            GetReward() override
		{ return m_services[m_service] ? m_services[m_service]->GetReward() : NULL; }
	virtual ICryStats*             GetStats() override
		{ return m_services[m_service] ? m_services[m_service]->GetStats() : NULL; }
	virtual ICryOnlineStorage*     GetOnlineStorage() override
		{ return m_services[m_service] ? m_services[m_service]->GetOnlineStorage() : NULL; }
	virtual ICryLobbyUI*           GetLobbyUI() override
		{ return m_services[m_service] ? m_services[m_service]->GetLobbyUI() : NULL; }
	virtual ICryFriends*           GetFriends() override
		{ return m_services[m_service] ? m_services[m_service]->GetFriends() : NULL; }
	virtual ICryFriendsManagement* GetFriendsManagement() override
		{ return m_services[m_service] ? m_services[m_service]->GetFriendsManagement() : NULL; }
	virtual ICryLobbyService*      GetLobbyService(ECryLobbyService service) override
		{ assert(service < eCLS_NumServices); return m_services[service]; }
	virtual ICryLobbyGFWLExtras*   GetLobbyExtras()
		{ return m_gfwlExtras; }

	virtual const SCryLobbyParameters& GetLobbyParameters() const override;
	virtual void                       SetUserPacketEnd(uint32 end) override { m_userPacketEnd = end; CalculatePacketTypeEncoding(); }

#if NETWORK_HOST_MIGRATION
	virtual EHostMigrationReturn OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual void                 OnComplete(SHostMigrationInfo& hostMigrationInfo) override {}
	virtual EHostMigrationReturn OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	virtual EHostMigrationReturn OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) override;
	uint8                        GetActiveConnections(void);
#endif
	virtual void                 AddHostMigrationEventListener(IHostMigrationEventListener* pListener, const char* pWho, uint32 priority) override;
	virtual void                 RemoveHostMigrationEventListener(IHostMigrationEventListener* pListener) override;

	virtual void                 RegisterEventInterest(ECryLobbySystemEvent eventOfInterest, CryLobbyEventCallback cb, void* pUserData) override;
	virtual void                 UnregisterEventInterest(ECryLobbySystemEvent eventOfInterest, CryLobbyEventCallback cb, void* pUserData) override;

	// Description:
	//	 Is the given flag true for the lobby service of the given type?
	// Arguments:
	//	 service - lobby service to be queried
	//	 flag - flag to be queried
	// Return:
	//	 True if the flag is true.
	virtual bool        GetLobbyServiceFlag(ECryLobbyService service, ECryLobbyServiceFlag flag) override;

	virtual void        SetCryEngineLoadHint(const char* pLevelName, const char* pGameRules) override
		{ cry_strcpy(m_lvlName, pLevelName); cry_strcpy(m_rulName, pGameRules); }
	virtual const char* GetCryEngineLevelNameHint() override                                 { return m_lvlName; }
	virtual const char* GetCryEngineRulesNameHint() override                                 { return m_rulName; }

	void                DispatchEvent(ECryLobbySystemEvent evnt, UCryLobbyEventData data);

	void                GetConfigurationInformation(SConfigurationParams* infos, uint32 infoCnt);

	void                InviteAccepted(ECryLobbyService service, uint32 user, CrySessionID sessionID, ECryLobbyError error);
	void                InviteAccepted(ECryLobbyService service, uint32 user, CryUserID userID, ECryLobbyError error, ECryLobbyInviteType inviteType);

	CWorkQueue*         GetToGameQueue()   { return &m_toGameQueue; }
	CWorkQueue*         GetFromGameQueue() { return &m_fromGameQueue; }

	CryLobbyTaskID      CreateTask();
	void                ReleaseTask(CryLobbyTaskID id);
#if ENABLE_CRYLOBBY_DEBUG_TESTS
	bool                DebugGenerateError(CryLobbyTaskID id, ECryLobbyError& error);
	bool                DebugOKToStartTaskRunning(CryLobbyTaskID id);
	bool                DebugTickCallStartTaskRunning(CryLobbyTaskID id);
	bool                DebugOKToTickTask(CryLobbyTaskID id, bool running);
#endif

	void                     InternalSocketCreate(ECryLobbyService service);
	void                     InternalSocketFree(ECryLobbyService service);
	uint16                   GetInternalSocketPort(ECryLobbyService service);
	IDatagramSocketPtr       GetInternalSocket(ECryLobbyService service);
	ESocketError             Send(CCryLobbyPacket* pPacket, const TNetAddress& to, CryLobbyConnectionID connectionID, CryLobbySendID* pSendID);
	ESocketError             SendVoice(CCryLobbyPacket* pPacket, const TNetAddress& to);
	CryLobbyConnectionID     CreateConnection(const TNetAddress& address);
	CryLobbyConnectionID     FindConnection(const TNetAddress& address);
	void                     FreeConnection(CryLobbyConnectionID c);
	void                     ConnectionAddRef(CryLobbyConnectionID c);
	void                     ConnectionRemoveRef(CryLobbyConnectionID c);
	bool                     ConnectionHasReference(CryLobbyConnectionID c);
	void                     ConnectionSetState(CryLobbyConnectionID c, ECryLobbyConnectionState state);
	ECryLobbyConnectionState ConnectionGetState(CryLobbyConnectionID c);
	bool                     ConnectionFromAddress(CryLobbyConnectionID* connection, const TNetAddress& address);
	bool                     AddressFromConnection(TNetAddress& address, CryLobbyConnectionID connection);
	bool                     SetConnectionAddress(CryLobbyConnectionID connection, TNetAddress& address);
	virtual CryPing          GetConnectionPing(CryLobbyConnectionID connectionID);
	void                     FlushMessageQueue(void);
	void                     SetNATType(ENatType natType) { m_natType = natType;  }
	ENatType                 GetNATType(void) const       { return m_natType;     }

	//start of IDatagramListener
	virtual void                OnPacket(const TNetAddress& addr, const uint8* pData, uint32 length) override;
	virtual void                OnError(const TNetAddress& addr, ESocketError error) override;
	//end of IDatagramListener
	void                        OnSendComplete(const TNetAddress& addr);
	void                        ProcessPacket(const TNetAddress& addr, CryLobbyConnectionID connectionID, CCryLobbyPacket* pPacket);
	void                        ProcessCachedPacketBuffer(void);

	CryLobbyMutex&              GetMutex()             { return m_mutex; }
	void                        MutexLock()            { m_mutex.Lock(); }
	bool                        MutexTryLock()         { return m_mutex.TryLock(); }
	void                        MutexUnlock()          { m_mutex.Unlock(); }
	TMemHdl                     MemAlloc(size_t sz)    { return m_memAllocator.MemAlloc(sz); }
	void                        MemFree(TMemHdl h)     { m_memAllocator.MemFree(h); }
	void*                       MemGetPtr(TMemHdl h)   { return m_memAllocator.MemGetPtr(h); }
	TMemHdl                     MemGetHdl(void* ptr)   { return m_memAllocator.MemGetHdl(ptr); }

	void                        LockToGameMutex()      { m_safetyToGameMutex.Lock(); }
	void                        UnlockToGameMutex()    { m_safetyToGameMutex.Unlock(); }

	uint32                      TimeSincePacketInMS(CryLobbyConnectionID c) const;
	void                        ForceTimeoutConnection(CryLobbyConnectionID c);
	void                        SetServicePacketEnd(ECryLobbyService service, uint32 end) { m_servicePacketEnd[service] = end; CalculatePacketTypeEncoding(); }
	const TNetAddress*          GetNetAddress(CryLobbyConnectionID c) const;

	static bool                 DecodeAddress(const TNetAddress& address, uint32* pIP, uint16* pPort);
	static void                 DecodeAddress(uint32 ip, uint16 port, char* ipstring, bool ignorePort);
	static void                 DecodeAddress(const TNetAddress& address, char* ipstring, bool ignorePort);

	static bool                 ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR_IN* pSockAddr);
	static bool                 ConvertAddr(const TNetAddress& addrIn, CRYSOCKADDR* pSockAddr, int* addrLen);

	static ISocketIOManager*    GetExternalSocketIOManager();
	static ISocketIOManager*    GetInternalSocketIOManager();
	static CNetAddressResolver* GetResolver();
	static const CNetCVars*     GetNetCVars();

	static uint8                GetLobbyPacketID();

#if NETWORK_HOST_MIGRATION
	THostMigrationEventListenerContainer* GetHostMigrationListeners();
#endif

	struct SSocketService                                   // Ideally we could move these into the services themselves, but maybe this will result in
	{
		//less duplication. For now, a seperate socket will only be created if the socketTearDown flag
		IDatagramSocketPtr m_socket;                          //is not set for the service. To allow this to work, services that don't set the flag are
		                                                      //required to use unique ports.
		uint16             m_socketListenPort;
		uint16             m_socketConnectPort;
	};

	void             MakeAddrPCCompatible(TNetAddress& addr);
	void             MakeConnectionPCCompatible(CryLobbyConnectionID connectionID);
	SSocketService*  GetCorrectSocketServiceForAddr(const TNetAddress& addr);
	ECryLobbyService GetCorrectSocketServiceTypeForAddr(const TNetAddress& addr);
	SSocketService*  GetCorrectSocketService(ECryLobbyService serviceType);

private:
	const uint8*   EncryptPacket(const uint8* buffer, uint32 length);
	const uint8*   DecryptPacket(const uint8* buffer, uint32 length);

	static void    InitialiseServiceCB(ECryLobbyError error, CCryLobby* arg, ECryLobbyService service);
	static void    TerminateServiceCB(ECryLobbyError error, CCryLobby* arg, ECryLobbyService service);
	void           InternalSocketDie(ECryLobbyService service);
	ECryLobbyError CheckAllocGlobalResources();
	void           CheckFreeGlobalResources();
	uint32         GetDisconnectTimeOut();
	void           UpdateConnectionPing(CryLobbyConnectionID connectionID, CryPing ping);
	bool           AddPacketToReliableBuildPacket(CryLobbyConnectionID connectionID, CCryLobbyPacket* pPacket);
	bool           AddReliablePacketToSendQueue(CryLobbyConnectionID connectionID, CCryLobbyPacket* pPacket);
	void           LogPacketsInBuffer(const uint8* pBuffer, uint32 size);
	bool           GetNextPacketFromBuffer(const uint8* pData, uint32 dataSize, uint32& dataPos, CCryLobbyPacket* pPacket);
	void           CalculatePacketTypeEncoding();
	void           EncodePacketDataHeader(CCryLobbyPacket* pPacket);
	void           DecodePacketDataHeader(CCryLobbyPacket* pPacket);
	bool           KeepPacketAfterDisconnect(uint32 packetType);

	static ICryLobby* m_pLobby;
	CLobbyCVars       m_cvars;

	char              m_lvlName[128];
	char              m_rulName[128];

	struct SEventCBData
	{
		ECryLobbySystemEvent  event;
		CryLobbyEventCallback cb;
		void*                 pUserData;

		bool operator==(const SEventCBData& other) const { return (this->event == other.event && this->cb == other.cb && this->pUserData == other.pUserData); }
	};

	typedef std::vector<SEventCBData> EventCBList;

	CTimeValue           m_lastTickTime;
	EventCBList          m_callbacks;

	ECryLobbyService     m_service;
	CCryLobbyServicePtr  m_services[eCLS_NumServices];
	SCryLobbyParameters  m_serviceParams[eCLS_NumServices];

	ICryLobbyGFWLExtras* m_gfwlExtras;

	struct STask
	{
		CryLobbyCallback cb;
		void*            cbArg;
	} m_task[eCLS_NumServices];

#if USE_LOBBY_REMOTE_CONNECTIONS
	struct SConnection
	{
	#if RESET_CONNECTED_CONNECTION
		uint64          sendCookie;
		uint64          recvCookie;
	#endif
		uint32          timeSinceSend;
		uint32          timeSinceRecv;
		uint32          disconnectTimer;
		TNetAddress     addr;

		uint8           reliableBuildPacketBuffer[MAX_LOBBY_PACKET_SIZE];
		CCryLobbyPacket reliableBuildPacket;

		struct SData
		{
			TMemHdl data;
			uint16  dataSize;
			uint8   counter;
		};

		MiniQueue<SData, SEND_DATA_QUEUE_SIZE> dataQueue;
		ECryLobbyConnectionState               state;

		struct SPing
		{
			CryPing aveTime;
			CryPing times[NUM_LOBBY_PINGS];
			CryPing currentTime;
		}     ping;

		uint8 refCount;
		uint8 counterIn;
		uint8 counterOut;
		bool  used;

		// All members are expected to be uninitialized if !used
		// cppcheck-suppress uninitMemberVar
		SConnection() : used(false) {}
	};

	CryLobbyIDArray<SConnection, CryLobbyConnectionID, MAX_LOBBY_CONNECTIONS> m_connection;
#endif //USE_LOBBY_REMOTE_CONNECTIONS

	class CCachedPacketBuffer
	{
	public:
		CCachedPacketBuffer(void)
#if DEBUG_CACHED_PACKET_BUFFER
			: m_highWatermark(0)
#endif // DEBUG_CACHED_PACKET_BUFFER
		{
			Reset();
		}

		~CCachedPacketBuffer(void)
		{
#if DEBUG_CACHED_PACKET_BUFFER
			CryLogAlways("CLobby::CCachedPacketBuffer high water mark = %u bytes", m_highWatermark);
#endif // DEBUG_CACHED_PACKET_BUFFER
		}

		void Reset(void)
		{
			m_writeIndex = 0;
			m_readIndex = 0;
		}

		bool WritePacket(const TNetAddress& addr, const uint8* pData, uint32 length)
		{
			uint32 spaceRequired = sizeof(CTimeValue) + sizeof(addr) + sizeof(length) + length;
			uint32 spaceAvailable = CACHED_PACKET_BUFFER_MEMORY - m_writeIndex;
			bool written = false;

			if (spaceRequired <= spaceAvailable)
			{
				const CTimeValue storedTime = gEnv->pSystem->GetITimer()->GetAsyncTime();
				memcpy(&m_buffer[m_writeIndex], &storedTime, sizeof(storedTime));
				m_writeIndex += sizeof(storedTime);

				memcpy(&m_buffer[m_writeIndex], &addr, sizeof(addr));
				m_writeIndex += sizeof(addr);

				memcpy(&m_buffer[m_writeIndex], &length, sizeof(length));
				m_writeIndex += sizeof(length);

				memcpy(&m_buffer[m_writeIndex], pData, length);
				m_writeIndex += length;

#if DEBUG_CACHED_PACKET_BUFFER
				if (m_writeIndex > m_highWatermark)
				{
					m_highWatermark = m_writeIndex;
				}
#endif  // DEBUG_CACHED_PACKET_BUFFER

				written = true;
			}
			else
			{
				NetLog("CCachedPacketBuffer::WritePacket() buffer full! capacity [%d], used [%u], available [%u], requested [%u]", CACHED_PACKET_BUFFER_MEMORY, m_writeIndex, spaceAvailable, spaceRequired);
			}

			return written;
		}

		bool ReadPacket(CTimeValue& recvTime, TNetAddress& addr, const uint8*& pData, uint32& length)
		{
			bool read = false;

			if (PacketsAvailable())
			{
				memcpy(&recvTime, &m_buffer[m_readIndex], sizeof(recvTime));
				m_readIndex += sizeof(recvTime);

				memcpy(&addr, &m_buffer[m_readIndex], sizeof(addr));
				m_readIndex += sizeof(addr);

				memcpy(&length, &m_buffer[m_readIndex], sizeof(length));
				m_readIndex += sizeof(length);

				pData = &m_buffer[m_readIndex];
				m_readIndex += length;

				read = true;
			}

			return read;
		}

		bool PacketsAvailable(void) const { return (m_readIndex < m_writeIndex); }

	protected:
		uint8  m_buffer[CACHED_PACKET_BUFFER_MEMORY];
		uint32 m_writeIndex;
		uint32 m_readIndex;
#if DEBUG_CACHED_PACKET_BUFFER
		uint32 m_highWatermark;
#endif // DEBUG_CACHED_PACKET_BUFFER
	};

	struct SServiceTask
	{
		bool used;

#if ENABLE_CRYLOBBY_DEBUG_TESTS
		CTimeValue startTaskTime;
		CTimeValue tickTaskTime;
		bool       generateErrorDone;
		bool       startTaskTimerStarted;
		bool       startTaskDone;
		bool       tickTaskTimerStarted;
		bool       tickTaskDone;
#endif
	} m_serviceTask[MAX_LOBBY_TASKS];

	class CMemAllocator
	{
	public:
		CMemAllocator();
		~CMemAllocator();

		TMemHdl MemAlloc(size_t sz);
		void    MemFree(TMemHdl h);
		void*   MemGetPtr(TMemHdl h);
		TMemHdl MemGetHdl(void* ptr);
		void    PrintStats();

	private:
#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
		typedef BucketAllocator<BucketAllocatorDetail::DefaultTraits<LOBBY_BUCKET_ALLOCATOR_SIZE, BucketAllocatorDetail::SyncPolicyUnlocked, false>> LobbyBuckets;
#else
		typedef node_alloc<eCryDefaultMalloc, true, LOBBY_BUCKET_ALLOCATOR_SIZE>                                                                     LobbyBuckets;
#endif

		static LobbyBuckets m_bucketAllocator;
		IGeneralMemoryHeap* m_pGeneralHeap;

		size_t              m_totalBucketAllocated;
		size_t              m_totalGeneralHeapAllocated;
		size_t              m_MaxBucketAllocated;
		size_t              m_MaxGeneralHeapAllocated;
	};

	CMemAllocator                 m_memAllocator;

	CWorkQueue                    m_toGameQueue;
	CWorkQueue                    m_fromGameQueue;

	CryLobbyMutex                 m_mutex;
	CryLobbyMutex                 m_safetyToGameMutex;
	CryLobbyConfigurationCallback m_configCB;   // allows platform specific configuration to be requested by any attached service
	ENatType                      m_natType;

	SSocketService                m_socketServices[eCLS_NumServices];
	CCachedPacketBuffer           m_cachedPacketBuffer;

	/////////////////////////////////////////////////////////////////////////////
	// Host Migration event listener
#if NETWORK_HOST_MIGRATION
	THostMigrationEventListenerContainer m_hostMigrationListeners;
#endif
	//
	/////////////////////////////////////////////////////////////////////////////

	uint16 m_servicePacketEnd[eCLS_NumServices];
	uint16 m_userPacketEnd;
	uint8  m_encodePacketType[256];
	uint8  m_decodePacketType[256];

	enum ELobbyHostMigrationStatus
	{
		eLHMS_InitiateMigrate,
		eLHMS_Migrating
	};

#if ENCRYPT_LOBBY_PACKETS
	TCipher m_cipher;
	uint8   m_tempEncryptionBuffer[MAX_LOBBY_PACKET_SIZE];
	uint8   m_tempDecryptionBuffer[MAX_LOBBY_PACKET_SIZE];
#endif
};

struct SCrySharedUserID : public SCryUserID
{
	void* operator new(size_t size) throw()
	{
		CCryLobby* pLobby = (CCryLobby*)CCryLobby::GetLobby();

		if (pLobby)
		{
			TMemHdl hdl = pLobby->MemAlloc(size);

			if (hdl != TMemInvalidHdl)
			{
				SCrySharedUserID* id = (SCrySharedUserID*)pLobby->MemGetPtr(hdl);

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
				pLobby->MemFree(((SCrySharedUserID*)p)->hdl);
			}
		}
	}

	TMemHdl hdl;
};

#define LOBBY_AUTO_LOCK AUTO_LOCK_T(CryLobbyMutex, ((CCryLobby*)CCryLobby::GetLobby())->GetMutex())

inline CryLobbySendID CryLobbyCreateSendID(CryLobbyConnectionID connection, uint32 count)
{
	return connection.GetID() | (count << 16);
}

inline CryLobbyConnectionID CryLobbyGetConnectionIDFromSendID(CryLobbySendID id)
{
	return id & 0xffff;
}

extern CTimeValue g_time;

#endif // __CRYLOBBY_H__
