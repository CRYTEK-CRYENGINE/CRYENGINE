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


extern CD3D9Renderer gcpRendD3D;

using namespace NCryVulkan;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_queryPool(VK_NULL_HANDLE)
	, m_fence(0)
	, m_measurable(false)
{}

CDeviceTimestampGroup::~CDeviceTimestampGroup()
{
	if (m_queryPool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(GetDevice()->GetVkDevice(), m_queryPool, nullptr);
	}

	if (m_fence != 0)
	{
		GetDeviceObjectFactory().ReleaseFence(m_fence);
	}
}

void CDeviceTimestampGroup::Init()
{
	GetDeviceObjectFactory().CreateFence(m_fence);

	VkQueryPoolCreateInfo poolInfo;
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	poolInfo.queryCount = kMaxTimestamps;
	poolInfo.pipelineStatistics = 0;

	VkResult result = vkCreateQueryPool(GetDevice()->GetVkDevice(), &poolInfo, nullptr, &m_queryPool);
	CRY_ASSERT(result == VK_SUCCESS);
}

void CDeviceTimestampGroup::BeginMeasurement()
{
	CDeviceCommandListRef coreCommandList = GetDeviceObjectFactory().GetCoreCommandList();
	vkCmdResetQueryPool(coreCommandList.GetVKCommandList()->GetVkCommandList(), m_queryPool, 0, kMaxTimestamps);

	m_numTimestamps = 0;
	m_measurable = false;
}

void CDeviceTimestampGroup::EndMeasurement()
{
	GetDeviceObjectFactory().IssueFence(m_fence);
	m_measurable = true;
}

uint32 CDeviceTimestampGroup::IssueTimestamp(CDeviceCommandList* pCommandList)
{
	CRY_ASSERT(m_numTimestamps < kMaxTimestamps);

	CDeviceCommandListRef deviceCommandList = pCommandList ? *pCommandList : GetDeviceObjectFactory().GetCoreCommandList();
	vkCmdWriteTimestamp(deviceCommandList.GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPool, m_numTimestamps);

	return m_numTimestamps++;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (!m_measurable)
		return false;
	if (m_numTimestamps == 0)
		return true;

	if (GetDeviceObjectFactory().SyncFence(m_fence, false, true) != S_OK)
		return false;
	if (vkGetQueryPoolResults(GetDevice()->GetVkDevice(), m_queryPool, 0, m_numTimestamps, sizeof(m_timestampData), m_timestampData.data(), sizeof(uint64), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS)
		return false;
	
	return true;
}

float CDeviceTimestampGroup::GetTimeMS(uint32 timestamp0, uint32 timestamp1)
{
	const float ticksToNanoseconds = GetDevice()->GetPhysicalDeviceInfo()->deviceProperties.limits.timestampPeriod;

	uint64 ticks = std::max(m_timestampData[timestamp0], m_timestampData[timestamp1]) - std::min(m_timestampData[timestamp0], m_timestampData[timestamp1]);
	return ticks * ticksToNanoseconds / 1000000.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceCommandListImpl::SetProfilerMarker(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		VkDebugMarkerMarkerInfoEXT markerInfo;
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.pNext = nullptr;
		markerInfo.pMarkerName = label;
		ZeroArray(markerInfo.color);

		Extensions::EXT_debug_marker::CmdDebugMarkerInsert(GetVKCommandList()->GetVkCommandList(), &markerInfo);
	}
}

void CDeviceCommandListImpl::BeginProfilerEvent(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		VkDebugMarkerMarkerInfoEXT markerInfo;
		markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		markerInfo.pNext = nullptr;
		markerInfo.pMarkerName = label;
		ZeroArray(markerInfo.color);

		Extensions::EXT_debug_marker::CmdDebugMarkerBegin(GetVKCommandList()->GetVkCommandList(), &markerInfo);
	}
}

void CDeviceCommandListImpl::EndProfilerEvent(const char* label)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		Extensions::EXT_debug_marker::CmdDebugMarkerEnd(GetVKCommandList()->GetVkCommandList());
	}
}

void CDeviceCommandListImpl::RequestTransition(CImageResource* pResource, VkImageLayout desiredLayout, VkAccessFlags desiredAccess) const
{
	pResource->TrackAnnouncedAccess(desiredAccess);
	pResource->TransitionBarrier(GetVKCommandList(), desiredLayout);
}

void CDeviceCommandListImpl::RequestTransition(NCryVulkan::CBufferResource* pResource, VkAccessFlags desiredAccess) const
{
	pResource->TrackAnnouncedAccess(desiredAccess);
	pResource->RangeBarrier(GetVKCommandList());
}

CDeviceCommandListImpl::~CDeviceCommandListImpl()
{
}

void CDeviceCommandListImpl::CeaseCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == pCommandList || m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = nullptr;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		VK_NOT_IMPLEMENTED;
	}
#endif

	reinterpret_cast<CDeviceCommandList*>(this)->Reset();
}

void CDeviceCommandListImpl::ResumeCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = pCommandList;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		VK_NOT_IMPLEMENTED;
	}
#endif

	if (CDeviceGraphicsCommandInterfaceImpl* pGraphicsInterface = GetGraphicsInterfaceImpl())
	{
		pGraphicsInterface->BindNullVertexBuffers();
	}
}

void CDeviceCommandListImpl::ResetImpl()
{
	m_graphicsState.custom.pendingBindings.Reset();
	m_computeState.custom.pendingBindings.Reset();
}

void CDeviceCommandListImpl::LockToThreadImpl()
{
	NCryVulkan::CCommandList* pCommandListVK = GetVKCommandList();

	pCommandListVK->Begin();

	if (CDeviceGraphicsCommandInterfaceImpl* pGraphicsInterface = GetGraphicsInterfaceImpl())
	{
		pGraphicsInterface->BindNullVertexBuffers();
	}
}

void CDeviceCommandListImpl::CloseImpl()
{
	FUNCTION_PROFILER_RENDERER();
	GetVKCommandList()->End();
}

void CDeviceCommandListImpl::ApplyPendingBindings(VkCommandBuffer vkCommandList, VkPipelineLayout vkPipelineLayout, VkPipelineBindPoint vkPipelineBindPoint, const SPendingBindings& RESTRICT_REFERENCE bindings)
{
	uint32 toBind = bindings.validMask;

	while (toBind != 0)
	{
		uint32 start = countTrailingZeros32(toBind);
		uint32 curBit = (1 << start);

		const bool bStaticOffset = !(bindings.dynamicOffsetMask & curBit);

		while (true)
		{
			curBit <<= 1;

			if ((toBind & curBit) == 0)
				break;

			const bool curStaticOffset = !(bindings.dynamicOffsetMask & curBit);
			if (curStaticOffset != bStaticOffset)
				break;
		}

		uint32 end = countTrailingZeros32(curBit);
		toBind &= ~uint32(curBit - 1);

		vkCmdBindDescriptorSets(vkCommandList, vkPipelineBindPoint, vkPipelineLayout,
			start, end - start, &bindings.descriptorSets[start],
			bStaticOffset ? 0 : end - start, bStaticOffset ? nullptr : &bindings.dynamicOffsets[start]);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews, bool bCompute) const
{
	for (uint32 i = 0; i < viewCount; ++i)
	{
		RequestTransition(pViews[i]->GetDevBuffer()->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareRenderPassForUseImpl(CDeviceRenderPass& renderPass) const
{
	if (renderPass.m_pDepthStencilTarget)
	{
		// TODO: Read-only DSV?
		RequestTransition(renderPass.m_pDepthStencilTarget, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	}

	for (uint32 i = 0; i < renderPass.m_RenderTargetCount; ++i)
	{
		RequestTransition(renderPass.m_renderTargets[i], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CDeviceResourceSet_Vulkan* const pResourceSet = static_cast<CDeviceResourceSet_Vulkan*>(pResources);
	auto lambda = [this](CMemoryResource* pResource, bool bWritable)
	{
		if (CDynamicOffsetBufferResource* pBuffer = pResource->AsDynamicOffsetBuffer())
		{
			RequestTransition(pBuffer, VK_ACCESS_UNIFORM_READ_BIT);
		}
		else
		{
			const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if (CBufferResource* pBuffer = pResource->AsBuffer())
			{
				RequestTransition(pBuffer, access);
			}
			else if (CImageResource* pImage = pResource->AsImage())
			{
				RequestTransition(pImage, layout, access);
			}
		}
	};
	pResourceSet->EnumerateInUseResources(lambda);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourceForUseImpl(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const
{
	const uint64_t viewKey = pTexture->GetDevTexture()->LookupResourceView(TextureView).first.m_Desc.Key;
	const bool bWritable = SResourceView::IsUnorderedAccessView(viewKey);
	const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
	const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	RequestTransition(pTexture->GetDevTexture()->Get2DTexture(), layout, access);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, SHADERSTAGE_FROM_SHADERCLASS(shaderClass));
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages) const
{
	buffer_size_t offset, bytes;
	CBufferResource* const pActualBuffer = pBuffer->GetD3D(&offset, &bytes);

	CRY_ASSERT(pActualBuffer->AsDynamicOffsetBuffer() != nullptr); // Needs to be a buffer with dynamic offsets
	RequestTransition(pActualBuffer, VK_ACCESS_UNIFORM_READ_BIT);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareVertexBuffersForUseImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const
{
	buffer_size_t offset;
	for (uint32 i = 0; i < numStreams; ++i)
	{
		const SStreamInfo stream = vertexStreams[i];
		CBufferResource* const pActualBuffer = gcpRendD3D.m_DevBufMan.GetD3D(stream.hStream, &offset);
		RequestTransition(pActualBuffer, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareIndexBufferForUseImpl(const CDeviceInputStream* indexStream) const
{
	buffer_size_t offset;
	const SStreamInfo stream = *indexStream;
	CBufferResource* const pActualBuffer = gcpRendD3D.m_DevBufMan.GetD3D(stream.hStream, &offset);
	RequestTransition(pActualBuffer, VK_ACCESS_INDEX_READ_BIT);
}

void CDeviceGraphicsCommandInterfaceImpl::BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea)
{
	// clamp render limits to valid area. high level code sometimes provides wrong size.
	VkOffset2D renderAreaTopLeft = { renderArea.left, renderArea.top };
	VkExtent2D renderAreaSize =
	{
		min(renderPass.m_frameBufferExtent.width,  uint32(renderArea.right - renderArea.left)),
		min(renderPass.m_frameBufferExtent.height, uint32(renderArea.bottom - renderArea.top))
	};

	VkRenderPassBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.renderPass = renderPass.m_renderPassHandle;
	beginInfo.framebuffer = renderPass.m_frameBufferHandle;
	beginInfo.renderArea.offset = renderAreaTopLeft;
	beginInfo.renderArea.extent = renderAreaSize;
	beginInfo.clearValueCount = 0;
	beginInfo.pClearValues = nullptr;

	vkCmdBeginRenderPass(GetVKCommandList()->GetVkCommandList(), &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CDeviceGraphicsCommandInterfaceImpl::EndRenderPassImpl(const CDeviceRenderPass& renderPass)
{
	vkCmdEndRenderPass(GetVKCommandList()->GetVkCommandList());
}

void CDeviceGraphicsCommandInterfaceImpl::SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports)
{
	vkCmdSetViewport(GetVKCommandList()->GetVkCommandList(), 0, vpCount, reinterpret_cast<const VkViewport*>(pViewports));
}

void CDeviceGraphicsCommandInterfaceImpl::SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects)
{
	vkCmdSetScissor(GetVKCommandList()->GetVkCommandList(), 0, rcCount, reinterpret_cast<const VkRect2D*>(pRects));
}

void CDeviceGraphicsCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceGraphicsPSO* pDevicePSO)
{
	const CDeviceGraphicsPSO_Vulkan* pVkDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_Vulkan*>(pDevicePSO);

	vkCmdBindPipeline(GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_BIND_POINT_GRAPHICS, pVkDevicePSO->GetVkPipeline());
	m_computeState.pPipelineState = nullptr;
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	m_graphicsState.custom.pendingBindings.Reset();
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	const CDeviceResourceSet_Vulkan* pVkResources = reinterpret_cast<const CDeviceResourceSet_Vulkan*>(pResources);
	CRY_ASSERT(pVkResources->GetVKDescriptorSet() != VK_NULL_HANDLE);

	m_graphicsState.custom.pendingBindings.AppendDescriptorSet(bindSlot, pVkResources->GetVKDescriptorSet(), nullptr);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, EShaderStage_None);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);

	buffer_size_t offset, size;
	NCryVulkan::CBufferResource* pVkBuffer = pBuffer->GetD3D(&offset, &size);
	VK_ASSERT(pVkBuffer && pVkBuffer->GetHandle() && pVkBuffer->AsDynamicOffsetBuffer() != nullptr);

	VkDescriptorSet dynamicDescriptorSet = pVkBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet();
	CRY_ASSERT(dynamicDescriptorSet != VK_NULL_HANDLE);

	m_graphicsState.custom.pendingBindings.AppendDescriptorSet(bindSlot, dynamicDescriptorSet, &offset);
}

void CDeviceGraphicsCommandInterfaceImpl::SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams)
{
	VkBuffer buffers[8];
	VkDeviceSize offsets[8];

	for (int i = 0; i <= lastStreamSlot + 1; ++i)
		buffers[i] = VK_NULL_HANDLE;

	for (uint32 i = 0; i < numStreams; i++)
	{
		const CDeviceInputStream& vertexStream = vertexStreams[i];
		const int curSlot = vertexStream.nSlot;

		CRY_ASSERT(vertexStream.hStream != ~0u);
		{
			buffer_size_t offset;
			auto* pBuffer = gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset);

			buffers[curSlot] = pBuffer->GetHandle();
			offsets[curSlot] = offset;
		}
	}

	// NOTE: assuming slot 0 is always used
	VK_ASSERT(buffers[0] != VK_NULL_HANDLE && "Invalid vertex buffer stream #0");
	for (int rangeStart = 0, rangeEnd = 1; rangeStart <= lastStreamSlot; rangeStart = rangeEnd = rangeEnd + 1)
	{
		while (buffers[rangeEnd] != VK_NULL_HANDLE)
			++rangeEnd;

		if (rangeEnd > rangeStart)
		{
			vkCmdBindVertexBuffers(GetVKCommandList()->GetVkCommandList(), rangeStart, rangeEnd - rangeStart, buffers + rangeStart, offsets + rangeStart);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetIndexBufferImpl(const CDeviceInputStream* indexStream)
{
	CRY_ASSERT(indexStream->hStream != ~0u);
	{
		buffer_size_t offset;
		auto* pBuffer = gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset);
		VK_ASSERT(pBuffer && pBuffer->GetHandle() && "Invalid index buffer stream");

#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
		vkCmdBindIndexBuffer(GetVKCommandList()->GetVkCommandList(), pBuffer->GetHandle(), offset, VK_INDEX_TYPE_UINT16);
#else
		vkCmdBindIndexBuffer(GetVKCommandList()->GetVkCommandList(), pBuffer->GetHandle(), offset, indexStream->nStride == DXGI_FORMAT_R16_UINT ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
#endif
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetStencilRefImpl(uint8 stencilRefValue)
{
	vkCmdSetStencilReference(GetVKCommandList()->GetVkCommandList(), VK_STENCIL_FRONT_AND_BACK, stencilRefValue);
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp)
{
	vkCmdSetDepthBias(GetVKCommandList()->GetVkCommandList(), constBias, biasClamp, slopeBias);
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBoundsImpl(float fMin, float fMax)
{
	vkCmdSetDepthBounds(GetVKCommandList()->GetVkCommandList(), fMin, fMax);
}

void CDeviceGraphicsCommandInterfaceImpl::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsState.custom.pendingBindings);
	m_graphicsState.custom.pendingBindings.Reset();

	vkCmdDraw(GetVKCommandList()->GetVkCommandList(), VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	GetVKCommandList()->m_nCommands += CLCOUNT_DRAW;
}

void CDeviceGraphicsCommandInterfaceImpl::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_graphicsState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsState.custom.pendingBindings);
	m_graphicsState.custom.pendingBindings.Reset();

	vkCmdDrawIndexed(GetVKCommandList()->GetVkCommandList(), IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartIndexLocation);
	GetVKCommandList()->m_nCommands += CLCOUNT_DRAW;
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRects)
{
	CRY_ASSERT(NumRects == 0); // currently not supported on Vulkan

	CImageResource* pImage = pView->GetResource();
	VK_ASSERT(pImage && pImage->GetFlag(NCryVulkan::kImageFlagColorAttachment) && "The image was not created as a color-target");
	VkImageSubresourceRange subresource = pView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

	VkClearColorValue clearValue;
	clearValue.float32[0] = Color[0];
	clearValue.float32[1] = Color[1];
	clearValue.float32[2] = Color[2];
	clearValue.float32[3] = Color[3];

	RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
	GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects)
{
	CRY_ASSERT(numRects == 0); // currently not supported on Vulkan

	VkImageAspectFlags aspectMask = 0;
	aspectMask |= (clearFlags & CLEAR_ZBUFFER) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	aspectMask |= (clearFlags & CLEAR_STENCIL) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;

	CImageResource* pImage = pView->GetResource();
	VK_ASSERT(pImage && pImage->GetFlag(NCryVulkan::kImageFlagDepthAttachment) && "The image was not created as a depth-target");
	VkImageSubresourceRange subresource = pView->GetSubresourceRange(aspectMask);

	VkClearDepthStencilValue clearValue;
	clearValue.depth = depth;
	clearValue.stencil = stencil;

	RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdClearDepthStencilImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
	GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
}

void CDeviceGraphicsCommandInterfaceImpl::BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	pQuery->Begin(GetVKCommandList()->GetVkCommandList());
	GetVKCommandList()->m_nCommands += CLCOUNT_QUERY;
}

void CDeviceGraphicsCommandInterfaceImpl::EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	pQuery->End(GetVKCommandList()->GetVkCommandList());
	GetVKCommandList()->m_nCommands += CLCOUNT_QUERY;
}

void CDeviceGraphicsCommandInterfaceImpl::BindNullVertexBuffers()
{
	// bind null buffers to all potentially used slots so we don't get validation issues when binding streams with gaps
	VkBuffer nullBuffer = GetDeviceObjectFactory().GetNullResource(D3D11_RESOURCE_DIMENSION_BUFFER)->AsBuffer()->GetHandle();

	VkBuffer     buffers[VSF_NUM];
	VkDeviceSize offsets[VSF_NUM];

	for (int i = 0; i < CRY_ARRAY_COUNT(buffers); ++i)
	{
		buffers[i] = nullBuffer;
		offsets[i] = 0;
	}

	vkCmdBindVertexBuffers(GetVKCommandList()->GetVkCommandList(), 0, CRY_ARRAY_COUNT(buffers), buffers, offsets);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeviceComputeCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews) const
{
	for (uint32 i = 0; i < viewCount; ++i)
	{
		RequestTransition(pViews[i]->GetDevBuffer()->GetBuffer(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CDeviceResourceSet_Vulkan* const pResourceSet = static_cast<CDeviceResourceSet_Vulkan*>(pResources);
	auto lambda = [this](CMemoryResource* pResource, bool bWritable)
	{
		if (CDynamicOffsetBufferResource* pBuffer = pResource->AsDynamicOffsetBuffer())
		{
			RequestTransition(pBuffer, VK_ACCESS_UNIFORM_READ_BIT);
		}
		else
		{
			const VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | (bWritable ? VK_ACCESS_SHADER_WRITE_BIT : 0);
			const VkImageLayout layout = bWritable ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if (CBufferResource* pBuffer = pResource->AsBuffer())
			{
				RequestTransition(pBuffer, access);
			}
			else if (CImageResource* pImage = pResource->AsImage())
			{
				RequestTransition(pImage, layout, access);
			}
		}
	};
	pResourceSet->EnumerateInUseResources(lambda);
}

void CDeviceComputeCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlots, EShaderStage shaderStages) const
{
	buffer_size_t offset, bytes;
	CBufferResource* const pActualBuffer = pBuffer->GetD3D(&offset, &bytes);
	RequestTransition(pActualBuffer, VK_ACCESS_UNIFORM_READ_BIT);
	CRY_ASSERT(pActualBuffer->AsDynamicOffsetBuffer() != nullptr);
}

void CDeviceComputeCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	const CDeviceResourceSet_Vulkan* pVkResources = reinterpret_cast<const CDeviceResourceSet_Vulkan*>(pResources);
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	VkDescriptorSet descriptorSetHandle = pVkResources->GetVKDescriptorSet();
	CRY_ASSERT(descriptorSetHandle != VK_NULL_HANDLE);

	m_computeState.custom.pendingBindings.AppendDescriptorSet(bindSlot, descriptorSetHandle, nullptr);
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	buffer_size_t offset, size;
	NCryVulkan::CBufferResource* pVkBuffer = pBuffer->GetD3D(&offset, &size);
	CRY_ASSERT(pVkBuffer->AsDynamicOffsetBuffer() != nullptr); // Needs to be a buffer with dynamic offsets

	VkDescriptorSet dynamicDescriptorSet = pVkBuffer->AsDynamicOffsetBuffer()->GetDynamicDescriptorSet();
	CRY_ASSERT(dynamicDescriptorSet != VK_NULL_HANDLE);

	m_computeState.custom.pendingBindings.AppendDescriptorSet(bindSlot, dynamicDescriptorSet, &offset);
}

void CDeviceComputeCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO)
{
	const CDeviceComputePSO_Vulkan* pVkDevicePSO = reinterpret_cast<const CDeviceComputePSO_Vulkan*>(pDevicePSO);

	vkCmdBindPipeline(GetVKCommandList()->GetVkCommandList(), VK_PIPELINE_BIND_POINT_COMPUTE, pVkDevicePSO->GetVkPipeline());
	m_graphicsState.pPipelineState = nullptr;
}

void CDeviceComputeCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	m_computeState.custom.pendingBindings.Reset();
}

void CDeviceComputeCommandInterfaceImpl::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	const CDeviceResourceLayout_Vulkan* pVkLayout = reinterpret_cast<const CDeviceResourceLayout_Vulkan*>(m_computeState.pResourceLayout.cachedValue);

	ApplyPendingBindings(GetVKCommandList()->GetVkCommandList(), pVkLayout->GetVkPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, m_computeState.custom.pendingBindings);
	m_computeState.custom.pendingBindings.Reset();

	GetVKCommandList()->PendingResourceBarriers();
	vkCmdDispatch(GetVKCommandList()->GetVkCommandList(), X, Y, Z);
	GetVKCommandList()->m_nCommands += CLCOUNT_DISPATCH;
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	VK_ASSERT(NumRects == 0 && "Clear area not supported (yet)");
	VK_ASSERT(pView->GetResource()->GetFlag(kResourceFlagCopyWritable) && "Resource does not have required capabilities");

	CImageResource* pImage = pView->GetResource()->AsImage();
	if (pImage)
	{
		CImageView* pImageView = static_cast<CImageView*>(pView);
		VkImageSubresourceRange subresource = pImageView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

		VkClearColorValue clearValue;
		clearValue.float32[0] = Values[0];
		clearValue.float32[1] = Values[1];
		clearValue.float32[2] = Values[2];
		clearValue.float32[3] = Values[3];

		RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
	else
	{
		CBufferResource* pBuffer = pView->GetResource()->AsBuffer();
		CBufferView* pBufferView = static_cast<CBufferView*>(pView);

		VK_ASSERT(Values[0] == Values[1] && Values[0] == Values[2] && Values[0] == Values[3] && "Can only fill with 32 bit data");
		VK_ASSERT(pBuffer && "Unknown resource type");

		VkDescriptorBufferInfo info;
		pBufferView->FillInfo(info);

		RequestTransition(pBuffer, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdFillBuffer(GetVKCommandList()->GetVkCommandList(), info.buffer, info.offset, info.range, reinterpret_cast<const uint32*>(Values)[0]);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	VK_ASSERT(NumRects == 0 && "Clear area not supported (yet)");
	VK_ASSERT(pView->GetResource()->GetFlag(kResourceFlagCopyWritable) && "Resource does not have required capabilities");

	CImageResource* pImage = pView->GetResource()->AsImage();
	if (pImage)
	{
		CImageView* pImageView = static_cast<CImageView*>(pView);
		VkImageSubresourceRange subresource = pImageView->GetSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

		VkClearColorValue clearValue;
		clearValue.uint32[0] = Values[0];
		clearValue.uint32[1] = Values[1];
		clearValue.uint32[2] = Values[2];
		clearValue.uint32[3] = Values[3];

		RequestTransition(pImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdClearColorImage(GetVKCommandList()->GetVkCommandList(), pImage->GetHandle(), pImage->GetLayout(), &clearValue, 1, &subresource);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
	else
	{
		CBufferResource* pBuffer = pView->GetResource()->AsBuffer();
		CBufferView* pBufferView = static_cast<CBufferView*>(pView);

		VK_ASSERT(Values[0] == Values[1] && Values[0] == Values[2] && Values[0] == Values[3] && "Can only fill with 32 bit data");
		VK_ASSERT(pBuffer && "Unknown resource type");

		VkDescriptorBufferInfo info;
		pBufferView->FillInfo(info);

		RequestTransition(pBuffer, VK_ACCESS_TRANSFER_WRITE_BIT);
		GetVKCommandList()->PendingResourceBarriers();

		vkCmdFillBuffer(GetVKCommandList()->GetVkCommandList(), info.buffer, info.offset, info.range, Values[0]);
		GetVKCommandList()->m_nCommands += CLCOUNT_CLEAR;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static VkImageAspectFlags GetCopyableAspects(CImageResource* pImage)
{
	VkImageAspectFlags result = 0;
	result |= pImage->GetFlag(kImageFlagDepthAttachment) ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	result |= pImage->GetFlag(kImageFlagStencilAttachment) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
	result |= result == 0 ? VK_IMAGE_ASPECT_COLOR_BIT : 0;
	return result;
}

bool GetBlockSize(VkFormat format, uint32_t& blockWidth, uint32_t& blockHeight, uint32_t& blockBytes)
{
	if (VK_FORMAT_BC1_RGB_UNORM_BLOCK <= format && format <= VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
	{
		// Algorithmically select from table: 0000 1111 0011 1111 0000 1100 11...11 (into lowest bit)
		// Attribute (base >> 2)            : 0000 1111 0000 1111 0000 1111
		// Attribute (base > 21)            : 0000 0000 0000 0000 0000 0011
		// Attribute ((base & 30) == 10)    : 0000 0000 0011 0000 0000 0000
		const uint32_t base = format - VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		const uint32_t pickBit = (base >> 2) ^ (base > 21) | ((base & 30) == 10) | (base > 23);
		blockWidth = 4;
		blockHeight = 4;
		blockBytes = 8 << (pickBit & 1);
		return true;
	}
	else if (VK_FORMAT_ASTC_5x4_UNORM_BLOCK <= format && format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
	{
		static const uint8_t kBlockSizeTable[] =
		{
			(5 << 4) | 4,   // 5x4
			(5 << 4) | 5,   // 5x5
			(6 << 4) | 5,   // 6x5
			(6 << 4) | 6,   // 6x6
			(8 << 4) | 5,   // 8x5
			(8 << 4) | 6,   // 8x6
			(8 << 4) | 8,   // 8x8
			(10 << 4) | 5,  // 10x5
			(10 << 4) | 6,  // 10x6
			(10 << 4) | 8,  // 10x8
			(10 << 4) | 10, // 10x10
			(12 << 4) | 10, // 12x10
			(12 << 4) | 12, // 12x12
		};
		const uint32_t base = format - VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
		const uint8_t packed = kBlockSizeTable[base >> 1];
		blockWidth = packed >> 4;
		blockHeight = packed & 0xF;
		blockBytes = 16;
		return true;
	}

	return false;
}

void CDeviceCopyCommandInterfaceImpl::CopyBuffer(CBufferResource* pSrc, CBufferResource* pDst, const SResourceRegionMapping& mapping)
{
	const uint32_t srcSize = pSrc->GetStride() * pSrc->GetElementCount();
	const uint32_t dstSize = pDst->GetStride() * pDst->GetElementCount();
	VK_ASSERT(pSrc->GetStride() == pDst->GetStride() && "Buffer element sizes must be compatible"); // Not actually a Vk requirement, but likely a bug
	VK_ASSERT(mapping.Extent.Width > 0 && "Invalid extents");
	VK_ASSERT(mapping.SourceOffset.Left + mapping.Extent.Width <= srcSize && "Source region too large");
	VK_ASSERT(mapping.DestinationOffset.Left + mapping.Extent.Width <= dstSize && "Destination region too large");
	VK_ASSERT(mapping.SourceOffset.Subresource == 0 && mapping.DestinationOffset.Subresource == 0 && mapping.Extent.Subresources == 1 && "Buffer array slices not yet supported");

	if (pSrc == pDst)
	{
		RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	else
	{
		RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT);
		RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	GetVKCommandList()->PendingResourceBarriers();

	VkBufferCopy copy;
	copy.srcOffset = mapping.SourceOffset.Left;
	copy.dstOffset = mapping.DestinationOffset.Left;
	copy.size = mapping.Extent.Width;
	vkCmdCopyBuffer(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pDst->GetHandle(), 1, &copy);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

void CDeviceCopyCommandInterfaceImpl::CopyImage(CImageResource* pSrc, CImageResource* pDst, const SResourceRegionMapping& mapping)
{
	const uint32_t srcMips = pSrc->GetMipCount();
	const uint32_t srcFirstMip = mapping.SourceOffset.Subresource % srcMips;
	const uint32_t srcFirstSlice = mapping.SourceOffset.Subresource / srcMips;
	const uint32_t srcLastMip = (mapping.SourceOffset.Subresource + mapping.Extent.Subresources - 1) % srcMips;
	const uint32_t srcLastSlice = (mapping.SourceOffset.Subresource + mapping.Extent.Subresources - 1) / srcMips;
	const uint32_t numMips = srcLastMip - srcFirstMip + 1;
	const uint32_t numSlices = srcLastSlice - srcFirstSlice + 1;
	VK_ASSERT(mapping.Extent.Width * mapping.Extent.Height * mapping.Extent.Depth * mapping.Extent.Subresources > 0 && "Invalid extents");
	VK_ASSERT(numMips * numSlices == mapping.Extent.Subresources && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(srcFirstMip + numMips <= srcMips && srcFirstSlice + numSlices <= pSrc->GetSliceCount() && "Source subresource out of bounds");

	const uint32_t dstMips = pDst->GetMipCount();
	const uint32_t dstFirstMip = mapping.DestinationOffset.Subresource % dstMips;
	const uint32_t dstFirstSlice = mapping.DestinationOffset.Subresource / dstMips;
	VK_ASSERT(dstFirstMip + numMips <= dstMips && dstFirstSlice + numSlices <= pDst->GetSliceCount() && "Destination subresource out of bounds");

	const VkImageAspectFlags srcAspects = GetCopyableAspects(pSrc);
	const VkImageAspectFlags dstAspects = GetCopyableAspects(pDst);

	uint32_t mipIndex = 0;
	uint32_t srcWidth = mapping.Extent.Width;
	uint32_t srcHeight = mapping.Extent.Height;
	uint32_t srcDepth = mapping.Extent.Depth;
	uint32_t srcX = mapping.SourceOffset.Left;
	uint32_t srcY = mapping.SourceOffset.Top;
	uint32_t srcZ = mapping.SourceOffset.Front;
	uint32_t dstWidth = srcWidth;
	uint32_t dstHeight = srcHeight;
	uint32_t dstDepth = srcDepth;
	uint32_t dstX = mapping.DestinationOffset.Left;
	uint32_t dstY = mapping.DestinationOffset.Top;
	uint32_t dstZ = mapping.DestinationOffset.Front;

	uint32_t srcBlockWidth = 1, srcBlockHeight = 1, srcBlockBytes;
	uint32_t dstBlockWidth = 1, dstBlockHeight = 1, dstBlockBytes;
	const bool bSrcIsBlocks = GetBlockSize(pSrc->GetFormat(), srcBlockWidth, srcBlockHeight, srcBlockBytes);
	const bool bDstIsBlocks = GetBlockSize(pDst->GetFormat(), dstBlockWidth, dstBlockHeight, dstBlockBytes);
	VK_ASSERT(!(bSrcIsBlocks && bDstIsBlocks) || (srcBlockWidth == dstBlockWidth && srcBlockHeight == dstBlockHeight && srcBlockBytes == dstBlockBytes) && "Incompatible block compression");

	// Round up extents to block size, so we can do shifts on them in the loop.
	// Unlike extents, the input coordinates MUST always align to block size for all mips.
	// Note: ASTC block sizes are not necessary power-of-two, so we have to actually do modulo/division here :(
	if (bSrcIsBlocks)
	{
		srcWidth = (srcWidth + srcBlockWidth - 1) / srcBlockWidth * srcBlockWidth;
		srcHeight = (srcHeight + srcBlockHeight - 1) / srcBlockHeight * srcBlockHeight;
	}
	if (bDstIsBlocks)
	{
		dstWidth = (dstWidth + dstBlockWidth - 1) / dstBlockWidth * dstBlockWidth;
		dstHeight = (dstHeight + dstBlockHeight - 1) / dstBlockHeight * dstBlockHeight;
	}
	if (bSrcIsBlocks && !bDstIsBlocks)
	{
		dstWidth /= srcBlockWidth;
		dstHeight /= srcBlockHeight;
	}
	else if (!bSrcIsBlocks && bDstIsBlocks)
	{
		dstWidth *= srcBlockWidth;
		dstHeight *= srcBlockHeight;
	}
	VK_ASSERT(srcX % (srcBlockWidth << (numMips - 1)) == 0 && "Source offset X not aligned for block-mip access");
	VK_ASSERT(srcY % (srcBlockHeight << (numMips - 1)) == 0 && "Source offset Y not aligned for block-mip access");
	VK_ASSERT(dstX % (dstBlockWidth << (numMips - 1)) == 0 && "Destination offset X not aligned for block-mip access");
	VK_ASSERT(dstY % (dstBlockHeight << (numMips - 1)) == 0 && "Destination offset Y not aligned for block-mip access");
	VK_ASSERT(srcWidth % (srcBlockWidth << (numMips - 1)) == 0 && "Source width is not aligned for block-mip access");
	VK_ASSERT(srcHeight % (srcBlockHeight << (numMips - 1)) == 0 && "Source height is not aligned for block-mip access");
	VK_ASSERT(dstWidth % (dstBlockWidth << (numMips - 1)) == 0 && "Destination width is not aligned for block-mip access");
	VK_ASSERT(dstHeight % (dstBlockHeight << (numMips - 1)) == 0 && "Destination height is not aligned for block-mip access");

	const uint32_t srcMipWidth = std::max(pSrc->GetWidth() >> srcFirstMip, srcBlockWidth);
	const uint32_t srcMipHeight = std::max(pSrc->GetHeight() >> srcFirstMip, srcBlockHeight);
	const uint32_t srcMipDepth = std::max(pSrc->GetDepth() >> srcFirstMip, 1U);
	const uint32_t dstMipWidth = std::max(pDst->GetWidth() >> dstFirstMip, dstBlockWidth);
	const uint32_t dstMipHeight = std::max(pDst->GetHeight() >> dstFirstMip, dstBlockHeight);
	const uint32_t dstMipDepth = std::max(pDst->GetDepth() >> dstFirstMip, 1U);
	VK_ASSERT(srcX + srcWidth <= srcMipWidth && srcY + srcHeight <= srcMipHeight && srcZ + srcDepth <= srcMipDepth && "Source region too large");
	VK_ASSERT(dstX + dstWidth <= dstMipWidth && dstY + dstHeight <= dstMipHeight && dstZ + dstDepth <= dstMipDepth && "Destination region too large");

	const uint32_t kBatchSize = 16;
	VkImageCopy batch[kBatchSize];
	VK_ASSERT(numMips <= kBatchSize && "Cannot copy more than 16 mips at a time");
	for (uint32_t srcMip = srcFirstMip; srcMip <= srcLastMip; ++srcMip, ++mipIndex)
	{
		batch[mipIndex].srcSubresource.aspectMask = srcAspects;
		batch[mipIndex].srcSubresource.mipLevel = srcMip;
		batch[mipIndex].srcSubresource.baseArrayLayer = srcFirstSlice;
		batch[mipIndex].srcSubresource.layerCount = numSlices;
		batch[mipIndex].srcOffset.x = srcX;
		batch[mipIndex].srcOffset.y = srcY;
		batch[mipIndex].srcOffset.z = srcZ;
		batch[mipIndex].dstSubresource.aspectMask = srcAspects;
		batch[mipIndex].dstSubresource.mipLevel = dstFirstMip + mipIndex;
		batch[mipIndex].dstSubresource.baseArrayLayer = dstFirstSlice;
		batch[mipIndex].dstSubresource.layerCount = numSlices;
		batch[mipIndex].dstOffset.x = dstX;
		batch[mipIndex].dstOffset.y = dstY;
		batch[mipIndex].dstOffset.z = dstZ;
		batch[mipIndex].extent.width = srcWidth;
		batch[mipIndex].extent.height = srcHeight;
		batch[mipIndex].extent.depth = srcDepth;

		srcWidth = std::max(srcWidth >> 1, srcBlockWidth);
		srcHeight = std::max(srcHeight >> 1, srcBlockHeight);
		srcDepth = std::max(srcDepth >> 1, 1U);
		srcX >>= 1;
		srcY >>= 1;
		srcZ >>= 1;
		dstX >>= 1;
		dstY >>= 1;
		dstZ >>= 1;
	}

	auto prevSrcImgLayout = pSrc->GetLayout(), prevDstImgLayout = pDst->GetLayout();
	auto prevSrcImgAccess = pSrc->GetAccess(), prevDstImgAccess = pDst->GetAccess();

	if (pSrc == pDst)
	{
		RequestTransition(pSrc, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	else
	{
		RequestTransition(pSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
		RequestTransition(pDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	GetVKCommandList()->PendingResourceBarriers();

	vkCmdCopyImage(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pSrc->GetLayout(), pDst->GetHandle(), pDst->GetLayout(), mipIndex, batch);

	if (pSrc == pDst)
	{
		RequestTransition(pSrc, prevSrcImgLayout, prevSrcImgAccess);
	}
	else
	{
		RequestTransition(pSrc, prevSrcImgLayout, prevSrcImgAccess);
		RequestTransition(pDst, prevDstImgLayout, prevDstImgAccess);
	}

	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

bool CDeviceCopyCommandInterfaceImpl::FillBuffer(const void* pSrc, CBufferResource* pDst, const SResourceMemoryMapping& mapping)
{
	const uint32_t srcBytes = mapping.MemoryLayout.volumeStride;
	const uint32_t dstOffset = mapping.ResourceOffset.Left;
	const uint32_t dstBytes = mapping.Extent.Width;

	VK_ASSERT(pDst->GetFlag(kResourceFlagCpuWritable) && "Destination buffer must be mappable for the CPU");
	VK_ASSERT(mapping.ResourceOffset.Left + mapping.Extent.Width <= pDst->GetSize() && "Invalid target region");
	VK_ASSERT(mapping.ResourceOffset.Subresource == 0 && "Buffer array slices not yet supported");
	VK_ASSERT(srcBytes <= dstBytes && "Source data does not fit into target region");

	uint8_t* pMapped = static_cast<uint8_t*>(pDst->Map());
	if (pMapped)
	{
		::memcpy(pMapped + dstOffset, pSrc, srcBytes); // Non-temporal hint?
		pDst->Unmap();
	}
	return pMapped != nullptr;
}

CBufferResource* CDeviceCopyCommandInterfaceImpl::UploadBuffer(const void* pSrc, CBufferResource* pDst, const SResourceMemoryMapping& mapping, bool bAllowGpu)
{
	const uint32_t srcBytes = mapping.MemoryLayout.volumeStride;
	const uint32_t dstOffset = mapping.ResourceOffset.Left;
	VK_ASSERT(mapping.MemoryLayout.rowStride == mapping.MemoryLayout.volumeStride && "Invalid source memory layout"); // Fill uses volumeStride since it also fills images.
	VK_ASSERT(mapping.ResourceOffset.Subresource == 0 && "Buffer array slices not yet supported");

	const bool bIsUsedByGpu = false; // TODO: Implement this

	CBufferResource* pStaging = pDst;
	const SResourceMemoryMapping* pMapping = &mapping;
	SResourceMemoryMapping tempMapping;

	const bool bStage = !pDst->GetFlag(kResourceFlagCpuWritable) || bIsUsedByGpu;
	if (bStage)
	{
		// When we are not updating at the start of the buffer, we need to adjust the fill parameters,
		// since the staging buffer is not necessarily the same size as the destination buffer.
		if (mapping.ResourceOffset.Left != 0)
		{
			tempMapping = mapping;
			tempMapping.ResourceOffset.Left = 0;
			pMapping = &tempMapping;
		}

		if (GetDevice()->CreateOrReuseStagingResource(pDst, srcBytes, &pStaging, true) != VK_SUCCESS)
		{
			VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
			return nullptr;
		}
	}

	const bool bFilled = FillBuffer(pSrc, pStaging, *pMapping);
	VK_ASSERT(bFilled && "Unable to fill staging buffer");

	if (bStage)
	{
		if (bFilled)
		{
			if (!bAllowGpu)
			{
				// We would need to use the GPU to perform the upload, however the caller chose not to allow this.
				// Instead hand-off the staging buffer containing the data to the caller to handle in some other way.
				VK_ASSERT(srcBytes % pDst->GetStride() == 0 && dstOffset % pDst->GetStride() == 0);
				pStaging->SetStrideAndElementCount(pDst->GetStride(), srcBytes / pDst->GetStride());
				return pStaging;
			}

			RequestTransition(pStaging, VK_ACCESS_TRANSFER_READ_BIT);
			RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
			GetVKCommandList()->PendingResourceBarriers();

			VkBufferCopy info;
			info.srcOffset = 0;
			info.dstOffset = dstOffset;
			info.size = srcBytes;
			vkCmdCopyBuffer(GetVKCommandList()->GetVkCommandList(), pStaging->GetHandle(), pDst->GetHandle(), 1, &info);
			GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
		}

		pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
	}

	return nullptr;
}

void CDeviceCopyCommandInterfaceImpl::UploadImage(const void* pSrc, CImageResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	// For now, always stage image uploads, because all images should be tiled anyway, so direct CPU write should never be set in general.
	// However, Vk does not forbid direct CPU write to images entirely, so we may need to implement a FillImage + direct write at some future point.
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	CBufferResource* pStaging = nullptr;
	const uint32 srcBytes = mapping.MemoryLayout.volumeStride;
	if (GetDevice()->CreateOrReuseStagingResource(pDst, srcBytes, &pStaging, true) != VK_SUCCESS)
	{
		VK_ASSERT(false && "Upload buffer skipped: Cannot create staging buffer");
		return;
	}

	SResourceMemoryMapping bufferMapping;

	bufferMapping.MemoryLayout = mapping.MemoryLayout;
	bufferMapping.ResourceOffset.Left = 0;
	bufferMapping.ResourceOffset.Subresource = 0;
	bufferMapping.Extent.Width = srcBytes;
	bufferMapping.Extent.Subresources = 1;

	if (FillBuffer(pSrc, pStaging, bufferMapping))
	{
		UploadImage(pStaging, pDst, mapping, bExt);
	}

	pStaging->Release(); // trigger ReleaseLater with kResourceFlagReusable
}


void CDeviceCopyCommandInterfaceImpl::UploadImage(CBufferResource* pSrc, CImageResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	RequestTransition(pDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT);
	RequestTransition(pSrc, VK_ACCESS_TRANSFER_READ_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	const uint32_t dstMips = pDst->GetMipCount();
	const uint32_t dstFirstMip = mapping.ResourceOffset.Subresource % dstMips;
	const uint32_t dstFirstSlice = mapping.ResourceOffset.Subresource / dstMips;
	const uint32_t dstLastMip = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) % dstMips;
	const uint32_t dstLastSlice = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) / dstMips;
	const uint32_t numMips = dstLastMip - dstFirstMip + 1;
	const uint32_t numSlices = dstLastSlice - dstFirstSlice + 1;
	VK_ASSERT((bExt || numMips * numSlices == mapping.Extent.Subresources) && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(dstFirstMip + numMips <= dstMips && dstFirstSlice + numSlices <= pDst->GetSliceCount() && "Destination subresource out of bounds");

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes = mapping.MemoryLayout.typeStride;
	GetBlockSize(pDst->GetFormat(), blockWidth, blockHeight, blockBytes);

	// This is a limitation of high-level, may eventually change.
	// When it does, we need to loop over the mips and fill one struct per iteration.
	// However, we can still batch to one vkCmdCopyBufferToImage
	VK_ASSERT(numMips == 1 && "Only 1 mip at a time expected");

	VkBufferImageCopy batch;
	batch.bufferOffset = 0;
	batch.bufferRowLength = mapping.MemoryLayout.rowStride / mapping.MemoryLayout.typeStride * blockWidth;
	batch.bufferImageHeight = mapping.MemoryLayout.planeStride / mapping.MemoryLayout.rowStride * blockHeight;
	batch.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	batch.imageSubresource.mipLevel = dstFirstMip;
	batch.imageSubresource.baseArrayLayer = dstFirstSlice;
	batch.imageSubresource.layerCount = numSlices;
	batch.imageOffset.x = mapping.ResourceOffset.Left;
	batch.imageOffset.y = mapping.ResourceOffset.Top;
	batch.imageOffset.z = mapping.ResourceOffset.Front;
	batch.imageExtent.width = mapping.Extent.Width;
	batch.imageExtent.height = mapping.Extent.Height;
	batch.imageExtent.depth = mapping.Extent.Depth;
	VK_ASSERT(batch.bufferRowLength >= batch.imageExtent.width && "Bad row-stride specified");
	VK_ASSERT(batch.bufferImageHeight >= batch.imageExtent.height && "Bad plane-stride specified");
	vkCmdCopyBufferToImage(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pDst->GetHandle(), pDst->GetLayout(), 1, &batch);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

void CDeviceCopyCommandInterfaceImpl::DownloadImage(NCryVulkan::CImageResource* pSrc, NCryVulkan::CBufferResource* pDst, const SResourceMemoryMapping& mapping, bool bExt)
{
	VK_ASSERT(mapping.MemoryLayout.rowStride % mapping.MemoryLayout.typeStride == 0 && "Texel rows must be aligned to the texel size");
	VK_ASSERT(mapping.MemoryLayout.planeStride % mapping.MemoryLayout.rowStride == 0 && "Texel planes must be aligned to the texel rows");
	VK_ASSERT(mapping.MemoryLayout.volumeStride % mapping.MemoryLayout.planeStride == 0 && "Texel volumes must be aligned to the texel planes");

	RequestTransition(pDst, VK_ACCESS_TRANSFER_WRITE_BIT);
	RequestTransition(pSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT);
	GetVKCommandList()->PendingResourceBarriers();

	const uint32_t srcMips = pSrc->GetMipCount();
	const uint32_t srcFirstMip = mapping.ResourceOffset.Subresource % srcMips;
	const uint32_t srcFirstSlice = mapping.ResourceOffset.Subresource / srcMips;
	const uint32_t srcLastMip = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) % srcMips;
	const uint32_t srcLastSlice = (mapping.ResourceOffset.Subresource + mapping.Extent.Subresources - 1) / srcMips;
	const uint32_t numMips = srcLastMip - srcFirstMip + 1;
	const uint32_t numSlices = srcLastSlice - srcFirstSlice + 1;
	VK_ASSERT((bExt || numMips * numSlices == mapping.Extent.Subresources) && "Partial copies that cross slice must start at mip 0, and request whole mip chains");
	VK_ASSERT(srcFirstMip + numMips <= srcMips && srcFirstSlice + numSlices <= pSrc->GetSliceCount() && "Destination subresource out of bounds");

	uint32_t blockWidth = 1, blockHeight = 1, blockBytes = mapping.MemoryLayout.typeStride;
	GetBlockSize(pSrc->GetFormat(), blockWidth, blockHeight, blockBytes);

	// This is a limitation of high-level, may eventually change.
	// When it does, we need to loop over the mips and fill one struct per iteration.
	// However, we can still batch to one vkCmdCopyImageToBuffer
	VK_ASSERT(numMips == 1 && "Only 1 mip at a time expected");

	VkBufferImageCopy batch;
	batch.bufferOffset = 0;
	batch.bufferRowLength = mapping.MemoryLayout.rowStride / mapping.MemoryLayout.typeStride * blockWidth;
	batch.bufferImageHeight = mapping.MemoryLayout.planeStride / mapping.MemoryLayout.rowStride * blockHeight;
	batch.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	batch.imageSubresource.mipLevel = srcFirstMip;
	batch.imageSubresource.baseArrayLayer = srcFirstSlice;
	batch.imageSubresource.layerCount = numSlices;
	batch.imageOffset.x = mapping.ResourceOffset.Left;
	batch.imageOffset.y = mapping.ResourceOffset.Top;
	batch.imageOffset.z = mapping.ResourceOffset.Front;
	batch.imageExtent.width = mapping.Extent.Width;
	batch.imageExtent.height = mapping.Extent.Height;
	batch.imageExtent.depth = mapping.Extent.Depth;
	VK_ASSERT(batch.bufferRowLength >= batch.imageExtent.width && "Bad row-stride specified");
	VK_ASSERT(batch.bufferImageHeight >= batch.imageExtent.height && "Bad plane-stride specified");
	vkCmdCopyImageToBuffer(GetVKCommandList()->GetVkCommandList(), pSrc->GetHandle(), pSrc->GetLayout(), pDst->GetHandle(), 1, &batch);
	GetVKCommandList()->m_nCommands += CLCOUNT_COPY;
}

