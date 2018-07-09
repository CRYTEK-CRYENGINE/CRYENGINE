// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{
		namespace Internal
		{

			//===================================================================================
			//
			// CGen_PropagateShuttledItems<>
			//
			//===================================================================================

			template <class TItem>
			class CGen_PropagateShuttledItems : public CGeneratorBase<CGen_PropagateShuttledItems<TItem>, TItem>
			{
			private:
				typedef CGeneratorBase<CGen_PropagateShuttledItems<TItem>, TItem> BaseClass;

			public:

				struct SParams
				{
					// nothing

					UQS_EXPOSE_PARAMS_BEGIN
						// nothing
					UQS_EXPOSE_PARAMS_END
				};

				explicit CGen_PropagateShuttledItems(const SParams& params)
				{
					// nothing
				}

				typename BaseClass::EUpdateStatus DoUpdate(const typename BaseClass::SUpdateContext& updateContext, CItemListProxy_Writable<TItem>& itemListToPopulate)
				{
					CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);	// mainly for keeping an eye on the copy operation of the passed-in items below

					if (!updateContext.blackboard.pShuttledItems)
					{
						updateContext.error.Format("CGen_PropagateShuttledItems<>::DoUpdate: updateContext.blackboard.pShuttledItems == NULL (this can happen if there was no previous query that forwarded its resulting items)");
						return BaseClass::EUpdateStatus::ExceptionOccurred;
					}

					Core::IItemList& underlyingItemList = itemListToPopulate.GetUnderlyingItemList();

					// ensure both item lists have the same ItemFactory
					const IItemFactory& expectedItemFactory = underlyingItemList.GetItemFactory();
					const IItemFactory& receivedItemFactory = updateContext.blackboard.pShuttledItems->GetItemFactory();
					if (&expectedItemFactory != &receivedItemFactory)
					{
						updateContext.error.Format("CGen_PropagateShuttledItems<>::DoUpdate: incompatible item-factories (generator wants to propagate items of type '%s', but the blackboard contains items of type '%s')", expectedItemFactory.GetItemType().name(), receivedItemFactory.GetItemType().name());
						return BaseClass::EUpdateStatus::ExceptionOccurred;
					}

					// copy the items from the previous query to given output list
					underlyingItemList.CopyOtherToSelf(*updateContext.blackboard.pShuttledItems);

					return BaseClass::EUpdateStatus::FinishedGeneratingItems;
				}
			};

		}
	}
}
