// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12AsyncCommandQueue.hpp"
#include "DX12CommandList.hpp"
#include "DriverD3D.h"

#ifdef DX12_LINKEDADAPTER
	#include "Redirections/D3D12Device.inl"
#endif

namespace NCryDX12
{

void CAsyncCommandQueue::SExecuteCommandlist::Process(const STaskArgs& args)
{
	args.pCommandListPool->GetD3D12CommandQueue()->ExecuteCommandLists(1, &pCommandList);
}

void CAsyncCommandQueue::SResetCommandlist::Process(const STaskArgs& args)
{
	pCommandList->Reset();
}

void CAsyncCommandQueue::SSignalFence::Process(const STaskArgs& args)
{
	args.pCommandListPool->GetD3D12CommandQueue()->Signal(pFence, FenceValue);
	args.pCommandListPool->SetSignalledFenceValue(FenceValue);
}

void CAsyncCommandQueue::SWaitForFence::Process(const STaskArgs& args)
{
	args.pCommandListPool->GetD3D12CommandQueue()->Wait(pFence, FenceValue);
}

void CAsyncCommandQueue::SWaitForFences::Process(const STaskArgs& args)
{
	if (FenceValues[CMDQUEUE_GRAPHICS]) args.pCommandListPool->GetD3D12CommandQueue()->Wait(pFences[CMDQUEUE_GRAPHICS], FenceValues[CMDQUEUE_GRAPHICS]);
	if (FenceValues[CMDQUEUE_COMPUTE]) args.pCommandListPool->GetD3D12CommandQueue()->Wait(pFences[CMDQUEUE_COMPUTE], FenceValues[CMDQUEUE_COMPUTE]);
	if (FenceValues[CMDQUEUE_COPY]) args.pCommandListPool->GetD3D12CommandQueue()->Wait(pFences[CMDQUEUE_COPY], FenceValues[CMDQUEUE_COPY]);
}

#ifdef DX12_LINKEDADAPTER
void CAsyncCommandQueue::SSyncAdapters::Process(const STaskArgs& args)
{
	BroadcastableD3D12CommandQueue<2>* broadcastCQ = (BroadcastableD3D12CommandQueue<2>*)(args.pCommandListPool->GetD3D12CommandQueue());
	broadcastCQ->SyncAdapters(pFence, FenceValue);
}
#endif

void CAsyncCommandQueue::SPresentBackbuffer::Process(const STaskArgs& args)
{
	DWORD result = S_OK;

	#ifdef __dxgi1_3_h__
	if (Desc->Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
	{
		// Check if the swapchain is ready to accept another frame
		HANDLE frameLatencyWaitableObject = pSwapChain->GetFrameLatencyWaitableObject();
		result = WaitForSingleObjectEx(frameLatencyWaitableObject, 0, true);
	}

	if (Desc->Windowed && (Desc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING))
	{
		Flags |= DXGI_PRESENT_ALLOW_TEARING;
		SyncInterval = 0;
	}
	#endif

	if (result == S_OK)
	{
		*pPresentResult = pSwapChain->Present(SyncInterval, Flags);
	}

	CryInterlockedDecrement(args.QueueFramesCounter);
}

bool CAsyncCommandQueue::IsSynchronous()
{
	return !(CRenderer::CV_r_D3D12SubmissionThread & BIT(m_pCmdListPool->GetD3D12QueueType()));
}

CAsyncCommandQueue::CAsyncCommandQueue()
	: m_pCmdListPool(nullptr)
	, m_QueuedFramesCounter(0)
	, m_bStopRequested(false)
	, m_TaskEvent(INT_MAX, 0)
{
}

CAsyncCommandQueue::~CAsyncCommandQueue()
{
	Clear();
}

void CAsyncCommandQueue::Init(CCommandListPool* pCommandListPool)
{
	m_pCmdListPool = pCommandListPool;
	m_QueuedFramesCounter = 0;
	m_bStopRequested = false;
	m_bSleeping = true;

	GetISystem()->GetIThreadManager()->SpawnThread(this, "DX12 AsyncCommandQueue");
}

void CAsyncCommandQueue::Clear()
{
	SignalStop();
	Flush();
	m_TaskEvent.Release();

	GetISystem()->GetIThreadManager()->JoinThread(this, eJM_Join);

	m_pCmdListPool = nullptr;
}

void CAsyncCommandQueue::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
	for (int i = 0; i < NumCommandLists; ++i)
	{
		SSubmissionTask task;
		ZeroStruct(task);

		task.type = eTT_ExecuteCommandList;
		task.Data.ExecuteCommandList.pCommandList = ppCommandLists[i];

		AddTask<SExecuteCommandlist>(task);
	}
}

void CAsyncCommandQueue::ResetCommandList(CCommandList* pCommandList)
{
	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_ResetCommandList;
	task.Data.ResetCommandList.pCommandList = pCommandList;

	AddTask<SResetCommandlist>(task);
}

void CAsyncCommandQueue::Signal(ID3D12Fence* pFence, const UINT64 FenceValue)
{
	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_SignalFence;
	task.Data.SignalFence.pFence = pFence;
	task.Data.SignalFence.FenceValue = FenceValue;

	AddTask<SSignalFence>(task);
}

void CAsyncCommandQueue::Wait(ID3D12Fence* pFence, const UINT64 FenceValue)
{
	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_WaitForFence;
	task.Data.WaitForFence.pFence = pFence;
	task.Data.WaitForFence.FenceValue = FenceValue;

	AddTask<SWaitForFence>(task);
}

void CAsyncCommandQueue::Wait(ID3D12Fence** pFences, const UINT64 (&FenceValues)[CMDQUEUE_NUM])
{
	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_WaitForFences;
	task.Data.WaitForFences.pFences = pFences;
	task.Data.WaitForFences.FenceValues[CMDQUEUE_GRAPHICS] = FenceValues[CMDQUEUE_GRAPHICS];
	task.Data.WaitForFences.FenceValues[CMDQUEUE_COMPUTE] = FenceValues[CMDQUEUE_COMPUTE];
	task.Data.WaitForFences.FenceValues[CMDQUEUE_COPY] = FenceValues[CMDQUEUE_COPY];

	AddTask<SWaitForFences>(task);
}

#ifdef DX12_LINKEDADAPTER
void CAsyncCommandQueue::SyncAdapters(ID3D12Fence* pFence, const UINT64 FenceValue)
{
	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_SyncAdapters;
	task.Data.SyncAdapters.pFence = pFence;
	task.Data.SyncAdapters.FenceValue = FenceValue;

	AddTask<SSyncAdapters>(task);
}
#endif

void CAsyncCommandQueue::Present(IDXGISwapChain3ToCall* pSwapChain, HRESULT* pPresentResult, UINT SyncInterval, UINT Flags, const DXGI_SWAP_CHAIN_DESC& Desc, UINT bufferIndex)
{
	CryInterlockedIncrement(&m_QueuedFramesCounter);

	SSubmissionTask task;
	ZeroStruct(task);

	task.type = eTT_PresentBackbuffer;
	task.Data.PresentBackbuffer.pSwapChain = pSwapChain;
	task.Data.PresentBackbuffer.pPresentResult = pPresentResult;
	task.Data.PresentBackbuffer.Flags = Flags;
	task.Data.PresentBackbuffer.Desc = &Desc;
	task.Data.PresentBackbuffer.SyncInterval = SyncInterval;

	AddTask<SPresentBackbuffer>(task);

	{
		while (m_QueuedFramesCounter > MAX_FRAMES_GPU_LAG)
		{
			CryMT::CryYieldThread();
		}
	}
}

void CAsyncCommandQueue::Flush(UINT64 lowerBoundFenceValue)
{
	if (lowerBoundFenceValue != (~0ULL))
	{
		while (lowerBoundFenceValue > m_pCmdListPool->GetSignalledFenceValue())
		{
			CryMT::CryYieldThread();
		}
	}
	else
	{
		while (!m_bSleeping)
		{
			CryMT::CryYieldThread();
		}
	}
}

void CAsyncCommandQueue::FlushNextPresent()
{
	const int numQueuedFrames = m_QueuedFramesCounter;
	if (numQueuedFrames > 0)
	{
		while (numQueuedFrames == m_QueuedFramesCounter)
		{
			CryMT::CryYieldThread();
		}
	}
}

void CAsyncCommandQueue::ThreadEntry()
{
	const STaskArgs taskArgs = { m_pCmdListPool, &m_QueuedFramesCounter };
	SSubmissionTask task;

	while (!m_bStopRequested)
	{
		m_TaskEvent.Acquire();

		if (m_TaskQueue.dequeue(task))
		{
			switch (task.type)
			{
			case eTT_ExecuteCommandList:
				task.Process<SExecuteCommandlist>(taskArgs);
				break;
			case eTT_ResetCommandList:
				task.Process<SResetCommandlist>(taskArgs);
				break;
			case eTT_SignalFence:
				task.Process<SSignalFence>(taskArgs);
				break;
			case eTT_WaitForFence:
				task.Process<SWaitForFence>(taskArgs);
				break;
			case eTT_WaitForFences:
				task.Process<SWaitForFences>(taskArgs);
				break;
#ifdef DX12_LINKEDADAPTER
			case eTT_SyncAdapters:
				task.Process<SSyncAdapters>(taskArgs);
				break;
#endif
			case eTT_PresentBackbuffer:
				task.Process<SPresentBackbuffer>(taskArgs);
				break;
			}
		}
	}
}

}
