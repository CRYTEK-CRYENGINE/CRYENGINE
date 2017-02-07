#pragma once

#include "ISimpleWeapon.h"

////////////////////////////////////////////////////////
// Rifle weapon entity that shoots entities as bullets
////////////////////////////////////////////////////////
class CRifle 
	: public CGameObjectExtensionHelper<CRifle, ISimpleWeapon>
{
public:
	virtual ~CRifle() {}

	// IWeapon
	virtual void RequestFire(const Vec3 &initialBulletPosition, const Quat &initialBulletRotation) override;
	// ~IWeapon
};