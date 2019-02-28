// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEMUTEX__
#define __CRYSIMPLEMUTEX__

#include "../Common.h"

class CCrySimpleMutex
{
	//HANDLE		m_Mutex;
	CRITICAL_SECTION cs;
public:
						CCrySimpleMutex();
						~CCrySimpleMutex();

	void			Lock();
	void			Unlock();
};

class CCrySimpleMutexAutoLock
{
	CCrySimpleMutex&			m_rMutex;
public:
												CCrySimpleMutexAutoLock(CCrySimpleMutex& rMutex):
												m_rMutex(rMutex)
												{
													rMutex.Lock();
												}
												~CCrySimpleMutexAutoLock()
												{
													m_rMutex.Unlock();
												}
};
#endif
