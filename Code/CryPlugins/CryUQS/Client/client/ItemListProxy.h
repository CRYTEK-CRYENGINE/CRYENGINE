// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CItemListProxy_Readable<>
		//
		//===================================================================================

		template <class TItem>
		class CItemListProxy_Readable
		{
		public:
			explicit                        CItemListProxy_Readable(const Core::IItemList& itemList);
			explicit                        CItemListProxy_Readable();   // default ctor required for CItemFactoryInternal<>::CreateItems()

			// implicitly allow copying

			size_t                          GetItemCount() const;
			const TItem&                    GetItemAtIndex(size_t index) const;

		private:
			// - these point into the IItemList that was passed in to the ctor
			// - we cache them to circumvent some virtual function calls, rather than invoking the according Get*() method on the IItemList each time we need access to one particular item
			// - of course, the passed in IItemList must not go out of scope or change its content while we read from it
			const Client::IItemFactory*     m_pItemFactory;
			const void*                     m_pItems;
			size_t                          m_itemCount;
		};

		template <class TItem>
		CItemListProxy_Readable<TItem>::CItemListProxy_Readable(const Core::IItemList& itemList)
			: m_pItemFactory(&itemList.GetItemFactory())
			, m_pItems(itemList.GetItems())
			, m_itemCount(itemList.GetItemCount())
		{
			// ensure type correctness (this presumes that given item-list has already been provided with an item-factory)
			assert(m_pItemFactory->GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo());
		}

		template <class TItem>
		CItemListProxy_Readable<TItem>::CItemListProxy_Readable()
			: m_pItemFactory(nullptr)
			, m_pItems(nullptr)
			, m_itemCount(0)
		{
			// nothing
		}

		template <class TItem>
		size_t CItemListProxy_Readable<TItem>::GetItemCount() const
		{
			return m_itemCount;
		}

		template <class TItem>
		const TItem& CItemListProxy_Readable<TItem>::GetItemAtIndex(size_t index) const
		{
			assert(index < m_itemCount);
			return *static_cast<const TItem*>(m_pItemFactory->GetItemAtIndex(m_pItems, index));
		}

		//===================================================================================
		//
		// CItemListProxy_Writable<>
		//
		//===================================================================================

		template <class TItem>
		class CItemListProxy_Writable
		{
		public:
			explicit                  CItemListProxy_Writable(Core::IItemList& itemList);
			void                      CreateItemsByItemFactory(size_t numItemsToCreate);
			void                      CloneItems(const TItem* pOriginalItems, size_t numItemsToClone);
			TItem&                    GetItemAtIndex(size_t index);
			Core::IItemList&          GetUnderlyingItemList();

		private:
			                          UQS_NON_COPYABLE(CItemListProxy_Writable);

		private:
			Core::IItemList&          m_itemList;

			// - these point into the IItemList that was passed in to the ctor
			// - we cache them to circumvent some virtual function calls, rather than invoking the according Get*() method on the IItemList each time we need access to one particular item
			// - of course, the passed in IItemList must not go out of scope or change its content while we write to it
			Client::IItemFactory&     m_itemFactory;
			void*                     m_pItems;
			size_t                    m_itemCount;
		};

		template <class TItem>
		CItemListProxy_Writable<TItem>::CItemListProxy_Writable(Core::IItemList& itemList)
			: m_itemList(itemList)
			, m_itemFactory(itemList.GetItemFactory())
			, m_pItems(itemList.GetItems())         // this may be or may not point to potentially created items, depending on whether the underlying item-list had been filled in a previous roundtrip already
			, m_itemCount(itemList.GetItemCount())  // ditto
		{
			// ensure type correctness (this presumes that given item-list has already been provided with an item-factory)
			assert(itemList.GetItemFactory().GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo());
		}

		template <class TItem>
		void CItemListProxy_Writable<TItem>::CreateItemsByItemFactory(size_t numItemsToCreate)
		{
			m_itemList.CreateItemsByItemFactory(numItemsToCreate);

			// retrieve the items in case they hadn't been created in a previous roundtrip
			m_pItems = m_itemList.GetItems();
			m_itemCount = numItemsToCreate;
		}

		template <class TItem>
		void CItemListProxy_Writable<TItem>::CloneItems(const TItem* pOriginalItems, size_t numItemsToClone)
		{
			assert(pOriginalItems);

			m_itemList.CloneItems(pOriginalItems, numItemsToClone);

			// retrieve the items in case they hadn't been created in a previous roundtrip
			m_pItems = m_itemList.GetItems();
			m_itemCount = numItemsToClone;
		}

		template <class TItem>
		TItem& CItemListProxy_Writable<TItem>::GetItemAtIndex(size_t index)
		{
			assert(index < m_itemCount);
			return *static_cast<TItem*>(m_itemFactory.GetItemAtIndex(m_pItems, index));
		}

		template <class TItem>
		Core::IItemList& CItemListProxy_Writable<TItem>::GetUnderlyingItemList()
		{
			return m_itemList;
		}

	}
}
