// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------

Description: Helper class for melee collision detection

-------------------------------------------------------------------------
History:
- 10:03:2010   Benito G.R.

*************************************************************************/

#pragma once

#ifndef _MELEE_COLLISION_HELPER_H_
#define _MELEE_COLLISION_HELPER_H_


struct SCollisionTestParams
{	
	SCollisionTestParams()
		: m_pos(ZERO)
		, m_dir(FORWARD_DIRECTION)
		, m_distance(1.0f)
		, m_remote(false)
	{
		m_ignoredEntityIds[0] = m_ignoredEntityIds[1] = 0;
	}

	SCollisionTestParams(const Vec3& pos, const Vec3& dir, float distance, EntityId ignoredEntityId, EntityId ignoredEntityId2,  bool remote)
		: m_pos(pos)
		, m_dir(dir)
		, m_distance(distance)
		, m_remote(remote)
	{
		m_ignoredEntityIds[0] = ignoredEntityId;
		m_ignoredEntityIds[1] = ignoredEntityId2;
	}

	Vec3 m_pos;
	Vec3 m_dir;	
	float m_distance;
	EntityId m_ignoredEntityIds[2];

	bool m_remote;
};

struct IMeleeCollisionHelperListener
{
	virtual ~IMeleeCollisionHelperListener(){}
	virtual void OnSuccesfulHit(const ray_hit& hitResult) = 0;
	virtual void OnFailedHit() = 0; 
};

class CMeleeCollisionHelper
{

public:

	CMeleeCollisionHelper()
		: m_pUser(NULL)
		, m_queuedRayID(0)
	{
	}

	~CMeleeCollisionHelper()
	{
		CancelPendingRay();
	}

	void SetUser(IMeleeCollisionHelperListener* pUser)
	{
		CRY_ASSERT(pUser);
		CRY_ASSERT(!m_pUser);

		m_pUser = pUser;
	}

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);;

	//Request a collision test (when results arrives, it will callback to the user)
	void DoCollisionTest(const SCollisionTestParams& requestInfo);

	//Tries to simulate a hit on an autotarget
	bool PerformMeleeOnAutoTarget(EntityId targetId);

	//Auxiliar functions
	EntityId GetBestAutoAimTargetForUser(EntityId userId, const Vec3& scanPosition, const Vec3& scanDirection, float range, float angle) const;
	
	void Impulse(IPhysicalEntity *pCollider, const Vec3 &position, const Vec3 &impulse, int partId, int ipart, int hitTypeID);
	void GenerateArtificialCollision(IEntity* pUser, IPhysicalEntity *pCollider, const Vec3 &position, const Vec3& normal, const Vec3 &speed, int partId, int ipart, int surfaceIdx, int iPrim );

	ILINE const SCollisionTestParams& GetCollisionTestParams() const { return m_testParams; }
	ILINE bool IsBlocked() const { return (m_queuedRayID != 0); }

private:

	ILINE void OnSuccesfulHit(const ray_hit& hitResult) { CRY_ASSERT(m_pUser); m_pUser->OnSuccesfulHit(hitResult); }
	ILINE void OnFailedHit() { CRY_ASSERT(m_pUser); m_pUser->OnFailedHit(); };
	void CancelPendingRay();

	IMeleeCollisionHelperListener* m_pUser;

	SCollisionTestParams m_testParams;
	QueuedRayID m_queuedRayID;
};

#endif //_MELEE_COLLISION_HELPER_H_