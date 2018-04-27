// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#include "CrySimpleMutex.hpp"
#include "../Error.hpp"
#include "../STLHelper.hpp"


CCrySimpleMutex::CCrySimpleMutex()
{
	//m_Mutex = CreateMutex(NULL,FALSE,NULL);
	InitializeCriticalSectionAndSpinCount(&cs,10000);
}

CCrySimpleMutex::~CCrySimpleMutex()
{
	DeleteCriticalSection(&cs);
	//CloseHandle(m_Mutex);
}


void CCrySimpleMutex::Lock()
{
	EnterCriticalSection(&cs);
	//while(WAIT_OBJECT_0!=WaitForSingleObject(m_Mutex,INFINITE)) {}
}

void CCrySimpleMutex::Unlock()
{
	LeaveCriticalSection(&cs);
	//ReleaseMutex(m_Mutex);
}

