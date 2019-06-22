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
				struct SWorkingData
				{
					int                                                handedOutPriorityInCurrentFrame = 0;
					int                                                remainingPriorityForNextFrame = 0;
				};

				explicit                                               SRunningQueryInfo();
				std::shared_ptr<CQueryBase>                            pQuery;
				std::shared_ptr<const CQueryBlueprint>                 pQueryBlueprint;
				Functor1<const SQueryResult&>                          pCallback;
				int                                                    priority;
				std::unique_ptr<SWorkingData>                          pWorkingData;
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
				explicit                                               SFinishedQueryInfo(const std::shared_ptr<CQueryBase>& _pQuery, const std::shared_ptr<const CQueryBlueprint>& _pQueryBlueprint, const Functor1<const SQueryResult&>& _pCallback, int _priority, const CTimeValue& _queryFinishedTimestamp, bool _bQueryFinishedWithSuccess, const string& _errorIfAny);
				std::shared_ptr<CQueryBase>                            pQuery;
				std::shared_ptr<const CQueryBlueprint>                 pQueryBlueprint;
				Functor1<const SQueryResult&>                          pCallback;
				int                                                    priority;
				CTimeValue                                             queryFinishedTimestamp;
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
				explicit                                               SHistoryQueryInfo2D(const CQueryID &_queryID, int _priority, const CQueryBase::SStatistics& _statistics, bool _bQueryFinishedWithSuccess, const CTimeValue& _timestamp);
				CQueryID                                               queryID;
				int                                                    priority;
				CQueryBase::SStatistics                                statistics;
				bool                                                   bQueryFinishedWithSuccess;
				CTimeValue                                             finishedTimestamp;     // time of when the query was finished; for fading out after a short moment
			};

			//===================================================================================
			//
			// SRoundRobinDebugSnapshot
			//
			// - to visualize the granted time-budget per query in the round-robin list of the current frame
			//
			//===================================================================================

			struct SRoundRobinDebugSnapshot
			{
				struct SElement
				{
					Shared::CUqsString                                 queryIdAsString;
					int                                                priority = 0;           // priority level of the query in general
					int                                                priorityHandedOut = 0;  // how much was used in the current frame
					int                                                priorityRemaining = 0;  // how much will spill over to the next frame
				};

				int                                                    roundRobinLimit = 0;    // max. no. of queries that could fit into the round-robin list in the current frame
				std::vector<SElement>                                  elements;               // all queries that actually ended up in the round-robin list
			};

			//===================================================================================
			//
			// CPredEqualQueryID
			//
			//===================================================================================

			class CPredEqualQueryID
			{
			public:
				explicit                                               CPredEqualQueryID(const CQueryID& queryID);
				bool                                                   operator()(const SRunningQueryInfo& runningQueryInfo) const;

			private:
				CQueryID                                               m_queryID;
			};

		public:
			explicit                                                   CQueryManager();

			// IQueryManager
			virtual CQueryID                                           StartQuery(const Client::SQueryRequest& request, Shared::IUqsString& errorMessage) override;
			virtual void                                               CancelQuery(const CQueryID& idOfQueryToCancel) override;
			virtual void                                               AddItemMonitorToQuery(const CQueryID& queryID, Client::ItemMonitorUniquePtr&& pItemMonitorToInstall) override;
			virtual void                                               RegisterQueryFinishedListener(Client::IQueryFinishedListener* pListenerToRegister) override;
			virtual void                                               UnregisterQueryFinishedListener(Client::IQueryFinishedListener* pListenerToUnregister) override;
			virtual void                                               RegisterQueryWarningListener(Client::IQueryWarningListener* pListenerToRegister) override;
			virtual void                                               UnregisterQueryWarningListener(Client::IQueryWarningListener* pListenerToUnregister) override;
			virtual void                                               VisitRunningQueries(Client::IQueryVisitor& visitor) override;
			// ~IQueryManager

			CQueryBase*                                                FindQueryByQueryID(const CQueryID& queryID);
			void                                                       Update();
			void                                                       DebugDrawRunningQueriesStatistics2D() const;
			void                                                       PrintRunningQueriesToConsole(CLogger& logger) const;
			void                                                       CancelAllRunningQueriesDueToUpcomingTearDownOfHub();

		private:
			                                                           UQS_NON_COPYABLE(CQueryManager);

			void                                                       UpdateQueries();
			void                                                       BuildRoundRobinList(std::vector<const SRunningQueryInfo*>& outRoundRobinQueries);
			void                                                       UpdateRoundRobinQueries(const std::vector<const SRunningQueryInfo*>& roundRobinQueries, std::vector<SFinishedQueryInfo>& outFinishedQueries);
			void                                                       FinalizeFinishedQueries(const std::vector<SFinishedQueryInfo>& finishedQueries);
			void                                                       ExpireDebugDrawStatisticHistory2D();
			void                                                       NotifyCallbacksOfFinishedQuery(const SFinishedQueryInfo& finishedQueryInfo) const;
			void                                                       NotifyOfQueryPerformanceWarning(const SRunningQueryInfo& problematicQuery, const char* szFmt, ...) const PRINTF_PARAMS(3, 4);

			static CTimeValue                                          HelpUpdateSingleQuery(const SRunningQueryInfo& queryToUpdate, const CTimeValue& timeBudgetForThisQuery, std::vector<SFinishedQueryInfo>& outFinishedQueries);
			static void                                                DebugPrintQueryStatistics(CLogger& logger, const CQueryBase& query, const CQueryID& queryID);
			static int                                                 DebugDrawQueryStatistics(const CQueryBase::SStatistics& statisticsToDraw, const CQueryID& queryID, int priority, int row, const ColorF& color);
			void                                                       DebugDrawRoundRobinLoad() const;

		private:
			std::list<SRunningQueryInfo>                               m_queries;                    // all currently running queries (contains only top-level queries; child queries are managed by their parent)
			std::list<SRunningQueryInfo>::const_iterator               m_roundRobinStart;            // bookmark of where the next round-robin update will start; points into m_queries; care is being take to prevent this iterator from dangling whenever the m_queries container is changed
			bool                                                       m_bQueriesUpdateInProgress;   // safety guard to detect calls to CancelQuery() in the middle of iterating through all m_queries (this would invalidate iterators!!!)
			std::deque<SHistoryQueryInfo2D>                            m_debugDrawHistory2D;         // for 2D on-screen drawing of statistics of all current and some past queries
			SRoundRobinDebugSnapshot                                   m_roundRobinDebugSnapshot;    // round-robin load of the current frame for visualizing on screen
			std::vector<Client::IQueryFinishedListener*>               m_queryFinishedListeners;
			std::vector<Client::IQueryWarningListener*>                m_queryWarningListeners;

			static const CTimeValue                                    s_delayBeforeFadeOut;
			static const CTimeValue                                    s_fadeOutDuration;
			static const CTimeValue                                    s_totalDebugDrawDuration;
		};

	}
}
