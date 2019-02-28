// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*

   Handles a multitude of PSN work including maintaining a state machine of connection to the service.
   PSN for Orbis

 */

#ifndef __CRYPSN2_SUPPORT_H__
#define __CRYPSN2_SUPPORT_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include <np.h>
		#include <libnetctl.h>
		#include <libhttp.h>
		#include <net.h>
		#include <json2.h>
		#include <libsysmodule.h>

		#include "CryPSN2Response.h"
		#include "CryPSN2WebApi.h"

		#include <CryCore/Platform/IPlatformOS.h>

		#define MAX_SUPPORT_EVENT_HANDLERS (5)          //-- Max number of registered event handlers.
                                                    //-- Currently 5 - for CryPSNLobbyService, CryPSNMatchMaking, CryPSNStats, CryPSNLobbyUI, CryPSNFriends

		#include <CryCore/Platform/OrbisSpecific.h>

		#define PSN_OK                             SCE_OK
		#define INVALID_NET_CTL_HANDLER_ID         (-1)
		#define INVALID_NET_MEMPOOL_ID             (-1)
		#define INVALID_PSN_SSL_CONTEXT            (-1)
		#define INVALID_PSN_HTTP_CONTEXT           (-1)
		#define INVALID_PSN_WEBAPI_CONTEXT         (-1)
		#define INVALID_PSN_MATCHMAKING_CONTEXT    SCE_NP_MATCHING2_INVALID_CONTEXT_ID
		#define INVALID_PSN_STATS_CONTEXT          0
		#define INVALID_PSN_SERVER_ID              SCE_NP_MATCHING2_INVALID_SERVER_ID
		#define INVALID_PSN_WORLD_ID               SCE_NP_MATCHING2_INVALID_WORLD_ID
		#define INVALID_PSN_ROOM_ID                SCE_NP_MATCHING2_INVALID_ROOM_ID
		#define INVALID_PSN_ROOM_MEMBER_ID         SCE_NP_MATCHING2_INVALID_ROOM_MEMBER_ID
		#define INVALID_PSN_REQUEST_ID             SCE_NP_MATCHING2_INVALID_REQUEST_ID
		#define INVALID_PRIVILEGE_REQUEST_ID       0

		#define MATCHING2_CONTEXT_EVENT_STARTED    SCE_NP_MATCHING2_CONTEXT_EVENT_STARTED
		#define MATCHING2_CONTEXT_EVENT_STOPPED    SCE_NP_MATCHING2_CONTEXT_EVENT_STOPPED
		#define MATCHING2_CONTEXT_EVENT_START_OVER SCE_NP_MATCHING2_CONTEXT_EVENT_START_OVER

		#define SCryPSNSignalingNetInfo            SceNpMatching2SignalingNetInfo

		#define RegisterNetCtlHandler              sceNetCtlRegisterCallback
		#define UnregisterNetCtlHandler            sceNetCtlUnregisterCallback

enum ECryPSNOnlineState
{
	//-- Only use a limited set of these with FlagPendingError(): Offline / Online / Matchmaking_Ready
	ePSNOS_Uninitialised = 0,
	ePSNOS_Offline,                       //-- Has nothing set - Must start whole state machine from scratch.
	ePSNOS_WaitingForDialogToOpen,
	ePSNOS_DialogFinished,
	ePSNOS_WaitingForDialogToClose,
	ePSNOS_DiscoverNpId,
	ePSNOS_OnlinePrivilegeTest,
	ePSNOS_WaitingForOnlinePrivilegeTest,
	ePSNOS_OnlinePrivilegeRestrictions,
	ePSNOS_WaitingForOnlinePrivilegeRestrictions,
	ePSNOS_RegisterOnlineServices,
	ePSNOS_Online,                        //-- PSNID is valid and we're signed into PSN, but matchmaking is not initialised.
	ePSNOS_Matchmaking_Initialise,
	ePSNOS_Matchmaking_WaitingForPrivilegeTest,
	ePSNOS_Matchmaking_WaitingForPrivilegeDialog,
	ePSNOS_Matchmaking_StartContext,
	ePSNOS_Matchmaking_WaitingForContextToStart,
	ePSNOS_Matchmaking_ContextReady,
	ePSNOS_Matchmaking_ReadyToEnumerateServers,
	ePSNOS_Matchmaking_GettingNextServer,
	ePSNOS_Matchmaking_WaitingForServerInfo,
	ePSNOS_Matchmaking_ServerInfoRetrieved,
	ePSNOS_Matchmaking_ServerListDone,
	ePSNOS_Matchmaking_WaitingForServerContext,
	ePSNOS_Matchmaking_ServerJoined,
	ePSNOS_Matchmaking_Available,             //-- We're connected to a server. Ready to do matchmaking
};

enum ECryPSNPendingError
{
	ePSNPE_None = 0,
	ePSNPE_NetLibraryNotInitialised,      // netCtlInit has not been called
	ePSNPE_InternetDisabled,              // User has disabled internet connection
	ePSNPE_NotConnected,                  // Failed to establish connection to NP servers
	ePSNPE_PSNUnavailable,                // PSN service not available (might be under maintenance, etc. Not the same as unreachable!)
	ePSNPE_NoPSNAccount,                  // User does not have an NP account
	ePSNPE_PSNAccountSuspended,           // User managed to communicate with PSN, but log in was refused because the account was banned or suspended.
	ePSNPE_CableDisconnected,             // User network cable is disconnected
	ePSNPE_SignInCancelled,               // User cancelled sign in process
	ePSNPE_Offline,                       // No longer connected to NP servers
	ePSNPE_ContextError,                  // Problem with internal context establishment
	ePSNPE_OutOfMemory,                   // Failed to allocate internal resources
	ePSNPE_TimedOut,                      // Time out
	ePSNPE_InternalError,                 // Generic error
	ePSNPE_IncorrectState,                // Indicates a logic problem in internal state machine
	ePSNPE_BadArgument,                   // Bad argument passed to function
	ePSNPE_RestrictedPrivileges,          // Insufficient privileges to play online.
	ePSNPE_AgeRestricted,                 // Restricted to offline because of age.

	ePSNPE_MatchingLibraryNotInitialised,   // sceNpMatching2Init2 has not been called
	ePSNPE_NoServersAvailable,              // There are no servers available (all down for maintainance, or full)
	ePSNPE_NoWorldsAvailable,               // There are no available worlds to join (all full)
	ePSNPE_RoomIsFull,                      // Trying to join a room that is full.

	ePSNPE_SignalingLibraryNotInitialised,  // sceNpInit has not been called
	ePSNPE_SignalingError,                  //	signaling issue detected

	ePSNPE_ScoreLibraryNotInitialised,      // sceNpScoreInit has not been called

	ePSNPE_TUSLibraryNotInitialised,        //-- sceNpTusInit has not been called

	ePSNPE_RestartForInvite                 //-- system restarted to process an invite
};

enum ECryPSNSupportCallbackEvent
{
	eCE_ErrorEvent   = 0x8000,
	eCE_SystemEvent  = 0x0001,
	eCE_RequestEvent = 0x0002,
	eCE_SignalEvent  = 0x0004,
	eCE_RoomEvent    = 0x0008,
	eCE_VoipEvent    = 0x0010,
	eCE_WebApiEvent  = 0x0020,
};

struct SCryPSNSupportCallbackEventData
{
	struct SErrorEvent
	{
		ECryPSNPendingError m_error;
		ECryPSNOnlineState  m_oldState;
		ECryPSNOnlineState  m_newState;
	};

	struct SSystemEvent
	{
		uint64 m_status;
		uint64 m_param;
		int    m_error;
		bool   m_bHandled;
	};

	struct SRequestEvent
	{
		enum ERequestEventReturnFlags
		{
			REQUEST_EVENT_NOT_HANDLED = 0x0,
			REQUEST_EVENT_OWNS_MEMORY = 0x1,
			REQUEST_EVENT_HANDLED     = 0x2
		};

		TMemHdl                 m_dataHdl;
		size_t                  m_dataSize;
		SceNpMatching2Event     m_event;
		SceNpMatching2RequestId m_reqId;
		int                     m_error;
		uint8                   m_returnFlags;
	};

	struct SSignalEvent
	{
		int                        m_event;
		SceNpMatching2RoomMemberId m_subject; // On Orbis, m_subject is a SceNpMatching2RoomMemberId.
		SceNpMatching2RoomId       m_roomId;
		int                        m_error;
		bool                       m_bHandled;
	};

	struct SRoomEvent
	{
		enum ERequestEventReturnFlags
		{
			ROOM_EVENT_NOT_HANDLED = 0x0,
			ROOM_EVENT_OWNS_MEMORY = 0x1,
			ROOM_EVENT_HANDLED     = 0x2
		};

		TMemHdl              m_dataHdl;
		size_t               m_dataSize;
		SceNpMatching2Event  m_event;
		SceNpMatching2RoomId m_roomId;
		int                  m_error;
		uint8                m_returnFlags;
	};

	struct SWebApiEvent
	{
		enum EWebApiEventReturnFlags
		{
			WEBAPI_EVENT_NOT_HANDLED = 0x00,
			WEBAPI_EVENT_OWNS_MEMORY = 0x01,
			WEBAPI_EVENT_HANDLED     = 0x02,
		};

		SCryPSNWebApiResponseBody* m_pResponseBody;
		CryWebApiJobId             m_id;
		int                        m_error;
		uint8                      m_returnFlags;
	};

	union
	{
		SErrorEvent   m_errorEvent;
		SSystemEvent  m_systemEvent;
		SRequestEvent m_requestEvent;
		SRoomEvent    m_roomEvent;
		SSignalEvent  m_signalEvent;
		SWebApiEvent  m_webApiEvent;
	};
};

typedef void (* CryPSNSupportCallback)(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);

struct SCryPSNSessionID;

		#define HandlePSNError(ret) HandlePSNErrorInternal(ret, __FILE__, __LINE__)

class CCryPSNSupport : public CMultiThreadRefCount, public IPlatformOS::IPlatformListener
{
public:

	CCryPSNSupport(CCryLobby* pLobby, CCryLobbyService* pService, ECryLobbyServiceFeatures features);
	~CCryPSNSupport();

	ECryLobbyError     Initialise();
	ECryLobbyError     Terminate();
	void               Tick();              // keep PSN in check

	bool               HandlePSNErrorInternal(int retCode, const char* const file = NULL, const int line = -1);

	void               ResumeTransitioning(ECryPSNOnlineState state);
	bool               HasTransitioningReachedState(ECryPSNOnlineState state);
	ECryPSNOnlineState GetOnlineState() const { return m_curState; }
	bool               IsRunning() const      { return (m_bIsTransitioningActive == true); }
	bool               IsPaused() const       { return (m_bIsTransitioningActive != true); }
	bool               IsDead() const         { return false; }

	bool RegisterEventHandler(int eventTypes, CryPSNSupportCallback, void*);

	const SCryPSNServerInfoResponse* const GetServerInfo() const           { return &m_serverBestInfo; }
	SCryPSNSignalingNetInfo                GetNetInfo() const              { return m_netInfo; }

	SceNpMatching2ContextId                GetMatchmakingContextId() const { return m_matchmakingCtxId; }
	CCryPSNWebApiJobController&            GetWebApiInterface()            { return m_WebApiController; }
		#if USE_CRY_STATS
	int                                    GetScoringContextId() const     { return m_scoringCtxId; }
	int                                    GetTUSContextId() const         { return m_tusCtxId; }
		#endif

	const SceNpId* const GetLocalNpId() const     { return (m_myNpId.handle.data[0] != 0) ? &m_myNpId : NULL; }
	const char* const    GetLocalUserName() const { return m_myNpId.handle.data; }

	int                  IsChatRestricted() const { return m_isChatRestricted; }

	CCryLobby* const     GetLobby() const         { return m_pLobby; }

	void                 ReturnWebApiEvent(CryWebApiJobId id, int errorCode, TMemHdl responseBodyHdl);

		#if USE_CRY_MATCHMAKING
	//	void											PrepareForInviteOrJoin(SCryPSNSessionID* pSessionId);
		#endif // USE_CRY_MATCHMAKING

private:

	void CleanTo(ECryPSNOnlineState state, ECryPSNPendingError eReason);
	bool FlagPendingError(ECryPSNPendingError errorCode, ECryPSNOnlineState newState);

	void BroadcastEventToHandlers(ECryPSNSupportCallbackEvent, SCryPSNSupportCallbackEventData &);

	// State Transition Handlers
	void    TransitionSigninDialogOpen();
	void    TransitionSigninDialogClose();
	void    TransitionDiscoverNpId();
	void    TransitionOnlinePrivilegeTest();
	void    TransitionWaitingForOnlinePrivilegeTest();
	void    TransitionOnlinePrivilegeRestrictions();
	void    TransitionWaitingForOnlinePrivilegeRestrictions();
	void    TransitionRegisterOnlineServices();
	void    TransitionMatchmakingPrivilegeTest();
	void    TransitionMatchmakingWaitingForPrivilegeTest();
	void    TransitionMatchmakingWaitingForPrivilegeDialog();
	void    TransitionMatchmakingStartContext();
	void    TransitionMatchmakingRegisterHandlers();
	void    TransitionMatchmakingEnumerateServers();
	void    TransitionMatchmakingServerInfoDiscovery();
	void    TransitionMatchmakingNextServer();
	void    TransitionMatchmakingCreateServerContext();
	void    TransitionMatchmakingAvailable();

	void    PrepareOnlineStatus(ECryPSNOnlineState state, ECryPSNPendingError eReason);
	void    DispatchInviteAccepted(CrySessionID session, ECryLobbyError error);
	void    DispatchOnlineStatus(EOnlineState eState, ECryLobbyError eReason, bool bIsConnected);
	void    DispatchLinkState(ECableState state);
	void    DispatchChatRestrictedEvent(int isRestricted);

	uint32  RandomNumber(uint32 range) const;

	TMemHdl CloneResponse(SceNpMatching2Event eventType, const void* pData, uint32* pReturnMemSize);
	void    ReleaseClonedResponse(SceNpMatching2Event eventType, TMemHdl memHdl);

	// IPlatformOS::IPlatformListener
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& event);
	// ~IPlatformOS::IPlatformListener
	// Callback handler for handling system events passed in by OnPlatformEvent
	void SystemCallbackHandler(SceSystemServiceEvent event);

	// Callback handlers for handling events on Network thread
	void ContextCallbackHandler(SceNpMatching2ContextId id, SceNpMatching2Event myEvent, SceNpMatching2EventCause eventCause, int errorCode);
	void NetCtlCallbackHandler(int event);
	void RequestCallbackHandler(SceNpMatching2ContextId id, SceNpMatching2RequestId reqId, SceNpMatching2Event myEvent, int errorCode, TMemHdl dataHandle, uint32 dataSize);
	void RoomEventCallbackHandler(SceNpMatching2ContextId id, SceNpMatching2RoomId roomId, SceNpMatching2Event myEvent, int errorCode, TMemHdl dataHandle, uint32 dataSize);
	void SignalingCallbackHandler(uint32 id, uint32 subject, SceNpMatching2RoomId roomId, int myEvent, int errorCode);

	// Callbacks registered with PSN API. Several threads may invoke these callbacks (Network thread, PSN internal threads, etc), so
	// these callbacks will just put data onto a queue for the Network thread to process later
	static void PSNContextCallback(SceNpMatching2ContextId id, SceNpMatching2Event myEvent, SceNpMatching2EventCause eventCause, int errorCode, void* pArg);
	static void PSNNetCtlCallback(int event, void* pArg);
	static void PSNRequestCallback(SceNpMatching2ContextId id, SceNpMatching2RequestId reqId, SceNpMatching2Event myEvent, int errorCode, const void* pData, void* pArg);
	static void PSNRoomEventCallback(SceNpMatching2ContextId id, SceNpMatching2RoomId roomId, SceNpMatching2Event myEvent, const void* pData, void* pArg);
	static void PSNSignalingCallback(SceNpMatching2ContextId id, SceNpMatching2RoomId roomId, SceNpMatching2RoomMemberId subject, SceNpMatching2Event myEvent, int errorCode, void* pArg);

	// WebApi responses are passed from webapi thread to lobby thread via the same queue that handles PSN callbacks.
	void WebApiCallbackHandler(CryWebApiJobId id, int errorCode, TMemHdl responseBodyHdl);

	struct SJsonLibrary
	{
		SJsonLibrary()
		{
			if (sceSysmoduleLoadModule(SCE_SYSMODULE_JSON2) != SCE_OK)
			{
				CryFatalError("Unable to load JSON2 library");
			}
		}
		~SJsonLibrary()
		{
			sceSysmoduleUnloadModule(SCE_SYSMODULE_JSON2);
		}
	} m_jsonLibrary;

	//-- Special Queue for dealing with callbacks that occur from threads other than the one the lobby is using
	CWorkQueue    m_lobbyThreadQueueProcessing;
	CWorkQueue    m_lobbyThreadQueueBuilding;
	CryLobbyMutex m_lobbyThreadQueueBuildingMutex;

	//-- Callback to lobby (mostly for mem allocs)
	CCryLobby*               m_pLobby;

	ECryLobbyServiceFeatures m_features;

	//-- Context Information
	int                     m_netCtlHandlerID;
	int                     m_netMemPoolId;
	int                     m_sslCtxId;
	int                     m_httpCtxId;
	int                     m_webApiCtxId;
	SceNpMatching2ContextId m_matchmakingCtxId;
		#if USE_CRY_STATS
	int                     m_scoringCtxId;
	int                     m_tusCtxId;
		#endif

	// WebApi Info
	CCryPSNWebApiUtility       m_WebApiUtility;
	// Just 1 controller for now.
	CCryPSNWebApiJobController m_WebApiController;

	//-- State machine
	ECryPSNOnlineState        m_curState;
	ECryPSNOnlineState        m_wantedState;
	bool                      m_bIsTransitioningActive;

	SceUserServiceUserId      m_myUserId;

	int                       m_onlinePrivTestReqId;
	int                       m_onlinePrivRestrictionsReqId;
	int8                      m_onlinePrivAge;
	SceNpParentalControlInfo  m_onlinePrivRestrictionsResult;
	int                       m_plusPrivReqId;
	SceNpCheckPlusResult      m_plusPrivResult;

	SCryPSNServerInfoResponse m_serverBestInfo;
	SCryPSNSignalingNetInfo   m_netInfo;

	SceNpId                   m_myNpId;                     // NpId of signed in user.
	int                       m_isChatRestricted;

	struct SCryPSNSupportInviteData
	{
		SceNpMatching2WorldId  m_worldId;
		SceNpMatching2ServerId m_serverId;
		bool                   m_bUseInvite;
	} m_inviteInfo;

	struct SCryPSNSupportCallbackHandler
	{
		CryPSNSupportCallback m_CallbackFunc;
		void*                 m_CallbackArg;
		int                   m_CallbackEventTypes;
	};

	SCryPSNSupportCallbackHandler m_CallbackHandlers[MAX_SUPPORT_EVENT_HANDLERS];
	uint32                        m_nCallbacksRegistered;
};

ECryLobbyError MapSupportErrorToLobbyError(ECryPSNPendingError pendingError);

	#endif // USE_PSN
#endif   // CRY_PLATFORM_ORBIS

#endif // __CRYPSNSUPPORT_H__
