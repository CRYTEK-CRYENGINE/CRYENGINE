// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryMatchMaking.h"
#include "Platform.h"

// XXX - ATG
class ChatIntegrationLayer;

#if USE_DURANGOLIVE && USE_CRY_MATCHMAKING

	#include "CryDurangoLiveLobbyPacket.h"

struct SCryDurangoLiveSessionID : public SCrySharedSessionID
{
	GUID m_sessionID;

	SCryDurangoLiveSessionID(GUID sessionId) : m_sessionID(sessionId)
	{
	}

	virtual bool operator==(const SCrySessionID& other)
	{
		return !!IsEqualGUID(m_sessionID, reinterpret_cast<SCryDurangoLiveSessionID const*>(&other)->m_sessionID);
	}
	virtual bool operator<(const SCrySessionID& other)
	{
		return (memcmp(&m_sessionID, &reinterpret_cast<SCryDurangoLiveSessionID const*>(&other)->m_sessionID, sizeof(m_sessionID)) < 0);
	}
	bool IsFromInvite() const
	{
		return false;
	}

	void AsCStr(char* pOutString, int inBufferSize) const
	{
		pOutString[0] = 0;
	}
};

class CCryDurangoLiveMatchMaking : public CCryMatchMaking
{
public:
	CCryDurangoLiveMatchMaking(CCryLobby* pLobby, CCryLobbyService* pService, ECryLobbyService serviceType);

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();
	void                   Tick(CTimeValue tv);

	void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);

	virtual ECryLobbyError SetINetworkingUser(Live::State::INetworkingUser* user) override;
	virtual ECryLobbyError SessionRegisterUserData(SCrySessionUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionCreate(uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionMigrate(CrySessionHandle gh, uint32* pUsers, int numUsers, uint32 flags, SCrySessionData* pData, CryLobbyTaskID* pTaskID, CryMatchmakingSessionCreateCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionUpdate(CrySessionHandle gh, SCrySessionUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionUpdateSlots(CrySessionHandle gh, uint32 numPublic, uint32 numPrivate, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionQuery(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionQueryCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionGetUsers(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetUsersCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionStart(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionEnd(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionDelete(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionSearch(uint32 user, SCrySessionSearchParam* pParam, CryLobbyTaskID* pTaskID, CryMatchmakingSessionSearchCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionJoin(uint32* pUsers, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionJoinCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionSetLocalUserData(CrySessionHandle gh, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg);
	virtual CrySessionID   SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h);

	virtual uint32         GetSessionIDSizeInPacket() const;
	virtual ECryLobbyError WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const;
	virtual CrySessionID   ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const;

	virtual ECryLobbyError SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	#if NETWORK_HOST_MIGRATION
	virtual void           HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual void           HostMigrationFinalise(CryLobbySessionHandle h);
	virtual ECryLobbyError HostMigrationServer(SHostMigrationInfo* pInfo);
	virtual bool           GetNewHostAddress(char* pAddress, SHostMigrationInfo* pInfo);
	#endif

	TNetAddress GetHostAddressFromSessionHandle(CrySessionHandle gh);

	// XXX - ATG
	friend class ChatIntegrationLayer;

private:
	enum ETask
	{
		eT_SessionRegisterUserData = eT_MatchMakingPlatformSpecificTask,
		eT_SessionCreate,
		eT_SessionMigrate,
		eT_SessionUpdate,
		eT_SessionUpdateSlots,
		eT_SessionStart,
		eT_SessionGetUsers,
		eT_SessionEnd,
		eT_SessionDelete,
		eT_SessionSearch,
		eT_SessionJoin,
		eT_SessionJoinCreateAssociation,
		eT_SessionRequestJoin,
		eT_SessionQuery,
		eT_SessionSetLocalUserData,

		// ^^New task IDs *must* go before host migration tasks^^
		eT_SessionMigrateHostStart = HOST_MIGRATION_TASK_ID_FLAG,
		eT_SessionMigrateHostServer,
	};

	struct  SRegisteredUserData
	{
		SCryLobbyUserData data[MAX_MATCHMAKING_SESSION_USER_DATA];
		uint32            num;
	};

	struct  SSession : public CCryMatchMaking::SSession
	{
		virtual const char* GetLocalUserName(uint32 localUserIndex) const;
		virtual void        Reset();

		struct SLConnection : public CCryMatchMaking::SSession::SLConnection
		{
			Live::Xuid xuid;
			char       name[CRYLOBBY_USER_NAME_LENGTH];
			uint8      userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
		};

		struct SRConnection : public CCryMatchMaking::SSession::SRConnection
		{
			enum ERemoteConnectionState
			{
				eRCS_CreatingAssociation,
				eRCS_CreateAssociationSuccess,
				eRCS_CreateAssociationFailed
			};

			Live::Xuid xuid;
			Microsoft::WRL::ComPtr<ABI::Windows::Xbox::Networking::ISecureDeviceAddress>     pSecureDeviceAddress;
			Microsoft::WRL::ComPtr<ABI::Windows::Xbox::Networking::ISecureDeviceAssociation> pSecureDeviceAssociation;
			ERemoteConnectionState state;
			char                   name[CRYLOBBY_USER_NAME_LENGTH];
			uint8                  userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
		};

		GUID         sessionID;
		SLConnection localConnection;
		CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;
	};

	struct  STask : public CCryMatchMaking::STask
	{
		enum EAsyncTaskState
		{
			eATS_Pending,
			eATS_Success,
			eATS_Failed
		};

		EAsyncTaskState asyncTaskState;
	};

	CryLobbySessionHandle      FindSessionFromID(GUID const& sessionID);
	void                       StartAsyncActionTask(CryMatchMakingTaskID mmTaskID, ABI::Windows::Foundation::IAsyncAction* pAsyncAction);
	void                       StartConcurrencyTaskTask(CryMatchMakingTaskID mmTaskID, concurrency::task<HRESULT> pTask);
	ECryLobbyError             StartTask(ETask etask, bool startRunning, CryMatchMakingTaskID* pMMTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCB, void* pCBArg);
	void                       StartSubTask(ETask etask, CryMatchMakingTaskID mmTaskID);
	void                       StartTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       StopTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       EndTask(CryMatchMakingTaskID mmTaskID);
	ECryLobbyError             CreateSessionHandle(CryLobbySessionHandle* pH, bool host, uint32 createFlags, int numUsers);
	CryMatchMakingConnectionID AddRemoteConnection(CryLobbySessionHandle h, ABI::Windows::Xbox::Networking::ISecureDeviceAddress* pSecureDeviceAddress, ABI::Windows::Xbox::Networking::ISecureDeviceAssociation* pSecureDeviceAssociation, Live::Xuid const xuid);
	virtual void               FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	concurrency::task<HRESULT> FreeRemoteConnectionAsync(CryLobbySessionHandle h, CryMatchMakingConnectionID id);

	virtual uint64             GetSIDFromSessionHandle(CryLobbySessionHandle h);

	void                       StartCreateSecureDeviceAssociation(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void                       CreateSecureDeviceAssociationHandler(ABI::Windows::Xbox::Networking::ISecureDeviceAssociation* pSecureDeviceAssociation, CryLobbySessionHandle h, CryMatchMakingConnectionID id);

	void                       StartSessionCreate(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionCreate(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionCreate(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionSearch(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionSearch(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionSearch(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionUpdate(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionUpdate(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionUpdateSlots(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionUpdateSlots(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionStart(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionStart(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionEnd(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionEnd(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionJoinCreateAssociation(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionRequestJoin(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       ProcessSessionRequestJoin(const TNetAddress& addr, CCryDurangoLiveLobbyPacket* pPacket);
	void                       ProcessSessionRequestJoinResult(const TNetAddress& addr, CCryDurangoLiveLobbyPacket* pPacket);

	void                       StartSessionDelete(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionDelete(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionGetUsers(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionGetUsers(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionQuery(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionQuery(CryMatchMakingTaskID mmTaskID);

	#if NETWORK_HOST_MIGRATION
	void                HostMigrationServerNT(CryMatchMakingTaskID mmTaskID);
	void                TickHostMigrationServerNT(CryMatchMakingTaskID mmTaskID);
	void                HostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	void                TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	#endif
	void                LogConnectionStatus();
	void                SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void                ConnectionEstablishedUserDataEvent(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void                InitialUserDataEvent(CryLobbySessionHandle h);

	void                ResetTask(CryMatchMakingTaskID mmTaskID);
	virtual const char* GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const;
	virtual uint64      GetConnectionUserID(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const;

	SRegisteredUserData m_registeredUserData;
	CryLobbyIDArray<SSession, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;
	STask               m_task[MAX_MATCHMAKING_TASKS];

	Microsoft::WRL::ComPtr<ABI::Windows::Xbox::Networking::ISecureDeviceAssociationTemplate> m_pSecureDeviceAssociationTemplate;
	Live::State::INetworkingUser* m_user;

	EventRegistrationToken        m_AssociationIncoming;
};

#endif // USE_DURANGOLIVE && USE_CRY_MATCHMAKING
