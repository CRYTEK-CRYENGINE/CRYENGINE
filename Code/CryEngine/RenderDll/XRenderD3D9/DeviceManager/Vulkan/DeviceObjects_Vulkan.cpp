// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Vulkan/API/VKInstance.hpp"
#include "Vulkan/API/VKBufferResource.hpp"
#include "Vulkan/API/VKImageResource.hpp"
#include "Vulkan/API/VKSampler.hpp"
#include "Vulkan/API/VKExtensions.hpp"

#include <Common/Renderer.h>

#include "DeviceResourceSet_Vulkan.h"	
#include "DevicePSO_Vulkan.h"
#include "DeviceCommandListCommon_Vulkan.h"	
#include "DeviceRenderPass_Vulkan.h"


extern CD3D9Renderer gcpRendD3D;

using namespace NCryVulkan;

extern VkShaderStageFlags GetShaderStageFlags(EShaderStage shaderStages);

extern bool GetBlockSize(VkFormat format, uint32_t& blockWidth, uint32_t& blockHeight, uint32_t& blockBytes);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



static auto lambdaCeaseCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->CeaseCommandListEvent(nPoolId);
};

static auto lambdaResumeCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->ResumeCommandListEvent(nPoolId);
};




CDeviceObjectFactory::CDeviceObjectFactory()
	: m_inlineConstantBufferLayout(VK_NULL_HANDLE)
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_pCoreCommandList.reset(new CDeviceCommandList());
	m_pCoreCommandList->m_sharedState.pCommandList = nullptr;

	m_pVKDevice    = nullptr;
	m_pVKScheduler = nullptr;
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
	m_pVKDevice    = pDevice;
	m_pVKScheduler = &m_pVKDevice->GetScheduler();

	m_pVKScheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaCeaseCallback, NCryVulkan::CCommandScheduler::CMDQUEUE_EVENT_CEASE);
	m_pVKScheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaResumeCallback, NCryVulkan::CCommandScheduler::CMDQUEUE_EVENT_RESUME);

	m_pCoreCommandList->m_sharedState.pCommandList = m_pVKScheduler->GetCommandList(CMDQUEUE_GRAPHICS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_Vulkan>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_Vulkan>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_Vulkan>(GetDevice(), flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	auto pResult = std::make_shared<CDeviceResourceLayout_Vulkan>(GetDevice(), resourceLayoutDesc.GetRequiredResourceBindings());
	if (pResult->Init(resourceLayoutDesc))
		return pResult;

	return nullptr;
}

const std::vector<uint8>* CDeviceObjectFactory::LookupResourceLayoutEncoding(uint64 layoutHash)
{
	auto encodedLayout = m_encodedResourceLayouts.find(layoutHash);
	if (encodedLayout != m_encodedResourceLayouts.end())
		return &m_encodedResourceLayouts[layoutHash];

	return nullptr;
}

void CDeviceObjectFactory::RegisterEncodedResourceLayout(uint64 layoutHash, std::vector<uint8>&& encodedLayout)
{
	if (m_encodedResourceLayouts.find(layoutHash) != m_encodedResourceLayouts.end())
		CryFatalError("An encoded resource layout with provided hash is already available!");
	m_encodedResourceLayouts[layoutHash] = std::move(encodedLayout);
}

void CDeviceObjectFactory::UnRegisterEncodedResourceLayout(uint64 layoutHash)
{
	m_encodedResourceLayouts.erase(layoutHash);
}

uint32 CDeviceObjectFactory::GetEncodedResourceLayoutSize(const std::vector<uint8>& encodedLayout)
{
	uint32 encodedResourceLayoutSize = 0;

	if (encodedLayout.size() > 0)
	{
		encodedResourceLayoutSize++;
		uint8 numberOfResourceLayout = encodedLayout[0];
		for (uint i = 0; i < numberOfResourceLayout; ++i)
		{
			encodedResourceLayoutSize += 2 * encodedLayout[encodedResourceLayoutSize] + 1;
		}
	}

	return encodedResourceLayoutSize;
}

VkDescriptorSetLayout CDeviceObjectFactory::GetInlineConstantBufferLayout()
{
	// NOTE: In order to avoid building a set of descriptors for each constant buffer
	// (due to sub allocation) we currently set stageFlags to all possible stages.
	if (m_inlineConstantBufferLayout == VK_NULL_HANDLE)
	{
		VkDescriptorSetLayoutBinding cbBinding;
		cbBinding.binding = 0;
		cbBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		cbBinding.descriptorCount = 1;
		cbBinding.stageFlags = GetShaderStageFlags(EShaderStage_All);
		cbBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.pNext = nullptr;
		layoutCreateInfo.flags = 0;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &cbBinding;

		VkResult result = vkCreateDescriptorSetLayout(GetDevice()->GetVkDevice(), &layoutCreateInfo, nullptr, &m_inlineConstantBufferLayout);
		CRY_ASSERT(result == VK_SUCCESS);
	}

	return m_inlineConstantBufferLayout;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permission is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);
	m_pVKScheduler->CeaseAllCommandQueues(false);

	VK_PTR(CCommandList) pCL;
	pQueue.AcquireCommandList(pCL);

	m_pVKScheduler->ResumeAllCommandQueues();
	auto pResult = stl::make_unique<CDeviceCommandList>();
	pResult->m_sharedState.pCommandList = pCL;
	return pResult;
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);
	m_pVKScheduler->CeaseAllCommandQueues(false);

	std::vector<CDeviceCommandListUPtr> pCommandLists;
	VK_PTR(CCommandList) pCLs[256];

	// Allocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		pQueue.AcquireCommandLists(chunkCount, pCLs);

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			pCommandLists.emplace_back(stl::make_unique<CDeviceCommandList>());
			pCommandLists.back()->m_sharedState.pCommandList = pCLs[b];
		}
	}

	m_pVKScheduler->ResumeAllCommandQueues();
	return pCommandLists;
}

// Command-list sinks, will automatically submit command-lists in allocation-order
void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);

	if (pCommandList)
	{
		pQueue.ForfeitCommandList(pCommandList->m_sharedState.pCommandList);
	}
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryVulkan::CCommandListPool& pQueue = m_pVKScheduler->GetCommandListPool(eQueueType);

	const uint32 listCount = pCommandLists.size();
	VK_PTR(CCommandList) pCLs[256];

	// Deallocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		uint32 validCount = 0;

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			if (pCommandLists[b])
			{
				pCLs[validCount++] = pCommandLists[b]->m_sharedState.pCommandList;
			}
		}

		if (validCount)
		{
			pQueue.ForfeitCommandLists(validCount, pCLs);
		}
	}
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{
	m_pVKDevice->FlushAndWaitForGPU();

	if (m_inlineConstantBufferLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(GetDevice()->GetVkDevice(), m_inlineConstantBufferLayout, nullptr);
		m_inlineConstantBufferLayout = VK_NULL_HANDLE;
	}
}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	VkFormat vkFormat = DXGIFormatToVKFormat(Format);
	if (vkFormat != VK_FORMAT_UNDEFINED)
	{
		const SPhysicalDeviceInfo* pInfo = GetVKDevice()->GetPhysicalDeviceInfo();
		const VkFormatProperties& Props = pInfo->formatProperties[vkFormat];
		VkFormatFeatureFlags PropsAny = Props.linearTilingFeatures | Props.optimalTilingFeatures | Props.bufferFeatures;
		VkFormatFeatureFlags PropsTex = Props.linearTilingFeatures | Props.optimalTilingFeatures;

		// *INDENT-OFF*
		return
			(Props.bufferFeatures                                        ? FMTSUPPORT_BUFFER                      : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE1D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE2D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURE3D                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_TEXTURECUBE                 : 0) |
			(Props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT  ? FMTSUPPORT_IA_VERTEX_BUFFER            : 0) |
			(Props.bufferFeatures                                        ? FMTSUPPORT_IA_INDEX_BUFFER             : 0) |
			(false                                                       ? FMTSUPPORT_SO_BUFFER                   : 0) |
			(PropsTex                                                    ? FMTSUPPORT_MIP                         : 0) |
//			(PropsTex                                                    ? FMTSUPPORT_SRGB                        : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_LOAD                 : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_GATHER               : 0) |
			(true                                                        ? FMTSUPPORT_SHADER_GATHER_COMPARISON    : 0) |
			(PropsTex & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT              ? FMTSUPPORT_SHADER_SAMPLE               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT              ? FMTSUPPORT_SHADER_SAMPLE_COMPARISON    : 0) |
//			(nOptions & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ? FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW : 0) |
			(PropsTex & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT   ? FMTSUPPORT_DEPTH_STENCIL               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT           ? FMTSUPPORT_RENDER_TARGET               : 0) |
			(PropsTex & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT     ? FMTSUPPORT_BLENDABLE                   : 0) |
//			(?                                                           ? FMTSUPPORT_DISPLAYABLE                 : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_LOAD            : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_RESOLVE         : 0) |
			(false /* extension?*/                                       ? FMTSUPPORT_MULTISAMPLE_RENDERTARGET    : 0);
		// *INDENT-ON*
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Fence API

HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new UINT64);
	hr = query ? S_OK : E_FAIL;
	if (!FAILED(hr))
	{
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : S_FALSE;
	delete reinterpret_cast<UINT64*>(query);
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		*handle = m_pVKScheduler->InsertFence();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = query ? S_FALSE : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		hr = m_pVKScheduler->TestForFence(*handle);
		if (hr != S_OK)
		{
			// Can only flush from render thread
			CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread());

			// Test + Flush + No-Block is okay
			// Test + No-Flush + Block may not be okay, caution advised as it could deadlock
			if (flush)
			{
				m_pVKScheduler->FlushToFence(*handle);
			}

			if (block)
			{
				hr = m_pVKScheduler->WaitForFence(*handle);
			}
		}
	}
	return hr;
}

////////////////////////////////////////////////////////////////////////////
// SamplerState API

CDeviceSamplerState* CDeviceObjectFactory::CreateSamplerState(const SSamplerState& state)
{
	CDeviceSamplerState* const pResult = new CDeviceSamplerState(GetDevice());
	if (pResult->Init(state) == VK_SUCCESS)
	{
		return pResult;
	}
	pResult->Release();
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CDeviceInputLayout* CDeviceObjectFactory::CreateInputLayout(const SInputLayout& pLayout, const SShaderBlob* m_pConsumingVertexShader)
{
	VK_NOT_IMPLEMENTED;
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
	bool bAnyFailed = false;
	if (!m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER])
	{
		CDynamicOffsetBufferResource* pNullBuffer;
		VkBufferCreateInfo info;

		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.size = 256;
		info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT       | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		             VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
		             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT        | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr;

		if (GetDevice()->CreateCommittedResource(kHeapSources, info, &pNullBuffer) == VK_SUCCESS)
		{
			pNullBuffer->CMemoryResource::AddFlags(NCryVulkan::kResourceFlagNull);
			pNullBuffer->SetStrideAndElementCount(16, 1);
			m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER] = pNullBuffer;

			vkCmdFillBuffer(GetCoreCommandList().GetVKCommandList()->GetVkCommandList(), pNullBuffer->AsBuffer()->GetHandle(), 0, info.size, 0x0);

			auto setLayout = GetDeviceObjectFactory().GetInlineConstantBufferLayout();
			pNullBuffer->CreateDynamicDescriptorSet(setLayout, static_cast<uint32_t>(info.size));
		}
		else
		{
			bAnyFailed = true;
		}

		m_NullBufferViewStructured = new NCryVulkan::CBufferView(pNullBuffer, 0, 16, VK_FORMAT_UNDEFINED);
		m_NullBufferViewTyped = new NCryVulkan::CBufferView(pNullBuffer, 0, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
	}
	else
	{
		m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER]->AddRef();
		m_NullBufferViewStructured->AddRef();
		m_NullBufferViewTyped->AddRef();

	}

	for (int dimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D; dimension <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; ++dimension)
	{
		if (!m_NullResources[dimension])
		{
			const VkImageType dimensionality = 
				dimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D ? VK_IMAGE_TYPE_1D :
				dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D;

			CImageResource* pNullImage;
			VkImageCreateInfo info;

			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.pNext = nullptr;
			info.flags = (dimensionality == VK_IMAGE_TYPE_2D) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
			info.imageType = dimensionality;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.extent.width = 1;
			info.extent.height = 1;
			info.extent.depth = 1;
			info.mipLevels = 1;
			info.arrayLayers = (dimensionality == VK_IMAGE_TYPE_2D) ? 6 : 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.queueFamilyIndexCount = 0;
			info.pQueueFamilyIndices = nullptr;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (GetDevice()->CreateCommittedResource(kHeapSources, info, &pNullImage) == VK_SUCCESS)
			{
				pNullImage->CMemoryResource::AddFlags(NCryVulkan::kResourceFlagNull);
				m_NullResources[dimension] = pNullImage;

				VkClearColorValue clearColor = {{ 0 }};
				VkImageSubresourceRange clearRange;
				clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				clearRange.baseArrayLayer = 0;
				clearRange.baseMipLevel = 0;
				clearRange.layerCount = info.arrayLayers;
				clearRange.levelCount = info.mipLevels;

				pNullImage->TrackAnnouncedAccess(VK_ACCESS_TRANSFER_WRITE_BIT);
				pNullImage->TransitionBarrier(GetCoreCommandList().GetVKCommandList(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

				// TODO: add to API
				GetCoreCommandList().GetVKCommandList()->PendingResourceBarriers();
				vkCmdClearColorImage(GetCoreCommandList().GetVKCommandList()->GetVkCommandList(), pNullImage->GetHandle(), pNullImage->GetLayout(), &clearColor, 1, &clearRange);
			}
			else
			{
				bAnyFailed = true;
			}
		}
		else
		{
			m_NullResources[dimension]->AddRef();
		}
	}
	VK_ASSERT(!bAnyFailed && "Cannot create null resources, this will cause issues for resource-sets using them!");
}

void CDeviceObjectFactory::ReleaseNullResources()
{
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_BUFFER]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE1D]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE2D]);
	SAFE_RELEASE(m_NullResources[D3D11_RESOURCE_DIMENSION_TEXTURE3D]);
}

void CDeviceObjectFactory::UpdateDeferredUploads()
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "CDeviceObjectFactory::UpdateDeferredUploads");
	AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);

	for (auto& upload : m_deferredUploads)
	{
		if (CImageResource* const pImage = upload.pTarget->AsImage())
		{
			GetCoreCommandList().GetCopyInterface()->UploadImage(
				upload.pStagingBuffer,
				pImage,
				upload.targetMapping,
				upload.bExtendedAdressing);
		}
		else if (CBufferResource* const pBuffer = upload.pTarget->AsBuffer())
		{
			SResourceRegionMapping mapping;
			mapping.DestinationOffset = upload.targetMapping.ResourceOffset;
			mapping.Extent = upload.targetMapping.Extent;
			mapping.Flags = upload.targetMapping.Flags;
			mapping.SourceOffset.Left = 0;
			mapping.SourceOffset.Front = 0;
			mapping.SourceOffset.Top = 0;
			mapping.SourceOffset.Subresource = 0;

			GetCoreCommandList().GetCopyInterface()->CopyBuffer(
				upload.pStagingBuffer,
				pBuffer,
				mapping);
		}
	}

	m_deferredUploads.clear();
}

//=============================================================================

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	VkResult result = VK_ERROR_FEATURE_NOT_PRESENT;
	CBufferResource* pStaging = nullptr;

	if (CBufferResource* pBuffer = pForTex->AsBuffer())
	{
		const VkDeviceSize minSize = pBuffer->GetStride() * pBuffer->GetElementCount();
		result = GetDevice()->CreateOrReuseStagingResource(pBuffer, minSize, &pStaging, bUpload);
	}
	else if (CImageResource* pImage = pForTex->AsImage())
	{
		SResourceMemoryMapping mapping;
		SelectStagingLayout(pImage, 0, mapping);
		result = GetDevice()->CreateOrReuseStagingResource(pImage, mapping.MemoryLayout.volumeStride, &pStaging, bUpload);
	}

	if (result == VK_SUCCESS)
	{
		if (pMappedAddress = pStaging->Map())
		{
			return pStaging;
		}

		pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
	}

	VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
	return nullptr;
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStaging)
{
	pStaging->Unmap();
	pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
}
#endif

void CDeviceObjectFactory::ReleaseResource(D3DResource* pResource)
{
	pResource->Release(); // trigger ReleaseLater without kResourceFlagReusable
}

void CDeviceObjectFactory::RecycleResource(D3DResource* pResource)
{
	pResource->Release(); // trigger ReleaseLater with kResourceFlagReusable
}

//=============================================================================
// TODO: Function should be in DeviceObjects_Vulkan.inl
template<typename T>
static inline EHeapType SuggestVKHeapType(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE)) != (CDeviceObjectFactory::USAGE_CPU_READ | CDeviceObjectFactory::USAGE_CPU_WRITE), "CPU Read and Write can't be requested together!");

	// *INDENT-OFF*
	return EHeapType(
		((desc                & (CDeviceObjectFactory::USAGE_CPU_READ       ) ? kHeapDownload :
		((desc                & (CDeviceObjectFactory::USAGE_CPU_WRITE      ) ? kHeapUpload :
		((desc                & (CDeviceObjectFactory::BIND_DEPTH_STENCIL   |
		                         CDeviceObjectFactory::BIND_RENDER_TARGET   |
		                         CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? kHeapTargets :
		                                                                        kHeapSources)))))));
	// *INDENT-ON*
}

// Set info.usage and info.tiling based on usage flag, returns an appropriate heap.
static EHeapType ConfigureUsage(VkImageCreateInfo& info, uint32 nUsage)
{
	info.usage = ConvertToVKImageUsageBits(nUsage);

	info.tiling = ConvertToVKImageTiling(nUsage);
	if (info.tiling == VK_IMAGE_TILING_LINEAR)
	{
		// CPU access of some kind requested, this will fail on anything except copy-only resources!
		VK_ASSERT((info.usage & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) == 0 && "CPU access not allowed on images, when the image is not copy-only");
	}

	return SuggestVKHeapType(nUsage);
}

// Set info.usage, returns an appropriate heap.
static EHeapType ConfigureUsage(VkBufferCreateInfo& info, uint32 nUsage)
{
	info.usage = ConvertToVKBufferUsageBits(nUsage);

	return SuggestVKHeapType(nUsage);
}

void CDeviceObjectFactory::UploadInitialImageData(SSubresourcePayload pSrcMips[], CImageResource* pDst, const VkImageCreateInfo& info)
{
	CRY_ASSERT(!(info.arrayLayers > 1 && info.extent.depth > 1)); // arrays of volumes not supported

	const bool bImmediateUpload = gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();

	uint32_t width  = info.extent.width;
	uint32_t height = info.extent.height;
	uint32_t depth  = info.extent.depth;

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes;
	const bool bIsBlocks = GetBlockSize(info.format, blockWidth, blockHeight, blockBytes);
	if (!bIsBlocks)
	{
		blockBytes = pSrcMips[0].m_sSysMemAlignment.rowStride / width;
	}

	SResourceMemoryMapping imageMapping;
	SResourceMemoryMapping bufferMapping;

	imageMapping.MemoryLayout.typeStride = blockBytes;
	imageMapping.ResourceOffset.Left = 0;
	imageMapping.ResourceOffset.Top = 0;
	imageMapping.ResourceOffset.Front = 0;
	imageMapping.Extent.Subresources = (info.arrayLayers - 1) * info.mipLevels + 1; // Select all the slices in this mip (non-standard addressing!)

	bufferMapping.ResourceOffset.Subresource = 0;
	bufferMapping.Extent.Subresources = 1;

	for (uint32_t mip = 0; mip < info.mipLevels; ++mip)
	{
		const SSubresourcePayload* const pData = pSrcMips + mip;

		// Image mapping for all slices.
		imageMapping.MemoryLayout.rowStride    = pData->m_sSysMemAlignment.rowStride;
		imageMapping.MemoryLayout.planeStride  = pData->m_sSysMemAlignment.planeStride;
		imageMapping.MemoryLayout.volumeStride = pData->m_sSysMemAlignment.planeStride * info.arrayLayers * depth;
		imageMapping.ResourceOffset.Subresource = mip; // Select given mip on first array slice
		imageMapping.Extent.Width  = width;
		imageMapping.Extent.Height = height;
		imageMapping.Extent.Depth  = depth;

		// Buffer mapping for one slice.
		bufferMapping.ResourceOffset.Left = 0;
		bufferMapping.MemoryLayout = imageMapping.MemoryLayout;
		bufferMapping.Extent.Width = bufferMapping.MemoryLayout.volumeStride = pData->m_sSysMemAlignment.planeStride * depth;

		// Create buffer for all slices.
		CBufferResource* pStagingRaw = nullptr;
		if (GetDevice()->CreateOrReuseStagingResource(pDst, imageMapping.MemoryLayout.volumeStride, &pStagingRaw, true) != VK_SUCCESS) 
		{
			VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
			return;
		}
		_smart_ptr<CBufferResource> pStaging;
		pStaging.Assign_NoAddRef(pStagingRaw);

		// Fill first slice of the array.
		bool bFilled = CDeviceCopyCommandInterface::FillBuffer(pData->m_pSysMem, pStaging, bufferMapping);

		// Fill additional slices of the array (if needed).
		// The slices must have identical layout as the first slice.
		const SSubresourcePayload* pSlice = pData;
		for (uint32_t slice = 1; slice < info.arrayLayers; ++slice)
		{
			pSlice += info.mipLevels;
			bufferMapping.ResourceOffset.Left += pData->m_sSysMemAlignment.planeStride;
			
			VK_ASSERT(pSlice->m_sSysMemAlignment.rowStride == pData->m_sSysMemAlignment.rowStride && pSlice->m_sSysMemAlignment.planeStride == pData->m_sSysMemAlignment.planeStride && "Slice data not compatible");
			bFilled &= CDeviceCopyCommandInterface::FillBuffer(pSlice->m_pSysMem, pStaging, bufferMapping);
		}

		if (bFilled)
		{
			const bool bExtendedAddressing = true;

			if (bImmediateUpload)
			{
				GetCoreCommandList().GetCopyInterface()->UploadImage(pStaging, pDst, imageMapping, bExtendedAddressing);
			}
			else
			{
				SDeferredUploadData uploadData;
				uploadData.pStagingBuffer = std::move(pStaging);
				uploadData.pTarget = pDst;
				uploadData.targetMapping = imageMapping;
				uploadData.bExtendedAdressing = bExtendedAddressing;

				AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);
				m_deferredUploads.push_back(uploadData);
			}
		}

		width  = std::max(width  >> 1, blockWidth);
		height = std::max(height >> 1, blockHeight);
		depth  = std::max(depth  >> 1, 1U);
	}
}

void CDeviceObjectFactory::SelectStagingLayout(const NCryVulkan::CImageResource* pImage, uint32 subResource, SResourceMemoryMapping& result)
{
	const uint32 mip = subResource % pImage->GetMipCount();
	const uint32 slice = subResource / pImage->GetMipCount();
	VK_ASSERT(mip < pImage->GetMipCount() && slice < pImage->GetSliceCount() && "Bad subresource selected");

	const uint32 width = std::max(pImage->GetWidth() >> mip, 1U);
	const uint32 height = std::max(pImage->GetHeight() >> mip, 1U);
	const uint32 depth = std::max(pImage->GetDepth() >> mip, 1U);

	uint32 blockWidth = 1, blockHeight = 1, blockBytes;
	const bool bIsBlocks = GetBlockSize(pImage->GetFormat(), blockWidth, blockHeight, blockBytes);
	if (!bIsBlocks)
	{
		blockBytes = s_FormatWithSize[s_VkFormatToDXGI[pImage->GetFormat()]].Size; // TODO: Use a better API for this?
		VK_ASSERT(blockBytes >= 1 && blockBytes <= 16 && "Unknown texel size");
	}

	const uint32 widthInBlocks = (width + blockWidth - 1U) / blockWidth;
	const uint32 heightInBlocks = (height + blockHeight - 1U) / blockHeight;

	result.MemoryLayout = SResourceMemoryAlignment::Linear(blockBytes, widthInBlocks, heightInBlocks, depth);
	result.ResourceOffset.Left = 0;
	result.ResourceOffset.Top = 0;
	result.ResourceOffset.Front = 0;
	result.ResourceOffset.Subresource = subResource;
	result.Extent.Width = width;
	result.Extent.Height = height;
	result.Extent.Depth = depth;
	result.Extent.Subresources = 1;
	result.Flags = 0;
}

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI, int32 nESRAMOffset)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = ConvertFormat(Format);
	info.extent.width = nWidth;
	info.extent.height = nHeight;
	info.extent.depth = 1;
	info.mipLevels = nMips;
	info.arrayLayers = nArraySize;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		info.samples = (VkSampleCountFlagBits)pTI->m_nDstMSAASamples;
	}

	if (info.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		// R16_TYPELESS/_UNORM and R32_TYPELESS/_FLOAT in Vk are NOT aliases of D16_UNORM/D32_FLOAT
		// In this case, ConvertFormat doesn't do the right thing since it doesn't check usage flags.
		switch (Format)
		{
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_TYPELESS:
			info.format = VK_FORMAT_D16_UNORM;
			break;
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_TYPELESS:
			info.format = VK_FORMAT_D32_SFLOAT;
			break;
		}

		// Anything depth/stencil can NEVER be interpreted as some other format (Vk limitation).
		// Therefore, remove the VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT flag.
		info.flags &= ~VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	}

	VK_ASSERT(nWidth * nHeight * nArraySize * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT | VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = ConvertFormat(Format);
	info.extent.width = nSize;
	info.extent.height = nSize;
	info.extent.depth = 1;
	info.mipLevels = nMips;
	info.arrayLayers = nArraySize; assert(!(nArraySize % 6));
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		info.samples = (VkSampleCountFlagBits)pTI->m_nDstMSAASamples;
	}

	VK_ASSERT(nSize * nSize * nArraySize * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	VkImageCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	info.imageType = VK_IMAGE_TYPE_3D;
	info.format = ConvertFormat(Format);
	info.extent.width = nWidth;
	info.extent.height = nHeight;
	info.extent.depth = nDepth;
	info.mipLevels = nMips;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	const EHeapType heapHint = ConfigureUsage(info, nUsage);

	if (pTI)
	{
		VK_ASSERT(pTI->m_nDstMSAASamples == 1 && "MSAA samples not supported on 3D textures");
	}

	VK_ASSERT(nWidth * nHeight * nDepth * nMips != 0);
	CImageResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;
	if (nUsage & USAGE_HIFREQ_HEAP)
		hResult = GetDevice()->CreateOrReuseCommittedResource(heapHint, info, &pResult);
	else
		hResult = GetDevice()->CreateCommittedResource(heapHint, info, &pResult);

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	CDeviceTexture* const pWrapper = (*ppDevTexture = new CDeviceTexture());
	if (pTI && pTI->m_pSysMemSubresourceData)
	{
		VK_ASSERT(pTI->m_eSysMemTileMode == eTM_None && "Tiled upload not supported");
		UploadInitialImageData(pTI->m_pSysMemSubresourceData, pResult, info);
	}

	pWrapper->m_pNativeResource = pResult;
	pWrapper->m_nBaseAllocatedSize = (size_t)pResult->GetSize();

	return S_OK;
}

HRESULT CDeviceObjectFactory::CreateBuffer(buffer_size_t nSize, buffer_size_t elemSize, uint32 nUsage, uint32 nBindFlags, D3DBuffer** ppBuff, const void* pData)
{
	VkBufferCreateInfo info;

	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0;
	info.size = nSize * elemSize;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.queueFamilyIndexCount = 0;
	info.pQueueFamilyIndices = nullptr;
	const EHeapType heapHint = ConfigureUsage(info, nUsage | nBindFlags);

	VK_ASSERT(nSize * elemSize != 0);
	CBufferResource* pResult = nullptr;
	VkResult hResult = VK_SUCCESS;

	if (nBindFlags & BIND_CONSTANT_BUFFER)
	{
		CDynamicOffsetBufferResource* pDynamicBuffer = nullptr;
		if (nUsage & USAGE_HIFREQ_HEAP)
			hResult = GetDevice()->CreateOrReuseCommittedResource<CDynamicOffsetBufferResource>(heapHint, info, &pDynamicBuffer);
		else
			GetDevice()->CreateCommittedResource<CDynamicOffsetBufferResource>(heapHint, info, &pDynamicBuffer);
		pResult = pDynamicBuffer;
	}
	else
	{
		if (nUsage & USAGE_HIFREQ_HEAP)
			hResult = GetDevice()->CreateOrReuseCommittedResource<CBufferResource>(heapHint, info, &pResult);
		else
			hResult = GetDevice()->CreateCommittedResource<CBufferResource>(heapHint, info, &pResult);
	}

	if (hResult != VK_SUCCESS)
	{
		return E_FAIL;
	}

	pResult->SetStrideAndElementCount(elemSize, nSize);

	*ppBuff = pResult;
	if (pData)
	{
		SResourceMemoryMapping mapping;
		ZeroStruct(mapping);

		mapping.ResourceOffset.Left = 0 * elemSize;
		mapping.ResourceOffset.Subresource = 0;
		mapping.Extent.Width = nSize * elemSize;
		mapping.Extent.Subresources = 1;
		mapping.MemoryLayout = SResourceMemoryAlignment::Linear(elemSize, nSize);

		const bool bImmediateUpload = gcpRendD3D->m_pRT->IsRenderThread() && !gcpRendD3D->m_pRT->IsRenderLoadingThread();

		CDeviceCommandListRef commandList = GetCoreCommandList();
		if (CBufferResource* const pStaging = commandList.GetCopyInterface()->UploadBuffer(pData, pResult, mapping, bImmediateUpload))
		{
			// Unable to perform upload immediately (not called from render-thread and the desired buffer is not CPU writable).
			// We have to perform the staging at the earliest convenience of the render-thread instead.
			SDeferredUploadData uploadData;
			uploadData.pStagingBuffer.Assign_NoAddRef(pStaging);
			uploadData.pTarget = pResult;
			uploadData.targetMapping = mapping;
			uploadData.bExtendedAdressing = false;

			AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_deferredUploadCS);
			m_deferredUploads.push_back(uploadData);
		}
	}

	return S_OK;
}

//=============================================================================

#if !(CRY_PLATFORM_DURANGO && BUFFER_ENABLE_DIRECT_ACCESS)
HRESULT CDeviceObjectFactory::InvalidateCpuCache(void* buffer_ptr, size_t size, size_t offset)
{
	return S_OK;
}

HRESULT CDeviceObjectFactory::InvalidateGpuCache(D3DBuffer* buffer, void* buffer_ptr, buffer_size_t size, buffer_size_t offset)
{
	return S_OK;
}
#endif

void CDeviceObjectFactory::InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id)
{
}

//=============================================================================

uint8* CDeviceObjectFactory::Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE || mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	return reinterpret_cast<uint8*>(buffer->Map());
}

void CDeviceObjectFactory::Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE || mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	buffer->Unmap();
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks)
{
	VK_ASSERT(mode == D3D11_MAP_WRITE_NO_OVERWRITE); // only no-overwrite supported currently

	if (!bDirectAccess)
	{
		const uint8* pInData = reinterpret_cast<const uint8*>(pInDataCPU);
		uint8* pOutData = CDeviceObjectFactory::Map(buffer, subresource, offset, 0, mode) + offset;

		StreamBufferData(pOutData, pInData, size);

		CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
	}
	else
	{
		// Transfer sub-set of GPU resource to CPU, also allows graphics debugger and multi-gpu broadcaster to do the right thing
		CDeviceObjectFactory::MarkReadRange(buffer, offset, 0, D3D11_MAP_WRITE);

		StreamBufferData(pOutDataGPU, pInDataCPU, size);

		CDeviceObjectFactory::MarkWriteRange(buffer, offset, size, D3D11_MAP_WRITE);
	}
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	VK_ASSERT(buffer->GetHeapType() == kHeapDownload);
	VK_ASSERT(mode == D3D11_MAP_READ); // NOTE: access to resources in use by the gpu MUST be synchronized externally
	VK_ASSERT(pInDataGPU == nullptr);  // Currently not handled

	uint8* pInData = CDeviceObjectFactory::Map(buffer, subresource, offset, size, mode) + offset;
	memcpy(pOutDataCPU, pInData, size);
	CDeviceObjectFactory::Unmap(buffer, subresource, offset, size, mode);
}

// Explicit instantiation
template
void CDeviceObjectFactory::UploadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::UploadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);

template
void CDeviceObjectFactory::DownloadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::DownloadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);

// Shader API
static NCryVulkan::CShader* CreateShader(NCryVulkan::CDevice* pDevice, const void* pShaderBytecode, size_t bytecodeLength)
{
	auto pShader = new NCryVulkan::CShader(pDevice);
	if (pShader->Init(pShaderBytecode, bytecodeLength) == VK_SUCCESS)
	{
		return pShader;
	}
	pShader->Release();
	return nullptr;
}

ID3D11VertexShader* CDeviceObjectFactory::CreateVertexShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11PixelShader* CDeviceObjectFactory::CreatePixelShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11GeometryShader* CDeviceObjectFactory::CreateGeometryShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11HullShader* CDeviceObjectFactory::CreateHullShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11DomainShader* CDeviceObjectFactory::CreateDomainShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

ID3D11ComputeShader* CDeviceObjectFactory::CreateComputeShader(const void* pData, size_t bytes)
{
	return CreateShader(GetVKDevice(), pData, bytes);
}

// Occlusion Query API
D3DOcclusionQuery* CDeviceObjectFactory::CreateOcclusionQuery()
{
	std::unique_ptr<D3DOcclusionQuery> pResult(new NCryVulkan::COcclusionQuery(GetVKDevice()->GetOcclusionQueries()));
	return pResult->IsValid() ? pResult.release() : nullptr;
}

bool CDeviceObjectFactory::GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed)
{
	return pQuery->GetResults(samplesPassed);
}
