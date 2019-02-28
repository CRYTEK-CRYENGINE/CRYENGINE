// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "VKBase.hpp"
#include "../../GpuHeap.h"

typedef std::pair<VkDeviceMemory, VkDeviceSize> VkDeviceMemoryLocation;

namespace NCryVulkan
{

// Represents a GPU-accessible heap allocation
class CMemoryHandle
{
public:
	CMemoryHandle()
		: handle(0)
	{}

	CMemoryHandle(CMemoryHandle&& other) noexcept
		: handle(other.handle)
	{
		other.handle = 0;
	}

	~CMemoryHandle()
	{
		VK_ASSERT(handle == 0 && "Must deallocate memory handle before destroying it");
	}

	CMemoryHandle& operator=(CMemoryHandle&& other)
	{
		VK_ASSERT(handle == 0 && "Must deallocate memory handle before overwriting it");
		handle = other.handle;
		other.handle = 0;
		return *this;
	}

	explicit operator bool() const
	{
		return handle != 0;
	}

private:
	friend class CHeap;
	CGpuHeap::THandle handle;
};

typedef std::pair<CMemoryHandle, VkDeviceMemoryLocation> CMemoryLocation;

// Supported predefined heap types.
// These are passed in as hint to the allocator on how the resource will be used.
enum EHeapType
{
	// Heap ID     // Vulkan heap properties                | CPU read | CPU write | Tiled | Intended GPU usage
	//-------------++---------------------------------------+----------+-----------+-------+-------------------
	kHeapTargets,  // DEVICE_LOCAL                          | No       | No        | Yes   | RTV, DSV, UAV
	kHeapSources,  // HOPEFULLY_DEVICE_LOCAL                | No       | No        | Yes   | SRV, CBV
	kHeapDownload, // HOST_VISIBLE | HOST_CACHED            | Yes      | Yes       | No    | copy-target
	kHeapUpload,   // HOST_VISIBLE                          | No       | Yes (WC)  | No    | copy-source
	kHeapDynamic,  // HOST_VISIBLE | HOPEFULLY_DEVICE_LOCAL | No       | Yes (WC)  | No    | SRV, CBV
	kHeapCount,
};

struct SPoolStats
{
	size_t numAllocations = 0;
	size_t numDeallocations = 0;
	size_t bytesAllocated = 0;
	size_t bytesDeallocated = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPU heap manager
class CHostAllocator
{
public:
	// Fills a callback struct that can be passed into the Vk API at several points.
	// This implementation performs statistics tracking in non-release builds.
	void GetCpuHeapCallbacks(VkAllocationCallbacks& callbacks)
	{
		callbacks.pUserData = this;
		callbacks.pfnAllocation = CpuMalloc;
		callbacks.pfnReallocation = CpuRealloc;
		callbacks.pfnFree = CpuFree;
		callbacks.pfnInternalAllocation = CpuForeignMalloc;
		callbacks.pfnInternalFree = CpuForeignFree;
	}

private:
	// Entry-points for VkAllocationCallbacks
	static void* VKAPI_PTR CpuMalloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope scope);
	static void* VKAPI_PTR CpuRealloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope scope);
	static void VKAPI_PTR  CpuFree(void* pUserData, void* pMemory);
	static void VKAPI_PTR  CpuForeignMalloc(void* pUserData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope);
	static void VKAPI_PTR  CpuForeignFree(void* pUserData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope);

#if !defined(_RELEASE)
	static const size_t        kNumPools = VK_SYSTEM_ALLOCATION_SCOPE_RANGE_SIZE + VK_INTERNAL_ALLOCATION_TYPE_RANGE_SIZE;
	volatile struct SPoolStats m_stats[kNumPools];

	// Access stats structure for some pool.
	volatile SPoolStats& AtStats(VkSystemAllocationScope scope) { return m_stats[scope]; }
	volatile SPoolStats& AtStats(VkInternalAllocationType type) { return m_stats[type + VK_SYSTEM_ALLOCATION_SCOPE_RANGE_SIZE]; }
#endif
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GPU heap manager
class CHeap final : CGpuHeap
{
public:
	CHeap() : CGpuHeap(VK_MAX_MEMORY_TYPES) {}
	CHeap(const CHeap&) = delete;
	CHeap(CHeap&&) = delete;

	// Initialize the heap for a given physical device.
	// This sets up preference information for heap types.
	void Init(VkPhysicalDevice physicalDevice, VkDevice actualDevice);

	// Allocates a memory block matching the specified requirements.
	// The heapHint is used to select a preferred heap for a resource of that type.
	CMemoryHandle Allocate(const VkMemoryRequirements& requirements, EHeapType heapHint);

	// Get the bind-address for a resource.
	VkDeviceMemoryLocation GetBindAddress(CMemoryHandle& handle)
	{
		uint32_t offset32;
		const TBlockHandle blockHandle = GetBlockHandle(handle.handle, offset32, false);
		return VkDeviceMemoryLocation(m_blocks[blockHandle - 1], offset32);
	}

	uint32_t GetFlags(const CMemoryHandle& handle) const
	{
		return m_heapProperties.memoryTypes[GetMemoryType(handle.handle)].propertyFlags;
	}

	// Deallocates a memory block previously return by AllocateXXX
	void Deallocate(CMemoryHandle& handle)
	{
		CGpuHeap::Deallocate(handle.handle);
		handle.handle = 0;
	}

	// Retrieve actual size of the allocation (which may be larger than request)
	VkDeviceSize GetSize(const CMemoryHandle& handle) const
	{
		return CGpuHeap::GetSize(handle.handle);
	}

	// Retrieve the minimum actual size of an allocation, without actually performing the allocation.
	// ie; GetSize(Allocate(requirements)) >= AdjustSize(requirements).
	VkDeviceSize GetAdjustedSize(const VkMemoryRequirements& requirements) const
	{
		return CGpuHeap::GetAdjustedSize(static_cast<uint32>(requirements.size), static_cast<uint32>(requirements.alignment));
	}

	// Maps memory for CPU access
	void* Map(CMemoryHandle& handle)
	{
		return CGpuHeap::Map(handle.handle);
	}

	// Unmaps memory for CPU access
	void Unmap(CMemoryHandle& handle)
	{
		CGpuHeap::Unmap(handle.handle);
	}

	void LogHeapConfiguration() const;
	void LogHeapSummary() const;

private:
	// Vulkan-specific heap implementation
	virtual TBlockHandle AllocateBlock(uint32_t memoryType, uint32_t bytes, uint32_t align) override;
	virtual void         DeallocateBlock(uint32_t memoryType, TBlockHandle blockHandle, uint32_t bytes) override;
	virtual void*        MapBlock(uint32_t memoryType, TBlockHandle blockHandle) override;
	virtual void         UnmapBlock(uint32_t memoryType, TBlockHandle blockHandle, void* pAddress) override;

	static const char*   FormatSize(char buffer[10], uint64_t bytes);

	VkPhysicalDeviceMemoryProperties m_heapProperties;
	uint32_t                         m_heapPreferences[kHeapCount];
	VkDevice                         m_device;
	std::vector<VkDeviceMemory>      m_blocks;
	TBlockHandle                     m_blockProbe;
};

}
