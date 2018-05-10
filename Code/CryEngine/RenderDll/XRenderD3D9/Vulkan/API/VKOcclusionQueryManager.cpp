// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKOcclusionQueryManager.hpp"

namespace NCryVulkan
{

COcclusionQueryManager::COcclusionQueryManager()
	: m_queryPool(VK_NULL_HANDLE)
	, m_bQueryInUse(0)
{
	m_fences.fill(0);
}

COcclusionQueryManager::~COcclusionQueryManager()
{
	vkDestroyQueryPool(GetDeviceObjectFactory().GetVKDevice()->GetVkDevice(), m_queryPool, nullptr);

	for (int i = 0; i < kMaxQueryCount; ++i)
		GetDeviceObjectFactory().ReleaseFence(m_fences[i]);
}

bool COcclusionQueryManager::Init(VkDevice device)
{
	VkQueryPoolCreateInfo poolInfo;
	poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = 0;
	poolInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
	poolInfo.queryCount = kMaxQueryCount;
	poolInfo.pipelineStatistics = 0;

	if (vkCreateQueryPool(device, &poolInfo, nullptr, &m_queryPool) == VK_SUCCESS)
		return true;

	return false;
}

COcclusionQueryManager::QueryHandle COcclusionQueryManager::CreateQuery()
{
	for (int query = 0; query < kMaxQueryCount; ++query)
	{
		if (!m_bQueryInUse[query])
			return query;
	}

	return InvalidQuery;
}

void COcclusionQueryManager::ReleaseQuery(COcclusionQueryManager::QueryHandle query)
{
	CRY_ASSERT(query >= 0 && query < kMaxQueryCount);
	m_bQueryInUse[query] = false;
}

void COcclusionQueryManager::BeginQuery(COcclusionQueryManager::QueryHandle query, VkCommandBuffer commandBuffer)
{
	CRY_ASSERT(query >= 0 && query < kMaxQueryCount);

	vkCmdResetQueryPool(commandBuffer, m_queryPool, query, 1);
	vkCmdBeginQuery(commandBuffer, m_queryPool, query, VK_QUERY_CONTROL_PRECISE_BIT);
}

void COcclusionQueryManager::EndQuery(COcclusionQueryManager::QueryHandle query, VkCommandBuffer commandBuffer)
{
	CRY_ASSERT(query >= 0 && query < kMaxQueryCount);

	vkCmdEndQuery(commandBuffer, m_queryPool, query);
	m_fences[query] = GetDeviceObjectFactory().GetVKScheduler()->InsertFence();
}

bool COcclusionQueryManager::GetQueryResults(COcclusionQueryManager::QueryHandle query, uint64& samplesPassed) const
{
	if (GetDeviceObjectFactory().GetVKScheduler()->TestForFence(m_fences[query]) != S_OK)
	{
		GetDeviceObjectFactory().GetVKScheduler()->FlushToFence(m_fences[query]);
		return false;
	}

	VkResult querySuccess = vkGetQueryPoolResults(GetDeviceObjectFactory().GetVKDevice()->GetVkDevice(), m_queryPool, query, 1, sizeof(uint64), &samplesPassed, sizeof(uint64), VK_QUERY_RESULT_64_BIT);
	return querySuccess == VK_SUCCESS;
}

}
