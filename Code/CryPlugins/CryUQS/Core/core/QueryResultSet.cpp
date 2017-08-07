// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

		CQueryResultSet::CQueryResultSet()
			: m_pItemFactory(nullptr)
		{
			// nothing
		}

		Client::IItemFactory& CQueryResultSet::GetItemFactory() const
		{
			assert(m_pItemFactory);
			return *m_pItemFactory;
		}

		size_t CQueryResultSet::GetResultCount() const
		{
			assert(m_pItemFactory);
			assert(m_items.GetItemCount() == m_scores.size());

			return m_items.GetItemCount();
		}

		IQueryResultSet::SResultSetEntry CQueryResultSet::GetResult(size_t index) const
		{
			assert(m_pItemFactory);
			assert(m_items.GetItemCount() == m_scores.size());
			assert(index < m_items.GetItemCount());

			void* pItem = m_items.GetItemAtIndex(index);
			float score = m_scores[index];
			return SResultSetEntry(pItem, score);
		}

		void CQueryResultSet::SetItemFactoryAndCreateItems(Client::IItemFactory& itemFactory, size_t numItemsToCreate)
		{
			assert(!m_pItemFactory);

			m_pItemFactory = &itemFactory;
			m_items.SetItemFactory(itemFactory);

			m_items.CreateItemsByItemFactory(numItemsToCreate);
			m_scores.resize(numItemsToCreate);
		}

		void CQueryResultSet::SetItemAndScore(size_t index, const void* pSourceItem, float score)
		{
			assert(m_pItemFactory);
			assert(m_items.GetItemCount() == m_scores.size());
			assert(index < m_items.GetItemCount());

			void* pTargetItem = m_items.GetItemAtIndex(index);
			m_pItemFactory->CopyItem(pTargetItem, pSourceItem);
			m_scores[index] = score;
		}

		void CQueryResultSet::DeleteSelf()
		{
			delete this;
		}

	}
}
