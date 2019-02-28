// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:
-------------------------------------------------------------------------
History:
- 07:04:2010: Created by Filipe Amim

*************************************************************************/

#pragma once

#ifndef _HOMMING_SWARM_PROJECTILE_H_
#define _HOMMING_SWARM_PROJECTILE_H_

#include "Projectile.h"



class CHommingSwarmProjectile: public CProjectile
{
private:
	struct SVehicleStatus
	{
		SVehicleStatus(IEntity* pEntity, IPhysicalEntity* pPhysics);

		Vec3 position;
		Vec3 velocity;
		Vec3 front;
		float speed;
	};

public:
	CHommingSwarmProjectile();
	virtual ~CHommingSwarmProjectile();

	virtual void Update(SEntityUpdateContext &ctx, int updateSlot);
	virtual void HandleEvent(const SGameObjectEvent &);
	virtual void Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale);

	virtual void Deflected(const Vec3& dir);

	virtual void FullSerialize(TSerialize ser);

	virtual void SetDestination(const Vec3& pos) {CProjectile::SetDestination(pos);}
	virtual void SetDestination(EntityId targetId);

private:
	typedef CProjectile BaseClass;

	Vec3 GetHomingTarget(CWeapon* pOwnerWeapon);

	Vec3 Seek(const SVehicleStatus& vehicle, const Vec3& targetPosition);
	Vec3 Wander(const SVehicleStatus& vehicle, float deltaTime);
	Vec3 Curl(const SVehicleStatus& vehicle, float deltaTime);

	void ExplodeIfMissingTarget(const SVehicleStatus& vehicle, const Vec3& velocity, const Vec3& targetPosition);

	Vec3 m_spawnDirection;
	Vec3 m_hommingDirection;
	Vec3 m_lockedDestination;
	Vec3 m_wanderState;

	EntityId m_fixedTarget;

	float m_wanderTimer;
	float m_controlLostTimer;
	float m_Roll;
	float m_curlDirection;
	float m_curlTime;
	float m_curlShift;
	bool m_hommingBehaviour;
};


#endif
