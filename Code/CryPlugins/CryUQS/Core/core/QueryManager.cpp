// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryManager.h"
#include <CryRenderer/IRenderer.h>

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
		{}

		//===================================================================================
		//
		// CQueryManager::SFinishedQueryInfo
		//
		//===================================================================================

		CQueryManager::SFinishedQueryInfo::SFinishedQueryInfo(const std::shared_ptr<CQueryBase>& _pQuery, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, const Functor1<const SQueryResult&>& _pCallback, const CQueryID& _queryID, bool _bQueryFinishedWithSuccess, const string& _errorIfAny)
			: pQuery(_pQuery)
			, pQueryBlueprint(_pQueryBlueprint)
			, pCallback(_pCallback)
			, queryID(_queryID)
			, bQueryFinishedWithSuccess(_bQueryFinishedWithSuccess)
			, errorIfAny(_errorIfAny)
		{}

		//===================================================================================
		//
		// CQueryManager::SHistoryQueryInfo2D
		//
		//===================================================================================

		CQueryManager::SHistoryQueryInfo2D::SHistoryQueryInfo2D(const CQueryID &_queryID, const CQueryBase::SStatistics& _statistics, bool _bQueryFinishedWithSuccess, const CTimeValue& _timestamp)
			: queryID(_queryID)
			, statistics(_statistics)
			, bQueryFinishedWithSuccess(_bQueryFinishedWithSuccess)
			, finishedTimestamp(_timestamp)
		{}

		//===================================================================================
		//
		// CQueryManager
		//
		//===================================================================================

		const CTimeValue CQueryManager::s_delayBeforeFadeOut(2.5f);
		const CTimeValue CQueryManager::s_fadeOutDuration(0.5f);
		const CTimeValue CQueryManager::s_totalDebugDrawDuration = CQueryManager::s_delayBeforeFadeOut + CQueryManager::s_fadeOutDuration;

		CQueryManager::CQueryManager(CQueryHistoryManager& queryHistoryManager)
			: m_queryIDProvider(CQueryID::CreateInvalid())
			, m_queryHistoryManager(queryHistoryManager)
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

			static const CQueryID noParentQueryID = CQueryID::CreateInvalid();
			std::shared_ptr<CItemList> pEmptyResultSinceThereIsNoPreviousQuery;
			return StartQueryInternal(noParentQueryID, qbp, request.runtimeParams, request.szQuerierName, request.callback, pEmptyResultSinceThereIsNoPreviousQuery, errorMessage);
		}

		void CQueryManager::CancelQuery(const CQueryID& idOfQueryToCancel)
		{
			auto it = m_queries.find(idOfQueryToCancel);
			if (it != m_queries.end())
			{
				CQueryBase* pQueryToCancel = it->second.pQuery.get();
				pQueryToCancel->Cancel();
				m_queries.erase(it);
			}
		}

		void CQueryManager::AddItemMonitorToQuery(const CQueryID& queryID, Client::ItemMonitorUniquePtr&& pItemMonitorToInstall)
		{
			assert(pItemMonitorToInstall);

			if (CQueryBase* pQuery = FindQueryByQueryID(queryID))
			{
				pQuery->AddItemMonitor(std::move(pItemMonitorToInstall));
			}
		}

		CQueryID CQueryManager::StartQueryInternal(const CQueryID& parentQueryID, std::shared_ptr<const CQueryBlueprint> pQueryBlueprint, const Shared::IVariantDict& runtimeParams, const char* szQuerierName, Functor1<const Core::SQueryResult&> pCallback, const std::shared_ptr<CItemList>& pPotentialResultingItemsFromPreviousQuery, Shared::IUqsString& errorMessage)
		{
			// generate a new query ID (even if the query fails to start)
			const CQueryID id = ++m_queryIDProvider;

			// enable history-logging for this query according to a cvar
			HistoricQuerySharedPtr pOptionalHistoryEntry;
			if (SCvars::logQueryHistory)
			{
				pOptionalHistoryEntry = m_queryHistoryManager.AddNewLiveHistoricQuery(id, szQuerierName, parentQueryID);
			}

			// create a new query instance through the query-blueprint
			const CQueryBase::SCtorContext queryCtorContext(id, szQuerierName, pOptionalHistoryEntry, pPotentialResultingItemsFromPreviousQuery);
			std::unique_ptr<CQueryBase> q = pQueryBlueprint->CreateQuery(queryCtorContext);

			// instantiate that query (cannot be done in the query's ctor as it needs to return success/failure)
			Shared::CUqsString error;
			if (!q->InstantiateFromQueryBlueprint(pQueryBlueprint, runtimeParams, error))
			{
				errorMessage.Format("CQueryManager::StartQueryInternal: %s", error.c_str());
				return CQueryID::CreateInvalid();
			}

			// keep track of and update the new query from now on
			SRunningQueryInfo newEntry;
			newEntry.pQuery = std::move(q);
			newEntry.pQueryBlueprint = pQueryBlueprint;
			newEntry.pCallback = pCallback;
			m_queries[id] = newEntry;

			return id;
		}

		CQueryBase* CQueryManager::FindQueryByQueryID(const CQueryID& queryID)
		{
			auto it = m_queries.find(queryID);
			return (it == m_queries.end()) ? nullptr : it->second.pQuery.get();
		}

		void CQueryManager::Update()
		{
			CRY_PROFILE_FUNCTION(UQS_PROFILED_SUBSYSTEM_TO_USE);

			UpdateQueries();

			ExpireDebugDrawStatisticHistory2D();
		}

		void CQueryManager::DebugDrawRunningQueriesStatistics2D() const
		{
			const CTimeValue now = gEnv->pTimer->GetAsyncTime();
			int row = 1;

			//
			// number of currently running queries
			//

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
				row = DebugDrawQueryStatistics(historyEntry.statistics, historyEntry.queryID, row, color);
				++row;
			}

			//
			// draw statistics of all ongoing queries in white
			//

			for (const auto& pair : m_queries)
			{
				const CQueryID& queryID = pair.first;
				const SRunningQueryInfo& runningInfo = pair.second;
				CQueryBase::SStatistics stats;
				runningInfo.pQuery->GetStatistics(stats);
				row = DebugDrawQueryStatistics(stats, queryID, row, Col_White);
				++row;
			}
		}

		void CQueryManager::UpdateQueries()
		{
			if (!m_queries.empty())
			{
				//
				// update all queries and collect the finished ones
				//

				std::vector<SFinishedQueryInfo> finishedOnes;
				CTimeValue totalRemainingTimeBudget = SCvars::timeBudgetInSeconds;

				// TODO: prematurely bail out if exceeding the time budget before all queries were updated (should continue from there the next time)
				for (auto it = m_queries.cbegin(); it != m_queries.cend(); ++it)
				{
					//
					// Compute the granted time budget for every query again, since the previous query might have donated some unused time to the whole pool and
					// more queries might have been spawned also.
					// => let this query benefit a bit from that extra time
					//
					// As a further optimization to distribute the overall time budget as good as possible, we take a look at the current and remaining queries and ask them
					// for whether they potentially require some time budget or not. If they don't need a time budget, then their unused time budget gets prematurely donated
					// to the remaining queries (this happens implicitly).
					//

					CQueryBase* pQuery = it->second.pQuery.get();
					CTimeValue timeBudgetForThisQuery;   // 0.0 seconds by default

					const bool bThisQueryRequiresSomeTimeBudgetForExecution = pQuery->RequiresSomeTimeBudgetForExecution();

					if (bThisQueryRequiresSomeTimeBudgetForExecution)
					{
						size_t numRemainingQueriesThatRequireSomeTimeBudget = 1;

						// see how many queries require a time budget
						auto it2 = it;
						++it2;
						for (; it2 != m_queries.cend(); ++it2)
						{
							const CQueryBase* pQuery2 = it2->second.pQuery.get();
							if (pQuery2->RequiresSomeTimeBudgetForExecution())
							{
								++numRemainingQueriesThatRequireSomeTimeBudget;
							}
						}

						timeBudgetForThisQuery = totalRemainingTimeBudget.GetSeconds() / (float)numRemainingQueriesThatRequireSomeTimeBudget;
					}

					totalRemainingTimeBudget -= timeBudgetForThisQuery;

					//
					// - now update the query and deal with its update status
					// - keep track of the used time for donating unused time to the whole pool (so that the upcoming queries can benefit from it)
					//

					Shared::CUqsString error;
					const CTimeValue timestampBeforeQueryUpdate = gEnv->pTimer->GetAsyncTime();
					const CQueryBase::EUpdateState queryState = pQuery->Update(timeBudgetForThisQuery, error);
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
							const std::shared_ptr<CQueryBase>& pQuery = it->second.pQuery;
							const std::shared_ptr<const CQueryBlueprint>& pQueryBlueprint = it->second.pQueryBlueprint;
							const Functor1<const SQueryResult&>& pCallback = it->second.pCallback;
							const CQueryID& queryID = it->first;
							const bool bQueryFinishedWithSuccess = true;
							finishedOnes.emplace_back(pQuery, pQueryBlueprint, pCallback, queryID, bQueryFinishedWithSuccess, "");
						}
						break;

					case CQueryBase::EUpdateState::ExceptionOccurred:
						{
							const std::shared_ptr<CQueryBase>& pQuery = it->second.pQuery;
							const std::shared_ptr<const CQueryBlueprint>& pQueryBlueprint = it->second.pQueryBlueprint;
							const Functor1<const SQueryResult&>& pCallback = it->second.pCallback;
							const CQueryID& queryID = it->first;
							const bool bQueryFinishedWithSuccess = false;
							finishedOnes.emplace_back(pQuery, pQueryBlueprint, pCallback, queryID, bQueryFinishedWithSuccess, error.c_str());
						}
						break;

					default:
						assert(0);
					}

					//
					// check for some unused time and donate it to the remaining queries
					//

					const CTimeValue timeUsedByThisQuery = (timestampAfterQueryUpdate - timestampBeforeQueryUpdate);

					if (timeUsedByThisQuery < timeBudgetForThisQuery)
					{
						const CTimeValue unusedTime = timeBudgetForThisQuery - timeUsedByThisQuery;
						totalRemainingTimeBudget += unusedTime;
					}
					else if (bThisQueryRequiresSomeTimeBudgetForExecution)
					{
						//
						// check for having exceeded the granted time by some percentage
						// -> if this is the case, then issue a warning to the console and to the query history
						//

						const float allowedTimeBudgetExcess = (SCvars::timeBudgetExcessThresholdInPercentBeforeWarning * 0.01f) * timeBudgetForThisQuery.GetMilliSeconds();
						const bool bExceededTimeBudgetTooMuch = (timeUsedByThisQuery - timeBudgetForThisQuery).GetMilliSeconds() > allowedTimeBudgetExcess;

						if (bExceededTimeBudgetTooMuch)
						{
							it->second.pQuery->EmitTimeExcessWarningToConsoleAndQueryHistory(timeBudgetForThisQuery, timeUsedByThisQuery);
						}
					}
				}

				//
				// if some queries have finished, then notify the listeners of the result and store their statistic for short-term 2D on-screen debug rendering
				//

				if (!finishedOnes.empty())
				{
					// first, notify all listeners that these queries have finished
					for (const SFinishedQueryInfo& entry : finishedOnes)
					{
						if (entry.pCallback)
						{
							NotifyCallbackOfFinishedQuery(entry);
						}

						// add a new entry to the debug history for 2D on-screen rendering
						if (SCvars::debugDraw)
						{
							CQueryBase::SStatistics stats;
							entry.pQuery->GetStatistics(stats);
							SHistoryQueryInfo2D newHistoryEntry(entry.queryID, stats, entry.bQueryFinishedWithSuccess, gEnv->pTimer->GetAsyncTime());
							m_debugDrawHistory2D.push_back(std::move(newHistoryEntry));
						}
					}

					// now remove all the finished queries
					for (const SFinishedQueryInfo& entry : finishedOnes)
					{
						auto it = m_queries.find(entry.queryID);
						if (it != m_queries.end())   // this may fail if a finished query got explicitly canceled in the callback from above
						{
							m_queries.erase(it);
						}
					}
				}
			}
		}

		void CQueryManager::NotifyCallbackOfFinishedQuery(const SFinishedQueryInfo& finishedQueryInfo)
		{
			CRY_PROFILE_FUNCTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, finishedQueryInfo.pQueryBlueprint->GetName());

			if (finishedQueryInfo.bQueryFinishedWithSuccess)
			{
				QueryResultSetUniquePtr pResultSet = finishedQueryInfo.pQuery->ClaimResultSet();
				const SQueryResult result = SQueryResult::CreateSuccess(finishedQueryInfo.queryID, pResultSet);
				finishedQueryInfo.pCallback(result);
			}
			else
			{
				QueryResultSetUniquePtr pResultSetDummy;
				const SQueryResult result = SQueryResult::CreateError(finishedQueryInfo.queryID, pResultSetDummy, finishedQueryInfo.errorIfAny.c_str());
				finishedQueryInfo.pCallback(result);
			}
		}

		void CQueryManager::ExpireDebugDrawStatisticHistory2D()
		{
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

			for (const auto& pair : m_queries)
			{
				const CQueryID& queryID = pair.first;
				const CQueryBase& query = *pair.second.pQuery;

				logger.Printf("");
				DebugPrintQueryStatistics(logger, query, queryID);
				logger.Printf("------------------------");
			}

			logger.Printf("");
		}

		void CQueryManager::CancelAllRunningQueriesDueToUpcomingTearDownOfHub()
		{
			// operate on a copy in case the query's callback cancels queries recursively (happens in hierarchical queries)
			std::map<CQueryID, SRunningQueryInfo> copyOfRunningQueries;
			copyOfRunningQueries.swap(m_queries);

			for (auto it = copyOfRunningQueries.begin(); it != copyOfRunningQueries.end(); ++it)
			{
				const SRunningQueryInfo& runningQueryInfo = it->second;

				// notify the originator of the query that we're prematurely canceling the query
				if (runningQueryInfo.pCallback)
				{
					const CQueryID& queryID = it->first;
					QueryResultSetUniquePtr pDummyResultSet;
					const SQueryResult result = SQueryResult::CreateCanceledByHubTearDown(queryID, pDummyResultSet);
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

			logger.Printf("elapsed frames:             %i", (int)stats.totalElapsedFrames);
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
				assert(stats.deferredEvaluatorsFullRuns.size() == stats.deferredEvaluatorsAbortedRuns.size());
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

		int CQueryManager::DebugDrawQueryStatistics(const CQueryBase::SStatistics& statisticsToDraw, const CQueryID& queryID, int row, const ColorF& color)
		{
			Shared::CUqsString queryIDAsString;
			queryID.ToString(queryIDAsString);

			CDrawUtil2d::DrawLabel(row, color, "#%s: '%s' / '%s' (%i/%i) still to inspect: %i",
				queryIDAsString.c_str(),
				statisticsToDraw.querierName.c_str(),
				statisticsToDraw.queryBlueprintName.c_str(),
				(int)statisticsToDraw.numItemsInFinalResultSet,
				(int)statisticsToDraw.numGeneratedItems,
				(int)statisticsToDraw.numRemainingItemsToInspect);
			++row;

			return row;
		}

	}
}
