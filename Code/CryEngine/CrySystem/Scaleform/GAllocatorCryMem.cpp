// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef INCLUDE_SCALEFORM_SDK

	#include "GAllocatorCryMem.h"
	#include "SharedStates.h"
	#include "SharedResources.h"
	#include "System.h"

	#define TRACK_ALLOCATIONS

//////////////////////////////////////////////////////////////////////////
// GSysAllocCryMem

GSysAllocCryMem::GSysAllocCryMem(size_t addressSpaceSize)
	: m_addressSpaceSize(addressSpaceSize)
{
	memset(&m_stats, 0, sizeof(m_stats));
	m_pHeap = CryGetIMemoryManager()->CreatePageMappingHeap(addressSpaceSize, "Scaleform");
	m_arenas.SetAlloc(this);
}

GSysAllocCryMem::~GSysAllocCryMem()
{
	m_arenas.SetAlloc(0);
	m_pHeap->Release();
}

void GSysAllocCryMem::GetInfo(Info* i) const
{
	if (i)
	{
		size_t granularity = m_pHeap->GetGranularity();
		i->MinAlign = granularity;
		i->MaxAlign = granularity;
		i->Granularity = max((int)MinGranularity, (int)granularity);
		i->HasRealloc = false;
	}
}

void* GSysAllocCryMem::Alloc(UPInt size, UPInt align)
{
	const size_t granularity = m_pHeap->GetGranularity();
	size = (size + granularity - 1) & ~(granularity - 1);

	const bool allocateInHeap = size < FlashHeapAllocSizeThreshold;
	void* ptr = allocateInHeap ? m_pHeap->Map(size) : CryGetIMemoryManager()->AllocPages(size);

	#ifndef GHEAP_TRACE_ALL
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	MEMREPLAY_SCOPE_ALLOC(ptr, size, granularity);
	#endif

	#if defined(TRACK_ALLOCATIONS)
	CryInterlockedExchangeAdd(&m_stats.AllocCount, 1);
	IF (ptr, 1)
	{
		CryInterlockedExchangeAdd(&m_stats.Allocated, size);
		IF (allocateInHeap, 1)
			CryInterlockedExchangeAdd(&m_stats.AllocatedInHeap, size);
	}
	#endif

	#if !defined(_RELEASE)
	IF (!ptr, 0)
	{
		CryGFxLog::GetAccess().LogError("Allocation request for %d bytes failed. Unexpected behavior to occur!", (int)size);
		CryGFxLog::GetAccess().LogError("Either ran out of memory or Flash address space is fragmented");
		CryGFxLog::GetAccess().LogError("%.2f MB allocated for Flash", m_stats.Allocated / (1024.0f * 1024.0f));
		CryGFxLog::GetAccess().LogError("%.2f MB allocated inside Flash heap", m_stats.AllocatedInHeap / (1024.0f * 1024.0f));
		CryGFxLog::GetAccess().LogError("%.2f MB allocated outside of Flash heap", (m_stats.Allocated - m_stats.AllocatedInHeap) / (1024.0f * 1024.0f));
		CryGFxLog::GetAccess().LogError("%.2f MB address space size for Flash heap", m_addressSpaceSize / (1024.0f * 1024.0f));
		CryGFxLog::GetAccess().LogError("%d bytes is size threshold for allocations to go through Flash heap", FlashHeapAllocSizeThreshold);
		ICVar* const pVar = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_error_debugbreak") : nullptr;
		IF (pVar && pVar->GetIVal(), 0)
		{
			__debugbreak();
		}
	}
	#endif

	return ptr;
}

bool GSysAllocCryMem::Free(void* ptr, UPInt size, UPInt align)
{
	#if defined(TRACK_ALLOCATIONS)
	CryInterlockedExchangeAdd(&m_stats.FreeCount, 1);
	IF (ptr, 1)
	{
		CryInterlockedExchangeAdd(&m_stats.Allocated, -(long)size);
		IF (size < FlashHeapAllocSizeThreshold, 1)
			CryInterlockedExchangeAdd(&m_stats.AllocatedInHeap, -(long)size);
	}
	#endif

	#ifndef GHEAP_TRACE_ALL
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	MEMREPLAY_SCOPE_FREE(ptr);
	#endif

	if (m_pHeap->IsInAddressRange(ptr))
		m_pHeap->Unmap(ptr, size);
	else
		CryGetIMemoryManager()->FreePages(ptr, size);

	return true;
}

CryGFxMemInterface::Stats GSysAllocCryMem::GetStats() const
{
	return m_stats;
}

void GSysAllocCryMem::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this) + m_stats.Allocated);
}

GSysAllocBase* GSysAllocCryMem::GetSysAllocImpl() const
{
	return (GSysAllocBase*)this;
}

GFxMemoryArenaWrapper& GSysAllocCryMem::GetMemoryArenas()
{
	return m_arenas;
}

float GSysAllocCryMem::GetFlashHeapFragmentation() const
{
	return m_pHeap ? (float) (1.0 - (double) m_pHeap->FindLargestFreeBlockSize() / (double) (m_addressSpaceSize - (size_t)m_stats.AllocatedInHeap)) : 0;
}

//////////////////////////////////////////////////////////////////////////
// GSysAllocStaticCryMem

class GSysAllocStaticChecked : public GSysAllocStatic
{
public:
	virtual void* Alloc(UPInt size, UPInt align)
	{
		void* p = GSysAllocStatic::Alloc(size, align);
		assert(p);
		if (!p)
		{
			CryGFxLog::GetAccess().LogError("Allocation request for %d bytes in static pool failed! (pool size %.1f MB, %.1f MB used / %.1f MB footprint).",
			                                (int)size, GetSize() / (1024.0f * 1024.0f), GetUsedSpace() / (1024.0f * 1024.0f), GetFootprint() / (1024.0f * 1024.0f));
		}
		return p;
	}

public:
	GSysAllocStaticChecked(void* p0 = 0, UPInt size0 = 0, void* p1 = 0, UPInt size1 = 0, void* p2 = 0, UPInt size2 = 0, void* p3 = 0, UPInt size3 = 0)
		: GSysAllocStatic(p0, size0, p1, size1, p2, size2, p3, size3) {}

	~GSysAllocStaticChecked() {}
};

GSysAllocStaticCryMem::GSysAllocStaticCryMem(size_t poolSize)
	: m_pStaticAlloc(0)
	, m_pMem(0)
	, m_size(poolSize)
	, m_arenas()
{
	m_pMem = malloc(m_size);
	if (m_pMem)
	{
		m_pStaticAlloc = new GSysAllocStaticChecked(m_pMem, m_size);
		m_arenas.SetAlloc(m_pStaticAlloc);
	}
}

GSysAllocStaticCryMem::~GSysAllocStaticCryMem()
{
	m_arenas.SetAlloc(0);
	SAFE_DELETE(m_pStaticAlloc);
	if (m_pMem)
	{
		free(m_pMem);
		m_pMem = 0;
	}
}

CryGFxMemInterface::Stats GSysAllocStaticCryMem::GetStats() const
{
	Stats stats;
	stats.AllocCount = 0;
	stats.FreeCount = 0;
	stats.Allocated = 0;
	stats.AllocatedInHeap = 0;
	return stats;
}

void GSysAllocStaticCryMem::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pStaticAlloc, sizeof(*this));
	pSizer->AddObject(m_pMem, m_size);
}

GSysAllocBase* GSysAllocStaticCryMem::GetSysAllocImpl() const
{
	assert(m_pStaticAlloc);
	return m_pStaticAlloc;
}

GFxMemoryArenaWrapper& GSysAllocStaticCryMem::GetMemoryArenas()
{
	return m_arenas;
}

float GSysAllocStaticCryMem::GetFlashHeapFragmentation() const
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// GFxMemoryArenaWrapper

int GFxMemoryArenaWrapper::ms_sys_flash_use_arenas(1);

void GFxMemoryArenaWrapper::InitCVars()
{
	#if defined(_DEBUG)
	{
		static bool s_init = false;
		assert(!s_init);
		s_init = true;
	}
	#endif

	REGISTER_CVAR2("sys_flash_use_arenas", &ms_sys_flash_use_arenas, 1,
	               0, "Enables creation of Flash asset instances through designated memory arenas.");
}

GFxMemoryArenaWrapper::GFxMemoryArenaWrapper()
	: m_lock()
	, m_pAlloc(0)
	, m_arenasActive(0)
	, m_arenasResetCache(0)
	//, m_arenasRefCnt()
{
	memset(m_arenasRefCnt, 0, sizeof(m_arenasRefCnt));
}

GFxMemoryArenaWrapper::~GFxMemoryArenaWrapper()
{
	assert(m_arenasActive == 0);
}

int GFxMemoryArenaWrapper::Create(unsigned int arenaID, bool resetCache)
{
	if (arenaID == 0 || arenaID > MaxNumArenas - 1)
		return 0;

	CryAutoCriticalSection lock(m_lock);

	if (m_pAlloc && ms_sys_flash_use_arenas)
	{
		const unsigned int areaIDAsBit = 1 << arenaID;

		if (!m_arenasRefCnt[arenaID])
		{
			GMemory::CreateArena(arenaID, this);
			m_arenasActive |= areaIDAsBit;
		}

		++m_arenasRefCnt[arenaID];

		if (resetCache)
			m_arenasResetCache |= areaIDAsBit;

		return arenaID;
	}

	return 0;
}

void GFxMemoryArenaWrapper::Destroy(unsigned int arenaID)
{
	if (arenaID == 0 || arenaID > MaxNumArenas - 1)
		return;

	CryAutoCriticalSection lock(m_lock);

	#if !defined(_RELEASE)
	IF (!m_arenasRefCnt[arenaID], 0) __debugbreak();
	#endif
	if (!--m_arenasRefCnt[arenaID])
	{
		if (GMemory::ArenaIsEmpty(arenaID))
		{
			const unsigned int areaIDAsBit = 1 << arenaID;

			GMemory::DestroyArena(arenaID);
			m_arenasActive &= ~areaIDAsBit;

			if (m_arenasResetCache & areaIDAsBit)
			{
				CSharedFlashPlayerResources::GetAccess().ResetMeshCache();
				m_arenasResetCache &= ~areaIDAsBit;
			}
		}
	#if !defined(_RELEASE)
		else
			__debugbreak();
	#endif
	}
}

void GFxMemoryArenaWrapper::SetAlloc(GSysAllocPaged* pAlloc)
{
	assert(!pAlloc || !m_pAlloc);
	m_pAlloc = pAlloc;
}

void GFxMemoryArenaWrapper::GetInfo(Info* i) const
{
	assert(m_pAlloc);
	m_pAlloc->GetInfo(i);
}

void* GFxMemoryArenaWrapper::Alloc(UPInt size, UPInt align)
{
	assert(m_pAlloc);
	return m_pAlloc->Alloc(size, align);
}

bool GFxMemoryArenaWrapper::Free(void* ptr, UPInt size, UPInt align)
{
	assert(m_pAlloc);
	return m_pAlloc->Free(ptr, size, align);
}

#endif // #ifdef INCLUDE_SCALEFORM_SDK
