// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//
		// 2 classes:
		//
		//   CQueryHost
		//   CQueryHostT<>
		//


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
			// - the returned query ID may be invalid (which happens when the query couldn't even be started, e. g. due to missing runtime-parameters) (in this case, the callback will be triggered as well)
			Core::CQueryID                  StartQuery();

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

		inline CQueryHost::CQueryHost()
			: m_runningStatus(ERunningStatus::HasNotBeenStartedYet)
			, m_pExpectedOutputType(nullptr)
			, m_queryBlueprintID()
			, m_pCallback(nullptr)
			, m_pCallbackUserData(nullptr)
			, m_runtimeParams()
			, m_queryID(Core::CQueryID::CreateInvalid())
			, m_pResultSet()
		{
			// nothing
		}

		inline CQueryHost::~CQueryHost()
		{
			// cancel a possibly running query
			if (m_queryID.IsValid())
			{
				if (Core::IHub* pHub = Core::IHubPlugin::GetHubPtr())
				{
					pHub->GetQueryManager().CancelQuery(m_queryID);
				}
			}
		}

		inline void CQueryHost::SetExpectedOutputType(const Shared::CTypeInfo* pExpectedOutputType)
		{
			m_pExpectedOutputType = pExpectedOutputType;
		}

		inline void CQueryHost::SetQueryBlueprint(const char* szQueryBlueprintName)
		{
			if (Core::IHub* pHub = Core::IHubPlugin::GetHubPtr())
			{
				m_queryBlueprintID = pHub->GetQueryBlueprintLibrary().FindQueryBlueprintIDByName(szQueryBlueprintName);
			}

			m_queryBlueprintName = szQueryBlueprintName;
		}

		inline const char* CQueryHost::GetQueryBlueprintName() const
		{
			return m_queryBlueprintName.c_str();
		}

		inline void CQueryHost::SetQueryBlueprint(const Core::CQueryBlueprintID& queryBlueprintID)
		{
			m_queryBlueprintID = queryBlueprintID;
			m_queryBlueprintName = queryBlueprintID.GetQueryBlueprintName();
		}

		inline void CQueryHost::SetQuerierName(const char* szQuerierName)
		{
			m_querierName = szQuerierName;
		}

		inline void CQueryHost::SetCallback(const Functor1<void*>& pCallback, void* pUserData /* = nullptr */)
		{
			m_pCallback = pCallback;
			m_pCallbackUserData = pUserData;
		}

		inline Shared::CVariantDict& CQueryHost::GetRuntimeParamsStorage()
		{
			return m_runtimeParams;
		}

		inline Core::CQueryID CQueryHost::StartQuery()
		{
			HelpStartQuery();

			// check for whether starting the query already caused an exception and report to the caller
			if (m_runningStatus == ERunningStatus::ExceptionOccurred)
			{
				if (m_pCallback)
				{
					m_pCallback(m_pCallbackUserData);
				}
			}

			return m_queryID;
		}

		inline void CQueryHost::CancelQuery()
		{
			if (m_queryID.IsValid())
			{
				if (Core::IHub* pHub = Core::IHubPlugin::GetHubPtr())
				{
					pHub->GetQueryManager().CancelQuery(m_queryID);
				}

				m_queryID = Core::CQueryID::CreateInvalid();
			}
		}

		inline void CQueryHost::HelpStartQuery()
		{
			Core::IHub* pHub = Core::IHubPlugin::GetHubPtr();

			//
			// get rid of the previous result set (if not yet done via ClearResultSet())
			//

			m_pResultSet.reset();

			//
			// cancel a possibly running query
			//

			if (m_queryID.IsValid())
			{
				if (pHub)
				{
					pHub->GetQueryManager().CancelQuery(m_queryID);
				}

				m_queryID = Core::CQueryID::CreateInvalid();
			}

			//
			// ensure the UQS plugin is present
			//

			if (!pHub)
			{
				m_exceptionMessageIfAny = "UQS Plugin is not present.";
				m_runningStatus = ERunningStatus::ExceptionOccurred;
				return;
			}

			//
			// retrieve the query blueprint for a type check below
			// (it could be that the blueprint has been reloaded after it was changed by the designer [at runtime!] and is now producing a different kind of items)
			//

			const Core::IQueryBlueprint* pQueryBlueprint = pHub->GetQueryBlueprintLibrary().GetQueryBlueprintByID(m_queryBlueprintID);
			if (!pQueryBlueprint)
			{
				m_exceptionMessageIfAny.Format("Query blueprint '%s' is not present in the blueprint library.", m_queryBlueprintID.GetQueryBlueprintName());
				m_runningStatus = ERunningStatus::ExceptionOccurred;
				return;
			}

			//
			// if the caller expects a certain item type in the result set then verify it
			//

			if (m_pExpectedOutputType)
			{
				const Shared::CTypeInfo& typeOfGeneratedItems = pQueryBlueprint->GetOutputType();

				if (*m_pExpectedOutputType != typeOfGeneratedItems)
				{
					m_exceptionMessageIfAny.Format("Query blueprint '%s': item type mismatch: generated items are of type %s, but we expected %s", m_queryBlueprintID.GetQueryBlueprintName(), typeOfGeneratedItems.name(), m_pExpectedOutputType->name());
					m_runningStatus = ERunningStatus::ExceptionOccurred;
					return;
				}
			}

			//
			// start the query
			//

			const SQueryRequest queryRequest(m_queryBlueprintID, m_runtimeParams, m_querierName.c_str(), functor(*this, &CQueryHost::OnUQSQueryFinished));
			UQS::Shared::CUqsString error;
			m_queryID = pHub->GetQueryManager().StartQuery(queryRequest, error);

			if (m_queryID.IsValid())
			{
				m_runningStatus = ERunningStatus::StillRunning;
			}
			else
			{
				m_exceptionMessageIfAny = error.c_str();
				m_runningStatus = ERunningStatus::ExceptionOccurred;
			}
		}

		inline bool CQueryHost::IsStillRunning() const
		{
			return m_runningStatus == ERunningStatus::StillRunning;
		}

		inline bool CQueryHost::HasSucceeded() const
		{
			return m_runningStatus == ERunningStatus::FinishedWithSuccess;
		}

		inline const Core::IQueryResultSet& CQueryHost::GetResultSet() const
		{
			assert(m_runningStatus == ERunningStatus::FinishedWithSuccess);
			assert(m_pResultSet);
			return *m_pResultSet;
		}

		inline const char* CQueryHost::GetExceptionMessage() const
		{
			assert(m_runningStatus == ERunningStatus::ExceptionOccurred);
			return m_exceptionMessageIfAny.c_str();
		}

		inline void CQueryHost::ClearResultSet()
		{
			m_pResultSet.reset();
		}

		inline void CQueryHost::OnUQSQueryFinished(const Core::SQueryResult& result)
		{
			assert(result.queryID == m_queryID);

			m_queryID = Core::CQueryID::CreateInvalid();

			switch (result.status)
			{
			case Core::SQueryResult::EStatus::Success:
				m_pResultSet = std::move(result.pResultSet);
				m_runningStatus = ERunningStatus::FinishedWithSuccess;
				break;

			case Core::SQueryResult::EStatus::ExceptionOccurred:
				m_exceptionMessageIfAny = result.szError;
				m_runningStatus = ERunningStatus::ExceptionOccurred;
				break;

			case Core::SQueryResult::EStatus::CanceledByHubTearDown:
				// FIXME: is it OK to treat this as an exception as well? (after all, the query hasn't fully finished!)
				m_exceptionMessageIfAny = "Query got canceled due to Hub being torn down now.";
				m_runningStatus = ERunningStatus::ExceptionOccurred;
				break;

			default:
				assert(0);
				m_exceptionMessageIfAny = "CQueryHost::OnUQSQueryFinished: unhandled status enum.";
				m_runningStatus = ERunningStatus::ExceptionOccurred;
				break;
			}

			if (m_pCallback)
			{
				m_pCallback(m_pCallbackUserData);
			}
		}

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
