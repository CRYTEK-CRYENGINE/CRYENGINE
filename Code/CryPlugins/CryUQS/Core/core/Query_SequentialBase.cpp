// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CQuery_SequentialBase::CQuery_SequentialBase(const SCtorContext& ctorContext)
			: CQueryBase(ctorContext)
			, m_priority(ctorContext.priority)
			, m_indexOfNextChildToInstantiate(0)
			, m_bExceptionOccurredUponChildInstantiation(false)
		{
			// nothing
		}

		bool CQuery_SequentialBase::OnStart(const Shared::IVariantDict& runtimeParams, Shared::IUqsString& error)
		{
			// no children? -> exception
			if (m_pQueryBlueprint->GetChildCount() == 0)
			{
				error.Format("CQuery_SequentialBase::OnStart: at least 1 child is required to instanitate this query");
				return false;
			}

			// keep the runtime-params around for the child queries to instantiate
			runtimeParams.AddSelfToOtherAndReplace(m_runtimeParams);

			// instantiate the first child query
			InstantiateNextChildQueryBlueprint(m_pOptionalShuttledItems);
			if (m_bExceptionOccurredUponChildInstantiation)
			{
				error.Set(m_exceptionMessageFromChildInstantiation.c_str());
				return false;
			}

			return true;
		}

		CQueryBase::EUpdateState CQuery_SequentialBase::OnUpdate(const CTimeValue& amountOfGrantedTime, Shared::CUqsString& error)
		{
			CRY_ASSERT(m_pCurrentlyRunningChildQuery);
			CRY_ASSERT(!m_bExceptionOccurredUponChildInstantiation);

			//
			// update the child query and deal with its status
			// (we do this in an infinite loop to update newly instantiated children in a timely manner to prevent unnecessary latency)
			//

			CTimeValue remainingTimeBudget = amountOfGrantedTime;

			while (1)
			{
				const CTimeValue timestampBefore = gEnv->pTimer->GetAsyncTime();
				const EUpdateState state = m_pCurrentlyRunningChildQuery->Update(remainingTimeBudget, error);
				const CTimeValue elapsedTime = gEnv->pTimer->GetAsyncTime() - timestampBefore;

				remainingTimeBudget -= elapsedTime;

				if (state == EUpdateState::StillRunning)
				{
					return EUpdateState::StillRunning;
				}

				if (state == EUpdateState::ExceptionOccurred)
				{
					return EUpdateState::ExceptionOccurred;
				}

				CRY_ASSERT(state == EUpdateState::Finished);

				// the query has finished => ask the derived class what to do next
				QueryBaseUniquePtr q = std::move(m_pCurrentlyRunningChildQuery);
				HandleChildQueryFinishedWithSuccess(*q);

				// the derived class may or may not have decided to instantiate further children
				// -> see if maybe an exception has occurred while trying to instantiate the next child
				if (m_bExceptionOccurredUponChildInstantiation)
				{
					error = m_exceptionMessageFromChildInstantiation;
					return EUpdateState::ExceptionOccurred;
				}

				if (m_pCurrentlyRunningChildQuery)
				{
					// the derived class has successfully instantiated the next child
					// => keep the update loop running if we have some time left (to make optimal use of the remaining time budget)

					if (remainingTimeBudget.GetValue() > 0)
					{
						// keep the update loop running
						continue;
					}
					else
					{
						// no more time left => continue updating the child query in the next frame
						return EUpdateState::StillRunning;
					}
				}

				// we're done (the derived class figured that there are no more child queries that it could instantiate or is already happy with the outcome of the just finished child)
				return EUpdateState::Finished;
			}
		}

		void CQuery_SequentialBase::OnCancel()
		{
			// cancel the possibly running child
			if (m_pCurrentlyRunningChildQuery)
			{
				m_pCurrentlyRunningChildQuery->Cancel();
				m_pCurrentlyRunningChildQuery.reset();
			}
		}

		void CQuery_SequentialBase::OnGetStatistics(SStatistics& out) const
		{
			// number of items in the result set
			// (this might be misleading as we're actually not generating items, yet a derived class may grab the resulting items from one of its children to hand it out to the caller)
			if (m_pResultSet != nullptr)
			{
				out.numItemsInFinalResultSet = m_pResultSet->GetResultCount();
			}

			// nothing else to add
		}

		bool CQuery_SequentialBase::HasMoreChildrenLeftToInstantiate() const
		{
			return (m_indexOfNextChildToInstantiate < m_pQueryBlueprint->GetChildCount());
		}

		void CQuery_SequentialBase::InstantiateNextChildQueryBlueprint(const std::shared_ptr<CItemList>& pResultingItemsOfPotentialPreviousChildQuery)
		{
			CRY_ASSERT(!m_bExceptionOccurredUponChildInstantiation);
			CRY_ASSERT(m_indexOfNextChildToInstantiate < m_pQueryBlueprint->GetChildCount());
			CRY_ASSERT(!m_pCurrentlyRunningChildQuery);

			const size_t indexOfChildToInstantiate = m_indexOfNextChildToInstantiate++;
			const std::shared_ptr<const CQueryBlueprint> pChildQueryBlueprintToInstantiate = m_pQueryBlueprint->GetChild(indexOfChildToInstantiate);

			m_pCurrentlyRunningChildQuery = CQueryBase::CreateQuery(
				m_queryID,
				pChildQueryBlueprintToInstantiate,
				m_querierName.c_str(),
				pResultingItemsOfPotentialPreviousChildQuery,
				m_priority);

			if (!m_pCurrentlyRunningChildQuery->Start(m_runtimeParams, m_exceptionMessageFromChildInstantiation))
			{
				m_bExceptionOccurredUponChildInstantiation = true;
			}
		}

	}
}
