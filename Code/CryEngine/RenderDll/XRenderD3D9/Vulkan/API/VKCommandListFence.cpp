// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKCommandListFence.hpp"
#include "DriverD3D.h"

namespace NCryVulkan
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCommandListFence::CCommandListFence(CDevice* device)
	: m_pDevice(device)
	, m_CurrentValue(0)
	, m_SubmittedValue(0)
	, m_LastCompletedValue(0)
{
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFence::~CCommandListFence()
{
	for (int f = 0, n = m_Fence.first.size(); f < n; ++f)
	{
//		while ((vkWaitForFences(m_pDevice->GetVkDevice(), 1, &m_Fence.first[f], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
		vkDestroyFence(m_pDevice->GetVkDevice(), m_Fence.first[f], nullptr);
	}

	for (int f = 0, n = m_Fence.second.size(); f < n; ++f)
	{
//		while ((vkWaitForSemaphores(m_pDevice->GetVkDevice(), 1, &m_Fence.second[f], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
		vkDestroySemaphore(m_pDevice->GetVkDevice(), m_Fence.second[f], nullptr);
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandListFence::Init()
{
	VkFenceCreateInfo     FenceInfo;
	VkSemaphoreCreateInfo SemaphoreInfo;

	ZeroStruct(FenceInfo);
	ZeroStruct(SemaphoreInfo);

	FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceInfo.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT;

	SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int f = 0, n = m_Fence.first.size(); f < n; ++f)
	{
		VkFence fence;
		if (VK_SUCCESS != vkCreateFence(m_pDevice->GetVkDevice(), &FenceInfo, nullptr, &fence))
		{
			VK_ERROR("Could not create fence object!");
			return false;
		}

		m_Fence.first[f] = fence;
	}

	for (int f = 0, n = m_Fence.second.size(); f < n; ++f)
	{
		VkSemaphore semaphore;
		if (VK_SUCCESS != vkCreateSemaphore(m_pDevice->GetVkDevice(), &SemaphoreInfo, nullptr, &semaphore))
		{
			VK_ERROR("Could not create semaphore object!");
			return false;
		}

		m_Fence.second[f] = semaphore;
	}

	return true;
}

void CCommandListFence::WaitForFence(UINT64 fenceValue)
{
	if (!IsCompleted(fenceValue))
	{
		VK_LOG(VK_FENCE_ANALYZER, "Waiting CPU for fence: %lld (is %lld currently)", fenceValue, ProbeCompletion());
		{
#ifdef VK_LINKEDADAPTER
			// Waiting in a multi-adapter situation can be more complex
			if (m_pDevice->WaitForCompletion(m_Fence, fenceValue))
#endif
			if (m_LastCompletedValue < m_SubmittedValue)
			{
				const int size  = int(m_Fence.first.size());
				const int start = int((m_LastCompletedValue + 1) % size);
				const int end   = int((m_SubmittedValue     + 1) % size);

				m_FenceAccessLock.RLock();

				// TODO: optimize
				if (end > start)
				{
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  - start, &m_Fence.first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
				}
				else
				{
					if (start < size)
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), size - start, &m_Fence.first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
					if (end > 0)
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  -     0, &m_Fence.first[    0], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
				}

				m_FenceAccessLock.RUnlock();

			}
		}
		VK_LOG(VK_FENCE_ANALYZER, "Completed CPU on fence: %lld", ProbeCompletion());

		AdvanceCompletion();
	}
}

UINT64 CCommandListFence::AdvanceCompletion() threadsafe_const
{
	// Check current completed fence
	UINT64 currentCompletedValue = ProbeCompletion();

	if (m_LastCompletedValue < currentCompletedValue)
	{
		VK_LOG(VK_FENCE_ANALYZER, "Completed fence(s): %lld to %lld", m_LastCompletedValue + 1, currentCompletedValue);

		const int size  = int(m_Fence.first.size());
		const int start = int((m_LastCompletedValue  + 1) % size);
		const int end   = int((currentCompletedValue + 1) % size);

		m_FenceAccessLock.WLock();

		// TODO: optimize
		if (end > start)
		{
			vkResetFences(m_pDevice->GetVkDevice(), end  - start, &m_Fence.first[start]);
		}
		else
		{
			if (start < size)
			vkResetFences(m_pDevice->GetVkDevice(), size - start, &m_Fence.first[start]);
			if (end > 0)
			vkResetFences(m_pDevice->GetVkDevice(), end  -     0, &m_Fence.first[    0]);
		}

		m_FenceAccessLock.WUnlock();
	}

#ifdef VK_IN_ORDER_TERMINATION
	VK_ASSERT(m_LastCompletedValue <= currentCompletedValue, "Getting new fence which is older than the last!");
	// We do not allow smaller fences being submitted, and fences always complete in-order, no max() neccessary
	m_LastCompletedValue = currentCompletedValue;
#else
	// CLs may terminate in any order. Is it higher than last known completed fence? If so, update it!
	MaxFenceValue(m_LastCompletedValue, currentCompletedValue);
#endif

	return currentCompletedValue;
}

UINT64 CCommandListFence::ProbeCompletion() threadsafe_const
{
	// Check current completed fence
	UINT64 currentCompletedValue = m_LastCompletedValue;

	if (m_LastCompletedValue < m_SubmittedValue)
	{
		const int size  = int(m_Fence.first.size());
		const int start = int(m_LastCompletedValue + 1);
		const int end   = int(m_SubmittedValue     + 1);
			
		for (int i = start; i < end; ++i)
		{
			if (VK_SUCCESS != vkGetFenceStatus(m_pDevice->GetVkDevice(), m_Fence.first[i % size]))
				break;

			++currentCompletedValue;
		}
	}

	return currentCompletedValue;
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFenceSet::CCommandListFenceSet(CDevice* device)
	: m_pDevice(device)
{
	// *INDENT-OFF*
	m_LastCompletedValues[CMDQUEUE_GRAPHICS] = m_LastCompletedValues[CMDQUEUE_COMPUTE] = m_LastCompletedValues[CMDQUEUE_COPY] = 0;
	m_SubmittedValues    [CMDQUEUE_GRAPHICS] = m_SubmittedValues    [CMDQUEUE_COMPUTE] = m_SubmittedValues    [CMDQUEUE_COPY] = 0;
	m_SignalledValues    [CMDQUEUE_GRAPHICS] = m_SignalledValues    [CMDQUEUE_COMPUTE] = m_SignalledValues    [CMDQUEUE_COPY] = 0;
	m_CurrentValues      [CMDQUEUE_GRAPHICS] = m_CurrentValues      [CMDQUEUE_COMPUTE] = m_CurrentValues      [CMDQUEUE_COPY] = 0;
	// *INDENT-ON*
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListFenceSet::~CCommandListFenceSet()
{
	for (int i = 0; i < CMDQUEUE_NUM; ++i)
	{
		for (int f = 0, n = m_Fences[i].first.size(); f < n; ++f)
		{
//			while ((vkWaitForFences(m_pDevice->GetVkDevice(), 1, &m_Fences[i].first[f], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
			vkDestroyFence(m_pDevice->GetVkDevice(), m_Fences[i].first[f], nullptr);
		}

		for (int f = 0, n = m_Fences[i].second.size(); f < n; ++f)
		{
//			while ((vkWaitForSemaphore(m_pDevice->GetVkDevice(), 1, &m_Fences[i].second[f], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
			vkDestroySemaphore(m_pDevice->GetVkDevice(), m_Fences[i].second[f], nullptr);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
bool CCommandListFenceSet::Init()
{
	VkFenceCreateInfo     FenceInfo;
	VkSemaphoreCreateInfo SemaphoreInfo;

	ZeroStruct(FenceInfo);
	ZeroStruct(SemaphoreInfo);

	FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceInfo.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT;

	SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (int i = 0; i < CMDQUEUE_NUM; ++i)
	{
		for (int f = 0, n = m_Fences[i].first.size(); f < n; ++f)
		{
			VkFence fence;
			if (VK_SUCCESS != vkCreateFence(m_pDevice->GetVkDevice(), &FenceInfo, nullptr, &fence))
			{
				VK_ERROR("Could not create fence object!");
				return false;
			}

			m_Fences[i].first[f] = fence;
		}

		for (int f = 0, n = m_Fences[i].second.size(); f < n; ++f)
		{
			VkSemaphore semaphore;
			if (VK_SUCCESS != vkCreateSemaphore(m_pDevice->GetVkDevice(), &SemaphoreInfo, nullptr, &semaphore))
			{
				VK_ERROR("Could not create semaphore object!");
				return false;
			}

			m_Fences[i].second[f] = semaphore;
		}
	}

	return true;
}

void CCommandListFenceSet::SignalFence(const UINT64 fenceValue, const int id) threadsafe_const
{
	VK_ERROR("Fences can't be signaled by the CPU under Vulkan");
}

void CCommandListFenceSet::WaitForFence(const UINT64 fenceValue, const int id) threadsafe_const
{
	VK_LOG(VK_FENCE_ANALYZER, "Waiting CPU for fence %s: %lld (is %lld currently)",
	       id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
	       fenceValue,
	       ProbeCompletion(id));

	{
#ifdef VK_LINKEDADAPTER
		// NOTE: Waiting in a multi-adapter situation can be more complex
		if (!m_pDevice->WaitForCompletion(m_Fences[id], fenceValue))
#endif
		if (m_LastCompletedValues[id] < fenceValue)
		{
			VK_ASSERT(m_CurrentValues  [id] >= fenceValue, "Fence to be tested not allocated!");
			VK_ASSERT(m_SubmittedValues[id] >= fenceValue, "Fence to be tested not submitted!");
			VK_ASSERT(m_SignalledValues[id] >= fenceValue, "Fence to be tested not signalled!");

			const int size  = int(m_Fences[id].first.size());
			const int start = int((m_LastCompletedValues[id] + 1) % size);
			const int end   = int((fenceValue                + 1) % size);

			m_FenceAccessLock.RLock();

			// TODO: optimize
			if (end > start)
			{
				while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  - start, &m_Fences[id].first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
			}
			else
			{
				if (start < size)
				while ((vkWaitForFences(m_pDevice->GetVkDevice(), size - start, &m_Fences[id].first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
				if (end > 0)
				while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  -     0, &m_Fences[id].first[    0], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
			}

			m_FenceAccessLock.RUnlock();

		}
	}

	VK_LOG(VK_FENCE_ANALYZER, "Completed CPU on fence %s: %lld",
	       id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
	       ProbeCompletion(id));

	AdvanceCompletion(id);
}

void CCommandListFenceSet::WaitForFence(const UINT64 (&fenceValues)[CMDQUEUE_NUM]) threadsafe_const
{
	// TODO: the pool which waits for the fence can be omitted (in-order-execution)
	VK_LOG(VK_FENCE_ANALYZER, "Waiting CPU for fences: [%lld,%lld,%lld] (is [%lld,%lld,%lld] currently)",
	       fenceValues[CMDQUEUE_GRAPHICS],
	       fenceValues[CMDQUEUE_COMPUTE],
	       fenceValues[CMDQUEUE_COPY],
	       ProbeCompletion(CMDQUEUE_GRAPHICS),
	       ProbeCompletion(CMDQUEUE_COMPUTE),
	       ProbeCompletion(CMDQUEUE_COPY));

	{
#ifdef VK_LINKEDADAPTER
		if (m_pDevice->IsMultiAdapter())
		{
			// NOTE: Waiting in a multi-adapter situation can be more complex
			// In this case we can't collect all events to wait for all events at the same time
			for (int id = 0; id < CMDQUEUE_NUM; ++id)
			{
				if (fenceValues[id] && (m_LastCompletedValues[id] < fenceValues[id]))
				{
					m_pDevice->WaitForCompletion(m_Fences[id], fenceValues[id]);
				}
			}
		}
		else
#endif
		for (int id = 0; id < CMDQUEUE_NUM; ++id)
		{
			if (m_LastCompletedValues[id] < fenceValues[id])
			{
				VK_ASSERT(m_CurrentValues  [id] >= fenceValues[id], "Fence to be tested not allocated!");
				VK_ASSERT(m_SubmittedValues[id] >= fenceValues[id], "Fence to be tested not submitted!");
				VK_ASSERT(m_SignalledValues[id] >= fenceValues[id], "Fence to be tested not signalled!");

				const int size  = int(m_Fences[id].first.size());
				const int start = int((m_LastCompletedValues[id] + 1) % size);
				const int end   = int((fenceValues          [id] + 1) % size);

				// TODO: optimize
				if (end > start)
				{
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  - start, &m_Fences[id].first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
				}
				else
				{
					if (start < size)
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), size - start, &m_Fences[id].first[start], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
					if (end > 0)
					while ((vkWaitForFences(m_pDevice->GetVkDevice(), end  -     0, &m_Fences[id].first[    0], VK_TRUE, ~0ULL)) == VK_TIMEOUT);
				}
			}
		}
	}

	VK_LOG(VK_FENCE_ANALYZER, "Completed CPU on fences: [%lld,%lld,%lld]",
	       ProbeCompletion(CMDQUEUE_GRAPHICS),
	       ProbeCompletion(CMDQUEUE_COMPUTE),
	       ProbeCompletion(CMDQUEUE_COPY));

	AdvanceCompletion();
}

UINT64 CCommandListFenceSet::AdvanceCompletion(const int id) threadsafe_const
{
	// Check current completed fence
	UINT64 currentCompletedValue = ProbeCompletion(id);

	if (m_LastCompletedValues[id] < currentCompletedValue)
	{
		VK_LOG(VK_FENCE_ANALYZER, "Completed GPU fence %s: %lld to %lld", id == CMDQUEUE_GRAPHICS ? "gfx" : id == CMDQUEUE_COMPUTE ? "cmp" : "cpy", m_LastCompletedValues[id] + 1, currentCompletedValue);

		const int size  = int(m_Fences[id].first.size());
		const int start = int((m_LastCompletedValues[id] + 1) % size);
		const int end   = int((currentCompletedValue     + 1) % size);

		m_FenceAccessLock.WLock();

		// TODO: optimize
		if (end > start)
		{
			vkResetFences(m_pDevice->GetVkDevice(), end  - start, &m_Fences[id].first[start]);
		}
		else
		{
			if (start < size)
			vkResetFences(m_pDevice->GetVkDevice(), size - start, &m_Fences[id].first[start]);
			if (end > 0)
			vkResetFences(m_pDevice->GetVkDevice(), end  -     0, &m_Fences[id].first[    0]);
		}

		m_FenceAccessLock.WUnlock();
	}

#ifdef VK_IN_ORDER_TERMINATION
	VK_ASSERT(m_LastCompletedValues[id] <= currentCompletedValue, "Getting new fence which is older than the last!");
	// We do not allow smaller fences being submitted, and fences always complete in-order, no max() neccessary
	m_LastCompletedValues[id] = currentCompletedValue;
#else
	// CLs may terminate in any order. Is it higher than last known completed fence? If so, update it!
	MaxFenceValue(m_LastCompletedValues[id], currentCompletedValue);
#endif

	return currentCompletedValue;
}

UINT64 CCommandListFenceSet::ProbeCompletion(const int id) threadsafe_const
{
	// Check current completed fence
	UINT64 currentCompletedValue = m_LastCompletedValues[id];

	if (m_LastCompletedValues[id] < m_SignalledValues[id])
	{
		const int size  = int(m_Fences[id].first.size());
		const int start = int(m_LastCompletedValues[id] + 1);
		const int end   = int(m_SignalledValues    [id] + 1);
			
		m_FenceAccessLock.RLock();

		for (int i = start; i < end; ++i)
		{
			if (VK_SUCCESS != vkGetFenceStatus(m_pDevice->GetVkDevice(), m_Fences[id].first[i % size]))
				break;

			++currentCompletedValue;
		}

		m_FenceAccessLock.RUnlock();
	}

	return currentCompletedValue;
}

}
