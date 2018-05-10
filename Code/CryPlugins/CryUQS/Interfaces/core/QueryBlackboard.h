// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// SQueryBlackboard
		//
		// - bundles some stuff that is owned by a running query and is handed out to all sub components at execution time
		// - the running query writes to the blackboard whereas functions, generators and evaluators read from it
		//
		//===================================================================================

		struct SQueryBlackboard
		{
			explicit                            SQueryBlackboard(const Shared::IVariantDict& _globalParams, const IItemList* _pShuttledItems, const ITimeBudget& _timeBudget, IDebugRenderWorldPersistent* _pDebugRenderWorldPersistent);

			const Shared::IVariantDict&         globalParams;
			const IItemList* const              pShuttledItems;              // resulting items of the previous query (in the chained query); might be a nullptr if there was no previous query
			const SItemIterationContext*        pItemIterationContext;       // this is only set during the evaluator phase when iterating through all generated items; it's a nullptr during the generation of items
			const ITimeBudget&                  timeBudget;                  // allows generators and deferred-evaluators to see if some of the granted time is still available (for when doing time-sliced work) or whether we've already run out of time
			IDebugRenderWorldPersistent* const  pDebugRenderWorldPersistent; // optional pointer to an IDebugRenderWorldPersisent for persisting debug-primitives; may be a nullptr if logging of the historic query is not desired
			const IDebugRenderWorldImmediate*   pDebugRenderWorldImmediate;  // optional pointer to an IDebugRenderWorldImmediate for drawing debug-primitives immediately (instead of persisting them in the historic query); may be a nullptr if debug-drawing is not desired
		};

		inline SQueryBlackboard::SQueryBlackboard(const Shared::IVariantDict& _globalParams, const IItemList* _pShuttledItems, const ITimeBudget& _timeBudget, IDebugRenderWorldPersistent* _pDebugRenderWorldPersistent)
			: globalParams(_globalParams)
			, pShuttledItems(_pShuttledItems)
			, timeBudget(_timeBudget)
			, pItemIterationContext(nullptr)
			, pDebugRenderWorldPersistent(_pDebugRenderWorldPersistent)
			, pDebugRenderWorldImmediate(nullptr)
		{}

	}
}
