// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidObject.cpp
//  Version:     v1.00
//  Created:     8/2010 by Luciano Morpurgo (refactored from flock.cpp)
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BoidCollision.h"


void CBoidCollision::SetCollision(const RayCastResult& hitResult)
{
	if(hitResult.hitCount)
	{
		const ray_hit& hit = hitResult.hits[0];
		m_dist = hit.dist;
		m_point = hit.pt;
		m_normal = hit.n;
	}
	else
		SetNoCollision();
}

void CBoidCollision::SetCollisionCallback(const QueuedRayID& rayID, const RayCastResult& hitResult)
{
	SetCollision(hitResult);
}

void CBoidCollision::Reset()
{
	SetNoCollision();
	m_lastCheckTime.SetMilliSeconds(0);
	m_checkDistance = 5.f;
	if(m_isRequestingRayCast)
		g_pGame->GetRayCaster().Cancel(m_reqId);

	m_callback = NULL;
	m_isRequestingRayCast = false;
}

void CBoidCollision::QueueRaycast(EntityId entId, const Vec3& rayStart, const Vec3& rayDirection , CGame::GlobalRayCaster::ResultCallback* resultCallback)
{
	if (m_isRequestingRayCast)
		return;

	const int flags = rwi_colltype_any | rwi_ignore_back_faces | rwi_stop_at_pierceable | rwi_queue;
	const int entityTypes = ent_static | ent_sleeping_rigid | ent_rigid | ent_terrain;

	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entId);
	IPhysicalEntity* pSkipList = pEntity ? pEntity->GetPhysics(): NULL;
	int n = pSkipList ? 1 : 0;

	m_callback = resultCallback ? *resultCallback : functor(*this,&CBoidCollision::SetCollisionCallback);

	m_isRequestingRayCast = true;
	//m_callback = resultCallback;
	m_reqId = g_pGame->GetRayCaster().Queue(
		RayCastRequest::HighPriority,
		RayCastRequest(rayStart, rayDirection,
		entityTypes, flags, pSkipList? &pSkipList : NULL, n, 1),
		functor(*this, &CBoidCollision::RaycastCallback));
	return;
}


////////////////////////////////////////////////////////////////

void CBoidCollision::RaycastCallback(const QueuedRayID& rayID, const RayCastResult& result)
{
	if(m_callback)
		m_callback(rayID,result);
	m_isRequestingRayCast = false;
}

