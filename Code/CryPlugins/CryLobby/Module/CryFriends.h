// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYFRIENDS_H__
#define __CRYFRIENDS_H__

#pragma once

#include <CryLobby/ICryFriends.h>
#include "CryLobby.h"
#include "CryMatchMaking.h"

#if USE_CRY_FRIENDS

	#define MAX_FRIENDS_PARAMS 2
	#define MAX_FRIENDS_TASKS  2

typedef uint32 CryFriendsTaskID;
const CryFriendsTaskID CryFriendsInvalidTaskID = 0xffffffff;

class CCryFriends : public CMultiThreadRefCount, public ICryFriends
{
public:
	CCryFriends(CCryLobby* pLobby, CCryLobbyService* pService);

	virtual ECryLobbyError Initialise();
	virtual ECryLobbyError Terminate();

	virtual bool           IsDead() const { return false; }

protected:

	struct  STask
	{
		CryLobbyTaskID        lTaskID;
		ECryLobbyError        error;
		uint32                startedTask;
		uint32                subTask;
		CryLobbySessionHandle session;
		void*                 pCb;
		void*                 pCbArg;
		TMemHdl               paramsMem[MAX_FRIENDS_PARAMS];
		uint32                paramsNum[MAX_FRIENDS_PARAMS];
		bool                  used;
		bool                  running;
		bool                  canceled;
	};

	ECryLobbyError StartTask(uint32 eTask, bool startRunning, CryFriendsTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	virtual void   FreeTask(CryFriendsTaskID fTaskID);
	void           CancelTask(CryLobbyTaskID lTaskID);
	ECryLobbyError CreateTaskParamMem(CryFriendsTaskID fTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	void           UpdateTaskError(CryFriendsTaskID fTaskID, ECryLobbyError error);

	STask*            m_pTask[MAX_FRIENDS_TASKS];
	CCryLobby*        m_pLobby;
	CCryLobbyService* m_pService;
};

#endif // USE_CRY_FRIENDS
#endif // __CRYFRIENDS_H__
