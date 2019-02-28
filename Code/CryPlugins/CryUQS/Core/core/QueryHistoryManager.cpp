// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CQueryHistoryManager::CQueryHistoryManager()
			: m_queryIDOfCurrentHistoricQuery{ CQueryID::CreateInvalid(), CQueryID::CreateInvalid() }
			, m_historyToManage(EHistoryOrigin::Live)
			, m_indexOfFocusedItem(s_noItemFocusedIndex)
			, m_bAutomaticUpdateDebugRendering3DInProgress(false)
		{
			// nothing
		}

		void CQueryHistoryManager::RegisterQueryHistoryListener(IQueryHistoryListener* pListener)
		{
			assert(pListener);
			stl::push_back_unique(m_listeners, pListener);
		}

		void CQueryHistoryManager::UnregisterQueryHistoryListener(IQueryHistoryListener* pListener)
		{
			stl::find_and_erase(m_listeners, pListener);
		}

		void CQueryHistoryManager::UpdateDebugRendering3D(const SDebugCameraView* pOptionalView, const SEvaluatorDrawMasks& evaluatorDrawMasks)
		{
			// - if this assert fails, then the game code tries to do the debug-render when it hasn't declared to do so
			// - this check is done to prevent updating from more than one place
			assert(gEnv->IsEditing() || (m_bAutomaticUpdateDebugRendering3DInProgress == !g_pHub->GetOverrideFlags().Check(EHubOverrideFlags::CallUpdateDebugRendering3D)));

			const CQueryHistory& history = m_queryHistories[m_historyToManage];
			const CQueryID& queryIdOfSelectedHistoricQuery = m_queryIDOfCurrentHistoricQuery[m_historyToManage];
			const CTimeValue now = gEnv->pTimer->GetAsyncTime();

			//
			// if debug-drawing is enabled, then draw all ongoing queries in the 3D world while they haven't been finished (destroyed) yet
			//

			if (SCvars::debugDraw)
			{
				for (size_t i = 0, n = history.GetHistorySize(); i < n; ++i)
				{
					const CHistoricQuery& query = history.GetHistoryEntryByIndex(i);

					// skip the query that we've selected for permanent drawing (we'll draw it separately outside this loop)
					if (query.GetQueryID() == queryIdOfSelectedHistoricQuery)
						continue;

					// skip this query if it's finished and too old (we'll draw finished queries for just a short moment beyond their lifetime)
					if (query.IsQueryDestroyed() && (now - query.GetQueryDestroyedTimestamp() > 2.0f))
						continue;

					query.DrawDebugPrimitivesInWorld(CDebugRenderWorldPersistent::kIndexWithoutAssociation, SEvaluatorDrawMasks::CreateAllBitsSet());
				}
			}

			//
			// - have the currently selected historic query draw its debug primitives in the world (this is independent of whether debug-drawing is enabled or not)
			// - figure out what the currently focused item by the camera is
			//

			if (queryIdOfSelectedHistoricQuery.IsValid())
			{
				if (const CHistoricQuery* pHistoricQueryToDraw = history.FindHistoryEntryByQueryID(queryIdOfSelectedHistoricQuery))
				{
					const size_t indexOfPreviouslyFocusedItem = m_indexOfFocusedItem;
					const bool bCurrentlyFocusingAnItem = pOptionalView && pHistoricQueryToDraw->FindClosestItemInView(*pOptionalView, m_indexOfFocusedItem);

					if (bCurrentlyFocusingAnItem)
					{
						pHistoricQueryToDraw->DrawDebugPrimitivesInWorld(m_indexOfFocusedItem, evaluatorDrawMasks);
					}
					else
					{
						m_indexOfFocusedItem = s_noItemFocusedIndex;
						pHistoricQueryToDraw->DrawDebugPrimitivesInWorld(CDebugRenderWorldPersistent::kIndexWithoutAssociation, evaluatorDrawMasks);
					}

					if (m_indexOfFocusedItem != indexOfPreviouslyFocusedItem)
					{
						NotifyListeners(IQueryHistoryListener::EEventType::FocusedItemChanged);
					}
				}
			}
		}

		bool CQueryHistoryManager::SerializeLiveQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error)
		{
			//
			// add some meta data to the live history before serializing it
			//

			{
				// add date + time as meta data
				char dateAndTimeAsString[1024] = "";
				time_t ltime;
				if (time(&ltime) != (time_t)-1)
				{
					if (struct tm* pTm = localtime(&ltime))
					{
						strftime(dateAndTimeAsString, CRY_ARRAY_COUNT(dateAndTimeAsString), "%Y-%m-%d %H:%M:%S", pTm);
					}
				}
				m_queryHistories[EHistoryOrigin::Live].SetArbitraryMetaDataForSerialization("serializationDateTime", dateAndTimeAsString);

				// TODO: could add more meta data (e. g. engine version)
			}

			//
			// serialize the query history
			//

			Serialization::IArchiveHost* pArchiveHost = gEnv->pSystem->GetArchiveHost();
			if (pArchiveHost->SaveXmlFile(szXmlFilePath, Serialization::SStruct(m_queryHistories[EHistoryOrigin::Live]), "UQSQueryHistory"))
			{
				return true;
			}
			else
			{
				error.Format("Could not serialize the live query history to xml file '%s' (Serialization::IArchiveHost::SaveXmlFile() failed for some reason)", szXmlFilePath);
				return false;
			}
		}

		bool CQueryHistoryManager::DeserializeQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error)
		{
			CQueryHistory tempQueryHistory;

			Serialization::IArchiveHost* pArchiveHost = gEnv->pSystem->GetArchiveHost();
			if (pArchiveHost->LoadXmlFile(Serialization::SStruct(tempQueryHistory), szXmlFilePath))
			{
				m_queryHistories[EHistoryOrigin::Deserialized] = std::move(tempQueryHistory);
				m_queryIDOfCurrentHistoricQuery[EHistoryOrigin::Deserialized] = CQueryID::CreateInvalid();
				if (m_historyToManage == EHistoryOrigin::Deserialized)
				{
					m_indexOfFocusedItem = s_noItemFocusedIndex;
				}
				NotifyListeners(IQueryHistoryListener::EEventType::QueryHistoryDeserialized);
				return true;
			}
			else
			{
				error.Format("Could not de-serialize the query history from xml file '%s'", szXmlFilePath);
				return false;
			}
		}

		void CQueryHistoryManager::MakeQueryHistoryCurrent(EHistoryOrigin whichHistory)
		{
			if (whichHistory != m_historyToManage)
			{
				m_historyToManage = whichHistory;
				m_indexOfFocusedItem = s_noItemFocusedIndex;
				NotifyListeners(IQueryHistoryListener::EEventType::CurrentQueryHistorySwitched);
			}
		}

		IQueryHistoryManager::EHistoryOrigin CQueryHistoryManager::GetCurrentQueryHistory() const
		{
			return m_historyToManage;
		}

		void CQueryHistoryManager::ClearQueryHistory(EHistoryOrigin whichHistory)
		{
			m_queryHistories[whichHistory].Clear();
			m_queryIDOfCurrentHistoricQuery[whichHistory] = CQueryID::CreateInvalid();

			if (whichHistory == m_historyToManage)
			{
				m_indexOfFocusedItem = s_noItemFocusedIndex;
			}

			switch (whichHistory)
			{
			case EHistoryOrigin::Live:
				NotifyListeners(IQueryHistoryListener::EEventType::LiveQueryHistoryCleared);
				break;

			case EHistoryOrigin::Deserialized:
				NotifyListeners(IQueryHistoryListener::EEventType::DeserializedQueryHistoryCleared);
				break;

			default:
				assert(0);
			}
		}

		void CQueryHistoryManager::EnumerateSingleHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToEnumerate, IQueryHistoryConsumer& receiver) const
		{
			const CQueryHistory& history = m_queryHistories[whichHistory];

			if (const CHistoricQuery* pHistoricQuery = history.FindHistoryEntryByQueryID(queryIDToEnumerate))
			{
				const bool bHighlight = (whichHistory == m_historyToManage) && (m_queryIDOfCurrentHistoricQuery[whichHistory] == queryIDToEnumerate);

				pHistoricQuery->FillQueryHistoryConsumerWithShortInfoAboutQuery(receiver, bHighlight);
			}
		}

		void CQueryHistoryManager::EnumerateHistoricQueries(EHistoryOrigin whichHistory, IQueryHistoryConsumer& receiver) const
		{
			const CQueryHistory& historyToEnumerate = m_queryHistories[whichHistory];

			for (size_t i = 0, n = historyToEnumerate.GetHistorySize(); i < n; ++i)
			{
				const CHistoricQuery& historicQuery = historyToEnumerate.GetHistoryEntryByIndex(i);

				const bool bHighlight = (whichHistory == m_historyToManage) && (m_queryIDOfCurrentHistoricQuery[whichHistory] == historicQuery.GetQueryID());

				historicQuery.FillQueryHistoryConsumerWithShortInfoAboutQuery(receiver, bHighlight);
			}
		}

		void CQueryHistoryManager::MakeHistoricQueryCurrentForInWorldRendering(EHistoryOrigin whichHistory, const CQueryID& queryIDToMakeCurrent)
		{
			if (m_queryIDOfCurrentHistoricQuery[whichHistory] != queryIDToMakeCurrent)
			{
				m_queryIDOfCurrentHistoricQuery[whichHistory] = queryIDToMakeCurrent;
				m_indexOfFocusedItem = s_noItemFocusedIndex;
				NotifyListeners(IQueryHistoryListener::EEventType::DifferentHistoricQuerySelected);
			}
		}

		void CQueryHistoryManager::EnumerateInstantEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver)
		{
			if (const CHistoricQuery* pHistoricQuery = m_queryHistories[fromWhichHistory].FindHistoryEntryByQueryID(idOfQueryUsingTheseEvaluators))
			{
				pHistoricQuery->FillQueryHistoryConsumerWithInstantEvaluatorNames(receiver);
			}
		}

		void CQueryHistoryManager::EnumerateDeferredEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver)
		{
			if (const CHistoricQuery* pHistoricQuery = m_queryHistories[fromWhichHistory].FindHistoryEntryByQueryID(idOfQueryUsingTheseEvaluators))
			{
				pHistoricQuery->FillQueryHistoryConsumerWithDeferredEvaluatorNames(receiver);
			}
		}

		CQueryID CQueryHistoryManager::GetCurrentHistoricQueryForInWorldRendering(EHistoryOrigin fromWhichHistory) const
		{
			return m_queryIDOfCurrentHistoricQuery[fromWhichHistory];
		}

		void CQueryHistoryManager::GetDetailsOfHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToGetDetailsFor, IQueryHistoryConsumer& receiver) const
		{
			const CQueryHistory& history = m_queryHistories[whichHistory];

			if (const CHistoricQuery* pHistoricQuery = history.FindHistoryEntryByQueryID(queryIDToGetDetailsFor))
			{
				pHistoricQuery->FillQueryHistoryConsumerWithDetailedInfoAboutQuery(receiver);
			}
		}

		void CQueryHistoryManager::GetDetailsOfFocusedItem(IQueryHistoryConsumer& receiver) const
		{
			if (m_indexOfFocusedItem != s_noItemFocusedIndex)
			{
				const CQueryID& historicQueryID = m_queryIDOfCurrentHistoricQuery[m_historyToManage];
				if (historicQueryID.IsValid())
				{
					const CQueryHistory& history = m_queryHistories[m_historyToManage];
					if (const CHistoricQuery* pHistoricQuery = history.FindHistoryEntryByQueryID(historicQueryID))
					{
						pHistoricQuery->FillQueryHistoryConsumerWithDetailedInfoAboutItem(receiver, m_indexOfFocusedItem);
					}
				}
			}
		}

		size_t CQueryHistoryManager::GetRoughMemoryUsageOfQueryHistory(EHistoryOrigin whichHistory) const
		{
			return m_queryHistories[whichHistory].GetRoughMemoryUsage();
		}

		size_t CQueryHistoryManager::GetHistoricQueriesCount(EHistoryOrigin whichHistory) const
		{
			return m_queryHistories[whichHistory].GetHistorySize();
		}

		SDebugCameraView CQueryHistoryManager::GetIdealDebugCameraView(EHistoryOrigin whichHistory, const CQueryID& queryID, const SDebugCameraView& currentCameraView) const
		{
			if (const CHistoricQuery* pHistoricQuery = m_queryHistories[whichHistory].FindHistoryEntryByQueryID(queryID))
			{
				return pHistoricQuery->GetIdealDebugCameraView(currentCameraView);
			}
			else
			{
				return currentCameraView;
			}
		}

		HistoricQuerySharedPtr CQueryHistoryManager::AddNewLiveHistoricQuery(const CQueryID& queryID, const char* querierName, const CQueryID& parentQueryID)
		{
			HistoricQuerySharedPtr newHistoricQuery = m_queryHistories[IQueryHistoryManager::EHistoryOrigin::Live].AddNewHistoryEntry(queryID, querierName, parentQueryID, this);
			return newHistoricQuery;
		}

		void CQueryHistoryManager::UnderlyingQueryJustGotCreated(const CQueryID& queryID)
		{
			NotifyListeners(IQueryHistoryListener::EEventType::HistoricQueryJustGotCreatedInLiveQueryHistory, queryID);
		}

		void CQueryHistoryManager::UnderlyingQueryIsGettingDestroyed(const CQueryID& queryID)
		{
			NotifyListeners(IQueryHistoryListener::EEventType::HistoricQueryJustFinishedInLiveQueryHistory, queryID);
		}

		void CQueryHistoryManager::AutomaticUpdateDebugRendering3DBegin()
		{
			assert(!m_bAutomaticUpdateDebugRendering3DInProgress);
			m_bAutomaticUpdateDebugRendering3DInProgress = true;
		}

		void CQueryHistoryManager::AutomaticUpdateDebugRendering3DEnd()
		{
			assert(m_bAutomaticUpdateDebugRendering3DInProgress);
			m_bAutomaticUpdateDebugRendering3DInProgress = false;
		}

		void CQueryHistoryManager::NotifyListeners(IQueryHistoryListener::EEventType eventType) const
		{
			if (!m_listeners.empty())
			{
				NotifyListeners(eventType, CQueryID::CreateInvalid());
			}
		}

		void CQueryHistoryManager::NotifyListeners(IQueryHistoryListener::EEventType eventType, const CQueryID& relatedQueryID) const
		{
			if (!m_listeners.empty())
			{
				const IQueryHistoryListener::SEvent ev(eventType, relatedQueryID);

				for (IQueryHistoryListener* pListener : m_listeners)
				{
					pListener->OnQueryHistoryEvent(ev);
				}
			}
		}

	}
}
