// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"
#include "DX12SwapChain.hpp"

namespace NCryDX12
{

class CResource : public CDeviceObject
{
public:
	CResource(CDevice* pDevice);
	virtual ~CResource();

	CResource(CResource&& r);
	CResource& operator=(CResource&& r);

	void       InitDeferred(CCommandList* pCmdList, D3D12_RESOURCE_STATES eAnnouncedState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	bool       Init(ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, const D3D12_RESOURCE_DESC& desc);
	bool       Init(ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState)
	{
		return Init(pResource, eInitialState, pResource->GetDesc());
	}

	std::string GetName() threadsafe_const
	{
		UINT len = 512;
		char str[512] = "-";

		if (m_pD3D12Resource->GetPrivateData(WKPDID_D3DDebugObjectName, &len, str) != S_OK)
		{
			sprintf(str, "%p", this);
		}

		return str;
	}

	// Initialization of contents
	struct SInitialData
	{
		std::vector<D3D12_SUBRESOURCE_DATA> m_SubResourceData;
		UINT64                              m_Size;

		SInitialData()
			: m_Size(0)
		{

		}
	};

	struct SCpuAccessibleData
	{
		struct SubresourceData
		{
			UINT              Subresource;
			UINT64            Size;
			D3D12_MEMCPY_DEST Data;
			bool              IsDirty;
		};

		SubresourceData& GetData(UINT Subresource);
		std::vector<SubresourceData> m_SubResourceData;
	};

	SInitialData*                        GetOrCreateInitialData();
	SCpuAccessibleData::SubresourceData& GetOrCreateCpuAccessibleData(UINT Subresource, bool bAllocate);
	void                                 DiscardCpuAccessibleData(UINT Subresource);

	ILINE bool                           InitHasBeenDeferred() const
	{
		return !!m_InitialData;
	}
	ILINE void ClearDeferredInit()
	{
		DiscardInitialData();
	}

	// Swap-chain associativity
	ILINE void SetDX12SwapChain(CSwapChain* pOwner)
	{
		m_pSwapChainOwner = pOwner;
	}
	ILINE CSwapChain* GetDX12SwapChain() const
	{
		return m_pSwapChainOwner;
	}
	ILINE bool IsBackBuffer() const
	{
		return m_pSwapChainOwner != NULL;
	}
	ILINE void VerifyBackBuffer() const
	{
		DX12_ASSERT((!IsBackBuffer() || !GetDX12SwapChain()->IsPresentScheduled()), "Flush didn't dry out all outstanding Present() calls!");
		DX12_ASSERT((!IsBackBuffer() || GetD3D12Resource() == GetDX12SwapChain()->GetCurrentBackBuffer().GetD3D12Resource()), "Resource is referring to old swapchain index!");
	}

	// Utility functions
	ILINE bool IsCompressed() const
	{
		return m_bCompressed;
	}
	ILINE bool IsPersistentMappable() const
	{
		return m_HeapType != D3D12_HEAP_TYPE_READBACK;
	}
	ILINE bool IsOffCard() const
	{
		return m_HeapType == D3D12_HEAP_TYPE_READBACK || m_HeapType == D3D12_HEAP_TYPE_UPLOAD;
	}
	ILINE bool IsTarget() const
	{
		return !!(GetMergedState() & (D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}
	ILINE bool IsGeneric() const
	{
		return !!(GetMergedState() & (D3D12_RESOURCE_STATE_UNORDERED_ACCESS | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}
	ILINE bool IsGraphics() const
	{
		return !!(GetMergedState() & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_STREAM_OUT));
	}

	ILINE D3D12_RESOURCE_DESC& GetDesc()
	{
		return m_Desc;
	}
	ILINE const D3D12_RESOURCE_DESC& GetDesc() const
	{
		return m_Desc;
	}
	ILINE uint32 GetPlaneCount() const
	{
		return m_PlaneCount;
	}
	ILINE UINT64 GetSize(UINT FirstSubresource, UINT NumSubresources) const
	{
		UINT64 Size;
		GetDevice()->GetD3D12Device()->GetCopyableFootprints(&m_Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &Size);
		return Size;
	}
	ILINE const NODE64& GetNodeMasks() const
	{
		return m_NodeMasks;
	}

	ILINE ID3D12Resource* GetD3D12UploadResource() const
	{
		return m_pD3D12UploadBuffer;
	}
	ILINE void SetD3D12UploadResource(ID3D12Resource* pResource)
	{
		m_pD3D12UploadBuffer = pResource;
	}

	ILINE ID3D12Resource* GetD3D12DownloadResource() const
	{
		return m_pD3D12DownloadBuffer;
	}
	ILINE void SetD3D12DownloadResource(ID3D12Resource* pResource)
	{
		m_pD3D12DownloadBuffer = pResource;
	}

	ILINE ID3D12Resource* GetD3D12Resource() const
	{
		return m_pD3D12Resource;
	}
	ILINE D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
	{
		return m_GPUVirtualAddress;
	}

	// Overrides current resource barrier state (be careful with this!)
	ILINE void SetState(D3D12_RESOURCE_STATES state)
	{
		m_eCurrentState = state;
	}
	// Get current known resource barrier state
	ILINE D3D12_RESOURCE_STATES GetState() const
	{
		return m_eCurrentState;
	}

	// Overrides announced resource barrier state (be careful with this!)
	ILINE void SetAnnouncedState(D3D12_RESOURCE_STATES state)
	{
		m_eAnnouncedState = state;
	}
	// Get announced resource barrier state
	ILINE D3D12_RESOURCE_STATES GetAnnouncedState() const
	{
		return m_eAnnouncedState;
	}

	ILINE D3D12_RESOURCE_STATES GetMergedState() const
	{
		return D3D12_RESOURCE_STATES(m_eCurrentState | (m_eAnnouncedState != (D3D12_RESOURCE_STATES)-1 ? m_eAnnouncedState : 0));
	}

	ILINE D3D12_RESOURCE_STATES GetTargetState() const
	{
		return D3D12_RESOURCE_STATES(m_eAnnouncedState != (D3D12_RESOURCE_STATES)-1 ? m_eAnnouncedState : m_eCurrentState);
	}
	
	template<const bool bCheckCVar = true>
	ILINE bool IsConcurrentWritable() const
	{
		return m_bConcurrentWritable & !(bCheckCVar && !CRenderer::CV_r_D3D12AsynchronousCompute);
	}
	ILINE void MakeConcurrentWritable(bool bCW)
	{
		m_bConcurrentWritable = bCW;
	}

	ILINE bool IsReusableResource() const
	{
		return m_bReusableResource;
	}
	ILINE void MakeReusableResource(bool bRR)
	{
		m_bReusableResource = bRR;
	}

	// Transition resource to desired state
	bool                  NeedsTransitionBarrier(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState, bool bPrepare = false) const;
	bool                  NeedsTransitionBarrier(CCommandList* pCmdList, const CView& view, D3D12_RESOURCE_STATES desiredState, bool bPrepare = false) const;
	D3D12_RESOURCE_STATES DecayTransitionBarrier(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES TransitionBarrier(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES TransitionBarrierStatic(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState, D3D12_RESOURCE_STATES& eCurrentState) const;
	D3D12_RESOURCE_STATES TransitionBarrier(CCommandList* pCmdList, const CView& view, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES BeginTransitionBarrier(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES BeginTransitionBarrier(CCommandList* pCmdList, const CView& view, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES EndTransitionBarrier(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredState);
	D3D12_RESOURCE_STATES EndTransitionBarrier(CCommandList* pCmdList, const CView& view, D3D12_RESOURCE_STATES desiredState);

	int                   SelectQueueForTransitionBarrier(int altQueue, int desiredQueue, D3D12_RESOURCE_STATES desiredState) const;

	ILINE bool            HasSubresourceTransitionBarriers() const
	{
		return m_eCurrentState == D3D12_RESOURCE_STATES(-1);
	}

	void   EnableSubresourceTransitionBarriers();
	void   DisableSubresourceTransitionBarriers(CCommandList* pCmdList, D3D12_RESOURCE_STATES desiredCommonState);

	UINT64 SetFenceValue(UINT64 fenceValue, const int id, const int type) threadsafe
	{
		// Check submitted completed fence
		UINT64 utilizedValue = fenceValue;
		UINT64 previousValue = m_FenceValues[type][id];

	#define DX12_FREETHREADED_RESOURCES
	#ifndef DX12_FREETHREADED_RESOURCES
		m_FenceValues[type][id] = (previousValue > fenceValue ? previousValue : fenceValue);

		if (type == CMDTYPE_ANY)
		{
			m_FenceValues[CMDTYPE_READ][id] =
			  m_FenceValues[CMDTYPE_WRITE][id] =
			    m_FenceValues[CMDTYPE_ANY][id];
		}
		else
		{
			m_FenceValues[CMDTYPE_ANY][id] = std::max(
			  m_FenceValues[CMDTYPE_READ][id],
			  m_FenceValues[CMDTYPE_WRITE][id]);
		}
	#else
		// CLs may submit in any order. Is it higher than last known completed fence? If so, update it!
		MaxFenceValue(m_FenceValues[type][id], fenceValue);

		if (type == CMDTYPE_ANY)
		{
			MaxFenceValue(m_FenceValues[CMDTYPE_READ][id], m_FenceValues[CMDTYPE_ANY][id]);
			MaxFenceValue(m_FenceValues[CMDTYPE_WRITE][id], m_FenceValues[CMDTYPE_ANY][id]);
		}
		else
		{
			MaxFenceValue(m_FenceValues[CMDTYPE_ANY][id], std::max(m_FenceValues[CMDTYPE_READ][id], m_FenceValues[CMDTYPE_WRITE][id]));
		}
	#endif

		return previousValue;
	}

	ILINE UINT64 GetFenceValue(const int id, const int type) threadsafe_const
	{
		return m_FenceValues[type][id];
	}

	ILINE const FVAL64 (&GetFenceValues(const int type) threadsafe_const)[CMDQUEUE_NUM]
	{
		return m_FenceValues[type];
	}

	// Mask fence-values of the current command-list (fenceValueClamp), to only collect the fences from _before_
	// If a resource is used twice in a CL the fences could otherwise include the current CLs fence-value
	ILINE void MaxFenceValues(UINT64 (&RESTRICT_REFERENCE rFenceValues)[CMDTYPE_NUM][CMDQUEUE_NUM], const UINT64 fenceValueClamp, const int nPoolFenceId, const int type, const FVAL64 (&RESTRICT_REFERENCE rCompletedFenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		// Mask in-order queues, preventing a Wait()-command being generated
		const bool bMasked = CCommandListPool::IsInOrder(nPoolFenceId);

		// The pool which waits for the fence can be omitted (in-order-execution)
		UINT64 fenceValuesMasked[CMDQUEUE_NUM];

		if ((type == CMDTYPE_READ) || (type == CMDTYPE_ANY))
		{
			fenceValuesMasked[CMDQUEUE_GRAPHICS] = m_FenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS];
			fenceValuesMasked[CMDQUEUE_COMPUTE] = m_FenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE];
			fenceValuesMasked[CMDQUEUE_COPY] = m_FenceValues[CMDTYPE_READ][CMDQUEUE_COPY];
			fenceValuesMasked[nPoolFenceId] = (!bMasked && (fenceValuesMasked[nPoolFenceId] < fenceValueClamp) ? fenceValuesMasked[nPoolFenceId] : 0);

			std::upr(rFenceValues[CMDTYPE_READ], fenceValuesMasked);
		}

		if ((type == CMDTYPE_WRITE) || (type == CMDTYPE_ANY))
		{
			fenceValuesMasked[CMDQUEUE_GRAPHICS] = m_FenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS];
			fenceValuesMasked[CMDQUEUE_COMPUTE] = m_FenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE];
			fenceValuesMasked[CMDQUEUE_COPY] = m_FenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY];
			fenceValuesMasked[nPoolFenceId] = (!bMasked && (fenceValuesMasked[nPoolFenceId] < fenceValueClamp) ? fenceValuesMasked[nPoolFenceId] : 0);

			std::upr(rFenceValues[CMDTYPE_WRITE], fenceValuesMasked);
		}

	#if DX12_CONCURRENCY_ANALYZER
		bool bFoundDependency =
		  (((type == CMDTYPE_READ) || (type == CMDTYPE_ANY)) && (
		     ((rFenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS] == fenceValuesMasked[CMDQUEUE_GRAPHICS]) && (fenceValuesMasked[CMDQUEUE_GRAPHICS] > rCompletedFenceValues[CMDQUEUE_GRAPHICS])) ||
		     ((rFenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE] == fenceValuesMasked[CMDQUEUE_COMPUTE]) && (fenceValuesMasked[CMDQUEUE_COMPUTE] > rCompletedFenceValues[CMDQUEUE_COMPUTE])) ||
		     ((rFenceValues[CMDTYPE_READ][CMDQUEUE_COPY] == fenceValuesMasked[CMDQUEUE_COPY]) && (fenceValuesMasked[CMDQUEUE_COPY] > rCompletedFenceValues[CMDQUEUE_COPY]))
		     ))
		  ||
		  (((type == CMDTYPE_WRITE) || (type == CMDTYPE_ANY)) && (
		     ((rFenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS] == fenceValuesMasked[CMDQUEUE_GRAPHICS]) && (fenceValuesMasked[CMDQUEUE_GRAPHICS] > rCompletedFenceValues[CMDQUEUE_GRAPHICS])) ||
		     ((rFenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE] == fenceValuesMasked[CMDQUEUE_COMPUTE]) && (fenceValuesMasked[CMDQUEUE_COMPUTE] > rCompletedFenceValues[CMDQUEUE_COMPUTE])) ||
		     ((rFenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY] == fenceValuesMasked[CMDQUEUE_COPY]) && (fenceValuesMasked[CMDQUEUE_COPY] > rCompletedFenceValues[CMDQUEUE_COPY]))
		     ));

		if (bFoundDependency)
		{
			DX12_LOG(bFoundDependency, "Resource %s (ID3D12: %p) blocks on %s because of %s: [%c%d,%c%d,%c%d]", GetName().c_str(), m_pD3D12Resource.get(),
			         nPoolFenceId == CMDQUEUE_GRAPHICS ? "gfx" : nPoolFenceId == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			         type == CMDTYPE_READ ? "READ" : type == CMDTYPE_WRITE ? "WRITE" : "R/W",
			         (fenceValuesMasked[CMDQUEUE_GRAPHICS] > rCompletedFenceValues[CMDQUEUE_GRAPHICS]) ? '!' : '~', fenceValuesMasked[CMDQUEUE_GRAPHICS],
			         (fenceValuesMasked[CMDQUEUE_COMPUTE] > rCompletedFenceValues[CMDQUEUE_COMPUTE]) ? '!' : '~', fenceValuesMasked[CMDQUEUE_COMPUTE],
			         (fenceValuesMasked[CMDQUEUE_COPY] > rCompletedFenceValues[CMDQUEUE_COPY]) ? '!' : '~', fenceValuesMasked[CMDQUEUE_COPY]);
		}
	#endif
	}

	template<const bool bPerspGPU>
	ILINE bool IsUsed(CCommandListPool& pCmdListPool, const int type = CMDTYPE_ANY) threadsafe_const
	{
		return !pCmdListPool.IsCompleted<FVAL64, bPerspGPU>(m_FenceValues[type]);
	}

	template<const bool bBlockCPU>
	ILINE void WaitForUnused(CCommandListPool& pCmdListPool, const int type = CMDTYPE_ANY) threadsafe
	{
		std::integral_constant<bool, bBlockCPU> tag;
		WaitForUnusedImpl(pCmdListPool, type, tag);
	}

	ILINE void WaitForUnusedImpl(CCommandListPool& pCmdListPool, const int type, std::integral_constant<bool, false> tag) threadsafe
	{
		pCmdListPool.WaitForFenceOnGPU(m_FenceValues[type]);
	}

	ILINE void WaitForUnusedImpl(CCommandListPool& pCmdListPool, const int type, std::integral_constant<bool, true> tag) threadsafe
	{
		pCmdListPool.WaitForFenceOnCPU(m_FenceValues[type]);
	}

	bool SubstituteUsed();

	HRESULT MappedWriteToSubresource(UINT Subresource, const D3D12_RANGE* Range, const void* pInData);
	HRESULT MappedReadFromSubresource(UINT Subresource, const D3D12_RANGE* Range, void* pOutData);

protected:
	void DiscardInitialData();

	// Never changes after construction
	D3D12_RESOURCE_DESC m_Desc;
	D3D12_HEAP_TYPE m_HeapType;
	D3D12_GPU_VIRTUAL_ADDRESS m_GPUVirtualAddress;
	DX12_PTR(ID3D12Resource) m_pD3D12Resource;      // can be replaced by MAP_DISCARD, which is deprecated
	DX12_PTR(ID3D12Resource) m_pD3D12UploadBuffer;
	DX12_PTR(ID3D12Resource) m_pD3D12DownloadBuffer;

	CSwapChain* m_pSwapChainOwner;
	NODE64 m_NodeMasks;
	bool m_bCompressed;
	uint8 m_PlaneCount;
	bool m_bConcurrentWritable;
	bool m_bReusableResource;

	// Potentially changes on every resource-use
	D3D12_RESOURCE_STATES m_eInitialState;
	D3D12_RESOURCE_STATES m_eCurrentState;
	D3D12_RESOURCE_STATES m_eAnnouncedState;
	std::vector<D3D12_RESOURCE_STATES> m_SubresourceStates;
	mutable FVAL64 m_FenceValues[CMDTYPE_NUM][CMDQUEUE_NUM];

	// Used when using the resource the first time
	SInitialData* m_InitialData;
	// Used when data is transfered to/from the CPU
	SCpuAccessibleData* m_CpuAccessibleData;
};

}
