// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_MATCHMAKING_H__
#define __CRYPSN2_MATCHMAKING_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include "CryMatchMaking.h"
		#include "CryPSN2Support.h"
		#include <np.h>

		#if USE_CRY_MATCHMAKING

			#define MAX_PSN_LOCAL_USERS     (1)
			#define MAX_PSN_ROOM_MEMBERS    (16)  // enough for c2
			#define MAX_ATTRIB_IDS          (10)  // 8 searchable ints + 2 binary packets
			#define MAX_PSN_QUERYABLE_ROOMS (1)   // query API only allows for one room request at present
			#define MAX_PSN_QUERYABLE_ATTRS (10)  // Enough for all defined external room attributes

			#define MAX_WORLDS_TO_SEARCH    (10)
			#define MAX_ROOMS_TO_STORE      (50)  // Originally set to SCE_NP_MATCHING2_RANGE_FILTER_MAX (=20), which is the maximum number of results that
                                            // can be returned by a single PSN search. However, since we now do a search on every world,
                                            // this number could be increased, maybe to 50 to match Xbox 360?
			#define MAX_WAITTIME_FOR_QOS (10000)  // Don't wait more than 10 seconds for qos results from a task

struct SSynchronisedSessionID
{
	SceNpMatching2RoomId   m_roomId;
	SceNpMatching2WorldId  m_worldId;
	uint32                 m_gameType;
	SceNpMatching2ServerId m_serverId;
};

struct SCryPSNSessionID : public SCrySharedSessionID
{
	// Theoretically, a session will always now have a m_sessionId, but not always a m_sessionInfo.
	// SessionIDs from invites/join via XMB will have m_sessionId only.
	// SessionIDs from matchmaking SessionCreate/SessionJoin will first have m_sessionInfo, then will get m_sessionId slightly later in the create/join task.
	// (but by the time the session is returned to game code, it should have both).
	// SessionIds returned from SessionQuery or SessionSearch will NOT have a m_sessionId.
	SceNpSessionId         m_sessionId;           // PSN Advertisement session info
	SSynchronisedSessionID m_sessionInfo;         // PSN Matching2 room info
	bool                   m_fromInvite;

	SCryPSNSessionID()
		: m_fromInvite(false)
	{
		memset(&m_sessionInfo, 0, sizeof(m_sessionInfo));
		memset(&m_sessionId, 0, sizeof(m_sessionId));
	}

	bool IsSessionIdEqual(const SCrySessionID& other)
	{
		return (memcmp(&m_sessionId, &((const SCryPSNSessionID&)other).m_sessionId, sizeof(m_sessionId)) == 0);
	}
	bool IsSessionInfoEqual(const SCrySessionID& other)
	{
		return (memcmp(&m_sessionInfo.m_roomId, &((const SCryPSNSessionID&)other).m_sessionInfo.m_roomId, sizeof(m_sessionInfo.m_roomId)) == 0);
	}

	virtual bool operator==(const SCrySessionID& other)
	{
		return (IsSessionIdEqual(other) && IsSessionInfoEqual(other));
	}
	virtual bool operator<(const SCrySessionID& other)
	{
		return ((strcmp(m_sessionId.data, ((const SCryPSNSessionID&)other).m_sessionId.data) < 0) ||
		        (m_sessionInfo.m_serverId < ((const SCryPSNSessionID&)other).m_sessionInfo.m_serverId) ||
		        ((m_sessionInfo.m_serverId == ((const SCryPSNSessionID&)other).m_sessionInfo.m_serverId) && (m_sessionInfo.m_worldId < ((const SCryPSNSessionID&)other).m_sessionInfo.m_worldId)) ||
		        ((m_sessionInfo.m_serverId == ((const SCryPSNSessionID&)other).m_sessionInfo.m_serverId) && (m_sessionInfo.m_worldId == ((const SCryPSNSessionID&)other).m_sessionInfo.m_worldId) && (m_sessionInfo.m_roomId < ((const SCryPSNSessionID&)other).m_sessionInfo.m_roomId)));
	}
	virtual bool IsFromInvite() const
	{
		return m_fromInvite;
	}
	SCryPSNSessionID& operator=(const SCryPSNSessionID& other)
	{
		m_sessionInfo.m_roomId = other.m_sessionInfo.m_roomId;
		m_sessionInfo.m_worldId = other.m_sessionInfo.m_worldId;
		m_sessionInfo.m_serverId = other.m_sessionInfo.m_serverId;
		m_sessionInfo.m_gameType = other.m_sessionInfo.m_gameType;

		memcpy(&m_sessionId, &other.m_sessionId, sizeof(m_sessionId));

		m_fromInvite = other.m_fromInvite;

		return *this;
	}

	void AsCStr(char* pOutString, int inBufferSize) const
	{
		cry_sprintf(pOutString, inBufferSize, "%d - %d - %" PRIu64 " - %s", m_sessionInfo.m_serverId, m_sessionInfo.m_worldId, m_sessionInfo.m_roomId, m_sessionId.data);
	}
};

class CCryPSNMatchMaking : public CCryMatchMaking
{
public:
	CCryPSNMatchMaking(CCryLobby* lobby, CCryLobbyService* service, CCryPSNSupport* pSupport, ECryLobbyService serviceType);
	virtual ~CCryPSNMatchMaking();

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();
	void                   Tick(CTimeValue tv);

	void                   OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);

	virtual ECryLobbyError SessionRegisterUserData(SCrySessionUserData* data, uint32 numData, CryLobbyTaskID* taskID, CryMatchmakingCallback cb, void* cbArg);
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
	virtual void           SessionDisconnectRemoteConnection(CrySessionHandle gh, const TNetAddress& addr);
	virtual ECryLobbyError SessionSetLocalUserData(CrySessionHandle h, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg);
	virtual CrySessionID   SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h);
	virtual ECryLobbyError SessionSetAdvertisementData(CrySessionHandle gh, uint8* pData, uint32 dataSize, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCb, void* pCbArg);
	virtual ECryLobbyError SessionGetAdvertisementData(CrySessionID sessionId, CryLobbyTaskID* pTaskID, CryMatchmakingSessionGetAdvertisementDataCallback pCb, void* pCbArg);

	virtual uint32         GetSessionIDSizeInPacket() const;
	virtual ECryLobbyError WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const;
	virtual CrySessionID   ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const;

	virtual ECryLobbyError SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
			#if NETWORK_HOST_MIGRATION
	virtual void           HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual ECryLobbyError HostMigrationServer(SHostMigrationInfo* pInfo);
	virtual bool           GetNewHostAddress(char* address, SHostMigrationInfo* pInfo);
	virtual ECryLobbyError SendHostHintExternal(CryLobbySessionHandle h);
			#endif

	static void            SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);

	TNetAddress            GetHostAddressFromSessionHandle(CrySessionHandle h);

	virtual ECryLobbyError CopySessionIDToBuffer(CrySessionID sessionId, void* pBuffer, size_t& bufferSize);
	virtual ECryLobbyError CopySessionIDFromBuffer(const void* pBuffer, size_t bufferSize, CrySessionID& sessionId);
	virtual void           ForceFromInvite(CrySessionID sessionId);

private:

	enum ETask
	{
		eT_SessionRegisterUserData = eT_MatchMakingPlatformSpecificTask,
		eT_SessionMigrate,
		eT_SessionUpdate,
		eT_SessionUpdateSlots,
		eST_WaitingForUpdateRoomRequestCallback,

		eT_SessionStart,
		eT_SessionEnd,

		eT_SessionSearch,
		eST_WaitingForSearchRoomWorldInfo,
		eST_SessionSearchRequest,
		eST_WaitingForSearchRoomRequestCallback,
		eST_SessionSearchKickQOS,
		eST_WaitingForSearchQOSRequestCallback,

		eT_SessionCreate,
		eST_WaitingForCreateRoomWorldInfo,
		eST_SessionCreateRequest,
		eST_WaitingForCreateRoomRequestCallback,
		eST_WaitingForCreateRoomSignalingCallback,
		eST_WaitingForCreateRoomWebApiCallback,

		eT_SessionJoin,
		eST_WaitingForJoinRoomRequestCallback,
		eST_WaitingForJoinRoomSignalingCallback,
		eST_WaitingForJoinRoomWebApiCallback,

		eT_SessionDelete,
		eST_WaitingForLeaveRoomRequestCallback,
		eST_WaitingForLeaveRoomWebApiCallback,

		eST_WaitingForGrantRoomOwnerRequestCallback,
		eT_SessionGetUsers,

		eT_SessionQuery,
		eST_WaitingForQueryRoomRequestCallback,

		eT_SessionUserData,
		eST_WaitingForUserDataRequestCallback,

		eT_SessionSendHostHintExternal,
		eST_WaitingForNewHostHintCallback,

		eT_SessionSetAdvertisementData,
		eST_WaitingForUpdateAdvertisementDataWebApiCallback,

		eT_SessionGetAdvertisementData,
		eST_WaitingForGetAdvertisementDataWebApiCallback,

		// ^^New task IDs *must* go before host migration tasks^^
		eT_SessionMigrateHostStart = HOST_MIGRATION_TASK_ID_FLAG,
		eT_SessionMigrateHostServer
	};

	enum EHostHintTaskBehaviour
	{
		eHHTB_NORMAL = 0,
		eHHTB_REQUIRES_RESTART
	};

	struct SMappingToPSNLobbyData
	{
		uint32 integerField : 1;
		uint32 fieldOffset  : 31;                 // either 0-7 (representing integer field), or 0-511 (representing offset into binary chunk)
	};

	struct  SRegisteredUserData
	{
		SCrySessionUserData    data[MAX_MATCHMAKING_SESSION_USER_DATA];
		SMappingToPSNLobbyData mapping[MAX_MATCHMAKING_SESSION_USER_DATA];
		uint32                 num;
	};

	enum ECryPSNMemberInfo
	{
		ePSNMI_None       = 0,
		ePSNMI_Me         = BIT(0),
		ePSNMI_Owner      = BIT(1),
		ePSNMI_MeOwner    = ePSNMI_Me | ePSNMI_Owner,
		ePSNMI_Other      = BIT(2),
		ePSNMI_OtherOwner = ePSNMI_Other | ePSNMI_Owner,
	};

	enum ECryPSNConnectionState
	{
		ePSNCS_None,
		ePSNCS_Pending,
		ePSNCS_Dead,
		ePSNCS_Active
	};

	struct SSearchInfoData
	{
		SCrySessionSearchResult                   m_results[MAX_ROOMS_TO_STORE];
		SCrySessionUserData                       m_resultUserData[MAX_ROOMS_TO_STORE][MAX_MATCHMAKING_SESSION_USER_DATA];

		SceNpMatching2IntSearchFilter             m_intFilterExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM];
		SceNpMatching2AttributeId                 m_attrId[MAX_ATTRIB_IDS];
		SceNpMatching2SearchRoomRequest           m_searchRequest;

		SceNpMatching2SignalingGetPingInfoRequest m_QOSRequest[MAX_ROOMS_TO_STORE];
		SceNpMatching2RequestId                   m_reqId[MAX_ROOMS_TO_STORE];

		SceNpMatching2WorldId                     m_worldIds[MAX_WORLDS_TO_SEARCH];
		uint32 m_numWorlds;
		uint32 m_numResults;
		uint32 m_nIndex;
	};

	struct SCreateParamData
	{
		SceNpMatching2CreateJoinRoomRequestA m_createRequest;
		SceNpMatching2BinAttr               m_binAttrExternal[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM];
		SceNpMatching2IntAttr               m_intAttrExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM];
		SceNpMatching2BinAttr               m_binAttrInternal[SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_NUM];
		char                                m_binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM];
		char                                m_binAttrInternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_INTERNAL_NUM];
		CryWebApiJobId                      m_createJobId;
	};

	struct SUpdateParamData
	{
		SceNpMatching2SetRoomDataExternalRequest m_updateRequest;
		SceNpMatching2BinAttr                    m_binAttrExternal[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM];
		SceNpMatching2IntAttr                    m_intAttrExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM];
		char m_binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM];
	};

	struct SJoinParamData
	{
		SceNpMatching2JoinRoomRequestA m_joinRequest;
		CryWebApiJobId                m_joinJobId;
	};

	struct SLeaveParamData
	{
		SceNpMatching2LeaveRoomRequest m_leaveRequest;
		CryWebApiJobId                 m_leaveJobId;
	};

	struct SQueryParamData
	{
		SceNpMatching2GetRoomDataExternalListRequest m_queryRequest;
		SceNpMatching2RoomId                         m_roomIds[MAX_PSN_QUERYABLE_ROOMS];
		SceNpMatching2AttributeId                    m_attrTable[MAX_PSN_QUERYABLE_ATTRS];
	};

	struct SUserDataParamData
	{
		SceNpMatching2SetRoomMemberDataInternalRequest m_userDataRequest;
		SceNpMatching2BinAttr                          m_binAttrInternal[SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_NUM];
		char m_binAttrInternalData[SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOMMEMBER_BIN_ATTR_INTERNAL_NUM];
	};

	struct SHostHintParamData
	{
		SceNpMatching2SetRoomDataInternalRequest m_reqParam;
		SceNpMatching2RoomMemberId               m_memberId[MAX_PSN_ROOM_MEMBERS];
	};

	struct SGrantRoomOwnerParamData
	{
		SceNpMatching2GrantRoomOwnerRequest m_reqParam;
	};

	struct SUpdateAdvertisementDataParamData
	{
		CryWebApiJobId m_jobId;
	};

	struct SGetAdvertisementDataParamData
	{
		CryWebApiJobId m_jobId;
	};

	struct  SSession : public CCryMatchMaking::SSession
	{
		typedef CCryMatchMaking::SSession PARENT;

		virtual const char* GetLocalUserName(uint32 localUserIndex) const;
		virtual void        Reset();

		SceNpMatching2RoomId       m_roomId;      // ordered by size in current sdk (3.0.0)
		SceNpMatching2WorldId      m_worldId;
		SceNpMatching2ServerId     m_serverId;
		SceNpSessionId             m_sessionId;

		uint32                     m_flags;
		uint32                     m_gameType;
		uint32                     m_gameMode;
		uint32                     m_numPublicSlots;
		uint32                     m_numPrivateSlots;

		SceNpMatching2RoomMemberId m_myMemberId;
		SceNpMatching2RoomMemberId m_ownerMemberId;

		bool IsUsed() { return (localFlags & CRYSESSION_LOCAL_FLAG_USED); }

		struct SRoomMember
		{
			SceNpId                    m_npId;
			in_addr                    m_peerAddr;
			in_port_t                  m_peerPort;
			SceNpMatching2RoomMemberId m_memberId;
			ECryPSNMemberInfo          m_valid;

			CTimeValue                 m_signalingTimer;
			ECryPSNConnectionState     m_signalingState;

			uint8                      m_userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
			bool                       m_bHostJoinAck;

			bool IsUsed()                        { return (m_valid != ePSNMI_None); }
			bool IsValid(ECryPSNMemberInfo info) { return ((m_valid & info) == info); }

		} m_members[MAX_PSN_ROOM_MEMBERS];

		struct SLConnection : public CCryMatchMaking::SSession::SLConnection
		{
			DWORD users[MAX_PSN_LOCAL_USERS];
			BOOL  privateSlot[MAX_PSN_LOCAL_USERS];
		}                     localConnection;

		struct SRConnection : public CCryMatchMaking::SSession::SRConnection
		{
			SceNpMatching2RoomMemberId memberId;
			bool                       registered;
		};

		CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;

		CryMatchMakingTaskID remoteConnectionTaskID;

		bool                 remoteConnectionProcessingToDo;
		bool                 started;
		bool                 unregisteredConnectionsKicked;
	};

	struct  STask : public CCryMatchMaking::STask
	{
		SceNpMatching2RequestId m_reqId;
	};

	static void                DumpMemberInfo(SSession::SRoomMember* pMember);

	virtual uint64             GetSIDFromSessionHandle(CryLobbySessionHandle h);

	virtual void               FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void                       ClearRoomMember(CryLobbySessionHandle sessionHandle, uint32 nMember);
	void                       ClearSessionInfo(CryLobbySessionHandle sessionHandle);
	CryMatchMakingConnectionID FindConnectionIDFromRoomMemberID(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId memberId);
	SSession::SRoomMember*     FindRoomMemberFromSignalingID(CryLobbySessionHandle sessionHandle, uint32 id);
	SSession::SRoomMember*     FindRoomMemberFromRoomMemberID(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId memberId);

	void                       AddInitialRoomMembers(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternalList& pMemberList);
	void                       AddRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& memberInfo, ECryPSNMemberInfo info);
	void                       RemoveRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& memberInfo);
	void                       ChangeRoomOwner(CryLobbySessionHandle sessionHandle, SceNpMatching2RoomMemberId prevOwner, SceNpMatching2RoomMemberId newOwner);
	void                       UpdateRoomMember(CryLobbySessionHandle sessionHandle, const SCryPSNRoomMemberDataInternal& updatedMemberInfo);

	ECryPSNConnectionState     CheckSignaling(CryMatchMakingTaskID mmTaskID);
	void                       UpdateSignaling(CryMatchMakingTaskID mmTaskID);

	void                       DispatchForcedFromRoomEvent(CrySessionHandle h, SceNpMatching2Event how, SceNpMatching2EventCause why);
	void                       DispatchRoomOwnerChangedEvent(TMemHdl mh);

	void                       AddVoiceUser(SSession::SRoomMember* pMember);
	void                       ClearVoiceUser(SSession::SRoomMember* pMember);
	void                       UpdateVoiceUsers();

	void                       TickSessionWaitForQOS(CryMatchMakingTaskID mmTaskID);

	void                       ProcessRequestEvent(SCryPSNSupportCallbackEventData& data);
	void                       ProcessSignalingEvent(SCryPSNSupportCallbackEventData& data);
	void                       ProcessRoomEvent(SCryPSNSupportCallbackEventData& data);
	void                       ProcessErrorEvent(SCryPSNSupportCallbackEventData& data);
	void                       ProcessWebApiEvent(SCryPSNSupportCallbackEventData& data);

	ECryLobbyError             StartTask(ETask etask, bool startRunning, uint32 user, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg);
	void                       StartTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       StopTaskRunning(CryMatchMakingTaskID mmTaskID);
	void                       EndTask(CryMatchMakingTaskID mmTaskID);

	ECryLobbyError             CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32* users, int numUsers, uint32 flags);

	void                       TickSessionGetUsers(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionQuery(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionQuery(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionCreate(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionCreateRequest(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionCreateSignaling(CryMatchMakingTaskID mmTaskID);
	void                       EventWebApiCreateSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);
	void                       EndSessionCreate(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionMigrate(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionSearch(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionSearch(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionSearchRequest(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionQOSSearch(CryMatchMakingTaskID mmTaskID);
	void                       EndSessionSearch(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionJoin(CryMatchMakingTaskID mmTaskID);
	void                       ProcessSessionJoinHostAck(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	bool                       CheckSessionJoinHostAck(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionJoinSignaling(CryMatchMakingTaskID mmTaskID);
	void                       EventWebApiJoinSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);
	void                       EndSessionJoin(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionUpdate(CryMatchMakingTaskID mmTaskID);

	void                       StartSessionDelete(CryMatchMakingTaskID mmTaskID);
	void                       TickSessionDelete(CryMatchMakingTaskID mmTaskID);
	void                       EventWebApiLeaveSession(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);

	void                       TickSessionUserData(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionSetAdvertisementData(CryMatchMakingTaskID mmTaskID);
	void                       EventWebApiUpdateSessionAdvertisement(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);
	void                       EndSessionSetAdvertisementData(CryMatchMakingTaskID mmTaskID);

	void                       TickSessionGetAdvertisementData(CryMatchMakingTaskID mmTaskID);
	void                       EventWebApiGetSessionLinkData(CryMatchMakingTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);
	void                       EndSessionGetAdvertisementData(CryMatchMakingTaskID mmTaskID);

			#if NETWORK_HOST_MIGRATION
	void HostMigrationServerNT(CryMatchMakingTaskID mmTaskID);
	void TickHostMigrationServerNT(CryMatchMakingTaskID mmTaskID);
	void ProcessHostMigrationFromServer(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void HostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	void TickHostMigrationStartNT(CryMatchMakingTaskID mmTaskID);
	void ProcessHostMigrationStart(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void StartSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID);
	void TickSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID);
	void EndSessionSendHostHintExternal(CryMatchMakingTaskID mmTaskID);
			#endif

	typedef bool (* EventRequestResponseCallback)(CCryPSNMatchMaking* _this, CCryPSNMatchMaking::STask* pTask, uint32 taskId, SCryPSNSupportCallbackEventData& data);

	bool          ProcessCorrectTaskForRequest(SCryPSNSupportCallbackEventData& data, ETask subTaskCheck, const char* pRequestString, EventRequestResponseCallback pCB);
	bool          ProcessCorrectTaskForSearch(SCryPSNSupportCallbackEventData& data, ETask subTaskCheck, const char* pRequestString, EventRequestResponseCallback pCB);

	static bool   EventRequestResponse_SetRoomMemberDataInternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_GrantRoomOwner(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static uint32 SortWorlds(const SCryPSNGetWorldInfoListResponse* pWorldList, SceNpMatching2WorldId* pReturnIdList, uint32 nMaxListSize, bool bBusiestFirst);
	static bool   EventRequestResponse_GetWorldInfoListForCreate(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_GetWorldInfoListForSearch(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_LeaveRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_SetRoomDataExternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_SetRoomDataInternal(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_GetRoomDataExternalList(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_SearchRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_SearchQOS(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_JoinRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);
	static bool   EventRequestResponse_CreateRoom(CCryPSNMatchMaking* _this, STask* pTask, CryMatchMakingTaskID taskId, SCryPSNSupportCallbackEventData& data);

	void          GetRoomMemberData(CryLobbySessionHandle sessionHandle, SCryUserInfoResult& result, SCryMatchMakingConnectionUID& uid);
	void          ProcessExternalRoomData(const SCryPSNRoomDataExternal& roomData, SCrySessionSearchResult& result, SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA]);
	void          BuildUserSessionParams(SSession* pSession, SCrySessionUserData* pData, uint32 numData,
	                                     SceNpMatching2BinAttr binAttrExternal[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM],
	                                     SceNpMatching2IntAttr intAttrExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM],
	                                     char binAttrExternalData[SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_MAX_SIZE * SCE_NP_MATCHING2_ROOM_BIN_ATTR_EXTERNAL_NUM]);
	uint32 BuildSearchFilterParams(SCrySessionSearchData* pData, uint32 numData,
	                               SceNpMatching2IntSearchFilter intFilterExternal[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM]);

	void                SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	void                InitialUserDataEvent(CryLobbySessionHandle h);

	void                ResetTask(CryMatchMakingTaskID mmTaskID);

	virtual const char* GetConnectionName(CCryMatchMaking::SSession::SRConnection* pConnection, uint32 localUserIndex) const;

	SRegisteredUserData m_registeredUserData;

	CryLobbyIDArray<SSession, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;

	STask           m_task[MAX_MATCHMAKING_TASKS];

	uint32          m_oldNatType;
	CCryPSNSupport* m_pPSNSupport;
	int             net_enable_psn_hinting;
};

		#endif // USE_CRY_MATCHMAKING
	#endif   // USE_PSN
#endif     // CRY_PLATFORM_ORBIS

#endif // __CRYPSNMATCHMAKING_H__
