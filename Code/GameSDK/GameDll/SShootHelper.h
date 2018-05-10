// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SSHOOT_HELPER_H__
#define __SSHOOT_HELPER_H__

class CProjectile;
struct SAmmoParams;

class SShootHelper
{
public:
	static CProjectile* Shoot(EntityId ownerID, const EntityId weaponID, const char* ammo, int hitTypeId, const Vec3& firePos, const Vec3& fireDir, int damage, bool isProxy = false);
	static void Explode(const EntityId ownerID, const EntityId weaponID, const char* ammo, const Vec3& firePos, const Vec3& fireDir, int damage, float desiredRadius = -1.0f, bool skipExplosionEffect = false);
	static void DoLocalExplodeEffect(EntityId ownerID, const char* ammo, const Vec3& firePos, const Vec3& fireDir, float desiredRadius = -1.0f);
private:
	static const SAmmoParams* GetAmmoParams(const char* ammo);
};

#endif