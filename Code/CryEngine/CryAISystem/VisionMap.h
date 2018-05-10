// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a visibility determination map for use with AI.

   -------------------------------------------------------------------------
   History:
   - 4:3:2009   11:38 : Created by Márcio Martins

*************************************************************************/
#ifndef __VisionMap_h__
#define __VisionMap_h__

#pragma once

#include <CryAISystem/IVisionMap.h>

#include <CryAISystem/HashGrid.h>

#ifdef CRYAISYSTEM_DEBUG
	#define VISIONMAP_DEBUG 1
#endif

class CVisionMap : public IVisionMap
{
public:
	CVisionMap();
	virtual ~CVisionMap();

	virtual void                    Reset();

	virtual VisionID                CreateVisionID(const char* name);

	virtual void                    RegisterObserver(const ObserverID& observerID, const ObserverParams& observerParams);
	virtual void                    UnregisterObserver(const ObserverID& observerID);

	virtual void                    RegisterObservable(const ObservableID& observableID, const ObservableParams& observerParams);
	virtual void                    UnregisterObservable(const ObservableID& observableID);

	virtual void                    ObserverChanged(const ObserverID& observerID, const ObserverParams& observerParams, uint32 hint);
	virtual void                    ObservableChanged(const ObservableID& observableID, const ObservableParams& newObservableParams, uint32 hint);

	virtual bool                    IsVisible(const ObserverID& observerID, const ObservableID& observableID) const;
	virtual float                   GetNormalizedVisibiliy(const ObserverID& observerID, const ObservableID& observableID) const;

	virtual const ObserverParams*   GetObserverParams(const ObserverID& observerID) const;
	virtual const ObservableParams* GetObservableParams(const ObservableID& observableID) const;

	virtual void                    AddPriorityMapEntry(const PriorityMapEntry& priorityMapEntry);
	virtual void                    ClearPriorityMap();

	virtual void                    Update(float frameTime);

#if VISIONMAP_DEBUG
	void DebugDraw();
#endif

private:
	struct ObservableInfo
	{
		ObservableInfo(const ObservableID& _observableID, const ObservableParams& _observableParams)
			: observableID(_observableID)
			, observableParams(_observableParams) {};

		ObservableID     observableID;
		ObservableParams observableParams;
	};

	struct ObservablePosition
	{
		inline Vec3 operator()(const ObservableInfo* observableInfo) const
		{
			return observableInfo->observableParams.observablePositions[0];
		}
	};

	typedef hash_grid<256, ObservableInfo*, hash_grid_2d<Vec3, Vec3i>, ObservablePosition> ObservablesGrid;
	typedef std::unordered_map<ObservableID, ObservableInfo, stl::hash_uint32>             Observables;

	struct PVSEntry
	{
		PVSEntry(const ObservableInfo& _observableInfo, RayCastRequest::Priority _priority)
			: pendingRayID(0)
			, observableInfo(_observableInfo)
			, visible(false)
			, numberOfSucceededRaycasts(0)
			, numberOfCompletedRaycasts(0)
			, currentTestPositionIndex(0)
			, priority(_priority)
			, needsUpdate(true)
			, lastComputedNormalizedVisibility(0.0f)
#if VISIONMAP_DEBUG
			, obstructionPosition(ZERO)
			, lastObserverPositionChecked(ZERO)
			, lastObservablePositionChecked(ZERO)
			, rayQueueTimestamp(0.0f)
#endif
		{};

		QueuedRayID              pendingRayID;
		RayCastRequest::Priority priority;
		const ObservableInfo&    observableInfo;

		bool                     visible;
		int8                     numberOfSucceededRaycasts;
		int8                     numberOfCompletedRaycasts;
		bool                     needsUpdate;
		int8                     currentTestPositionIndex;
		int8                     firstVisPos;
		float                    lastComputedNormalizedVisibility;

#if VISIONMAP_DEBUG
		Vec3  obstructionPosition;
		Vec3  lastObserverPositionChecked;
		Vec3  lastObservablePositionChecked;
		float rayQueueTimestamp;
#endif
	};

	typedef std::map<ObservableID, PVSEntry> PVS;

	struct ObserverInfo
	{
		ObserverInfo(ObserverID _observerID)
			: observerID(_observerID)
			, needsPVSUpdate(false)
			, needsVisibilityUpdate(false)
			, queuedForPVSUpdate(false)
			, queuedForVisibilityUpdate(false)
			, updateAllVisibilityStatus(false)
			, nextPVSUpdateTime(0.0f)
			, nextVisibilityUpdateTime(0.0f)
		{
		}

		ObserverID     observerID;
		ObserverParams observerParams;

		PVS            pvs;
		bool           needsPVSUpdate;
		bool           needsVisibilityUpdate;
		bool           queuedForPVSUpdate;
		bool           queuedForVisibilityUpdate;
		bool           updateAllVisibilityStatus;
		CTimeValue     nextPVSUpdateTime;
		CTimeValue     nextVisibilityUpdateTime;
#if VISIONMAP_DEBUG
		CTimeValue     queuedForPVSUpdateTime;
		CTimeValue     queuedForVisibilityUpdateTime;
#endif
	};

	typedef std::unordered_map<ObserverID, ObserverInfo, stl::hash_uint32> Observers;

	struct PendingRayInfo
	{
		PendingRayInfo(const ObserverInfo& _observerInfo, PVSEntry& _pvsEntry)
			: observerInfo(_observerInfo)
			, pvsEntry(_pvsEntry)
#if VISIONMAP_DEBUG
			, observerPosition(ZERO)
			, observablePosition(ZERO)
#endif
		{
		}

		const ObserverInfo& observerInfo;
		PVSEntry&           pvsEntry;

#if VISIONMAP_DEBUG
		Vec3 observerPosition;
		Vec3 observablePosition;
#endif
	};

	std::vector<PriorityMapEntry> m_priorityMap;

	void AcquireSkipList(IPhysicalEntity** skipList, uint32 skipListSize);
	void ReleaseSkipList(IPhysicalEntity** skipList, uint32 skipListSize);

#if VISIONMAP_DEBUG
	void UpdateDebugVisionMapObjects();
	void DebugDrawObservers();
	void DebugDrawObservables();
	void DebugDrawVisionMapStats();
#endif

	void                     AddToObserverPVS(ObserverInfo& observerInfo, const ObservableInfo& observableInfo);

	void                     UpdateObservers();
	void                     UpdatePVS(ObserverInfo& observerInfo);

	bool                     ShouldBeAddedToObserverPVS(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;

	void                     UpdateVisibilityStatus(ObserverInfo& observerInfo);

	bool                     ShouldObserve(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;
	bool                     IsInSightRange(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;
	bool                     IsInFoV(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;
	bool                     IsInStatisticalAnalysisSightRange(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;
	bool                     IsUserConditionSatisfied(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const;

	void                     QueueRay(const ObserverInfo& observerInfo, PVSEntry& pvsEntry);
	bool                     RayCastSubmit(const QueuedRayID& queuedRayID, RayCastRequest& request);
	void                     RayCastComplete(const QueuedRayID& queuedRayID, const RayCastResult& rayCastResult);
	RayCastRequest::Priority GetRayCastRequestPriority(const ObserverParams& observerParams, const ObservableParams& observable);

	void                     DeletePendingRay(PVSEntry& pvsEntry);
	void                     DeletePendingRays(PVS& pvs);

	void                     TriggerObserverCallback(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo, bool visible);
	void                     TriggerObservableCallback(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo, bool visible);

	Observers       m_observers;
	Observables     m_observables;
	ObservablesGrid m_observablesGrid;

	typedef std::deque<ObserverID> ObserverQueue;
	ObserverQueue m_observerPVSUpdateQueue;
	ObserverQueue m_observerVisibilityUpdateQueue;

	typedef std::vector<std::pair<float, ObservableInfo*>> QueryObservables;
	QueryObservables m_queryObservables;

	typedef std::map<QueuedRayID, PendingRayInfo> PendingRays;
	PendingRays m_pendingRays;

#if VISIONMAP_DEBUG
	float      m_debugTimer;
	uint32     m_numberOfPVSUpdatesThisFrame;
	uint32     m_numberOfVisibilityUpdatesThisFrame;
	uint32     m_numberOfRayCastsSubmittedThisFrame;

	VisionID   m_debugObserverVisionID;
	VisionID   m_debugObservableVisionID;

	CTimeValue m_pvsUpdateQueueLatency;
	CTimeValue m_visibilityUpdateQueueLatency;

	struct LatencyInfo
	{
		struct
		{
			float latency;
			float occurred;
		}   buffer[512];

		int bufferIndex;
		int usedBufferSize;
	};

	LatencyInfo m_latencyInfo[RayCastRequest::TotalNumberOfPriorities];

	int         m_pendingRayCounts[RayCastRequest::TotalNumberOfPriorities];
#endif

	uint32 m_visionIdCounter;
};

#endif // __VisionMap_h__
