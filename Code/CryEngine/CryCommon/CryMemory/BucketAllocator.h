// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef BUCKETALLOCATOR_H
#define BUCKETALLOCATOR_H

#ifdef USE_GLOBAL_BUCKET_ALLOCATOR

	#if CRY_PLATFORM_DURANGO
		#include <CryCore/Assert/CryAssert.h>
	#endif

	#ifndef _RELEASE
		#define BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
		#define BUCKET_ALLOCATOR_TRAP_FREELIST_TRAMPLING
		#define BUCKET_ALLOCATOR_FILL_ALLOCS
		#define BUCKET_ALLOCATOR_CHECK_DEALLOCATE_ADDRESS
		#define BUCKET_ALLOCATOR_TRACK_CONSUMED
		#define BUCKET_ALLOCATOR_TRAP_BAD_SIZE_ALLOCS
		#define BUCKET_ALLOCATOR_TRAP_CLEANUP_OOM
	#endif

	#define BUCKET_ALLOCATOR_DEBUG 0

	#if BUCKET_ALLOCATOR_DEBUG
		#define BucketAssert(expr) if (!(expr)) { __debugbreak(); }
	#else
		#define BucketAssert(expr)
	#endif

	#include "BucketAllocatorPolicy.h"

namespace BucketAllocatorDetail
{
struct SystemAllocator
{
	class CleanupAllocator
	{
	public:
		CleanupAllocator(size_t reserveCapacity);
		~CleanupAllocator();

		bool  IsValid() const;

		void* Calloc(size_t num, size_t sz);
		void  Free(void* ptr);


	private:
		CleanupAllocator(const CleanupAllocator&) = delete;
		CleanupAllocator& operator=(const CleanupAllocator&) = delete;

	private:
		void* m_base;
		void* m_end;
		const size_t m_reserveCapacity;
	};

	static UINT_PTR ReserveAddressSpace(size_t numPages, size_t pageLen);
	static void     UnreserveAddressSpace(UINT_PTR base, size_t numPages, size_t pageLen);

	static UINT_PTR Map(UINT_PTR base, size_t len);
	static void     UnMap(UINT_PTR addr);
};
}

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		#define BUCKET_ALLOCATOR_DEFAULT_SIZE (256 * 1024 * 1024)
	#else
		#define BUCKET_ALLOCATOR_DEFAULT_SIZE (128 * 1024 * 1024)
	#endif

template<typename TraitsT = BucketAllocatorDetail::DefaultTraits<BUCKET_ALLOCATOR_DEFAULT_SIZE, BucketAllocatorDetail::SyncPolicyLocked>>
class BucketAllocator :
	private BucketAllocatorDetail::SystemAllocator,
	private TraitsT::SyncPolicy
{
	typedef typename TraitsT::SyncPolicy SyncingPolicy;

public:
	enum
	{
		MaxSize      = TraitsT::MaxSize,
		MaxAlignment = TraitsT::MaxSize,
	};

public:

	BucketAllocator() {}
	explicit BucketAllocator(void* baseAddress, bool allowExpandCleanups = true, bool cleanupOnDestruction = false);
	~BucketAllocator();

public:
	void* allocate(size_t sz)
	{
		void* ptr = NULL;

		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		if (TraitsT::FallbackOnCRTAllowed && (sz > MaxSize))
		{
	#ifdef BUCKET_SIMULATOR
			ptr = malloc(sz);
	#else
			ptr = CryCrtMalloc(sz);
	#endif
		}
		else
		{
			ptr = AllocateFromBucket(sz);
		}

		MEMREPLAY_SCOPE_ALLOC(ptr, sz, 0);

		return ptr;
	}

	bool CanGuaranteeAlignment(size_t sz, size_t align)
	{
		if (sz > MaxSize)
			return false;

		if (((SmallBlockLength % sz) == 0) && (sz % align) == 0)
			return true;

		if (((sz % 16) == 0) && (align <= 16))
			return true;

		if (((sz % 8) == 0) && (align <= 8))
			return true;

		return false;
	}

	void* allocate(size_t sz, size_t align)
	{
		void* ptr = NULL;

		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		if ((sz > MaxSize) || (SmallBlockLength % align))
		{
			if (TraitsT::FallbackOnCRTAllowed)
			{
				ptr = CryModuleMemalign(sz, align);
			}
		}
		else
		{
			ptr = AllocateFromBucket(sz);
		}

		MEMREPLAY_SCOPE_ALLOC(ptr, sz, 0);
		return ptr;
	}

	ILINE void* alloc(size_t sz)
	{
		return allocate(sz);
	}

	size_t deallocate(void* ptr)
	{
		using namespace BucketAllocatorDetail;

		MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);

		size_t sz = 0;

		if (this->IsInAddressRange(ptr))
		{
	#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
			AllocHeader* hdr = reinterpret_cast<AllocHeader*>(ptr);
			if ((hdr->magic == FreeListMagic) && ((hdr->next == NULL) || this->IsInAddressRange(hdr->next)))
			{
				// If this fires, chances are this is a double delete.
				__debugbreak();
			}

			hdr->magic = FreeListMagic;
	#endif

			UINT_PTR uptr = reinterpret_cast<UINT_PTR>(ptr);
			UINT_PTR bgAlign = uptr & PageAlignMask;
			Page* page = reinterpret_cast<Page*>(bgAlign);

			size_t index = (uptr & PageOffsetMask) / SmallBlockLength;
			uint8 bucket = page->hdr.GetBucketId(index);
			uint8 stability = page->hdr.GetStability(index);
			size_t generation = TraitsT::GetGenerationForStability(stability);

			sz = TraitsT::GetSizeForBucket(bucket);

	#ifdef BUCKET_ALLOCATOR_CHECK_DEALLOCATE_ADDRESS
			{
				UINT_PTR sbOffset = uptr - ((uptr & SmallBlockAlignMask) + page->hdr.GetBaseOffset(index));
				if (sbOffset % sz)
				{
					// If this fires, the pointer being deleted hasn't been returned from an allocate, or is a double delete
					// (and the small block has been recycled).
					__debugbreak();
				}
			}
	#endif

	#ifdef BUCKET_ALLOCATOR_FILL_ALLOCS
			if (ptr)
			{
				memset(reinterpret_cast<AllocHeader*>(ptr) + 1, AllocFillMagic + 1, sz - sizeof(AllocHeader));
			}
	#endif // BUCKET_ALLOCATOR_FILL_ALLOCS

	#ifdef BUCKET_ALLOCATOR_TRACK_CONSUMED
			CryInterlockedAdd(&m_consumed, -(int)sz);
	#endif

			this->PushOnto(m_freeLists[bucket * NumGenerations + generation], reinterpret_cast<AllocHeader*>(ptr));
			m_bucketTouched[bucket] = 1;
		}
		else if (TraitsT::FallbackOnCRTAllowed)
		{
	#ifdef BUCKET_SIMULATOR
			free(ptr);
			sz = 0;
	#else
			sz = CryCrtFree(ptr);
	#endif
		}

		MEMREPLAY_SCOPE_FREE(ptr);

		return sz;
	}

	ILINE size_t dealloc(void* ptr)
	{
		return deallocate(ptr);
	}

	ILINE size_t dealloc(void* ptr, size_t sz)
	{
		(void) sz;
		return deallocate(ptr);
	}

	ILINE bool IsInAddressRange(void* ptr)
	{
		UINT_PTR ptri = reinterpret_cast<UINT_PTR>(ptr);
		SegmentHot* pSegments = m_segmentsHot;

		for (int i = 0, c = m_numSegments; i != c; ++i)
		{
			UINT_PTR ba = pSegments[i].m_baseAddress;
			if ((ptri - ba) < NumPages * PageLength)
				return true;
		}
		return false;
	}

	size_t getSizeEx(void* ptr)
	{
		if (this->IsInAddressRange(ptr))
		{
			return GetSizeInternal(ptr);
		}
		else
		{
			return 0;
		}
	}

	size_t getSize(void* ptr)
	{
		size_t sz = getSizeEx(ptr);
		if (!sz)
			sz = CryCrtSize(ptr);
		return sz;
	}

	size_t getHeapSize()
	{
		return this->GetBucketStorageSize();
	}

	size_t GetBucketStorageSize();
	size_t GetBucketStoragePages();
	size_t GetBucketConsumedSize();

	void   cleanup();

	void   EnableExpandCleanups(bool enable)
	{
		m_disableExpandCleanups = !enable;

	#if CAPTURE_REPLAY_LOG
		SegmentHot* pSegments = m_segmentsHot;
		for (int i = 0, c = m_numSegments; i != c; ++i)
			CryGetIMemReplay()->BucketEnableCleanups(reinterpret_cast<void*>(pSegments[i].m_baseAddress), enable);
	#endif
	}

	ILINE size_t get_heap_size()            { return getHeapSize(); }
	ILINE size_t get_wasted_in_blocks()     { return 0; }
	ILINE size_t get_wasted_in_allocation() { return 0; }
	ILINE size_t _S_get_free()              { return 0; }

	#if CAPTURE_REPLAY_LOG
	void ReplayRegisterAddressRange(const char* name)
	{
		SegmentHot* pSegments = m_segmentsHot;
		for (int i = 0, c = m_numSegments; i != c; ++i)
			CryGetIMemReplay()->RegisterFixedAddressRange(reinterpret_cast<void*>(pSegments[i].m_baseAddress), NumPages * PageLength, name);
	}
	#endif

private:
	enum
	{
		NumPages           = TraitsT::NumPages,
		NumBuckets         = TraitsT::NumBuckets,

		PageLength         = TraitsT::PageLength,
		SmallBlockLength   = TraitsT::SmallBlockLength,
		SmallBlocksPerPage = TraitsT::SmallBlocksPerPage,

		NumGenerations     = TraitsT::NumGenerations,
		MaxSegments        = TraitsT::MaxNumSegments,

		AllocFillMagic     = 0xde,
	};

	static const UINT_PTR SmallBlockAlignMask = ~(SmallBlockLength - 1);
	static const UINT_PTR PageAlignMask = ~(PageLength - 1);
	static const UINT_PTR SmallBlockOffsetMask = SmallBlockLength - 1;
	static const UINT_PTR PageOffsetMask = PageLength - 1;
	#if CRY_PLATFORM_64BIT
	static const UINT_PTR FreeListMagic = 0x63f9ab2df2ee1157;
	#else
	static const UINT_PTR FreeListMagic = 0xf2ee1157;
	#endif

	struct PageSBHot
	{
		uint8 bucketId;
		uint8 stability;
	};

	struct PageHeader
	{
		PageSBHot   smallBlocks[SmallBlocksPerPage];
		uint8       smallBlockBaseOffsets[SmallBlocksPerPage];

		ILINE uint8 GetBucketId(size_t sbId)
		{
			return smallBlocks[sbId].bucketId & 0x7f;
		}

		ILINE void SetBucketId(size_t sbId, uint8 id, bool spills)
		{
			smallBlocks[sbId].bucketId = id | (spills ? 0x80 : 0x00);
		}

		ILINE bool DoesSmallBlockSpill(size_t sbId)
		{
			return (smallBlocks[sbId].bucketId & 0x80) != 0;
		}

		ILINE void SetBaseOffset(size_t sbId, size_t offs)
		{
			BucketAssert(sbId < sizeof(smallBlockBaseOffsets));
			BucketAssert(offs < 1024);
			BucketAssert((offs & 0x3) == 0);
			smallBlockBaseOffsets[sbId] = static_cast<uint8>(offs / 4);
		}

		ILINE size_t GetBaseOffset(size_t sbId)
		{
			return smallBlockBaseOffsets[sbId] * 4;
		}

		ILINE size_t GetItemCountForBlock(size_t sbId)
		{
			size_t sbOffset = GetBaseOffset(sbId);
			size_t itemSize = TraitsT::GetSizeForBucket(GetBucketId(sbId));
			size_t basicCount = (SmallBlockLength - sbOffset) / itemSize;

			if (DoesSmallBlockSpill(sbId))
				++basicCount;

			return basicCount;
		}

		ILINE void ResetBlockStability(size_t sbId)
		{
			smallBlocks[sbId].stability = 0;
		}

		ILINE void IncrementBlockStability(size_t sbId)
		{
			smallBlocks[sbId].stability = min(smallBlocks[sbId].stability + 1, 0xff);
		}

		ILINE uint8 GetStability(size_t sbId)
		{
			return smallBlocks[sbId].stability;
		}
	};

	union Page
	{
		PageHeader hdr;
		struct
		{
			uint8 data[SmallBlockLength];
		} smallBlocks[SmallBlocksPerPage]; //!< The first small block overlaps the header.
	};

	struct FreeBlockHeader
	{
		FreeBlockHeader* prev;
		FreeBlockHeader* next;
		UINT_PTR         start;
		UINT_PTR         end;
	};

	struct SmallBlockCleanupInfo
	{
		uint32 itemSize;
		uint32 freeItemCount : 31;
		uint32 cleaned       : 1;

		void   SetItemSize(uint32 sz) { itemSize = sz; }
		uint32 GetItemSize() const    { return itemSize; }
	};

	struct SegmentHot
	{
		UINT_PTR m_baseAddress;
	};

	struct SegmentCold
	{
		UINT_PTR m_committed;
		uint32   m_pageMap[(NumPages + 31) / 32];
		uint32   m_lastPageMapped;
	};

private:
	BucketAllocatorDetail::AllocHeader* AllocateFromBucket(size_t sz)
	{
		using namespace BucketAllocatorDetail;

	#ifdef BUCKET_ALLOCATOR_TRAP_BAD_SIZE_ALLOCS
		if ((sz == 0) || (sz > MaxSize))
			__debugbreak();
	#endif

		uint8 bucket = TraitsT::GetBucketForSize(sz);

		AllocHeader* ptr = NULL;

		do
		{
			for (int fl = bucket * NumGenerations, flEnd = fl + NumGenerations; !ptr && fl != flEnd; ++fl)
				ptr = this->PopOff(m_freeLists[fl]);
		}
		while (!ptr && Refill(bucket));

	#ifdef BUCKET_ALLOCATOR_TRAP_FREELIST_TRAMPLING
		if (ptr)
		{
			if (ptr->next && (!this->IsInAddressRange(ptr->next) || this->GetBucketInternal(ptr->next) != bucket))
			{
				// If this fires, something has trampled the free list and caused the next pointer to point
				// either outside the allocator's address range, or into another bucket. Either way, baaad.
				__debugbreak();

		#if CAPTURE_REPLAY_LOG
				CryGetIMemReplay()->Stop();
		#endif // CAPTURE_REPLAY_LOG
			}

		#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
			if (ptr->magic != FreeListMagic)
			{
				// If this fires, something has trampled the free list items.
				__debugbreak();

			#if CAPTURE_REPLAY_LOG
				CryGetIMemReplay()->Stop();
			#endif // CAPTURE_REPLAY_LOG
			}
		#endif // BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES

		}
	#endif // BUCKET_ALLOCATOR_TRAP_FREELIST_TRAMPLING

	#ifdef BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES
		if (ptr)
		{
			reinterpret_cast<AllocHeader*>(ptr)->magic = 0;
		}
	#endif // BUCKET_ALLOCATOR_TRAP_DOUBLE_DELETES

	#ifdef BUCKET_ALLOCATOR_FILL_ALLOCS
		if (ptr)
		{
			memset(ptr, AllocFillMagic, TraitsT::GetSizeForBucket(TraitsT::GetBucketForSize(sz)));
		}
	#endif // BUCKET_ALLOCATOR_FILL_ALLOCS

	#ifdef BUCKET_ALLOCATOR_TRACK_CONSUMED
		CryInterlockedAdd(&m_consumed, TraitsT::GetSizeForBucket(TraitsT::GetBucketForSize(sz)));
	#endif // BUCKET_ALLOCATOR_TRACK_CONSUMED

		return ptr;
	}

	FreeBlockHeader* InsertFreeBlock(FreeBlockHeader* after, UINT_PTR start, UINT_PTR end)
	{
		bool isFreeBlockDone = (start & SmallBlockAlignMask) == (end & SmallBlockAlignMask) || (start > end - (SmallBlockLength / 2));

		if (isFreeBlockDone)
			return after;

		FreeBlockHeader* fbh = reinterpret_cast<FreeBlockHeader*>(start);

		fbh->start = start;
		fbh->end = end;
		if (after)
		{
			fbh->prev = after;
			fbh->next = after->next;
			if (after->next)
				after->next->prev = fbh;
			after->next = fbh;
		}
		else
		{
			fbh->prev = NULL;
			fbh->next = m_freeBlocks;
			m_freeBlocks = fbh;
		}

	#if CAPTURE_REPLAY_LOG
		CryGetIMemReplay()->MarkBucket(-3, 4, fbh, fbh->end - fbh->start);
	#endif

		return fbh;
	}

	static uint8 GetBucketInternal(void* ptr)
	{
		UINT_PTR uptr = reinterpret_cast<UINT_PTR>(ptr);
		UINT_PTR smAlign = uptr & SmallBlockAlignMask;
		UINT_PTR bgAlign = uptr & PageAlignMask;
		Page* page = reinterpret_cast<Page*>(bgAlign);

		size_t index = (smAlign - bgAlign) / SmallBlockLength;
		return page->hdr.GetBucketId(index);
	}

	static size_t GetSizeInternal(void* ptr)
	{
		return TraitsT::GetSizeForBucket(GetBucketInternal(ptr));
	}

	static ILINE int GetSegmentForAddress(void* ptr, const UINT_PTR* segmentBases, int numSegments)
	{
		int segIdx = 0;
		UINT_PTR segBaseAddress = 0;
		for (; segIdx != numSegments; ++segIdx)
		{
			segBaseAddress = segmentBases[segIdx];
			if ((reinterpret_cast<UINT_PTR>(ptr) - segBaseAddress) < NumPages * PageLength)
				break;
		}

		return segIdx;
	}

	static size_t GetRefillItemCountForBucket_Spill(UINT_PTR start, size_t itemSize)
	{
		size_t sbOffset = start & SmallBlockOffsetMask;
		size_t basicCount = (SmallBlockLength - sbOffset) / itemSize;
		if (((sbOffset + basicCount * itemSize) & SmallBlockOffsetMask) != 0)
			++basicCount;

		return basicCount;
	}

	static size_t GetRefillItemCountForBucket_LCM(size_t itemSize)
	{
		int lcm = (itemSize * SmallBlockLength) / GreatestCommonDenominator((int) itemSize, SmallBlockLength);
		return lcm / itemSize;
	}

	static int GreatestCommonDenominator(int a, int b)
	{
		while (b)
		{
			size_t c = a;
			a = b;
			b = c - b * (c / b);
		}
		return a;
	}

private:
	FreeBlockHeader* CreatePage(FreeBlockHeader* after);
	bool             DestroyPage(Page* page);

	bool             Refill(uint8 bucket);
	FreeBlockHeader* FindFreeBlock(bool useForward, UINT_PTR alignmentMask, size_t itemSize, size_t& numItems);

	void             CleanupInternal(bool sortFreeLists);
	static void      FreeCleanupInfo(SystemAllocator::CleanupAllocator& alloc, SmallBlockCleanupInfo** infos, size_t infoCapacity);

private:
	void* AllocatePageStorage();
	bool  DeallocatePageStorage(void* ptr);

private:
	typename SyncingPolicy::FreeListHeader m_freeLists[NumBuckets * NumGenerations];
	volatile int              m_bucketTouched[NumBuckets];

	volatile int              m_numSegments;
	SegmentHot                m_segmentsHot[MaxSegments];

	FreeBlockHeader* volatile m_freeBlocks;
	SegmentCold               m_segmentsCold[MaxSegments];

	#ifdef BUCKET_ALLOCATOR_TRACK_CONSUMED
	volatile int m_consumed;
	#endif

	int m_disableExpandCleanups;
	int m_cleanupOnDestruction;
};

#else
// if node allocator is used instead of global bucket allocator, windows.h is required
	#include <CryCore/Platform/CryWindows.h>
#endif

#endif
