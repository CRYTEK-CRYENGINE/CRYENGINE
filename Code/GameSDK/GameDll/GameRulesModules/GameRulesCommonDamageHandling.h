// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 01:08:2010  : Created by Ben Parbury

*************************************************************************/

#ifndef _GAME_RULES_COMMON_DAMAGE_HANDLING_H_
#define _GAME_RULES_COMMON_DAMAGE_HANDLING_H_

#define MAX_BONES 100
#define MAX_PARTS 16

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesDamageHandlingModule.h"
#include "GameRulesModules/IGameRulesKillListener.h"

class CGameRulesCommonDamageHandling :	public IGameRulesDamageHandlingModule
{
	struct SCollisionEntityInfo
	{
		explicit SCollisionEntityInfo(const IEntity* _pEntity);

		EntityId			entityId;
		const IEntity*		pEntity;
		const IActor*		pEntityActor;
		const IVehicle*		pEntityVehicle;
	};

public:
	CGameRulesCommonDamageHandling();
	virtual ~CGameRulesCommonDamageHandling();

	// IGameRulesDamageHandlingModule
	virtual void Init(XmlNodeRef xml);
	virtual void PostInit();
	virtual void Update(float frameTime);

	virtual bool SvOnHit(const HitInfo &hitInfo);
	virtual bool SvOnHitScaled(const HitInfo &hitInfo);
	virtual void SvOnExplosion(const ExplosionInfo &explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities);
	virtual void SvOnCollision(const IEntity *entity, const CGameRules::SCollisionHitInfo& colHitInfo);

	virtual void ClProcessHit(Vec3 dir, EntityId shooterId, EntityId weaponId, float damage, uint16 projectileClassId, uint8 hitTypeId);

	virtual bool AllowHitIndicatorForType(int hitTypeId);

	virtual void MakeMovementVisibleToAIForEntityClass(const IEntityClass* pEntityClass);

	virtual void OnGameEvent(const IGameRulesDamageHandlingModule::EGameEvents& gameEvent);
	// ~IGameRulesDamageHandlingModule

protected:

	float GetCollisionEnergy(const IEntity *pEntity, const CGameRules::SCollisionHitInfo& colHitInfo) const;
	float GetCollisionDamageMult(const IEntity *pEntity, const IEntity *pCollider, const CGameRules::SCollisionHitInfo& colHitInfo) const;

	float GetAIPlayerAgainstColliderEnergyScale(const IEntity& collider) const;
	float GetPlayerAgainstColliderEnergyScale(const IEntity& collider) const;

	float GetPlayerSelfCollisionMultiplier(const CGameRulesCommonDamageHandling::SCollisionEntityInfo& colliderInfo) const;
	float GetAIPlayerSelfCollisionMultiplier(const CGameRulesCommonDamageHandling::SCollisionEntityInfo& colliderInfo) const;
	float GetVehicleForeignCollisionMultiplier(const IVehicle& vehicle, const CGameRulesCommonDamageHandling::SCollisionEntityInfo& colliderInfo, const CGameRules::SCollisionHitInfo& colHitInfo ) const;

	void LogHit(const HitInfo& hit, bool extended, bool dead);

#ifndef _RELEASE
	bool DebugCollisions() const;
#else
	ILINE bool DebugCollisions() const { return false; }
#endif

	SmartScriptTable m_scriptHitInfo;

	CGameRules *m_pGameRules;

	IEntityClass* m_pEnvironmentalWeaponClass;
};

#endif // _GAME_RULES_COMMON_DAMAGE_HANDLING_H_
