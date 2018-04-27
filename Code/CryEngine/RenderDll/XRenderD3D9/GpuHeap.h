// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <stdint.h>

namespace Detail
{
struct SSubHeap;
union UPage;
}

// Utility for sub-allocating from large blocks of GPU accessible memory.
// This heap implementation does not require the allocated memory to be CPU visible or accessible.
// Note: The heap allocates ~1MB (32-bit) or ~1.5MB (64-bit) of CPU memory once on startup, but nothing after that.
// This is a base-class, the derived class should implement the abstract methods for large blocks.
class CGpuHeap
{
#if CRY_PLATFORM_64BIT
	static const bool kMapPersistentDefault = true;
#else
	static const bool kMapPersistentDefault = false;
#endif

public:
	// Handle to a block of memory.
	typedef uint32_t THandle;

protected:
	// Create a GPU heap with the given number of memory types.
	// Note: The maximum number of memory types is currently 32.
	// Set the map-persistent flag to save the API overhead of repeated map/unmap, and instead always map, at the cost of CPU virtual memory space.
	// Set the commit-regions flag if the heap wants to commit/decommit regions separate from the block allocations. This can be used to eliminate physical memory fragmentation, if the heap can control the page-mapping.
	CGpuHeap(uint32_t numMemoryTypes, bool bMapPersistent = kMapPersistentDefault, bool bCommitRegions = false);

	// Destroys the GPU heap.
	// All remaining allocated API blocks will be unmapped and deallocated.
	// Any handles returned from Allocate(), Map() or GetBlockHandle() can no longer be used.
	virtual ~CGpuHeap();

	// Handle to an API block.
	typedef uint32_t TBlockHandle;

	// Allocate a large block of some memory type.
	// May return 0 if not possible to allocate the block.
	// Otherwise, the handle value must be <= GetMaximumBlockHandle().
	virtual TBlockHandle AllocateBlock(uint32_t memoryType, uint32_t bytes, uint32_t align) = 0;

	// Deallocates a large block of some type.
	virtual void DeallocateBlock(uint32_t memoryType, TBlockHandle blockHandle, uint32_t bytes) = 0;

	// Maps a block into CPU memory view.
	// Return null if not possible.
	// Note: Will not be called recursively.
	virtual void* MapBlock(uint32_t memoryType, TBlockHandle blockHandle) = 0;

	// Unmaps a block from CPU memory view.
	// Note: Will not be called recursively.
	virtual void UnmapBlock(uint32_t memoryType, TBlockHandle blockHandle, void* pAddress) = 0;

	// Commits one or more regions inside a block, which is always aligned on the region size.
	// Note: Will not be called recursively, only used if bCommitRegions was set on creation.
	virtual bool CommitRegions(uint32_t memoryType, TBlockHandle blockHandle, uint32_t offset, uint32_t bytes) { return false; }

	// De-commits one or more regions inside a block, which is always aligned on the region size.
	// Note: Will not be called recursively, only used if bCommitRegions was set on creation.
	virtual void DecommitRegions(uint32_t memoryType, TBlockHandle blockHandle, uint32_t offset, uint32_t bytes) { return; }

	// Retrieves the block handle for a sub-allocation.
	// Returns a handle returned by AllocateBlock() and an offset into that block (in bytes).
	// Specify bCallback if this function is being called from inside a virtual override from above.
	TBlockHandle GetBlockHandle(THandle handle, uint32_t& offset, bool bCallback) const;

	// Retrieves the maximum valid value for a handle.
	// AllocateBlock() must return a value smaller than this.
	static TBlockHandle GetMaximumBlockHandle();

	// Retrieves the size of a region inside a block.
	// This may be used to determine if the implementation wants to handle CommitRegions/DecommitRegions.
	static uint32_t GetRegionSize();

	// Allocate or deallocate a block, while inside one of the virtual override from above.
	// You cannot touch any state with the same memory-type as the one being used for call-back.
	THandle AllocateInternal(uint32_t memoryType, uint32_t bytes, uint32_t align, uint32_t bin = ~0U);
	void    DeallocateInternal(THandle handle);

public:
	// Allocates a block of memory with the given parameters.
	// Returns 0 if no block could be allocated.
	THandle Allocate(uint32_t memoryType, uint32_t bytes, uint32_t align);

	// Deallocates a block of memory previously allocated with Allocate().
	void Deallocate(THandle handle);

	// Gets the actual size of a handle previously allocated with Allocate().
	// This may be larger than the number of requested bytes.
	uint32_t GetSize(THandle handle) const;

	// Gets the minimum actual size of a block with the requested size and alignment, does not perform an actual allocation.
	// The following guarantee will hold: GetSize(Allocate(..., bytes, align) >= GetAdjustedSize(bytes, align).
	// Note: It's possible (but unlikely) that Allocate() will return an even larger block in case of fall-back in near-OOM situations.
	uint32_t GetAdjustedSize(uint32_t bytes, uint32_t align) const;

	// Gets the memory type a handle previously allocated with Allocate().
	uint32_t GetMemoryType(THandle handle) const;

	// Maps the given memory into CPU memory view.
	// Note: You may call this recursively if you want.
	// May return nullptr if the memory is not CPU accessible, or no VM is available. In this case, don't call Unmap().
	void* Map(THandle handle);

	// Unmaps the given memory from CPU memory view.
	// Note: You may call this recursively if you want.
	void Unmap(THandle handle);

	// Walks the heap, returning all handles currently allocated, as well as properties of the allocation.
	// Optionally restricts the types of memory to walk the heap for.
	// NOTE: This is a very expensive call, it should only be used for non-time-critical debugging purposes.
	typedef void (* TAllocationCallback)(void* pContext, THandle handle, uint32_t memoryType, TBlockHandle blockHandle, uint32_t blockOffset, uint32_t allocationSize, void* pMappedAddress);
	typedef void (* TSummaryCallback)(void* pContext, uint32_t memoryType, uint64_t totalApiAllocated, uint64_t totalClientAllocated, uint32_t totalClientHandles, uint32_t cpuSideOverhead);
	uint32_t Walk(void* pContext, TSummaryCallback pfnSummary, TAllocationCallback pfnAllocation = nullptr, uint32_t memoryTypeMask = 0xFFFFFFFFU) const;

private:
	Detail::UPage* UnpackHandle(THandle handle, uint32_t& offset) const;

	THandle        AllocateTiny(uint32_t memoryType, uint32_t bin);
	THandle        AllocateSmall(uint32_t memoryType, uint32_t bin);
	THandle        AllocateLarge(uint32_t memoryType, uint32_t bin, uint32_t bytes, bool bLink);
	THandle        AllocateHuge(uint32_t memoryType, uint32_t bytes, uint32_t align, bool bCommit);

	void           DeallocateTiny(uint32_t memoryType, uint32_t pageId, uint32_t blockState);
	void           DeallocateSmall(uint32_t memoryType, uint32_t pageId, uint32_t blockState);
	void           DeallocateLarge(uint32_t memoryType, uint32_t pageId, bool bUnlink, bool bCommit);
	void           DeallocateHuge(uint32_t memoryType, uint32_t pageId, bool bCommit);

	const bool                     m_bMapPersistent;
	const bool                     m_bCommitRegions;
	const uint32_t                 m_numHeaps;
	CryCriticalSectionNonRecursive m_lock;
	Detail::UPage*                 m_pPages;
	Detail::SSubHeap*              m_pHeaps;
};
