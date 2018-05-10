// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This module keeps track of all hazards in the world and will notify 
// entities/agents when they are inside any of them (most of the 
// interfacing is applied via the behavior systems).

#pragma once

#ifndef HazardModule_h
#define HazardModule_h

#include "../GameAIHelpers.h"

#include "Hazard.h"
#include "HazardProjectile.h"
#include "HazardSphere.h"


// Forward declarations:
class Agent;


namespace HazardSystem
{


// Interfacing between actors and the hazard module they are registered to.
struct HazardModuleInstance : public CGameAIInstanceBase
{
};


// The hazard module keeps track of all hazardous areas in the world and 
// notifies entities that have registered as listeners when they entered
// these areas.
class HazardModule : public AIModule<HazardModule, HazardModuleInstance, 16>
{
public:
	typedef AIModule<HazardModule, HazardModuleInstance, 16> BaseClass;

public:
	// The debug rendering color.
	static const ColorF                 debugColor;

	// How long it takes to fade out the debug rendered primitives (>= 0.0f) (in seconds).
	static const float                  debugPrimitiveFadeDelay;


public:
	HazardModule();
	
	virtual void                        Reset(bool bUnload);
	virtual const char*                 GetName() const { return "HazardModule"; }
	virtual void                        Update(float elapsedTime);
	virtual void                        Serialize(TSerialize ser);	
	virtual void                        PostSerialize();

	// Hazard management:
	HazardSystem::HazardProjectileID    ReportHazard(const HazardSystem::HazardSetup& hazardSetup, const HazardSystem::HazardProjectile& hazardContext);
	HazardSystem::HazardSphereID        ReportHazard(const HazardSystem::HazardSetup& hazardSetup, const HazardSystem::HazardSphere& hazardContext);
	void                                ExpireHazard(const HazardSystem::HazardProjectileID& hazardInstanceID);
	void                                ExpireHazard(const HazardSystem::HazardSphereID& hazardInstanceID);
	bool                                ModifyHazard(const HazardSystem::HazardProjectileID& hazardInstanceID, const Vec3& newPos, const Vec3& newNormal);
	bool                                IsHazardExpired(const HazardSystem::HazardProjectileID& HazardProjectileID) const;
	bool                                IsHazardExpired(const HazardSystem::HazardSphereID& HazardSphereID) const;


protected:

	friend class HazardSystem::HazardDataRayCast;

	// Ray-casting:
	void                                OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);


private:
	// All the projectile hazard instances that have been registered to the module.
	typedef std::vector<HazardSystem::HazardDataProjectile> HazardDataProjectiles;
	HazardDataProjectiles				m_ProjectileHazards;

	// All the sphere hazard instances that have been registered to the module.
	typedef std::vector<HazardSystem::HazardDataSphere> HazardDataSpheres;
	HazardDataSpheres                   m_SphereHazards;

	// If > 0 then we are not allowed to register/unregister entities and/or 
	// hazards.
	int                                 m_UpdateLockCount;

	// The next instance ID that is available (note: these IDs are shared between all
	// the different types so that there can never be more than 1 of each ID in
	// usage).
	HazardSystem::HazardID              m_InstanceIDGen;


private:
	// Collision detection:
	void                                ReportHazardCollisions();
	inline void                         ReportHazardCollisionsHelper();
	inline void                         ProcessCollisionsWithEntity(const EntityId entityID);
	template<class CONTAINER> void      ProcessCollisionsWithEntityProcessContainer(CONTAINER& container, Agent& agent, const char *signalFunctionName, const EntityId entityID);
	void                                ProcessAgentAndProjectile(Agent& agent, const HazardSystem::HazardDataProjectile& HazardData);
	void                                ProcessAgentAndSphere(Agent& agent, const HazardSystem::HazardDataSphere& HazardData);
	void                                SendSignalToAgent(Agent& agent, const char *warningName, const Vec3& estimatedHazardPos, const Vec3& hazardNormal);

	// Hazard instance management:
	template<class CONTAINER> int       IndexOfInstanceID(const CONTAINER& container, const HazardSystem::HazardID instanceID) const;	
	HazardSystem::HazardID              GenerateHazardID();	
	template<class CONTAINER> typename CONTAINER::iterator ExpireAndDeleteHazardInstance(CONTAINER& container, typename CONTAINER::iterator iter);
	void                                RemoveExpiredHazards();
	inline void                         RemoveExpiredHazardsHelper();
	template<class CONTAINER> void      RemoveExpiredHazardsHelperProcessContainer(CONTAINER& container);
	void                                PurgeAllHazards();
	template<class CONTAINER> void	    PurgeAllHazardsProcessContainer(CONTAINER& container);
	template<class CONTAINER> void		ExpireHazardProcessContainer(CONTAINER& container, const HazardSystem::HazardID hazardInstanceID);
	
	// Ray-casting:	
	HazardSystem::HazardDataProjectile* FindPendingProjectileRay(const QueuedRayID rayID);
	void                                PurgeHazardsWithPendingRayRequests();
	template<class CONTAINER> void      PurgeHazardsWithPendingRayRequestsProcessContainer(CONTAINER& container);
	void                                StartRequestedRayCasts();
	template<class CONTAINER> void      StartRequestedRayCastsProcessContainer(CONTAINER& container);

#if !defined(_RELEASE)

	// Debugging:
	void                                RenderDebug() const;
	template<class CONTAINER> void		RenderDebugProcessContainer(const CONTAINER& container, IPersistantDebug& debugRenderer) const;

#endif // !defined(_RELEASE)
};


}; // namespace HazardSystem


#endif // HazardModule_h
