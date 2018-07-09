// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Device.hpp"
#include "CCryDX12DeviceContext.hpp"

#include "DX12/Resource/CCryDX12Resource.hpp"

#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"
#include "DX12/Resource/Misc/CCryDX12Shader.hpp"
#include "DX12/Resource/Misc/CCryDX12Query.hpp"

#include "DX12/Resource/State/CCryDX12SamplerState.hpp"

#include "DX12/Resource/Texture/CCryDX12Texture1D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture3D.hpp"

#include "DX12/Resource/View/CCryDX12DepthStencilView.hpp"
#include "DX12/Resource/View/CCryDX12RenderTargetView.hpp"
#include "DX12/Resource/View/CCryDX12ShaderResourceView.hpp"
#include "DX12/Resource/View/CCryDX12UnorderedAccessView.hpp"

#include "DX12/API/DX12Device.hpp"

#define DX12_SUBMISSION_UNBOUND 3 // never commit, only when the heaps overflow, or on present
#define DX12_SUBMISSION_PERPSO  2 // commit whenever the PSO changes
#define DX12_SUBMISSION_PERDRAW 1 // commit whenever a new draw is requested
#define DX12_SUBMISSION_SYNC    0 // commit always and wait as well

#define DX12_SUBMISSION_MODE    DX12_SUBMISSION_UNBOUND
//efine	DX12_SUBMISSION_LOWLATENCY // reevaluate when we have separate queues/copy queue to not interrupt pipeline state from command-list to command-list

// Workaround for old pipeline submission. It can not be predicted in which precise
// state the resource is going to be used when the resource is entering the regular
// graphics pipeline. So we just add more state-flags to be conservative (flags are inclusive).
// NOTE: This makes the emulator less correct, revert it if you want to use it in the future.
#define CS_STATE	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
#define PS_STATE	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
#define CP_STATE	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE

CCryDX12DeviceContext* CCryDX12DeviceContext::Create(CCryDX12Device* pDevice, UINT nodeMask, bool isDeferred)
{
	return DX12_NEW_RAW(CCryDX12DeviceContext(pDevice, nodeMask, isDeferred));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static auto lambdaCeaseCallback = [](void* ctx, uint nPoolId)
{
	reinterpret_cast<CCryDX12DeviceContext*>(ctx)->CeaseCommandQueueCallback(nPoolId);
};

static auto lambdaResumeCallback = [](void* ctx, uint nPoolId)
{
	reinterpret_cast<CCryDX12DeviceContext*>(ctx)->ResumeCommandQueueCallback(nPoolId);
};

CCryDX12DeviceContext::CCryDX12DeviceContext(CCryDX12Device* pDevice, UINT nodeMask, bool bDeferred)
	: Super()
	, m_bDeferred(bDeferred)
	, m_nodeMask(nodeMask)
	, m_pDevice(pDevice)
	, m_pDX12Device(pDevice->GetDX12Device())
	, m_Scheduler(pDevice->GetDX12Device()->GetScheduler())
	, m_TimestampHeap(pDevice->GetDX12Device())
	, m_OcclusionHeap(pDevice->GetDX12Device())
{
	DX12_FUNC_LOG

	//	g_nPrintDX12 = true;

	m_TimestampIndex = 0;
	m_OcclusionIndex = 0;

	// Timer query heap
	{
		D3D12_QUERY_HEAP_DESC desc = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 4096, m_nodeMask };
		m_TimestampHeap.Init(m_pDX12Device, desc);
	}

	// Occlusion query heap
	{
		D3D12_QUERY_HEAP_DESC desc = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, 64, m_nodeMask };
		m_OcclusionHeap.Init(m_pDX12Device, desc);
	}

	if (S_OK != m_pDX12Device->CreateOrReuseCommittedResource(
	      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, blsi(m_nodeMask), m_nodeMask),
	      D3D12_HEAP_FLAG_NONE,
	      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * m_TimestampHeap.GetCapacity()),
	      D3D12_RESOURCE_STATE_COPY_DEST,
	      nullptr,
	      IID_GFX_ARGS(&m_TimestampDownloadBuffer)))
	{
		DX12_ERROR("Could not create intermediate timestamp download buffer!");
	}

	if (S_OK != m_pDX12Device->CreateOrReuseCommittedResource(
	      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, blsi(m_nodeMask), m_nodeMask),
	      D3D12_HEAP_FLAG_NONE,
	      &CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * m_OcclusionHeap.GetCapacity()),
	      D3D12_RESOURCE_STATE_COPY_DEST,
	      nullptr,
	      IID_GFX_ARGS(&m_OcclusionDownloadBuffer)))
	{
		DX12_ERROR("Could not create intermediate occlusion download buffer!");
	}

	m_pSamplerHeap = NULL;
	m_pResourceHeap = NULL;

	m_TimestampMemory = nullptr;
	m_OcclusionMemory = nullptr;
	m_TimestampMapValid = false;
	m_OcclusionMapValid = false;

	m_bCmdListBegins[CMDQUEUE_GRAPHICS] = true;
	m_bCmdListBegins[CMDQUEUE_COMPUTE] = true;
	m_bCmdListBegins[CMDQUEUE_COPY] = true;

	m_Scheduler.AddQueueEventCallback(this, lambdaCeaseCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_CEASE);
	m_Scheduler.AddQueueEventCallback(this, lambdaResumeCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_RESUME);
}

CCryDX12DeviceContext::~CCryDX12DeviceContext()
{
	DX12_FUNC_LOG

	D3D12_RANGE sNoWrite = { 0, 0 };
	if (m_TimestampMemory)
	{
		m_TimestampDownloadBuffer->Unmap(0, &sNoWrite);
	}
	if (m_OcclusionMemory)
	{
		m_OcclusionDownloadBuffer->Unmap(0, &sNoWrite);
	}

	m_TimestampDownloadBuffer->Release();
	m_OcclusionDownloadBuffer->Release();
}

bool CCryDX12DeviceContext::PrepareGraphicsPSO()
{
	const UINT& stateFlags = m_GraphicsState.m_StateFlags;

	bool bNewPSO = false;
	bool bNewRS = false;

	if (stateFlags & EPSPB_PipelineState)
	{
		NCryDX12::CDevice* device = m_pDevice->GetDX12Device();

		DX12_PTR(NCryDX12::CRootSignature) newRootSignature;
		DX12_PTR(NCryDX12::CGraphicsPSO) newPSO;

		NCryDX12::CRootSignature::SInitParams rsParams;
		NCryDX12::CGraphicsPSO::SInitParams psoParams;

		m_GraphicsState.InitRootSignatureInitParams(rsParams);
		m_GraphicsState.InitD3D12Descriptor(psoParams, m_nodeMask);

		// Get or create root signature
		// Get or create PSO
		{
			device->GetRootSignatureCache().GetOrCreateRootSignature(rsParams, true, m_nodeMask, newRootSignature);
			psoParams.m_Desc.pRootSignature = newRootSignature->GetD3D12RootSignature();
			device->GetPSOCache().GetOrCreatePSO(psoParams, newPSO);
		}

		// Did something change?
		bNewPSO = m_bCmdListBegins[CMDQUEUE_GRAPHICS] || (newPSO != m_pCurrentPSO) || !newPSO || !m_pCurrentPSO;
		bNewRS = m_bCmdListBegins[CMDQUEUE_GRAPHICS] || (newRootSignature != m_pCurrentRootSignature) || !newRootSignature || !m_pCurrentRootSignature;
		if (bNewPSO || bNewRS)
		{
			m_pCurrentPSO = newPSO;
			m_pCurrentRootSignature = newRootSignature;

			if (!m_pCurrentPSO || !m_pCurrentRootSignature)
			{
				return false;
			}
		}
		else
		{
			// False positive, clear dirty flags
			m_GraphicsState.m_StateFlags &= ~EPSPB_PipelineState;
		}
	}

	// check for overflow and submit early
	if (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUtilized() && (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsFull(
	                                                       /* 256, */ m_pCurrentRootSignature->GetResourceMappings().m_NumResources,
	                                                       /*  16, */ m_pCurrentRootSignature->GetResourceMappings().m_NumDynamicSamplers,
	                                                       /*   8, */ m_GraphicsState.OutputMerger.NumRenderTargets.Get(),
	                                                       /*   1, */ m_GraphicsState.OutputMerger.DepthStencilView ? 1 : 0
	                                                       ) || ((DX12_SUBMISSION_MODE == DX12_SUBMISSION_PERPSO) && (stateFlags & EPSPB_PipelineState))))
	{
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, false);

#ifdef DX12_STATS
		m_NumCommandListOverflows++;
#endif // DX12_STATS

		bNewPSO = true;
		bNewRS = true;

		m_GraphicsState.Invalidate();
	}

	if (stateFlags & EPSPB_PipelineState)
	{
		// assume the constant buffer is set on the vertex shader always (don't need to search for the first occurrence)
		D3D12_GPU_VIRTUAL_ADDRESS vConstViews[8 /* CB_NUM */] = { 0 };

		// Assign new PSO and root signature and set resource binding dirty flags
		m_bCmdListBegins[CMDQUEUE_GRAPHICS] = false;
		if (bNewPSO)
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetPSO(m_pCurrentPSO);
		if (bNewRS)
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetRootSignature(m_pCurrentRootSignature, true, vConstViews);

		// If a root signature is changed on a command list, all previous root signature bindings
		// become stale and all newly expected bindings must be set before Draw/Dispatch
		if (bNewRS)
			m_GraphicsState.m_StateFlags |= EPSPB_InputResources;
	}

	return true;
}

bool CCryDX12DeviceContext::PrepareComputePSO()
{
	const UINT& stateFlags = m_ComputeState.m_StateFlags;

	bool bNewPSO = false;
	bool bNewRS = false;

	if (stateFlags & EPSPB_PipelineState)
	{
		NCryDX12::CDevice* device = m_pDevice->GetDX12Device();

		DX12_PTR(NCryDX12::CRootSignature) newRootSignature;
		DX12_PTR(NCryDX12::CComputePSO) newPSO;

		NCryDX12::CRootSignature::SInitParams rsParams;
		NCryDX12::CComputePSO::SInitParams psoParams;

		m_ComputeState.InitRootSignatureInitParams(rsParams);
		m_ComputeState.InitD3D12Descriptor(psoParams, m_nodeMask);

		// Get or create root signature
		// Get or create PSO
		{
			device->GetRootSignatureCache().GetOrCreateRootSignature(rsParams, false, m_nodeMask, newRootSignature);
			psoParams.m_Desc.pRootSignature = newRootSignature->GetD3D12RootSignature();
			device->GetPSOCache().GetOrCreatePSO(psoParams, newPSO);
		}

		// Did something change?
		bNewPSO = m_bCmdListBegins[CMDQUEUE_COMPUTE] || (newPSO != m_pCurrentPSO) || !newPSO || !m_pCurrentPSO;
		bNewRS = m_bCmdListBegins[CMDQUEUE_COMPUTE] || (newRootSignature != m_pCurrentRootSignature) || !newRootSignature || !m_pCurrentRootSignature;
		if (bNewPSO || bNewRS)
		{
			m_pCurrentPSO = newPSO;
			m_pCurrentRootSignature = newRootSignature;

			if (!m_pCurrentPSO || !m_pCurrentRootSignature)
			{
				return false;
			}
		}
		else
		{
			// False positive, clear dirty flags
			m_ComputeState.m_StateFlags &= ~EPSPB_PipelineState;
		}
	}

	// check for overflow and submit early
	if (!m_bCmdListBegins[CMDQUEUE_COMPUTE] && (m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE)->IsFull(
	                                              /* 256, */ m_pCurrentRootSignature->GetResourceMappings().m_NumResources,
	                                              /*  16, */ m_pCurrentRootSignature->GetResourceMappings().m_NumDynamicSamplers,
	                                              /*   0, */ 0,
	                                              /*   0, */ 0
	                                              ) || ((DX12_SUBMISSION_MODE == DX12_SUBMISSION_PERPSO) && (stateFlags & EPSPB_PipelineState))))
	{
		m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, false);

#ifdef DX12_STATS
		m_NumCommandListOverflows++;
#endif // DX12_STATS

		bNewPSO = true;
		bNewRS = true;

		m_ComputeState.Invalidate();
	}

	if (stateFlags & EPSPB_PipelineState)
	{
		// assume the constant buffer is set on the compute shader always (don't need to search for the first occurrence)
		D3D12_GPU_VIRTUAL_ADDRESS vConstViews[8 /* CB_NUM */] = { 0 };

		// Assign new PSO and root signature and set resource binding dirty flags
		m_bCmdListBegins[CMDQUEUE_COMPUTE] = false;
		if (bNewPSO)
			m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE)->SetPSO(m_pCurrentPSO);
		if (bNewRS)
			m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE)->SetRootSignature(m_pCurrentRootSignature, false, vConstViews);

		// If a root signature is changed on a command list, all previous root signature bindings
		// become stale and all newly expected bindings must be set before Draw/Dispatch
		if (bNewRS)
			m_ComputeState.m_StateFlags |= EPSPB_InputResources;
	}

	return true;
}

void CCryDX12DeviceContext::PrepareGraphicsFF()
{
	const UINT& stateFlags = m_GraphicsState.m_StateFlags;

	if (stateFlags & EPSPB_IndexBuffer & (m_GraphicsState.InputAssembler.IndexBuffer ? ~0 : 0))
	{
		DX12_ASSERT(m_GraphicsState.InputAssembler.IndexBuffer, "IndexBuffer is required by the Draw but has not been set!");
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->BindAndSetIndexBufferView(m_GraphicsState.InputAssembler.IndexBuffer->GetDX12View(), m_GraphicsState.InputAssembler.IndexBufferFormat.Get(), m_GraphicsState.InputAssembler.IndexBufferOffset.Get());
	}

	if (stateFlags & EPSPB_PrimitiveTopology)
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(m_GraphicsState.InputAssembler.PrimitiveTopology.Get()));
	}

	if (stateFlags & EPSPB_VertexBuffers)
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearVertexBufferHeap(m_GraphicsState.InputAssembler.NumVertexBuffers.Get());

		for (UINT i = 0, S = m_GraphicsState.InputAssembler.NumVertexBuffers.Get(); i < S; ++i)
		{
			CCryDX12Buffer* buffer = m_GraphicsState.InputAssembler.VertexBuffers.Get(i);
			if (buffer)
			{
				TRange<UINT> bindRange = TRange<UINT>(m_GraphicsState.InputAssembler.Offsets.Get(i), m_GraphicsState.InputAssembler.Offsets.Get(i));
				UINT bindStride = m_GraphicsState.InputAssembler.Strides.Get(i);
				DX12_ASSERT(buffer, "VertexBuffer is required by the PSO but has not been set!");
				//				DX12_ASSERT(bindStride == m_GraphicsState.InputAssembler.InputLayout->GetInputSize(i), "Vertex buffer to be bound is incompatible with input layout bound!");
				m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->BindVertexBufferView(buffer->GetDX12View(), i, bindRange, bindStride);
			}
		}

		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetVertexBufferHeap(m_GraphicsState.InputAssembler.NumVertexBuffers.Get());
	}

	if (stateFlags & EPSPB_Viewports)
	{
		UINT numScissors = m_GraphicsState.Rasterizer.ScissorEnabled.Get() ? m_GraphicsState.Rasterizer.NumScissors.Get() : 0;
		if (m_GraphicsState.Rasterizer.NumViewports.Get() >= numScissors)
		{
			D3D12_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
			D3D12_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

			for (UINT i = 0, S = m_GraphicsState.Rasterizer.NumViewports.Get(); i < S; ++i)
			{
				const D3D11_VIEWPORT& v = m_GraphicsState.Rasterizer.Viewports.Get(i);
				viewports[i].TopLeftX = v.TopLeftX;
				viewports[i].TopLeftY = v.TopLeftY;
				viewports[i].Width = v.Width;
				viewports[i].Height = v.Height;
				viewports[i].MinDepth = v.MinDepth;
				viewports[i].MaxDepth = v.MaxDepth;

				if (i < numScissors)
				{
					const D3D11_RECT& s = m_GraphicsState.Rasterizer.Scissors.Get(i);

					scissors[i].bottom = s.bottom;
					scissors[i].left = s.left;
					scissors[i].right = s.right;
					scissors[i].top = s.top;
				}
				else
				{
					scissors[i].top = static_cast<LONG>(v.TopLeftY);
					scissors[i].left = static_cast<LONG>(v.TopLeftX);
					scissors[i].right = static_cast<LONG>(v.TopLeftX + v.Width);
					scissors[i].bottom = static_cast<LONG>(v.TopLeftY + v.Height);
				}
			}

			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetViewports(m_GraphicsState.Rasterizer.NumViewports.Get(), viewports);
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetScissorRects(m_GraphicsState.Rasterizer.NumViewports.Get(), scissors);
		}
		else
		{
			// This should not happen, ever!
			DX12_NOT_IMPLEMENTED;
		}
	}

	if (stateFlags & EPSPB_StencilRef)
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetStencilRef(m_GraphicsState.OutputMerger.StencilRef.Get());
	}

	if (stateFlags & EPSPB_OutputResources)
	{
		BindOutputViews();
	}
}

void CCryDX12DeviceContext::PrepareIO(bool bGfx)
{
	const UINT stateFlags = bGfx ? m_GraphicsState.m_StateFlags : m_ComputeState.m_StateFlags;

	if (stateFlags & EPSPB_InputResources)
	{
		BindResources(bGfx);
	}
}

bool CCryDX12DeviceContext::PrepareGraphicsState()
{
	if (!m_GraphicsState.AreShadersBound())
	{
		return false;
	}

	if (!PrepareGraphicsPSO())
		return false;

	PrepareGraphicsFF();
	PrepareIO(true);

	// We can safely clear the pipeline state flags there (command list is up to date at this point)
	m_GraphicsState.m_StateFlagsEncountered |= m_GraphicsState.m_StateFlags;
	m_GraphicsState.m_StateFlags = 0;

	return true;
}

bool CCryDX12DeviceContext::PrepareComputeState()
{
	if (!m_ComputeState.AreShadersBound())
	{
		return false;
	}

	if (!PrepareComputePSO())
		return false;

	PrepareIO(false);

	// We can safely clear the pipeline state flags there (command list is up to date at this point)
	m_ComputeState.m_StateFlagsEncountered |= m_ComputeState.m_StateFlags;
	m_ComputeState.m_StateFlags = 0;

	return true;
}

void CCryDX12DeviceContext::BindResources(bool bGfx)
{
	ID3D12Device* pDevice12 = GetD3D12Device();
	const SCryDX11PipelineState& rState = bGfx ? m_GraphicsState : m_ComputeState;
	NCryDX12::CCommandList* pCmdList = bGfx ? m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS) : m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE);
	const NCryDX12::SResourceMappings& mappings = m_pCurrentRootSignature->GetResourceMappings();

	const UINT stateFlags = rState.m_StateFlags;
	DX12_ASSERT(stateFlags & (BIT(EPSP_ConstantBuffers) | BIT(EPSP_Resources) | BIT(EPSP_Samplers)), "Redundant BindResources() called without state-changes");

	UINT uResourceDTOffset = 0;
	UINT uSamplerDTOffset = 0;

	// Bind resources
	if (stateFlags & (EPSPB_ConstantBuffers | EPSPB_Resources))
	{
		for (UINT r = 0, n = mappings.m_NumRESs; r < n; ++r)
		{
			auto& resInput = mappings.m_RESs[r];

			const NCryDX12::EShaderStage i = resInput.ShaderStage;
			if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
			{
				CCryDX12Buffer* buffer = rState.Stages[i].ConstantBufferViews.Get(resInput.ShaderSlot);
				auto bindRange = rState.Stages[i].ConstBufferBindRange.Get(resInput.ShaderSlot);
				DX12_ASSERT(buffer, "ConstantBuffer is required by the PSO but has not been set!");
				DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "ConstantBuffer offset has shifted: resource mapping invalid!");
				if (!buffer) continue;
				pCmdList->BindResourceView(buffer->GetDX12View(), uResourceDTOffset++, bindRange);
			}
			else if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
			{
				CCryDX12ShaderResourceView* resource = rState.Stages[i].ShaderResourceViews.Get(resInput.ShaderSlot);
				DX12_ASSERT(resource, "ShaderResourceView is required by the PSO but has not been set!");
				DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "ShaderResourceView offset has shifted: resource mapping invalid!");
				if (!resource) continue;
				pCmdList->BindResourceView(resource->GetDX12View(), uResourceDTOffset++, TRange<UINT>());
			}
			else if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
			{
				CCryDX12UnorderedAccessView* resource = rState.Stages[i].UnorderedAccessViews.Get(resInput.ShaderSlot);
				DX12_ASSERT(resource, "UnorderedAccessView is required by the PSO but has not been set!");
				DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "UnorderedAccessView offset has shifted: resource mapping invalid!");
				if (!resource) continue;
				pCmdList->BindResourceView(resource->GetDX12View(), uResourceDTOffset++, TRange<UINT>());
			}
		}
	}

	// Bind samplers
	if (stateFlags & EPSPB_Samplers)
	{
		for (UINT s = 0, n = mappings.m_NumSMPs; s < n; ++s)
		{
			auto& smpInput = mappings.m_SMPs[s];

			const NCryDX12::EShaderStage i = smpInput.ShaderStage;
			DX12_ASSERT(smpInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "");
			{
				CCryDX12SamplerState* sampler = rState.Stages[i].SamplerState.Get(smpInput.ShaderSlot);
				DX12_ASSERT(sampler, "Sampler is required by the PSO but has not been set!");
				DX12_ASSERT(uSamplerDTOffset == smpInput.DescriptorOffset, "Sampler offset has shifted: resource mapping invalid!");
				if (!sampler) continue;
				pCmdList->BindSamplerState(sampler->GetDX12SamplerState(), uSamplerDTOffset++);
			}
		}
	}

	if (stateFlags & (BIT(EPSP_ConstantBuffers) | BIT(EPSP_Resources)))
	{
		if (bGfx)
		{
			pCmdList->SetGraphicsDescriptorTables(m_pCurrentRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			pCmdList->IncrementInputCursors(mappings.m_NumResources, 0);
		}
		else
		{
			pCmdList->SetComputeDescriptorTables(m_pCurrentRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			pCmdList->IncrementInputCursors(mappings.m_NumResources, 0);
		}
	}

	if (stateFlags & (BIT(EPSP_Samplers)))
	{
		if (bGfx)
		{
			pCmdList->SetGraphicsDescriptorTables(m_pCurrentRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			pCmdList->IncrementInputCursors(0, mappings.m_NumDynamicSamplers);
		}
		else
		{
			pCmdList->SetComputeDescriptorTables(m_pCurrentRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			pCmdList->IncrementInputCursors(0, mappings.m_NumDynamicSamplers);
		}
	}
}

void CCryDX12DeviceContext::BindOutputViews()
{
	const SCryDX11PipelineState& rState = m_GraphicsState;
	const NCryDX12::CCommandList* pCmdList = m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS);

	const NCryDX12::CView* dsv = NULL;
	const NCryDX12::CView* rtv[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	size_t numRTVs = 0;

	// Get current depth stencil views
	{
		CCryDX12DepthStencilView* view = rState.OutputMerger.DepthStencilView;
		if (view)
		{
			dsv = &view->GetDX12View();
		}
	}

	// Get current render target views
	for (UINT i = 0, S = rState.OutputMerger.NumRenderTargets.Get(); i < S; ++i)
	{
		CCryDX12RenderTargetView* view = rState.OutputMerger.RenderTargetViews.Get(i);
		if (view)
		{
			view->GetDX12Resource().VerifyBackBuffer();
			rtv[numRTVs++] = &view->GetDX12View();
		}
	}

	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->BindAndSetOutputViews(numRTVs, rtv, dsv);
}

namespace NCryDX12 {
const char* StateToString(D3D12_RESOURCE_STATES state);
}

void CCryDX12DeviceContext::DebugPrintResources(bool bGfx)
{
	ID3D12Device* pDevice12 = GetD3D12Device();
	const SCryDX11PipelineState& rState = bGfx ? m_GraphicsState : m_ComputeState;
	const NCryDX12::CCommandList* pCmdList = bGfx ? m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS) : m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE);
	const NCryDX12::SResourceMappings& mappings = m_pCurrentRootSignature->GetResourceMappings();

	DX12_LOG(g_nPrintDX12, "Resource Heap Descriptor Tables:");

	UINT stateFlags = rState.m_StateFlags;
	UINT uResourceDTOffset = 0;
	UINT uSamplerDTOffset = 0;

	// Bind constant buffers
	for (UINT r = 0, n = mappings.m_NumRESs; r < n; ++r)
	{
		auto& resInput = mappings.m_RESs[r];

		const NCryDX12::EShaderStage i = resInput.ShaderStage;
		if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
		{
			CCryDX12Buffer* buffer = rState.Stages[i].ConstantBufferViews.Get(resInput.ShaderSlot);
			auto bindRange = rState.Stages[i].ConstBufferBindRange.Get(resInput.ShaderSlot);
			DX12_ASSERT(buffer, "ConstantBuffer is required by the PSO but has not been set!");
			DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "ConstantBuffer offset has shifted: resource mapping invalid!");

			DX12_LOG(g_nPrintDX12, " %s: C %2d -> %2d %s [%s]",
			         i == NCryDX12::ESS_Compute ? "Every    shader stage" :
			         i == NCryDX12::ESS_Vertex ? "Vertex   shader stage" :
			         i == NCryDX12::ESS_Hull ? "Hull     shader stage" :
			         i == NCryDX12::ESS_Domain ? "Domain   shader stage" :
			         i == NCryDX12::ESS_Geometry ? "Geometry shader stage" :
			         i == NCryDX12::ESS_Pixel ? "Pixel    shader stage" : "Unknown  shader stage",
			         resInput.ShaderSlot,
			         uResourceDTOffset++,
			         buffer ? buffer->GetName().c_str() : "nullptr",
			         buffer ? NCryDX12::StateToString(buffer->GetDX12Resource().GetState()) : "-");
		}
		else if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
		{
			CCryDX12ShaderResourceView* resource = rState.Stages[i].ShaderResourceViews.Get(resInput.ShaderSlot);
			DX12_ASSERT(resource, "ShaderResourceView is required by the PSO but has not been set!");
			DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "ShaderResourceView offset has shifted: resource mapping invalid!");

			DX12_LOG(g_nPrintDX12, " %s: T %2d -> %2d %s [%s]",
			         i == NCryDX12::ESS_Compute ? "Every    shader stage" :
			         i == NCryDX12::ESS_Vertex ? "Vertex   shader stage" :
			         i == NCryDX12::ESS_Hull ? "Hull     shader stage" :
			         i == NCryDX12::ESS_Domain ? "Domain   shader stage" :
			         i == NCryDX12::ESS_Geometry ? "Geometry shader stage" :
			         i == NCryDX12::ESS_Pixel ? "Pixel    shader stage" : "Unknown  shader stage",
			         resInput.ShaderSlot,
			         uResourceDTOffset++,
			         resource ? resource->GetResourceName().c_str() : "nullptr",
			         resource ? NCryDX12::StateToString(resource->GetDX12Resource().GetState()) : "-");
		}
		else if (resInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
		{
			CCryDX12UnorderedAccessView* resource = rState.Stages[i].UnorderedAccessViews.Get(resInput.ShaderSlot);
			DX12_ASSERT(resource, "UnorderedAccessView is required by the PSO but has not been set!");
			DX12_ASSERT(uResourceDTOffset == resInput.DescriptorOffset, "UnorderedAccessView offset has shifted: resource mapping invalid!");

			DX12_LOG(g_nPrintDX12, " %s: U %2d -> %2d %s [%s]",
			         i == NCryDX12::ESS_Compute ? "Every    shader stage" :
			         i == NCryDX12::ESS_Vertex ? "Vertex   shader stage" :
			         i == NCryDX12::ESS_Hull ? "Hull     shader stage" :
			         i == NCryDX12::ESS_Domain ? "Domain   shader stage" :
			         i == NCryDX12::ESS_Geometry ? "Geometry shader stage" :
			         i == NCryDX12::ESS_Pixel ? "Pixel    shader stage" : "Unknown  shader stage",
			         resInput.ShaderSlot,
			         uResourceDTOffset++,
			         resource ? resource->GetResourceName().c_str() : "nullptr",
			         resource ? NCryDX12::StateToString(resource->GetDX12Resource().GetState()) : "-");
		}
	}

	// Bind samplers
	for (UINT s = 0, n = mappings.m_NumSMPs; s < n; ++s)
	{
		auto& smpInput = mappings.m_SMPs[s];

		const NCryDX12::EShaderStage i = smpInput.ShaderStage;
		DX12_ASSERT(smpInput.ViewType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, "");
		{
			CCryDX12SamplerState* sampler = rState.Stages[i].SamplerState.Get(smpInput.ShaderSlot);
			DX12_ASSERT(sampler, "Sampler is required by the PSO but has not been set!");
			DX12_ASSERT(uSamplerDTOffset == smpInput.DescriptorOffset, "Sampler offset has shifted: resource mapping invalid!");

			DX12_LOG(g_nPrintDX12, " %s: S %2d -> %2d",
			         i == NCryDX12::ESS_Compute ? "Every    shader stage" :
			         i == NCryDX12::ESS_Vertex ? "Vertex   shader stage" :
			         i == NCryDX12::ESS_Hull ? "Hull     shader stage" :
			         i == NCryDX12::ESS_Domain ? "Domain   shader stage" :
			         i == NCryDX12::ESS_Geometry ? "Geometry shader stage" :
			         i == NCryDX12::ESS_Pixel ? "Pixel    shader stage" : "Unknown  shader stage",
			         smpInput.ShaderSlot,
			         uSamplerDTOffset++);
		}
	}
}

void CCryDX12DeviceContext::CeaseCommandQueueCallback(int nPoolId)
{
	if (nPoolId == CMDQUEUE_GRAPHICS)
		m_GraphicsState.m_StateFlagsEncountered |= m_GraphicsState.m_StateFlags;
	//	m_GraphicsState.m_StateFlags = 0;
	if (nPoolId == CMDQUEUE_COMPUTE)
		m_ComputeState.m_StateFlagsEncountered |= m_ComputeState.m_StateFlags;
	//	m_ComputeState.m_StateFlags = 0;

	// Synchronize dx12 command list's internal state with CDeviceCommandList's cached state after command list is closed.
	GetDeviceObjectFactory().GetCoreCommandList().Reset();
}

void CCryDX12DeviceContext::ResumeCommandQueueCallback(int nPoolId)
{
	m_bCmdListBegins[nPoolId] = true;

	if (nPoolId == CMDQUEUE_GRAPHICS)
		m_GraphicsState.m_StateFlags = m_GraphicsState.m_StateFlagsEncountered;
	//	m_GraphicsState.m_StateFlagsEncountered = 0;
	if (nPoolId == CMDQUEUE_COMPUTE)
		m_ComputeState.m_StateFlags = m_ComputeState.m_StateFlagsEncountered;
	//	m_ComputeState.m_StateFlagsEncountered = 0;
}

void CCryDX12DeviceContext::Finish(NCryDX12::CSwapChain* pDX12SwapChain)
{
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->PresentSwapChain(pDX12SwapChain);

	Finish();
}

void CCryDX12DeviceContext::Finish()
{
	m_Scheduler.EndOfFrame(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

#ifdef DX12_STATS
	m_NumMapDiscardSkips = 0;
	m_NumMapDiscards = 0;
	m_NumCopyDiscardSkips = 0;
	m_NumCopyDiscards = 0;
#endif // DX12_STATS
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT CCryDX12DeviceContext::TimestampIndex(NCryDX12::CCommandList* pCmdList)
{
	UINT index = m_TimestampIndex;
	m_TimestampIndex = (m_TimestampIndex + 1) % m_TimestampHeap.GetCapacity();
	return index;
}

void CCryDX12DeviceContext::InsertTimestamp(NCryDX12::CCommandList* pCmdList, UINT index)
{
	pCmdList->EndQuery(m_TimestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, index);
}

void CCryDX12DeviceContext::ResolveTimestamps(NCryDX12::CCommandList* pCmdList)
{
	if (m_TimestampMemory)
	{
		const D3D12_RANGE sNoWrite = { 0, 0 };
		m_TimestampDownloadBuffer->Unmap(0, &sNoWrite);
		m_TimestampMemory = nullptr;
	}

	pCmdList->ResolveQueryData(m_TimestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, m_TimestampHeap.GetCapacity(), m_TimestampDownloadBuffer, 0);
}

void CCryDX12DeviceContext::QueryTimestamp(NCryDX12::CCommandList* pCmdList, UINT index, void* mem)
{
	if (mem)
	{
		if (!m_TimestampMemory)
		{
			// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
			const D3D12_RANGE sFullRead = { 0, sizeof(UINT64) * m_TimestampHeap.GetCapacity() };
			m_TimestampDownloadBuffer->Map(0, &sFullRead, &m_TimestampMemory);
		}

		memcpy(mem, (char*)m_TimestampMemory + index * sizeof(UINT64), sizeof(UINT64));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT CCryDX12DeviceContext::OcclusionIndex(NCryDX12::CCommandList* pCmdList, bool counter)
{
	UINT index = m_OcclusionIndex;
	m_OcclusionIndex = (m_OcclusionIndex + 1) % m_OcclusionHeap.GetCapacity();
	return index;
}

void CCryDX12DeviceContext::InsertOcclusionStart(NCryDX12::CCommandList* pCmdList, UINT index, bool counter)
{
	pCmdList->BeginQuery(m_OcclusionHeap, counter ? D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION, index);
}

void CCryDX12DeviceContext::InsertOcclusionStop(NCryDX12::CCommandList* pCmdList, UINT index, bool counter)
{
	// TODO: investigate! ResolveQueryData() should not needed to be called. Bug in DX12 API?
	pCmdList->EndQuery(m_OcclusionHeap, counter ? D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION, index);
	pCmdList->ResolveQueryData(m_OcclusionHeap, counter ? D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION, index, 1, m_OcclusionDownloadBuffer, index * 8);

	m_OcclusionMapValid = false;
}

void CCryDX12DeviceContext::ResolveOcclusion(NCryDX12::CCommandList* pCmdList, UINT index, void* mem)
{
	if (mem)
	{
		if (!m_OcclusionMapValid)
		{
			pCmdList->ResolveQueryData(m_OcclusionHeap, D3D12_QUERY_TYPE_OCCLUSION, 0, m_OcclusionHeap.GetCapacity(), m_OcclusionDownloadBuffer, 0);
			m_OcclusionMapValid = true;
		}

		if (!m_OcclusionMemory)
		{
			// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
			const D3D12_RANGE sFullRead = { 0, sizeof(UINT64) * m_OcclusionHeap.GetCapacity() };
			m_OcclusionDownloadBuffer->Map(0, &sFullRead, &m_OcclusionMemory);
		}

		memcpy(mem, (char*)m_OcclusionMemory + index * sizeof(UINT64), sizeof(UINT64));

		if (m_OcclusionMemory)
		{
			const D3D12_RANGE sNoWrite = { 0, 0 };
			m_OcclusionDownloadBuffer->Unmap(0, &sNoWrite);
			m_OcclusionMemory = nullptr;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region /* ID3D11DeviceChild implementation */

void STDMETHODCALLTYPE CCryDX12DeviceContext::GetDevice(
  _Out_ ID3D11Device** ppDevice)
{
	DX12_FUNC_LOG

}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::GetPrivateData(
  _In_ REFGUID guid,
  _Inout_ UINT* pDataSize,
  _Out_writes_bytes_opt_(*pDataSize)  void* pData)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::SetPrivateData(
  _In_ REFGUID guid,
  _In_ UINT DataSize,
  _In_reads_bytes_opt_(DataSize)  const void* pData)
{
	DX12_FUNC_LOG
	return -1;
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::SetPrivateDataInterface(
  _In_ REFGUID guid,
  _In_opt_ const IUnknown* pData)
{
	DX12_FUNC_LOG
	return -1;
}

#pragma endregion

#pragma region /* ID3D11DeviceContext implementation */

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Pixel].ShaderResourceViews.Set(i, srv) && srv)
		{
			NCryDX12::CResource& resource = srv->GetDX12Resource();
			{
				resource.BeginTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), srv->GetDX12View(), PS_STATE);
			}
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSSetShader(
  _In_opt_ ID3D11PixelShader* pPixelShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_GraphicsState.Stages[NCryDX12::ESS_Pixel].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pPixelShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_GraphicsState.Stages[NCryDX12::ESS_Pixel].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSSetShader(
  _In_opt_ ID3D11VertexShader* pVertexShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_GraphicsState.Stages[NCryDX12::ESS_Vertex].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pVertexShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawIndexed(
  _In_ UINT IndexCount,
  _In_ UINT StartIndexLocation,
  _In_ INT BaseVertexLocation)
{
	DX12_FUNC_LOG

	if (PrepareGraphicsState())
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
		m_GraphicsState.DebugPrint();
		if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Draw(
  _In_ UINT VertexCount,
  _In_ UINT StartVertexLocation)
{
	DX12_FUNC_LOG

	if (PrepareGraphicsState())
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->DrawInstanced(VertexCount, 1, StartVertexLocation, 0);
		m_GraphicsState.DebugPrint();
		if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	}
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::Map(
  _In_ ID3D11Resource* pResource,
  _In_ UINT Subresource,
  _In_ D3D11_MAP MapType,
  _In_ UINT MapFlags,
  _Out_ D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	static D3D12_RANGE sRg[] =
	{
		{ 0, 0 }, // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin
		{ 0, 0 }  // It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin.
	};

	static D3D12_RANGE* pRanges[] =
	{
		nullptr, // 0
		nullptr, // D3D11_MAP_READ = 1,
		&sRg[0], // D3D11_MAP_WRITE = 2,
		nullptr, // D3D11_MAP_READ_WRITE = 3,
		&sRg[0], // D3D11_MAP_WRITE_DISCARD = 4,
		&sRg[0], // D3D11_MAP_WRITE_NO_OVERWRITE = 5
	};

	return Map(pResource, Subresource, reinterpret_cast<SIZE_T*>(pRanges[MapType]), MapType, MapFlags, pMappedResource);
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::Map(
  _In_ ID3D11Resource* pResource,
  _In_ UINT Subresource,
  _In_ SIZE_T* BeginEnd,
  _In_ D3D11_MAP MapType,
  _In_ UINT MapFlags,
  _Out_ D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	DX12_FUNC_LOG
	DX12_LOG(g_nPrintDX12, "  Mapping resource: %p (%d)", pResource, Subresource);

	ZeroMemory(pMappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource();

	switch (MapType & (DX12_MAP_DISCARD_MARKER - 1))
	{
	case D3D11_MAP_READ:
		// Ensure the command-list using the resource is executed
		{
			CryCriticalSectionNonRecursive safe;
			m_Scheduler.SubmitAllCommands(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, resource.GetFenceValues(CMDTYPE_WRITE));
		}

		// Block the CPU-thread until the resource is safe to map
		resource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS), CMDTYPE_WRITE);
		break;
	case D3D11_MAP_WRITE:
	case D3D11_MAP_READ_WRITE:
		// Ensure the command-list using the resource is executed
		{
			CryCriticalSectionNonRecursive safe;
			m_Scheduler.SubmitAllCommands(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, resource.GetFenceValues(CMDTYPE_ANY));
		}

		// Block the CPU-thread until the resource is safe to map
		resource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS), CMDTYPE_ANY);
		break;
	case D3D11_MAP_WRITE_DISCARD:
		DX12_LOG(g_nPrintDX12, "Using D3D11_MAP_WRITE_DISCARD on old ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pResource));

#ifdef DX12_STATS
		m_NumMapDiscardSkips += !resource.IsUsed(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS));
		m_NumMapDiscards++;
#endif // DX12_STATS

		// If the resource is not currently used, we do not need to discard the memory
		if (dx12Resource->SubstituteUsed())
		{
			// ConstantBuffer/ShaderResource/UnorderedAccess marker
			if (MapType & DX12_MAP_DISCARD_MARKER)
			{
				DX12_ASSERT(false, "MAP_DISCARD does not work under DX12 when there is a view on the discardable buffer! Please rewrite the algorithm to use NO_OVERWRITE");
				abort();
			}
		}

		DX12_LOG(g_nPrintDX12, "New ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pResource));
		break;
	case D3D11_MAP_WRITE_NO_OVERWRITE:
		break;
	default:
		break;
	}

	DX12_ASSERT(!D3D12IsLayoutOpaque(resource.GetDesc().Layout), "Opaque layouts are unmappable until 12.2!");
	HRESULT hr = dx12Resource->GetD3D12Resource()->Map(Subresource, reinterpret_cast<D3D12_RANGE*>(BeginEnd), &(pMappedResource->pData));

	if (S_OK != hr)
	{
		DX12_ASSERT(0, "Could not map resource!");
		return hr;
	}

	return S_OK;
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Unmap(
  _In_ ID3D11Resource* pResource,
  _In_ UINT Subresource)
{
	// NOTE: Don't know MapType, can't optimize writeRange!
	Unmap(pResource, Subresource, nullptr);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Unmap(
  _In_ ID3D11Resource* pResource,
  _In_ UINT Subresource,
  _In_ SIZE_T* BeginEnd)
{
	DX12_FUNC_LOG
	DX12_LOG(g_nPrintDX12, "Unmapping resource: %p (%d)", pResource, Subresource);

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource();

	DX12_ASSERT(!D3D12IsLayoutOpaque(resource.GetDesc().Layout), "Opaque layouts are unmappable until 12.2!");
	dx12Resource->GetD3D12Resource()->Unmap(Subresource, reinterpret_cast<D3D12_RANGE*>(BeginEnd));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IASetInputLayout(
  _In_opt_ ID3D11InputLayout* pInputLayout)
{
	DX12_FUNC_LOG

	m_GraphicsState.InputAssembler.InputLayout.Set(reinterpret_cast<CCryDX12InputLayout*>(pInputLayout));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IASetVertexBuffers(
  _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppVertexBuffers,
  _In_reads_opt_(NumBuffers)  const UINT* pStrides,
  _In_reads_opt_(NumBuffers)  const UINT* pOffsets)
{
	DX12_FUNC_LOG

	for (UINT i = StartSlot, S = StartSlot + NumBuffers; i < S; ++i, ++ppVertexBuffers, ++pStrides, ++pOffsets)
	{
		CCryDX12Buffer* vb = reinterpret_cast<CCryDX12Buffer*>(*ppVertexBuffers);

		m_GraphicsState.InputAssembler.Strides.Set(i, *pStrides);
		m_GraphicsState.InputAssembler.Offsets.Set(i, *pOffsets);
		if (m_GraphicsState.InputAssembler.VertexBuffers.Set(i, vb) && vb)
		{
			vb->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}
	}

	UINT numVertexBuffers = 0;
	for (UINT i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
	{
		if (m_GraphicsState.InputAssembler.VertexBuffers.Get(i))
		{
			numVertexBuffers = i + 1;
		}
	}

	m_GraphicsState.InputAssembler.NumVertexBuffers.Set(numVertexBuffers);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IASetIndexBuffer(
  _In_opt_ ID3D11Buffer* pIndexBuffer,
  _In_ DXGI_FORMAT Format,
  _In_ UINT Offset)
{
	DX12_FUNC_LOG

	CCryDX12Buffer* ib = reinterpret_cast<CCryDX12Buffer*>(pIndexBuffer);

	m_GraphicsState.InputAssembler.IndexBufferFormat.Set(Format);
	m_GraphicsState.InputAssembler.IndexBufferOffset.Set(Offset);
	if (m_GraphicsState.InputAssembler.IndexBuffer.Set(ib) && ib)
	{
		ib->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawIndexedInstanced(
  _In_ UINT IndexCountPerInstance,
  _In_ UINT InstanceCount,
  _In_ UINT StartIndexLocation,
  _In_ INT BaseVertexLocation,
  _In_ UINT StartInstanceLocation)
{
	DX12_FUNC_LOG

	if (PrepareGraphicsState())
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
		m_GraphicsState.DebugPrint();
		if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawInstanced(
  _In_ UINT VertexCountPerInstance,
  _In_ UINT InstanceCount,
  _In_ UINT StartVertexLocation,
  _In_ UINT StartInstanceLocation)
{
	DX12_FUNC_LOG

	if (PrepareGraphicsState())
	{
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
		m_GraphicsState.DebugPrint();
		if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSSetShader(
  _In_opt_ ID3D11GeometryShader* pShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_GraphicsState.Stages[NCryDX12::ESS_Geometry].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IASetPrimitiveTopology(
  _In_ D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	DX12_FUNC_LOG
	m_GraphicsState.InputAssembler.PrimitiveTopology.Set(Topology);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Vertex].ShaderResourceViews.Set(i, srv) && srv)
		{
			srv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_GraphicsState.Stages[NCryDX12::ESS_Vertex].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Begin(
  _In_ ID3D11Asynchronous* pAsync)
{
	DX12_FUNC_LOG
	auto pQuery = reinterpret_cast<CCryDX12Query*>(pAsync);

	D3D11_QUERY_DESC desc;
	pQuery->GetDesc(&desc);

	if (desc.Query == D3D11_QUERY_EVENT)
	{
		return;
	}
	else if (desc.Query == D3D11_QUERY_OCCLUSION || desc.Query == D3D11_QUERY_OCCLUSION_PREDICATE)
	{
#ifdef DX12_SUBMISSION_LOWLATENCY
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, m_CmdFenceSet.GetCurrentValue(CMDQUEUE_GRAPHICS));
#endif

		auto pOcclusionQuery = reinterpret_cast<CCryDX12ResourceQuery*>(pAsync);
		pOcclusionQuery->SetFenceValue(InsertFence());
		pOcclusionQuery->SetQueryIndex(OcclusionIndex(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), desc.Query == D3D11_QUERY_OCCLUSION));

		InsertOcclusionStart(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), pOcclusionQuery->GetQueryIndex(), desc.Query == D3D11_QUERY_OCCLUSION);
	}
	else
	{
		__debugbreak();
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::End(
  _In_ ID3D11Asynchronous* pAsync)
{
	DX12_FUNC_LOG
	auto pQuery = reinterpret_cast<CCryDX12Query*>(pAsync);

	D3D11_QUERY_DESC desc;
	pQuery->GetDesc(&desc);

	if (desc.Query == D3D11_QUERY_EVENT)
	{
#ifdef DX12_SUBMISSION_LOWLATENCY
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, m_CmdFenceSet.GetCurrentValue(CMDQUEUE_GRAPHICS));
#endif

		// Record fence of commands prior to this point
		auto pEventQuery = reinterpret_cast<CCryDX12EventQuery*>(pAsync);
		pEventQuery->SetFenceValue(InsertFence());
	}
	else if (desc.Query == D3D11_QUERY_OCCLUSION)
	{
		auto pOcclusionQuery = reinterpret_cast<CCryDX12ResourceQuery*>(pAsync);
		pOcclusionQuery->SetFenceValue(InsertFence());
		pOcclusionQuery->SetQueryResource(m_OcclusionDownloadBuffer);

		InsertOcclusionStop(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), pOcclusionQuery->GetQueryIndex(), desc.Query == D3D11_QUERY_OCCLUSION);

#ifdef DX12_SUBMISSION_LOWLATENCY
		// TODO: group time-stamp queries
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
#endif
	}
	else // || desc.Query == D3D11_QUERY_OCCLUSION_PREDICATE)
	{
		__debugbreak();
	}
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::GetData(
  _In_ ID3D11Asynchronous* pAsync,
  _Out_writes_bytes_opt_(DataSize)  void* pData,
  _In_ UINT DataSize,
  _In_ UINT GetDataFlags)
{
	DX12_FUNC_LOG
	auto pQuery = reinterpret_cast<CCryDX12Query*>(pAsync);

	D3D11_QUERY_DESC desc;
	pQuery->GetDesc(&desc);

	if (desc.Query == D3D11_QUERY_EVENT)
	{
		DX12_ASSERT(DataSize >= sizeof(BOOL));

		auto pEventQuery = reinterpret_cast<CCryDX12EventQuery*>(pAsync);
		bool bComplete = TestForFence(pEventQuery->GetFenceValue()) == S_OK;

		if (!bComplete && !(GetDataFlags & D3D11_ASYNC_GETDATA_DONOTFLUSH))
		{
			// Ensure the command-list issuing the query is executed
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, pEventQuery->GetFenceValue());
			FlushToFence(pEventQuery->GetFenceValue());
		}

		return (*reinterpret_cast<BOOL*>(pData) = bComplete) ? S_OK : S_FALSE;
	}
	else if (desc.Query == D3D11_QUERY_OCCLUSION)
	{
		DX12_ASSERT(DataSize >= sizeof(UINT64));

		auto pOcclusionQuery = reinterpret_cast<CCryDX12ResourceQuery*>(pAsync);
		bool bComplete = TestForFence(pOcclusionQuery->GetFenceValue()) == S_OK;

		if (!bComplete && !(GetDataFlags & D3D11_ASYNC_GETDATA_DONOTFLUSH))
		{
			// Ensure the command-list issuing the query is executed
			m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, pOcclusionQuery->GetFenceValue());
			FlushToFence(pOcclusionQuery->GetFenceValue());
		}

		if (bComplete)
		{
			ResolveOcclusion(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), pOcclusionQuery->GetQueryIndex(), pData);
		}

		return (bComplete) ? S_OK : S_FALSE;
	}
	else // || desc.Query == D3D11_QUERY_OCCLUSION_PREDICATE
	{
		__debugbreak();
	}

	return S_OK;
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::SetPredication(
  _In_opt_ ID3D11Predicate* pPredicate,
  _In_ BOOL PredicateValue)
{
	DX12_FUNC_LOG
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Geometry].ShaderResourceViews.Set(i, srv) && srv)
		{
			srv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_GraphicsState.Stages[NCryDX12::ESS_Geometry].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMSetRenderTargets(
  _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11RenderTargetView* const* ppRenderTargetViews,
  _In_opt_ ID3D11DepthStencilView* pDepthStencilView)
{
	DX12_FUNC_LOG

	m_GraphicsState.OutputMerger.NumRenderTargets.Set(NumViews);
	for (UINT i = 0; i < NumViews; ++i)
	{
		CCryDX12RenderTargetView* rtv = reinterpret_cast<CCryDX12RenderTargetView*>(ppRenderTargetViews[i]);
		if (m_GraphicsState.OutputMerger.RenderTargetViews.Set(i, rtv))
		{
			if (!rtv)
			{
				m_GraphicsState.OutputMerger.RTVFormats.Set(i, DXGI_FORMAT_UNKNOWN);
				continue;
			}

			DX12_LOG(g_nPrintDX12, "Setting render target: %p (%d)", rtv, i);

			// TODO: this might not be the earliest moment when it is known that the resource is used as render-target
			D3D11_RENDER_TARGET_VIEW_DESC desc;
			rtv->GetDesc(&desc);
			rtv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));

			m_GraphicsState.OutputMerger.RTVFormats.Set(i, desc.Format);
		}
	}

	for (UINT i = NumViews; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		m_GraphicsState.OutputMerger.RenderTargetViews.Set(i, nullptr);
		m_GraphicsState.OutputMerger.RTVFormats.Set(i, DXGI_FORMAT_UNKNOWN);
	}

	{
		CCryDX12DepthStencilView* dsv = reinterpret_cast<CCryDX12DepthStencilView*>(pDepthStencilView);
		if (m_GraphicsState.OutputMerger.DepthStencilView.Set(dsv))
		{
			if (!dsv)
			{
				m_GraphicsState.OutputMerger.DSVFormat.Set(DXGI_FORMAT_UNKNOWN);
				return;
			}

			DX12_LOG(g_nPrintDX12, "Setting depth stencil view: %p", dsv);

			// TODO: this might not be the earliest moment when it is known that the resource is used as depth-stencil
			D3D11_DEPTH_STENCIL_VIEW_DESC desc;
			dsv->GetDesc(&desc);
			dsv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));

			m_GraphicsState.OutputMerger.DSVFormat.Set(desc.Format);
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(
  _In_ UINT NumRTVs,
  _In_reads_opt_(NumRTVs)  ID3D11RenderTargetView* const* ppRenderTargetViews,
  _In_opt_ ID3D11DepthStencilView* pDepthStencilView,
  _In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT UAVStartSlot,
  _In_ UINT NumUAVs,
  _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
  _In_reads_opt_(NumUAVs)  const UINT* pUAVInitialCounts)
{
	DX12_FUNC_LOG

	if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)
	{
		OMSetRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView);
	}

	for (UINT i = UAVStartSlot, S = UAVStartSlot + NumUAVs; i < S; ++i, ++ppUnorderedAccessViews)
	{
		CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(*ppUnorderedAccessViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Pixel].UnorderedAccessViews.Set(i, uav) && uav)
		{
			NCryDX12::CResource& resource = uav->GetDX12Resource();

			{
				resource.BeginTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS), uav->GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMSetBlendState(
  _In_opt_ ID3D11BlendState* pBlendState,
  _In_opt_ const FLOAT BlendFactor[4],
  _In_ UINT SampleMask)
{
	DX12_FUNC_LOG

	m_GraphicsState.OutputMerger.BlendState.Set(reinterpret_cast<CCryDX12BlendState*>(pBlendState));
	m_GraphicsState.OutputMerger.SampleMask.Set(SampleMask);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMSetDepthStencilState(
  _In_opt_ ID3D11DepthStencilState* pDepthStencilState,
  _In_ UINT StencilRef)
{
	DX12_FUNC_LOG

	m_GraphicsState.Rasterizer.DepthStencilState.Set(reinterpret_cast<CCryDX12DepthStencilState*>(pDepthStencilState));
	m_GraphicsState.OutputMerger.StencilRef.Set(StencilRef);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::SOSetTargets(
  _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppSOTargets,
  _In_reads_opt_(NumBuffers)  const UINT* pOffsets)
{
	DX12_FUNC_LOG
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawAuto()
{
	DX12_FUNC_LOG
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawIndexedInstancedIndirect(
  _In_ ID3D11Buffer* pBufferForArgs,
  _In_ UINT AlignedByteOffsetForArgs)
{
	DX12_FUNC_LOG

	//	PrepareState();
	//	m_pGraphicsCmdList->ExecuteIndirect(???, ???, pBufferForArgs, AlignedByteOffsetForArgs, nullptr, nullptr);
	//	m_GraphicsState.DebugPrint();
	//	if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
	//		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DrawInstancedIndirect(
  _In_ ID3D11Buffer* pBufferForArgs,
  _In_ UINT AlignedByteOffsetForArgs)
{
	DX12_FUNC_LOG

	//	PrepareState();
	//	m_pGraphicsCmdList->ExecuteIndirect(???, ???, pBufferForArgs, AlignedByteOffsetForArgs, nullptr, nullptr);
	//	m_GraphicsState.DebugPrint();
	//	if (DX12_SUBMISSION_MODE <= DX12_SUBMISSION_PERDRAW)
	//		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Dispatch(
  _In_ UINT ThreadGroupCountX,
  _In_ UINT ThreadGroupCountY,
  _In_ UINT ThreadGroupCountZ)
{
	DX12_FUNC_LOG

	// Ensure the command-list using the resource is executed (TODO: get largest fence of all used resources)
	m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

	if (PrepareComputeState())
	{
		m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE)->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		m_ComputeState.DebugPrint();

		m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DispatchIndirect(
  _In_ ID3D11Buffer* pBufferForArgs,
  _In_ UINT AlignedByteOffsetForArgs)
{
	DX12_FUNC_LOG

	//	PrepareState();
	//	m_pComputeCmdList->ExecuteIndirect(???, ???, pBufferForArgs, AlignedByteOffsetForArgs, nullptr, nullptr);
	//	m_ComputeState.DebugPrint();
	//	m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSSetState(
  _In_opt_ ID3D11RasterizerState* pRasterizerState)
{
	DX12_FUNC_LOG

	m_GraphicsState.Rasterizer.RasterizerState.Set(reinterpret_cast<CCryDX12RasterizerState*>(pRasterizerState));
	if (pRasterizerState)
	{
		const D3D11_RASTERIZER_DESC& d3d11Desc = m_GraphicsState.Rasterizer.RasterizerState->GetD3D11RasterizerDesc();

		m_GraphicsState.Rasterizer.ScissorEnabled.Set(d3d11Desc.ScissorEnable);
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSSetViewports(
  _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
  _In_reads_opt_(NumViewports)  const D3D11_VIEWPORT* pViewports)
{
	DX12_FUNC_LOG

	m_GraphicsState.Rasterizer.NumViewports.Set(NumViewports);
	if (NumViewports)
	{
		for (UINT i = 0; i < NumViewports; ++i, ++pViewports)
		{
			m_GraphicsState.Rasterizer.Viewports.Set(i, *pViewports);
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSSetScissorRects(
  _In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
  _In_reads_opt_(NumRects)  const D3D11_RECT* pRects)
{
	DX12_FUNC_LOG

	m_GraphicsState.Rasterizer.NumScissors.Set(NumRects);
	if (NumRects)
	{
		for (UINT i = 0; i < NumRects; ++i, ++pRects)
		{
			m_GraphicsState.Rasterizer.Scissors.Set(i, *pRects);
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopySubresourceRegion(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_ UINT DstX,
  _In_ UINT DstY,
  _In_ UINT DstZ,
  _In_ ID3D11Resource* pSrcResource,
  _In_ UINT SrcSubresource,
  _In_opt_ const D3D11_BOX* pSrcBox)
{
	DX12_FUNC_LOG

	CopySubresourcesRegion1(
	  pDstResource,
	  DstSubresource,
	  DstX,
	  DstY,
	  DstZ,
	  pSrcResource,
	  SrcSubresource,
	  pSrcBox,
	  0,
	  1);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopySubresourcesRegion(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_ UINT DstX,
  _In_ UINT DstY,
  _In_ UINT DstZ,
  _In_ ID3D11Resource* pSrcResource,
  _In_ UINT SrcSubresource,
  _In_opt_ const D3D11_BOX* pSrcBox,
  _In_ UINT NumSubresources)
{
	DX12_FUNC_LOG

	CopySubresourcesRegion1(
	  pDstResource,
	  DstSubresource,
	  DstX,
	  DstY,
	  DstZ,
	  pSrcResource,
	  SrcSubresource,
	  pSrcBox,
	  0,
	  NumSubresources);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopyResource(
  _In_ ID3D11Resource* pDstResource,
  _In_ ID3D11Resource* pSrcResource)
{
	DX12_FUNC_LOG

	CopyResource1(
	  pDstResource,
	  pSrcResource,
	  0);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::UploadResource(
  _In_ ICryDX12Resource* pDstResource,
  _In_ size_t NumInitialData,
  _In_reads_(NumInitialData)  const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DX12_FUNC_LOG

	ID3D12Resource* pDstResource12 = pDstResource->GetD3D12Resource();
	NCryDX12::CResource& dstResource = pDstResource->GetDX12Resource();
	NCryDX12::CResource::SInitialData* id = dstResource.GetOrCreateInitialData();
	const D3D12_RESOURCE_DESC& desc = dstResource.GetDesc();

	id->m_Size = dstResource.GetSize(0, NumInitialData);
	id->m_SubResourceData.resize(NumInitialData);

	for (UINT i = 0; i < NumInitialData; ++i)
	{
		D3D12_SUBRESOURCE_DATA initialData;

		initialData.RowPitch = pInitialData[i].SysMemPitch;
		initialData.SlicePitch = pInitialData[i].SysMemSlicePitch;

		// TODO: doesn't work, returns the size on the GPU instead of the real one
		UINT64 curSize = dstResource.GetSize(i, 1);
		UINT64 cpySize = curSize;

		// HACK: works only because CreateDeviceTexture puts it into the field
		D3D12_RESOURCE_DESC Desc = pDstResource12->GetDesc();
		if (Desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D && initialData.SlicePitch != 0)
		{
			cpySize = initialData.SlicePitch, initialData.SlicePitch = 0;
		}
		else if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D && initialData.SlicePitch != 0)
		{
			cpySize = (desc.DepthOrArraySize >> i) * initialData.SlicePitch;
		}

		assert(cpySize != 0);
		if (cpySize == 0)
		{
			__debugbreak();
		}

		initialData.pData = CryModuleMemalign(size_t(curSize), CRY_PLATFORM_ALIGNMENT);
		memset((char*)(initialData.pData) + cpySize, 0, size_t(curSize - cpySize));
		memcpy(const_cast<void*>(initialData.pData), pInitialData[i].pSysMem, size_t(cpySize));
		id->m_SubResourceData[i] = initialData;
	}

	UploadResource(&dstResource, id);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::UploadResource(
  _In_ NCryDX12::CResource* pDstResource,
  _In_ const NCryDX12::CResource::SInitialData* pSrcData)
{
	DX12_FUNC_LOG

	// TODO: This needs to be thread-safe
	// pDstResource->InitDeferred(m_pGraphicsCmdList);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::UpdateSubresource(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_opt_ const D3D11_BOX* pDstBox,
  _In_ const void* pSrcData,
  _In_ UINT SrcRowPitch,
  _In_ UINT SrcDepthPitch)
{
	DX12_FUNC_LOG

	UpdateSubresource1(
	  pDstResource,
	  DstSubresource,
	  pDstBox,
	  pSrcData,
	  SrcRowPitch,
	  SrcDepthPitch,
	  0);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopyStructureCount(
  _In_ ID3D11Buffer* pDstBuffer,
  _In_ UINT DstAlignedByteOffset,
  _In_ ID3D11UnorderedAccessView* pSrcView)
{
	DX12_FUNC_LOG

}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearRenderTargetView(
  _In_ ID3D11RenderTargetView* pRenderTargetView,
  _In_ const FLOAT ColorRGBA[4])
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pRenderTargetView);
	CCryDX12RenderTargetView* rtv = reinterpret_cast<CCryDX12RenderTargetView*>(pRenderTargetView);
	DX12_LOG(g_nPrintDX12, "Clearing render target view: %p %s", pRenderTargetView, rtv->GetResourceName().c_str());

	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearRenderTargetView(*view, ColorRGBA);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearUnorderedAccessViewUint(
  _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
  _In_ const UINT Values[4])
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pUnorderedAccessView);
	CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(pUnorderedAccessView);
	DX12_LOG(g_nPrintDX12, "Clearing unordered access view [int]: %p %s", pUnorderedAccessView, uav->GetResourceName().c_str());

	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearUnorderedAccessView(*view, Values);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearUnorderedAccessViewFloat(
  _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
  _In_ const FLOAT Values[4])
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pUnorderedAccessView);
	CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(pUnorderedAccessView);
	DX12_LOG(g_nPrintDX12, "Clearing unordered access view [float]: %p %s", pUnorderedAccessView, uav->GetResourceName().c_str());

	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearUnorderedAccessView(*view, Values);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearDepthStencilView(
  _In_ ID3D11DepthStencilView* pDepthStencilView,
  _In_ UINT ClearFlags,        // DX11 and DX12 clear flags are identical
  _In_ FLOAT Depth,
  _In_ UINT8 Stencil)
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pDepthStencilView);
	CCryDX12DepthStencilView* dsv = reinterpret_cast<CCryDX12DepthStencilView*>(pDepthStencilView);
	DX12_LOG(g_nPrintDX12, "Clearing depth stencil view: %p %s", pDepthStencilView, dsv->GetResourceName().c_str());

	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearDepthStencilView(*view, D3D12_CLEAR_FLAGS(ClearFlags), Depth, Stencil);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GenerateMips(
  _In_ ID3D11ShaderResourceView* pShaderResourceView)
{
	DX12_FUNC_LOG
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::SetResourceMinLOD(
  _In_ ID3D11Resource* pResource,
  FLOAT MinLOD)
{
	DX12_FUNC_LOG
}

FLOAT STDMETHODCALLTYPE CCryDX12DeviceContext::GetResourceMinLOD(
  _In_ ID3D11Resource* pResource)
{
	DX12_FUNC_LOG
	return 0.0f;
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ResolveSubresource(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_ ID3D11Resource* pSrcResource,
  _In_ UINT SrcSubresource,
  _In_ DXGI_FORMAT Format)
{
	DX12_FUNC_LOG
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Hull].ShaderResourceViews.Set(i, srv) && srv)
		{
			srv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSSetShader(
  _In_opt_ ID3D11HullShader* pHullShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_GraphicsState.Stages[NCryDX12::ESS_Hull].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pHullShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_GraphicsState.Stages[NCryDX12::ESS_Hull].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_GraphicsState.Stages[NCryDX12::ESS_Domain].ShaderResourceViews.Set(i, srv) && srv)
		{
			srv->BeginResourceStateTransition(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS));
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSSetShader(
  _In_opt_ ID3D11DomainShader* pDomainShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_GraphicsState.Stages[NCryDX12::ESS_Domain].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pDomainShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_GraphicsState.Stages[NCryDX12::ESS_Domain].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSSetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _In_reads_opt_(NumViews)  ID3D11ShaderResourceView* const* ppShaderResourceViews)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumViews; i < S; ++i, ++ppShaderResourceViews)
	{
		CCryDX12ShaderResourceView* srv = reinterpret_cast<CCryDX12ShaderResourceView*>(*ppShaderResourceViews);
		if (m_ComputeState.Stages[NCryDX12::ESS_Compute].ShaderResourceViews.Set(i, srv) && srv)
		{
			NCryDX12::CResource& resource = srv->GetDX12Resource();

			const bool bGfx = (resource.IsBackBuffer() || resource.IsTarget() || resource.IsGraphics());
			if (bGfx && (CMDLIST_COMPUTE != CMDLIST_TYPE_DIRECT))
			{
				if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE), srv->GetDX12View(), CS_STATE))
				{
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceState(resource, srv->GetDX12View(), CS_STATE);
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
				}
			}
			else
			{
				resource.BeginTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE), srv->GetDX12View(), CS_STATE);
			}
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSSetUnorderedAccessViews(
  _In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_1_UAV_SLOT_COUNT - StartSlot)  UINT NumUAVs,
  _In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
  _In_reads_opt_(NumUAVs)  const UINT* pUAVInitialCounts)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumUAVs; i < S; ++i, ++ppUnorderedAccessViews)
	{
		CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(*ppUnorderedAccessViews);
		if (m_ComputeState.Stages[NCryDX12::ESS_Compute].UnorderedAccessViews.Set(i, uav) && uav)
		{
			NCryDX12::CResource& resource = uav->GetDX12Resource();

			const bool bGfx = (resource.IsBackBuffer() || resource.IsTarget() || resource.IsGraphics());
			if (bGfx && (CMDLIST_COMPUTE != CMDLIST_TYPE_DIRECT))
			{
				if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE), uav->GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
				{
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceState(resource, uav->GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
				}
			}
			else
			{
				resource.BeginTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE), uav->GetDX12View(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}
		}
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSSetShader(
  _In_opt_ ID3D11ComputeShader* pComputeShader,
  _In_reads_opt_(NumClassInstances)  ID3D11ClassInstance* const* ppClassInstances,
  UINT NumClassInstances)
{
	DX12_FUNC_LOG
	m_ComputeState.Stages[NCryDX12::ESS_Compute].Shader.Set(reinterpret_cast<CCryDX12Shader*>(pComputeShader));
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSSetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _In_reads_opt_(NumSamplers)  ID3D11SamplerState* const* ppSamplers)
{
	DX12_FUNC_LOG
	for (UINT i = StartSlot, S = StartSlot + NumSamplers; i < S; ++i, ++ppSamplers)
	{
		m_ComputeState.Stages[NCryDX12::ESS_Compute].SamplerState.Set(i, reinterpret_cast<CCryDX12SamplerState*>(*ppSamplers));
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSSetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _In_reads_opt_(NumBuffers)  ID3D11Buffer* const* ppConstantBuffers)
{
	CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSGetShader(
  _Out_ ID3D11PixelShader** ppPixelShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG

}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSGetShader(
  _Out_ ID3D11VertexShader** ppVertexShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::PSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IAGetInputLayout(
  _Out_ ID3D11InputLayout** ppInputLayout)
{
	DX12_FUNC_LOG
	* ppInputLayout = m_GraphicsState.InputAssembler.InputLayout;
	if (*ppInputLayout)
	{
		(*ppInputLayout)->AddRef();
	}
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IAGetVertexBuffers(
  _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppVertexBuffers,
  _Out_writes_opt_(NumBuffers)  UINT* pStrides,
  _Out_writes_opt_(NumBuffers)  UINT* pOffsets)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IAGetIndexBuffer(
  _Out_opt_ ID3D11Buffer** pIndexBuffer,
  _Out_opt_ DXGI_FORMAT* Format,
  _Out_opt_ UINT* Offset)
{
	DX12_FUNC_LOG
	* pIndexBuffer = m_GraphicsState.InputAssembler.IndexBuffer;
	if (*pIndexBuffer)
	{
		(*pIndexBuffer)->AddRef();
	}

	*Format = m_GraphicsState.InputAssembler.IndexBufferFormat.Get();
	*Offset = m_GraphicsState.InputAssembler.IndexBufferOffset.Get();
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSGetShader(
  _Out_ ID3D11GeometryShader** ppGeometryShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::IAGetPrimitiveTopology(
  _Out_ D3D11_PRIMITIVE_TOPOLOGY* pTopology)
{
	DX12_FUNC_LOG
	* pTopology = m_GraphicsState.InputAssembler.PrimitiveTopology.Get();
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::VSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GetPredication(
  _Out_opt_ ID3D11Predicate** ppPredicate,
  _Out_opt_ BOOL* pPredicateValue)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::GSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMGetRenderTargets(
  _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11RenderTargetView** ppRenderTargetViews,
  _Out_opt_ ID3D11DepthStencilView** ppDepthStencilView)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(
  _In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
  _Out_writes_opt_(NumRTVs)  ID3D11RenderTargetView** ppRenderTargetViews,
  _Out_opt_ ID3D11DepthStencilView** ppDepthStencilView,
  _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
  _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
  _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMGetBlendState(
  _Out_opt_ ID3D11BlendState** ppBlendState,
  _Out_opt_ FLOAT BlendFactor[4],
  _Out_opt_ UINT* pSampleMask)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::OMGetDepthStencilState(
  _Out_opt_ ID3D11DepthStencilState** ppDepthStencilState,
  _Out_opt_ UINT* pStencilRef)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::SOGetTargets(
  _In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppSOTargets)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSGetState(
  _Out_ ID3D11RasterizerState** ppRasterizerState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSGetViewports(
  _Inout_ UINT* pNumViewports,
  _Out_writes_opt_(*pNumViewports)  D3D11_VIEWPORT* pViewports)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::RSGetScissorRects(
  _Inout_ UINT* pNumRects,
  _Out_writes_opt_(*pNumRects)  D3D11_RECT* pRects)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSGetShader(
  _Out_ ID3D11HullShader** ppHullShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::HSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSGetShader(
  _Out_ ID3D11DomainShader** ppDomainShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::DSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSGetShaderResources(
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
  _Out_writes_opt_(NumViews)  ID3D11ShaderResourceView** ppShaderResourceViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSGetUnorderedAccessViews(
  _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
  _Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView** ppUnorderedAccessViews)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSGetShader(
  _Out_ ID3D11ComputeShader** ppComputeShader,
  _Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance** ppClassInstances,
  _Inout_opt_ UINT* pNumClassInstances)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSGetSamplers(
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
  _Out_writes_opt_(NumSamplers)  ID3D11SamplerState** ppSamplers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CSGetConstantBuffers(
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
  _In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
  _Out_writes_opt_(NumBuffers)  ID3D11Buffer** ppConstantBuffers)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearState()
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::Flush()
{
	DX12_FUNC_LOG

	m_Scheduler.Flush(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE CCryDX12DeviceContext::GetType()
{
	DX12_FUNC_LOG
	return D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

UINT STDMETHODCALLTYPE CCryDX12DeviceContext::GetContextFlags()
{
	DX12_FUNC_LOG
	return 0;
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ExecuteCommandList(
  _In_ ID3D11CommandList* pCommandList,
  _In_ BOOL RestoreContextState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::FinishCommandList(
  BOOL RestoreDeferredContextState,
  _Out_opt_ ID3D11CommandList** ppCommandList)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

#pragma endregion

#pragma region /* D3D 11.1 specific functions */

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopySubresourceRegion1(
	_In_ ID3D11Resource* pDstResource,
	_In_ UINT DstSubresource,
	_In_ UINT DstX,
	_In_ UINT DstY,
	_In_ UINT DstZ,
	_In_ ID3D11Resource* pSrcResource,
	_In_ UINT SrcSubresource,
	_In_opt_ const D3D11_BOX* pSrcBox,
	_In_ UINT CopyFlags)
{
	DX12_FUNC_LOG

	CopySubresourcesRegion1(
		pDstResource,
		DstSubresource,
		DstX,
		DstY,
		DstZ,
		pSrcResource,
		SrcSubresource,
		pSrcBox,
		CopyFlags,
		1);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopySubresourcesRegion1(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_ UINT DstX,
  _In_ UINT DstY,
  _In_ UINT DstZ,
  _In_ ID3D11Resource* pSrcResource,
  _In_ UINT SrcSubresource,
  _In_opt_ const D3D11_BOX* pSrcBox,
  _In_ UINT CopyFlags,
  _In_ UINT NumSubresources)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dstDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pDstResource);
	ICryDX12Resource* srcDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pSrcResource);
	NCryDX12::CResource& dstResource = dstDX12Resource->GetDX12Resource(); dstResource.VerifyBackBuffer();
	NCryDX12::CResource& srcResource = srcDX12Resource->GetDX12Resource(); srcResource.VerifyBackBuffer();

	// TODO: copy from active render-target should be more elegant
	if (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(srcResource) ||
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(dstResource))
	{
		// Make the render-targets rebind, so the resource-barrier is closed
		m_GraphicsState.m_StateFlags |= EPSPB_OutputResources;
	}

	D3D12_RESOURCE_STATES prevSrcState = srcResource.GetTargetState();
	D3D12_RESOURCE_STATES prevDstState = dstResource.GetTargetState();

	if (CopyFlags & DX12_COPY_PIXELSTATE_MARKER)
		prevDstState = CP_STATE;

	// TODO: copy command on the swap chain are special (can't execute on any other queue), make an API for that
	const bool bGfx = !(srcResource.IsOffCard() || dstResource.IsOffCard()); // TODO: (srcResource.IsBackBuffer() || dstResource.IsBackBuffer()); is underperforming
	const bool bCpy = (srcResource.IsOffCard() || dstResource.IsOffCard());
	const int copyQueue = bCpy ? CMDQUEUE_COPY : CMDQUEUE_COMPUTE;
	int activeQueue = CMDQUEUE_GRAPHICS;

	if (!bGfx && ((bCpy && (CMDLIST_COPY != CMDLIST_TYPE_DIRECT)) || (!bCpy && (CMDLIST_COMPUTE != CMDLIST_TYPE_DIRECT))))
	{
		activeQueue = srcResource.SelectQueueForTransitionBarrier(CMDQUEUE_GRAPHICS,   copyQueue, D3D12_RESOURCE_STATE_COPY_SOURCE);
		activeQueue = dstResource.SelectQueueForTransitionBarrier(CMDQUEUE_GRAPHICS, activeQueue, D3D12_RESOURCE_STATE_COPY_DEST);

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, std::max(
		                         srcResource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_WRITE),
		                         dstResource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_ANY)));

		m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, std::max(
		                        srcResource.GetFenceValue(CMDQUEUE_COMPUTE, CMDTYPE_WRITE),
		                        dstResource.GetFenceValue(CMDQUEUE_COMPUTE, CMDTYPE_ANY)));

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COPY_SOURCE))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COPY_DEST))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COPY_DEST);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		m_Scheduler.GetCommandList(copyQueue)->PendingResourceBarriers();

		if (srcResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && dstResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			UINT64 offset = (pSrcBox ? pSrcBox->left : 0);
			UINT64 length = (pSrcBox ? pSrcBox->right : srcResource.GetDesc().Width) - offset;

			m_Scheduler.GetCommandList(copyQueue)->GetD3D12CommandList()->CopyBufferRegion(dstResource.GetD3D12Resource(), DstX, srcResource.GetD3D12Resource(), offset, length);
			m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;
		}
		else if (srcResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && dstResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			for (UINT n = 0; n < NumSubresources; ++n)
			{
				CD3DX12_TEXTURE_COPY_LOCATION src(srcResource.GetD3D12Resource(), SrcSubresource + n);
				CD3DX12_TEXTURE_COPY_LOCATION dst(dstResource.GetD3D12Resource(), DstSubresource + n);

				m_Scheduler.GetCommandList(copyQueue)->GetD3D12CommandList()->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, reinterpret_cast<const D3D12_BOX*>(pSrcBox));
				m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;
			}
		}
		else if (dstResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[1];
			GetD3D12Device()->GetCopyableFootprints(&dstResource.GetDesc(), DstSubresource, 1, 0, Layouts, nullptr, nullptr, nullptr);

			CD3DX12_TEXTURE_COPY_LOCATION src(srcResource.GetD3D12Resource(), Layouts[0]);
			CD3DX12_TEXTURE_COPY_LOCATION dst(dstResource.GetD3D12Resource(), DstSubresource);

			m_Scheduler.GetCommandList(copyQueue)->GetD3D12CommandList()->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, reinterpret_cast<const D3D12_BOX*>(pSrcBox));
			m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;
		}
		else if (srcResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[1];
			GetD3D12Device()->GetCopyableFootprints(&srcResource.GetDesc(), SrcSubresource, 1, 0, Layouts, nullptr, nullptr, nullptr);

			CD3DX12_TEXTURE_COPY_LOCATION src(srcResource.GetD3D12Resource(), SrcSubresource);
			CD3DX12_TEXTURE_COPY_LOCATION dst(dstResource.GetD3D12Resource(), Layouts[0]);

			m_Scheduler.GetCommandList(copyQueue)->CopyTextureRegion(&dst, DstX, DstY, DstZ, &src, reinterpret_cast<const D3D12_BOX*>(pSrcBox));
			m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;
		}

		m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_WRITE);
		m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
		m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_READ);
		m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(copyQueue, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

		if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
		{
			if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), prevSrcState))
			{
				m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, prevSrcState);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
			}

			if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), prevDstState))
			{
				m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, prevDstState);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
			}
		}

		return;
	}

	// TODO: move into the command-list function
	switch (CopyFlags)
	{
	case D3D11_COPY_DISCARD:
		DX12_LOG(g_nPrintDX12, "Using D3D11_COPY_DISCARD on old ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));

#ifdef DX12_STATS
		m_NumCopyDiscardSkips += !resource.IsUsed(m_Scheduler.GetCommandListPool(activeQueue));
		m_NumCopyDiscards++;
#endif // DX12_STATS

		// If the resource is not currently used, we do not need to discard the memory
		if (dstDX12Resource->SubstituteUsed())
		{
			;
		}

		DX12_LOG(g_nPrintDX12, "New ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));
		break;
	case D3D11_COPY_NO_OVERWRITE:
		break;
	default:
		// Block the GPU-thread until the resource is safe to be updated (unlike Map() we stage the copy and don't need to block the CPU)
		break;
	}

	m_Scheduler.GetCommandList(activeQueue)->CopySubresources(dstResource, DstSubresource, DstX, DstY, DstZ, srcResource, SrcSubresource, reinterpret_cast<const D3D12_BOX*>(pSrcBox), NumSubresources);

	if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
	{
		m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, prevSrcState);
		m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, prevDstState);
	}

	if (DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC)
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopyResource1(
  _In_ ID3D11Resource* pDstResource,
  _In_ ID3D11Resource* pSrcResource,
  _In_ UINT CopyFlags)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dstDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pDstResource);
	ICryDX12Resource* srcDX12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pSrcResource);
	NCryDX12::CResource& dstResource = dstDX12Resource->GetDX12Resource(); dstResource.VerifyBackBuffer();
	NCryDX12::CResource& srcResource = srcDX12Resource->GetDX12Resource(); srcResource.VerifyBackBuffer();

	// TODO: copy from active render-target should be more elegant
	if (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(srcResource) ||
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(dstResource))
	{
		// Make the render-targets rebind, so the resource-barrier is closed
		m_GraphicsState.m_StateFlags |= EPSPB_OutputResources;
	}

	D3D12_RESOURCE_STATES prevSrcState = srcResource.GetTargetState();
	D3D12_RESOURCE_STATES prevDstState = dstResource.GetTargetState();

	if (CopyFlags & DX12_COPY_PIXELSTATE_MARKER)
		prevDstState = CP_STATE;

	// TODO: copy command on the swap chain are special (can't execute on any other queue), make an API for that
	const bool bGfx = !(srcResource.IsOffCard() || dstResource.IsOffCard()); // TODO: (srcResource.IsBackBuffer() || dstResource.IsBackBuffer()); is sstill buggy
	const bool bCpy = (srcResource.IsOffCard() || dstResource.IsOffCard());
	const int copyQueue = bCpy ? CMDQUEUE_COPY : CMDQUEUE_COMPUTE;
	int activeQueue = CMDQUEUE_GRAPHICS;

	if (!bGfx && ((bCpy && (CMDLIST_COPY != CMDLIST_TYPE_DIRECT)) || (!bCpy && (CMDLIST_COMPUTE != CMDLIST_TYPE_DIRECT))))
	{
		activeQueue = srcResource.SelectQueueForTransitionBarrier(CMDQUEUE_GRAPHICS,   copyQueue, D3D12_RESOURCE_STATE_COPY_SOURCE);
		activeQueue = dstResource.SelectQueueForTransitionBarrier(CMDQUEUE_GRAPHICS, activeQueue, D3D12_RESOURCE_STATE_COPY_DEST);

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, std::max(
		                         srcResource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_WRITE),
		                         dstResource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_ANY)));

		m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, std::max(
		                        srcResource.GetFenceValue(CMDQUEUE_COMPUTE, CMDTYPE_WRITE),
		                        dstResource.GetFenceValue(CMDQUEUE_COMPUTE, CMDTYPE_ANY)));

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COPY_SOURCE))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COPY_DEST))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COPY_DEST);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		DX12_ASSERT(srcResource.GetDesc().Dimension == dstResource.GetDesc().Dimension, "Can't copy resources of different dimension");
		m_Scheduler.GetCommandList(copyQueue)->PendingResourceBarriers();
		m_Scheduler.GetCommandList(copyQueue)->GetD3D12CommandList()->CopyResource(dstResource.GetD3D12Resource(), srcResource.GetD3D12Resource());
		m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;

		m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_WRITE);
		m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
		m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_READ);
		m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);

		if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(srcResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
		}

		if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(dstResource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(copyQueue, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

		if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
		{
			if (srcResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), prevSrcState))
			{
				m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(srcResource, CMDTYPE_ANY);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, prevSrcState);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(srcResource, CMDTYPE_WRITE);
			}

			if (dstResource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), prevDstState))
			{
				m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(dstResource, CMDTYPE_ANY);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, prevDstState);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(dstResource, CMDTYPE_WRITE);
			}
		}

		return;
	}

	// TODO: move into the command-list function
	switch (CopyFlags)
	{
	case D3D11_COPY_DISCARD:
		DX12_LOG(g_nPrintDX12, "Using D3D11_COPY_DISCARD on old ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));

#ifdef DX12_STATS
		m_NumCopyDiscardSkips += !resource.IsUsed(m_Scheduler.GetCommandListPool(activeQueue));
		m_NumCopyDiscards++;
#endif // DX12_STATS

		// If the resource is not currently used, we do not need to discard the memory
		if (dstDX12Resource->SubstituteUsed())
		{
			;
		}

		DX12_LOG(g_nPrintDX12, "New ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));
		break;
	case D3D11_COPY_NO_OVERWRITE:
		break;
	default:
		// Block the GPU-thread until the resource is safe to be updated (unlike Map() we stage the update and don't need to block the CPU)
		break;
	}

	m_Scheduler.GetCommandList(activeQueue)->CopyResource(dstResource, srcResource);

	if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
	{
		m_Scheduler.GetCommandList(activeQueue)->SetResourceState(srcResource, prevSrcState);
		m_Scheduler.GetCommandList(activeQueue)->SetResourceState(dstResource, prevDstState);
	}

	if (DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC)
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::UpdateSubresource1(
  _In_ ID3D11Resource* pDstResource,
  _In_ UINT DstSubresource,
  _In_opt_ const D3D11_BOX* pDstBox,
  _In_ const void* pSrcData,
  _In_ UINT SrcRowPitch,
  _In_ UINT SrcDepthPitch,
  _In_ UINT CopyFlags)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pDstResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource(); resource.VerifyBackBuffer();

	// TODO: copy from active render-target should be more elegant
	if (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(resource))
	{
		// Make the render-targets rebind, so the resource-barrier is closed
		m_GraphicsState.m_StateFlags |= EPSPB_OutputResources;
	}

	D3D12_RESOURCE_STATES prevState = resource.GetTargetState();

	if (CopyFlags & DX12_COPY_PIXELSTATE_MARKER)
		prevState = CP_STATE;

	// TODO: copy command on the swap chain are special (can't execute on any other queue), make an API for that
	const bool bGfx = (resource.IsBackBuffer());
	int copyQueue = CMDQUEUE_COPY;
	int activeQueue = CMDQUEUE_GRAPHICS;

	// NOTE: this works around a AMD copy-queue bug (resources with "non-multiple of 4" strides)
	if ((CRenderer::CV_r_D3D12HardwareCopyQueue != 5) && (resource.GetDesc().Format == DXGI_FORMAT_A8_UNORM))
		copyQueue = CMDQUEUE_COMPUTE;

	if (!bGfx && (CMDLIST_COPY != CMDLIST_TYPE_DIRECT))
	{
		activeQueue = resource.SelectQueueForTransitionBarrier(CMDQUEUE_GRAPHICS, copyQueue, D3D12_RESOURCE_STATE_COPY_DEST);

		if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceState(resource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC,
		                       resource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_ANY)
		                       );

		m_Scheduler.SubmitCommands(CMDQUEUE_COMPUTE, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC,
		                      resource.GetFenceValue(CMDQUEUE_COMPUTE, CMDTYPE_ANY)
		                      );

		if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COPY_DEST))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(resource, D3D12_RESOURCE_STATE_COPY_DEST);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
		}

		ID3D12Resource* res12 = resource.GetD3D12Resource();
		const D3D12_RESOURCE_DESC& desc = resource.GetDesc();

		// Determine temporary upload buffer size (mostly only pow2-alignment of textures)
		UINT64 uploadBufferSize;
		D3D12_RESOURCE_DESC uploadDesc = desc;
		UINT DstSubresourceBox = DstSubresource;

		if (pDstBox)
		{
			// If the size is specified manually we can't specify the sub-resource to calculate footprint
			uploadDesc.Width  = pDstBox->right - pDstBox->left;
			uploadDesc.Height = pDstBox->bottom - pDstBox->top;
			DstSubresourceBox = 0;
		}

		GetD3D12Device()->GetCopyableFootprints(&uploadDesc, DstSubresourceBox, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

		DX12_ASSERT((desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) || (!pDstBox), "Box used for buffer update, that's not supported!");
		DX12_ASSERT((desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) || (SrcDepthPitch != 0), "Slice-pitch is missing for UpdateSubresourceRegion(), this is a required parameter!");
		ID3D12Resource* uploadBuffer;

		D3D12_SUBRESOURCE_DATA subData;
		ZeroMemory(&subData, sizeof(D3D12_SUBRESOURCE_DATA));
		subData.pData = pSrcData;
		subData.RowPitch = SrcRowPitch;
		subData.SlicePitch = SrcDepthPitch;
		assert(subData.pData != nullptr);

		const NCryDX12::NODE64& uploadMasks = resource.GetNodeMasks();
		if (S_OK != GetDevice()->GetDX12Device()->CreateOrReuseCommittedResource(
		      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, blsi(uploadMasks.creationMask), uploadMasks.creationMask),
		      D3D12_HEAP_FLAG_NONE,
		      &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		      D3D12_RESOURCE_STATE_GENERIC_READ,
		      nullptr,
		      IID_GFX_ARGS(&uploadBuffer)))
		{
			DX12_ERROR("Could not create intermediate upload buffer!");
			return;
		}

		m_Scheduler.GetCommandList(copyQueue)->PendingResourceBarriers();
		::UpdateSubresources<1>(GetD3D12Device(), m_Scheduler.GetCommandList(copyQueue)->GetD3D12CommandList(), res12, uploadBuffer, 0, DstSubresource, 1, &subData, reinterpret_cast<const D3D12_BOX*>(pDstBox));
		m_Scheduler.GetCommandList(copyQueue)->m_nCommands += CLCOUNT_COPY;

		m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
		m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(resource, CMDTYPE_WRITE);

		GetDevice()->GetDX12Device()->ReleaseLater(resource.GetFenceValues(CMDTYPE_WRITE), uploadBuffer);
		uploadBuffer->Release();

		if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(copyQueue), D3D12_RESOURCE_STATE_COMMON))
		{
			m_Scheduler.GetCommandList(copyQueue)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceState(resource, D3D12_RESOURCE_STATE_COMMON);
			m_Scheduler.GetCommandList(copyQueue)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
		}

		m_Scheduler.SubmitCommands(copyQueue, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

		if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
		{
			if (resource.NeedsTransitionBarrier(m_Scheduler.GetCommandList(activeQueue), prevState))
			{
				m_Scheduler.GetCommandList(activeQueue)->MaxResourceFenceValue(resource, CMDTYPE_ANY);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceState(resource, prevState);
				m_Scheduler.GetCommandList(activeQueue)->SetResourceFenceValue(resource, CMDTYPE_WRITE);
			}
		}

		return;
	}

	// TODO: move into the command-list function
	switch (CopyFlags)
	{
	case D3D11_COPY_DISCARD:
		DX12_LOG(g_nPrintDX12, "Using D3D11_COPY_DISCARD on old ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));

#ifdef DX12_STATS
		m_NumCopyDiscardSkips += !resource.IsUsed(rCmdListPool);
		m_NumCopyDiscards++;
#endif // DX12_STATS

		// If the resource is not currently used, we do not need to discard the memory
		if (dx12Resource->SubstituteUsed())
		{
			;
		}

		DX12_LOG(g_nPrintDX12, "New ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pDstResource));
		break;
	case D3D11_COPY_NO_OVERWRITE:
		break;
	default:
		// Block the GPU-thread until the resource is safe to be updated (unlike Map() we stage the update and don't need to block the CPU)
		break;
	}

	m_Scheduler.GetCommandList(activeQueue)->UpdateSubresourceRegion(resource, DstSubresource, reinterpret_cast<const D3D12_BOX*>(pDstBox), pSrcData, SrcRowPitch, SrcDepthPitch);

	if (CopyFlags & DX12_COPY_REVERTSTATE_MARKER)
	{
		m_Scheduler.GetCommandList(activeQueue)->SetResourceState(resource, prevState);
	}

	if (DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC)
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void CCryDX12DeviceContext::UpdateSubresources(
  _In_ NCryDX12::CResource* pDstResource,
  _In_ UINT64 UploadBufferSize,
  _In_ UINT NumInitialData,
  _In_reads_(NumInitialData) D3D12_SUBRESOURCE_DATA* pSrcData)
{
	DX12_FUNC_LOG

	NCryDX12::CResource& resource = *pDstResource;

	// TODO: copy command on the swap chain are special (can't execute on any other queue), make an API for that
	/*const*/ bool bGfx = (resource.IsBackBuffer());

	NCryDX12::CCommandListPool& rCmdListPool = bGfx ? m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS) : m_Scheduler.GetCommandListPool(CMDQUEUE_COPY);
	NCryDX12::CCommandList* pCmdList = bGfx ? m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS) : m_Scheduler.GetCommandList(CMDQUEUE_COPY);

	// Ensure the command-list using the resource is executed
	if (!bGfx)
		m_Scheduler.SubmitAllCommands(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC,
		                  resource.GetFenceValues(CMDTYPE_ANY)
		                  );

	// TODO: copy from active render-target should be more elegant
	if (m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->IsUsedByOutputViews(resource))
	{
		// Make the render-targets rebind, so the resource-barrier is closed
		m_GraphicsState.m_StateFlags |= EPSPB_OutputResources;
	}

	pCmdList->UpdateSubresources(resource, CP_STATE, UploadBufferSize, NumInitialData, pSrcData);

	if (!bGfx)
		m_Scheduler.SubmitCommands(CMDQUEUE_COPY, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
	else if (DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC)
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void CCryDX12DeviceContext::DiscardResource(ID3D11Resource* pResource)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource(); resource.VerifyBackBuffer();

	// Ensure the command-list using the resource is executed
	m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC,
	                       resource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_ANY)
	                       );

	m_Scheduler.GetCommandList(CMDQUEUE_COPY)->DiscardResource(resource, nullptr);
	m_Scheduler.SubmitCommands(CMDQUEUE_COPY, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);
}

void CCryDX12DeviceContext::DiscardView(ID3D11View* pResourceView)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Vertex, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, true);
}

void CCryDX12DeviceContext::HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Hull, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, true);
}

void CCryDX12DeviceContext::DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Domain, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, true);
}

void CCryDX12DeviceContext::GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Geometry, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, true);
}

void CCryDX12DeviceContext::PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Pixel, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, true);
}

void CCryDX12DeviceContext::CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants)
{
	DX12_FUNC_LOG

	  SetConstantBuffers1(NCryDX12::ESS_Compute, StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants, false);
}

void CCryDX12DeviceContext::SetConstantBuffers1(NCryDX12::EShaderStage shaderStage, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants, bool bGfx)
{
	SCryDX11PipelineState& rState = bGfx ? m_GraphicsState : m_ComputeState;
	NCryDX12::CCommandList* pCmdList = bGfx ? m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS) : m_Scheduler.GetCommandList(CMDQUEUE_COMPUTE);

	for (UINT i = StartSlot, S = StartSlot + NumBuffers, j = 0; i < S; ++i, ++ppConstantBuffers, ++j)
	{
		CCryDX12Buffer* cb = reinterpret_cast<CCryDX12Buffer*>(*ppConstantBuffers);

		TRange<UINT> bindRange;
		if (pFirstConstant)
		{
			bindRange.start = pFirstConstant[j] * NCryDX12::CONSTANT_BUFFER_ELEMENT_SIZE;
			bindRange.end = bindRange.start + pNumConstants[j] * NCryDX12::CONSTANT_BUFFER_ELEMENT_SIZE;
		}

		rState.Stages[shaderStage].ConstBufferBindRange.Set(i, bindRange);
		if (rState.Stages[shaderStage].ConstantBufferViews.Set(i, cb) && cb)
		{
			cb->BeginResourceStateTransition(pCmdList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}
	}
}

void CCryDX12DeviceContext::VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers, UINT* pFirstConstant, UINT* pNumConstants)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::SwapDeviceContextState(ID3DDeviceContextState* pState, ID3DDeviceContextState** ppPreviousState)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void CCryDX12DeviceContext::ClearView(ID3D11View* pView, const FLOAT Color[4], const D3D11_RECT* pRect, UINT NumRects)
{
	DX12_FUNC_LOG
	  DX12_LOG(g_nPrintDX12, "Clearing view: %p", pView);

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pView);

	// TODO: see if they are also doable with the copy queue
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearView(*view, Color, NumRects, pRect);
}

void CCryDX12DeviceContext::DiscardView1(ID3D11View* pResourceView, const D3D11_RECT* pRects, UINT NumRects)
{
	DX12_FUNC_LOG
	  DX12_NOT_IMPLEMENTED
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearRectsRenderTargetView(
  _In_ ID3D11RenderTargetView* pRenderTargetView,
  _In_ const FLOAT ColorRGBA[4],
  _In_ UINT NumRects,
  _In_reads_opt_(NumRects)  const D3D11_RECT* pRects)
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pRenderTargetView);
	CCryDX12RenderTargetView* rtv = reinterpret_cast<CCryDX12RenderTargetView*>(pRenderTargetView);
	DX12_LOG(g_nPrintDX12, "Clearing render target view: %p %s", pRenderTargetView, rtv->GetResourceName().c_str());

	// TODO: see if they are also doable with the copy queue
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearRenderTargetView(*view, ColorRGBA, NumRects, pRects);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearRectsUnorderedAccessViewUint(
  _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
  _In_ const UINT Values[4],
  _In_ UINT NumRects,
  _In_reads_opt_(NumRects)  const D3D11_RECT* pRects)
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pUnorderedAccessView);
	CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(pUnorderedAccessView);
	DX12_LOG(g_nPrintDX12, "Clearing unordered access view [int]: %p %s", pUnorderedAccessView, uav->GetResourceName().c_str());

	// TODO: see if they are also doable with the copy queue
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearUnorderedAccessView(*view, Values, NumRects, pRects);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearRectsUnorderedAccessViewFloat(
  _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
  _In_ const FLOAT Values[4],
  _In_ UINT NumRects,
  _In_reads_opt_(NumRects)  const D3D11_RECT* pRects)
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pUnorderedAccessView);
	CCryDX12UnorderedAccessView* uav = reinterpret_cast<CCryDX12UnorderedAccessView*>(pUnorderedAccessView);
	DX12_LOG(g_nPrintDX12, "Clearing unordered access view [float]: %p %s", pUnorderedAccessView, uav->GetResourceName().c_str());

	// TODO: see if they are also doable with the copy queue
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearUnorderedAccessView(*view, Values, NumRects, pRects);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::ClearRectsDepthStencilView(
  _In_ ID3D11DepthStencilView* pDepthStencilView,
  _In_ UINT ClearFlags,        // DX11 and DX12 clear flags are identical
  _In_ FLOAT Depth,
  _In_ UINT8 Stencil,
  _In_ UINT NumRects,
  _In_reads_opt_(NumRects)  const D3D11_RECT* pRects)
{
	DX12_FUNC_LOG

	NCryDX12::CView* view = DX12_EXTRACT_DX12VIEW(pDepthStencilView);
	CCryDX12DepthStencilView* dsv = reinterpret_cast<CCryDX12DepthStencilView*>(pDepthStencilView);
	DX12_LOG(g_nPrintDX12, "Clearing depth stencil view: %p %s", pDepthStencilView, dsv->GetResourceName().c_str());

	// TODO: see if they are also doable with the copy queue
	m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->ClearDepthStencilView(*view, D3D12_CLEAR_FLAGS(ClearFlags), Depth, Stencil, NumRects, pRects);
}

#ifdef DX12_LINKEDADAPTER
	#include "../API/Redirections/D3D12Device.inl"
#endif

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopyResourceOvercross(
	_In_ ID3D11Resource* pDstResource,
	_In_ ID3D11Resource* pSrcResource)
{
	CopyResourceOvercross1(
		pDstResource, 
		pSrcResource, 
		0);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::CopyResourceOvercross1(
	_In_ ID3D11Resource* pDstResource,
	_In_ ID3D11Resource* pSrcResource,
	_In_ UINT CopyFlags)
{
#ifdef DX12_LINKEDADAPTER
	if (m_pDevice->GetDX12Device()->IsMultiAdapter())
	{
		ICryDX12Resource* srcDX12ResourceL = DX12_EXTRACT_ICRYDX12RESOURCE(pSrcResource);
		ICryDX12Resource* srcDX12ResourceR = DX12_EXTRACT_ICRYDX12RESOURCE(pDstResource);
		NCryDX12::CResource& srcResourceL = srcDX12ResourceL->GetDX12Resource(); srcResourceL.VerifyBackBuffer();
		NCryDX12::CResource& srcResourceR = srcDX12ResourceR->GetDX12Resource(); srcResourceR.VerifyBackBuffer();

		if (srcResourceL.NeedsTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COPY), D3D12_RESOURCE_STATE_COPY_SOURCE))
		{
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(srcResourceL, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceState(srcResourceL, D3D12_RESOURCE_STATE_COPY_SOURCE);
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(srcResourceL, CMDTYPE_WRITE);
		}

		if (srcResourceR.NeedsTransitionBarrier(m_Scheduler.GetCommandList(CMDQUEUE_COPY), D3D12_RESOURCE_STATE_COPY_DEST))
		{
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(srcResourceR, CMDTYPE_ANY);
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceState(srcResourceR, D3D12_RESOURCE_STATE_COPY_DEST);
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(srcResourceR, CMDTYPE_WRITE);
		}

		// Create a point for which each GPU can wait for the fences of every other GPU
		// The resource transitions also need to occur before this intersection point
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

		m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).GetAsyncCommandQueue().SyncAdapters
		(
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->GetCommandListPool().GetFences().GetD3D12Fence(CMDQUEUE_GRAPHICS),
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->GetCommandListPool().GetFences().GetSubmittedValue(CMDQUEUE_GRAPHICS)
		);

		DX12_ASSERT(srcResourceL.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER, "");
		DX12_ASSERT(srcResourceR.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER, "");

		BroadcastableD3D12GraphicsCommandList<2>* broadcastCL = (BroadcastableD3D12GraphicsCommandList<2>*)(m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->GetD3D12CommandList());

		broadcastCL->CopyResourceOvercross
		(
			srcResourceR.GetD3D12Resource(),
			srcResourceL.GetD3D12Resource()
		);

		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->m_nCommands += CLCOUNT_COPY;

		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(srcResourceL, CMDTYPE_WRITE);
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->MaxResourceFenceValue(srcResourceR, CMDTYPE_ANY);
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(srcResourceL, CMDTYPE_READ);
		m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->SetResourceFenceValue(srcResourceR, CMDTYPE_WRITE);

		// Create a point for which each GPU can wait for the fences of every other GPU
		// The resource transitions also need to occur before this intersection point
		m_Scheduler.SubmitCommands(CMDQUEUE_GRAPHICS, DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC);

		m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).GetAsyncCommandQueue().SyncAdapters
		(
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->GetCommandListPool().GetFences().GetD3D12Fence(CMDQUEUE_GRAPHICS),
			m_Scheduler.GetCommandList(CMDQUEUE_GRAPHICS)->GetCommandListPool().GetFences().GetSubmittedValue(CMDQUEUE_GRAPHICS)
		);

		return;
	}
#endif

	CopyResource(pDstResource, pSrcResource);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::JoinSubresourceRegion(
	_In_ ID3D11Resource* pDstResource,
	_In_ UINT DstSubresource,
	_In_ UINT DstX,
	_In_ UINT DstY,
	_In_ UINT DstZ,
	_In_ ID3D11Resource* pSrcResourceL,
	_In_ ID3D11Resource* pSrcResourceR,
	_In_ UINT SrcSubresource,
	_In_opt_ const D3D11_BOX* pSrcBox)
{
	DX12_FUNC_LOG

	JoinSubresourceRegion1(
	  pDstResource,
	  DstSubresource,
	  DstX,
	  DstY,
	  DstZ,
	  pSrcResourceL,
	  pSrcResourceR,
	  SrcSubresource,
	  pSrcBox,
	  0);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::JoinSubresourceRegion1(
	_In_ ID3D11Resource* pDstResource,
	_In_ UINT DstSubresource,
	_In_ UINT DstX,
	_In_ UINT DstY,
	_In_ UINT DstZ,
	_In_ ID3D11Resource* pSrcResourceL,
	_In_ ID3D11Resource* pSrcResourceR,
	_In_ UINT SrcSubresource,
	_In_opt_ const D3D11_BOX* pSrcBox,
	_In_ UINT CopyFlags)
{
	DX12_FUNC_LOG

	// TODO: make this type-safe!
	D3D11_TEXTURE2D_DESC desc;
	((ID3D11Texture2D*)pDstResource)->GetDesc(&desc);

#ifdef DX12_LINKEDADAPTER
	CopyResourceOvercross(pSrcResourceR, pSrcResourceL);
#endif

	CopySubresourceRegion1(
	  pDstResource,
	  0,
	  0,
	  0,
	  0,
	  pSrcResourceL,
	  0,
	  nullptr,
	  CopyFlags);

	CopySubresourceRegion1(
	  pDstResource,
	  0,
	  desc.Width >> 1,
	  0,
	  0,
	  pSrcResourceR,
	  0,
	  nullptr,
	  CopyFlags);
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::CopyStagingResource(
  _In_ ID3D11Resource* pStagingResource,
  _In_ ID3D11Resource* pSourceResource,
  _In_ UINT SubResource,
  _In_ BOOL Upload)
{
	DX12_FUNC_LOG

	if (Upload)
	{
		CopySubresourceRegion1(pSourceResource, SubResource, 0, 0, 0, pStagingResource, 0, nullptr, D3D11_COPY_NO_OVERWRITE);
	}
	else
	{
		CopySubresourceRegion1(pStagingResource, 0, 0, 0, 0, pSourceResource, SubResource, nullptr, D3D11_COPY_NO_OVERWRITE);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::MapStagingResource(
  _In_ ID3D11Resource* pStagingResource,
  _In_ BOOL Upload,
  _Out_ void** ppStagingMemory)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

	const D3D12_RANGE sNoRead = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin
	const D3D12_RANGE sFullRead = { 0, SIZE_T(rResource.GetSize(0, 1)) };

	return rResource.GetD3D12Resource()->Map(0, Upload ? &sNoRead : &sFullRead, ppStagingMemory);
}

void STDMETHODCALLTYPE CCryDX12DeviceContext::UnmapStagingResource(
  _In_ ID3D11Resource* pStagingResource,
  _In_ BOOL Upload)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

	const D3D12_RANGE sNoWrite = { 0, 0 }; // It is valid to specify the CPU didn't write any data by passing a range where End is less than or equal to Begin.
	const D3D12_RANGE sFullWrite = { 0, SIZE_T(rResource.GetSize(0, 1)) };

	rResource.GetD3D12Resource()->Unmap(0, Upload ? &sFullWrite : &sNoWrite);
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::TestStagingResource(
  _In_ ID3D11Resource* pStagingResource)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

	return
	  TestForFence(rResource.GetFenceValue(CMDQUEUE_GRAPHICS, CMDTYPE_ANY)) &&
	  TestForFence(rResource.GetFenceValue(CMDQUEUE_COMPUTE , CMDTYPE_ANY)) &&
	  TestForFence(rResource.GetFenceValue(CMDQUEUE_COPY    , CMDTYPE_ANY))
		
	? S_OK : S_FALSE;
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::WaitStagingResource(
  _In_ ID3D11Resource* pStagingResource)
{
	DX12_FUNC_LOG

	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pStagingResource);
	NCryDX12::CResource& rResource = dx12Resource->GetDX12Resource();

	rResource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS), CMDTYPE_ANY);
	rResource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_COMPUTE ), CMDTYPE_ANY);
	rResource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_COPY    ), CMDTYPE_ANY);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::MappedWriteToSubresource(
	_In_ ID3D11Resource* pResource,
	_In_ UINT Subresource,
	_In_ SIZE_T Offset,
	_In_ SIZE_T Size,
	_In_ D3D11_MAP MapType,
	_In_reads_bytes_(Size) const void* pInData,
	_In_ const UINT numDataBlocks)
{
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource();

	switch (MapType & (DX12_MAP_DISCARD_MARKER - 1))
	{
	case D3D11_MAP_READ:
	case D3D11_MAP_READ_WRITE:
		return E_FAIL;
	case D3D11_MAP_WRITE:
		// Ensure the command-list using the resource is executed
		{
			CryCriticalSectionNonRecursive safe;
			m_Scheduler.SubmitAllCommands(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, resource.GetFenceValues(CMDTYPE_ANY));
		}

		// Block the CPU-thread until the resource is safe to map
		resource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS), CMDTYPE_ANY);
		break;
	case D3D11_MAP_WRITE_DISCARD:
		DX12_LOG(g_nPrintDX12, "Using D3D11_MAP_WRITE_DISCARD on old ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pResource));

#ifdef DX12_STATS
		m_NumMapDiscardSkips += !resource.IsUsed(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS));
		m_NumMapDiscards++;
#endif // DX12_STATS

		// If the resource is not currently used, we do not need to discard the memory
		if (dx12Resource->SubstituteUsed())
		{
			// ConstantBuffer/ShaderResource/UnorderedAccess marker
			if (MapType & DX12_MAP_DISCARD_MARKER)
			{
				DX12_ASSERT(false, "MAP_DISCARD does not work under DX12 when there is a view on the discardable buffer! Please rewrite the algorithm to use NO_OVERWRITE");
				abort();
			}
		}

		DX12_LOG(g_nPrintDX12, "New ID3D12Resource: %p", DX12_EXTRACT_D3D12RESOURCE(pResource));
		break;
	case D3D11_MAP_WRITE_NO_OVERWRITE:
		break;
	default:
		break;
	}

	const D3D12_RANGE sRangeWrite = { Offset, Offset + Size };

#ifdef DX12_LINKEDADAPTER
	if (m_pDevice->GetDX12Device()->IsMultiAdapter())
	{
		BroadcastableD3D12Resource<2>* broadcastR = (BroadcastableD3D12Resource<2>*)(resource.GetD3D12Resource());
		return broadcastR->MappedWriteToSubresource(Subresource, &sRangeWrite, pInData, numDataBlocks);
	}
#endif

	return resource.MappedWriteToSubresource(Subresource, &sRangeWrite, pInData);
}

HRESULT STDMETHODCALLTYPE CCryDX12DeviceContext::MappedReadFromSubresource(
	_In_ ID3D11Resource* pResource,
	_In_ UINT Subresource,
	_In_ SIZE_T Offset,
	_In_ SIZE_T Size,
	_In_ D3D11_MAP MapType,
	_Out_writes_bytes_(Size) void* pOutData,
	_In_ const UINT numDataBlocks)
{
	ICryDX12Resource* dx12Resource = DX12_EXTRACT_ICRYDX12RESOURCE(pResource);
	NCryDX12::CResource& resource = dx12Resource->GetDX12Resource();
	
	switch (MapType & (DX12_MAP_DISCARD_MARKER - 1))
	{
	case D3D11_MAP_READ:
		// Ensure the command-list using the resource is executed
		{
			CryCriticalSectionNonRecursive safe;
			m_Scheduler.SubmitAllCommands(DX12_SUBMISSION_MODE == DX12_SUBMISSION_SYNC, resource.GetFenceValues(CMDTYPE_WRITE));
		}

		// Block the CPU-thread until the resource is safe to map
		resource.WaitForUnused<true>(m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS), CMDTYPE_WRITE);
		break;
	case D3D11_MAP_READ_WRITE:
	case D3D11_MAP_WRITE:
	case D3D11_MAP_WRITE_DISCARD:
	case D3D11_MAP_WRITE_NO_OVERWRITE:
		return E_FAIL;
	default:
		break;
	}

	const D3D12_RANGE sRangeRead = { Offset, Offset + Size };

#ifdef DX12_LINKEDADAPTER
	if (m_pDevice->GetDX12Device()->IsMultiAdapter())
	{
		BroadcastableD3D12Resource<2>* broadcastR = (BroadcastableD3D12Resource<2>*)(resource.GetD3D12Resource());
		return broadcastR->MappedReadFromSubresource(Subresource, &sRangeRead, pOutData, numDataBlocks);
	}
#endif

	return resource.MappedReadFromSubresource(Subresource, &sRangeRead, pOutData);
}

void CCryDX12DeviceContext::ResetCachedState(bool bGraphics, bool bCompute)
{
	if (bGraphics) m_GraphicsState.Invalidate();
	if (bCompute)  m_ComputeState.Invalidate();

	m_pCurrentRootSignature.reset();
	m_pCurrentPSO.reset();
}

#pragma endregion
