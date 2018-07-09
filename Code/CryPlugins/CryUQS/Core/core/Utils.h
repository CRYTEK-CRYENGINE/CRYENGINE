// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		class CHub;

		//===================================================================================
		//
		// CUtils
		//
		//===================================================================================

		class CUtils : public IUtils, public IFactoryDatabaseListener<Client::IItemFactory>
		{
		public:
			// IUtils
			virtual Client::IItemFactory*                           FindItemFactoryByType(const Shared::CTypeInfo& type) const override;
			virtual const IQueryFactory&                            GetDefaultQueryFactory() const override;
			// ~IUtils

			// IFactoryDatabaseListener<Client::IItemFactory>
			virtual void                                            OnFactoryRegistered(Client::IItemFactory* pFreshlyRegisteredFactory) override;
			// ~IFactoryDatabaseListener<Client::IItemFactory>

			// - called by CHub's ctor + dtor
			// - this is to ensure that all member variables in CHub are definitely constructed before giving CUtils access to them
			void                                                    SubscribeToStuffInHub(CHub& hub);
			void                                                    UnsubscribeFromStuffInHub(CHub& hub);

		private:
			std::map<Shared::CTypeInfo, Client::IItemFactory*>      m_type2itemFactory;
		};

	}
}
