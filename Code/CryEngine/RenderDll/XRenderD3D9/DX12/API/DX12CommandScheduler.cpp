// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12CommandScheduler.hpp"

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CCommandScheduler::CCommandScheduler(CDevice* pDevice, uint nodeMask)
	: CDeviceObject(pDevice)
	, m_CmdFenceSet(pDevice)
#if defined(_ALLOW_INITIALIZER_LISTS)
	// *INDENT-OFF*
	, m_CmdListPools
	{
		{ pDevice, m_CmdFenceSet, CMDQUEUE_GRAPHICS },
		{ pDevice, m_CmdFenceSet, CMDQUEUE_COMPUTE	},
		{ pDevice, m_CmdFenceSet, CMDQUEUE_COPY }
	}
	// *INDENT-ON*
#endif
{
#if !defined(_ALLOW_INITIALIZER_LISTS)
	m_CmdListPools.emplace_back(pDevice, m_CmdFenceSet, CMDQUEUE_GRAPHICS);
	m_CmdListPools.emplace_back(pDevice, m_CmdFenceSet, CMDQUEUE_COMPUTE);
	m_CmdListPools.emplace_back(pDevice, m_CmdFenceSet, CMDQUEUE_COPY);
#endif

	m_CmdListPools[CMDQUEUE_GRAPHICS].Init(D3D12_COMMAND_LIST_TYPE(CMDLIST_GRAPHICS), nodeMask);
	m_CmdListPools[CMDQUEUE_COMPUTE].Init(D3D12_COMMAND_LIST_TYPE(CMDLIST_COMPUTE), nodeMask);
	m_CmdListPools[CMDQUEUE_COPY].Init(D3D12_COMMAND_LIST_TYPE(CMDLIST_COPY), nodeMask);

	m_CmdFenceSet.Init();

	ZeroMemory(&m_FrameFenceValuesSubmitted, sizeof(m_FrameFenceValuesSubmitted));
	ZeroMemory(&m_FrameFenceValuesCompleted, sizeof(m_FrameFenceValuesCompleted));

	m_FrameFenceCursor = 0;
}

//---------------------------------------------------------------------------------------------------------------------
CCommandScheduler::~CCommandScheduler()
{

}

//---------------------------------------------------------------------------------------------------------------------
void CCommandScheduler::BeginScheduling()
{
	m_CmdListPools[CMDQUEUE_GRAPHICS].AcquireCommandList(m_pCmdLists[CMDQUEUE_GRAPHICS]);
	m_CmdListPools[CMDQUEUE_COMPUTE].AcquireCommandList(m_pCmdLists[CMDQUEUE_COMPUTE]);
	m_CmdListPools[CMDQUEUE_COPY].AcquireCommandList(m_pCmdLists[CMDQUEUE_COPY]);

	m_pCmdLists[CMDQUEUE_GRAPHICS]->Begin();
	m_pCmdLists[CMDQUEUE_GRAPHICS]->SetResourceAndSamplerStateHeaps();

	m_pCmdLists[CMDQUEUE_COMPUTE]->Begin();
	m_pCmdLists[CMDQUEUE_COMPUTE]->SetResourceAndSamplerStateHeaps();

	m_pCmdLists[CMDQUEUE_COPY]->Begin();
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandScheduler::RecreateCommandListPool(int queueIndex, uint nodeMask)
{
	CeaseCommandQueue(queueIndex, true);

	m_CmdListPools[queueIndex].Clear();
	m_CmdListPools[queueIndex].Configure();
	m_CmdListPools[queueIndex].Init(CCommandListPool::m_MapQueueType[queueIndex], nodeMask);

	ResumeCommandQueue(queueIndex);

	return true;
}

void CCommandScheduler::CeaseCommandQueue(int queueIndex, bool bWait)
{
	DX12_ASSERT(m_pCmdLists[queueIndex] != nullptr, "CommandList hasn't been allocated!");

	for (const auto& cb : m_ceaseCallbacks)
		cb.second(cb.first, queueIndex);

	m_pCmdLists[queueIndex]->End();
	m_CmdListPools[queueIndex].ForfeitCommandList(m_pCmdLists[queueIndex], bWait);
}

void CCommandScheduler::ResumeCommandQueue(int queueIndex)
{
	DX12_ASSERT(m_pCmdLists[queueIndex] == nullptr, "CommandList hasn't been submitted!");
	m_CmdListPools[queueIndex].AcquireCommandList(m_pCmdLists[queueIndex]);

	m_pCmdLists[queueIndex]->Begin();
	if (queueIndex != CMDQUEUE_COPY)
		m_pCmdLists[queueIndex]->SetResourceAndSamplerStateHeaps();

	for (const auto& cb : m_resumeCallbacks)
		cb.second(cb.first, queueIndex);
}

void CCommandScheduler::CeaseAllCommandQueues(bool bWait)
{
	CeaseCommandQueue(CMDQUEUE_GRAPHICS, bWait);
	CeaseCommandQueue(CMDQUEUE_COMPUTE, bWait);
	CeaseCommandQueue(CMDQUEUE_COPY, bWait);
}

void CCommandScheduler::ResumeAllCommandQueues()
{
	ResumeCommandQueue(CMDQUEUE_GRAPHICS);
	ResumeCommandQueue(CMDQUEUE_COMPUTE);
	ResumeCommandQueue(CMDQUEUE_COPY);
}

void CCommandScheduler::SubmitCommands(int queueIndex, bool bWait)
{
	CeaseCommandQueue(queueIndex, bWait);
	ResumeCommandQueue(queueIndex);
}

void CCommandScheduler::SubmitCommands(int queueIndex, bool bWait, const UINT64 fenceValue)
{
	if (IsCommandListUtilized(queueIndex, fenceValue))
	{
#ifdef DX12_STATS
		m_NumCommandListSplits++;
#endif // DX12_STATS

		SubmitCommands(queueIndex, bWait);
	}
}

void CCommandScheduler::SubmitAllCommands(bool bWait, const UINT64 (&fenceValues)[CMDQUEUE_NUM])
{
	SubmitCommands(CMDQUEUE_GRAPHICS, bWait, fenceValues[CMDQUEUE_GRAPHICS]);
	SubmitCommands(CMDQUEUE_COMPUTE, bWait, fenceValues[CMDQUEUE_COMPUTE]);
	SubmitCommands(CMDQUEUE_COPY, bWait, fenceValues[CMDQUEUE_COPY]);
}

void CCommandScheduler::SubmitAllCommands(bool bWait, const FVAL64(&fenceValues)[CMDQUEUE_NUM])
{
	SubmitCommands(CMDQUEUE_GRAPHICS, bWait, fenceValues[CMDQUEUE_GRAPHICS]);
	SubmitCommands(CMDQUEUE_COMPUTE, bWait, fenceValues[CMDQUEUE_COMPUTE]);
	SubmitCommands(CMDQUEUE_COPY, bWait, fenceValues[CMDQUEUE_COPY]);
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

#ifdef DX12_STATS
	m_NumCommandListOverflows = 0;
	m_NumCommandListSplits = 0;
#endif // DX12_STATS
}

}
