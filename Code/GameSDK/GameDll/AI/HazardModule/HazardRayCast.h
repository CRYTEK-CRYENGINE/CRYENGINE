// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// An 'interface' class that assists hazard definitions that utilize a ray-cast in some way.

#pragma once

#ifndef HazardRayCast_h
#define HazardRayCast_h

#include <CryEntitySystem/IEntity.h>

#include "HazardShared.h"
#include "Hazard.h"


// Forward declarations:
class Agent;
class CWeapon;


namespace HazardSystem
{


// Forward declarations:
class HazardModule;


// An 'interface' for hazard data that utilizes ray-casting in some form
// or other.
class HazardDataRayCast : public HazardData
{
public:
	typedef HazardData BaseClass;

	enum RayCastState
	{
		RayCastRequested = 0,			// We request a ray-cast to be performed as soon as possible.
		RayCastPending,
		RayCastCompleted,
		RayCastLast						// Only used for serialization.
	};


public:
	HazardDataRayCast();
	HazardDataRayCast(const HazardDataRayCast& source);
	virtual ~HazardDataRayCast();

	// Life-cycle:
	void                                Serialize(TSerialize ser);
	virtual void						Expire();

	// Ray-casting:
	virtual bool                        HasPendingRayCasts() const;
	virtual void						StartRequestedRayCasts(HazardModule *hazardModule);
	bool                                IsWaitingForRay(const QueuedRayID rayID) const;
	virtual void						QueueRayCast(HazardModule *hazardModule, const Vec3& rayStartPos, const Vec3& rayNormal);
	void                                MainProcessRayCastResult(const QueuedRayID rayID, const RayCastResult& result);
	void                                CancelPendingRayCast();
	virtual CWeapon *					GetIgnoredWeapon() const = 0;
	virtual float						GetMaxRayCastDistance() const = 0;
	virtual void						ProcessRayCastResult(const Vec3& rayStartPos, const Vec3& rayNormal,	const RayCastResult& result) = 0;


private:

	// The current ray-cast state.
	RayCastState						m_RayCastState;	

	// The ID of the queued ray we are using to determine how far ahead to 
	// warn agents (0 if no request has been queued).
	QueuedRayID                         m_QueuedRayID;

	// The start position position of the pending ray (we need this information in case a 
	// ray-cast is restarted due to serialization for example).
	Vec3								m_PendingRayStartPos;

	// The ray normal direction of the pending volume (also see m_PendingRayStartPos).
	Vec3								m_PendingRayNormal;
};


}; // namespace HazardSystem


#endif // Hazard_h
