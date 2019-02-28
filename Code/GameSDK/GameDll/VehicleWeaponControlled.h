// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(__VEHICLE_WEAPON_CONTROLLED_H__)
#define __VEHICLE_WEAPON_CONTROLLED_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include <IItemSystem.h>
#include <CryAnimation/CryCharAnimationParams.h>
#include "VehicleMountedWeapon.h"
#include "VehicleWeapon.h"

struct IVehicle;
struct IVehiclePart;
struct IVehicleSeat;

#define WEAPON_CONTROLLED_HIT_RANGE 250.0f

class CControlledLocator : public IWeaponFiringLocator
{
public:

  virtual ~CControlledLocator();

  bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit);
  bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos);
  bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
  bool GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
  bool GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir);
  void WeaponReleased();
};

class CVehicleWeaponControlled : public CVehicleMountedWeapon
{
  typedef CVehicleMountedWeapon Base;

  CControlledLocator  m_ControlledLocator;
  float               m_CurrentTime;
  Ang3                m_Angles;
  bool                m_Firing;
  bool                m_FirePause;
  bool                m_FireBlocked;

public:

  CVehicleWeaponControlled();
  virtual ~CVehicleWeaponControlled() {}

  virtual void  StartFire();
  virtual void  StopFire();
  virtual void  OnReset();

  void StartUse(EntityId userId);
  void StopUse(EntityId userId);

  virtual void Update(SEntityUpdateContext& ctx, int update);
  void         Update3PAnim(CPlayer *player, float goalTime, float frameTime, const Matrix34 &mat);

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}
};


class CVehicleWeaponPulseC : public CVehicleWeapon
{
  typedef CVehicleWeapon Base;

  Vec3  m_StartDir;
  Vec3  m_TargetPos;

public:

  CVehicleWeaponPulseC();
  virtual ~CVehicleWeaponPulseC() {}

  virtual void  StartFire();
  void SetDestination(const Vec3& pos);

  virtual void Update(SEntityUpdateContext& ctx, int update);

  virtual void GetMemoryUsage(ICrySizer * s) const
  {
    s->AddObject(this, sizeof(*this));
    CWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
  }
};


#endif// #if !defined(__VEHICLE_WEAPON_CONTROLLED_H__)