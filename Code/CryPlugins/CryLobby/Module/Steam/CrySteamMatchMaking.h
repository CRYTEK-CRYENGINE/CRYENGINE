// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamMatchMaking.h
//  Created:     11/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryMatchMaking
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSTEAMMATCHMAKING_H__
#define __CRYSTEAMMATCHMAKING_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryMatchMaking.h"

#if USE_STEAM && USE_CRY_MATCHMAKING

////////////////////////////////////////////////////////////////////////////////

	#include "steam/steam_api.h"
	#include "steam/steam_gameserver.h"

////////////////////////////////////////////////////////////////////////////////

// Utility class to map a CryMatchMakingTaskID to a steam callback
template<class T, class P> struct SCrySteamCallback
{
	typedef void (T::* func_t)(P*);

	SCrySteamCallback(T* pType, func_t func)
		: m_callBack(pType, func)
		, m_taskID(CryMatchMakingInvalidTaskID)
	{}

	CCallback<T, P, false> m_callBack;
	uint32                 m_taskID;
};

// Utility class to map a CryMatchMakingTaskID to a steam callresult
template<class T, class P> struct SCrySteamCallResult
{
	SCrySteamCallResult(void)
		: m_taskID(CryMatchMakingInvalidTaskID)
	{}

	CCallResult<T, P> m_callResult;
	uint32            m_taskID;
};

// Prevent anyone using STEAM_CALLBACK
	#if defined(STEAM_CALLBACK)
		#undef STEAM_CALLBACK
		#define STEAM_CALLBACK # error Use CRYSTEAM_CALLBACK instead
	#endif

// Wrapper for steam callbacks.  Can't use STEAM_CALLBACK because we need to
// associate the CryMatchMakingTaskID with the callback
	#define CRYSTEAM_CALLBACK(thisclass, func, param, var) \
	  SCrySteamCallback<thisclass, param> var;             \
	  void func(param * pParam)

// Wrapper for steam callresults.  Steam doesn't define a STEAM_CALLRESULT, and
// we need to associate the CryMatchMakingTaskID with the callresult anyway so
// I've made an analogous macro to CRYSTEAM_CALLBACK above
	#define CRYSTEAM_CALLRESULT(thisclass, func, param, var) \
	  SCrySteamCallResult<thisclass, param> var;             \
	  void func(param * pParam, bool ioFailure)

////////////////////////////////////////////////////////////////////////////////

	#define STEAMID_AS_STRING_LENGTH (32)
static CryFixedStringT<STEAMID_AS_STRING_LENGTH> CSteamIDAsString(const CSteamID& id)
{
	CryFixedStringT<STEAMID_AS_STRING_LENGTH> result;
	result.Format("%02X:%X:%05X:%08X", id.GetEUniverse(), id.GetEAccountType(), id.GetUnAccountInstance(), id.GetAccountID());

	return result;
};

////////////////////////////////////////////////////////////////////////////////

struct SCrySteamSessionID : public SCrySharedSessionID
{
	SCrySteamSessionID()
		: m_steamID(CSteamID())
		, m_fromInvite(false)
	{}

	SCrySteamSessionID(CSteamID& id, bool fromInvite)
		: m_steamID(id)
		, m_fromInvite(fromInvite)
	{}

	SCrySteamSessionID& operator=(const SCrySteamSessionID& other)
	{
		m_steamID = other.m_steamID;
		return *this;
	}

	// SCrySessionID
	virtual bool operator==(const SCrySessionID& other)
	{
		return (m_steamID == ((SCrySteamSessionID&)other).m_steamID);
	}

	virtual bool operator<(const SCrySessionID& other)
	{
		return (m_steamID < ((SCrySteamSessionID&)other).m_steamID);
	}

	virtual bool IsFromInvite() const
	{
		return m_fromInvite;
	}

	virtual void AsCStr(char* pOutString, int inBufferSize) const
	{
		cry_sprintf(pOutString, inBufferSize, "%s", CSteamIDAsString(m_steamID).c_str());
	}
	// ~SCrySessionID

	static uint32 GetSizeInPacket()
	{
		return sizeof(uint64);
	}

	void WriteToPacket(CCryLobbyPacket* pPacket) const
	{
		pPacket->WriteUINT64(m_steamID.ConvertToUint64());
	}

	void ReadFromPacket(CCryLobbyPacket* pPacket)
	{
		m_steamID.SetFromUint64(pPacket->ReadUINT64());
	}

	CSteamID m_steamID;
	bool     m_fromInvite;
};

////////////////////////////////////////////////////////////////////////////////

struct SCrySteamUserID : public SCrySharedUserID
{
	SCrySteamUserID()
		: m_steamID(CSteamID())
	{}

	SCrySteamUserID(CSteamID& steamID)
		: m_steamID(steamID)
	{}

	virtual bool operator==(const SCryUserID& other) const
	{
		return (m_steamID == static_cast<const SCrySteamUserID&>(other).m_steamID);
	}

	virtual bool operator<(const SCryUserID& other) const
	{
		return (m_steamID < static_cast<const SCrySteamUserID&>(other).m_steamID);
	}

	virtual CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> GetGUIDAsString() const
	{
		CryFixedStringT<CRYLOBBY_USER_GUID_STRING_LENGTH> result(CSteamIDAsString(m_steamID));
		return result;
	};

	void WriteToPacket(CCryLobbyPacket* pPacket) const
	{
		pPacket->WriteUINT64(m_steamID.ConvertToUint64());
	}

	void ReadFromPacket(CCryLobbyPacket* pPacket)
	{
		m_steamID.SetFromUint64(pPacket->ReadUINT64());
	}

	SCrySteamUserID& operator=(const SCrySteamUserID& other)
	{
		m_steamID = other.m_steamID;
		return *this;
	}

	CSteamID m_steamID;
};

////////////////////////////////////////////////////////////////////////////////

class CCrySteamMatchMaking : public CCryMatchMaking
{
public:
	CCrySteamMatchMaking(CCryLobby* lobby, CCryLobbyService* service, ECryLobbyService serviceType);

	ECryLobbyError Initialise();
	void           Tick(CTimeValue tv);

	void           OnPacket(const TNetAddress& addr, CCryLobbyPacket* pPacket);
	/* void						ProfileSettingsChanged(ULONG_PTR param);
	          void						UserSignedOut(uint32 user);
	 */

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
	virtual ECryLobbyError SessionJoin(uint32* pUsers, int numUsers, uint32 flags, CrySessionID id, CryLobbyTaskID* pTaskID, CryMatchmakingSessionJoinCallback pCB, void* pCBArg);
	virtual ECryLobbyError SessionSetLocalUserData(CrySessionHandle h, CryLobbyTaskID* pTaskID, uint32 user, uint8* pData, uint32 dataSize, CryMatchmakingCallback pCB, void* pCBArg);
	virtual CrySessionID   SessionGetCrySessionIDFromCrySessionHandle(CrySessionHandle h);

	virtual uint32         GetSessionIDSizeInPacket() const;
	virtual ECryLobbyError WriteSessionIDToPacket(CrySessionID sessionId, CCryLobbyPacket* pPacket) const;
	virtual CrySessionID   ReadSessionIDFromPacket(CCryLobbyPacket* pPacket) const;

	virtual ECryLobbyError SessionEnsureBestHost(CrySessionHandle gh, CryLobbyTaskID* pTaskID, CryMatchmakingCallback pCB, void* pCBArg);
	#if NETWORK_HOST_MIGRATION
	virtual void           HostMigrationInitialise(CryLobbySessionHandle h, EDisconnectionCause cause);
	virtual ECryLobbyError HostMigrationServer(SHostMigrationInfo* pInfo);
	virtual bool           GetNewHostAddress(char* address, SHostMigrationInfo* pInfo);
	#endif

	SCrySteamSessionID GetSteamSessionIDFromSession(CryLobbySessionHandle lsh);

	//////////////////////////////////////////////////////////////////////////////
	// LobbyIDAddr housekeeping
	TNetAddress          GetHostAddressFromSessionHandle(CrySessionHandle h);
	virtual void         LobbyAddrIDTick();
	virtual bool         LobbyAddrIDHasPendingData();
	virtual ESocketError LobbyAddrIDSend(const uint8* buffer, uint32 size, const TNetAddress& addr);
	virtual void         LobbyAddrIDRecv(void (* cb)(void*, uint8* buffer, uint32 size, CRYSOCKET sock, TNetAddress& addr), void* cbData);
	//////////////////////////////////////////////////////////////////////////////

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
		eT_SessionQuery,
		eT_SessionSetLocalUserData,
	};

	struct  SRegisteredUserData
	{
		SCrySessionUserData data[MAX_MATCHMAKING_SESSION_USER_DATA];
		uint32              num;
	};

	struct SSession : public CCryMatchMaking::SSession
	{
		typedef CCryMatchMaking::SSession PARENT;

		virtual const char* GetLocalUserName(uint32 localUserIndex) const;
		virtual void        Reset();

		void                InitialiseLocalConnection(SCryMatchMakingConnectionUID uid);

		SCrySessionUserData userData[MAX_MATCHMAKING_SESSION_USER_DATA];
		SCrySessionData     data;
		HANDLE              handle;
		DWORD               flags;
		DWORD               gameType;
		DWORD               gameMode;

		struct SLConnection : public CCryMatchMaking::SSession::SLConnection
		{
			char            name[CRYLOBBY_USER_NAME_LENGTH];
			uint8           userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
			SCrySteamUserID userID;
		}                     localConnection;

		struct SRConnection : public CCryMatchMaking::SSession::SRConnection
		{
			char            name[CRYLOBBY_USER_NAME_LENGTH];
			uint8           userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES];
			SCrySteamUserID userID;

			enum ERemoteConnectionState
			{
				eRCS_None,
				eRCS_WaitingToJoin,
				eRCS_Joining,
				eRCS_JoiningButWantToLeave,
				eRCS_Joined,
				eRCS_WaitingToLeave,
				eRCS_Leaving
			}    state;

			bool m_isDedicated;
	#if NETWORK_HOST_MIGRATION
			bool m_migrated;
	#endif
		};

		CryLobbyIDArray<SRConnection, CryMatchMakingConnectionID, MAX_LOBBY_CONNECTIONS> remoteConnection;
		SCrySteamSessionID m_id;
	};

	struct  STask : public CCryMatchMaking::STask
	{
		void Reset() {};
	};

	//////////////////////////////////////////////////////////////////////////////
	// Task housekeeping
	ECryLobbyError StartTask(ETask etask, bool startRunning, CryMatchMakingTaskID* mmTaskID, CryLobbyTaskID* lTaskID, CryLobbySessionHandle h, void* cb, void* cbArg);
	void           StartSubTask(ETask etask, CryMatchMakingTaskID mmTaskID);
	void           StartTaskRunning(CryMatchMakingTaskID mmTaskID);
	void           StopTaskRunning(CryMatchMakingTaskID mmTaskID);
	void           EndTask(CryMatchMakingTaskID mmTaskID);
	bool           CheckTaskStillValid(CryMatchMakingTaskID mmTaskID);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Remote connection housekeeping
	CryMatchMakingConnectionID AddRemoteConnection(CryLobbySessionHandle h, CryLobbyConnectionID connectionID, SCryMatchMakingConnectionUID uid, CSteamID steamID, uint32 numUsers, const char* name, uint8 userData[CRYLOBBY_USER_DATA_SIZE_IN_BYTES], bool isDedicated);
	virtual void               FreeRemoteConnection(CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Session housekeeping
	ECryLobbyError CreateSessionHandle(CryLobbySessionHandle* h, bool host, uint32 createFlags, int numUsers);
	virtual uint64 GetSIDFromSessionHandle(CryLobbySessionHandle h);
	void           SetSessionUserData(CryMatchMakingTaskID mmTaskID, SCrySessionUserData* pData, uint32 numData);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Tasks
	void StartSessionCreate(CryMatchMakingTaskID mmTaskID);

	void StartSessionMigrate(CryMatchMakingTaskID mmTaskID);
	void TickSessionMigrate(CryMatchMakingTaskID mmTaskID);

	void StartSessionSearch(CryMatchMakingTaskID mmTaskID);
	void TickSessionSearch(CryMatchMakingTaskID mmTaskID);
	void EndSessionSearch(CryMatchMakingTaskID mmTaskID);

	void StartSessionUpdate(CryMatchMakingTaskID mmTaskID);
	void StartSessionUpdateSlots(CryMatchMakingTaskID mmTaskID);

	void EndSessionQuery(CryMatchMakingTaskID mmTaskID);

	void EndSessionGetUsers(CryMatchMakingTaskID mmTaskID);

	void StartSessionStart(CryMatchMakingTaskID mmTaskID);
	void TickSessionStart(CryMatchMakingTaskID mmTaskID);

	void StartSessionEnd(CryMatchMakingTaskID mmTaskID);
	void TickSessionEnd(CryMatchMakingTaskID mmTaskID);

	void StartSessionJoin(CryMatchMakingTaskID mmTaskID);
	void TickSessionJoin(CryMatchMakingTaskID mmTaskID);
	void EndSessionJoin(CryMatchMakingTaskID mmTaskID);
	void ProcessSessionRequestJoinResult(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);
	void ProcessSessionAddRemoteConnections(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);

	void StartSessionDelete(CryMatchMakingTaskID mmTaskID);
	void TickSessionDelete(CryMatchMakingTaskID mmTaskID);

	void StartSessionSetLocalUserData(CryMatchMakingTaskID mmTaskID);
	void ProcessLocalUserData(const TNetAddress& addr, CCrySharedLobbyPacket* pPacket);

	void InitialUserDataEvent(CryLobbySessionHandle h);
	void SessionUserDataEvent(ECryLobbySystemEvent event, CryLobbySessionHandle h, CryMatchMakingConnectionID id);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Steam callbacks are broadcast to all listeners
	CRYSTEAM_CALLBACK(CCrySteamMatchMaking, OnSteamShutdown, SteamShutdown_t, m_SteamOnShutdown);
	CRYSTEAM_CALLBACK(CCrySteamMatchMaking, OnLobbyDataUpdated, LobbyDataUpdate_t, m_SteamOnLobbyDataUpdated);
	CRYSTEAM_CALLBACK(CCrySteamMatchMaking, OnP2PSessionRequest, P2PSessionRequest_t, m_SteamOnP2PSessionRequest);
	CRYSTEAM_CALLBACK(CCrySteamMatchMaking, OnLobbyChatUpdate, LobbyChatUpdate_t, m_SteamOnLobbyChatUpdate);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Steam callresults target one listener
	CRYSTEAM_CALLRESULT(CCrySteamMatchMaking, OnLobbyCreated, LobbyCreated_t, m_SteamOnLobbyCreated);
	CRYSTEAM_CALLRESULT(CCrySteamMatchMaking, OnLobbySearchResults, LobbyMatchList_t, m_SteamOnLobbySearchResults);
	CRYSTEAM_CALLRESULT(CCrySteamMatchMaking, OnLobbyEntered, LobbyEnter_t, m_SteamOnLobbyEntered);
	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Utility functions to send data back to the game on the game thread
	void SendSessionSearchResultsToGame(CryMatchMakingTaskID mmTaskID, TMemHdl resultsHdl, TMemHdl userHdl);
	//////////////////////////////////////////////////////////////////////////////

	ECryLobbyError ConvertSteamErrorToCryLobbyError(uint32 steamErrorCode);

	#if !defined(_RELEASE)
	//////////////////////////////////////////////////////////////////////////////
	// Debug functions
	void Debug_DumpLobbyMembers(CryLobbySessionHandle lsh);
	//////////////////////////////////////////////////////////////////////////////
	#endif // !defined(_RELEASE)

	SRegisteredUserData m_registeredUserData;
	CryLobbyIDArray<SSession, CryLobbySessionHandle, MAX_MATCHMAKING_SESSIONS> m_sessions;
	STask               m_task[MAX_MATCHMAKING_TASKS];
};

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_MATCHMAKING
#endif // __CRYSTEAMMATCHMAKING_H__
