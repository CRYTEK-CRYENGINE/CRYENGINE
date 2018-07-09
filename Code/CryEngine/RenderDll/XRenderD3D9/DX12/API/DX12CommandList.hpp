// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"
#include "DX12CommandListFence.hpp"
#include "DX12PSO.hpp"
#include "DX12DescriptorHeap.hpp"
#include "DX12QueryHeap.hpp"
#include "DX12View.hpp"
#include "DX12SamplerState.hpp"
#include "DX12AsyncCommandQueue.hpp"

#define CMDLIST_TYPE_DIRECT   0 // D3D12_COMMAND_LIST_TYPE_DIRECT
#define CMDLIST_TYPE_BUNDLE   1 // D3D12_COMMAND_LIST_TYPE_BUNDLE
#define CMDLIST_TYPE_COMPUTE  2 // D3D12_COMMAND_LIST_TYPE_COMPUTE
#define CMDLIST_TYPE_COPY     3 // D3D12_COMMAND_LIST_TYPE_COPY

#define CMDLIST_DEFAULT       CMDLIST_TYPE_DIRECT
#define CMDLIST_GRAPHICS      NCryDX12::CCommandListPool::m_MapQueueType[CMDQUEUE_GRAPHICS]
#define CMDLIST_COMPUTE       NCryDX12::CCommandListPool::m_MapQueueType[CMDQUEUE_COMPUTE]
#define CMDLIST_COPY          NCryDX12::CCommandListPool::m_MapQueueType[CMDQUEUE_COPY]

#define CMDQUEUE_GRAPHICS_IOE true  // graphics queue will terminate in-order
#define CMDQUEUE_COMPUTE_IOE  false // compute queue can terminate out-of-order
#define CMDQUEUE_COPY_IOE     false // copy queue can terminate out-of-order

#define DX12_BARRIER_FUSION   true

namespace NCryDX12
{

class CCommandListPool;
class CCommandList;
class CSwapChain;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CCommandListPool
{
public:
	CCommandListPool(CDevice* device, CCommandListFenceSet& rCmdFence, int nPoolFenceId);
	~CCommandListPool();

	void Configure();
	bool Init(D3D12_COMMAND_LIST_TYPE eType = D3D12_COMMAND_LIST_TYPE(CMDLIST_DEFAULT), UINT nodeMask = 0);
	void Clear();

	void AcquireCommandList(DX12_PTR(CCommandList) & result) threadsafe;
	void ForfeitCommandList(DX12_PTR(CCommandList) & result, bool bWait = false) threadsafe;
	void AcquireCommandLists(uint32 numCLs, DX12_PTR(CCommandList) * results) threadsafe;
	void ForfeitCommandLists(uint32 numCLs, DX12_PTR(CCommandList) * results, bool bWait = false) threadsafe;

	ILINE ID3D12CommandQueue* GetD3D12CommandQueue() threadsafe_const
	{
		return m_pCmdQueue;
	}

	ILINE D3D12_COMMAND_LIST_TYPE GetD3D12QueueType() threadsafe_const
	{
		return m_ePoolType;
	}

	ILINE NCryDX12::CAsyncCommandQueue& GetAsyncCommandQueue() threadsafe
	{
		return m_AsyncCommandQueue;
	}

	ILINE CDevice* GetDevice() threadsafe_const
	{
		return m_pDevice;
	}

	ILINE UINT GetNodeMask() threadsafe_const
	{
		return m_nodeMask;
	}

	ILINE CCommandListFenceSet& GetFences() threadsafe
	{
		return m_rCmdFences;
	}

	ILINE ID3D12Fence** GetD3D12Fences() threadsafe_const
	{
		return m_rCmdFences.GetD3D12Fences();
	}

	ILINE ID3D12Fence* GetD3D12Fence() threadsafe_const
	{
		return m_rCmdFences.GetD3D12Fence(m_nPoolFenceId);
	}

	ILINE int GetFenceID() threadsafe_const
	{
		return m_nPoolFenceId;
	}

	ILINE void SetSignalledFenceValue(const UINT64 fenceValue) threadsafe
	{
		return m_rCmdFences.SetSignalledValue(fenceValue, m_nPoolFenceId);
	}

	ILINE UINT64 GetSignalledFenceValue() threadsafe
	{
		return m_rCmdFences.GetSignalledValue(m_nPoolFenceId);
	}

	ILINE void SetSubmittedFenceValue(const UINT64 fenceValue) threadsafe
	{
		return m_rCmdFences.SetSubmittedValue(fenceValue, m_nPoolFenceId);
	}

	ILINE UINT64 GetSubmittedFenceValue() threadsafe_const
	{
		return m_rCmdFences.GetSubmittedValue(m_nPoolFenceId);
	}

	ILINE void SetCurrentFenceValue(const UINT64 fenceValue) threadsafe
	{
		return m_rCmdFences.SetCurrentValue(fenceValue, m_nPoolFenceId);
	}

	ILINE UINT64 GetCurrentFenceValue() threadsafe_const
	{
		return m_rCmdFences.GetCurrentValue(m_nPoolFenceId);
	}

public:
	ILINE UINT64 GetLastCompletedFenceValue() threadsafe_const
	{
		return m_rCmdFences.GetLastCompletedFenceValue(m_nPoolFenceId);
	}

	static bool IsInOrder(int nPoolFenceId)
	{
		return
		  (nPoolFenceId == CMDQUEUE_GRAPHICS && CMDQUEUE_GRAPHICS_IOE) ||
		  (nPoolFenceId == CMDQUEUE_COMPUTE && CMDQUEUE_COMPUTE_IOE) ||
		  (nPoolFenceId == CMDQUEUE_COPY && CMDQUEUE_COPY_IOE);
	}

	ILINE bool IsInOrder() const
	{
		return IsInOrder(m_nPoolFenceId);
	}

	#define PERSP_CPU false
	#define PERSP_GPU true
	template<class T64>
	ILINE void WaitForFenceOnGPU(const T64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe
	{
		// Mask in-order queues, preventing a Wait()-command being generated
		const bool bMasked = PERSP_GPU && IsInOrder();

		// The pool which waits for the fence can be omitted (in-order-execution)
		UINT64 fenceValuesMasked[CMDQUEUE_NUM];

		fenceValuesMasked[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
		fenceValuesMasked[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
		fenceValuesMasked[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];
		fenceValuesMasked[m_nPoolFenceId] = (!bMasked ? fenceValuesMasked[m_nPoolFenceId] : 0);

		if (!m_rCmdFences.IsCompleted(fenceValuesMasked))
		{
			DX12_LOG(DX12_CONCURRENCY_ANALYZER || DX12_FENCE_ANALYZER,
			         "Waiting GPU for fences %s: [%lld,%lld,%lld] (is [%lld,%lld,%lld] currently)",
			         m_nPoolFenceId == CMDQUEUE_GRAPHICS ? "gfx" : m_nPoolFenceId == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			         UINT64(fenceValues[CMDQUEUE_GRAPHICS]),
			         UINT64(fenceValues[CMDQUEUE_COMPUTE]),
			         UINT64(fenceValues[CMDQUEUE_COPY]),
			         m_rCmdFences.GetD3D12Fence(CMDQUEUE_GRAPHICS)->GetCompletedValue(),
			         m_rCmdFences.GetD3D12Fence(CMDQUEUE_COMPUTE)->GetCompletedValue(),
			         m_rCmdFences.GetD3D12Fence(CMDQUEUE_COPY)->GetCompletedValue());

	#ifdef DX12_STATS
			m_NumWaitsGPU += 2;
	#endif // DX12_STATS

			DX12_ASSERT(fenceValuesMasked[CMDQUEUE_GRAPHICS] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_GRAPHICS), "Waiting for unsubmitted gfx CL, deadlock imminent!");
			DX12_ASSERT(fenceValuesMasked[CMDQUEUE_COMPUTE] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COMPUTE), "Waiting for unsubmitted cmp CL, deadlock imminent!");
			DX12_ASSERT(fenceValuesMasked[CMDQUEUE_COPY] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COPY), "Waiting for unsubmitted cpy CL, deadlock imminent!");

			m_AsyncCommandQueue.Wait(m_rCmdFences.GetD3D12Fences(), fenceValuesMasked);
		}
	}

	ILINE void WaitForFenceOnGPU(const UINT64 fenceValue, const int id) threadsafe
	{
		if (!m_rCmdFences.IsCompleted(fenceValue, id))
		{
			DX12_LOG(DX12_CONCURRENCY_ANALYZER || DX12_FENCE_ANALYZER,
			         "Waiting GPU for fence %s: %lld (is %lld currently)",
			         id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			         fenceValue,
			         m_rCmdFences.GetD3D12Fence(id)->GetCompletedValue());

	#ifdef DX12_STATS
			m_NumWaitsGPU++;
	#endif // DX12_STATS

			DX12_ASSERT(fenceValue <= m_rCmdFences.GetSubmittedValue(id), "Waiting for unsubmitted CL, deadlock imminent!");

			m_AsyncCommandQueue.Wait(m_rCmdFences.GetD3D12Fence(id), fenceValue);
		}
	}

	ILINE void WaitForFenceOnGPU(const UINT64 fenceValue) threadsafe
	{
		return WaitForFenceOnGPU(fenceValue, m_nPoolFenceId);
	}

	template<class T64>
	ILINE void WaitForFenceOnCPU(const T64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		UINT64 fenceValuesCopied[CMDQUEUE_NUM];

		fenceValuesCopied[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
		fenceValuesCopied[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
		fenceValuesCopied[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];

		if (!m_rCmdFences.IsCompleted(fenceValuesCopied))
		{
	#ifdef DX12_STATS
			m_NumWaitsCPU += 2;
	#endif // DX12_STATS

			DX12_ASSERT(fenceValuesCopied[CMDQUEUE_GRAPHICS] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_GRAPHICS), "Waiting for unsubmitted gfx CL, deadlock imminent!");
			DX12_ASSERT(fenceValuesCopied[CMDQUEUE_COMPUTE] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COMPUTE), "Waiting for unsubmitted cmp CL, deadlock imminent!");
			DX12_ASSERT(fenceValuesCopied[CMDQUEUE_COPY] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COPY), "Waiting for unsubmitted cpy CL, deadlock imminent!");

			m_rCmdFences.WaitForFence(fenceValuesCopied);
		}
	}

	ILINE void WaitForFenceOnCPU(const UINT64 fenceValue, const int id) threadsafe_const
	{
		if (!m_rCmdFences.IsCompleted(fenceValue, id))
		{
	#ifdef DX12_STATS
			m_NumWaitsCPU++;
	#endif // DX12_STATS

			DX12_ASSERT(fenceValue <= m_rCmdFences.GetSubmittedValue(id), "Waiting for unsubmitted CL, deadlock imminent!");

			m_rCmdFences.WaitForFence(fenceValue, id);
		}
	}

	ILINE void WaitForFenceOnCPU(const UINT64 fenceValue) threadsafe_const
	{
		return WaitForFenceOnCPU(fenceValue, m_nPoolFenceId);
	}

	ILINE void WaitForFenceOnCPU() threadsafe_const
	{
		return WaitForFenceOnCPU(m_rCmdFences.GetSubmittedValues());
	}

	ILINE void SignalFenceOnGPU(const UINT64 fenceValue)
	{
		GetAsyncCommandQueue().Signal(GetD3D12Fence(), fenceValue);
	}

	ILINE void SignalFenceOnCPU(const UINT64 fenceValue)
	{
		GetD3D12Fence()->Signal(fenceValue);
	}

	template<class T64, const bool bPerspective>
	ILINE bool IsCompleted(const T64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
	{
		// Mask in-order queues, preventing a Wait()-command being generated
		const bool bMasked = bPerspective && IsInOrder();

		// The pool which checks for the fence can be omitted (in-order-execution)
		UINT64 fenceValuesMasked[CMDQUEUE_NUM];

		fenceValuesMasked[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
		fenceValuesMasked[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
		fenceValuesMasked[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];
		fenceValuesMasked[m_nPoolFenceId] = (!bMasked ? fenceValuesMasked[m_nPoolFenceId] : 0);

		return m_rCmdFences.IsCompleted(fenceValuesMasked);
	}

	ILINE bool IsCompletedPerspCPU(const UINT64 fenceValue, const int id) threadsafe_const
	{
		return m_rCmdFences.IsCompleted(fenceValue, id);
	}

	ILINE bool IsCompletedPerspCPU(const UINT64 fenceValue) threadsafe_const
	{
		return IsCompletedPerspCPU(fenceValue, m_nPoolFenceId);
	}

private:
	void ScheduleCommandLists();
	void CreateOrReuseCommandList(DX12_PTR(CCommandList) & result);

	CDevice*                m_pDevice;
	CCommandListFenceSet&   m_rCmdFences;
	int                     m_nPoolFenceId;
	D3D12_COMMAND_LIST_TYPE m_ePoolType;

	typedef std::deque<DX12_PTR(CCommandList)> TCommandLists;
	TCommandLists m_LiveCommandLists;
	TCommandLists m_BusyCommandLists;
	TCommandLists m_FreeCommandLists;

	DX12_PTR(ID3D12CommandQueue) m_pCmdQueue;
	NCryDX12::CAsyncCommandQueue m_AsyncCommandQueue;

	UINT                         m_nodeMask;

	#ifdef DX12_STATS
	size_t m_PeakNumCommandListsAllocated;
	size_t m_PeakNumCommandListsInFlight;
	#endif // DX12_STATS

public:
	static D3D12_COMMAND_LIST_TYPE m_MapQueueType[CMDQUEUE_NUM];
};

class CCommandList : public CDeviceObject
{
	friend class CCommandListPool;

public:
	virtual ~CCommandList();

	bool Init(UINT64 currentFenceValue);
	bool Reset();

	ILINE ID3D12GraphicsCommandList* GetD3D12CommandList() threadsafe_const
	{
		return m_pCmdList;
	}
	ILINE D3D12_COMMAND_LIST_TYPE GetD3D12ListType() threadsafe_const
	{
		return m_eListType;
	}
	ILINE ID3D12CommandAllocator* GetD3D12CommandAllocator() threadsafe_const
	{
		return m_pCmdAllocator;
	}
	ILINE ID3D12CommandQueue* GetD3D12CommandQueue() threadsafe_const
	{
		return m_pCmdQueue;
	}
	ILINE CCommandListPool& GetCommandListPool() threadsafe_const
	{
		return m_rPool;
	}

	//---------------------------------------------------------------------------------------------------------------------
	// Stage 1
	ILINE bool IsFree() const
	{
		return m_eState == CLSTATE_FREE;
	}

	// Stage 2
	void       Begin();
	ILINE bool IsUtilized() const
	{
	#define CLCOUNT_DRAW     1
	#define CLCOUNT_DISPATCH 1
	#define CLCOUNT_COPY     1
	#define CLCOUNT_CLEAR    1
	#define CLCOUNT_DISCARD  1
	#define CLCOUNT_BARRIER  1
	#define CLCOUNT_QUERY    1
	#define CLCOUNT_RESOLVE  1
	#define CLCOUNT_SETIO    0
	#define CLCOUNT_SETSTATE 0

		return m_nCommands > 0;
	}

	void       End();
	ILINE bool IsCompleted() const
	{
		return m_eState >= CLSTATE_COMPLETED;
	}

	void       Register();
	void       Schedule();
	ILINE bool IsScheduled() const
	{
		return m_eState == CLSTATE_SCHEDULED;
	}

	// Stage 3
	UINT64 SignalFenceOnGPU()
	{
		DX12_LOG(DX12_CONCURRENCY_ANALYZER || DX12_FENCE_ANALYZER, "Signaling GPU fence %s: %lld", m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy", m_CurrentFenceValue);
		m_rPool.SignalFenceOnGPU(m_CurrentFenceValue);
		return m_CurrentFenceValue;
	}

	UINT64 SignalFenceOnCPU()
	{
		DX12_LOG(DX12_CONCURRENCY_ANALYZER || DX12_FENCE_ANALYZER, "Signaling CPU fence %s: %lld", m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy", m_CurrentFenceValue);
		m_rPool.SignalFenceOnCPU(m_CurrentFenceValue);
		return m_CurrentFenceValue;
	}

	void       Submit();
	ILINE bool IsSubmitted() const
	{
		return m_eState >= CLSTATE_SUBMITTED;
	}

	// Stage 4
	void WaitForFinishOnGPU()
	{
		DX12_ASSERT(m_eState == CLSTATE_SUBMITTED, "GPU fence waits for itself to be complete: deadlock imminent!");
		m_rPool.WaitForFenceOnGPU(m_CurrentFenceValue);
	}

	void WaitForFinishOnCPU() const
	{
		DX12_ASSERT(m_eState == CLSTATE_SUBMITTED, "CPU fence waits for itself to be complete: deadlock imminent!");
		m_rPool.WaitForFenceOnCPU(m_CurrentFenceValue);
	}

	ILINE bool IsFinishedOnGPU() const
	{
		if ((m_eState == CLSTATE_SUBMITTED) && m_rPool.IsCompletedPerspCPU(m_CurrentFenceValue))
		{
			m_eState = CLSTATE_FINISHED;
		}

		return m_eState == CLSTATE_FINISHED;
	}

	ILINE UINT64 GetCurrentFenceValue() const
	{
		return m_CurrentFenceValue;
	}

	// Stage 5
	void       Clear();
	ILINE bool IsClearing() const
	{
		return m_eState == CLSTATE_CLEARING;
	}

	//---------------------------------------------------------------------------------------------------------------------
	void ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);
	void PendingResourceBarriers();

	// Transition resource to desired state, returns the previous state
	template<class T>
	ILINE D3D12_RESOURCE_STATES SetResourceState(T& pResource, D3D12_RESOURCE_STATES desiredState, UINT sub = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
	{
		return pResource.TransitionBarrier(this, desiredState);
	}
	template<class T>
	ILINE D3D12_RESOURCE_STATES SetResourceState(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState)
	{
		return pResource.TransitionBarrier(this, view, desiredState);
	}
	template<class T>
	ILINE D3D12_RESOURCE_STATES BeginResourceStateTransition(T& pResource, D3D12_RESOURCE_STATES desiredState)
	{
		return pResource.BeginTransitionBarrier(this, desiredState);
	}
	template<class T>
	ILINE D3D12_RESOURCE_STATES BeginResourceStateTransition(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState)
	{
		return pResource.BeginTransitionBarrier(this, view, desiredState);
	}
	template<class T>
	ILINE D3D12_RESOURCE_STATES EndResourceStateTransition(T& pResource, D3D12_RESOURCE_STATES desiredState)
	{
		return pResource.EndTransitionBarrier(this, desiredState);
	}
	template<class T>
	ILINE D3D12_RESOURCE_STATES EndResourceStateTransition(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState)
	{
		return pResource.EndTransitionBarrier(this, view, desiredState);
	}
	// Collect the highest fenceValues of all given Resource to wait before the command-list is finally submitted
	template<class T>
	ILINE void MaxResourceFenceValue(const T& pResource, const int type)
	{
		pResource.MaxFenceValues(m_UsedFenceValues, m_CurrentFenceValue, m_rPool.GetFenceID(), type, m_rPool.GetFences().GetLastCompletedFenceValues());
	}

	// Mark resource with the current fence, returns the previous fence
	template<class T>
	ILINE UINT64 SetResourceFenceValue(T& pResource, const int type) const
	{
		return pResource.SetFenceValue(m_CurrentFenceValue, m_rPool.GetFenceID(), type);
	}

	template<class T>
	ILINE void TrackResourceUsage(T& pResource, D3D12_RESOURCE_STATES stateUsage, int cmdBlocker = CMDTYPE_WRITE, int cmdUsage = CMDTYPE_READ)
	{
		MaxResourceFenceValue(pResource, cmdBlocker);
		EndResourceStateTransition(pResource, stateUsage);
		SetResourceFenceValue(pResource, cmdUsage);
	}

	template<class T>
	ILINE void TrackResourceUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES stateUsage, int cmdBlocker = CMDTYPE_WRITE, int cmdUsage = CMDTYPE_READ)
	{
		MaxResourceFenceValue(pResource, cmdBlocker);
		
		if (view.MapsFullResource())
			EndResourceStateTransition(pResource, stateUsage);
		else
			EndResourceStateTransition(pResource, view, stateUsage);
		
		SetResourceFenceValue(pResource, cmdUsage);
	}

	template<class T> ILINE void   TrackResourceIBVUsage(T& pResource,                    D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_INDEX_BUFFER              ) { TrackResourceUsage(pResource,       desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourceVBVUsage(T& pResource,                    D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) { TrackResourceUsage(pResource,       desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourceCBVUsage(T& pResource,                    D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) { TrackResourceUsage(pResource,       desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
    template<class T> ILINE void   TrackResourceSRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_GENERIC_READ              ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourceCRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourcePRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourceUAVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS          ) { TrackResourceUsage(pResource, view, desiredState, pResource.IsConcurrentWritable() ? CMDTYPE_WRITE : CMDTYPE_ANY, pResource.IsConcurrentWritable() ? CMDTYPE_READ : CMDTYPE_ANY); }
	template<class T> ILINE void   TrackResourceUCVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS          ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY  , CMDTYPE_WRITE); }
	template<class T> ILINE void   TrackResourceDSVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE               ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY  , CMDTYPE_WRITE); }
	template<class T> ILINE void   TrackResourceDRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_DEPTH_READ                ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_WRITE, CMDTYPE_READ ); }
	template<class T> ILINE void   TrackResourceRTVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_RENDER_TARGET             ) { TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY  , CMDTYPE_WRITE); }
	
	template<class T> ILINE void PrepareResourceIBVUsage(T& pResource                   , D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_INDEX_BUFFER              ) { if (pResource.NeedsTransitionBarrier(this,       desiredState, true)) TrackResourceUsage(pResource,       desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceVBVUsage(T& pResource                   , D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) { if (pResource.NeedsTransitionBarrier(this,       desiredState, true)) TrackResourceUsage(pResource,       desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceCBVUsage(T& pResource                   , D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) { if (pResource.NeedsTransitionBarrier(this,       desiredState, true)) TrackResourceUsage(pResource,       desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
    template<class T> ILINE void PrepareResourceSRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_GENERIC_READ              ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceCRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourcePRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceUAVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS          ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceUCVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS          ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceDSVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE               ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceDRVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_DEPTH_READ                ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }
	template<class T> ILINE void PrepareResourceRTVUsage(T& pResource, const CView& view, D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_RENDER_TARGET             ) { if (pResource.NeedsTransitionBarrier(this, view, desiredState, true)) TrackResourceUsage(pResource, view, desiredState, CMDTYPE_ANY, CMDTYPE_WRITE); }

	//---------------------------------------------------------------------------------------------------------------------
	bool       IsFull(size_t numResources, size_t numSamplers, size_t numRendertargets, size_t numDepthStencils) const;
	bool       IsUsedByOutputViews(const CResource& res) const;

	ILINE void IncrementInputCursors(size_t numResources, size_t numSamplers)
	{
		GetResourceHeap().IncrementCursor(numResources);
		GetSamplerHeap().IncrementCursor(numSamplers);
	}

	ILINE void IncrementOutputCursors()
	{
		GetRenderTargetHeap().IncrementCursor(m_CurrentNumRTVs);
		if (m_pDSV)
		{
			GetDepthStencilHeap().IncrementCursor();
		}
	}

	void       BindResourceView(const CView& view, const TRange<UINT>& bindRange, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle);
	ILINE void BindResourceView(const CView& view, INT offset, const TRange<UINT>& bindRange)
	{
		BindResourceView(view, bindRange, GetResourceHeap().GetHandleOffsetCPU(offset));
	}

	void       BindSamplerState(const CSamplerState& state, D3D12_CPU_DESCRIPTOR_HANDLE dstHandle);
	ILINE void BindSamplerState(const CSamplerState& state, INT offset)
	{
		BindSamplerState(state, GetSamplerHeap().GetHandleOffsetCPU(offset));
	}

	void BindVertexBufferView(const CView &view, INT offset, const TRange<UINT> &bindRange, UINT bindStride, D3D12_VERTEX_BUFFER_VIEW(&heap)[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]);
	ILINE void BindVertexBufferView(const CView& view, INT offset, const TRange<UINT>& bindRange, UINT bindStride)
	{
		BindVertexBufferView(view, offset, bindRange, bindStride, m_VertexBufferHeap);
	}

	void       SetRootSignature(const CRootSignature* pRootSignature, bool bGfx, const D3D12_GPU_VIRTUAL_ADDRESS(&vConstViews)[8 /*CB_PER_FRAME*/]);
	void       SetGraphicsRootSignature(const CRootSignature* pRootSignature, const D3D12_GPU_VIRTUAL_ADDRESS (&vConstViews)[8 /*CB_PER_FRAME*/]);
	void       SetComputeRootSignature(const CRootSignature* pRootSignature, const D3D12_GPU_VIRTUAL_ADDRESS (&vConstViews)[8 /*CB_PER_FRAME*/]);

	ILINE void SetPSO(const CPSO* pso)
	{
		m_pCmdList->SetPipelineState(pso->GetD3D12PipelineState());
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetGraphicsRootSignature(const CRootSignature* rootSign)
	{
		m_pCmdList->SetGraphicsRootSignature(rootSign->GetD3D12RootSignature());
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetComputeRootSignature(const CRootSignature* rootSign)
	{
		m_pCmdList->SetComputeRootSignature(rootSign->GetD3D12RootSignature());
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetGraphicsConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
	{
		m_pCmdList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetComputeConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
	{
		m_pCmdList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetGraphics32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
	{
		m_pCmdList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetCompute32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
	{
		m_pCmdList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		m_nCommands += CLCOUNT_SETIO;
	}

	void       SetGraphicsDescriptorTables(const CRootSignature* pRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE eType);
	void       SetComputeDescriptorTables(const CRootSignature* pRootSignature, D3D12_DESCRIPTOR_HEAP_TYPE eType);

	ILINE void SetGraphicsDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
	{
		m_pCmdList->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void SetComputeDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
	{
		m_pCmdList->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);
		m_nCommands += CLCOUNT_SETIO;
	}

	void       ClearVertexBufferHeap(INT num);
	void       SetVertexBufferHeap(INT num);
	void       BindAndSetIndexBufferView(const CView& view, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT offset = 0);

	void       SetResourceAndSamplerStateHeaps();
	void       BindAndSetOutputViews(INT numRTVs, const CView** rtv, const CView* dsv);

	ILINE void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topo)
	{
		m_pCmdList->IASetPrimitiveTopology(topo);
		m_nCommands += CLCOUNT_SETIO;
	}

	ILINE void DrawInstanced(UINT vertexCountPerInstance, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation)
	{
		PendingResourceBarriers();

	#ifdef DX12_STATS
		m_NumDraws++;
	#endif // DX12_STATS

		m_pCmdList->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
		m_nCommands += CLCOUNT_DRAW;
	}

	ILINE void DrawIndexedInstanced(UINT indexCountPerInstance, UINT instanceCount, UINT startIndexLocation, UINT baseVertexLocation, UINT startInstanceLocation)
	{
		PendingResourceBarriers();

	#ifdef DX12_STATS
		m_NumDraws++;
	#endif // DX12_STATS

		m_pCmdList->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
		m_nCommands += CLCOUNT_DRAW;
	}

	ILINE void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
	{
		PendingResourceBarriers();

	#ifdef DX12_STATS
		m_NumDraws++;
	#endif // DX12_STATS

		m_pCmdList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		m_nCommands += CLCOUNT_DISPATCH;
	}

	void         ClearDepthStencilView(const CView& view, D3D12_CLEAR_FLAGS clearFlags, float depthValue, UINT stencilValue, UINT NumRects = 0U, const D3D12_RECT* pRect = nullptr);
	void         ClearRenderTargetView(const CView& view, const FLOAT rgba[4], UINT NumRects = 0U, const D3D12_RECT* pRect = nullptr);
	void         ClearUnorderedAccessView(const CView& view, const UINT rgba[4], UINT NumRects = 0U, const D3D12_RECT* pRect = nullptr);
	void         ClearUnorderedAccessView(const CView& view, const FLOAT rgba[4], UINT NumRects = 0U, const D3D12_RECT* pRect = nullptr);
	void         ClearView(const CView& view, const FLOAT rgba[4], UINT NumRects = 0U, const D3D12_RECT* pRect = nullptr);

	void         CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox = nullptr);
	void         CopyResource(ID3D12Resource* pDstResource, ID3D12Resource* pSrcResource);

	void         CopyResource(CResource& pDstResource, CResource& pSrcResource);
	void         CopySubresources(CResource& pDestResource, UINT destSubResource, UINT x, UINT y, UINT z, CResource& pSrcResource, UINT srcSubResource, const D3D12_BOX* srcBox, UINT NumSubresources);
	void         UpdateSubresourceRegion(CResource& pResource, UINT subResource, const D3D12_BOX* box, const void* data, UINT rowPitch, UINT depthPitch);
	void         UpdateSubresources(CResource& rResource, D3D12_RESOURCE_STATES eAnnouncedState, UINT64 uploadBufferSize, UINT subResources, D3D12_SUBRESOURCE_DATA* subResourceData);
	void         DiscardResource(CResource& pResource, const D3D12_DISCARD_REGION* pRegion);

	bool         PresentSwapChain(CSwapChain* pDX12SwapChain);

	ILINE void   SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
	{
		m_pCmdList->RSSetViewports(NumViewports, pViewports);
		m_nCommands += CLCOUNT_SETSTATE;
	}

	ILINE void SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
	{
		m_pCmdList->RSSetScissorRects(NumRects, pRects);
		m_nCommands += CLCOUNT_SETSTATE;
	}
	
	ILINE void SetDepthBounds(float fMin, float fMax)
	{
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
		if (m_pCmdList1 && s_DepthBoundsTestSupported)
		{
			m_pCmdList1->OMSetDepthBounds(fMin, fMax);
			m_nCommands += CLCOUNT_SETSTATE;
		}
#endif
	}

	ILINE void SetStencilRef(UINT Ref)
	{
		m_pCmdList->OMSetStencilRef(Ref);
		m_nCommands += CLCOUNT_SETSTATE;
	}

	ILINE void BeginQuery(CQueryHeap& queryHeap, D3D12_QUERY_TYPE Type, UINT Index)
	{
		m_pCmdList->BeginQuery(queryHeap.GetD3D12QueryHeap(), Type, Index);
		m_nCommands += CLCOUNT_QUERY;
	}

	ILINE void EndQuery(CQueryHeap& queryHeap, D3D12_QUERY_TYPE Type, UINT Index)
	{
		m_pCmdList->EndQuery(queryHeap.GetD3D12QueryHeap(), Type, Index);
		m_nCommands += CLCOUNT_QUERY;
	}

	ILINE void ResolveQueryData(CQueryHeap& queryHeap, D3D12_QUERY_TYPE Type,
	                            _In_ UINT StartIndex,
	                            _In_ UINT NumQueries,
	                            _In_ ID3D12Resource* pDestinationBuffer,
	                            _In_ UINT64 AlignedDestinationBufferOffset)
	{
		PendingResourceBarriers();

		m_pCmdList->ResolveQueryData(queryHeap.GetD3D12QueryHeap(), Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
		m_nCommands += CLCOUNT_RESOLVE;
	}

protected:
	CCommandList(CCommandListPool& pPool);
	CCommandListPool& m_rPool;
	
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
	static BOOL s_DepthBoundsTestSupported;
#endif
#if NTDDI_WIN10_RS3 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS3)
	static D3D12_COMMAND_LIST_SUPPORT_FLAGS s_WriteBufferImmediateSupportFlags;
#endif

	void BindDepthStencilView(const CView& dsv);
	void BindRenderTargetView(const CView& rtv);
	void BindUnorderedAccessView(const CView& uav);

	ID3D12Device* m_pD3D12Device;
	DX12_PTR(ID3D12GraphicsCommandList)  m_pCmdList;  // RTM
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
	DX12_PTR(ID3D12GraphicsCommandList1) m_pCmdList1; // Creator's Update
#endif
#if NTDDI_WIN10_RS3 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS3)
	DX12_PTR(ID3D12GraphicsCommandList2) m_pCmdList2; // Fall Creator's Update
#endif
	DX12_PTR(ID3D12CommandAllocator) m_pCmdAllocator;
	DX12_PTR(ID3D12CommandQueue) m_pCmdQueue;
	UINT64 m_UsedFenceValues[CMDTYPE_NUM][CMDQUEUE_NUM];

	// Only used by IsUsedByOutputViews()
	const CView* m_pDSV;
	const CView* m_pRTV[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
	INT m_CurrentNumRTVs;

	UINT64 m_CurrentFenceValue;
	D3D12_COMMAND_LIST_TYPE m_eListType;

	#if defined(_ALLOW_INITIALIZER_LISTS)
	CDescriptorBlock m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	#else
	std::vector<CDescriptorBlock> m_DescriptorHeaps;
	#endif

	ILINE CDescriptorBlock&       GetResourceHeap()           { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]; }
	ILINE CDescriptorBlock&       GetSamplerHeap()            { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]; }
	ILINE CDescriptorBlock&       GetRenderTargetHeap()       { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]; }
	ILINE CDescriptorBlock&       GetDepthStencilHeap()       { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]; }

	ILINE const CDescriptorBlock& GetResourceHeap() const     { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]; }
	ILINE const CDescriptorBlock& GetSamplerHeap() const      { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]; }
	ILINE const CDescriptorBlock& GetRenderTargetHeap() const { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]; }
	ILINE const CDescriptorBlock& GetDepthStencilHeap() const { return m_DescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]; }

	INT m_CurrentNumVertexBuffers;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferHeap[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

	INT m_CurrentNumPendingBarriers;
	D3D12_RESOURCE_BARRIER m_PendingBarrierHeap[256];

public: // Not good
	UINT m_nCommands;
	mutable enum
	{
		CLSTATE_FREE,
		CLSTATE_STARTED,
		CLSTATE_UTILIZED,
		CLSTATE_COMPLETED,
		CLSTATE_SCHEDULED,
		CLSTATE_SUBMITTED,
		CLSTATE_FINISHED,
		CLSTATE_CLEARING
	}
	m_eState;

	UINT m_nodeMask;

	#ifdef DX12_STATS
	size_t m_NumDraws;
	size_t m_NumWaitsGPU;
	size_t m_NumWaitsCPU;
	#endif // DX12_STATS
};

}
