// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Inline helper structs and move to .cpp?
// #SchematycTODO : Sort slots by scope address?

#pragma once

#include <CrySchematyc2/Services/IUpdateScheduler.h>
#include <ILevelSystem.h>

#ifndef _RELEASE
#define DEBUG_RELEVANCE_GRID (1)
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
		typedef VectorMap<EntityId, ushort> TEntityToGridCell;

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
#endif

	private:
		TEntityToGridCell m_entityToGridCellIndexLookup;
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
		virtual bool ScopeDestroyed(CUpdateScope& scope) override;
		virtual bool InFrame() const override;
		virtual bool BeginFrame(float frameTime) override;
		virtual bool Update(UpdatePriority beginPriority = EUpdateStage::PrePhysics | EUpdateDistribution::Earliest, UpdatePriority endPriority = EUpdateStage::Post | EUpdateDistribution::End, CUpdateRelevanceContext* pRelevanceContext = nullptr) override;
		virtual bool EndFrame() override;
		virtual void VerifyCleanup() override;
		virtual void Reset() override;
		virtual const UpdateSchedulerStats::IFrameUpdateStats* GetFrameUpdateStats() const override;
		virtual void SetShouldUseRelevanceGridCallback(UseRelevanceGridPredicate directConnect) override;
		virtual void SetIsDynamicObjectCallback(IsDynamicObjectPredicate isDynamicObject) override;
		// ~IUpdateScheduler

	private:

		bool ConnectInternal(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency = EUpdateFrequency::EveryFrame, UpdatePriority priority = EUpdatePriority::Default, const UpdateFilter& filter = UpdateFilter());
		void DisconnectInternal(CUpdateScope &scope);

		struct SSlot
		{
			SSlot();

			SSlot(CUpdateScope* _pScope, uint32 _firstBucketIdx, UpdateFrequency _frequency, UpdatePriority _priority);

			CUpdateScope*   pScope;
			uint32          firstBucketIdx;
			UpdateFrequency frequency;
			UpdatePriority  priority;
		};

		typedef std::vector<SSlot> SlotVector;

		struct SFindSlot
		{
			inline SFindSlot(const CUpdateScope* _pScope)
				: pScope(_pScope)
			{}

			inline bool operator () (const SSlot& lhs) const
			{
				return lhs.pScope == pScope;
			}

			const CUpdateScope* pScope;
		};

		struct SObserver
		{
			SObserver(const CUpdateScope* _pScope, const UpdateCallback& _callback, UpdateFrequency _frequency, UpdatePriority _priority, const UpdateFilter& _filter);

			const CUpdateScope* pScope;
			UpdateCallback      callback;
			UpdateFrequency     frequency;
			UpdatePriority      currentPriority;
			UpdatePriority      newPriority;
			UpdateFilter        filter;
		};

		typedef std::vector<SObserver> ObserverVector;

		struct SSortObservers
		{
			inline bool operator () (const SObserver& lhs, const SObserver& rhs) const
			{
				return (lhs.currentPriority > rhs.currentPriority) || ((lhs.currentPriority == rhs.currentPriority) && (lhs.callback < rhs.callback));
			}
		};

		struct SLowerBoundObserver
		{
			inline bool operator () (const SObserver& lhs, const UpdatePriority& rhs) const
			{
				return lhs.currentPriority > rhs;
			}
		};

		struct SFrameUpdateStats : public UpdateSchedulerStats::IFrameUpdateStats
		{
			enum { k_maxStagesCount = 8 };

			SFrameUpdateStats();

			void Add(const UpdateSchedulerStats::SUpdateStageStats& stats);
			void Reset();

			virtual const UpdateSchedulerStats::SUpdateStageStats* Get(size_t& outCount) const override;

			UpdateSchedulerStats::SUpdateStageStats stagesStats[k_maxStagesCount];
			size_t count;
		};

		class CBucket
		{
		public:

			CBucket();

			void Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter);
			void Disconnect(CUpdateScope& scope, UpdatePriority priority);
			void BeginUpdate();
			void Update(const SUpdateContext (&frequencyUpdateContexts)[EUpdateFrequency::Count], UpdatePriority beginPriority, UpdatePriority endPriority, UpdateSchedulerStats::SUpdateStageStats& outUpdateStats);
			void EndUpdate();
			void Reset();
			
			inline size_t GetSize() const
			{
				return m_observers.size() - m_garbageCount;
			}

		private:

			SObserver* FindObserver(CUpdateScope& scope, UpdatePriority priority);

			ObserverVector m_observers;
			size_t         m_dirtyCount;
			size_t         m_garbageCount;
			size_t         m_pos;
		};

		static const size_t s_bucketCount = 1 << (EUpdateFrequency::Count - 1);

		SlotVector        m_slots;
		CBucket           m_buckets[s_bucketCount];
		float             m_frameTimes[s_bucketCount];
		size_t            m_currentBucketIdx;
		bool              m_bInFrame;
		SFrameUpdateStats m_stats;
		// Relevance grid related members
		CRelevanceGrid                              m_relevanceGrid;
		IUpdateScheduler::UseRelevanceGridPredicate m_useRelevanceGrid;
		IUpdateScheduler::IsDynamicObjectPredicate  m_isDynamicObject;
	};
}
