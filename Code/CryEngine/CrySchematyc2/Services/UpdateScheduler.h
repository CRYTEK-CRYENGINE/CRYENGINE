// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Inline helper structs and move to .cpp?
// #SchematycTODO : Sort slots by scope address?

#pragma once

#include <CrySchematyc2/Services/IUpdateScheduler.h>
#include <ILevelSystem.h>

#ifndef _RELEASE
#define DEBUG_RELEVANCE_GRID (1)
#define DEBUG_BUCKETS_NAMES (1)
#endif

namespace Schematyc2
{
	class CUpdateScheduler;

	// A simple grid implementation, which is connecting/disconnecting schematyc component updates based on relevant entities (players)
	class CRelevanceGrid 
		: public ISystemEventListener
		, public ILevelSystemListener
	{
	public:
		enum ECellChangedResult
		{
			eCellChangedResult_Same,
			eCellChangedResult_Changed,
			eCellChangedResult_NotFound,
			eCellChangedResult_Count
		};

		// Stores all the info needed to hook a update callback from schematyc components
		struct SUpdateCallback
		{
			SUpdateCallback(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);

			CUpdateScope*       pScope;
			UpdateCallback      callback;
			UpdateFrequency     frequency;
			UpdatePriority      priority;
			UpdateFilter        filter;
		};

		typedef std::vector<SUpdateCallback> TUpdateCallbacks;

		// Add more information here in case we need better filtering for dynamic objects
		struct SDynamicObject
		{
			SDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);

			SUpdateCallback updateCallback;
		};

		typedef std::vector<SDynamicObject> TDynamicObjects;

		struct SGridCell
		{
			SGridCell()
				: relevanceCounter(0)
			{}

			TUpdateCallbacks globalUpdateCallbacks;
			TUpdateCallbacks localGridUpdateCallbacks;
			short            relevanceCounter;
		};

		typedef std::vector<SGridCell> TGridCells;

		struct SRelevantCell
		{
			SRelevantCell()
				: x(0)
				, y(0)
				, cellIdx(0)
				, id(INVALID_ENTITYID)
				, bIsLocal(false)
			{}

			SRelevantCell(ushort x, ushort y, ushort cellIdx, EntityId id, bool bIsLocal)
				: x(x)
				, y(y)
				, cellIdx(cellIdx)
				, id(id)
				, bIsLocal(bIsLocal)
			{}

			ushort   x;
			ushort   y;
			ushort   cellIdx;
			bool     bIsLocal;
			EntityId id;
		};

		typedef std::vector<SRelevantCell>  TRelevantCells;

	public:
		CRelevanceGrid();
		virtual ~CRelevanceGrid();

		// ISystemEventListener
		virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
		// ~ISystemEventListener

		// ILevelSystemListener
		virtual void OnLevelNotFound(const char* levelName) override {}
		virtual void OnLoadingStart(ILevelInfo* pLevel) override {}
		virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) override;
		virtual void OnLoadingComplete(ILevelInfo* pLevel) override {}
		virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) override {}
		virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override {}
		virtual void OnUnloadComplete(ILevelInfo* pLevel) override {}
		// ~ILevelSystemListener

		bool Register(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);
		void Unregister(CUpdateScope* pScope);
		bool RegisterDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);
		void UnregisterDynamicObject(CUpdateScope* pScope);
		void Update(CUpdateRelevanceContext* pRelevanceContext = nullptr);
		void SetUpdateScheduler(CUpdateScheduler* pScheduler);

	private:
		void               Construct();
		void               SetupUpdateCallbacksAroundCell(ushort cellX, ushort cellY, bool bIsLocal, bool bConnect);
		void               ConnectCell(ushort idx, bool bIsLocal);
		void               DisconnectCell(ushort idx, bool bIsLocal);
		ECellChangedResult DidRelevantCellIndexChange(const SRelevantCell& cellToFind, SRelevantCell*& pCellInfo);
		void               GetCellCoordinates(const Vec3 worldPos, ushort &x, ushort& y, ushort& cellIdx) const;
		ushort             GetCellIndex(ushort x, ushort y) const;

#if DEBUG_RELEVANCE_GRID
		void DebugDrawStatic(CUpdateRelevanceContext* pRelevanceContext);
		void DebugLogStaticMemoryStats();
#endif

	private:
		TGridCells        m_grid;
		TDynamicObjects   m_dynamicObjects;
		TRelevantCells    m_relevantCells;
		CUpdateScheduler* m_pUpdateScheduler;
		std::vector<Vec3> m_relevantEntitiesWorldPos;
		int               m_gridSize; // The grid size. Its a square so m_gridSize x m_gridSize
		int               m_gridSizeMinusOne; // Cached constantly needed value
		int               m_updateHalfDistance; // The update distance around the relevant cells in cells (e.g. 1, 2... cells)

#if DEBUG_RELEVANCE_GRID
		int               m_debugUpdateFrame;
#endif
	};

	class CUpdateScheduler : public IUpdateScheduler
	{
		friend class CRelevanceGrid;

	public:

		CUpdateScheduler();

		~CUpdateScheduler();

		// IUpdateScheduler
		virtual bool Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency = EUpdateFrequency::EveryFrame, UpdatePriority priority = EUpdatePriority::Default, const UpdateFilter& filter = UpdateFilter()) override;
		virtual void Disconnect(CUpdateScope& scope) override;
		virtual bool InFrame() const override;
		virtual bool BeginFrame(float frameTime) override;
		virtual bool Update(UpdatePriority beginPriority = EUpdateStage::PrePhysics | EUpdateDistribution::Earliest, UpdatePriority endPriority = EUpdateStage::Post | EUpdateDistribution::End, CUpdateRelevanceContext* pRelevanceContext = nullptr) override;
		virtual bool EndFrame() override;
		virtual void VerifyCleanup() override;
		virtual void Reset() override;
		virtual const UpdateSchedulerStats::IFrameUpdateStats* GetFrameUpdateStats() const override;
		virtual void SetShouldUseRelevanceGridCallback(UseRelevanceGridPredicate directConnect) override;
		virtual void SetIsDynamicObjectCallback(IsDynamicObjectPredicate isDynamicObject) override;
		virtual void SetDebugPriorityNames(const DebugPriorityNameArray& debugNames) override;
		// ~IUpdateScheduler

	private:

		bool ConnectInternal(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency = EUpdateFrequency::EveryFrame, UpdatePriority priority = EUpdatePriority::Default, const UpdateFilter& filter = UpdateFilter());
		void DisconnectInternal(CUpdateScope &scope);
		void UnregisterFromRelevanceGrid(CUpdateScope& scope);

		struct SUpdateSlot
		{
			SUpdateSlot();
			SUpdateSlot(const UpdatePriority priority, const UpdateFrequency frequency);

			UpdatePriority  priority;
			UpdateFrequency bucketIndex;

			inline bool operator <(const SUpdateSlot& rhs) const
			{
				return priority > rhs.priority;
			}
		};

		typedef std::vector<SUpdateSlot> TUpdateSlots;

		struct SObserver
		{
			SObserver();
			SObserver(CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter, UpdateFrequency frequency, UpdateFrequency stride);

			CUpdateScope*   pScope;
			UpdateCallback  callback;
			UpdateFilter    filter;
			UpdateFrequency frequency : 8;
			UpdateFrequency stride : 8;

			static_assert(sizeof(UpdateFrequency) == 2, "UpdateFrequency expected to be 16 bit");
		};

		typedef std::vector<SObserver> TObserverVector;

		struct SPendingObserver : public SObserver
		{
			SPendingObserver();
			SPendingObserver(CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter, UpdateFrequency frequency, UpdateFrequency stride, UpdatePriority priority);

			UpdatePriority priority;
		};

		struct SFrameUpdateStats : public UpdateSchedulerStats::IFrameUpdateStats
		{
			enum { k_maxStagesCount = 8 };
			enum { k_maxBucketsCount = EUpdateFrequency::Count };

			SFrameUpdateStats();

			void Add(const UpdateSchedulerStats::SUpdateStageStats& stats);
			void AddConnected(int64 timeTicks);
			void AddDisconnected(int64 timeTicks);
			void Reset();

			virtual const UpdateSchedulerStats::SUpdateStageStats* GetStageStats(size_t& outCount) const override;
			virtual const UpdateSchedulerStats::SUpdateBucketStats* GetBucketStats(size_t& outCount) const override;
			virtual const UpdateSchedulerStats::SChangeStats* GetChangeStats() const override { return &changeStats; }

			UpdateSchedulerStats::SUpdateStageStats stagesStats[k_maxStagesCount];
			size_t stagesCount;
			UpdateSchedulerStats::SUpdateBucketStats bucketStats[k_maxBucketsCount];
			UpdateSchedulerStats::SChangeStats changeStats;
		};

		typedef std::vector<size_t> TDirtyIndicesVector;

		struct SObserverGroup
		{
			SObserverGroup();

			TObserverVector     observers;
			TDirtyIndicesVector dirtyIndices;
		};

		typedef VectorMap<UpdatePriority, SObserverGroup> TObserverMap;
		typedef std::vector<SPendingObserver> TPendingObservers;
		typedef std::vector<UpdatePriority> TPendingPriorities;
		typedef VectorMap<UpdatePriority, const char*> TPriorityDebugNameMap;

		class CBucket
		{
		public:
			struct SUpdateStats;

			CBucket();

			void Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);
			void Disconnect(CUpdateScope& scope, UpdatePriority priority);
			void BeginUpdate(TPendingPriorities& pendingPriorities);
			template <bool UseStride>
			void Update(const SUpdateContext context, UpdatePriority priority, uint32 frameId, UpdateSchedulerStats::SUpdateBucketStats& outUpdateStats, const TPriorityDebugNameMap& debugNames);
			void Reset();
			void CleanUp();
			void SetFrequency(UpdateFrequency frequency);

		private:
			UpdateFrequency SelectNewStride() const;
			void AddObserver(TObserverVector& observers, CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter);
			void DeleteDirtyObservers();
			void AddPendingObservers(TPendingPriorities& pendingPriorities);
			const char* GetDebugName(UpdatePriority priority, const TPriorityDebugNameMap& debugNames);

		private:

			TObserverMap        m_observers;
			TPendingObservers   m_pendingNewPriorityObservers;
			size_t              m_totalObserverCount;
			size_t              m_totalDirtyCount;
			std::vector<size_t> m_frameStrideBuckets;
			UpdateFrequency     m_frequency;
		};

		static const size_t s_maxFrameStride = 1 << (EUpdateFrequency::Count - 1);


		CBucket           m_buckets[EUpdateFrequency::Count];
		TUpdateSlots      m_updateOrder;
		size_t            m_updatePosition;
		float             m_frameTimes[s_maxFrameStride];
		uint32            m_frameIdx;
		bool              m_bInFrame;

		TPriorityDebugNameMap m_debugNameMap;
		SFrameUpdateStats m_stats;
		TPendingPriorities m_pendingPriorities;
		// Relevance grid related members
		CRelevanceGrid                              m_relevanceGrid;
		IUpdateScheduler::UseRelevanceGridPredicate m_useRelevanceGrid;
		IUpdateScheduler::IsDynamicObjectPredicate  m_isDynamicObject;
	};
}
