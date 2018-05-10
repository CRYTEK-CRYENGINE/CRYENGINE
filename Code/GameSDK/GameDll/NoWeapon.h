// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NO_WEAPON__
#define __NO_WEAPON__

#pragma once

#include "Weapon.h"

class CNoWeapon: public CWeapon
{
	typedef CWeapon BaseClass;

public:

	CNoWeapon();
	virtual ~CNoWeapon();

	virtual void OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value);
	virtual void Select(bool select);

	virtual bool UpdateAimAnims(SParams_WeaponFPAiming &aimAnimParams);

protected:
	virtual bool ShouldDoPostSerializeReset() const;

private:

	void RegisterActions();
	bool OnActionMelee(EntityId actorId, const ActionId& actionId, int activationMode, float value);
};

#endif