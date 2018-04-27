// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryInput/IInput.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryHistoryInGameGUI
		//
		// - simple UI that lets the user scroll through the historic queries that have been recorded so far
		// - draws detailed information about the selected historic query as well as of the item that is currently focused by the camera
		//
		//===================================================================================

		class CQueryHistoryInGameGUI : public IQueryHistoryListener, public IQueryHistoryConsumer, public ISystemEventListener, public IInputEventListener
		{
		public:

			explicit                                     CQueryHistoryInGameGUI(IQueryHistoryManager& queryHistoryManager);
			                                             ~CQueryHistoryInGameGUI();

			// IQueryHistoryListener
			virtual void                                 OnQueryHistoryEvent(const IQueryHistoryListener::SEvent& ev) override;
			// ~IQueryHistoryListener

			// IQueryHistoryConsumer
			virtual void                                 AddOrUpdateHistoricQuery(const SHistoricQueryOverview& overview) override;
			virtual void                                 AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* szFormat, ...) override;
			virtual void                                 AddTextLineToFocusedItem(const ColorF& color, const char* szFormat, ...) override;
			virtual void                                 AddInstantEvaluatorName(const char* szInstantEvaluatorName) override;
			virtual void                                 AddDeferredEvaluatorName(const char* szDeferredEvaluatorName) override;
			// ~IQueryHistoryConsumer

			// ISystemEventListener
			virtual void                                 OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
			// ~ISystemEventListener

			// IInputEventListener
			virtual bool                                 OnInputEvent(const SInputEvent& event) override;
			// ~IInputEventListener

			void                                         Draw() const;

		private:

			                                             UQS_NON_COPYABLE(CQueryHistoryInGameGUI);

			void                                         RefreshListOfHistoricQueries();
			void                                         RefreshDetailedInfoAboutCurrentHistoricQuery();
			void                                         RefreshDetailedInfoAboutFocusedItem();
			void                                         FindScrollIndexInHistoricQueries();

			int                                          DrawQueryHistoryOverview(IQueryHistoryManager::EHistoryOrigin whichHistory, const char* szDescriptiveHistoryName, float xPos, int row) const;
			int                                          DrawListOfHistoricQueries(float xPos, int row) const;
			int                                          DrawDetailsAboutCurrentHistoricQuery(float xPos, int row) const;
			int                                          DrawDetailsAboutFocusedItem(float xPos, int row) const;

			int                                          ComputeIndentationLevelOfHistoricQuery(CQueryID queryID) const;

		private:

			//===================================================================================
			//
			// SHistoricQueryShortInfo
			//
			// - one of these exists for each historic query
			// - they're used to scroll through all historic queries via PGUP/PGDN
			//
			//===================================================================================

			struct SHistoricQueryShortInfo
			{
				ColorF                                   color;
				CQueryID                                 queryID;
				CQueryID                                 parentQueryID;
				string                                   shortInfo;
				explicit                                 SHistoricQueryShortInfo(const ColorF& _color, const CQueryID& _queryID, const CQueryID& _parentQueryID, string&& _shortInfo);
			};

			//===================================================================================
			//
			// SHistoricQueryDetailedTextLine
			//
			// - this is a single text line in the detailed information of the currently selected historic query
			// - the detailed information may be made up of multiple lines
			//
			//===================================================================================

			struct SHistoricQueryDetailedTextLine
			{
				ColorF                                   color;
				string                                   textLine;
				explicit                                 SHistoricQueryDetailedTextLine(const ColorF& _color, string&& _textLine);
			};

			//===================================================================================
			//
			// SFocusedItemDetailedTextLine
			//
			// - this is a single text line in the detailed information of the item that the camera is currently focusing
			// - the detailed information may be made up of multiple lines
			//
			//===================================================================================

			struct SFocusedItemDetailedTextLine
			{
				ColorF                                   color;
				string                                   textLine;
				explicit                                 SFocusedItemDetailedTextLine(const ColorF& _color, string&& _textLine);
			};

		private:

			IQueryHistoryManager&                        m_queryHistoryManager;
			std::vector<SHistoricQueryShortInfo>         m_historicQueries;                  // each element represents a single historic query with a short description
			std::vector<SHistoricQueryDetailedTextLine>  m_textLinesOfCurrentHistoricQuery;  // this represents currently selected historic query; it contains multiple lines of text with details about that historic query
			std::vector<SFocusedItemDetailedTextLine>    m_textLinesOfFocusedItem;           // this represents the currently focused item in the 3D debug render world; it contains multiple lines of text describing the item
			size_t                                       m_scrollIndexInHistoricQueries;     // points into m_historicQueries

			static const size_t                          s_noScrollIndex = (size_t)-1;
		};

	}
}
