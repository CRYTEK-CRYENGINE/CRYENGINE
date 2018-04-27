// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MementoMemoryManager.h"
#include "Network.h"
#include <CrySystem/ITextModeConsole.h>
#include <CryRenderer/IRenderAuxGeom.h>

#define PREVENT_ZERO_ALLOCS(x) x = std::max((size_t)x, (size_t)1)

#if MMM_USE_BUCKET_ALLOCATOR
	#include <CryMemory/BucketAllocatorImpl.h>
CMementoMemoryManager::CMementoMemoryManagerAllocator::MMMBuckets CMementoMemoryManager::CMementoMemoryManagerAllocator::m_bucketAllocator(NULL, false);
#endif

#define MMM_SAVE 0
#define MMM_PLAY 0

#if MMM_SAVE || MMM_PLAY
	#define MMM_TYPE_ALLOC           0
	#define MMM_TYPE_FREE            1
	#define MMM_TYPE_TICK            2

	#define MMM_SAVE_DATA_ALLOC_SIZE (32 * 1024)
static uint32 s_MMMSaveDataSize = MMM_SAVE_DATA_ALLOC_SIZE;
struct MMMSaveData
{
	void* p;
};
static MMMSaveData* s_pMMMSaveData;

static FILE* s_pMMMSaveFile = NULL;

	#define MMM_SAVE_DATA_FILE_CACHE_SIZE (32 * 1024)
struct MMMSaveDataFileCache
{
	MMMSaveDataFileCache()
	{
	}

	MMMSaveDataFileCache(uint16 t, uint16 s, uint32 i)
	{
		type = t;
		size = s;
		index = i;
	}

	uint32 index;
	uint16 size;
	uint16 type;
};
static MMMSaveDataFileCache s_mmmSaveDataFileCache[MMM_SAVE_DATA_FILE_CACHE_SIZE];
static uint32 s_mmmSaveDataFileCacheSize = 0;
static uint32 s_mmmSaveDataFileCacheIndex = 0;
#endif

#if MMM_SAVE
static void WriteMMMDataCache()
{
	if (gEnv->pSystem->GetApplicationInstance() == 0)
	{
		s_pMMMSaveFile = fxopen("mmm.dat", "ab");

		for (uint32 i = 0; i < s_mmmSaveDataFileCacheIndex; i++)
		{
			s_mmmSaveDataFileCache[i].index = htonl(s_mmmSaveDataFileCache[i].index);
			s_mmmSaveDataFileCache[i].size = htons(s_mmmSaveDataFileCache[i].size);
			s_mmmSaveDataFileCache[i].type = htons(s_mmmSaveDataFileCache[i].type);
			fwrite(&s_mmmSaveDataFileCache[i], 1, sizeof(s_mmmSaveDataFileCache[i]), s_pMMMSaveFile);
		}

		fclose(s_pMMMSaveFile);
	}

	s_mmmSaveDataFileCacheIndex = 0;
}

static void WriteMMMData(uint32 type, void* p, uint32 size)
{
	if (gEnv->pSystem->GetApplicationInstance() == 0)
	{
		uint32 index;

		switch (type)
		{
		case MMM_TYPE_ALLOC:
			for (index = 0; index < s_MMMSaveDataSize; index++)
			{
				if (!s_pMMMSaveData[index].p)
				{
					break;
				}
			}

			if (index == s_MMMSaveDataSize)
			{
				MMMSaveData* pOldPtr = s_pMMMSaveData;
				uint32 oldSize = s_MMMSaveDataSize;

				s_MMMSaveDataSize += MMM_SAVE_DATA_ALLOC_SIZE;
				s_pMMMSaveData = new MMMSaveData[s_MMMSaveDataSize];
				memset(s_pMMMSaveData, 0, s_MMMSaveDataSize * sizeof(s_pMMMSaveData[0]));
				memcpy(s_pMMMSaveData, pOldPtr, oldSize * sizeof(s_pMMMSaveData[0]));

				delete[] pOldPtr;
			}

			s_pMMMSaveData[index].p = p;

			break;

		case MMM_TYPE_FREE:
			for (index = 0; index < s_MMMSaveDataSize; index++)
			{
				if (s_pMMMSaveData[index].p == p)
				{
					s_pMMMSaveData[index].p = NULL;
					break;
				}
			}

			if (index == s_MMMSaveDataSize)
			{
				NetLog("WriteMMMData: Error freeing pointer %p that hasn't been allocated", p);
			}

			break;

		default:
			index = 0;
			break;
		}

		s_mmmSaveDataFileCache[s_mmmSaveDataFileCacheIndex].type = type;
		s_mmmSaveDataFileCache[s_mmmSaveDataFileCacheIndex].size = size;
		s_mmmSaveDataFileCache[s_mmmSaveDataFileCacheIndex].index = index;
		s_mmmSaveDataFileCacheIndex++;

		if (s_mmmSaveDataFileCacheIndex == MMM_SAVE_DATA_FILE_CACHE_SIZE)
		{
			WriteMMMDataCache();
		}
	}
}
#else
	#define WriteMMMDataCache()
	#define WriteMMMData(type, p, size)
#endif

#if MMM_PLAY
static CTimeValue s_readDataTime;
static void ReadMMMDataCache()
{
	if (gEnv->pSystem->GetApplicationInstance() == 0)
	{
		CTimeValue start = gEnv->pTimer->GetAsyncTime();
		s_mmmSaveDataFileCacheIndex = 0;
		s_mmmSaveDataFileCacheSize = MMM_SAVE_DATA_FILE_CACHE_SIZE;

		for (uint32 i = 0; i < MMM_SAVE_DATA_FILE_CACHE_SIZE; i++)
		{
			fread(&s_mmmSaveDataFileCache[i], 1, sizeof(s_mmmSaveDataFileCache[i]), s_pMMMSaveFile);
			s_mmmSaveDataFileCache[i].index = ntohl(s_mmmSaveDataFileCache[i].index);
			s_mmmSaveDataFileCache[i].size = ntohs(s_mmmSaveDataFileCache[i].size);
			s_mmmSaveDataFileCache[i].type = ntohs(s_mmmSaveDataFileCache[i].type);

			if (feof(s_pMMMSaveFile))
			{
				s_mmmSaveDataFileCacheSize = i;
				break;
			}
		}

		CTimeValue end = gEnv->pTimer->GetAsyncTime();
		s_readDataTime += (end - start);
	}
}

static bool ReadMMMData(MMMSaveDataFileCache& entry)
{
	if (s_mmmSaveDataFileCacheIndex >= s_mmmSaveDataFileCacheSize)
	{
		ReadMMMDataCache();
	}

	if (s_mmmSaveDataFileCacheIndex < s_mmmSaveDataFileCacheSize)
	{
		entry = s_mmmSaveDataFileCache[s_mmmSaveDataFileCacheIndex];
		s_mmmSaveDataFileCacheIndex++;
		return true;
	}

	return false;
}
#endif

#define ENABLE_MMM_DEBUG ENABLE_NETWORK_MEM_INFO

CMementoMemoryManager* CMementoMemoryRegion::m_pMMM = 0;
CMementoMemoryManager::CMementoMemoryManagerAllocator* CMementoMemoryManager::CMementoMemoryManagerAllocator::m_allocator = NULL;
int CMementoMemoryManager::CMementoMemoryManagerAllocator::m_numCMementoMemoryManagers = 0;
#if MMM_MUTEX_ENABLE
CryLockT<CRYLOCK_RECURSIVE> CMementoMemoryManager::CMementoMemoryManagerAllocator::m_mutex;
#endif

#if ENABLE_NETWORK_MEM_INFO
CMementoMemoryManager::TManagers* CMementoMemoryManager::m_pManagers;
#endif

static void DrawDebugLine(int x, int y, const char* fmt, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, fmt);
	cry_vsprintf(buffer, fmt, args);
	va_end(args);

	float white[] = { 1, 1, 1, 1 };

	IRenderAuxText::Draw2dLabel((float)(x * 12 + 12), (float)(y * 12 + 12), 1.2f, white, false, "%s", buffer);

	if (ITextModeConsole* pC = gEnv->pSystem->GetITextModeConsole())
	{
		pC->PutText(x, y, buffer);
	}
}

#if ENABLE_MMM_DEBUG
// pool sizes are 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
static const int MMMDEBUG_FIRST_POOL = 3; // == 8bytes
static const int MMMDEBUG_LAST_POOL = 12; // == 4096bytes
static const int MMMDEBUG_NPOOLS = MMMDEBUG_LAST_POOL - MMMDEBUG_FIRST_POOL + 1;
struct MMMDebugData
{
	#if !MMM_USE_BUCKET_ALLOCATOR
	int m_numAllocs[MMMDEBUG_NPOOLS];
	int m_numFrees[MMMDEBUG_NPOOLS];
	#endif
	int m_totalNumAllocs;
	int m_totalNumFrees;

	int m_currentNumAllocs;
};

static MMMDebugData s_MMMDebug;

static void MMMDebugClearFrameData()
{
	#if !MMM_USE_BUCKET_ALLOCATOR
	memset(s_MMMDebug.m_numAllocs, 0, sizeof(s_MMMDebug.m_numAllocs));
	memset(s_MMMDebug.m_numFrees, 0, sizeof(s_MMMDebug.m_numFrees));
	#endif
	s_MMMDebug.m_totalNumAllocs = 0;
	s_MMMDebug.m_totalNumFrees = 0;
}

static void MMMDebugInit()
{
	MMMDebugClearFrameData();

	s_MMMDebug.m_currentNumAllocs = 0;

	#if MMM_SAVE || MMM_PLAY
	s_pMMMSaveData = new MMMSaveData[s_MMMSaveDataSize];
	memset(s_pMMMSaveData, 0, s_MMMSaveDataSize * sizeof(s_pMMMSaveData[0]));
	#endif

	#if MMM_SAVE
	if (gEnv->pSystem->GetApplicationInstance() == 0)
	{
		s_pMMMSaveFile = fxopen("mmm.dat", "wb");
		fclose(s_pMMMSaveFile);
	}
	#endif
}

static void MMMDebugAddAlloc(void* p, size_t sz)
{
	PREVENT_ZERO_ALLOCS(sz);
	#if !MMM_USE_BUCKET_ALLOCATOR
	int pool = std::max((int)IntegerLog2_RoundUp(sz) - MMMDEBUG_FIRST_POOL, 0);

	if (pool < MMMDEBUG_NPOOLS)
	{
		s_MMMDebug.m_numAllocs[pool]++;
	}
	#endif

	s_MMMDebug.m_totalNumAllocs++;
	s_MMMDebug.m_currentNumAllocs++;

	WriteMMMData(MMM_TYPE_ALLOC, p, sz);
}

static void MMMDebugRemoveAlloc(void* p, size_t sz)
{
	PREVENT_ZERO_ALLOCS(sz);
	#if !MMM_USE_BUCKET_ALLOCATOR
	int pool = std::max((int)IntegerLog2_RoundUp(sz) - MMMDEBUG_FIRST_POOL, 0);

	if (pool < MMMDEBUG_NPOOLS)
	{
		s_MMMDebug.m_numFrees[pool]++;
	}
	#endif

	s_MMMDebug.m_totalNumFrees++;
	s_MMMDebug.m_currentNumAllocs--;

	WriteMMMData(MMM_TYPE_FREE, p, sz);
}

static void MMMDebugDraw(int x, int& y)
{
	#if !MMM_USE_BUCKET_ALLOCATOR
	for (int i = 0; i < MMMDEBUG_NPOOLS; i++)
	{
		DrawDebugLine(x, y++, "Pool %d (%4d byte blocks): Frame Allocs %4d Frame Frees %4d", i, 1 << (i + MMMDEBUG_FIRST_POOL), s_MMMDebug.m_numAllocs[i], s_MMMDebug.m_numFrees[i]);
	}
	#endif

	DrawDebugLine(x, y++, "Total: Frame Allocs %4d Frame Frees %4d", s_MMMDebug.m_totalNumAllocs, s_MMMDebug.m_totalNumFrees);
	y++;
	DrawDebugLine(x, y++, "Current Num Allocations %4d", s_MMMDebug.m_currentNumAllocs);

	MMMDebugClearFrameData();
}
#else
	#define MMMDebugInit()
	#define MMMDebugRemoveAlloc(p, sz)
	#define MMMDebugAddAlloc(p, sz)
	#define MMMDebugDraw(x, y)
#endif

CMementoMemoryManager::CMementoMemoryManagerAllocator::CMementoMemoryManagerAllocator()
{
#if MMM_USE_BUCKET_ALLOCATOR
	m_bucketAllocator.EnableExpandCleanups(false);
	m_bucketTotalRequested = 0;
	m_bucketTotalAllocated = 0;
	#if LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
	m_bucketHighWaterMark = 0;
	m_generalHeapHighWaterMark = 0;
	#endif // LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
#else
	memset(m_freeList, 0, sizeof(m_freeList));
	memset(m_numAllocated, 0, sizeof(m_numAllocated));
	memset(m_numFree, 0, sizeof(m_numFree));
#endif

#if !defined(PURE_CLIENT)
	m_pGeneralHeap = CryGetIMemoryManager()->CreateGeneralExpandingMemoryHeap(MMM_GENERAL_HEAP_SIZE, MMM_GENERAL_HEAP_SIZE, "Memento General");
	m_generalHeapTotalRequested = 0;
	m_generalHeapTotalAllocated = 0;
#endif

	MMMDebugInit();
}

CMementoMemoryManager::CMementoMemoryManagerAllocator::~CMementoMemoryManagerAllocator()
{
	NET_ASSERT(m_handles.size() == m_freeHandles.size());

#if !defined(PURE_CLIENT)
	if (m_pGeneralHeap)
	{
		m_pGeneralHeap->Release();
	}
#endif
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::AddCMementoMemoryManager()
{
#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	if (m_numCMementoMemoryManagers == 0)
	{
		m_allocator = new CMementoMemoryManagerAllocator();
	}

	m_numCMementoMemoryManagers++;

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::RemoveCMementoMemoryManager()
{
#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	m_numCMementoMemoryManagers--;

	if (m_numCMementoMemoryManagers == 0)
	{
#if LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
		CryLogAlways("[CMementoMemoryManager] high water marks: bucket = %d, general heap = %d", m_allocator->m_bucketHighWaterMark, m_allocator->m_generalHeapHighWaterMark);
#endif // LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK

		delete m_allocator;
		m_allocator = NULL;
	}

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif
}

#ifdef _DEBUG
	#define MMM_MUTEX_DEBUG (1)
#else
	#define MMM_MUTEX_DEBUG (0)
#endif

#if MMM_MUTEX_DEBUG

	#define MMM_CRASH_ON_MUTEX_FAILURE (0)

void MMM_ASSERT_GLOBAL_LOCK(void)
{
	NetFastMutex& nfm = CNetwork::Get()->GetMutex();
	if (CNetwork::Get()->IsMultithreaded() && !nfm.IsLocked())
	{
		CryLogAlways("USE OF MEMENTO WHEN WE HAVEN'T ACQUIRED THE NETWORK THREAD LOCK!");
	#if MMM_CRASH_ON_MUTEX_FAILURE
		int* a = 0;
		*a = 32;
	#endif
	}
}
#else
	#define MMM_ASSERT_GLOBAL_LOCK()
#endif

CMementoMemoryManager::Hdl CMementoMemoryManager::CMementoMemoryManagerAllocator::AllocHdl(size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();
	Hdl hdl;
	PREVENT_ZERO_ALLOCS(sz);

#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	if (m_freeHandles.empty())
	{
		hdl = m_handles.size();
		m_handles.push_back(SHandleData());
	}
	else
	{
		hdl = m_freeHandles.back();
		m_freeHandles.pop_back();
	}

	InitHandleData(m_handles[hdl], sz);

	hdl = ProtectHdl(hdl);

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif

	return hdl;
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::ResizeHdl(Hdl hdl, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();
	PREVENT_ZERO_ALLOCS(sz);
#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	hdl = UnprotectHdl(hdl);

	NET_ASSERT(hdl != InvalidHdl);

	SHandleData& hd = m_handles[hdl];

#if MMM_USE_BUCKET_ALLOCATOR
	if (sz != hd.size)
	{
		SHandleData hdp;

		InitHandleData(hdp, sz);

		if (sz < hd.size)
		{
			memcpy(hdp.p, hd.p, sz);
		}
		else
		{
			memcpy(hdp.p, hd.p, hd.size);
		}

		FreePtr(hd.p, hd.size);

		hd = hdp;
	}
#else
	if (sz > hd.size) // growing
	{
		if (sz > hd.capacity) // growing and changing pools
		{
			SHandleData hdp;
			InitHandleData(hdp, sz);
			memcpy(hdp.p, hd.p, hd.size);
			FreePtr(hd.p, hd.size);
			hd = hdp;
		}
		else
		{
			hd.size = sz;
		}
	}
	else if (sz < hd.size) // shrinking
	{
		if (sz <= hd.capacity / 2) // shrinking and changing pools
		{
			SHandleData hdp;
			InitHandleData(hdp, sz);
			memcpy(hdp.p, hd.p, sz);
			FreePtr(hd.p, hd.size);
			hd = hdp;
		}
		else
		{
			hd.size = sz;
		}
	}
#endif

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::FreeHdl(Hdl hdl)
{
	MMM_ASSERT_GLOBAL_LOCK();
#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	hdl = UnprotectHdl(hdl);

	if (hdl != InvalidHdl)
	{
		SHandleData& hd = m_handles[hdl];

		FreePtr(hd.p, hd.size);
		m_freeHandles.push_back(hdl);
	}

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif
}

void* CMementoMemoryManager::CMementoMemoryManagerAllocator::AllocPtr(size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();
	SHandleData hd;
	PREVENT_ZERO_ALLOCS(sz);

#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	InitHandleData(hd, sz);

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif

	return hd.p;
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::FreePtr(void* p, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();
	PREVENT_ZERO_ALLOCS(sz);
#if MMM_MUTEX_ENABLE
	m_mutex.Lock();
#endif

	MMMDebugRemoveAlloc(p, sz);

#if MMM_USE_BUCKET_ALLOCATOR
	if (m_bucketAllocator.IsInAddressRange(p))
	{
		m_bucketTotalRequested -= sz;
		m_bucketTotalAllocated -= m_bucketAllocator.getSize(p);
		m_bucketAllocator.deallocate(p);
	}
	else
	{
	#if !defined(PURE_CLIENT)
		m_generalHeapTotalRequested -= sz;
		m_generalHeapTotalAllocated -= m_pGeneralHeap->UsableSize(p);
		m_pGeneralHeap->Free(p);
	#else
		free(p);
	#endif
	}
#else
	int pool = std::max((int)IntegerLog2_RoundUp(sz) - FIRST_POOL, 0);

	if (pool < NPOOLS)
	{
		((SFreeListHeader*)p)->pNext = m_freeList[pool].pNext;
		m_freeList[pool].pNext = (SFreeListHeader*)p;
		m_numFree[pool]++;
		m_numAllocated[pool]--;
	}
	else
	{
	#if !defined(PURE_CLIENT)
		m_generalHeapTotalRequested -= sz;
		m_generalHeapTotalAllocated -= m_pGeneralHeap->UsableSize(p);
		m_pGeneralHeap->Free(p);
	#else
		free(p);
	#endif
	}
#endif

#if MMM_MUTEX_ENABLE
	m_mutex.Unlock();
#endif
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::InitHandleData(SHandleData& hd, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);
#if MMM_USE_BUCKET_ALLOCATOR
	if (sz <= m_bucketAllocator.MaxSize)
	{
		hd.p = m_bucketAllocator.allocate(sz);
		if (hd.p == NULL)
		{
			CryFatalError("Memento Allocation For %" PRISIZE_T " Failed - Suggest increasing the bucket allocator size!!!!", sz);
		}
		hd.size = sz;
		hd.capacity = m_bucketAllocator.getSize(hd.p);
		m_bucketTotalRequested += hd.size;
		m_bucketTotalAllocated += hd.capacity;
	#if LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
		if (m_bucketTotalAllocated > m_bucketHighWaterMark)
		{
			m_bucketHighWaterMark = m_bucketTotalAllocated;
		}
	#endif // LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
	}
	else
	{
	#if !defined(PURE_CLIENT)
		hd.p = m_pGeneralHeap->Malloc(sz, "MMM");
		if (hd.p == NULL)
		{
			CryFatalError("Memento Allocation For %" PRISIZE_T " Failed - Suggest increasing the general heap size!!!!", sz);
		}
		hd.size = sz;
		hd.capacity = m_pGeneralHeap->UsableSize(hd.p);
		m_generalHeapTotalRequested += hd.size;
		m_generalHeapTotalAllocated += hd.capacity;
		#if LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
		if (m_generalHeapTotalAllocated > m_generalHeapHighWaterMark)
		{
			m_generalHeapHighWaterMark = m_generalHeapTotalAllocated;
		}
		#endif // LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
	#else
		hd.p = malloc(sz);
		if (hd.p == NULL)
		{
			CryFatalError("Allocation for %" PRISIZE_T " Failed", sz);
		}
		hd.size = sz;
		hd.capacity = sz;
	#endif
	}
#else
	size_t szP2 = IntegerLog2_RoundUp(sz);
	uint32 pool = std::max((int)szP2 - FIRST_POOL, 0);

	if (pool < NPOOLS)
	{
		if (!m_freeList[pool].pNext)
		{
			uint32 testPool;

			for (testPool = pool + 1; testPool < NPOOLS; testPool++)
			{
				if (m_freeList[testPool].pNext)
				{
					break;
				}
			}

			if (testPool == NPOOLS)
			{
				testPool = NPOOLS - 1;
				SFreeListHeader* p1 = (SFreeListHeader*)m_pool.Allocate();
				p1->pNext = m_freeList[testPool].pNext;
				m_freeList[testPool].pNext = p1;
				m_numFree[testPool]++;
			}

			for (; testPool > pool; testPool--)
			{
				SFreeListHeader* p1 = m_freeList[testPool].pNext;
				SFreeListHeader* p2 = (SFreeListHeader*)((uint8*)p1 + (uint32)(1 << (testPool + FIRST_POOL - 1)));

				m_freeList[testPool].pNext = m_freeList[testPool].pNext->pNext;
				m_numFree[testPool]--;

				p1->pNext = m_freeList[testPool - 1].pNext;
				p2->pNext = p1;
				m_freeList[testPool - 1].pNext = p2;
				m_numFree[testPool - 1] += 2;
			}
		}

		if (m_freeList[pool].pNext)
		{
			hd.p = m_freeList[pool].pNext;
			m_freeList[pool].pNext = m_freeList[pool].pNext->pNext;
			m_numFree[pool]--;
			m_numAllocated[pool]++;
			hd.size = sz;
			hd.capacity = (size_t)1 << szP2;
		}
		else
		{
			hd.p = NULL;
			hd.size = 0;
			hd.capacity = 0;
		}
	}
	else
	{
	#if !defined(PURE_CLIENT)
		hd.p = m_pGeneralHeap->Malloc(sz, "MMM");
		hd.size = sz;
		hd.capacity = m_pGeneralHeap->UsableSize(hd.p);
		m_generalHeapTotalRequested += hd.size;
		m_generalHeapTotalAllocated += hd.capacity;
	#else
		hd.p = malloc(sz);
		hd.size = sz;
		hd.capacity = sz;
	#endif
	}
#endif

	if (hd.p == NULL)
	{
		CryFatalError("Memento Allocation For %" PRISIZE_T " Failed - Suggest increasing the memento heap size!!!!", sz);
	}

	MMMDebugAddAlloc(hd.p, hd.size);
	NET_ASSERT(hd.capacity >= sz);
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::Tick()
{
}

void CMementoMemoryManager::CMementoMemoryManagerAllocator::DebugDraw(int x, int& y, size_t& totalAllocated)
{
#if ENABLE_NETWORK_MEM_INFO

	#if MMM_USE_BUCKET_ALLOCATOR
	DrawDebugLine(x, y++, "Memento allocator memory");
	DrawDebugLine(x, y++, "Bucket Allocator Requested %8" PRISIZE_T " Allocated %8" PRISIZE_T " Storage Size %8" PRISIZE_T " Storage Capacity %8" PRISIZE_T " pages", m_bucketTotalRequested, m_bucketTotalAllocated, m_bucketAllocator.GetBucketStorageSize(), m_bucketAllocator.GetBucketStoragePages());
	totalAllocated += m_bucketTotalAllocated;
	#else
	DrawDebugLine(x, y++, "Memento allocator memory");

	for (int i = 0; i < NPOOLS; i++)
	{
		DrawDebugLine(x, y++, "Pool %d (%4d byte blocks): Num Allocated %4d Num Free %4d Amount Allocated %8d Amount Free %8d", i, 1 << (i + FIRST_POOL), m_numAllocated[i], m_numFree[i], m_numAllocated[i] * (1 << (i + FIRST_POOL)), m_numFree[i] * (1 << (i + FIRST_POOL)));
		totalAllocated += m_numAllocated[i] * (1 << (i + FIRST_POOL));
	}

	DrawDebugLine(x, y++, "Total: Allocated %8d Pool Capacity %8d System Allocated %8d", totalAllocated, m_pool.GetTotalMemory().nUsed, m_pool.GetTotalMemory().nAlloc);
	#endif

	#if !defined(PURE_CLIENT)
	DrawDebugLine(x, y++, "General Heap Requested %8d Allocated %8d", m_generalHeapTotalRequested, m_generalHeapTotalAllocated);
	totalAllocated += m_generalHeapTotalAllocated;
	#endif
#endif
}

CMementoMemoryManager::CMementoMemoryManager(const string& name) : m_name(name)
{
	ASSERT_GLOBAL_LOCK;

	CMementoMemoryManagerAllocator::AddCMementoMemoryManager();

#if ENABLE_NETWORK_MEM_INFO
	if (!m_pManagers)
	{
		m_pManagers = new TManagers;
	}

	m_pManagers->push_back(this);
#endif

	arith_zeroSizeHdl = InvalidHdl;

	for (int i = 0; i < sizeof(pThings) / sizeof(*pThings); i++)
	{
		pThings[i] = 0;
	}

	m_totalAllocations = 0;
}

CMementoMemoryManager::~CMementoMemoryManager()
{
	SCOPED_GLOBAL_LOCK;

	FreeHdl(arith_zeroSizeHdl);

	MMM_REGION(this);

	for (int i = 0; i < sizeof(pThings) / sizeof(*pThings); i++)
	{
		SAFE_RELEASE(pThings[i]);
	}

	NET_ASSERT(m_totalAllocations == 0);

#if ENABLE_NETWORK_MEM_INFO
	stl::find_and_erase(*m_pManagers, this);
#endif

	CMementoMemoryManagerAllocator::RemoveCMementoMemoryManager();
}

CMementoMemoryManager::Hdl CMementoMemoryManager::AllocHdl(size_t sz, void* callerOverride)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	Hdl hdl = CMementoMemoryManagerAllocator::GetAllocator()->AllocHdl(sz);

	m_totalAllocations += sz;

#if MMM_CHECK_LEAKS
	void* caller = callerOverride ? callerOverride : UP_STACK_PTR;
	m_hdlToAlloc[hdl] = caller;
	m_allocAmt[caller] += sz;
#endif

	return hdl;
}

CMementoMemoryManager::Hdl CMementoMemoryManager::CloneHdl(Hdl hdl)
{
	MMM_ASSERT_GLOBAL_LOCK();

	Hdl out = AllocHdl(GetHdlSize(hdl), UP_STACK_PTR);

	memcpy(PinHdl(out), PinHdl(hdl), GetHdlSize(hdl));

	return out;
}

void CMementoMemoryManager::ResizeHdl(Hdl hdl, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	size_t oldSize = GetHdlSize(hdl);

	CMementoMemoryManagerAllocator::GetAllocator()->ResizeHdl(hdl, sz);

	m_totalAllocations -= oldSize;
	m_totalAllocations += sz;

#if MMM_CHECK_LEAKS
	void* caller = m_hdlToAlloc[hdl];
	uint32& callerAlloc = m_allocAmt[caller];
	callerAlloc -= oldSize;
	callerAlloc += sz;
#endif
}

void CMementoMemoryManager::FreeHdl(Hdl hdl)
{
	MMM_ASSERT_GLOBAL_LOCK();

	size_t size = GetHdlSize(hdl);

	m_totalAllocations -= size;

#if MMM_CHECK_LEAKS
	void* caller = m_hdlToAlloc[hdl];

	m_hdlToAlloc.erase(hdl);

	if (!(m_allocAmt[caller] -= size))
	{
		m_allocAmt.erase(caller);
	}
#endif

	CMementoMemoryManagerAllocator::GetAllocator()->FreeHdl(hdl);
}

void* CMementoMemoryManager::AllocPtr(size_t sz, void* callerOverride)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	m_totalAllocations += sz;

	void* ret = CMementoMemoryManagerAllocator::GetAllocator()->AllocPtr(sz);

#if MMM_CHECK_LEAKS
	void* caller = callerOverride ? callerOverride : UP_STACK_PTR;
	m_ptrToAlloc[ret] = caller;
	m_allocAmt[caller] += sz;
#endif

	return ret;
}

void CMementoMemoryManager::FreePtr(void* p, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	CMementoMemoryManagerAllocator::GetAllocator()->FreePtr(p, sz);

	m_totalAllocations -= sz;

#if MMM_CHECK_LEAKS
	void* caller = m_ptrToAlloc[p];

	m_ptrToAlloc.erase(p);

	if (!(m_allocAmt[caller] -= sz))
	{
		m_allocAmt.erase(caller);
	}
#endif
}

void CMementoMemoryManager::AddHdlToSizer(Hdl hdl, ICrySizer* pSizer)
{
	if (hdl != InvalidHdl)
	{
		pSizer->AddObject(PinHdl(hdl), GetHdlSize(hdl));
	}
}

void CMementoMemoryManager::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis /* = false */)
{
	SIZER_COMPONENT_NAME(pSizer, "CMementoMemoryManager");

	if (countingThis)
	{
		pSizer->Add(*this);
	}
}

void CMementoMemoryManager::DebugDraw()
{
#if ENABLE_NETWORK_MEM_INFO
	if (m_pManagers && CMementoMemoryManagerAllocator::GetAllocator() && CVARS.MemInfo & eDMM_Mementos)
	{
		static size_t maxTotalRequested = 0;
		static size_t maxTotalAllocated = 0;
		size_t totalRequested = 0;
		size_t totalAllocated = 0;
		int x = 0;
		int y = 2;
		bool changed = false;

		CMementoMemoryManagerAllocator::GetAllocator()->DebugDraw(x, y, totalAllocated);
		y++;

		for (TManagers::iterator it = m_pManagers->begin(); it != m_pManagers->end(); ++it)
		{
			DrawDebugLine(x, y++, "Memento memory for %16s: Requested %8d", (*it)->m_name.c_str(), (*it)->m_totalAllocations);
			totalRequested += (*it)->m_totalAllocations;
		}

		y++;

		if (totalRequested > maxTotalRequested)
		{
			maxTotalRequested = totalRequested;
			changed = true;
		}

		if (totalAllocated > maxTotalAllocated)
		{
			maxTotalAllocated = totalAllocated;
			changed = true;
		}

		DrawDebugLine(x, y++, "Total MMM: Requested %8d Allocated %8d", totalRequested, totalAllocated);
		DrawDebugLine(x, y++, "Max Total MMM: Requested %8d Allocated %8d", maxTotalRequested, maxTotalAllocated);
		y++;

		if (changed)
		{
			CryLogAlways("[net mmm] Max Total MMM: Requested %" PRISIZE_T " Allocated %" PRISIZE_T, maxTotalRequested, maxTotalAllocated);
		}

		MMMDebugDraw(x, y);
	}
#endif
}

void CMementoMemoryManager::Tick()
{
	WriteMMMData(MMM_TYPE_TICK, 0, 0);
	WriteMMMDataCache();

#if MMM_PLAY
	static bool s_playbackDone = false;
	if (!s_playbackDone && (gEnv->pSystem->GetApplicationInstance() == 0))
	{
		s_playbackDone = true;
		s_pMMMSaveFile = fxopen("mmm.dat", "rb");

		s_readDataTime = CTimeValue();
		CTimeValue start = gEnv->pTimer->GetAsyncTime();

		NetLog("MMM playback start time %" PRId64, start.GetMilliSecondsAsInt64());

		while (true)
		{
			MMMSaveDataFileCache entry;

			if (ReadMMMData(entry))
			{
				switch (entry.type)
				{
				case MMM_TYPE_ALLOC:
					if (entry.index >= s_MMMSaveDataSize)
					{
						MMMSaveData* pOldPtr = s_pMMMSaveData;
						uint32 oldSize = s_MMMSaveDataSize;

						do
						{
							s_MMMSaveDataSize += MMM_SAVE_DATA_ALLOC_SIZE;
						}
						while (entry.index >= s_MMMSaveDataSize);

						s_pMMMSaveData = new MMMSaveData[s_MMMSaveDataSize];
						memset(s_pMMMSaveData, 0, s_MMMSaveDataSize * sizeof(s_pMMMSaveData[0]));
						memcpy(s_pMMMSaveData, pOldPtr, oldSize * sizeof(s_pMMMSaveData[0]));

						delete[] pOldPtr;
					}

					if (!s_pMMMSaveData[entry.index].p)
					{
						s_pMMMSaveData[entry.index].p = CMementoMemoryManager::CMementoMemoryManagerAllocator::GetAllocator()->AllocPtr(entry.size);
					}
					else
					{
						NetLog("MMM Playerback: Error allocating into a slot already allocated.");
					}

					break;

				case MMM_TYPE_FREE:
					if ((entry.index < s_MMMSaveDataSize) && s_pMMMSaveData[entry.index].p)
					{
						CMementoMemoryManager::CMementoMemoryManagerAllocator::GetAllocator()->FreePtr(s_pMMMSaveData[entry.index].p, entry.size);
						s_pMMMSaveData[entry.index].p = NULL;
					}
					else
					{
						NetLog("MMM Playerback: Error freeing from a slot not allocated.");
					}
					break;

				case MMM_TYPE_TICK:
					CMementoMemoryManager::CMementoMemoryManagerAllocator::GetAllocator()->Tick();
					break;
				}
			}
			else
			{
				break;
			}
		}

		CTimeValue end = gEnv->pTimer->GetAsyncTime();
		NetLog("MMM playback end time %" PRId64 " time taken %" PRId64 " File read time %" PRId64 " time taken - file read time %" PRId64, end.GetMilliSecondsAsInt64(), (end - start).GetMilliSecondsAsInt64(), s_readDataTime.GetMilliSecondsAsInt64(), ((end - start) - s_readDataTime).GetMilliSecondsAsInt64());

		fclose(s_pMMMSaveFile);
	}
#endif

	CMementoMemoryManager::CMementoMemoryManagerAllocator::GetAllocator()->Tick();
}

/*
 * CMementoStreamAllocator
 */

CMementoStreamAllocator::CMementoStreamAllocator(const CMementoMemoryManagerPtr& mmm) : m_hdl(CMementoMemoryManager::InvalidHdl), m_pPin(0), m_mmm(mmm)
{
}

void* CMementoStreamAllocator::Alloc(size_t sz, void* callerOverride)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	NET_ASSERT(m_hdl == CMementoMemoryManager::InvalidHdl);

	m_hdl = m_mmm->AllocHdl(sz, callerOverride ? callerOverride : UP_STACK_PTR);

	return m_pPin = m_mmm->PinHdl(m_hdl);
}

void* CMementoStreamAllocator::Realloc(void* old, size_t sz)
{
	MMM_ASSERT_GLOBAL_LOCK();

	PREVENT_ZERO_ALLOCS(sz);

	NET_ASSERT(m_pPin == old && m_pPin);
	m_mmm->ResizeHdl(m_hdl, sz);

	return m_pPin = m_mmm->PinHdl(m_hdl);
}

void CMementoStreamAllocator::Free(void* old)
{
}
