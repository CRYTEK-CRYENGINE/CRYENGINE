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
			, queryID(CQueryID::CreateInvalid())
			, parentQueryID(CQueryID::CreateInvalid())
			, bPerformanceOffender(false)
		{}

		//===================================================================================
		//
		// CQueryManager::SFinishedQueryInfo
		//
		//===================================================================================

		CQueryManager::SFinishedQueryInfo::SFinishedQueryInfo(const std::shared_ptr<CQueryBase>& _pQuery, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, const Functor1<const SQueryResult&>& _pCallback, const CQueryID& _queryID, const CQueryID& _parentQueryID, const CTimeValue& _queryFinishedTimestamp, bool _bQueryFinishedWithSuccess, const string& _errorIfAny)
			: pQuery(_pQuery)
			, pQueryBlueprint(_pQueryBlueprint)
			, pCallback(_pCallback)
			, queryID(_queryID)
			, parentQueryID(_parentQueryID)
			, queryFinishedTimestamp(_queryFinishedTimestamp)
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
		// CQueryManager::CPredEqualQueryID
		//
		//===================================================================================

		CQueryManager::CPredEqualQueryID::CPredEqualQueryID(const CQueryID& queryID)
			: m_queryID(queryID)
		{}

		bool CQueryManager::CPredEqualQueryID::operator()(const SRunningQueryInfo& runningQueryInfo) const
		{
			return m_queryID == runningQueryInfo.queryID;
		}

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
			, m_bQueriesUpdateInProgress(false)
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
			assert(!m_bQueriesUpdateInProgress);

			auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(idOfQueryToCancel));
			if (it != m_queries.end())
			{
				CQueryBase* pQueryToCancel = it->pQuery.get();
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
					runningQueryInfo.queryID,
					runningQueryInfo.parentQueryID,
					stats.querierName.c_str(),
					stats.queryBlueprintName.c_str(),
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

		CQueryID CQueryManager::StartQueryInternal(const CQueryID& parentQueryID, std::shared_ptr<const CQueryBlueprint> pQueryBlueprint, const Shared::IVariantDict& runtimeParams, const char* szQuerierName, Functor1<const SQueryResult&> pCallback, const std::shared_ptr<CItemList>& pPotentialResultingItemsFromPreviousQuery, Shared::IUqsString& errorMessage)
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
				SFinishedQueryInfo finishedQueryInfo(
					std::move(q),		// unique_ptr -> shared_ptr conversion
					pQueryBlueprint,
					nullptr,
					id,
					parentQueryID,
					gEnv->pTimer->GetAsyncTime(),
					false,
					error.c_str());
				NotifyCallbacksOfFinishedQuery(finishedQueryInfo);
				errorMessage.Format("CQueryManager::StartQueryInternal: %s", error.c_str());
				return CQueryID::CreateInvalid();
			}

			// keep track of and update the new query from now on
			SRunningQueryInfo newEntry;
			newEntry.pQuery = std::move(q);
			newEntry.pQueryBlueprint = pQueryBlueprint;
			newEntry.pCallback = pCallback;
			newEntry.queryID = id;
			newEntry.parentQueryID = parentQueryID;
			m_queries.emplace_back(std::move(newEntry));

			return id;
		}

		CQueryBase* CQueryManager::FindQueryByQueryID(const CQueryID& queryID)
		{
			auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(queryID));
			return (it == m_queries.end()) ? nullptr : it->pQuery.get();
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

			for (const SRunningQueryInfo& runningQueryInfo : m_queries)
			{
				CQueryBase::SStatistics stats;
				runningQueryInfo.pQuery->GetStatistics(stats);
				row = DebugDrawQueryStatistics(stats, runningQueryInfo.queryID, row, Col_White);
				++row;
			}
		}

		void CQueryManager::UpdateQueries()
		{
			struct SWorstPerformingQuery
			{
				explicit SWorstPerformingQuery(const std::list<SRunningQueryInfo>::iterator& it)
					: itInQueries(it)
				{}

				std::list<SRunningQueryInfo>::iterator itInQueries;   // points into m_queries
				float usedFractionOfTimeBudget = 0.0f;                // fraction of the time budget that was used in the last update cycle of .itInQueries
			};

			if (!m_queries.empty())
			{
				m_bQueriesUpdateInProgress = true;	// detect unintended calls to CancelQuery() while we're iterating through m_queries

				//
				// update all queries and collect the finished ones
				//

				const CTimeValue totalTimeBudget = SCvars::timeBudgetInSeconds;

				std::vector<SFinishedQueryInfo> finishedOnes;
				CTimeValue totalRemainingTimeBudget = totalTimeBudget;
				CTimeValue totalTimeUsedSoFar;

				SWorstPerformingQuery worstPerformingQuery(m_queries.end());
				bool bEncounteredSomeNonPerformanceOffenderByNow = false;

				for (auto it = m_queries.begin(); it != m_queries.end(); ++it)
				{
					const SRunningQueryInfo& runningQueryInfo = *it;

					CQueryBase* pQuery = runningQueryInfo.pQuery.get();

					const bool bThisQueryRequiresSomeTimeBudgetForExecution = pQuery->RequiresSomeTimeBudgetForExecution();

					//
					// - see if we need to skip this query due to punishment of having exceeded the time budget a lot and having interrupted the whole process before
					// - the idea is to allow running all queries that *precede* the performance offender until they're finished and to halt the performance offender in the meanwhile
					// - this is to make sure that the performance offender does no longer block these preceding queries by continuously interrupting everything
					//

					if (runningQueryInfo.bPerformanceOffender)
					{
						if (bEncounteredSomeNonPerformanceOffenderByNow)
						{
							NotifyOfQueryPerformanceWarning(runningQueryInfo, "system frame #%i: skipping query due to being a performance offender", (int)gEnv->nMainFrameID);
							continue;
						}
					}
					else if (bThisQueryRequiresSomeTimeBudgetForExecution)	// don't get infinitely blocked by parent queries that wait for their children (i.e. that wait for exactly this query we're now at)
					{
						bEncounteredSomeNonPerformanceOffenderByNow = true;
					}

					//
					// Compute the granted time budget for every query again, since the previous query might have donated some unused time to the whole pool and
					// more queries might have been spawned also.
					// => let this query benefit a bit from that extra time
					//
					// As a further optimization to distribute the overall time budget as good as possible, we take a look at the current and remaining queries and ask them
					// for whether they potentially require some time budget or not. If they don't need a time budget, then their unused time budget gets prematurely donated
					// to the remaining queries (this happens implicitly).
					//

					CTimeValue timeBudgetForThisQuery;   // 0.0 seconds by default

					if (bThisQueryRequiresSomeTimeBudgetForExecution)
					{
						size_t numRemainingQueriesThatRequireSomeTimeBudget = 1;

						// see how many queries require a time budget
						auto it2 = it;
						++it2;
						for (; it2 != m_queries.cend(); ++it2)
						{
							const CQueryBase* pQuery2 = it2->pQuery.get();
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
							const bool bQueryFinishedWithSuccess = true;
							finishedOnes.emplace_back(
								runningQueryInfo.pQuery,
								runningQueryInfo.pQueryBlueprint,
								runningQueryInfo.pCallback,
								runningQueryInfo.queryID,
								runningQueryInfo.parentQueryID,
								timestampAfterQueryUpdate,
								bQueryFinishedWithSuccess, "");
						}
						break;

					case CQueryBase::EUpdateState::ExceptionOccurred:
						{
							const bool bQueryFinishedWithSuccess = false;
							finishedOnes.emplace_back(
								runningQueryInfo.pQuery,
								runningQueryInfo.pQueryBlueprint,
								runningQueryInfo.pCallback,
								runningQueryInfo.queryID,
								runningQueryInfo.parentQueryID,
								timestampAfterQueryUpdate,
								bQueryFinishedWithSuccess,
								error.c_str());
						}
						break;

					default:
						assert(0);
					}

					//
					// check for some unused time and donate it to the remaining queries
					//

					const CTimeValue timeUsedByThisQuery = (timestampAfterQueryUpdate - timestampBeforeQueryUpdate);

					if (timeUsedByThisQuery <= timeBudgetForThisQuery)
					{
						const CTimeValue unusedTime = timeBudgetForThisQuery - timeUsedByThisQuery;
						totalRemainingTimeBudget += unusedTime;
					}
					else if (bThisQueryRequiresSomeTimeBudgetForExecution)
					{
						//
						// keep track of the potentially worst performing query (this one may get skipped in upcoming updates, depending on whether the overall time budget will get exceeded by the extra threshold)
						//

						const float usedFractionOfTimeBudget = (timeBudgetForThisQuery.GetValue() > 0) ? (timeUsedByThisQuery.GetMilliSeconds() / timeBudgetForThisQuery.GetMilliSeconds()) : 1.0f;

						if (worstPerformingQuery.itInQueries == m_queries.end() || worstPerformingQuery.usedFractionOfTimeBudget < usedFractionOfTimeBudget)
						{
							worstPerformingQuery.itInQueries = it;
							worstPerformingQuery.usedFractionOfTimeBudget = usedFractionOfTimeBudget;
						}

						//
						// check for overall excess
						//

						totalTimeUsedSoFar += timeUsedByThisQuery;

						if (totalTimeUsedSoFar > totalTimeBudget)
						{
							//
							// see how much has been exceeded already, and if it's too much, then simply interrupt everything and mark the worst performing query as "performance offender" so that
							// it will get skipped in subsequent updates until all its preceding queries are finished
							//

							const float overallFractionUsedSoFar = (totalTimeBudget.GetValue() > 0) ? (totalTimeUsedSoFar.GetMilliSeconds() / totalTimeBudget.GetMilliSeconds()) : 1.0f;

							if (overallFractionUsedSoFar > 1.0f + SCvars::timeBudgetExcessThresholdInPercent * 0.01f)
							{
								assert(worstPerformingQuery.itInQueries != m_queries.end());

								NotifyOfQueryPerformanceWarning(*worstPerformingQuery.itInQueries, "system frame #%i: query has just been flagged as a performance offender (consumed %.1f%% of its granted time: %fms vs %fms)", (int)gEnv->nMainFrameID, worstPerformingQuery.usedFractionOfTimeBudget * 100.0f, timeUsedByThisQuery.GetMilliSeconds(), timeBudgetForThisQuery.GetMilliSeconds());

								// move the worst performing query to the end of the queue such that preceding queries won't get offended anymore
								SRunningQueryInfo worstOne = std::move(*worstPerformingQuery.itInQueries);
								worstOne.bPerformanceOffender = true;
								m_queries.erase(worstPerformingQuery.itInQueries);
								m_queries.push_back(std::move(worstOne));

								// prematurely interrupt processing of the remaining queries
								break;
							}
						}
					}
				}

				m_bQueriesUpdateInProgress = false;

				//
				// if some queries have finished, then notify the listeners of the result and store their statistic for short-term 2D on-screen debug rendering
				//

				if (!finishedOnes.empty())
				{
					// first, notify all listeners that these queries have finished
					for (const SFinishedQueryInfo& entry : finishedOnes)
					{
						NotifyCallbacksOfFinishedQuery(entry);

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
						auto it = std::find_if(m_queries.begin(), m_queries.end(), CPredEqualQueryID(entry.queryID));
						if (it != m_queries.end())   // this may fail if a finished query got explicitly canceled in the callback from above
						{
							m_queries.erase(it);
						}
					}
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

			//
			// call all global callbacks (that are interested in *all* finished queries)
			//

			if (!m_queryFinishedListeners.empty())
			{
				CQueryBase::SStatistics stats;
				finishedQueryInfo.pQuery->GetStatistics(stats);

				const Client::IQueryFinishedListener::SQueryInfo queryInfo(
					finishedQueryInfo.queryID,
					finishedQueryInfo.parentQueryID,
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
				problematicQuery.queryID.ToString(queryIdAsString);
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
					problematicQuery.queryID,
					problematicQuery.parentQueryID,
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
				DebugPrintQueryStatistics(logger, *runningQueryInfo.pQuery, runningQueryInfo.queryID);
				logger.Printf("------------------------");
			}

			logger.Printf("");
		}

		void CQueryManager::CancelAllRunningQueriesDueToUpcomingTearDownOfHub()
		{
			// operate on a copy in case the query's callback cancels queries recursively (happens in hierarchical queries)
			std::list<SRunningQueryInfo> copyOfRunningQueries;
			copyOfRunningQueries.swap(m_queries);

			for (const SRunningQueryInfo& runningQueryInfo : copyOfRunningQueries)
			{
				// notify the originator of the query that we're prematurely canceling the query
				if (runningQueryInfo.pCallback)
				{
					QueryResultSetUniquePtr pDummyResultSet;
					const SQueryResult result = SQueryResult::CreateCanceledByHubTearDown(runningQueryInfo.queryID, pDummyResultSet);
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
