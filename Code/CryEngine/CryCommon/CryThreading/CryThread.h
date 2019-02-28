// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Include basic multithread primitives.
#include "CryAtomics.h"
#include <CryCore/BitFiddling.h>

class CryConditionVariable;
class CrySemaphore;
class CryFastSemaphore;
class CryRWLock;

#define THREAD_NAME_LENGTH_MAX 64

enum CryLockType
{
	CRYLOCK_FAST      = 1,  //!< A fast potentially (non-recursive) mutex.
	CRYLOCK_RECURSIVE = 2,  //!< A recursive mutex.
};

//! Primitive locks and conditions.
//! Primitive locks are represented by instance of class CryLockT<Type>.
template<CryLockType Type> class CryLockT
{
	/* Unsupported lock type. */
};

//////////////////////////////////////////////////////////////////////////
// Typedefs.
//////////////////////////////////////////////////////////////////////////
typedef CryLockT<CRYLOCK_RECURSIVE> CryCriticalSection;
typedef CryLockT<CRYLOCK_FAST>      CryCriticalSectionNonRecursive;

//////////////////////////////////////////////////////////////////////////
//! CryAutoCriticalSection implements a helper class to automatically.
//! lock critical section in constructor and release on destructor.
template<class LockClass> class CryAutoLock
{
public:
	CryAutoLock() = delete;
	CryAutoLock(const CryAutoLock<LockClass>&) = delete;
	CryAutoLock<LockClass>& operator=(const CryAutoLock<LockClass>&) = delete;

	CryAutoLock(LockClass& Lock) : m_pLock(&Lock) { m_pLock->Lock(); }
	CryAutoLock(const LockClass& Lock) : m_pLock(const_cast<LockClass*>(&Lock)) { m_pLock->Lock(); }
	~CryAutoLock() { m_pLock->Unlock(); }
private:
	LockClass* m_pLock;
};

template<class LockClass> class CryAutoReadLock
{
public:
	CryAutoReadLock() = delete;
	CryAutoReadLock(const CryAutoReadLock<LockClass>&) = delete;
	CryAutoReadLock<LockClass>& operator=(const CryAutoReadLock<LockClass>&) = delete;

	CryAutoReadLock(LockClass& Lock) : m_pLock(&Lock) { m_pLock->RLock(); }
	CryAutoReadLock(const LockClass& Lock) : m_pLock(const_cast<LockClass*>(&Lock)) { m_pLock->RLock(); }
	~CryAutoReadLock() { m_pLock->RUnlock(); }
private:
	LockClass* m_pLock;
};

template<class LockClass> class CryAutoWriteLock
{
public:
	CryAutoWriteLock() = delete;
	CryAutoWriteLock(const CryAutoWriteLock<LockClass>&) = delete;
	CryAutoWriteLock<LockClass>& operator=(const CryAutoWriteLock<LockClass>&) = delete;

	CryAutoWriteLock(LockClass& Lock) : m_pLock(&Lock) { m_pLock->WLock(); }
	CryAutoWriteLock(const LockClass& Lock) : m_pLock(const_cast<LockClass*>(&Lock)) { m_pLock->WLock(); }
	~CryAutoWriteLock() { m_pLock->WUnlock(); }
private:
	LockClass* m_pLock;
};


//! CryOptionalAutoLock implements a helper class to automatically.
//! Lock critical section (if needed) in constructor and release on destructor.
template<class LockClass> class CryOptionalAutoLock
{
private:
	LockClass* m_Lock;
	bool       m_bLockAcquired;

	CryOptionalAutoLock();
	CryOptionalAutoLock(const CryOptionalAutoLock<LockClass>&);
	CryOptionalAutoLock<LockClass>& operator=(const CryOptionalAutoLock<LockClass>&);

public:
	CryOptionalAutoLock(LockClass& Lock, bool acquireLock) : m_Lock(&Lock), m_bLockAcquired(false)
	{
		if (acquireLock)
		{
			Acquire();
		}
	}
	~CryOptionalAutoLock()
	{
		Release();
	}
	void Release()
	{
		if (m_bLockAcquired)
		{
			m_Lock->Unlock();
			m_bLockAcquired = false;
		}
	}
	void Acquire()
	{
		if (!m_bLockAcquired)
		{
			m_Lock->Lock();
			m_bLockAcquired = true;
		}
	}
};

//! CryAutoSet implements a helper class to automatically.
//! set and reset value in constructor and release on destructor.
template<class ValueClass> class CryAutoSet
{
private:
	ValueClass* m_pValue;

	CryAutoSet();
	CryAutoSet(const CryAutoSet<ValueClass>&);
	CryAutoSet<ValueClass>& operator=(const CryAutoSet<ValueClass>&);

public:
	CryAutoSet(ValueClass& value) : m_pValue(&value) { *m_pValue = (ValueClass)1; }
	~CryAutoSet() { *m_pValue = (ValueClass)0; }
};

//! Auto critical section is the most commonly used type of auto lock.
typedef CryAutoLock<CryCriticalSection>             CryAutoCriticalSection;
typedef CryAutoLock<CryCriticalSectionNonRecursive> CryAutoCriticalSectionNoRecursive;

#define AUTO_LOCK_T(Type, lock) PREFAST_SUPPRESS_WARNING(6246); CryAutoLock<Type> __AutoLock(lock)
#define AUTO_LOCK(lock)         AUTO_LOCK_T(CryCriticalSection, lock)
#define AUTO_LOCK_CS(csLock)    CryAutoCriticalSection __AL__ ## csLock(csLock)

///////////////////////////////////////////////////////////////////////////////
//! Base class for lockless Producer/Consumer queue, due platforms specific they are implemented in CryThead_platform.h.
namespace CryMT {
namespace detail {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SingleProducerSingleConsumerQueueBase
{
public:
	SingleProducerSingleConsumerQueueBase()
	{}

	void Push(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, uint32 nBufferSize, void* arrBuffer, uint32 nObjectSize);
	void Pop(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, uint32 nBufferSize, void* arrBuffer, uint32 nObjectSize);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class N_ProducerSingleConsumerQueueBase
{
public:
	N_ProducerSingleConsumerQueueBase()
	{
		CryInitializeSListHead(fallbackList);
	}

	void Push(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, volatile uint32& rRunning, void* arrBuffer, uint32 nBufferSize, uint32 nObjectSize, volatile uint32* arrStates);
	bool Pop(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, volatile uint32& rRunning, void* arrBuffer, uint32 nBufferSize, uint32 nObjectSize, volatile uint32* arrStates);

private:
	SLockFreeSingleLinkedListHeader fallbackList;
	struct SFallbackList
	{
		SLockFreeSingleLinkedListEntry nextEntry;
		char                           alignment_padding[128 - sizeof(SLockFreeSingleLinkedListEntry)];
		char                           object[1];         //!< Struct will be overallocated with enough memory for the object
	};
};

} // namespace detail
} // namespace CryMT

//////////////////////////////////////////////////////////////////////////
namespace CryMT {
	void CryMemoryBarrier();
	void CryYieldThread();
} // namespace CryMT

// Include architecture specific code.
#if CRY_PLATFORM_WINAPI
	#include <CryThreading/CryThread_win32.h>
#elif CRY_PLATFORM_POSIX
	#include <CryThreading/CryThread_posix.h>
#else
// Put other platform specific includes here!
	#include <CryThreading/CryThread_dummy.h>
#endif

//! Sync primitive for multiple reads and exclusive locking change access.
//! Useful in case if you have rarely modified object that needs
//! to be read quite often from different threads but still
//! need to be exclusively modified sometimes.
//! Debug functionality:.
//! Can be used for debug-only lock calls, which verify that no
//! simultaneous access is attempted.
//! Use the bDebug argument of LockRead or LockModify,
//! or use the DEBUG_READLOCK or DEBUG_MODIFYLOCK macros.
//! There is no overhead in release builds, if you use the macros,
//! and the lock definition is inside #ifdef _DEBUG.
class CryReadModifyLock
{
public:
	CryReadModifyLock()
		: m_modifyCount(0), m_readCount(0)
	{
		SetDebugLocked(false);
	}

	bool LockRead(bool bTry = false, cstr strDebug = 0, bool bDebug = false) const
	{
		if (!WriteLock(bTry, bDebug, strDebug))     // wait until write unlocked
			return false;
		CryInterlockedIncrement(&m_readCount);      // increment read counter
		m_writeLock.Unlock();
		return true;
	}
	void UnlockRead() const
	{
		SetDebugLocked(false);
		const int counter = CryInterlockedDecrement(&m_readCount);     // release read
		assert(counter >= 0);
		if (m_writeLock.TryLock())
			m_writeLock.Unlock();
		else if (counter == 0 && m_modifyCount)
			m_ReadReleased.Set();                     // signal the final read released
	}
	bool LockModify(bool bTry = false, cstr strDebug = 0, bool bDebug = false) const
	{
		if (!WriteLock(bTry, bDebug, strDebug))
			return false;
		CryInterlockedIncrement(&m_modifyCount);    // increment write counter (counter is for nested cases)
		while (m_readCount)
			m_ReadReleased.Wait();                    // wait for all threads finish read operation
		return true;
	}
	void UnlockModify() const
	{
		SetDebugLocked(false);
		int counter = CryInterlockedDecrement(&m_modifyCount);    // decrement write counter
		assert(counter >= 0);
		m_writeLock.Unlock();                       // release exclusive lock
	}

protected:
	mutable volatile int       m_readCount;
	mutable volatile int       m_modifyCount;
	mutable CryEvent           m_ReadReleased;
	mutable CryCriticalSection m_writeLock;
	mutable bool               m_debugLocked;
	mutable const char*        m_debugLockStr;

	void SetDebugLocked(bool b, const char* str = 0) const
	{
#ifdef _DEBUG
		m_debugLocked = b;
		m_debugLockStr = str;
#endif
	}

	bool WriteLock(bool bTry, bool bDebug, const char* strDebug) const
	{
		if (!m_writeLock.TryLock())
		{
#ifdef _DEBUG
			assert(!m_debugLocked);
			assert(!bDebug);
#endif
			if (bTry)
				return false;
			m_writeLock.Lock();
		}
#ifdef _DEBUG
		if (!m_readCount && !m_modifyCount)         // not yet locked
			SetDebugLocked(bDebug, strDebug);
#endif
		return true;
	}
};

//! Auto-locking classes.
template<class T, bool bDEBUG = false>
class AutoLockRead
{
protected:
	const T& m_lock;
public:
	AutoLockRead(const T& lock, cstr strDebug = 0)
		: m_lock(lock) { m_lock.LockRead(bDEBUG, strDebug, bDEBUG); }
	~AutoLockRead()
	{ m_lock.UnlockRead(); }
};

template<class T, bool bDEBUG = false>
class AutoLockModify
{
protected:
	const T& m_lock;
public:
	AutoLockModify(const T& lock, cstr strDebug = 0)
		: m_lock(lock) { m_lock.LockModify(bDEBUG, strDebug, bDEBUG); }
	~AutoLockModify()
	{ m_lock.UnlockModify(); }
};

#define AUTO_READLOCK(p)      PREFAST_SUPPRESS_WARNING(6246) AutoLockRead<CryReadModifyLock> __readlock ## __LINE__(p, __FUNC__)
#define AUTO_READLOCK_PROT(p) PREFAST_SUPPRESS_WARNING(6246) AutoLockRead<CryReadModifyLock> __readlock_prot ## __LINE__(p, __FUNC__)
#define AUTO_MODIFYLOCK(p)    PREFAST_SUPPRESS_WARNING(6246) AutoLockModify<CryReadModifyLock> __modifylock ## __LINE__(p, __FUNC__)

#if defined(_DEBUG)
	#define DEBUG_READLOCK(p)   AutoLockRead<CryReadModifyLock> __readlock ## __LINE__(p, __FUNC__)
	#define DEBUG_MODIFYLOCK(p) AutoLockModify<CryReadModifyLock> __modifylock ## __LINE__(p, __FUNC__)
#else
	#define DEBUG_READLOCK(p)
	#define DEBUG_MODIFYLOCK(p)
#endif

///////////////////////////////////////////////////////////////////////////////
//! Base class for lockless Producer/Consumer queue, due platforms specific they are implemented in CryThead_platform.h.
namespace CryMT {
namespace detail {

///////////////////////////////////////////////////////////////////////////////
inline void SingleProducerSingleConsumerQueueBase::Push(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, uint32 nBufferSize, void* arrBuffer, uint32 nObjectSize)
{
	// spin if queue is full
	CSimpleThreadBackOff backoff;
	while (rProducerIndex - rConsumerIndex == nBufferSize)
	{
		backoff.backoff();
	}

	CryMT::CryMemoryBarrier();
	char* pBuffer = alias_cast<char*>(arrBuffer);
	uint32 nIndex = rProducerIndex % nBufferSize;

	memcpy(pBuffer + (nIndex * nObjectSize), pObj, nObjectSize);
	CryMT::CryMemoryBarrier();
	rProducerIndex += 1;
	CryMT::CryMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  void SingleProducerSingleConsumerQueueBase::Pop(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, uint32 nBufferSize, void* arrBuffer, uint32 nObjectSize)
{
	CryMT::CryMemoryBarrier();
	// busy-loop if queue is empty
	CSimpleThreadBackOff backoff;
	while (rProducerIndex - rConsumerIndex == 0)
	{
		backoff.backoff();
	}

	char* pBuffer = alias_cast<char*>(arrBuffer);
	uint32 nIndex = rConsumerIndex % nBufferSize;

	memcpy(pObj, pBuffer + (nIndex * nObjectSize), nObjectSize);
	CryMT::CryMemoryBarrier();
	rConsumerIndex += 1;
	CryMT::CryMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  void N_ProducerSingleConsumerQueueBase::Push(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, volatile uint32& rRunning, void* arrBuffer, uint32 nBufferSize, uint32 nObjectSize, volatile uint32* arrStates)
{
	CryMT::CryMemoryBarrier();
	uint32 nProducerIndex;
	uint32 nConsumerIndex;

	int iter = 0;
	CSimpleThreadBackOff backoff;
	do
	{
		nProducerIndex = rProducerIndex;
		nConsumerIndex = rConsumerIndex;

		if (nProducerIndex - nConsumerIndex == nBufferSize)
		{
			if (iter++ > CSimpleThreadBackOff::kHardYieldInterval)
			{
				uint32 nSizeToAlloc = sizeof(SFallbackList) + nObjectSize - 1;
				SFallbackList* pFallbackEntry = (SFallbackList*)CryModuleMemalign(nSizeToAlloc, 128);
				memcpy(pFallbackEntry->object, pObj, nObjectSize);
				CryMT::CryMemoryBarrier();
				CryInterlockedPushEntrySList(fallbackList, pFallbackEntry->nextEntry);
				return;
			}
			backoff.backoff();
			continue;
		}

		if (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&rProducerIndex), nProducerIndex + 1, nProducerIndex) == nProducerIndex)
			break;
	}
	while (true);

	CryMT::CryMemoryBarrier();
	char* pBuffer = alias_cast<char*>(arrBuffer);
	uint32 nIndex = nProducerIndex % nBufferSize;

	memcpy(pBuffer + (nIndex * nObjectSize), pObj, nObjectSize);
	CryMT::CryMemoryBarrier();
	arrStates[nIndex] = 1;
	CryMT::CryMemoryBarrier();
}

///////////////////////////////////////////////////////////////////////////////
inline  bool N_ProducerSingleConsumerQueueBase::Pop(void* pObj, volatile uint32& rProducerIndex, volatile uint32& rConsumerIndex, volatile uint32& rRunning, void* arrBuffer, uint32 nBufferSize, uint32 nObjectSize, volatile uint32* arrStates)
{
	CryMT::CryMemoryBarrier();

	// busy-loop if queue is empty
	CSimpleThreadBackOff backoff;
	if (rRunning && rProducerIndex - rConsumerIndex == 0)
	{
		while (rRunning && rProducerIndex - rConsumerIndex == 0)
		{
			backoff.backoff();
		}
	}

	if (rRunning == 0 && rProducerIndex - rConsumerIndex == 0)
	{
		SFallbackList* pFallback = (SFallbackList*)CryInterlockedPopEntrySList(fallbackList);
		IF (pFallback, 0)
		{
			memcpy(pObj, pFallback->object, nObjectSize);
			CryModuleMemalignFree(pFallback);
			return true;
		}
		// if the queue was empty, make sure we really are empty
		return false;
	}

	backoff.reset();
	while (arrStates[rConsumerIndex % nBufferSize] == 0)
	{
		backoff.backoff();
	}

	char* pBuffer = alias_cast<char*>(arrBuffer);
	uint32 nIndex = rConsumerIndex % nBufferSize;

	memcpy(pObj, pBuffer + (nIndex * nObjectSize), nObjectSize);
	CryMT::CryMemoryBarrier();
	arrStates[nIndex] = 0;
	CryMT::CryMemoryBarrier();
	rConsumerIndex += 1;
	CryMT::CryMemoryBarrier();

	return true;
}

} // namespace detail
} // namespace CryMT

// Include all multithreading containers.
#include "MultiThread_Containers.h"
