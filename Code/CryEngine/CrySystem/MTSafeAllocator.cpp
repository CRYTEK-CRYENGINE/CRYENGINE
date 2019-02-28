// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MTSafeAllocator.h"
#include <CrySystem/IConsole.h>

extern CMTSafeHeap* g_pPakHeap;

// Uncomment this define to enable time tracing of the MTSAFE heap
#define MTSAFE_PROFILE 1
//#undef MTSAFE_PROFILE

namespace
{
class CSimpleTimer
{
	LARGE_INTEGER& m_result;
	LARGE_INTEGER  m_start;
public:

	CSimpleTimer(LARGE_INTEGER& li)
		: m_result(li)
	{ QueryPerformanceCounter(&m_start); }

	~CSimpleTimer()
	{
		LARGE_INTEGER end;
		QueryPerformanceCounter(&end);
		m_result.QuadPart = end.QuadPart - m_start.QuadPart;
	}
};
};

//////////////////////////////////////////////////////////////////////////
CMTSafeHeap::CMTSafeHeap()
	: m_LiveTempAllocations(),
	m_TotalAllocations(),
	m_TempAllocationsFailed(),
	m_TempAllocationsTime()
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CryPak Heap");

#if MTSAFE_USE_INPLACE_POOL
	if (MTSAFE_TEMPORARY_POOL_SIZE == 0)
	{
		CryFatalError("CMTSafeHeap::CMTSafeHeap(): size for temporary pool is 0");
		std::abort();
	}

	// Allocate the backing storage for the temporary pool
	// Note: Aligned to 128 bytes to minimize straddled cachelines
	uint8* backingStorage = reinterpret_cast<uint8*>(CryModuleMemalign(MTSAFE_TEMPORARY_POOL_SIZE, 128));
	if (!backingStorage)
	{
		CryFatalError("CMTSafeHeap::CMTSafeHeap(): could not allocate %d bytes for temportary pool", MTSAFE_TEMPORARY_POOL_SIZE);
		std::abort();
	}

	// Initialize the actual pool
	m_TemporaryPool.InitMem(MTSAFE_TEMPORARY_POOL_SIZE, backingStorage);
#endif  //  MTSAFE_USE_INPLACE_POOL

#if MTSAFE_USE_BIGPOOL
	ZeroStruct(m_iBigPoolUsed);
	ZeroStruct(m_iBigPoolSize);
	#if CRY_PLATFORM_WINDOWS
	m_iBigPoolSize[0] = 32 * 1024;
	m_iBigPoolSize[1] = 32 * 1024;
	m_iBigPoolSize[2] = 32 * 1024;
	m_iBigPoolSize[3] = 32 * 1024;

	m_iBigPoolSize[4] = 128 * 1024;
	m_iBigPoolSize[5] = 128 * 1024;

	m_iBigPoolSize[6] = 256 * 1024;
	m_iBigPoolSize[7] = 256 * 1024;
	m_iBigPoolSize[8] = 256 * 1024;
	m_iBigPoolSize[9] = 512 * 1024;
	m_iBigPoolSize[10] = 512 * 1024;
	m_iBigPoolSize[11] = 512 * 1024;
	m_iBigPoolSize[12] = 1024 * 1024;
	m_iBigPoolSize[13] = 2048 * 1024;
	m_iBigPoolSize[14] = 5 * 1024 * 1024;
	#else
	m_iBigPoolSize[0] = 33 * 1024;
	m_iBigPoolSize[1] = 33 * 1024;
	m_iBigPoolSize[2] = 33 * 1024;
	m_iBigPoolSize[3] = 33 * 1024;
	m_iBigPoolSize[4] = 65 * 1024;
	m_iBigPoolSize[5] = 129 * 1024;
	m_iBigPoolSize[6] = 257 * 1024;
	m_iBigPoolSize[7] = 257 * 1024;
	m_iBigPoolSize[8] = 560 * 1024;
	#endif // MTSAFE_USE_BIGPOOL

	for (int i = 0; i < NUM_POOLS; ++i)
	{
		m_pBigPool[i] = 0;
		if (m_iBigPoolSize[i])
			m_pBigPool[i] = static_cast<void*>(new char[m_iBigPoolSize[i]]);

		m_iBigPoolUsed[i] = 0;
		m_sBigPoolDescription[i] = "";
		if (!m_pBigPool[i])
		{
			CryFatalError("Failed CMTSafeHeap::m_pBigPool allocation");
		}
	}
#endif  //  MTSAFE_USE_BIGPOOL

#if MTSAFE_USE_GENERAL_HEAP
	m_pGeneralHeapStorage = (char*)malloc(MTSAFE_GENERAL_HEAP_SIZE);
	m_pGeneralHeapStorageEnd = m_pGeneralHeapStorage + MTSAFE_GENERAL_HEAP_SIZE;
	m_pGeneralHeap = CryGetIMemoryManager()->CreateGeneralMemoryHeap(m_pGeneralHeapStorage, MTSAFE_GENERAL_HEAP_SIZE, "MTSafeHeap");
#endif

#if MTSAFE_USE_VIRTUALALLOC
	m_smallHeap = HeapCreate(0, 64 * 1024, 32 * 1024 * 1024);
#endif
}

//////////////////////////////////////////////////////////////////////////
CMTSafeHeap::~CMTSafeHeap()
{
#if MTSAFE_USE_INPLACE_POOL
	if (m_TemporaryPool.Data())
	{
		m_TemporaryPool.InitMem(0, NULL);
		std::free(m_TemporaryPool.Data());
	}
#endif  //  MTSAFE_USE_INPLACE_POOL

#if MTSAFE_USE_BIGPOOL
	for (int i = 0; i < NUM_POOLS; ++i)
		delete[] (uint8*)m_pBigPool[i];
#endif  //  MTSAFE_USE_BIGPOOL

#if MTSAFE_USE_GENERAL_HEAP
	SAFE_RELEASE(m_pGeneralHeap);
	free(m_pGeneralHeapStorage);
#endif

#if MTSAFE_USE_VIRTUALALLOC
	HeapDestroy(m_smallHeap);
#endif
}

//////////////////////////////////////////////////////////////////////////
size_t CMTSafeHeap::PersistentAllocSize(size_t nSize)
{
	return nSize;
}

//////////////////////////////////////////////////////////////////////////
void* CMTSafeHeap::PersistentAlloc(size_t nSize)
{
	return malloc(nSize);
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::FreePersistent(void* p)
{
	free(p);
}

//////////////////////////////////////////////////////////////////////////
void* CMTSafeHeap::TempAlloc(size_t nSize, const char* szDbgSource, bool& bFallBackToMalloc, uint32 align)
{
	// provide hint to mem replay on debug source
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, szDbgSource);

#if MTSAFE_PROFILE
	CSimpleTimer timer(m_TempAllocationsTime);
#endif

#if MTSAFE_USE_INPLACE_POOL
	if (!(nSize >= MTSAFE_TEMPORARY_POOL_MINALLOC && nSize <= MTSAFE_TEMPORARY_POOL_MAXALLOC))
	{
	#if MTSAFE_USE_VIRTUALALLOC
		return (char*) TempVirtualAlloc(nSize);
	#else
		return (char*) malloc(nSize);
	#endif
	}
#endif

#if MTSAFE_USE_INPLACE_POOL
	uint16 spinCount = 32;
	while (true)
	{
		if (m_LockTemporary.TryLock())
		{
			void* p = m_TemporaryPool.Allocate<void*>(nSize, MTSAFE_DEFAULT_ALIGNMENT);
			m_LockTemporary.Unlock();
			if (p)
				return p;
		}
		if (--spinCount == 0)
			break;
		YieldProcessor();
	}
#endif  // MTSAFE_USE_INPLACE_POOL

#if MTSAFE_USE_BIGPOOL
	for (int i = 0; i < NUM_POOLS; ++i)
	{
		if (!m_iBigPoolUsed[i] && (int)nSize <= m_iBigPoolSize[i])
		{
			m_sBigPoolDescription[i] = szDbgSource;
			if (CryInterlockedCompareExchange(&m_iBigPoolUsed[i], 1, 0) == 0)
			{
				return m_pBigPool[i];
			}
		}
	}
	//CryWarning(VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"CMTSafeHeap::m_pBigPool is already used. %s uses malloc instead", szDbgSource );
#endif  // MTSAFE_USE_BIGPOOL

#if MTSAFE_USE_GENERAL_HEAP
	void* ptr = NULL;
	if (align)
	{
		ptr = m_pGeneralHeap->Memalign(align, nSize, szDbgSource);
	}
	else
	{
		ptr = m_pGeneralHeap->Malloc(nSize, szDbgSource);
	}

	//explicit alignment not supported beyond this point, safer to return NULL
	if (ptr || !bFallBackToMalloc)
	{
		bFallBackToMalloc = false;
		return ptr;
	}
#endif

	bFallBackToMalloc = true;

#if MTSAFE_PROFILE
	CryInterlockedAdd((volatile int*)&m_TempAllocationsFailed, (int)nSize);
#endif
	{
		// provide hint to mem replay on debug source
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, szDbgSource);

#if MTSAFE_USE_VIRTUALALLOC
		return TempVirtualAlloc(nSize);
#else
		return CryModuleMemalign(nSize, align > 0 ? align : 4);
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::FreeTemporary(void* p)
{
#if MTSAFE_PROFILE
	CSimpleTimer timer(m_TempAllocationsTime);
#endif

#if MTSAFE_USE_INPLACE_POOL
	if (p >= m_TemporaryPool.Data() && p <= (uint8*)m_TemporaryPool.Data() + MTSAFE_TEMPORARY_POOL_SIZE)
	{
		AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_LockTemporary);
		m_TemporaryPool.Free(p);
		return;
	}
#endif  // MTSAFE_USE_INPLACE_POOL

#if MTSAFE_USE_BIGPOOL
	for (int i = 0; i < NUM_POOLS; ++i)
	{
		if (p == m_pBigPool[i])
		{
			m_sBigPoolDescription[i] = "";
			m_iBigPoolUsed[i] = 0;
			return;
		}
	}
#endif  // MTSAFE_USE_BIGPOOL

#if MTSAFE_USE_GENERAL_HEAP
	if (m_pGeneralHeap->IsInAddressRange(p))
	{
		m_pGeneralHeap->Free(p);
		return;
	}
#endif

#if MTSAFE_USE_VIRTUALALLOC
	TempVirtualFree(p);
#else
	// Fallback to free
	CryModuleMemalignFree(p);
#endif
}

//////////////////////////////////////////////////////////////////////////
void* CMTSafeHeap::StaticAlloc(void* pOpaque, unsigned nItems, unsigned nSize)
{
	return g_pPakHeap->TempAlloc(nItems * nSize, "StaticAlloc");
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::StaticFree(void* pOpaque, void* pAddress)
{
	g_pPakHeap->FreeTemporary(pAddress);
}

//////////////////////////////////////////////////////////////////////////
void CMTSafeHeap::GetMemoryUsage(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "FileSystem Pool");

#if MTSAFE_USE_INPLACE_POOL
	pSizer->AddObject(m_TemporaryPool.Data(), MTSAFE_TEMPORARY_POOL_SIZE);
#endif  // MTSAFE_USE_INPLACE_POOL

#if MTSAFE_USE_BIGPOOL
	for (int i = 0; i < NUM_POOLS; ++i)
	{
		if (m_pBigPool[i])
		{
			pSizer->AddObject(m_pBigPool[i], m_iBigPoolSize[i]);
		}
	}
#endif  // MTSAFE_USE_BIGPOOL
}

void CMTSafeHeap::PrintStats()
{
#if MTSAFE_PROFILE
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	const double rFreq = 1. / static_cast<double>(freq.QuadPart);

	CryLogAlways("mtsafe temporary pool failed for %" PRISIZE_T " bytes, time spent in allocations %3.08f seconds",
	             m_TempAllocationsFailed, static_cast<double>(m_TempAllocationsTime.QuadPart) * rFreq);
#endif

}

#if MTSAFE_USE_VIRTUALALLOC
void* CMTSafeHeap::TempVirtualAlloc(size_t nSize)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

	char* ptr = NULL;

	static_assert(sizeof(bool) <= MTSAFE_DEFAULT_ALIGNMENT, "Invalid type size!");

	if ((nSize + MTSAFE_DEFAULT_ALIGNMENT) >= 4096)
	{
		ptr = (char*) VirtualAlloc(NULL, nSize + MTSAFE_DEFAULT_ALIGNMENT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (ptr)
		{
			ptr += MTSAFE_DEFAULT_ALIGNMENT;
			reinterpret_cast<bool*>(ptr)[-1] = IsVirtualAlloced;
		}
	}
	else
	{
		ptr = (char*) HeapAlloc(m_smallHeap, 0, nSize + MTSAFE_DEFAULT_ALIGNMENT);
		if (ptr)
		{
			ptr += MTSAFE_DEFAULT_ALIGNMENT;
			reinterpret_cast<bool*>(ptr)[-1] = !IsVirtualAlloced;
		}
	}

	MEMREPLAY_SCOPE_ALLOC(ptr, nSize, 0);

	return ptr;
}

void CMTSafeHeap::TempVirtualFree(void* ptr)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

	char* ptrb = reinterpret_cast<char*>(ptr);
	bool source = reinterpret_cast<bool*>(ptrb)[-1];

	ptrb -= MTSAFE_DEFAULT_ALIGNMENT;

	if (source == IsVirtualAlloced)
	{
		VirtualFree(ptrb, 0, MEM_RELEASE);
	}
	else
	{
		HeapFree(m_smallHeap, 0, ptrb);
	}

	MEMREPLAY_SCOPE_FREE(ptr);
}
#endif
