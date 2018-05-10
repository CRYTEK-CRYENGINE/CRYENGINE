// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryHistoryManager
		//
		//===================================================================================

		class CQueryHistoryManager : public IQueryHistoryManager
		{
		public:
			explicit                           CQueryHistoryManager();

			// IQueryHistoryManager
			virtual void                       RegisterQueryHistoryListener(IQueryHistoryListener* pListener) override;
			virtual void                       UnregisterQueryHistoryListener(IQueryHistoryListener* pListener) override;
			virtual void                       UpdateDebugRendering3D(const SDebugCameraView* pOptionalView, const SEvaluatorDrawMasks& evaluatorDrawMasks) override;
			virtual bool                       SerializeLiveQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error) override;
			virtual bool                       DeserializeQueryHistory(const char* szXmlFilePath, Shared::IUqsString& error) override;
			virtual void                       MakeQueryHistoryCurrent(EHistoryOrigin whichHistory) override;
			virtual EHistoryOrigin             GetCurrentQueryHistory() const override;
			virtual void                       ClearQueryHistory(EHistoryOrigin whichHistory) override;
			virtual void                       EnumerateSingleHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToEnumerate, IQueryHistoryConsumer& receiver) const override;
			virtual void                       EnumerateHistoricQueries(EHistoryOrigin whichHistory, IQueryHistoryConsumer& receiver) const override;
			virtual void                       MakeHistoricQueryCurrentForInWorldRendering(EHistoryOrigin whichHistory, const CQueryID& queryIDToMakeCurrent) override;
			virtual void                       EnumerateInstantEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver) override;
			virtual void                       EnumerateDeferredEvaluatorNames(EHistoryOrigin fromWhichHistory, const CQueryID& idOfQueryUsingTheseEvaluators, IQueryHistoryConsumer& receiver) override;
			virtual CQueryID                   GetCurrentHistoricQueryForInWorldRendering(EHistoryOrigin fromWhichHistory) const override;
			virtual void                       GetDetailsOfHistoricQuery(EHistoryOrigin whichHistory, const CQueryID& queryIDToGetDetailsFor, IQueryHistoryConsumer& receiver) const override;
			virtual void                       GetDetailsOfFocusedItem(IQueryHistoryConsumer& receiver) const override;
			virtual size_t                     GetRoughMemoryUsageOfQueryHistory(EHistoryOrigin whichHistory) const override;
			virtual size_t                     GetHistoricQueriesCount(EHistoryOrigin whichHistory) const override;
			virtual SDebugCameraView           GetIdealDebugCameraView(EHistoryOrigin whichHistory, const CQueryID& queryID, const SDebugCameraView& currentCameraView) const override;
			// ~IQueryHistoryManager

			HistoricQuerySharedPtr             AddNewLiveHistoricQuery(const CQueryID& queryID, const char* szQuerierName, const CQueryID& parentQueryID);
			void                               UnderlyingQueryJustGotCreated(const CQueryID& queryID);
			void                               UnderlyingQueryIsGettingDestroyed(const CQueryID& queryID);

			void                               AutomaticUpdateDebugRendering3DBegin();
			void                               AutomaticUpdateDebugRendering3DEnd();

		private:
			                                   UQS_NON_COPYABLE(CQueryHistoryManager);

			void                               NotifyListeners(IQueryHistoryListener::EEventType eventType) const;
			void                               NotifyListeners(IQueryHistoryListener::EEventType eventType, const CQueryID& relatedQueryID) const;

		private:
			CQueryHistory                      m_queryHistories[2];                  // "live" and "deserialized" query history; the live one gets filled by ongoing queries from the CQueryManager
			CQueryID                           m_queryIDOfCurrentHistoricQuery[2];   // ditto
			EHistoryOrigin                     m_historyToManage;                    // used as index into m_queryHistories[] and m_queryIDOfCurrentHistoricQuery[]
			size_t                             m_indexOfFocusedItem;
			std::list<IQueryHistoryListener*>  m_listeners;
			bool                               m_bAutomaticUpdateDebugRendering3DInProgress;

			static const size_t                s_noItemFocusedIndex = (size_t)-1;
		};

	}
}
