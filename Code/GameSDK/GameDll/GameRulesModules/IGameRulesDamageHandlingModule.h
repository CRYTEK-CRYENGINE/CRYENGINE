// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 07:09:2009  : Created by Ben Johnson
	- 08:09:2009  : Written by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_DAMAGE_HANDLING_MODULE_H_
#define _GAME_RULES_DAMAGE_HANDLING_MODULE_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameRulesSystem.h>
#include "GameRules.h"

class CWeapon;

class IGameRulesDamageHandlingModule
{
public:

	enum EGameEvents
	{
		eGameEvent_LocalPlayerEnteredMercyTime = 0,
		eGameEvent_TrainRideStarts,
		eGameEvent_TrainRideEnds
	};

	virtual ~IGameRulesDamageHandlingModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;
	virtual void Update(float frameTime) = 0;

	virtual bool SvOnHit(const HitInfo &hitInfo) = 0;
	virtual bool SvOnHitScaled(const HitInfo &hitInfo) = 0;
	virtual void SvOnExplosion(const ExplosionInfo &explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities) = 0;
	virtual void SvOnCollision(const IEntity *entity, const CGameRules::SCollisionHitInfo& colHitInfo) = 0;

	virtual void ClProcessHit(Vec3 dir, EntityId shooterId, EntityId weaponId, float damage, uint16 projectileClassId, uint8 hitTypeId) = 0;

	virtual bool AllowHitIndicatorForType(int hitTypeId) = 0;

	virtual void MakeMovementVisibleToAIForEntityClass(const IEntityClass* pEntityClass) = 0;

	virtual void OnGameEvent(const EGameEvents& gameEvent) = 0;

	enum ePID_flags
	{
		PID_None = 0,
		PID_Headshot = BIT(1),
	};
};

#endif // _GAME_RULES_DAMAGE_HANDLING_MODULE_H_