// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Shared
	{

		//===================================================================================
		//
		// CVariantDict
		//
		//===================================================================================

		class CVariantDict : public IVariantDict
		{
		public:
			struct SDataEntry
			{
				explicit                         SDataEntry();
				Client::IItemFactory*            pItemFactory;
				void*                            pObject;
			};

		public:
			                                     CVariantDict();
			                                     ~CVariantDict();

			// IVariantDict
			virtual void                         AddOrReplace(const char* szKey, Client::IItemFactory& itemFactory, const void* pItemToClone) override;
			virtual void                         AddSelfToOtherAndReplace(IVariantDict& out) const override;
			virtual bool                         Exists(const char* szKey) const override;
			virtual Client::IItemFactory*        FindItemFactory(const char* szKey) const override;
			virtual bool                         FindItemFactoryAndObject(const char* szKey, Client::IItemFactory* &pOutItemItemFactory, void* &pOutObject) const override;
			// ~IVariantDict

			const std::map<string, SDataEntry>&  GetEntries() const;

			template <class TItem>
			void                                 AddOrReplaceFromOriginalValue(const char* szKey, const TItem& originalValueToClone);

			void                                 Clear();

		private:
			                                     UQS_NON_COPYABLE(CVariantDict);

		private:
			std::map<string, SDataEntry>         m_dataItems;
		};

		inline CVariantDict::SDataEntry::SDataEntry()
			: pItemFactory(nullptr)
			, pObject(nullptr)
		{
			// nothing
		}

		inline CVariantDict::CVariantDict()
		{
			// nothing
		}

		inline CVariantDict::~CVariantDict()
		{
			Clear();
		}

		inline void CVariantDict::AddOrReplace(const char* szKey, Client::IItemFactory& itemFactory, const void* pItemToClone)
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

		inline void CVariantDict::AddSelfToOtherAndReplace(IVariantDict& out) const
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

		inline bool CVariantDict::Exists(const char* szKey) const
		{
			return (m_dataItems.find(szKey) != m_dataItems.cend());
		}

		inline Client::IItemFactory* CVariantDict::FindItemFactory(const char* szKey) const
		{
			auto it = m_dataItems.find(szKey);
			return (it == m_dataItems.cend()) ? nullptr : it->second.pItemFactory;
		}

		inline bool CVariantDict::FindItemFactoryAndObject(const char* szKey, Client::IItemFactory* &pOutItemItemFactory, void* &pOutObject) const
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

		inline const std::map<string, CVariantDict::SDataEntry>& CVariantDict::GetEntries() const
		{
			return m_dataItems;
		}

		inline void CVariantDict::Clear()
		{
			for (auto pair : m_dataItems)
			{
				SDataEntry& entry = pair.second;
				assert(entry.pItemFactory && entry.pObject);
				entry.pItemFactory->DestroyItems(entry.pObject);
			}
			m_dataItems.clear();
		}

		template <class TItem>
		void CVariantDict::AddOrReplaceFromOriginalValue(const char* szKey, const TItem& originalValueToClone)
		{
			UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr();

			if (!pHub)
			{
				CryWarning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_ERROR, "UQS: CVariantDict::AddOrReplaceFromOriginalValue: UQS Plugin is not loaded. The dictionary you're trying to fill will be lacking the entry with key = '%s'.", szKey);
				return;
			}

			// FIXME: searching for the item-factory might not be the best approach; we could have the caller pass in the item-factory and assert() type matching,
			//        but that would also mean more responsibility on the client side
			const CTypeInfo& typeOfOriginalValue = Shared::SDataTypeHelper<TItem>::GetTypeInfo();
			Client::IItemFactory* pItemFactoryOfOriginalValue = pHub->GetUtils().FindItemFactoryByType(typeOfOriginalValue);

			// If this fails then there is obviously no item-factory registered that can create items of given type.
			// Currently we do nothing about it since it should be the responsibility of the caller to ensure consistency beforehand.
			// Ideally, we should trigger a CryFatalError(), but that sounds a bit too harsh?! For now, we only issue a warning.
			assert(pItemFactoryOfOriginalValue);
			if (pItemFactoryOfOriginalValue)
			{
				AddOrReplace(szKey, *pItemFactoryOfOriginalValue, &originalValueToClone);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_ERROR, "UQS: CVariantDict::AddOrReplaceFromOriginalValue: tried to add a value of type [%s] under the name '%s' but there is no item-factory registered for that type.", typeOfOriginalValue.name(), szKey);
			}
		}

	}
}
