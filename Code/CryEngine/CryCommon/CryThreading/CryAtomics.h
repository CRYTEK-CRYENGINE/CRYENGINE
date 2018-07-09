// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// Interlocked API
//////////////////////////////////////////////////////////////////////////

// Returns the resulting incremented value
LONG CryInterlockedIncrement(int volatile* pDst);

// Returns the resulting decremented value
LONG CryInterlockedDecrement(int volatile* pDst);

// Returns the resulting added value
LONG CryInterlockedAdd(volatile LONG* pVal, LONG add);

// Returns the resulting added value
int64 CryInterlockedAdd(volatile int64* pVal, int64 add);

// Returns the resulting added value
size_t CryInterlockedAdd(volatile size_t* pVal, size_t add);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG CryInterlockedExchange(volatile LONG* pDst, LONG exchange);

// Returns initial value prior exchange
int64 CryInterlockedExchange64(volatile int64* addr, int64 exchange);

// Returns initial value prior exchange
LONG CryInterlockedExchangeAdd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
size_t CryInterlockedExchangeAdd(volatile size_t* pDst, size_t value);

// Returns initial value prior exchange
LONG CryInterlockedExchangeAnd(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
LONG CryInterlockedExchangeOr(volatile LONG* pDst, LONG value);

// Returns initial value prior exchange
void* CryInterlockedExchangePointer(void* volatile* pDst, void* pExchange);

//////////////////////////////////////////////////////////////////////////
// Returns initial value prior exchange
LONG CryInterlockedCompareExchange(volatile LONG* pDst, LONG exchange, LONG comperand);

// Returns initial value prior exchange
int64 CryInterlockedCompareExchange64(volatile int64* pDst, int64 exchange, int64 comperand);

#if CRY_PLATFORM_64BIT
// Returns initial value prior exchange
unsigned char CryInterlockedCompareExchange128(volatile int64* pDst, int64 exchangeHigh, int64 exchangeLow, int64* comparandResult);
#endif

// Returns initial address prior exchange
void* CryInterlockedCompareExchangePointer(void* volatile* pDst, void* pExchange, void* pComperand);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// CryInterlocked*SList Function, these are specialized C-A-S
// functions for single-linked lists which prevent the A-B-A problem there
// there are implemented in the platform specific CryThread_*.h files
//NOTE: The sizes are verified at compile-time in the implementation functions, but this is still ugly

#if CRY_PLATFORM_64BIT
	#define LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN 16
#elif CRY_PLATFORM_32BIT
	#define LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN 8
#else
	#error "Unsupported plaform"
#endif

struct SLockFreeSingleLinkedListEntry
{
	CRY_ALIGN(LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN) SLockFreeSingleLinkedListEntry * volatile pNext;
};
static_assert(std::alignment_of<SLockFreeSingleLinkedListEntry>::value == sizeof(uintptr_t) * 2, "Alignment failure for SLockFreeSingleLinkedListEntry");

struct SLockFreeSingleLinkedListHeader
{
	//! Initializes the single-linked list.
	friend void CryInitializeSListHead(SLockFreeSingleLinkedListHeader& list);

	//! Push one element atomically onto the front of a single-linked list.
	friend void CryInterlockedPushEntrySList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& element);

	//! Push a list of elements atomically onto the front of a single-linked list.
	//! \note The entries must already be linked (ie, last must be reachable by moving through pNext starting at first).
	friend void CryInterlockedPushListSList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& first, SLockFreeSingleLinkedListEntry& last, uint32 count);

	//! Retrieves a pointer to the first item on a single-linked list.
	//! \note This does not remove the item from the list, and it's unsafe to inspect anything but the returned address.
	friend void* CryRtlFirstEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Pops one element atomically from the front of a single-linked list, and returns a pointer to the item.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* CryInterlockedPopEntrySList(SLockFreeSingleLinkedListHeader& list);

	//! Flushes the entire single-linked list, and returns a pointer to the first item that was on the list.
	//! \note If the list was empty, nullptr is returned instead.
	friend void* CryInterlockedFlushSList(SLockFreeSingleLinkedListHeader& list);

private:
	CRY_ALIGN(LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN) SLockFreeSingleLinkedListEntry * volatile pNext;

#if CRY_PLATFORM_ORBIS
	// Only need "salt" on platforms using CAS (ORBIS uses embedded salt)
#elif CRY_PLATFORM_POSIX
	// If pointers 32bit, salt should be as well. Otherwise we get 4 bytes of padding between pNext and salt and CAS operations fail
	#if CRY_PLATFORM_64BIT
	volatile uint64 salt;
	#else
	volatile uint32 salt;
	#endif
#endif
};
static_assert(std::alignment_of<SLockFreeSingleLinkedListHeader>::value == sizeof(uintptr_t) * 2, "Alignment failure for SLockFreeSingleLinkedListHeader");
#undef LOCK_FREE_LINKED_LIST_DOUBLE_SIZE_PTR_ALIGN

#if CRY_PLATFORM_WINAPI
	#include "CryAtomics_win32.h"
#elif CRY_PLATFORM_ORBIS
	#include "CryAtomics_sce.h"
#elif CRY_PLATFORM_POSIX
	#include "CryAtomics_posix.h"
#endif

#define WRITE_LOCK_VAL (1 << 16)

void*      CryCreateCriticalSection();
void       CryCreateCriticalSectionInplace(void*);
void       CryDeleteCriticalSection(void* cs);
void       CryDeleteCriticalSectionInplace(void* cs);
void       CryEnterCriticalSection(void* cs);
bool       CryTryCriticalSection(void* cs);
void       CryLeaveCriticalSection(void* cs);

ILINE void CrySpinLock(volatile int* pLock, int checkVal, int setVal)
{
	static_assert(sizeof(int) == sizeof(LONG), "Unsecured cast. Int is not same size as LONG.");
	CSimpleThreadBackOff threadBackoff;
	while (CryInterlockedCompareExchange((volatile LONG*)pLock, setVal, checkVal) != checkVal)
	{
		threadBackoff.backoff();
	}
}

ILINE void CryReleaseSpinLock(volatile int* pLock, int setVal)
{
	*pLock = setVal;
}

ILINE void CryReadLock(volatile int* rw)
{
	CryInterlockedAdd(rw, 1);
#ifdef NEED_ENDIAN_SWAP
	volatile char* pw = (volatile char*)rw + 1;
#else
	volatile char* pw = (volatile char*)rw + 2;
#endif

	CSimpleThreadBackOff backoff;
	for (; *pw; )
	{
		backoff.backoff();
	}
}

ILINE void CryReleaseReadLock(volatile int* rw)
{
	CryInterlockedAdd(rw, -1);
}

ILINE void CryWriteLock(volatile int* rw)
{
	CrySpinLock(rw, 0, WRITE_LOCK_VAL);
}

ILINE void CryReleaseWriteLock(volatile int* rw)
{
	CryInterlockedAdd(rw, -WRITE_LOCK_VAL);
}

//////////////////////////////////////////////////////////////////////////
struct ReadLock
{
	ReadLock(volatile int& rw) : prw(&rw)
	{
		CryReadLock(prw);
	}

	~ReadLock()
	{
		CryReleaseReadLock(prw);
	}

private:
	volatile int* const prw;
};

struct ReadLockCond
{
	ReadLockCond(volatile int& rw, int bActive) : iActive(0),prw(&rw)
	{
		if (bActive)
		{
			iActive = 1;
			CryReadLock(prw);
		}
	}

	void SetActive(int bActive = 1)
	{
		iActive = bActive;
	}

	void Release()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	~ReadLockCond()
	{
		CryInterlockedAdd(prw, -iActive);
	}

private:
	int                 iActive;
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLock
{
	WriteLock(volatile int& rw) : prw(&rw)
	{
		CryWriteLock(&rw);
	}

	~WriteLock()
	{
		CryReleaseWriteLock(prw);
	}

private:
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteAfterReadLock
{
	WriteAfterReadLock(volatile int& rw) : prw(&rw)
	{
		CrySpinLock(&rw, 1, WRITE_LOCK_VAL + 1);
	}

	~WriteAfterReadLock()
	{
		CryInterlockedAdd(prw, -WRITE_LOCK_VAL);
	}

private:
	volatile int* const prw;
};

//////////////////////////////////////////////////////////////////////////
struct WriteLockCond
{
	WriteLockCond(volatile int& rw, int bActive = 1) : prw(&rw), iActive(0)
	{
		if (bActive)
		{
			iActive = WRITE_LOCK_VAL;
			CrySpinLock(prw, 0, iActive);
		}
	}

	WriteLockCond() : iActive(0), prw(&iActive) {}

	~WriteLockCond()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	void SetActive(int bActive = 1)
	{
		iActive = -bActive & WRITE_LOCK_VAL;
	}

	void Release()
	{
		CryInterlockedAdd(prw, -iActive);
	}

	int           iActive; //!< Not private because used directly in Physics RWI.
	volatile int* prw;     //!< Not private because used directly in Physics RWI.
};

//////////////////////////////////////////////////////////////////////////
#if defined(EXCLUDE_PHYSICS_THREAD)
ILINE void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	*(int*)pLock = setVal;
}
ILINE void AtomicAdd(volatile int* pVal, int iAdd)                    { *(int*)pVal += iAdd; }
ILINE void AtomicAdd(volatile unsigned int* pVal, int iAdd)           { *(unsigned int*)pVal += iAdd; }

ILINE void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { CrySpinLock(pLock, checkVal, setVal); }
#else
ILINE void SpinLock(volatile int* pLock, int checkVal, int setVal)
{
	CrySpinLock(pLock, checkVal, setVal);
}
ILINE void AtomicAdd(volatile int* pVal, int iAdd)                    { CryInterlockedAdd(pVal, iAdd); }
ILINE void AtomicAdd(volatile unsigned int* pVal, int iAdd)           { CryInterlockedAdd((volatile int*)pVal, iAdd); }

ILINE void JobSpinLock(volatile int* pLock, int checkVal, int setVal) { SpinLock(pLock, checkVal, setVal); }
#endif

ILINE void JobAtomicAdd(volatile int* pVal, int iAdd)
{
	CryInterlockedAdd(pVal, iAdd);
}
ILINE void JobAtomicAdd(volatile unsigned int* pVal, int iAdd) { CryInterlockedAdd((volatile int*)pVal, iAdd); }
