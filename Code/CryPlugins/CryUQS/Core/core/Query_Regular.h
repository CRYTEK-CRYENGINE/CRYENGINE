// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQuery_Regular
		//
		// - this is the actual query that generates items and runs evaluators on them
		// - the whole process is divided into multiple "phases", implemented as a method pointer
		// - once the last phase has finished, the query returns the "finished" status and the caller can then grab the resulting items
		//
		//===================================================================================

		class CQuery_Regular final : public CQueryBase
		{
		private:

			//===================================================================================
			//
			// SPhaseUpdateContext
			//
			// - this is passed to the method pointer of the current phase of the whole query processing
			//
			//===================================================================================

			struct SPhaseUpdateContext
			{
				explicit                                          SPhaseUpdateContext(Shared::CUqsString& _error);

				Shared::CUqsString&                               error;                            // output error message for when EPhaseStatus::ExceptionOccurred is returned
			};

			//===================================================================================
			//
			// EPhaseStatus
			//
			// - glorified "bool" to make the return value of the current phase method more readable
			// - it's used to prevent confusion when propagating the return value of several helper methods which return a plain bool
			//
			//===================================================================================

			enum EPhaseStatus
			{
				ExceptionOccurred,
				Ok,
			};

			//===================================================================================
			//
			// SItemWorkingData
			//
			// - this represents the "working data" of a single item while evaluators are running on it
			// - it contains state information for all instant- and deferred-evaluators and is used by them to "communicate" to each other
			// - one of these exists for each generated item
			//
			//===================================================================================

			struct SItemWorkingData
			{
				explicit                                          SItemWorkingData();

				size_t                                            indexInGeneratedItems;                         // index of the item in m_generatedItems
				float                                             accumulatedAndWeightedScoreSoFar;              // sum of the weighted scores of all evaluators that have been run so far

				evaluatorsBitfield_t                              bitsFinishedInstantEvaluators;                 // each bit represents the index of one of the instant evaluators that has fully run the item; if less bits than the number of instant-evaluator blueprint are set after the query has finished, it means that an instant-evaluator discarded the item (or an exception occurred) before the next one was asked to run
				evaluatorsBitfield_t                              bitsDiscardedByInstantEvaluators;              // each bit represents the index of one of the instant evaluators that has discarded the item; usually, only 1 bit at most will be set, since after that, the item already counts as discarded; the fact that this is a bitfield and not a bool is meant for inspection/debugging purpose to find out *which* of the instant evaluators failed
				evaluatorsBitfield_t                              bitsExceptionByInstantEvaluatorsThemselves;    // each bit represents the index of one of the instant evaluators that caused an exception during its execution
				evaluatorsBitfield_t                              bitsExceptionByInstantEvaluatorFunctionCalls;  // each bit represents the index of one of the instant evaluators whose preceding functions calls caused an exception

				evaluatorsBitfield_t                              bitsWorkingDeferredEvaluators;                 // each bit represents the index of one of the deferred evaluators; every time a deferred evaluator starts working on this item, it sets its bit in here; when finished or prematurely aborted, the bit is cleared again
				evaluatorsBitfield_t                              bitsFinishedDeferredEvaluators;                // each bit represents the index of one of the deferred evaluators; every time a deferred evaluator finishes (i. e. wasn't prematurely aborted), it sets its bit in here; the caller checks the bits and realizes that all evaluators had their say once all possible bits are set
				evaluatorsBitfield_t                              bitsAbortedDeferredEvaluators;                 // bits of deferred-evaluators that were started, but prematurely aborted as the item they were working on got discarded by another deferred-evaluator or was deemed to no longer yield a satisfying score
				evaluatorsBitfield_t                              bitsDiscardedByDeferredEvaluators;             // bits of evaluators having discarded the item; the item counts as discarded as soon as one bit in here is set; the fact that multiple bits can be set is only for inspection/debugging purpose
				evaluatorsBitfield_t                              bitsExceptionByDeferredEvaluatorsThemselves;   // each bit represents the index of one of the deferred evaluators that caused an exception during its execution
				evaluatorsBitfield_t                              bitsExceptionByDeferredEvaluatorFunctionCalls; // each bit represents the index of one of the deferred evaluators whose preceding functions calls caused an exception

				bool                                              bDisqualifiedDueToBadScore;                    // an item may get disqualified if it cannot make it into the candidates anymore due to a too bad score, even when some deferred-evaluators would still need to run
			};

			//===================================================================================
			//
			// SInstantEvaluatorWithIndex
			//
			// - contains the instantiated instant-evaluator-blueprint and its original position in the query blueprint
			// - before running all instant-evaluators, we sort them by cost and by modality so that they will (hopefully) be called
			//   from "cheap" to "expensive" and within this range once again from "testers" to "scorers"
			// - keeping track of where they were originally located in the query blueprint is necessary to index into the correct
			//   CQuery_Regular::m_functionCallHierarchyPerInstantEvalBP[] and to notify the caller of debugging stuff like "which evaluator did what"
			//
			//===================================================================================

			struct SInstantEvaluatorWithIndex
			{
				explicit                                          SInstantEvaluatorWithIndex(Client::InstantEvaluatorUniquePtr _pInstantEvaluator, Client::ParamsHolderUniquePtr _pParamsHolder, const Client::IInputParameterRegistry* _pInputParameterRegistry, const CEvaluationResultTransform& _evaluationResultTransform, size_t _originalIndexInQueryBlueprint);
				explicit                                          SInstantEvaluatorWithIndex(SInstantEvaluatorWithIndex&& other);
				SInstantEvaluatorWithIndex&                       operator=(SInstantEvaluatorWithIndex&& other);

				Client::InstantEvaluatorUniquePtr                 pInstantEvaluator;                    // instantiated exactly once for all items; gets re-used as it's stateless
				Client::ParamsHolderUniquePtr                     pParamsHolder;                        // also instantiated exactly once; gets refreshed to on each item iteration before passing it into the instant-evaluator
				const Client::IInputParameterRegistry*            pInputParameterRegistry;              // points back into the instant-evaluator factory (who owns it); used when making function calls to get the offsets of all parameters in memory so that each function knows where to write its return value to
				CEvaluationResultTransform                        evaluationResultTransform;            // copy of the evaluation-result-transform that resides in the instant-evaluator-blueprint (we use a copy for cache-friendliness)
				size_t                                            originalIndexInQueryBlueprint;        // the original position among the instant-evaluator blueprints in the query blueprint (*after* it was loaded from the datasource)
			};

			//===================================================================================
			//
			// SDeferredEvaluatorWithIndex
			//
			// - similar to SInstantEvaluatorWithIndex except:
			//   -- there is *no* sorting from "cheap" to "expensive" and from "testers" to "scorers" as these distinctions don't exist for deferred-evaluators (they're implicitly all expensive)
			//   -- the deferred-evaluator-blueprints get instantiated for each item individually (not just one instance for all items)
			//
			//===================================================================================

			struct SDeferredEvaluatorWithIndex
			{
				explicit                                          SDeferredEvaluatorWithIndex(Client::DeferredEvaluatorUniquePtr _pDeferredEvaluator, const CEvaluationResultTransform& _evaluationResultTransform, size_t _originalIndexInQueryBlueprint);
				explicit                                          SDeferredEvaluatorWithIndex(SDeferredEvaluatorWithIndex&& other);
				SDeferredEvaluatorWithIndex&                      operator=(SDeferredEvaluatorWithIndex&& other);

				Client::DeferredEvaluatorUniquePtr                pDeferredEvaluator;                   // instantiated exactly once for all items; gets re-used as it's stateless
				CEvaluationResultTransform                        evaluationResultTransform;            // copy of the evaluation-result-transform that resides in the deferred-evaluator-blueprint (we use a copy for cache-friendliness)
				size_t                                            originalIndexInQueryBlueprint;        // the original position among the deferred-evaluator blueprints in the query blueprint as it was loaded from the datasource
			};

			//===================================================================================
			//
			// SDeferredTask
			//
			// - used by the expensive-evaluation phase to evaluate a single item
			// - it contains all deferred-evaluators that need to run on the item
			// - the expensive instant-evaluators are not contained in here, since they will be run and finished immediately before starting such a task
			// - once all deferred-evaluators are finished, the whole task counts as finished
			// - several of these tasks will run concurrently, but never more than the desired result-set size (to prevent unnecessary extra work)
			//
			//===================================================================================

			struct SDeferredTask
			{
				// TODO: currently, these status flags are only set, but not read; they could be used to prevent the caller from spawning more deferred evaluators in case of resource congestion (-> could help resource balancing among multiple queries)
				static const uint8                                statusFlag_atLeastOneEvaluatorIsWaitingForExternalResourceInCurrentFrame = 1 << 0;
				static const uint8                                statusFlag_atLeastOneEvaluatorIsShortOnResources                         = 1 << 1;
				static const uint8                                statusFlag_atLeastOneEvaluatorIsDoingTimeSlicedWork                      = 1 << 2;

				explicit                                          SDeferredTask(SItemWorkingData* _pWorkingData);

				SItemWorkingData*                                 pWorkingData;                         // the .deferredEvaluators will write their outcome to here; this pointer gets transferred from m_remainingItemWorkingDatasToInspect
				std::list<SDeferredEvaluatorWithIndex>            deferredEvaluators;                   // these are still evaluating the item; shrinks whenever one finishes
				uint8                                             status;
			};

		public:

			explicit                                              CQuery_Regular(const SCtorContext& ctorContext);

		private:

			                                                      UQS_NON_COPYABLE(CQuery_Regular);

			// CQueryBase
			virtual bool                                          OnInstantiateFromQueryBlueprint(const Shared::IVariantDict& runtimeParams, Shared::CUqsString& error) override;
			virtual EUpdateState                                  OnUpdate(Shared::CUqsString& error) override;
			virtual void                                          OnCancel() override;
			virtual void                                          OnGetStatistics(SStatistics& out) const override;
			// ~CQueryBase

			EPhaseStatus                                          Phase1_PrepareGenerationPhase(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase2_GenerateItems(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase3_CreateDebugRepresentationsOfGeneratedItemsIfHistoryLoggingIsDesired(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase4_PrepareEvaluationPhase(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase5_RunCheapEvaluators(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase6_SortByScoreSoFar(const SPhaseUpdateContext& phaseUpdateContext);
			EPhaseStatus                                          Phase7_RunExpensiveEvaluators(const SPhaseUpdateContext& phaseUpdateContext);

			void                                                  RunInstantEvaluator(const SInstantEvaluatorWithIndex& instantEvaluatorToRun, SItemWorkingData& workingDataToWriteResultTo);
			void                                                  UpdateDeferredTasks();
			void                                                  UpdateDeferredTask(SDeferredTask& taskToUpdate);
			void                                                  AbortDeferredTask(SDeferredTask& taskToAbort, const char* szReasonForAbort);
			bool                                                  CanItemStillBeatTheWorstCandidate(const SItemWorkingData& itemToCheck) const;
			void                                                  AddItemToResultSetOrDisqualifyIt(SItemWorkingData& itemThatJustFinishedAndSurvivedAllEvaluators);
			void                                                  FinalizeItemAfterDeferredEvaluation(SItemWorkingData& itemToFinalize);
			void                                                  StartMoreEvaluatorsOnRemainingItems(const SPhaseUpdateContext& phaseUpdateContext);
			SDeferredTask*                                        StartDeferredTask(SItemWorkingData* pWorkingDataToInspectNext);

		private:

			EPhaseStatus                                          (CQuery_Regular::*m_currentPhaseFn)(const SPhaseUpdateContext&); // current phase we're in; each phase method is responsible for switching to the next phase when appropriate; the whole process is finished when m_currentPhaseFn is set to nullptr

			// debugging/inspection BEGIN
			std::vector<size_t>                                   m_elapsedFramesPerPhase;                                 // individually elapsed frames in each phase
			std::vector<CTimeValue>                               m_elapsedTimePerPhase;                                   // individually spent time in each phase
			std::vector<CTimeValue>                               m_peakElapsedTimePerPhaseUpdate;                         // how long did the longest method call take in each phase?
			// debugging/inspection END

			// used throughout several phases
			std::vector<SItemWorkingData>                         m_itemWorkingDatas;                                      // items with their intermediate scores and states of all evaluators; the order of this list stays intact as we may decide to inspect/debug some of the outcomes or see which evaluators were running on which items
			std::vector<SItemWorkingData*>                        m_remainingItemWorkingDatasToInspect;                    // pointers into m_itemWorkingDatas for items that haven't been discarded yet and for which some evaluators still need to run; gets sorted by score-so-far after all cheap instant-evaluators are run; shrinks as soon as an instant-evaluator discards an item or when all remaining deferred-evaluators are currently runnning on the item
			std::vector<SItemWorkingData*>                        m_candidates;                                            // candidates for the final result set; these ultimately point into m_itemWorkingData; this list grows as items survive all evaluators; once the desired result set size has been reached, the worst item might get kicked out by better ones

			// phase 1
			Client::GeneratorUniquePtr                            m_pGenerator;                                            // generates items into m_generatedItems in phase 2

			// phase 2
			CItemList                                             m_generatedItems;                                        // all the items that are being evaluated; this is kind of a read-only storage after they have been generated

			// phase 3
			size_t                                                m_currentItemIndexForCreatingDebugRepresentations;       // for continuing with creating debug-visualization and debug-proxies of all generated items in the next frame (in case we run out of time)

			// phase 4
			std::unique_ptr<SItemIterationContext>                m_pItemIterationContext;                                 // this gets instantiated right after all items got generated; it's used to provide the functions in the evaluation phase with the current item we're iterating on
			std::vector<std::unique_ptr<CFunctionCallHierarchy>>  m_functionCallHierarchyPerInstantEvalBP;                 // the function call hierarchy of each instant-evaluator-blueprint is re-used for the particular evaluator during item iteration
			std::vector<std::unique_ptr<CFunctionCallHierarchy>>  m_functionCallHierarchyPerDeferredEvalBP;                // ditto for deferred-evaluators
			size_t                                                m_maxCandidates;                                         // considering the number of generated items and the desired limit in the final result set, this is the maximum number of items that will actually make it into the very final result set

			// phase 5
			std::vector<SInstantEvaluatorWithIndex>               m_cheapInstantEvaluators;
			size_t                                                m_remainingItemWorkingDatasIndexForCheapInstantEvaluators;    // for continuing with cheap instant-evaluators in the next frame from where we left off when we ran out of time

			// phase 7
			std::vector<SInstantEvaluatorWithIndex>               m_expensiveInstantEvaluators;
			std::list<SDeferredTask>                              m_deferredTasks;
		};

	}
}
