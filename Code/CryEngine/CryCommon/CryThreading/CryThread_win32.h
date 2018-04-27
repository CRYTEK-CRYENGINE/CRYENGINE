// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <process.h>

namespace CryMT {
namespace detail {
enum eLOCK_TYPE
{
	eLockType_CRITICAL_SECTION,
	eLockType_SRW,
	eLockType_MUTEX
};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//! From winnt.h.
// Since we are not allowed to include windows.h while being included from platform.h and there seems to be no good way to include the
// required windows headers directly; without including a lot of other header, define a 1:1 copy of the required primitives defined in winnt.h.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
struct CRY_CRITICAL_SECTION // From winnt.h
{
	void*          DebugInfo;
	long           LockCount;
	long           RecursionCount;
	void*          OwningThread;
	void*          LockSemaphore;
	unsigned long* SpinCount;  //!< Force size on 64-bit systems when packed.
};

//////////////////////////////////////////////////////////////////////////
struct CRY_SRWLOCK // From winnt.h
{
	CRY_SRWLOCK();
	void* SRWLock_;
};

//////////////////////////////////////////////////////////////////////////
struct CRY_CONDITION_VARIABLE // From winnt.h
{
	CRY_CONDITION_VARIABLE();
	void* condVar_;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
class CryLock_SRWLOCK
{
public:
	static const eLOCK_TYPE s_value = eLockType_SRW;
	friend class CryConditionVariable;
public:
	CryLock_SRWLOCK() = default;

	void Lock();
	void Unlock();
	bool TryLock();

	void LockShared();
	void UnlockShared();
	bool TryLockShared();

private:
	CryLock_SRWLOCK(const CryLock_SRWLOCK&) = delete;
	CryLock_SRWLOCK& operator=(const CryLock_SRWLOCK&) = delete;

private:
	CRY_SRWLOCK m_win32_lock_type;
};

//////////////////////////////////////////////////////////////////////////
// SRW Lock (Slim Reader/Writer Lock)
// Faster + lighter than CriticalSection. Also only enters into kernel mode if contended.
// Cannot be shared between processes.
class CryLock_SRWLOCK_Recursive
{
public:
	static const eLOCK_TYPE s_value = eLockType_SRW;
	friend class CryConditionVariable;

public:
	CryLock_SRWLOCK_Recursive() : m_recurseCounter(0), m_exclusiveOwningThreadId(THREADID_NULL) {}

	void Lock();
	void Unlock();
	bool TryLock();

	// Deprecated
#ifndef _RELEASE
	bool IsLocked()
	{
		return m_exclusiveOwningThreadId == CryGetCurrentThreadId();
	}
#endif

private:
	CryLock_SRWLOCK_Recursive(const CryLock_SRWLOCK_Recursive&) = delete;
	CryLock_SRWLOCK_Recursive& operator=(const CryLock_SRWLOCK_Recursive&) = delete;

private:
	CryLock_SRWLOCK m_win32_lock_type;
	uint32          m_recurseCounter;
	
	// Due to its semantics, this member can be accessed in an unprotected manner,
	// but only for comparison with the current tid.
	threadID        m_exclusiveOwningThreadId;
};

//////////////////////////////////////////////////////////////////////////
// Critical section
// Faster then WinMutex as it only enters into kernel mode if contended.
// Cannot be shared between processes.
class CryLock_CriticalSection
{
public:
	static const eLOCK_TYPE s_value = eLockType_CRITICAL_SECTION;
	friend class CryConditionVariable;

public:
	CryLock_CriticalSection();
	~CryLock_CriticalSection();

	void Lock();
	void Unlock();
	bool TryLock();

	//! Deprecated: do not use this function - its return value might already be wrong the moment it is returned.
#ifndef _RELEASE
	bool IsLocked()
	{
		return m_win32_lock_type.RecursionCount > 0 && (UINT_PTR)m_win32_lock_type.OwningThread == CryGetCurrentThreadId();
	}
#endif

private:
	CryLock_CriticalSection(const CryLock_CriticalSection&) = delete;
	CryLock_CriticalSection& operator=(const CryLock_CriticalSection&) = delete;

private:
	CRY_CRITICAL_SECTION m_win32_lock_type;
};

//////////////////////////////////////////////////////////////////////////
// WinMutex: (slow)
// Calls into kernel even when not contended.
// A named mutex can be shared between different processes.
class CryLock_WinMutex
{
public:
	static const eLOCK_TYPE s_value = eLockType_MUTEX;

	CryLock_WinMutex();
	~CryLock_WinMutex();

	void Lock();
	void Unlock();
	bool TryLock();
#ifndef _RELEASE
	//! Deprecated: do not use this function - its return value might already be wrong the moment it is returned.
	bool IsLocked()
	{
		if (TryLock())
		{
			Unlock();
			return true;
		}
		else
		{
			return false;
		}
	}
#endif
private:
	CryLock_WinMutex(const CryLock_WinMutex&) = delete;
	CryLock_WinMutex& operator=(const CryLock_WinMutex&) = delete;

private:
	void* m_win32_lock_type;
};
} // detail
} // CryMT

  //////////////////////////////////////////////////////////////////////////
  /////////////////////////    DEFINE LOCKS    /////////////////////////////
  //////////////////////////////////////////////////////////////////////////

template<> class CryLockT<CRYLOCK_RECURSIVE> : public CryMT::detail::CryLock_SRWLOCK_Recursive
{
};
template<> class CryLockT<CRYLOCK_FAST> : public CryMT::detail::CryLock_SRWLOCK
{
};

typedef CryMT::detail::CryLock_SRWLOCK_Recursive CryMutex;
typedef CryMT::detail::CryLock_SRWLOCK           CryMutexFast; // Not recursive

//////////////////////////////////////////////////////////////////////////
//! CryEvent represent a synchronization event.
class CryEvent
{
public:
	CryEvent();
	~CryEvent();

	//! Reset the event to the unsignalled state.
	void Reset();

	//! Set the event to the signalled state.
	void Set();

	//! Access a HANDLE to wait on.
	void* GetHandle() const { return m_handle; };

	//! Wait indefinitely for the object to become signalled.
	void Wait() const;

	//! Wait, with a time limit, for the object to become signalled.
	bool Wait(const uint32 timeoutMillis) const;

private:
	CryEvent(const CryEvent&);
	CryEvent& operator=(const CryEvent&);

private:
	void* m_handle;
};
typedef CryEvent CryEventTimed;

//////////////////////////////////////////////////////////////////////////
class CryConditionVariable
{
public:
	CryConditionVariable() = default;
	void Wait(CryMutex& lock);
	void Wait(CryMutexFast& lock);
	bool TimedWait(CryMutex& lock, uint32 millis);
	bool TimedWait(CryMutexFast& lock, uint32 millis);
	void NotifySingle();
	void Notify();

private:
	CryConditionVariable(const CryConditionVariable&);
	CryConditionVariable& operator=(const CryConditionVariable&);

private:
	CryMT::detail::CRY_CONDITION_VARIABLE m_condVar;
};

//////////////////////////////////////////////////////////////////////////
//! Platform independent wrapper for a counting semaphore.
class CrySemaphore
{
public:
	CrySemaphore(int nMaximumCount, int nInitialCount = 0);
	~CrySemaphore();
	void Acquire();
	void Release();

private:
	void* m_Semaphore;
};

//////////////////////////////////////////////////////////////////////////
//! Platform independent wrapper for a counting semaphore
//! except that this version uses C-A-S only until a blocking call is needed.
//! -> No kernel call if there are object in the semaphore.
class CryFastSemaphore
{
public:
	CryFastSemaphore(int nMaximumCount, int nInitialCount = 0);
	~CryFastSemaphore();
	void Acquire();
	void Release();

private:
	CrySemaphore   m_Semaphore;
	volatile int32 m_nCounter;
};

//////////////////////////////////////////////////////////////////////////
class CryRWLock
{
public:
	CryRWLock() = default;

	void RLock();
	void RUnlock();

	void WLock();
	void WUnlock();

	void Lock();
	void Unlock();

	bool TryRLock();
	bool TryWLock();
	bool TryLock();
private:
	CryMT::detail::CryLock_SRWLOCK m_srw;

	CryRWLock(const CryRWLock&) = delete;
	CryRWLock& operator=(const CryRWLock&) = delete;
};
