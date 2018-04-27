// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CItemList
		//
		//===================================================================================

		class CItemList final : public IItemList
		{
		public:
			explicit                                   CItemList();
			                                           ~CItemList();

			// IItemList
			virtual void                               CreateItemsByItemFactory(size_t numItemsToCreate) override;
			virtual void                               CloneItems(const void* pOriginalItems, size_t numItemsToClone) override;
			virtual size_t                             GetItemCount() const override;
			virtual Client::IItemFactory&              GetItemFactory() const override;
			virtual void*                              GetItems() const override;
			virtual void                               CopyOtherToSelf(const IItemList& other) override;
			// ~IItemList

			void                                       SetItemFactory(Client::IItemFactory& itemFactory);
			void                                       CopyOtherToSelfViaIndexList(const IItemList& other, const size_t* pIndexes, size_t numIndexes);
			void*                                      GetItemAtIndex(size_t index) const;
			size_t                                     GetMemoryUsedSafe() const;    // it's safe to call this method, even if no item-factory has been set yet

		private:
			                                           UQS_NON_COPYABLE(CItemList);

		private:
			Client::IItemFactory*                      m_pItemFactory;
			void*                                      m_pItems;
			size_t                                     m_numItems;
		};

	}
}
