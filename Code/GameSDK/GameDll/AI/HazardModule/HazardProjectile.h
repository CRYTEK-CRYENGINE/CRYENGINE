// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// The internal representation of a missile within the hazard system.

#pragma once

#ifndef HazardMissile_h
#define HazardMissile_h


#include <CryPhysics/RayCastQueue.h>
#include <CryMath/Cry_Vector3.h>
#include <CryNetwork/ISerialize.h>

#include "HazardRayCast.h"


// Forward declarations:
struct IPhysicalEntity;
class CWeapon;


namespace HazardSystem
{

// Forward declarations:
class HazardProjectileID;
class HazardModule;


// The context information of a hazard entry for a projectile (e.g.: missiles).
// This will basically be a capsule placed in front of the projectile.
//
// Example of how to define entities that should be ignored for ray-casting:
//
//     HazardProjectile proj;
//     static IPhysicalEntity* pSkipEnts[10];
//     proj.m_SkipEntities       = pSkipEnts;
//	   proj.m_SkipEntitiesAmount = CSingle::GetSkipEntities(GetOwnerEntity(), pSkipEnts, 10);
//
class HazardProjectile
{
public:
	// The position of the projectile (in world-space).
	// This will be the actual start of the warning area.
	Vec3                                m_Pos;

	// The normalized movement direction of the projectile (in world-space).
	Vec3                                m_MoveNormal;

	// The radius of the capsule that forms the hazard volume (>= 0.0f)
	// (in meters).
	float                               m_Radius;

	// The max. length of the hazard warning area in front of the projectile (>= 0.0) (in meters)
	// (in world-space). This is basically the maximum ray-cast distance.
	float                               m_MaxScanDistance;

	// The maximum deviation allowed on the position of the projectile before
	// the warning area should be reconstructed when modifying the hazard
	// before it expires (>= 0.0f) (in meters).
	float								m_MaxPosDeviationDistance;

	// The maximum deviation angle allowed on the direction of the projectile
	// before the warning area should be reconstructed when modifying the 
	// hazard before it expires (>= 0.0f) (in radians).
	float								m_MaxAngleDeviationRad;

	// The entity ID of the weapon (and its owners) that should be ignored
	// when the ray-caster scans forwards to determine the size of the 
	// hazard area (0 if none). This is normally the weapon from which
	// the projectile was fired.
	EntityId							m_IgnoredWeaponEntityID;


public:
	HazardProjectile();
	HazardProjectile(const Vec3& pos, const Vec3& moveNormal, const float radius, const float maxDistance, const float maxPosDeviationDistance, const float maxAngleDeviationDistance, EntityId ignoredWeaponEntityID);
	~HazardProjectile();
};


// Special instance of a hazard entry for a projectile (e.g.: missiles).
class HazardDataProjectile : public HazardDataRayCast
{
public:
	typedef HazardDataRayCast BaseClass;

public:	
	// The position of the projectile (in world-space).
	// This will be the actual start of the warning area.
	Vec3                                m_AreaStartPos;

	// The normalized movement direction of the projectile (in world-space).
	Vec3                                m_MoveNormal;

	// The radius of the capsule that forms the hazard volume (>= 0.0f)
	// (in meters).
	float                               m_Radius;

	// The max. distance of the hazard warning area in front of the projectile (>= 0.0) (in meters)
	// (in world-space). This is basically the maximum ray-cast distance.
	float                               m_MaxScanDistance;

	// The maximum deviation allowed on the position of the projectile before
	// the warning area should be reconstructed when modifying the hazard
	// before it expires (>= 0.0f) (in meters).
	float								m_MaxPosDeviationDistance;

	// The maximum cosined deviation angle allowed on the direction of the projectile
	// before the warning area should be reconstructed when modifying the 
	// hazard before it expires (>= 0.0f) (in cosined radians).
	float								m_MaxCosAngleDeviation;

	// The length of the hazard capsule area (>= 0.0) (in meters) (in world-space). 
	// Will only be valid after the ray-cast succeeds. If -1.0f then we the
	// area is currently undefined.
	float                               m_AreaLength;

	// The entity ID of the weapon (and its owners) that should be ignored
	// when the ray-caster scans forwards to determine the size of the 
	// hazard area (0 if none).
	EntityId							m_IgnoredWeaponEntityID;


public:
	HazardDataProjectile();
	HazardDataProjectile(const HazardDataProjectile& source);

	// Life-time:
	void                                Serialize(TSerialize ser);

	// Queries:
	bool								IsHazardAreaDefined() const;
	HazardProjectileID					GetTypeInstanceID() const;	
	bool								IsApproximationAcceptable(const Vec3& pos, const Vec3& moveNormal) const;
	virtual const Vec3&                 GetNormal() const;
	virtual bool                        IsAgentAwareOfDanger(const Agent& agent, const Vec3& avoidPos) const;

	// Collisions:
	virtual void                        CheckCollision(Agent& agent, HazardCollisionResult* result) const;

	// Ray-casting:	
	virtual CWeapon *					GetIgnoredWeapon() const;
	virtual float						GetMaxRayCastDistance() const;	
	virtual void						ProcessRayCastResult(const Vec3& rayStartPos, const Vec3& rayNormal, const RayCastResult& result);

#if !defined(_RELEASE)

	// Debug:
	virtual void						DebugRender (IPersistantDebug& debugRenderer) const;

#endif // !defined(_RELEASE)
};


};


#endif // HazardMissile_h
