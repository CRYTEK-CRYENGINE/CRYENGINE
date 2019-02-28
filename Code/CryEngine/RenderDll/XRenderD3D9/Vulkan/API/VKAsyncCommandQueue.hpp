// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryThreading/IThreadManager.h>
#include <concqueue/concqueue.hpp>
#include "VKBase.hpp"

namespace NCryVulkan
{

class CCommandListPool;

class CAsyncCommandQueue : public IThread
{
public:
	static const int MAX_FRAMES_GPU_LAG = 1;   // Maximum number of frames GPU can lag behind CPU. TODO: tie to cvar

	CAsyncCommandQueue();
	~CAsyncCommandQueue();

	bool IsSynchronous();
	void Init(CCommandListPool* pCommandListPool);
	void Clear();
	void Flush(UINT64 lowerBoundFenceValue = ~0ULL);
	void FlushNextPresent();
	void SignalStop() { m_bStopRequested = true; }

	// Equates to the number of pending Present() calls
	int  GetQueuedFramesCount() const { return m_QueuedFramesCounter; }

	void Present(
		VkSwapchainKHR SwapChain, UINT Index,
		INT NumPendingWSemaphores, VkSemaphore const* pPendingWSemaphoreHeap,
		VkResult* pPresentResult);
	void ResetCommandList(
		CCommandList* pCommandList);
	void ExecuteCommandLists(
		UINT NumCommandLists, VkCommandBuffer const* ppCommandLists,
		INT NumPendingWSemaphores, VkSemaphore const* pPendingWSemaphoreHeap, VkPipelineStageFlags const* pPendingWStageMaskHeap,
		INT NumPendingSSemaphores, VkSemaphore const* pPendingSSemaphoreHeap,
		VkFence PendingFence, UINT64 PendingFenceValue);

#ifdef VK_LINKEDADAPTER
	void   SyncAdapters(VkFence pFence, const UINT64 Value);
#endif

private:

	enum eTaskType
	{
		eTT_ExecuteCommandList,
		eTT_ResetCommandList,
		eTT_PresentBackbuffer,
		eTT_SyncAdapters
	};

	struct STaskArgs
	{
		CCommandListPool* pCommandListPool;
		volatile int*     QueueFramesCounter;
	};

	struct SExecuteCommandlist
	{
		VkCommandBuffer CommandList;
		INT             NumWaitableSemaphores;
		INT             NumSignalableSemaphores;
		VkSemaphore     WaitableSemaphores[64];
		VkPipelineStageFlags StageMasks[64];
		VkSemaphore     SignalableSemaphores[64];
		VkFence         Fence;
		UINT64          FenceValue;

		void            Process(const STaskArgs& args);
	};

	struct SResetCommandlist
	{
		CCommandList* CommandList;

		void          Process(const STaskArgs& args);
	};

#ifdef VK_LINKEDADAPTER
	struct SSyncAdapters
	{
		VkNaryFence* Fence;
		UINT64           FenceValue;

		void             Process(const STaskArgs& args);
	};
#endif

	struct SPresentBackbuffer
	{
		VkSwapchainKHR   SwapChain;
		INT              NumWaitableSemaphores;
		VkSemaphore      WaitableSemaphores[64];
		uint32           Index;
		VkResult*        pPresentResult;

		void             Process(const STaskArgs& args);
	};

	struct SSubmissionTask
	{
		eTaskType type;

		union
		{
			SExecuteCommandlist ExecuteCommandList;
			SResetCommandlist   ResetCommandList;

#ifdef VK_LINKEDADAPTER
			SSyncAdapters       SyncAdapters;
#endif

			SPresentBackbuffer  PresentBackbuffer;
		} Data;

		template<typename TaskType>
		void Process(const STaskArgs& args)
		{
			TaskType* pTask = reinterpret_cast<TaskType*>(&Data);
			pTask->Process(args);
		}
	};

	template<typename TaskType>
	void AddTask(SSubmissionTask& task)
	{
		if (!IsSynchronous())
		{
			m_TaskQueue.enqueue(task);
			m_TaskEvent.Release();
		}
		else
		{
			Flush();
			STaskArgs taskArgs = { m_pCmdListPool, &m_QueuedFramesCounter };
			task.Process<TaskType>(taskArgs);
		}
	}

	void ThreadEntry() override;

	volatile int                            m_QueuedFramesCounter;
	volatile bool                           m_bStopRequested;
	volatile bool                           m_bSleeping;

	CCommandListPool*                       m_pCmdListPool;
	ConcQueue<UnboundMPSC, SSubmissionTask> m_TaskQueue;
	CryFastSemaphore                        m_TaskEvent;
};

}
