// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12CommandList.hpp"
#include "DX12Resource.hpp"
#include "DX12View.hpp"
#include "DX12SamplerState.hpp"
#include "DX12/GI/CCryDX12SwapChain.hpp"

#define DX12_REPLACE_BACKBUFFER

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CCommandList::CCommandList(CCommandListPool& rPool)
	: CDeviceObject(rPool.GetDevice())
	, m_rPool(rPool)
	, m_nodeMask(rPool.GetNodeMask())
	, m_pCmdQueue(NULL)
	, m_pCmdAllocator(NULL)
	, m_pCmdList(NULL)
	, m_pD3D12Device(NULL)
	, m_CurrentNumRTVs(0)
	, m_CurrentNumVertexBuffers(0)
	, m_CurrentNumPendingBarriers(0)
	, m_CurrentFenceValue(0)
	, m_eListType(rPool.GetD3D12QueueType())
	, m_eState(CLSTATE_FREE)
#if defined(_ALLOW_INITIALIZER_LISTS)
	// *INDENT-OFF*
	, m_DescriptorHeaps{
		{ rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024) },
		{ rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024) },
		{ rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024) },
		{ rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024) }
	}
	// *INDENT-ON*
#endif
{
#if !defined(_ALLOW_INITIALIZER_LISTS)
	m_DescriptorHeaps.emplace_back(rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024));
	m_DescriptorHeaps.emplace_back(rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024));
	m_DescriptorHeaps.emplace_back(rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024));
	m_DescriptorHeaps.emplace_back(rPool.GetDevice()->GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1024));
#endif
}

//---------------------------------------------------------------------------------------------------------------------
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
BOOL CCommandList::s_DepthBoundsTestSupported = false;
#endif

//---------------------------------------------------------------------------------------------------------------------
CCommandList::~CCommandList()
{
}

//---------------------------------------------------------------------------------------------------------------------

bool CCommandList::Init(UINT64 currentFenceValue)
{
	m_pD3D12Device = GetDevice()->GetD3D12Device();

	m_nCommands = 0;
	m_pDSV = nullptr;
	m_CurrentNumRTVs = 0;
	m_CurrentNumVertexBuffers = 0;
	m_CurrentNumPendingBarriers = 0;

	m_CurrentFenceValue = currentFenceValue;
	ZeroMemory(m_UsedFenceValues, sizeof(m_UsedFenceValues));

	for (D3D12_DESCRIPTOR_HEAP_TYPE eType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; eType < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; eType = D3D12_DESCRIPTOR_HEAP_TYPE(eType + 1))
	{
		m_DescriptorHeaps[eType].Reset();
	}

	if (!m_pCmdQueue)
	{
		m_pCmdQueue = m_rPool.GetD3D12CommandQueue();
	}

	D3D12_COMMAND_LIST_TYPE eCmdListType = m_pCmdQueue->GetDesc().Type;

	if (!m_pCmdAllocator)
	{
		ID3D12CommandAllocator* pCmdAllocator = nullptr;
		if (S_OK != m_pD3D12Device->CreateCommandAllocator(eCmdListType, IID_GFX_ARGS(&pCmdAllocator)))
		{
			DX12_ERROR("Could not create command allocator!");
			return false;
		}

		m_pCmdAllocator = pCmdAllocator;
		pCmdAllocator->Release();
	}

	if (!m_pCmdList)
	{
		ID3D12GraphicsCommandList* pCmdList = nullptr;
		if (S_OK != m_pD3D12Device->CreateCommandList(m_nodeMask, eCmdListType, m_pCmdAllocator, nullptr, IID_GFX_ARGS(&pCmdList)))
		{
			DX12_ERROR("Could not create command list");
			return false;
		}

		m_pCmdList = pCmdList;
		pCmdList->Release();
		
		if (m_eListType == D3D12_COMMAND_LIST_TYPE_COPY)
		{
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
			m_pCmdList1 = nullptr;
#endif
#if NTDDI_WIN10_RS3 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS3)
			m_pCmdList2 = nullptr;
#endif
		}
		else
		{
			// Creator's Update
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
			ID3D12GraphicsCommandList1* pCmdList1 = nullptr;
			pCmdList->QueryInterface(__uuidof(ID3D12GraphicsCommandList1), (void**)&pCmdList1);
			if (m_pCmdList1 = pCmdList1)
				pCmdList1->Release();

			D3D12_FEATURE_DATA_D3D12_OPTIONS2 Options2;
			if (GetDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &Options2, sizeof(Options2)) == S_OK)
				s_DepthBoundsTestSupported = Options2.DepthBoundsTestSupported;
#endif

			// Fall Creator's Update
#if NTDDI_WIN10_RS3 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS3)
			ID3D12GraphicsCommandList2* pCmdList2 = nullptr;
			pCmdList->QueryInterface(__uuidof(ID3D12GraphicsCommandList1), (void**)&pCmdList2);
			if (m_pCmdList2 = pCmdList2)
				pCmdList2->Release();
#endif
		}
	}

#ifdef DX12_STATS
	m_NumWaitsGPU = 0;
	m_NumWaitsCPU = 0;
#endif // DX12_STATS

	return true;
}

void CCommandList::Register()
{
	UINT64 nextFenceValue = m_rPool.GetCurrentFenceValue() + 1;

	// Increment fence on allocation, this has the effect that
	// acquired CommandLists need to be submitted in-order to prevent
	// dead-locking
	m_rPool.SetCurrentFenceValue(nextFenceValue);

	Init(nextFenceValue);

	m_eState = CLSTATE_STARTED;
}

void CCommandList::Begin()
{
}

void CCommandList::End()
{
	if (IsUtilized())
	{
		PendingResourceBarriers();

		HRESULT res = m_pCmdList->Close();
		DX12_ASSERT(res == S_OK, "Could not close command list!");
	}

	m_eState = CLSTATE_COMPLETED;
}

void CCommandList::Schedule()
{
	if (m_eState < CLSTATE_SCHEDULED)
	{
		m_eState = CLSTATE_SCHEDULED;

#ifdef DX12_OMITTABLE_COMMANDLISTS
		const bool bRecyclable = (m_rPool.GetCurrentFenceValue() == m_CurrentFenceValue);
		if (bRecyclable & !IsUtilized())
		{
			// Rewind fence value and don't declare submitted
			m_rPool.SetCurrentFenceValue(m_CurrentFenceValue - 1);

			// The command-list doesn't "exist" now (makes all IsFinished() checks return true)
			m_CurrentFenceValue = 0;
		}
		else
#endif
		{
			m_rPool.SetSubmittedFenceValue(m_CurrentFenceValue);
		}
	}
}

void CCommandList::Submit()
{
	if (IsUtilized())
	{
		// Inject a Wait() into the CommandQueue prior to executing it to wait for all required resources being available either readable or writable
		// *INDENT-OFF*
#ifdef VK_IN_ORDER_SUBMISSION
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COMPUTE ] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE ]);
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_GRAPHICS] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS]);
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COPY    ] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COPY    ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY    ]);
#else
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COMPUTE ], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE ]));
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_GRAPHICS], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS]));
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COPY    ], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COPY    ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY    ]));
#endif
		// *INDENT-ON*

		m_rPool.WaitForFenceOnGPU(m_UsedFenceValues[CMDTYPE_ANY]);

		// Then inject the Execute() which is possibly blocked by the Wait()
		ID3D12CommandList* ppCommandLists[] = { GetD3D12CommandList() };
		m_rPool.GetAsyncCommandQueue().ExecuteCommandLists(1, ppCommandLists); // TODO: allow to submit multiple command-lists in one go
	}

#ifdef DX12_OMITTABLE_COMMANDLISTS
	if (IsUtilized())
#endif
	{
		// Inject the signal of the utilized fence to unblock code which picked up the fence of the command-list (even if it doesn't have contents)
		SignalFenceOnGPU();

		DX12_LOG(DX12_CONCURRENCY_ANALYZER, "######################################## END [%s %lld] CL ########################################",
			m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			m_rPool.GetSubmittedFenceValue());
	}

	m_eState = CLSTATE_SUBMITTED;
}

void CCommandList::Clear()
{
	m_eState = CLSTATE_CLEARING;
	m_rPool.GetAsyncCommandQueue().ResetCommandList(this);
}

bool CCommandList::Reset()
{
	HRESULT ret = S_OK;

	if (IsUtilized())
	{
		// reset the allocator before the list re-occupies it, otherwise the whole state of the allocator starts leaking
		if (m_pCmdAllocator)
		{
			ret = m_pCmdAllocator->Reset();
		}
		// make the list re-occupy this allocator, reseting the list will _not_ reset the allocator
		if (m_pCmdList)
		{
			ret = m_pCmdList->Reset(m_pCmdAllocator, NULL);
		}
	}

	m_eState = CLSTATE_FREE;
	m_nCommands = 0;
	return ret == S_OK;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers)
{
	if (DX12_BARRIER_FUSION && CRenderer::CV_r_D3D12BatchResourceBarriers)
	{
#if 1
		if (CRenderer::CV_r_D3D12BatchResourceBarriers > 1)
		{
			CRY_ASSERT(m_CurrentNumPendingBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
			{
				UINT j = 0;

				if (pBarriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
				{
					for (; j < m_CurrentNumPendingBarriers; ++j)
					{
						if (pBarriers[i].Type == m_PendingBarrierHeap[j].Type &&
							pBarriers[i].Transition.pResource == m_PendingBarrierHeap[j].Transition.pResource &&
							pBarriers[i].Transition.Subresource == m_PendingBarrierHeap[j].Transition.Subresource)
						{
							break;
						}
					}
				}

				else if (pBarriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
				{
					for (; j < m_CurrentNumPendingBarriers; ++j)
					{
						if (pBarriers[i].Type == m_PendingBarrierHeap[j].Type &&
							pBarriers[i].Transition.pResource == m_PendingBarrierHeap[j].Transition.pResource &&
							pBarriers[i].Transition.Subresource == m_PendingBarrierHeap[j].Transition.Subresource)
						{
							if (m_PendingBarrierHeap[j].Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE)
							if (pBarriers[i].Flags == D3D12_RESOURCE_BARRIER_FLAG_NONE)
							{
								assert(m_PendingBarrierHeap[j].Transition.StateAfter == pBarriers[i].Transition.StateBefore);
								m_PendingBarrierHeap[j].Transition.StateAfter = pBarriers[i].Transition.StateAfter;
								break;
							}
					
							if (m_PendingBarrierHeap[j].Flags == D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
							if (pBarriers[i].Flags == D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
							{
								assert(m_PendingBarrierHeap[j].Transition.StateAfter == pBarriers[i].Transition.StateAfter);
								assert(m_PendingBarrierHeap[j].Transition.StateBefore == pBarriers[i].Transition.StateBefore);
								m_PendingBarrierHeap[j].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
								break;
							}
						}
					}
				}

				if (j == m_CurrentNumPendingBarriers)
				{
					m_PendingBarrierHeap[m_CurrentNumPendingBarriers] = pBarriers[i];
					m_CurrentNumPendingBarriers += 1;
					m_nCommands += CLCOUNT_BARRIER * 1;
				}
				else
				{
					if (m_PendingBarrierHeap[j].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
					if (m_PendingBarrierHeap[j].Transition.StateBefore == m_PendingBarrierHeap[j].Transition.StateAfter)
					{
						m_CurrentNumPendingBarriers--;

						memmove(m_PendingBarrierHeap + j, m_PendingBarrierHeap + j + 1, (m_CurrentNumPendingBarriers - j) * sizeof(D3D12_RESOURCE_BARRIER));
					}
				}
			}
		}
		else
#endif
		{
			CRY_ASSERT(m_CurrentNumPendingBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
				m_PendingBarrierHeap[m_CurrentNumPendingBarriers + i] = pBarriers[i];

			m_CurrentNumPendingBarriers += NumBarriers;
			m_nCommands += CLCOUNT_BARRIER * NumBarriers;
		}
	}
	else
	{
		m_pCmdList->ResourceBarrier(NumBarriers, pBarriers);
		m_nCommands += CLCOUNT_BARRIER;
	}
}

void CCommandList::PendingResourceBarriers()
{
	if (DX12_BARRIER_FUSION && m_CurrentNumPendingBarriers)
	{
		m_pCmdList->ResourceBarrier(m_CurrentNumPendingBarriers, m_PendingBarrierHeap);
		m_CurrentNumPendingBarriers = 0;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::SetRootSignature(const CRootSignature* pRootSignature, bool bGfx, const D3D12_GPU_VIRTUAL_ADDRESS (&vConstViews)[8 /*CB_PER_FRAME*/])
{
	if (bGfx)
	{
		m_pCmdList->SetGraphicsRootSignature(pRootSignature->GetD3D12RootSignature());
		m_nCommands += CLCOUNT_SETIO;

		SetGraphicsRootSignature(pRootSignature, vConstViews);
	}
	else
	{
		m_pCmdList->SetComputeRootSignature(pRootSignature->GetD3D12RootSignature());
		m_nCommands += CLCOUNT_SETIO;

		SetComputeRootSignature(pRootSignature, vConstViews);
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandList::IsFull(size_t numResources, size_t numSamplers, size_t numRendertargets, size_t numDepthStencils) const
{
	return
	  (GetResourceHeap().GetCursor() + numResources >= GetResourceHeap().GetCapacity()) ||
	  (GetSamplerHeap().GetCursor() + numSamplers >= GetSamplerHeap().GetCapacity()) ||
	  (GetRenderTargetHeap().GetCursor() + numRendertargets >= GetRenderTargetHeap().GetCapacity()) ||
	  (GetDepthStencilHeap().GetCursor() + numDepthStencils >= GetDepthStencilHeap().GetCapacity());
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::SetGraphicsRootSignature(const CRootSignature* pRootSignature, const D3D12_GPU_VIRTUAL_ADDRESS (&vConstViews)[8 /*CB_PER_FRAME*/])
{
	DX12_ASSERT(pRootSignature);

	const SResourceMappings& rm = pRootSignature->GetResourceMappings();
	const CD3DX12_ROOT_PARAMETER* pTableInfo = rm.m_RootParameters;
	for (UINT p = 0, n = rm.m_NumRootParameters; p < n; ++p, ++pTableInfo)
	{
		if (pTableInfo->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS addr = pTableInfo->ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL ? vConstViews[pTableInfo->Descriptor.ShaderRegister] : D3D12_GPU_VIRTUAL_ADDRESS(0);
		switch (pTableInfo->ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
			m_pCmdList->SetGraphicsRootConstantBufferView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_SRV:
			m_pCmdList->SetGraphicsRootShaderResourceView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			m_pCmdList->SetGraphicsRootUnorderedAccessView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		}
	}
}

void CCommandList::SetGraphicsDescriptorTables(const CRootSignature* pRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE eType)
{
	DX12_ASSERT(pRootSignature);

	const SResourceMappings& rm = pRootSignature->GetResourceMappings();
	const CD3DX12_ROOT_PARAMETER* pTableInfo = rm.m_RootParameters;
	for (UINT p = 0, r = 0, n = rm.m_NumRootParameters; p < n; ++p, ++pTableInfo)
	{
		if (pTableInfo->ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			continue;
		}

		if (rm.m_DescriptorTables[r].m_Type == eType)
		{
			m_pCmdList->SetGraphicsRootDescriptorTable(p, m_DescriptorHeaps[eType].GetHandleOffsetGPU(rm.m_DescriptorTables[r].m_Offset));
			m_nCommands += CLCOUNT_SETIO;
		}

		++r;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::SetComputeRootSignature(const CRootSignature* pRootSignature, const D3D12_GPU_VIRTUAL_ADDRESS (&vConstViews)[8 /*CB_PER_FRAME*/])
{
	DX12_ASSERT(pRootSignature);

	const SResourceMappings& rm = pRootSignature->GetResourceMappings();
	const CD3DX12_ROOT_PARAMETER* pTableInfo = rm.m_RootParameters;
	for (UINT p = 0, n = rm.m_NumRootParameters; p < n; ++p, ++pTableInfo)
	{
		if (pTableInfo->ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			continue;
		}

		D3D12_GPU_VIRTUAL_ADDRESS addr = pTableInfo->ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL ? vConstViews[pTableInfo->Descriptor.ShaderRegister] : D3D12_GPU_VIRTUAL_ADDRESS(0);
		switch (pTableInfo->ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
			m_pCmdList->SetComputeRootConstantBufferView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_SRV:
			m_pCmdList->SetComputeRootShaderResourceView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			m_pCmdList->SetComputeRootUnorderedAccessView(p, addr);
			m_nCommands += CLCOUNT_SETIO;
			break;
		}
	}
}

void CCommandList::SetComputeDescriptorTables(const CRootSignature* pRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE eType)
{
	DX12_ASSERT(pRootSignature);

	const SResourceMappings& rm = pRootSignature->GetResourceMappings();
	const CD3DX12_ROOT_PARAMETER* pTableInfo = rm.m_RootParameters;
	for (UINT p = 0, r = 0, n = rm.m_NumRootParameters; p < n; ++p, ++pTableInfo)
	{
		if (pTableInfo->ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			continue;
		}

		if (rm.m_DescriptorTables[r].m_Type == eType)
		{
			m_pCmdList->SetComputeRootDescriptorTable(p, m_DescriptorHeaps[eType].GetHandleOffsetGPU(rm.m_DescriptorTables[r].m_Offset));
			m_nCommands += CLCOUNT_SETIO;
		}

		++r;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindDepthStencilView(const CView& dsv)
{
	CResource& resource = dsv.GetDX12Resource();
	const D3D12_CPU_DESCRIPTOR_HANDLE handle = GetDepthStencilHeap().GetHandleOffsetCPU(0);

	if (INVALID_CPU_DESCRIPTOR_HANDLE != dsv.GetDescriptorHandle())
	{
		m_pD3D12Device->CopyDescriptorsSimple(1, handle, dsv.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}
	else
	{
		m_pD3D12Device->CreateDepthStencilView(resource.GetD3D12Resource(), dsv.HasDesc() ? &(dsv.GetDSVDesc()) : NULL, handle);
	}

	if (dsv.GetDSVDesc().Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH)
	{
		TrackResourceDRVUsage(resource, dsv);
	}
	else
	{
		TrackResourceDSVUsage(resource, dsv);
	}
}

//---------------------------------------------------------------------------------------------------------------------

bool CCommandList::PresentSwapChain(CSwapChain* pDX12SwapChain)
{
	CResource& pBB = pDX12SwapChain->GetCurrentBackBuffer(); pBB.VerifyBackBuffer();

	if (SetResourceState(pBB, D3D12_RESOURCE_STATE_PRESENT) != D3D12_RESOURCE_STATE_PRESENT)
	{
		return true;
	}

	return false;
}

void CCommandList::BindRenderTargetView(const CView& rtv)
{
	CResource& resource = rtv.GetDX12Resource();
	const D3D12_CPU_DESCRIPTOR_HANDLE handle = GetRenderTargetHeap().GetHandleOffsetCPU(0);

	if (INVALID_CPU_DESCRIPTOR_HANDLE != rtv.GetDescriptorHandle())
	{
		m_pD3D12Device->CopyDescriptorsSimple(1, handle, rtv.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	else
	{
		m_pD3D12Device->CreateRenderTargetView(resource.GetD3D12Resource(), rtv.HasDesc() ? &(rtv.GetRTVDesc()) : NULL, handle);
	}

	TrackResourceRTVUsage(resource, rtv);
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindUnorderedAccessView(const CView& uav)
{
	CResource& resource = uav.GetDX12Resource();
	const D3D12_CPU_DESCRIPTOR_HANDLE handle = GetResourceHeap().GetHandleOffsetCPU(0);

	if (INVALID_CPU_DESCRIPTOR_HANDLE != uav.GetDescriptorHandle())
	{
		m_pD3D12Device->CopyDescriptorsSimple(1, handle, uav.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	else
	{
		m_pD3D12Device->CreateUnorderedAccessView(resource.GetD3D12Resource(), nullptr, uav.HasDesc() ? &(uav.GetUAVDesc()) : NULL, handle);
	}

	TrackResourceUAVUsage(resource, uav);
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindResourceView(const CView& view, const TRange<UINT>& bindRange, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle)
{
	CResource& resource = view.GetDX12Resource(); resource.VerifyBackBuffer();

	switch (view.GetType())
	{
	case EVT_ConstantBufferView:
		if (INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle())
		{
			m_pD3D12Device->CopyDescriptorsSimple(1, dstHandle, view.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else
		{
			// NOTE: someone may (?) use MAP_DISCARD, that's why the GPU-address has to be fetched all the time instead of using the cached value
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc = view.GetCBVDesc();
			cbvDesc.BufferLocation = view.GetDX12Resource().GetGPUVirtualAddress() + bindRange.start;
			cbvDesc.SizeInBytes = bindRange.Length() > 0 ? bindRange.Length() : cbvDesc.SizeInBytes - bindRange.start;
			cbvDesc.SizeInBytes = min(cbvDesc.SizeInBytes, D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * NCryDX12::CONSTANT_BUFFER_ELEMENT_SIZE);
			m_pD3D12Device->CreateConstantBufferView(&cbvDesc, dstHandle);
		}

		TrackResourceCBVUsage(resource);
		break;

	case EVT_ShaderResourceView:
		if (INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle())
		{
			m_pD3D12Device->CopyDescriptorsSimple(1, dstHandle, view.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else
		{
			DX12_ASSERT(resource.GetD3D12Resource(), "No resource to create SRV from!");
			m_pD3D12Device->CreateShaderResourceView(resource.GetD3D12Resource(), &view.GetSRVDesc(), dstHandle);
		}

		if (m_eListType == D3D12_COMMAND_LIST_TYPE_COMPUTE)
		{
			TrackResourceCRVUsage(resource, view);
		}
		else
		{
			TrackResourcePRVUsage(resource, view);
		}
		break;

	case EVT_UnorderedAccessView:
		if (INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle())
		{
			m_pD3D12Device->CopyDescriptorsSimple(1, dstHandle, view.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else
		{
			DX12_ASSERT(resource.GetD3D12Resource(), "No resource to create UAV from!");
			m_pD3D12Device->CreateUnorderedAccessView(resource.GetD3D12Resource(), nullptr, &view.GetUAVDesc(), dstHandle);
		}

		TrackResourceUAVUsage(resource, view);
		break;

	case EVT_DepthStencilView:
		DX12_ASSERT(0, "Unsupported DSV creation for input!");

		TrackResourceDSVUsage(resource, view);
		break;

	case EVT_RenderTargetView:
		DX12_ASSERT(0, "Unsupported RTV creation for input!");

		TrackResourceRTVUsage(resource, view);
		break;

	case EVT_VertexBufferView:
		DX12_ASSERT(0, "Unsupported VSV creation for input!");

		TrackResourceVBVUsage(resource);
		break;

	default:
		DX12_ASSERT(0, "Unsupported resource-type for input!");
		break;
	}
}

void CCommandList::BindSamplerState(const CSamplerState& state, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle)
{
	if (INVALID_CPU_DESCRIPTOR_HANDLE != state.GetDescriptorHandle())
	{
		m_pD3D12Device->CopyDescriptorsSimple(1, dstHandle, state.GetDescriptorHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}
	else
	{
		//		GetDevice()->CreateOrReuseSampler(&state.GetSamplerDesc(), GetSamplerHeap().GetDescriptorSize(), GetSamplerHeap().GetHandleOffsetCPU(offset));
		m_pD3D12Device->CreateSampler(&state.GetSamplerDesc(), dstHandle);
	}
}

void CCommandList::SetResourceAndSamplerStateHeaps()
{
	ID3D12DescriptorHeap* heaps[] =
	{
		GetResourceHeap().GetDescriptorHeap()->GetD3D12DescriptorHeap(),
		GetSamplerHeap().GetDescriptorHeap()->GetD3D12DescriptorHeap()
	};

	m_pCmdList->SetDescriptorHeaps(2, heaps);
	m_nCommands += CLCOUNT_SETIO;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindAndSetOutputViews(INT numRTVs, const CView** rtv, const CView* dsv)
{
	if (dsv)
	{
		BindDepthStencilView(*dsv);
		GetDepthStencilHeap().IncrementCursor();
	}

	m_pDSV = dsv;

	for (INT i = 0; i < numRTVs; ++i)
	{
		BindRenderTargetView(*rtv[i]);
		GetRenderTargetHeap().IncrementCursor();

		m_pRTV[i] = rtv[i];
	}

	m_CurrentNumRTVs = numRTVs;

	D3D12_CPU_DESCRIPTOR_HANDLE RTVDescriptors[] = { GetRenderTargetHeap().GetHandleOffsetCPU(-numRTVs) };
	D3D12_CPU_DESCRIPTOR_HANDLE DSVDescriptors[] = { GetDepthStencilHeap().GetHandleOffsetCPU(-1) };

	m_pCmdList->OMSetRenderTargets(numRTVs, numRTVs ? RTVDescriptors : NULL, true, dsv ? DSVDescriptors : NULL);
	m_nCommands += CLCOUNT_SETIO;
}

bool CCommandList::IsUsedByOutputViews(const CResource& res) const
{
	if (m_pDSV && (&m_pDSV->GetDX12Resource() == &res))
	{
		return true;
	}

	for (INT i = 0, n = m_CurrentNumRTVs; i < n; ++i)
	{
		if (m_pRTV[i] && (&m_pRTV[i]->GetDX12Resource() == &res))
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindVertexBufferView(const CView& view, INT offset, const TRange<UINT>& bindRange, UINT bindStride, D3D12_VERTEX_BUFFER_VIEW (&heap)[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT])
{
	CResource& resource = view.GetDX12Resource();

	// NOTE: transient buffers use MAP_DISCARD, that's why the GPU-address has to be fetched all the time instead of using the cached value
	D3D12_VERTEX_BUFFER_VIEW& vbvDesc = heap[offset];
	vbvDesc = view.GetVBVDesc();
	vbvDesc.BufferLocation = view.GetDX12Resource().GetGPUVirtualAddress() + bindRange.start;
	vbvDesc.SizeInBytes = bindRange.Length() > 0 ? bindRange.Length() : vbvDesc.SizeInBytes - bindRange.start;
	vbvDesc.StrideInBytes = bindStride;

	// Should offseted vertex buffer views be also aligned to 256 bytes?
	// if (descs->m_Offset)
	//   view->SizeInBytes = ((view->SizeInBytes + 255) & ~255) - 256;

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceVBVUsage(resource);
}

void CCommandList::ClearVertexBufferHeap(INT num)
{
	memset(m_VertexBufferHeap, 0, std::max(num, m_CurrentNumVertexBuffers) * sizeof(m_VertexBufferHeap[0]));
}

void CCommandList::SetVertexBufferHeap(INT num)
{
	m_pCmdList->IASetVertexBuffers(0, std::max(num, m_CurrentNumVertexBuffers), m_VertexBufferHeap);
	m_nCommands += CLCOUNT_SETIO;

	m_CurrentNumVertexBuffers = num;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::BindAndSetIndexBufferView(const CView& view, DXGI_FORMAT format, UINT offset)
{
	CResource& resource = view.GetDX12Resource();

	// NOTE: transient buffers use MAP_DISCARD, that's why the GPU-address has to be fetched all the time instead of using the cached value
	D3D12_INDEX_BUFFER_VIEW ibvDesc;
	ibvDesc = view.GetIBVDesc();
	ibvDesc.BufferLocation = view.GetDX12Resource().GetGPUVirtualAddress() + offset;
	ibvDesc.SizeInBytes = ibvDesc.SizeInBytes - offset;
	ibvDesc.Format = (format == DXGI_FORMAT_UNKNOWN ? resource.GetDesc().Format : format);

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceIBVUsage(resource);

	m_pCmdList->IASetIndexBuffer(&ibvDesc);
	m_nCommands += CLCOUNT_SETIO;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::ClearDepthStencilView(const CView& view, D3D12_CLEAR_FLAGS clearFlags, float depthValue, UINT stencilValue, UINT NumRects, const D3D12_RECT* pRect)
{
	DX12_ASSERT(INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle(), "View has no descriptor handle, that is not allowed!");

	CResource& resource = view.GetDX12Resource();

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceDSVUsage(resource, view);

#if !defined(RELEASE) && defined(_DEBUG) // will break a lot currently, won't really be coherent in the future
	UINT len = 512;
	char str[512] = "-";
	UINT cen = 20;
	D3D12_CLEAR_VALUE clearValue;

	if (view.GetD3D12Resource()->GetPrivateData(WKPDID_D3DDebugClearValue, &cen, &clearValue) == S_OK)
	{
		view.GetD3D12Resource()->GetPrivateData(WKPDID_D3DDebugObjectName, &len, str);

		DX12_ASSERT(!(clearFlags & D3D12_CLEAR_FLAG_DEPTH) || (depthValue == clearValue.DepthStencil.Depth), "...");
		DX12_ASSERT(!(clearFlags & D3D12_CLEAR_FLAG_STENCIL) || (stencilValue == clearValue.DepthStencil.Stencil), "...");
	}
#endif

	PendingResourceBarriers();

	m_pCmdList->ClearDepthStencilView(view.GetDescriptorHandle(), clearFlags, depthValue, stencilValue, NumRects, pRect);
	m_nCommands += CLCOUNT_CLEAR;
}

void CCommandList::ClearRenderTargetView(const CView& view, const FLOAT rgba[4], UINT NumRects, const D3D12_RECT* pRect)
{
	DX12_ASSERT(INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle(), "View has no descriptor handle, that is not allowed!");

	CResource& resource = view.GetDX12Resource(); resource.VerifyBackBuffer();

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceRTVUsage(resource, view);

#if !defined(RELEASE) && defined(_DEBUG) // will break a lot currently, won't really be coherent in the future
	UINT len = 512;
	char str[512] = "-";
	UINT cen = 20;
	D3D12_CLEAR_VALUE clearValue;

	if (view.GetD3D12Resource()->GetPrivateData(WKPDID_D3DDebugClearValue, &cen, &clearValue) == S_OK)
	{
		view.GetD3D12Resource()->GetPrivateData(WKPDID_D3DDebugObjectName, &len, str);

		DX12_ASSERT((rgba[0] == clearValue.Color[0]), "...");
		DX12_ASSERT((rgba[1] == clearValue.Color[1]), "...");
		DX12_ASSERT((rgba[2] == clearValue.Color[2]), "...");
		DX12_ASSERT((rgba[3] == clearValue.Color[3]), "...");
	}
#endif

	PendingResourceBarriers();

	m_pCmdList->ClearRenderTargetView(view.GetDescriptorHandle(), rgba, NumRects, pRect);
	m_nCommands += CLCOUNT_CLEAR;
}

void CCommandList::ClearUnorderedAccessView(const CView& view, const UINT rgba[4], UINT NumRects, const D3D12_RECT* pRect)
{
	DX12_ASSERT(INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle(), "View has no descriptor handle, that is not allowed!");

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceUCVUsage(view.GetDX12Resource(), view);

	PendingResourceBarriers();

	m_pCmdList->ClearUnorderedAccessViewUint(GetDevice()->GetUnorderedAccessCacheHeap().GetHandleGPUFromCPU(view.GetDescriptorHandle()), view.GetDescriptorHandle(), view.GetD3D12Resource(), rgba, NumRects, pRect);
	m_nCommands += CLCOUNT_CLEAR;
}

void CCommandList::ClearUnorderedAccessView(const CView& view, const FLOAT rgba[4], UINT NumRects, const D3D12_RECT* pRect)
{
	DX12_ASSERT(INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle(), "View has no descriptor handle, that is not allowed!");

	// TODO: if we know early that the resource(s) will be PRESENT we can begin the barrier early and end it here
	TrackResourceUCVUsage(view.GetDX12Resource(), view);

	PendingResourceBarriers();

	m_pCmdList->ClearUnorderedAccessViewFloat(GetDevice()->GetUnorderedAccessCacheHeap().GetHandleGPUFromCPU(view.GetDescriptorHandle()), view.GetDescriptorHandle(), view.GetD3D12Resource(), rgba, NumRects, pRect);
	m_nCommands += CLCOUNT_CLEAR;
}

void CCommandList::ClearView(const CView& view, const FLOAT rgba[4], UINT NumRects, const D3D12_RECT* pRect)
{
	DX12_ASSERT(INVALID_CPU_DESCRIPTOR_HANDLE != view.GetDescriptorHandle(), "View has no descriptor handle, that is not allowed!");

	switch (view.GetType())
	{
	case EVT_UnorderedAccessView:
		ClearUnorderedAccessView(view, rgba, NumRects, pRect);
		break;

	case EVT_DepthStencilView:   // Optional implementation under DX11.1
		ClearDepthStencilView(view, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, FLOAT(rgba[0]), UINT(rgba[1]), NumRects, pRect);
		break;

	case EVT_RenderTargetView:
		ClearRenderTargetView(view, rgba, NumRects, pRect);
		break;

	case EVT_ShaderResourceView:
	case EVT_ConstantBufferView:
	case EVT_VertexBufferView:
	default:
		DX12_ASSERT(0, "Unsupported resource-type for input!");
		break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
	PendingResourceBarriers();

	m_pCmdList->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
	m_nCommands += CLCOUNT_COPY;
}

void CCommandList::CopyResource(ID3D12Resource* pDstResource, ID3D12Resource* pSrcResource)
{
	PendingResourceBarriers();

	m_pCmdList->CopyResource(pDstResource, pSrcResource);
	m_nCommands += CLCOUNT_COPY;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::CopyResource(CResource& rDstResource, CResource& rSrcResource)
{
	DX12_ASSERT(rSrcResource.GetDesc().Dimension == rDstResource.GetDesc().Dimension, "Can't copy resources of different dimension");

	MaxResourceFenceValue(rDstResource, CMDTYPE_ANY);
	MaxResourceFenceValue(rSrcResource, CMDTYPE_WRITE);

	// TODO: if we know early that the resource(s) will be DEST and SOURCE we can begin the barrier early and end it here
	D3D12_RESOURCE_STATES prevDstState = SetResourceState(rDstResource, D3D12_RESOURCE_STATE_COPY_DEST);        // compatible with D3D12_HEAP_TYPE_READBACK's D3D12_RESOURCE_STATE_COPY_DEST requirement
	D3D12_RESOURCE_STATES prevSrcState = SetResourceState(rSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);      // compatible with D3D12_HEAP_TYPE_UPLOAD's D3D12_RESOURCE_STATE_GENERIC_READ requirement

	PendingResourceBarriers();

	m_pCmdList->CopyResource(rDstResource.GetD3D12Resource(), rSrcResource.GetD3D12Resource());
	m_nCommands += CLCOUNT_COPY;

	/* NOTE: putting it into the previous state can lead to multiple transitions,
	 * this is wasted performance if they are not merged by the driver
	 * EXAMPLE: DRAW1 -> DEPTH_WRITE -> COPYS -> DEPTH_WRITE -> DEPTH_READ -> DRAW2
	 *
	 * BeginResourceStateTransition(rDstResource, prevDstState);
	 * BeginResourceStateTransition(rSrcResource, prevSrcState);
	 */

	SetResourceFenceValue(rDstResource, CMDTYPE_WRITE);
	SetResourceFenceValue(rSrcResource, CMDTYPE_READ);
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::CopySubresources(CResource& rDstResource, UINT dstSubResource, UINT x, UINT y, UINT z, CResource& rSrcResource, UINT srcSubResource, const D3D12_BOX* srcBox, UINT NumSubresources)
{
	MaxResourceFenceValue(rDstResource, CMDTYPE_ANY);
	MaxResourceFenceValue(rSrcResource, CMDTYPE_WRITE);

	// TODO: if we know early that the resource(s) will be DEST and SOURCE we can begin the barrier early and end it here
	D3D12_RESOURCE_STATES prevDstState = SetResourceState(rDstResource, D3D12_RESOURCE_STATE_COPY_DEST);        // compatible with D3D12_HEAP_TYPE_READBACK's D3D12_RESOURCE_STATE_COPY_DEST requirement
	D3D12_RESOURCE_STATES prevSrcState = SetResourceState(rSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);      // compatible with D3D12_HEAP_TYPE_UPLOAD's D3D12_RESOURCE_STATE_GENERIC_READ requirement

	PendingResourceBarriers();

	if (rSrcResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && rDstResource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		UINT64 offset = (srcBox ? srcBox->left : 0);
		UINT64 length = (srcBox ? srcBox->right : rSrcResource.GetDesc().Width) - offset;

		m_pCmdList->CopyBufferRegion(rDstResource.GetD3D12Resource(), x, rSrcResource.GetD3D12Resource(), offset, length);
		m_nCommands += CLCOUNT_COPY;
	}
	else if (rSrcResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && rDstResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		if (NumSubresources <= 1)
		{
			CD3DX12_TEXTURE_COPY_LOCATION src(rSrcResource.GetD3D12Resource(), srcSubResource);
			CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetD3D12Resource(), dstSubResource);

			m_pCmdList->CopyTextureRegion(&dst, x, y, z, &src, srcBox);
			m_nCommands += CLCOUNT_COPY;
		}
		else
		{
			D3D12_RESOURCE_DESC srcDesc = rSrcResource.GetDesc();
			D3D12_RESOURCE_DESC dstDesc = rDstResource.GetDesc();
			D3D12_BOX srcBoxBck = { 0 };
			D3D12_BOX srcRegion = { 0 };
			D3D12_BOX dstRegion = { x, y, z };
			if (srcBox)
			{
				srcBoxBck = *srcBox;
				srcRegion = *srcBox;
				srcBox = &srcRegion;
			}

			// NOTE: too complex case which is not supported as it leads to fe. [slice,mip] sequences like [0,4],[0,5],[0,6],[1,0],[1,1],...
			// which we don't support because the offsets and dimensions are relative to a intermediate mip-level, while crossing the
			// slice-boundary forces us to extrapolate dimensions to larger mips, which is probably not what is wanted in the first place.
			DX12_ASSERT(!srcDesc.MipLevels || !((srcSubResource) % (srcDesc.MipLevels)) || (srcSubResource + NumSubresources <= srcDesc.MipLevels));
			DX12_ASSERT(!dstDesc.MipLevels || !((dstSubResource) % (dstDesc.MipLevels)) || (dstSubResource + NumSubresources <= dstDesc.MipLevels));

			for (UINT n = 0; n < NumSubresources; ++n)
			{
				const UINT srcSlice = (srcSubResource + n) / (srcDesc.MipLevels);
				const UINT dstSlice = (dstSubResource + n) / (dstDesc.MipLevels);
				const UINT srcLevel = (srcSubResource + n) % (srcDesc.MipLevels);
				const UINT dstLevel = (dstSubResource + n) % (dstDesc.MipLevels);

				// reset dimensions/coordinates when crossing slice-boundary
				if (!srcLevel)
					srcRegion = srcBoxBck;
				if (!dstLevel)
					dstRegion = { x, y, z };

				CD3DX12_TEXTURE_COPY_LOCATION src(rSrcResource.GetD3D12Resource(), srcSubResource + n);
				CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetD3D12Resource(), dstSubResource + n);

				m_pCmdList->CopyTextureRegion(&dst, dstRegion.left, dstRegion.top, dstRegion.front, &src, srcBox);
				m_nCommands += CLCOUNT_COPY;

				srcRegion.left   >>= 1;
				srcRegion.top    >>= 1;
				srcRegion.front  >>= 1;

				srcRegion.right  >>= 1; if (srcRegion.right  == srcRegion.left ) srcRegion.right  = srcRegion.left + 1;
				srcRegion.bottom >>= 1; if (srcRegion.bottom == srcRegion.top  ) srcRegion.bottom = srcRegion.top  + 1;
				srcRegion.back   >>= 1; if (srcRegion.back   == srcRegion.front) srcRegion.back   = srcRegion.front+ 1;
				
				dstRegion.left   >>= 1;
				dstRegion.top    >>= 1;
				dstRegion.front  >>= 1;
			}
		}
	}
	else if (rDstResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[1];
		GetDevice()->GetD3D12Device()->GetCopyableFootprints(&rDstResource.GetDesc(), dstSubResource, 1, 0, Layouts, nullptr, nullptr, nullptr);

		CD3DX12_TEXTURE_COPY_LOCATION src(rSrcResource.GetD3D12Resource(), Layouts[0]);
		CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetD3D12Resource(), dstSubResource);

		m_pCmdList->CopyTextureRegion(&dst, x, y, z, &src, srcBox);
		m_nCommands += CLCOUNT_COPY;
	}
	else if (rSrcResource.GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
	{
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[1];
		GetDevice()->GetD3D12Device()->GetCopyableFootprints(&rSrcResource.GetDesc(), srcSubResource, 1, 0, Layouts, nullptr, nullptr, nullptr);

		CD3DX12_TEXTURE_COPY_LOCATION src(rSrcResource.GetD3D12Resource(), srcSubResource);
		CD3DX12_TEXTURE_COPY_LOCATION dst(rDstResource.GetD3D12Resource(), Layouts[0]);

		m_pCmdList->CopyTextureRegion(&dst, x, y, z, &src, srcBox);
		m_nCommands += CLCOUNT_COPY;
	}

	/* NOTE: putting it into the previous state can lead to multiple transitions,
	 * this is wasted performance if they are not merged by the driver
	 * EXAMPLE: DRAW1 -> DEPTH_WRITE -> COPYS -> DEPTH_WRITE -> DEPTH_READ -> DRAW2
	 *
	 * BeginResourceStateTransition(rDstResource, prevDstState);
	 * BeginResourceStateTransition(rSrcResource, prevSrcState);
	 */

	SetResourceFenceValue(rDstResource, CMDTYPE_WRITE);
	SetResourceFenceValue(rSrcResource, CMDTYPE_READ);
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::UpdateSubresourceRegion(CResource& rResource, UINT subResource, const D3D12_BOX* box, const void* data, UINT rowPitch, UINT slicePitch)
{
	MaxResourceFenceValue(rResource, CMDTYPE_ANY);

	// TODO: if we know early that the resource(s) will be DEST we can begin the barrier early and end it here
	D3D12_RESOURCE_STATES prevDstState = SetResourceState(rResource, D3D12_RESOURCE_STATE_COPY_DEST);        // compatible with D3D12_HEAP_TYPE_READBACK's D3D12_RESOURCE_STATE_COPY_DEST requirement

	ID3D12Resource* res12 = rResource.GetD3D12Resource();
	const D3D12_RESOURCE_DESC& desc = rResource.GetDesc();

	// Determine temporary upload buffer size (mostly only pow2-alignment of textures)
	UINT64 uploadBufferSize;
	D3D12_RESOURCE_DESC uploadDesc = desc;

	uploadDesc.Width = box ? box->right - box->left : uploadDesc.Width;
	uploadDesc.Height = box ? box->bottom - box->top : uploadDesc.Height;

	GetDevice()->GetD3D12Device()->GetCopyableFootprints(&uploadDesc, subResource, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

	DX12_ASSERT((desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) || (box == nullptr), "Box used for buffer update, that's not supported!");
	DX12_ASSERT((desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) || (slicePitch != 0), "Slice-pitch is missing for UpdateSubresourceRegion(), this is a required parameter!");
	ID3D12Resource* uploadBuffer;

	D3D12_SUBRESOURCE_DATA subData;
	ZeroMemory(&subData, sizeof(D3D12_SUBRESOURCE_DATA));
	subData.pData = data;
	subData.RowPitch = rowPitch;
	subData.SlicePitch = slicePitch;
	assert(subData.pData != nullptr);

	// NOTE: this is a staging resource, a single instance for all GPUs is valid
	const NODE64& uploadMasks = rResource.GetNodeMasks();
	if (S_OK != GetDevice()->CreateOrReuseCommittedResource(
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

	PendingResourceBarriers();

	::UpdateSubresources<1>(GetDevice()->GetD3D12Device(), m_pCmdList, res12, uploadBuffer, 0, subResource, 1, &subData, box);
	m_nCommands += CLCOUNT_COPY;

	/* NOTE: putting it into the previous state can lead to multiple transitions,
	 * this is wasted performance if they are not merged by the driver
	 * EXAMPLE: DRAW1 -> DEPTH_WRITE -> COPYS -> DEPTH_WRITE -> DEPTH_READ -> DRAW2
	 *
	 * BeginResourceStateTransition(rResource, prevDstState);
	 */

	SetResourceFenceValue(rResource, CMDTYPE_WRITE);

	GetDevice()->ReleaseLater(rResource.GetFenceValues(CMDTYPE_WRITE), uploadBuffer);
	uploadBuffer->Release();
}

void CCommandList::UpdateSubresources(CResource& rResource, D3D12_RESOURCE_STATES eAnnouncedState, UINT64 uploadBufferSize, UINT subResources, D3D12_SUBRESOURCE_DATA* subResourceData)
{
	MaxResourceFenceValue(rResource, CMDTYPE_ANY);

	ID3D12Resource* res12 = rResource.GetD3D12Resource();
	ID3D12Resource* uploadBuffer = NULL;

	// NOTE: this is a staging resource, a single instance for all GPUs is valid
	const NODE64& uploadMasks = rResource.GetNodeMasks();
	if (S_OK != GetDevice()->CreateOrReuseCommittedResource(
	      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, blsi(uploadMasks.creationMask), uploadMasks.creationMask),
	      D3D12_HEAP_FLAG_NONE,
	      &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
	      D3D12_RESOURCE_STATE_GENERIC_READ,
	      nullptr,
	      IID_GFX_ARGS(&uploadBuffer)))
	{
		DX12_ERROR("Could not create intermediate upload buffer!");
	}

	// TODO: if we know early that the resource(s) will be DEST and SOURCE we can begin the barrier early and end it here
	SetResourceState(rResource, D3D12_RESOURCE_STATE_COPY_DEST);        // compatible with D3D12_HEAP_TYPE_READBACK's D3D12_RESOURCE_STATE_COPY_DEST requirement

	PendingResourceBarriers();

	::UpdateSubresources(GetDevice()->GetD3D12Device(), m_pCmdList, res12, uploadBuffer, 0, 0, subResources, subResourceData, nullptr);
	m_nCommands += CLCOUNT_COPY;

	BeginResourceStateTransition(rResource, eAnnouncedState);

	SetResourceFenceValue(rResource, CMDTYPE_WRITE);

	GetDevice()->ReleaseLater(rResource.GetFenceValues(CMDTYPE_WRITE), uploadBuffer);
	uploadBuffer->Release();
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::DiscardResource(CResource& rResource, const D3D12_DISCARD_REGION* pRegion)
{
	MaxResourceFenceValue(rResource, CMDTYPE_ANY);

	m_pCmdList->DiscardResource(rResource.GetD3D12Resource(), pRegion);
	m_nCommands += CLCOUNT_DISCARD;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

D3D12_COMMAND_LIST_TYPE CCommandListPool::m_MapQueueType[CMDQUEUE_NUM] =
{
	D3D12_COMMAND_LIST_TYPE(CMDLIST_TYPE_DIRECT),
	D3D12_COMMAND_LIST_TYPE(CMDLIST_TYPE_COMPUTE),
	D3D12_COMMAND_LIST_TYPE(CMDLIST_TYPE_COPY)
};

//---------------------------------------------------------------------------------------------------------------------
CCommandListPool::CCommandListPool(CDevice* device, CCommandListFenceSet& rCmdFence, int nPoolFenceId)
	: m_pDevice(device)
	, m_rCmdFences(rCmdFence)
	, m_nPoolFenceId(nPoolFenceId)
{
#ifdef DX12_STATS
	m_PeakNumCommandListsAllocated = 0;
	m_PeakNumCommandListsInFlight = 0;
#endif // DX12_STATS

	Configure();
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListPool::~CCommandListPool()
{
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandListPool::Configure()
{
	m_MapQueueType[CMDQUEUE_GRAPHICS] = D3D12_COMMAND_LIST_TYPE(CMDLIST_TYPE_DIRECT); // 0 -> 0
	m_MapQueueType[CMDQUEUE_COMPUTE] = D3D12_COMMAND_LIST_TYPE((CRenderer::CV_r_D3D12HardwareComputeQueue % 2) * 2); // 0 -> 0, 1 -> 2
	m_MapQueueType[CMDQUEUE_COPY] = D3D12_COMMAND_LIST_TYPE((CRenderer::CV_r_D3D12HardwareCopyQueue % 3) + !!(CRenderer::CV_r_D3D12HardwareCopyQueue % 3)); // 0 -> 0, 1 -> 2, 2-> 3
}

bool CCommandListPool::Init(D3D12_COMMAND_LIST_TYPE eType, UINT nodeMask)
{
	if (!m_pCmdQueue)
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};

		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = m_ePoolType = eType;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.NodeMask = m_nodeMask = nodeMask;

		ID3D12CommandQueue* pCmdQueue = NULL;
		if (S_OK != m_pDevice->GetD3D12Device()->CreateCommandQueue(&queueDesc, IID_GFX_ARGS(&pCmdQueue)))
		{
			DX12_ERROR("Could not create command queue");
			return false;
		}

		m_pCmdQueue = pCmdQueue;
		pCmdQueue->Release();
	}

	m_AsyncCommandQueue.Init(this);

#ifdef DX12_STATS
	m_PeakNumCommandListsAllocated = 0;
	m_PeakNumCommandListsInFlight = 0;
#endif // DX12_STATS

	return true;
}

void CCommandListPool::Clear()
{
	// No running command lists of any type allowed:
	// * trigger Live-to-Busy transitions
	// * trigger Busy-to-Free transitions
	while (m_LiveCommandLists.size() || m_BusyCommandLists.size())
		ScheduleCommandLists(); 

	m_FreeCommandLists.clear();
	m_AsyncCommandQueue.Clear();

	m_pCmdQueue = nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandListPool::ScheduleCommandLists()
{
	// Remove finished command-lists from the head of the live-list
	while (m_LiveCommandLists.size())
	{
		DX12_PTR(CCommandList) pCmdList = m_LiveCommandLists.front();

		// free -> complete -> submitted -> finished -> clearing -> free
		if (pCmdList->IsFinishedOnGPU() && !pCmdList->IsClearing())
		{
			m_LiveCommandLists.pop_front();
			m_BusyCommandLists.push_back(pCmdList);

			pCmdList->Clear();
		}
		else
		{
			break;
		}
	}

	// Submit completed but not yet submitted command-lists from the head of the live-list
	for (uint32 t = 0; t < m_LiveCommandLists.size(); ++t)
	{
		DX12_PTR(CCommandList) pCmdList = m_LiveCommandLists[t];

		if (pCmdList->IsScheduled() && !pCmdList->IsSubmitted())
		{
			pCmdList->Submit();
		}

		if (!pCmdList->IsSubmitted())
		{
			break;
		}
	}

	// Remove cleared/deallocated command-lists from the head of the busy-list
	while (m_BusyCommandLists.size())
	{
		DX12_PTR(CCommandList) pCmdList = m_BusyCommandLists.front();

		// free -> complete -> submitted -> finished -> clearing -> free
		if (pCmdList->IsFree())
		{
			m_BusyCommandLists.pop_front();
			m_FreeCommandLists.push_back(pCmdList);
		}
		else
		{
			break;
		}
	}
}

void CCommandListPool::CreateOrReuseCommandList(DX12_PTR(CCommandList)& result)
{
	if (m_FreeCommandLists.empty())
	{
		result = new CCommandList(*this);
	}
	else
	{
		result = m_FreeCommandLists.front();

		m_FreeCommandLists.pop_front();
	}

	result->Register();
	m_LiveCommandLists.push_back(result);

#ifdef DX12_STATS
	m_PeakNumCommandListsAllocated = std::max(m_PeakNumCommandListsAllocated, m_LiveCommandLists.size() + m_BusyCommandLists.size() + m_FreeCommandLists.size());
	m_PeakNumCommandListsInFlight = std::max(m_PeakNumCommandListsInFlight, m_LiveCommandLists.size());
#endif // DX12_STATS
}

void CCommandListPool::AcquireCommandList(DX12_PTR(CCommandList)& result) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);

	ScheduleCommandLists();

	CreateOrReuseCommandList(result);
}

void CCommandListPool::ForfeitCommandList(DX12_PTR(CCommandList)& result, bool bWait) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);
	DX12_PTR(CCommandList) pWaitable = result;

	{
		DX12_ASSERT(result->IsCompleted(), "It's not possible to forfeit an unclosed command list!");
		result->Schedule();
		result = nullptr;
	}

	ScheduleCommandLists();

	if (bWait)
		pWaitable->WaitForFinishOnCPU();
}

void CCommandListPool::AcquireCommandLists(uint32 numCLs, DX12_PTR(CCommandList)* results) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);

	ScheduleCommandLists();

	for (uint32 i = 0; i < numCLs; ++i)
		CreateOrReuseCommandList(results[i]);
}

void CCommandListPool::ForfeitCommandLists(uint32 numCLs, DX12_PTR(CCommandList)* results, bool bWait) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);
	DX12_PTR(CCommandList) pWaitable = results[numCLs - 1];

	int32 i = numCLs;
	while (--i >= 0)
	{
		DX12_ASSERT(results[i]->IsCompleted(), "It's not possible to forfeit an unclosed command list!");
		results[i]->Schedule();
		results[i] = nullptr;
	}

	ScheduleCommandLists();

	if (bWait)
		pWaitable->WaitForFinishOnCPU();
}

}
