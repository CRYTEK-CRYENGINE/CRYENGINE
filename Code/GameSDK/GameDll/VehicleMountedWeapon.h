// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  Mounted machine gun that can be ripped off by the player
and move around with it - vehicle mounted version
-------------------------------------------------------------------------
History:
- 16:02:2010: Created by SNH

*************************************************************************/

#pragma once

#ifndef _VEHICLE_MOUNTED_WEAPON_H_
#define _VEHICLE_MOUNTED_WEAPON_H_

#include <IItemSystem.h>
#include <CryAnimation/CryCharAnimationParams.h>
#include "HeavyMountedWeapon.h"

struct IVehicle;
struct IVehiclePart;
struct IVehicleSeat;

class CVehicleMountedWeapon : public CHeavyMountedWeapon
{
public:

	CVehicleMountedWeapon();

	// CWeapon
	virtual void StartUse(EntityId userId);
	virtual void ApplyViewLimit(EntityId userId, bool apply);

	virtual void StartFire();

	virtual void Update(SEntityUpdateContext& ctx, int update);

	virtual void SetAmmoCount(IEntityClass* pAmmoType, int count);
	virtual void SetInventoryAmmoCount(IEntityClass* pAmmoType, int count);

	virtual bool CanZoom() const;

	virtual void GetMemoryUsage(ICrySizer * s) const
	{
		s->AddObject(this, sizeof(*this));
		CHeavyMountedWeapon::GetInternalMemoryUsage(s); // collect memory of parent class
	}	

	virtual bool ShouldBindOnInit() const { return false; }

	virtual bool ApplyActorRecoil() const { return (m_pOwnerSeat == m_pSeatUser); }  

	virtual void FullSerialize(TSerialize ser);
	virtual void PostSerialize();
	// ~CWeapon

	virtual void Use(EntityId userId);
	virtual void StopUse(EntityId userId);
	bool CanRipOff() const;

	bool CanUse(EntityId userId) const;

	virtual void MountAtEntity(EntityId entityId, const Vec3 &pos, const Ang3 &angles);

protected:

	bool CheckWaterLevel() const;
	virtual void PerformRipOff(CActor* pOwner);
	virtual void FinishRipOff();
	void ResetState();

	EntityId m_vehicleId;
	IVehicleSeat* m_pOwnerSeat; // owner seat of the weapon
	IVehicleSeat* m_pSeatUser; // seat of the weapons user

private:

	void CorrectRipperEntityPosition(float timeStep);

	Quat	m_previousVehicleRotation;
	Vec3    m_previousWSpaceOffsetPosition; 
	Vec3	m_localRipUserOffset; 
	float	m_dtWaterLevelCheck;
	bool	m_usedThisFrame; //Stop this item being used multiple times in a single frame (As you end up exiting then re-entering)
};

#endif // _VEHICLEHMG_H_
