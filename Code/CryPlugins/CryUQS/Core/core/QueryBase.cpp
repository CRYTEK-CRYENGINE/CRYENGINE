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

		CQueryBase::SCtorContext::SCtorContext(const CQueryID& _queryID, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, int _priority, const char* _szQuerierName, const HistoricQuerySharedPtr& _pOptionalHistoryToWriteTo, const std::shared_ptr<CItemList>& _pOptionalResultingItemsFromPreviousQuery)
			: queryID(_queryID)
			, pQueryBlueprint(_pQueryBlueprint)
			, priority(_priority)
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
			, queryCreatedFrame(0)
			, queryCreatedTimestamp()
			, totalConsumedFrames(0)
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
			ar(queryCreatedFrame, "queryCreatedFrame");
			ar(queryCreatedTimestamp, "queryCreatedTimestamp");
			ar(totalConsumedFrames, "totalConsumedFrames");
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

		CQueryID CQueryBase::s_queryIDProvider(CQueryID::CreateInvalid());
		const CDebugRenderWorldImmediate CQueryBase::s_debugRenderWorldImmediate;

		CQueryBase::CQueryBase(const SCtorContext& ctorContext)
			: m_querierName(ctorContext.szQuerierName)
			, m_pHistory(ctorContext.pOptionalHistoryToWriteTo)
			, m_bAlreadyStarted(false)
			, m_queryID(ctorContext.queryID)
			, m_pQueryBlueprint(ctorContext.pQueryBlueprint)
			, m_pOptionalShuttledItems(ctorContext.pOptionalResultingItemsFromPreviousQuery)
			, m_queryCreatedFrame(gEnv->nMainFrameID)
			, m_queryCreatedTimestamp(gEnv->pTimer->GetAsyncTime())
			, m_totalConsumedFrames(0)
			, m_queryContext(*m_pQueryBlueprint.get(), m_querierName.c_str(), m_queryID, m_globalParams, m_pOptionalShuttledItems.get(), m_timeBudgetForCurrentUpdate, ctorContext.pOptionalHistoryToWriteTo ? &ctorContext.pOptionalHistoryToWriteTo->GetDebugRenderWorldPersistent() : nullptr, ctorContext.pOptionalHistoryToWriteTo ? &ctorContext.pOptionalHistoryToWriteTo->GetDebugMessageCollection() : nullptr)
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryCreated(m_queryCreatedFrame, m_queryCreatedTimestamp, m_pQueryBlueprint->GetName());
			}
		}

		CQueryBase::~CQueryBase()
		{
			if (m_pHistory)
			{
				m_pHistory->OnQueryDestroyed();
			}
		}

		QueryBaseUniquePtr CQueryBase::CreateQuery(const CQueryID& parentQueryID, std::shared_ptr<const CQueryBlueprint> pQueryBlueprint, const char* szQuerierName, const std::shared_ptr<CItemList>& pPotentialResultingItemsFromPreviousQuery, int priority)
		{
			// generate a new query ID (even if the query fails to start later on)
			const CQueryID id = ++s_queryIDProvider;

			// enable history-logging for this query according to a cvar
			HistoricQuerySharedPtr pOptionalHistoryEntry;
			if (SCvars::logQueryHistory)
			{
				pOptionalHistoryEntry = g_pHub->GetQueryHistoryManager().AddNewLiveHistoricQuery(id, szQuerierName, parentQueryID, priority);
			}

			// create a new query instance through the query-blueprint
			const SCtorContext queryCtorContext(id, pQueryBlueprint, priority, szQuerierName, pOptionalHistoryEntry, pPotentialResultingItemsFromPreviousQuery);
			QueryBaseUniquePtr q = pQueryBlueprint->CreateQuery(queryCtorContext);
			return q;
		}

		bool CQueryBase::Start(const Shared::IVariantDict& runtimeParams, Shared::IUqsString& error)
		{
			CRY_PROFILE_FUNCTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, m_pQueryBlueprint->GetName());

			CRY_ASSERT(!m_bAlreadyStarted);	// we don't support recycling the query

			m_bAlreadyStarted = true;

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
				CRY_PROFILE_SECTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, "UQS::Core::CQueryBase::Start: check global runtime parameters", m_pQueryBlueprint->GetName());

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
			// allow the derived class to do further custom start logic
			//

			const bool bFurtherStartingInDerivedClassSucceeded = OnStart(runtimeParams, error);

			if (!bFurtherStartingInDerivedClassSucceeded && m_pHistory)
			{
				SStatistics stats;
				GetStatistics(stats);
				m_pHistory->OnExceptionOccurred(error.c_str(), stats);
			}

			return bFurtherStartingInDerivedClassSucceeded;
		}

		void CQueryBase::AddItemMonitor(Client::ItemMonitorUniquePtr&& pItemMonitor)
		{
			CRY_ASSERT(pItemMonitor);
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

			++m_totalConsumedFrames;

			// immediate debug-rendering ON/OFF
			if (SCvars::debugDraw)
			{
				m_queryContext.pDebugRenderWorldImmediate = &s_debugRenderWorldImmediate;
			}
			else
			{
				m_queryContext.pDebugRenderWorldImmediate = nullptr;
			}

			bool bCorruptionOccurred = false;

			//
			// check the item-monitors for having encountered a corruption *before* updating the query
			//

			if (!m_itemMonitors.empty())
			{
				CRY_PROFILE_SECTION_ARG(UQS_PROFILED_SUBSYSTEM_TO_USE, "UQS::Core::CQueryBase::Update: item monitors", m_pQueryBlueprint->GetName());

				for (const Client::ItemMonitorUniquePtr& pItemMonitor : m_itemMonitors)
				{
					CRY_ASSERT(pItemMonitor);

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

			const EUpdateState state = bCorruptionOccurred ? EUpdateState::ExceptionOccurred : OnUpdate(amountOfGrantedTime, error);

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

			out.queryCreatedFrame = m_queryCreatedFrame;
			out.queryCreatedTimestamp = m_queryCreatedTimestamp;
			out.totalConsumedFrames = m_totalConsumedFrames;
			out.totalConsumedTime = m_totalConsumedTime;
			out.grantedAndUsedTimePerFrame = m_grantedAndUsedTimePerFrame;

			//
			// allow the derived class to add some more statistics
			//

			OnGetStatistics(out);
		}

		QueryResultSetUniquePtr CQueryBase::ClaimResultSet()
		{
			return std::move(m_pResultSet);
		}

		const CQueryID& CQueryBase::GetQueryID() const
		{
			return m_queryID;
		}

		const char* CQueryBase::GetQuerierName() const
		{
			return m_querierName.c_str();
		}

		HistoricQuerySharedPtr CQueryBase::GetHistoricQuery() const
		{
			return m_pHistory;
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
							CRY_ASSERT(it != globalParamsAsMap.cend());

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
							CRY_ASSERT(it != globalParamsAsMap.cend());

							const Shared::CVariantDict::SDataEntry& entry = it->second;
							entry.pItemFactory->AddItemToDebugRenderWorld(entry.pObject, debugRW);
						}
					}
				}

			}
		}

	}
}
