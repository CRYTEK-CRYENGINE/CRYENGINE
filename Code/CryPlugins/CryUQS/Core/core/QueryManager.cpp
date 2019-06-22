// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryManager.h"
#include <CryRenderer/IRenderer.h>
#include <numeric>	// std::accumulate()

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryManager::SRunningQueryInfo
		//
		//===================================================================================

		CQueryManager::SRunningQueryInfo::SRunningQueryInfo()
			: pQuery()
			, pQueryBlueprint()
			, pCallback(0)
			, priority(0)	// invalid default value
			, pWorkingData(new SWorkingData)
		{}

		//===================================================================================
		//
		// CQueryManager::SFinishedQueryInfo
		//
		//===================================================================================

		CQueryManager::SFinishedQueryInfo::SFinishedQueryInfo(const std::shared_ptr<CQueryBase>& _pQuery, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, const Functor1<const SQueryResult&>& _pCallback, int _priority, const CTimeValue& _queryFinishedTimestamp, bool _bQueryFinishedWithSuccess, const string& _errorIfAny)
			: pQuery(_pQuery)
			, pQueryBlueprint(_pQueryBlueprint)
			, pCallback(_pCallback)
			, priority(_priority)
			, queryFinishedTimestamp(_queryFinishedTimestamp)
			, bQueryFinishedWithSuccess(_bQueryFinishedWithSuccess)
			, errorIfAny(_errorIfAny)
		{}

		//===================================================================================
		//
		// CQueryManager::SHistoryQueryInfo2D
		//
		//===================================================================================

		CQueryManager::SHistoryQueryInfo2D::SHistoryQueryInfo2D(const CQueryID &_queryID, int _priority, const CQueryBase::SStatistics& _statistics, bool _bQueryFinishedWithSuccess, const CTimeValue& _timestamp)
			: queryID(_queryID)
			, priority(_priority)
			, statistics(_statistics)
			, bQueryFinishedWithSuccess(_bQueryFinishedWithSuccess)
			, finishedTimestamp(_timestamp)
		{}

		//===================================================================================
		//
		// CQueryManager::CPredEqualQueryID
		//
		//===================================================================================

		CQueryManager::CPredEqualQueryID::CPredEqualQueryID(const CQueryID& queryID)
			: m_queryID(queryID)
		{}

		bool CQueryManager::CPredEqualQueryID::operator()(const SRunningQueryInfo& runningQueryInfo) const
		{
			return m_queryID == runningQueryInfo.pQuery->GetQueryID();
		}

		//===================================================================================
		//
		// CQueryManager
		//
		//===================================================================================

		const CTimeValue CQueryManager::s_delayBeforeFadeOut(2.5f);
		const CTimeValue CQueryManager::s_fadeOutDuration(0.5f);
		const CTimeValue CQueryManager::s_totalDebugDrawDuration = CQueryManager::s_delayBeforeFadeOut + CQueryManager::s_fadeOutDuration;

		CQueryManager::CQueryManager()
			: m_roundRobinStart(m_queries.cend())
			, m_bQueriesUpdateInProgress(false)
		{
			// nothing
		}

		CQueryID CQueryManager::StartQuery(const Client::SQueryRequest& request, Shared::IUqsString& errorMessage)
		{
			if (!request.queryBlueprintID.IsOrHasBeenValid())
			{
				// note: the caller should have already become suspicious when he searched for a specific blueprint (by name) but received a CQueryBlueprintID that is not and never has been valid
				errorMessage.Format("CQueryManager::StartQuery: unknown query blueprint: '%s'", request.queryBlueprintID.GetQueryBlueprintName());
				return CQueryID::CreateInvalid();
			}

			std::shared_ptr<const CQueryBlueprint> qbp = g_pHub->GetQueryBlueprintLibrary().GetQueryBlueprintByIDInternal(request.queryBlueprintID);
			if (!qbp)
			{
				errorMessage.Format("CQueryManager::StartQuery: the blueprint '%s' was once in the library, but has been removed and not been (successfully) reloaded since then", request.queryBlueprintID.GetQueryBlueprintName());
				return CQueryID::CreateInvalid();
			}

			if (request.priority < 1)
			{
				errorMessage.Format("CQueryManager::StartQuery: bad 'priority' level given: %i (should be > 0)", request.priority);
				return CQueryID::CreateInvalid();
			}

			static const CQueryID noParentQueryID = CQueryID::CreateInvalid();
			static const std::shared_ptr<CItemList> pEmptyResultSinceThereIsNoPreviousQuery;

			QueryBaseUniquePtr q = CQueryBase::CreateQuery(noParentQueryID, qbp, request.szQuerierName, pEmptyResultSinceThereIsNoPreviousQuery, request.priority);

			if(!q->Start(request.runtimeParams, errorMessage))
			{
				// something went wrong (e.g. missing runtime parameter)
				SFinishedQueryInfo finishedQueryInfo(
					std::move(q),		// unique_ptr -> shared_ptr conversion
					qbp,
					nullptr,
					request.priority,
					gEnv->pTimer->GetAsyncTime(),
					false,
					errorMessage.c_str());
				NotifyCallbacksOfFinishedQuery(finishedQueryInfo);
				return CQueryID::CreateInvalid();
			}

			// keep track of and update the new query from now on
			SRunningQueryInfo newEntry;
			newEntry.pQuery = std::move(q);		// unique_ptr -> shared_ptr conversion
			newEntry.pQueryBlueprint = qbp;
			newEntry.pCallback = request.callback;
			newEntry.priority = request.priority;
			newEntry.pWorkingData->remainingPriorityForNextFrame = request.priority;
			m_queries.emplace_back(std::move(newEntry));

			return m_queries.back().pQuery->GetQueryID();
		}

		void CQueryManager::CancelQuery(const CQueryID& idOfQueryToCancel)
		{
			CRY_ASSERT(!m_bQueriesUpdateInProgress);

			auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(idOfQueryToCancel));
			if (it != m_queries.end())
			{
				it->pQuery->Cancel();
				if (m_roundRobinStart == it)
				{
					m_roundRobinStart = std::next(it);
				}
				m_queries.erase(it);
			}
		}

		void CQueryManager::AddItemMonitorToQuery(const CQueryID& queryID, Client::ItemMonitorUniquePtr&& pItemMonitorToInstall)
		{
			CRY_ASSERT(pItemMonitorToInstall);

			if (CQueryBase* pQuery = FindQueryByQueryID(queryID))
			{
				pQuery->AddItemMonitor(std::move(pItemMonitorToInstall));
			}
		}

		void CQueryManager::RegisterQueryFinishedListener(Client::IQueryFinishedListener* pListenerToRegister)
		{
			stl::push_back_unique(m_queryFinishedListeners, pListenerToRegister);
		}

		void CQueryManager::UnregisterQueryFinishedListener(Client::IQueryFinishedListener* pListenerToUnregister)
		{
			stl::find_and_erase_all(m_queryFinishedListeners, pListenerToUnregister);
		}

		void CQueryManager::RegisterQueryWarningListener(Client::IQueryWarningListener* pListenerToRegister)
		{
			stl::push_back_unique(m_queryWarningListeners, pListenerToRegister);
		}

		void CQueryManager::UnregisterQueryWarningListener(Client::IQueryWarningListener* pListenerToUnregister)
		{
			stl::find_and_erase_all(m_queryWarningListeners, pListenerToUnregister);
		}

		void CQueryManager::VisitRunningQueries(Client::IQueryVisitor& visitor)
		{
			for (const SRunningQueryInfo& runningQueryInfo : m_queries)
			{
				CQueryBase::SStatistics stats;
				runningQueryInfo.pQuery->GetStatistics(stats);

				const Client::IQueryVisitor::SQueryInfo queryInfo(
					runningQueryInfo.pQuery->GetQueryID(),
					stats.querierName.c_str(),
					stats.queryBlueprintName.c_str(),
					runningQueryInfo.priority,
					(int)stats.numGeneratedItems,
					(int)stats.numRemainingItemsToInspect,
					(int)stats.queryCreatedFrame,
					stats.queryCreatedTimestamp,
					(int)stats.totalConsumedFrames,
					(int)(gEnv->nMainFrameID - stats.queryCreatedFrame),
					stats.totalConsumedTime);

				visitor.OnQueryVisited(queryInfo);
			}
		}

		CQueryBase* CQueryManager::FindQueryByQueryID(const CQueryID& queryID)
		{
			auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(queryID));
			return (it == m_queries.end()) ? nullptr : it->pQuery.get();
		}

		void CQueryManager::Update()
		{
			UpdateQueries();

			ExpireDebugDrawStatisticHistory2D();
		}

		void CQueryManager::DebugDrawRunningQueriesStatistics2D() const
		{
			const CTimeValue now = gEnv->pTimer->GetAsyncTime();

			//
			// visualize the current round-robin load
			//

			DebugDrawRoundRobinLoad();

			//
			// number of currently running queries
			//

			int row = 8;	// the round-robin slots are rendered just above
			CDrawUtil2d::DrawLabel(row, Col_White, "=== %i UQS queries currently running ===", (int)m_queries.size());
			++row;

			//
			// draw a 2D statistics history with fade-out effect: green = query succeeded (even if no item was found), red = query failed unexpectedly
			//

			for (const SHistoryQueryInfo2D& historyEntry : m_debugDrawHistory2D)
			{
				const CTimeValue age = (now - historyEntry.finishedTimestamp);
				const float alpha = (age < s_delayBeforeFadeOut) ? 1.0f : clamp_tpl(1.0f - (age - s_delayBeforeFadeOut).GetSeconds() / s_fadeOutDuration.GetSeconds(), 0.0f, 1.0f);
				const ColorF color = historyEntry.bQueryFinishedWithSuccess ? ColorF(0.0f, 1.0f, 0.0f, alpha) : ColorF(1.0f, 0.0f, 0.0f, alpha);
				row = DebugDrawQueryStatistics(historyEntry.statistics, historyEntry.queryID, historyEntry.priority, row, color);
				++row;
			}

			//
			// draw statistics of all ongoing queries in white
			//

			for (const SRunningQueryInfo& runningQueryInfo : m_queries)
			{
				CQueryBase::SStatistics stats;
				runningQueryInfo.pQuery->GetStatistics(stats);
				row = DebugDrawQueryStatistics(stats, runningQueryInfo.pQuery->GetQueryID(), runningQueryInfo.priority, row, Col_White);
				++row;
			}
		}

		CTimeValue CQueryManager::HelpUpdateSingleQuery(const CQueryManager::SRunningQueryInfo& queryToUpdate, const CTimeValue& timeBudgetForThisQuery, std::vector<CQueryManager::SFinishedQueryInfo>& outFinishedQueries)
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			//
			// - update the query and deal with its update status
			// - keep track of the used time to return it to the caller
			//

			Shared::CUqsString error;
			const CTimeValue timestampBeforeQueryUpdate = gEnv->pTimer->GetAsyncTime();
			const CQueryBase::EUpdateState queryState = queryToUpdate.pQuery->Update(timeBudgetForThisQuery, error);
			const CTimeValue timestampAfterQueryUpdate = gEnv->pTimer->GetAsyncTime();

			//
			// deal with the query's update status
			//

			switch (queryState)
			{
			case CQueryBase::EUpdateState::StillRunning:
				// nothing (keep it running)
				break;

			case CQueryBase::EUpdateState::Finished:
				{
					const bool bQueryFinishedWithSuccess = true;
					outFinishedQueries.emplace_back(
						queryToUpdate.pQuery,
						queryToUpdate.pQueryBlueprint,
						queryToUpdate.pCallback,
						queryToUpdate.priority,
						timestampAfterQueryUpdate,
						bQueryFinishedWithSuccess, "");
				}
				break;

			case CQueryBase::EUpdateState::ExceptionOccurred:
				{
					const bool bQueryFinishedWithSuccess = false;
					outFinishedQueries.emplace_back(
						queryToUpdate.pQuery,
						queryToUpdate.pQueryBlueprint,
						queryToUpdate.pCallback,
						queryToUpdate.priority,
						timestampAfterQueryUpdate,
						bQueryFinishedWithSuccess,
						error.c_str());
				}
				break;

			default:
				CRY_ASSERT(0);
			}

			return timestampAfterQueryUpdate - timestampBeforeQueryUpdate;
		}

		void CQueryManager::UpdateQueries()
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			if (m_queries.empty())
				return;

			std::vector<const SRunningQueryInfo*> roundRobinQueries;
			std::vector<SFinishedQueryInfo> finishedQueries;

			finishedQueries.reserve(m_queries.size());
			roundRobinQueries.reserve(m_queries.size());

			m_bQueriesUpdateInProgress = true;

			//
			// build a temporary round-robin list of queries that should be updated in this frame
			//

			BuildRoundRobinList(roundRobinQueries);

			//
			// update all queries in the round-robin container
			//

			UpdateRoundRobinQueries(roundRobinQueries, finishedQueries);

			m_bQueriesUpdateInProgress = false;

			//
			// deal with freshly finished queries
			//

			FinalizeFinishedQueries(finishedQueries);
		}

		void CQueryManager::BuildRoundRobinList(std::vector<const SRunningQueryInfo*>& outRoundRobinQueries)
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			CRY_ASSERT(!m_queries.empty());	// optimization to circumvent special cases

			std::list<SRunningQueryInfo>::const_iterator it = m_roundRobinStart;
			std::list<SRunningQueryInfo>::const_iterator itLastAddedQueryToRoundRobinList = m_queries.cend();

			// start from the beginning if already residing at the end (this would be the initial situation or when removing the back-most query)
			if (it == m_queries.cend())
			{
				it = m_roundRobinStart = m_queries.cbegin();
			}

			int remainingCakeSlices = SCvars::roundRobinLimit > 0 ? SCvars::roundRobinLimit : m_queries.size();

			while (1)
			{
				const SRunningQueryInfo& runningInfo = *it;

				//
				// put this query into the round-robin list
				//

				CRY_ASSERT(runningInfo.pWorkingData->remainingPriorityForNextFrame > 0);

				// grant this query a piece of the time-budget cake (in "cake slice" units)
				const int numSlicesToHandOut = std::min(remainingCakeSlices, runningInfo.pWorkingData->remainingPriorityForNextFrame);
				runningInfo.pWorkingData->handedOutPriorityInCurrentFrame = numSlicesToHandOut;
				runningInfo.pWorkingData->remainingPriorityForNextFrame -= numSlicesToHandOut;
				remainingCakeSlices -= numSlicesToHandOut;

				// add it to the round-robin list
				outRoundRobinQueries.push_back(&runningInfo);
				itLastAddedQueryToRoundRobinList = it;

				// reached round-robin capacity?
				if ((int)outRoundRobinQueries.size() == SCvars::roundRobinLimit)
					break;

				// no more cake slices left?
				if (remainingCakeSlices == 0)
					break;

				// advance to next query (and wrap around at the end)
				if (++it == m_queries.cend())
					it = m_queries.cbegin();

				// stop if one time around the list already
				if (it == m_roundRobinStart)
					break;
			}

			//
			// update the round-robin bookmark for next frame:
			// - if there is a query whose priority spills over to the next frame, then continue with that query
			// - but if all queries in the round-robin list perfectly fit their priorities into the round-robin limit, then proceed with the next query in the list
			//

			if (!outRoundRobinQueries.empty())
			{
				if (itLastAddedQueryToRoundRobinList->pWorkingData->remainingPriorityForNextFrame > 0)
				{
					// have this query spill over to the next frame
					m_roundRobinStart = itLastAddedQueryToRoundRobinList;
				}
				else
				{
					// continue the round-robin update on the next frame from here on
					m_roundRobinStart = it;
				}
			}

			// debug: track last round-robin load for debug rendering on screen
			if (SCvars::debugDraw)
			{
				m_roundRobinDebugSnapshot.roundRobinLimit = SCvars::roundRobinLimit > 0 ? SCvars::roundRobinLimit : m_queries.size();
				m_roundRobinDebugSnapshot.elements.clear();

				for (const SRunningQueryInfo* pRunningInfo : outRoundRobinQueries)
				{
					SRoundRobinDebugSnapshot::SElement debugElement;
					pRunningInfo->pQuery->GetQueryID().ToString(debugElement.queryIdAsString);
					debugElement.priority = pRunningInfo->priority;
					debugElement.priorityHandedOut = pRunningInfo->pWorkingData->handedOutPriorityInCurrentFrame;
					debugElement.priorityRemaining = pRunningInfo->pWorkingData->remainingPriorityForNextFrame;
					m_roundRobinDebugSnapshot.elements.push_back(debugElement);
				}
			}
		}

		void CQueryManager::UpdateRoundRobinQueries(const std::vector<const SRunningQueryInfo*>& roundRobinQueries, std::vector<SFinishedQueryInfo>& outFinishedQueries)
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			const CTimeValue totalTimeBudget(SCvars::timeBudgetInSeconds);
			const CTimeValue extraTimeBufferBeforeWarning(SCvars::timeBudgetExcessThresholdInPercent * 0.01f * SCvars::timeBudgetInSeconds);

			CTimeValue remainingOverallTimeBudget(SCvars::timeBudgetInSeconds);

			for (auto it = roundRobinQueries.cbegin(); it != roundRobinQueries.cend(); ++it)
			{
				auto accumulateRemainingPriorities = [](int accumulatedPriosSoFar, const SRunningQueryInfo* pRunningInfo)
				{
					return accumulatedPriosSoFar + pRunningInfo->pWorkingData->handedOutPriorityInCurrentFrame;
				};

				const int accumulatedPrioritiesOfRemainingQueries = std::accumulate(it, roundRobinQueries.cend(), 0, accumulateRemainingPriorities);

				//
				// compute the time-budget to grant in relation to how much the other queries in the round-robin list would still get granted
				//

				const SRunningQueryInfo* pRunningInfo = *it;
				const float fractionOfOverallRemainingTimeToGrant = (float)pRunningInfo->pWorkingData->handedOutPriorityInCurrentFrame / (float)accumulatedPrioritiesOfRemainingQueries;
				const CTimeValue timeBudgetForThisQuery(remainingOverallTimeBudget.GetSeconds() * fractionOfOverallRemainingTimeToGrant);

				//
				// run the query
				//

				const CTimeValue timeUsedByThisQuery = HelpUpdateSingleQuery(*pRunningInfo, timeBudgetForThisQuery, outFinishedQueries);

				//
				// check for some unused time and donate it to the remaining queries
				//

				if (timeUsedByThisQuery <= timeBudgetForThisQuery)
				{
					// donate the unused time
					remainingOverallTimeBudget += (timeBudgetForThisQuery - timeUsedByThisQuery);
				}
				else
				{
					remainingOverallTimeBudget -= timeBudgetForThisQuery;

					//
					// check for having exceeded even the extra time buffer and emit a warning if so
					//

					if (timeUsedByThisQuery > timeBudgetForThisQuery + extraTimeBufferBeforeWarning)
					{
						NotifyOfQueryPerformanceWarning(*pRunningInfo, "system frame #%i: query has just consumed %.1f%% of its granted time: %fms vs %fms",
							(int)gEnv->nMainFrameID,
							(timeUsedByThisQuery.GetMilliSeconds() / timeBudgetForThisQuery.GetMilliSeconds()) * 100.0f,
							timeUsedByThisQuery.GetMilliSeconds(),
							timeBudgetForThisQuery.GetMilliSeconds());
					}
				}

				//
				// restore the query's priority once it has used all the time-budget that it was granted (possibly over multiple frames)
				//

				if (pRunningInfo->pWorkingData->remainingPriorityForNextFrame == 0)
				{
					pRunningInfo->pWorkingData->remainingPriorityForNextFrame = pRunningInfo->priority;
				}
			}
		}

		void CQueryManager::FinalizeFinishedQueries(const std::vector<SFinishedQueryInfo>& finishedQueries)
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			//
			// first, notify all listeners that these queries have finished
			//

			for (const SFinishedQueryInfo& entry : finishedQueries)
			{
				// Although technically not needed, we check for the existence of the freshly finished query via FindQueryByQueryID(), because
				// the callback of another query, that finished in the exact same frame, might have just canceled this query via CancelQuery(), 
				// thus removing it from m_queries. The intention of the caller might have been to cancel the said query before it could possibly 
				// call back with the result-set and trigger something else, though it was canceled just before and therefore should logically 
				// not exist anymore.

				if (FindQueryByQueryID(entry.pQuery->GetQueryID()))
				{
					NotifyCallbacksOfFinishedQuery(entry);

					// add a new entry to the debug history for 2D on-screen rendering
					if (SCvars::debugDraw)
					{
						CQueryBase::SStatistics stats;
						entry.pQuery->GetStatistics(stats);
						SHistoryQueryInfo2D newHistoryEntry(entry.pQuery->GetQueryID(), entry.priority, stats, entry.bQueryFinishedWithSuccess, gEnv->pTimer->GetAsyncTime());
						m_debugDrawHistory2D.push_back(std::move(newHistoryEntry));
					}
				}
			}

			//
			// now remove all the finished queries
			//

			for (const SFinishedQueryInfo& entry : finishedQueries)
			{
				auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(entry.pQuery->GetQueryID()));
				if (it != m_queries.end())   // this may fail if a finished query got explicitly canceled in the callback from above
				{
					if (m_roundRobinStart == it)
					{
						m_roundRobinStart = std::next(it);
					}
					m_queries.erase(it);
				}
			}
		}

		void CQueryManager::NotifyCallbacksOfFinishedQuery(const SFinishedQueryInfo& finishedQueryInfo) const
		{
			CRY_PROFILE_FUNCTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, finishedQueryInfo.pQueryBlueprint->GetName());

			//
			// call the callback that is tied to given query
			//

			if (finishedQueryInfo.pCallback)
			{
				if (finishedQueryInfo.bQueryFinishedWithSuccess)
				{
					QueryResultSetUniquePtr pResultSet = finishedQueryInfo.pQuery->ClaimResultSet();
					const SQueryResult result = SQueryResult::CreateSuccess(finishedQueryInfo.pQuery->GetQueryID(), pResultSet);
					finishedQueryInfo.pCallback(result);
				}
				else
				{
					QueryResultSetUniquePtr pResultSetDummy;
					const SQueryResult result = SQueryResult::CreateError(finishedQueryInfo.pQuery->GetQueryID(), pResultSetDummy, finishedQueryInfo.errorIfAny.c_str());
					finishedQueryInfo.pCallback(result);
				}
			}

			//
			// call all global callbacks (that are interested in *all* finished queries)
			//

			if (!m_queryFinishedListeners.empty())
			{
				CQueryBase::SStatistics stats;
				finishedQueryInfo.pQuery->GetStatistics(stats);

				const Client::IQueryFinishedListener::SQueryInfo queryInfo(
					finishedQueryInfo.pQuery->GetQueryID(),
					stats.querierName.c_str(),
					stats.queryBlueprintName.c_str(),
					(int)stats.numGeneratedItems,
					(int)stats.numItemsInFinalResultSet,
					stats.queryCreatedTimestamp,
					finishedQueryInfo.queryFinishedTimestamp,
					(int)stats.totalConsumedFrames,
					(int)(gEnv->nMainFrameID - stats.queryCreatedFrame),
					!finishedQueryInfo.bQueryFinishedWithSuccess,
					finishedQueryInfo.errorIfAny.c_str());

				for (Client::IQueryFinishedListener* pListener : m_queryFinishedListeners)
				{
					pListener->OnQueryFinished(queryInfo);
				}
			}
		}

		void CQueryManager::NotifyOfQueryPerformanceWarning(const SRunningQueryInfo& problematicQuery, const char* szFmt, ...) const
		{
			CRY_PROFILE_FUNCTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, problematicQuery.pQueryBlueprint->GetName());

			va_list ap;
			stack_string commonWarningMessage;
			va_start(ap, szFmt);
			commonWarningMessage.FormatV(szFmt, ap);
			va_end(ap);

			//
			// print the warning to the console
			//

			if (SCvars::printTimeExcessWarningsToConsole == 1)
			{
				Shared::CUqsString queryIdAsString;
				problematicQuery.pQuery->GetQueryID().ToString(queryIdAsString);
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[UQS] QueryID #%s: %s / %s: %s",
					queryIdAsString.c_str(),
					problematicQuery.pQueryBlueprint->GetName(),
					problematicQuery.pQuery->GetQuerierName(),
					commonWarningMessage.c_str());
			}

			//
			// log the warning to the query history
			//

			if (HistoricQuerySharedPtr pHistory = problematicQuery.pQuery->GetHistoricQuery())
			{
				pHistory->GetDebugMessageCollection().AddWarning("%s", commonWarningMessage.c_str());
			}

			//
			// notify all listeners of the warning
			//

			if (!m_queryWarningListeners.empty())
			{
				const Client::IQueryWarningListener::SWarningInfo warningInfo(
					problematicQuery.pQuery->GetQueryID(),
					problematicQuery.priority,
					problematicQuery.pQuery->GetQuerierName(),
					problematicQuery.pQueryBlueprint->GetName(),
					commonWarningMessage.c_str());

				for (Client::IQueryWarningListener* pListener : m_queryWarningListeners)
				{
					pListener->OnQueryWarning(warningInfo);
				}
			}
		}

		void CQueryManager::ExpireDebugDrawStatisticHistory2D()
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			const CTimeValue now = gEnv->pTimer->GetAsyncTime();

			while (!m_debugDrawHistory2D.empty() && m_debugDrawHistory2D.front().finishedTimestamp + s_totalDebugDrawDuration < now)
			{
				m_debugDrawHistory2D.pop_front();
			}
		}

		void CQueryManager::PrintRunningQueriesToConsole(CLogger& logger) const
		{
			logger.Printf("");
			logger.Printf("--- UQS: %i running queries at the moment ---", (int)m_queries.size());
			CLoggerIndentation _indent;

			for (const SRunningQueryInfo& runningQueryInfo : m_queries)
			{
				logger.Printf("");
				DebugPrintQueryStatistics(logger, *runningQueryInfo.pQuery, runningQueryInfo.pQuery->GetQueryID());
				logger.Printf("------------------------");
			}

			logger.Printf("");
		}

		void CQueryManager::CancelAllRunningQueriesDueToUpcomingTearDownOfHub()
		{
			// operate on a copy in case the query's callback cancels queries recursively (happens in hierarchical queries)
			std::list<SRunningQueryInfo> copyOfRunningQueries;
			copyOfRunningQueries.swap(m_queries);
			m_roundRobinStart = m_queries.cend();

			for (const SRunningQueryInfo& runningQueryInfo : copyOfRunningQueries)
			{
				// notify the originator of the query that we're prematurely canceling the query
				if (runningQueryInfo.pCallback)
				{
					QueryResultSetUniquePtr pDummyResultSet;
					const SQueryResult result = SQueryResult::CreateCanceledByHubTearDown(runningQueryInfo.pQuery->GetQueryID(), pDummyResultSet);
					runningQueryInfo.pCallback(result);
				}

				// now cancel it (this might attempt to do some recursive cancelations, but they will effectively end up in CancelQuery() as a NOP since m_queries has already been emptied)
				runningQueryInfo.pQuery->Cancel();
			}
		}

		void CQueryManager::DebugPrintQueryStatistics(CLogger& logger, const CQueryBase& query, const CQueryID& queryID)
		{
			CQueryBase::SStatistics stats;
			query.GetStatistics(stats);

			Shared::CUqsString queryIdAsString;
			queryID.ToString(queryIdAsString);
			logger.Printf("--- UQS query #%s ('%s': '%s') ---", queryIdAsString.c_str(), stats.querierName.c_str(), stats.queryBlueprintName.c_str());

			CLoggerIndentation _indent;

			logger.Printf("consumed frames:            %i", (int)stats.totalConsumedFrames);
			logger.Printf("consumed seconds:           %f (%.2f millisecs)", stats.totalConsumedTime.GetSeconds(), stats.totalConsumedTime.GetSeconds() * 1000.0f);
			logger.Printf("generated items:            %i", (int)stats.numGeneratedItems);
			logger.Printf("remaining items to inspect: %i", (int)stats.numRemainingItemsToInspect);
			logger.Printf("final items:                %i", (int)stats.numItemsInFinalResultSet);
			logger.Printf("items memory:               %i (%i kb)", (int)stats.memoryUsedByGeneratedItems, (int)stats.memoryUsedByGeneratedItems / 1024);
			logger.Printf("items working data memory:  %i (%i kb)", (int)stats.memoryUsedByItemsWorkingData, (int)stats.memoryUsedByItemsWorkingData / 1024);

			// Instant-Evaluators
			{
				const size_t numInstantEvaluators = stats.instantEvaluatorsRuns.size();
				for (size_t i = 0; i < numInstantEvaluators; ++i)
				{
					logger.Printf("Instant-Evaluator #%i:  full runs = %i", (int)i, (int)stats.instantEvaluatorsRuns[i]);
				}
			}

			// Deferred-Evaluators
			{
				CRY_ASSERT(stats.deferredEvaluatorsFullRuns.size() == stats.deferredEvaluatorsAbortedRuns.size());
				const size_t numDeferredEvaluators = stats.deferredEvaluatorsFullRuns.size();
				for (size_t i = 0; i < numDeferredEvaluators; ++i)
				{
					logger.Printf("Deferred-Evaluator #%i: full runs = %i, aborted runs = %i", (int)i, (int)stats.deferredEvaluatorsFullRuns[i], (int)stats.deferredEvaluatorsAbortedRuns[i]);
				}
			}

			// Phases
			{
				const size_t numPhases = stats.elapsedTimePerPhase.size();
				for (size_t i = 0; i < numPhases; ++i)
				{
					// print a human-readable phase index
					logger.Printf("Phase '%i'  = %i frames, %f seconds (%.2f millisecs) [longest call = %f seconds (%.2f millisecs)]",
						(int)i + 1,
						(int)stats.elapsedFramesPerPhase[i],
						stats.elapsedTimePerPhase[i].GetSeconds(),
						stats.elapsedTimePerPhase[i].GetSeconds() * 1000.0f,
						stats.peakElapsedTimePerPhaseUpdate[i].GetSeconds(),
						stats.peakElapsedTimePerPhaseUpdate[i].GetSeconds() * 1000.0f);
				}
			}
		}

		int CQueryManager::DebugDrawQueryStatistics(const CQueryBase::SStatistics& statisticsToDraw, const CQueryID& queryID, int priority, int row, const ColorF& color)
		{
			Shared::CUqsString queryIDAsString;
			queryID.ToString(queryIDAsString);

			CDrawUtil2d::DrawLabel(row, color, "#%s: '%s' / '%s' (prio = %i) (%i/%i) still to inspect: %i",
				queryIDAsString.c_str(),
				statisticsToDraw.querierName.c_str(),
				statisticsToDraw.queryBlueprintName.c_str(),
				priority,
				(int)statisticsToDraw.numItemsInFinalResultSet,
				(int)statisticsToDraw.numGeneratedItems,
				(int)statisticsToDraw.numRemainingItemsToInspect);
			++row;

			return row;
		}

		void CQueryManager::DebugDrawRoundRobinLoad() const
		{
			float x = 20;
			float y = 20;

			const float blockSize = 50;
			const float extraSpaceBetweenBlocks = 2;

			for (const SRoundRobinDebugSnapshot::SElement& element : m_roundRobinDebugSnapshot.elements)
			{
				float oldX = x;

				// fully updated?
				if (element.priorityHandedOut == element.priority && element.priorityRemaining == 0)
				{
					ColorB col(0, 255, 0);	// green
					for (int i = 0; i < element.priorityHandedOut; i++)
					{
						CDrawUtil2d::DrawFilledQuad(x, y, x + blockSize, y + blockSize, col);
						x += blockSize + extraSpaceBetweenBlocks;
					}
				}

				// going to spill over?
				if (element.priorityHandedOut < element.priority && element.priorityRemaining > 0)
				{
					ColorB col1(0, 255, 255);	// cyan
					for (int i = 0; i < element.priorityHandedOut; i++)
					{
						CDrawUtil2d::DrawFilledQuad(x, y, x + blockSize, y + blockSize, col1);
						x += blockSize + extraSpaceBetweenBlocks;
					}

					ColorB col2(255, 105, 180);	// pink
					for (int i = 0; i < element.priorityRemaining; i++)
					{
						CDrawUtil2d::DrawFilledQuad(x, y, x + blockSize, y + blockSize, col2);
						x += blockSize + extraSpaceBetweenBlocks;
					}
				}

				// remainder just spilled over from previous frame?
				if (element.priorityHandedOut < element.priority && element.priorityRemaining == 0)
				{
					ColorB col(0, 255, 255);	// cyan
					for (int i = 0; i < element.priorityHandedOut; i++)
					{
						CDrawUtil2d::DrawFilledQuad(x, y, x + blockSize, y + blockSize, col);
						x += blockSize + extraSpaceBetweenBlocks;
					}
				}

				// query ID
				CDrawUtil2d::DrawLabel(oldX, y, 1.6f, Col_White, "#%s", element.queryIdAsString.c_str());

				// priority
				CDrawUtil2d::DrawLabel(oldX, y + 15, 1.6f, Col_White, "p: %i", element.priorityHandedOut);
			}
		}

	}
}
