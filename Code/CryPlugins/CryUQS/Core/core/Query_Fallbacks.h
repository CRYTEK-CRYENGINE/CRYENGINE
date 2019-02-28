// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQuery_Fallbacks
		//
		// - composite query that tries to run one child query after another until one comes up with at least 1 item
		// - if by the time the last query has run and the result set is still empty, then an empty result set will ultimately be returned
		// - all item-monitors of the one child that came up with the result set will be transferred to ourself, so that these item-monitors will bubble up in the hierarchy and keep monitoring
		// - for children that failed to deliver items, we don't care about their item-monitors as they don't have a saying on the final items anyway
		//
		//===================================================================================

		class CQuery_Fallbacks final : public CQuery_SequentialBase
		{
		public:
			explicit        CQuery_Fallbacks(const SCtorContext& ctorContext);

		private:
			// CQuery_SequentialBase
			virtual void    HandleChildQueryFinishedWithSuccess(const CQueryID& childQueryID, QueryResultSetUniquePtr&& pResultSet) override;
			// ~CQuery_SequentialBase
		};

	}
}
