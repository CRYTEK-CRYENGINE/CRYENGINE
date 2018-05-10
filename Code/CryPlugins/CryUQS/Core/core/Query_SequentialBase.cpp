// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CQuery_SequentialBase::CQuery_SequentialBase(const SCtorContext& ctorContext)
			: CQueryBase(ctorContext, false)
			, m_indexOfNextChildToInstantiate(0)
			, m_queryIDOfCurrentlyRunningChild(CQueryID::CreateInvalid())
			, m_bExceptionOccurredInChild(false)
		{
			// nothing
		}

		CQuery_SequentialBase::~CQuery_SequentialBase()
		{
			// - cancel the possibly running child
			// - it's important to get rid of the callback pointer before the child could possibly call back into us after we've been destroyed
			g_pHub->GetQueryManager().CancelQuery(m_queryIDOfCurrentlyRunningChild);
		}

		bool CQuery_SequentialBase::OnInstantiateFromQueryBlueprint(const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error)
		{
			// no children? -> exception
			if (m_pQueryBlueprint->GetChildCount() == 0)
			{
				error.Format("CQuery_SequentialBase::OnInstantiateFromQueryBlueprint: at least 1 child is required to instanitate this query");
				return false;
			}

			// keep the runtime-params around for the child queries to instantiate
			runtimeParams.AddSelfToOtherAndReplace(m_runtimeParams);

			// instantiate the first child query
			InstantiateNextChildQueryBlueprint(m_pOptionalShuttledItems);
			if (m_bExceptionOccurredInChild)
			{
				error = m_exceptionMessageFromChild;
				return false;
			}

			return true;
		}

		CQueryBase::EUpdateState CQuery_SequentialBase::OnUpdate(Shared::CUqsString& error)
		{
			// did an error occur during the last child query?
			if (m_bExceptionOccurredInChild)
			{
				error = m_exceptionMessageFromChild;
				return EUpdateState::ExceptionOccurred;
			}

			// we assume the following: if there are more children that could potentially run, then the derived class should have already instantiated the next one in its HandleChildQueryFinishedWithSuccess()
			// -> in other words: if none is running now, then this composite query counts as finished
			if (!m_queryIDOfCurrentlyRunningChild.IsValid())
			{
				// notice: the result set is expected to have already been made availabe to the caller in HandleChildQueryFinishedWithSuccess()
				return EUpdateState::Finished;
			}

			return EUpdateState::StillRunning;
		}

		void CQuery_SequentialBase::OnCancel()
		{
			// cancel the possibly running child
			g_pHub->GetQueryManager().CancelQuery(m_queryIDOfCurrentlyRunningChild);
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

		void CQuery_SequentialBase::OnChildQueryFinished(const SQueryResult& result)
		{
			assert(m_queryIDOfCurrentlyRunningChild == result.queryID);

			m_queryIDOfCurrentlyRunningChild = CQueryID::CreateInvalid();

			switch (result.status)
			{
			case SQueryResult::EStatus::Success:
				assert(result.pResultSet != nullptr);
				HandleChildQueryFinishedWithSuccess(result.queryID, std::move(result.pResultSet));
				break;

			case SQueryResult::EStatus::ExceptionOccurred:
				// track the exception for returning it on next OnUpdate()
				m_bExceptionOccurredInChild = true;
				m_exceptionMessageFromChild.Set(result.szError);
				break;

			case SQueryResult::EStatus::CanceledByHubTearDown:
				// (nothing)
				break;

			default:
				assert(0);
			}
		}

		bool CQuery_SequentialBase::HasMoreChildrenLeftToInstantiate() const
		{
			return (m_indexOfNextChildToInstantiate < m_pQueryBlueprint->GetChildCount());
		}

		void CQuery_SequentialBase::InstantiateNextChildQueryBlueprint(const std::shared_ptr<CItemList>& pResultingItemsOfPotentialPreviousChildQuery)
		{
			assert(m_indexOfNextChildToInstantiate < m_pQueryBlueprint->GetChildCount());
			assert(!m_queryIDOfCurrentlyRunningChild.IsValid());

			const size_t indexOfChildToInstantiate = m_indexOfNextChildToInstantiate++;
			std::shared_ptr<const CQueryBlueprint> pChildQueryBlueprintToInstantiate = m_pQueryBlueprint->GetChild(indexOfChildToInstantiate);

			stack_string querierName;
			querierName.Format("%s::[childQuery_#%i]", m_querierName.c_str(), (int)indexOfChildToInstantiate);

			m_queryIDOfCurrentlyRunningChild = g_pHub->GetQueryManager().StartQueryInternal(
				m_queryID,
				pChildQueryBlueprintToInstantiate,
				m_runtimeParams,
				querierName.c_str(),
				functor(*this, &CQuery_SequentialBase::OnChildQueryFinished),
				pResultingItemsOfPotentialPreviousChildQuery,
				m_exceptionMessageFromChild);

			if (!m_queryIDOfCurrentlyRunningChild.IsValid())
			{
				// the error message is already stored in m_exceptionMessageFromChild
				m_bExceptionOccurredInChild = true;
			}
		}

	}
}
