// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>

// PTHREAD_MUTEX_FAST_NP is only defined by Pthreads-w32 & Pthreads-w64
#if !defined(PTHREAD_MUTEX_FAST_NP)
	#define PTHREAD_MUTEX_FAST_NP PTHREAD_MUTEX_NORMAL
#endif

namespace CryMT {
namespace detail {

enum eLOCK_TYPE
{
	eLockType_NORMAL     = PTHREAD_MUTEX_FAST_NP,
	eLockType_RECURSIVE  = PTHREAD_MUTEX_RECURSIVE,
	eLockType_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK
};

//////////////////////////////////////////////////////////////////////////
// Forward declarations (in same namespace)
template<eLOCK_TYPE pthreadMutexType> class CryLock_Mutex;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// PThreadLockAttr wrapper
struct CryPThreadLockAttr
{
public:
	CryPThreadLockAttr(eLOCK_TYPE lockType)
	{
		pthread_mutexattr_init(&m_Attr);
		pthread_mutexattr_settype(&m_Attr, lockType);
	}
	~CryPThreadLockAttr()
	{
		pthread_mutexattr_destroy(&m_Attr);
	}

	pthread_mutexattr_t m_Attr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// PThread mutex can be of type:
// eLockType_NORMAL: No recursive lock functionality
// eLockType_RECURSIVE: Recursive lock functionality
// eLockType_ERRORCHECK: No recursive lock functionality. Added functionality of returning error on invalid lock procedures.
//

template<eLOCK_TYPE pthreadMutexType>
class CryLock_Mutex
{
public:
	static const eLOCK_TYPE s_value = pthreadMutexType;
	friend class::CryConditionVariable;

public:
	CryLock_Mutex()
	{
		const CryPThreadLockAttr attr(pthreadMutexType);
		pthread_mutex_init(&m_posix_mutex, &attr.m_Attr);
	}
	~CryLock_Mutex()
	{
		pthread_mutex_destroy(&m_posix_mutex);
	}

	void Lock()
	{
		const int errorCode = pthread_mutex_lock(&m_posix_mutex);
		if (pthreadMutexType == eLockType_ERRORCHECK && errorCode != 0)
		{
			// An invalid lock operation was performed.
			// Please refer to PThread documentation about the error code returned.
			__debugbreak();
		}

#ifndef _RELEASE
		++m_LockCount;
#endif
	}

	void Unlock()
	{
#ifndef _RELEASE
		--m_LockCount;
#endif
		const int errorCode = pthread_mutex_unlock(&m_posix_mutex);

		if (pthreadMutexType == eLockType_ERRORCHECK && errorCode != 0)
		{
			// An invalid lock operation was performed.
			// Please refer to PThread documentation about the error code returned.
			__debugbreak();
		}
	}

	bool TryLock()
	{
		const int errorCode = pthread_mutex_trylock(&m_posix_mutex);
		if (errorCode == 0)
		{
#ifndef _RELEASE
			++m_LockCount;
#endif
			return true;
		}

		if (pthreadMutexType == eLockType_ERRORCHECK && errorCode != EBUSY)
		{
			// An invalid lock operation was performed.
			// Please refer to PThread documentation about the error code returned.
			__debugbreak();
		}
		return false;
	}

	// Deprecated. Value might already be wrong on return
#ifndef _RELEASE
	bool IsLocked() const
	{
		return m_LockCount > 0;
	}
#endif

protected:
	pthread_mutex_t m_posix_mutex;
#ifndef _RELEASE
	volatile uint   m_LockCount = 0;
#endif
};

} // detail
} // CryMT

//////////////////////////////////////////////////////////////////////////
/////////////////////////    DEFINE LOCKS    /////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<> class CryLockT<CRYLOCK_RECURSIVE> : public CryMT::detail::CryLock_Mutex<CryMT::detail::eLockType_RECURSIVE>
{
};
template<> class CryLockT<CRYLOCK_FAST> : public CryMT::detail::CryLock_Mutex<CryMT::detail::eLockType_NORMAL>
{
};

typedef CryMT::detail::CryLock_Mutex<CryMT::detail::eLockType_RECURSIVE> CryMutex;
typedef CryMT::detail::CryLock_Mutex<CryMT::detail::eLockType_NORMAL>    CryMutexFast;        // Not recursive

//////////////////////////////////////////////////////////////////////////
class CryConditionVariable
{
public:
	CryConditionVariable()
	{
		pthread_cond_init(&m_condVar, NULL);
	};

	~CryConditionVariable()
	{
		pthread_cond_destroy(&m_condVar);
	}

	void Wait(CryMutex& lock)
	{
		pthread_cond_wait(&m_condVar, &lock.m_posix_mutex);
	}

	void Wait(CryMutexFast& lock)
	{
		pthread_cond_wait(&m_condVar, &lock.m_posix_mutex);
	}

	bool TimedWait(CryMutex& lock, uint32 millis)
	{
		return TimedWait_Internal(lock, millis);
	}

	bool TimedWait(CryMutexFast& lock, uint32 millis)
	{
		return TimedWait_Internal(lock, millis);
	}

	void NotifySingle() { pthread_cond_signal(&m_condVar); }
	void Notify()       { pthread_cond_broadcast(&m_condVar); }

private:
	template<typename T>
	bool TimedWait_Internal(T& lock, uint32 millis)
	{
		struct timeval now;
		struct timespec timeout;
		int err;

		gettimeofday(&now, NULL);
		while (true)
		{
			timeout.tv_sec = now.tv_sec + millis / 1000;
			uint64 nsec = (uint64)now.tv_usec * 1000 + (uint64)millis * 1000000;
			if (nsec >= 1000000000)
			{
				timeout.tv_sec += (long)(nsec / 1000000000);
				nsec %= 1000000000;
			}
			timeout.tv_nsec = (long)nsec;
			err = pthread_cond_timedwait(&m_condVar, &lock.m_posix_mutex, &timeout);
			if (err == EINTR)
			{
				// Interrupted by a signal.
				continue;
			}
			else if (err == ETIMEDOUT)
			{
				return false;
			}
			else
				assert(err == 0);
			break;
		}
		return true;
	}

private:
	CryConditionVariable(const CryConditionVariable&);
	CryConditionVariable& operator=(const CryConditionVariable&);

private:
	pthread_cond_t m_condVar;
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
	sem_t m_Semaphore;
};

//////////////////////////////////////////////////////////////////////////
inline CrySemaphore::CrySemaphore(int nMaximumCount, int nInitialCount)
{
	sem_init(&m_Semaphore, 0, nInitialCount);
}

//////////////////////////////////////////////////////////////////////////
inline CrySemaphore::~CrySemaphore()
{
	sem_destroy(&m_Semaphore);
}

//////////////////////////////////////////////////////////////////////////
inline void CrySemaphore::Acquire()
{
	sem_wait(&m_Semaphore);
}

//////////////////////////////////////////////////////////////////////////
inline void CrySemaphore::Release()
{
	sem_post(&m_Semaphore);
}

//////////////////////////////////////////////////////////////////////////
//! Platform independet wrapper for a counting semaphore,
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
inline CryFastSemaphore::CryFastSemaphore(int nMaximumCount, int nInitialCount) :
	m_Semaphore(nMaximumCount),
	m_nCounter(nInitialCount)
{
}

//////////////////////////////////////////////////////////////////////////
inline CryFastSemaphore::~CryFastSemaphore()
{
}

/////////////////////////////////////////////////////////////////////////
inline void CryFastSemaphore::Acquire()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount - 1, nCount) != nCount);

	// if the count would have been 0 or below, go to kernel semaphore
	if ((nCount - 1) < 0)
		m_Semaphore.Acquire();
}

//////////////////////////////////////////////////////////////////////////
inline void CryFastSemaphore::Release()
{
	int nCount = ~0;
	do
	{
		nCount = *const_cast<volatile int*>(&m_nCounter);

	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&m_nCounter), nCount + 1, nCount) != nCount);

	// wake up kernel semaphore if we have waiter
	if (nCount < 0)
		m_Semaphore.Release();
}

//////////////////////////////////////////////////////////////////////////
class CryRWLock
{
	pthread_rwlock_t m_Lock;

	CryRWLock(const CryRWLock&);
	CryRWLock& operator=(const CryRWLock&);

public:
	CryRWLock() { pthread_rwlock_init(&m_Lock, NULL); }
	~CryRWLock() { pthread_rwlock_destroy(&m_Lock); }
	void RLock() { pthread_rwlock_rdlock(&m_Lock); }
	bool TryRLock()
	{
		return pthread_rwlock_tryrdlock(&m_Lock) != EBUSY;
	}
	void RUnlock() { Unlock(); }
	void WLock()   { pthread_rwlock_wrlock(&m_Lock); }
	bool TryWLock()
	{
		return pthread_rwlock_trywrlock(&m_Lock) != EBUSY;
	}
	void WUnlock() { Unlock(); }
	void Lock()    { WLock(); }
	bool TryLock() { return TryWLock(); }
	void Unlock()  { pthread_rwlock_unlock(&m_Lock); }
};

//! Provide TLS implementation using pthreads for those platforms without thread.
struct SCryPthreadTLSBase
{
	SCryPthreadTLSBase(void(*pDestructor)(void*))
	{
		pthread_key_create(&m_kKey, pDestructor);
	}

	~SCryPthreadTLSBase()
	{
		pthread_key_delete(m_kKey);
	}

	void* GetSpecific()
	{
		return pthread_getspecific(m_kKey);
	}

	void SetSpecific(const void* pValue)
	{
		pthread_setspecific(m_kKey, pValue);
	}

	pthread_key_t m_kKey;
};

template<typename T, bool bDirect> struct SCryPthreadTLSImpl {};

template<typename T> struct SCryPthreadTLSImpl<T, true> : private SCryPthreadTLSBase
{
	SCryPthreadTLSImpl()
		: SCryPthreadTLSBase(NULL)
	{
	}

	T Get()
	{
		void* pSpecific(GetSpecific());
		return *reinterpret_cast<const T*>(&pSpecific);
	}

	void Set(const T& kValue)
	{
		SetSpecific(*reinterpret_cast<const void* const*>(&kValue));
	}
};

template<typename T> struct SCryPthreadTLSImpl<T, false> : private SCryPthreadTLSBase
{
	SCryPthreadTLSImpl()
		: SCryPthreadTLSBase(&Destroy)
	{
	}

	T* GetPtr()
	{
		T* pPtr(static_cast<T*>(GetSpecific()));
		if (pPtr == NULL)
		{
			pPtr = new T();
			SetSpecific(pPtr);
		}
		return pPtr;
	}

	static void Destroy(void* pPointer)
	{
		delete static_cast<T*>(pPointer);
	}

	const T& Get()
	{
		return *GetPtr();
	}

	void Set(const T& kValue)
	{
		*GetPtr() = kValue;
	}
};

template<typename T>
struct SCryPthreadTLS : SCryPthreadTLSImpl<T, sizeof(T) <= sizeof(void*)>
{
};

//! CryEvent(Timed) represent a synchronization event.
class CryEventTimed
{
public:
	CryEventTimed() { m_flag = false; }
	~CryEventTimed() {}

	//! Reset the event to the unsignaled state.
	void Reset();

	//! Set the event to the signaled state.
	void Set();

	//! Access a HANDLE to wait on.
	void* GetHandle() const { return NULL; };

	//! Wait indefinitely for the object to become signalled.
	void Wait();

	//! Wait, with a time limit, for the object to become signalled.
	bool Wait(const uint32 timeoutMillis);

private:
	//! Lock for synchronization of notifications.
	CryCriticalSection   m_lockNotify;
	CryConditionVariable m_cond;
	volatile bool        m_flag;
};

typedef CryEventTimed CryEvent;

///////////////////////////////////////////////////////////////////////////////
namespace CryMT {

//////////////////////////////////////////////////////////////////////////
inline void CryMemoryBarrier()
{
	MemoryBarrier();
}

//////////////////////////////////////////////////////////////////////////
inline void CryYieldThread()
{
	sched_yield();
}

} //CryMT
