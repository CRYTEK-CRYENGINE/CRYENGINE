// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CQuery_Fallbacks::CQuery_Fallbacks(const SCtorContext& ctorContext)
			: CQuery_SequentialBase(ctorContext)
		{
			// nothing
		}

		void CQuery_Fallbacks::HandleChildQueryFinishedWithSuccess(const CQueryID& childQueryID, QueryResultSetUniquePtr&& pResultSet)
		{
			assert(pResultSet != nullptr);

			if (pResultSet->GetResultCount() == 0 && HasMoreChildrenLeftToInstantiate())
			{
				// try the next child (always provide all our children with the shuttled items that *we* hold on to)
				InstantiateNextChildQueryBlueprint(m_pOptionalShuttledItems);

				// notice: we don't care to keep the child's item-monitors alive, since they were specific to that query that we just rejected (so: no need to keep monitoring what is not needed anymore)
			}
			else
			{
				// make the final result set available to the caller (even if we ran out of fallback queries and thus end up with 0 items)
				m_pResultSet = std::move(pResultSet);

				// transfer all item-monitors from the child to ourself to keep monitoring until a higher-level query decides differently
				CQueryBase* pChildQuery = g_pHub->GetQueryManager().FindQueryByQueryID(childQueryID);
				assert(pChildQuery);
				pChildQuery->TransferAllItemMonitorsToOtherQuery(*this);
			}
		}

	}
}
