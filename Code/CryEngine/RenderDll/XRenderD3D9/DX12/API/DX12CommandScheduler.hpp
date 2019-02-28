// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"
#include "DX12Device.hpp"
#include "DX12CommandList.hpp"

namespace NCryDX12 {

class CDevice;

class CCommandScheduler : public CDeviceObject
{
	typedef std::function<void(void*, int)> QueueEventCallbackType;

public:
	CCommandScheduler(CDevice* pDevice, uint nodeMask);
	virtual ~CCommandScheduler();

	inline CCommandListPool& GetCommandListPool(int queueIndex)       { return m_CmdListPools[queueIndex]; }
	inline CCommandList*     GetCommandList    (int queueIndex) const { return m_pCmdLists   [queueIndex]; }

	void BeginScheduling();
	bool RecreateCommandListPool(int queueIndex, uint nodeMask);

	bool IsCommandListUtilized(int queueIndex, const UINT64 fenceValue)
	{
		return (m_pCmdLists[queueIndex]->IsUtilized() && (m_CmdFenceSet.GetCurrentValue(queueIndex) == fenceValue));
	}

	void CeaseCommandQueue(int queueIndex, bool bWait);
	void ResumeCommandQueue(int queueIndex);
	void CeaseAllCommandQueues(bool bWait);
	void ResumeAllCommandQueues();

	void SubmitCommands(int queueIndex, bool bWait);
	void SubmitCommands(int queueIndex, bool bWait, const UINT64 fenceValue);
	void SubmitAllCommands(bool bWait);
	void SubmitAllCommands(bool bWait, const UINT64(&fenceValues)[CMDQUEUE_NUM]);
	void SubmitAllCommands(bool bWait, const FVAL64(&fenceValues)[CMDQUEUE_NUM]);

	void Flush(bool bWait) { SubmitAllCommands(bWait, m_CmdFenceSet.GetCurrentValues()); }
	void GarbageCollect();
	void SyncFrame();
	void EndOfFrame(bool bWait);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fence management API
	ILINE const CCommandListFenceSet& GetFenceManager() const { return m_CmdFenceSet; }

	ILINE UINT64 InsertFence(int nPoolId = CMDQUEUE_GRAPHICS)
	{
		return m_CmdFenceSet.GetCurrentValue(nPoolId) - !m_pCmdLists[nPoolId]->IsUtilized();
	}

	ILINE HRESULT FlushToFence(UINT64 fenceValue)
	{
		SubmitCommands(CMDQUEUE_GRAPHICS, false, fenceValue);
		m_CmdListPools[CMDQUEUE_GRAPHICS].GetAsyncCommandQueue().Flush(fenceValue);
		return S_OK;
	}

	ILINE HRESULT TestForFence(UINT64 fenceValue)
	{
		return m_CmdFenceSet.IsCompleted(fenceValue, CMDQUEUE_GRAPHICS) ? S_OK : S_FALSE;
	}

	ILINE HRESULT WaitForFence(UINT64 fenceValue)
	{
		m_CmdFenceSet.WaitForFence(fenceValue, CMDQUEUE_GRAPHICS);
		return S_OK;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Callback API
	enum
	{
		CMDQUEUE_EVENT_CEASE  = BIT(0),
		CMDQUEUE_EVENT_RESUME = BIT(1)
	};

	inline void AddQueueEventCallback(void* listener, QueueEventCallbackType callback, uint eType)
	{
		if (eType & CMDQUEUE_EVENT_CEASE)
			m_ceaseCallbacks.emplace(listener, callback);
		if (eType & CMDQUEUE_EVENT_RESUME)
			m_resumeCallbacks.emplace(listener, callback);
	}

	inline void RemoveQueueEventCallbacks(void* listener)
	{
		m_ceaseCallbacks.erase(listener);
		m_resumeCallbacks.erase(listener);
	}

private:
#define FRAME_FENCES         32
#define FRAME_FENCE_LATENCY  32
#define FRAME_FENCE_INFLIGHT std::min(MAX_FRAMES_IN_FLIGHT, CRendererCVars::CV_r_MaxFrameLatency + 1)
	UINT64                        m_FrameFenceValuesSubmitted[FRAME_FENCES][CMDQUEUE_NUM];
	UINT64                        m_FrameFenceValuesCompleted[FRAME_FENCES][CMDQUEUE_NUM];
	ULONG                         m_FrameFenceCursor;

	CCommandListFenceSet          m_CmdFenceSet;
#if defined(_ALLOW_INITIALIZER_LISTS)
	CCommandListPool              m_CmdListPools[CMDQUEUE_NUM];
#else
	std::vector<CCommandListPool> m_CmdListPools;
#endif
	DX12_PTR(CCommandList)        m_pCmdLists[CMDQUEUE_NUM];

#ifdef DX12_STATS
	size_t m_NumCommandListOverflows;
	size_t m_NumCommandListSplits;
#endif // DX12_STATS

	std::unordered_map<void*, QueueEventCallbackType> m_ceaseCallbacks;
	std::unordered_map<void*, QueueEventCallbackType> m_resumeCallbacks;
};

}
