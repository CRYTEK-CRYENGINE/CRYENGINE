// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		class CQueryHistoryManager;

		//===================================================================================
		//
		// CHistoricQuery
		//
		// - a single entry in the CQueryHistory
		// - represents the final statistics of a single query, details about the outcome of the evaluators and all its debug-draw information that was emitted by the evaluators
		//
		//===================================================================================

		class CHistoricQuery
		{
		// This section should be private, but the nested enums in these structs need to be accessible by yasli and "befriending" yasli
		// won't work due to a yasli-internal function being declared as static.
		//private:
		public:

			//===================================================================================
			//
			// SHistoricInstantEvaluatorResult
			//
			//===================================================================================

			struct SHistoricInstantEvaluatorResult
			{
				enum class EStatus
				{
					HasNotRunYet,
					HasFinishedAndDiscardedTheItem,
					HasFinishedAndScoredTheItem,
					ExceptionOccurredInFunctionCall,
					ExceptionOccurredInHimself
				};

				explicit                                        SHistoricInstantEvaluatorResult();
				void                                            Serialize(Serialization::IArchive& ar);

				EStatus                                         status;
				float                                           nonWeightedScore;               // only relevant if status == HasFinishedAndScoredTheItem
				float                                           weightedScore;
				string                                          furtherInformationAboutStatus;  // some detailed message about why the status is now what it is (only used by EStatus::ExceptionOccurredInFunctionCall and EStatus::ExceptionOccurredInHimself)
			};

			//===================================================================================
			//
			// SHistoricDeferredEvaluatorResult
			//
			//===================================================================================

			struct SHistoricDeferredEvaluatorResult
			{
				enum class EStatus
				{
					HasNotRunYet,
					IsRunningNow,
					HasFinishedAndDiscardedTheItem,
					HasFinishedAndScoredTheItem,
					GotAborted,
					ExceptionOccurredInFunctionCall,
					ExceptionOccurredInHimself
				};

				explicit                                        SHistoricDeferredEvaluatorResult();
				void                                            Serialize(Serialization::IArchive& ar);

				EStatus                                         status;
				float                                           nonWeightedScore;               // only relevant if status == HasFinishedAndScoredTheItem
				float                                           weightedScore;
				string                                          furtherInformationAboutStatus;  // some detailed message about why the status is now what it is (only used by EStatus::GotAborted, EStatus::ExceptionOccurredInFunctionCall and EStatus::ExceptionOccurredInHimself)
			};

			//===================================================================================
			//
			// SHistoricItem
			//
			//===================================================================================

			struct SHistoricItem
			{
				                                                SHistoricItem();            // ctors need to be non-explicit for serializing via yasli containers
				                                                SHistoricItem(const SHistoricItem& other);
				SHistoricItem&                                  operator=(const SHistoricItem& other);
				void                                            Serialize(Serialization::IArchive& ar);

				std::vector<SHistoricInstantEvaluatorResult>    resultOfAllInstantEvaluators;
				std::vector<SHistoricDeferredEvaluatorResult>   resultOfAllDeferredEvaluators;
				float                                           accumulatedAndWeightedScoreSoFar;
				bool                                            bDisqualifiedDueToBadScore;
				std::unique_ptr<CItemDebugProxyBase>            pDebugProxy;
			};

			//===================================================================================
			//
			// EQueryLifetimeStatus
			//
			//===================================================================================

			enum class EQueryLifetimeStatus
			{
				QueryIsNotCreatedYet,
				QueryIsAlive,
				QueryIsDestroyed
			};

		public:

			explicit                                            CHistoricQuery();
			explicit                                            CHistoricQuery(const CQueryID& queryID, const char* szQuerierName, const CQueryID& parentQueryID, CQueryHistoryManager* pOwningHistoryManager);

			CDebugRenderWorldPersistent&                        GetDebugRenderWorldPersistent();
			void                                                OnQueryCreated();
			void                                                OnQueryBlueprintInstantiationStarted(const char* szQueryBlueprintName);
			void                                                OnQueryCanceled(const CQueryBase::SStatistics& finalStatistics);
			void                                                OnQueryFinished(const CQueryBase::SStatistics& finalStatistics);
			void                                                OnQueryDestroyed();
			void                                                OnExceptionOccurred(const char* szExceptionMessage, const CQueryBase::SStatistics& finalStatistics);
			void                                                OnWarningOccurred(const char* szWarningMessage);
			void                                                OnGenerationPhaseFinished(size_t numGeneratedItems, const CQueryBlueprint& queryBlueprint);
			void                                                OnInstantEvaluatorScoredItem(size_t instantEvaluatorIndex, size_t itemIndex, float nonWeightedSingleScore, float weightedSingleScore, float accumulatedAndWeightedScoreSoFar);
			void                                                OnInstantEvaluatorDiscardedItem(size_t instantEvaluatorIndex, size_t itemIndex);
			void                                                OnFunctionCallExceptionOccurredInInstantEvaluator(size_t instantEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage);
			void                                                OnExceptionOccurredInInstantEvaluator(size_t instantEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage);
			void                                                OnDeferredEvaluatorStartedRunningOnItem(size_t deferredEvaluatorIndex, size_t itemIndex);
			void                                                OnDeferredEvaluatorScoredItem(size_t deferredEvaluatorIndex, size_t itemIndex, float nonWeightedSingleScore, float weightedSingleScore, float accumulatedAndWeightedScoreSoFar);
			void                                                OnDeferredEvaluatorDiscardedItem(size_t deferredEvaluatorIndex, size_t itemIndex);
			void                                                OnDeferredEvaluatorGotAborted(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szReasonForAbort);
			void                                                OnFunctionCallExceptionOccurredInDeferredEvaluator(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage);
			void                                                OnExceptionOccurredInDeferredEvaluator(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage);
			void                                                OnItemGotDisqualifiedDueToBadScore(size_t itemIndex);
			void                                                CreateItemDebugProxyViaItemFactoryForItem(const Client::IItemFactory& itemFactory, const void* pItem, size_t indexInGeneratedItemsForWhichToCreateTheProxy);

			size_t                                              GetRoughMemoryUsage() const;
			bool                                                FindClosestItemInView(const SDebugCameraView& cameraView, size_t& outItemIndex) const;     // returns true and outputs the index of the closest item to outItemIndex or just returns false if there are no good candidates nearby
			void                                                DrawDebugPrimitivesInWorld(size_t indexOfItemCurrentlyBeingFocused, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks) const;
			SDebugCameraView                                    GetIdealDebugCameraView(const SDebugCameraView& currentCameraView) const;

			void                                                FillQueryHistoryConsumerWithShortInfoAboutQuery(IQueryHistoryConsumer& consumer, bool bHighlight) const;
			void                                                FillQueryHistoryConsumerWithDetailedInfoAboutQuery(IQueryHistoryConsumer& consumer) const;
			void                                                FillQueryHistoryConsumerWithDetailedInfoAboutItem(IQueryHistoryConsumer& consumer, size_t itemIndex) const;
			void                                                FillQueryHistoryConsumerWithInstantEvaluatorNames(IQueryHistoryConsumer& consumer) const;
			void                                                FillQueryHistoryConsumerWithDeferredEvaluatorNames(IQueryHistoryConsumer& consumer) const;

			const CQueryID&                                     GetQueryID() const;
			const CQueryID&                                     GetParentQueryID() const;
			bool                                                IsQueryDestroyed() const;
			const CTimeValue&                                   GetQueryDestroyedTimestamp() const;

			void                                                Serialize(Serialization::IArchive& ar);

		private:

			                                                    UQS_NON_COPYABLE(CHistoricQuery);

			enum EItemAnalyzeStatus
			{
				ExceptionOccurred,
				DiscardedByAtLeastOneEvaluator,
				DisqualifiedDueToBadScoreBeforeAllEvaluatorsHadRun,
				DisqualifiedDueToBadScoreAfterAllEvaluatorsHadRun,
				StillBeingEvaluated,
				SurvivedAllEvaluators,
			};

			static EItemAnalyzeStatus                           AnalyzeItemStatus(const SHistoricItem& itemToAnalyze, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks, float& outAccumulatedAndWeightedScoreOfMaskedEvaluators, bool& outFoundScoreOutsideValidRange);
			CTimeValue                                          ComputeElapsedTimeFromQueryCreationToDestruction() const;

		private:

			CQueryHistoryManager*                               m_pOwningHistoryManager;
			CQueryID                                            m_queryID;
			CQueryID                                            m_parentQueryID;
			string                                              m_querierName;
			string                                              m_queryBlueprintName;
			EQueryLifetimeStatus                                m_queryLifetimeStatus;
			CTimeValue                                          m_queryCreatedTimestamp;
			CTimeValue                                          m_queryDestroyedTimestamp;
			bool                                                m_bGotCanceledPrematurely;
			bool                                                m_bExceptionOccurred;
			string                                              m_exceptionMessage;
			std::vector<string>                                 m_warningMessages;
			CDebugRenderWorldPersistent                         m_debugRenderWorldPersistent;
			std::vector<SHistoricItem>                          m_items;                            // counter-part of all the generated items
			std::vector<string>                                 m_instantEvaluatorNames;            // cached names of all instant-evaluator factories; this is necessary in case query blueprints get reloaded at runtime to prevent dangling pointers
			std::vector<string>                                 m_deferredEvaluatorNames;           // ditto for deferred-evaluator factories
			size_t                                              m_longestEvaluatorName;             // length of the longest name of either the instant- or deferred-evaluators; used for nice indentation when drawing item details as 2D text on screen
			CQueryBase::SStatistics                             m_finalStatistics;                  // final statistics that get passed in when the live query gets destroyed
			CItemDebugProxyFactory                              m_itemDebugProxyFactory;
		};

		//===================================================================================
		//
		// CQueryHistory
		//
		// - collection of CHistoricQuery instances
		// - logging of queries must be enabled before starting the query
		//
		//===================================================================================

		class CQueryHistory
		{
		public:
			explicit                                       CQueryHistory();
			CQueryHistory&                                 operator=(CQueryHistory&& other);
			HistoricQuerySharedPtr                         AddNewHistoryEntry(const CQueryID& queryID, const char* szQuerierName, const CQueryID& parentQueryID, CQueryHistoryManager* pOwningHistoryManager);
			void                                           Clear();
			size_t                                         GetHistorySize() const;		// number of CHistoricQueries
			const CHistoricQuery&                          GetHistoryEntryByIndex(size_t index) const;
			const CHistoricQuery*                          FindHistoryEntryByQueryID(const CQueryID& queryID) const;
			size_t                                         GetRoughMemoryUsage() const;
			void                                           SetArbitraryMetaDataForSerialization(const char* szKey, const char* szValue);    // adds arbitrary key/value pairs to the history, which will blindly be serialized
			void                                           Serialize(Serialization::IArchive& ar);

		private:
			std::vector<HistoricQuerySharedPtr>            m_history;
			std::map<string, string>                       m_metaData;
		};

	}
}
