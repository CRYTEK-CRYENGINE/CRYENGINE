// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// The internal representation of a missile within the hazard system.

#include "StdAfx.h"

#include "HazardProjectile.h"

#include "HazardShared.h"

#include "HazardModule.h"

#include "../Agent.h"
#include "../../Weapon.h"


using namespace HazardSystem;


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardProjectile -- HazardProjectile -- HazardProjectile --
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardProjectile::HazardProjectile() :
	m_Pos(ZERO)
	, m_MoveNormal(ZERO)
	, m_Radius(0.0f)
	, m_MaxScanDistance(0.0f)	
	, m_MaxPosDeviationDistance(0.0f)
	, m_MaxAngleDeviationRad(0.0f)	
	, m_IgnoredWeaponEntityID((EntityId)0)	
{
}


HazardProjectile::HazardProjectile(
	const Vec3& pos, const Vec3& moveNormal, const float radius, const float maxScanDistance, 	
	const float maxPosDeviationDistance, const float maxAngleDeviationRad,
	EntityId ignoredWeaponEntityID) :
	m_Pos(pos)
	, m_MoveNormal(moveNormal)
	, m_Radius(radius)
	, m_MaxScanDistance(maxScanDistance)		
	, m_MaxPosDeviationDistance(maxPosDeviationDistance)
	, m_MaxAngleDeviationRad(maxAngleDeviationRad)
	, m_IgnoredWeaponEntityID(ignoredWeaponEntityID)
{
}


HazardProjectile::~HazardProjectile()
{
	// Nothing to do here.
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardDataProjectile -- HazardDataProjectile -- HazardDataProjectile --
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardDataProjectile::HazardDataProjectile() :	
	m_AreaStartPos(0.0f)
	, m_MoveNormal(0.0f)
	, m_Radius(0.0f)
	, m_MaxScanDistance(0.0f)	
	, m_MaxPosDeviationDistance(0.0f)
	, m_MaxCosAngleDeviation(0.0f)
	, m_AreaLength(-1.0f)
	, m_IgnoredWeaponEntityID((EntityId)0)
{
}


HazardDataProjectile::HazardDataProjectile(const HazardDataProjectile& source) :
	m_AreaStartPos(source.m_AreaStartPos)
	, m_MoveNormal(source.m_MoveNormal)
	, m_Radius(source.m_Radius)
	, m_MaxScanDistance(source.m_MaxScanDistance)	
	, m_MaxPosDeviationDistance(source.m_MaxPosDeviationDistance)
	, m_MaxCosAngleDeviation(source.m_MaxCosAngleDeviation)
	, m_AreaLength(source.m_AreaLength)
	, m_IgnoredWeaponEntityID(source.m_IgnoredWeaponEntityID)
{
}


void HazardDataProjectile::Serialize(TSerialize ser)
{
	BaseClass::Serialize(ser);

	ser.BeginGroup("HazardDataProjectile");
	
	ser.Value("AreaStartPos", m_AreaStartPos);
	ser.Value("MoveNormal", m_MoveNormal);
	ser.Value("Radius", m_Radius);
	ser.Value("MaxScanDistance", m_MaxScanDistance);
	ser.Value("MaxPosDeviationDistance", m_MaxPosDeviationDistance);
	ser.Value("MaxCosAngleDeviation", m_MaxCosAngleDeviation);

	ser.Value("AreaLength", m_AreaLength);
	ser.Value("IgnoredWeaponEntityID", m_IgnoredWeaponEntityID);

	ser.EndGroup();
}


// ===========================================================================
//	Query if the hazard area is defined.
//
//	Return:		True if defined; otherwise false (we are probably
//				waiting for a ray-cast result).
//
bool HazardDataProjectile::IsHazardAreaDefined() const
{
	return (m_AreaLength >= 0.0f);
}


// ===========================================================================
//	Get the hazard projectile ID.
//
//	Returns:		The hazard projectile ID.
//
HazardProjectileID HazardDataProjectile::GetTypeInstanceID() const
{
	return HazardProjectileID(BaseClass::GetInstanceID());
}


// ===========================================================================
//	Query if the current hazard volume is a close enough approximation for
//	a certain projectile pose.
//
//	In:		The position of the projectile (in world-space).
//	In:		The move normal vector of the projectile (in world-space).
//
//	Returns:	True if the approximation is close enough; otherwise false.
//
bool HazardDataProjectile::IsApproximationAcceptable(const Vec3& pos, const Vec3& moveNormal) const
{	
	if (Distance::Point_PointSq(m_AreaStartPos, pos) > (m_MaxPosDeviationDistance * m_MaxPosDeviationDistance))
	{
		return false;
	}
	if (m_MoveNormal.Dot(moveNormal) < m_MaxCosAngleDeviation)
	{
		return false;
	}

	return true;
}


// ============================================================================
//	Get the 'direction' normal vector of the hazard.
//
//	Returns:	The direction normal vector of the hazard (or a 0 vector
//				if there is no specific direction, such as an explosion).
//
const Vec3& HazardDataProjectile::GetNormal() const
{
	assert(IsHazardAreaDefined());
	return m_MoveNormal;
}


// ============================================================================
//	Query if the agent is/can be aware of the danger.
//
//	In:			The agent.
//	In:			The avoid position in world-space.
//
//	Returns:	True if the agent is/can be aware of the danger; otherwise
//				false.
//
bool HazardDataProjectile::IsAgentAwareOfDanger(
	const Agent& agent, const Vec3& avoidPos) const
{
	// Make sure we have received the ray-cast data and finalized the entry.
	if (!IsHazardAreaDefined())
	{
		return false;
	}

	// We are cheating a bit here: we want to give agents a chance to dodge the 
	// projectile if it is coming straight at them. Otherwise we should depend 
	// on the perception system to supply visual/audio cues (it will be good 
	// enough if the agent at least turns around to face the thread for example).
	// Note: taking the area start position here because it reflects the
	// the firing position (or at least the position from which the danger is
	// incoming).
	if (!agent.IsPointInFOV(m_AreaStartPos))
	{
		return false;
	}

	return true;
}


// ===========================================================================
// Process possible collision between an agent and the hazard.
//
//	In:		The agent.
//	Out:	The collision result (NULL is invalid!)
//
void HazardDataProjectile::CheckCollision(Agent& agent, HazardCollisionResult* result) const
{
	assert(result != NULL);

	result->Reset();

	// Make sure we have received the ray-cast data and finalized the entry.
	if (!IsHazardAreaDefined())
	{
		return;
	}

	// For now we are just going to check if the pivot of the agent is within
	// the movement volume of the projectile (point vs capsule test).
	Vec3 closestPos;
	Vec3 towardsAgentVec = agent.GetPos() - m_AreaStartPos;
	float closestPosDist = towardsAgentVec.Dot(m_MoveNormal);
	if (closestPosDist <= 0.0f)
	{	// At the start of the path volume.
		closestPos = m_AreaStartPos;
	}
	else if (closestPosDist >= m_AreaLength)
	{	// At the end of the path volume.
		closestPos = m_AreaStartPos + (m_MoveNormal * m_AreaLength);
	}
	else
	{	// Somewhere halfway.
		closestPos = m_AreaStartPos + (m_MoveNormal * closestPosDist);
	}
	if (Distance::Point_PointSq(agent.GetPos(), closestPos) > (m_Radius * m_Radius))
	{
		return;
	}

	// Currently we are returning the area start position so that the agent AI 
	// scripting can try and steer away from the ray that forms the projectile's 
	// movement path.
	result->m_CollisionFlag = true;
	result->m_HazardOriginPos = m_AreaStartPos;
	return;
}


// ============================================================================
//	Get the ignored weapon instance.
//
//	Returns:	The ignored weapon instance (or NULL if none).
//
CWeapon *HazardDataProjectile::GetIgnoredWeapon() const
{
	if (m_IgnoredWeaponEntityID == (EntityId)0)
	{
		return NULL;
	}
	IItem* item = gEnv->pGameFramework->GetIItemSystem()->GetItem(
		m_IgnoredWeaponEntityID);
	if (item == NULL)
	{
		return NULL;
	}
	IWeapon* weaponInterface = item->GetIWeapon();
	if (weaponInterface == NULL)
	{
		return NULL;
	}

	return static_cast<CWeapon*>(weaponInterface);
}


// ============================================================================
//	Get the maximum scanning distance for ray-casting.
//	
//	@returns	The maximum scanning distance for ray-casting (>= 0.0f)
//				(in meters).
//
float HazardDataProjectile::GetMaxRayCastDistance() const
{
	return m_MaxScanDistance;
}


// ===========================================================================
//	Event: received ray-cast data for constructing the hazard warning area.
//
//	In:		The ray start position (in world-space).
//	In:		The ray normal (in world-space).
//	In:		Th ray-cast results.
//
void HazardDataProjectile::ProcessRayCastResult(
	const Vec3& rayStartPos, const Vec3& rayNormal,
	const RayCastResult& result)
{
	if (result.hitCount > 0)
	{
		m_AreaLength = result->dist;
	}
	else
	{
		m_AreaLength = m_MaxScanDistance;
	}	

	m_AreaStartPos = rayStartPos;
	m_MoveNormal = rayNormal;
}


#if !defined(_RELEASE)


// ============================================================================
//	Render debug information.
//
//	In,out:		The debug renderer.
//
void HazardDataProjectile::DebugRender (IPersistantDebug& debugRenderer) const
{
	if (!IsHazardAreaDefined())
	{
		return;
	}

	debugRenderer.AddCylinder(
		m_AreaStartPos + (m_MoveNormal * m_AreaLength * 0.5f),
		m_MoveNormal,
		m_Radius,
		m_AreaLength,
		HazardModule::debugColor, HazardModule::debugPrimitiveFadeDelay);
	debugRenderer.AddLine(
		m_AreaStartPos, m_AreaStartPos + (m_MoveNormal * m_AreaLength), 
		HazardModule::debugColor, HazardModule::debugPrimitiveFadeDelay);
}


#endif // !defined(_RELEASE)
