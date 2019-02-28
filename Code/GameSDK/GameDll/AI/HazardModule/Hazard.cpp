// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// The base class for storing hazard information in the hazard system.

#include "StdAfx.h"

#include "Hazard.h"


using namespace HazardSystem;


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardSetup -- HazardSetup -- HazardSetup -- HazardSetup -- HazardSetup -- 
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardSetup::HazardSetup() :
	m_OriginEntityID((EntityId )0)	
	, m_ExpireDelay(0.0f)
	, m_WarnOriginEntityFlag(true)
{
}


HazardSetup::HazardSetup(
	const EntityId originEntityID,  
	const float expireDelay /* = 0.0f */,
	const bool warnOriginEntityFlag /* = true */) :
	m_OriginEntityID(originEntityID)
	, m_ExpireDelay(expireDelay)
	, m_WarnOriginEntityFlag(warnOriginEntityFlag)	
{
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardCollisionResult -- HazardCollisionResult -- HazardCollisionResult --
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardCollisionResult::HazardCollisionResult() :
	m_CollisionFlag(false)
	, m_HazardOriginPos(ZERO)
{
}


void HazardCollisionResult::Reset()
{
	m_CollisionFlag = false;
	m_HazardOriginPos = Vec3(ZERO);
}


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardData -- HazardData -- HazardData -- HazardData -- HazardData --
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardData::HazardData() :
	m_InstanceID(gUndefinedHazardInstanceID)
	, m_ExpireTimeIndex(0.0f)
	, m_OriginEntityID((EntityId)0)
	, m_WarnOriginEntityFlag(false)
{	
}


HazardData::HazardData(const HazardData& source) :
	m_InstanceID(source.m_InstanceID)
	, m_ExpireTimeIndex(source.m_ExpireTimeIndex)
	, m_OriginEntityID(source.m_OriginEntityID)
	, m_WarnOriginEntityFlag(source.m_WarnOriginEntityFlag)
{
}


HazardData::~HazardData()
{
	// Nothing to do here.
}


// ============================================================================
//	Perform basic initialization.
//
//	In:		The unique instance ID (gUndefinedHazardInstanceID is invalid!)
//	In:		The hazard setup information.
//
void HazardData::BasicInit(
	const HazardID instanceID, const HazardSetup& hazardSetup)
{
	assert(instanceID != gUndefinedHazardInstanceID);

	m_InstanceID = instanceID;	
	m_OriginEntityID = hazardSetup.m_OriginEntityID;
	m_WarnOriginEntityFlag = hazardSetup.m_WarnOriginEntityFlag;
	if (hazardSetup.m_ExpireDelay < 0.0f)
	{
		m_ExpireTimeIndex = -1.0f;
	}
	else
	{
		m_ExpireTimeIndex = gEnv->pTimer->GetCurrTime() + hazardSetup.m_ExpireDelay;
	}	
}


void HazardData::Serialize(TSerialize ser)
{	
	ser.BeginGroup("HazardData");

	ser.Value("InstanceID", m_InstanceID);
	ser.Value("ExpireTimeIndex", m_ExpireTimeIndex);
	ser.Value("OriginEntityID", m_OriginEntityID);	

	ser.EndGroup();
}


// ===========================================================================
//	Event: The hazard has been expired.
//
void HazardData::Expire()
{
	// Nothing to do here.
}


// ============================================================================
//	Query if we should warn a specific entity for this hazard.
//
//	In:		The entity ID (0 will abort).
//
//	Returns:	True if we should warn the entity; otherwise false.
//
bool HazardData::ShouldWarnEntityID(EntityId entityID) const
{
	if (entityID == (EntityId)0)
	{
		return false;
	}

	if (m_OriginEntityID == entityID)
	{
		if (!m_WarnOriginEntityFlag)
		{
			return false;
		}
	}

	return true;
}


// ============================================================================
//	Get the 'direction' normal vector of the hazard.
//
//	Returns:	The direction normal vector of the hazard (or a 0 vector
//				if there is no specific direction, such as explosion).
//
//const Vec3& HazardData::GetNormal() const


// ============================================================================
//	Query if the agent is/can be aware of the danger.
//
//	In:			The agent.
//	In:			The avoid position in world-space.
//
//	Returns:	True if the agent is/can be aware of the danger; otherwise
//				false.
//
//bool HazardData::IsAgentAwareOfDanger(const Agent& agent, const Vec3& avoidPos) const = 0


// ============================================================================
//	Get the unique instance ID.
//	
//	Returns:	The unique instance ID (HIID_UNDEFINED if none has been assigned
//				yet).
//
HazardID HazardData::GetInstanceID() const
{
	return m_InstanceID;
}


// ============================================================================
//	Get the expire time index.
//
//	Returns:	The time index at which the hazard expires (>= 0.0) (in seconds).
//
float HazardData::GetExpireTimeIndex() const
{
	return m_ExpireTimeIndex;
}


// ============================================================================
//	Get the ID of the entity from which the hazard originated.
//
//	Returns:	The ID if the entity from which the hazard originated (0 if 
//				undefined).
//
EntityId HazardData::GetOriginEntityID() const
{
	return m_OriginEntityID;
}


// ===========================================================================
// Process possible collision between an agent and the hazard.
//
//	In:		The agent.
//	Out:	The collision result (NULL is invalid!)
//
//void HazardData::CheckCollision(Agent& agent, HazardCollisionResult* result) const


#if !defined(_RELEASE)

// ============================================================================
//	Render debug information.
//
//	In,out:		The debug renderer.
//
//void HazardData::DebugRender (IPersistantDebug& debugRenderer) const


#endif // !defined(_RELEASE)
