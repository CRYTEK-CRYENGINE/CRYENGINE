// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 4:3:2009   11:38 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "VisionMap.h"

#include "DebugDrawContext.h"
#include <CryAISystem/VisionMapTypes.h>

namespace
{
static const float positionEpsilon = 0.05f;
static const float orientationEpsilon = 0.05f;
}

CVisionMap::CVisionMap()
	: m_visionIdCounter(0)
{
	Reset();
}

CVisionMap::~CVisionMap()
{
	Reset();
}

void CVisionMap::Reset()
{
	static_assert(ObserverParams::MaxSkipListSize + ObservableParams::MaxSkipListSize <= RayCastRequest::MaxSkipListCount, "Unexpected enum value!");

	for (Observers::iterator observerIt = m_observers.begin(), end = m_observers.end(); observerIt != end; ++observerIt)
	{
		ObserverInfo& observerInfo = observerIt->second;

		ReleaseSkipList(&observerInfo.observerParams.skipList[0], observerInfo.observerParams.skipListSize);
		DeletePendingRays(observerIt->second.pvs);
	}

	for (Observables::iterator observableIt = m_observables.begin(), end = m_observables.end(); observableIt != end; ++observableIt)
	{
		ObservableInfo& observableInfo = observableIt->second;

		ReleaseSkipList(&observableInfo.observableParams.skipList[0], observableInfo.observableParams.skipListSize);
	}

#if VISIONMAP_DEBUG
	m_debugTimer = 0.0f;
	m_numberOfPVSUpdatesThisFrame = 0;
	m_numberOfVisibilityUpdatesThisFrame = 0;
	m_numberOfRayCastsSubmittedThisFrame = 0;
	m_debugObserverVisionID = VisionID();
	m_debugObservableVisionID = VisionID();
	memset(m_latencyInfo, 0, sizeof(m_latencyInfo));
	memset(m_pendingRayCounts, 0, sizeof(m_pendingRayCounts));

	m_pvsUpdateQueueLatency = 0.0f;
	m_visibilityUpdateQueueLatency = 0.0f;
#endif

	m_observers.clear();

	m_observablesGrid.clear();
	m_observables.clear();

	m_observerPVSUpdateQueue.clear();
	m_observerVisibilityUpdateQueue.clear();
}

VisionID CVisionMap::CreateVisionID(const char* name)
{
	++m_visionIdCounter;
	while (!m_visionIdCounter)
		++m_visionIdCounter;

	return VisionID(m_visionIdCounter, name);
}

void CVisionMap::RegisterObserver(const ObserverID& observerID, const ObserverParams& observerParams)
{
	if (!observerID)
		return;

	m_observers.insert(Observers::value_type(observerID, ObserverInfo(observerID)));

	ObserverChanged(observerID, observerParams, eChangedAll);
}

void CVisionMap::UnregisterObserver(const ObserverID& observerID)
{
	if (!observerID)
		return;

	Observers::iterator observerIt = m_observers.find(observerID);
	if (observerIt == m_observers.end())
		return;

	ObserverInfo& observerInfo = observerIt->second;
	ReleaseSkipList(&observerInfo.observerParams.skipList[0], observerInfo.observerParams.skipListSize);
	DeletePendingRays(observerInfo.pvs);

	if (observerInfo.queuedForPVSUpdate)
		stl::find_and_erase(m_observerPVSUpdateQueue, observerInfo.observerID);

	if (observerInfo.queuedForVisibilityUpdate)
		stl::find_and_erase(m_observerVisibilityUpdateQueue, observerInfo.observerID);

	for (const auto& pvsIdEntryPair : observerInfo.pvs)
	{
		const PVSEntry& pvsEntry = pvsIdEntryPair.second;
		if (pvsEntry.visible)
		{
			TriggerObserverCallback(observerInfo, pvsEntry.observableInfo, false);
			TriggerObservableCallback(observerInfo, pvsEntry.observableInfo, false);
		}
	}

	m_observers.erase(observerIt);
}

void CVisionMap::RegisterObservable(const ObservableID& observableID, const ObservableParams& observableParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!observableID)
		return;

	assert(observableParams.observablePositionsCount > 0);
	assert(observableParams.observablePositionsCount <= ObservableParams::MaxPositionCount);

	std::pair<Observables::iterator, bool> result = m_observables.insert(Observables::value_type(observableID, ObservableInfo(observableID, ObservableParams())));

	ObservableInfo& insertedObservableInfo = result.first->second;
	m_observablesGrid.insert(insertedObservableInfo.observableParams.observablePositions[0], &insertedObservableInfo);

	ObservableChanged(observableID, observableParams, eChangedAll);

	for (Observers::iterator observerIt = m_observers.begin(), end = m_observers.end(); observerIt != end; ++observerIt)
	{
		ObserverInfo& observerInfo = observerIt->second;
		if (ShouldBeAddedToObserverPVS(observerInfo, insertedObservableInfo))
			AddToObserverPVS(observerInfo, insertedObservableInfo);
	}
}

void CVisionMap::UnregisterObservable(const ObservableID& observableID)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!observableID)
		return;

	Observables::iterator observableIt = m_observables.find(observableID);
	if (observableIt == m_observables.end())
		return;

	ObservableInfo& observableInfo = observableIt->second;
	ReleaseSkipList(&observableInfo.observableParams.skipList[0], observableInfo.observableParams.skipListSize);

	for (Observers::iterator observerIt = m_observers.begin(), end = m_observers.end(); observerIt != end; ++observerIt)
	{
		if (observerIt->first == observableID)
			continue;

		ObserverInfo& observerInfo = observerIt->second;

		PVS::iterator pvsIt = observerInfo.pvs.find(observableID);
		if (pvsIt == observerInfo.pvs.end())
			continue;

		PVSEntry& pvsEntry = pvsIt->second;

		if (pvsEntry.visible)
		{
			TriggerObserverCallback(observerInfo, pvsEntry.observableInfo, false);
			TriggerObservableCallback(observerInfo, pvsEntry.observableInfo, false);
		}

		DeletePendingRay(pvsEntry);

		observerInfo.needsPVSUpdate = true;
		observerInfo.pvs.erase(pvsIt);
	}

	m_observablesGrid.erase(observableInfo.observableParams.observablePositions[0], &observableInfo);
	m_observables.erase(observableIt);
}

void CVisionMap::ObserverChanged(const ObserverID& observerID, const ObserverParams& newObserverParams, uint32 hint)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Observers::iterator it = m_observers.find(observerID);
	assert(it != m_observers.end());
	if (it == m_observers.end())
		return;

	bool needsUpdate = false;
	ObserverInfo& observerInfo = it->second;
	ObserverParams& currentObserverParams = observerInfo.observerParams;

	if (hint & eChangedFaction)
	{
		currentObserverParams.faction = newObserverParams.faction;
		needsUpdate = true;
	}

	if (hint & eChangedFactionsToObserveMask)
	{
		currentObserverParams.factionsToObserveMask = newObserverParams.factionsToObserveMask;
	}

	if (hint & eChangedTypesToObserveMask)
	{
		currentObserverParams.typesToObserveMask = newObserverParams.typesToObserveMask;
	}

	if (hint & eChangedSightRange)
	{
		currentObserverParams.sightRange = newObserverParams.sightRange;
		needsUpdate = true;
	}

	if (hint & eChangedFOV)
	{
		currentObserverParams.fovCos = newObserverParams.fovCos;
		needsUpdate = true;
	}

	if (hint & eChangedPosition)
	{
		if (!IsEquivalent(currentObserverParams.eyePosition, newObserverParams.eyePosition, positionEpsilon))
		{
			currentObserverParams.eyePosition = newObserverParams.eyePosition;
			needsUpdate = true;
		}
	}

	if (hint & eChangedOrientation)
	{
		if (!IsEquivalent(currentObserverParams.eyeDirection, newObserverParams.eyeDirection, orientationEpsilon))
		{
			currentObserverParams.eyeDirection = newObserverParams.eyeDirection;
			needsUpdate = true;
		}
	}

	if (hint & eChangedSkipList)
	{
		assert(newObserverParams.skipListSize <= ObserverParams::MaxSkipListSize);

		uint32 skipListSize = std::min<uint32>(newObserverParams.skipListSize, ObserverParams::MaxSkipListSize);
		AcquireSkipList(const_cast<IPhysicalEntity**>(&newObserverParams.skipList[0]), skipListSize);
		ReleaseSkipList(&currentObserverParams.skipList[0], currentObserverParams.skipListSize);

		currentObserverParams.skipListSize = skipListSize;
		for (int i = 0; i < currentObserverParams.skipListSize; ++i)
			currentObserverParams.skipList[i] = newObserverParams.skipList[i];

#ifdef _DEBUG
		std::sort(&currentObserverParams.skipList[0], &currentObserverParams.skipList[currentObserverParams.skipListSize]);

		for (uint i = 1; i < currentObserverParams.skipListSize; ++i)
		{
			assert(currentObserverParams.skipList[i - 1] != currentObserverParams.skipList[i]);
		}
#endif
	}

	if (hint & eChangedCallback)
	{
		currentObserverParams.callback = newObserverParams.callback;
	}

	if (hint & eChangedUserData)
	{
		currentObserverParams.userData = newObserverParams.userData;
	}

	if (hint & eChangedTypeMask)
	{
		currentObserverParams.typeMask = newObserverParams.typeMask;
		needsUpdate = true;
	}

	if (hint & eChangedRaycastFlags)
	{
		currentObserverParams.raycastFlags = newObserverParams.raycastFlags;
		needsUpdate = true;
	}

	if (hint & eChangedEntityId)
	{
		currentObserverParams.entityId = newObserverParams.entityId;
	}

	if (hint & eChangedUserCondition)
	{
		needsUpdate = true;
	}

	if (hint & eChangedUserConditionCallback)
	{
		currentObserverParams.userConditionCallback = newObserverParams.userConditionCallback;
		needsUpdate = true;
	}

	if (needsUpdate)
	{
		observerInfo.needsPVSUpdate = true;
		observerInfo.needsVisibilityUpdate = true;
		observerInfo.updateAllVisibilityStatus = true;
	}
}

void CVisionMap::ObservableChanged(const ObservableID& observableID, const ObservableParams& newObservableParams, uint32 hint)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	Observables::iterator observableIt = m_observables.find(observableID);
	assert(observableIt != m_observables.end());
	if (observableIt == m_observables.end())
		return;

	ObservableInfo& observableInfo = observableIt->second;
	ObservableParams& currentObservableParams = observableInfo.observableParams;

	bool observableVisibilityPotentiallyChanged = false;

	if (hint & eChangedPosition)
	{
		assert(newObservableParams.observablePositionsCount > 0);
		assert(newObservableParams.observablePositionsCount <= ObservableParams::MaxPositionCount);
		assert(newObservableParams.observablePositions[0].IsValid());

		Vec3 oldPosition = currentObservableParams.observablePositions[0];

		if (!IsEquivalent(oldPosition, newObservableParams.observablePositions[0], positionEpsilon))
		{
			CRY_PROFILE_REGION(PROFILE_AI, "CVisionMap::ObservableChanged_UpdateHashGrid");

			currentObservableParams.observablePositions[0] = newObservableParams.observablePositions[0];
			m_observablesGrid.move(m_observablesGrid.find(oldPosition, &observableInfo), newObservableParams.observablePositions[0]);
			observableVisibilityPotentiallyChanged = true;

			currentObservableParams.observablePositionsCount = newObservableParams.observablePositionsCount;

			for (int i = 1; i < currentObservableParams.observablePositionsCount; ++i)
			{
				assert(newObservableParams.observablePositions[i].IsValid());
				currentObservableParams.observablePositions[i] = newObservableParams.observablePositions[i];
			}
		}
	}

	if (hint & eChangedUserData)
	{
		currentObservableParams.userData = newObservableParams.userData;
	}

	if (hint & eChangedCallback)
	{
		currentObservableParams.callback = newObservableParams.callback;
	}

	if (hint & eChangedSkipList)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "CVisionMap::ObservableChanged_UpdateSkipList");

		assert(newObservableParams.skipListSize <= ObserverParams::MaxSkipListSize);

		uint32 skipListSize = std::min<uint32>(newObservableParams.skipListSize, ObserverParams::MaxSkipListSize);
		AcquireSkipList(const_cast<IPhysicalEntity**>(&newObservableParams.skipList[0]), skipListSize);
		ReleaseSkipList(&currentObservableParams.skipList[0], currentObservableParams.skipListSize);

		currentObservableParams.skipListSize = skipListSize;
		for (int i = 0; i < currentObservableParams.skipListSize; ++i)
			currentObservableParams.skipList[i] = newObservableParams.skipList[i];

#ifdef _DEBUG
		std::sort(&currentObservableParams.skipList[0], &currentObservableParams.skipList[currentObservableParams.skipListSize]);

		for (uint i = 1; i < currentObservableParams.skipListSize; ++i)
		{
			assert(currentObservableParams.skipList[i - 1] != currentObservableParams.skipList[i]);
		}
#endif
	}

	if (hint & eChangedFaction)
	{
		currentObservableParams.faction = newObservableParams.faction;
		observableVisibilityPotentiallyChanged = true;
	}

	if (hint & eChangedTypeMask)
	{
		currentObservableParams.typeMask = newObservableParams.typeMask;
		observableVisibilityPotentiallyChanged = true;
	}

	if (hint & eChangedEntityId)
	{
		currentObservableParams.entityId = newObservableParams.entityId;
	}

	if (hint & eChangedUserCondition)
	{
		observableVisibilityPotentiallyChanged = true;
	}

	if (hint & eChangedObservableMode)
	{
		currentObservableParams.mode = newObservableParams.mode;
		currentObservableParams.collectFullStatisticsOnObservableMaxRange = newObservableParams.collectFullStatisticsOnObservableMaxRange;
	}

	if (observableVisibilityPotentiallyChanged)
	{
		CRY_PROFILE_REGION(PROFILE_AI, "CVisionMap::ObservableChanged_VisibilityChanged");

		CTimeValue now = gEnv->pTimer->GetFrameStartTime();

		for (Observers::iterator observersIt = m_observers.begin(), end = m_observers.end(); observersIt != end; ++observersIt)
		{
			ObserverInfo& observerInfo = observersIt->second;

			if (observerInfo.observerID == observableID)
				continue;

			if (observerInfo.needsPVSUpdate)
				continue;

			PVS::iterator pvsIt = observerInfo.pvs.find(observableID);
			bool inObserverPVS = pvsIt != observerInfo.pvs.end();

			if (ShouldObserve(observerInfo, observableInfo))
			{
				if (inObserverPVS)
				{
					PVSEntry& pvsEntry = pvsIt->second;
					pvsEntry.needsUpdate = true;
					observerInfo.needsVisibilityUpdate = true;
				}
				else
				{
					AddToObserverPVS(observerInfo, observableInfo);
				}
			}
			else
			{
				if (inObserverPVS)
				{
					PVSEntry& pvsEntry = pvsIt->second;

					if (pvsEntry.visible)
					{
						TriggerObserverCallback(observerInfo, observableInfo, false);
						TriggerObservableCallback(observerInfo, observableInfo, false);
					}

					DeletePendingRay(pvsEntry);

					observerInfo.pvs.erase(pvsIt);
				}
			}
		}
	}
}

bool CVisionMap::IsVisible(const ObserverID& observerID, const ObservableID& observableID) const
{
	Observers::const_iterator observerIt = m_observers.find(observerID);
	if (observerIt == m_observers.end())
		return false;

	const ObserverInfo& observerInfo = observerIt->second;
	PVS::const_iterator pvsIt = observerInfo.pvs.find(observableID);
	if (pvsIt == observerInfo.pvs.end())
		return false;

	return pvsIt->second.visible;
}

float CVisionMap::GetNormalizedVisibiliy(const ObserverID& observerID, const ObservableID& observableID) const
{
	Observers::const_iterator observerIt = m_observers.find(observerID);
	if (observerIt == m_observers.end())
		return 0.0f;

	const ObserverInfo& observerInfo = observerIt->second;
	PVS::const_iterator pvsIt = observerInfo.pvs.find(observableID);
	if (pvsIt == observerInfo.pvs.end())
		return 0.0f;

	const ObservableInfo& observableInfo = pvsIt->second.observableInfo;
	if (observableInfo.observableParams.mode == EObservableMode::Statistical && 
		IsInStatisticalAnalysisSightRange(observerInfo, observableInfo))
	{
		return pvsIt->second.lastComputedNormalizedVisibility;
	}

	return pvsIt->second.visible ? 1.0f : 0.0f;
}

const ObserverParams* CVisionMap::GetObserverParams(const ObserverID& observerID) const
{
	Observers::const_iterator obsIt = m_observers.find(observerID);
	if (obsIt == m_observers.end())
		return 0;

	const ObserverInfo& observerInfo = obsIt->second;

	return &observerInfo.observerParams;
}

const ObservableParams* CVisionMap::GetObservableParams(const ObservableID& observableID) const
{
	Observables::const_iterator obsIt = m_observables.find(observableID);
	if (obsIt == m_observables.end())
		return 0;

	const ObservableInfo& observableInfo = obsIt->second;

	return &observableInfo.observableParams;
}

void CVisionMap::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if VISIONMAP_DEBUG
	m_numberOfPVSUpdatesThisFrame = 0;
	m_numberOfVisibilityUpdatesThisFrame = 0;
	m_numberOfRayCastsSubmittedThisFrame = 0;
	m_debugTimer += frameTime;
#endif

	UpdateObservers();
}

bool CVisionMap::IsInStatisticalAnalysisSightRange(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	if (observableInfo.observableParams.collectFullStatisticsOnObservableMaxRange <= 0.0f)
		return true;

	const float distanceSquare = (observableInfo.observableParams.observablePositions[0] - observerInfo.observerParams.eyePosition).len2();
	return distanceSquare <= sqr(observableInfo.observableParams.collectFullStatisticsOnObservableMaxRange);
}

bool CVisionMap::IsInSightRange(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	if (observerInfo.observerParams.sightRange <= 0.0f)
		return true;

	const float distanceSquare = (observableInfo.observableParams.observablePositions[0] - observerInfo.observerParams.eyePosition).len2();
	return distanceSquare <= sqr(observerInfo.observerParams.sightRange);
}

bool CVisionMap::IsInFoV(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	if (observerInfo.observerParams.fovCos <= -1.0f)
		return true;

	for (int i = 0; i < observableInfo.observableParams.observablePositionsCount; i++)
	{
		Vec3 directionToObservablePosition = (observableInfo.observableParams.observablePositions[i] - observerInfo.observerParams.eyePosition);
		directionToObservablePosition.NormalizeSafe();
		const float dot = directionToObservablePosition.Dot(observerInfo.observerParams.eyeDirection);
		if (observerInfo.observerParams.fovCos <= dot)
			return true;
	}

	return false;
}

bool CVisionMap::IsUserConditionSatisfied(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	if (!observerInfo.observerParams.userConditionCallback)
		return true;

	return observerInfo.observerParams.userConditionCallback(observerInfo.observerID, observerInfo.observerParams, observableInfo.observableID, observableInfo.observableParams);
}

bool CVisionMap::ShouldObserve(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	const bool matchesType = ((observerInfo.observerParams.typesToObserveMask & observableInfo.observableParams.typeMask) != 0);
	if (!matchesType)
		return false;

	const bool matchesFaction = ((observerInfo.observerParams.factionsToObserveMask & (1 << observableInfo.observableParams.faction)) != 0);
	if (!matchesFaction)
		return false;

	const bool observingMyself = (observerInfo.observerParams.entityId == observableInfo.observableParams.entityId);
	if (observingMyself)
		return false;

	if (!IsInSightRange(observerInfo, observableInfo))
		return false;

	if (!IsInFoV(observerInfo, observableInfo))
		return false;

	if (!IsUserConditionSatisfied(observerInfo, observableInfo))
		return false;

	return true;
}

RayCastRequest::Priority CVisionMap::GetRayCastRequestPriority(const ObserverParams& observerParams, const ObservableParams& observableParams)
{
	RayCastRequest::Priority priority = RayCastRequest::MediumPriority;

	uint32 fromTypeMask = observerParams.typeMask;
	uint32 toTypeMask = observableParams.typeMask;
	uint8 fromFaction = observerParams.faction;
	uint8 toFaction = observableParams.faction;

	for (std::vector<PriorityMapEntry>::iterator it = m_priorityMap.begin(), itEnd = m_priorityMap.end(); it != itEnd; ++it)
	{
		const PriorityMapEntry& priorityMapEntry = *it;

		if ((priorityMapEntry.fromTypeMask & fromTypeMask) &&
		    (priorityMapEntry.fromFactionMask & (1 << fromFaction)) &&
		    (priorityMapEntry.toTypeMask & toTypeMask) &&
		    (priorityMapEntry.toFactionMask & (1 << toFaction)))
		{
			switch (priorityMapEntry.priority)
			{
			case eLowPriority:
				priority = RayCastRequest::LowPriority;
				break;

			case eMediumPriority:
				priority = RayCastRequest::MediumPriority;
				break;

			case eHighPriority:
				priority = RayCastRequest::HighPriority;
				break;

			case eVeryHighPriority:
				priority = RayCastRequest::HighestPriority;
				break;

			default:
				CRY_ASSERT("bad priority specified in vision map priority table");
				break;
			}
			break;
		}
	}

	return priority;
}

void CVisionMap::AddToObserverPVS(ObserverInfo& observerInfo, const ObservableInfo& observableInfo)
{
	observerInfo.needsVisibilityUpdate = true;
	const RayCastRequest::Priority priority = GetRayCastRequestPriority(observerInfo.observerParams, observableInfo.observableParams);
	std::pair<PVS::iterator, bool> result = observerInfo.pvs.insert(PVS::value_type(observableInfo.observableID, PVSEntry(observableInfo, priority)));
	assert(result.second);
}

void CVisionMap::UpdatePVS(ObserverInfo& observerInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

#if VISIONMAP_DEBUG
	++m_numberOfPVSUpdatesThisFrame;
#endif

	PVS& pvs = observerInfo.pvs;

	// Step1:
	// - Make sure everything in the PVS is in supposed to be there
	// - Delete what it's not
	{
		CRY_PROFILE_REGION(PROFILE_AI, "UpdatePVS_Step1");

		for (PVS::iterator pvsIt = pvs.begin(), end = pvs.end(); pvsIt != end; )
		{
			const ObservableID& observableID = pvsIt->first;
			const ObservableInfo& observableInfo = pvsIt->second.observableInfo;

			if (!ShouldObserve(observerInfo, observableInfo))
			{
				PVSEntry& pvsEntry = pvsIt->second;
				PVS::iterator nextIt = pvsIt;
				++nextIt;

				if (pvsEntry.visible)
				{
					TriggerObserverCallback(observerInfo, observableInfo, false);
					TriggerObservableCallback(observerInfo, observableInfo, false);
				}

				DeletePendingRay(pvsEntry);

				pvs.erase(pvsIt);
				pvsIt = nextIt;

				continue;
			}

			++pvsIt;
		}
	}

	// Step2:
	// - Go through all objects in range
	// - If object is already in the PVS skip it
	// - Otherwise check if it should be added and add it
	{
		CRY_PROFILE_REGION(PROFILE_AI, "UpdatePVS_Step2");

		if (observerInfo.observerParams.sightRange > 0.0f)
		{
			// the sight range is defined query the obsevableGrid
			m_queryObservables.clear();
			uint32 observableCount = m_observablesGrid.query_sphere_distance(observerInfo.observerParams.eyePosition, observerInfo.observerParams.sightRange, m_queryObservables);

			for (QueryObservables::iterator it = m_queryObservables.begin(), end = m_queryObservables.end(); it != end; ++it)
			{
				const ObservableInfo& observableInfo = *it->second;
				if (ShouldBeAddedToObserverPVS(observerInfo, observableInfo))
					AddToObserverPVS(observerInfo, observableInfo);
			}
		}
		else
		{
			// the sight range is unlimited check all observables
			for (Observables::iterator it = m_observables.begin(), end = m_observables.end(); it != end; ++it)
			{
				ObservableInfo& observableInfo = it->second;
				if (ShouldBeAddedToObserverPVS(observerInfo, observableInfo))
					AddToObserverPVS(observerInfo, observableInfo);
			}
		}
	}
}

bool CVisionMap::ShouldBeAddedToObserverPVS(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo) const
{
	if (observableInfo.observableID == observerInfo.observerID)
		return false;

	PVS::const_iterator pvsIt = observerInfo.pvs.find(observableInfo.observableID);
	if (pvsIt != observerInfo.pvs.end())
		return false;

	return ShouldObserve(observerInfo, observableInfo);
}

void CVisionMap::QueueRay(const ObserverInfo& observerInfo, PVSEntry& pvsEntry)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	assert(!pvsEntry.pendingRayID);

	const QueuedRayID queuedRayID = gAIEnv.pRayCaster->Queue(
	  pvsEntry.priority,
	  functor(*this, &CVisionMap::RayCastComplete),
	  functor(*this, &CVisionMap::RayCastSubmit));
	assert(queuedRayID);

	std::pair<PendingRays::iterator, bool> result = m_pendingRays.insert(
	  PendingRays::value_type(queuedRayID, PendingRayInfo(observerInfo, pvsEntry)));
	assert(result.second);

#if VISIONMAP_DEBUG
	if (queuedRayID)
	{
		m_pendingRayCounts[pvsEntry.priority]++;
		pvsEntry.rayQueueTimestamp = m_debugTimer;
	}
#endif

	pvsEntry.pendingRayID = queuedRayID;
}

void CVisionMap::DeletePendingRay(PVSEntry& pvsEntry)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (!pvsEntry.pendingRayID)
		return;

	gAIEnv.pRayCaster->Cancel(pvsEntry.pendingRayID);
	m_pendingRays.erase(pvsEntry.pendingRayID);
	pvsEntry.pendingRayID = 0;

#if VISIONMAP_DEBUG
	m_pendingRayCounts[pvsEntry.priority]--;
#endif
}

void CVisionMap::DeletePendingRays(PVS& pvs)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	for (PVS::iterator pvsIt = pvs.begin(), end = pvs.end(); pvsIt != end; ++pvsIt)
	{
		PVSEntry& pvsEntry = pvsIt->second;
		DeletePendingRay(pvsEntry);
	}
}

bool CVisionMap::RayCastSubmit(const QueuedRayID& queuedRayID, RayCastRequest& rayCastRequest)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	PendingRays::iterator pendingRayIt = m_pendingRays.find(queuedRayID);

	assert(pendingRayIt != m_pendingRays.end());
	if (pendingRayIt == m_pendingRays.end())
		return false;

	PendingRayInfo& pendingRayInfo = pendingRayIt->second;

	const ObserverParams& observerParams = pendingRayInfo.observerInfo.observerParams;
	const ObservableParams& observableParams = pendingRayInfo.pvsEntry.observableInfo.observableParams;

	const Vec3& observerPosition = observerParams.eyePosition;
	const Vec3& observablePosition = observableParams.observablePositions[pendingRayInfo.pvsEntry.currentTestPositionIndex];

#if VISIONMAP_DEBUG
	pendingRayInfo.observerPosition = observerPosition;
	pendingRayInfo.observablePosition = observablePosition;
#endif

	rayCastRequest.pos = observerPosition;
	rayCastRequest.dir = observablePosition - observerPosition;
	rayCastRequest.objTypes = COVER_OBJECT_TYPES;
	rayCastRequest.flags = observerParams.raycastFlags;
	rayCastRequest.maxHitCount = 2;

	size_t skipListCount = observerParams.skipListSize + observableParams.skipListSize;
	assert(rayCastRequest.skipListCount <= RayCastRequest::MaxSkipListCount);

	skipListCount = std::min<size_t>(skipListCount, RayCastRequest::MaxSkipListCount);
	rayCastRequest.skipListCount = static_cast<uint8>(skipListCount);

	size_t observerSkipListCount = std::min<size_t>(observerParams.skipListSize, RayCastRequest::MaxSkipListCount);
	memcpy(rayCastRequest.skipList, observerParams.skipList, observerSkipListCount * sizeof(IPhysicalEntity*));

	size_t observableSkipListCount =
	  std::min<size_t>(observableParams.skipListSize, RayCastRequest::MaxSkipListCount - observerSkipListCount);
	memcpy(rayCastRequest.skipList + observerSkipListCount, observableParams.skipList, observableSkipListCount * sizeof(IPhysicalEntity*));

#if VISIONMAP_DEBUG
	m_numberOfRayCastsSubmittedThisFrame++;

	float latency = m_debugTimer - pendingRayInfo.pvsEntry.rayQueueTimestamp;
	LatencyInfo* latencyInfo = &m_latencyInfo[pendingRayInfo.pvsEntry.priority];
	latencyInfo->buffer[latencyInfo->bufferIndex].latency = latency;
	latencyInfo->buffer[latencyInfo->bufferIndex].occurred = m_debugTimer;
	latencyInfo->bufferIndex = (latencyInfo->bufferIndex + 1) % CRY_ARRAY_COUNT(latencyInfo->buffer);
	latencyInfo->usedBufferSize = std::max(latencyInfo->usedBufferSize, latencyInfo->bufferIndex);
#endif

	return true;
}

void CVisionMap::RayCastComplete(const QueuedRayID& queuedRayID, const RayCastResult& rayCastResult)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	PendingRays::iterator pendingRayIt = m_pendingRays.find(queuedRayID);

	assert(pendingRayIt != m_pendingRays.end());
	if (pendingRayIt == m_pendingRays.end())
		return;

	PendingRayInfo& pendingRayInfo = pendingRayIt->second;
	PVSEntry& pvsEntry = pendingRayInfo.pvsEntry;
	pvsEntry.pendingRayID = 0;

#if VISIONMAP_DEBUG
	m_pendingRayCounts[pvsEntry.priority]--;
#endif
	bool visible = !rayCastResult;

#if VISIONMAP_DEBUG
	pvsEntry.lastObserverPositionChecked = pendingRayInfo.observerPosition;
	pvsEntry.lastObservablePositionChecked = pendingRayInfo.observablePosition;
#endif
	const ObserverInfo& observerInfo = pendingRayInfo.observerInfo;
	const ObservableInfo& observableInfo = pvsEntry.observableInfo;
	const bool requiresStatisticalAnalysis = (observableInfo.observableParams.mode == EObservableMode::Statistical) &&
		IsInStatisticalAnalysisSightRange(observerInfo, observableInfo);
	static_assert(static_cast<int>(EObservableMode::Count) == 2, "Enum changed!");

	if (!visible)
	{
#if VISIONMAP_DEBUG
		pvsEntry.obstructionPosition = rayCastResult->pt;
#endif
		if (!requiresStatisticalAnalysis)
		{
			if (++pvsEntry.currentTestPositionIndex < observableInfo.observableParams.observablePositionsCount)
			{
				m_pendingRays.erase(pendingRayIt);
				QueueRay(observerInfo, pvsEntry);
				return;
			}
		}
	}

	bool isLastRaycast = false;
	if (requiresStatisticalAnalysis)
	{
		pvsEntry.numberOfSucceededRaycasts += visible ? 1 : 0;
		pvsEntry.numberOfCompletedRaycasts++;

		isLastRaycast = (pvsEntry.numberOfCompletedRaycasts == observableInfo.observableParams.observablePositionsCount);

		if (visible && !pvsEntry.visible)
		{
			pvsEntry.firstVisPos = pvsEntry.currentTestPositionIndex;
		}

		if (isLastRaycast)
		{
			const float newNormalizedVisibility = (float)pvsEntry.numberOfSucceededRaycasts / (float)pvsEntry.numberOfCompletedRaycasts;
			if (newNormalizedVisibility != pvsEntry.lastComputedNormalizedVisibility)
			{
				pvsEntry.lastComputedNormalizedVisibility = newNormalizedVisibility;
			}
		}

		if (++pvsEntry.currentTestPositionIndex < observableInfo.observableParams.observablePositionsCount)
		{
			m_pendingRays.erase(pendingRayIt);
			QueueRay(observerInfo, pvsEntry);
			return;
		}
	}
	else
	{
		pvsEntry.currentTestPositionIndex = 0;
	}

	const bool triggersCallback =
		(!requiresStatisticalAnalysis && pvsEntry.visible != visible) ||                                                  // default mode     - changed visibility
		(requiresStatisticalAnalysis && (visible && !pvsEntry.visible && !isLastRaycast)) ||                             // statistical mode - became visible
		(requiresStatisticalAnalysis && (pvsEntry.numberOfSucceededRaycasts == 0 && pvsEntry.visible && isLastRaycast)); // statistical mode - became invisible

	if (triggersCallback)
	{
		pvsEntry.visible = visible;

		TriggerObserverCallback(observerInfo, observableInfo, visible);
		TriggerObservableCallback(observerInfo, observableInfo, visible);

		// find the pending ray again in case it was removed by the callback
		PendingRays::iterator pendingRaysToBeErasedIt = m_pendingRays.find(queuedRayID);
		if (pendingRaysToBeErasedIt != m_pendingRays.end())
			m_pendingRays.erase(pendingRaysToBeErasedIt);
	}
	else
	{
		m_pendingRays.erase(pendingRayIt);
	}

	if (requiresStatisticalAnalysis && isLastRaycast)
	{
		pvsEntry.currentTestPositionIndex = 0;
		pvsEntry.numberOfCompletedRaycasts = 0;
		pvsEntry.numberOfSucceededRaycasts = 0;
	}

}

void CVisionMap::AcquireSkipList(IPhysicalEntity** skipList, uint32 skipListSize)
{
	for (uint32 i = 0; i < skipListSize; ++i)
		skipList[i]->AddRef();
}

void CVisionMap::ReleaseSkipList(IPhysicalEntity** skipList, uint32 skipListSize)
{
	for (uint32 i = 0; i < skipListSize; ++i)
		skipList[i]->Release();
}

void CVisionMap::AddPriorityMapEntry(const PriorityMapEntry& priorityMapEntry)
{
	m_priorityMap.push_back(priorityMapEntry);
}

void CVisionMap::ClearPriorityMap()
{
	m_priorityMap.clear();
}

void CVisionMap::UpdateVisibilityStatus(ObserverInfo& observerInfo)
{
#if VISIONMAP_DEBUG
	++m_numberOfVisibilityUpdatesThisFrame;
#endif

	for (PVS::iterator pvsIt = observerInfo.pvs.begin(), end = observerInfo.pvs.end(); pvsIt != end; ++pvsIt)
	{
		PVSEntry& pvsEntry = pvsIt->second;

		if (observerInfo.updateAllVisibilityStatus || pvsEntry.needsUpdate)
		{
			pvsEntry.needsUpdate = false;

			if (pvsEntry.pendingRayID != 0)
				DeletePendingRay(pvsEntry);

			QueueRay(observerInfo, pvsEntry);
		}
	}

	observerInfo.updateAllVisibilityStatus = false;
}

void CVisionMap::UpdateObservers()
{
	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	// Update PVS ////////////////////////////////////////////////////////////

	for (Observers::iterator it = m_observers.begin(), end = m_observers.end(); it != end; ++it)
	{
		ObserverInfo& observerInfo = it->second;

		assert(observerInfo.observerParams.eyePosition.IsValid());

		if (observerInfo.needsPVSUpdate && !observerInfo.queuedForPVSUpdate && now > observerInfo.nextPVSUpdateTime)
		{
			m_observerPVSUpdateQueue.push_back(observerInfo.observerID);
			observerInfo.queuedForPVSUpdate = true;

#if VISIONMAP_DEBUG
			observerInfo.queuedForPVSUpdateTime = now;
#endif
		}
	}

	int numberOfPVSUpdatesLeft = gAIEnv.CVars.VisionMapNumberOfPVSUpdatesPerFrame;
	while (numberOfPVSUpdatesLeft > 0 && m_observerPVSUpdateQueue.size() > 0)
	{
		Observers::iterator observerIt = m_observers.find(m_observerPVSUpdateQueue.front());
		ObserverInfo& observerInfo = observerIt->second;

		UpdatePVS(observerInfo);
		observerInfo.needsPVSUpdate = false;

		m_observerPVSUpdateQueue.pop_front();
		observerInfo.queuedForPVSUpdate = false;

		numberOfPVSUpdatesLeft--;

#if VISIONMAP_DEBUG
		m_pvsUpdateQueueLatency = now - observerInfo.queuedForPVSUpdateTime;
#endif
	}

	// Update Visibility /////////////////////////////////////////////////////
	for (Observers::iterator it = m_observers.begin(), end = m_observers.end(); it != end; ++it)
	{
		ObserverInfo& observerInfo = it->second;

		if (observerInfo.needsVisibilityUpdate && !observerInfo.queuedForVisibilityUpdate && now > observerInfo.nextVisibilityUpdateTime)
		{
			m_observerVisibilityUpdateQueue.push_back(observerInfo.observerID);
			observerInfo.queuedForVisibilityUpdate = true;

#if VISIONMAP_DEBUG
			observerInfo.queuedForVisibilityUpdateTime = now;
#endif
		}
	}

	int numberOfVisibilityUpdatesLeft = gAIEnv.CVars.VisionMapNumberOfVisibilityUpdatesPerFrame;
	while (numberOfVisibilityUpdatesLeft > 0 && m_observerVisibilityUpdateQueue.size() > 0)
	{
		Observers::iterator observerIt = m_observers.find(m_observerVisibilityUpdateQueue.front());
		ObserverInfo& observerInfo = observerIt->second;

		UpdateVisibilityStatus(observerInfo);
		observerInfo.nextVisibilityUpdateTime = now + observerInfo.observerParams.updatePeriod;
		observerInfo.needsVisibilityUpdate = false;

		m_observerVisibilityUpdateQueue.pop_front();
		observerInfo.queuedForVisibilityUpdate = false;

		numberOfVisibilityUpdatesLeft--;

#if VISIONMAP_DEBUG
		m_visibilityUpdateQueueLatency = now - observerInfo.queuedForVisibilityUpdateTime;
#endif
	}
}

void CVisionMap::TriggerObserverCallback(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo, bool visible)
{
	if (observerInfo.observerParams.callback)
		observerInfo.observerParams.callback(observerInfo.observerID, observerInfo.observerParams, observableInfo.observableID, observableInfo.observableParams, visible);
}

void CVisionMap::TriggerObservableCallback(const ObserverInfo& observerInfo, const ObservableInfo& observableInfo, bool visible)
{
	if (observableInfo.observableParams.callback)
		observableInfo.observableParams.callback(observerInfo.observerID, observerInfo.observerParams, observableInfo.observableID, observableInfo.observableParams, visible);
}

#if VISIONMAP_DEBUG

void CVisionMap::DebugDraw()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (gAIEnv.CVars.DebugDrawVisionMap)
	{
		UpdateDebugVisionMapObjects();

		DebugDrawObservers();

		if (gAIEnv.CVars.DebugDrawVisionMapObservables)
			DebugDrawObservables();

		if (gAIEnv.CVars.DebugDrawVisionMapStats)
			DebugDrawVisionMapStats();
	}
}

void CVisionMap::DebugDrawVisionMapStats()
{
	struct DisplayInfo
	{
		float avg;
		float min;
		float max;
		int   count;
	};

	DisplayInfo displayInfoArray[CRY_ARRAY_COUNT(m_latencyInfo)];
	float cutOffTime = m_debugTimer - 10.0f;

	for (int i = 0; i < CRY_ARRAY_COUNT(m_latencyInfo); i++)
	{
		DisplayInfo* displayInfo = &displayInfoArray[i];
		LatencyInfo* latencyInfo = &m_latencyInfo[i];
		int count = 0;
		displayInfo->min = FLT_MAX;
		displayInfo->max = 0.0f;
		displayInfo->avg = 0.0f;

		for (int j = 0; j < latencyInfo->usedBufferSize; j++)
		{
			if (latencyInfo->buffer[j].occurred >= cutOffTime)
			{
				displayInfo->min = std::min(latencyInfo->buffer[j].latency, displayInfo->min);
				displayInfo->max = std::max(latencyInfo->buffer[j].latency, displayInfo->max);
				displayInfo->avg += latencyInfo->buffer[j].latency;
				count++;
			}
		}

		if (count)
		{
			displayInfo->avg /= count;
		}
		else
		{
			displayInfo->min = 0.0f;
		}

		displayInfo->count = count;
	}

	int numObservers = m_observers.size();
	int numObservables = m_observables.size();

	CDebugDrawContext dc;

	int totalPending =
	  m_pendingRayCounts[RayCastRequest::LowPriority] +
	  m_pendingRayCounts[RayCastRequest::MediumPriority] +
	  m_pendingRayCounts[RayCastRequest::HighPriority] +
	  m_pendingRayCounts[RayCastRequest::HighestPriority];

	const float fontSize = 1.5f;
	const float x = dc->GetWidth() * 0.5f;
	float y = dc->GetHeight() * 0.25f;

	stack_string text;
	text.Format(
	  "# Observers: %d\n"
	  "# Observables: %d\n\n"
	  "# PVS updates: %u\n"
	  "# PVS queue size: %" PRISIZE_T "\n"
	                                  "# PVS queue latency : %.2f\n\n"
	                                  "# Visibility updates: %u\n"
	                                  "# Visibility queue size: %" PRISIZE_T "\n"
	                                                                         "# Visibility queue latency : %.2f\n\n"
	                                                                         "# Raycast submits: %u\n"
	                                                                         "# Pending raycast: %d\n\n"
	                                                                         "Raycast latency:\n"
	                                                                         "Priority  | avg  | min  | max  | # pending\n"
	                                                                         "Low       | %.2f | %.2f | %.2f | %d\n"
	                                                                         "Medium    | %.2f | %.2f | %.2f | %d\n"
	                                                                         "High      | %.2f | %.2f | %.2f | %d\n"
	                                                                         "Very High | %.2f | %.2f | %.2f | %d\n",
	  numObservers,
	  numObservables,
	  m_numberOfPVSUpdatesThisFrame,
	  m_observerPVSUpdateQueue.size(),
	  m_pvsUpdateQueueLatency.GetSeconds(),
	  m_numberOfVisibilityUpdatesThisFrame,
	  m_observerVisibilityUpdateQueue.size(),
	  m_visibilityUpdateQueueLatency.GetSeconds(),
	  m_numberOfRayCastsSubmittedThisFrame,
	  totalPending,
	  displayInfoArray[RayCastRequest::LowPriority].avg,
	  displayInfoArray[RayCastRequest::LowPriority].min,
	  displayInfoArray[RayCastRequest::LowPriority].max,
	  m_pendingRayCounts[RayCastRequest::LowPriority],
	  displayInfoArray[RayCastRequest::MediumPriority].avg,
	  displayInfoArray[RayCastRequest::MediumPriority].min,
	  displayInfoArray[RayCastRequest::MediumPriority].max,
	  m_pendingRayCounts[RayCastRequest::MediumPriority],
	  displayInfoArray[RayCastRequest::HighPriority].avg,
	  displayInfoArray[RayCastRequest::HighPriority].min,
	  displayInfoArray[RayCastRequest::HighPriority].max,
	  m_pendingRayCounts[RayCastRequest::HighPriority],
	  displayInfoArray[RayCastRequest::HighestPriority].avg,
	  displayInfoArray[RayCastRequest::HighestPriority].min,
	  displayInfoArray[RayCastRequest::HighestPriority].max,
	  m_pendingRayCounts[RayCastRequest::HighestPriority]);

	dc->Draw2dLabel(x, y, fontSize, Col_White, false, "%s", text.c_str());
}

void CVisionMap::UpdateDebugVisionMapObjects()
{
	{
		CAIObject* debugObserverAIObject = gAIEnv.pAIObjectManager->GetAIObjectByName("VisionMapObserver");
		if (debugObserverAIObject)
		{
			ObserverParams observerParams;
			observerParams.eyePosition = debugObserverAIObject->GetPos();
			observerParams.eyeDirection = debugObserverAIObject->GetEntityDir();

			if (!m_debugObserverVisionID)
			{
				m_debugObserverVisionID = CreateVisionID("DebugObserver");
				observerParams.factionsToObserveMask = 0xffffffff;
				observerParams.typesToObserveMask = 0xffffffff;
				observerParams.typeMask = 0xffffffff;
				observerParams.faction = 0xff;
				observerParams.fovCos = 0.5f;
				observerParams.sightRange = 25.0f;
				RegisterObserver(m_debugObserverVisionID, observerParams);
			}

			ObserverChanged(m_debugObserverVisionID, observerParams, eChangedPosition | eChangedOrientation);
		}
		else if (m_debugObserverVisionID)
		{
			UnregisterObserver(m_debugObserverVisionID);
			m_debugObserverVisionID = VisionID();
		}
	}

	{
		CAIObject* debugObservableAIObject = gAIEnv.pAIObjectManager->GetAIObjectByName("VisionMapObservable");
		if (debugObservableAIObject)
		{
			ObservableParams observableParams;
			observableParams.observablePositionsCount = 1;
			observableParams.observablePositions[0] = debugObservableAIObject->GetPos();

			if (!m_debugObservableVisionID)
			{
				m_debugObservableVisionID = CreateVisionID("DebugObservable");
				observableParams.typeMask = 0xffffffff;
				observableParams.faction = 0xff;
				RegisterObservable(m_debugObservableVisionID, observableParams);
			}

			ObservableChanged(m_debugObservableVisionID, observableParams, eChangedPosition);
		}
		else if (m_debugObservableVisionID)
		{
			UnregisterObservable(m_debugObservableVisionID);
			m_debugObservableVisionID = VisionID();
		}
	}
}

void CVisionMap::DebugDrawObservers()
{
	CDebugDrawContext dc;
	dc->SetAlphaBlended(true);
	dc->SetBackFaceCulling(false);
	dc->SetDepthWrite(false);

	ColorF observersColor = ColorF(0.0f, 0.749f, 1.0f);

	for (Observers::iterator observerIt = m_observers.begin(), observerEndIt = m_observers.end(); observerIt != observerEndIt; ++observerIt)
	{
		const ObserverInfo& observerInfo = observerIt->second;

		const Vec3& currentObserverPosition = observerInfo.observerParams.eyePosition;

		// Visibility Checks ///////////////////////////////////////////////////

		if (gAIEnv.CVars.DebugDrawVisionMapVisibilityChecks)
		{
			for (PVS::const_iterator pvsIt = observerInfo.pvs.begin(), end = observerInfo.pvs.end(); pvsIt != end; ++pvsIt)
			{
				const PVSEntry& pvsEntry = pvsIt->second;

				Vec3 currentObservablePosition = pvsEntry.observableInfo.observableParams.observablePositions[pvsEntry.currentTestPositionIndex];
				Vec3 lastObserverPositionChecked = pvsEntry.lastObserverPositionChecked;
				Vec3 lastObservablePositionChecked = pvsEntry.lastObservablePositionChecked;

				bool pvsEntryHasBeenChecked = (!lastObserverPositionChecked.IsZero() && !lastObservablePositionChecked.IsZero());

				// move player's observable pos down a bit, so that when in 1st person you can tell the AI is looking at you via the debug lines
				if (pvsEntry.observableInfo.observableParams.typeMask & Player)
				{
					static const float zOffset = -0.05f;
					currentObservablePosition.z += zOffset;
					lastObservablePositionChecked.z += zOffset;
				}

				dc->DrawLine(currentObserverPosition, Col_LightGray, currentObservablePosition, Col_LightGray);

				if (pvsEntryHasBeenChecked)
				{
					if (pvsEntry.visible)
					{
						dc->DrawLine(lastObserverPositionChecked, Col_DarkGreen, lastObservablePositionChecked, Col_Green, 5.0f);
						dc->DrawWireSphere(lastObservablePositionChecked, 0.05f, Col_Green);
					}
					else
					{
						dc->DrawLine(lastObserverPositionChecked, Col_IndianRed, pvsEntry.obstructionPosition, Col_Red, 5.0f);
						dc->DrawWireSphere(pvsEntry.obstructionPosition, 0.05f, Col_Red);
					}
				}
			}
		}

		if (gAIEnv.CVars.DebugDrawVisionMapObservers)
		{
			// Stats /////////////////////////////////////////////////////////////////
			{
				float x, y, z;
				if (dc->ProjectToScreen(currentObserverPosition.x, currentObserverPosition.y, currentObserverPosition.z + 0.45f, &x, &y, &z))
				{
					if ((z >= 0.0f) && (z <= 1.0f))
					{
						int visibleCount = 0;
						for (PVS::const_iterator pvsIt = observerInfo.pvs.begin(), end = observerInfo.pvs.end(); pvsIt != end; ++pvsIt)
						{
							const PVSEntry& pvsEntry = pvsIt->second;
							if (pvsEntry.visible)
								++visibleCount;
						}

	#ifdef VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID
						x *= dc->GetWidth() * 0.01f;
						y *= dc->GetHeight() * 0.01f;
						const float fontSize = 1.15f;
						stack_string text;
						text.Format("%s\nVisible: %d/%" PRISIZE_T "\n", observerInfo.observerID.m_debugName.c_str(), visibleCount, observerInfo.pvs.size());

						dc->Draw2dLabel(x, y, fontSize, observersColor, true, "%s", text.c_str());
	#endif    // VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID
					}
				}
			}

			// Location //////////////////////////////////////////////////////////////
			{
				dc->DrawWireSphere(currentObserverPosition, 0.15f, observersColor);
			}

			// FOVs //////////////////////////////////////////////////////////////////
			if (gAIEnv.CVars.DebugDrawVisionMapObserversFOV)
			{
				const ObserverParams& observerParams = observerInfo.observerParams;

				const Vec3& eyePosition = observerParams.eyePosition;
				Vec3 eyeDirection = observerParams.eyeDirection;

				if (eyeDirection.IsZero())
					continue;

				eyeDirection.NormalizeFast();

				dc->DrawLine(eyePosition, observersColor, eyePosition + eyeDirection * 2.5f, observersColor, 5.0f);

				const uint32 pointCount = 32;

				Vec3 points[pointCount];
				Vec3 triangles[(pointCount - 1) * 2 * 3];

				Vec3 y(eyeDirection.x, eyeDirection.y, eyeDirection.z);
				Vec3 x(y.Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalized());

				float anglePrimary = acosf(observerParams.fovCos) * 2.0f;

				for (uint32 i = 0; i < pointCount; ++i)
				{
					float angle = ((float)i / (float)(pointCount - 1) - 0.5f) * anglePrimary;
					points[i] = y * cosf(angle) + x * sinf(angle);
				}

				float radius = observerParams.sightRange;
				float inner = 0.25f;

				uint32 triangleVertexCount = 0;
				for (uint32 i = 0; i < pointCount - 1; ++i)
				{
					triangles[triangleVertexCount++] = eyePosition + points[i] * inner;
					triangles[triangleVertexCount++] = eyePosition + points[i + 1] * radius;
					triangles[triangleVertexCount++] = eyePosition + points[i] * radius;

					triangles[triangleVertexCount++] = eyePosition + points[i] * inner;
					triangles[triangleVertexCount++] = eyePosition + points[i + 1] * inner;
					triangles[triangleVertexCount++] = eyePosition + points[i + 1] * radius;
				}

				dc->DrawTriangles(triangles, triangleVertexCount, ColorF(observersColor, 0.5f));
			}
		}
	}
}

void CVisionMap::DebugDrawObservables()
{
	CDebugDrawContext dc;
	dc->SetAlphaBlended(true);
	dc->SetBackFaceCulling(false);

	for (Observables::iterator it = m_observables.begin(), end = m_observables.end(); it != end; ++it)
	{
		// Stats /////////////////////////////////////////////////////////////////

		ObservableInfo& observableInfo = it->second;

		const Vec3& firstObservablePosition = observableInfo.observableParams.observablePositions[0];

	#ifdef VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID

		float x, y, z;
		if (dc->ProjectToScreen(firstObservablePosition.x, firstObservablePosition.y, firstObservablePosition.z - 0.45f, &x, &y, &z))
		{
			if ((z >= 0.0f) && (z <= 1.0f))
			{
				x *= dc->GetWidth() * 0.01f;
				y *= dc->GetHeight() * 0.01f;
				const float fontSize = 1.0f;
				stack_string text;
				text.Format("%s\n", observableInfo.observableID.m_debugName.c_str());

				dc->Draw2dLabel(x, y, fontSize, ColorF(0.8f, 0.498039f, 0.196078f), true, "%s", text.c_str());
			}
		}

	#endif // VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID

		// Location //////////////////////////////////////////////////////////////

		for (int i = 0; i < observableInfo.observableParams.observablePositionsCount; i++)
		{
			dc->DrawWireSphere(observableInfo.observableParams.observablePositions[i], 0.10f, ColorF(0.8f, 0.498039f, 0.196078f));
		}
	}
}

#endif
