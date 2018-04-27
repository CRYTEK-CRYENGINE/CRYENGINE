// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// IItemList
		//
		// - an item list is owned by a "live" Query and handed out to the generator to populate it with items
		// - these items will then get evaluated and the best ones will get copied to the final result set
		//
		//===================================================================================

		struct IItemList
		{
			virtual                                ~IItemList() {}
			virtual void                           CreateItemsByItemFactory(size_t numItemsToCreate) = 0;
			virtual void                           CloneItems(const void* pOriginalItems, size_t numItemsToClone) = 0;
			virtual size_t                         GetItemCount() const = 0;
			virtual Client::IItemFactory&          GetItemFactory() const = 0;
			virtual void*                          GetItems() const = 0;
			virtual void                           CopyOtherToSelf(const IItemList& other) = 0;
		};

	}
}
