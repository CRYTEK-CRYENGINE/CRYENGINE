// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "DeviceResourceSet_D3D12.h"
#include "DeviceCommandListCommon_D3D12.h"
#include "DevicePSO_D3D12.h"

#if CRY_PLATFORM_WINDOWS
#	ifdef ENABLE_PROFILING_CODE
#		define USE_PIX
#	endif
#	include <WinPixEventRuntime/pix3.h>
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDeviceTimestampGroup::s_reservedGroups[MAX_TIMESTAMP_GROUPS] = { false };

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_groupIndex(0xFFFFFFFF)
	, m_fence(0)
	, m_frequency(0)
	, m_measurable(false)
	, m_measured(false)
{
	m_timeValues.fill(0);
}

CDeviceTimestampGroup::~CDeviceTimestampGroup()
{
	s_reservedGroups[m_groupIndex] = false;
}

void CDeviceTimestampGroup::Init()
{
	GetDeviceObjectFactory().CreateFence(m_fence);
	m_numTimestamps = 0;

	m_groupIndex = 0xFFFFFFFF;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(s_reservedGroups); i++)
	{
		if (s_reservedGroups[i] == false)
		{
			s_reservedGroups[i] = true;
			m_groupIndex = i;
			break;
		}
	}

	DX12_ASSERT(m_groupIndex < 0xFFFFFFFF);
}

void CDeviceTimestampGroup::BeginMeasurement()
{
	m_numTimestamps = 0;
	m_frequency = 0;
	m_measurable = false;
	m_measured = false;
}

void CDeviceTimestampGroup::EndMeasurement()
{
	auto* pDX12Device = GetDeviceObjectFactory().GetDX12Device();
	auto* pDX12Scheduler = GetDeviceObjectFactory().GetDX12Scheduler();
	auto* pDX12CmdList = GetDeviceObjectFactory().GetCoreCommandList().GetDX12CommandList();

	pDX12Device->IssueTimestampResolve(pDX12CmdList, m_groupIndex * kMaxTimestamps, m_numTimestamps);
	m_fence = (DeviceFenceHandle)pDX12Scheduler->InsertFence();

	m_measurable = true;
}

uint32 CDeviceTimestampGroup::IssueTimestamp(CDeviceCommandList* pCommandList)
{
	// Passing a nullptr means we want to use the current core command-list
	auto* pDX12Device = GetDeviceObjectFactory().GetDX12Device();
	auto* pDX12CmdList = (pCommandList ? *pCommandList : GetDeviceObjectFactory().GetCoreCommandList()).GetDX12CommandList();

	DX12_ASSERT(m_numTimestamps < kMaxTimestamps);

	const uint32 timestampIndex = m_groupIndex * kMaxTimestamps + m_numTimestamps;
	pDX12Device->InsertTimestamp(pDX12CmdList, timestampIndex);

	if((m_numTimestamps + 1) < kMaxTimestamps)
	{
		++m_numTimestamps;
	}

	return timestampIndex;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (!m_measurable)
		return false;
	if (m_measured || m_numTimestamps == 0)
		return true;

	auto* pDX12Device = GetDeviceObjectFactory().GetDX12Device();
	auto* pDX12Scheduler = GetDeviceObjectFactory().GetDX12Scheduler();

	if (pDX12Scheduler->TestForFence(m_fence) != S_OK)
		return false;

	m_frequency = pDX12Device->GetTimestampFrequency();
	pDX12Device->QueryTimestamps(m_groupIndex * kMaxTimestamps, m_numTimestamps, &m_timeValues[0]);

	return m_measured = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCommandListImpl::SetProfilerMarker(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PIXSetMarker(GetDX12CommandList()->GetD3D12CommandList(), 0, label);
#endif
}

void CDeviceCommandListImpl::BeginProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	m_profilerEventStack.push_back(label);
	PIXBeginEvent(GetDX12CommandList()->GetD3D12CommandList(), 0, label);
#endif
}

void CDeviceCommandListImpl::EndProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PIXEndEvent(GetDX12CommandList()->GetD3D12CommandList());
	m_profilerEventStack.pop_back();
#endif
}

void CDeviceCommandListImpl::ClearStateImpl(bool bOutputMergerOnly) const
{
}

void CDeviceCommandListImpl::CeaseCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

#if defined(USE_CRY_ASSERT) || defined(ENABLE_FRAME_PROFILER_LABELS)
	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
#endif

	CRY_ASSERT(m_sharedState.pCommandList == pCommandList || m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = nullptr;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (size_t num = m_profilerEventStack.size(), i = 0; i < num; ++i)
	{
		PIXEndEvent(pCommandList->GetD3D12CommandList());
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
		PIXBeginEvent(pCommandList->GetD3D12CommandList(), 0, pEventLabel);
	}
#endif
}

void CDeviceCommandListImpl::ResetImpl()
{
	m_graphicsState.custom.primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void CDeviceCommandListImpl::LockToThreadImpl()
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	pCommandListDX12->Begin();
	pCommandListDX12->SetResourceAndSamplerStateHeaps();
}

void CDeviceCommandListImpl::CloseImpl()
{
	FUNCTION_PROFILER_RENDERER();
	GetDX12CommandList()->End();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CDeviceBuffer** pViews, bool bCompute) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	for (int v = 0; v < viewCount; ++v)
	{
		// TODO: ResourceViewHandles[]
		const NCryDX12::CView& View = GET_DX12_UNORDERED_VIEW(pViews[v], EDefaultResourceViews::UnorderedAccess)->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			bCompute
			? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
			: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareRenderPassForUseImpl(CDeviceRenderPass& renderPass) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	if (renderPass.m_pDepthStencilView)
	{
		const NCryDX12::CView& View = renderPass.m_pDepthStencilView->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		D3D11_DEPTH_STENCIL_VIEW_DESC Desc; renderPass.m_pDepthStencilView->GetDesc(&Desc);
		const D3D12_RESOURCE_STATES desiredState = Desc.Flags & D3D11_DSV_READ_ONLY_DEPTH ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		pCommandListDX12->PrepareResourceDSVUsage(Resource, View, desiredState);
	}

	// Get current render target views
	for (int i = 0; i < renderPass.m_RenderTargetCount; ++i)
	{
		const NCryDX12::CView& View = renderPass.m_RenderTargetViews[i]->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource(); Resource.VerifyBackBuffer(true);

		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;

		pCommandListDX12->PrepareResourceRTVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourceForUseImpl(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	CCryDX12ShaderResourceView* it = GET_DX12_SHADER_VIEW(pTexture->GetDevTexture(), TextureView);
	{
		const NCryDX12::CView& View = it->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			(srvUsage &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON) |
			(srvUsage & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceSRVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}

		pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceSRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_CONSTANTBUFFER_RESOURCE(pBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	if (Resource.InitHasBeenDeferred())
	{
		Resource.InitDeferred(pCommandListDX12, desiredState);
	}

	pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, eHWSC_NumGfx);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineShaderResourceForUseImpl(uint32 bindSlot, CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_RAWBUFFER_RESOURCE(pBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	if (Resource.InitHasBeenDeferred())
	{
		Resource.InitDeferred(pCommandListDX12, desiredState);
	}

	pCommandListDX12->PrepareResourceSRVUsage(Resource, desiredState);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineShaderResourceForUseImpl(uint32 bindSlot, CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
	PrepareInlineShaderResourceForUseImpl(bindSlot, pBuffer, shaderSlot, eHWSC_NumGfx);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareVertexBuffersForUseImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const
{
	if (m_graphicsState.vertexStreams != vertexStreams)
	{
		NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

		for (uint32 s = 0; s < numStreams; ++s)
		{
			const CDeviceInputStream& vertexStream = vertexStreams[s];

			CRY_ASSERT(vertexStream.hStream != ~0u);
			{
				// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
				buffer_size_t offset;
				auto pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset));
				NCryDX12::CResource& Resource = pBuffer->GetDX12Resource();
				const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

				if (Resource.InitHasBeenDeferred())
				{
					Resource.InitDeferred(pCommandListDX12, desiredState);
				}

				pCommandListDX12->PrepareResourceVBVUsage(Resource, desiredState);
			}
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareIndexBufferForUseImpl(const CDeviceInputStream* indexStream) const
{
	if (m_graphicsState.indexStream != indexStream)
	{
		NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

		CRY_ASSERT(indexStream->hStream != ~0u);
		{
			// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
			buffer_size_t offset;
			auto pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset));
			NCryDX12::CResource& Resource = pBuffer->GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_INDEX_BUFFER;

			if (Resource.InitHasBeenDeferred())
			{
				Resource.InitDeferred(pCommandListDX12, desiredState);
			}

			pCommandListDX12->PrepareResourceIBVUsage(Resource, desiredState);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::BeginResourceTransitionsImpl(uint32 numTextures, CTexture** pTextures, EResourceTransitionType type)
{
	if (!CRenderer::CV_r_D3D12EarlyResourceBarriers)
		return;

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	uint32 numBarriers = 0;
	for (uint32 i = 0; i < numTextures; i++)
	{
		CCryDX12Resource<ID3D11ResourceToImplement>* pResource = nullptr;
		if (pTextures[i] && pTextures[i]->GetDevTexture())
		{
			pResource = DX12_EXTRACT_RESOURCE(pTextures[i]->GetDevTexture()->GetBaseTexture());
		}

		if (pResource != nullptr)
		{
			NCryDX12::CResource& resource = pResource->GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			if (resource.NeedsTransitionBarrier(pCommandListDX12, desiredState))
			{
				pCommandListDX12->MaxResourceFenceValue(resource, CMDTYPE_WRITE);
				pCommandListDX12->BeginResourceStateTransition(resource, desiredState);
				pCommandListDX12->SetResourceFenceValue(resource, CMDTYPE_READ);
				numBarriers += 1;
			}
		}
	}

	if (numBarriers > 0)
	{
		pCommandListDX12->PendingResourceBarriers();

		if (CRenderer::CV_r_D3D12EarlyResourceBarriers > 1)
			gcpRendD3D->GetDeviceContext_Unsynchronized().Flush();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView* pDSV = nullptr;
	const NCryDX12::CView* pRTV[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };

	// Get current depth stencil views
	if (renderPass.m_pDepthStencilView)
	{
		const NCryDX12::CView& View = renderPass.m_pDepthStencilView->GetDX12View();

		pDSV = &View;

		D3D11_DEPTH_STENCIL_VIEW_DESC Desc; renderPass.m_pDepthStencilView->GetDesc(&Desc);
		const D3D12_RESOURCE_STATES desiredState = Desc.Flags & D3D11_DSV_READ_ONLY_DEPTH ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		DX12_ASSERT(!View.GetDX12Resource().NeedsTransitionBarrier(pCommandListDX12, View, desiredState));
	}

	// Get current render target views
	for (int i = 0; i < renderPass.m_RenderTargetCount; ++i)
	{
		const NCryDX12::CView& View = renderPass.m_RenderTargetViews[i]->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource(); Resource.VerifyBackBuffer(true);

		pRTV[i] = &View;

		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, *pRTV[i], D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	pCommandListDX12->BindAndSetOutputViews(renderPass.m_RenderTargetCount, pRTV, pDSV);
}

void CDeviceGraphicsCommandInterfaceImpl::SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports)
{
	// D3D11_VIEWPORT := D3D12_VIEWPORT
	GetDX12CommandList()->SetViewports(vpCount, (D3D12_VIEWPORT*)pViewports);
}

void CDeviceGraphicsCommandInterfaceImpl::SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects)
{
	// D3D11_RECT := D3D12_RECT
	GetDX12CommandList()->SetScissorRects(rcCount, (D3D12_RECT*)pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceGraphicsPSO* devicePSO)
{
	auto pCommandListDX12 = GetDX12CommandList();
	const CDeviceGraphicsPSO_DX12* pDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_DX12*>(devicePSO);

	pCommandListDX12->SetPSO(pDevicePSO->GetGraphicsPSO());
	m_computeState.pPipelineState = nullptr; // on dx12 pipeline state is shared between graphics and compute

	D3D12_PRIMITIVE_TOPOLOGY psoPrimitiveTopology = pDevicePSO->GetPrimitiveTopology();
	if (m_graphicsState.custom.primitiveTopology.Set(psoPrimitiveTopology))
	{
#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumTopologySets);
#endif

		pCommandListDX12->SetPrimitiveTopology(psoPrimitiveTopology);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* resourceLayout)
{
	const CDeviceResourceLayout_DX12* pResourceLayoutDX12 = reinterpret_cast<const CDeviceResourceLayout_DX12*>(resourceLayout);
	GetDX12CommandList()->SetGraphicsRootSignature(pResourceLayoutDX12->GetRootSignature());
}

void CDeviceGraphicsCommandInterfaceImpl::SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	pCommandListDX12->ClearVertexBufferHeap(lastStreamSlot + 1);

	for (uint32 s = 0; s < numStreams; ++s)
	{
		const CDeviceInputStream& vertexStream = vertexStreams[s];

		CRY_ASSERT(vertexStream.hStream != ~0u);
		{
			// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
			buffer_size_t offset;
			auto* pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset));

			DX12_ASSERT(!pBuffer->GetDX12Resource().InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", pBuffer->GetDX12Resource().GetName());
			DX12_ASSERT(!pBuffer->GetDX12Resource().NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
			CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundVertexBuffers[pBuffer->GetDX12Resource().IsOffCard()]);
#endif

			pCommandListDX12->BindVertexBufferView(pBuffer->GetDX12View(), vertexStream.nSlot, TRange<uint32>(offset, offset), vertexStream.nStride);
		}
	}

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->SetVertexBufferHeap(lastStreamSlot + 1);
}

void CDeviceGraphicsCommandInterfaceImpl::SetIndexBufferImpl(const CDeviceInputStream* indexStream)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	CRY_ASSERT(indexStream->hStream != ~0u);
	{
		// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
		buffer_size_t offset;
		auto* pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset));

		DX12_ASSERT(!pBuffer->GetDX12Resource().InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", pBuffer->GetDX12Resource().GetName());
		DX12_ASSERT(!pBuffer->GetDX12Resource().NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_INDEX_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundIndexBuffers[pBuffer->GetDX12Resource().IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
		pCommandListDX12->BindAndSetIndexBufferView(pBuffer->GetDX12View(), DXGI_FORMAT_R16_UINT, offset);
#else
		pCommandListDX12->BindAndSetIndexBufferView(pBuffer->GetDX12View(), (DXGI_FORMAT)indexStream->nStride, UINT(offset));
#endif
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	const CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<const CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundConstBuffers[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, View, desiredState));

#if defined(ENABLE_PROFILING_CODE)
		const bool bIsBuffer = Resource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
		if (bIsBuffer) CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformBuffers[Resource.IsOffCard()]);
		else           CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformTextures[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourcePRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

#if defined(ENABLE_PROFILING_CODE)
		const bool bIsBuffer = Resource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
		if (bIsBuffer) CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformBuffers[Resource.IsOffCard()]);
		else           CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformTextures[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceUAVUsage(Resource, View, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	const CDescriptorBlock& descriptorBlock = pResourcesDX12->GetDescriptorBlock();
	pCommandListDX12->SetGraphicsDescriptorTable(bindSlot, descriptorBlock.GetHandleOffsetGPU(0));
}



void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages)
{
	SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, eHWSC_Num);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_CONSTANTBUFFER_RESOURCE(pBuffer)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = View.GetCBVDesc().BufferLocation + pBuffer->m_offset;
	pCommandListDX12->SetGraphicsConstantBufferView(bindSlot, gpuAddress);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineShaderResourceImpl(uint32 bindSlot, const CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot, ::EShaderStage shaderStages, ResourceViewHandle resourceViewID)
{
	SetInlineShaderResourceImpl(bindSlot, pBuffer, shaderSlot, eHWSC_Num, resourceViewID);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineShaderResourceImpl(uint32 bindSlot, const CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot, EHWShaderClass shaderClass, ResourceViewHandle resourceViewID)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_SHADER_VIEW(pBuffer, resourceViewID)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	DX12_ASSERT(!Resource.InitHasBeenDeferred());
	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceSRVUsage(Resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = Resource.GetGPUVirtualAddress();
	pCommandListDX12->SetGraphicsShaderResourceView(bindSlot, gpuAddress);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	GetDX12CommandList()->SetGraphics32BitConstants(bindSlot, constantCount, pConstants, 0);
}

void CDeviceGraphicsCommandInterfaceImpl::SetStencilRefImpl(uint8 stencilRefValue)
{
	GetDX12CommandList()->SetStencilRef(stencilRefValue);
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp)
{
	CRY_ASSERT_MESSAGE(false, "Depth bias can only be set via PSO on DirectX 12");
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBoundsImpl(float fMin, float fMax)
{
	GetDX12CommandList()->SetDepthBounds(fMin, fMax);
}

void CDeviceGraphicsCommandInterfaceImpl::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	GetDX12CommandList()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CDeviceGraphicsCommandInterfaceImpl::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	GetDX12CommandList()->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12RenderTargetView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearRenderTargetView(View, Color, NumRects, pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12DepthStencilView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearDepthStencilView(View, D3D12_CLEAR_FLAGS(clearFlags), depth, stencil, numRects, pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::DiscardContentsImpl(D3DResource* pResource, uint32 numRects, const D3D11_RECT* pRects)
{
	NCryDX12::CResource& Resource = reinterpret_cast<CCryDX12Resource<IEmptyResource>*>(pResource)->GetDX12Resource();
	D3D12_DISCARD_REGION rRegion = { numRects, pRects, 0, Resource.GetDesc().MipLevels * Resource.GetDesc().DepthOrArraySize };
	GetDX12CommandList()->DiscardResource(Resource, &rRegion);
}

void CDeviceGraphicsCommandInterfaceImpl::DiscardContentsImpl(D3DBaseView* pView, uint32 numRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12RenderTargetView*>(pView)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();
	D3D12_DISCARD_REGION rRegion = { numRects, pRects, 0, 0x7FFFFFFF }; // TODO: calculate sub-resources and loop according to the given view
	GetDX12CommandList()->DiscardResource(Resource, &rRegion);
}

void CDeviceGraphicsCommandInterfaceImpl::BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	CRY_ASSERT(GetDX12CommandList() == GetDeviceObjectFactory().GetCoreCommandList().GetDX12CommandList());
	gcpRendD3D->GetDeviceContext().GetRealDeviceContext()->Begin(pQuery);
}

void CDeviceGraphicsCommandInterfaceImpl::EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	CRY_ASSERT(GetDX12CommandList() == GetDeviceObjectFactory().GetCoreCommandList().GetDX12CommandList());
	gcpRendD3D->GetDeviceContext().GetRealDeviceContext()->End(pQuery);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceComputeCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CDeviceBuffer** pViews) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	for (int v = 0; v < viewCount; ++v)
	{
		// TODO: ResourceViewHandles[]
		const NCryDX12::CView& View = GET_DX12_UNORDERED_VIEW(pViews[v], EDefaultResourceViews::UnorderedAccess)->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}

		pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceCRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		DX12_ASSERT(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_CONSTANTBUFFER_RESOURCE(pBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
}

void CDeviceComputeCommandInterfaceImpl::PrepareInlineShaderResourceForUseImpl(uint32 bindSlot, CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_RAWBUFFER_RESOURCE(pBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	pCommandListDX12->PrepareResourceSRVUsage(Resource, desiredState);
}

void CDeviceComputeCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO)
{
	const CDeviceComputePSO_DX12* pDevicePsoDX12 = reinterpret_cast<const CDeviceComputePSO_DX12*>(pDevicePSO);
	GetDX12CommandList()->SetPSO(pDevicePsoDX12->GetComputePSO());
	m_graphicsState.pPipelineState = nullptr; // on dx12 pipeline state is shared between graphics and compute
}

void CDeviceComputeCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	const CDeviceResourceLayout_DX12* pResourceLayoutDX12 = reinterpret_cast<const CDeviceResourceLayout_DX12*>(pResourceLayout);
	GetDX12CommandList()->SetComputeRootSignature(pResourceLayoutDX12->GetRootSignature());
}

void CDeviceComputeCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	const CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<const CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundConstBuffers[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, View, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformTextures[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCRVUsage(Resource, View, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
		DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundUniformBuffers[Resource.IsOffCard()]);
#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceUAVUsage(Resource, View, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	const CDescriptorBlock& descriptorBlock = pResourcesDX12->GetDescriptorBlock();
	pCommandListDX12->SetComputeDescriptorTable(bindSlot, descriptorBlock.GetHandleOffsetGPU(0));
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_CONSTANTBUFFER_RESOURCE(pBuffer)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	DX12_ASSERT(!Resource.InitHasBeenDeferred(), "Resource %s hasn't been uploaded prior to use!", Resource.GetName());
	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = View.GetCBVDesc().BufferLocation + pBuffer->m_offset;
	pCommandListDX12->SetComputeConstantBufferView(bindSlot, gpuAddress);
}

void CDeviceComputeCommandInterfaceImpl::SetInlineShaderResourceImpl(uint32 bindSlot, const CDeviceBuffer* pBuffer, EShaderResourceShaderSlot shaderSlot, ResourceViewHandle resourceViewID)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_SHADER_VIEW(pBuffer, resourceViewID)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&SRenderStatistics::Write().m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceSRVUsage(Resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = Resource.GetGPUVirtualAddress();
	pCommandListDX12->SetComputeShaderResourceView(bindSlot, gpuAddress);
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	GetDX12CommandList()->SetCompute32BitConstants(bindSlot, constantCount, pConstants, 0);
}

void CDeviceComputeCommandInterfaceImpl::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	GetDX12CommandList()->Dispatch(X, Y, Z);
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12UnorderedAccessView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearUnorderedAccessView(View, Values, NumRects, pRects);
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12UnorderedAccessView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearUnorderedAccessView(View, Values, NumRects, pRects);
}

void CDeviceComputeCommandInterfaceImpl::DiscardUAVContentsImpl(D3DResource* pResource, uint32 numRects, const D3D11_RECT* pRects)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	NCryDX12::CResource& Resource = reinterpret_cast<CCryDX12Resource<IEmptyResource>*>(pResource)->GetDX12Resource();
	D3D12_DISCARD_REGION rRegion = { numRects, pRects, 0, 1 }; // NOTE: UAV-arrays are technically possible but not supported here
	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	pCommandListDX12->DiscardResource(Resource, &rRegion);
}

void CDeviceComputeCommandInterfaceImpl::DiscardUAVContentsImpl(D3DBaseView* pView, uint32 numRects, const D3D11_RECT* pRects)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12RenderTargetView*>(pView)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();
	D3D12_DISCARD_REGION rRegion = { numRects, pRects, 0, 0x7FFFFFFF }; // TODO: calculate sub-resources and loop according to the given view
	DX12_ASSERT(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	pCommandListDX12->DiscardResource(Resource, &rRegion);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseBuffer(), pSrc->GetBaseBuffer());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseBuffer(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseBuffer(), region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };

	if (region.Flags & DX12_COPY_CONCURRENT_MARKER)
	{
		ICryDX12Resource* dstDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pDst->GetBaseTexture());
		ICryDX12Resource* srcDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pSrc->GetBaseTexture());
		NCryDX12::CResource& rDstResource = dstDX12Resource->GetDX12Resource(); rDstResource.VerifyBackBuffer(true);
		NCryDX12::CResource& rSrcResource = srcDX12Resource->GetDX12Resource(); rSrcResource.VerifyBackBuffer(false);


		D3D12_RESOURCE_STATES prevDstState = rDstResource.GetState(), dstState = prevDstState;
		D3D12_RESOURCE_STATES prevSrcState = rSrcResource.GetState(), srcState = prevSrcState;

		GetDX12CommandList()->MaxResourceFenceValue(rDstResource, CMDTYPE_ANY);
		GetDX12CommandList()->MaxResourceFenceValue(rSrcResource, CMDTYPE_WRITE);

		rDstResource.TransitionBarrierStatic(GetDX12CommandList(), D3D12_RESOURCE_STATE_COPY_DEST, dstState);
		rSrcResource.TransitionBarrierStatic(GetDX12CommandList(), D3D12_RESOURCE_STATE_COPY_SOURCE, srcState);

		GetDX12CommandList()->PendingResourceBarriers();

		CD3DX12_TEXTURE_COPY_LOCATION src(rSrcResource.GetD3D12Resource(), region.SourceOffset.Subresource);
		CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetD3D12Resource(), region.DestinationOffset.Subresource);

		GetDX12CommandList()->CopyTextureRegion(
			&dst, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front,
			&src, reinterpret_cast<const D3D12_BOX*>(&box));
		GetDX12CommandList()->m_nCommands += CLCOUNT_COPY;

		GetDX12CommandList()->SetResourceFenceValue(rDstResource, CMDTYPE_WRITE);
		GetDX12CommandList()->SetResourceFenceValue(rSrcResource, CMDTYPE_READ);

		rDstResource.TransitionBarrierStatic(GetDX12CommandList(), prevDstState, dstState);
		rSrcResource.TransitionBarrierStatic(GetDX12CommandList(), prevSrcState, srcState);
	}
	else
	{
		rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
	}
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetD3D(), 0, nullptr, pSrc, 0, 0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseBuffer(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseTexture(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetD3D(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseBuffer(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseTexture(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	DX12_ASSERT(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	DX12_ASSERT(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	DX12_ASSERT(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	DX12_ASSERT(0);
}
