// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKCommandList.hpp"
#include "VKInstance.hpp"

#define VK_REPLACE_BACKBUFFER

namespace NCryVulkan
{

//---------------------------------------------------------------------------------------------------------------------
CCommandList::CCommandList(CCommandListPool& rPool)
	: CDeviceObject(rPool.GetDevice())
	, m_rPool(rPool)
	, m_CmdQueue(VK_NULL_HANDLE)
	, m_CmdAllocator(VK_NULL_HANDLE)
	, m_CmdList(VK_NULL_HANDLE)
	, m_VkDevice(VK_NULL_HANDLE)
	, m_CurrentNumPendingWSemaphores(0)
	, m_CurrentNumPendingSSemaphores(0)
	, m_CurrentNumPendingWFences(0)
	, m_CurrentNumPendingSFences(0)
	, m_CurrentNumPendingMBarriers(0)
	, m_CurrentNumPendingBBarriers(0)
	, m_CurrentNumPendingIBarriers(0)
	, m_CurrentFenceValue(0)
	, m_eListType(rPool.GetVkQueueType())
	, m_eState(CLSTATE_FREE)
{
	for (int i = 0; i < 64; ++i)
		m_PendingWStageMaskHeap[i] = 0x0001FFFF;
}

//---------------------------------------------------------------------------------------------------------------------
CCommandList::~CCommandList()
{
	if (m_CmdList)
	{
		vkFreeCommandBuffers(m_VkDevice, m_CmdAllocator, 1, &m_CmdList);
	}
	if (m_CmdAllocator)
	{
		vkDestroyCommandPool(m_VkDevice, m_CmdAllocator, nullptr);
	}
}

//---------------------------------------------------------------------------------------------------------------------

bool CCommandList::Init(UINT64 currentFenceValue)
{
	m_VkDevice = GetDevice()->GetVkDevice();

	m_nCommands = 0;

	m_CurrentNumPendingWSemaphores = 0;
	m_CurrentNumPendingSSemaphores = 0;
	m_CurrentNumPendingWFences = 0;
	m_CurrentNumPendingSFences = 0;
	m_CurrentNumPendingMBarriers = 0;
	m_CurrentNumPendingBBarriers = 0;
	m_CurrentNumPendingIBarriers = 0;

	m_CurrentFenceValue = currentFenceValue;
	ZeroMemory(m_UsedFenceValues, sizeof(m_UsedFenceValues));

	if (!m_CmdQueue)
	{
		m_CmdQueue = m_rPool.GetVkCommandQueue();
	}

	VkQueueFlagBits eCmdListType = m_rPool.GetVkQueueType();
	uint32 nCmdListFamily = m_rPool.GetVkQueueFamily();

	if (!m_CmdAllocator)
	{
		VkCommandPoolCreateInfo PoolInfo;
		ZeroStruct(PoolInfo);
		
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		PoolInfo.queueFamilyIndex = nCmdListFamily;

		VkCommandPool CmdAllocator = VK_NULL_HANDLE;
		if (VK_SUCCESS != vkCreateCommandPool(m_VkDevice, &PoolInfo, nullptr, &CmdAllocator))
		{
			VK_ERROR("Could not create command allocator!");
			return false;
		}

		m_CmdAllocator = CmdAllocator;
	}

	if (!m_CmdList)
	{
		VkCommandBufferAllocateInfo BufferInfo;
		ZeroStruct(BufferInfo);

		BufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		BufferInfo.commandPool = m_CmdAllocator;
		BufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		BufferInfo.commandBufferCount = 1;

		VkCommandBuffer CmdList = VK_NULL_HANDLE;
		if (VK_SUCCESS != vkAllocateCommandBuffers(m_VkDevice, &BufferInfo, &CmdList))
		{
			VK_ERROR("Could not create command list");
			return false;
		}

		m_CmdList = CmdList;
	}

#ifdef VK_STATS
	m_NumWaitsGPU = 0;
	m_NumWaitsCPU = 0;
#endif // VK_STATS

	return true;
}

void CCommandList::Register()
{
	UINT64 nextFenceValue = m_rPool.GetCurrentFenceValue() + 1;

	// Increment fence on allocation, this has the effect that
	// acquired CommandLists need to be submitted in-order to prevent
	// dead-locking
	m_rPool.SetCurrentFenceValue(nextFenceValue);

	Init(nextFenceValue);

	m_eState = CLSTATE_STARTED;
}

void CCommandList::Begin()
{
	const VkCommandBufferBeginInfo Info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr };

	VkResult res = vkBeginCommandBuffer(m_CmdList, &Info);
	VK_ASSERT(res == VK_SUCCESS, "Could not open command list!");
}

void CCommandList::End()
{
	if (IsUtilized())
	{
		PendingResourceBarriers();
	}

	VkResult res = vkEndCommandBuffer(m_CmdList);
	VK_ASSERT(res == VK_SUCCESS, "Could not close command list!");

	m_eState = CLSTATE_COMPLETED;
}

void CCommandList::Schedule()
{
	if (m_eState < CLSTATE_SCHEDULED)
	{
		m_eState = CLSTATE_SCHEDULED;

#ifdef VK_OMITTABLE_COMMANDLISTS
		const bool bRecyclable = (m_rPool.GetCurrentFenceValue() == m_CurrentFenceValue);
		if (bRecyclable & !IsUtilized())
		{
			// Rewind fence value and don't declare submitted
			m_rPool.SetCurrentFenceValue(m_CurrentFenceValue - 1);

			// The command-list doesn't "exist" now (makes all IsFinished() checks return true)
			m_CurrentFenceValue = 0;
		}
		else
#endif
		{
			m_rPool.SetSubmittedFenceValue(m_CurrentFenceValue);
		}
	}
}

void CCommandList::Submit()
{
	if (IsUtilized())
	{
		// Inject a Wait() into the CommandQueue prior to executing it to wait for all required resources being available either readable or writable
		// *INDENT-OFF*
#ifdef VK_IN_ORDER_SUBMISSION
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COMPUTE ] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE ]);
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_GRAPHICS] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS]);
		         m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COPY    ] = std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COPY    ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY    ]);
#else
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COMPUTE ], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COMPUTE ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COMPUTE ]));
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_GRAPHICS], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_GRAPHICS], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_GRAPHICS]));
		std::upr(m_UsedFenceValues[CMDTYPE_ANY][CMDQUEUE_COPY    ], std::max(m_UsedFenceValues[CMDTYPE_READ][CMDQUEUE_COPY    ], m_UsedFenceValues[CMDTYPE_WRITE][CMDQUEUE_COPY    ]));
#endif
		// *INDENT-ON*

		m_rPool.WaitForFenceOnGPU(m_UsedFenceValues[CMDTYPE_ANY], m_CurrentNumPendingWSemaphores, m_PendingWSemaphoreHeap);
	}

#ifdef VK_OMITTABLE_COMMANDLISTS
	if (IsUtilized())
#endif
	{
		// Inject the signal of the utilized fence to unblock code which picked up the fence of the command-list (even if it doesn't have contents)
		SignalFenceOnGPU();

		VK_LOG(VK_CONCURRENCY_ANALYZER, "######################################## END [%s %lld] CL ########################################",
			m_rPool.GetFenceID() == CMDQUEUE_GRAPHICS ? "gfx" : m_rPool.GetFenceID() == CMDQUEUE_COMPUTE ? "cmp" : "cpy",
			m_rPool.GetSubmittedFenceValue());
	}

	if (IsUtilized())
	{
		// Then inject the Execute() which is possibly blocked by the Wait()
		VkCommandBuffer ppCommandLists[] = { GetVkCommandList() };

		VkNaryFence* pFence = m_rPool.GetVkFence();
		m_rPool.GetAsyncCommandQueue().ExecuteCommandLists(
			1, ppCommandLists, // TODO: allow to submit multiple command-lists in one go
			m_CurrentNumPendingWSemaphores, &m_PendingWSemaphoreHeap[0], &m_PendingWStageMaskHeap[0],
			m_CurrentNumPendingSSemaphores, &m_PendingSSemaphoreHeap[0],
			pFence->first[m_CurrentFenceValue % pFence->first.size()], m_CurrentFenceValue);
	}

	m_eState = CLSTATE_SUBMITTED;
}

void CCommandList::Clear()
{
	m_eState = CLSTATE_CLEARING;
	m_rPool.GetAsyncCommandQueue().ResetCommandList(this);
}

bool CCommandList::Reset()
{
	HRESULT ret = VK_SUCCESS;

	if (m_CmdList)
	{
		PROFILE_FRAME("vkResetCommandBuffer");
		ret = vkResetCommandBuffer(m_CmdList, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		VK_ASSERT(ret == VK_SUCCESS);
	}

	m_eState = CLSTATE_FREE;
	m_nCommands = 0;
	return ret == VK_SUCCESS;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandList::ResourceBarrier(UINT NumBarriers, const VkMemoryBarrier* pBarriers)
{
	if (VK_BARRIER_FUSION && CRenderer::CV_r_VkBatchResourceBarriers)
	{
#if 1
		if (CRenderer::CV_r_VkBatchResourceBarriers > 1)
		{
			CRY_ASSERT(m_CurrentNumPendingMBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
			{
				UINT j = m_CurrentNumPendingMBarriers;

				if (j == m_CurrentNumPendingMBarriers)
				{
					m_PendingMBarrierHeap[m_CurrentNumPendingMBarriers] = pBarriers[i];
					m_CurrentNumPendingMBarriers += 1;
					m_nCommands += CLCOUNT_BARRIER * 1;
				}
			}
		}
		else
#endif
		{
			CRY_ASSERT(m_CurrentNumPendingMBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
				m_PendingMBarrierHeap[m_CurrentNumPendingMBarriers + i] = pBarriers[i];

			m_CurrentNumPendingMBarriers += NumBarriers;
			m_nCommands += CLCOUNT_BARRIER * NumBarriers;
		}
	}
	else
	{
		vkCmdPipelineBarrier(m_CmdList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			NumBarriers, pBarriers,
			0, nullptr,
			0, nullptr);

		m_nCommands += CLCOUNT_BARRIER;
	}
}

void CCommandList::ResourceBarrier(UINT NumBarriers, const VkBufferMemoryBarrier* pBarriers)
{
	if (VK_BARRIER_FUSION && CRenderer::CV_r_VkBatchResourceBarriers)
	{
#if 1
		if (CRenderer::CV_r_VkBatchResourceBarriers > 1)
		{
			CRY_ASSERT(m_CurrentNumPendingBBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
			{
				UINT j = m_CurrentNumPendingBBarriers;

				if (j == m_CurrentNumPendingBBarriers)
				{
					m_PendingBBarrierHeap[m_CurrentNumPendingBBarriers] = pBarriers[i];
					m_CurrentNumPendingBBarriers += 1;
					m_nCommands += CLCOUNT_BARRIER * 1;
				}
				else
				{
					if (m_PendingBBarrierHeap[j].offset == m_PendingBBarrierHeap[j].offset &&
						m_PendingBBarrierHeap[j].size == m_PendingBBarrierHeap[j].size)
					{
						m_CurrentNumPendingBBarriers--;

						memmove(m_PendingBBarrierHeap + j, m_PendingBBarrierHeap + j + 1, (m_CurrentNumPendingBBarriers - j) * sizeof(VkBufferMemoryBarrier));
					}
				}
			}
		}
		else
#endif
		{
			CRY_ASSERT(m_CurrentNumPendingBBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
				m_PendingBBarrierHeap[m_CurrentNumPendingBBarriers + i] = pBarriers[i];

			m_CurrentNumPendingBBarriers += NumBarriers;
			m_nCommands += CLCOUNT_BARRIER * NumBarriers;
		}
	}
	else
	{
		vkCmdPipelineBarrier(m_CmdList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			0, nullptr,
			NumBarriers, pBarriers,
			0, nullptr);

		m_nCommands += CLCOUNT_BARRIER;
	}
}

void CCommandList::ResourceBarrier(UINT NumBarriers, const VkImageMemoryBarrier* pBarriers)
{
	if (VK_BARRIER_FUSION && CRenderer::CV_r_VkBatchResourceBarriers)
	{
#if 1
		if (CRenderer::CV_r_VkBatchResourceBarriers > 1)
		{
			CRY_ASSERT(m_CurrentNumPendingIBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
			{
				UINT j = 0;

				for (; j < m_CurrentNumPendingIBarriers; ++j)
				{
					if (pBarriers[i].image == m_PendingIBarrierHeap[j].image &&
						pBarriers[i].subresourceRange.aspectMask     == m_PendingIBarrierHeap[j].subresourceRange.aspectMask &&
						pBarriers[i].subresourceRange.baseMipLevel   == m_PendingIBarrierHeap[j].subresourceRange.baseMipLevel &&
						pBarriers[i].subresourceRange.levelCount     == m_PendingIBarrierHeap[j].subresourceRange.levelCount &&
						pBarriers[i].subresourceRange.baseArrayLayer == m_PendingIBarrierHeap[j].subresourceRange.baseArrayLayer &&
						pBarriers[i].subresourceRange.layerCount     == m_PendingIBarrierHeap[j].subresourceRange.layerCount)
					{
						assert(m_PendingIBarrierHeap[j].newLayout == pBarriers[i].oldLayout);
						m_PendingIBarrierHeap[j].newLayout = pBarriers[i].newLayout;
						break;
					}
				}

				if (j == m_CurrentNumPendingIBarriers)
				{
					m_PendingIBarrierHeap[m_CurrentNumPendingIBarriers] = pBarriers[i];
					m_CurrentNumPendingIBarriers += 1;
					m_nCommands += CLCOUNT_BARRIER * 1;
				}
				else
				{
					if (m_PendingIBarrierHeap[j].oldLayout == m_PendingIBarrierHeap[j].newLayout)
					{
						m_CurrentNumPendingIBarriers--;

						memmove(m_PendingIBarrierHeap + j, m_PendingIBarrierHeap + j + 1, (m_CurrentNumPendingIBarriers - j) * sizeof(VkImageMemoryBarrier));
					}
				}
			}
		}
		else
#endif
		{
			CRY_ASSERT(m_CurrentNumPendingIBarriers + NumBarriers < 256);
			for (UINT i = 0; i < NumBarriers; ++i)
				m_PendingIBarrierHeap[m_CurrentNumPendingIBarriers + i] = pBarriers[i];

			m_CurrentNumPendingIBarriers += NumBarriers;
			m_nCommands += CLCOUNT_BARRIER * NumBarriers;
		}
	}
	else
	{
		vkCmdPipelineBarrier(m_CmdList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			0, nullptr,
			0, nullptr,
			NumBarriers, pBarriers);

		m_nCommands += CLCOUNT_BARRIER;
	}
}

void CCommandList::PendingResourceBarriers()
{
	if (VK_BARRIER_FUSION && (m_CurrentNumPendingMBarriers + m_CurrentNumPendingBBarriers + m_CurrentNumPendingIBarriers))
	{
		vkCmdPipelineBarrier(m_CmdList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
			m_CurrentNumPendingMBarriers, m_PendingMBarrierHeap,
			m_CurrentNumPendingBBarriers, m_PendingBBarrierHeap,
			m_CurrentNumPendingIBarriers, m_PendingIBarrierHeap);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CCommandList::PresentSwapChain(CSwapChain* pVKSwapChain)
{
	CImageResource& pBB = pVKSwapChain->GetCurrentBackBuffer();
	pBB.TrackAnnouncedAccess(VK_ACCESS_MEMORY_READ_BIT);
	pBB.TransitionBarrier(this, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkQueueFlagBits CCommandListPool::m_MapQueueType[CMDQUEUE_NUM] =
{
	VkQueueFlagBits(CMDLIST_TYPE_DIRECT),
	VkQueueFlagBits(CMDLIST_TYPE_COMPUTE),
	VkQueueFlagBits(CMDLIST_TYPE_COPY)
};

std::pair<uint32, uint32> CCommandListPool::m_MapQueueBits[1U << 4] =
{
	std::make_pair(0, 0)
};

uint32 CCommandListPool::m_MapQueueIndices[32] =
{
	0
};

//---------------------------------------------------------------------------------------------------------------------
CCommandListPool::CCommandListPool(CDevice* device, CCommandListFenceSet& rCmdFence, int nPoolFenceId)
	: m_pDevice(device)
	, m_rCmdFences(rCmdFence)
	, m_nPoolFenceId(nPoolFenceId)
	, m_nPoolFamily(0)
	, m_nPoolIndex(0)
	, m_CmdQueue(VK_NULL_HANDLE)
{
#ifdef VK_STATS
	m_PeakNumCommandListsAllocated = 0;
	m_PeakNumCommandListsInFlight = 0;
#endif // VK_STATS

	Configure();
}

//---------------------------------------------------------------------------------------------------------------------
CCommandListPool::~CCommandListPool()
{
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandListPool::Configure()
{
	m_MapQueueType[CMDQUEUE_GRAPHICS] = VkQueueFlagBits(CMDLIST_TYPE_DIRECT);
	m_MapQueueType[CMDQUEUE_COMPUTE] = VkQueueFlagBits(CMDLIST_TYPE_COMPUTE /*CRenderer::CV_r_VkHardwareComputeQueue & 7*/);
	m_MapQueueType[CMDQUEUE_COPY] = VkQueueFlagBits(CMDLIST_TYPE_COPY /*CRenderer::CV_r_VkHardwareCopyQueue & 7*/);

	memset(m_MapQueueBits, ~0U, sizeof(m_MapQueueBits));
	memset(m_MapQueueIndices, 0U, sizeof(m_MapQueueIndices));
	if (const SPhysicalDeviceInfo* pInfo = m_pDevice->GetPhysicalDeviceInfo())
	{
		for (int f = 0, n = pInfo->queueFamilyProperties.size(); f < n; ++f)
		{
			int queueFlags = pInfo->queueFamilyProperties[f].queueFlags;
			int queueCount = pInfo->queueFamilyProperties[f].queueCount;

			// Add TRANSFER_BIT if missing (implied)
			queueFlags |= !!(queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) * VK_QUEUE_TRANSFER_BIT;

			m_MapQueueBits[queueFlags] = std::make_pair(f, queueCount);
		}

		for (int i = 0x0; i <= 0x3; ++i)
		for (int f = 0x0; f <= 0xF; ++f)
		for (int n = 0x0; n <= 0x3; ++n)
		{
			int queueFlags = f & ~(1U << n);

			if (m_MapQueueBits[queueFlags].first == 0xFFFFFFFF)
				m_MapQueueBits[queueFlags] = m_MapQueueBits[f];
		}
	}
}

bool CCommandListPool::Init(VkQueueFlagBits eType /*= VkQueueFlagBits(CMDLIST_DEFAULT)*/)
{
	if (!m_CmdQueue)
	{
		// In-order execution creating fenceValue
		m_LastSignalledFenceValue = 0;

		m_ePoolType = eType;
		m_nPoolFamily = m_MapQueueBits[eType].first;
		m_nPoolIndex = m_MapQueueIndices[m_nPoolFamily];

		if (m_nPoolFamily == 0xFFFFFFFF)
		{
			VK_ERROR("Could not create command queue");
			return false;
		}

		// If more command-list pools for this queue-type are created, assign an incrementing index to each one.
		m_MapQueueIndices[m_nPoolFamily] = (m_MapQueueIndices[m_nPoolFamily] + 1) % m_MapQueueBits[eType].second;

		// Unfailable
		VkQueue CmdQueue = VK_NULL_HANDLE;
		vkGetDeviceQueue(m_pDevice->GetVkDevice(), m_nPoolFamily, m_nPoolIndex, &CmdQueue);

		m_CmdQueue = CmdQueue;
	}

	m_AsyncCommandQueue.Init(this);

#ifdef VK_STATS
	m_PeakNumCommandListsAllocated = 0;
	m_PeakNumCommandListsInFlight = 0;
#endif // VK_STATS

	return true;
}

void CCommandListPool::Clear()
{
	// No running command lists of any type allowed:
	// * trigger Live-to-Busy transitions
	// * trigger Busy-to-Free transitions
	while (m_LiveCommandLists.size() || m_BusyCommandLists.size())
		ScheduleCommandLists(); 

	m_FreeCommandLists.clear();
	m_AsyncCommandQueue.Clear();

	m_CmdQueue = VK_NULL_HANDLE;
	m_nPoolIndex = 0;
	m_nPoolFamily = 0xFFFFFFFF;
	m_ePoolType = VkQueueFlagBits(0);

	// In-order execution creating fenceValue
	m_LastSignalledFenceValue = 0;
}

//---------------------------------------------------------------------------------------------------------------------
void CCommandListPool::ScheduleCommandLists()
{
	// Remove finished command-lists from the head of the live-list
	while (m_LiveCommandLists.size())
	{
		VK_PTR(CCommandList) pCmdList = m_LiveCommandLists.front();

		// free -> complete -> submitted -> finished -> clearing -> free
		if (pCmdList->IsFinishedOnGPU() && !pCmdList->IsClearing())
		{
			m_LiveCommandLists.pop_front();
			m_BusyCommandLists.push_back(pCmdList);

			pCmdList->Clear();
		}
		else
		{
			break;
		}
	}

	// Submit completed but not yet submitted command-lists from the head of the live-list
	for (uint32 t = 0; t < m_LiveCommandLists.size(); ++t)
	{
		VK_PTR(CCommandList) pCmdList = m_LiveCommandLists[t];

		if (pCmdList->IsScheduled() && !pCmdList->IsSubmitted())
		{
			pCmdList->Submit();
		}

		if (!pCmdList->IsSubmitted())
		{
			break;
		}
	}

	// Remove cleared/deallocated command-lists from the head of the busy-list
	while (m_BusyCommandLists.size())
	{
		VK_PTR(CCommandList) pCmdList = m_BusyCommandLists.front();

		// free -> complete -> submitted -> finished -> clearing -> free
		if (pCmdList->IsFree())
		{
			m_BusyCommandLists.pop_front();
			m_FreeCommandLists.push_back(pCmdList);
		}
		else
		{
			break;
		}
	}
}

void CCommandListPool::CreateOrReuseCommandList(VK_PTR(CCommandList)& result)
{
	if (m_FreeCommandLists.empty())
	{
		result = new CCommandList(*this);
	}
	else
	{
		result = m_FreeCommandLists.front();

		m_FreeCommandLists.pop_front();
	}

	result->Register();
	m_LiveCommandLists.push_back(result);

#ifdef VK_STATS
	m_PeakNumCommandListsAllocated = std::max(m_PeakNumCommandListsAllocated, m_LiveCommandLists.size() + m_BusyCommandLists.size() + m_FreeCommandLists.size());
	m_PeakNumCommandListsInFlight = std::max(m_PeakNumCommandListsInFlight, m_LiveCommandLists.size());
#endif // VK_STATS
}

void CCommandListPool::AcquireCommandList(VK_PTR(CCommandList)& result) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);

	ScheduleCommandLists();

	CreateOrReuseCommandList(result);
}

void CCommandListPool::ForfeitCommandList(VK_PTR(CCommandList)& result, bool bWait) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);
	VK_PTR(CCommandList) pWaitable = result;

	{
		VK_ASSERT(result->IsCompleted(), "It's not possible to forfeit an unclosed command list!");
		result->Schedule();
		result = nullptr;
	}

	ScheduleCommandLists();

	if (bWait)
		pWaitable->WaitForFinishOnCPU();
}

void CCommandListPool::AcquireCommandLists(uint32 numCLs, VK_PTR(CCommandList)* results) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);

	ScheduleCommandLists();

	for (uint32 i = 0; i < numCLs; ++i)
		CreateOrReuseCommandList(results[i]);
}

void CCommandListPool::ForfeitCommandLists(uint32 numCLs, VK_PTR(CCommandList)* results, bool bWait) threadsafe
{
	static CryCriticalSectionNonRecursive csThreadSafeScope;
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(csThreadSafeScope);
	VK_PTR(CCommandList) pWaitable = results[numCLs - 1];

	int32 i = numCLs;
	while (--i >= 0)
	{
		VK_ASSERT(results[i]->IsCompleted(), "It's not possible to forfeit an unclosed command list!");
		results[i]->Schedule();
		results[i] = nullptr;
	}

	ScheduleCommandLists();

	if (bWait)
		pWaitable->WaitForFinishOnCPU();
}

}
