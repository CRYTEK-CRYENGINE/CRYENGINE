// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Throw Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 261:10:2005   15:45 : Created by Márcio Martins
-  18:07:2008		Slightly Refactored (cleaned-up): Benito G.R.

*************************************************************************/
#ifndef __THROW_H__
#define __THROW_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Single.h"

#define MAX_TRAJECTORY_SAMPLES 200

class CThrow :	public CSingle
{
private:

	typedef CSingle BaseClass;

	struct ThrowAction;
	struct FinishAction;
	struct PrimeAction;
	struct HoldAction;
	struct ShowItemAction;
		
	class CThrowFiringLocator : public IWeaponFiringLocator
	{
	public:
		CThrowFiringLocator();
		void SetValues(const Vec3& pos, const Vec3& dir);

	private:
		virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit) {return false; }
		virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos);
		virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
		virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos) {return false; }
		virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) {return false; }
		virtual void WeaponReleased() {}

		Vec3 m_eyeDir;
		Vec3 m_eyePos;
	};


public:
	CRY_DECLARE_GTI(CThrow);

	CThrow();
	virtual ~CThrow();

	virtual void Update(float frameTime, uint32 frameId) override;
	virtual void GetMemoryUsage(ICrySizer * s) const override;

	virtual void SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams) override;

	virtual void Activate(bool activate) override;

	virtual bool CanReload() const override;

	virtual void StartFire() override;
	virtual void StopFire() override;

	virtual void NetStartFire() override;
	virtual void NetStopFire() override;
	virtual void NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle) override;
	virtual void ReplayShoot() override;

	virtual bool IsReadyToFire() const override;
	virtual bool Shoot(bool resetAnimation, bool autoreload =true , bool isRemote=false ) override;
	virtual bool OutOfAmmo() const override;

	virtual bool IsFiring() const  override { return m_firing || m_throwing; };

	// If we need more of this secondary functions, we might consider create one common in IFireMode interface
	virtual void Prime();
	virtual bool IsReadyToThrow() const { return (m_firing && !m_throwing); }
	void FumbleGrenade();

	ILINE const Vec3* GetTrajectory() const { return m_trajectory; }
	ILINE const uint32 GetTrajectoryLength() const { return m_trajectoryLength; }

	ILINE const float	GetExplodeTime() const { return m_explodeTime; }
	ILINE const float GetProjectileLifeTime() const { return m_projectileLifeTime; }

protected:
	virtual void CheckNearMisses(const Vec3 &probableHit, const Vec3 &pos, const Vec3 &dir, float range, float radius) override;
	virtual bool CheckAmmo();
	virtual void DoThrow();
	virtual void CalculateTrajectory();
	void RenderTrajectory(const Vec3* trajectory, unsigned int sampleCount, const float time);

	void ShootInternal(Vec3 hit, Vec3 pos, Vec3 dir, Vec3 vel, bool clientIsShooter);
	void StartFireInternal();

private:

	CThrowFiringLocator m_throwFiringLocator;

	Vec3 m_trajectory[MAX_TRAJECTORY_SAMPLES];

	Vec3 m_predictedPos;
	Vec3 m_predictedDir;
	Vec3 m_predictedHit;
	
	bool		m_predicted;
	bool    m_throwing;
	bool		m_primed;
	float		m_primeTime;
	float   m_projectileLifeTime;
	float   m_firedTime;

	float		m_explodeTime;

	float			m_startFireTime;

	uint32		m_trajectoryLength;
};

#endif 
