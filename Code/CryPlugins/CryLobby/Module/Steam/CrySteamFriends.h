// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamFriends.h
//  Created:     26/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryFriends
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSTEAMFRIENDS_H__
#define __CRYSTEAMFRIENDS_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CrySteamMatchMaking.h"

#if USE_STEAM && USE_CRY_FRIENDS

	#include "CryFriends.h"

////////////////////////////////////////////////////////////////////////////////

class CCrySteamFriends : public CCryFriends
{
public:
	CCrySteamFriends(CCryLobby* pLobby, CCryLobbyService* pService);

	// ICryFriends
	virtual ECryLobbyError FriendsGetFriendsList(uint32 user, uint32 start, uint32 num, CryLobbyTaskID* pTaskID, CryFriendsGetFriendsListCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsSendGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsCallback pCb, void* pCbArg);
	// ~ICryFriends

	void Tick(CTimeValue tv);

private:
	enum ETask
	{
		eT_FriendsGetFriendsList,
		eT_FriendsSendGameInvite
	};

	//////////////////////////////////////////////////////////////////////////////

	struct  STask : public CCryFriends::STask
	{
		CTimeValue m_timeStarted;
	};

	//////////////////////////////////////////////////////////////////////////////

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartTaskRunning(CryFriendsTaskID fTaskID);
	void           EndTask(CryFriendsTaskID fTaskID);
	void           StopTaskRunning(CryFriendsTaskID fTaskID);

	//////////////////////////////////////////////////////////////////////////////

	void StartFriendsGetFriendsList(CryFriendsTaskID fTaskID);
	void TickFriendsGetFriendsList(CryFriendsTaskID fTaskID);
	void EndFriendsGetFriendsList(CryFriendsTaskID fTaskID);

	void StartFriendsSendGameInvite(CryFriendsTaskID fTaskID);

	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Steam callbacks are broadcast to all listeners
	CRYSTEAM_CALLBACK(CCrySteamFriends, OnFriendRichPresenceUpdate, FriendRichPresenceUpdate_t, m_SteamOnFriendRichPresenceUpdate);
	CRYSTEAM_CALLBACK(CCrySteamFriends, OnGameLobbyJoinRequested, GameLobbyJoinRequested_t, m_SteamOnGameLobbyJoinRequested);
	//////////////////////////////////////////////////////////////////////////////

	STask m_task[MAX_FRIENDS_TASKS];
};

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_FRIENDS
#endif // __CRYSTEAMFRIENDS_H__
