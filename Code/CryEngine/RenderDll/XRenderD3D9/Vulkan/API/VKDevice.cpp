// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKDevice.hpp"
#include "VKInstance.hpp"

namespace NCryVulkan
{

static int g_cvar_vk_heap_summary;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

_smart_ptr<CDevice> CDevice::Create(const SPhysicalDeviceInfo* pDeviceInfo, VkAllocationCallbacks* hostAllocator, const std::vector<const char*>& layersToEnable, const std::vector<const char*>& extensionsToEnable)
{
	std::vector<VkDeviceQueueCreateInfo> queueRequests;
	std::vector<float> queuePriorities(64, 1.0f);

	VkDeviceQueueCreateInfo QueueInfo;
	ZeroStruct(QueueInfo);

	QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	QueueInfo.flags = 0;

	for (int i = 0, n = pDeviceInfo->queueFamilyProperties.size(); i < n; ++i)
	{
		QueueInfo.queueFamilyIndex = i;
		QueueInfo.queueCount = pDeviceInfo->queueFamilyProperties[i].queueCount;
		QueueInfo.pQueuePriorities = queuePriorities.data();
		VK_ASSERT(QueueInfo.queueCount <= 64);

		queueRequests.push_back(QueueInfo);
	}

	VkDeviceCreateInfo DeviceInfo;
	ZeroStruct(DeviceInfo);

	DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	DeviceInfo.queueCreateInfoCount = queueRequests.size();
	DeviceInfo.pQueueCreateInfos = queueRequests.data();
	DeviceInfo.enabledLayerCount = layersToEnable.size();
	DeviceInfo.ppEnabledLayerNames = layersToEnable.size() > 0 ? layersToEnable.data() : nullptr;
	DeviceInfo.enabledExtensionCount = extensionsToEnable.size();
	DeviceInfo.ppEnabledExtensionNames = extensionsToEnable.size() > 0 ? extensionsToEnable.data() : nullptr;
	DeviceInfo.pEnabledFeatures = &pDeviceInfo->deviceFeatures;

	VkDevice Device = VK_NULL_HANDLE;
	VkResult res = vkCreateDevice(pDeviceInfo->device, &DeviceInfo, hostAllocator, &Device);
	if (res == VK_ERROR_LAYER_NOT_PRESENT)
	{
		// (debug) layer not installed, don't consider fatal.
		DeviceInfo.enabledLayerCount = 0;
		DeviceInfo.ppEnabledLayerNames = nullptr;

		res = vkCreateDevice(pDeviceInfo->device, &DeviceInfo, hostAllocator, &Device);
		if (res == VK_SUCCESS)
		{
			// In case we succeed now, at least put this in the log.
			// If not, use the below error handling.
			CryLogAlways("vkCreateDevice: Discarded %" PRISIZE_T " layers during Vulkan initialization", layersToEnable.size());
		}
	}

	_smart_ptr<CDevice> pSmart;
	pSmart.Assign_NoAddRef(new CDevice(pDeviceInfo, hostAllocator, Device));
	return pSmart;
}

//---------------------------------------------------------------------------------------------------------------------
CDevice::CDevice(const SPhysicalDeviceInfo* pDeviceInfo, VkAllocationCallbacks* hostAllocator, VkDevice Device)
	: m_pDeviceInfo(pDeviceInfo)
	, m_Allocator(*hostAllocator)
	, m_device(Device)
	, m_pipelineCache(VK_NULL_HANDLE)
	// Must be constructed last as it relies on functionality from the heaps
	, m_Scheduler(this)
{
	GetHeap().Init(pDeviceInfo->device, Device);
#ifndef _RELEASE
	REGISTER_CVAR2("vk_heap_summary", &g_cvar_vk_heap_summary, 0, VF_DEV_ONLY, "Set to non-zero to log summary of heap state (once)");
#endif

	// allocate descriptor pool
	const uint32 setCount                  = 65535;
	const uint32 sampledImageCount         = 32 * 65536;
	const uint32 storageImageCount         = 1  * 65536;
	const uint32 uniformBufferCount        = 1  * 65536;
	const uint32 uniformBufferDynamicCount = 4  * 65536;
	const uint32 storageBufferCount        = 1  * 65536;
	const uint32 uniformTexelBufferCount   = 8192;
	const uint32 storageTexelBufferCount   = 8192;
	const uint32 samplerCount              = 2  * 65536;

	VkDescriptorPoolSize poolSizes[8];

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	poolSizes[0].descriptorCount = sampledImageCount;

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = storageImageCount;

	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = uniformBufferCount;

	poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[3].descriptorCount = uniformBufferDynamicCount;

	poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[4].descriptorCount = storageBufferCount;

	poolSizes[5].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	poolSizes[5].descriptorCount = samplerCount;

	poolSizes[6].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	poolSizes[6].descriptorCount = uniformTexelBufferCount;

	poolSizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	poolSizes[7].descriptorCount = storageTexelBufferCount;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allocated descriptor sets will release their allocations back to the pool
	descriptorPoolCreateInfo.maxSets = setCount;
	descriptorPoolCreateInfo.poolSizeCount = CRY_ARRAY_COUNT(poolSizes);
	descriptorPoolCreateInfo.pPoolSizes = poolSizes;

	VkResult result = vkCreateDescriptorPool(GetVkDevice(), &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);

	if (result != VK_SUCCESS)
		CryFatalError("Failed to initialize global descriptor pool!");

	if (!m_occlusionQueries.Init(GetVkDevice()))
		CRY_ASSERT_MESSAGE(false, "Failed to initialize occlusion queries");

	m_Scheduler.BeginScheduling();
}

CDevice::~CDevice()
{
	vkDeviceWaitIdle(m_device);

	for (uint32_t i = 0; i < kDeferTicks; ++i)
	{
		TickDestruction();
	}

	if (m_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(m_device, &m_Allocator);
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CDevice::DeferDestruction(CBufferView&& view)
{
	if (view.GetHandle() != VK_NULL_HANDLE)
	{
		CryAutoCriticalSection lock(m_deferredLock);
		m_deferredBufferViews->emplace_back(std::move(view));
	}
}

void CDevice::DeferDestruction(CImageView&& view)
{
	CryAutoCriticalSection lock(m_deferredLock);
	m_deferredImageViews->emplace_back(std::move(view));
}

void CDevice::DeferDestruction(CSampler&& sampler)
{
	CryAutoCriticalSection lock(m_deferredLock);
	m_deferredSamplers->emplace_back(std::move(sampler));
}

void CDevice::DeferDestruction(VkRenderPass renderPass, VkFramebuffer frameBuffer)
{
	SRenderPass helper = { GetVkDevice(), renderPass, frameBuffer };
	CryAutoCriticalSection lock(m_deferredLock);
	m_deferredRenderPasses->emplace_back(std::move(helper));
}

CDevice::SRenderPass::~SRenderPass()
{
	renderPass.Destroy(vkDestroyRenderPass, self);
	frameBuffer.Destroy(vkDestroyFramebuffer, self);
}

template<typename T, size_t N>
static std::vector<T> TickDestructionHelper(std::vector<T>(&collections)[N])
{
	std::vector<T> result;

	using std::swap;
	for (size_t i = 0; i < N; ++i)
	{
		swap(result, collections[i]);
	}

	return result;
}

void CDevice::TickDestruction()
{
#if !defined(_RELEASE)
	{
		// TODO: Move this to a general tick function?
		if (g_cvar_vk_heap_summary)
		{
			g_cvar_vk_heap_summary = 0;
			m_heap.LogHeapSummary();
		}
	}
#endif

	m_deferredLock.Lock();
	auto bufferViews = TickDestructionHelper(m_deferredBufferViews);
	auto imageViews = TickDestructionHelper(m_deferredImageViews);
	auto samplers = TickDestructionHelper(m_deferredSamplers);
	auto renderPasses = TickDestructionHelper(m_deferredRenderPasses);
	m_deferredLock.Unlock();

	// Automatic cleanup happens here.
}

//---------------------------------------------------------------------------------------------------------------------
VkResult vkBindResourceMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) { return vkBindBufferMemory(device, buffer, memory, memoryOffset); }
VkResult vkBindResourceMemory(VkDevice device, VkImage  image , VkDeviceMemory memory, VkDeviceSize memoryOffset) { return vkBindImageMemory (device, image , memory, memoryOffset); }

void vkGetResourceMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) { vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements); }
void vkGetResourceMemoryRequirements(VkDevice device, VkImage  image , VkMemoryRequirements* pMemoryRequirements) { vkGetImageMemoryRequirements (device, image , pMemoryRequirements); }

VkResult vkCreateResource(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) { return vkCreateBuffer(device, pCreateInfo, pAllocator, pBuffer); }
VkResult vkCreateResource(VkDevice device, const VkImageCreateInfo * pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage * pImage ) { return vkCreateImage (device, pCreateInfo, pAllocator, pImage ); }

void vkDestroyResource(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) { vkDestroyBuffer(device, buffer, pAllocator); }
void vkDestroyResource(VkDevice device, VkImage  image , const VkAllocationCallbacks* pAllocator) { vkDestroyImage (device, image , pAllocator); }

CryCriticalSectionNonRecursive CDevice::m_ReleaseHeapTheadSafeScope[2];
CryCriticalSectionNonRecursive CDevice::m_RecycleHeapTheadSafeScope[2];

template<> CryCriticalSectionNonRecursive& CDevice::GetReleaseHeapCriticalSection<CBufferResource>() { return m_ReleaseHeapTheadSafeScope[0]; }
template<> CryCriticalSectionNonRecursive& CDevice::GetReleaseHeapCriticalSection<CImageResource >() { return m_ReleaseHeapTheadSafeScope[1]; }
template<> CryCriticalSectionNonRecursive& CDevice::GetRecycleHeapCriticalSection<CBufferResource>() { return m_RecycleHeapTheadSafeScope[0]; }
template<> CryCriticalSectionNonRecursive& CDevice::GetRecycleHeapCriticalSection<CImageResource >() { return m_RecycleHeapTheadSafeScope[1]; }
template<> CDevice::TReleaseHeap<CBufferResource>& CDevice::GetReleaseHeap<CBufferResource>() { return m_BufferReleaseHeap; }
template<> CDevice::TReleaseHeap<CImageResource >& CDevice::GetReleaseHeap<CImageResource >() { return m_ImageReleaseHeap; }
template<> CDevice::TRecycleHeap<CBufferResource>& CDevice::GetRecycleHeap<CBufferResource>() { return m_BufferRecycleHeap; }
template<> CDevice::TRecycleHeap<CImageResource >& CDevice::GetRecycleHeap<CImageResource >() { return m_ImageRecycleHeap; }

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
VkResult CDevice::CommitResource(EHeapType heapHint, CResource* pInputResource) threadsafe
{
	VkMemoryRequirements requirements;
	vkGetResourceMemoryRequirements(GetVkDevice(), pInputResource->GetHandle(), &requirements);

	CMemoryHandle boundMemory;
	if (boundMemory = GetHeap().Allocate(requirements, heapHint))
	{
		VkDeviceMemoryLocation memloc = GetHeap().GetBindAddress(boundMemory);

		VkResult result;
		if ((result = vkBindResourceMemory(GetVkDevice(), pInputResource->GetHandle(), memloc.first, memloc.second)) == VK_SUCCESS)
		{
			pInputResource->CMemoryResource::Init(heapHint, std::move(boundMemory));

			return VK_SUCCESS;
		}

		GetHeap().Deallocate(boundMemory);
		return result;
	}

	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource, class VkCreateInfo>
VkResult CDevice::CreateCommittedResource(EHeapType heapHint, const VkCreateInfo& createInfo, CResource** ppOutputResource) threadsafe
{
	typename CResource::VkResource hVkResource = VK_NULL_HANDLE;

	VkResult result = vkCreateResource(GetVkDevice(), &createInfo, nullptr, &hVkResource);
	if (result == VK_SUCCESS)
	{
		CResource* pVkResource = new CResource(this);
		pVkResource->CResource::Init(hVkResource, createInfo);

		// Commit memory for the resource. TODO: Don't always commit everything?
		if ((result = CommitResource(heapHint, pVkResource)) == VK_SUCCESS)
		{
			*ppOutputResource = pVkResource;

			return VK_SUCCESS;
		}

		vkDestroyResource(GetVkDevice(), hVkResource, nullptr);
		delete pVkResource;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
VkResult CDevice::DuplicateCommittedResource(CResource* pInputResource, CResource** ppOutputResource) threadsafe
{
	return CreateOrReuseCommittedResource<CResource, typename CResource::VkCreateInfo>(pInputResource->GetHeapType(), pInputResource->GetCreateInfo(), ppOutputResource);
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
VkResult CDevice::SubstituteUsedCommittedResource(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CResource** ppSubstituteResource) threadsafe
{
	const auto& fenceManager = GetScheduler().GetFenceManager();
	if (fenceManager.IsCompleted(fenceValues[CMDQUEUE_GRAPHICS], CMDQUEUE_GRAPHICS) &&
		fenceManager.IsCompleted(fenceValues[CMDQUEUE_COMPUTE ], CMDQUEUE_COMPUTE ) &&
		fenceManager.IsCompleted(fenceValues[CMDQUEUE_COPY    ], CMDQUEUE_COPY    ))
	{
		// Can continued to be used without substitution
		return VK_NOT_READY;
	}

	CResource* pDisposableResource = *ppSubstituteResource;
	VkResult result = DuplicateCommittedResource(pDisposableResource, ppSubstituteResource);

	if (result == VK_SUCCESS && *ppSubstituteResource != nullptr)
	{
		pDisposableResource->Release(); // trigger ReleaseLater with kResourceFlagReusable

		//	TODO: ReleaseLater(fenceValues, pDisposableResource, pDisposableResource->GetFlag(kResourceFlagReusable));

		return VK_SUCCESS;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
VkResult CDevice::CreateOrReuseStagingResource(CResource* pInputResource, VkDeviceSize minSize, CBufferResource** ppStagingResource, bool bUpload) threadsafe
{
	VkBufferCreateInfo createInfo;
	ZeroStruct(createInfo);

	VkMemoryRequirements minRequirements;
	minRequirements.size = minSize;
	minRequirements.alignment = 1;

	const auto& pResourceDesc = pInputResource->GetCreateInfo();

	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.size = GetHeap().GetAdjustedSize(minRequirements); // Note: Adjusted size is used to improve hash hit ratio (ie, resources of 250 and 251 bytes hash the same).
	createInfo.usage = bUpload ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // pResourceDesc.sharingMode;
	createInfo.queueFamilyIndexCount = pResourceDesc.queueFamilyIndexCount;
	createInfo.pQueueFamilyIndices = pResourceDesc.pQueueFamilyIndices;

	EHeapType heapType = bUpload ? kHeapUpload : kHeapDownload;

	CBufferResource* stagingResource = nullptr;
	VkResult result = CreateOrReuseCommittedResource(
		heapType,
		createInfo,
		&stagingResource);

	if (result == VK_SUCCESS && stagingResource != nullptr)
	{
		*ppStagingResource = stagingResource;
		stagingResource->SetStrideAndElementCount(1, static_cast<uint32>(minSize)); // Treat as a byte buffer, which is always valid
		return VK_SUCCESS;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------

static THash CalculateResourceHash(const VkBufferCreateInfo& createInfo, EHeapType HeapHint)
{
	uint32 hashableData[] = 
	{
		HeapHint,
		createInfo.flags, 
		createInfo.sharingMode, 
		uint32(createInfo.size >> 32), 
		uint32(createInfo.size),
		createInfo.usage
	};

	return ComputeSmallHash<sizeof(hashableData)>(hashableData);
}

static THash CalculateResourceHash(const VkImageCreateInfo& createInfo, EHeapType HeapHint)
{
	uint32 hashableData[] =
	{
		HeapHint,
		createInfo.flags,
		createInfo.imageType,
		createInfo.format,
		createInfo.extent.width,
		createInfo.extent.height,
		createInfo.extent.depth,
		createInfo.mipLevels,
		createInfo.arrayLayers,
		createInfo.samples,
		createInfo.tiling,
		createInfo.usage,
		createInfo.sharingMode
	};

	return ComputeSmallHash<sizeof(hashableData)>(hashableData);
}

//---------------------------------------------------------------------------------------------------------------------

template<class CResource, class VkCreateInfo>
VkResult CDevice::CreateOrReuseCommittedResource(EHeapType HeapHint, const VkCreateInfo& createInfo, CResource** ppOutputResource) threadsafe
{
	TRecycleHeap<CResource>& RecycleHeap = GetRecycleHeap<CResource>();
	THash hHash = CalculateResourceHash(createInfo, HeapHint);
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(GetRecycleHeapCriticalSection<CResource>());

		// Best-case O(1) lookup
		typename TRecycleHeap<CResource>::iterator result = RecycleHeap.find(hHash); // TODO: This is bugged in case of hash collision! At least check key equality (in non-release?)
		if (result != RecycleHeap.end())
		{
			if (ppOutputResource)
			{
				(*ppOutputResource = result->second.pObject)->AddRef();
				VK_ASSERT(result->second.pObject->IsUniquelyOwned(), "Ref-Counter of VK resource is not 1, implementation will crash!");

				RecycleHeap.erase(result);
			}

			return VK_SUCCESS;
		}
	}

	VkResult result = CreateCommittedResource(HeapHint, createInfo, ppOutputResource);

	if (result == VK_SUCCESS)
	{
		(*ppOutputResource)->AddFlags(kResourceFlagReusable);
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
void CDevice::FlushReleaseHeap(const UINT64 (&completedFenceValues)[CMDQUEUE_NUM], const UINT64 (&pruneFenceValues)[CMDQUEUE_NUM]) threadsafe
{
	TReleaseHeap<CResource>& ReleaseHeap = GetReleaseHeap<CResource>();
	TRecycleHeap<CResource>& RecycleHeap = GetRecycleHeap<CResource>();

	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(GetReleaseHeapCriticalSection<CResource>());

		typename TReleaseHeap<CResource>::iterator it, nx;
		for (it = ReleaseHeap.begin(); it != ReleaseHeap.end(); it = nx)
		{
			nx = it;
			++nx;

			if (((it->second.fenceValues[CMDQUEUE_GRAPHICS]) <= completedFenceValues[CMDQUEUE_GRAPHICS]) /* &&
			    ((it->second.fenceValues[CMDQUEUE_COMPUTE ]) <= completedFenceValues[CMDQUEUE_COMPUTE ]) &&
			    ((it->second.fenceValues[CMDQUEUE_COPY    ]) <= completedFenceValues[CMDQUEUE_COPY    ])*/)
			{
				if (it->second.bFlags & 1)
				{
					CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(GetRecycleHeapCriticalSection<CResource>());

					RecycleInfo<CResource> recycleInfo;

					recycleInfo.pObject = it->first;
					recycleInfo.fenceValues[CMDQUEUE_GRAPHICS] = completedFenceValues[CMDQUEUE_GRAPHICS];
					recycleInfo.fenceValues[CMDQUEUE_COMPUTE ] = completedFenceValues[CMDQUEUE_COMPUTE ];
					recycleInfo.fenceValues[CMDQUEUE_COPY    ] = completedFenceValues[CMDQUEUE_COPY    ];

					// TODO: could be accumulated to a local list and batch-merged in the next code-block to prevent the locking
					RecycleHeap.emplace(it->second.hHash, std::move(recycleInfo));
				}
				else
				{
				//	it->first->Release();
					VK_ASSERT(it->first->IsDeletable(), "Ref-Counter of VK resource is not 0, implementation will crash!");
					delete it->first;
				}

				ReleaseHeap.erase(it);
			}
		}
	}
	
	//	if (0)
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(GetRecycleHeapCriticalSection<CResource>());

		typename TRecycleHeap<CResource>::iterator it, nx;
		for (it = RecycleHeap.begin(); it != RecycleHeap.end(); it = nx)
		{
			nx = it;
			++nx;

			if (((it->second.fenceValues[CMDQUEUE_GRAPHICS]) <= pruneFenceValues[CMDQUEUE_GRAPHICS]) /*&&
			    ((it->second.fenceValues[CMDQUEUE_COMPUTE ]) <= pruneFenceValues[CMDQUEUE_COMPUTE ]) &&
			    ((it->second.fenceValues[CMDQUEUE_COPY    ]) <= pruneFenceValues[CMDQUEUE_COPY    ])*/)
			{
			//	ULONG counter = it->second.pObject->Release();
				VK_ASSERT(it->second.pObject->IsDeletable(), "Ref-Counter of VK resource is not 0, implementation will crash!");
				delete it->second.pObject;
				RecycleHeap.erase(it);
			}
		}
	}
}

void CDevice::FlushReleaseHeaps(const UINT64 (&completedFenceValues)[CMDQUEUE_NUM], const UINT64 (&pruneFenceValues)[CMDQUEUE_NUM]) threadsafe
{
	FlushReleaseHeap<CBufferResource>(completedFenceValues, pruneFenceValues);
	FlushReleaseHeap<CImageResource >(completedFenceValues, pruneFenceValues);
}

//---------------------------------------------------------------------------------------------------------------------
template<class CResource>
void CDevice::ReleaseLater(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CResource* pObject, bool bReusable) threadsafe
{
	if (pObject == VK_NULL_HANDLE)
		return;

	TReleaseHeap<CResource>& ReleaseHeap = GetReleaseHeap<CResource>();

	ReleaseInfo releaseInfo;
	releaseInfo.hHash = CalculateResourceHash(pObject->GetCreateInfo(), pObject->GetHeapType());
	releaseInfo.bFlags = bReusable ? 1 : 0;
	releaseInfo.fenceValues[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
	releaseInfo.fenceValues[CMDQUEUE_COMPUTE ] = fenceValues[CMDQUEUE_COMPUTE ];
	releaseInfo.fenceValues[CMDQUEUE_COPY    ] = fenceValues[CMDQUEUE_COPY    ];

	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(GetReleaseHeapCriticalSection<CResource>());

		std::pair<typename TReleaseHeap<CResource>::iterator, bool> result = ReleaseHeap.emplace(pObject, std::move(releaseInfo));

		VK_ASSERT(pObject->IsDeletable(), "Ref-Counter is not 0, this might work, but is bad practice.");
	//	// Only count if insertion happened
	//	if (result.second)
	//	{
	//		pObject->AddRef();
	//	}
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Explicit instantiations
template VkResult CDevice::CommitResource<CBufferResource>(EHeapType HeapHint, CBufferResource* pInputResource) threadsafe;
template VkResult CDevice::CommitResource<CImageResource >(EHeapType HeapHint, CImageResource * pInputResource) threadsafe;
template VkResult CDevice::CreateCommittedResource<CBufferResource, VkBufferCreateInfo>(EHeapType HeapHint, const VkBufferCreateInfo& createInfo, CBufferResource** ppOutputResource) threadsafe;
template VkResult CDevice::CreateCommittedResource<CDynamicOffsetBufferResource, VkBufferCreateInfo>(EHeapType HeapHint, const VkBufferCreateInfo& createInfo, CDynamicOffsetBufferResource** ppOutputResource) threadsafe;
template VkResult CDevice::CreateCommittedResource<CImageResource, VkImageCreateInfo >(EHeapType HeapHint, const VkImageCreateInfo & createInfo, CImageResource ** ppOutputResource) threadsafe;
template VkResult CDevice::DuplicateCommittedResource<CBufferResource>(CBufferResource* pInputResource, CBufferResource** ppOutputResource) threadsafe;
template VkResult CDevice::DuplicateCommittedResource<CImageResource >(CImageResource * pInputResource, CImageResource ** ppOutputResource) threadsafe;
template VkResult CDevice::SubstituteUsedCommittedResource<CBufferResource>(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CBufferResource** ppSubstituteResource) threadsafe;
template VkResult CDevice::SubstituteUsedCommittedResource<CImageResource >(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CImageResource ** ppSubstituteResource) threadsafe;
template VkResult CDevice::CreateOrReuseStagingResource<CBufferResource>(CBufferResource* pInputResource, VkDeviceSize minSize, CBufferResource** ppStagingResource, bool Upload) threadsafe;
template VkResult CDevice::CreateOrReuseStagingResource<CImageResource >(CImageResource * pInputResource, VkDeviceSize minSize, CBufferResource** ppStagingResource, bool Upload) threadsafe;
template VkResult CDevice::CreateOrReuseCommittedResource<CBufferResource, VkBufferCreateInfo>(EHeapType HeapHint, const VkBufferCreateInfo& createInfo, CBufferResource** ppOutputResource) threadsafe;
template VkResult CDevice::CreateOrReuseCommittedResource<CImageResource , VkImageCreateInfo >(EHeapType HeapHint, const VkImageCreateInfo & createInfo, CImageResource ** ppOutputResource) threadsafe;
template void     CDevice::ReleaseLater<CBufferResource>(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CBufferResource* pObject, bool bReusable) threadsafe;
template void     CDevice::ReleaseLater<CImageResource >(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], CImageResource * pObject, bool bReusable) threadsafe;

//---------------------------------------------------------------------------------------------------------------------
void CDevice::FlushAndWaitForGPU()
{
	// Submit pending command-lists in case there are left-overs, make sure it's flushed to and executed on the hardware
	m_Scheduler.Flush(false);
	m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).GetAsyncCommandQueue().Flush();
	m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).WaitForFenceOnCPU();
}

}
