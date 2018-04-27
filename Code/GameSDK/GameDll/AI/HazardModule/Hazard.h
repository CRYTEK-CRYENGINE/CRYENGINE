// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// The base class for storing hazard information in the hazard system.

#pragma once

#ifndef Hazard_h
#define Hazard_h

#include <CryEntitySystem/IEntity.h>

#include "HazardShared.h"


// Forward declarations:
class Agent;


namespace HazardSystem
{


// Forward declarations:
class HazardModule;


// In this container we store information on how the hazard will be setup.
class HazardSetup
{
public:
	// The entity ID from which the hazard originates (or 0 if anonymous).
	EntityId				            m_OriginEntityID;

	// How long the hazard should exist before it will be automatically 
	// purged (in seconds). If < 0.0f then the hazard will never expire.
	// Note: the hazard will always exist for at least one frame, even if
	// 0.0f is specified.
	// WARNING: Some type of hazards, such as the projectile will use
	// ray-casting to construct the actual warning area volume, so make
	// sure that the delay is long enough for a ray-cast to succeed.
	float                               m_ExpireDelay;

	// True if the origin entity should also be warned of its own hazards
	// (set this to false for projectiles fired from weapons and such).
	bool                                m_WarnOriginEntityFlag;


public:
	HazardSetup();
	HazardSetup(const EntityId originEntityID, const float expireDelay = 0.0f, const bool warnOriginEntityFlag = true);
};


// The resulting information for testing if an agent is inside a hazard.
class HazardCollisionResult
{
public:
	HazardCollisionResult();

	void								Reset();

public:

	// True if the agent's pivot was inside the hazard; otherwise false.
	bool                                m_CollisionFlag;

	// The point from which the hazard originates (so this could also
	// be the start of a ray for example) (in world-space).
	Vec3								m_HazardOriginPos;
};


// A single instance entry in the table of hazards in the world.
class HazardData
{
public:
	HazardData();
	HazardData(const HazardData& source);	
	virtual ~HazardData();
	
	// Life-cycle:
	void                                BasicInit(const HazardID instanceID, const HazardSetup& hazardSetup);		
	void                                Serialize(TSerialize ser);
	virtual void						Expire();

	// Queries:
	bool                                ShouldWarnEntityID(EntityId entityID) const;
	virtual const Vec3&                 GetNormal() const = 0;
	virtual bool                        IsAgentAwareOfDanger(const Agent& agent, const Vec3& avoidPos) const = 0;

	// Access:
	HazardID                            GetInstanceID() const;
	float                               GetExpireTimeIndex() const;
	EntityId                            GetOriginEntityID() const;	
	
	// Collisions:
	virtual void                        CheckCollision(Agent& agent, HazardCollisionResult* result) const = 0;

#if !defined(_RELEASE)

	// Debug:
	virtual void						DebugRender (IPersistantDebug& debugRenderer) const = 0;

#endif // !defined(_RELEASE)


private:
	// The unique instance ID that was assigned to this hazard
	// instance or gUndefinedHazardInstanceID if not defined.
	HazardID							m_InstanceID;

	// The time index at which this instance will expire (or -1.0 if it
	// should never expire).
	float                               m_ExpireTimeIndex;

	// The ID if the entity from which the hazard originated (0 if undefined).
	EntityId                            m_OriginEntityID;

	// True if the origin entity should also be warned of its own hazards
	// (set this to false for projectiles fired from weapons and such).
	bool                                m_WarnOriginEntityFlag;
};


}; // namespace HazardSystem


#endif // Hazard_h
