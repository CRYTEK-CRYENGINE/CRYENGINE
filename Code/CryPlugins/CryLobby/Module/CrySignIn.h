// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CrySignIn.h
//  Version:     v1.00
//  Created:     09/11/2011 by Paul Mikell.
//  Description: CCrySignIn class definition
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSIGNIN_H__

#define __CRYSIGNIN_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryLobby.h"
#include <CryLobby/ICrySignIn.h>

#define MAX_SIGNIN_PARAMS 10
#define MAX_SIGNIN_TASKS  4

class CCrySignIn : public CMultiThreadRefCount, public ICrySignIn
{
public:

	virtual ~CCrySignIn();

	// ICrySignIn
	virtual void CancelTask(CryLobbyTaskID lTaskID);
	// ~ICrySignIn

	// CWorkQueue::CClassJob*< ..., class U >::U
	virtual bool IsDead() const { return false; }
	// ~CWorkQueue::CClassJob*< ..., class U >::U

	virtual ECryLobbyError Initialise();
	virtual ECryLobbyError Terminate();
	virtual void           Tick(CTimeValue tv);

protected:

	enum ETask
	{
		eT_Invalid = -1,
		eT_SignInUser,
		eT_SignOutUser,
		eT_CreateUser,
		eT_CreateAndSignInUser,
		eT_GetUserCredentials
	};

	struct  STask
	{
		CryLobbyTaskID lTaskID;
		ECryLobbyError error;
		uint32         startedTask;
		uint32         subTask;
		void*          pCb;
		void*          pCbArg;
		TMemHdl        paramsMem[MAX_SIGNIN_PARAMS];
		uint32         paramsNum[MAX_SIGNIN_PARAMS];
		bool           used;
		bool           running;
		bool           cancelled;
	};

	struct SCrySignInTaskID {};
	typedef CryLobbyID<SCrySignInTaskID, MAX_SIGNIN_TASKS>             CrySignInTaskID;
	typedef CryLobbyIDArray<STask*, CrySignInTaskID, MAX_SIGNIN_TASKS> CrySignInTaskPtrArray;

	CCrySignIn(CCryLobby* pLobby, CCryLobbyService* pService);

	virtual void   StartTaskRunning(CrySignInTaskID siTaskID);
	virtual void   StopTaskRunning(CrySignInTaskID siTaskID);
	virtual void   EndTask(CrySignInTaskID siTaskID);

	ECryLobbyError StartTask(uint32 eTask, CrySignInTaskID* pSITaskID, CryLobbyTaskID* pLTaskID, void* pCb, void* pCbArg);

	virtual void   FreeTask(CrySignInTaskID siTaskID);
	ECryLobbyError CreateTaskParamMem(CrySignInTaskID siTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	void           UpdateTaskError(CrySignInTaskID siTaskID, ECryLobbyError error);
	virtual void   CreateTask(CrySignInTaskID siTaskID, uint32 eTask, CryLobbyTaskID lTaskID, void* pCb, void* pCbArg);

	CrySignInTaskPtrArray m_pTask;
	CCryLobby*            m_pLobby;
	CCryLobbyService*     m_pService;

private:

	virtual void ClearTask(CrySignInTaskID siTaskID);
};

#endif //__CRYSIGNIN_H__
