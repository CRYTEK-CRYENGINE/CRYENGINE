#pragma once

#include "Entities/Helpers/ISimpleExtension.h"

////////////////////////////////////////////////////////
// Interface for weapons, see CRifle for example implementation
////////////////////////////////////////////////////////
struct ISimpleWeapon : public ISimpleExtension
{
	virtual ~ISimpleWeapon() {}

	// Call to request that the weapon is fired
	virtual void RequestFire(const Vec3 &initialBulletPosition, const Quat &initialBulletRotation) = 0;
};