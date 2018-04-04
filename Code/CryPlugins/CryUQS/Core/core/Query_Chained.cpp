// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQuery_Chained
		//
		//===================================================================================

		CQuery_Chained::CQuery_Chained(const SCtorContext& ctorContext)
			: CQuery_SequentialBase(ctorContext)
		{
			// nothing
		}

		void CQuery_Chained::HandleChildQueryFinishedWithSuccess(const CQueryID& childQueryID, QueryResultSetUniquePtr&& pResultSet)
		{
			assert(pResultSet != nullptr);

			// * if there are more queries in chain, then do the following:
			//    - convert the result set to a list of "pre-generated" items
			//    - instantiate the next query and pass that list to it (so that it resides on its private blackboard)
			//      -> that way, the next query can use that list as some kind of input without having to generate items on its own
			//
			// * but if this was the last query in the chain, then copy the result set away to have it picked up by the CQueryManager (or the parent query, of course)

			if (HasMoreChildrenLeftToInstantiate())
			{
				StoreResultSetForUseInNextChildQuery(*pResultSet);
				InstantiateNextChildQueryBlueprint();
			}
			else
			{
				// all queries in the chain finished
				// -> prepare the final result set for letting the caller inspect it
				m_pResultSet = std::move(pResultSet);
			}

			// transfer all item-monitors from the child to ourself to keep monitoring until a higher-level query decides differently
			CQueryBase* pChildQuery = g_pHub->GetQueryManager().FindQueryByQueryID(childQueryID);
			assert(pChildQuery);
			pChildQuery->TransferAllItemMonitorsToOtherQuery(*this);
		}

	}
}
