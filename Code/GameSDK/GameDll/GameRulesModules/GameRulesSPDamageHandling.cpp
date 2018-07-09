// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameRulesSPDamageHandling.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "Actor.h"
#include "Player.h"
#include "PersistantDebug.h"
#include "IVehicleSystem.h"
#include "GameRulesModules/IGameRulesAssistScoringModule.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "BodyDamage.h"
#include "HitDeathReactions.h"
#include <limits>

#include "AI/GameAISystem.h"
#include "AI/VisibleObjectsHelper.h"

#include <CryAISystem/VisionMapTypes.h>
#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/IAIActor.h>
#include "WeaponSystem.h"
#include "PersistantStats.h"

#include "BodyManager.h"

#define EntityCollisionLatentUpdateTimer 1.0f // Time (in seconds) to check through last entity collisions and remove them.
#define EntityCollisionIgnoreTimeBetweenCollisions 0.3f // Time (in seconds) to ignore subsequent collisions for between the same 2 entities.



// SvOnCollision and SvOnHitScaled could be refactored to share a lot of code

namespace
{
	const float DAMAGE_TRESHOLD_COLLISIONS = 0.5f;
	const float DAMAGE_TRESHOLD_COLLISION_REACTIONS = 15.0f;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CMercyTimeFilter::CMercyTimeFilter()
	: m_lastMercyTimeFilterCount(0)
{
	while(m_difficultyTolerance.size() < m_difficultyTolerance.max_size())
	{
		m_difficultyTolerance.push_back(1);
	}
}

void CMercyTimeFilter::Init( const XmlNodeRef& xml )
{
	CRY_ASSERT(xml != NULL);

	const XmlNodeRef difficultyFilterNode = xml->findChild("DifficultyFilters");
	if(difficultyFilterNode != NULL)
	{
		const int childCount = difficultyFilterNode->getChildCount();
		for(int i = 0; i < childCount; ++i)
		{
			const XmlNodeRef& childNode = difficultyFilterNode->getChild(i);

			int difficultyLevel = 0;
			if(childNode->getAttr("level", difficultyLevel) && 
				((difficultyLevel > 0) && (difficultyLevel <= MaxDifficultyLevels)))
			{
				uint32 allowedHits = 0;
				childNode->getAttr("killAfterHits", allowedHits);
				m_difficultyTolerance[difficultyLevel-1] = allowedHits;
			}
		}
	}

	const XmlNodeRef projectileFilterNode = xml->findChild("ProjectileFilters");
	if(projectileFilterNode != NULL)
	{
		const int childCount = projectileFilterNode->getChildCount();

		m_filteredProjectiles.reserve( childCount );

		for (int i = 0; i < childCount; ++i)
		{
			const XmlNodeRef& childNode = projectileFilterNode->getChild( i );

			ClassFilter classFilter;
			classFilter.classId = ~uint16(0); 
			classFilter.type = ClassFilter::eType_None;

			if(g_pGame->GetIGameFramework()->GetNetworkSafeClassId( classFilter.classId, childNode->getAttr("class") ))
			{
				const char* filterType = childNode->haveAttr("filter") ? childNode->getAttr("filter") : "none";

				if(strcmp(filterType, "self") == 0)
				{
					classFilter.type = ClassFilter::eType_Self;
				}

				m_filteredProjectiles.push_back( classFilter );
			}
		}

		std::sort( m_filteredProjectiles.begin(), m_filteredProjectiles.end(), CompareClassFilter() );
	}
}

bool CMercyTimeFilter::Filtered( const uint16 projectileClassId, const EntityId ownerId, const EntityId targetId ) const
{
	FilteredProjectiles::const_iterator it = std::find(m_filteredProjectiles.begin(), m_filteredProjectiles.end(), projectileClassId);
	if (it == m_filteredProjectiles.end())
	{
		return false;
	}

	const uint32 currentDifficulty = (uint32)CLAMP(g_pGameCVars->g_difficultyLevel, 1, MaxDifficultyLevels);
	const uint32 difficultyKillerHitsTolerance = m_difficultyTolerance[currentDifficulty-1];

	const bool filterMercyTime = (it->type == ClassFilter::eType_Self) ? (ownerId != targetId) : true;
	if(difficultyKillerHitsTolerance <= 1)
	{
		return filterMercyTime;
	}
	else
	{
		m_lastMercyTimeFilterCount += filterMercyTime ? 1 : 0;
		return (filterMercyTime && (m_lastMercyTimeFilterCount >= difficultyKillerHitsTolerance));
	}
}

void CMercyTimeFilter::OnLocalPlayerEnteredMercyTime()
{
	m_lastMercyTimeFilterCount = 0;
}

void CInvulnerableFilter::Init()
{
	const char* levelName = g_pGame->GetIGameFramework()->GetLevelName();
	
	const bool isTutorialLevel = ((levelName != NULL) && (stricmp(levelName, "Tutorial") == 0));
	
	m_enableState.SetFlags( eReason_InTutorial, isTutorialLevel );

	m_minHealthThreshold = g_pGameCVars->g_playerLowHealthThreshold;
}

bool CInvulnerableFilter::FilterIncomingDamage( const HitInfo& hitInfo, const float currentTargetHealth ) const
{
	IF_UNLIKELY(IsEnabled())
	{
		if (currentTargetHealth <= m_minHealthThreshold)
		{
			//If current health is above the min threshold, this would be filtered later on
			return true;
		}
		else if (currentTargetHealth < hitInfo.damage)
		{
			//If not, check for 'punish' hit types and filter early on
			return ((hitInfo.type == CGameRules::EHitType::Punish) || (hitInfo.type == CGameRules::EHitType::PunishFall) || (hitInfo.type == CGameRules::EHitType::VehicleDestruction));
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CGameRulesSPDamageHandling::CGameRulesSPDamageHandling()
: m_entityLastDamageUpdateTimer(0.0f)
{
	m_entityClassesWithTrackedMovement.reserve(4);
}


CGameRulesSPDamageHandling::~CGameRulesSPDamageHandling()
{

}


void CGameRulesSPDamageHandling::Init( XmlNodeRef xml )
{
	CGameRulesCommonDamageHandling::Init(xml);

	const XmlNodeRef& mercyTimeNode = xml->findChild("MercyTimeFilters");
	if (mercyTimeNode != NULL)
	{
		const XmlNodeRef& rootNode = gEnv->pSystem->LoadXmlFromFile( mercyTimeNode->getAttr("path") );
		if (rootNode != NULL)
		{
			m_mercyTimeFilter.Init( rootNode );
		}
	}

	m_invulnerableFilter.Init();
}

#ifndef _RELEASE
int g_collCount = 0;
int CollisionFilter(const EventPhysCollision *pEvent)
{
	if (max(pEvent->pEntity[0]->GetType(), pEvent->pEntity[1]->GetType()) > PE_RIGID)
		return 1;
	return ++g_collCount <= g_pGameCVars->g_MaxSimpleCollisions;
}
#endif

void CGameRulesSPDamageHandling::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	m_entityLastDamageUpdateTimer += frameTime;
	if (m_entityLastDamageUpdateTimer > EntityCollisionLatentUpdateTimer)
	{
		m_entityLastDamageUpdateTimer = 0.0f;

		if (!m_entityCollisionRecords.empty())
		{
			float currentTime = gEnv->pTimer->GetCurrTime();

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

#ifndef _RELEASE
	g_collCount = 0;
	if (g_pGameCVars->g_DisableCollisionDamage)
		gEnv->pPhysicalWorld->AddEventClient(EventPhysCollision::id, (int(*)(const EventPhys*))CollisionFilter, 1, 10.0f);
	else
		gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, (int(*)(const EventPhys*))CollisionFilter, 1);

	if (g_pGameCVars->g_debugFakeHits)
		DrawFakeHits(frameTime);
#endif
}

#ifndef _RELEASE
void CGameRulesSPDamageHandling::DrawFakeHits(float frameTime)
{
	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	float highestAlpha = 0.0f;

	for (size_t i = 0; i < m_fakeHits.size(); )
	{
		FakeHitDebug& hit = m_fakeHits[i];

		float dt = (now - hit.time).GetSeconds();

		if (dt > 0.75f)
		{
			std::swap(hit, m_fakeHits.back());
			m_fakeHits.pop_back();
		}
		else
			++i;

		float alpha =  1.0f - std::min(dt / 0.75f, 1.0f);
		if (alpha > highestAlpha)
			highestAlpha = alpha;
	}

	if (highestAlpha > 0.001f)
	{
		float color[4] = { 0.85f, 0.0f, 0.0f, std::min(highestAlpha, 1.0f) };
		IRenderAuxText::Draw2dLabel(1024.0f * 0.5f, 500.0f, 2.45f, color, true, "FAKE HIT!");
	}
}
#endif

// Returns true if entity is killed, false if it is not
bool CGameRulesSPDamageHandling::SvOnHit(const HitInfo& hit)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const EntityId victimID = hit.targetId;

	assert(victimID != 0);
	if (victimID != 0)
	{
		CActor* pVictimActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimID));
		
		const bool victimIsActor = pVictimActor != NULL;
		const bool victimIsLocalClientPlayer = pVictimActor != NULL && pVictimActor->IsClient();

		if (victimIsLocalClientPlayer)
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(pVictimActor);
			SDamageHandling damageHandling(&hit, 1.0f);
			
			HitInfo hitInfo(hit);
			if (ShouldApplyMercyTime(*pVictimActor, hitInfo))
			{
				hitInfo.damage = 0;
			}
			hitInfo.damage *= damageHandling.damageMultiplier;
			return SvOnHitScaled(hitInfo);
		}
		else
		{
			HitInfo hitInfoWithModifiedDamage = hit;
			if (victimIsActor)
			{
				hitInfoWithModifiedDamage.damage *= pVictimActor->GetBodyDamageMultiplier(hit);
			}
			else
			{
				CBodyDamageManager* pBodyDamageManager = g_pGame->GetBodyDamageManager();
				CRY_ASSERT(pBodyDamageManager);

				const TBodyDamageProfileId bodyDamageProfileId = pBodyDamageManager->FindBodyDamageProfileIdBinding(victimID);
				if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
				{
					IEntity* pEntity = gEnv->pEntitySystem->GetEntity(victimID);
					const float multiplier = pBodyDamageManager->GetDamageMultiplier(bodyDamageProfileId, *pEntity, hit);
					hitInfoWithModifiedDamage.damage *= multiplier;
				}
			}

			return SvOnHitScaled(hitInfoWithModifiedDamage);
		}

	}
	else
	{
		return false;
	}
}


bool CGameRulesSPDamageHandling::SvOnHitScaled(const HitInfo& hitInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	bool victimDiedAfterHit = false;

	const EntityId victimID = hitInfo.targetId;

	// TODO: These pointers could be passed in if the method signature in the interface was changed.
	IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntity(victimID);
	CActor* pVictimActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimID));
	IVehicle* pVictimVehicle = NULL;
	if(!pVictimActor)
	{
		pVictimVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(victimID);

		if(pVictimVehicle)
		{
			// for vehicles, no need to call lua any more, we can just apply the hit directly to the vehicle
			{
				HitInfo hitTemp = hitInfo;
				hitTemp.explosion = false;
				pVictimVehicle->OnHit(hitTemp);
			}

			if (g_pGameCVars->g_debugHits != 0)
			{
				LogHit(hitInfo, (g_pGameCVars->g_debugHits > 1), pVictimVehicle->IsDestroyed());
			}

			// no need for further processing, or calling OnEntityKilled, so just return early.
			return pVictimVehicle->IsDestroyed();
		}
	}


	if (pVictimEntity)
	{
		float victimActorInitialHealth = 0.0f;
		if (IScriptTable* victimScript = pVictimEntity->GetScriptTable())
		{
			bool victimDeadBeforeHit = false;

			HSCRIPTFUNCTION onDeadHitFunc = NULL;

			if (pVictimActor)
			{
				victimActorInitialHealth = pVictimActor->GetHealth();

				victimDeadBeforeHit = pVictimActor->IsDead();
				if (victimDeadBeforeHit && pVictimActor->GetActorStats()->isRagDoll)
				{
					victimScript->GetValue("OnDeadHit", onDeadHitFunc);
				}
			}
			else
			{
				// The victim is not an actor or vehicle, so we can't just check the health.
				// Instead we call the customizable Lua function IsDead, if it exists.
				//
				// isDead = result from Lua function IsDead in victim entity script

				CRY_PROFILE_REGION(PROFILE_GAME, "SvOnHitScaled IsDead scope");

				HSCRIPTFUNCTION isDeadFunc = NULL;
				if (victimScript->GetValue("IsDead", isDeadFunc))
				{
					Script::CallReturn(gEnv->pScriptSystem, isDeadFunc, victimScript, victimDeadBeforeHit);
					gEnv->pScriptSystem->ReleaseFunc(isDeadFunc);
				}
			}

			HSCRIPTFUNCTION onHitFunc = NULL;
			SmartScriptTable victimServerScript;

			if (!victimDeadBeforeHit)
			{
				if (victimScript->GetValue("Server", victimServerScript))
				{
					victimServerScript->GetValue("OnHit", onHitFunc);
				}
			}

			if (onHitFunc || onDeadHitFunc)
			{
				HitInfo modifiedHitInfo(hitInfo);

				bool isInvulnerable = false;
				HSCRIPTFUNCTION isInvulnerableFunc = NULL;
				if (victimScript->GetValue("IsInvulnerable", isInvulnerableFunc) && isInvulnerableFunc)
				{
					Script::CallReturn(gEnv->pScriptSystem, isInvulnerableFunc, victimScript, isInvulnerable);
					gEnv->pScriptSystem->ReleaseFunc(isInvulnerableFunc);
				}

				bool canKill = false;

				switch (modifiedHitInfo.type)
				{
				case CGameRules::EHitType::Collision:
				case CGameRules::EHitType::StealthKill:
				case CGameRules::EHitType::StealthKill_Maximum:
				case CGameRules::EHitType::Punish:
				case CGameRules::EHitType::PunishFall:
				case CGameRules::EHitType::VehicleDestruction:
				case CGameRules::EHitType::Fall:
				case CGameRules::EHitType::Melee:
				case CGameRules::EHitType::SilentMelee:
					canKill = true;
					break;
				default:
					break;
				}

				if (isInvulnerable)
					modifiedHitInfo.damage = 0.0;
				else if (!canKill)
				{
					IEntity* shooter = gEnv->pEntitySystem->GetEntity(modifiedHitInfo.shooterId);
					IAIObject* shooterAI = shooter ? shooter->GetAI() : 0;
					IAIActor* aiActor = shooterAI ? shooterAI->CastToIAIActor() : 0;

					if (aiActor && shooter != pVictimEntity && !aiActor->CanDamageTarget(pVictimEntity->GetAI()))
					{
#ifndef _RELEASE
						if (pVictimEntity->GetId() == gEnv->pGameFramework->GetClientActorId())
							m_fakeHits.push_back(FakeHitDebug(gEnv->pTimer->GetFrameStartTime()));
#endif
						modifiedHitInfo.damage = 1.0f;
					}
				}

				m_pGameRules->CreateScriptHitInfo(m_scriptHitInfo, modifiedHitInfo);

				if (onDeadHitFunc)
				{
					Script::CallMethod(victimScript, onDeadHitFunc, m_scriptHitInfo);
					gEnv->pScriptSystem->ReleaseFunc(onDeadHitFunc);
					onDeadHitFunc = NULL;
				}

				if (onHitFunc)
				{
					if(Script::CallReturn(gEnv->pScriptSystem, onHitFunc, victimScript, m_scriptHitInfo, victimDiedAfterHit))
					{
						if (pVictimActor)
						{
							const float victimActorAfterHitHealth = pVictimActor->GetHealth();
							pVictimActor->ProcessDestructiblesHit(modifiedHitInfo, victimActorInitialHealth, victimActorAfterHitHealth);
						}

						if (victimDiedAfterHit)
						{
							if (pVictimActor)
							{
								ProcessDeath(pVictimActor, victimScript, modifiedHitInfo);
							}
							else
								m_pGameRules->OnEntityKilled(modifiedHitInfo);
						}
					}

					gEnv->pScriptSystem->ReleaseFunc(onHitFunc);
					onHitFunc = NULL;
				}
			}
		}
	}

	if (g_pGameCVars->g_debugHits != 0)
	{
		LogHit(hitInfo, (g_pGameCVars->g_debugHits > 1), victimDiedAfterHit);
	}

	return victimDiedAfterHit;
}



void CGameRulesSPDamageHandling::ProcessDeath(IActor* victimActor, const SmartScriptTable& victimScript, const HitInfo& hitInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const bool ragdoll = !victimActor || !victimActor->GetLinkedVehicle();

	HSCRIPTFUNCTION killFunc = NULL;
	if (victimScript->GetValue("Kill", killFunc))
	{
		Script::CallMethod(victimScript, killFunc, m_scriptHitInfo);
		gEnv->pScriptSystem->ReleaseFunc(killFunc);
		killFunc = NULL;
	}

	const bool dropItem = (hitInfo.type != CGameRules::EHitType::PunishFall);
	if(victimActor)
		m_pGameRules->KillPlayer(victimActor, dropItem, ragdoll, hitInfo);
}



void CGameRulesSPDamageHandling::SvOnExplosion(const ExplosionInfo& explosionInfo, const CGameRules::TExplosionAffectedEntities& affectedEntities)
{
	// SinglePlayer.lua 1721

	// Calculate damage for each entity
	CGameRules::TExplosionAffectedEntities::const_iterator it = affectedEntities.begin();
	bool bHitActor = false;
	for ( ; it != affectedEntities.end(); ++it)
	{
		bool success = false;
		IEntity* entity = it->first;
		float obstruction = 1.0f - it->second;

		bool incone = true;
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

		if (incone)
		{
			float damage = floor_tpl(0.5f + CalcExplosionDamage(entity, explosionInfo, obstruction));

			HitInfo explHit;

			Vec3 dir = entity->GetWorldPos() - explosionInfo.pos;

			explHit.pos = explosionInfo.pos;
			explHit.radius = explosionInfo.radius;
			explHit.partId = -1;

			dir.Normalize();

			uint16 weaponClassId = ~uint16(0);
			const IEntity* pWeaponEntity = gEnv->pEntitySystem->GetEntity(explosionInfo.weaponId);
			if (pWeaponEntity)
			{
				g_pGame->GetIGameFramework()->GetNetworkSafeClassId(weaponClassId, pWeaponEntity->GetClass()->GetName());
			}

			explHit.targetId = entity->GetId();
			explHit.weaponId = explosionInfo.weaponId;
			explHit.weaponClassId = weaponClassId;
			explHit.shooterId = explosionInfo.shooterId;
			explHit.projectileId = explosionInfo.projectileId;
			explHit.projectileClassId = explosionInfo.projectileClassId;
			explHit.material = 0;
			explHit.damage = damage;
			explHit.type = explosionInfo.type;
			explHit.dir = dir;
			explHit.normal = -dir; // Set normal to reverse of direction to avoid backface cull of damage
			explHit.explosion = true;

			float victimActorInitialHealth = 0.0f;
			CActor* pVictimActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entity->GetId()));
			if(pVictimActor && pVictimActor->IsClient())
			{
				if (ShouldApplyMercyTime(*pVictimActor, explHit))
				{
					explHit.damage = 0;
				}
			}

			if (pVictimActor != NULL && !pVictimActor->IsPlayer())
			{
				victimActorInitialHealth = pVictimActor->GetHealth();
				explHit.damage *= pVictimActor->GetBodyExplosionDamageMultiplier(explHit);
				if(!bHitActor)
				{
					CProjectile* projectile = g_pGame->GetWeaponSystem()->GetProjectile(explHit.projectileId);
					if (projectile)
					{
						CPersistantStats::GetInstance()->IncrementWeaponHits(*projectile, entity->GetId());
					}
					bHitActor = true;
				}
			}
			else if (pVictimActor == NULL)
			{
				CBodyDamageManager* pBodyDamageManager = g_pGame->GetBodyDamageManager();
				CRY_ASSERT(pBodyDamageManager);

				const TBodyDamageProfileId bodyDamageProfileId = pBodyDamageManager->FindBodyDamageProfileIdBinding(explHit.targetId);
				if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
				{
					const float multiplier = pBodyDamageManager->GetExplosionDamageMultiplier(bodyDamageProfileId, *entity, explHit);
					explHit.damage *= multiplier;
				}
			}

			if (pVictimActor)
			{
				pVictimActor->GetDamageEffectController().OnHit(&explHit);
			}

			if (explHit.damage > 0.0f)
			{
				if (!IsDead(pVictimActor, entity->GetScriptTable()))
				{
					// avoid lua call for vehicle hits
					if(IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(entity->GetId()))
					{
						HitInfo hitInfo;
						hitInfo.projectileClassId = explosionInfo.projectileClassId;
						hitInfo.projectileId = explosionInfo.projectileId;
						hitInfo.weaponClassId = weaponClassId;
						hitInfo.weaponId = explosionInfo.weaponId;
						hitInfo.targetId = explosionInfo.impact_targetId;
						hitInfo.shooterId = explHit.shooterId;
						hitInfo.damage = explHit.damage;
						hitInfo.pos = explosionInfo.pos;
						hitInfo.radius = explosionInfo.radius;
						hitInfo.type = explosionInfo.type;
						hitInfo.explosion = true;
						pVehicle->OnHit(hitInfo);
					}	
					else
					{
						DelegateServerHit(entity->GetScriptTable(), explHit, pVictimActor);
					}
				}
			}
		}
	}
}



float CGameRulesSPDamageHandling::CalcExplosionDamage(IEntity* entity, const ExplosionInfo &explosionInfo, float obstruction)
{
	// SinglePlayer.lua 228

	// Impact explosions directly damage the impact target
	if (explosionInfo.impact && explosionInfo.impact_targetId && explosionInfo.impact_targetId==entity->GetId())
	{
		return explosionInfo.damage;
	}

	float effect = 0.0f;

	AABB entityBoundingBox;
	entity->GetWorldBounds(entityBoundingBox);
	if (entityBoundingBox.IsContainPoint(explosionInfo.pos))
	{
		effect = 1.0f;
	}
	else
	{
		float distanceSq = std::numeric_limits<float>::max();

		if (IPhysicalEntity* physicalEntity = entity->GetPhysics())
		{
			pe_status_dynamics status;
			if (physicalEntity->GetStatus(&status) > 0)
				distanceSq = status.centerOfMass.GetSquaredDistance(explosionInfo.pos);
			else
				distanceSq = entity->GetWorldPos().GetSquaredDistance(explosionInfo.pos);
		}
		else
		{
			distanceSq = entity->GetWorldPos().GetSquaredDistance(explosionInfo.pos);
		}

		if (distanceSq <= square(explosionInfo.radius))
		{
			if (distanceSq <= square(explosionInfo.minRadius))
			{
				effect = 1.0f;
			}
			else
			{
				assert(explosionInfo.radius - explosionInfo.minRadius > 0.0f);
				effect = 1.0f - (sqrtf(distanceSq) - explosionInfo.minRadius) / (explosionInfo.radius - explosionInfo.minRadius);
				effect *= effect;
			}
		}
	}

	CRY_ASSERT_TRACE (effect >= 0.0f && effect <= 1.0f, ("Effectiveness of explosion should be between 0 and 1 but it's %.3f (minRadius=%.3f, maxRadius=%.3f)", effect, explosionInfo.minRadius, explosionInfo.radius));

	return explosionInfo.damage * effect * (1.0f - obstruction);
}



void CGameRulesSPDamageHandling::SvOnCollision(const IEntity* pVictimEntity, const CGameRules::SCollisionHitInfo& collisionHitInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#if !defined(_RELEASE)
	if (g_pGameCVars->g_DisableCollisionDamage)
		return;
#endif

	assert(pVictimEntity);
	PREFAST_ASSUME(pVictimEntity);

	IGameFramework* gameFramwork = g_pGame->GetIGameFramework();

	EntityId victimID = pVictimEntity->GetId();
	EntityId offenderID = collisionHitInfo.targetId;

	const IEntity* pOffenderEntity = gEnv->pEntitySystem->GetEntity(offenderID);

	IScriptTable* pVictimScript = pVictimEntity ? pVictimEntity->GetScriptTable() : NULL;
	IScriptTable* pOffenderScript = pOffenderEntity ? pOffenderEntity->GetScriptTable() : NULL;

	const float currentTime = gEnv->pTimer->GetCurrTime();

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

	//Notify object movement to AI
	if (pVictimEntity)
	{
		CRY_PROFILE_REGION(PROFILE_GAME, "Notify object movement to AI");
	
		if (stl::find(m_entityClassesWithTrackedMovement, pVictimEntity->GetClass()))
		{
			IPhysicalEntity *pPhysics = pVictimEntity->GetPhysics();

			// Basic observable parameters
			ObservableParams observableParams;
			observableParams.faction = 0;
			observableParams.typeMask = General;
			observableParams.observablePositionsCount = 1;
			observableParams.observablePositions[0] = pVictimEntity->GetWorldPos();
			observableParams.skipListSize = pPhysics ? 1 : 0;
			observableParams.skipList[0] = pPhysics;
			observableParams.entityId = offenderID;

			uint32 visibleObjectRules = eVOR_OnlyWhenMoving|eVOR_UseMaxViewDist|eVOR_FlagDropOnceInvisible;
			if (offenderID)
			{
				visibleObjectRules |= eVOR_FlagNotifyOnSeen;
			}
			g_pGame->GetGameAISystem()->GetVisibleObjectsHelper().RegisterObject(victimID, observableParams, visibleObjectRules);
		}
	}

	CActor* victimActor = static_cast<CActor*>(gameFramwork->GetIActorSystem()->GetActor(victimID));

	IVehicle* offenderVehicle = gameFramwork->GetIVehicleSystem()->GetVehicle(offenderID);

	if (offenderVehicle != NULL && offenderVehicle->GetStatus().beingFlipped)
		return;

	float offenderMass = collisionHitInfo.target_mass;

	enum
	{
		CollisionWithEntity,
		CollisionWithStaticWorld
	}
	collisionType = (pOffenderEntity || offenderMass > 0.0f) ? CollisionWithEntity : CollisionWithStaticWorld;

	const Vec3& victimVelocity = collisionHitInfo.velocity;
	const Vec3& offenderVelocity = collisionHitInfo.target_velocity;
	const Vec3 relativeVelocity = victimVelocity - offenderVelocity;

	float relativeSpeed = 0.0f;
	float minSpeedToCareAboutCollision = 0.0f;
	float contactMass = 0.0f;

	switch (collisionType)
	{
	case CollisionWithEntity:
		{
			relativeSpeed = relativeVelocity.GetLength();

			minSpeedToCareAboutCollision = 10.0f;

			if (victimActor && !victimActor->IsPlayer())
			{
				const float slightlyMoreThanMaximumPlayerWalkSpeed = 5.0f;
				minSpeedToCareAboutCollision = slightlyMoreThanMaximumPlayerWalkSpeed;
			}

			if (victimActor && offenderVehicle)
			{
				minSpeedToCareAboutCollision = 6.0f; // -- otherwise we don't get damage at slower speeds
			}

			contactMass = offenderMass;
			break;
		}

	case CollisionWithStaticWorld:
		{
			relativeSpeed = victimVelocity.GetLength();
			minSpeedToCareAboutCollision = 7.5f;

			if (IPhysicalEntity* phys = pVictimEntity->GetPhysics())
			{
				pe_status_dynamics dyn;
				phys->GetStatus(&dyn);
				contactMass = dyn.mass;
			}
			else
			{
				contactMass = 0.0f;
			}

			break;
		}
	}

	const bool contactMassIsTooLowToCare = contactMass < 0.01f;
	if (contactMassIsTooLowToCare)
		return;

	if (relativeSpeed >= minSpeedToCareAboutCollision /*|| bigObject*/)
	{
		HitInfo hit;

		// Calculate damage from collision energy
		IVehicle* victimVehicle = gameFramwork->GetIVehicleSystem()->GetVehicle(victimID);
		float fEnergy = GetCollisionEnergy(pVictimEntity, collisionHitInfo);
		if (victimVehicle)
		{
			hit.damage = 0.0005f * fEnergy;
		}
		else
		{
			hit.damage = 0.0025f * fEnergy;
		}

		// Apply damage multipliers
		hit.damage *= GetCollisionDamageMult(pVictimEntity, pOffenderEntity, collisionHitInfo);

		hit.pos = collisionHitInfo.pos;
		if (collisionHitInfo.target_velocity.GetLengthSquared() > 1e-6)
			hit.dir = collisionHitInfo.target_velocity.GetNormalized();
		hit.radius = 0.0f;
		hit.partId = collisionHitInfo.partId;
		hit.targetId = victimID;
		hit.weaponId = offenderID;
		hit.shooterId = offenderID;
		hit.material = 0;
		hit.type = CGameRules::EHitType::Collision;

		//
		// Special damage handling for actors
		//

		SReactionInfoOnHit reactionInfo;

		IActor* offenderActor = gameFramwork->GetIActorSystem()->GetActor(offenderID);

		if (victimActor)
		{
			const bool victimIsPlayer = victimActor->IsPlayer();

			if (victimIsPlayer)
			{
				hit.damage = AdjustPlayerCollisionDamage(pVictimEntity, offenderVehicle != NULL, collisionHitInfo, hit.damage);

				reactionInfo.bTriggerHitReaction = false;
			}
			else // victim is AI
			{
				bool bIsInvulnerable = false;
				if (IScriptTable* pTargetScript = victimActor->GetEntity()->GetScriptTable())
				{
					HSCRIPTFUNCTION isInvulnerableFunc = NULL;
					if (pTargetScript->GetValue("IsInvulnerable", isInvulnerableFunc) && isInvulnerableFunc)
					{
						Script::CallReturn(gEnv->pScriptSystem, isInvulnerableFunc, pTargetScript, bIsInvulnerable);
						gEnv->pScriptSystem->ReleaseFunc(isInvulnerableFunc);
					}
				}

				if (!bIsInvulnerable)
				{
					hit.damage *= victimActor->GetBodyDamageMultiplier(hit);
				}
				else
				{
					hit.damage = 0.0f;
				}
				

				if (pOffenderEntity)
				{
					float minSpeedForFallAndPlaySquared = g_pGameCVars->AICollisions.minSpeedForFallAndPlay * g_pGameCVars->AICollisions.minSpeedForFallAndPlay;
					if (offenderActor)
					{
						CPlayer* offenderPlayer = static_cast<CPlayer*>(offenderActor);
						if (offenderPlayer->IsPlayer())
						{
							const SAutoaimTarget* pTargetInfo = g_pGame->GetAutoAimManager().GetTargetInfo(victimID);
							bool bVictimIsFriendly = pTargetInfo && !pTargetInfo->HasFlagSet(eAATF_AIHostile);

							const bool playerIsAirborne = !offenderPlayer->GetActorStats()->onGround;
							const bool playerIsSliding = offenderPlayer->IsSliding();
							// sprinting trigering for fall and play is temporary deactivated.
//							const bool playerIsSprinting = offenderPlayer->IsSprinting();

							if (playerIsAirborne || playerIsSliding) //  || playerIsSprinting)
							{
								if (bVictimIsFriendly)
								{
									hit.damage = 0.0f; // No damage for sliding or jumping into friendlies
									reactionInfo.bMakeVictimFall = true; //this is an friend
								}
								else
								{
									reactionInfo.bMakeVictimFall = true;  //this is an enemey
								}
							}

							reactionInfo.bTriggerHitReaction = false;
						}
						else // offender is an AI
						{
							reactionInfo.bTriggerHitReaction = false;

							const SActorStats* pStats = static_cast<CActor*>(offenderActor)->GetActorStats();
							if (pStats && pStats->isRagDoll)
							{
								// we use absolute offender speed too, because we dont want the ai to fall when trip over an static ragdoll
								if (relativeSpeed>=g_pGameCVars->AICollisions.minSpeedForFallAndPlay && offenderVelocity.GetLengthSquared()>=minSpeedForFallAndPlaySquared)
								{
									reactionInfo.bTriggerHitReaction = true;
									reactionInfo.bMakeVictimFall = false; /*!bIsInvulnerable*/
								}
							}
						}
					}
					else // Offender is an object
					{
 						// we use absolute offender speed too, because we dont want the ai to fall when trip over an object
 						if (relativeSpeed>=g_pGameCVars->AICollisions.minSpeedForFallAndPlay &&
							offenderMass>=g_pGameCVars->AICollisions.minMassForFallAndPlay && 
 							offenderVelocity.GetLengthSquared()>=minSpeedForFallAndPlaySquared)
 						{
							// Only wanting to do falls when you're hit by a rigid, this excludes ragdolls, for example.
							const IPhysicalEntity* pent = pOffenderEntity->GetPhysics();
							if( pent && (pent->GetType() == PE_RIGID) )
							{
 								reactionInfo.bMakeVictimFall = !bIsInvulnerable;
							}
 						}

						reactionInfo.bTriggerHitReaction = hit.damage > DAMAGE_TRESHOLD_COLLISION_REACTIONS;
					}
				}
			}

			if (offenderVehicle)
			{
				hit.damage = AdjustVehicleActorCollisionDamage(offenderVehicle, victimActor, collisionHitInfo, hit.damage);

				reactionInfo.bMakeVictimFall = !victimIsPlayer && (hit.damage > DAMAGE_TRESHOLD_COLLISION_REACTIONS);
			}
		}
		else
		{
			CBodyDamageManager* pBodyDamageManager = g_pGame->GetBodyDamageManager();
			CRY_ASSERT(pBodyDamageManager);

			const TBodyDamageProfileId bodyDamageProfileId = pBodyDamageManager->FindBodyDamageProfileIdBinding(victimID);
			if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(victimID);
				const float multiplier = pBodyDamageManager->GetDamageMultiplier(bodyDamageProfileId, *pEntity, hit);
				hit.damage *= multiplier;
			}
		}

#ifndef _RELEASE
		if (g_pGameCVars->AICollisions.showInLog==2 || g_pGameCVars->AICollisions.showInLog==1 && hit.damage>=DAMAGE_TRESHOLD_COLLISIONS)
		{
			CryLog("---Collision. victim: ID:%d '%s'   offender: ID:%d '%s'   relativeSpeed: %f  offenderSpeed: %f  offenderMass: %f  damage: %f  fallAndPlay: %s",
				victimID, pVictimEntity ? pVictimEntity->GetName() : "<null>",
				offenderID, pOffenderEntity?pOffenderEntity->GetName() : "<null>",
				relativeSpeed, offenderVelocity.GetLength(), offenderMass, hit.damage,
				reactionInfo.bMakeVictimFall ? "YES" : "NO" 
				);
		}
#endif

		if ((hit.damage >= DAMAGE_TRESHOLD_COLLISIONS) || reactionInfo.bMakeVictimFall || reactionInfo.bTriggerHitReaction)
		{
			if (!pOffenderEntity && pVictimEntity)
			{
				pOffenderEntity = pVictimEntity;
				offenderID = victimID;
			}

			m_entityCollisionRecords[victimID] = EntityCollisionRecord(offenderID, currentTime);

			if(victimVehicle)
			{
				HitInfo hitInfo;
				hitInfo.targetId = victimID;
				hitInfo.shooterId = offenderID;
				hitInfo.damage = hit.damage;
				hitInfo.pos = hit.pos;
				hitInfo.type = hit.type;

				victimVehicle->OnHit(hitInfo);
			}	
			else if (pVictimScript)
			{
				CRY_PROFILE_REGION(PROFILE_GAME, "Call to OnHit");

				if (!IsDead(victimActor, pVictimScript))
				{
					if (IActor* offenderDriver = offenderVehicle ? offenderVehicle->GetDriver() : NULL)
						hit.shooterId = offenderDriver->GetEntityId();

					DelegateServerHit(pVictimScript, hit, victimActor, &reactionInfo);
				}
			}
		}
	}
}


// float CGameRulesSPDamageHandling::GetCollisionMinVelocity(EntityId victimID, const CGameRules::SCollisionHitInfo& colHitInfo)
// {
// 	CRY_PROFILE_FUNCTION(PROFILE_GAME);
// 
// 	float minVel = 10.0f;
// 
// 	if ((victimActor && !victimActor->IsPlayer()))
// 	{
// 		minVel = 1.0f;
// 	}
// 
// 	if (victimActor && offenderVehicle)
// 	{
// 		minVel = 6.0f; // -- otherwise we don't get damage at slower speeds
// 	}
// 
// 	if (offenderSpeedSq == 0.0f) // -- if collision target it not moving
// 	{
// 		minVel = minVel * 2.0f;
// 	}
// 
// 	return minVel;
// }

float CGameRulesSPDamageHandling::AdjustPlayerCollisionDamage(const IEntity* victimEntity, bool offenderIsVehicle, const CGameRules::SCollisionHitInfo& colHitInfo, float damage)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (offenderIsVehicle)
		return damage;

	// This seems to happen when we're colliding with the world.
	if (colHitInfo.target_velocity.len() == 0.0f)
	{
		damage *= 0.2f;
	}

	CActor* entityActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimEntity->GetId()));
	if (entityActor)
	{
		if (entityActor->GetActorClass() == CPlayer::GetActorClassType())
		{
			CPlayer* pPlayer = static_cast<CPlayer*>(entityActor);
			if (pPlayer->IsOnLedge())
			{
				return 0.0f;	//No collision damage while performing a ledge grab
			}
			if (pPlayer->IsInMercyTime())
			{
				return 0.0f;
			}
		}

		float healthThreshold = (float)g_pGameCVars->pl_health.collision_health_threshold;
		float currentHealth = entityActor->GetHealth();

		if (currentHealth <= healthThreshold)
		{
			damage = 0.0f;
		}
		else if ((currentHealth - damage) < healthThreshold)
		{
			damage = currentHealth - healthThreshold;
		}
	}

	return damage;
}

float CGameRulesSPDamageHandling::AdjustVehicleActorCollisionDamage(IVehicle* pOffenderVehicle, IActor* pVictimActor, const CGameRules::SCollisionHitInfo& colHitInfo, float damage)
{
	assert(pOffenderVehicle && pVictimActor);
	IActor* pActorDriver = pOffenderVehicle->GetDriver();

	// in the case of non-driven vehicles no multiplier to damage
	if(!pActorDriver)
	{
		return damage;
	}
	
	const SVehicleDamageParams& params = pOffenderVehicle->GetDamageParams();

	bool playerDriver = pActorDriver ? pActorDriver->IsPlayer() : false;
	bool playerVictim = pVictimActor->IsPlayer();

	float speedSq = colHitInfo.target_velocity.GetLengthSquared();

	if(playerDriver)
	{
		if(playerVictim)
		{
			// player hitting player - no change to damage			
		}
		else
		{
			// player hitting AI. If above the speed threshold specified in the vehicle xml, kill the AI
			if(params.playerKillAISpeed > 0.1f && (speedSq > params.playerKillAISpeed * params.playerKillAISpeed))
			{
				damage = pVictimActor->GetMaxHealth();
			}
		}
	}
	else
	{
		if(playerVictim)
		{
			// AI hitting player
			if(params.aiKillPlayerSpeed > 0.1f && (speedSq > params.aiKillPlayerSpeed * params.aiKillPlayerSpeed))
			{
				damage = pVictimActor->GetMaxHealth();
			}
		}
		else
		{
			// AI hitting AI
			uint8 driverFaction = pActorDriver->GetEntity()->GetAI()->GetFactionID();
			uint8 victimFaction = pVictimActor->GetEntity()->GetAI()->GetFactionID();

			IFactionMap& map = gEnv->pAISystem->GetFactionMap();
			if(map.GetReaction(driverFaction, victimFaction) == IFactionMap::Hostile)
			{
				if(params.aiKillAISpeed > 0.1f && (speedSq > params.aiKillAISpeed * params.aiKillAISpeed))
				{
					damage = pVictimActor->GetMaxHealth();
				}
			}
			else
			{
				// AI don't damage non-hostile AI
				damage = 0.0f;
			}

		}
	}

	return damage;
}

//float CGameRulesSPDamageHandling::ProcessPlayerToActorCollision(const IEntity* entity, const IEntity* collider, const CGameRules::SCollisionHitInfo& colHitInfo, float colInfo_damage)
//{
//	CRY_PROFILE_FUNCTION(PROFILE_GAME);
//
//	// Cancel out damage if the offender is in a ragdoll state
//// 	CActor* colliderActor = collider ? static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(collider->GetId())) : NULL;
//// 	if (colliderActor)
//// 	{
//// 		uint8 profile = colliderActor->GetGameObject()->GetAspectProfile(eEA_Physics);
//// 		if ((profile == eAP_Sleep) || (profile == eAP_Ragdoll))
//// 		{
//// 			return 0.0f;
//// 		}
//// 	}
//
//	return colInfo_damage;
//}




void CGameRulesSPDamageHandling::DelegateServerHit(IScriptTable* victimScript, const HitInfo& hit, CActor* pVictimActor, const SReactionInfoOnHit* pReactionInfo/* = NULL*/)
{
	SmartScriptTable victimServerScript;
	if (victimScript->GetValue("Server", victimServerScript))
	{
		HSCRIPTFUNCTION pfnOnHit = 0;
		if (victimServerScript->GetValue("OnHit", pfnOnHit))
		{
			bool diedAfterHit = false;
			bool isInvulnerable = false;
			victimScript->GetValue("invulnerable", isInvulnerable);

			const float fTargetHealthBeforeHit = pVictimActor ? pVictimActor->GetHealth() : 0.0f;

			m_pGameRules->CreateScriptHitInfo(m_scriptHitInfo, hit);
			if (isInvulnerable || Script::CallReturn(gEnv->pScriptSystem, pfnOnHit, victimScript, m_scriptHitInfo, diedAfterHit))
			{
				if (hit.explosion && pVictimActor)
				{
					const float fTargetHealthAfterHit = pVictimActor->GetHealth();
					pVictimActor->ProcessDestructiblesOnExplosion(hit, fTargetHealthBeforeHit, fTargetHealthAfterHit);
				}

				if (diedAfterHit)
				{
					if (pVictimActor)
						ProcessDeath(pVictimActor, victimScript, hit);
					else
						m_pGameRules->OnEntityKilled(hit);
				}
				else // Hit was not deadly
				{
					CRY_PROFILE_REGION(PROFILE_GAME, "Trigger hit reaction");

					// Trigger hit reaction
					bool hitReactionProcessed = false;
					if ((!pReactionInfo || pReactionInfo->bTriggerHitReaction) && pVictimActor)
					{
						CPlayer* pVictimPlayer = static_cast<CPlayer*>(pVictimActor);
						CHitDeathReactionsPtr hitReactions =  pVictimPlayer->GetHitDeathReactions();
						if (hitReactions)
						{
							const float fCausedDamage = fTargetHealthBeforeHit - pVictimActor->GetHealth();
							hitReactionProcessed = hitReactions->OnHit(hit, fCausedDamage);
						}
					}

					if (!hitReactionProcessed && pReactionInfo)
					{
						if (pReactionInfo->bMakeVictimFall && pVictimActor && static_cast<CPlayer*>(pVictimActor)->CanFall())
							pVictimActor->Fall(hit);

						if (pReactionInfo->bMakeOffenderFall && pReactionInfo->pOffenderActor && static_cast<CPlayer*>(pReactionInfo->pOffenderActor)->CanFall())
							static_cast<CActor*> (pReactionInfo->pOffenderActor)->Fall(hit);
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

void CGameRulesSPDamageHandling::MakeMovementVisibleToAIForEntityClass( const IEntityClass* pEntityClass )
{
	stl::push_back_unique(m_entityClassesWithTrackedMovement, pEntityClass);
}

void CGameRulesSPDamageHandling::OnGameEvent(const IGameRulesDamageHandlingModule::EGameEvents& gameEvent)
{
	switch(gameEvent)
	{
	case IGameRulesDamageHandlingModule::eGameEvent_LocalPlayerEnteredMercyTime:
		{
			m_mercyTimeFilter.OnLocalPlayerEnteredMercyTime();
		}
		break;	

	case IGameRulesDamageHandlingModule::eGameEvent_TrainRideStarts:
		{
			m_invulnerableFilter.AddInvulnerableReason(CInvulnerableFilter::eReason_TrainRide);
		}
		break;

	case  IGameRulesDamageHandlingModule::eGameEvent_TrainRideEnds:
		{
			m_invulnerableFilter.RemoveInvulnerableReason(CInvulnerableFilter::eReason_TrainRide);
		}
		break;
	}
}

bool CGameRulesSPDamageHandling::IsDead(CActor* actor, IScriptTable* actorScript)
{
	if (actor)
	{
		return actor->IsDead();
	}
	else
	{
		// The victim is not an actor, so we can't just check the health.
		// Instead we call the customizable Lua function IsDead, if it exists.
		//
		// isDead = result from Lua function IsDead in victim entity script

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

bool CGameRulesSPDamageHandling::ShouldApplyMercyTime(CActor& localClientActor, const HitInfo& hitInfo) const
{
	assert(localClientActor.IsClient());

	IF_UNLIKELY(m_invulnerableFilter.FilterIncomingDamage( hitInfo, localClientActor.GetHealth()))
	{
		return true;
	}

	if (!localClientActor.IsInMercyTime())
	{
		return false;
	}

	IF_UNLIKELY ((hitInfo.type == CGameRules::EHitType::Punish) || (hitInfo.type == CGameRules::EHitType::PunishFall) || (hitInfo.type == CGameRules::EHitType::VehicleDestruction))
	{
		return false;
	}

	return !(m_mercyTimeFilter.Filtered(hitInfo.projectileClassId, hitInfo.shooterId, hitInfo.targetId));
}
