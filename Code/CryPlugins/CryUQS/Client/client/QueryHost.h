// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CQueryHost
		//
		// - a simple wrapper to make it easier to run a query
		// - it basically allows to start an arbitrary query blueprint, keeps track of its execution and allows access to the result set once the query is finished
		// - by optionally providing an expected item type, the caller can be assured that the items in the result set will be of exactly that type (otherwise, an exception would get triggered when trying to start the query)
		// - as such, he definitely needs to know the output type of the query he intends to run (which is the actual point of this class)
		//
		//===================================================================================

		class CQueryHost
		{
		private:

			enum class ERunningStatus
			{
				HasNotBeenStartedYet,
				StillRunning,
				ExceptionOccurred,
				FinishedWithSuccess,
			};

		public:

			explicit                        CQueryHost();
				                            ~CQueryHost();

			// - can be optionally used to specify what item type is expected in the result set
			// - the query will fail with an exception and not yield a result set if the types mismatch
			// - this is basically an assurance to always get the types that the caller can deal with
			// - persists when starting a new query
			void                            SetExpectedOutputType(const Shared::CTypeInfo* pExpectedOutputType);

			// - needs to be called before starting a query so that we know which query blueprint to instantiate
			// - persists when starting a new query
			void                            SetQueryBlueprint(const char* szQueryBlueprintName);
			void                            SetQueryBlueprint(const Core::CQueryBlueprintID& queryBlueprintID);

			// - returns the name of the query blueprint (previously set via SetQueryBlueprint())
			// - can be used for error messages
			const char*                     GetQueryBlueprintName() const;

			// - can be optionally used to specify a querier name for debugging purposes
			// - that querier name will then appear in the query history and allow for easier identification of certain queries
			// - persists when starting a new query
			void                            SetQuerierName(const char* szQuerierName);

			// - sets an optional callback that will get triggered once the running query finishes (no matter with or without success)
			// - setting a callback is not necessary, but can be used to circumvent polling the CQueryHost class until it reports that the running query has finished
			// - notice: the callback will also be triggered by StartQuery() if something went wrong when attempting to start the query (e. g. missing runtime-parameter)
			// - the void-pointer parameter in the callback is user data provided via 'pUserData'
			// - persists when starting a new query
			void                            SetCallback(const Functor1<void*>& pCallback, void* pUserData = nullptr);

			// - gives write-access to the runtime-parameters that the query may need
			// - persists when starting a new query (unless explicitly clearing them)
			Shared::CVariantDict&           GetRuntimeParamsStorage();

			// - actually starts the query that has been prepared so far
			// - cancels a possibly running query
			void                            StartQuery();

			// - prematurely cancels the possibly running query
			// - will *not* trigger the potential callback previously set via SetCallback()
			// - has no effect if no query is running at the moment
			void                            CancelQuery();

			// - returns whether the query started via StartQuery() is still running
			bool                            IsStillRunning() const;

			// - once IsStillRunning() returns false, this can be used to see if the query finished fine or encountered an exception
			// - notice: whether this returns true does *not* say anything about the potetnially found number of item in the result set (even 0 items counts as "success" as long as no excption occurred)
			bool                            HasSucceeded() const;

			// - gives access to the result set for a successfully finished query
			// - may *only* be called if HasSucceeded() returns true
			const Core::IQueryResultSet&    GetResultSet() const;

			// - allows inspecting what has gone wrong at the start or during the execution of the query
			// - this may *only* be called if HasSucceed() return false
			const char*                     GetExceptionMessage() const;

			// - allows clearing the potential result set of a finished query
			// - this can be done for freeing up the memory used by those items
			// - otherwise the result set will be cleared when starting a new query anyway
			void                            ClearResultSet();

		private:

			                                UQS_NON_COPYABLE(CQueryHost);

			void                            HelpStartQuery();  // helper for StartQuery()
			void                            OnUQSQueryFinished(const Core::SQueryResult& result);

		private:

			string                          m_queryBlueprintName;
			ERunningStatus                  m_runningStatus;
			const Shared::CTypeInfo*        m_pExpectedOutputType;
			Core::CQueryBlueprintID         m_queryBlueprintID;
			string                          m_querierName;
			Functor1<void*>                 m_pCallback;
			void*                           m_pCallbackUserData;
			Shared::CVariantDict            m_runtimeParams;
			Core::CQueryID                  m_queryID;
			Core::QueryResultSetUniquePtr   m_pResultSet;
			string                          m_exceptionMessageIfAny;
		};

		//===================================================================================
		//
		// CQueryHostT<>
		//
		// - template version of CQueryHost for direct use in code
		// - can be used when the caller already knows the query's output type at compile time
		// - this simplifies type-safety as the caller no longer needs to specify the expected item type and also gets type-safe access to the result set
		// - but still, the output may still mismatch if the caller accidentally specifies the wrong query blueprint (or if it gets edited at runtime)
		//
		//===================================================================================

		template <class TItem>
		class CQueryHostT : private CQueryHost
		{
		public:

			struct SResultingItemWithScore
			{
				const TItem&                item;
				float                       score;
			};

		public:

			explicit                        CQueryHostT();

			using                           CQueryHost::SetQueryBlueprint;
			using                           CQueryHost::GetQueryBlueprintName;
			using                           CQueryHost::SetQuerierName;
			using                           CQueryHost::SetCallback;
			using                           CQueryHost::GetRuntimeParamsStorage;
			using                           CQueryHost::StartQuery;
			using                           CQueryHost::CancelQuery;
			using                           CQueryHost::IsStillRunning;
			using                           CQueryHost::HasSucceeded;
			using                           CQueryHost::GetExceptionMessage;
			using                           CQueryHost::ClearResultSet;

			size_t                          GetResultSetItemCount() const;
			SResultingItemWithScore         GetResultSetItem(size_t index) const;
		};

		template <class TItem>
		CQueryHostT<TItem>::CQueryHostT()
		{
			SetExpectedOutputType(&Shared::SDataTypeHelper<TItem>::GetTypeInfo());
		}

		template <class TItem>
		size_t CQueryHostT<TItem>::GetResultSetItemCount() const
		{
			return GetResultSet().GetResultCount();
		}

		template <class TItem>
		typename CQueryHostT<TItem>::SResultingItemWithScore CQueryHostT<TItem>::GetResultSetItem(size_t index) const
		{
			assert(GetResultSet().GetItemFactory().GetItemType() == Shared::SDataTypeHelper<TItem>::GetTypeInfo());
			const Core::IQueryResultSet::SResultSetEntry& resultSetEntry = GetResultSet().GetResult(index);
			SResultingItemWithScore res = { *static_cast<const TItem*>(resultSetEntry.pItem), resultSetEntry.score };
			return res;
		}

	}
}
