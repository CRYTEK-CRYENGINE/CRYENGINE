// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DX11CommandScheduler.hpp"

namespace NCryDX11
{

//---------------------------------------------------------------------------------------------------------------------
CCommandScheduler::CCommandScheduler(CDevice* pDevice)
	: CDeviceObject(pDevice)
	, m_CmdFenceSet(pDevice)
{
	m_CmdFenceSet.Init();

	ZeroMemory(&m_FrameFenceValuesSubmitted, sizeof(m_FrameFenceValuesSubmitted));
	ZeroMemory(&m_FrameFenceValuesCompleted, sizeof(m_FrameFenceValuesCompleted));

	m_FrameFenceCursor = 0;
}

//---------------------------------------------------------------------------------------------------------------------
CCommandScheduler::~CCommandScheduler()
{

}

void CCommandScheduler::Flush(bool bWait)
{
	auto naryFences = m_CmdFenceSet.GetD3D11Fence(CMDQUEUE_IMMEDIATE);

	const UINT64 currFenceValue = m_CmdFenceSet.GetCurrentValue(CMDQUEUE_IMMEDIATE);
	const UINT64 nextFenceValue = currFenceValue + 1;

	{
		// Same as "Schedule" (submit) + "Execute" (signal)
		gcpRendD3D->GetDeviceContext().End((*naryFences)[currFenceValue % naryFences->size()]);
		m_CmdFenceSet.SetSubmittedValue(currFenceValue, CMDQUEUE_IMMEDIATE);

		gcpRendD3D->GetDeviceContext().Flush();
		m_CmdFenceSet.SetSignalledValue(currFenceValue, CMDQUEUE_IMMEDIATE);

		// Wait if necessary
		if (bWait)
		{
			m_CmdFenceSet.WaitForFence(currFenceValue, CMDQUEUE_IMMEDIATE);
		}
	}

	{
		// Raise fence for continuous time-line
	//	gcpRendD3D->GetDeviceContext().Begin((*naryFences)[nextFenceValue % naryFences->size()]);
		m_CmdFenceSet.SetCurrentValue(nextFenceValue, CMDQUEUE_IMMEDIATE);
	}
}

void CCommandScheduler::GarbageCollect()
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "FLUSH GPU HEAPS");

	// Ring buffer for the _completed_ fences of past number of frames
	m_CmdFenceSet.AdvanceCompletion();
	m_CmdFenceSet.GetLastCompletedFenceValues(
		m_FrameFenceValuesCompleted[(m_FrameFenceCursor)]);
	GetDevice()->FlushReleaseHeap(
		m_FrameFenceValuesCompleted[(m_FrameFenceCursor)],
		m_FrameFenceValuesCompleted[(m_FrameFenceCursor + (FRAME_FENCES - std::max(1, FRAME_FENCE_LATENCY - 1))) % FRAME_FENCES]);
}

void CCommandScheduler::SyncFrame()
{
	// Stall render thread until GPU has finished processing previous frame (in case max frame latency is 1)
	CRY_PROFILE_REGION(PROFILE_RENDERER, "SYNC TO FRAME FENCE");

	// Block when more than N frames have not been rendered yet
	m_CmdFenceSet.GetSubmittedValues(
		m_FrameFenceValuesSubmitted[(m_FrameFenceCursor)]);
	m_CmdFenceSet.WaitForFence(
		m_FrameFenceValuesSubmitted[(m_FrameFenceCursor + (FRAME_FENCES - std::max(1, FRAME_FENCE_INFLIGHT - 1))) % FRAME_FENCES]);
}

void CCommandScheduler::EndOfFrame(bool bWait)
{
	// TODO: add final bWait if back-buffer is copied directly before the present
	Flush(bWait);

	if (CRendererCVars::CV_r_SyncToFrameFence)
		SyncFrame();

	GarbageCollect();

	m_FrameFenceCursor = ((++m_FrameFenceCursor) % FRAME_FENCES);
}

}
