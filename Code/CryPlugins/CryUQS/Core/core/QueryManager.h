// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CQueryManager
		//
		// - starts and runs all "live" queries and notifies the listener once a query has finished
		// - also draws details about currently running and recently finished queries as 2D text on screen
		//
		//===================================================================================

		class CQueryManager : public IQueryManager
		{
		private:

			//===================================================================================
			//
			// SRunningQueryInfo
			//
			// - entries in the main queue (m_queries)
			//
			//===================================================================================

			struct SRunningQueryInfo
			{
				explicit                                               SRunningQueryInfo();
				std::shared_ptr<CQueryBase>                            pQuery;
				std::shared_ptr<const CQueryBlueprint>                 pQueryBlueprint;
				Functor1<const SQueryResult&>                          pCallback;
			};

			//===================================================================================
			//
			// SFinishedQueryInfo
			//
			// - entries transferred from the main queue to a temporary container once the query finished to notify listeners outside the update loop
			//
			//===================================================================================

			struct SFinishedQueryInfo
			{
				explicit                                               SFinishedQueryInfo(const std::shared_ptr<CQueryBase>& _pQuery, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, const Functor1<const SQueryResult&>& _pCallback, const CQueryID& _queryID, bool _bQueryFinishedWithSuccess, const string& _errorIfAny);
				std::shared_ptr<CQueryBase>                            pQuery;
				std::shared_ptr<const CQueryBlueprint>                 pQueryBlueprint;
				Functor1<const SQueryResult&>                          pCallback;
				CQueryID                                               queryID;
				bool                                                   bQueryFinishedWithSuccess;
				string                                                 errorIfAny;
			};

			//===================================================================================
			//
			// SHistoryQueryInfo2D
			//
			// - 2D on-screen debug drawing: entry in the list of finished queries to stay a bit on screen and then fade out
			//
			//===================================================================================

			struct SHistoryQueryInfo2D
			{
				explicit                                               SHistoryQueryInfo2D(const CQueryID &_queryID, const CQueryBase::SStatistics& _statistics, bool _bQueryFinishedWithSuccess, const CTimeValue& _timestamp);
				CQueryID                                               queryID;
				CQueryBase::SStatistics                                statistics;
				bool                                                   bQueryFinishedWithSuccess;
				CTimeValue                                             finishedTimestamp;     // time of when the query was finished; for fading out after a short moment
			};

		public:
			explicit                                                   CQueryManager(CQueryHistoryManager& queryHistoryManager);

			// IQueryManager
			virtual CQueryID                                           StartQuery(const Client::SQueryRequest& request, Shared::IUqsString& errorMessage) override;
			virtual void                                               CancelQuery(const CQueryID& idOfQueryToCancel) override;
			virtual void                                               AddItemMonitorToQuery(const CQueryID& queryID, Client::ItemMonitorUniquePtr&& pItemMonitorToInstall) override;
			// ~IQueryManager

			CQueryID                                                   StartQueryInternal(const CQueryID& parentQueryID, std::shared_ptr<const CQueryBlueprint> pQueryBlueprint, const Shared::IVariantDict& runtimeParams, const char* szQuerierName, Functor1<const Core::SQueryResult&> callback, const std::shared_ptr<CItemList>& pPotentialResultingItemsFromPreviousQuery, Shared::IUqsString& errorMessage);
			CQueryBase*                                                FindQueryByQueryID(const CQueryID& queryID);
			void                                                       Update();
			void                                                       DebugDrawRunningQueriesStatistics2D() const;
			void                                                       PrintRunningQueriesToConsole(CLogger& logger) const;
			void                                                       CancelAllRunningQueriesDueToUpcomingTearDownOfHub();

		private:
			                                                           UQS_NON_COPYABLE(CQueryManager);

			void                                                       UpdateQueries();
			void                                                       ExpireDebugDrawStatisticHistory2D();

			static void                                                NotifyCallbackOfFinishedQuery(const SFinishedQueryInfo& finishedQueryInfo);
			static void                                                DebugPrintQueryStatistics(CLogger& logger, const CQueryBase& query, const CQueryID& queryID);
			static int                                                 DebugDrawQueryStatistics(const CQueryBase::SStatistics& statisticsToDraw, const CQueryID& queryID, int row, const ColorF& color);

		private:
			CQueryID                                                   m_queryIDProvider;
			std::map<CQueryID, SRunningQueryInfo>                      m_queries;                    // using a sorted map to guarantee updating all queries in the order in which they were started
			CQueryHistoryManager&                                      m_queryHistoryManager;        // for allocating new historic queries each time a query blueprint gets instantiated and runs
			std::deque<SHistoryQueryInfo2D>                            m_debugDrawHistory2D;         // for 2D on-screen drawing of statistics of all current and some past queries

			static const CTimeValue                                    s_delayBeforeFadeOut;
			static const CTimeValue                                    s_fadeOutDuration;
			static const CTimeValue                                    s_totalDebugDrawDuration;
		};

	}
}
