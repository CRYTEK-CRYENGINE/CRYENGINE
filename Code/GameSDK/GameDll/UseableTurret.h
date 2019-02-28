// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _USEABLE_TURRET_H_
#define _USEABLE_TURRET_H_

#include "HeavyMountedWeapon.h"


class CUseableTurret : public CHeavyMountedWeapon
{
private:
	typedef CHeavyMountedWeapon BaseClass;

public:

	enum ECUTFiringMode
	{
		ECUTFM_Rapid,
		ECUTFM_Rockets,
	};

	CUseableTurret();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void Select(bool select);
	virtual bool CanRipOff() const;
	virtual bool CanUse(EntityId userId) const;

	void SetLockedEntity(EntityId lockedEntity);

	ECUTFiringMode GetCurrentMode() { return m_currentMode; }

private:
	virtual float GetZoomTimeMultiplier();
	virtual void OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);

	void SetFiringMode(ECUTFiringMode mode);
	ECUTFiringMode GetNextMode() const;
	void CenterPlayerView(CPlayer* pPlayer);

	ECUTFiringMode m_currentMode;
	EntityId m_lockedEntity;
};


#endif
