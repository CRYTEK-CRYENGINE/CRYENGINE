// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "DX11Base.hpp"
#include "DX11Device.hpp"
#include "DX11CommandListFence.hpp"

namespace NCryDX11 {

class CDevice;

class CCommandScheduler : public CDeviceObject
{
	typedef std::function<void(void*, int)> QueueEventCallbackType;

public:
	CCommandScheduler(CDevice* pDevice);
	virtual ~CCommandScheduler();

	void Flush(bool bWait);
	void GarbageCollect();
	void SyncFrame();
	void EndOfFrame(bool bWait);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Fence management API
	ILINE const CCommandListFenceSet& GetFenceManager() const { return m_CmdFenceSet; }

	ILINE UINT64 InsertFence(int nPoolId = CMDQUEUE_IMMEDIATE)
	{
		return m_CmdFenceSet.GetCurrentValue(nPoolId);
	}

	ILINE HRESULT FlushToFence(UINT64 fenceValue)
	{
		Flush(false);
		return S_OK;
	}

	ILINE HRESULT TestForFence(UINT64 fenceValue)
	{
		return m_CmdFenceSet.IsCompleted(fenceValue, CMDQUEUE_IMMEDIATE) ? S_OK : S_FALSE;
	}

	ILINE HRESULT WaitForFence(UINT64 fenceValue)
	{
		m_CmdFenceSet.WaitForFence(fenceValue, CMDQUEUE_IMMEDIATE);
		return S_OK;
	}

private:
#define FRAME_FENCES         32
#define FRAME_FENCE_LATENCY  32
#define FRAME_FENCE_INFLIGHT std::min(MAX_FRAMES_IN_FLIGHT, CRendererCVars::CV_r_MaxFrameLatency + 1)
	UINT64                        m_FrameFenceValuesSubmitted[FRAME_FENCES][CMDQUEUE_NUM];
	UINT64                        m_FrameFenceValuesCompleted[FRAME_FENCES][CMDQUEUE_NUM];
	ULONG                         m_FrameFenceCursor;

	CCommandListFenceSet          m_CmdFenceSet;
};

}
