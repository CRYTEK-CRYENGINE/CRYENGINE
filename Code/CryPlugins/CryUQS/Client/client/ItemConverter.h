// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

#if UQS_SCHEMATYC_SUPPORT

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CItemConverterCollection
			//
			//===================================================================================

			class CItemConverterCollection final : public IItemConverterCollection
			{
			public:

				void                                AddItemConverter(const IItemConverter* pItemConverter);

				// IItemConverterCollection
				virtual size_t                      GetItemConverterCount() const override;
				virtual const IItemConverter&       GetItemConverter(size_t index) const override;
				// ~IItemConverterCollection

			private:

				std::vector<const IItemConverter*>  m_itemConverters;
			};

			inline void CItemConverterCollection::AddItemConverter(const IItemConverter* pItemConverter)
			{
				m_itemConverters.push_back(pItemConverter);
			}

			inline size_t CItemConverterCollection::GetItemConverterCount() const
			{
				return m_itemConverters.size();
			}

			inline const IItemConverter& CItemConverterCollection::GetItemConverter(size_t index) const
			{
				assert(index < m_itemConverters.size());
				return *m_itemConverters[index];
			}

			//===================================================================================
			//
			// CItemConverter<>
			//
			//===================================================================================

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			class CItemConverter final : public IItemConverter
			{
			public:

				explicit                            CItemConverter(const char* szFromName, const char* szToName, const CryGUID& guid, CItemConverterCollection& convertersToAddSelfTo);

				// IItemConverter
				virtual const char*                 GetFromName() const override;
				virtual const char*                 GetToName() const override;
				virtual const Shared::CTypeInfo&    GetFromItemType() const override;
				virtual const Shared::CTypeInfo&    GetToItemType() const override;
				virtual void                        ConvertItem(const void* pFromItem, void* pToItem) const override;
				virtual const CryGUID&              GetGUID() const override;
				// ~IItemConverter

			private:

				                                    UQS_NON_COPYABLE(CItemConverter);

			private:

				string                              m_fromName;
				string                              m_toName;
				CryGUID                             m_guid;

			};

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			CItemConverter<TFromType, TToType, pConvertFunc>::CItemConverter(const char* szFromName, const char* szToName, const CryGUID& guid, CItemConverterCollection& convertersToAddSelfTo)
				: m_fromName(szFromName)
				, m_toName(szToName)
				, m_guid(guid)
			{
				convertersToAddSelfTo.AddItemConverter(this);
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			const char* CItemConverter<TFromType, TToType, pConvertFunc>::GetFromName() const
			{
				return m_fromName.c_str();
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			const char* CItemConverter<TFromType, TToType, pConvertFunc>::GetToName() const
			{
				return m_toName.c_str();
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			const CryGUID& CItemConverter<TFromType, TToType, pConvertFunc>::GetGUID() const
			{
				return m_guid;
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			const Shared::CTypeInfo& CItemConverter<TFromType, TToType, pConvertFunc>::GetFromItemType() const
			{
				return Shared::SDataTypeHelper<TFromType>::GetTypeInfo();
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			const Shared::CTypeInfo& CItemConverter<TFromType, TToType, pConvertFunc>::GetToItemType() const
			{
				return Shared::SDataTypeHelper<TToType>::GetTypeInfo();
			}

			template <class TFromType, class TToType, void(*pConvertFunc)(const TFromType&, TToType&)>
			void CItemConverter<TFromType, TToType, pConvertFunc>::ConvertItem(const void* pFromItem, void* pToItem) const
			{
				assert(pConvertFunc);
				assert(pToItem);
				assert(pFromItem);

				const TFromType* pFrom = static_cast<const TFromType*>(pFromItem);
				TToType* pTo = static_cast<TToType*>(pToItem);

				(*pConvertFunc)(*pFrom, *pTo);
			}

		} // namespace Internal

		//===================================================================================
		//
		// CItemConverterCollectionMgr<>
		//
		//===================================================================================

		template <class TItem>
		class CItemConverterCollectionMgr
		{
		public:

			template <class TFromForeignType, void(*pConvertFunc)(const TFromForeignType&, TItem&)>
			void                                AddFromForeignTypeConverter(const char* szFromName, const char* szToName, const CryGUID& guid);

			template <class TToForeignType, void(*pConvertFunc)(const TItem&, TToForeignType&)>
			void                                AddToForeignTypeConverter(const char* szFromName, const char* szToName, const CryGUID& guid);

			const IItemConverterCollection&     GetFromForeignTypeConverters() const;
			const IItemConverterCollection&     GetToForeignTypeConverters() const;

		private:

			Internal::CItemConverterCollection  m_fromForeignTypeConverters;
			Internal::CItemConverterCollection  m_toForeignTypeConverters;
		};

		template <class TItem>
		template <class TFromForeignType, void(*pConvertFunc)(const TFromForeignType&, TItem&)>
		void CItemConverterCollectionMgr<TItem>::AddFromForeignTypeConverter(const char* szFromName, const char* szToName, const CryGUID& guid)
		{
			static const Internal::CItemConverter<TFromForeignType, TItem, pConvertFunc> converter(szFromName, szToName, guid, m_fromForeignTypeConverters);
		}

		template <class TItem>
		template <class TToForeignType, void(*pConvertFunc)(const TItem&, TToForeignType&)>
		void CItemConverterCollectionMgr<TItem>::AddToForeignTypeConverter(const char* szFromName, const char* szToName, const CryGUID& guid)
		{
			static const Internal::CItemConverter<TItem, TToForeignType, pConvertFunc> converter(szFromName, szToName, guid, m_toForeignTypeConverters);
		}

		template <class TItem>
		const IItemConverterCollection& CItemConverterCollectionMgr<TItem>::GetFromForeignTypeConverters() const
		{
			return m_fromForeignTypeConverters;
		}

		template <class TItem>
		const IItemConverterCollection& CItemConverterCollectionMgr<TItem>::GetToForeignTypeConverters() const
		{
			return m_toForeignTypeConverters;
		}

	}
}

#endif // UQS_SCHEMATYC_SUPPORT
