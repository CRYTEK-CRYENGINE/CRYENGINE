// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UILobbyMP.h
//  Version:     v1.00
//  Created:     08/06/2012 by Michiel Meesters.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UILOBBYMP_H_
#define __UILOBBYMP_H_

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryLobby/ICryStats.h>
#include <CryLobby/ICryLobby.h>
#include "Network/Lobby/GameLobbyData.h"
#include <CryLobby/ICryFriends.h>

class CUILobbyMP 
	: public IUIGameEventSystem
	, public IUIModule
{
public:
	CUILobbyMP();

	// IUIGameEventSystem
	UIEVENTSYSTEM( "UILobbyMP" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;

	// IUIModule
	virtual void Reset() override;
	// ~IUIModule

	// UI functions
	void InviteAccepted();
	void SearchCompleted();
	void SearchStarted();
	void UpdateNatType();
	void ServerFound(SCrySessionSearchResult session, string sServerName);
	void PlayerListReturn(const SUIArguments& players, const SUIArguments& playerids);
	void ReadLeaderBoard(int leaderboardIdx, int mode, int nrOfEntries);
	void WriteLeaderBoard(int leaderboardIdx, const char* values);
	void RegisterLeaderBoard(string leaderboardName, int leaderBoardIdx, int nrOfColumns);
	void ShowLoadingDialog(const  char* sLoadingDialog);
	void HideLoadingDialog(const  char* sLoadingDialog);
	void LeaderboardEntryReturn(const SUIArguments& customColumns);
	void InviteFriends(int containerIdx, int friendIdx);
	void SendUserStat(SUIArguments& arg);

	//Callback when session is found
	static void MatchmakingSessionSearchCallback(CryLobbyTaskID taskID, ECryLobbyError error, SCrySessionSearchResult* session, void* arg);
	static void ReadLeaderBoardCB(CryLobbyTaskID TaskID, ECryLobbyError Error, SCryStatsLeaderBoardReadResult *Result, void *Arg);
	static void RegisterLeaderboardCB(CryLobbyTaskID TaskID, ECryLobbyError Error, void *Arg);
	static void WriteLeaderboardCallback(CryLobbyTaskID TaskID, ECryLobbyError Error, void *Arg);

	static void ReadUserDataCallback(CryLobbyTaskID taskID, ECryLobbyError error, SCryLobbyUserData* pData, uint32 numData, void* pArg);

	static void GetFriendsCB(CryLobbyTaskID taskID, ECryLobbyError error, SFriendInfo* pFriendInfo, uint32 numFriends, void* pArg);
	static void InviteFriendsCB(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

private:
	// UI events
	void GetUserStats();
	void SearchGames(bool bLan);
	void AwardTrophy(int trophy);
	void JoinGame(unsigned int sessionID);
	void HostGame(bool bLan, string sMapPath, string sGameRules);
	void SetMultiplayer(bool bIsMultiplayer);
	void LockController( bool bLock );
	void LeaveGame();
	void GetPlayerList();
	void MutePlayer(int playerId, bool mute);
	void GetFriends(int containerIdx);

private:
	enum EUIEvent
	{
		eUIE_ServerFound = 0,
		eUIE_PlayerListReturn,
		eUIE_PlayerIdListReturn,
		eUIE_InviteAccepted,
		eUIE_SearchStarted,
		eUIE_SearchCompleted,
		eUIE_NatTypeUpdated,
		eUIE_ShowLoadingDialog,
		eUIE_HideLoadingDialog,
		eUIE_LeaderboardEntryReturn,
		eUIE_GetFriendsCompleted,
		eUIE_UserStatRead,
	};

	SUIEventReceiverDispatcher<CUILobbyMP> m_eventDispatcher;
	SUIEventSenderDispatcher<EUIEvent> m_eventSender;
	IUIEventSystem* m_pUIEvents;
	IUIEventSystem* m_pUIFunctions;
	std::vector<SCrySessionSearchResult> m_FoundServers;
};


#endif // __UILOBBYMP_H_
