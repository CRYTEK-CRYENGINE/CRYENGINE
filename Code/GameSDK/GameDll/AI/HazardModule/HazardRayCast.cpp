// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// An 'interface' class that assists hazard definitions that utilize a ray-cast in some way.

#include "StdAfx.h"

#include "HazardRayCast.h"

#include "HazardModule.h"

#include "../../Single.h"

using namespace HazardSystem;


// ============================================================================
// ============================================================================
// ============================================================================
//
// -- HazardDataRayCast -- HazardDataRayCast -- HazardDataRayCast --
//
// ============================================================================
// ============================================================================
// ============================================================================


HazardDataRayCast::HazardDataRayCast() :
	m_RayCastState(RayCastRequested)
	, m_QueuedRayID((QueuedRayID)0)
	, m_PendingRayStartPos(ZERO)
	, m_PendingRayNormal(ZERO)
{
}


HazardDataRayCast::HazardDataRayCast(const HazardDataRayCast& source) :
	BaseClass(source)
	, m_RayCastState(source.m_RayCastState)
	, m_QueuedRayID(source.m_QueuedRayID)
	, m_PendingRayStartPos(source.m_PendingRayStartPos)
	, m_PendingRayNormal(source.m_PendingRayNormal)
{
}


HazardDataRayCast::~HazardDataRayCast()
{
	// Nothing to do here.
}


void HazardDataRayCast::Serialize(TSerialize ser)
{
	BaseClass::Serialize(ser);

	ser.BeginGroup("HazardDataRayCast");

	ser.EnumValue("RayCastState", m_RayCastState, RayCastRequested, RayCastLast);

	// Safety code: the queued rays cannot be transitioned across save-games,
	// so we will just have to redo them.
	if (ser.IsReading())
	{	
		m_QueuedRayID = (QueuedRayID)0;

		switch (m_RayCastState)
		{
		case RayCastRequested:
			break;

		case RayCastPending:
			m_RayCastState = RayCastRequested;
			break;

		case RayCastCompleted:
			break;

		default:
			// We should never get here!
			assert(false);
		}
	}

	ser.Value("PendingRayStartPos", m_PendingRayStartPos);
	ser.Value("PendingRayNormal", m_PendingRayNormal);

	ser.EndGroup();
}


// ===========================================================================
//	Event: The hazard has been expired.
//
void HazardDataRayCast::Expire()
{
	BaseClass::Expire();

	CancelPendingRayCast();	
}


// ============================================================================
//	Query if the hazard has any pending ray-casts.
//
//	Returns:	True if it has pending ray-casts; otherwise false.
//
bool HazardDataRayCast::HasPendingRayCasts() const
{
	switch (m_RayCastState)
	{
	case RayCastRequested:
	case RayCastCompleted:
		return false;

	case RayCastPending:
		return true;

	default:
		break;
	};

	// We should never get here!
	assert(false);
	return false;
}


// ============================================================================
//	Start any ray-casts that were requested but are not yet pending.
//
//	In,out:		The hazard module (NULL is invalid!)
//
void HazardDataRayCast::StartRequestedRayCasts(HazardModule *hazardModule)
{
	assert(hazardModule != NULL);

	if (m_RayCastState == RayCastRequested)
	{
		QueueRayCast(hazardModule, m_PendingRayStartPos, m_PendingRayNormal);
	}
}


// ===========================================================================
//	Query if we are waiting for a specific ray ID.
//
//	In:		The ray ID.
//
//	Returns:	True if waiting for the ray with the specified ID; otherwise false.
//
bool HazardDataRayCast::IsWaitingForRay(const QueuedRayID rayID) const
{
	if (!HasPendingRayCasts())
	{
		return false;
	}
	if (m_QueuedRayID != rayID)
	{
		return false;
	}
	return true;
}


// ===========================================================================
//	Queue a ray-cast so that we can construct the hazard volume later on.
//
//	Note: this function should alway be called through the entire inheritance 
//	tree.
//
//	In,out:	The hazard module (NULL is invalid!)
//	In:		The start position of the ray (in world-space). This will be the 
//			actual start of the warning area.
//	In:		The ray direction normal (in world-space).
//
void HazardDataRayCast::QueueRayCast(
	HazardModule *hazardModule,	
	const Vec3& rayStartPos, const Vec3& rayNormal)
{
	assert(hazardModule != NULL);	

	CancelPendingRayCast(); // (Just in case).

	static const int objTypes = ent_all;
	static const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (7 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

	static PhysSkipList skipList;
	skipList.clear();

	m_PendingRayStartPos = rayStartPos;
	m_PendingRayNormal   = rayNormal;

	CWeapon* weapon = GetIgnoredWeapon();
	if (weapon != NULL)
	{	
		CSingle::GetSkipEntities(weapon, skipList);
	}

	const float maxScanDistance = GetMaxRayCastDistance();
	assert(maxScanDistance >= 0.0f);

	m_QueuedRayID = g_pGame->GetRayCaster().Queue(
		RayCastRequest::HighPriority,
		RayCastRequest(
		rayStartPos, 
		rayNormal * maxScanDistance,					
		objTypes,
		flags,
		!skipList.empty() ? &skipList[0] : NULL,
		skipList.size()),
		// The reason we can't deliver these notifications on this same object is that
		// it is stored in a vector and thus its memory location may be altered
		// due to vector manipulations!
		functor(*hazardModule, &HazardModule::OnRayCastDataReceived));

	m_RayCastState = RayCastPending;
}


// ===========================================================================
//	Main event handler: received ray-cast data for constructing the hazard 
//  warning area.
//
//	In:		The ray ID.
//	In:		Th ray-cast results.
//
void HazardDataRayCast::MainProcessRayCastResult(const QueuedRayID rayID, const RayCastResult& result)
{
	assert(IsWaitingForRay(rayID));

	m_QueuedRayID = (QueuedRayID)0;

	m_RayCastState = RayCastCompleted;

	ProcessRayCastResult(m_PendingRayStartPos, m_PendingRayNormal, result);
}


// ===========================================================================
//	Event: received ray-cast data for constructing the hazard warning area.
//
//	In:		The ray start position (in world-space).
//	In:		The ray normal (in world-space).
//	In:		Th ray-cast results.
//
//void HazardDataProjectile::ProcessRayCastResult(const Vec3& rayStartPos, const Vec3& rayNormal, const RayCastResult& result) = 0;


// ============================================================================
//	Cancel a possible pending ray-cast (if needed).
//
//	This function will also request a new ray-cast to be performed as soon
//	as possible.
//
void HazardDataRayCast::CancelPendingRayCast()
{
	if ((m_RayCastState != RayCastPending) && (m_QueuedRayID == (QueuedRayID)0))
	{
		return;
	}

	g_pGame->GetRayCaster().Cancel(m_QueuedRayID);
	m_RayCastState = RayCastRequested;
	m_QueuedRayID = (QueuedRayID)0;	
}


// ============================================================================
//	Get the ignored weapon instance.
//
//	Returns:	The ignored weapon instance (or NULL if none).
//
//CWeapon *HazardDataRayCast::GetIgnoredWeapon() const = 0;


// ============================================================================
//	Get the maximum scanning distance for ray-casting.
//	
//	@returns	The maximum scanning distance for ray-casting (>= 0.0f)
//				(in meters).
//
//float HazardDataRayCast::GetMaxRayCastDistance() const = 0;
