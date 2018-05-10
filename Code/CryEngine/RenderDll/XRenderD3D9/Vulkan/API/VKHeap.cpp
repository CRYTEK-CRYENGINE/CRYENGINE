// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKHeap.hpp"

const VkDeviceSize kMaxPagesPerType = 128;          // Max number of pages per type
const VkDeviceSize kMinPageSize = 16 * 1024 * 1024; // Pages at least 16 MB each
const VkDeviceSize kMinPagePopulation = 90;         // Percent of page that should be used before allocating separate pages
const bool kMapPersistent = true;                   // Map all allocated host-visible pages persistently. Maybe turn off for 32-bit hosts?

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void* VKAPI_CALL NCryVulkan::CHostAllocator::CpuMalloc(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
	void* const pResult = CryModuleMemalign(size, alignment);
#if !defined(_RELEASE)
	if (pResult)
	{
		const size_t actualSize = CryModuleMemSize(pResult, alignment);
		volatile SPoolStats& stats = static_cast<CHostAllocator*>(pUserData)->AtStats(scope);
		CryInterlockedAdd(&stats.numAllocations, 1);
		CryInterlockedAdd(&stats.bytesAllocated, actualSize);
	}
#endif
	return pResult;
}

void* VKAPI_CALL NCryVulkan::CHostAllocator::CpuRealloc(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
#if !defined(_RELEASE)
	volatile SPoolStats& stats = static_cast<CHostAllocator*>(pUserData)->AtStats(scope);
	if (pOriginal)
	{
		const size_t previousSize = CryModuleMemSize(pOriginal, alignment);
		CryInterlockedAdd(&stats.numDeallocations, 1);
		CryInterlockedAdd(&stats.bytesDeallocated, previousSize);
	}
#endif
	void* const pResult = CryModuleReallocAlign(pOriginal, size, alignment);
#if !defined(_RELEASE)
	if (pResult)
	{
		const size_t actualSize = CryModuleMemSize(pResult, alignment);
		CryInterlockedAdd(&stats.numAllocations, 1);
		CryInterlockedAdd(&stats.bytesAllocated, actualSize);
	}
#endif
	return pResult;
}

void VKAPI_CALL NCryVulkan::CHostAllocator::CpuFree(void* pUserData, void* pMemory)
{
#if !defined(_RELEASE)
	if (pMemory)
	{
		const size_t previousSize = CryModuleMemSize(pMemory, 1);
		/* TODO: determine scope?
		SPoolStats& stats = static_cast<CHostAllocator*>(pUserData)->AtStats(???);
		CryInterlockedAdd(&stats.numDeallocations, 1);
		CryInterlockedAdd(&stats.bytesDeallocated, previousSize);
		*/
	}
#endif
	CryModuleMemalignFree(pMemory);
}

void VKAPI_CALL NCryVulkan::CHostAllocator::CpuForeignMalloc(void* pUserData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
{
#if !defined(_RELEASE)
	if (size)
	{
		volatile SPoolStats& stats = static_cast<CHostAllocator*>(pUserData)->AtStats(type);
		CryInterlockedAdd(&stats.numAllocations, 1);
		CryInterlockedAdd(&stats.bytesAllocated, size);
	}
#endif
}

void VKAPI_CALL NCryVulkan::CHostAllocator::CpuForeignFree(void* pUserData, size_t size, VkInternalAllocationType type, VkSystemAllocationScope scope)
{
#if !defined(_RELEASE)
	if (size)
	{
		volatile SPoolStats& stats = static_cast<CHostAllocator*>(pUserData)->AtStats(type);
		CryInterlockedAdd(&stats.numDeallocations, 1);
		CryInterlockedAdd(&stats.bytesDeallocated, size);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void NCryVulkan::CHeap::Init(VkPhysicalDevice physicalDevice, VkDevice device)
{
	// Get device information
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_heapProperties);
	m_device = device;

	// Collect largest heap information
	VkDeviceSize largestDeviceHeap = -1;
	VkDeviceSize largestHostHeap = -1;
	VkDeviceSize largestDeviceSize = 0;
	VkDeviceSize largestHostSize = 0;
	for (uint32_t i = 0; i < m_heapProperties.memoryHeapCount; ++i)
	{
		const VkMemoryHeap& heap = m_heapProperties.memoryHeaps[i];
		if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
		{
			if (heap.size > largestDeviceSize)
			{
				largestDeviceSize = heap.size;
				largestDeviceHeap = i;
			}
		}
		else if (heap.size > largestHostSize)
		{
			largestHostSize = heap.size;
			largestHostHeap = i;
		}
	}

	// Collect type flags
	uint32_t deviceLocalTypes = 0;
	uint32_t hostLocalTypes = 0;
	uint32_t hostVisibleTypes = 0;
	uint32_t hostCoherentTypes = 0;
	uint32_t hostCachedTypes = 0;
	for (uint32_t i = 0; i < m_heapProperties.memoryTypeCount; ++i)
	{
		const VkMemoryType& type = m_heapProperties.memoryTypes[i];
		if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		{
			deviceLocalTypes |= 1U << i;
		}
		else
		{
			hostLocalTypes |= 1U << i;
		}
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
		{
			hostVisibleTypes |= 1U << i;
			if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
			{
				hostCachedTypes |= 1U << i;
			}
			if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			{
				hostCoherentTypes |= 1U << i;
			}
		}
	}

	const uint32_t deviceStagingTypes = (hostLocalTypes ? deviceLocalTypes & hostVisibleTypes : 0);

	// Always take away the "upload" heap from all the heap-preferences (never fall back to "upload" heap)
	if (deviceLocalTypes  != deviceStagingTypes) deviceLocalTypes  &= ~deviceStagingTypes;
	if (hostLocalTypes    != deviceStagingTypes) hostLocalTypes    &= ~deviceStagingTypes;
	if (hostVisibleTypes  != deviceStagingTypes) hostVisibleTypes  &= ~deviceStagingTypes;
	if (hostCoherentTypes != deviceStagingTypes) hostCoherentTypes &= ~deviceStagingTypes;
	if (hostCachedTypes   != deviceStagingTypes) hostCachedTypes   &= ~deviceStagingTypes;

	// kHeapTargets must be device-local.
	m_heapPreferences[kHeapTargets] = deviceLocalTypes;

	// kHeapSources ideally should be device-local.
	// However, if there is less than 1GB of that, and there is a (much) larger host-local heap, we should probably pick that heap.
	if (largestDeviceSize < 1 * 1024 * 1024 * 1024 && largestHostSize > largestDeviceSize * 2)
	{
		if (hostVisibleTypes & ~hostCoherentTypes)
		{
			// We don't care about memory being incoherent for GPU-only resources, pick this if available
			m_heapPreferences[kHeapSources] = hostVisibleTypes & ~hostCoherentTypes;
		}
		else
		{
			m_heapPreferences[kHeapSources] = hostVisibleTypes;
		}
	}
	else
	{
		m_heapPreferences[kHeapSources] = deviceLocalTypes;
	}

	// kHeapDownload should be anything we can read cached, and ideally coherent.
	if (hostCachedTypes & hostCoherentTypes)
	{
		m_heapPreferences[kHeapDownload] = hostCachedTypes & hostCoherentTypes;
	}
	else
	{
		m_heapPreferences[kHeapDownload] = hostCachedTypes;
	}

	// kHeapUpload should be anything we can write to with the CPU, and ideally coherent.
	// If possible, avoid aliasing the download heap (ie, cached) since we want WC pages.
	if (hostCoherentTypes)
	{
		if (hostCoherentTypes & ~m_heapPreferences[kHeapDownload])
		{
			m_heapPreferences[kHeapUpload] = hostCoherentTypes & ~m_heapPreferences[kHeapDownload];
		}
		else
		{
			m_heapPreferences[kHeapUpload] = hostCoherentTypes;
		}
	}
	else
	{
		if (hostVisibleTypes & ~m_heapPreferences[kHeapDownload])
		{
			m_heapPreferences[kHeapUpload] = hostVisibleTypes & ~m_heapPreferences[kHeapDownload];
		}
		else
		{
			m_heapPreferences[kHeapUpload] = hostVisibleTypes;
		}
	}

	// kHeapDynamic must be host-visible, but ideally it's also device-local and coherent (in that order).
	if (deviceStagingTypes)
	{
		if (deviceStagingTypes & hostCoherentTypes)
		{
			m_heapPreferences[kHeapDynamic] = deviceStagingTypes & hostCoherentTypes;
		}
		else
		{
			m_heapPreferences[kHeapDynamic] = deviceStagingTypes;
		}
	}
	else
	{
		m_heapPreferences[kHeapDynamic] = m_heapPreferences[kHeapUpload];
	}

	LogHeapConfiguration();

	m_blockProbe = 0;
	m_blocks.resize(GetMaximumBlockHandle());
}

const char* NCryVulkan::CHeap::FormatSize(char buffer[10], uint64_t bytes)
{
	const char* szQuantity = "B";
	VkDeviceSize postFix = 0;
	if (bytes > 1024)
	{
		postFix = bytes % 1024;
		bytes /= 1024;
		szQuantity = "KB";
	}
	if (bytes > 1024)
	{
		postFix = bytes % 1024;
		bytes /= 1024;
		szQuantity = "MB";
	}
	if (bytes > 1024)
	{
		postFix = bytes % 1024;
		bytes /= 1024;
		szQuantity = "GB";
	}
	if (bytes > 1024)
	{
		postFix = bytes % 1024;
		bytes /= 1024;
		szQuantity = "TB";
	}
	postFix = postFix < 1000 ? postFix / 10 : 99;
	cry_sprintf(buffer, 10, "%u.%02u%s", bytes, postFix, szQuantity);
	return buffer;
};

void NCryVulkan::CHeap::LogHeapConfiguration() const
{
	VkDeviceSize totalBytes = 0;
	char buffer[10];
	for (uint32_t i = 0; i < m_heapProperties.memoryHeapCount; ++i)
	{
		CryLog("Vulkan memory heap %u (%s, %s)", i,
		       FormatSize(buffer, m_heapProperties.memoryHeaps[i].size),
		       (m_heapProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "CPU");
		CryLog("| pool# | page | coh || tgt | src | dwl | upl | dyn |");
		for (uint32_t j = 0; j < m_heapProperties.memoryTypeCount; ++j)
		{
			if (m_heapProperties.memoryTypes[j].heapIndex != i)
				continue;

			CryLog("|  %03u  |  %s  |  %c  ||  %c  |  %c  |  %c  |  %c  |  %c  |", j,
			       (m_heapProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? ((m_heapProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "WB" : "WC") : "--",
			       (m_heapProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? ((m_heapProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 'Y' : 'N') : '-',
			       (m_heapPreferences[kHeapTargets] & (1U << j)) ? 'X' : ' ',
			       (m_heapPreferences[kHeapSources] & (1U << j)) ? 'X' : ' ',
			       (m_heapPreferences[kHeapDownload] & (1U << j)) ? 'X' : ' ',
			       (m_heapPreferences[kHeapUpload] & (1U << j)) ? 'X' : ' ',
			       (m_heapPreferences[kHeapDynamic] & (1U << j)) ? 'X' : ' ');
		}
		totalBytes += m_heapProperties.memoryHeaps[i].size;
	}
	CryLog("Vulkan total GPU visible memory: %s", FormatSize(buffer, totalBytes));
}

namespace Detail
{
// Converts a C++ lambda to a C style callback (with an opaque first parameter of void*)
template<typename TFunctor>
struct DeduceCFunctor
{
	template<typename TResult, typename ... TArgs>
	static TResult WrappedCallback(void* pContext, TArgs ... args)
	{
		TFunctor& functor = *static_cast<TFunctor*>(pContext);
		return functor(args ...);
	}

	template<typename TResult, typename ... TArgs>
	static auto Select(TResult (TFunctor::* pfnMember)(TArgs ...) const)
	{
		return WrappedCallback<TResult, TArgs ...>;
	}

	static auto Deduce()
	->decltype(Select(&TFunctor::operator()))
	{
		return Select(&TFunctor::operator());
	}
};

// Wraps the given lambda into a C-style functor (ie, free function pointer).
// The returned free function pointer has a leading void* parameter to refer to the lambda context.
template<typename TLambda>
decltype(DeduceCFunctor<TLambda>::Deduce()) MakeCFunctor(TLambda& lambda)
{
	return DeduceCFunctor<TLambda>::Deduce();
}
}

void NCryVulkan::CHeap::LogHeapSummary() const
{
	struct STypeSummary
	{
		uint64_t committed;
		uint64_t allocated;
		uint32_t blockCount;
	} summary[VK_MAX_MEMORY_TYPES];

	uint32_t cpuStructuresUsed = 0;
	uint32_t cpuStructuresFree = 0;
	uint64_t gpuCommitted = 0;
	uint64_t gpuAllocated = 0;
	
	auto callback = [&](uint32_t memoryType, uint64_t totalApiAllocated, uint64_t totalClientAllocated, uint32_t totalClientHandles, uint32_t cpuSideOverhead)
	{
		summary[memoryType].committed = totalApiAllocated;
		summary[memoryType].allocated = totalClientAllocated;
		summary[memoryType].blockCount = totalClientHandles;
		(totalApiAllocated ? cpuStructuresUsed : cpuStructuresFree) += cpuSideOverhead;
	};

	cpuStructuresFree += Walk(&callback, Detail::MakeCFunctor(callback));
	
	char buffer[2][10];

	for (uint32_t heapIndex = 0; heapIndex < m_heapProperties.memoryHeapCount; ++heapIndex)
	{
		auto& heap = m_heapProperties.memoryHeaps[heapIndex];
		CryLog("Vulkan memory heap %u (%s %s) summary", heapIndex,
			FormatSize(buffer[0], heap.size),
			(heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "CPU");
		CryLog("| pool# | page | coh ||   commit   | allocated | blocks |");
		for (uint32_t typeIndex = 0; typeIndex < m_heapProperties.memoryTypeCount; ++typeIndex)
		{
			auto& type = m_heapProperties.memoryTypes[typeIndex];
			if (type.heapIndex != heapIndex)
				continue;

			CryLog("|  %03u  |  %s  |  %c  || %10s | %9s | %6u |", typeIndex,
				(type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? "WB" : "WC") : "--",
				(type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 'Y' : 'N') : '-',
				FormatSize(buffer[0], summary[typeIndex].committed),
				FormatSize(buffer[1], summary[typeIndex].allocated),
				summary[typeIndex].blockCount);

			gpuCommitted += summary[typeIndex].committed;
			gpuAllocated += summary[typeIndex].allocated;
		}
	}
	CryLog("Vulkan total GPU committed %s, allocated %s", FormatSize(buffer[0], gpuCommitted), FormatSize(buffer[1], gpuAllocated));
}

NCryVulkan::CMemoryHandle NCryVulkan::CHeap::Allocate(const VkMemoryRequirements& requirements, EHeapType heapHint)
{
	CMemoryHandle result;
	const uint32_t preferredTypes = m_heapPreferences[heapHint];
	const uint32_t memoryTypes[2] = { requirements.memoryTypeBits & preferredTypes, requirements.memoryTypeBits & ~preferredTypes };
	for (const uint32_t types : memoryTypes)
	{
		uint32_t remainingPools = types;
		while (remainingPools)
		{
			const uint32_t bit = countTrailingZeros32(remainingPools); // Order intended here, try lowest pool index first, as this is what Vulkan drivers may use to express their preference.
			remainingPools ^= 1U << bit;

			if (result.handle = CGpuHeap::Allocate(bit, static_cast<uint32_t>(requirements.size), static_cast<uint32_t>(requirements.alignment)))
			{
				return result;
			}
		}

		assert(types == 0 && "Memory fallback happened!");
	}
	return result;
}

NCryVulkan::CHeap::TBlockHandle NCryVulkan::CHeap::AllocateBlock(uint32_t memoryType, uint32_t bytes, uint32_t align)
{
	// Linear search for an empty slot in the block handle array.
	// We keep the last index that succeeded to reduce probe time.
	// This should be pretty fast, but we could also keep a stack of "empty" indices.
	auto itProbe = m_blocks.begin() + m_blockProbe;
	auto itEnd = m_blocks.end();
	auto itResult = itProbe + 1;
	for (; itResult != itEnd; ++itResult)
	{
		if (!*itResult)
		{
			break;
		}
	}
	if (itResult == itEnd)
	{
		for (itResult = m_blocks.begin(); itResult != itProbe; ++itResult)
		{
			if (!*itResult)
			{
				break;
			}
		}
	}
	if (itResult == itProbe)
	{
		// At maximum population
		// We should run out of memory long before this though.
		VK_ASSERT(false && "Out of block handles");
		return 0;
	}
	const TBlockHandle blockHandle = static_cast<TBlockHandle>(&*itResult - m_blocks.data());
	VK_ASSERT(blockHandle < m_blocks.size() && "Bad block handle");
	m_blockProbe = blockHandle;

	VkMemoryAllocateInfo info;
	info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.memoryTypeIndex = memoryType;
	info.allocationSize = bytes;

	VkDeviceMemory block;
	if (vkAllocateMemory(m_device, &info, nullptr, &block) == VK_SUCCESS)
	{
		m_blocks[blockHandle] = block;
		return blockHandle + 1;
	}

	return 0;
}

void NCryVulkan::CHeap::DeallocateBlock(uint32_t memoryType, TBlockHandle blockHandle, uint32_t bytes)
{
	VkDeviceMemory block = m_blocks[blockHandle - 1];
	m_blocks[blockHandle - 1] = 0;
	vkFreeMemory(m_device, block, nullptr);
}

void* NCryVulkan::CHeap::MapBlock(uint32_t memoryType, TBlockHandle blockHandle)
{
	if (m_heapProperties.memoryTypes[memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VkDeviceMemory block = m_blocks[blockHandle - 1];
		void* pResult;
		if (vkMapMemory(m_device, block, 0, VK_WHOLE_SIZE, 0, &pResult) == VK_SUCCESS)
		{
			return pResult;
		}
	}
	return nullptr;
}

void NCryVulkan::CHeap::UnmapBlock(uint32_t memoryType, TBlockHandle blockHandle, void* pAddress)
{
	(void)pAddress;

	if (m_heapProperties.memoryTypes[memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VkDeviceMemory block = m_blocks[blockHandle - 1];
		vkUnmapMemory(m_device, block);
	}
}
