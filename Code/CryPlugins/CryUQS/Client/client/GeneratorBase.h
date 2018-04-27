// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CGeneratorBase<>
		//
		//===================================================================================

		template <class TGenerator, class TItem>
		class CGeneratorBase : public IGenerator
		{
		public:
			typedef TItem             ItemType;     // used by CGeneratorFactory<>::GetTypeOfItemsToGenerate() and CreateGenerator()

		public:
			virtual IItemFactory&     GetItemFactory() const override final;
			virtual EUpdateStatus     Update(const SUpdateContext& updateContext, Core::IItemList& itemListToPopulate) override final;

		protected:
			explicit                  CGeneratorBase();

		private:
			IItemFactory*             m_pItemFactory;
		};

		template <class TGenerator, class TItem>
		CGeneratorBase<TGenerator, TItem>::CGeneratorBase()
			: m_pItemFactory(nullptr)
		{
			// find the ItemFactory by the type of items that this generator generates
			const Shared::CTypeInfo& itemTypeToGenerate = Shared::SDataTypeHelper<TItem>::GetTypeInfo();
			m_pItemFactory = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(itemTypeToGenerate);

			// if this fails, then no such item factory was registered by the client code (in fact, this error must have been detected by StartupConsistencyChecker already)
			assert(m_pItemFactory);
		}

		template <class TGenerator, class TItem>
		IItemFactory& CGeneratorBase<TGenerator, TItem>::GetItemFactory() const
		{
			assert(m_pItemFactory);   // should have been detected in the ctor already (and, of course, by the StartupConsistencyChecker)
			return *m_pItemFactory;
		}

		template <class TGenerator, class TItem>
		IGenerator::EUpdateStatus CGeneratorBase<TGenerator, TItem>::Update(const SUpdateContext& updateContext, Core::IItemList& itemListToPopulate)
		{
			// if this assert() fails, then something must have gone wrong in InitItemListWithProperItemFactory()
			assert(itemListToPopulate.GetItemFactory().GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo());
			CItemListProxy_Writable<TItem> actualItemListToPopulate(itemListToPopulate);
			TGenerator* pActualGenerator = static_cast<TGenerator*>(this);
			return pActualGenerator->DoUpdate(updateContext, actualItemListToPopulate);
		}

	}
}
