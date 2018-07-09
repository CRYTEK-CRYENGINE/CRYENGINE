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

#include "StdAfx.h"
#include "GameRulesMPDamageHandling.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Actor.h"
#include "Player.h"
#include "PersistantDebug.h"
#include "IVehicleSystem.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesAssistScoringModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "BodyDamage.h"
#include "VehicleMovementBase.h"
#include "ActorManager.h"
#include "Melee.h"
#include "BodyManager.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "PersistantStats.h"
#include "Environment/InteractiveObject.h"


#define EntityCollisionLatentUpdateTimer 1.0f // Time (in seconds) to check through last entity collisions and remove them.
#define EntityCollisionIgnoreTimeBetweenCollisions 0.3f // Time (in seconds) to ignore subsequent collisions for between the same 2 entities.
#define DAMAGE_THRESHOLD_COLLISIONS 0.5f

//------------------------------------------------------------------------
CGameRulesMPDamageHandling::CGameRulesMPDamageHandling()
{
	CryLog("CGameRulesMPDamageHandling::CGameRulesMPDamageHandling()");
 
	m_numKickableCarRecords = 0;
	m_localMeleeScreenFxTimer = 0.f;
	m_entityLastDamageUpdateTimer = 0.f;
}

//------------------------------------------------------------------------
CGameRulesMPDamageHandling::~CGameRulesMPDamageHandling()
{
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::Init( XmlNodeRef xml )
{
	CGameRulesCommonDamageHandling::Init(xml);

	m_numKickableCarRecords = 0;

	m_vehicleDamageSettings.killSpeed = 10.f;

	if (XmlNodeRef table = xml->findChild("Table"))
	{
		if (const char* path = table->getAttr("path"))
		{
			if (XmlNodeRef damageTable = GetISystem()->LoadXmlFromFile(path))
			{
				if (XmlNodeRef vehicleDamage = damageTable->findChild("VehicleDamage"))
				{
					InitVehicleDamage(vehicleDamage);
				}
			}
		}
	}
}

void CGameRulesMPDamageHandling::InitVehicleDamage(XmlNodeRef vehicleDamage)
{
	vehicleDamage->getAttr("collisionKillSpeed", m_vehicleDamageSettings.killSpeed);
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	float currentTime = gEnv->pTimer->GetCurrTime();
	m_entityLastDamageUpdateTimer += frameTime;
	if (m_entityLastDamageUpdateTimer > EntityCollisionLatentUpdateTimer)
	{
		m_entityLastDamageUpdateTimer = 0.0f;

		if (!m_entityCollisionRecords.empty())
		{
			EntityCollisionRecords::iterator it = m_entityCollisionRecords.begin();
			while (it != m_entityCollisionRecords.end())
			{
				const EntityCollisionRecord& record = it->second;

				if ((record.time + EntityCollisionIgnoreTimeBetweenCollisions) < currentTime)
				{
					m_entityCollisionRecords.erase(it++);
				}
				else
				{
					++it;
				}
			}
		}
	}

	UpdateKickableCarRecords(frameTime, currentTime);

	if (gEnv->IsClient())
	{

#ifndef _RELEASE
		if (g_pGameCVars->pl_melee.mp_victim_screenfx_dbg_force_test_duration > 0.f)
		{
			m_localMeleeScreenFxTimer = (g_pGameCVars->pl_melee.mp_victim_screenfx_dbg_force_test_duration + g_pGameCVars->pl_melee.mp_victim_screenfx_blendout_duration);
			g_pGameCVars->pl_melee.mp_victim_screenfx_dbg_force_test_duration = 0.f;
		}
#endif

		if (m_localMeleeScreenFxTimer > 0.f)
		{
			UpdateLocalMeleeScreenFx(frameTime);
		}
	}
}

//------------------------------------------------------------------------
// returns true if entity is killed, false if it is not
bool CGameRulesMPDamageHandling::SvOnHit( const HitInfo &hitInfo )
{
	const HitTypeInfo * pHitTypeInfo = m_pGameRules->GetHitTypeInfo(hitInfo.type);

#if !defined(_RELEASE)
	if(!pHitTypeInfo)
		CryFatalError("By ::SvOnHit() all hit info should have a hit type that is valid and registered in the GameRules. This isn't true of type %d!", hitInfo.type);
#endif

	SDamageHandling damageHandling(&hitInfo, 1.0f);

	float damage = hitInfo.damage;

	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	CActor *pTargetActor = static_cast<CActor*>(pActorSystem->GetActor(hitInfo.targetId));
	CActor *pShooterActor = static_cast<CActor*>(pActorSystem->GetActor(hitInfo.shooterId));
	CPlayer* pShooterPlayer = (pShooterActor && pShooterActor->IsPlayer()) ? static_cast<CPlayer*>(pShooterActor) : NULL ;

	bool isPlayer = pTargetActor != NULL && pTargetActor->IsPlayer();

#ifndef _RELEASE
	//--- Fix to allow the damage handling to work for these entity classes in the same way as for Players
	static IEntityClass* sDamEntClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DamageTestEnt");
	isPlayer |= pTargetActor != NULL && pTargetActor->GetEntity()->GetClass() == sDamEntClass;
#endif

	CPlayer* pPlayer = isPlayer ? static_cast<CPlayer*>(pTargetActor) : NULL;
	const bool isMelee = ((pHitTypeInfo->m_flags & CGameRules::EHitTypeFlag::IsMeleeAttack) != 0);
	const bool checkHeadshots = ((pHitTypeInfo->m_flags & CGameRules::EHitTypeFlag::IgnoreHeadshots) == 0);

	bool bIsHeadShot = false;
	if(pPlayer && hitInfo.partId >= 0 && checkHeadshots)
	{
		bIsHeadShot = pPlayer->IsHeadShot(hitInfo);

		if (!bIsHeadShot && g_pGameCVars->g_mpHeadshotsOnly)
		{
			damage = 0.f;
		}
	}

	//If the player has died more than kTimeToAllowKillsAfterDeath seconds ago, we disallow any damage they apply, unless the hit type is flagged as allowing it.
	static const float kTimeToAllowKillsAfterDeath = 0.05f;
	if(pTargetActor && pShooterPlayer && !hitInfo.hitViaProxy
		&& ((pHitTypeInfo->m_flags & CGameRules::EHitTypeFlag::AllowPostDeathDamage) == 0) && pShooterActor->IsDead()
		&& (gEnv->pTimer->GetFrameStartTime().GetSeconds() - pShooterPlayer->GetDeathTime()) > kTimeToAllowKillsAfterDeath)
	{
		damage = 0.0f;
	}

	IGameRulesStateModule *stateModule = m_pGameRules->GetStateModule();
	IGameRulesRoundsModule* pRoundsModule = m_pGameRules->GetRoundsModule();

	if ( (stateModule != NULL && (stateModule->GetGameState() == IGameRulesStateModule::EGRS_PostGame)) || 
		   (pRoundsModule!= NULL && !pRoundsModule->IsInProgress() ))
	{
		// No damage allowed once the game has ended, except in cases where it would cause graphical glitches
		if (hitInfo.type != CGameRules::EHitType::PunishFall)
		{
			damage = 0.0f;
		}
	}

	IEntity *pTarget = gEnv->pEntitySystem->GetEntity(hitInfo.targetId);

	IEntityClass* pTargetClass = pTarget ? pTarget->GetClass() : NULL;

	// Check for friendly fire
	if( bool bCheckFriendlyFire = ((hitInfo.targetId!=hitInfo.shooterId) && (hitInfo.type!=CGameRules::EHitType::EventDamage)) )
	{
		if(IVehicle* pTargetVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(hitInfo.targetId))
		{
			if(IActor* pDriverTargetVehicle = pTargetVehicle->GetDriver())
			{
				// Vehicle driver shot own vehicle (same as shooting yourself), don't do friendly fire.
				bCheckFriendlyFire = pDriverTargetVehicle->GetEntityId()!=hitInfo.shooterId;
			}
		}
		if(bCheckFriendlyFire)
		{
			if (m_pGameRules->GetTeamCount() > 1)
			{
				int shooterTeamId = m_pGameRules->GetTeam(hitInfo.shooterId);
				int targetTeamId = m_pGameRules->GetTeam(hitInfo.targetId);

				if (shooterTeamId && (shooterTeamId == targetTeamId))
				{
					damage = GetFriendlyFireDamage(damage, hitInfo, pTargetActor);
				}
			}
		}
	}

	if (damage <= 0.f)
	{
		// If the hit isn't doing anything bail, this means any hit that gets past here has damage associated with it and thus wants to
		// display a hit indicator
		return false;
	}

	if (pPlayer)
	{
		if(hitInfo.partId >= 0 && !isMelee)
		{
			damageHandling.damageMultiplier *= pPlayer->GetBodyDamageMultiplier(hitInfo);
		}

		if (isMelee)
		{
			damageHandling.damageMultiplier *= g_pGameCVars->pl_melee.damage_multiplier_mp;
		}
	}

	damage *= damageHandling.damageMultiplier;

	HitInfo hitInfoWithDamage = hitInfo;
	hitInfoWithDamage.damage = damage;

	if(pPlayer)
	{
		SActorStats* pStats = pPlayer->GetActorStats();
		float actorHealth = pPlayer->GetHealth();

		if(pStats)
		{
#ifndef _RELEASE
			if (g_pGameCVars->g_LogDamage)
			{
				char weaponClassName[64], projectileClassName[64];

				CryLog ("[DAMAGE] %s '%s' took %.3f '%s' damage (%.3f x %.3f) weapon=%s projectile=%s",
						pPlayer->GetEntity()->GetClass()->GetName(), pPlayer->GetEntity()->GetName(),
						damage, m_pGameRules->GetHitType(hitInfo.type),
						hitInfo.damage, damageHandling.damageMultiplier,
						g_pGame->GetIGameFramework()->GetNetworkSafeClassName(weaponClassName, sizeof(weaponClassName), hitInfo.weaponClassId) ? weaponClassName : "none",
						g_pGame->GetIGameFramework()->GetNetworkSafeClassName(projectileClassName, sizeof(projectileClassName), hitInfo.projectileClassId) ? projectileClassName : "none");
			}
#endif

			if(pStats->bStealthKilling && actorHealth <= damage)
			{
				if(pPlayer->GetStealthKill().GetTargetId() != hitInfoWithDamage.shooterId)
				{
					pPlayer->StoreDelayedKillingHitInfo(hitInfoWithDamage);
				}
				
				hitInfoWithDamage.damage = 0;
			}
			else if (pStats->bStealthKilled && hitInfoWithDamage.type != CGameRules::EHitType::StealthKill)
			{
				hitInfoWithDamage.damage = 0;
			}
		}
	}
		
	bool bKilled = SvOnHitScaled(hitInfoWithDamage);

	return bKilled;
}

float CGameRulesMPDamageHandling::GetFriendlyFireDamage(float damage, const HitInfo &hitInfo, IActor* pActor)
{
	float friendlyFireRatio = m_pGameRules->GetFriendlyFireRatio();

	if(friendlyFireRatio >= 0.f)
	{
		damage *= friendlyFireRatio;
	}
	else
	{
		if(pActor && !pActor->IsDead())
		{
			friendlyFireRatio *= -1.f;

			HitInfo newHitInfo;
			newHitInfo.damage = damage * friendlyFireRatio;
			newHitInfo.targetId = hitInfo.shooterId;
			newHitInfo.shooterId = hitInfo.shooterId;
			newHitInfo.type = CGameRules::EHitType::Punish;
			newHitInfo.dir = ZERO;

			m_pGameRules->ClientHit(newHitInfo);
		}

		damage = 0.f;
	}

	return damage;
}

bool CGameRulesMPDamageHandling::SvOnHitScaled( const HitInfo &hitInfo )
{
	bool bReturnDied = false;
	IEntity *pTarget = gEnv->pEntitySystem->GetEntity(hitInfo.targetId);
	CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(hitInfo.targetId));

	if(!pActor)
	{
		IVehicle* pVictimVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(hitInfo.targetId);

		if(pVictimVehicle)
		{
			float vehicleDamageMultiplier = 1.f;

			//Additional damage scaling check for vehicles with associated body damage files
			CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();

			TBodyDamageProfileId bodyDamageProfileId;
			TVehicleBodyDamageMap::const_iterator iter = m_vehicleBodyDamageMap.find( hitInfo.targetId );
			if( iter != m_vehicleBodyDamageMap.end() )
			{
				bodyDamageProfileId = iter->second;
			}
			else
			{
				bodyDamageProfileId = pBodyDamageManager->GetBodyDamage( *pTarget );
				m_vehicleBodyDamageMap[hitInfo.targetId] = bodyDamageProfileId;
			}

			if(bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
			{
				vehicleDamageMultiplier *= pBodyDamageManager->GetDamageMultiplier(bodyDamageProfileId, *pTarget, hitInfo);
			}

			float fPreviousHealth = pVictimVehicle->GetStatus().health;

			// for vehicles, no need to call lua any more, we can just apply the hit directly to the vehicle
			{
				HitInfo hitInfoTemp = hitInfo;
				hitInfoTemp.damage *= vehicleDamageMultiplier;
				hitInfoTemp.explosion = false;
				pVictimVehicle->OnHit(hitInfoTemp);
			}


			float fNewHealth = pVictimVehicle->GetStatus().health;

			//Hit indicator for driver
			//	Health check is required to take into account damage removal due to friendly fire.
			if(fNewHealth != fPreviousHealth)
			{
				if(IActor* pDriver = pVictimVehicle->GetDriver())
				{
					if(IEntity* pShooterEntity = gEnv->pEntitySystem->GetEntity(hitInfo.shooterId))
					{
						Vec3 shooterPos = pShooterEntity->GetWorldPos();

						if(pDriver->IsClient())
						{
							SHUDEvent hitEvent(eHUDEvent_OnShowHitIndicatorPlayerUpdated);
							hitEvent.ReserveData(3);
							hitEvent.AddData(shooterPos.x);
							hitEvent.AddData(shooterPos.y);
							hitEvent.AddData(shooterPos.z);
							CHUDEventDispatcher::CallEvent(hitEvent);
						}
						else
						{
							m_pGameRules->GetGameObject()->InvokeRMI( CGameRules::ClActivateHitIndicator(), CGameRules::ActivateHitIndicatorParams(shooterPos), eRMI_ToClientChannel, pDriver->GetChannelId() );
						}
					}
				}
			}

			// no need for further processing, or calling OnEntityKilled, so just return early.
			if(pVictimVehicle->GetDamageRatio() >= 1.f)
			{
				if(fNewHealth != fPreviousHealth)
				{
					SVehicleDestroyedParams params(hitInfo.targetId, hitInfo.weaponId, hitInfo.type, hitInfo.projectileClassId);
					if(hitInfo.shooterId == gEnv->pGameFramework->GetClientActorId())
					{
						CPersistantStats::GetInstance()->OnClientDestroyedVehicle(params);
					}
					else if(IGameObject * pShooterGameObject = g_pGame->GetIGameFramework()->GetGameObject(hitInfo.shooterId))
					{
						m_pGameRules->GetGameObject()->InvokeRMIWithDependentObject(CGameRules::ClVehicleDestroyed(), params, eRMI_ToClientChannel, hitInfo.shooterId, pShooterGameObject->GetChannelId());
					}					
				}
				m_pGameRules->OnEntityKilled(hitInfo);
				return true; 
			}
			else
			{
				return false;
			}
		}
	}

	if (pTarget)
	{
		IScriptTable *pTargetScript = pTarget->GetScriptTable();
		if (pTargetScript)
		{
			m_pGameRules->CreateScriptHitInfo(m_scriptHitInfo, hitInfo);

			bool isDead = false;

			if (pActor)
			{
				if (pActor->IsDead())
				{
					isDead = true;

					// Target is already dead
					HSCRIPTFUNCTION pfnOnDeadHit = 0;
					if (pActor->GetActorStats()->isRagDoll && pTargetScript->GetValue("OnDeadHit", pfnOnDeadHit))
					{
						Script::CallMethod(pTargetScript, pfnOnDeadHit, m_scriptHitInfo);
						gEnv->pScriptSystem->ReleaseFunc(pfnOnDeadHit);
					}
				}
			}

			if (!isDead)
			{
				SmartScriptTable targetServerScript;
				if (pTargetScript->GetValue("Server", targetServerScript))
				{
					HSCRIPTFUNCTION pfnOnHit = 0;
					if (targetServerScript->GetValue("OnHit", pfnOnHit))
					{
						if (Script::CallReturn(gEnv->pScriptSystem, pfnOnHit, pTargetScript, m_scriptHitInfo, bReturnDied))
						{
							if (bReturnDied)
							{
								if (pActor)
								{
									ProcessDeath(hitInfo, *pActor);
								}
								else
								{
									m_pGameRules->OnEntityKilled(hitInfo);
								}
							}
						}
						gEnv->pScriptSystem->ReleaseFunc(pfnOnHit);
					}
				}
			}
		}
	}

	return bReturnDied;
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::ProcessDeath( const HitInfo &hitInfo, CActor& rTarget )
{
	if(rTarget.GetActorClass() == CPlayer::GetActorClassType())
	{
		m_pGameRules->KillPlayer(&rTarget, (hitInfo.type != CGameRules::EHitType::PunishFall), true, hitInfo);
	}
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::ProcessVehicleDeath( const HitInfo &hitInfo )
{
	CRY_TODO(08, 09, 2009, "Implement ProcessVehicleDeath");
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::SvOnExplosion(const ExplosionInfo &explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities)
{
	// SinglePlayer.lua 1721

	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();
	float totalDamage = 0.f;

	// calculate damage for each entity
	CGameRules::TExplosionAffectedEntities::const_iterator it = affectedEntities.begin();
	for (; it != affectedEntities.end(); ++it)
	{
		bool success = false;
		IEntity* entity = it->first;
		float obstruction = 1.0f - it->second;

		bool incone=true;
		if (explosionInfo.angle > 0 && explosionInfo.angle < 2*3.1415f)
		{
			Vec3 explosionEntityPos = entity->GetWorldPos();
			Vec3 entitypos = explosionEntityPos;
			float ha = explosionInfo.angle * 0.5f;
			Vec3 edir = entitypos - explosionInfo.pos;
			edir.normalize();
			float dot = 1;

			if (edir != Vec3(ZERO))
			{
				dot = edir.dot(explosionInfo.dir);
			}

			float angle = abs(acos_tpl(dot));
			if (angle > ha)
			{
				incone = false;
			}
		}

		if (incone && gEnv->bServer)
		{
			float damage = floor_tpl(0.5f + CalcExplosionDamage(entity, explosionInfo, obstruction));	

			bool dead = false;

			HitInfo explHit;

			Vec3 dir = entity->GetWorldPos() - explosionInfo.pos;

			explHit.pos = explosionInfo.pos;
			explHit.radius = explosionInfo.radius;
			explHit.partId = -1;

			dir.Normalize();

			explHit.targetId = entity->GetId();
			explHit.weaponId = explosionInfo.weaponId;
			explHit.shooterId = explosionInfo.shooterId;
			explHit.projectileId = explosionInfo.projectileId;
			explHit.projectileClassId = explosionInfo.projectileClassId;

			uint16 weaponClass = ~uint16(0);
			const IEntity* pWeaponEntity = gEnv->pEntitySystem->GetEntity(explosionInfo.weaponId);
			if (pWeaponEntity)
			{
				g_pGame->GetIGameFramework()->GetNetworkSafeClassId(weaponClass, pWeaponEntity->GetClass()->GetName());
			}
			explHit.weaponClassId = weaponClass;

			explHit.material = 0;
			explHit.damage = damage;
			explHit.type = explosionInfo.type;
			explHit.hitViaProxy = explosionInfo.explosionViaProxy;

			explHit.dir = dir;
			explHit.normal = -dir; //set normal to reverse of  direction to avoid backface cull of damage

			if (explHit.damage > 0.0f || explosionInfo.damage == 0.f)
			{
				CActor* pActor = static_cast<CActor*>(pActorSystem->GetActor(entity->GetId()));

				if (pActor)
				{
					const float damageMultiplier = pActor->GetBodyExplosionDamageMultiplier(explHit);
					explHit.damage *= damageMultiplier; 

					// Protect players who are currently shielded
					if(pActor->IsPlayer() && static_cast<CPlayer*>(pActor)->ShouldFilterOutExplosion(explHit))
					{
						explHit.damage = 0.0f;
					}
				}
				else
				{
					CInteractiveObjectEx* pInteractiveObject = static_cast<CInteractiveObjectEx*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(entity->GetId(), "InteractiveObjectEx"));
					if(pInteractiveObject)
					{
						pInteractiveObject->OnExploded(explHit.pos);
					}
				}
			
				if(!explosionInfo.explosionViaProxy) 
				{
					if(pActor)
					{
						if(!pActor->IsFriendlyEntity(explHit.shooterId))
						{
							totalDamage += damage;
						}
					}
					else
					{
						IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entity->GetId());

						if(pVehicle)
						{
							IActor* pDriver = pVehicle->GetDriver();
							int seatNum = 0;
							int numSeats = pVehicle->GetSeatCount();

							while(!pDriver && seatNum < numSeats)
							{
								IVehicleSeat* pSeat = pVehicle->GetSeatById(seatNum++);

								EntityId passengerId = pSeat ? pSeat->GetPassenger(true) : 0;
								pDriver = pActorSystem->GetActor(passengerId);
							}

							if(pDriver)
							{
								CActor* pDriverActor = static_cast<CActor*>(pDriver);
								if(!pDriverActor->IsFriendlyEntity(explHit.shooterId))
								{
									totalDamage += damage;
								}
							}
						}
					}
				}
				
				m_pGameRules->ServerHit(explHit);
			}
		}
	}

	if (totalDamage > 0.f)
	{
		IGameRulesPlayerStatsModule*  pPlayStatsMod = m_pGameRules->GetPlayerStatsModule();

		if(pPlayStatsMod)
		{
			pPlayStatsMod->ProcessSuccessfulExplosion(explosionInfo.shooterId, totalDamage, explosionInfo.projectileClassId);
		}
	}
}

//------------------------------------------------------------------------
float CGameRulesMPDamageHandling::CalcExplosionDamage(IEntity* entity, const ExplosionInfo &explosionInfo, float obstruction)
{
	// impact explosions directly damage the impact target
	if (explosionInfo.impact && explosionInfo.impact_targetId && explosionInfo.impact_targetId==entity->GetId())
	{
		return explosionInfo.damage;
	}

	float effect = 0.f;
	AABB entityAABB;

	entity->GetWorldBounds(entityAABB);

	float distanceSq = entityAABB.GetDistanceSqr(explosionInfo.pos);
	

	if (distanceSq <= explosionInfo.radius * explosionInfo.radius)
	{
		if (distanceSq <= explosionInfo.minRadius * explosionInfo.minRadius)
		{
			effect = 1.f;
		}
		else
		{
			effect = 1.f - (sqrtf(distanceSq) - explosionInfo.minRadius) / (explosionInfo.radius - explosionInfo.minRadius);
			effect *= effect;
		}
	}

	CRY_ASSERT_TRACE (effect >= 0.0f && effect <= 1.0f, ("Effectiveness of explosion should be between 0 and 1 but it's %.3f (distance = %.3f, minRadius=%.3f, maxRadius=%.3f)", effect, sqrtf(distanceSq), explosionInfo.minRadius, explosionInfo.radius));

	return explosionInfo.damage * effect * (1.0f - sqr(obstruction));
}

bool CGameRulesMPDamageHandling::IsDead(CActor* actor, IScriptTable* actorScript)
{
	if (actor)
	{
		return actor->IsDead();
	}
	else
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "SvOnCollision IsDead scope");
		HSCRIPTFUNCTION isDeadFunc = NULL;
		if (actorScript->GetValue("IsDead", isDeadFunc))
		{
			bool dead = false;
			Script::CallReturn(gEnv->pScriptSystem, isDeadFunc, actorScript, dead);
			gEnv->pScriptSystem->ReleaseFunc(isDeadFunc);
			return dead;
		}

		return false;
	}
}

void CGameRulesMPDamageHandling::UpdateKickableCarRecords(float frameTime, float currentTime)
{
	if (const int N = m_numKickableCarRecords)
	{
		int n = 0;
		SKickableCarRecord* lastRecord = m_kickableCarRecords;
		for (int i=0; i<N; i++)
		{
			SKickableCarRecord* record = m_kickableCarRecords + i;
			record->timer -= frameTime;
			if (record->timer<=0.f)
				continue; // Remove Record
			// Keep Record
			*lastRecord = *record;
			lastRecord++;
			n++;
		}
		m_numKickableCarRecords = n;
	}
}

void CGameRulesMPDamageHandling::InsertKickableCarRecord(EntityId vehicle, EntityId victim)
{
	if (m_numKickableCarRecords<maxKickableCarRecords)
	{
		SKickableCarRecord* record = m_kickableCarRecords + m_numKickableCarRecords;
		record->vehicleId = vehicle;
		record->victimId = victim;
		record->timer = 1.5f;
		m_numKickableCarRecords++;
	}
	else
	{
		GameWarning("MP Damage: Couldn't make a record of kickable car damage");
	}
}

CGameRulesMPDamageHandling::SKickableCarRecord* CGameRulesMPDamageHandling::FindKickableCarRecord(EntityId vehicleId, EntityId victimId)
{
	for (int i=0; i<m_numKickableCarRecords; i++)
	{
		if (m_kickableCarRecords[i].vehicleId == vehicleId && m_kickableCarRecords[i].victimId == victimId)
			return &m_kickableCarRecords[i];
	}
	return NULL;
}

float CGameRulesMPDamageHandling::ProcessActorKickedVehicle(IActor* victimActor, EntityId victimId, EntityId kickerId, EntityId vehicleId, float damage, const CGameRules::SCollisionHitInfo& collisionHitInfo)
{
	float angSpeedSq = 0.f;
	const IEntity* pVehicleEntity = gEnv->pEntitySystem->GetEntity(vehicleId);
	if (pVehicleEntity)
	{
		IPhysicalEntity* pent = pVehicleEntity->GetPhysics();
		if (pent)
		{
			pe_status_dynamics psd;
			if (pent->GetStatus(&psd))
			{
				angSpeedSq = psd.w.GetLengthSquared();
			}
		}
	}

	const Vec3& actorVelocity = collisionHitInfo.velocity;
	const Vec3& vehicleVelocity = collisionHitInfo.target_velocity;
	const float vehicleSpeedSq = vehicleVelocity.GetLengthSquared() + angSpeedSq;

	if (vehicleSpeedSq < 1.5f)
		return 0.f;

	float damageScale = 1.f;

	if (g_pGameCVars->g_mpKickableCars)
	{
		if (kickerId==victimId || FindKickableCarRecord(vehicleId, victimId))
			return 0.f;

		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules->GetTeamCount() > 1)
		{
			int kickerTeamId = pGameRules->GetTeam(kickerId);
			int victimTeamId = pGameRules->GetTeam(victimId);

			if (kickerTeamId==victimTeamId)	// Friendly fire
			{
				float friendlyFireRatio = pGameRules->GetFriendlyFireRatio();
				if (friendlyFireRatio>0.f)
				{
					damageScale = friendlyFireRatio;
				}
				else
				{
					return 0.f;
				}
			}
		}

		InsertKickableCarRecord(vehicleId, victimId);
	}

	// Damage, for now, is based purely on vehicle speed
	const float vehicleKillSpeed = m_vehicleDamageSettings.killSpeed;
	const float invVehicleKillSpeed = 1.f/(vehicleKillSpeed+0.01f);
	const float maxActorHealth = victimActor->GetMaxHealth();
	damage = 0.1f + sqrtf(vehicleSpeedSq+0.04f) * invVehicleKillSpeed;
	damage = min(damage, 1.f) * maxActorHealth * damageScale;

	return damage;
}
	
float CGameRulesMPDamageHandling::ProcessActorVehicleCollision(IActor* victimActor, EntityId victimId, IVehicle* pVehicle, EntityId vehicleId, float damage, const CGameRules::SCollisionHitInfo& collisionHitInfo, EntityId& kickerId)
{
	kickerId = 0;
	if (g_pGameCVars->g_mpKickableCars)
	{
		if (CVehicleMovementBase* pMovement = StaticCast_CVehicleMovementBase(pVehicle->GetMovement()))
		{
			if (const SVehicleMovementLargeObjectInfo* info = pMovement->GetLargeObjectInfo())
			{
				kickerId = info->kicker;
			}
		}
	}
	return ProcessActorKickedVehicle(victimActor, victimId, kickerId, vehicleId, damage, collisionHitInfo);
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::SvOnCollision(const IEntity *pVictimEntity, const CGameRules::SCollisionHitInfo& collisionHitInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	CRY_ASSERT(gEnv->bMultiplayer);

#if !defined(_RELEASE)
	if (g_pGameCVars->g_DisableCollisionDamage)
		return;
#endif

	IGameFramework* gameFramwork = g_pGame->GetIGameFramework();

	EntityId victimID = pVictimEntity->GetId();
	EntityId offenderID = collisionHitInfo.targetId;

	const IEntity* pOffenderEntity = gEnv->pEntitySystem->GetEntity(offenderID);

	float currentTime = gEnv->pTimer->GetCurrTime();

	CActor* victimActor = static_cast<CActor*>(gameFramwork->GetIActorSystem()->GetActor(victimID));
	IVehicle* offenderVehicle = gameFramwork->GetIVehicleSystem()->GetVehicle(offenderID);
	IVehicle* victimVehicle = gameFramwork->GetIVehicleSystem()->GetVehicle(victimID);
	IActor* offenderActor = gameFramwork->GetIActorSystem()->GetActor(offenderID);

	if(pOffenderEntity && !offenderVehicle && !offenderActor)
	{
		if( IEntity* pParent = pOffenderEntity->GetParent() )
		{
			 offenderVehicle = gameFramwork->GetIVehicleSystem()->GetVehicle(pParent->GetId());
		}
	}

	// Vehicles being flipped do no damage, for now
	if (offenderVehicle != NULL && offenderVehicle->GetStatus().beingFlipped)
		return;

	// Players can't damage vehicles
	if (victimVehicle && offenderActor)
		return;

	// Filter frequent collisions
	if (pOffenderEntity)
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "Filter out recent collisions");

		EntityCollisionRecords::const_iterator collisionRecordIter = m_entityCollisionRecords.find(victimID);
		if (collisionRecordIter != m_entityCollisionRecords.end())
		{
			const EntityCollisionRecord& record = collisionRecordIter->second;
			if (record.entityID == offenderID &&
			    record.time + EntityCollisionIgnoreTimeBetweenCollisions > currentTime)
			{
				return;
			}
		}
	}

	float offenderMass = collisionHitInfo.target_mass;

	enum
	{
		CollisionWithEntity,
		CollisionWithStaticWorld
	}
	collisionType = (pOffenderEntity || offenderMass > 0.0f) ? CollisionWithEntity : CollisionWithStaticWorld;

	const Vec3& victimVelocity = collisionHitInfo.velocity;
	const Vec3& offenderVelocity = collisionHitInfo.target_velocity;

	float relativeSpeedSq = 0.0f;
	float minSpeedToCareAboutCollisionSq = 0.0f;
	float contactMass = 0.0f;

	bool offenderIsBig = offenderMass > 1000.f;

	switch (collisionType)
	{
	case CollisionWithEntity:
		{
			Vec3 relativeVelocity = victimVelocity - offenderVelocity;
			relativeSpeedSq = relativeVelocity.GetLengthSquared();

			minSpeedToCareAboutCollisionSq = sqr(10.0f);

			if (victimActor && offenderIsBig)
			{
				minSpeedToCareAboutCollisionSq = sqr(1.0f);
			}

			if (victimActor && offenderVehicle)
			{
				//Players won't be hurt by vehicles with a negative kill player speed
				if(offenderVehicle->GetDamageParams().aiKillPlayerSpeed < 0.f)
				{
					return;
				}

				minSpeedToCareAboutCollisionSq = sqr(2.0f);
			}

			const float offenderSpeedSq = offenderVelocity.GetLengthSquared();
			if (offenderSpeedSq == 0.0f) // -- if collision target it not moving
			{
				minSpeedToCareAboutCollisionSq *= sqr(2.0f);
			}

			//////////////////////////////////////////////////////////////////////////

			contactMass = offenderMass;
			break;
		}

	case CollisionWithStaticWorld:
		{
			// Actors don't take damage from running into walls!
			if (victimActor)
			{
				return;
			}

			relativeSpeedSq = victimVelocity.GetLengthSquared();
			minSpeedToCareAboutCollisionSq = sqr(7.5f);
			contactMass = collisionHitInfo.mass;

			break;
		}
	}

	const bool contactMassIsTooLowToCare = contactMass < 0.01f;
	if (contactMassIsTooLowToCare)
		return;

			
	//////////////////////////////////////////////////////////////////////////
	// Calculate the collision damage
	if (relativeSpeedSq >= minSpeedToCareAboutCollisionSq)
	{
		bool useDefaultCalculation = true;
		float fEnergy = 0.f;
		float damage = 0.f;
		EntityId kickerId = 0;

		// Calculate damage
		if (offenderVehicle && victimActor)
		{
			useDefaultCalculation = false;
			damage = ProcessActorVehicleCollision(victimActor, victimID, offenderVehicle, offenderID, damage, collisionHitInfo, kickerId);
		}
		else if (offenderIsBig && victimActor) // i.e. a kickable car
		{
			// Try to find the kicker
			CTimeValue time = gEnv->pTimer->GetAsyncTime();
			IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();
			IActorIteratorPtr pActorIterator = pActorSystem->CreateActorIterator();
			IActor* pActor = pActorIterator->Next();
			float lowestTime = 5.f;
			while (pActor != NULL)
			{
				CPlayer* pPlayer = static_cast<CPlayer*>(pActor);
				EntityId kicked = pPlayer->GetLargeObjectInteraction().GetLastObjectId();
				if (kicked==offenderID)
				{
					float timeSinceKick = (time - pPlayer->GetLargeObjectInteraction().GetLastObjectTime()).GetSeconds();
					if (timeSinceKick < lowestTime)
					{
						// We found the kicker and the kicked
						kickerId = pActor->GetEntityId();
						lowestTime = timeSinceKick;
					}
				}
				pActor = pActorIterator->Next();
			}
			damage = ProcessActorKickedVehicle(victimActor, victimID, kickerId, offenderID, damage, collisionHitInfo);
			useDefaultCalculation = false;
		}

		if (useDefaultCalculation)
		{
			fEnergy = GetCollisionEnergy(pVictimEntity, collisionHitInfo);
			if (victimVehicle || offenderIsBig)
			{
				damage = 0.0005f * fEnergy;
			}
			else
			{
				damage = 0.0025f * fEnergy;
			}

			// Apply damage multipliers
			damage *= GetCollisionDamageMult(pVictimEntity, pOffenderEntity, collisionHitInfo);

			if (victimActor)
			{
				const bool victimIsPlayer = victimActor->IsPlayer();

				if (victimIsPlayer)
				{
					damage = AdjustPlayerCollisionDamage(pVictimEntity, pOffenderEntity, collisionHitInfo, damage);
				}
			}
		}

		if (damage >= DAMAGE_THRESHOLD_COLLISIONS)
		{
			HitInfo hit;
			hit.damage = damage;
			hit.pos = collisionHitInfo.pos;
			if (collisionHitInfo.target_velocity.GetLengthSquared() > 1e-6)
				hit.dir = collisionHitInfo.target_velocity.GetNormalized();
			hit.radius = 0.0f;
			hit.partId = collisionHitInfo.partId;
			hit.targetId = victimID;
			hit.weaponId = offenderID;
			hit.shooterId = kickerId != 0 ? kickerId : offenderID;
			hit.material = 0;
			hit.type = CGameRules::EHitType::Collision;
			hit.explosion = false;

			CGameRules *pGameRules = g_pGame->GetGameRules();
			if (pGameRules->GetTeamCount() > 1)
			{
				int shooterTeamId = pGameRules->GetTeam(hit.shooterId);
				int targetTeamId = pGameRules->GetTeam(hit.targetId);

				if (shooterTeamId && (shooterTeamId == targetTeamId))
				{
					damage = GetFriendlyFireDamage(damage, hit, victimActor);
				}
			}

			if (damage >= DAMAGE_THRESHOLD_COLLISIONS)
			{
				IScriptTable* pVictimScript = pVictimEntity ? pVictimEntity->GetScriptTable() : NULL;
				IScriptTable* pOffenderScript = pOffenderEntity ? pOffenderEntity->GetScriptTable() : NULL;

				if (!pOffenderEntity && pVictimEntity)
				{
					pOffenderEntity = pVictimEntity;
					offenderID = victimID;
				}

				m_entityCollisionRecords[victimID] = EntityCollisionRecord(offenderID, currentTime);

				if(victimVehicle)
				{
					victimVehicle->OnHit(hit);
				}	
				else if (pVictimScript)
				{
					CRY_PROFILE_REGION(PROFILE_GAME, "Call to OnHit");

					if (!IsDead(victimActor, pVictimScript))
					{
						if (IActor* offenderDriver = offenderVehicle ? offenderVehicle->GetDriver() : NULL)
							hit.shooterId = offenderDriver->GetEntityId();

						DelegateServerHit(pVictimScript, hit, victimActor);
					}
				}
			}
		}
	}
}
//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::DelegateServerHit(IScriptTable* victimScript, const HitInfo& hit, CActor* pVictimActor)
{
	SmartScriptTable victimServerScript;
	if (victimScript->GetValue("Server", victimServerScript))
	{
		HSCRIPTFUNCTION pfnOnHit = 0;
		if (victimServerScript->GetValue("OnHit", pfnOnHit))
		{
			bool diedAfterHit = false;
			m_pGameRules->CreateScriptHitInfo(m_scriptHitInfo, hit);
			if (Script::CallReturn(gEnv->pScriptSystem, pfnOnHit, victimScript, m_scriptHitInfo, diedAfterHit))
			{
				if (diedAfterHit)
				{
					// Hit was deadly
					if (pVictimActor)
					{
						ProcessDeath(hit, *pVictimActor);
					}
					else
					{
						m_pGameRules->OnEntityKilled(hit);
					}
				}

				if (g_pGameCVars->g_debugHits > 0)
				{
					LogHit(hit, (g_pGameCVars->g_debugHits > 1), diedAfterHit);
				}
			}

			gEnv->pScriptSystem->ReleaseFunc(pfnOnHit);
		}
	}
}

//------------------------------------------------------------------------
float CGameRulesMPDamageHandling::GetCollisionMinVelocity(const IEntity *pEntity, const CGameRules::SCollisionHitInfo& colHitInfo)
{
	float minVel = 10.f;

	CActor	*pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
	if ((pActor != NULL && !pActor->IsPlayer())/* || advancedDoor*/)
	{
		minVel = 1.f; // --Door or character hit	
	}

	IVehicle *vehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(colHitInfo.targetId);
	if(pActor && vehicle)
	{
		minVel = 6.f; // -- otherwise we don't get damage at slower speeds
	}

	if (colHitInfo.target_velocity.len2() == 0.f) // -- if collision target it not moving
	{
		minVel = minVel * 2.f;
	}

	return minVel;
}

//------------------------------------------------------------------------
float CGameRulesMPDamageHandling::AdjustPlayerCollisionDamage(const IEntity *pEntity, const IEntity *pCollider, const CGameRules::SCollisionHitInfo& colHitInfo, float colInfo_damage)
{
	float result = colInfo_damage;

	if (colHitInfo.target_velocity.len() == 0.f)
	{
		result = colInfo_damage * 0.2f;
	}

	CActor	*pEntityActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
	if (pEntityActor)
	{
		float healthThreshold = (float)g_pGameCVars->pl_health.collision_health_threshold;
		float currentHealth = pEntityActor->GetHealth();

		if (currentHealth <= healthThreshold)
		{
			result = 0.f;
		}
		else if ((currentHealth - result) < healthThreshold)
		{
			result = currentHealth - healthThreshold;
		}
	}

	return result;
}

//------------------------------------------------------------------------
//float CGameRulesMPDamageHandling::ProcessPlayerToActorCollision(const IEntity *pEntity, const IEntity *pCollider, const CGameRules::SCollisionHitInfo& colHitInfo, float colInfo_damage)
//{
//	float ragdoll_to_player = 0.f; // -- Max damage from ragdoll collision
//
//	CActor *pColliderActor = pCollider ? static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pCollider->GetId())) : NULL;
//	if (pColliderActor)
//	{
//		uint8 profile=pColliderActor->GetGameObject()->GetAspectProfile(eEA_Physics);
//		if ((profile == eAP_Sleep) || (profile == eAP_Ragdoll))
//		{
//			return MIN(colInfo_damage, ragdoll_to_player);
//		}
//	}
//
//	return colInfo_damage;
//}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::ClProcessHit(Vec3 dir, EntityId shooterId, EntityId weaponId, float damage, uint16 projectileClassId, uint8 hitTypeId)
{
	CryLog("CGameRulesMPDamageHandling::ClProcessHit() type='%s' from weapon %d, shooter %d", m_pGameRules->GetHitTypeInfo(hitTypeId)->m_name.c_str(), weaponId, shooterId);
	CRY_ASSERT(gEnv->IsClient());

	if (hitTypeId == CGameRules::EHitType::Melee)
	{
		StartLocalMeleeScreenFx();
	}

	CGameRulesCommonDamageHandling::ClProcessHit(dir, shooterId, weaponId, damage, projectileClassId, hitTypeId);
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::StartLocalMeleeScreenFx()
{
		m_localMeleeScreenFxTimer = (g_pGameCVars->pl_melee.mp_victim_screenfx_duration + g_pGameCVars->pl_melee.mp_victim_screenfx_blendout_duration);
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::StopLocalMeleeScreenFx()
{
	CRY_ASSERT(gEnv->IsClient());

	gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", 0.f);
	m_localMeleeScreenFxTimer = 0.f;
}

//------------------------------------------------------------------------
void CGameRulesMPDamageHandling::UpdateLocalMeleeScreenFx(const float frameTime)
{
	CRY_ASSERT(gEnv->IsClient());
	CRY_ASSERT(m_localMeleeScreenFxTimer > 0.f);

	m_localMeleeScreenFxTimer -= frameTime;

	if (m_localMeleeScreenFxTimer > 0.f)
	{
		const float  blendOutDuration = g_pGameCVars->pl_melee.mp_victim_screenfx_blendout_duration;
		float  amount = g_pGameCVars->pl_melee.mp_victim_screenfx_intensity;

		if (m_localMeleeScreenFxTimer < blendOutDuration)
		{
			amount *= (1.f - ((blendOutDuration - m_localMeleeScreenFxTimer) / blendOutDuration));
		}

		gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Type", 0, true);  // 0 is Gaussian
		gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", amount, true);
	}
	else
	{
		StopLocalMeleeScreenFx();
	}
}

float CGameRulesMPDamageHandling::CalculateFriendlyFireRatio(EntityId entityId1, EntityId entityId2)
{
	CGameRules *pGameRules = m_pGameRules;
	if (entityId1 != entityId2 && pGameRules->GetTeamCount() > 1)
	{
		int team1 = pGameRules->GetTeam(entityId1);
		if( team1 ==  pGameRules->GetTeam(entityId2) )
		{
			return pGameRules->GetFriendlyFireRatio();
		}
	}

	//Not on same team so full damage
	return 1.f;
}
