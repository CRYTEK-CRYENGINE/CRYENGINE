// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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

		template <class TItem>
		void CVariantDict::AddOrReplaceFromOriginalValue(const char* szKey, const TItem& originalValueToClone)
		{
			// FIXME: searching for the item-factory might not be the best approach; we could have the caller pass in the item-factory and assert() type matching,
			//        but that would also mean more responsibility on the client side
			const CTypeInfo& typeOfOriginalValue = Shared::SDataTypeHelper<TItem>::GetTypeInfo();
			Client::IItemFactory* pItemFactoryOfOriginalValue = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(typeOfOriginalValue);

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
