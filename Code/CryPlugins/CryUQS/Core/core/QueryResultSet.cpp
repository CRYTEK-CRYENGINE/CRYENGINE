// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryResultSet
		//
		//===================================================================================

		CQueryResultSet::CQueryResultSet(const IItemList& generatedItems, std::vector<size_t>&& itemIndexes, std::vector<float>&& itemScores)
			: m_scores(std::move(itemScores))
		{
			assert(itemIndexes.size() == m_scores.size());

			const size_t numIndexes = itemIndexes.size();
			const size_t* pIndexes = (numIndexes > 0) ? &itemIndexes.front() : nullptr;

			m_items.CopyOtherToSelfViaIndexList(generatedItems, pIndexes, numIndexes);
		}

		Client::IItemFactory& CQueryResultSet::GetItemFactory() const
		{
			return m_items.GetItemFactory();
		}

		size_t CQueryResultSet::GetResultCount() const
		{
			assert(m_items.GetItemCount() == m_scores.size());

			return m_items.GetItemCount();
		}

		IQueryResultSet::SResultSetEntry CQueryResultSet::GetResult(size_t index) const
		{
			assert(m_items.GetItemCount() == m_scores.size());
			assert(index < m_items.GetItemCount());

			void* pItem = m_items.GetItemAtIndex(index);
			float score = m_scores[index];
			return SResultSetEntry(pItem, score);
		}

		const CQueryResultSet& CQueryResultSet::GetImplementation() const
		{
			return *this;
		}

		const CItemList& CQueryResultSet::GetItemList() const
		{
			return m_items;
		}

		void CQueryResultSet::DeleteSelf()
		{
			delete this;
		}

	}
}
