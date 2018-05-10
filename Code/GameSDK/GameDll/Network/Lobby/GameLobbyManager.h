// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Controller to handle multiple game lobby sessions and 
interface with the UI. Need to handle multiple sessions for lobby merging 
so that you can be hosting a session, while joining another session to 
reserve enough space to merge you're currently hosted session.

-------------------------------------------------------------------------
History:
- 28:04:2010 : Created By Ben Parbury

*************************************************************************/

#ifndef ___GAMELOBBYMANAGER_H___
#define ___GAMELOBBYMANAGER_H___

#include <CryLobby/ICryLobby.h>
#include "GameMechanismManager/GameMechanismBase.h"

class CGameLobby;
class CMatchMakingHandler;

struct IPrivateGameListener
{
	virtual ~IPrivateGameListener(){}
	virtual void SetPrivateGame(const bool privateGame) = 0;
};

class CGameLobbyManager : public CGameMechanismBase
{
public:
	enum ELeavingSessionReason
	{
		eLSR_Menu,																// User has backed out of the lobby using the menu
		eLSR_ReceivedSquadLeavingFromSquadHost,		// Squad is leaving - message came from squad host
		eLSR_ReceivedSquadLeavingFromGameHost,		// Squad is leaving - message came from game host
		eLSR_AcceptingInvite,											// User has accepted an invite
		eLSR_SignedOut,														// User has signed out
		eLSR_SwitchGameType,											// Switching game type (MP->SP or SP->MP)
		eLSR_KickedFromSquad,											// User has been kicked by the squad leader
	};

	CGameLobbyManager();
	virtual ~CGameLobbyManager();

	CGameLobby* GetGameLobby() const;
	CGameLobby* GetNextGameLobby() const;

	CMatchMakingHandler* GetMatchMakingHandler() const { return m_pMatchMakingHandler; }

	bool IsPrimarySession(CGameLobby *pLobby);
	bool IsNewSession(CGameLobby *pLobby);

	bool NewSessionRequest(CGameLobby* pLobby, CrySessionID sessionId);
	void NewSessionResponse(CGameLobby* pLobby, CrySessionID sessionId);
	void CompleteMerge(CrySessionID sessionId);

	void DeletedSession(CGameLobby* pLobby);

	void CancelMoveSession(CGameLobby* pLobby);
	
	void SetMultiplayer(const bool multiplayer);
	bool IsMultiplayer() { return m_multiplayer; }

	bool IsChatRestricted() { return m_isChatRestricted; }

	bool IsLobbyMerging() const
	{
		return (m_nextLobby != NULL);
	}

	bool IsMergingComplete() const
	{
		return m_bMergingIsComplete;
	}

	bool HaveActiveLobby(bool includeLeaving=true) const;

	void LeaveGameSession(ELeavingSessionReason reason);

	void SetCableConnected(bool isConnected) { m_isCableConnected = isConnected; }
	bool IsCableConnected() { return m_isCableConnected; }
	EOnlineState GetOnlineState(uint32 userIndex) const { CRY_ASSERT(userIndex < MAX_LOCAL_USERS); return m_onlineState[userIndex]; }

	// CGameMechanismBase
	virtual void Update( float dt );
	// ~CGameMechanismBase
	
	void MoveUsers(CGameLobby *pFromLobby);

	void AddPrivateGameListener(IPrivateGameListener* pListener);
	void RemovePrivateGameListener(IPrivateGameListener* pListener);

	void SetPrivateGame(CGameLobby *pLobby, const bool privateGame);

	ECryLobbyError DoUserSignOut();
	bool IsSigningOut() { return m_signOutTaskID != CryLobbyInvalidTaskID; }

protected:
	void DoPendingDeleteSession(CGameLobby *pLobby);

	typedef std::vector<IPrivateGameListener*> TPrivateGameListenerVec;
	TPrivateGameListenerVec m_privateGameListeners;

	CGameLobby *m_primaryLobby;
	CGameLobby *m_nextLobby;

	CMatchMakingHandler*	m_pMatchMakingHandler;

	EOnlineState m_onlineState[MAX_LOCAL_USERS];
	CryLobbyTaskID m_signOutTaskID;

	bool m_multiplayer;

	bool m_pendingPrimarySessionDelete;
	bool m_pendingNextSessionDelete;
	bool m_isCableConnected;
	bool m_isChatRestricted;
	bool m_bMergingIsComplete;

	// lobby callbacks
	static void OnlineCallback(UCryLobbyEventData eventData, void *arg);
	static void EthernetStateCallback(UCryLobbyEventData eventData, void *arg);
	static void ChatRestrictedCallback(UCryLobbyEventData eventData, void *arg);
	static void UserSignoutCallback(CryLobbyTaskID taskID, ECryLobbyError error, uint32 user, void *pArg); 
};

#endif
