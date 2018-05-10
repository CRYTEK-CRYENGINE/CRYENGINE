// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYFRIENDSMANAGEMENT_H__
#define __CRYFRIENDSMANAGEMENT_H__

#pragma once

#include <CryLobby/ICryFriendsManagement.h>
#include "CryLobby.h"
#include "CryMatchMaking.h"

#define MAX_FRIENDS_MANAGEMENT_PARAMS 3
#define MAX_FRIENDS_MANAGEMENT_TASKS  4

typedef uint32 CryFriendsManagementTaskID;
const CryFriendsManagementTaskID CryFriendsManagementInvalidTaskID = 0xffffffff;

class CCryFriendsManagement : public CMultiThreadRefCount, public ICryFriendsManagement
{
public:
	CCryFriendsManagement(CCryLobby* pLobby, CCryLobbyService* pService);

	ECryLobbyError Initialise();
	ECryLobbyError Terminate();

	virtual bool   IsDead() const { return false; }

	virtual void   CancelTask(CryLobbyTaskID lTaskID);

protected:

	struct  STask
	{
		CryLobbyTaskID lTaskID;
		ECryLobbyError error;
		uint32         startedTask;
		uint32         subTask;
		void*          pCb;
		void*          pCbArg;
		TMemHdl        paramsMem[MAX_FRIENDS_MANAGEMENT_PARAMS];
		uint32         paramsNum[MAX_FRIENDS_MANAGEMENT_PARAMS];
		bool           used;
		bool           running;
		bool           canceled;
	};

	ECryLobbyError StartTask(uint32 eTask, bool startRunning, CryFriendsManagementTaskID* pFTaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg);
	virtual void   FreeTask(CryFriendsManagementTaskID fTaskID);
	ECryLobbyError CreateTaskParamMem(CryFriendsManagementTaskID fTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	void           UpdateTaskError(CryFriendsManagementTaskID fTaskID, ECryLobbyError error);

	STask*            m_pTask[MAX_FRIENDS_MANAGEMENT_TASKS];
	CCryLobby*        m_pLobby;
	CCryLobbyService* m_pService;
};

#endif // __CRYFRIENDSMANAGEMENT_H__
