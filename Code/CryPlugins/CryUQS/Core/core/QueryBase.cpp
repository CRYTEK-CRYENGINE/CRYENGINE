// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// make the global Serialize() functions available for use in yasli serialization
using UQS::Core::Serialize;

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryBase::SCtorContext
		//
		//===================================================================================

		CQueryBase::SCtorContext::SCtorContext(const CQueryID& _queryID, const char* _szQuerierName, const HistoricQuerySharedPtr& _pOptionalHistoryToWriteTo, const std::shared_ptr<CItemList>& _pOptionalResultingItemsFromPreviousQuery)
			: queryID(_queryID)
			, szQuerierName(_szQuerierName)
			, pOptionalHistoryToWriteTo(_pOptionalHistoryToWriteTo)
			, pOptionalResultingItemsFromPreviousQuery(_pOptionalResultingItemsFromPreviousQuery)
		{}

		//===================================================================================
		//
		// CQueryBase::SGrantedAndUsedTime
		//
		//===================================================================================

		CQueryBase::SGrantedAndUsedTime::SGrantedAndUsedTime()
		{}

		CQueryBase::SGrantedAndUsedTime::SGrantedAndUsedTime(const CTimeValue& _granted, const CTimeValue& _used)
			: granted(_granted)
			, used(_used)
		{}

		void CQueryBase::SGrantedAndUsedTime::Serialize(Serialization::IArchive& ar)
		{
			ar(granted, "granted");
			ar(used, "used");
		}

		//===================================================================================
		//
		// CQueryBase::SStatistics
		//
		//===================================================================================

		CQueryBase::SStatistics::SStatistics()
			: querierName()
			, queryBlueprintName()
			, totalElapsedFrames(0)
			, totalConsumedTime()
			, grantedAndUsedTimePerFrame()

			, numDesiredItems(0)
			, numGeneratedItems(0)
			, numRemainingItemsToInspect(0)
			, numItemsInFinalResultSet(0)
			, memoryUsedByGeneratedItems(0)
			, memoryUsedByItemsWorkingData(0)

			, elapsedFramesPerPhase()
			, elapsedTimePerPhase()
			, peakElapsedTimePerPhaseUpdate()
			, instantEvaluatorsRuns()
			, deferredEvaluatorsFullRuns()
			, deferredEvaluatorsAbortedRuns()
		{}

		void CQueryBase::SStatistics::Serialize(Serialization::IArchive& ar)
		{
			ar(querierName, "querierName");
			ar(queryBlueprintName, "queryBlueprintName");
			ar(totalElapsedFrames, "totalElapsedFrames");
			ar(totalConsumedTime, "totalConsumedTime");
			ar(grantedAndUsedTimePerFrame, "grantedAndUsedTimePerFrame");

			ar(numDesiredItems, "numDesiredItems");
			ar(numGeneratedItems, "numGeneratedItems");
			ar(numRemainingItemsToInspect, "numRemainingItemsToInspect");
			ar(numItemsInFinalResultSet, "numItemsInFinalResultSet");
			ar(memoryUsedByGeneratedItems, "memoryUsedByGeneratedItems");
			ar(memoryUsedByItemsWorkingData, "memoryUsedByItemsWorkingData");

			ar(elapsedFramesPerPhase, "elapsedFramesPerPhase");
			ar(elapsedTimePerPhase, "elapsedTimePerPhase");
			ar(peakElapsedTimePerPhaseUpdate, "peakElapsedTimePerPhaseUpdate");
			ar(instantEvaluatorsRuns, "instantEvaluatorsRuns");
			ar(deferredEvaluatorsFullRuns, "deferredEvaluatorsFullRuns");
			ar(deferredEvaluatorsAbortedRuns, "deferredEvaluatorsAbortedRuns");
		}

		//===================================================================================
		//
		// CQueryBase
		//
		//===================================================================================

		const CDebugRenderWorldImmediate CQueryBase::s_debugRenderWorldImmediate;

		CQueryBase::CQueryBase(const SCtorContext& ctorContext, bool bRequiresSomeTimeBudgetForExecution)
			: m_querierName(ctorContext.szQuerierName)
			, m_pHistory(ctorContext.pOptionalHistoryToWriteTo)
			, m_queryID(ctorContext.queryID)
			, m_pOptionalShuttledItems(ctorContext.pOptionalResultingItemsFromPreviousQuery)
			, m_totalElapsedFrames(0)
			, m_bRequiresSomeTimeBudgetForExecution(bRequiresSomeTimeBudgetForExecution)
			, m_blackboard(m_globalParams, m_pOptionalShuttledItems.get(), m_timeBudgetForCurrentUpdate, ctorContext.pOptionalHistoryToWriteTo ? &ctorContext.pOptionalHistoryToWriteTo->GetDebugRenderWorldPersistent() : nullptr)
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryCreated();
			}
		}

		CQueryBase::~CQueryBase()
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryDestroyed();
			}
		}

		bool CQueryBase::RequiresSomeTimeBudgetForExecution() const
		{
			return m_bRequiresSomeTimeBudgetForExecution;
		}

		bool CQueryBase::InstantiateFromQueryBlueprint(const std::shared_ptr<const CQueryBlueprint>& pQueryBlueprint, const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error)
		{
			assert(!m_pQueryBlueprint);	// we don't support recycling the query

			m_pQueryBlueprint = pQueryBlueprint;

			if (m_pHistory)
			{
				m_pHistory->OnQueryBlueprintInstantiationStarted(pQueryBlueprint->GetName());
			}

			//
			// ensure that the max. number of instant- and deferred-evaluators is not exceeded
			//

			{
				const size_t numInstantEvaluators = m_pQueryBlueprint->GetInstantEvaluatorBlueprints().size();
				if (numInstantEvaluators > UQS_MAX_EVALUATORS)
				{
					error.Format("Exceeded the maximum number of instant-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numInstantEvaluators);
					if (m_pHistory)
					{
						SStatistics stats;
						GetStatistics(stats);
						m_pHistory->OnExceptionOccurred(error.c_str(), stats);
					}
					return false;
				}
			}

			{
				const size_t numDeferredEvaluators = m_pQueryBlueprint->GetDeferredEvaluatorBlueprints().size();
				if (numDeferredEvaluators > UQS_MAX_EVALUATORS)
				{
					error.Format("Exceeded the maximum number of deferred-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numDeferredEvaluators);
					if (m_pHistory)
					{
						SStatistics stats;
						GetStatistics(stats);
						m_pHistory->OnExceptionOccurred(error.c_str(), stats);
					}
					return false;
				}
			}

			//
			// - ensure that all required runtime-params have been passed in and that their data types are correct
			// - note: we need to do this only for top-level queries (child queries will get recursively checked when their parent is about to start)
			//

			if (!m_pQueryBlueprint->GetParent())
			{
				if (!m_pQueryBlueprint->CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(runtimeParams, error))
				{
					if (m_pHistory)
					{
						SStatistics stats;
						GetStatistics(stats);
						m_pHistory->OnExceptionOccurred(error.c_str(), stats);
					}
					return false;
				}
			}

			//
			// merge constant-params and runtime-params into global params
			//

			const CGlobalConstantParamsBlueprint& constantParamsBlueprint = m_pQueryBlueprint->GetGlobalConstantParamsBlueprint();
			constantParamsBlueprint.AddSelfToDictAndReplace(m_globalParams); // TODO: don't duplicate the already present constant parameters, but then again, we need to both, constant- and runtime-params, to reside in the m_globalParams container
			runtimeParams.AddSelfToOtherAndReplace(m_globalParams);

			//
			// debug-draw all global parameters that want to be shown
			//

			AddItemsFromGlobalParametersToDebugRenderWorld();

			//
			// allow the derived class to do further custom instantiation
			//

			const bool bFurtherInstantiationInDerivedClassSucceeded = OnInstantiateFromQueryBlueprint(runtimeParams, error);

			if (!bFurtherInstantiationInDerivedClassSucceeded && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnExceptionOccurred(error.c_str(), stats);
			}

			return bFurtherInstantiationInDerivedClassSucceeded;

		}

		void CQueryBase::AddItemMonitor(Client::ItemMonitorUniquePtr&& pItemMonitor)
		{
			assert(pItemMonitor);
			m_itemMonitors.push_back(std::move(pItemMonitor));
		}

		void CQueryBase::TransferAllItemMonitorsToOtherQuery(CQueryBase& receiver)
		{
			if (!m_itemMonitors.empty())
			{
				for (Client::ItemMonitorUniquePtr& pItemMonitor : m_itemMonitors)
				{
					receiver.m_itemMonitors.push_back(std::move(pItemMonitor));
				}
				m_itemMonitors.clear();
			}
		}

		CQueryBase::EUpdateState CQueryBase::Update(const CTimeValue& amountOfGrantedTime, Shared::CUqsString& error)
		{
			CRY_PROFILE_FUNCTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, m_pQueryBlueprint->GetName());

			m_timeBudgetForCurrentUpdate.Restart(amountOfGrantedTime);

			const CTimeValue startTime = gEnv->pTimer->GetAsyncTime();

			++m_totalElapsedFrames;

			// immediate debug-rendering ON/OFF
			if (SCvars::debugDraw)
			{
				m_blackboard.pDebugRenderWorldImmediate = &s_debugRenderWorldImmediate;
			}
			else
			{
				m_blackboard.pDebugRenderWorldImmediate = nullptr;
			}

			bool bCorruptionOccurred = false;

			//
			// check the item-monitors for having encountered a corruption *before* updating the query
			//

			if (!m_itemMonitors.empty())
			{
				for (const Client::ItemMonitorUniquePtr& pItemMonitor : m_itemMonitors)
				{
					assert(pItemMonitor);

					const Client::IItemMonitor::EHealthState healthState = pItemMonitor->UpdateAndCheckForCorruption(error);

					if (healthState == Client::IItemMonitor::EHealthState::CorruptionOccurred)
					{
						bCorruptionOccurred = true;
						break;
					}
				}
			}

			//
			// allow the derived class to update itself if no item corruption has occurred yet
			//

			const EUpdateState state = bCorruptionOccurred ? EUpdateState::ExceptionOccurred : OnUpdate(error);

			//
			// finish timings
			//

			const CTimeValue timeSpent = gEnv->pTimer->GetAsyncTime() - startTime;
			m_totalConsumedTime += timeSpent;
			m_grantedAndUsedTimePerFrame.emplace_back(amountOfGrantedTime, timeSpent);

			//
			// track a possible exception
			//

			if (state == EUpdateState::ExceptionOccurred && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnExceptionOccurred(error.c_str(), stats);
			}

			//
			// track when the query finishes
			//

			if (state == EUpdateState::Finished && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnQueryFinished(stats);
			}

			return state;
		}

		void CQueryBase::Cancel()
		{
			if (m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnQueryCanceled(stats);
			}

			//
			// allow the derived class to do some custom cancellation
			//

			OnCancel();
		}

		void CQueryBase::GetStatistics(SStatistics& out) const
		{
			out.querierName = m_querierName;

			if (m_pQueryBlueprint)
				out.queryBlueprintName = m_pQueryBlueprint->GetName();

			out.totalElapsedFrames = m_totalElapsedFrames;
			out.totalConsumedTime = m_totalConsumedTime;
			out.grantedAndUsedTimePerFrame = m_grantedAndUsedTimePerFrame;

			//
			// allow the derived class to add some more statistics
			//

			OnGetStatistics(out);
		}

		void CQueryBase::EmitTimeExcessWarningToConsoleAndQueryHistory(const CTimeValue& timeGranted, const CTimeValue& timeUsed) const
		{
			stack_string commonWarningMessage;
			commonWarningMessage.Format("Exceeded time-budget in current frame: granted time = %f ms, actually consumed = %f ms", timeGranted.GetMilliSeconds(), timeUsed.GetMilliSeconds());

			// print a warning to the console
			if(SCvars::printTimeExcessWarningsToConsole == 1)
			{
				Shared::CUqsString queryIdAsString;
				m_queryID.ToString(queryIdAsString);
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[UQS] QueryID #%s: %s / %s: %s",
					queryIdAsString.c_str(),
					m_pQueryBlueprint->GetName(),
					m_querierName.c_str(),
					commonWarningMessage.c_str());
			}

			// log the warning to the query history
			{
				if (m_pHistory)
				{
					m_pHistory->OnWarningOccurred(commonWarningMessage.c_str());
				}
			}
		}

		QueryResultSetUniquePtr CQueryBase::ClaimResultSet()
		{
			return std::move(m_pResultSet);
		}

		void CQueryBase::AddItemsFromGlobalParametersToDebugRenderWorld() const
		{
			if (m_pHistory)
			{
				CDebugRenderWorldPersistent& debugRW = m_pHistory->GetDebugRenderWorldPersistent();
				const std::map<string, Shared::CVariantDict::SDataEntry>& globalParamsAsMap = m_globalParams.GetEntries();

				//
				// add all items from the global constant-parameters to the debug-render-world that want to be shown
				//

				{
					const std::map<string, CGlobalConstantParamsBlueprint::SParamInfo>& constantParamsBlueprint = m_pQueryBlueprint->GetGlobalConstantParamsBlueprint().GetParams();

					for (const auto& pair : constantParamsBlueprint)
					{
						if (pair.second.bAddToDebugRenderWorld)
						{
							const string& paramName = pair.first;
							auto it = globalParamsAsMap.find(paramName);
							assert(it != globalParamsAsMap.cend());

							const Shared::CVariantDict::SDataEntry& entry = it->second;
							entry.pItemFactory->AddItemToDebugRenderWorld(entry.pObject, debugRW);
						}
					}
				}

				//
				// add all items from the global runtime-parameters to the debug-render-world that want to be shown
				//

				{
					const std::map<string, CGlobalRuntimeParamsBlueprint::SParamInfo>& runtimeParamsBlueprint = m_pQueryBlueprint->GetGlobalRuntimeParamsBlueprint().GetParams();

					for (const auto& pair : runtimeParamsBlueprint)
					{
						if (pair.second.bAddToDebugRenderWorld)
						{
							const string& paramName = pair.first;
							auto it = globalParamsAsMap.find(paramName);
							assert(it != globalParamsAsMap.cend());

							const Shared::CVariantDict::SDataEntry& entry = it->second;
							entry.pItemFactory->AddItemToDebugRenderWorld(entry.pObject, debugRW);
						}
					}
				}

			}
		}

	}
}
