// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SShootHelper.h"

#include "Game.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include "GameRules.h"

#include "EntityUtility/EntityEffects.h"

void SShootHelper::DoLocalExplodeEffect(EntityId ownerID, const char* ammo, const Vec3& firePos, const Vec3& fireDir, float desiredRadius)
{
	const SAmmoParams* pAmmoParams = GetAmmoParams(ammo);
	if(!pAmmoParams)
	{
		CRY_ASSERT_MESSAGE(false, ("unable to GetAmmoParams for %s", ammo));
		return;
	}

	const SExplosionParams* pExplosionParams = pAmmoParams->pExplosion;
	if(!pExplosionParams)
	{
		CRY_ASSERT_MESSAGE(false, ("unable to SExplosionParams for %s", ammo));
		return;
	}

	
	if (!pExplosionParams->effectName.empty())
	{
		const float scaleRadius = desiredRadius > 0 ? desiredRadius/pExplosionParams->maxRadius : 1.0f;

		if (EntityEffects::SpawnParticleFX( pExplosionParams->effectName.c_str(), EntityEffects::SEffectSpawnParams( firePos, fireDir, pExplosionParams->effectScale * scaleRadius, -1.0f, false) ) == NULL)
		{
			CRY_ASSERT_MESSAGE(false, ("Unable find effect %s", pExplosionParams->effectName.c_str()));
		}
	}
}

void SShootHelper::Explode(const EntityId ownerID, const EntityId weaponID, const char* ammo, const Vec3& firePos, const Vec3& fireDir, int damage, float desiredRadius, bool skipExplosionEffect)
{
	if (gEnv->bServer)
	{
		const SAmmoParams* pAmmoParams = GetAmmoParams(ammo);
		if(!pAmmoParams)
		{
			CRY_ASSERT_MESSAGE(false, ("unable to GetAmmoParams for %s", ammo));
			return;
		}

		const SExplosionParams* pExplosionParams = pAmmoParams->pExplosion;
		if(!pExplosionParams)
		{
			CRY_ASSERT_MESSAGE(false, ("unable to find SExplosionParams for %s", ammo));
			return;
		}

		CGameRules *pGameRules = g_pGame->GetGameRules();
		CRY_ASSERT_MESSAGE(pGameRules, "Unable to find game rules, gonna crash");

		float scaleRadius = desiredRadius > 0 ? desiredRadius/pExplosionParams->maxRadius : 1.0f;

		CRY_ASSERT_MESSAGE(pExplosionParams->hitTypeId, string().Format("Invalid hit type '%s' in explosion params for '%s'", pExplosionParams->type.c_str(), ammo));
		CRY_ASSERT_MESSAGE(pExplosionParams->hitTypeId == pGameRules->GetHitTypeId(pExplosionParams->type.c_str()), "Sanity Check Failed: Stored explosion type id does not match the type string, possibly PreCacheLevelResources wasn't called on this ammo type");

		ExplosionInfo explosionInfo(ownerID, weaponID, 0, (float) damage, firePos, fireDir,
			pExplosionParams->minRadius * scaleRadius, pExplosionParams->maxRadius * scaleRadius,
			pExplosionParams->minPhysRadius * scaleRadius, pExplosionParams->maxPhysRadius * scaleRadius,
			0.0f, pExplosionParams->pressure, pExplosionParams->holeSize, pExplosionParams->hitTypeId);

		
		explosionInfo.SetEffect(skipExplosionEffect ? "" : pExplosionParams->effectName.c_str(), pExplosionParams->effectScale * scaleRadius, pExplosionParams->maxblurdist);
		explosionInfo.SetEffectClass(pAmmoParams->pEntityClass->GetName());
		explosionInfo.SetFriendlyFire(pExplosionParams->friendlyFire);
		explosionInfo.soundRadius = pExplosionParams->soundRadius;

		g_pGame->GetIGameFramework()->GetNetworkSafeClassId(explosionInfo.projectileClassId, ammo);

		pGameRules->QueueExplosion(explosionInfo);
	}
}

CProjectile* SShootHelper::Shoot(EntityId ownerID, const EntityId weaponID, const char* ammo, int hitTypeId, const Vec3& firePos, const Vec3& fireDir, int damage, bool isProxy)
{
	IEntityClass* pAmmoClass = gEnv->pEntitySystem ? gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo) : NULL;
	if(!pAmmoClass)
	{
		CRY_ASSERT_MESSAGE(false, string().Format("Unable to find ammo class - %s", ammo));
		return NULL;
	}

	CProjectile* pAmmo = g_pGame->GetWeaponSystem()->SpawnAmmo(pAmmoClass, false);
	if(!pAmmo)
	{
		CRY_ASSERT_MESSAGE(false, string().Format("Unable to Spawn ammo - %s", ammo));
		return NULL;
	}

	CRY_ASSERT_MESSAGE(hitTypeId, "Invalid hit type id passed to SShootHelper::Shoot");

	pAmmo->SetParams(CProjectile::SProjectileDesc(ownerID, 0, weaponID, damage, 0.f, 0.f, 0.f, hitTypeId, 0, false));
	pAmmo->Launch(firePos, fireDir, Vec3(0.0f, 0.0f, 0.0f));
	pAmmo->SetFiredViaProxy(isProxy);
	pAmmo->SetRemote(ownerID != gEnv->pGameFramework->GetClientActorId());

	return pAmmo;
}

const SAmmoParams* SShootHelper::GetAmmoParams(const char* ammo)
{
	IEntityClass* pAmmoClass = gEnv->pEntitySystem ? gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammo) : NULL;
	if(!pAmmoClass)
	{
		CRY_ASSERT_MESSAGE(false, string().Format("Unable to find ammo class - %s", ammo));
		return NULL;
	}

	return g_pGame->GetWeaponSystem()->GetAmmoParams(pAmmoClass);
}
