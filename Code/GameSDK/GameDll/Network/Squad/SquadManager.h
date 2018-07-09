// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Session handler for squads (similar to parties from other popular online shooters)

-------------------------------------------------------------------------
History:
- 05:03:2010 : Created By Ben Parbury

*************************************************************************/
#ifndef ___SQUAD_H___
#define ___SQUAD_H___

#include <CryLobby/ICryLobby.h>
#include "GameMechanismManager/GameMechanismBase.h"
#include "Network/Squad/ISquadEventListener.h"
#include "Network/Lobby/AutoLockData.h"
#include "Network/Lobby/SessionNames.h"
#include "Network/Lobby/GameUserPackets.h"
#include "Network/Lobby/GameLobby.h"
#include "Network/LobbyTaskQueue.h"

#define SQUADMGR_MAX_SQUAD_SIZE			(MAX_PLAYER_LIMIT)

#define SQUADMGR_DBG_ADD_FAKE_RESERVATION 0

#define SQUADMGR_NUM_STORED_KICKED_SESSION 8

class CSquadManager : public CGameMechanismBase,
											public IHostMigrationEventListener,
											public IGameWarningsListener
{
private:
	typedef std::vector<ISquadEventListener*> TSquadEventListenersVec;

public:
	enum EGameSessionChange
	{
		eGSC_JoinedNewSession,
		eGSC_LeftSession,
		eGSC_LobbyMerged,
		eGSC_LobbyMigrated,
	};

	enum ESessionSlotType
	{
		eSST_Public,
		eSST_Private,
	};

	CSquadManager();
	virtual ~CSquadManager();

	// Squad Event Listeners
	void RegisterSquadEventListener(ISquadEventListener *pListener);
	void UnregisterSquadEventListener(ISquadEventListener *pListener);

	void EventListenerSquaddieAdded(CryUserID userId);
	void EventListenerSquaddieRemoved(CryUserID userId);
	void EventListenerSquaddieUpdated(CryUserID userId);
	void EventListenerSquadLeaderChanged(CryUserID userId);
	void EventListenerSquadEvent(ISquadEventListener::ESquadEventType eventType);
	// ~Squad Event Listeners

	virtual void Update(float dt);

	inline bool HaveSquadMates() const { return m_nameList.Size() > 1; }
	inline bool InCharge() const { return m_squadLeader; }
	inline bool InSquad() const { return m_squadHandle != CrySessionInvalidHandle; }
	inline bool IsLeavingSquad() const { return m_leavingSession; }
	inline int GetSquadSize() const { return m_nameList.Size(); }
	inline CrySessionHandle GetSquadSessionHandle() { return m_squadHandle; }
	inline CryUserID GetSquadLeader() { return m_squadLeaderId; }

	bool IsSquadMateByUserId(CryUserID userId);
	
	void GameSessionIdChanged(EGameSessionChange eventType, CrySessionID gameSessionId);
	void ReservationsFinished(EReservationResult result);
	void LeftGameSessionInProgress();

	void JoinGameSession(CrySessionID gameSessionId, bool bIsMatchmakingSession);
	void TellMembersToLeaveGameSession();
	void RequestLeaveSquad();

	void Enable(bool enable, bool allowCreate);
	CryUserID GetUserIDFromChannelID(int channelId);

	void LocalUserDataUpdated();

#if !defined(_RELEASE)
	static void CmdCreate(IConsoleCmdArgs* pCmdArgs);
	static void CmdLeave(IConsoleCmdArgs* pCmdArgs);
	static void CmdKick(IConsoleCmdArgs* pCmdArgs);
#endif

	void SendSquadPacket(GameUserPacketDefinitions packetType, SCryMatchMakingConnectionUID connectionUID = CryMatchMakingInvalidConnectionUID);
	static void HandleCustomError(const char* dialogName, const char* msgPreLoc, const bool deleteSession, const bool returnToMainMenu);

	// IHostMigrationEventListener
	virtual EHostMigrationReturn OnInitiate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnDisconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnDemoteToClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnPromoteToServer(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnReconnectClient(SHostMigrationInfo& hostMigrationInfo, HMStateType& state) { return IHostMigrationEventListener::Listener_Done; }
	virtual EHostMigrationReturn OnFinalise(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual EHostMigrationReturn OnTerminate(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	virtual void OnComplete(SHostMigrationInfo& hostMigrationInfo) {}
	virtual EHostMigrationReturn OnReset(SHostMigrationInfo& hostMigrationInfo, HMStateType& state);
	// ~IHostMigrationEventListener

	// IGameWarningsListener
	virtual bool OnWarningReturn(THUDWarningId id, const char* returnValue);
	virtual void OnWarningRemoved(THUDWarningId id) {}
	// ~IGameWarningsListener

	void SetMultiplayer(bool multiplayer);
	bool GetSquadCommonDLCs(uint32 &commonDLCs);

	void InviteAccepted(CrySessionID id);
	void JoinInvite();
	inline void SetInvitePending(bool yesNo) { m_pendingInvite = yesNo; }

	void OnGameSessionStarted();
	void OnGameSessionEnded();

	bool SquadsSupported();
	void SessionChangeSlotType(ESessionSlotType type);

	void KickPlayer(CryUserID userId);
	bool AllowedToJoinSession(CrySessionID sessionId);
	void RemoveFromBannedList(CrySessionID sessionId);

	bool IsEnabled();

	const SSessionNames *GetSessionNames() const
	{
		return &m_nameList;
	}

private:

	struct SKickedSession
	{
		CrySessionID m_sessionId;
		CTimeValue m_timeKicked;
	};

	typedef CryFixedArray<SKickedSession, SQUADMGR_NUM_STORED_KICKED_SESSION> TKickedSessionsArray;

	struct SPendingGameJoin
	{
		CrySessionID	m_sessionID;
		uint32				m_playlistID;
		int						m_restrictRank;
		int						m_requireRank;
		bool					m_isMatchmakingGame;
		bool					m_isValid;

		SPendingGameJoin()
		{
			Invalidate();
		}

		void Set(CrySessionID sessionID, bool isMatchmaking, uint32 playlistID, int restrictRank, int requireRank, bool isValid)
		{
			m_sessionID = sessionID;
			m_playlistID = playlistID;
			m_restrictRank = restrictRank;
			m_isMatchmakingGame = isMatchmaking;
			m_requireRank = requireRank;
			m_isValid = isValid;
		}

		void Invalidate()
		{
			Set(CrySessionInvalidID, false, 0, 0, 0, false);
		}

		bool IsValid()
		{
			return m_isValid;
		}
	};

	TSquadEventListenersVec   m_squadEventListeners;

	SSessionNames m_nameList;

	TKickedSessionsArray m_kickedSessions;
	string m_kickedSessionsUsername;

	CrySessionHandle m_squadHandle;

	CrySessionID m_currentGameSessionId;
	CrySessionID m_requestedGameSessionId;
	CrySessionID m_inviteSessionId;

	CryUserID m_squadLeaderId;
	CryUserID m_pendingKickUserId;

	CryLobbyTaskID m_currentTaskId;

	ELobbyState	m_leaderLobbyState;

	ESessionSlotType m_slotType;
	ESessionSlotType m_requestedSlotType;
	ESessionSlotType m_inProgressSlotType;

	SPendingGameJoin m_pendingGameJoin;

	float m_inviteJoiningTime;

	bool m_squadLeader;
	bool m_isNewSquad;
	bool m_bMultiplayerGame;
	bool m_pendingInvite;
	bool m_bSessionStarted;
	bool m_bGameSessionStarted;
	bool m_sessionIsInvalid;
	bool m_leavingSession;

	CLobbyTaskQueue m_taskQueue;

	struct SFlashSquadPlayerInfo
	{
		SFlashSquadPlayerInfo()
		{
			m_conId = 0;
			m_rank = 0;
			m_reincarnations = 0;
			m_isLocal = false;
			m_isSquadLeader = false;
		}
		
		CryFixedStringT<DISPLAY_NAME_LENGTH> m_nameString;
		uint32 m_conId;
		uint8 m_rank;
		uint8 m_reincarnations;
		bool m_isLocal;
		bool m_isSquadLeader;
	};

	static void ReportError(ECryLobbyError);

	void SetSquadHandle(CrySessionHandle handle);

	void ReadSquadPacket(SCryLobbyUserPacketData** ppPacketData);

	void OnSquadLeaderChanged();
	void JoinUser(SCryUserInfoResult* user);
	void UpdateUser(SCryUserInfoResult* user);
	void LeaveUser(SCryUserInfoResult* user);

	void TaskFinished();
	bool CallbackReceived(CryLobbyTaskID taskId, ECryLobbyError result);

	void DeleteSession();
	void CleanUpSession();


	ECryLobbyError DoCreateSquad(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoJoinSquad(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoLeaveSquad(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoUpdateLocalUserData(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoStartSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoEndSession(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoSessionChangeSlotType(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);
	ECryLobbyError DoSessionSetLocalFlags(ICryMatchMaking *pMatchMaking, CryLobbyTaskID &taskId);


	void JoinSessionFinished(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle hdl);
	void SquadSessionDeleted(CryLobbyTaskID taskID, ECryLobbyError error);
	void SessionChangeSlotTypeFinished(CryLobbyTaskID taskID, ECryLobbyError error);

	void SquadJoinGame(CrySessionID sessionID, bool isMatchmakingGame, uint32 playlistID, int restrictRank, int requireRank);

	static void CreateCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, void* arg);
	static void JoinCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 ip, uint16 port, void* arg);
	static void DeleteCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* arg);
	static void SessionChangeSlotTypeCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);
	static void UserPacketCallback(UCryLobbyEventData eventData, void *userParam);
	static void OnlineCallback(UCryLobbyEventData eventData, void *userParam);
	static void JoinUserCallback(UCryLobbyEventData eventData, void *userParam);
	static void LeaveUserCallback(UCryLobbyEventData eventData, void *userParam);
	static void UpdateUserCallback(UCryLobbyEventData eventData, void *userParam);
	static void UpdateLocalUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* arg);
	static void UpdateOfflineState(CSquadManager *pSquadManager);
	static void SessionStartCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);
	static void SessionEndCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);
	static void SessionClosedCallback(UCryLobbyEventData eventData, void *userParam);
	static void SessionSetLocalFlagsCallback(CryLobbyTaskID taskID, ECryLobbyError error, CrySessionHandle h, uint32 flags, void* pArg);
	static void ForcedFromRoomCallback(UCryLobbyEventData eventData, void *pArg);
	
	static void TaskStartedCallback(CLobbyTaskQueue::ESessionTask task, void *pArg);
};

#endif
