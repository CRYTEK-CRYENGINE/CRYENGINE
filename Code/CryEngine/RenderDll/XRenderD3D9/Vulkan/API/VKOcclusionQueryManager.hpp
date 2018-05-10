// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "VKBase.hpp"
#include <bitset>

namespace NCryVulkan
{
	class COcclusionQueryManager
	{
		static const int kMaxQueryCount = 64;

	public:
		typedef int  QueryHandle;
		static const QueryHandle InvalidQuery = -1;

	public:
		COcclusionQueryManager();
		~COcclusionQueryManager();

		bool Init(VkDevice device);

		QueryHandle CreateQuery();
		void        ReleaseQuery(QueryHandle query);

		void BeginQuery(QueryHandle query, VkCommandBuffer commandBuffer);
		void EndQuery(QueryHandle query, VkCommandBuffer commandBuffer);
		bool GetQueryResults(QueryHandle query, uint64& samplesPassed) const;

	private:
		std::bitset<kMaxQueryCount>        m_bQueryInUse;
		std::array<uint64, kMaxQueryCount> m_fences;
		VkQueryPool                        m_queryPool;
	};

	// A simple inline wrapper class for a single occlusion query.
	// Has to be ref-counted since this is expected by high-level.
	class COcclusionQuery : public CRefCounted
	{
	public:
		COcclusionQuery(COcclusionQueryManager& manager)
			: m_pManager(&manager)
			, m_handle(manager.CreateQuery())
		{}

		~COcclusionQuery()
		{
			if (IsValid())
			{
				m_pManager->ReleaseQuery(m_handle);
			}
		}

		bool IsValid() const
		{
			return m_handle != COcclusionQueryManager::InvalidQuery;
		}

		void Begin(VkCommandBuffer commandBuffer)
		{
			m_pManager->BeginQuery(m_handle, commandBuffer);
		}

		void End(VkCommandBuffer commandBuffer)
		{
			m_pManager->EndQuery(m_handle, commandBuffer);
		}

		bool GetResults(uint64& samplesPassed)
		{
			return m_pManager->GetQueryResults(m_handle, samplesPassed);
		}

	private:
		COcclusionQueryManager* const             m_pManager;
		const COcclusionQueryManager::QueryHandle m_handle;
	};
}
