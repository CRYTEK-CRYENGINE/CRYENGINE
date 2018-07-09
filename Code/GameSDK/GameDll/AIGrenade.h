// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  AI grenade weapon
-------------------------------------------------------------------------
History:
- 17:01:2008: Created by Benito G.R.
							Split from OffHand to a separate, smaller and simpler class

*************************************************************************/

#ifndef __AIGRENADE_H__
#define __AIGRENADE_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "Weapon.h"

class CAIGrenade : public CWeapon, public IWeaponFiringLocator
{
	struct FinishGrenadeAction;
	typedef CWeapon BaseClass;

public:
	CAIGrenade();
	virtual ~CAIGrenade();

	//IWeapon
	virtual void StartFire();
	virtual void StartFire(const SProjectileLaunchParams& launchParams);
	virtual void OnAnimationEventShootGrenade(const AnimEventInstance &event);
	virtual int  GetAmmoCount(IEntityClass* pAmmoType) const { return 1; } //Always has ammo
	virtual void OnReset();
	virtual void SetCurrentFireMode(int idx);

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}

	virtual void PostSerialize();

private:
	virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit);
	virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos);
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	virtual bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir);
	virtual void WeaponReleased();
	void SetFiringPos(const char* boneName);

	bool m_waitingForAnimEvent;
	Vec3 m_grenadeLaunchPosition;
	Vec3 m_grenadeLaunchVelocity;
};


#endif // __AIGRENADE_H__