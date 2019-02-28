// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryCore/BitFiddling.h>

#include <stdio.h>
#include <CrySystem/ISystem.h>
#include <CryCore/Platform/platform.h>

#include <CryMemory/CryMemoryAllocator.h>
#if CRY_PLATFORM_WINDOWS
	#include "DebugCallStack.h"
#endif

#include "MemReplay.h"
#include "MemoryManager.h"

volatile bool g_replayCleanedUp = false;

#ifdef DANGLING_POINTER_DETECTOR
	#if CRY_PLATFORM_ORBIS
		#include "CryMemoryManager_sce.h"
	#else
void* DanglingPointerDetectorTransformAlloc(void* ptr, size_t size) { return ptr; }
void* DanglingPointerDetectorTransformFree(void* ptr)               { return ptr; }
void* DanglingPointerDetectorTransformNull(void* ptr)               { return ptr; }
	#endif
#endif

const bool bProfileMemManager = 0;

#ifdef CRYMM_SUPPORT_DEADLIST
namespace
{
struct DeadHeadSize
{
	DeadHeadSize* pPrev;
	UINT_PTR      nSize;
};
}

static CryCriticalSectionNonRecursive s_deadListLock;
static DeadHeadSize* s_pDeadListFirst = NULL;
static DeadHeadSize* s_pDeadListLast = NULL;
static size_t s_nDeadListSize;

static void CryFreeReal(void* p);

static void DeadListValidate(const uint8* p, size_t sz, uint8 val)
{
	for (const uint8* pe = p + sz; p != pe; ++p)
	{
		if (*p != val)
		{
			CryGetIMemReplay()->Stop();
			__debugbreak();
		}
	}
}

static void DeadListFlush_Locked()
{
	while ((s_nDeadListSize > (size_t)CCryMemoryManager::s_sys_MemoryDeadListSize) && s_pDeadListLast)
	{
		DeadHeadSize* pPrev = s_pDeadListLast->pPrev;

		size_t sz = s_pDeadListLast->nSize;
		void* pVal = s_pDeadListLast + 1;

		DeadListValidate((const uint8*)pVal, sz - sizeof(DeadHeadSize), 0xfe);

		CryFreeReal(s_pDeadListLast);

		s_pDeadListLast = pPrev;
		s_nDeadListSize -= sz;
	}

	if (!s_pDeadListLast)
		s_pDeadListFirst = NULL;
}

static void DeadListPush(void* p, size_t sz)
{
	if (sz >= sizeof(DeadHeadSize))
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lock(s_deadListLock);
		DeadHeadSize* pHead = (DeadHeadSize*)p;

		memset(pHead + 1, 0xfe, sz - sizeof(DeadHeadSize));
		pHead->nSize = sz;

		if (s_pDeadListFirst)
			s_pDeadListFirst->pPrev = pHead;

		pHead->pPrev = 0;
		s_pDeadListFirst = pHead;
		if (!s_pDeadListLast)
			s_pDeadListLast = pHead;

		s_nDeadListSize += sz;
		if (s_nDeadListSize > (size_t)CCryMemoryManager::s_sys_MemoryDeadListSize)
			DeadListFlush_Locked();
	}
	else
	{
		CryFreeReal(p);
	}
}

#endif

//////////////////////////////////////////////////////////////////////////
// Some globals for fast profiling.
//////////////////////////////////////////////////////////////////////////
LONG g_TotalAllocatedMemory = 0;

#ifndef CRYMEMORYMANAGER_API
	#define CRYMEMORYMANAGER_API
#endif // CRYMEMORYMANAGER_API

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API size_t CryFree(void* p, size_t alignment);
CRYMEMORYMANAGER_API void   CryFreeSize(void* p, size_t size);

CRYMEMORYMANAGER_API void   CryCleanup();

// Undefine malloc for memory manager itself..
#undef malloc
#undef realloc
#undef free

#define VIRTUAL_ALLOC_SIZE 524288

#include <CryMemory/CryMemoryAllocator.h>

#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	#include <CryMemory/BucketAllocatorImpl.h>
BucketAllocator<BucketAllocatorDetail::DefaultTraits<BUCKET_ALLOCATOR_DEFAULT_SIZE, BucketAllocatorDetail::SyncPolicyLocked, true, 8>> g_GlobPageBucketAllocator;
#else
node_alloc<eCryMallocCryFreeCRTCleanup, true, VIRTUAL_ALLOC_SIZE> g_GlobPageBucketAllocator;
#endif // defined(USE_GLOBAL_BUCKET_ALLOCATOR)

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API void* CryMalloc(size_t size, size_t& allocated, size_t alignment)
{
	if (!size)
	{
		allocated = 0;
		return 0;
	}

	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

	uint8* p;
	size_t sizePlus = size;

	if (!alignment || g_GlobPageBucketAllocator.CanGuaranteeAlignment(sizePlus, alignment))
	{
		if (alignment)
		{
			p = (uint8*)g_GlobPageBucketAllocator.allocate(sizePlus, alignment);
		}
		else
		{
			p = (uint8*)g_GlobPageBucketAllocator.alloc(sizePlus);
		}

		// The actual allocated memory will be the size of a whole bucket, which might be bigger than the requested size.
		// e.g. if we request 56bytes and the next fitting bucket is 64bytes, we will be allocating 64bytes.
		//
		// Having the correct value is important because the whole bucket size is later used by CryFree when
		// deallocating and both values should match for stats tracking to be accurate.
		//
		// We use getSize and not getSizeEx because in the case of an allocation with "alignment" == 0 and "size" bigger than the bucket size
		// the bucket allocator will end up allocating memory using CryCrtMalloc which falls outside the address range of the bucket allocator
		sizePlus = g_GlobPageBucketAllocator.getSize(p);
	}
	else
	{
		alignment = max(alignment, (size_t)16);

		// emulate alignment
		sizePlus += alignment;
		p = (uint8*) CrySystemCrtMalloc(sizePlus);

		if (alignment && p)
		{
			uint32 offset = (uint32)(alignment - ((UINT_PTR)p & (alignment - 1)));
			p += offset;
			reinterpret_cast<uint32*>(p)[-1] = offset;
		}
	}

	//////////////////////////////////////////////////////////////////////////
#if !defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	if (sizePlus < __MAX_BYTES + 1)
	{
		sizePlus = ((sizePlus - size_t(1)) >> (int)_ALIGN_SHIFT);
		sizePlus = (sizePlus + 1) << _ALIGN_SHIFT;
	}
#endif //!defined(USE_GLOBAL_BUCKET_ALLOCATOR)

	if (!p)
	{
		allocated = 0;
		gEnv->bIsOutOfMemory = true;
		CryFatalError("**** Memory allocation for %" PRISIZE_T " bytes failed ****", sizePlus);
		return 0;   // don't crash - allow caller to react
	}

	CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, sizePlus);
	allocated = sizePlus;

	MEMREPLAY_SCOPE_ALLOC(p, sizePlus, 0);

	assert(alignment == 0 || (reinterpret_cast<UINT_PTR>(p) & (alignment - 1)) == 0);

#ifdef DANGLING_POINTER_DETECTOR
	return DanglingPointerDetectorTransformAlloc(p, size);
#endif

	return p;
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API size_t CryGetMemSize(void* memblock, size_t alignment)
{
	//	ReadLock lock(g_lockMemMan);
#ifdef DANGLING_POINTER_DETECTOR
	memblock = DanglingPointerDetectorTransformNull(memblock);
#endif
	
	if (!alignment || g_GlobPageBucketAllocator.IsInAddressRange(memblock))
	{
		return g_GlobPageBucketAllocator.getSize(memblock);
	}
	else
	{
		uint8* pb = static_cast<uint8*>(memblock);
		uint32 adj = reinterpret_cast<uint32*>(pb)[-1];
		return CrySystemCrtSize(pb - adj) - adj;
	}
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API void* CryRealloc(void* memblock, size_t size, size_t& allocated, size_t& oldsize, size_t alignment)
{
	if (memblock == NULL)
	{
		oldsize = 0;
		return CryMalloc(size, allocated, alignment);
	}
	else
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CrtMalloc);

#ifdef DANGLING_POINTER_DETECTOR
		memblock = DanglingPointerDetectorTransformFree(memblock);
#endif

		void* np;
		np = CryMalloc(size, allocated, alignment);

		// get old size
		if (g_GlobPageBucketAllocator.IsInAddressRange(memblock))
		{
			oldsize = g_GlobPageBucketAllocator.getSize(memblock);
		}
		else
		{
			uint8* pb = static_cast<uint8*>(memblock);
			int adj = 0;
			if (alignment)
				adj = reinterpret_cast<uint32*>(pb)[-1];
			oldsize = CrySystemCrtSize(pb - adj) - adj;
		}

		if (!np && size)
		{
			gEnv->bIsOutOfMemory = true;
			CryFatalError("**** Memory allocation for %" PRISIZE_T " bytes failed ****", size);
			return 0;   // don't crash - allow caller to react
		}

		// copy data over
		memcpy(np, memblock, size > oldsize ? oldsize : size);
		CryFree(memblock, alignment);

		MEMREPLAY_SCOPE_REALLOC(memblock, np, size, alignment);

		assert(alignment == 0 || (reinterpret_cast<UINT_PTR>(np) & (alignment - 1)) == 0);
		return np;
	}
}

#ifdef CRYMM_SUPPORT_DEADLIST
static void CryFreeReal(void* p)
{
	UINT_PTR pid = (UINT_PTR)p;

	if (p != NULL)
	{
		if (g_GlobPageBucketAllocator.IsInAddressRange(p))
			g_GlobPageBucketAllocator.deallocate(p);
		else
			free(p);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
size_t CryFree(void* p, size_t alignment)
{
#ifdef DANGLING_POINTER_DETECTOR
	p = DanglingPointerDetectorTransformFree(p);
#endif

#ifdef CRYMM_SUPPORT_DEADLIST

	if (CCryMemoryManager::s_sys_MemoryDeadListSize > 0)
	{
		size_t size = 0;

		UINT_PTR pid = (UINT_PTR)p;

		if (p != NULL)
		{
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

			if (g_GlobPageBucketAllocator.IsInAddressRange(p))
			{
				size = g_GlobPageBucketAllocator.getSizeEx(p);
				DeadListPush(p, size);
			}
			else
			{
				if (alignment)
				{
					uint8* pb = static_cast<uint8*>(p);
					pb -= reinterpret_cast<uint32*>(pb)[-1];
					size = CrySystemCrtSize(pb);
					DeadListPush(pb, size);
				}
				else
				{
					size = CrySystemCrtSize(p);
					DeadListPush(p, size);
				}
			}

			LONG lsize = size;
			CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, -lsize);

			MEMREPLAY_SCOPE_FREE(pid);
		}

		return size;
	}
#endif

	size_t size = 0;

	UINT_PTR pid = (UINT_PTR)p;

	if (p != NULL)
	{
		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		if (g_GlobPageBucketAllocator.IsInAddressRange(p))
		{
			size = g_GlobPageBucketAllocator.deallocate(p);
		}
		else
		{
			if (alignment)
			{
				uint8* pb = static_cast<uint8*>(p);
				pb -= reinterpret_cast<uint32*>(pb)[-1];
				size = CrySystemCrtFree(pb);
			}
			else
			{
				size = CrySystemCrtFree(p);
			}
		}

		LONG lsize = size;
		CryInterlockedExchangeAdd(&g_TotalAllocatedMemory, -lsize);

		MEMREPLAY_SCOPE_FREE(pid);
	}

	return size;
}

CRYMEMORYMANAGER_API void CryFlushAll()  // releases/resets ALL memory... this is useful for restarting the game
{
	g_TotalAllocatedMemory = 0;
};

//////////////////////////////////////////////////////////////////////////
// Returns amount of memory allocated with CryMalloc/CryFree functions.
//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryMemoryGetAllocatedSize()
{
	return g_TotalAllocatedMemory;
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryMemoryGetPoolSize()
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CRYMEMORYMANAGER_API int CryStats(char* buf)
{
	return 0;
}

CRYMEMORYMANAGER_API int CryGetUsedHeapSize()
{
	return g_GlobPageBucketAllocator.get_heap_size();
}

CRYMEMORYMANAGER_API int CryGetWastedHeapSize()
{
	return g_GlobPageBucketAllocator.get_wasted_in_allocation();
}

CRYMEMORYMANAGER_API void CryCleanup()
{
	g_GlobPageBucketAllocator.cleanup();
}

#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
void EnableDynamicBucketCleanups(bool enable)
{
	g_GlobPageBucketAllocator.EnableExpandCleanups(enable);
}
void BucketAllocatorReplayRegisterAddressRange(const char* name)
{
	#if CAPTURE_REPLAY_LOG
	g_GlobPageBucketAllocator.ReplayRegisterAddressRange(name);
	#endif //CAPTURE_REPLAY_LOG
}
#endif //defined(USE_GLOBAL_BUCKET_ALLOCATOR)

#if CRY_PLATFORM_ORBIS
	#define CRT_IS_DLMALLOC
#endif

#ifdef CRT_IS_DLMALLOC
	#include <CryCore/Platform/CryDLMalloc.h>

static dlmspace s_dlHeap = 0;
static CryCriticalSection s_dlMallocLock;

static void* crt_dlmmap_handler(void* user, size_t sz)
{
	void* base = VirtualAllocator::AllocateVirtualAddressSpace(sz);
	if (!base)
		return dlmmap_error;

	if (!VirtualAllocator::MapPageBlock(base, sz, PAGE_SIZE))
		return dlmmap_error;

	return base;
}

static int crt_dlmunmap_handler(void* user, void* mem, size_t sz)
{
	VirtualAllocator::FreeVirtualAddressSpace(mem, sz);
	return 0;
}

static bool CrySystemCrtInitialiseHeap(void)
{
	size_t heapSize = 256 * 1024 * 1024; // 256MB
	s_dlHeap = dlcreate_mspace(heapSize, 0, NULL, crt_dlmmap_handler, crt_dlmunmap_handler);

	if (s_dlHeap)
	{
		return true;
	}

	__debugbreak();
	return false;
}
#endif

CRYMEMORYMANAGER_API void* CrySystemCrtMalloc(size_t size)
{
	void* ret = NULL;
#ifdef CRT_IS_DLMALLOC
	AUTO_LOCK(s_dlMallocLock);
	if (!s_dlHeap)
		CrySystemCrtInitialiseHeap();
	return dlmspace_malloc(s_dlHeap, size);
#elif CRY_PLATFORM_ORBIS || CRY_PLATFORM_ANDROID || (defined(NOT_USE_CRY_MEMORY_MANAGER) && (CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX))
	size_t* allocSize = (size_t*)malloc(size + 2 * sizeof(size_t));
	*allocSize = size;
	ret = (void*)(allocSize + 2);
#else
	ret = malloc(size);
#endif
	return ret;
}

CRYMEMORYMANAGER_API void* CrySystemCrtRealloc(void* p, size_t size)
{
	void* ret;
#ifdef CRT_IS_DLMALLOC
	AUTO_LOCK(s_dlMallocLock);
	if (!s_dlHeap)
		CrySystemCrtInitialiseHeap();
	#ifndef _RELEASE
	if (p && !VirtualAllocator::InAllocatedSpace(p))
		__debugbreak();
	#endif
	return dlmspace_realloc(s_dlHeap, p, size);
#elif CRY_PLATFORM_ORBIS || CRY_PLATFORM_ANDROID || (defined(NOT_USE_CRY_MEMORY_MANAGER) && (CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX))
	size_t* origPtr = (size_t*)p;
	size_t* newPtr = (size_t*)realloc(origPtr - 2, size + 2 * sizeof(size_t));
	*newPtr = size;
	ret = (void*)(newPtr + 2);
#else
	ret = realloc(p, size);
#endif
	return ret;
}

CRYMEMORYMANAGER_API size_t CrySystemCrtFree(void* p)
{
	size_t n = 0;
#ifdef CRT_IS_DLMALLOC
	AUTO_LOCK(s_dlMallocLock);
	if (p)
	{
	#ifndef _RELEASE
		if (!VirtualAllocator::InAllocatedSpace(p))
			__debugbreak();
	#endif
		const size_t size = dlmspace_usable_size(p);
		dlmspace_free(s_dlHeap, p);
		return size;
	}
	return 0;
#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	n = _msize(p);
#elif CRY_PLATFORM_ORBIS || CRY_PLATFORM_ANDROID || (defined(NOT_USE_CRY_MEMORY_MANAGER) && (CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX))
	size_t* ptr = (size_t*)p;
	ptr -= 2;
	n = *ptr;
	p = (void*)ptr;
#elif CRY_PLATFORM_APPLE
	n = malloc_size(p);
#else
	n = malloc_usable_size(p);
#endif
	free(p);
	return n;
}

CRYMEMORYMANAGER_API size_t CrySystemCrtSize(void* p)
{
#ifdef CRT_IS_DLMALLOC
	AUTO_LOCK(s_dlMallocLock);
	return dlmspace_usable_size(p);
#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	return _msize(p);
#elif CRY_PLATFORM_ORBIS || CRY_PLATFORM_ANDROID || (defined(NOT_USE_CRY_MEMORY_MANAGER) && (CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX))
	size_t* ptr = (size_t*)p;
	return *(ptr - 2);
#elif CRY_PLATFORM_APPLE
	return malloc_size(p);
#else
	return malloc_usable_size(p);
#endif
}

size_t CrySystemCrtGetUsedSpace()
{
	size_t used = 0;
#if defined(USE_GLOBAL_BUCKET_ALLOCATOR)
	used += g_GlobPageBucketAllocator.GetBucketConsumedSize();
#endif
#ifdef CRT_IS_DLMALLOC
	{
		AUTO_LOCK(s_dlMallocLock);
		if (s_dlHeap)
		{
			used += dlmspace_get_used_space(s_dlHeap);
		}
	}
#endif
	return used;
}

CRYMEMORYMANAGER_API void CryResetStats(void)
{
}

int GetPageBucketAlloc_wasted_in_allocation()
{
	return g_GlobPageBucketAllocator.get_wasted_in_allocation();
}

int GetPageBucketAlloc_get_free()
{
	return g_GlobPageBucketAllocator._S_get_free();
}
