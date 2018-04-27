// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
// A couple of helper class that manage pending ray-casts
// that we are using to compute sound attenuation ( = stimuli
// being dampened by surfaces that are in the way).

#pragma once

#include "AuditionMapDefinitions.h"
#include "AuditionMapItemPool.h"

#include <CryAISystem/IAuditionMap.h>
#include <CryPhysics/RayCastQueue.h>

namespace Perception
{
class CAuditionMap;

namespace AuditionHelpers
{

static const size_t s_invalidTargetEarIndex = ~0;

// Information on a single ray that has been cast for a pending stimulus.
struct SPendingStimulusRayInfo
{
	SPendingStimulusRayInfo();
	explicit SPendingStimulusRayInfo(const QueuedRayID queuedRayId, const size_t castedAtEarIndex);

	void operator=(const SPendingStimulusRayInfo& source);

	QueuedRayID queuedRayId;
	size_t      targetEarIndex;
};
typedef StaticDynArray<SPendingStimulusRayInfo, kAuditionMapMaxEarsPerListener> PendingStimulusRayInfoVector;

struct SPendingStimulusParams : public ItemCounted
{
	SPendingStimulusParams()
		: bReachedOneOrMoreEars(false)
	{}

	SSoundStimulusParams soundParams;
	//const Stimulus::SoundStimulusMultiTags soundTags;
	bool        bReachedOneOrMoreEars;
};

typedef CItemPoolRefCount<SPendingStimulusParams> PendingStimuliParams;
typedef PendingStimuliParams::size_type          PendingStimulusParamsIndex;

// Information about a stimulus that is pending to be delivered, waiting on ray casts to
// be resolved. So this is a stimulus that is being 'directed' straight at a specific
// listener instance!
struct SPendingListenerForStimulus
{
	SPendingListenerForStimulus();
	SPendingListenerForStimulus(const SPendingListenerForStimulus& other);

	void Clear();

	EntityId                     listenerEntityId;
	PendingStimulusParamsIndex   stimulusParamsIndex;
	PendingStimulusRayInfoVector queuedRayInfos;
};

// For now are assuming that the amount of pending stimuli will not be that large
// at any given time because we process them as fast as possible. This means we can
// keep all book-keeping very fast and straight forward using some vectors and indices.
typedef CItemPool<SPendingListenerForStimulus>  PendingListenersForStimulus;
typedef PendingListenersForStimulus::size_type PendingListenerForStimulusIndex;
static const PendingListenerForStimulusIndex s_invalidPendingStimulusIndex = (PendingListenerForStimulusIndex)(~0);

// Information on how and where to cast a ray at, and for which
// pending stimulus instance it was created.
struct SRayCastRequestInfo
{
	SRayCastRequestInfo();

	Vec3                            rayStartPos;
	Vec3                            rayEndPos;

	PendingListenerForStimulusIndex pendingStimulusIndex;

	// For every meter of distance that the ray travels, we will reduce the
	// 'strength' of the sound stimulus by this factor (in the range of
	// [0.0f .. 1.0f]).
	float soundLinearFallOffFactor;
};

// All the information on a single pending ray that was created.
struct SPendingRay
{
	SPendingRay(const SRayCastRequestInfo& requestInfo);

	SRayCastRequestInfo requestInfo;
};

// A manager to handle the rays we cast in order to determine if sound stimuli
// can pass through obstructions or not.
class CAuditionMapRayCastManager
{
	friend class SPendingStimulusParamsHelper;

public:
	CAuditionMapRayCastManager(CAuditionMap& owner);
	~CAuditionMapRayCastManager();

	void Update();

	// Sound properties computations:
	static float ComputeSoundLinearFallOffFactor(const float soundRadius);

	// Ray casting:
	void QueueRaysBetweenStimulusAndListener(const Vec3& rayStartPos, const PendingStimulusParamsIndex& pendingStimulusIndex, const float soundLinearFallOffFactor, const EntityId listenerEntityId, const Perception::SListenerParams& listenerParams);
	void CancelAllPendingStimuliDirectedAtListener(const EntityId listenerEntityId);

private:
	// Params used for notifying listeners about results of the stimulus raycast checking
	struct SParamsForListenerEvents
	{
		SSoundStimulusParams m_soundParams;
		size_t m_reachedEarIndex = -1;
		EntityId m_listenerEntityId = 0;

		bool m_bReachedEar = false;
		bool m_bReachedAtLeastOneEar = false;
		bool m_bLastRay = false;
	};

	PendingStimulusParamsIndex PreparePendingStimulusParams(const SSoundStimulusParams& params);
	void                       ReleasePendingStimulusParamsIfNotUsed(PendingStimulusParamsIndex stimulusParamsIndex);

	// Pending ray-casts management:
	QueuedRayID                             QueueRay(const SRayCastRequestInfo& requestInfo);
	void                                    CancelRay(const QueuedRayID queuedRayId);
	void                                    CancelAllRays();
	void                                    RetrieveAllQueuedRayIds(std::vector<QueuedRayID>* pQueuedRayIds) const;
	RayCastRequest::Priority                GetRayCastRequestPriority() const;
	void                                    OnRayCastComplete(const QueuedRayID& queuedRayID, const RayCastResult& rayCastResult);
	template<typename DEBUGGER> bool        HasRayCastReachedEar(const SRayCastRequestInfo& rayCastRequestInfo, const RayCastResult& rayCastResult, DEBUGGER& debugger) const;
	bool                                    DetermineSoundObstruction(ISurfaceTypeManager* pSurfaceTypeManager, const SRayCastRequestInfo& rayCastRequestInfo, const ray_hit& rayHitInfo, float& resultSoundObstruction) const;
	inline static bool                      DeterminePhysicsSurfaceSoundObstruction(ISurfaceTypeManager* pSurfaceTypeManager, const short surfaceIndex, float& resultSoundObstruction);
	inline static float                     ComputeFurthestPossibleEarDistanceBasedOnRemainingObstruction(const float oldAccumulatedObstruction, const float distanceStartToLastObstructionComputation, const SRayCastRequestInfo& rayCastRequestInfo);

	// Pending stimuli management:
	void CancelRemainingRaysForPendingStimulus(const PendingListenerForStimulusIndex& pendingStimulusIndex);
	void ProcessPendingStimulusRayCastResult(const PendingListenerForStimulusIndex& pendingStimulusIndex, const QueuedRayID queuedRayId, const bool bStimulusReachedEar);
	bool InterpretPendingStimulusRayCastResult(const PendingListenerForStimulusIndex& pendingStimulusIndex, const QueuedRayID queuedRayId, const bool bStimulusReachedEar, SParamsForListenerEvents& eventsParams);
	void CancelAllPendingStimuli();

private:

	CAuditionMap& m_owner;

	typedef std::map<QueuedRayID, SPendingRay> PendingRays;
	PendingRays m_pendingRays;

	// For all these pending stimuli we are waiting for one of its rays to reach an ear.
	PendingListenersForStimulus                  m_pendingListenersForStimulus;
	PendingStimuliParams                         m_pendingStimuliParams;

	std::vector<PendingListenerForStimulusIndex> m_foundPendingStimulusIndices;
};

class SPendingStimulusParamsHelper
{
public:
	SPendingStimulusParamsHelper(CAuditionMapRayCastManager* pRaycastManager, const SSoundStimulusParams& soundParams)
		: pRaycastManager(pRaycastManager)
	{
		stimulusParamsIndex = pRaycastManager->PreparePendingStimulusParams(soundParams);
	}
	~SPendingStimulusParamsHelper()
	{
		pRaycastManager->ReleasePendingStimulusParamsIfNotUsed(stimulusParamsIndex);
	}

	PendingStimulusParamsIndex GetStimulusIndex() const { return stimulusParamsIndex; }

private:
	CAuditionMapRayCastManager* pRaycastManager;
	PendingStimulusParamsIndex  stimulusParamsIndex;
};

}   // !AuditionHelpers

} // !Perception
