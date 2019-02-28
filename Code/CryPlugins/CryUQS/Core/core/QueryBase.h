// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		class CHistoricQuery;

		//===================================================================================
		//
		// HistoricQuerySharedPtr
		//
		//===================================================================================

		// FIXME: shouldn't be here (a better place would be QueryHistory.h since it defines CHistoricQuery), but some #include dependencies are currently enforcing us to do it this way
		typedef std::shared_ptr<CHistoricQuery> HistoricQuerySharedPtr;

		//===================================================================================
		//
		// CQueryBase
		//
		// - base class for all query classes
		// - the actual classes will get instantiated from a CQueryBlueprint at game runtime through their query factories
		//
		//===================================================================================

		class CQueryBlueprint;

		class CQueryBase
		{
		public:

			//===================================================================================
			//
			// SCtorContext
			//
			//===================================================================================

			struct SCtorContext
			{
				explicit                                SCtorContext(const CQueryID& _queryID, const char* _szQuerierName, const HistoricQuerySharedPtr& _pOptionalHistoryToWriteTo, const std::shared_ptr<CItemList>& _pOptionalResultingItemsFromPreviousQuery);

				CQueryID                                queryID;
				const char*                             szQuerierName;
				HistoricQuerySharedPtr                  pOptionalHistoryToWriteTo;
				std::shared_ptr<CItemList>              pOptionalResultingItemsFromPreviousQuery;     // this is how we pass items of a result set from one query to another
			};

			//===================================================================================
			//
			// EUpdateState
			//
			//===================================================================================

			enum class EUpdateState
			{
				StillRunning,                           // the query is still running
				Finished,                               // the query has finished without errors
				ExceptionOccurred                       // an error occurred while the query was running; an error message that was passed to Update() then contains some details
			};

			//===================================================================================
			//
			// SGrantedAndUsedTime
			//
			// - for detailed per-frame statistics of how much time was granted and how much time was actually used to do some work
			//
			//===================================================================================

			struct SGrantedAndUsedTime
			{
				explicit                                SGrantedAndUsedTime();
				explicit                                SGrantedAndUsedTime(const CTimeValue& _granted, const CTimeValue& _used);
				void                                    Serialize(Serialization::IArchive& ar);

				CTimeValue                              granted;
				CTimeValue                              used;
			};

			//===================================================================================
			//
			// SStatistics
			//
			// - overall statistics of the query
			// - can be retrieved at any time even while the query is still running to get some "live insight" into what's going on
			//
			//===================================================================================

			struct SStatistics
			{
				explicit                                SStatistics();
				void                                    Serialize(Serialization::IArchive& ar);

				// known to + set by CQueryBase (common stuff used by all query types)
				// this is stuff that CQueryManager::DebugDrawQueryStatistics() is partly interested in (but it's also interested in the number of items - hmmm, could we just display the elapsed frames instead?)
				string                                  querierName;
				string                                  queryBlueprintName;
				size_t                                  totalElapsedFrames;
				CTimeValue                              totalConsumedTime;
				std::vector<SGrantedAndUsedTime>        grantedAndUsedTimePerFrame;       // grows with each update call

				// could be known to composite query classes that pass around the resulting items (but currently only known to + set by CQuery_Regular)
				size_t                                  numDesiredItems;                  // number of items the user wants to find; this is often 1 (e. g. "give me *one* good shooting position"), but can be any number, whereas 0 has a special meaning: "give me all items you can find"
				size_t                                  numGeneratedItems;                // number of items the generator has produced
				size_t                                  numRemainingItemsToInspect;       // this number decreases with each finished item while the query is ongoing
				size_t                                  numItemsInFinalResultSet;         // number of items that finally made it into the result set
				size_t                                  memoryUsedByGeneratedItems;       // amount of memory used by all generated items; this memory is usually used on the client side (since they provide us with an item generator that we just call)
				size_t                                  memoryUsedByItemsWorkingData;     // amount of memory used to keep track of the working state of all items throughout the evaluation phases

 				// known to + set by CQuery_Regular only (as it deals with several phases which is specific for that class)
				std::vector<size_t>                     elapsedFramesPerPhase;
				std::vector<CTimeValue>                 elapsedTimePerPhase;
				std::vector<CTimeValue>                 peakElapsedTimePerPhaseUpdate;
				std::vector<size_t>                     instantEvaluatorsRuns;            // how often has each kind of instant-evaluator been run
				std::vector<size_t>                     deferredEvaluatorsFullRuns;       // how often has each kind of deferred-evaluator been fully run (i. e. started and run until it completed all work an a given item)
				std::vector<size_t>                     deferredEvaluatorsAbortedRuns;    // how often has each kind of deferred-evaluator been been started, but got prematurely aborted as the item was being discarded in the meantime
			};

		public:
			explicit                                    CQueryBase(const SCtorContext& ctorContext, bool bRequiresSomeTimeBudgetForExecution);
			virtual                                     ~CQueryBase();

			bool                                        RequiresSomeTimeBudgetForExecution() const;
			bool                                        InstantiateFromQueryBlueprint(const std::shared_ptr<const CQueryBlueprint>& pQueryBlueprint, const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error);
			void                                        AddItemMonitor(Client::ItemMonitorUniquePtr&& pItemMonitor);
			void                                        TransferAllItemMonitorsToOtherQuery(CQueryBase& receiver);
			EUpdateState                                Update(const CTimeValue& amountOfGrantedTime, Shared::CUqsString& error);
			void                                        Cancel();
			void                                        GetStatistics(SStatistics& out) const;
			void                                        EmitTimeExcessWarningToConsoleAndQueryHistory(const CTimeValue& timeGranted, const CTimeValue& timeUsed) const;

			// careful: using the result while the query is still in EUpdateState::StillRunning state is undefined behavior
			QueryResultSetUniquePtr                     ClaimResultSet();

		private:
			virtual bool                                OnInstantiateFromQueryBlueprint(const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error) = 0;
			virtual EUpdateState                        OnUpdate(Shared::CUqsString& error) = 0;
			virtual void                                OnCancel() = 0;
			virtual void                                OnGetStatistics(SStatistics& out) const = 0;

			void                                        AddItemsFromGlobalParametersToDebugRenderWorld() const;

		protected:
			// debugging
			string                                      m_querierName;                    // debug name that the caller passed in to re-identify his query for debugging purposes
			HistoricQuerySharedPtr                      m_pHistory;                       // optional; used to build a history of the ongoing query (and to pass its debug-renderworld to all evaluators)
			// ~debugging

			const CQueryID                              m_queryID;                        // the unique queryID that can be used to identify this instance from inside the CQueryManager
			std::shared_ptr<const CQueryBlueprint>      m_pQueryBlueprint;                // we'll instantiate all query components (generator, evaluators, etc) via this blueprint
			std::shared_ptr<CItemList>                  m_pOptionalShuttledItems;         // when queries are chained, the items in the result set of the previous query will be transferred to here (ready to get evaluated straight away)
			QueryResultSetUniquePtr                     m_pResultSet;                     // once the query has finished evaluating all items (and hasn't bumped into a runtime exception), it will write the final items to here
			CTimeBudget                                 m_timeBudgetForCurrentUpdate;     // this gets "restarted" on each Update() call with the amount of granted time that has been passed in by the caller

		private:
			// debugging
			size_t                                      m_totalElapsedFrames;             // runaway-counter that increments on each Update() call
			CTimeValue                                  m_totalConsumedTime;              // run-away timer that increments on each Update() call
			std::vector<SGrantedAndUsedTime>            m_grantedAndUsedTimePerFrame;     // keeps track of how much time we were given to do some work on each frame and how much time we actually spent; grows on each Update() call
			// ~debugging

			const bool                                  m_bRequiresSomeTimeBudgetForExecution;
			Shared::CVariantDict                        m_globalParams;                   // merge between constant- and runtime-params
			std::vector<Client::ItemMonitorUniquePtr>   m_itemMonitors;                   // Update() checks these to ensure that no corruption of the reasoning space goes unnoticed; when the query finishes, these monitors may get transferred to the parent to carry on monitoring alongside further child queries

		protected:
			// !! m_blackboard: this needs to come after all other member variables, as it relies on their proper initialization!!
			SQueryBlackboard                            m_blackboard;                     // bundles some stuff for functions, generators and evaluators to read from it

		private:
			static const CDebugRenderWorldImmediate     s_debugRenderWorldImmediate;      // handed out to all generators, evaluators and functions if immediate debug-rendering is turned on via SCvars::debugDraw
		};

	}
}
