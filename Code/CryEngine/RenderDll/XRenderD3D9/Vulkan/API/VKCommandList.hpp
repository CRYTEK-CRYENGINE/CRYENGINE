// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"
#include "VKCommandListFence.hpp"
#include "VKAsyncCommandQueue.hpp"

#define CMDLIST_TYPE_DIRECT   7 // VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT
#define CMDLIST_TYPE_COMPUTE  6 //                         VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT
#define CMDLIST_TYPE_COPY     4 //                                                VK_QUEUE_TRANSFER_BIT

#define CMDLIST_DEFAULT       CMDLIST_TYPE_DIRECT
#define CMDLIST_GRAPHICS      NCryVulkan::CCommandListPool::m_MapQueueType[CMDQUEUE_GRAPHICS]
#define CMDLIST_COMPUTE       NCryVulkan::CCommandListPool::m_MapQueueType[CMDQUEUE_COMPUTE]
#define CMDLIST_COPY          NCryVulkan::CCommandListPool::m_MapQueueType[CMDQUEUE_COPY]

#define CMDQUEUE_GRAPHICS_IOE true  // graphics queue will terminate in-order
#define CMDQUEUE_COMPUTE_IOE  false // compute queue can terminate out-of-order
#define CMDQUEUE_COPY_IOE     false // copy queue can terminate out-of-order

#define VK_BARRIER_FUSION     true

namespace NCryVulkan
{

class CCommandListPool;
class CCommandList;
class CSwapChain;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CCommandListPool
{
public:
	CCommandListPool(CDevice* device, CCommandListFenceSet& rCmdFence, int nPoolFenceId);
	CCommandListPool(const CCommandListPool& clp);
	~CCommandListPool();

	void Configure();
	bool Init(VkQueueFlagBits eType = VkQueueFlagBits(CMDLIST_DEFAULT));
	void Clear();

	void AcquireCommandList(VK_PTR(CCommandList)& result) threadsafe;
	void ForfeitCommandList(VK_PTR(CCommandList)& result, bool bWait = false) threadsafe;
	void AcquireCommandLists(uint32 numCLs, VK_PTR(CCommandList)* results) threadsafe;
	void ForfeitCommandLists(uint32 numCLs, VK_PTR(CCommandList)* results, bool bWait = false) threadsafe;

	ILINE VkQueue GetVkCommandQueue() threadsafe_const
	{
		return m_CmdQueue;
	}

	ILINE uint32 GetVkQueueFamily() threadsafe_const
	{
		return m_nPoolFamily;
	}

	ILINE uint32 GetVkQueueIndex() threadsafe_const
	{
		return m_nPoolIndex;
	}

	ILINE VkQueueFlagBits GetVkQueueType() threadsafe_const
	{
		return m_ePoolType;
	}

	ILINE NCryVulkan::CAsyncCommandQueue& GetAsyncCommandQueue() threadsafe
	{
		return m_AsyncCommandQueue;
	}

	ILINE CDevice* GetDevice() threadsafe_const
	{
		return m_pDevice;
	}

	ILINE CCommandListFenceSet& GetFences() threadsafe
	{
		return m_rCmdFences;
	}

	ILINE const VkNaryFence* GetVkFences() threadsafe_const
	{
		return m_rCmdFences.GetVkFences();
	}

	ILINE VkNaryFence* GetVkFence() threadsafe
	{
		return m_rCmdFences.GetVkFence(m_nPoolFenceId);
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
	ILINE void WaitForFenceOnGPU(const T64 (&fenceValues)[CMDQUEUE_NUM], INT& numPendingWSemaphores, std::array<VkSemaphore, 64>& PendingWSemaphoreHeap) threadsafe
	{
		// Mask in-order queues, preventing a Wait()-command being generated
		const bool bMasked = PERSP_GPU && IsInOrder();

		// The pool which waits for the fence can be omitted (in-order-execution)
		UINT64 fenceValuesMasked[CMDQUEUE_NUM];

		fenceValuesMasked[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
		fenceValuesMasked[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
		fenceValuesMasked[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];
	//	fenceValuesMasked[m_nPoolFenceId] = (!bMasked ? fenceValuesMasked[m_nPoolFenceId] : 0);

		// Under Vulkan it's not possible to Reset() GPU-only semaphores from the CPU-side, or through a command-list command except vkQueueSubmit().
		// Which means we are forced to pass through all "waits" even though we know they are nops.
	//	if (!m_rCmdFences.IsCompleted(fenceValuesMasked))
		{
			VK_LOG(VK_CONCURRENCY_ANALYZER || VK_FENCE_ANALYZER,
			       "Waiting GPU for fences %s: [%lld,%lld,%lld] (is [%lld,%lld,%lld] currently)",
			       m_nPoolFenceId == CMDQUEUE_GRAPHICS ? "gfx" : m_nPoolFenceId == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			       UINT64(fenceValues[CMDQUEUE_GRAPHICS]),
			       UINT64(fenceValues[CMDQUEUE_COMPUTE]),
			       UINT64(fenceValues[CMDQUEUE_COPY]),
			       m_rCmdFences.ProbeCompletion(CMDQUEUE_GRAPHICS),
			       m_rCmdFences.ProbeCompletion(CMDQUEUE_COMPUTE),
			       m_rCmdFences.ProbeCompletion(CMDQUEUE_COPY));

	#ifdef VK_STATS
			m_NumWaitsGPU += 2;
	#endif // VK_STATS

			VK_ASSERT(fenceValuesMasked[CMDQUEUE_GRAPHICS] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_GRAPHICS), "Waiting for unsubmitted gfx CL, deadlock imminent!");
			VK_ASSERT(fenceValuesMasked[CMDQUEUE_COMPUTE] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COMPUTE), "Waiting for unsubmitted cmp CL, deadlock imminent!");
			VK_ASSERT(fenceValuesMasked[CMDQUEUE_COPY] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COPY), "Waiting for unsubmitted cpy CL, deadlock imminent!");

			VK_ASSERT(numPendingWSemaphores < PendingWSemaphoreHeap.size(), "Too many other CLs to synchronize with!");

		//	m_AsyncCommandQueue.Wait(m_rCmdFences.GetVkFences(), fenceValuesMasked);
			const VkNaryFence* pFence = m_rCmdFences.GetVkFences();
			if (fenceValuesMasked[CMDQUEUE_GRAPHICS])
				PendingWSemaphoreHeap[numPendingWSemaphores++] = pFence[CMDQUEUE_GRAPHICS].second[fenceValuesMasked[CMDQUEUE_GRAPHICS] % pFence[CMDQUEUE_GRAPHICS].second.size()];
			if (fenceValuesMasked[CMDQUEUE_COMPUTE])
				PendingWSemaphoreHeap[numPendingWSemaphores++] = pFence[CMDQUEUE_COMPUTE].second[fenceValuesMasked[CMDQUEUE_COMPUTE] % pFence[CMDQUEUE_COMPUTE].second.size()];
			if (fenceValuesMasked[CMDQUEUE_COPY])
				PendingWSemaphoreHeap[numPendingWSemaphores++] = pFence[CMDQUEUE_COPY].second[fenceValuesMasked[CMDQUEUE_COPY] % pFence[CMDQUEUE_COPY].second.size()];

			// Unregister semaphore that needed to be waited on (creates in-order execution within one queue)
			if (m_LastSignalledFenceValue)
			{
				PendingWSemaphoreHeap[numPendingWSemaphores++] = AcquireLastSignalledSemaphore();
			}
		}
	}

	ILINE void WaitForFenceOnGPU(const UINT64 fenceValue, const int id, INT& numPendingWSemaphores, std::array<VkSemaphore, 64>& PendingWSemaphoreHeap) threadsafe
	{
		// Under Vulkan it's not possible to Reset(0 GPU-only semaphores from the CPU-side, or through a command-list command except vkQueueSubmit().
		// Which means we are forced to pass through all "waits" even though we know they are nops.
	//	if (!m_rCmdFences.IsCompleted(fenceValue, id))
		{
			VK_LOG(VK_CONCURRENCY_ANALYZER || VK_FENCE_ANALYZER,
			       "Waiting GPU for fence %s: %lld (is %lld currently)",
			       id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			       fenceValue,
			       m_rCmdFences.ProbeCompletion(id));

	#ifdef VK_STATS
			m_NumWaitsGPU++;
	#endif // VK_STATS

			VK_ASSERT(fenceValue <= m_rCmdFences.GetSubmittedValue(id), "Waiting for unsubmitted CL, deadlock imminent!");

			VK_ASSERT(numPendingWSemaphores < PendingWSemaphoreHeap.size(), "Too many other CLs to synchronize with!");

		//	m_AsyncCommandQueue.Wait(m_rCmdFences.GetVkFence(id), fenceValue);
			VkNaryFence* const pFence = m_rCmdFences.GetVkFence(id);
			if (true || fenceValue)
				PendingWSemaphoreHeap[numPendingWSemaphores++] = pFence->second[fenceValue % pFence->second.size()];
		}
	}

	ILINE void WaitForFenceOnGPU(const VkSemaphore semaphoreObject, const int id, INT& numPendingWSemaphores, std::array<VkSemaphore, 64>& PendingWSemaphoreHeap) threadsafe
	{
		// Under Vulkan it's not possible to Reset(0 GPU-only semaphores from the CPU-side, or through a command-list command except vkQueueSubmit().
		// Which means we are forced to pass through all "waits" even though we know they are nops.
	//	if (!m_rCmdFences.IsCompleted(semaphoreObject, id))
		{
			VK_LOG(VK_CONCURRENCY_ANALYZER || VK_FENCE_ANALYZER,
				"Waiting GPU for fence %s: [swap-chain sempahore] (is %lld currently)",
				id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
				m_rCmdFences.ProbeCompletion(id));

#ifdef VK_STATS
			m_NumWaitsGPU++;
#endif // VK_STATS

			VK_ASSERT(numPendingWSemaphores < PendingWSemaphoreHeap.size(), "Too many other CLs to synchronize with!");

			//	m_AsyncCommandQueue.Wait(m_rCmdFences.GetVkFence(id), fenceValue);
			VkNaryFence* const pFence = m_rCmdFences.GetVkFence(id);
			if (true)
				PendingWSemaphoreHeap[numPendingWSemaphores++] = semaphoreObject;
		}
	}

	ILINE void WaitForFenceOnGPU(const UINT64 fenceValue, INT& numPendingWSemaphores, std::array<VkSemaphore, 64>& PendingWSemaphoreHeap) threadsafe
	{
		return WaitForFenceOnGPU(fenceValue, m_nPoolFenceId, numPendingWSemaphores, PendingWSemaphoreHeap);
	}

	ILINE void WaitForFenceOnGPU(const VkSemaphore semaphoreObject, INT& numPendingWSemaphores, std::array<VkSemaphore, 64>& PendingWSemaphoreHeap) threadsafe
	{
		return WaitForFenceOnGPU(semaphoreObject, m_nPoolFenceId, numPendingWSemaphores, PendingWSemaphoreHeap);
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
	#ifdef VK_STATS
			m_NumWaitsCPU += 2;
	#endif // VK_STATS

			VK_ASSERT(fenceValuesCopied[CMDQUEUE_GRAPHICS] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_GRAPHICS), "Waiting for unsubmitted gfx CL, deadlock imminent!");
			VK_ASSERT(fenceValuesCopied[CMDQUEUE_COMPUTE] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COMPUTE), "Waiting for unsubmitted cmp CL, deadlock imminent!");
			VK_ASSERT(fenceValuesCopied[CMDQUEUE_COPY] <= m_rCmdFences.GetSubmittedValue(CMDQUEUE_COPY), "Waiting for unsubmitted cpy CL, deadlock imminent!");

			m_rCmdFences.WaitForFence(fenceValuesCopied);
		}
	}

	ILINE void WaitForFenceOnCPU(const UINT64 fenceValue, const int id) threadsafe_const
	{
		if (!m_rCmdFences.IsCompleted(fenceValue, id))
		{
	#ifdef VK_STATS
			m_NumWaitsCPU++;
	#endif // VK_STATS

			VK_ASSERT(fenceValue <= m_rCmdFences.GetSubmittedValue(id), "Waiting for unsubmitted CL, deadlock imminent!");

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

	ILINE void SignalFenceOnGPU(const UINT64 fenceValue, INT& numPendingSSemaphores, std::array<VkSemaphore, 64>& PendingSSemaphoreHeap)
	{
		VkNaryFence* pFence = GetVkFence();
		PendingSSemaphoreHeap[numPendingSSemaphores++] = pFence->second[fenceValue % pFence->second.size()];

		// Register that semaphore needs to be waited on (creates in-order execution within one queue)
		m_LastSignalledFenceValue = fenceValue;
	}

	ILINE void SignalFenceOnCPU(const UINT64 fenceValue)
	{
		m_rCmdFences.SignalFence(fenceValue, m_nPoolFenceId);
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

	ILINE VkSemaphore AcquireLastSignalledSemaphore() threadsafe
	{
		UINT64 lastSignalledFenceValue = m_LastSignalledFenceValue.exchange(0);
		VK_ASSERT(lastSignalledFenceValue != 0);
		VkNaryFence* const pFence = m_rCmdFences.GetVkFence(m_nPoolFenceId);
		return pFence->second[lastSignalledFenceValue % pFence->second.size()];
	}

private:
	void ScheduleCommandLists();
	void CreateOrReuseCommandList(VK_PTR(CCommandList) & result);

	CDevice*              m_pDevice;
	CCommandListFenceSet& m_rCmdFences;
	int                   m_nPoolFenceId;
	uint32                m_nPoolFamily;
	uint32                m_nPoolIndex;
	VkQueueFlagBits       m_ePoolType;
	FVAL64                m_LastSignalledFenceValue;

	typedef std::deque<VK_PTR(CCommandList)> TCommandLists;
	TCommandLists m_LiveCommandLists;
	TCommandLists m_BusyCommandLists;
	TCommandLists m_FreeCommandLists;

	VkQueue                        m_CmdQueue;
	NCryVulkan::CAsyncCommandQueue m_AsyncCommandQueue;

	#ifdef VK_STATS
	size_t m_PeakNumCommandListsAllocated;
	size_t m_PeakNumCommandListsInFlight;
	#endif // VK_STATS

public:
	static VkQueueFlagBits m_MapQueueType[CMDQUEUE_NUM];
	static std::pair<uint32, uint32> m_MapQueueBits[1U << 4];
	static uint32 m_MapQueueIndices[32];
};

class CCommandList : public CDeviceObject
{
	friend class CCommandListPool;

public:
	virtual ~CCommandList();

	bool Init(UINT64 currentFenceValue);
	bool Reset();

	ILINE VkCommandBuffer GetVkCommandList() threadsafe_const
	{
		return m_CmdList;
	}
	ILINE VkQueueFlagBits GetVkListType() threadsafe_const
	{
		return m_eListType;
	}
	ILINE VkCommandPool GetVkCommandAllocator() threadsafe_const
	{
		return m_CmdAllocator;
	}
	ILINE VkQueue GetVkCommandQueue() threadsafe_const
	{
		return m_CmdQueue;
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
	ILINE bool IsUsedInSynchronization() const
	{
		return !!(m_CurrentNumPendingWSemaphores + m_CurrentNumPendingSSemaphores);
	}
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

		return (m_nCommands > 0) || IsUsedInSynchronization() || m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS;
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
		VK_LOG(VK_CONCURRENCY_ANALYZER || VK_FENCE_ANALYZER, "Signaling GPU fence %s: %lld", m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy", m_CurrentFenceValue);
		m_rPool.SignalFenceOnGPU(m_CurrentFenceValue, m_CurrentNumPendingSSemaphores, m_PendingSSemaphoreHeap);
		return m_CurrentFenceValue;
	}

	UINT64 SignalFenceOnCPU()
	{
		VK_LOG(VK_CONCURRENCY_ANALYZER || VK_FENCE_ANALYZER, "Signaling CPU fence %s: %lld", m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy", m_CurrentFenceValue);
		m_rPool.SignalFenceOnCPU(m_CurrentFenceValue);
		return m_CurrentFenceValue;
	}

	void       Submit();
	ILINE bool IsSubmitted() const
	{
		return m_eState >= CLSTATE_SUBMITTED;
	}

	// Stage 4
	void WaitForFinishOnGPU(VkSemaphore semaphoreObject)
	{
		// Adding the semaphore will make the command-list submit even if there are no commands in it, because semaphores can not set/reset from CPU side.
		m_rPool.WaitForFenceOnGPU(semaphoreObject, m_CurrentNumPendingWSemaphores, m_PendingWSemaphoreHeap);
	}

	void WaitForFinishOnGPU()
	{
		VK_ASSERT(m_eState == CLSTATE_SUBMITTED, "GPU fence waits for itself to be complete: deadlock imminent!");
		m_rPool.WaitForFenceOnGPU(m_CurrentFenceValue, m_CurrentNumPendingWSemaphores, m_PendingWSemaphoreHeap);
	}

	void WaitForFinishOnCPU() const
	{
		VK_ASSERT(m_eState == CLSTATE_SUBMITTED, "CPU fence waits for itself to be complete: deadlock imminent!");
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
	void ResourceBarrier(UINT NumBarriers, const VkMemoryBarrier*       pBarriers);
	void ResourceBarrier(UINT NumBarriers, const VkBufferMemoryBarrier* pBarriers);
	void ResourceBarrier(UINT NumBarriers, const VkImageMemoryBarrier*  pBarriers);
	void PendingResourceBarriers();

	bool PresentSwapChain(CSwapChain* pDX12SwapChain);

protected:
	CCommandList(CCommandListPool& pPool);
	CCommandListPool& m_rPool;

	VkDevice m_VkDevice;
	VkCommandBuffer m_CmdList;
	VkCommandPool m_CmdAllocator;
	VkQueue m_CmdQueue;
	UINT64 m_UsedFenceValues[CMDTYPE_NUM][CMDQUEUE_NUM];

	INT m_CurrentNumPendingWSemaphores;
	INT m_CurrentNumPendingSSemaphores;
	std::array<VkSemaphore         , 64> m_PendingWSemaphoreHeap;
	std::array<VkPipelineStageFlags, 64> m_PendingWStageMaskHeap;
	std::array<VkSemaphore         , 64> m_PendingSSemaphoreHeap;

	INT m_CurrentNumPendingWFences;
	INT m_CurrentNumPendingSFences;
	std::array<VkFence, 64> m_PendingWFenceHeap;
	std::array<VkFence, 64> m_PendingSFenceHeap;

	UINT64 m_CurrentFenceValue;
	VkQueueFlagBits m_eListType;

	INT m_CurrentNumPendingMBarriers;
	INT m_CurrentNumPendingBBarriers;
	INT m_CurrentNumPendingIBarriers;
	VkMemoryBarrier       m_PendingMBarrierHeap[256];
	VkBufferMemoryBarrier m_PendingBBarrierHeap[256];
	VkImageMemoryBarrier  m_PendingIBarrierHeap[256];

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

	#ifdef VK_STATS
	size_t m_NumDraws;
	size_t m_NumWaitsGPU;
	size_t m_NumWaitsCPU;
	#endif // VK_STATS
};

}
