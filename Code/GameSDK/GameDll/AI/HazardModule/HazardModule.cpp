// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// This module keeps track of all hazards in the world and will notify 
// entities/agents when they are inside any of them (most of the 
// interfacing is applied via the behavior systems).

#include "StdAfx.h"
#include "HazardModule.h"

#include "../Agent.h"
#include <CrySystem/ISystem.h>
#include <CryAISystem/IAISystem.h>
#include "GameCVars.h"


using namespace HazardSystem;


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardModule -- HazardModule -- HazardModule -- HazardModule --
//
// ============================================================================
// ============================================================================
// ============================================================================


// The debug rendering color.
const ColorF HazardModule::debugColor = Col_Cyan;

// How long it takes to fade out the debug rendered primitives (>= 0.0f) (in seconds).
const float HazardModule::debugPrimitiveFadeDelay = 0.1f;
 

HazardModule::HazardModule() :
	m_UpdateLockCount(0)
	, m_InstanceIDGen(gUndefinedHazardInstanceID + 1)
{	
	m_ProjectileHazards.reserve(32);
	m_SphereHazards.reserve(32);
}


void HazardModule::Reset(bool bUnload)
{
	BaseClass::Reset(bUnload);

	CRY_ASSERT_MESSAGE((m_UpdateLockCount == 0), "Not allowed to modify hazards to the hazard module while it is updating!");

	m_InstanceIDGen = gUndefinedHazardInstanceID + 1;
	
	PurgeAllHazards();
}


// ============================================================================
//	Update the module.
//
//	In:		The amount of time that has elapsed (>= 0.0f) (in seconds).
//
void HazardModule::Update(float elapsedTime)
{	
	ReportHazardCollisions();

	RemoveExpiredHazards();

#if !defined(_RELEASE)
	if (g_pGameCVars->ai_HazardsDebug != 0)	
	{
		RenderDebug();
	}
#endif	// !defined(_RELEASE)
}


void HazardModule::Serialize(TSerialize ser)
{		
	BaseClass::Serialize(ser);

	ser.BeginGroup("HazardModule");

	uint32 hazardsAmount = (uint32)m_ProjectileHazards.size();
	ser.Value("ProjectileHazardsAmount", hazardsAmount);
	m_ProjectileHazards.resize(hazardsAmount);	
	HazardDataProjectiles::iterator hazardProjectileIter;
	HazardDataProjectiles::iterator hazardProjectileEndIter = m_ProjectileHazards.end();
	for (hazardProjectileIter = m_ProjectileHazards.begin() ; hazardProjectileIter != hazardProjectileEndIter ; 
		++hazardProjectileIter)
	{
		hazardProjectileIter->Serialize(ser);
	}

	hazardsAmount = (uint32)m_SphereHazards.size();
	ser.Value("SphereHazardsAmount", hazardsAmount);
	m_SphereHazards.resize(hazardsAmount);	
	HazardDataSpheres::iterator hazardSphereIter;
	HazardDataSpheres::iterator hazardSphereEndIter = m_SphereHazards.end();
	for (hazardSphereIter = m_SphereHazards.begin() ; hazardSphereIter != hazardSphereEndIter ; 
		++hazardSphereIter)
	{
		hazardSphereIter->Serialize(ser);
	}

	// Note: m_UpdateLockCount does not need to be serialized because it is only used for
	// sanity checks during updates.

	ser.Value("InstanceIDGen", m_InstanceIDGen);

	ser.EndGroup();
}


void HazardModule::PostSerialize()
{
	IGameAIModule::PostSerialize();

	StartRequestedRayCasts();
}


// ============================================================================
//	Report a projectile path hazard instance.
//
//	The hazard area for the projectile will be constructed by doing a ray-cast
//	into the world and seeing how far we can place a capsule to cover the
//	area.
//
//	In:		The hazard setup information.
//	In:		The projectile context information (will be copied into the module 
//			container).
//
//	Returns:	The unique instance ID that was assigned to the hazard (will 
//				automatically become invalid when the hazard expires).
//
HazardProjectileID HazardModule::ReportHazard(
	const HazardSetup& hazardSetup,
	const HazardProjectile& hazardContext)
{		
	// Sanity checks.
	assert(hazardContext.m_Pos.IsValid());
	assert(hazardContext.m_MoveNormal.IsValid());
	assert(hazardContext.m_MoveNormal.IsUnit());
	assert(hazardContext.m_Radius >= 0.0f);	
	assert(hazardContext.m_MaxScanDistance >= 0.0f);	
	assert(hazardContext.m_MaxPosDeviationDistance >= 0.0f);
	assert(hazardContext.m_MaxAngleDeviationRad >= 0.0f);
	
	CRY_ASSERT_MESSAGE((m_UpdateLockCount == 0), "Not allowed to register hazards to the hazard module while it is updating!");

	m_ProjectileHazards.push_back(HazardDataProjectile());

	HazardDataProjectile& newHazardData = m_ProjectileHazards.back();

	newHazardData.BasicInit(GenerateHazardID(), hazardSetup);
	
	newHazardData.m_AreaStartPos             = hazardContext.m_Pos;
	newHazardData.m_MoveNormal               = hazardContext.m_MoveNormal;
	newHazardData.m_Radius                   = hazardContext.m_Radius;	
	newHazardData.m_MaxScanDistance          = hazardContext.m_MaxScanDistance;
	newHazardData.m_MaxPosDeviationDistance  = hazardContext.m_MaxPosDeviationDistance;
	newHazardData.m_MaxCosAngleDeviation     = cos_tpl(hazardContext.m_MaxAngleDeviationRad);	
	newHazardData.m_IgnoredWeaponEntityID    = hazardContext.m_IgnoredWeaponEntityID;

	newHazardData.QueueRayCast(this, hazardContext.m_Pos, hazardContext.m_MoveNormal);
	
	return newHazardData.GetTypeInstanceID();
}


// ============================================================================
//	Report a sphere hazard instance.
//
//	In:		The hazard setup information.
//	In:		The sphere hazard data (will be copied into the module container).
//
//	Returns:	The unique instance ID that was assigned to the hazard (will 
//				automatically become invalid when the hazard expires).
//
HazardSphereID HazardModule::ReportHazard(
	const HazardSetup& hazardSetup,
	const HazardSphere& hazardContext)
{	
	m_SphereHazards.push_back(HazardDataSphere());

	HazardDataSphere& newHazardData = m_SphereHazards.back();

	newHazardData.BasicInit(GenerateHazardID(), hazardSetup);

	newHazardData.m_Context = hazardContext;

	newHazardData.m_RadiusSq = hazardContext.m_Radius * hazardContext.m_Radius;

	return newHazardData.GetTypeInstanceID();
}


// ============================================================================
//	Modify the position and forward direction of a projectile hazard.
//
//	In:		The hazard instance ID (and invalid one will abort).
//	In:		The new position of the projectile (see HazardProjectile::m_Pos).
//	In:		The new normalized movement direction of the projectile (see
//			HazardProjectile::m_MoveNormal).
//
//	Returns:	True if the hazard area will be modified as soon as possible;
//				otherwise false (e.g.: approximation is still good enough, or
//				a ray-cast is still pending for the current area).
//
bool HazardModule::ModifyHazard(
	const HazardSystem::HazardProjectileID& hazardInstanceID, 	
	const Vec3& newPos, const Vec3& newNormal)
{
	// Sanity checks.
	assert(newNormal.IsValid());
	assert(newNormal.IsUnit());

	int containerIndex = IndexOfInstanceID(m_ProjectileHazards, hazardInstanceID.GetHazardID());
	if (containerIndex < 0)
	{
		assert(false);
		return false;
	}
	HazardDataProjectile& projectileData = m_ProjectileHazards[containerIndex];

	// Do not overwrite the area while ray-cast is still in the queue or otherwise
	// the area will never update when ModifyHazard() is called every frame
	// for example.
	if (projectileData.HasPendingRayCasts())
	{
		return false;
	}

	// Determine if we should update the warning area or if the current one is
	// still 'acceptable'.
	if (projectileData.IsApproximationAcceptable(newPos, newNormal))
	{
		return false;
	}

	// We need to update the hazard to match the new situation more closely.
	// We therefore request a new ray-cast to be performed (note that the
	// current volume will still be used as a hazard warning area until the 
	// ray-cast has completed).
	projectileData.QueueRayCast(this, newPos, newNormal);	
	return true;
}


// ============================================================================
//	Expire a hazard and have it removed.
//
//	This function can be used to expire a hazard manually and/or prematurely.
//
//	In:		The unique hazard instance ID. We will abort if the ID is invalid
//			or has already expired.
//
void HazardModule::ExpireHazard(const HazardSystem::HazardProjectileID& hazardInstanceID)
{
	ExpireHazardProcessContainer(m_ProjectileHazards, hazardInstanceID.GetHazardID());
}


// ============================================================================
//	Expire a hazard and have it removed.
//
//	This function can be used to expire a hazard manually and/or prematurely.
//
//	In:		The unique hazard instance ID. We will abort if the ID is invalid
//			or has already expired.
//
void HazardModule::ExpireHazard(const HazardSystem::HazardSphereID& hazardInstanceID)
{
	ExpireHazardProcessContainer(m_SphereHazards, hazardInstanceID.GetHazardID());
}


// ============================================================================
//	Query if a hazard instance has expired.
//
//	In:		The unique hazard instance ID (undefined will abort).
//
//	Returns:	True if the hazard has expired or if undefined; otherwise false.
//
bool HazardModule::IsHazardExpired(const HazardProjectileID& hazardInstanceID) const
{
	// If we cannot find an entry in the container for the specified ID then
	// it must have expired.
	return (IndexOfInstanceID(m_ProjectileHazards, hazardInstanceID.GetHazardID()) < 0);
}


// ============================================================================
//	Query if a hazard instance has expired.
//
//	In:		The unique hazard instance ID (undefined will abort).
//
//	Returns:	True if the hazard has expired or if undefined; otherwise false.
//
bool HazardModule::IsHazardExpired(const HazardSphereID& hazardInstanceID) const
{
	// If we cannot find an entry in the container for the specified ID then
	// it must have expired.
	return (IndexOfInstanceID(m_SphereHazards, hazardInstanceID.GetHazardID()) < 0);
}


// ============================================================================
//	Report possible collisions between hazards and entities.
//
void HazardModule::ReportHazardCollisions()
{
	// Lock the module so that we can be sure that none of the iterators can 
	// suddenly 'break'.
	m_UpdateLockCount++;
	ReportHazardCollisionsHelper();
	m_UpdateLockCount--;
}


// ============================================================================
//	Helper: Report possible collisions between hazards and entities.
//
void HazardModule::ReportHazardCollisionsHelper()
{		
	assert(m_running.get() != NULL);

	if (m_running.get() == NULL)
	{
		return;
	}

	HazardModuleInstance* instance;
	Instances::iterator iter = m_running->begin();
	Instances::iterator iterEnd = m_running->end();	

	for (iter = m_running->begin() ; iter != iterEnd; ++iter)
	{
		instance = GetInstanceFromID(iter->second);
		if (instance != NULL)
		{
			ProcessCollisionsWithEntity(instance->GetEntityID());
		}
	}
}


// ============================================================================
//	Process possible collisions between hazards and a specific entity.
//
//	In:		The entity ID.
//
void HazardModule::ProcessCollisionsWithEntity(const EntityId entityID)
{
	// Currently we only support agents and their behavior system.
	Agent agent(entityID);
	if (!agent.IsValid())
	{
		return;
	}

	ProcessCollisionsWithEntityProcessContainer(
		m_ProjectileHazards, agent, "OnIncomingProjectile", entityID);
	// TODO: implement hazard spheres!
	//ProcessCollisionsWithEntityProcessContainer(
	//   m_SphereHazards, agent, "OnHazard", entityID);
}


// ============================================================================
//	Process possible collisions between hazards and a specific entity.
//
//	In:		The container the search through.
//	In:		The agent.
//	In:		The name of the Lua function to call in order to signal collisions
//			(NULL is invalid!)
//	In:		The entity ID.
//
template<class CONTAINER>
void HazardModule::ProcessCollisionsWithEntityProcessContainer(
	CONTAINER& container, Agent& agent,
	const char *signalFunctionName, const EntityId entityID)
{
	assert(signalFunctionName != NULL);
	
	HazardCollisionResult collisionResult;

	// NOTE: for now this will be quick and dirty. If the amount of hazards
	// becomes too big we should apply some spatial hashing schemes!
	typename CONTAINER::const_iterator hazardIter;
	typename CONTAINER::const_iterator hazardEndIter = container.end();
	for (hazardIter = container.begin() ; hazardIter != hazardEndIter ; ++hazardIter)
	{
		if (hazardIter->ShouldWarnEntityID(entityID))		
		{
			hazardIter->CheckCollision(agent, &collisionResult);
			if (collisionResult.m_CollisionFlag)			
			{
				if (hazardIter->IsAgentAwareOfDanger(agent, collisionResult.m_HazardOriginPos))
				{
					SendSignalToAgent(agent, signalFunctionName, 
						collisionResult.m_HazardOriginPos, hazardIter->GetNormal());
				}
			}
		}
	}
}


// ===========================================================================
//	Send hazard collision signal to an agent.
//
//	In,out:	The warning will be send to this agent.
//	In:		The name of the warning (NULL is invalid!)
//	In:		The estimated hazard's position in world-space.
//	In:		The normalized 'direction' of the hazard (can be a 0-vector
//			if unknown). This is usually the move or 'shooting' direction
//			of the hazard.
//
void HazardModule::SendSignalToAgent(	
	Agent& agent,
	const char *warningName,	
	const Vec3& estimatedHazardPos,
	const Vec3& hazardNormal)
{
	IAISignalExtraData* extraData = gEnv->pAISystem->CreateSignalExtraData();
	extraData->point = estimatedHazardPos;
	extraData->point2 = hazardNormal;
	
	// Debug visualization.
	//gEnv->pGameFramework->GetIPersistantDebug()->Begin("HazardDebugGraphics", false);
	//gEnv->pGameFramework->GetIPersistantDebug()->AddSphere(estimatedHazardPos, 0.3f, Col_DarkGray, 10.0f);

	agent.SetSignal(AISIGNAL_DEFAULT, warningName, extraData);
}


// ============================================================================
//	Convert an instance ID to a corresponding instance index.
//
//	In:		The container through which to search.
//	In:		The instance ID (HHID_UNDEFINED will abort!)
//
//	Returns:	The corresponding instance index into m_hazards or -1 if
//				if it could not be found.
//
template<class CONTAINER>
int HazardModule::IndexOfInstanceID(const CONTAINER& container, const HazardID instanceID) const
{
	if (instanceID == gUndefinedHazardInstanceID)
	{
		return -1;
	}

	// Ugly! In the future we should apply a decent hashing here!
	size_t index, size = container.size();
	for (index = 0 ; index < size ; index++)
	{
		if (container[index].GetInstanceID() == instanceID)
		{
			return (int)index;
		}
	}

	return -1;
}


// ============================================================================
//	Generate a new unique instance ID.
//
//	Returns:	A new unique instance ID.
//
HazardID HazardModule::GenerateHazardID()
{
	HazardID newInstanceID = m_InstanceIDGen;

	m_InstanceIDGen++;

	CRY_ASSERT_MESSAGE(m_InstanceIDGen != gUndefinedHazardInstanceID, 
		"HazardModule::GenerateNewInstanceID() too many hazards generated!");

	return newInstanceID;
}


// ============================================================================
//	Expire and delete a hazard instance.
//
//	In,out:		The container in which the hazard instance is stored.
//	In:			Iterator to the target hazard instance.
//
//	Returns:	Iterator to the next hazard instance in the container.
//
template<class CONTAINER>
typename CONTAINER::iterator HazardModule::ExpireAndDeleteHazardInstance(
	CONTAINER& container, typename CONTAINER::iterator iter)
{	
	iter->Expire();
	return container.erase(iter);
}


// ============================================================================
//	Remove hazards that have expired.
//
void HazardModule::RemoveExpiredHazards()
{
	// Lock the module so that we can be sure that none of the iterators can 
	// suddenly 'break'.
	m_UpdateLockCount++;
	RemoveExpiredHazardsHelper();
	m_UpdateLockCount--;
}


// ============================================================================
//	Helper: Remove hazards that have expired.
//
void HazardModule::RemoveExpiredHazardsHelper()
{
	RemoveExpiredHazardsHelperProcessContainer(m_ProjectileHazards);
	RemoveExpiredHazardsHelperProcessContainer(m_SphereHazards);
}


// ============================================================================
//	Helper: Remove hazards that have expired.
//
//	In,out:		The container that is to be processed.
//
template<class CONTAINER>
void HazardModule::RemoveExpiredHazardsHelperProcessContainer(CONTAINER& container)
{
	// NOTE: in the future we may want to keep the array sorted based on
	// expiry time or something.
	const float nowTimeIndex = gEnv->pTimer->GetCurrTime();
	bool expiredFlag = false;
	typename CONTAINER::iterator iter = container.begin();
	while (iter != container.end())
	{
		expiredFlag = false;
		if (iter->GetExpireTimeIndex() >= 0)
		{
			if (iter->GetExpireTimeIndex() <= nowTimeIndex)
			{
				expiredFlag = true;
			}
		}

		if (expiredFlag)
		{
			iter = ExpireAndDeleteHazardInstance(container, iter);	
			expiredFlag = false;
		}
		else
		{
			++iter;
		}
	}	
}


// ===========================================================================
//	Purge all hazards.
//
void HazardModule::PurgeAllHazards()
{
	CRY_ASSERT_MESSAGE((m_UpdateLockCount == 0), "Not allowed to modify hazards to the hazard module while it is updating!");
	
	PurgeAllHazardsProcessContainer(m_ProjectileHazards);
	PurgeAllHazardsProcessContainer(m_SphereHazards);
}


// ===========================================================================
//	Purge all hazards: process a container.
//
template<class CONTAINER>
void HazardModule::PurgeAllHazardsProcessContainer(CONTAINER& container)
{
	typename CONTAINER::iterator iter = container.begin();
	while (iter != container.end())
	{
		iter = ExpireAndDeleteHazardInstance(container, iter);
	}
}


// ============================================================================
//	Helper: Expire a hazard and have it removed.
//
//	This function can be used to expire a hazard manually and/or prematurely.
//
//	In:		The unique hazard instance ID. We will abort if the ID is invalid
//			or has already expired.
//
template<class CONTAINER>
void HazardModule::ExpireHazardProcessContainer(
	CONTAINER& container, const HazardID hazardInstanceID)
{
	CRY_ASSERT_MESSAGE((m_UpdateLockCount == 0), "Not allowed to unregister hazards to the hazard module while it is updating!");

	int index = IndexOfInstanceID(container, hazardInstanceID);
	if (index < 0)
	{
		return;
	}

	ExpireAndDeleteHazardInstance(container, container.begin() + index);
}


// ===========================================================================
//	Event: received ray-cast data for constructing the hazard warning area.
//
//	In:		The ray ID.
//	In:		The ray-cast results.
//
void HazardModule::OnRayCastDataReceived(
	const QueuedRayID& rayID, const RayCastResult& result)
{
	HazardDataProjectile* hazard = FindPendingProjectileRay(rayID);
	if (hazard == NULL)
	{
		assert(false);
		return;
	}
	hazard->MainProcessRayCastResult(rayID, result);
}


// ============================================================================
//	Find the projectile hazard that has a pending ray ID.
//
//	In:		The ray ID.
//
//	Returns:	The corresponding hazard entry or (NULL if it could not be found).
//
HazardDataProjectile *HazardModule::FindPendingProjectileRay(const QueuedRayID rayID)
{
	HazardDataProjectiles::iterator iter;
	HazardDataProjectiles::iterator iterEnd = m_ProjectileHazards.end();
	for (iter = m_ProjectileHazards.begin() ; iter != iterEnd ; ++iter)
	{
		if (iter->IsWaitingForRay(rayID))
		{
			return &(*iter);
		}
	}
	return NULL;
}


// ============================================================================
//	Purge all hazards that have pending ray-cast requests.
//
void HazardModule::PurgeHazardsWithPendingRayRequests()
{
	PurgeHazardsWithPendingRayRequestsProcessContainer(m_ProjectileHazards);	
}


// ============================================================================
//	Purge all hazards that have pending ray-cast requests: process container.
//
//	In,out:		The container to process.
//
template<class CONTAINER>
void HazardModule::PurgeHazardsWithPendingRayRequestsProcessContainer(CONTAINER& container)
{
	typename CONTAINER::iterator iter = container.begin();
	while (iter != container.end())
	{		
		if (iter->HasPendingRayCasts())
		{
			iter = ExpireAndDeleteHazardInstance(container, iter);
		}
		else
		{
			++iter;
		}
	}
}


// ============================================================================
//	Start all ray-casts that were requested.
//
void HazardModule::StartRequestedRayCasts()
{
	StartRequestedRayCastsProcessContainer(m_ProjectileHazards);	
}


// ============================================================================
//	Start all ray-casts that were requested: process container.
//
//	In,out:		The container to process.
//
template<class CONTAINER>
void HazardModule::StartRequestedRayCastsProcessContainer(CONTAINER& container)
{
	typename CONTAINER::iterator iter;
	typename CONTAINER::iterator iterEnd = container.end();
	for (iter = container.begin() ; iter != iterEnd ; ++iter)
	{	
		iter->StartRequestedRayCasts(this);	
	}
}


#if !defined(_RELEASE)


// ============================================================================
//	Render debug information.
//
void HazardModule::RenderDebug() const
{
	IPersistantDebug *debugRenderer = gEnv->pGameFramework->GetIPersistantDebug();
	if (debugRenderer == NULL)
	{
		return;
	}

	debugRenderer->Begin("HazardDebugGraphics", false);

	RenderDebugProcessContainer(m_ProjectileHazards, *debugRenderer);
	RenderDebugProcessContainer(m_SphereHazards, *debugRenderer);
}


// ============================================================================
//	Render debug information.
//
//	In:		Pointer to the container.
//	In:		Reference to the debug rendered (NULL is invalid!)
//
template<class CONTAINER>
void HazardModule::RenderDebugProcessContainer(
	const CONTAINER& container, IPersistantDebug& debugRenderer) const
{
	typename CONTAINER::const_iterator iter;
	typename CONTAINER::const_iterator Enditer = container.end();
	for (iter = container.begin() ; iter != Enditer ; ++iter)
	{
		iter->DebugRender(debugRenderer);
	}
}


#endif // !defined(_RELEASE)
