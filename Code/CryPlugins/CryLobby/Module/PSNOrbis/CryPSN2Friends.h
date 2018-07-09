// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSNFRIENDS_H__
#define __CRYPSNFRIENDS_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include "CryFriends.h"
		#include "CryPSN2Support.h"

class CCryPSNFriends : public CCryFriends
{
public:
	CCryPSNFriends(CCryLobby* pLobby, CCryLobbyService* pService, CCryPSNSupport* pSupport);

	virtual ECryLobbyError FriendsGetFriendsList(uint32 user, uint32 start, uint32 num, CryLobbyTaskID* pTaskID, CryFriendsGetFriendsListCallback cb, void* pCbArg);
	virtual ECryLobbyError FriendsSendGameInvite(uint32 user, CrySessionHandle h, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryFriendsCallback cb, void* pCbArg);

	static void            SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);
	static void            LobbyUIGameInviteCallback(CryLobbyTaskID taskID, ECryLobbyError error, void* pArg);

private:
	enum ETask
	{
		eT_FriendsGetFriendsList,
		eST_WaitingForFriendsListWebApiCallback,
		eT_FriendsSendGameInvite
	};

	void           ProcessWebApiEvent(SCryPSNSupportCallbackEventData& data);

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartSubTask(ETask etask, CryFriendsTaskID fTaskID);
	void           StartTaskRunning(CryFriendsTaskID fTaskID);
	void           EndTask(CryFriendsTaskID fTaskID);
	void           StopTaskRunning(CryFriendsTaskID fTaskID);

	void           StartFriendsGetFriendsList(CryFriendsTaskID fTaskID);
	void           EventWebApiGetFriends(CryFriendsTaskID mmTaskID, SCryPSNSupportCallbackEventData& data);
	void           EndFriendsGetFriendsList(CryFriendsTaskID fTaskID);

	void           StartFriendsSendGameInvite(CryFriendsTaskID fTaskID);

	STask           m_task[MAX_FRIENDS_TASKS];

	CCryPSNSupport* m_pPSNSupport;
};

	#endif // USE_CRY_FRIENDS
#endif   // USE_PSN

#endif // __CRYPSNFRIENDS_H__
