// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		Client::IItemFactory* CUtils::FindItemFactoryByType(const Shared::CTypeInfo& type) const
		{
			auto it = m_type2itemFactory.find(type);
			return (it == m_type2itemFactory.cend()) ? nullptr : it->second;
		}

		const IQueryFactory& CUtils::GetDefaultQueryFactory() const
		{
			return CQueryFactoryBase::GetDefaultQueryFactory();
		}

		void CUtils::OnFactoryRegistered(Client::IItemFactory* pFreshlyRegisteredFactory)
		{
			m_type2itemFactory[pFreshlyRegisteredFactory->GetItemType()] = pFreshlyRegisteredFactory;
		}

		void CUtils::SubscribeToStuffInHub(CHub& hub)
		{
			hub.GetItemFactoryDatabase().RegisterListener(this);
		}

		void CUtils::UnsubscribeFromStuffInHub(CHub& hub)
		{
			hub.GetItemFactoryDatabase().UnregisterListener(this);
		}

	}
}
