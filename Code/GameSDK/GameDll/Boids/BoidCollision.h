// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   BoidCollision.h
//  Version:     v1.00
//  Created:     8/2010 by Luciano Morpurgo (refactored from flock.h)
//  Compilers:   Visual C++ 7.0
//  Description: BoidCollision class declaration - manages boid collisions with deferred raycasts 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __boidcollision_h__
#define __boidcollision_h__

#if _MSC_VER > 1000
#pragma once
#endif


#include <CryPhysics/DeferredActionQueue.h>


class CBoidCollision
{
	Vec3 m_point;
	float m_dist;
	CTimeValue m_lastCheckTime;
	float m_checkDistance;
	Vec3 m_normal;
	bool m_isRequestingRayCast;
	uint32 m_reqId;
	CGame::GlobalRayCaster::ResultCallback m_callback;

public:
	inline float Distance() const {return m_dist;}
	inline Vec3& Point() {return m_point;}
	inline Vec3& Normal()  {return m_normal;}
	inline float CheckDistance() const {return m_checkDistance;}
	inline bool IsRequestingRayCast() const {return m_isRequestingRayCast;}
	inline void SetCheckDistance(float d) {m_checkDistance = d;}
	inline CTimeValue& LastCheckTime() {return m_lastCheckTime;}

	CBoidCollision()
	{
		m_isRequestingRayCast = false;
		Reset();
	}

	~CBoidCollision()
	{
		if(m_isRequestingRayCast)
			g_pGame->GetRayCaster().Cancel(m_reqId);
	}

	inline void SetNoCollision()
	{
		m_dist = -1;
	}

	void SetCollision(const RayCastResult& hitResult);
	void SetCollisionCallback(const QueuedRayID& rayID, const RayCastResult& hitResult);

	void Reset();

	inline bool IsColliding()
	{
		return m_dist>=0;
	}

	inline void UpdateTime()
	{
		m_lastCheckTime = gEnv->pTimer->GetFrameStartTime();
	}

	void QueueRaycast(EntityId entId, const Vec3& rayStart, const Vec3& rayDirection, CGame::GlobalRayCaster::ResultCallback* resultCallback = NULL);
	void RaycastCallback(const QueuedRayID& rayID, const RayCastResult& result);
};

#endif // __boidcollision_h__
