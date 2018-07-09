// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
static skill kill checks 
- 02:02:2010		Created by Ben Parbury
*************************************************************************/

#include "StdAfx.h"
#include "SkillKill.h"

#include "Player.h"
#include "GameRules.h"
#include "PersistantStats.h"

#include "IVehicleSystem.h"
#include "VehicleMovementBase.h"


#define RumbledTimeAllowedAfterKnockingOutOfStealth 0.5f

bool SkillKill::s_firstBlood = true;
uint16 SkillKill::s_flashBangGrenadesClassId = 0;

bool SkillKill::IsFirstBlood(IActor *pShooter, IActor *pVictim)
{
#if USE_PC_PREMATCH
	if(CGameRules * pGameRules = g_pGame->GetGameRules())
	{
		if(pGameRules->GetPrematchState() != CGameRules::ePS_Match)
		{
			return false;
		}
	}
#endif

	if(pShooter && pVictim && pShooter != pVictim)
	{
		CPlayer *pShooterPlayer = static_cast<CPlayer*>(pShooter);
		if(s_firstBlood && !pShooterPlayer->IsFriendlyEntity(pVictim->GetEntityId()))
		{
			s_firstBlood = false;
			return true;
		}
	}
	return false;
}

void SkillKill::SetFirstBlood(bool firstBlood)
{
	s_firstBlood = firstBlood;
}

bool SkillKill::GetFirstBlood()
{
	return s_firstBlood;
}

void SkillKill::Reset()
{
	s_firstBlood = true;

	if (IGameFramework* pFramework = g_pGame->GetIGameFramework())
	{
		pFramework->GetNetworkSafeClassId(s_flashBangGrenadesClassId, "FlashBangGrenades");
	}
}

bool SkillKill::IsHeadshot(CPlayer* pTargetPlayer, int hit_bodyPart, int material)
{
	HitInfo hitInfo;
	hitInfo.partId = hit_bodyPart;
	hitInfo.material = material;
	return pTargetPlayer->IsHeadShot(hitInfo);
}

bool SkillKill::IsAirDeath(CPlayer* pTargetPlayer)
{
	SActorStats* pTargetStats = pTargetPlayer->GetActorStats();
	return pTargetStats->inAir > CPersistantStats::k_actorStats_inAirMinimum && !pTargetStats->bStealthKilled;
}

bool SkillKill::IsStealthKill(CPlayer* pTargetPlayer)
{
	SActorStats* pTargetStats = pTargetPlayer->GetActorStats();
	return pTargetStats->bStealthKilled;
}

bool SkillKill::IsMeleeTakedown(CGameRules* pGameRules, int hit_type)
{
	const HitTypeInfo* pHitTypeInfo = pGameRules->GetHitTypeInfo( hit_type );
	
	return (pHitTypeInfo != NULL) && (pHitTypeInfo->m_flags & CGameRules::EHitTypeFlag::IsMeleeAttack);
}

bool SkillKill::IsBlindKill(CPlayer* pShooterPlayer)
{
	const SPlayerStats * pPlayerStats = static_cast<const SPlayerStats*>(pShooterPlayer->GetActorStats());
	return pPlayerStats->flashBangStunMult != 1.0f;
}

bool SkillKill::IsRumbled(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	return false;
}

bool SkillKill::IsDefiantKill(CPlayer* pShooterPlayer)
{
	float lowHealthTime = pShooterPlayer->GetTimeEnteredLowHealth();

	if(lowHealthTime > 0.f && !pShooterPlayer->IsDead())
	{
		return (gEnv->pTimer->GetFrameStartTime().GetSeconds() - lowHealthTime) >= g_pGameCVars->g_defiant_timeAtLowHealth;
	}

	return false;
}

bool SkillKill::IsKillJoy(CPlayer* pTargetPlayer)
{
	return g_pGame->GetPersistantStats()->GetCurrentStat(pTargetPlayer->GetEntityId(), ESIPS_Kills) > 2;
}

EPPType SkillKill::IsMultiKill(CPlayer* pShooterPlayer)
{
	const int multiKillStreak = g_pGame->GetPersistantStats()->GetCurrentStat(pShooterPlayer->GetEntityId(), ESIPS_MultiKillStreak);
	switch(multiKillStreak)
	{
	case 2:
		return EPP_DoubleKill;
	case 3:
		return EPP_TripleKill;
	case 4:
		return EPP_QuadKill;
	case 5:
		return EPP_QuinKill;
	}

	return EPP_Invalid;
}

bool SkillKill::IsPiercing(int penetration)
{
	return penetration > 0;
}

bool SkillKill::IsBlinding(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer, uint16 weaponClassId)
{
	if(pTargetPlayer->GetLastFlashbangShooterId() == pShooterPlayer->GetEntityId())
	{
		if(gEnv->pTimer->GetCurrTime() - pTargetPlayer->GetLastFlashbangTime() < g_pGameCVars->g_blinding_timeBetweenFlashbangAndKill)
		{
			if(weaponClassId != s_flashBangGrenadesClassId)	//not flashbang that killed them
			{
				return true;
			}
		}
	}

	return false;
}

bool SkillKill::IsFlushed(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer, CGameRules* pGameRules, int hit_type)
{
	if(pShooterPlayer->IsClient() && hit_type != CGameRules::EHitType::Frag && hit_type != CGameRules::EHitType::Explosion)
	{
		return g_pGame->GetPersistantStats()->HasClientFlushedTarget(pTargetPlayer->GetEntityId(), pTargetPlayer->GetEntity()->GetWorldPos());
	}

	return false;
}

bool SkillKill::IsDualWeapon(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	if(pShooterPlayer->IsClient())
	{
		return g_pGame->GetPersistantStats()->IsClientDualWeaponKill(pTargetPlayer->GetEntityId());
	}

	return false;
}

bool SkillKill::IsRecoveryKill(CPlayer* pShooterPlayer)
{
	return false;
}

bool SkillKill::IsRetaliationKill(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	bool retaliationKill = false;

	if(pShooterPlayer->IsClient())
	{
		retaliationKill = g_pGame->GetPersistantStats()->CheckRetaliationKillTarget(pTargetPlayer->GetEntityId());
	}

	return retaliationKill;
}

bool SkillKill::IsGotYourBackKill(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	//To use the new actor manager stuff when merged back to postalpha
	const float maxDistSq = sqr(g_pGameCVars->g_gotYourBackKill_targetDistFromFriendly);
	const float fovRange = cos_tpl(DEG2RAD(g_pGameCVars->g_gotYourBackKill_FOVRange));

	IActorIteratorPtr pActorIterator = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();

	Vec3 targetLocation = pTargetPlayer->GetEntity()->GetWorldPos();
	SMovementState targetMovementState;
	pTargetPlayer->GetMovementController()->GetMovementState(targetMovementState);

	Vec3 targetAimDirection = targetMovementState.aimDirection;

	while(CActor* pActor = static_cast<CActor*>(pActorIterator->Next()))
	{
		if(pActor != pShooterPlayer && !pActor->IsDead() && pShooterPlayer->IsFriendlyEntity(pActor->GetEntityId()))
		{
			Vec3 actorLocation = pActor->GetEntity()->GetWorldPos(); 
			Vec3 distance = actorLocation - targetLocation;

			if(distance.GetLengthSquared() < maxDistSq)
			{
				distance.Normalize();
				if(distance.Dot(targetAimDirection) > fovRange)
				{
					SMovementState actorMovementState;
					pActor->GetMovementController()->GetMovementState(actorMovementState);
					Vec3 actorAimDirection = actorMovementState.aimDirection;

					if(actorAimDirection.Dot(-distance) < fovRange)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool SkillKill::IsGuardianKill(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	EntityId targetId = pTargetPlayer->GetEntityId();

	float currentTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	IActorIteratorPtr pActorIterator = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();

	while(CActor* pActor = static_cast<CActor*>(pActorIterator->Next()))
	{
		if(pActor != pShooterPlayer && !pActor->IsDead() && pActor->IsPlayer() && pShooterPlayer->IsFriendlyEntity(pActor->GetEntityId()))
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pActor);

			if((pPlayer->GetLastAttacker() == targetId) && (currentTime - pPlayer->GetLastDamageSeconds() < g_pGameCVars->g_guardian_maxTimeSinceLastDamage))
			{
				return true;
			}
		}
	}

	return false;
}

bool SkillKill::IsInterventionKill(CPlayer* pShooterPlayer, CPlayer* pTargetPlayer)
{
	EntityId targetsTargetId = pTargetPlayer->GetCurrentTargetEntityId();
	if(targetsTargetId != 0)
	{
		if(pShooterPlayer->IsFriendlyEntity(targetsTargetId) && (pShooterPlayer->GetEntityId() != targetsTargetId))
		{
			CPlayer* pTargetsTargetPlayer = (CPlayer*) g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(targetsTargetId);
			if(pTargetsTargetPlayer != NULL && !pTargetsTargetPlayer->IsDead())
			{
				float curTime = gEnv->pTimer->GetCurrTime();
				if(pTargetPlayer->GetLastZoomedTime() + g_pGameCVars->g_intervention_timeBetweenZoomedAndKill > curTime)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool SkillKill::IsKickedCarKill(CPlayer* pKicker, CPlayer* pTargetPlayer, EntityId vehicleId)
{
	if(pKicker->IsClient() && g_pGameCVars->g_mpKickableCars)
	{
		if (IVehicle* pKickedVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(vehicleId))
		{
			if (CVehicleMovementBase* pMovement = StaticCast_CVehicleMovementBase(pKickedVehicle->GetMovement()))
			{
				if (const SVehicleMovementLargeObjectInfo* info = pMovement->GetLargeObjectInfo())
				{
					if (info->kicker && info->kicker == pKicker->GetEntityId())
					{
						return true;
					}
				}
			}
		}
		else
		{
			static const IEntityClass* pDestroyedVehicleClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DestroyedVehicle");
			CRY_ASSERT(pDestroyedVehicleClass);
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(vehicleId);
			return pEntity && pEntity->GetClass() == pDestroyedVehicleClass;
		}
	}

	return false;
}

bool SkillKill::IsSuperChargedKill( CPlayer* pShooter )
{
	return false;
}

bool SkillKill::IsIncomingKill( int hit_type )
{
	return hit_type == CGameRules::EHitType::Stamp;
}

bool SkillKill::IsPangKill( int hit_type )
{
	return hit_type == CGameRules::EHitType::EnvironmentalThrow || hit_type == CGameRules::EHitType::EnvironmentalMelee;
}

bool SkillKill::IsAntiAirSupportKill( int hit_type )
{
	return hit_type == CGameRules::EHitType::VTOLExplosion;
}
