// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamFriendsManagement.h
//  Created:     26/10/2012 by Andrew Catlender
//  Description: Integration of Steamworks API into CryFriends
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSTEAMFRIENDSMANAGEMENT_H__
#define __CRYSTEAMFRIENDSMANAGEMENT_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CrySteamMatchMaking.h"

#if USE_STEAM && USE_CRY_FRIENDS

	#include "CryFriendsManagement.h"

////////////////////////////////////////////////////////////////////////////////

class CCrySteamFriendsManagement : public CCryFriendsManagement
{
public:
	CCrySteamFriendsManagement(CCryLobby* pLobby, CCryLobbyService* pService);

	// ICryFriendsManagment
	virtual ECryLobbyError FriendsManagementSendFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementAcceptFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementRejectFriendRequest(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementRevokeFriendStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementIsUserFriend(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementFindUser(uint32 user, SFriendManagementSearchInfo* pUserName, uint32 maxResults, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementBlockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementUnblockUser(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementIsUserBlocked(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementInfoCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementAcceptInvite(uint32 user, CryUserID* pUserID, CryLobbyTaskID* pTaskID, CryFriendsManagementCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementGetName(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementSearchCallback pCb, void* pCbArg);
	virtual ECryLobbyError FriendsManagementGetStatus(uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsManagementStatusCallback pCb, void* pCbArg);
	// ~ICryFriendsManagment

	void Tick(CTimeValue tv);

private:
	enum ETask
	{
		eT_FriendsManagementIsUserFriend,
		eT_FriendsManagementIsUserBlocked,
		eT_FriendsManagementAcceptInvite,
		eT_FriendsManagementGetName,
		eT_FriendsManagementGetStatus
	};

	//////////////////////////////////////////////////////////////////////////////

	struct  STask : public CCryFriendsManagement::STask
	{
		CTimeValue m_timeStarted;
	};

	//////////////////////////////////////////////////////////////////////////////

	ECryLobbyError InitialiseTask(ETask task, uint32 user, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, void* pCb, void* pCbArg);
	ECryLobbyError StartTask(ETask task, bool startRunning, uint32 user, CryFriendsManagementTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg);
	void           StartTaskRunning(CryFriendsManagementTaskID fTaskID);
	void           EndTask(CryFriendsManagementTaskID fTaskID);
	void           StopTaskRunning(CryFriendsManagementTaskID fTaskID);

	//////////////////////////////////////////////////////////////////////////////

	void StartFriendsManagementIsUserFriend(CryFriendsManagementTaskID fTaskID);
	void EndFriendsManagementIsUserFriend(CryFriendsManagementTaskID fTaskID);

	void StartFriendsManagementIsUserBlocked(CryFriendsManagementTaskID fTaskID);
	void EndFriendsManagementIsUserBlocked(CryFriendsManagementTaskID fTaskID);

	void StartFriendsManagementAcceptInvite(CryFriendsManagementTaskID fTaskID);

	void StartFriendsManagementGetName(CryFriendsManagementTaskID fTaskID);
	void EndFriendsManagementGetName(CryFriendsManagementTaskID fTaskID);

	void StartFriendsManagementGetStatus(CryFriendsManagementTaskID fTaskID);
	void EndFriendsManagementGetStatus(CryFriendsManagementTaskID fTaskID);

	//////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	// Steam callbacks are broadcast to all listeners
	//////////////////////////////////////////////////////////////////////////////

	STask m_task[MAX_FRIENDS_MANAGEMENT_TASKS];
};

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_FRIENDS
#endif // __CRYSTEAMFRIENDSMANAGEMENT_H__
