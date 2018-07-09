// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuHeap.h"
#include <type_traits>
#include <utility>
#include <vector>

// Possible optimizations/extensions:
// - On 64-bit systems, also pack UPage within 16 bytes, which should significantly improve cache-hit chances of structures on the CPU
//   To achieve this, instead of taking a void* handle, we should pack mapped address + handle into one 64-bit field (ie, assume 4K aligned mapped addresses, and allow max 4K handles)
//   This would affect the API to be implemented by the platform layer, but not the client API.
// - In case we run out of heap structures, it's possible to maintain 64K pages per memory-type instead of globally.
//   This should reduce the contention on structures significantly.
// - Add support for defragmentation, something like the below could work:
//   Currently, we can already walk the heap, it should be possible to instead just find all shared-huge-pages and determine their load-factor.
//   Alternatively, we could keep track of this inside the huge-page structure perhaps.
//   Then, given some max number of bytes to be defragged per pass, we sum the loaded-bytes of the lowest available shared-huge-pages until we hit the max.
//   Then, mark the huge-pages as "not available for allocation", duplicate all the allocations in the marked pages (into unmarked pages)
//   Then, return the mapping "old handle->new handle" for all the blocks, and the set of marked-pages.
//   The platform must then issue copies from old-handle to new-handle, and all resources referring to old-handle must be updated to refer to new-handle instead.
//   Then, we deallocate all the old handles (which implies the huge pages are freed to the platform), and we are done.
//   If we can generate this copy-command-list between frames, there should be no hazardous accesses possible if we submit the command-list between the matching frames command-lists.
//   However, the resource APIs must have some way to identify the resource to which a handle belongs, and trigger rebuild of descriptors of those after defragmenting is done.
//   This implementation can be layered above CGpuHeap (ie, CDefragGpuHeap) so we can have both implementations available.

// If set, sub-allocation is enabled (should be on).
// For testing, you can turn this off, causing all allocations to be passed to the API directly.
#define HEAP_SUBALLOCATE 1

// Must be a power of two, directly limits the sub-allocated heap size.
// The implementation is designed to manage ~100K regions of this size (ie, ~13GB currently)
// For small devices, it could be worth trying to halve this, and for huge devices double this.
#define HEAP_REGION_SIZE (128U * 1024U)

// Must be set to 0 or 1, trades off the quantum size.
// 0: The largest small bin contains HEAP_REGION_SIZE * 4, the quantum size is HEAP_REGION_SIZE / 512
// 1: The largest small bin contains HEAP_REGION_SIZE * 3, the quantum size is HEAP_REGION_SIZE / 1024
// Basically, set to 0 unless there is lots of allocations <= HEAP_REGION_SIZE / 1024, as you will get HEAP_REGION_SIZE * 4 binned (ie, exert less CPU page pressure)
#define HEAP_DISTRIBUTION_SMALL 0

// Must be set to 0 or 1, trades off committed overhead vs heap structure usage.
// 0: Small blocks are always packed with 32 sub-blocks, uses least amount of heap structures.
// 1: Small blocks are packed based on block-size, reducing committed memory overhead.
#define HEAP_DYNAMIC_SMALL_BLOCKS 1

// Must be set to 0 or 1
// 0: Heaps allocate meta-data storage in ctor and free in dtor. This supports more than one heap instance.
// 1: Only one heap instance is supported. The heap uses static duration storage.
#ifdef CRY_PLATFORM_ORBIS
	#define HEAP_STATIC_STORAGE 1
#else
	#define HEAP_STATIC_STORAGE 0
#endif

// If set, MemReplay markers will be generated for GPU heap.
#ifndef _RELEASE
	#define HEAP_MEMREPLAY 1
#else
	#define HEAP_MEMREPLAY 0
#endif

// Heap checking/debugging can be enabled or disabled here.
#if defined(_DEBUG)
	#define HEAP_CHECK 1
	#define HEAP_ASSERT(x) CRY_ASSERT(x)
#else
	#define HEAP_CHECK 0
	#define HEAP_ASSERT(x)
#endif

namespace Detail
{
// Main heap properties
enum EBlockConfig : uint32_t
{
	// General limits
	kMaxMemoryTypes = 32U,               // The maximum number of memory types that can be managed.
	kPageIdBits     = 16U,               // The number of bits per page-id.
	kMaxPages       = 1U << kPageIdBits, // The number of addressable pages in the heap.
	kRegionSize     = HEAP_REGION_SIZE,  // Size of each region

	// Set up the bin counts.
	kTinyBlockBins  = 8U,
	kSmallBlockBins = 16U,
	kInternalBins   = 2U,
	kLargeBlockBins = 25U - (1U - HEAP_DISTRIBUTION_SMALL),

	// Configures the distribution of bin sizes in the tiny and small block ranges.
	// The number of bins and distribution lengths determine how many quanta are selectable in the bins.
	kDistribution1 = 4U + HEAP_DISTRIBUTION_SMALL, // Must be in range [2, 5]; 4 or 5 seem good.
	kDistribution2 = 12U,                          // Must be a multiple of 2, larger values focus distribution towards small bins.
};

// For a given bin index, find the minimum size of bin at compile-time (in quanta).
// The binning algorithm is based on splitting each power-of-two step in size into 4 bins, placed at 25% of the range.
// For example, the sub-part of the range is as follows: ..., 16 (a base power of two), 20 (+25%), 24 (+50%), 28 (+75%), 32 (next power of two), ...
// To limit the number of bins, the full range is split into 3 sub-ranges (dist-1, dist-2 and dist-4), the length of which is defined above.
// - dist-1: Keep 1 bin (the base), discard the 25%, 50% and 75% bins.
// - dist-2: Keep 2 bins (base and 50%), discard the 25% and 75% bins.
// - dist-4: Keep all 4 bins.
// This is used to discard non-integral bins at the very low values (ie, 1+50% and 2+75%), as well as ensure we get reasonable range out of our limited bucket count.
// We intentionally limit ourselves to 8 + 16 + 24 bins (defined above) to ensure we can pack our allocation keys into 32-bit values.
template<uint32_t B>
struct SBinSize
{
	enum EHelpers : uint32_t
	{
		kIsBinDist1 = B < kDistribution1,
		kIsBinDist2 = !kIsBinDist1 && B < (kDistribution1 + kDistribution2),
		kIsBinDist4 = !kIsBinDist1 && !kIsBinDist2,
		kIndex1     = kIsBinDist1 ? B : kDistribution1 - 1U,
		kIndex2     = kIsBinDist2 ? B - kIndex1 : kIsBinDist1 ? 0U : kDistribution2,
		kIndex4     = kIsBinDist4 ? B - kIndex1 - kIndex2 : 0U,
		kSize1      = 1U << kIndex1,
		kSize2      = (kSize1 << (kIndex2 / 2U)),
		kAdd2       = (kSize2 / 2U) * (kIndex2 & 1U),
		kSize4      = (kSize2 + kAdd2) << (kIndex4 / 4U),
		kAdd4       = (kSize4 / 4U) * (kIndex4 & 3U),
		kValue      = (kSize4 + kAdd4),
	};
};

// Compile time log2 (rounds up)
template<uint32_t kValue, uint32_t kResult = 0U>
struct SLog2 : SLog2<(kValue + 1U) / 2U, kResult + 1U> {};
template<uint32_t kResult>
struct SLog2<1U, kResult>
{
	static const uint32_t kValue = kResult;
};

// Page implementation type.
enum EPageType
{
	kPageTypeHuge,
	kPageTypeLarge,
	kPageTypeSmall,
	kPageTypeTiny,
	kPageTypeCount
};

// Page index type, making this larger than 16bit will blow all the packing.
// If this has to change at some point, re-evaluate all the structs below!
typedef uint16_t TPageId;
static_assert(sizeof(TPageId) * 8 >= kPageIdBits, "Too many page ID bits requested");

// Handle type
struct SHandle
{
	static const uint32_t kBlockStateBits = sizeof(CGpuHeap::THandle) * 8U - SLog2<kPageTypeCount>::kValue - SLog2<kMaxMemoryTypes>::kValue - kPageIdBits;

	CGpuHeap::THandle     pageIndex: kPageIdBits;                     // The index of the parent heap block
	CGpuHeap::THandle     pageType: SLog2<kPageTypeCount>::kValue;    // Page type (see EPageType)
	CGpuHeap::THandle     memoryType: SLog2<kMaxMemoryTypes>::kValue; // Memory type index [0, kMaxMemoryTypes)
	CGpuHeap::THandle     blockState: kBlockStateBits;                // Block state (meaning depends on page type)
};
static_assert(sizeof(CGpuHeap::THandle) == sizeof(SHandle), "Bad handle size, more than one word required for packing");

// Create internal handle type
static SHandle MakeInternalHandle(CGpuHeap::THandle handle)
{
	union
	{
		SHandle           internalHandle;
		CGpuHeap::THandle externalHandle;
	} helper;
	helper.externalHandle = handle;
	return helper.internalHandle;
}

// Create external handle type
static CGpuHeap::THandle MakeExternalHandle(TPageId pageIndex, EPageType pageType, uint32_t memoryType, uint32_t blockState)
{
	union
	{
		SHandle           internalHandle;
		CGpuHeap::THandle externalHandle;
	} helper;
	helper.internalHandle.pageIndex = pageIndex;
	helper.internalHandle.pageType = pageType;
	helper.internalHandle.memoryType = memoryType;
	helper.internalHandle.blockState = blockState;
	return helper.externalHandle;
}

// Double-linked list node in a circular list.
struct SLink
{
	TPageId prev;
	TPageId next;
};

// Contains intrusive links for N lists.
template<uint32_t kLinkCount_>
struct SLinkedPage
{
	static const uint32_t kLinkCount = kLinkCount_;
	SLink                 link[kLinkCount];
};

// Page maps directly to the API heap.
// Link used for heap-walking of huge blocks only.
struct SHugePage : SLinkedPage<1>
{
	static const uint32_t kAlignShift = 12;

	uint16_t              mapped;    // Number of times mapped
	uint16_t              size;      // Block size in large regions (note: shared huge-pages are always 256 regions large)
#if CRY_PLATFORM_32BIT
	size_t                apiHandle; // Handle from API level
	size_t                address;   // CPU accessible address (only set when mapped != 0)
#else
	size_t                apiHandle: kAlignShift;
	size_t                address: (64U - kAlignShift);
#endif

	void* GetAddress() const
	{
		return reinterpret_cast<void*>(static_cast<size_t>(address) << kAlignShift);
	}

	bool SetAddress(void* pAddress)
	{
		const uintptr_t bits = reinterpret_cast<size_t>(pAddress);
		if ((bits & ((1U << kAlignShift) - 1U)) == 0)
		{
			address = static_cast<size_t>(bits >> kAlignShift);
			return true;
		}
		else
		{
			address = 0U;
			return false;
		}
	}

	uint32_t GetHandle() const
	{
		return static_cast<uint32_t>(apiHandle) + 1U;
	}

	bool SetHandle(uint32_t handle)
	{
		HEAP_ASSERT(handle);
		if (handle <= (1U << kAlignShift))
		{
			apiHandle = static_cast<size_t>(handle - 1U);
			return true;
		}
		else
		{
			apiHandle = 0U;
			return false;
		}
	}
};

// Sub-allocates a huge-page with variable-size ranges, consisting of large regions.
// Each huge page is used as one block consisting of 256 large-blocks.
// Contains 2 links (first for bin-lists, second for address-coalescing).
struct SLargePage : SLinkedPage<2>
{
	struct SRange
	{
		uint8_t b; // First large region index in the range
		uint8_t e; // One-past-past large region index in the range
	};

	TPageId parentPage; // Parent page index (huge, shared)
	SRange  range;      // Range of the page within the huge-page

	void Init(TPageId hugePage)
	{
		parentPage = hugePage;
		HEAP_ASSERT((link[0].prev = link[0].next) == 0U);
	}

	bool IsFree() const
	{
		HEAP_ASSERT(range.b != range.e);
		return range.e > range.b;
	}

	uint32_t GetRegionCount(bool bFree) const
	{
		HEAP_ASSERT(bFree == IsFree());
		return bFree ? range.e - range.b : range.b - range.e;
	}

	uint32_t GetBegin(bool bFree) const
	{
		HEAP_ASSERT(bFree == IsFree());
		return (bFree ? range.b : range.e) + (link[1].prev != 0);
	}

	uint32_t GetEnd(bool bFree) const
	{
		HEAP_ASSERT(bFree == IsFree());
		return (bFree ? range.e : range.b) + (link[1].prev != 0);
	}

	void MarkBusy()
	{
		HEAP_ASSERT(IsFree());
		std::swap(range.b, range.e);
	}

	void MarkFree()
	{
		HEAP_ASSERT(!IsFree());
		std::swap(range.b, range.e);
	}
};
static_assert(SHandle::kBlockStateBits >= SLog2<kLargeBlockBins>::kValue, "Block state too small");

// Sub-allocates a huge page sub-range with up to 32 small blocks of fixed size.
// The whole sub-block behaves just like a huge page.
struct SSmallPage : SLargePage
{
	static const uint32_t kBlockCount = 32U;
	static const uint32_t kBlockBits = SLog2<kBlockCount>::kValue;

	uint32_t              freeBlocks;

	void ExtendLarge(uint32_t numBlocks)
	{
		HEAP_ASSERT(parentPage != 0);
		HEAP_ASSERT(numBlocks <= kBlockCount);
		freeBlocks = (numBlocks == kBlockCount ? ~0U : (1U << numBlocks) - 1U);
	}

	uint32_t Allocate()
	{
		HEAP_ASSERT(freeBlocks != 0U);
		const uint32_t blockIndex = countTrailingZeros32(freeBlocks);
		freeBlocks ^= 1U << blockIndex;
		return blockIndex;
	}

	void Deallocate(uint32_t blockIndex)
	{
		HEAP_ASSERT(blockIndex < kBlockCount && (freeBlocks & (1U << blockIndex)) == 0U);
		freeBlocks ^= 1U << blockIndex;
	}

	bool IsFull() const
	{
		return freeBlocks == 0U;
	}

	bool IsEmpty(uint32_t numBlocks) const
	{
		HEAP_ASSERT(numBlocks <= kBlockCount);
		return freeBlocks == (numBlocks == kBlockCount ? ~0U : (1U << numBlocks) - 1U);
	}

	bool IsAllocated(uint32_t blockIndex) const
	{
		HEAP_ASSERT(blockIndex <= kBlockCount);
		return (freeBlocks & (1U << blockIndex)) == 0U;
	}
};

// Sub-allocates another binned block (ie, SSmallPage) into exactly 64 tiny blocks.
// This is needed only when the total page size is too small to be tracked as a SSmallPage, at the cost of another meta-page and an indirection.
struct STinyPage : SLinkedPage<1U>
{
	static const uint32_t kBlockCount = 64U;
	static const uint32_t kBlockBits = SLog2<kBlockCount>::kValue;

	SHandle               parentBlock;
#if CRY_PLATFORM_64BIT
	uint64_t              freeBlocks;
#else
	uint32_t              freeBlocks[2];
#endif

	void Init(SHandle parent)
	{
		parentBlock = parent;
#if CRY_PLATFORM_64BIT
		freeBlocks = ~0ULL;
#else
		freeBlocks[0] = ~0U;
		freeBlocks[1] = ~0U;
#endif
	}

	uint32_t Allocate()
	{
#if CRY_PLATFORM_64BIT
		HEAP_ASSERT(freeBlocks != 0U);
		const uint32_t blockIndex = static_cast<uint32_t>(countTrailingZeros64(freeBlocks));
		freeBlocks ^= 1ULL << blockIndex;
		return blockIndex;
#else
		if (freeBlocks[0U] != 0U)
		{
			const uint32_t blockIndex = countTrailingZeros32(freeBlocks[0U]);
			freeBlocks[0U] ^= 1U << blockIndex;
			return blockIndex;
		}

		HEAP_ASSERT(freeBlocks[1] != 0U);
		const uint32_t blockIndex = countTrailingZeros32(freeBlocks[1U]);
		freeBlocks[1U] ^= 1U << blockIndex;
		return blockIndex + 32U;
#endif
	}

	void Deallocate(uint32_t blockIndex)
	{
		HEAP_ASSERT(blockIndex < kBlockCount);
#if CRY_PLATFORM_64BIT
		HEAP_ASSERT((freeBlocks & (1ULL << blockIndex)) == 0U);
		freeBlocks ^= 1ULL << blockIndex;
#else
		if (blockIndex < kBlockCount / 2U)
		{
			HEAP_ASSERT((freeBlocks[0U] & (1U << blockIndex)) == 0U);
			freeBlocks[0U] ^= 1U << blockIndex;
		}
		else
		{
			HEAP_ASSERT((freeBlocks[1] & (1U << blockIndex - 32U)) == 0U);
			freeBlocks[1U] ^= 1U << (blockIndex - kBlockCount / 2U);
		}
#endif
	}

	bool IsFull() const
	{
#if CRY_PLATFORM_64BIT
		return freeBlocks == 0ULL;
#else
		return (freeBlocks[0U] == 0U) && (freeBlocks[1U] == 0U);
#endif
	}

	bool IsEmpty() const
	{
#if CRY_PLATFORM_64BIT
		return freeBlocks == ~0ULL;
#else
		return freeBlocks[0] == ~0U && freeBlocks[1] == ~0U;
#endif
	}

	bool IsAllocated(uint32_t blockIndex) const
	{
		HEAP_ASSERT(blockIndex < kBlockCount);
#if CRY_PLATFORM_64BIT
		return (freeBlocks & (1ULL << blockIndex)) == 0U;
#else
		if (blockIndex < kBlockCount / 2U)
		{
			return (freeBlocks[0U] & (1U << blockIndex)) == 0U;
		}
		else
		{
			return (freeBlocks[1] & (1U << blockIndex - 32U)) == 0U;
		}
#endif
	}
};

enum EComputedConfig : uint32_t
{
	// The size of the quantum of allocation depends on how many bins we have before the first large bin.
	// This quantum size scales directly with HEAP_REGION_SIZE and is also affected as described by HEAP_DISTRIBUTION_SMALL.
	kMaxSmallQuanta = SBinSize<kTinyBlockBins + kSmallBlockBins - 1U>::kValue,
	kQuantumSize    = (kRegionSize * 4U) / (1U << SLog2<kMaxSmallQuanta>::kValue),

	// The sizes of the first and last bins by category, in bytes.
	kTinyBlockFirst  = SBinSize<0U>::kValue * kQuantumSize,
	kTinyBlockLast   = SBinSize<kTinyBlockBins - 1U>::kValue * kQuantumSize,
	kSmallBlockFirst = SBinSize<kTinyBlockBins>::kValue * kQuantumSize,
	kSmallBlockLast  = SBinSize<kTinyBlockBins + kSmallBlockBins - 1U>::kValue * kQuantumSize,
	kLargeBlockFirst = SBinSize<kTinyBlockBins + kSmallBlockBins>::kValue * kQuantumSize,
	kLargeBlockLast  = SBinSize<kTinyBlockBins + kSmallBlockBins + kLargeBlockBins - 1U>::kValue * kQuantumSize,
	kHugeBlockFirst  = kRegionSize * 128U,

	// The minimum number of regions that can be tracked in one large allocation.
	kMinRegions = SBinSize<kTinyBlockBins>::kValue * kQuantumSize * SSmallPage::kBlockCount / kRegionSize,
};

// Check that the block state can be packed into the handle for the given values
static_assert(SLog2<kTinyBlockBins>::kValue + STinyPage::kBlockBits <= SHandle::kBlockStateBits, "Too many tiny bins");
static_assert(SLog2<kSmallBlockBins>::kValue + SSmallPage::kBlockBits <= SHandle::kBlockStateBits, "Too many small bins");
static_assert(SLog2<256U>::kValue <= SHandle::kBlockStateBits, "Too many large bins");

// The last large bin is supposed to contain the [224-256) regions list, no matter the configuration.
static_assert(kLargeBlockLast == 256 * HEAP_REGION_SIZE, "Distribution error");

// Check that the internal bin count is correct.
static_assert(SBinSize<kTinyBlockBins + 0>::kValue * kQuantumSize * SSmallPage::kBlockCount < kLargeBlockFirst || kInternalBins <= 0, "Incorrect internal bin count");
static_assert(SBinSize<kTinyBlockBins + 1>::kValue * kQuantumSize * SSmallPage::kBlockCount < kLargeBlockFirst || kInternalBins <= 1, "Incorrect internal bin count");
static_assert(SBinSize<kTinyBlockBins + 2>::kValue * kQuantumSize * SSmallPage::kBlockCount < kLargeBlockFirst || kInternalBins <= 2, "Incorrect internal bin count");
static_assert(SBinSize<kTinyBlockBins + 3>::kValue * kQuantumSize * SSmallPage::kBlockCount < kLargeBlockFirst || kInternalBins <= 3, "Incorrect internal bin count");
static_assert(kInternalBins <= 4, "Incorrect internal bin count");

// SHandle contains the 2 bits necessary to disambiguate the page type.
union UPage
{
	SLinkedPage<1> freePage;
	STinyPage      tinyPage;
	SSmallPage     smallPage;
	SLargePage     largePage;
	SHugePage      hugePage;
};
static_assert(sizeof(UPage) == 16U, "All page types must be store-able in 16B");

// Returns the ID of a page on the heap.
// Note: The page may not refer to an aliased root node.
template<uint32_t N>
static TPageId GetPageId(const UPage* pPages, const SLinkedPage<N>* pPage)
{
	const size_t result = reinterpret_cast<const UPage*>(pPage) - pPages;
	HEAP_ASSERT(((char*)pPage - (char*)pPages) % sizeof(UPage) == 0 && result < kMaxPages);
	return static_cast<TPageId>(result);
}

// Helper for linked-list root of pages.
template<typename TPageType, uint32_t kLinkIndex = 0>
class CLinkRoot : SLink
{
	static const uint32_t kLinkCount = TPageType::kLinkCount;
	static_assert(std::is_base_of<SLinkedPage<kLinkCount>, TPageType>::value, "PageType must contain kLinkCount links");
	static_assert(kLinkIndex < kLinkCount, "kLinkIndex must be smaller than kLinkCount");

	// Aliases the root node such that it's storage overlaps the SLinkedPage's kLinkIndex'th link value.
	// Only the kLinkIndex'th link may be accessed using this alias!
	TPageType& Alias()
	{
		return *reinterpret_cast<TPageType*>(static_cast<SLink*>(this) - kLinkIndex);
	}
	const TPageType& Alias() const
	{
		return *reinterpret_cast<const TPageType*>(static_cast<const SLink*>(this) - kLinkIndex);
	}

	// Resolve a link-index in this list.
	template<bool bAllowRoot>
	TPageType* Resolve(UPage* pPages, TPageId id)
	{
		return id ? reinterpret_cast<TPageType*>(pPages + id) : bAllowRoot ? &Alias() : nullptr;
	}
	template<bool bAllowRoot>
	const TPageType* Resolve(const UPage* pPages, TPageId id) const
	{
		return id ? reinterpret_cast<const TPageType*>(pPages + id) : bAllowRoot ? &Alias() : nullptr;
	}

public:
	// Initialize the root
	void Init(TPageId front, TPageId back)
	{
		TPageType& alias = Alias();
		alias.link[kLinkIndex].prev = back;
		alias.link[kLinkIndex].next = front;
	}

	// Removes the first item from this list, and returns a pointer to the removed item.
	TPageType* PopFront(UPage* pPages)
	{
		TPageType& alias = Alias();
		const TPageId resultId = alias.link[kLinkIndex].next;
		TPageType* const pResult = Resolve<false>(pPages, resultId);
		if (pResult)
		{
			const TPageId nextId = pResult->link[kLinkIndex].next;
			TPageType& next = *Resolve<true>(pPages, nextId);
			HEAP_ASSERT(pResult->link[kLinkIndex].prev == 0U);   // First item must point back to root
			HEAP_ASSERT(next.link[kLinkIndex].prev == resultId); // Next node must point back to first item
			alias.link[kLinkIndex].next = nextId;
			next.link[kLinkIndex].prev = 0U;
			HEAP_ASSERT((pResult->link[kLinkIndex].prev = pResult->link[kLinkIndex].next = 0U) == 0U); // Clear links (for assert on next insert)
		}
		return pResult;
	}

	// Adds an item to the front of the list.
	void PushFront(UPage* pPages, TPageType* pPage)
	{
		const TPageId selfId = GetPageId(pPages, pPage);
		HEAP_ASSERT(pPage && pPage->link[kLinkIndex].prev == 0U && pPage->link[kLinkIndex].next == 0U); // Item must not be linked already
		TPageType& alias = Alias();
		const TPageId prevFrontId = alias.link[kLinkIndex].next;
		TPageType& prevFront = *Resolve<true>(pPages, prevFrontId);
		HEAP_ASSERT(prevFront.link[kLinkIndex].prev == 0U); // First item must point back to root
		alias.link[kLinkIndex].next = selfId;
		prevFront.link[kLinkIndex].prev = selfId;
		pPage->link[kLinkIndex].prev = 0U;
		pPage->link[kLinkIndex].next = prevFrontId;
	}

	// Adds an item behind another item.
	void InsertAfter(UPage* pPages, TPageType* pPage, TPageType& after)
	{
		const TPageId selfId = GetPageId(pPages, pPage);
		HEAP_ASSERT(pPage && pPage->link[kLinkIndex].prev == 0U && pPage->link[kLinkIndex].next == 0U); // Item must not be linked already
		const TPageId afterId = GetPageId(pPages, &after);
		const TPageId beforeId = after.link[kLinkIndex].next;
		TPageType& before = *Resolve<true>(pPages, beforeId);
		after.link[kLinkIndex].next = selfId;
		before.link[kLinkIndex].prev = selfId;
		pPage->link[kLinkIndex].next = beforeId;
		pPage->link[kLinkIndex].prev = afterId;
	}

	// Unlinks the given node from the current list.
	void Unlink(UPage* pPages, TPageType* pPage)
	{
		HEAP_ASSERT(pPage && Contains(pPages, pPage));
		TPageType& alias = Alias();
		const TPageId prevId = pPage->link[kLinkIndex].prev;
		const TPageId nextId = pPage->link[kLinkIndex].next;
		TPageType& prev = *Resolve<true>(pPages, prevId);
		TPageType& next = *Resolve<true>(pPages, nextId);
		HEAP_ASSERT(prev.link[kLinkIndex].next == GetPageId(pPages, pPage)); // Next must point back to current
		HEAP_ASSERT(next.link[kLinkIndex].prev == GetPageId(pPages, pPage)); // Prev must point forward to current
		prev.link[kLinkIndex].next = nextId;
		next.link[kLinkIndex].prev = prevId;
		HEAP_ASSERT((pPage->link[kLinkIndex].prev = pPage->link[kLinkIndex].next = 0U) == 0U); // Clear links (for assert on next insert)
	}

	// Walk all the items in the list, and calls the functor with the item.
	// The functor signature must be compatible with bool functor(TPageType&).
	// Enumeration is terminated when the functor returns false, and that page is returned.
	// If enumeration is never terminated by returning false (ie, completed or empty), nullptr is returned.
	template<typename TFunctor>
	TPageType* Walk(UPage* pPages, TFunctor&& functor)
	{
		const TPageId begin = Alias().link[kLinkIndex].next;
		const TPageId end = 0U;
		TPageId prev = 0U;
		for (TPageId it = begin; it != end; )
		{
			TPageType* pPage = Resolve<false>(pPages, it);
			HEAP_ASSERT(pPage && (pPage->link[kLinkIndex].prev == prev || !prev));
			if (!functor(*pPage))
			{
				return pPage;
			}
			prev = it;
			it = pPage->link[kLinkIndex].next;
		}
		return nullptr;
	}
	template<typename TFunctor>
	const TPageType* Walk(const UPage* pPages, TFunctor&& functor) const
	{
		const TPageId begin = Alias().link[kLinkIndex].next;
		const TPageId end = 0U;
		TPageId prev = 0U;
		for (TPageId it = begin; it != end; )
		{
			const TPageType* pPage = Resolve<false>(pPages, it);
			HEAP_ASSERT(pPage && (pPage->link[kLinkIndex].prev == prev || !prev));
			if (!functor(*pPage))
			{
				return pPage;
			}
			prev = it;
			it = pPage->link[kLinkIndex].next;
		}
		return nullptr;
	}

	// Tests if the list contains the specified item.
	bool Contains(const UPage* pPages, const TPageType* pPage) const
	{
		bool bResult = false;
		Walk(pPages, [&bResult, pPage](const TPageType& page) { bResult |= (&page == pPage); return !bResult; });
		return bResult;
	}

	// Returns a pointer to the first item in the list, or nullptr if the list is empty.
	TPageType* AtFront(UPage* pPages)
	{
		const TPageId resultId = Alias().link[kLinkIndex].next;
		return Resolve<false>(pPages, resultId);
	}
};

// Selects the bin in which the allocation of the given size would be stored.
// The returned value must be adjusted for a page type to be used as an index.
static uint32_t SelectBin(uint32_t bytes)
{
	static_assert(kDistribution1 > 1U && kDistribution2 % 2U == 0U, "Bad tiny/small distribution");
	const uint32_t quanta = (bytes + kQuantumSize - 1U) >> SLog2<kQuantumSize>::kValue;
	if (quanta <= 1U)
	{
		// Handle zero and one quantum case directly.
		// These would otherwise underflow or over-shift in the logic below.
		return 0;
	}

	// Find the size category of the request, where we split each range in-between subsequent power-of-two's into 4 equal-sized ranges.
	// Requests are rounded up to the next larger available range.
	// sizeCategory[31:2] is the power-of-two needed to satisfy the request.
	// sizeCategory[1:0] is the index into which the range falls.
	const uint32_t leadingZeroes = countLeadingZeros32(quanta);
	const uint32_t group = 31U - leadingZeroes;
	uint32_t sizeCategory;
	if (leadingZeroes < 29U)
	{
		// More than 8 quanta
		// Extract the 3 most significant bits (the leading one is assumed one, the lower two are stored in the lowest two bits of sizeCategory)
		const uint32_t rounding = 0xFFFFFFFFU >> (leadingZeroes + 3U);
		const uint32_t msb = (quanta + rounding) >> (group - 2U);
		sizeCategory = ((group + (msb == 8U)) << 2U) + (msb & 3U);
	}
	else
	{
		// Less than 8 quanta.
		// No need to perform rounding, but we have to construct fictional lower bits, as they can't be shifted.
		const uint32_t msb = (quanta > 3U ? quanta & 3U : (quanta == 3U) << 1U);
		sizeCategory = (group << 2U) + msb;
	}

	// Map the category onto the bin ranges.
	// The total range of tiny and small bins is split over 3 sub-ranges of different distributions.
	// The first range discards 2 bits of computed index, the second 1 bit, and the last no bits.
	const uint32_t binIndex = sizeCategory - 1U;
	const uint32_t numBins4 = (kDistribution1 - 1U) * 4U; // 4 sub-ranges per bin, -1 for the single quantum case handled at the top.
	const uint32_t numBins2 = kDistribution2 * 2U;        // 2 sub-ranges per bin
	if (binIndex < numBins4)
	{
		// First range
		return (binIndex >> 2U) + 1U;
	}
	else if (binIndex < numBins4 + numBins2)
	{
		// Second range
		return ((binIndex - (numBins4 - kDistribution1 - kDistribution1)) >> 1U);
	}
	else
	{
		// Third range
		return binIndex - (numBins4 + numBins2 - kDistribution1 - kDistribution2);
	}
}

// Selects the large bin (which may be internal) for a given number of regions.
static uint32_t SelectLargeBin(uint32_t regions)
{
	HEAP_ASSERT(regions > 0U && regions < 256U);
	const uint32_t bin = SelectBin(regions * kRegionSize);
	const uint32_t largeBin = bin - (kTinyBlockBins + kSmallBlockBins);
	if (largeBin > kLargeBlockBins + kInternalBins)
	{
		const uint32_t internalBin = (largeBin + 4U) >> 1U;
		HEAP_ASSERT(internalBin < kInternalBins);
		return internalBin;
	}
	else
	{
		return largeBin + kInternalBins;
	}
}

// Gets the size of the given bin, in bytes.
static uint32_t GetBinSize(uint32_t bin)
{
	// TODO: Optimize this!
	const bool bIsBinDist1 = bin < kDistribution1;
	const bool bIsBinDist2 = !bIsBinDist1 && bin < (kDistribution1 + kDistribution2);
	const bool bIsBinDist4 = !bIsBinDist1 && !bIsBinDist2;
	const uint32_t index1 = bIsBinDist1 ? bin : kDistribution1 - 1U;
	const uint32_t index2 = bIsBinDist2 ? bin - index1 : bIsBinDist1 ? 0U : kDistribution2;
	const uint32_t index4 = bIsBinDist4 ? bin - index1 - index2 : 0U;
	const uint32_t size1 = 1U << index1;
	const uint32_t size2 = (size1 << (index2 / 2U));
	const uint32_t add2 = (size2 / 2U) * (index2 & 1U);
	const uint32_t size4 = (size2 + add2) << (index4 / 4U);
	const uint32_t add4 = (size4 / 4U) * (index4 & 3U);
	return (size4 + add4) * kQuantumSize;
}

// Selects the number of small blocks for a small bin (return in range [2, 32]).
// The theoretical maximum overhead in small bins is SUM(N:0->kSmallBlockBins, (SelectSmallBinSize(N)-1)*GetBinSize(N))
// Thus, by selecting lower values for higher bin indices will result in less overhead.
static uint32_t SelectSmallBinSize(uint32_t bin)
{
#if HEAP_DYNAMIC_SMALL_BLOCKS
	const uint32_t binSize = GetBinSize(bin + kTinyBlockBins);
	const uint32_t binQuanta = binSize >> SLog2<kQuantumSize>::kValue;
	const uint32_t kRegionQuanta = kRegionSize / kQuantumSize;

	// Find an N, such that (binQuanta * N) % regionQuanta == 0.
	// This is required to avoid "waste" bytes in the regions.
	// Blocks before 1/2-region size are at most 12 regions large, which is acceptable.
	if (binQuanta >= (kRegionQuanta >> 1U))
	{
		// Because of how the distribution works out, we know that (binQuanta * 4 * N) % regionQuanta == 0 for any N.
		// Therefore, as long as we select a multiple of 4 we meet the requirement.
		// We target the whole block at either 8 or 12 regions, where possible.
		const uint32_t kTargetQuanta = 8U * kRegionQuanta;
		const uint32_t result = ((kTargetQuanta / binQuanta) + 3U) & ~3U;
		HEAP_ASSERT(binQuanta * result % kRegionQuanta == 0U && result > 0U && result <= 32U);
		return result;
	}
#endif
	return 32U;
}

struct SSubHeap
{
	static const uint32_t kLargeLists = kLargeBlockBins + kInternalBins;
	CLinkRoot<STinyPage>  tinyAlloc[kTinyBlockBins];     // List of blocks to visit for allocation.
	CLinkRoot<STinyPage>  tinyFull;                      // Tiny pages that are full.
	CLinkRoot<SSmallPage> smallAlloc[kSmallBlockBins];   // Small pages in lists per bin.
	CLinkRoot<SSmallPage> smallFull[kSmallBlockBins];    // Small pages that are full. Must be binned for accurate heap-walk :(
	CLinkRoot<SLargePage> largeAlloc[kLargeLists];       // List of large blocks grouped by minimum available size.
	CLinkRoot<SLargePage> largeFull;                     // List of large blocks that are full, in same grouping (required for enumeration).
	CLinkRoot<SHugePage>  hugeBlocks;                    // List of all huge blocks on this heap (for heap-walking / clean-up purposes only).
};

template<typename TPageType>
static TPageType* AllocatePage(UPage* pPages)
{
	return reinterpret_cast<CLinkRoot<TPageType>*>(&pPages->freePage)->PopFront(pPages);
}

template<typename TPageType>
static void DeallocatePage(UPage* pPages, TPageType* pPage)
{
	reinterpret_cast<CLinkRoot<TPageType>*>(&pPages->freePage)->PushFront(pPages, pPage);
}

// This is for NatVis only, it's never read by code.
// Assuming you have only one GpuHeap instance this will allow NatVis to follow links in GpuHeap structures.
static UPage* gpHeapPages;

#if HEAP_STATIC_STORAGE
// Static duration storage for the meta-data
static UPage gPageStorage[kMaxPages];
static SSubHeap gHeapStorage[kMaxMemoryTypes];
#endif

#if HEAP_MEMREPLAY
// Generates a (potentially fictional) address for MemReplay allocation/deallocation events.
void MakeMemReplayEvent(const SHugePage& page, uint32_t offset, bool bMapPersistent, bool bAllocate, uint32_t bytes, uint32_t align)
{
	if (bMapPersistent)
	{
		void* pPersistent = page.GetAddress();
		if (pPersistent)
		{
			const size_t address = reinterpret_cast<size_t>(pPersistent) + offset;
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			if (bAllocate)
			{
				MEMREPLAY_SCOPE_ALLOC(address, bytes, align);
			}
			else
			{
				MEMREPLAY_SCOPE_FREE(address);
			}
		}
	}

	#if CRY_PLATFORM_32BIT
	// Generate addresses in the top of the address space.
	const size_t fictionalBase = 0xC0000000U;

	// We treat every quantum as a single byte to enlarge our available "fictional" space.
	// This means that MemReplay will display tiny allocations.
	bytes = (bytes + kQuantumSize + 1) / kQuantumSize;
	align = (align + kQuantumSize + 1) / kQuantumSize;
	offset /= kQuantumSize;

	#else
	// Generate addresses in the top of the address space.
	const size_t fictionalBase = 0xC000000000000000ULL;
	#endif

	// Construct a (hopefully unique) fictional address.
	const size_t fictionalMask = ~fictionalBase;
	const size_t blockOffset = (fictionalMask + 1U) / (1U << SHugePage::kAlignShift);
	const size_t result = page.apiHandle * blockOffset + offset;
	const size_t address = (result & fictionalMask) | fictionalBase;

	// Fictional event
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	if (bAllocate)
	{
		MEMREPLAY_SCOPE_ALLOC(address, bytes, align);
	}
	else
	{
		MEMREPLAY_SCOPE_FREE(address);
	}
}
#endif
}

using namespace Detail;

CGpuHeap::CGpuHeap(uint32_t numMemoryTypes, bool bMapPersistent, bool bCommitRegions)
	: m_bMapPersistent(bMapPersistent)
	, m_bCommitRegions(bCommitRegions)
	, m_numHeaps(numMemoryTypes)
	, m_pPages(nullptr)
	, m_pHeaps(nullptr)
{
	if (numMemoryTypes <= kMaxMemoryTypes && numMemoryTypes != 0)
	{
#if HEAP_STATIC_STORAGE
		HEAP_ASSERT(gpHeapPages == nullptr && "Only one heap may be instantiated when HEAP_STATIC_STORAGE is set");
		m_pHeaps = gHeapStorage;
		m_pPages = gPageStorage;
	#if HEAP_CHECK
		gpHeapPages = gPageStorage;
	#endif
#else
		m_pHeaps = new SSubHeap[numMemoryTypes];
		m_pPages = new UPage[kMaxPages];

		::memset(m_pHeaps, 0, sizeof(SSubHeap) * numMemoryTypes);
	#if HEAP_CHECK
		::memset(m_pPages, 0, sizeof(UPage) * kMaxPages);
		gpHeapPages = m_pPages;
	#endif
#endif

		for (uint32_t i = 1U; i < kMaxPages; ++i)
		{
			DeallocatePage(m_pPages, &m_pPages[i].freePage);
		}
	}
}

CGpuHeap::~CGpuHeap()
{
	// When using committed regions, we must free all the blocks separately, since we have to reconstruct the large-block frees accurately.
	const bool bFreeAllHandles = HEAP_MEMREPLAY || HEAP_CHECK || m_bCommitRegions;
	if (bFreeAllHandles)
	{
		const auto pfnFree = [](void* pContext, THandle handle, uint32_t memoryType, TBlockHandle blockHandle, uint32_t blockOffset, uint32_t allocationSize, void* pMappedAddress)
		{
			CGpuHeap* const pThis = static_cast<CGpuHeap*>(pContext);
			pThis->DeallocateInternal(handle);
		};
		Walk(this, nullptr, pfnFree);
	}
	else
	{
		for (uint32_t i = 0; i < m_numHeaps; ++i)
		{
			m_pHeaps[i].hugeBlocks.Walk(m_pPages, [this, i](SHugePage& page) -> bool
			{
				if (page.mapped)
				{
				  void* const pAddress = page.GetAddress();
				  if (pAddress)
				  {
				    UnmapBlock(i, page.GetHandle(), pAddress);
				  }
				}
				DeallocateBlock(i, page.GetHandle(), page.size * kRegionSize);
				return true;
			});
		}
	}

#if !HEAP_STATIC_STORAGE
	delete[] m_pHeaps;
	delete[] m_pPages;
#else
	::memset(m_pHeaps, 0, sizeof(SSubHeap) * m_numHeaps);
	::memset(m_pPages, 0, sizeof(UPage) * kMaxPages);
#endif

#if HEAP_CHECK
	gpHeapPages = nullptr;
#endif
}

CGpuHeap::THandle CGpuHeap::Allocate(uint32_t memoryType, uint32_t bytes, uint32_t align)
{
	HEAP_ASSERT(memoryType < kMaxMemoryTypes);
	bytes = (bytes + align - 1) & ~(align - 1); // All blocks are a multiple of their alignment
	const uint32_t bin = SelectBin(bytes);
	HEAP_ASSERT(GetBinSize(bin) >= bytes && (!bin || GetBinSize(bin - 1U) < bytes));

	CryAutoCriticalSectionNoRecursive lock(m_lock);
	return AllocateInternal(memoryType, bytes, align, bin);
}

CGpuHeap::THandle CGpuHeap::AllocateInternal(uint32_t memoryType, uint32_t bytes, uint32_t align, uint32_t bin)
{
	THandle result = 0U;
	if (bin == ~0U)
	{
		HEAP_ASSERT(memoryType < kMaxMemoryTypes);
		bytes = (bytes + align - 1) & ~(align - 1); // All blocks are a multiple of their alignment
		bin = SelectBin(bytes);
		HEAP_ASSERT(GetBinSize(bin) >= bytes && (!bin || GetBinSize(bin - 1U) < bytes));
	}

	if (align <= kRegionSize)
	{
#if HEAP_SUBALLOCATE
		if (bin < kTinyBlockBins)
		{
			result = AllocateTiny(memoryType, bin);
		}
		else if (bin < kTinyBlockBins + kSmallBlockBins)
		{
			result = AllocateSmall(memoryType, bin - kTinyBlockBins);
		}
		else if (bin < kTinyBlockBins + kSmallBlockBins + kLargeBlockBins - kInternalBins)
		{
			result = AllocateLarge(memoryType, bin - kTinyBlockBins - kSmallBlockBins + kInternalBins, bytes, true);
		}
#endif
	}
	if (!result)
	{
		result = AllocateHuge(memoryType, bytes, align, true);
	}

#if HEAP_MEMREPLAY
	uint32_t offset;
	const SHugePage& page = UnpackHandle(result, offset)->hugePage;
	Detail::MakeMemReplayEvent(page, offset, m_bMapPersistent, true, bytes, align);
#endif

	HEAP_ASSERT(result); // OOM?
	HEAP_ASSERT(GetSize(result) >= bytes);
	HEAP_ASSERT((GetBlockHandle(result, bytes, true), (bytes & (align - 1)) == 0));
	return result;
}

void CGpuHeap::Deallocate(THandle handle)
{
	if (handle)
	{
		CryAutoCriticalSectionNoRecursive lock(m_lock);

		DeallocateInternal(handle);
	}
}

void CGpuHeap::DeallocateInternal(THandle handle)
{
	const SHandle internalHandle = MakeInternalHandle(handle);
	HEAP_ASSERT(internalHandle.memoryType < m_numHeaps);

#if HEAP_MEMREPLAY
	uint32_t offset;
	const SHugePage& page = UnpackHandle(handle, offset)->hugePage;
	Detail::MakeMemReplayEvent(page, offset, m_bMapPersistent, false, 0, 0);
#endif

	switch (internalHandle.pageType)
	{
	case kPageTypeTiny:
		DeallocateTiny(internalHandle.memoryType, internalHandle.pageIndex, internalHandle.blockState);
		break;
	case kPageTypeSmall:
		DeallocateSmall(internalHandle.memoryType, internalHandle.pageIndex, internalHandle.blockState);
		break;
	case kPageTypeLarge:
		DeallocateLarge(internalHandle.memoryType, internalHandle.pageIndex, true, true);
		break;
	case kPageTypeHuge:
		DeallocateHuge(internalHandle.memoryType, internalHandle.pageIndex, true);
		break;
	}
}

CGpuHeap::THandle CGpuHeap::AllocateTiny(uint32_t memoryType, uint32_t bin)
{
	HEAP_ASSERT(bin < kTinyBlockBins);
	CLinkRoot<STinyPage>& pages = m_pHeaps[memoryType].tinyAlloc[bin];
	STinyPage* pPage = pages.AtFront(m_pPages);
	if (!pPage)
	{
		const uint32_t blockSize = GetBinSize(bin);
		if (pPage = AllocatePage<STinyPage>(m_pPages))
		{
			static_assert(kDistribution2 == STinyPage::kBlockBits * 2U && kSmallBlockBins == kTinyBlockBins * 2, "Need to revise this mapping");
			const uint32_t smallBin = (5U - kDistribution1) + bin + bin;
			const THandle smallPage = AllocateSmall(memoryType, smallBin);
			if (smallPage)
			{
				pPage->Init(MakeInternalHandle(smallPage));
				pages.PushFront(m_pPages, pPage);
			}
			else
			{
				DeallocatePage(m_pPages, pPage);
				pPage = nullptr;
			}
		}
	}
	if (pPage)
	{
		const uint32_t pageId = GetPageId(m_pPages, pPage);
		const uint32_t blockIndex = pPage->Allocate();
		const uint32_t blockState = blockIndex | (bin << STinyPage::kBlockBits);
		if (pPage->IsFull())
		{
			STinyPage* pFirstPage = pages.PopFront(m_pPages);
			HEAP_ASSERT(pFirstPage == pPage);
			m_pHeaps[memoryType].tinyFull.PushFront(m_pPages, pPage);
		}
		return MakeExternalHandle(pageId, kPageTypeTiny, memoryType, blockState);
	}
	return 0U;
}

CGpuHeap::THandle CGpuHeap::AllocateSmall(uint32_t memoryType, uint32_t bin)
{
	// Fetch small page
	HEAP_ASSERT(bin < kSmallBlockBins);
	CLinkRoot<SSmallPage>& pages = m_pHeaps[memoryType].smallAlloc[bin];
	SSmallPage* pPage = pages.AtFront(m_pPages);
	if (!pPage)
	{
		// Create a large page sized to fit small blocks.
		const uint32_t blockSize = GetBinSize(bin + kTinyBlockBins);
		const uint32_t largeBin = SelectLargeBin(blockSize * 32U / kRegionSize);
		const THandle largeHandle = AllocateLarge(memoryType, largeBin, blockSize * 32U, false);
		const SHandle internalHandle = MakeInternalHandle(largeHandle);
		if (!largeHandle)
		{
			return 0U;
		}

		// Extend the large page to a small page.
		SLargePage& largePage = m_pPages[internalHandle.pageIndex].largePage;
		pPage = static_cast<SSmallPage*>(&largePage);
		pPage->ExtendLarge(SelectSmallBinSize(bin));

		// Add to small list.
		pages.PushFront(m_pPages, pPage);
		HEAP_ASSERT(!pPage->IsFree());
	}

	// Sub-allocate from small page.
	const uint32_t pageId = GetPageId(m_pPages, pPage);
	const uint32_t blockIndex = pPage->Allocate();
	const uint32_t blockState = blockIndex | (bin << SSmallPage::kBlockBits);
	if (pPage->IsFull())
	{
		SSmallPage* pFirstPage = pages.PopFront(m_pPages);
		HEAP_ASSERT(pFirstPage == pPage);
		m_pHeaps[memoryType].smallFull[bin].PushFront(m_pPages, pPage);
	}
	return MakeExternalHandle(pageId, kPageTypeSmall, memoryType, blockState);
}

CGpuHeap::THandle CGpuHeap::AllocateLarge(uint32_t memoryType, uint32_t bin, uint32_t bytes, bool bLink)
{
	HEAP_ASSERT(bin < kLargeBlockBins);
	const uint32_t regions = (bytes + kRegionSize - 1U) >> SLog2<kRegionSize>::kValue;

	// Search the heap for a block of the desired size.
	SLargePage* pPage = nullptr;
	const uint8_t initialBin = bin;
	while (bin < kLargeBlockBins + kInternalBins)
	{
		CLinkRoot<SLargePage>& pages = m_pHeaps[memoryType].largeAlloc[bin];
		uint32_t bestRegions = 256U;
		pages.Walk(m_pPages, [regions, &pPage, &bestRegions](SLargePage& page) -> bool // TODO: Limit iteration depth to guarantee O(1)?
		{
			HEAP_ASSERT(page.IsFree());
			const uint32_t pageRegions = page.GetRegionCount(true);
			if (pageRegions >= regions && pageRegions < bestRegions)
			{
			  bestRegions = pageRegions;
			  pPage = &page;
			  return pageRegions != regions;
			}
			return true;
		});
		if (pPage)
		{
			pages.Unlink(m_pPages, pPage);
			break;
		}
		if ((bin == initialBin && initialBin + 4U < kLargeBlockBins) || bin == initialBin + 3U)
		{
			bin += (bin == initialBin ? 4U : 2U); // First try splitting something of 2x the size, so the next time we visit this bin it won't be empty.
		}
		else if (bin == initialBin + 4U)
		{
			bin -= 3U; // Second try the bin which are next-best-fit.
		}
		else
		{
			++bin; // Last just keep trying larger and larger bins until we can split something.
		}
	}

	uint32_t allocatedRegions;
	if (!pPage)
	{
		// Create a new huge page to sub-allocate from.
		pPage = AllocatePage<SLargePage>(m_pPages);
		if (!pPage)
		{
			return 0;
		}

		// We may be reacquiring something that was not a large or small page, so we have to clear the address-order link-indices to ensure assert doesn't hit on insert.
		// For example, tiny pages will re-use the address-link offset to store a handle instead, which is not guaranteed to be zeroed before being freed (unlike the bin-links).
		HEAP_ASSERT((pPage->link[1].prev = pPage->link[1].next = 0U) == 0U);

		const THandle hugeHandle = AllocateHuge(memoryType, kRegionSize * 256U, kRegionSize, false);
		const SHandle sharedHandle = MakeInternalHandle(hugeHandle);
		if (!hugeHandle)
		{
			DeallocatePage(m_pPages, pPage);
			return 0;
		}

		pPage->Init(sharedHandle.pageIndex);
		pPage->link[1].prev = 0U;
		pPage->link[1].next = 0U;
		pPage->range.b = 0U;
		pPage->range.e = 255U;
		allocatedRegions = 256U;
	}
	else
	{
		allocatedRegions = pPage->GetRegionCount(true);
	}
	HEAP_ASSERT(allocatedRegions >= regions);

	// Split the allocated block as needed.
	const TPageId pageId = GetPageId(m_pPages, pPage);
	if (allocatedRegions >= regions + kMinRegions)
	{
		HEAP_ASSERT(allocatedRegions > regions);
		SLargePage* pTail = AllocatePage<SLargePage>(m_pPages);
		if (pTail)
		{
			// We may be reacquiring something that was not a large or small page, so we have to clear the address-order link-indices to ensure assert doesn't hit on insert.
			// For example, tiny pages will re-use the address-link offset to store a handle instead, which is not guaranteed to be zeroed before being freed (unlike the bin-links).
			HEAP_ASSERT((pTail->link[1].prev = pTail->link[1].next = 0U) == 0U);

			// Split regions
			const uint32_t pageBegin = pPage->GetBegin(true);
			const uint32_t pageEnd = pageBegin + allocatedRegions;
			pTail->Init(pPage->parentPage);
			pTail->range.b = pageBegin + regions - 1U;
			pTail->range.e = pageEnd - 1U;
			pPage->range.e = pPage->range.b + regions;

			// Register new free page
			bin = SelectLargeBin(allocatedRegions - regions);
			HEAP_ASSERT(bin < kLargeBlockBins + kInternalBins);
			m_pHeaps[memoryType].largeAlloc[bin].PushFront(m_pPages, pTail);

			// Split addresses
			CLinkRoot<SLargePage, 1> addressLinks;
			addressLinks.Init(pageId, pageId);
			addressLinks.InsertAfter(m_pPages, pTail, *pPage);

			// Check consistency
			HEAP_ASSERT(pTail->IsFree());
			HEAP_ASSERT(pTail->GetEnd(true) == pageEnd);
			HEAP_ASSERT(pTail->GetBegin(true) == pPage->GetEnd(true));
			HEAP_ASSERT(pTail->GetRegionCount(true) == allocatedRegions - regions);
			allocatedRegions = regions;
		}
		else if (allocatedRegions == 256U)
		{
			// Roll back
			DeallocateHuge(memoryType, pPage->parentPage, false);
			DeallocatePage(m_pPages, pPage);
			return 0U;
		}
	}

	pPage->MarkBusy();

	// Commit regions
	if (m_bCommitRegions)
	{
		const SHugePage& hugePage = m_pPages[pPage->parentPage].hugePage;
		const uint32_t firstRegion = pPage->GetBegin(false);
		const uint32_t lastRegion = pPage->GetEnd(false);
		if (!CommitRegions(memoryType, hugePage.GetHandle(), firstRegion * kRegionSize, (lastRegion - firstRegion) * kRegionSize))
		{
			// Cannot commit backing memory, roll-back
			DeallocateLarge(memoryType, GetPageId(m_pPages, pPage), false, false);
		}
	}

	if (bLink)
	{
		m_pHeaps[memoryType].largeFull.PushFront(m_pPages, pPage);
	}

	return MakeExternalHandle(pageId, kPageTypeLarge, memoryType, allocatedRegions);
}

CGpuHeap::THandle CGpuHeap::AllocateHuge(uint32_t memoryType, uint32_t bytes, uint32_t align, bool bCommit)
{
	SHugePage* const pPage = AllocatePage<SHugePage>(m_pPages);
	if (!pPage)
	{
		return 0U;
	}

	const uint32_t numRegions = (bytes + kRegionSize - 1U) >> SLog2<kRegionSize>::kValue;
	const uint32_t blockHandle = AllocateBlock(memoryType, numRegions * kRegionSize, align);
	if (!blockHandle || !pPage->SetHandle(blockHandle))
	{
		if (blockHandle)
		{
			HEAP_ASSERT(false && "Block handle value too large");
			DeallocateBlock(memoryType, blockHandle, numRegions * kRegionSize);
		}
		DeallocatePage(m_pPages, pPage);
		return 0U;
	}

	if (bCommit && m_bCommitRegions)
	{
		if (!CommitRegions(memoryType, blockHandle, 0, numRegions * kRegionSize))
		{
			DeallocateBlock(memoryType, blockHandle, numRegions * kRegionSize);
			DeallocatePage(m_pPages, pPage);
			return 0U;
		}
	}

	pPage->size = numRegions;

	if (m_bMapPersistent)
	{
		void* pAddress = MapBlock(memoryType, blockHandle);
		if (!pPage->SetAddress(pAddress))
		{
			UnmapBlock(memoryType, blockHandle, pAddress);
		}
		pPage->mapped = 1U;
	}
	else
	{
		pPage->SetAddress(nullptr);
		pPage->mapped = 0U;
	}

	m_pHeaps[memoryType].hugeBlocks.PushFront(m_pPages, pPage);

	const uint32_t pageId = GetPageId(m_pPages, pPage);
	return MakeExternalHandle(pageId, kPageTypeHuge, memoryType, 0x42U); // Any non-zero value may be stored
}

void CGpuHeap::DeallocateTiny(uint32_t memoryType, uint32_t pageId, uint32_t blockState)
{
	const uint32_t blockIndex = blockState & ((1U << STinyPage::kBlockBits) - 1U);
	const uint32_t bin = blockState >> STinyPage::kBlockBits;

	STinyPage& page = m_pPages[pageId].tinyPage;
	if (page.IsFull())
	{
		// Move from full to available list
		CLinkRoot<STinyPage>& fullPages = m_pHeaps[memoryType].tinyFull;
		CLinkRoot<STinyPage>& freePages = m_pHeaps[memoryType].tinyAlloc[bin];
		fullPages.Unlink(m_pPages, &page);
		freePages.PushFront(m_pPages, &page);
	}
	page.Deallocate(blockIndex);
	if (page.IsEmpty())
	{
		// Remove from available list
		CLinkRoot<STinyPage>& freePages = m_pHeaps[memoryType].tinyAlloc[bin];
		freePages.Unlink(m_pPages, &page);

		// Destroy page
		DeallocateSmall(memoryType, page.parentBlock.pageIndex, page.parentBlock.blockState);
		DeallocatePage(m_pPages, &page);
	}
}

void CGpuHeap::DeallocateSmall(uint32_t memoryType, uint32_t pageId, uint32_t blockState)
{
	const uint32_t blockIndex = blockState & ((1U << SSmallPage::kBlockBits) - 1U);
	const uint32_t bin = blockState >> SSmallPage::kBlockBits;

	SSmallPage& page = m_pPages[pageId].smallPage;
	if (page.IsFull())
	{
		// Move from full to available list
		CLinkRoot<SSmallPage>& fullPages = m_pHeaps[memoryType].smallFull[bin];
		CLinkRoot<SSmallPage>& freePages = m_pHeaps[memoryType].smallAlloc[bin];
		fullPages.Unlink(m_pPages, &page);
		freePages.PushFront(m_pPages, &page);
	}
	page.Deallocate(blockIndex);
	if (page.IsEmpty(SelectSmallBinSize(bin)))
	{
		// Remove from available list
		CLinkRoot<SSmallPage>& freePages = m_pHeaps[memoryType].smallAlloc[bin];
		freePages.Unlink(m_pPages, &page);

		// Destroy page
		DeallocateLarge(memoryType, pageId, false, true);
	}
}

void CGpuHeap::DeallocateLarge(uint32_t memoryType, uint32_t pageId, bool bUnlink, bool bCommit)
{
	SLargePage& page = m_pPages[pageId].largePage;
	page.MarkFree();
	if (bUnlink)
	{
		m_pHeaps[memoryType].largeFull.Unlink(m_pPages, &page);
	}

	// De-commit regions
	if (bCommit && m_bCommitRegions)
	{
		const SHugePage& hugePage = m_pPages[page.parentPage].hugePage;
		const uint32_t firstRegion = page.GetBegin(true);
		const uint32_t lastRegion = page.GetEnd(true);
		DecommitRegions(memoryType, hugePage.GetHandle(), firstRegion * kRegionSize, (lastRegion - firstRegion) * kRegionSize);
	}

	// Coalesce with neighboring blocks
	SLargePage* pPrevPage = page.link[1].prev ? &m_pPages[page.link[1].prev].largePage : nullptr;
	SLargePage* pNextPage = page.link[1].next ? &m_pPages[page.link[1].next].largePage : nullptr;
	CLinkRoot<SLargePage, 1> addressLinks;
	addressLinks.Init(page.link[1].prev ? page.link[1].prev : pageId, page.link[1].next ? page.link[1].next : pageId);
	HEAP_ASSERT(&page != pPrevPage && &page != pNextPage);
	HEAP_ASSERT(!pPrevPage || pPrevPage->link[1].next == pageId);
	HEAP_ASSERT(!pNextPage || pNextPage->link[1].prev == pageId);
	if (pNextPage && pNextPage->IsFree())
	{
		HEAP_ASSERT(pNextPage->GetBegin(true) == page.GetEnd(true));
		const uint32_t nextRegions = pNextPage->GetRegionCount(true);
		const uint32_t nextBin = SelectLargeBin(nextRegions);

		// Unlink from free list
		CLinkRoot<SLargePage>& pages = m_pHeaps[memoryType].largeAlloc[nextBin];
		pages.Unlink(m_pPages, pNextPage);

		// Unlink region reservation
		const uint32_t newEnd = pNextPage->GetEnd(true);
		page.range.e = newEnd - (pPrevPage || newEnd == 256U);

		// Unlink from address list
		const uint32_t nextNext = pNextPage->link[1U].next;
		addressLinks.Unlink(m_pPages, pNextPage);
		HEAP_ASSERT(page.link[1U].next == nextNext);
		HEAP_ASSERT(nextNext == 0U || m_pPages[nextNext].largePage.link[1U].prev == pageId);

		// Free the coalesced page
		HEAP_ASSERT(page.GetEnd(true) == newEnd || (page.range.b == 0U && page.range.e == 255U));
		DeallocatePage(m_pPages, pNextPage);
	}
	else
	{
		HEAP_ASSERT(!pNextPage || pNextPage->GetBegin(false) == page.GetEnd(true));
	}

	if (pPrevPage && pPrevPage->IsFree())
	{
		HEAP_ASSERT(pPrevPage->GetEnd(true) == page.GetBegin(true));
		const uint32_t prevRegions = pPrevPage->GetRegionCount(true);
		const uint32_t prevBin = SelectLargeBin(prevRegions);

		// Unlink from free list
		CLinkRoot<SLargePage>& pages = m_pHeaps[memoryType].largeAlloc[prevBin];
		pages.Unlink(m_pPages, pPrevPage);

		// Unlink region reservation
		const uint32_t newBegin = pPrevPage->GetBegin(true);
		page.range.b = newBegin - 1U;

		if (pPrevPage->link[1].prev == 0U)
		{
			// We have become the first page in the region list, so we must adjust our region counters, since they will be offset by one
			page.range.b += 1U;
			page.range.e += (page.range.e != 255U);
		}

		// Unlink from address list
		const TPageId prevPrev = pPrevPage->link[1U].prev;
		addressLinks.Unlink(m_pPages, pPrevPage);
		HEAP_ASSERT(page.link[1U].prev == prevPrev);
		HEAP_ASSERT(prevPrev == 0U || m_pPages[prevPrev].largePage.link[1U].next == pageId);

		// Free the coalesced page
		HEAP_ASSERT(page.GetBegin(true) == newBegin);
		DeallocatePage(m_pPages, pPrevPage);
	}
	else
	{
		HEAP_ASSERT(!pPrevPage || pPrevPage->GetEnd(false) == page.GetBegin(true));
	}

	if (page.range.b == 0U && page.range.e == 255U)
	{
		// Free the entire page
		HEAP_ASSERT(page.link[1].prev == 0U && page.link[1].next == 0U);
		DeallocateHuge(memoryType, page.parentPage, false);
		DeallocatePage(m_pPages, &page);
	}
	else
	{
		// Re-bin as a free page
		HEAP_ASSERT(page.link[1].prev != 0U || page.link[1].next != 0U);
		const uint32_t regions = page.GetRegionCount(true);
		HEAP_ASSERT(regions > 0U && regions < 0x100U);
		const uint32_t freeBin = SelectLargeBin(regions);
		m_pHeaps[memoryType].largeAlloc[freeBin].PushFront(m_pPages, &page);
	}
}

void CGpuHeap::DeallocateHuge(uint32_t memoryType, uint32_t pageId, bool bCommit)
{
	SHugePage& page = m_pPages[pageId].hugePage;
	m_pHeaps[memoryType].hugeBlocks.Unlink(m_pPages, &page);

	const uint32_t blockHandle = page.GetHandle();
	if (m_bMapPersistent)
	{
		HEAP_ASSERT(page.mapped == 1U);
		void* pAddress = page.GetAddress();
		if (pAddress)
		{
			UnmapBlock(memoryType, blockHandle, pAddress);
		}
	}
	else
	{
		HEAP_ASSERT(page.mapped == 0U);
	}

	if (bCommit && m_bCommitRegions)
	{
		DecommitRegions(memoryType, blockHandle, 0U, page.size * kRegionSize);
	}

	DeallocateBlock(memoryType, blockHandle, page.size * kRegionSize);
	DeallocatePage(m_pPages, &page);
}

uint32_t CGpuHeap::GetSize(THandle handle) const
{
	if (handle)
	{
		const SHandle internalHandle = MakeInternalHandle(handle);
		switch (internalHandle.pageType)
		{
		case kPageTypeTiny:
			return GetBinSize(internalHandle.blockState >> STinyPage::kBlockBits);
		case kPageTypeSmall:
			return GetBinSize((internalHandle.blockState >> SSmallPage::kBlockBits) + kTinyBlockBins);
		case kPageTypeLarge:
			return internalHandle.blockState * kRegionSize;
		case kPageTypeHuge:
			return m_pPages[internalHandle.pageIndex].hugePage.size * kRegionSize;
		}
	}
	return 0U;
}

uint32_t CGpuHeap::GetAdjustedSize(uint32_t bytes, uint32_t align) const
{
	bytes = std::max(bytes, align); // All blocks are at least the same size as their alignment
	const uint32_t bin = SelectBin(bytes);
	return GetBinSize(bin);
}

uint32_t CGpuHeap::GetMemoryType(THandle handle) const
{
	return MakeInternalHandle(handle).memoryType;
}

CGpuHeap::TBlockHandle CGpuHeap::GetBlockHandle(THandle handle, uint32_t& offset, bool bCallback) const
{
	if (handle)
	{
		if (bCallback)
		{
			return UnpackHandle(handle, offset)->hugePage.GetHandle();
		}
		else
		{
			CryAutoCriticalSectionNoRecursive lock(m_lock);
			return UnpackHandle(handle, offset)->hugePage.GetHandle();
		}
	}
	offset = 0U;
	return 0U;
}

CGpuHeap::TBlockHandle CGpuHeap::GetMaximumBlockHandle()
{
	return (1U << SHugePage::kAlignShift);
}

uint32_t CGpuHeap::GetRegionSize()
{
	return kRegionSize;
}

void* CGpuHeap::Map(THandle handle)
{
	if (!handle)
	{
		return nullptr;
	}

	CryAutoCriticalSectionNoRecursive lock(m_lock);
	uint32_t offset;
	SHugePage& page = UnpackHandle(handle, offset)->hugePage;
	if (!page.mapped)
	{
		const TBlockHandle blockHandle = page.GetHandle();
		const uint32_t memoryType = MakeInternalHandle(handle).memoryType;
		void* const pAddress = MapBlock(memoryType, blockHandle);
		if (pAddress && !page.SetAddress(pAddress))
		{
			UnmapBlock(memoryType, blockHandle, pAddress);
			HEAP_ASSERT(false && "Block alignment does not meet requested alignment");
		}
	}

	void* const pAddress = page.GetAddress();
	page.mapped += (pAddress != nullptr);
	return static_cast<char*>(pAddress) + (pAddress ? offset : 0U);
}

void CGpuHeap::Unmap(THandle handle)
{
	if (handle)
	{
		CryAutoCriticalSectionNoRecursive lock(m_lock);
		uint32_t offset;
		SHugePage& page = UnpackHandle(handle, offset)->hugePage;
		HEAP_ASSERT(page.mapped);
		--page.mapped;
		if (!page.mapped)
		{
			HEAP_ASSERT(!m_bMapPersistent);
			const uint32_t memoryType = MakeInternalHandle(handle).memoryType;
			UnmapBlock(memoryType, page.GetHandle(), page.GetAddress());
			page.SetAddress(nullptr);
		}
	}
}

UPage* CGpuHeap::UnpackHandle(THandle handle, uint32_t& offset) const
{
	HEAP_ASSERT(handle && "Null handle passed");
	SHandle internalHandle = MakeInternalHandle(handle);
	offset = 0;
	if (internalHandle.pageType == kPageTypeTiny)
	{
		const uint32_t bin = internalHandle.blockState >> STinyPage::kBlockBits;
		const uint32_t blockIndex = internalHandle.blockState & ((1U << STinyPage::kBlockBits) - 1U);
		const uint32_t blockSize = GetBinSize(bin);
		const STinyPage& page = m_pPages[internalHandle.pageIndex].tinyPage;
		HEAP_ASSERT((page.IsFull() ? m_pHeaps[internalHandle.memoryType].tinyFull : m_pHeaps[internalHandle.memoryType].tinyAlloc[bin]).Contains(m_pPages, &page) && page.IsAllocated(blockIndex) && "Tiny handle not allocated (or already freed)");
		offset += blockIndex * blockSize;
		internalHandle = page.parentBlock;
	}
	if (internalHandle.pageType == kPageTypeSmall)
	{
		const uint32_t bin = internalHandle.blockState >> SSmallPage::kBlockBits;
		const uint32_t blockIndex = internalHandle.blockState & ((1U << SSmallPage::kBlockBits) - 1U);
		const uint32_t blockSize = GetBinSize(bin + kTinyBlockBins);
		const SSmallPage& page = m_pPages[internalHandle.pageIndex].smallPage;
		HEAP_ASSERT((page.IsFull() ? m_pHeaps[internalHandle.memoryType].smallFull[bin] : m_pHeaps[internalHandle.memoryType].smallAlloc[bin]).Contains(m_pPages, &page) && page.IsAllocated(blockIndex) && "Small handle not allocated (or already freed)");
		offset += blockIndex * blockSize;
		offset += page.GetBegin(false) * kRegionSize;
		internalHandle.pageIndex = page.parentPage;
		internalHandle.pageType = kPageTypeHuge;
	}
	else if (internalHandle.pageType == kPageTypeLarge)
	{
		SLargePage& largePage = m_pPages[internalHandle.pageIndex].largePage;
		HEAP_ASSERT(m_pHeaps[internalHandle.memoryType].largeFull.Contains(m_pPages, &largePage) && !largePage.IsFree() && "Large handle not allocated (or already freed)");
		offset += largePage.GetBegin(false) * kRegionSize;
		internalHandle.pageIndex = largePage.parentPage;
		internalHandle.pageType = kPageTypeHuge;
	}
	HEAP_ASSERT(internalHandle.pageType == kPageTypeHuge && "Invalid handle passed");
	UPage* const pResult = m_pPages + internalHandle.pageIndex;
	HEAP_ASSERT(m_pHeaps[internalHandle.memoryType].hugeBlocks.Contains(m_pPages, &pResult->hugePage) && "Huge handle not allocated (or already freed)");
	HEAP_ASSERT(offset < pResult->hugePage.size * kRegionSize && "Sub-block out of bounds");
	return pResult;
}

uint32_t CGpuHeap::Walk(void* pContext, TSummaryCallback pfnSummary, TAllocationCallback pfnAllocation, uint32_t memoryTypeMask) const
{
	CryAutoCriticalSectionNoRecursive lock(m_lock);

	UPage* const pPages = m_pPages;
	uint32_t consumedPages = 0U;

	for (uint32_t memoryType = 0U; memoryType < m_numHeaps; ++memoryType)
	{
		if ((memoryTypeMask & (1U << memoryType)) == 0U)
		{
			continue;
		}
		const SSubHeap& heap = m_pHeaps[memoryType];

		uint32_t usedHandles = 0U; // Number of handles given out to the client.
		uint64_t usedBytes = 0U;   // Number of bytes given out to the client.
		uint32_t usedPages = 0U;   // Page structures used to track busy regions.
		uint32_t freePages = 0U;   // Page structures used to track free regions.
		uint32_t freeRegions = 0U; // Free whole-regions in large pages.
		uint32_t freeBytes = 0U;   // Free sub-blocks of small or tiny pages.

		std::vector<const SHugePage*> hugePages;
		std::vector<const SLargePage*> largePages;
		std::vector<std::pair<const SSmallPage*, uint64_t>> smallPages;
		std::vector<std::pair<const STinyPage*, uint32_t>> tinyPages;

		const auto removeHugePage = [&](TPageId pageId)
		{
			const SHugePage* const pHugePage = &pPages[pageId].hugePage;
			auto it = std::find(hugePages.begin(), hugePages.end(), pHugePage);
			if (it != hugePages.end())
			{
				*it = nullptr;
			}
		};

		const auto removeSmallBlock = [&](SHandle smallHandle) -> uint32_t
		{
			HEAP_ASSERT(smallHandle.memoryType == memoryType && smallHandle.pageType == kPageTypeSmall);
			const uint32_t blockIndex = smallHandle.blockState & ((1U << SSmallPage::kBlockBits) - 1U);
			const SSmallPage* const pSmallPage = &pPages[smallHandle.pageIndex].smallPage;
			auto it = std::find_if(smallPages.begin(), smallPages.end(), [pSmallPage](const std::pair<const SSmallPage*, uint64_t>& pair)
			{
				return pair.first == pSmallPage;
			});
			HEAP_ASSERT(it != smallPages.end());
			HEAP_ASSERT(static_cast<uint32_t>(it->second) & (1U << blockIndex));
			it->second ^= static_cast<uint64_t>(1U) << blockIndex;
			return GetBinSize(static_cast<uint32_t>(it->second >> 32U) + kTinyBlockBins) / 64U;
		};

		const auto invokeCallback = [&](THandle handle)
		{
			uint32_t offset;
			const SHugePage* pHugePage = &UnpackHandle(handle, offset)->hugePage;
			const uint32_t blockSize = GetSize(handle);
			if (pfnAllocation)
			{
				const TBlockHandle blockHandle = pHugePage->GetHandle();
				char* const pAddress = static_cast<char*>(pHugePage->GetAddress());
				pfnAllocation(pContext, handle, memoryType, blockHandle, offset, blockSize, pAddress + (pAddress ? offset : 0U));
			}
			++usedHandles;
			usedBytes += blockSize;
		};

		heap.hugeBlocks.Walk(pPages, [&](const SHugePage& hugePage) -> bool
		{
			hugePages.push_back(&hugePage);
			++usedPages;
			return true;
		});

		const uint32_t totalBlocks = static_cast<uint32_t>(hugePages.size());

		heap.largeFull.Walk(pPages, [&](const SLargePage& largePage) -> bool
		{
			removeHugePage(largePage.parentPage);
			largePages.push_back(&largePage);
			HEAP_ASSERT(!largePage.IsFree());
			++usedPages;
			return true;
		});

		for (auto& smallBin : heap.smallFull)
		{
			const uint64_t binMask = static_cast<uint64_t>(&smallBin - heap.smallFull) << 32U;
			smallBin.Walk(pPages, [&](const SSmallPage& smallPage) -> bool
			{
				removeHugePage(smallPage.parentPage);
				smallPages.emplace_back(&smallPage, ~smallPage.freeBlocks | binMask);
				HEAP_ASSERT(smallPage.IsFull() && !smallPage.IsFree());
				++usedPages;
				return true;
			});
		}

		for (auto& smallBin : heap.smallAlloc)
		{
			const uint32_t bin = static_cast<uint32_t>(&smallBin - heap.smallAlloc);
			const uint64_t binMask = static_cast<uint64_t>(bin) << 32U;
			smallBin.Walk(pPages, [&](const SSmallPage& smallPage) -> bool
			{
				removeHugePage(smallPage.parentPage);
				smallPages.emplace_back(&smallPage, ~smallPage.freeBlocks | binMask);
				HEAP_ASSERT(!smallPage.IsEmpty(SelectSmallBinSize(bin)) && !smallPage.IsFull() && !smallPage.IsFree());
				++usedPages;
				return true;
			});
		}

		hugePages.erase(std::remove(hugePages.begin(), hugePages.end(), nullptr), hugePages.end());

		for (auto& largeBin : heap.largeAlloc)
		{
			largeBin.Walk(pPages, [&](const SLargePage& largePage) -> bool
			{
				const SHugePage* const pHugePage = &pPages[largePage.parentPage].hugePage;
				HEAP_ASSERT(std::find(hugePages.begin(), hugePages.end(), pHugePage) == hugePages.end());
				++freePages;
				freeRegions += largePage.GetRegionCount(true);
				return true;
			});
		}

		heap.tinyFull.Walk(pPages, [&](const STinyPage& tinyPage) -> bool
		{
			const uint32_t size = removeSmallBlock(tinyPage.parentBlock);
			tinyPages.emplace_back(&tinyPage, size);
			HEAP_ASSERT(tinyPage.IsFull());
			++usedPages;
			return true;
		});

		for (auto& tinyBin : heap.tinyAlloc)
		{
			tinyBin.Walk(pPages, [&](const STinyPage& tinyPage) -> bool
			{
				const uint32_t size = removeSmallBlock(tinyPage.parentBlock);
				HEAP_ASSERT(SelectBin(size) == &tinyBin - heap.tinyAlloc);
				tinyPages.emplace_back(&tinyPage, size);
				HEAP_ASSERT(!tinyPage.IsFull() && !tinyPage.IsEmpty());
				++usedPages;
				return true;
			});
		}

		const uint32_t externalBlocks = static_cast<uint32_t>(hugePages.size());
		const uint32_t internalBlocks = totalBlocks - externalBlocks;
		uint64_t externalBytes = 0U;

		for (const SHugePage* pHugePage : hugePages)
		{
			const TPageId pageId = GetPageId(pPages, pHugePage);
			const THandle handle = MakeExternalHandle(pageId, kPageTypeHuge, memoryType, 0x42U);
			invokeCallback(handle);
			externalBytes += pHugePage->size * kRegionSize;
		}

		for (const SLargePage* pLargePage : largePages)
		{
			const TPageId pageId = GetPageId(pPages, pLargePage);
			const THandle handle = MakeExternalHandle(pageId, kPageTypeLarge, memoryType, pLargePage->GetRegionCount(false));
			invokeCallback(handle);
		}

		for (auto& pair : smallPages)
		{
			const SSmallPage* const pSmallPage = pair.first;
			const TPageId pageId = GetPageId(pPages, pSmallPage);
			const uint32_t bin = static_cast<uint32_t>(pair.second >> 32U);
			HEAP_ASSERT(bin < kSmallBlockBins && (pSmallPage->IsFull() ? heap.smallFull[bin] : heap.smallAlloc[bin]).Contains(pPages, pSmallPage));
			uint32_t allocatedBlocks = static_cast<uint32_t>(pair.second);
			uint32_t count = 0U;
			while (allocatedBlocks)
			{
				const uint32_t blockIndex = countTrailingZeros32(allocatedBlocks);
				allocatedBlocks ^= 1U << blockIndex;
				const uint32_t blockState = blockIndex | (bin << SSmallPage::kBlockBits);
				const THandle handle = MakeExternalHandle(pageId, kPageTypeSmall, memoryType, blockState);
				invokeCallback(handle);
				++count;
			}
			const uint32_t blockSize = GetBinSize(bin + kTinyBlockBins);
			const uint32_t inaccessibleBytes = pSmallPage->GetRegionCount(false) * kRegionSize - SelectSmallBinSize(bin) * blockSize;
			const uint32_t freeSubBlocks = SSmallPage::kBlockCount - count;
			freeBytes += freeSubBlocks * blockSize + inaccessibleBytes;
		}

		for (auto& pair : tinyPages)
		{
			const STinyPage* const pTinyPage = pair.first;
			const TPageId pageId = GetPageId(pPages, pTinyPage);
			const uint32_t bin = SelectBin(pair.second);
			HEAP_ASSERT(bin < kTinyBlockBins && (pTinyPage->IsFull() ? heap.tinyFull : heap.tinyAlloc[bin]).Contains(pPages, pTinyPage));
			uint32_t allocatedBlocks[2];
#if CRY_PLATFORM_32BIT
			allocatedBlocks[0] = ~pTinyPage->freeBlocks[0];
			allocatedBlocks[1] = ~pTinyPage->freeBlocks[1];
#else
			allocatedBlocks[0] = ~static_cast<uint32_t>(pTinyPage->freeBlocks);
			allocatedBlocks[1] = ~static_cast<uint32_t>(pTinyPage->freeBlocks >> 32U);
#endif
			uint32_t count = 0U;
			for (uint32_t i = 0; i < 2; ++i)
			{
				while (allocatedBlocks[i])
				{
					const uint32_t blockIndex = countTrailingZeros32(allocatedBlocks[i]);
					allocatedBlocks[i] ^= 1U << blockIndex;
					const uint32_t blockState = blockIndex + i * 32U | (bin << STinyPage::kBlockBits);
					const THandle handle = MakeExternalHandle(pageId, kPageTypeTiny, memoryType, blockState);
					invokeCallback(handle);
					++count;
				}
			}
			const uint32_t freeSubBlocks = STinyPage::kBlockCount - count;
			freeBytes += freeSubBlocks * pair.second;
			freeBytes -= 64U * pair.second;
		}

		// Check that all regions and blocks are accounted for.
		const uint64_t internalBytes = static_cast<uint64_t>(internalBlocks) * kRegionSize * 256U;
		const uint64_t availableBytes = static_cast<uint64_t>(freeRegions) * kRegionSize + freeBytes;
		HEAP_ASSERT(usedBytes + availableBytes == externalBytes + internalBytes);

		consumedPages += usedPages;
		consumedPages += freePages;

		if (pfnSummary)
		{
			const uint32_t structureUsed = (usedPages + freePages) * sizeof(UPage) + sizeof(SSubHeap);
			pfnSummary(pContext, memoryType, internalBytes + externalBytes, usedBytes, usedHandles, structureUsed);
		}
	}

	// Check all pages are accounted for
	uint32_t unusedPages = 1U;
	reinterpret_cast<CLinkRoot<const SLinkedPage<1>>*>(&pPages->freePage)->Walk(pPages, [&](const SLinkedPage<1>& freePage)
	{
		++unusedPages;
		return true;
	});
	HEAP_ASSERT(unusedPages + consumedPages == kMaxPages);

	// All other CPU side structures
	return unusedPages * sizeof(UPage) + sizeof(*this);
}
