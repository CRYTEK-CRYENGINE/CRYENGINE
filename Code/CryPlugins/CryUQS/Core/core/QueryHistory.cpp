// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryHistory.h"
#include <CryRenderer/IRenderAuxGeom.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

// make the global Serialize() functions available for use in yasli serialization
using UQS::Core::Serialize;

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CHistoricQuery: macros for enum serialization
		//
		//===================================================================================

		SERIALIZATION_ENUM_BEGIN_NESTED(CHistoricQuery, EQueryLifetimeStatus, "")
		SERIALIZATION_ENUM(CHistoricQuery::EQueryLifetimeStatus::QueryIsNotCreatedYet, "QueryIsNotCreatedYet", "")
		SERIALIZATION_ENUM(CHistoricQuery::EQueryLifetimeStatus::QueryIsAlive, "QueryIsAlive", "")
		SERIALIZATION_ENUM(CHistoricQuery::EQueryLifetimeStatus::QueryIsDestroyed, "QueryIsDestroyed", "")
		SERIALIZATION_ENUM_END()

		SERIALIZATION_ENUM_BEGIN_NESTED2(CHistoricQuery, SHistoricInstantEvaluatorResult, EStatus, "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet, "HasNotRunYet", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem, "HasFinishedAndDiscardedTheItem", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndScoredTheItem, "HasFinishedAndScoredTheItem", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall, "ExceptionOccurredInFunctionCall", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInHimself, "ExceptionOccurredInHimself", "")
		SERIALIZATION_ENUM_END()

		SERIALIZATION_ENUM_BEGIN_NESTED2(CHistoricQuery, SHistoricDeferredEvaluatorResult, EStatus, "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet, "HasNotRunYet", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow, "IsRunningNow", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem, "HasFinishedAndDiscardedTheItem", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndScoredTheItem, "HasFinishedAndScoredTheItem", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::GotAborted, "GotAborted", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall, "ExceptionOccurredInFunctionCall", "")
		SERIALIZATION_ENUM(CHistoricQuery::SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInHimself, "ExceptionOccurredInHimself", "")
		SERIALIZATION_ENUM_END()

		//===================================================================================
		//
		// CHistoricQuery::SHistoricInstantEvaluatorResult
		//
		//===================================================================================

		CHistoricQuery::SHistoricInstantEvaluatorResult::SHistoricInstantEvaluatorResult()
			: status(EStatus::HasNotRunYet)
			, nonWeightedScore(0.0f)
			, weightedScore(0.0f)
			, furtherInformationAboutStatus()
		{}

		void CHistoricQuery::SHistoricInstantEvaluatorResult::Serialize(Serialization::IArchive& ar)
		{
			ar(status, "status");
			ar(nonWeightedScore, "nonWeightedScore");
			ar(weightedScore, "weightedScore");
			ar(furtherInformationAboutStatus, "furtherInformationAboutStatus");
		}

		//===================================================================================
		//
		// CHistoricQuery::SHistoricDeferredEvaluatorResult
		//
		//===================================================================================

		CHistoricQuery::SHistoricDeferredEvaluatorResult::SHistoricDeferredEvaluatorResult()
			: status(EStatus::HasNotRunYet)
			, nonWeightedScore(0.0f)
			, weightedScore(0.0f)
			, furtherInformationAboutStatus()
		{}

		void CHistoricQuery::SHistoricDeferredEvaluatorResult::Serialize(Serialization::IArchive& ar)
		{
			ar(status, "status");
			ar(nonWeightedScore, "nonWeightedScore");
			ar(weightedScore, "weightedScore");
			ar(furtherInformationAboutStatus, "furtherInformationAboutStatus");
		}

		//===================================================================================
		//
		// CHistoricQuery::SHistoricItem
		//
		//===================================================================================

		CHistoricQuery::SHistoricItem::SHistoricItem()
			: accumulatedAndWeightedScoreSoFar(0.0f)
			, bDisqualifiedDueToBadScore(false)
		{}

		CHistoricQuery::SHistoricItem::SHistoricItem(const SHistoricItem& other)
			: resultOfAllInstantEvaluators(other.resultOfAllInstantEvaluators)
			, resultOfAllDeferredEvaluators(other.resultOfAllDeferredEvaluators)
			, accumulatedAndWeightedScoreSoFar(other.accumulatedAndWeightedScoreSoFar)
			, bDisqualifiedDueToBadScore(other.bDisqualifiedDueToBadScore)
			, pDebugProxy()	// bypass the unique_ptr
		{}

		CHistoricQuery::SHistoricItem& CHistoricQuery::SHistoricItem::operator=(const SHistoricItem& other)
		{
			if (this != &other)
			{
				resultOfAllInstantEvaluators = other.resultOfAllInstantEvaluators;
				resultOfAllDeferredEvaluators = other.resultOfAllDeferredEvaluators;
				accumulatedAndWeightedScoreSoFar = other.accumulatedAndWeightedScoreSoFar;
				bDisqualifiedDueToBadScore = other.bDisqualifiedDueToBadScore;
				// omit .pDebugProxy to bypass the unique_ptr
			}
			return *this;
		}

		void CHistoricQuery::SHistoricItem::Serialize(Serialization::IArchive& ar)
		{
			ar(resultOfAllInstantEvaluators, "resultOfAllInstantEvaluators");
			ar(resultOfAllDeferredEvaluators, "resultOfAllDeferredEvaluators");
			ar(accumulatedAndWeightedScoreSoFar, "accumulatedAndWeightedScoreSoFar");
			ar(bDisqualifiedDueToBadScore, "bDisqualifiedDueToBadScore");
			ar(pDebugProxy, "pDebugProxy");
		}

		//===================================================================================
		//
		// CHistoricQuery
		//
		//===================================================================================

		CHistoricQuery::CHistoricQuery()
			: m_pOwningHistoryManager(nullptr)
			, m_queryID(CQueryID::CreateInvalid())
			, m_parentQueryID(CQueryID::CreateInvalid())
			, m_queryLifetimeStatus(EQueryLifetimeStatus::QueryIsNotCreatedYet)
			, m_bGotCanceledPrematurely(false)
			, m_bExceptionOccurred(false)
			, m_longestEvaluatorName(0)
		{
			// nothing
		}

		CHistoricQuery::CHistoricQuery(const CQueryID& queryID, const char* szQuerierName, const CQueryID& parentQueryID, CQueryHistoryManager* pOwningHistoryManager)
			: m_pOwningHistoryManager(pOwningHistoryManager)
			, m_queryID(queryID)
			, m_parentQueryID(parentQueryID)
			, m_querierName(szQuerierName)
			, m_queryLifetimeStatus(EQueryLifetimeStatus::QueryIsNotCreatedYet)
			, m_bGotCanceledPrematurely(false)
			, m_bExceptionOccurred(false)
			, m_longestEvaluatorName(0)
		{
			// nothing
		}

		CDebugRenderWorldPersistent& CHistoricQuery::GetDebugRenderWorldPersistent()
		{
			return m_debugRenderWorldPersistent;
		}

		void CHistoricQuery::OnQueryCreated()
		{
			m_queryCreatedTimestamp = gEnv->pTimer->GetAsyncTime();
			m_queryLifetimeStatus = EQueryLifetimeStatus::QueryIsAlive;

			// notify the top-level query-history-manager that the underlying query has just been created/started
			m_pOwningHistoryManager->UnderlyingQueryJustGotCreated(m_queryID);
		}

		void CHistoricQuery::OnQueryBlueprintInstantiationStarted(const char* szQueryBlueprintName)
		{
			m_queryBlueprintName = szQueryBlueprintName;
		}

		void CHistoricQuery::OnQueryCanceled(const CQueryBase::SStatistics& finalStatistics)
		{
			m_bGotCanceledPrematurely = true;
			m_finalStatistics = finalStatistics;
		}

		void CHistoricQuery::OnQueryFinished(const CQueryBase::SStatistics& finalStatistics)
		{
			m_finalStatistics = finalStatistics;
		}

		void CHistoricQuery::OnQueryDestroyed()
		{
			m_queryDestroyedTimestamp = gEnv->pTimer->GetAsyncTime();
			m_queryLifetimeStatus = EQueryLifetimeStatus::QueryIsDestroyed;

			// notify the top-level query-history-manager that the underlying query has now been finished
			m_pOwningHistoryManager->UnderlyingQueryIsGettingDestroyed(m_queryID);
		}

		void CHistoricQuery::OnExceptionOccurred(const char* szExceptionMessage, const CQueryBase::SStatistics& finalStatistics)
		{
			m_exceptionMessage = szExceptionMessage;
			m_bExceptionOccurred = true;
			m_finalStatistics = finalStatistics;
		}

		void CHistoricQuery::OnWarningOccurred(const char* szWarningMessage)
		{
			m_warningMessages.push_back(szWarningMessage);
		}

		void CHistoricQuery::OnGenerationPhaseFinished(size_t numGeneratedItems, const CQueryBlueprint& queryBlueprint)
		{
			//
			// make room for all freshly generated items to record the outcome of all of their evaluators from now on
			//

			const std::vector<CInstantEvaluatorBlueprint*>& instantEvaluatorBlueprints = queryBlueprint.GetInstantEvaluatorBlueprints();
			const std::vector<CDeferredEvaluatorBlueprint*>& deferredEvaluatorBlueprints = queryBlueprint.GetDeferredEvaluatorBlueprints();

			const size_t numInstantEvaluatorBlueprints = instantEvaluatorBlueprints.size();
			const size_t numDeferredEvaluatorBlueprints = deferredEvaluatorBlueprints.size();

			// items
			m_items.resize(numGeneratedItems);
			for (SHistoricItem& item : m_items)
			{
				item.resultOfAllInstantEvaluators.resize(numInstantEvaluatorBlueprints);
				item.resultOfAllDeferredEvaluators.resize(numDeferredEvaluatorBlueprints);
			}

			// instant-evaluator names
			m_instantEvaluatorNames.reserve(numInstantEvaluatorBlueprints);
			for (const CInstantEvaluatorBlueprint* pIEBP : instantEvaluatorBlueprints)
			{
				const char* szName = pIEBP->GetFactory().GetName();
				m_instantEvaluatorNames.push_back(szName);
				m_longestEvaluatorName = std::max(m_longestEvaluatorName, strlen(szName));
			}

			// deferred-evaluator names
			m_deferredEvaluatorNames.reserve(numDeferredEvaluatorBlueprints);
			for (const CDeferredEvaluatorBlueprint* pDEBP : deferredEvaluatorBlueprints)
			{
				const char* szName = pDEBP->GetFactory().GetName();
				m_deferredEvaluatorNames.push_back(szName);
				m_longestEvaluatorName = std::max(m_longestEvaluatorName, strlen(szName));
			}
		}

		void CHistoricQuery::OnInstantEvaluatorScoredItem(size_t instantEvaluatorIndex, size_t itemIndex, float nonWeightedSingleScore, float weightedSingleScore, float accumulatedAndWeightedScoreSoFar)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricInstantEvaluatorResult& result = item.resultOfAllInstantEvaluators[instantEvaluatorIndex];
			assert(result.status == SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet);
			result.nonWeightedScore = nonWeightedSingleScore;
			result.weightedScore = weightedSingleScore;
			result.status = SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndScoredTheItem;
			item.accumulatedAndWeightedScoreSoFar = accumulatedAndWeightedScoreSoFar;
		}

		void CHistoricQuery::OnInstantEvaluatorDiscardedItem(size_t instantEvaluatorIndex, size_t itemIndex)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricInstantEvaluatorResult& result = item.resultOfAllInstantEvaluators[instantEvaluatorIndex];
			assert(result.status == SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem;
		}

		void CHistoricQuery::OnFunctionCallExceptionOccurredInInstantEvaluator(size_t instantEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricInstantEvaluatorResult& result = item.resultOfAllInstantEvaluators[instantEvaluatorIndex];
			assert(result.status == SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall;
			result.furtherInformationAboutStatus = szExceptionMessage;
		}

		void CHistoricQuery::OnExceptionOccurredInInstantEvaluator(size_t instantEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricInstantEvaluatorResult& result = item.resultOfAllInstantEvaluators[instantEvaluatorIndex];
			assert(result.status == SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInHimself;
			result.furtherInformationAboutStatus = szExceptionMessage;
		}

		void CHistoricQuery::OnDeferredEvaluatorStartedRunningOnItem(size_t deferredEvaluatorIndex, size_t itemIndex)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricDeferredEvaluatorResult& result = item.resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow;
		}

		void CHistoricQuery::OnDeferredEvaluatorScoredItem(size_t deferredEvaluatorIndex, size_t itemIndex, float nonWeightedSingleScore, float weightedSingleScore, float accumulatedAndWeightedScoreSoFar)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricDeferredEvaluatorResult& result = item.resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow);
			result.nonWeightedScore = nonWeightedSingleScore;
			result.weightedScore = weightedSingleScore;
			result.status = SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndScoredTheItem;
			item.accumulatedAndWeightedScoreSoFar = accumulatedAndWeightedScoreSoFar;
		}

		void CHistoricQuery::OnDeferredEvaluatorDiscardedItem(size_t deferredEvaluatorIndex, size_t itemIndex)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricDeferredEvaluatorResult& result = item.resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow);
			result.status = SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem;
		}

		void CHistoricQuery::OnDeferredEvaluatorGotAborted(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szReasonForAbort)
		{
			SHistoricDeferredEvaluatorResult& result = m_items[itemIndex].resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow);
			result.status = SHistoricDeferredEvaluatorResult::EStatus::GotAborted;
			result.furtherInformationAboutStatus = szReasonForAbort;
		}

		void CHistoricQuery::OnFunctionCallExceptionOccurredInDeferredEvaluator(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricDeferredEvaluatorResult& result = item.resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall;
			result.furtherInformationAboutStatus = szExceptionMessage;
		}

		void CHistoricQuery::OnExceptionOccurredInDeferredEvaluator(size_t deferredEvaluatorIndex, size_t itemIndex, const char* szExceptionMessage)
		{
			SHistoricItem& item = m_items[itemIndex];
			SHistoricDeferredEvaluatorResult& result = item.resultOfAllDeferredEvaluators[deferredEvaluatorIndex];
			assert(result.status == SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet);
			result.status = SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInHimself;
			result.furtherInformationAboutStatus = szExceptionMessage;
		}

		void CHistoricQuery::OnItemGotDisqualifiedDueToBadScore(size_t itemIndex)
		{
			m_items[itemIndex].bDisqualifiedDueToBadScore = true;
		}

		void CHistoricQuery::CreateItemDebugProxyViaItemFactoryForItem(const Client::IItemFactory& itemFactory, const void* pItem, size_t indexInGeneratedItemsForWhichToCreateTheProxy)
		{
			itemFactory.CreateItemDebugProxyForItem(pItem, m_itemDebugProxyFactory);
			std::unique_ptr<CItemDebugProxyBase> freshlyCreatedItemProxy = m_itemDebugProxyFactory.GetAndForgetLastCreatedDebugItemRenderProxy();
			m_items[indexInGeneratedItemsForWhichToCreateTheProxy].pDebugProxy = std::move(freshlyCreatedItemProxy);
		}

		size_t CHistoricQuery::GetRoughMemoryUsage() const
		{
			size_t sizeOfSingleItem = 0;

			if (!m_items.empty())
			{
				// notice: the actual capacities are not considered, but shouldn't be higher than the actual element counts, since we only called resize() on the initial containers
				sizeOfSingleItem += sizeof(SHistoricItem);
				sizeOfSingleItem += m_items[0].resultOfAllInstantEvaluators.size() * sizeof(SHistoricInstantEvaluatorResult);
				sizeOfSingleItem += m_items[0].resultOfAllDeferredEvaluators.size() * sizeof(SHistoricDeferredEvaluatorResult);
			}

			const size_t sizeOfAllItems = sizeOfSingleItem * m_items.size();
			const size_t sizeOfDebugRenderWorld = m_debugRenderWorldPersistent.GetRoughMemoryUsage();
			return sizeOfAllItems + sizeOfDebugRenderWorld;
		}

		CHistoricQuery::EItemAnalyzeStatus CHistoricQuery::AnalyzeItemStatus(const SHistoricItem& itemToAnalyze, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks, float& outAccumulatedAndWeightedScoreOfMaskedEvaluators, bool& outFoundScoreOutsideValidRange)
		{
			outAccumulatedAndWeightedScoreOfMaskedEvaluators = 0.0f;
			outFoundScoreOutsideValidRange = false;

			bool bHasEncounteredSomeException = false;
			bool bAllEvaluatorsHaveRun = true;
			bool bDiscardedByAtLeastOneEvaluator = false;

			//
			// instant-evaluators
			//

			for (size_t i = 0, n = itemToAnalyze.resultOfAllInstantEvaluators.size(); i < n; ++i)
			{
				// skip this evaluator if the caller is not interested in having it contribute to the item's drawing color
				const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)1 << i;
				if (!(evaluatorDrawMasks.maskInstantEvaluators & bit))
					continue;

				const SHistoricInstantEvaluatorResult& ieResult = itemToAnalyze.resultOfAllInstantEvaluators[i];

				switch (ieResult.status)
				{
				case SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndScoredTheItem:
					// check for illegal score (one that is outside [0.0 .. 1.0] which can happen if an evaluator is implemented wrongly)
					if (ieResult.nonWeightedScore < -FLT_EPSILON || ieResult.nonWeightedScore > 1.0f + FLT_EPSILON)
					{
						outFoundScoreOutsideValidRange = true;
					}
					outAccumulatedAndWeightedScoreOfMaskedEvaluators += ieResult.weightedScore;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem:
					bDiscardedByAtLeastOneEvaluator = true;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet:
					bAllEvaluatorsHaveRun = false;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall:
				case SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInHimself:
					bHasEncounteredSomeException = true;
					break;

				default:
					// the remaining stati are not relevant for our purpose
					break;
				}
			}

			//
			// deferred-evaluators
			//

			for (size_t i = 0, n = itemToAnalyze.resultOfAllDeferredEvaluators.size(); i < n; ++i)
			{
				// skip this evaluator if the caller is not interested in having it contribute to the item's drawing color
				const evaluatorsBitfield_t bit = (evaluatorsBitfield_t)1 << i;
				if (!(evaluatorDrawMasks.maskDeferredEvaluators & bit))
					continue;

				const SHistoricDeferredEvaluatorResult& deResult = itemToAnalyze.resultOfAllDeferredEvaluators[i];

				switch (deResult.status)
				{
				case SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndScoredTheItem:
					// check for illegal score (one that is outside [0.0 .. 1.0] which can happen if an evaluator is implemented wrongly)
					if (deResult.nonWeightedScore < -FLT_EPSILON || deResult.nonWeightedScore > 1.0f + FLT_EPSILON)
					{
						outFoundScoreOutsideValidRange = true;
					}
					outAccumulatedAndWeightedScoreOfMaskedEvaluators += deResult.weightedScore;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem:
					bDiscardedByAtLeastOneEvaluator = true;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet:
				case SHistoricDeferredEvaluatorResult::EStatus::GotAborted:
					bAllEvaluatorsHaveRun = false;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall:
				case SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInHimself:
					bHasEncounteredSomeException = true;
					break;

				default:
					// the remaining stati are not relevant for our purpose
					break;
				}
			}

			// exception?
			if (bHasEncounteredSomeException)
			{
				return EItemAnalyzeStatus::ExceptionOccurred;
			}

			// item got discarded?
			if (bDiscardedByAtLeastOneEvaluator)
			{
				return EItemAnalyzeStatus::DiscardedByAtLeastOneEvaluator;
			}

			// disqualified due to bad score?
			if (itemToAnalyze.bDisqualifiedDueToBadScore)
			{
				if (bAllEvaluatorsHaveRun)
				{
					return EItemAnalyzeStatus::DisqualifiedDueToBadScoreAfterAllEvaluatorsHadRun;
				}
				else
				{
					return EItemAnalyzeStatus::DisqualifiedDueToBadScoreBeforeAllEvaluatorsHadRun;
				}
			}

			// item still being evaluated?
			if (!bAllEvaluatorsHaveRun)
			{
				return EItemAnalyzeStatus::StillBeingEvaluated;
			}

			return EItemAnalyzeStatus::SurvivedAllEvaluators;
		}

		CTimeValue CHistoricQuery::ComputeElapsedTimeFromQueryCreationToDestruction() const
		{
			// elapsed time
			switch (m_queryLifetimeStatus)
			{
			case EQueryLifetimeStatus::QueryIsNotCreatedYet:
				return CTimeValue();

			case EQueryLifetimeStatus::QueryIsAlive:
				return (gEnv->pTimer->GetAsyncTime() - m_queryCreatedTimestamp);

			case EQueryLifetimeStatus::QueryIsDestroyed:
				return (m_queryDestroyedTimestamp - m_queryCreatedTimestamp);

			default:
				assert(0);
				return CTimeValue();
			}
		}

		bool CHistoricQuery::FindClosestItemInView(const SDebugCameraView& cameraView, size_t& outItemIndex) const
		{
			float closestDistanceSoFar = std::numeric_limits<float>::max();
			bool bFoundACloseEnoughItem = false;

			for (size_t i = 0, n = m_items.size(); i < n; ++i)
			{
				const SHistoricItem& item = m_items[i];

				if (item.pDebugProxy)
				{
					float dist;

					if (item.pDebugProxy->GetDistanceToCameraView(cameraView, dist) && dist < closestDistanceSoFar)
					{
						closestDistanceSoFar = dist;
						outItemIndex = i;
						bFoundACloseEnoughItem = true;
					}
				}
			}
			return bFoundACloseEnoughItem;
		}

		static ColorF ScoreToColor(float bestScoreAmongAllItems, float worstScoreAmongAllItems, float accumulatedAndWeightedScoreOfMaskedEvaluators)
		{
			const float range = bestScoreAmongAllItems - worstScoreAmongAllItems;
			const float itemRelativeScore = accumulatedAndWeightedScoreOfMaskedEvaluators - worstScoreAmongAllItems;
			const float fraction = (range > FLT_EPSILON) ? itemRelativeScore / range : 1.0f;
			return Lerp(Col_Red, Col_Green, fraction);
		}

		void CHistoricQuery::DrawDebugPrimitivesInWorld(size_t indexOfItemCurrentlyBeingFocused, const IQueryHistoryManager::SEvaluatorDrawMasks& evaluatorDrawMasks) const
		{
			m_debugRenderWorldPersistent.DrawAllAddedPrimitivesWithNoItemAssociation();

			//
			// - figure out the best and worst score of items that made it into the final result set
			// - we need these to get a "relative fitness" for each item do draw it with gradating color
			//

			float bestScoreAmongAllItems = std::numeric_limits<float>::min();
			float worstScoreAmongAllItems = std::numeric_limits<float>::max();

			for (const SHistoricItem& itemToAnalyze : m_items)
			{
				float accumulatedAndWeightedScoreOfMaskedEvaluators = 0.0f;
				bool bFoundScoreOutsideValidRange = false;

				const EItemAnalyzeStatus status = AnalyzeItemStatus(itemToAnalyze, evaluatorDrawMasks, accumulatedAndWeightedScoreOfMaskedEvaluators, bFoundScoreOutsideValidRange);

				if (status == EItemAnalyzeStatus::ExceptionOccurred)
					continue;

				if (status == EItemAnalyzeStatus::DiscardedByAtLeastOneEvaluator)
					continue;

				if (status == EItemAnalyzeStatus::DisqualifiedDueToBadScoreBeforeAllEvaluatorsHadRun)
					continue;

				// accept all remaining item stati

				assert(status == EItemAnalyzeStatus::DisqualifiedDueToBadScoreAfterAllEvaluatorsHadRun || status == EItemAnalyzeStatus::StillBeingEvaluated || status == EItemAnalyzeStatus::SurvivedAllEvaluators);

				//
				// update the range of best/worst score
				//

				bestScoreAmongAllItems = std::max(bestScoreAmongAllItems, accumulatedAndWeightedScoreOfMaskedEvaluators);
				worstScoreAmongAllItems = std::min(worstScoreAmongAllItems, accumulatedAndWeightedScoreOfMaskedEvaluators);
			}

			//
			// draw all items in the debug-renderworld
			//

			const float alphaValueForItemsInTheFinalResultSet = 1.0f;
			const float alphaValueForDiscardedItems = (float)crymath::clamp(SCvars::debugDrawAlphaValueOfDiscardedItems, 0, 255) / 255.0f;

			for (size_t i = 0, n = m_items.size(); i < n; ++i)
			{
				const SHistoricItem& item = m_items[i];
				const bool bShowDetails = (indexOfItemCurrentlyBeingFocused == i);

				ColorF color = Col_Black;
				bool bDrawScore = false;
				bool bShouldDrawAnExclamationMarkAsWarning = false;

				float accumulatedAndWeightedScoreOfMaskedEvaluators = 0.0f;
				bool bFoundItemScoreOutsideValidRange = false;

				const EItemAnalyzeStatus status = AnalyzeItemStatus(item, evaluatorDrawMasks, accumulatedAndWeightedScoreOfMaskedEvaluators, bFoundItemScoreOutsideValidRange);

				if (bFoundItemScoreOutsideValidRange)
				{
					bShouldDrawAnExclamationMarkAsWarning = true;
				}

				switch (status)
				{
				case EItemAnalyzeStatus::ExceptionOccurred:
					bShouldDrawAnExclamationMarkAsWarning = true;
					color = Col_Black;
					color.a = alphaValueForDiscardedItems;
					break;

				case EItemAnalyzeStatus::DiscardedByAtLeastOneEvaluator:
					color = Col_Black;
					color.a = alphaValueForDiscardedItems;
					break;

				case EItemAnalyzeStatus::DisqualifiedDueToBadScoreBeforeAllEvaluatorsHadRun:
					color = Col_Plum;
					color.a = alphaValueForDiscardedItems;
					break;

				case EItemAnalyzeStatus::StillBeingEvaluated:
					color = Col_Yellow;
					color.a = alphaValueForDiscardedItems;  // FIXME: this alpha value is actually for finished items that did not survive (and not for items still being evaluated)
					break;

				case EItemAnalyzeStatus::DisqualifiedDueToBadScoreAfterAllEvaluatorsHadRun:
					color = ScoreToColor(bestScoreAmongAllItems, worstScoreAmongAllItems, accumulatedAndWeightedScoreOfMaskedEvaluators);
					color.a = alphaValueForDiscardedItems;
					bDrawScore = true;
					break;

				case EItemAnalyzeStatus::SurvivedAllEvaluators:
					color = ScoreToColor(bestScoreAmongAllItems, worstScoreAmongAllItems, accumulatedAndWeightedScoreOfMaskedEvaluators);
					color.a = alphaValueForItemsInTheFinalResultSet;
					bDrawScore = true;
					break;

				default:
					assert(0);
				}

				m_debugRenderWorldPersistent.DrawAllAddedPrimitivesAssociatedWithItem(i, evaluatorDrawMasks, color, bShowDetails);

				if (item.pDebugProxy)
				{
					if (bDrawScore)
					{
						stack_string text;
						text.Format("%f", accumulatedAndWeightedScoreOfMaskedEvaluators);
						CDebugRenderPrimitive_Text::Draw(item.pDebugProxy->GetPivot(), 1.5f, text.c_str(), color, false);
					}

					if (bShowDetails)
					{
						// # item index
						stack_string text;
						text.Format("#%i", (int)i);
						CDebugRenderPrimitive_Text::Draw(item.pDebugProxy->GetPivot() + Vec3(0, 0, 1), 1.5f, text.c_str(), color, false);
					}

					if (bShouldDrawAnExclamationMarkAsWarning)
					{
						// big exclamation mark to grab the user's attention that something is wrong and that he should inspect that particular item in detail

						float textSize = 5.0f;

						if (bShowDetails)
						{
							// pulsate the text
							textSize *= CDebugRenderPrimitiveBase::Pulsate();
						}

						CDebugRenderPrimitive_Text::Draw(item.pDebugProxy->GetPivot() + Vec3(0, 0, 1), textSize, "!", Col_Red, false);
					}
				}
			}
		}

		SDebugCameraView CHistoricQuery::GetIdealDebugCameraView(const SDebugCameraView& currentCameraView) const
		{
			// put the direction flat on the x/y plane
			Vec3 dir = currentCameraView.dir;
			dir.z = 0.0f;
			dir.NormalizeSafe(Vec3(0, 1, 0));

			const Vec3 zAxis(0, 0, 1);
			const Vec3 xAxis = dir.Cross(zAxis).GetNormalized();
			const Matrix34 localTransform = Matrix34::CreateFromVectors(xAxis, dir, zAxis, currentCameraView.pos).GetInverted();
			AABB localExtents(AABB::RESET);

			for (const SHistoricItem& item : m_items)
			{
				if (const CItemDebugProxyBase* pDebugProxy = item.pDebugProxy.get())
				{
					Vec3 itemPivot = localTransform * pDebugProxy->GetPivot();
					localExtents.Add(itemPivot);
				}
			}

			SDebugCameraView suggestedCameraView = currentCameraView;

			if (!localExtents.IsReset())
			{
				suggestedCameraView.pos.x = (localExtents.max.x + localExtents.min.x) * 0.5f;
				suggestedCameraView.pos.y = localExtents.min.y - 3.0f;	// 3 meters behind the closest item
				suggestedCameraView.pos.z = localExtents.max.z + 3.0f;	// 3 meters above the highest item
				suggestedCameraView.pos = localTransform.GetInverted() * suggestedCameraView.pos;
			}

			return suggestedCameraView;
		}

		void CHistoricQuery::FillQueryHistoryConsumerWithShortInfoAboutQuery(IQueryHistoryConsumer& consumer, bool bHighlight) const
		{
			ColorF color;
			if (m_bExceptionOccurred)  // TODO: we should also draw in red when some items received invalid scores (outside [0.0 .. 1.0])
			{
				color = bHighlight ? Col_Red : Col_DeepPink;
			}
			else if (!m_warningMessages.empty())
			{
				color = bHighlight ? Col_OrangeRed : Col_Orange;
			}
			else
			{
				color = bHighlight ? Col_Cyan : Col_White;
			}

			const size_t numGeneratedItems = m_finalStatistics.numGeneratedItems;

			// tentative: - the actual items may have come from a child query whereas the parent query didn't even generate items
			//            - so we rely on CQuery_SequentialBase::OnGetStatistics() to have counted the final items from the result set of one of its child queries
			const size_t numItemsInFinalResultSet = m_finalStatistics.numItemsInFinalResultSet;

			const CTimeValue elapsedTime = ComputeElapsedTimeFromQueryCreationToDestruction();
			const bool bFoundTooFewItems = (m_finalStatistics.numItemsInFinalResultSet == 0) || (m_finalStatistics.numItemsInFinalResultSet < m_finalStatistics.numDesiredItems);
			const bool bEncounteredSomeWarnings = !m_warningMessages.empty();
			const IQueryHistoryConsumer::SHistoricQueryOverview overview(
				color,
				m_querierName.c_str(),
				m_queryID,
				m_parentQueryID,
				m_queryBlueprintName.c_str(),
				numGeneratedItems,
				numItemsInFinalResultSet,
				elapsedTime,
				m_queryCreatedTimestamp,
				m_queryDestroyedTimestamp,
				bFoundTooFewItems,
				m_bExceptionOccurred,
				bEncounteredSomeWarnings
			);
			consumer.AddOrUpdateHistoricQuery(overview);
		}

		void CHistoricQuery::FillQueryHistoryConsumerWithDetailedInfoAboutQuery(IQueryHistoryConsumer& consumer) const
		{
			const ColorF color = Col_White;

			// query ID
			{
				Shared::CUqsString queryIdAsString;
				m_queryID.ToString(queryIdAsString);
				consumer.AddTextLineToCurrentHistoricQuery(color, "=== Query #%s ===", queryIdAsString.c_str());
			}

			// querier name + query blueprint name
			{
				consumer.AddTextLineToCurrentHistoricQuery(color, "'%s' / '%s'", m_finalStatistics.querierName.c_str(), m_finalStatistics.queryBlueprintName.c_str());
			}

			// exception message
			{
				if (m_bExceptionOccurred)
				{
					consumer.AddTextLineToCurrentHistoricQuery(Col_Red, "Exception: %s", m_exceptionMessage.c_str());
				}
				else
				{
					consumer.AddTextLineToCurrentHistoricQuery(color, "No exception");
				}
			}

			// warning messages
			{
				if (m_warningMessages.empty())
				{
					consumer.AddTextLineToCurrentHistoricQuery(color, "No warnings");
				}
				else
				{
					consumer.AddTextLineToCurrentHistoricQuery(Col_Orange, "%i warnings:", (int)m_warningMessages.size());
					for (const string& warningMessage : m_warningMessages)
					{
						consumer.AddTextLineToCurrentHistoricQuery(Col_Orange, "%s", warningMessage.c_str());
					}
				}
			}

			// elapsed frames and time, and timestamps of creation + destruction of the query
			{
				// elapsed frames
				consumer.AddTextLineToCurrentHistoricQuery(color, "elapsed frames until result:  %i", (int)m_finalStatistics.totalElapsedFrames);

				// elapsed time (this is NOT the same as the *consumed* time)
				const float elapsedTimeInMS = ComputeElapsedTimeFromQueryCreationToDestruction().GetMilliSeconds();
				consumer.AddTextLineToCurrentHistoricQuery(color, "elapsed seconds until result: %f (%.2f milliseconds)", elapsedTimeInMS / 1024.0f, elapsedTimeInMS);

				// consumed time (this is the accumulation of the granted and consumed amounts of time per update call while the query was running)
				consumer.AddTextLineToCurrentHistoricQuery(color, "consumed seconds:             %f (%.2f milliseconds)", m_finalStatistics.totalConsumedTime.GetSeconds(), m_finalStatistics.totalConsumedTime.GetMilliSeconds());

				// timestamps of when the query was created and destroyed (notice: if the query was canceled prematurely it will miss the timestamp of query destruction)
				// -> "h:mm:ss:mmm"

				int hours, minutes, seconds, milliseconds;

				UQS::Shared::CTimeValueUtil::Split(m_queryCreatedTimestamp, &hours, &minutes, &seconds, &milliseconds);
				consumer.AddTextLineToCurrentHistoricQuery(color, "timestamp query created:      %i:%02i:%02i:%03i", hours, minutes, seconds, milliseconds);

				UQS::Shared::CTimeValueUtil::Split(m_queryDestroyedTimestamp, &hours, &minutes, &seconds, &milliseconds);
				consumer.AddTextLineToCurrentHistoricQuery(color, "timestamp query destroyed:    %i:%02i:%02i:%03i", hours, minutes, seconds, milliseconds);
			}

			// canceled: yes/no
			{
				consumer.AddTextLineToCurrentHistoricQuery(color, "canceled prematurely:         %s", m_bGotCanceledPrematurely ? "YES" : "NO");
			}

			// number of desired items, generated items, remaining items to inspect, final items
			{
				static const ColorF warningColor = Col_Yellow;

				const bool bHasDesiredItemLimit = (m_finalStatistics.numDesiredItems > 0);
				const bool bFoundTooFewItems = (m_finalStatistics.numItemsInFinalResultSet == 0) || (m_finalStatistics.numItemsInFinalResultSet < m_finalStatistics.numDesiredItems);

				const ColorF& colorForItemsGenerated = (m_finalStatistics.numGeneratedItems == 0) ? warningColor : color;
				const ColorF& colorForDesiredItems = (bHasDesiredItemLimit && bFoundTooFewItems) ? warningColor : color;
				const ColorF& colorForItemsInFinalResultSet = bFoundTooFewItems ? warningColor : color;

				// desired items
				consumer.AddTextLineToCurrentHistoricQuery(colorForDesiredItems,          "items desired:                %s", (m_finalStatistics.numDesiredItems < 1) ? "(no limit)" : stack_string().Format("%i", (int)m_finalStatistics.numDesiredItems).c_str());

				// generated items
				consumer.AddTextLineToCurrentHistoricQuery(colorForItemsGenerated,        "items generated:              %i", (int)m_finalStatistics.numGeneratedItems);

				// remaining items to inspect
				consumer.AddTextLineToCurrentHistoricQuery(color,                         "items still to inspect:       %i", (int)m_finalStatistics.numRemainingItemsToInspect);

				// final items
				consumer.AddTextLineToCurrentHistoricQuery(colorForItemsInFinalResultSet, "final items:                  %i", (int)m_finalStatistics.numItemsInFinalResultSet);
			}

			// memory usage
			{
				// items
				consumer.AddTextLineToCurrentHistoricQuery(color, "items memory:                 %i (%i kb)", (int)m_finalStatistics.memoryUsedByGeneratedItems, (int)m_finalStatistics.memoryUsedByGeneratedItems / 1024);

				// working data of items
				consumer.AddTextLineToCurrentHistoricQuery(color, "items working data memory:    %i (%i kb)", (int)m_finalStatistics.memoryUsedByItemsWorkingData, (int)m_finalStatistics.memoryUsedByItemsWorkingData / 1024);
			}

			// Instant-Evaluators
			{
				const size_t numInstantEvaluators = m_finalStatistics.instantEvaluatorsRuns.size();
				for (size_t i = 0; i < numInstantEvaluators; ++i)
				{
					consumer.AddTextLineToCurrentHistoricQuery(color, "Instant-Evaluator #%i:  full runs = %i", (int)i, (int)m_finalStatistics.instantEvaluatorsRuns[i]);
				}
			}

			// Deferred-Evaluators
			{
				const size_t numDeferredEvaluators = m_finalStatistics.deferredEvaluatorsFullRuns.size();
				for (size_t i = 0; i < numDeferredEvaluators; ++i)
				{
					consumer.AddTextLineToCurrentHistoricQuery(color, "Deferred-Evaluator #%i: full runs = %i, aborted runs = %i", (int)i, (int)m_finalStatistics.deferredEvaluatorsFullRuns[i], (int)m_finalStatistics.deferredEvaluatorsAbortedRuns[i]);
				}
			}

			// Phases
			{
				const size_t numPhases = m_finalStatistics.elapsedTimePerPhase.size();
				for (size_t i = 0; i < numPhases; ++i)
				{
					// print a human-readable phase index
					consumer.AddTextLineToCurrentHistoricQuery(color, "Phase '%i'  = %i frames, %f sec (%.2f ms) [longest call = %f sec (%.2f ms)]",
						(int)i + 1,
						(int)m_finalStatistics.elapsedFramesPerPhase[i],
						m_finalStatistics.elapsedTimePerPhase[i].GetSeconds(),
						m_finalStatistics.elapsedTimePerPhase[i].GetSeconds() * 1000.0f,
						m_finalStatistics.peakElapsedTimePerPhaseUpdate[i].GetSeconds(),
						m_finalStatistics.peakElapsedTimePerPhaseUpdate[i].GetSeconds() * 1000.0f);
				}
			}
		}

		void CHistoricQuery::FillQueryHistoryConsumerWithDetailedInfoAboutItem(IQueryHistoryConsumer& consumer, size_t itemIndex) const
		{
			assert(itemIndex < m_items.size());

			const SHistoricItem& item = m_items[itemIndex];

			// track which instant- and deferred-evaluators were implemented wrongly and gave the item a score outside the valid range [0.0 .. 1.0]
			std::vector<size_t> indexesOfInstantEvaluatorsWithIllegalScores;
			std::vector<size_t> indexesOfDeferredEvaluatorsWithIllegalScores;

			// item #
			consumer.AddTextLineToFocusedItem(Col_White, "--- item #%i ---", (int)itemIndex);

			// empty line
			consumer.AddTextLineToFocusedItem(Col_White, "");

			// item score
			consumer.AddTextLineToFocusedItem(Col_White, "final score: %f", item.accumulatedAndWeightedScoreSoFar);
			consumer.AddTextLineToFocusedItem(Col_White, "disqualified due to bad score: %s", item.bDisqualifiedDueToBadScore ? "yes" : "no");

			// empty line
			consumer.AddTextLineToFocusedItem(Col_White, "");

			// instant evaluators
			for (size_t i = 0, n = item.resultOfAllInstantEvaluators.size(); i < n; ++i)
			{
				const SHistoricInstantEvaluatorResult& ieResult = item.resultOfAllInstantEvaluators[i];
				ColorF ieColor = Col_White;
				stack_string text = "?";

				switch (ieResult.status)
				{
				case SHistoricInstantEvaluatorResult::EStatus::HasNotRunYet:
					text = "has not run yet";
					ieColor = Col_Plum;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem:
					text = "discarded the item";
					ieColor = Col_Red;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::HasFinishedAndScoredTheItem:
					text.Format("scored the item: %f", ieResult.nonWeightedScore);
					ieColor = Col_Green;
					if (ieResult.nonWeightedScore < -FLT_EPSILON || ieResult.nonWeightedScore > 1.0f + FLT_EPSILON)
					{
						indexesOfInstantEvaluatorsWithIllegalScores.push_back(i);
					}
					break;

				case SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall:
					text.Format("exception in function call: %s", ieResult.furtherInformationAboutStatus.c_str());
					ieColor = Col_Red;
					break;

				case SHistoricInstantEvaluatorResult::EStatus::ExceptionOccurredInHimself:
					text.Format("exception in himself: %s", ieResult.furtherInformationAboutStatus.c_str());
					ieColor = Col_Red;
					break;
				}

				consumer.AddTextLineToFocusedItem(ieColor, "IE #%i [%-*s]: %s", (int)i, (int)m_longestEvaluatorName, m_instantEvaluatorNames[i].c_str(), text.c_str());
			}

			// empty line
			consumer.AddTextLineToFocusedItem(Col_White, "");

			// deferred evaluators
			for (size_t i = 0, n = item.resultOfAllDeferredEvaluators.size(); i < n; ++i)
			{
				const SHistoricDeferredEvaluatorResult& deResult = item.resultOfAllDeferredEvaluators[i];
				ColorF deColor = Col_White;
				stack_string text = "?";

				switch (deResult.status)
				{
				case SHistoricDeferredEvaluatorResult::EStatus::HasNotRunYet:
					text = "has not run yet";
					deColor = Col_Plum;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::IsRunningNow:
					text = "running now";
					deColor = Col_Yellow;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndDiscardedTheItem:
					text = "discarded the item";
					deColor = Col_Red;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::HasFinishedAndScoredTheItem:
					text.Format("scored the item: %f", deResult.nonWeightedScore);
					deColor = Col_Green;
					if (deResult.nonWeightedScore < -FLT_EPSILON || deResult.nonWeightedScore > 1.0f + FLT_EPSILON)
					{
						indexesOfDeferredEvaluatorsWithIllegalScores.push_back(i);
					}
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::GotAborted:
					text.Format("got aborted prematurely: %s", deResult.furtherInformationAboutStatus.c_str());
					deColor = Col_DarkGray;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInFunctionCall:
					text.Format("exception in function call: %s", deResult.furtherInformationAboutStatus.c_str());
					deColor = Col_Red;
					break;

				case SHistoricDeferredEvaluatorResult::EStatus::ExceptionOccurredInHimself:
					text.Format("exception in himself: %s", deResult.furtherInformationAboutStatus.c_str());
					deColor = Col_Red;
					break;
				}

				consumer.AddTextLineToFocusedItem(deColor, "DE #%i [%-*s]: %s", (int)i, (int)m_longestEvaluatorName, m_deferredEvaluatorNames[i].c_str(), text.c_str());
			}

			// print the evaluators that gave scores outside the valid range [0.0 .. 1.0]
			if (!indexesOfInstantEvaluatorsWithIllegalScores.empty() || !indexesOfDeferredEvaluatorsWithIllegalScores.empty())
			{
				// empty line
				consumer.AddTextLineToFocusedItem(Col_White, "");

				consumer.AddTextLineToFocusedItem(Col_Red, "WARNING: these evaluators gave an illegal score (scores must always be in [0.0 .. 1.0]):");

				// empty line
				consumer.AddTextLineToFocusedItem(Col_White, "");

				for (size_t i = 0, n = indexesOfInstantEvaluatorsWithIllegalScores.size(); i < n; ++i)
				{
					const size_t index = indexesOfInstantEvaluatorsWithIllegalScores[i];
					const char* szIEName = m_instantEvaluatorNames[index];
					const SHistoricInstantEvaluatorResult& ieResult = item.resultOfAllInstantEvaluators[index];
					consumer.AddTextLineToFocusedItem(Col_Red, "IE #%i [%-*s]: %f", (int)index, (int)m_longestEvaluatorName, szIEName, ieResult.nonWeightedScore);
				}

				for (size_t i = 0, n = indexesOfDeferredEvaluatorsWithIllegalScores.size(); i < n; ++i)
				{
					const size_t index = indexesOfDeferredEvaluatorsWithIllegalScores[i];
					const char* szDEName = m_deferredEvaluatorNames[index];
					const SHistoricDeferredEvaluatorResult& deResult = item.resultOfAllDeferredEvaluators[index];
					consumer.AddTextLineToFocusedItem(Col_Red, "DE #%i [%-*s]: %f", (int)index, (int)m_longestEvaluatorName, szDEName, deResult.nonWeightedScore);
				}
			}
		}

		void CHistoricQuery::FillQueryHistoryConsumerWithInstantEvaluatorNames(IQueryHistoryConsumer& consumer) const
		{
			for (const string& ieName : m_instantEvaluatorNames)
			{
				consumer.AddInstantEvaluatorName(ieName.c_str());
			}
		}

		void CHistoricQuery::FillQueryHistoryConsumerWithDeferredEvaluatorNames(IQueryHistoryConsumer& consumer) const
		{
			for (const string& deName : m_deferredEvaluatorNames)
			{
				consumer.AddDeferredEvaluatorName(deName.c_str());
			}
		}

		const CQueryID& CHistoricQuery::GetQueryID() const
		{
			return m_queryID;
		}

		const CQueryID& CHistoricQuery::GetParentQueryID() const
		{
			return m_parentQueryID;
		}

		bool CHistoricQuery::IsQueryDestroyed() const
		{
			return m_queryLifetimeStatus == EQueryLifetimeStatus::QueryIsDestroyed;
		}

		const CTimeValue& CHistoricQuery::GetQueryDestroyedTimestamp() const
		{
			return m_queryDestroyedTimestamp;
		}

		void CHistoricQuery::Serialize(Serialization::IArchive& ar)
		{
			ar(m_queryID, "m_queryID");
			ar(m_parentQueryID, "m_parentQueryID");
			ar(m_querierName, "m_querierName");
			ar(m_queryBlueprintName, "m_queryBlueprintName");
			ar(m_queryLifetimeStatus, "m_queryLifetimeStatus");
			ar(m_queryCreatedTimestamp, "m_queryCreatedTimestamp");
			ar(m_queryDestroyedTimestamp, "m_queryDestroyedTimestamp");
			ar(m_bGotCanceledPrematurely, "m_bGotCanceledPrematurely");
			ar(m_bExceptionOccurred, "m_bExceptionOccurred");
			ar(m_exceptionMessage, "m_exceptionMessage");
			ar(m_warningMessages, "m_warningMessages");
			ar(m_debugRenderWorldPersistent, "m_debugRenderWorldPersistent");
			ar(m_items, "m_items");
			ar(m_instantEvaluatorNames, "m_instantEvaluatorNames");
			ar(m_deferredEvaluatorNames, "m_deferredEvaluatorNames");
			ar(m_longestEvaluatorName, "m_longestEvaluatorName");
			ar(m_finalStatistics, "m_finalStatistics");
			ar(m_itemDebugProxyFactory, "m_itemDebugProxyFactory");		// notice: it wouldn't even be necessary to serialize the CItemDebugProxyFactory as its only member variables is just used for intermediate storage and will always be set to its default value at the time of serialization
		}

		//===================================================================================
		//
		// CQueryHistory
		//
		//===================================================================================

		CQueryHistory::CQueryHistory()
		{
			// nothing
		}

		CQueryHistory& CQueryHistory::operator=(CQueryHistory&& other)
		{
			if (this != &other)
			{
				m_history = std::move(other.m_history);
				m_metaData = std::move(other.m_metaData);
			}
			return *this;
		}

		HistoricQuerySharedPtr CQueryHistory::AddNewHistoryEntry(const CQueryID& queryID, const char* szQuerierName, const CQueryID& parentQueryID, CQueryHistoryManager* pOwningHistoryManager)
		{
			//
			// insert the new historic query into the correct position in the array such that children will always reside somewhere after their parent
			// (this gives us the desired depth-first traversal when scrolling through all historic queries)
			//

			auto insertPos = m_history.end();

			if (parentQueryID.IsValid())
			{
				// search backwards from the end as this will find the insert position quicker
				// (this leverages the fact that child queries will often get started at roughly the same time as their parent, thus having them reside quite at the end)
				for (auto rit = m_history.rbegin(), rendIt = m_history.rend(); rit != rendIt; ++rit)
				{
					const CHistoricQuery* pCurrentHistoricQuery = rit->get();

					// found a child of our parent or our parent itself? => insert after this query then
					if (pCurrentHistoricQuery->GetParentQueryID() == parentQueryID || pCurrentHistoricQuery->GetQueryID() == parentQueryID)
					{
						insertPos = rit.base();  // notice: this conversion from reverse- to forward-iterator will make insertPos point to exactly where we want to insert the new historic query
						break;
					}
				}
			}

			HistoricQuerySharedPtr pNewEntry(new CHistoricQuery(queryID, szQuerierName, parentQueryID, pOwningHistoryManager));
			insertPos = m_history.insert(insertPos, std::move(pNewEntry));
			return *insertPos;
		}

		void CQueryHistory::Clear()
		{
			m_history.clear();
			// keep m_metaData
		}

		size_t CQueryHistory::GetHistorySize() const
		{
			return m_history.size();
		}

		const CHistoricQuery& CQueryHistory::GetHistoryEntryByIndex(size_t index) const
		{
			assert(index < m_history.size());
			return *m_history[index];
		}

		const CHistoricQuery* CQueryHistory::FindHistoryEntryByQueryID(const CQueryID& queryID) const
		{
			for (const HistoricQuerySharedPtr& pHistoricEntry : m_history)
			{
				if (pHistoricEntry->GetQueryID() == queryID)
				{
					return pHistoricEntry.get();
				}
			}
			return nullptr;
		}

		size_t CQueryHistory::GetRoughMemoryUsage() const
		{
			size_t memoryUsed = 0;

			for (const HistoricQuerySharedPtr& q : m_history)
			{
				memoryUsed += q->GetRoughMemoryUsage();
			}

			return memoryUsed;
		}

		void CQueryHistory::SetArbitraryMetaDataForSerialization(const char* szKey, const char* szValue)
		{
			m_metaData[szKey] = szValue;
		}

		void CQueryHistory::Serialize(Serialization::IArchive& ar)
		{
			ar(m_metaData, "m_metaData");
			ar(m_history, "m_history");
		}

	}
}
