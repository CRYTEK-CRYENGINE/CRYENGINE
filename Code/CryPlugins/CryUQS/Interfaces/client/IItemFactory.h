// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// IItemFactory
		//
		//===================================================================================

		struct IItemFactory
		{
			enum class EItemInitMode
			{
				UseDefaultConstructor,
				UseUserProvidedFunction
			};

			virtual                                   ~IItemFactory() {}
			virtual const char*                       GetName() const = 0;
			virtual const CryGUID&                    GetGUID() const = 0;
			virtual const char*                       GetDescription() const = 0;
			virtual bool                              IsContainerForShuttledItems() const = 0;

#if UQS_SCHEMATYC_SUPPORT
			virtual const IItemConverterCollection&   GetFromForeignTypeConverters() const = 0;
			virtual const IItemConverterCollection&   GetToForeignTypeConverters() const = 0;
#endif

			virtual void*                             CreateItems(size_t numItems, EItemInitMode itemInitMode) = 0;
			virtual void*                             CloneItem(const void* pOriginalItem) = 0;
			virtual void*                             CloneItems(const void* pOriginalItems, size_t numItemsToClone) = 0;
			virtual void*                             CloneItemsViaIndexList(const void* pOriginalItems, const size_t* pIndexes, size_t numIndexes) = 0;
			virtual void                              DestroyItems(void* pItems) = 0;
			virtual const Shared::CTypeInfo&          GetItemType() const = 0;
			virtual size_t                            GetItemSize() const = 0;
			virtual void                              CopyItem(void* pTargetItem, const void* pSourceItem) const = 0;
			virtual void*                             GetItemAtIndex(void* pItems, size_t index) const = 0;
			virtual const void*                       GetItemAtIndex(const void* pItems, size_t index) const = 0;
			virtual void                              AddItemToDebugRenderWorld(const void* pItem, Core::IDebugRenderWorldPersistent& debugRW) const = 0;
			virtual bool                              CanBeRepresentedInDebugRenderWorld() const = 0;
			virtual void                              CreateItemDebugProxyForItem(const void* pItem, Core::IItemDebugProxyFactory& itemDebugProxyFactory) const = 0;
			virtual bool                              CanBePersistantlySerialized() const = 0;
			virtual bool                              TrySerializeItem(const void* pItem, Serialization::IArchive& archive, const char* szName, const char* szLabel) const = 0;
			virtual bool                              TryDeserializeItem(void* pOutItem, Serialization::IArchive& archive, const char* szName, const char* szLabel) const = 0;
			virtual bool                              TryDeserializeItemIntoDict(Shared::IVariantDict& out, const char* szKey, Serialization::IArchive& archive, const char* szName, const char* szLabel) = 0;
		};

	}
}
