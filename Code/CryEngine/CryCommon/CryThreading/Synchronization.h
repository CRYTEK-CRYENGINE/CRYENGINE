// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Created by: Michael Smith
// Modified 2008-06, Scott Peter
//	Refactored into PSwap and PLock components, shortened names

//---------------------------------------------------------------------------
#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__

//---------------------------------------------------------------------------
// Synchronization policies, for classes (e.g. containers, allocators) that may
// or may not be multithread-safe.
//
// Policies should be used as a template argument to such classes,
// and these class implementations should then utilise the policy, as a base-class or member.
//
//---------------------------------------------------------------------------

#include "CryThread.h"

namespace stl
{
template<class Sync>
struct AutoLock
{
	ILINE AutoLock(Sync& sync)
		: _sync(sync)
	{
		sync.Lock();
	}
	ILINE ~AutoLock()
	{
		_sync.Unlock();
	}

private:
	Sync& _sync;
};

struct PSyncNone
{
	void Lock()   {}
	void Unlock() {}
};

struct PSyncMultiThread
{
	PSyncMultiThread()
		: _Semaphore(0) {}

	void Lock()
	{
		CryWriteLock(&_Semaphore);
	}
	void Unlock()
	{
		CryReleaseWriteLock(&_Semaphore);
	}
	int IsLocked() const volatile
	{
		return _Semaphore;
	}

private:
	volatile int _Semaphore;
};

#ifdef _DEBUG

struct PSyncDebug : public PSyncMultiThread
{
	void Lock()
	{
		assert(!IsLocked());
		PSyncMultiThread::Lock();
	}
};

#else

typedef PSyncNone PSyncDebug;

#endif

};

#endif // __SYNCHRONIZATION_H__
