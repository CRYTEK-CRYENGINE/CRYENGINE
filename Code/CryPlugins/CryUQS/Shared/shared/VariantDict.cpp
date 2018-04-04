// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VariantDict.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		CVariantDict::SDataEntry::SDataEntry()
			: pItemFactory(nullptr)
			, pObject(nullptr)
		{
			// nothing
		}

		CVariantDict::CVariantDict()
		{
			// nothing
		}

		CVariantDict::~CVariantDict()
		{
			Clear();
		}

		void CVariantDict::AddOrReplace(const char* szKey, Client::IItemFactory& itemFactory, const void* pItemToClone)
		{
			assert(pItemToClone);

			SDataEntry& entry = m_dataItems[szKey];

			assert((entry.pItemFactory && entry.pObject) || (!entry.pItemFactory && !entry.pObject));

			// same item-factory as before? -> just change the object's value
			if (entry.pItemFactory && entry.pItemFactory == &itemFactory)
			{
				entry.pItemFactory->CopyItem(entry.pObject, pItemToClone);
			}
			else
			{
				// we're about to get a different item-factory, so get rid of the old potential object beforehand
				if (entry.pItemFactory)
				{
					entry.pItemFactory->DestroyItems(entry.pObject);
				}

				entry.pObject = itemFactory.CloneItem(pItemToClone);
				entry.pItemFactory = &itemFactory;
			}
		}

		void CVariantDict::AddSelfToOtherAndReplace(IVariantDict& out) const
		{
			assert(this != &out);

			for (const auto& pair : m_dataItems)
			{
				const char* szKey = pair.first.c_str();
				const SDataEntry& entry = pair.second;

				assert(entry.pItemFactory && entry.pObject);

				out.AddOrReplace(szKey, *entry.pItemFactory, entry.pObject);
			}
		}

		bool CVariantDict::Exists(const char* szKey) const
		{
			return (m_dataItems.find(szKey) != m_dataItems.cend());
		}

		Client::IItemFactory* CVariantDict::FindItemFactory(const char* szKey) const
		{
			auto it = m_dataItems.find(szKey);
			return (it == m_dataItems.cend()) ? nullptr : it->second.pItemFactory;
		}

		bool CVariantDict::FindItemFactoryAndObject(const char* szKey, Client::IItemFactory* &pOutItemItemFactory, void* &pOutObject) const
		{
			auto it = m_dataItems.find(szKey);
			if (it != m_dataItems.cend())
			{
				pOutItemItemFactory = it->second.pItemFactory;
				pOutObject = it->second.pObject;
				return true;
			}
			else
			{
				return false;
			}
		}

		const std::map<string, CVariantDict::SDataEntry>& CVariantDict::GetEntries() const
		{
			return m_dataItems;
		}

		void CVariantDict::Clear()
		{
			for (auto pair : m_dataItems)
			{
				SDataEntry& entry = pair.second;
				assert(entry.pItemFactory && entry.pObject);
				entry.pItemFactory->DestroyItems(entry.pObject);
			}
			m_dataItems.clear();
		}

	}
}
