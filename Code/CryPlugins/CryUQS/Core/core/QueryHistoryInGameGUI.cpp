// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryHistoryInGameGUI::SHistoricQueryShortInfo
		//
		//===================================================================================

		CQueryHistoryInGameGUI::SHistoricQueryShortInfo::SHistoricQueryShortInfo(const ColorF& _color, const CQueryID& _queryID, const CQueryID& _parentQueryID, string&& _shortInfo)
			: color(_color)
			, queryID(_queryID)
			, parentQueryID(_parentQueryID)
			, shortInfo(_shortInfo)
		{}

		//===================================================================================
		//
		// CQueryHistoryInGameGUI::SHistoricQueryDetailedTextLine
		//
		//===================================================================================

		CQueryHistoryInGameGUI::SHistoricQueryDetailedTextLine::SHistoricQueryDetailedTextLine(const ColorF& _color, string&& _textLine)
			: color(_color)
			, textLine(_textLine)
		{}

		//===================================================================================
		//
		// CQueryHistoryInGameGUI::SFocusedItemDetailedTextLine
		//
		//===================================================================================

		CQueryHistoryInGameGUI::SFocusedItemDetailedTextLine::SFocusedItemDetailedTextLine(const ColorF& _color, string&& _textLine)
			: color(_color)
			, textLine(_textLine)
		{}

		//===================================================================================
		//
		// CQueryHistoryInGameGUI
		//
		//===================================================================================

		CQueryHistoryInGameGUI::CQueryHistoryInGameGUI(IQueryHistoryManager& queryHistoryManager)
			: m_queryHistoryManager(queryHistoryManager)
			, m_scrollIndexInHistoricQueries(s_noScrollIndex)
		{
			m_queryHistoryManager.RegisterQueryHistoryListener(this);

			// react on ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE so that we can safely subscribe to a by-then valid IInput pointer
			GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CQueryHistoryInGameGUI");
		}

		CQueryHistoryInGameGUI::~CQueryHistoryInGameGUI()
		{
			m_queryHistoryManager.UnregisterQueryHistoryListener(this);

			if (ISystem* pSystem = GetISystem())
			{
				pSystem->GetISystemEventDispatcher()->RemoveListener(this);

				if (IInput* pInput = pSystem->GetIInput())
				{
					pInput->RemoveEventListener(this);
				}
			}
		}

		void CQueryHistoryInGameGUI::OnQueryHistoryEvent(const IQueryHistoryListener::SEvent& ev)
		{
			switch (ev.type)
			{
			case IQueryHistoryListener::EEventType::QueryHistoryDeserialized:
				if (m_queryHistoryManager.GetCurrentQueryHistory() == IQueryHistoryManager::EHistoryOrigin::Deserialized)
				{
					RefreshListOfHistoricQueries();
					RefreshDetailedInfoAboutCurrentHistoricQuery();
					RefreshDetailedInfoAboutFocusedItem();
				}
				break;

			case IQueryHistoryListener::EEventType::HistoricQueryJustFinishedInLiveQueryHistory:
				if (m_queryHistoryManager.GetCurrentQueryHistory() == IQueryHistoryManager::EHistoryOrigin::Live)
				{
					RefreshListOfHistoricQueries();
				}
				break;

			case IQueryHistoryListener::EEventType::LiveQueryHistoryCleared:
				if (m_queryHistoryManager.GetCurrentQueryHistory() == IQueryHistoryManager::EHistoryOrigin::Live)
				{
					RefreshListOfHistoricQueries();
					RefreshDetailedInfoAboutCurrentHistoricQuery();
					RefreshDetailedInfoAboutFocusedItem();
				}
				break;

			case IQueryHistoryListener::EEventType::DeserializedQueryHistoryCleared:
				if (m_queryHistoryManager.GetCurrentQueryHistory() == IQueryHistoryManager::EHistoryOrigin::Deserialized)
				{
					RefreshListOfHistoricQueries();
					RefreshDetailedInfoAboutCurrentHistoricQuery();
					RefreshDetailedInfoAboutFocusedItem();
				}
				break;

			case IQueryHistoryListener::EEventType::CurrentQueryHistorySwitched:
				RefreshListOfHistoricQueries();
				RefreshDetailedInfoAboutCurrentHistoricQuery();
				RefreshDetailedInfoAboutFocusedItem();
				break;

			case IQueryHistoryListener::EEventType::DifferentHistoricQuerySelected:
				RefreshListOfHistoricQueries();	// little hack to get the color of the currently selected historic query also updated
				RefreshDetailedInfoAboutCurrentHistoricQuery();
				RefreshDetailedInfoAboutFocusedItem();
				break;

			case IQueryHistoryListener::EEventType::FocusedItemChanged:
				RefreshDetailedInfoAboutFocusedItem();
				break;
			}
		}

		void CQueryHistoryInGameGUI::AddOrUpdateHistoricQuery(const SHistoricQueryOverview& overview)
		{
			Shared::CUqsString queryIdAsString;
			overview.queryID.ToString(queryIdAsString);

			string shortInfo;
			shortInfo.Format("#%s: '%s' / '%s' (%i / %i items) [%.2f ms]", queryIdAsString.c_str(), overview.szQuerierName, overview.szQueryBlueprintName, (int)overview.numResultingItems, (int)overview.numGeneratedItems, overview.timeElapsedUntilResult.GetMilliSeconds());

			m_historicQueries.emplace_back(overview.color, overview.queryID, overview.parentQueryID, std::move(shortInfo));
		}

		void CQueryHistoryInGameGUI::AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* szFormat, ...)
		{
			string textLine;

			va_list args;
			va_start(args, szFormat);
			textLine.FormatV(szFormat, args);
			va_end(args);

			m_textLinesOfCurrentHistoricQuery.emplace_back(color, std::move(textLine));
		}

		void CQueryHistoryInGameGUI::AddTextLineToFocusedItem(const ColorF& color, const char* szFormat, ...)
		{
			string textLine;

			va_list args;
			va_start(args, szFormat);
			textLine.FormatV(szFormat, args);
			va_end(args);

			m_textLinesOfFocusedItem.emplace_back(color, std::move(textLine));
		}

		void CQueryHistoryInGameGUI::AddInstantEvaluatorName(const char* szInstantEvaluatorName)
		{
			// nothing (we don't request the names of all instant-evaluators by calling IQueryHistoryManager::EnumerateInstantEvaluatorNames())
		}

		void CQueryHistoryInGameGUI::AddDeferredEvaluatorName(const char* szDeferredEvaluatorName)
		{
			// nothing (we don't request the names of all deferred-evaluators by calling IQueryHistoryManager::EnumerateDeferredEvaluatorNames())
		}

		void CQueryHistoryInGameGUI::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			switch (event)
			{
			case ESYSTEM_EVENT_CRYSYSTEM_INIT_DONE:
				if (IInput* pInput = GetISystem()->GetIInput()) // FYI: if this fails, then we're most likely running on a dedicated server that has no input device attached
				{
					pInput->AddEventListener(this);
				}
				break;
			}
		}

		bool CQueryHistoryInGameGUI::OnInputEvent(const SInputEvent& event)
		{
			// don't scroll through the history if it's not being drawn at all
			if (!SCvars::debugDraw)
				return false;

			bool bSwallowEvent = false;

			// - scroll through the history entries via PGUP/PGDOWN
			// - switch between the "live" and "deserialized" query history via END
			if (event.deviceType == eIDT_Keyboard && event.state == eIS_Pressed)
			{
				switch (event.keyId)
				{
				case eKI_PgUp:
					{
						const size_t historySize = m_historicQueries.size();

						if (historySize > 0)
						{
							// standing on "no entry" -> start with the latest one again (as if we were wrapping around)
							if (m_scrollIndexInHistoricQueries == s_noScrollIndex)
							{
								m_scrollIndexInHistoricQueries = historySize - 1;
							}
							else if (m_scrollIndexInHistoricQueries-- == 0)
							{
								// scrolling backwards beyond the first entry doesn't yet wrap to the latest entry again, but rather sticks to "no entry" so that we can have no debug drawing
								m_scrollIndexInHistoricQueries = s_noScrollIndex;
							}

							CQueryID queryIdOfHistoricQueryToSwitchTo = (m_scrollIndexInHistoricQueries == s_noScrollIndex) ? CQueryID::CreateInvalid() : m_historicQueries[m_scrollIndexInHistoricQueries].queryID;
							m_queryHistoryManager.MakeHistoricQueryCurrentForInWorldRendering(m_queryHistoryManager.GetCurrentQueryHistory(), queryIdOfHistoricQueryToSwitchTo);
						}

						bSwallowEvent = true;
					}
					break;

				case eKI_PgDn:
					{
						const size_t historySize = m_historicQueries.size();

						if (historySize > 0)
						{
							// standing on "no entry" -> wrap to first entry again
							if (m_scrollIndexInHistoricQueries == s_noScrollIndex)
							{
								m_scrollIndexInHistoricQueries = 0;
							}
							else if (++m_scrollIndexInHistoricQueries == historySize)
							{
								// scrolling forwards beyond the last entry doesn't yet wrap to the first entry again, but rather sticks to "no entry" so that we have no debug drawing
								m_scrollIndexInHistoricQueries = s_noScrollIndex;
							}

							CQueryID queryIdOfHistoricQueryToSwitchTo = (m_scrollIndexInHistoricQueries == s_noScrollIndex) ? CQueryID::CreateInvalid() : m_historicQueries[m_scrollIndexInHistoricQueries].queryID;
							m_queryHistoryManager.MakeHistoricQueryCurrentForInWorldRendering(m_queryHistoryManager.GetCurrentQueryHistory(), queryIdOfHistoricQueryToSwitchTo);
						}

						bSwallowEvent = true;
					}
					break;

				case eKI_End:
					{
						switch (m_queryHistoryManager.GetCurrentQueryHistory())
						{
						case IQueryHistoryManager::EHistoryOrigin::Live:
							m_queryHistoryManager.MakeQueryHistoryCurrent(IQueryHistoryManager::EHistoryOrigin::Deserialized);
							break;

						case IQueryHistoryManager::EHistoryOrigin::Deserialized:
							m_queryHistoryManager.MakeQueryHistoryCurrent(IQueryHistoryManager::EHistoryOrigin::Live);
							break;

						default:
							assert(0);
						}
						bSwallowEvent = true;
					}
					break;
				}
			}

			return bSwallowEvent;
		}

		void CQueryHistoryInGameGUI::Draw() const
		{
			if (!gEnv->pRenderer)
				return;

			const float xPos = (float)(gEnv->pRenderer->GetOverlayWidth() / 2 + 50);        // position found out by trial and error
			int row = 1;

			row = DrawQueryHistoryOverview(IQueryHistoryManager::EHistoryOrigin::Live, "live", xPos, row);
			row = DrawQueryHistoryOverview(IQueryHistoryManager::EHistoryOrigin::Deserialized, "deserialized", xPos, row);
			++row;
			row = DrawListOfHistoricQueries(xPos, row);
			++row;
			row = DrawDetailsAboutCurrentHistoricQuery(xPos, row);
			row = DrawDetailsAboutFocusedItem(xPos, row);
		}

		void CQueryHistoryInGameGUI::RefreshListOfHistoricQueries()
		{
			const IQueryHistoryManager::EHistoryOrigin currentHistory = m_queryHistoryManager.GetCurrentQueryHistory();

			m_historicQueries.clear();
			m_queryHistoryManager.EnumerateHistoricQueries(currentHistory, *this);
			FindScrollIndexInHistoricQueries();
		}

		void CQueryHistoryInGameGUI::RefreshDetailedInfoAboutCurrentHistoricQuery()
		{
			const IQueryHistoryManager::EHistoryOrigin currentHistory = m_queryHistoryManager.GetCurrentQueryHistory();
			const CQueryID queryIdToScrollTo = m_queryHistoryManager.GetCurrentHistoricQueryForInWorldRendering(currentHistory);

			m_textLinesOfCurrentHistoricQuery.clear();
			m_queryHistoryManager.GetDetailsOfHistoricQuery(currentHistory, queryIdToScrollTo, *this);
			FindScrollIndexInHistoricQueries();
		}

		void CQueryHistoryInGameGUI::RefreshDetailedInfoAboutFocusedItem()
		{
			m_textLinesOfFocusedItem.clear();
			m_queryHistoryManager.GetDetailsOfFocusedItem(*this);
		}

		void CQueryHistoryInGameGUI::FindScrollIndexInHistoricQueries()
		{
			const IQueryHistoryManager::EHistoryOrigin currentHistory = m_queryHistoryManager.GetCurrentQueryHistory();
			const CQueryID queryIdToScrollTo = m_queryHistoryManager.GetCurrentHistoricQueryForInWorldRendering(currentHistory);

			m_scrollIndexInHistoricQueries = s_noScrollIndex;

			for (size_t scrollIndex = 0; scrollIndex < m_historicQueries.size(); ++scrollIndex)
			{
				if (m_historicQueries[scrollIndex].queryID == queryIdToScrollTo)
				{
					m_scrollIndexInHistoricQueries = scrollIndex;
					break;
				}
			}
		}

		int CQueryHistoryInGameGUI::DrawQueryHistoryOverview(IQueryHistoryManager::EHistoryOrigin whichHistory, const char* szDescriptiveHistoryName, float xPos, int row) const
		{
			static const ColorF colorOfSelectedQueryHistory = Col_Cyan;
			static const ColorF colorOfNonSelectedQueryHistory = Col_White;

			static const char* szMarkerOfSelectedQueryHistory = "*";
			static const char* szMarkerOfNonSelectedQueryHistory = " ";

			ColorF color;
			const char* szMarker;
			const char* szHelpTextForKeyboardControl = "";

			if (m_queryHistoryManager.GetCurrentQueryHistory() == whichHistory)
			{
				color = colorOfSelectedQueryHistory;
				szMarker = szMarkerOfSelectedQueryHistory;
				szHelpTextForKeyboardControl = " - press 'PGUP'/'PGDN'";
			}
			else
			{
				color = colorOfNonSelectedQueryHistory;
				szMarker = szMarkerOfNonSelectedQueryHistory;
				szHelpTextForKeyboardControl = " - press 'END'";
			}

			CDrawUtil2d::DrawLabel(xPos, row, color, "=== %s %i UQS queries in %s history log (%i KB)%s ===",
				szMarker,
				(int)m_queryHistoryManager.GetHistoricQueriesCount(whichHistory),
				szDescriptiveHistoryName,
				(int)m_queryHistoryManager.GetRoughMemoryUsageOfQueryHistory(whichHistory) / 1024,
				szHelpTextForKeyboardControl);
			++row;
			return row;
		}

		int CQueryHistoryInGameGUI::DrawListOfHistoricQueries(float xPos, int row) const
		{
			//
			// - draw the list of historic queries as 2D text rows (for scrolling through them via PGUP/PGDOWN)
			// - this list is displayed for a short moment before automatically disappearing (pressing PGUP/PGDOWN brings it up again)
			//

			CDrawUtil2d::DrawLabel(xPos, row, Col_White, "=== Query history (%i queries logged - %i KB) ===",
				(int)m_historicQueries.size(), (int)m_queryHistoryManager.GetRoughMemoryUsageOfQueryHistory(m_queryHistoryManager.GetCurrentQueryHistory()) / 1024);
			++row;

			const int windowHeight = 100;

			const int historySize = m_historicQueries.size();

			// offset the rows in case there are more than would fit onto the screen
			const int maxRowsWeCanDraw = (int)((float)windowHeight / CDrawUtil2d::GetRowSize());

			int firstRowToDraw = (int)m_scrollIndexInHistoricQueries - maxRowsWeCanDraw / 2;

			if (firstRowToDraw > historySize - maxRowsWeCanDraw)
				firstRowToDraw = historySize - maxRowsWeCanDraw;

			if (firstRowToDraw < 0)
				firstRowToDraw = 0;

			int lastRowToDraw = firstRowToDraw + maxRowsWeCanDraw;
			if (lastRowToDraw >= historySize)
				lastRowToDraw = historySize - 1;

			for (int i = firstRowToDraw; i <= lastRowToDraw; ++i, ++row)
			{
				const bool bIsCurrentlySelectedHistoryEntry = ((size_t)i == m_scrollIndexInHistoricQueries);
				const SHistoricQueryShortInfo& queryInfo = m_historicQueries[i];
				const float indentSize = CDrawUtil2d::GetIndentSize() * (float)ComputeIndentationLevelOfHistoricQuery(queryInfo.queryID);
				const char* szFormatString = bIsCurrentlySelectedHistoryEntry ? "* %s" : "  %s";
				CDrawUtil2d::DrawLabel(xPos + indentSize, row, queryInfo.color, szFormatString, queryInfo.shortInfo.c_str());
			}

			return row;
		}

		int CQueryHistoryInGameGUI::DrawDetailsAboutCurrentHistoricQuery(float xPos, int row) const
		{
			for (const SHistoricQueryDetailedTextLine& info : m_textLinesOfCurrentHistoricQuery)
			{
				CDrawUtil2d::DrawLabel(xPos, row, info.color, "%s", info.textLine.c_str());
				++row;
			}
			return row;
		}

		int CQueryHistoryInGameGUI::DrawDetailsAboutFocusedItem(float xPos, int row) const
		{
			for (const SFocusedItemDetailedTextLine& info : m_textLinesOfFocusedItem)
			{
				CDrawUtil2d::DrawLabel(xPos, row, info.color, "%s", info.textLine.c_str());
				++row;
			}
			return row;
		}

		int CQueryHistoryInGameGUI::ComputeIndentationLevelOfHistoricQuery(CQueryID queryID) const
		{
			int indentation = 0;

			while (1)
			{
				// find the entry for the current query ID
				const SHistoricQueryShortInfo* pQueryShortInfo = nullptr;
				for (size_t i = 0, n = m_historicQueries.size(); i < n; ++i)
				{
					const SHistoricQueryShortInfo& shortInfo = m_historicQueries[i];
					if (shortInfo.queryID == queryID)
					{
						pQueryShortInfo = &shortInfo;
						break;
					}
				}
				assert(pQueryShortInfo);

				if (pQueryShortInfo->parentQueryID.IsValid())
				{
					// recurse up
					++indentation;
					queryID = pQueryShortInfo->parentQueryID;
				}
				else
				{
					// we've hit a top-level query
					break;
				}
			}

			return indentation;
		}

	}
}
