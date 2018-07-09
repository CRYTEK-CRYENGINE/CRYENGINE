// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLESEATACTIONWEAPONSBONE_H
#define __VEHICLESEATACTIONWEAPONSBONE_H

#include "VehicleSeatActionWeapons.h"
#include <CryAnimation/ICryAnimation.h>

struct ISkeletonPose;

class CVehicleSeatActionWeaponsBone : public CVehicleSeatActionWeapons
{
public:
	CVehicleSeatActionWeaponsBone();
	virtual ~CVehicleSeatActionWeaponsBone();

	// IVehicleSeatAction
	virtual bool Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table);
	// ~IVehicleSeatAction

	// IWeaponFiringLocator
	virtual bool GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit);
	virtual bool GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos);
	virtual bool GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos);
	// ~IWeaponFiringLocator

	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void UpdateWeaponTM(SVehicleWeapon& weapon);

	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);

protected:

	bool CalcFiringPosDir(Vec3& rPos, Vec3* pDir = NULL, const IFireMode* pFireMode = NULL) const;

	Quat                       m_currentMovementRotation;
	ISkeletonPose*             m_pSkeletonPose;
	int                        m_positionBoneId;
	IAnimationOperatorQueuePtr m_poseModifier;
	float                      m_forwardOffset;
};

#endif
