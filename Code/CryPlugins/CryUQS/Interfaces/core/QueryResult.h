// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// SQueryResult
		//
		// - counterpart to client::SQueryRequest
		// - contains the potential final items if everything went fine during the query execution
		// - contains information about what might have gone wrong
		// - whoever handles this SQueryResult can claim ownership of the result set for processing it at a later time
		// - if he doesn't claim ownership then the result set will get destroyed automatically after the callback returns
		//
		//===================================================================================

		struct SQueryResult
		{
			enum EStatus
			{
				Success,                  // the query finished without runtime errors; the final result may still contain 0 items, though, but the .pResultSet pointer is definitely valid
				ExceptionOccurred,        // a runtime error occurred during the execution of the query; use the .error property to see what went wrong
				CanceledByHubTearDown     // the UQS Hub is about to get torn down and is therefore in the process of canceling all running queries
			};

			explicit                      SQueryResult(const CQueryID& _queryID, EStatus _status, QueryResultSetUniquePtr& _pResultSet, const char* _szError);
			static SQueryResult           CreateSuccess(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSet);
			static SQueryResult           CreateError(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSetDummy, const char* _szError);
			static SQueryResult           CreateCanceledByHubTearDown(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSetDummy);

			CQueryID                      queryID;
			EStatus                       status;
			QueryResultSetUniquePtr&      pResultSet;   // only set in case of EStatus::Success, nullptr otherwise; the code in the callback is free to claim ownership for later proecssing
			const char*                   szError;      // only set in case of EStatus::ExceptionOccurred, "" otherwise
		};

		inline SQueryResult::SQueryResult(const CQueryID& _queryID, EStatus _status, QueryResultSetUniquePtr& _pResultSet, const char* _szError)
			: queryID(_queryID)
			, status(_status)
			, pResultSet(_pResultSet)
			, szError(_szError)
		{}

		inline SQueryResult SQueryResult::CreateSuccess(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSet)
		{
			assert(_pResultSet != nullptr);
			return SQueryResult(_queryID, EStatus::Success, _pResultSet, "");
		}

		inline SQueryResult SQueryResult::CreateError(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSetDummy, const char* _szError)
		{
			return SQueryResult(_queryID, EStatus::ExceptionOccurred, _pResultSetDummy, _szError);
		}

		inline SQueryResult SQueryResult::CreateCanceledByHubTearDown(const CQueryID& _queryID, QueryResultSetUniquePtr& _pResultSetDummy)
		{
			return SQueryResult(_queryID, EStatus::CanceledByHubTearDown, _pResultSetDummy, "");
		}

	}
}
