// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQuery_Chained
		//
		// - composite query that runs its child queries one after another
		// - the resulting items are provided by the last query in the chain
		// - a chained query can be used to simulate the concept of "combinators" where a preceding query's result set gets passed to the next query via an input-parameter
		// - each time a child finishes, all its item-monitors will be transferred to ourself to ensure that they keep monitoring throughout the whole chain of queries
		//
		//===================================================================================

		class CQuery_Chained final : public CQuery_SequentialBase
		{
		public:
			explicit         CQuery_Chained(const SCtorContext& ctorContext);

		private:
			// CQuery_SequentialBase
			virtual void     HandleChildQueryFinishedWithSuccess(const CQueryID& childQueryID, QueryResultSetUniquePtr&& pResultSet) override;
			// ~CQuery_SequentialBase
		};

	}
}
