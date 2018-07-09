// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MEMENTOMEMORYMANAGER_H__
#define __MEMENTOMEMORYMANAGER_H__

#pragma once

#include <CryMemory/STLPoolAllocator.h>
#include "Config.h"

#ifdef USE_GLOBAL_BUCKET_ALLOCATOR
	#define MMM_USE_BUCKET_ALLOCATOR            1
	#define MMM_BUCKET_ALLOCATOR_SIZE           (4 * 1024 * 1024)
	#define LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK 0
#else
	#define MMM_USE_BUCKET_ALLOCATOR            0
#endif

#if MMM_USE_BUCKET_ALLOCATOR
	#include <CryMemory/BucketAllocator.h>
#endif

#define MMM_GENERAL_HEAP_SIZE           (1024 * 1024)

struct IMementoManagedThing
{
	virtual ~IMementoManagedThing(){}
	virtual void Release() = 0;
};

#define MMM_MUTEX_ENABLE (0)

// handle based memory manager that can repack data to save fragmentation
// TODO: re-introduce repacking for handles (but not pointers)
class CMementoMemoryManager : public CMultiThreadRefCount
{
	friend class CMementoStreamAllocator;

public:
	// hack for arithmetic alphabet stuff
	uint32                arith_zeroSizeHdl;
	IMementoManagedThing* pThings[64];

	// who's using pThings:
	//   0 - arith row sym cache
	//   1 - arith row low cache

	CMementoMemoryManager(const string& name);
	~CMementoMemoryManager();

	typedef uint32 Hdl;
	static const Hdl InvalidHdl = ~Hdl(0);

	void*        AllocPtr(size_t sz, void* callerOverride = 0);
	void         FreePtr(void* p, size_t sz);
	Hdl          AllocHdl(size_t sz, void* callerOverride = 0);
	Hdl          CloneHdl(Hdl hdl);
	void         ResizeHdl(Hdl hdl, size_t sz);
	void         FreeHdl(Hdl hdl);
	void         AddHdlToSizer(Hdl hdl, ICrySizer* pSizer);
	ILINE void*  PinHdl(Hdl hdl) const     { return CMementoMemoryManagerAllocator::GetAllocator()->PinHdl(hdl); }
	ILINE size_t GetHdlSize(Hdl hdl) const { return CMementoMemoryManagerAllocator::GetAllocator()->GetHdlSize(hdl); }

	void         GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false);

	static void  DebugDraw();
	static void  Tick();

private:
	class CMementoMemoryManagerAllocator
	{
		struct SPoolStats
		{
			SPoolStats() : allocated(0), used(0) {}
			size_t allocated;
			size_t used;

			float  GetWastePercent() const
			{
				return allocated ? 100.0f * (1.0f - float(used) / float(allocated)) : 0.0f;
			}
		};

#if MMM_USE_BUCKET_ALLOCATOR
		typedef BucketAllocator<BucketAllocatorDetail::DefaultTraits<MMM_BUCKET_ALLOCATOR_SIZE, BucketAllocatorDetail::SyncPolicyUnlocked, false>> MMMBuckets;

		static MMMBuckets m_bucketAllocator;
		size_t            m_bucketTotalRequested;
		size_t            m_bucketTotalAllocated;

		struct SHandleData
		{
			void*  p;
			size_t size;
			size_t capacity;
		};

#else
		static const size_t ALIGNMENT = 8; // alignment for mementos

		// pool sizes are 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
		static const int FIRST_POOL = 3; // == 8bytes
		static const int LAST_POOL = 12; // == 4096bytes
		static const int NPOOLS = LAST_POOL - FIRST_POOL + 1;

		struct SFreeListHeader
		{
			SFreeListHeader* pNext;
		};

		SFreeListHeader m_freeList[NPOOLS];
		int             m_numAllocated[NPOOLS];
		int             m_numFree[NPOOLS];
		stl::PoolAllocator<4096, stl::PoolAllocatorSynchronizationSinglethreaded, ALIGNMENT> m_pool;

		struct SHandleData
		{
			void*  p;
			size_t size;
			size_t capacity;
		};
#endif

#if !defined(PURE_CLIENT)
		IGeneralMemoryHeap* m_pGeneralHeap;
		size_t              m_generalHeapTotalRequested;
		size_t              m_generalHeapTotalAllocated;
#endif

	public:
		CMementoMemoryManagerAllocator();
		~CMementoMemoryManagerAllocator();

		static CMementoMemoryManagerAllocator* GetAllocator() { return m_allocator; }
		static void                            AddCMementoMemoryManager();
		static void                            RemoveCMementoMemoryManager();

		void                                   Tick();
		Hdl                                    AllocHdl(size_t sz);
		void                                   FreeHdl(Hdl hdl);
		void*                                  AllocPtr(size_t sz);
		void                                   FreePtr(void* p, size_t sz);
		void                                   ResizeHdl(Hdl hdl, size_t sz);
		void                                   InitHandleData(SHandleData& hd, size_t sz);
		void                                   DebugDraw(int x, int& y, size_t& totalAllocated);
		ILINE void*                            PinHdl(Hdl hdl) const
		{
			hdl = UnprotectHdl(hdl);
			return (hdl != InvalidHdl) ? m_handles[hdl].p : NULL;
		}
		ILINE size_t GetHdlSize(Hdl hdl) const
		{
			hdl = UnprotectHdl(hdl);
			return (hdl != InvalidHdl) ? m_handles[hdl].size : 0;
		}

#if LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK
		size_t m_bucketHighWaterMark;
		size_t m_generalHeapHighWaterMark;
#endif // LOG_BUCKET_ALLOCATOR_HIGH_WATERMARK

	private:
		ILINE Hdl ProtectHdl(Hdl x) const
		{
#if !CRY_PLATFORM_APPLE && !(CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) && !CRY_PLATFORM_ORBIS && CRYNETWORK_RELEASEBUILD
			if (x != InvalidHdl)
			{
				return (x << 1) ^ ((uint32)UINT_PTR(this) + 1);     // ensures 0xFFFFFFFF cannot be a valid result (this will always be at least 4 byte aligned)
			}
#endif
			return x;
		}

		ILINE Hdl UnprotectHdl(Hdl x) const
		{
#if !CRY_PLATFORM_APPLE && !(CRY_PLATFORM_LINUX && CRY_PLATFORM_64BIT) && !CRY_PLATFORM_ORBIS && CRYNETWORK_RELEASEBUILD
			if (x != InvalidHdl)
			{
				return (x ^ ((uint32)UINT_PTR(this) + 1)) >> 1;
			}
#endif
			return x;
		}

		std::vector<SHandleData>               m_handles;
		std::vector<uint32>                    m_freeHandles;

		static CMementoMemoryManagerAllocator* m_allocator;
		static int                             m_numCMementoMemoryManagers;
#if MMM_MUTEX_ENABLE
		static CryLockT<CRYLOCK_RECURSIVE>     m_mutex;
#endif
	};

	size_t m_totalAllocations;
	string m_name;

#if MMM_CHECK_LEAKS
	std::map<void*, void*>  m_ptrToAlloc;
	std::map<uint32, void*> m_hdlToAlloc;
	std::map<void*, uint32> m_allocAmt;
#endif

#if ENABLE_NETWORK_MEM_INFO
	typedef std::list<CMementoMemoryManager*> TManagers;
	static TManagers* m_pManagers;
#endif
};

typedef CMementoMemoryManager::Hdl TMemHdl;
const TMemHdl TMemInvalidHdl = CMementoMemoryManager::InvalidHdl;

typedef _smart_ptr<CMementoMemoryManager> CMementoMemoryManagerPtr;

class CMementoStreamAllocator : public IStreamAllocator
{
public:
	CMementoStreamAllocator(const CMementoMemoryManagerPtr& mmm);

	void*   Alloc(size_t sz, void* callerOverride);
	void*   Realloc(void* old, size_t sz);
	void    Free(void* old); // WARNING: no-op (calling code uses GetHdl() to grab this...)

	TMemHdl GetHdl() const { return m_hdl; }

private:
	TMemHdl                  m_hdl;
	void*                    m_pPin;
	CMementoMemoryManagerPtr m_mmm;
};

#endif
