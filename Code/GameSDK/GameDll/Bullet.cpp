// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 18:10:2005   14:14 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "Bullet.h"
#include "GameRules.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryGame/IGameTokens.h>
#include <CryAction/IMaterialEffects.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "AmmoParams.h"
#include "Actor.h"
#include "WeaponSystem.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryAISystem/IAIObject.h>

#include <IPerceptionManager.h>

struct SPhysicsRayWrapper
{
	SPhysicsRayWrapper()
		: m_pRay(NULL)
	{
		primitives::ray rayData; 
		rayData.origin.zero(); 
		rayData.dir = FORWARD_DIRECTION;
		m_pRay = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::ray::type, &rayData);
	}

	~SPhysicsRayWrapper()
	{
		if (m_pRay)
		{
			m_pRay->Release();
			m_pRay = NULL;
		}
	}

	IGeometry* GetRay(const Vec3& pos, const Vec3& dir)
	{
		primitives::ray rayData;
		rayData.origin = pos;
		rayData.dir = dir;
		m_pRay->SetData(&rayData);

		return m_pRay;
	}

	void ResetRay()
	{
		primitives::ray rayData; 
		rayData.origin.zero(); 
		rayData.dir = FORWARD_DIRECTION;
		m_pRay->SetData(&rayData);
	}

private:

	IGeometry*	m_pRay;
};

SPhysicsRayWrapper* CBullet::s_pRayWrapper;
IEntityClass* CBullet::EntityClass = 0;

#ifdef DEBUG_BULLET_PENETRATION
	SDebugBulletPenetration CBullet::s_debugBulletPenetration;
#endif

//------------------------------------------------------------------------
void CBullet::StaticInit()
{
	s_pRayWrapper = new SPhysicsRayWrapper;
}

//------------------------------------------------------------------------
void CBullet::StaticShutdown()
{
	SAFE_DELETE(s_pRayWrapper);
}

//------------------------------------------------------------------------
CBullet::CBullet()
: m_damageCap(FLT_MAX)
, m_damageFallOffAmount(0.0f)
, m_damageFallOffStart(0.0f)
, m_damageFalloffMin(0.0f)
, m_pointBlankAmount(1.0f)
, m_pointBlankDistance(0.0f)
, m_pointBlankFalloffDistance(0.0f)
, m_accumulatedDamageFallOffAfterPenetration(0.0f)
, m_bulletPierceability(0)
, m_penetrationCount(0)
, m_alive(true)
, m_ownerIsPlayer(false)
{

#if BULLET_PENETRATION_BACKSIDE_FX_ENABLED_SP
	m_backSideEffectsDisabled = false;
#else
	m_backSideEffectsDisabled = (gEnv->bMultiplayer == false);
#endif

}

//------------------------------------------------------------------------
CBullet::~CBullet()
{
}

//------------------------------------------------------------------------
void CBullet::SetParams(const SProjectileDesc& projectileDesc)
{
	m_damageFallOffAmount = projectileDesc.damageFallOffAmount;
	m_damageFallOffStart = projectileDesc.damageFallOffStart;
	m_damageFalloffMin = projectileDesc.damageFallOffMin;
	m_pointBlankAmount = projectileDesc.pointBlankAmount;
	m_pointBlankDistance = projectileDesc.pointBlankDistance;
	m_pointBlankFalloffDistance = projectileDesc.pointBlankFalloffDistance;

	CActor* pOwner = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(projectileDesc.ownerId));
	m_ownerIsPlayer = pOwner ? pOwner->IsPlayer() : false;

	BaseClass::SetParams(projectileDesc);
}

//------------------------------------------------------------------------
int CBullet::GetRopeBoneId(const EventPhysCollision& collision, IEntity& target, IPhysicalEntity* pRopePhysicalEntity) const
{
	int boneId = -1;

	ICharacterInstance* pCharacterInstance = target.GetCharacter(0);
	if (!pCharacterInstance)
		return boneId;

	ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
	if (!pSkeletonPose)
		return boneId;

	int auxPhys = 0;
	while (IPhysicalEntity* pPhysicalEntity = pSkeletonPose->GetCharacterPhysics(auxPhys))
	{
		if (pRopePhysicalEntity == pPhysicalEntity)
		{
			boneId = pSkeletonPose->GetAuxPhysicsBoneId(auxPhys, collision.partid[1]);
			break;
		}
		++auxPhys;
	}

	return boneId;
}

//------------------------------------------------------------------------
bool CBullet::CheckForPreviousHit(EntityId targetId, float& damage)
{
	int numHitActors = m_hitActors.size();

	bool hitFound = false;
	bool damageMaxxed = false;

	float totalDamage = 0.f;

	for (int i = 0; i < numHitActors; i++)
	{
		if (m_hitActors[i].hitActorId == targetId)
		{
			hitFound = true;
			damageMaxxed = true;
			
			if(!m_hitActors[i].previousHit)
			{
				m_hitActors[i].previousHit = true;

				damage = min(damage, m_damageCap - m_hitActors[i].linkedAccumDamage);

				if(damage > 0.f)
				{
					totalDamage = m_hitActors[i].linkedAccumDamage + damage;
					m_hitActors[i].linkedAccumDamage = totalDamage;
					
					damageMaxxed = false;
				}
			}

			break;
		}
	}

	if(!hitFound)
	{
		m_hitActors.push_back(SActorHitInfo(targetId, damage, true));

		totalDamage = damage;
	}

	if(totalDamage > 0.f && m_damageCap < FLT_MAX && IsLinked()) //don't bother updating the linked projs if there is no damage cap used
	{
		CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();
		const CWeaponSystem::TLinkedProjectileMap& projectileMap = pWeaponSystem->GetLinkedProjectiles();

		CWeaponSystem::TLinkedProjectileMap::const_iterator projIt = projectileMap.find(GetEntityId());
		CWeaponSystem::TLinkedProjectileMap::const_iterator projEnd = projectileMap.end();

		if(projIt != projEnd)
		{
			const CWeaponSystem::SLinkedProjectileInfo& info = projIt->second;

			for(CWeaponSystem::TLinkedProjectileMap::const_iterator currentProjIt = projectileMap.begin(); currentProjIt != projEnd; ++currentProjIt)
			{
				const CWeaponSystem::SLinkedProjectileInfo& curInfo = currentProjIt->second;
				if(curInfo.weaponId == info.weaponId && curInfo.shotId == info.shotId)
				{
					CProjectile* pProj = pWeaponSystem->GetProjectile(currentProjIt->first);

					if(pProj)
					{
						pProj->UpdateLinkedDamage(targetId, totalDamage);
					}
				}
			}
		}
	}
	
	return damageMaxxed;
}

void CBullet::UpdateLinkedDamage(EntityId hitActorId, float totalAccumDamage)
{
	int numHitActors = m_hitActors.size();

	for (int i = 0; i < numHitActors; i++)
	{
		if (m_hitActors[i].hitActorId == hitActorId)
		{
			m_hitActors[i].linkedAccumDamage = totalAccumDamage;
			return;
		}
	}

	m_hitActors.push_back(SActorHitInfo(hitActorId, totalAccumDamage, false));
}

//------------------------------------------------------------------------
void CBullet::ProcessHit(CGameRules& gameRules, const EventPhysCollision& collision, IEntity& target, float damage, int hitMatId, const Vec3& hitDir)
{
	if(damage > 0.f)
	{
		EntityId targetId = target.GetId();
		bool alreadyHit = CheckForPreviousHit(targetId, damage);

		if(!alreadyHit)
		{
			HitInfo hitInfo(m_ownerId ? m_ownerId : m_hostId, targetId, m_weaponId,
				damage, 0.0f, hitMatId, collision.partid[1],
				m_hitTypeId, collision.pt, hitDir, collision.n);

			hitInfo.remote = IsRemote();
			hitInfo.projectileId = GetEntityId();
			hitInfo.bulletType = m_pAmmoParams->bulletType;
			hitInfo.knocksDown = CheckAnyProjectileFlags(ePFlag_knocksTarget) && ( damage > m_minDamageForKnockDown );
			hitInfo.knocksDownLeg = m_chanceToKnockDownLeg>0 && damage>m_minDamageForKnockDownLeg && m_chanceToKnockDownLeg>cry_random(0, 99);
			hitInfo.penetrationCount = m_penetrationCount;
			hitInfo.hitViaProxy = CheckAnyProjectileFlags(ePFlag_firedViaProxy);
			hitInfo.aimed = CheckAnyProjectileFlags(ePFlag_aimedShot);

			IPhysicalEntity* pPhysicalEntity = collision.pEntity[1];
			if (pPhysicalEntity && pPhysicalEntity->GetType() == PE_ROPE)
			{
				hitInfo.partId = GetRopeBoneId(collision, target, pPhysicalEntity);
			}

			gameRules.ClientHit(hitInfo);

			ReportHit(targetId);
		}
	}
}

//------------------------------------------------------------------------
void CBullet::HandleEvent(const SGameObjectEvent &event)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	BaseClass::HandleEvent(event);

	if (event.event == eGFE_OnCollision)
	{
		if (CheckAnyProjectileFlags(ePFlag_destroying))
			return;

		EventPhysCollision *pCollision = reinterpret_cast<EventPhysCollision *>(event.ptr);
		if (!pCollision)
			return;

		float finalDamage = GetFinalDamage(pCollision->pt);
		if (finalDamage <= 0.0f)
			m_alive = false;
        
		IEntity *pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1]:0;
		CGameRules *pGameRules = g_pGame->GetGameRules();
		const int hitMatId = pCollision->idmat[1];

		Vec3 hitDir(ZERO);
		if (pCollision->vloc[0].GetLengthSquared() > 1e-6f)
		{
			hitDir = pCollision->vloc[0].GetNormalized();
		}

		const bool bProcessCollisionEvent = ProcessCollisionEvent(pTarget);
		if (bProcessCollisionEvent)
		{
			//================================= Process Hit =====================================
			//Only process hits that have a target
			if(pTarget)
			{
				if(FilterFriendlyAIHit(pTarget) == false)
				{
					ProcessHit(*pGameRules, *pCollision, *pTarget, finalDamage, hitMatId, hitDir);
				}
			}
			//====================================~ Process Hit ======================================

			//==================================== Notify AI    ======================================
			IPerceptionManager* perceptionManager = IPerceptionManager::GetInstance();
			if (perceptionManager)
			{
				if (gEnv->pEntitySystem->GetEntity(m_ownerId))
				{
					ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(hitMatId);
					const ISurfaceType::SSurfaceTypeAIParams* pParams = pSurfaceType ? pSurfaceType->GetAIParams() : 0;
					const float radius = pParams ? pParams->fImpactRadius : 2.5f;
					const float soundRadius = pParams ? pParams->fImpactSoundRadius : 20.0f;

					SAIStimulus stim(AISTIM_BULLET_HIT, 0, m_ownerId, pTarget ? pTarget->GetId() : 0, pCollision->pt, pCollision->vloc[0].GetNormalizedSafe(ZERO), radius);
					perceptionManager->RegisterStimulus(stim);

					SAIStimulus stimSound(AISTIM_SOUND, AISTIM_BULLET_HIT, m_ownerId, 0, pCollision->pt, ZERO, soundRadius);
					perceptionManager->RegisterStimulus(stimSound);
				}
			}
			//=========================================~ Notify AI ===============================
		}

		//========================================= Surface Pierceability ==============================
		if (pCollision->pEntity[0]->GetType() == PE_PARTICLE)
		{
			const SPierceabilityParams& pierceabilityParams = m_pAmmoParams->pierceabilityParams;

			//If collided water
			if( s_materialLookup.IsMaterial( pCollision->idmat[1], CProjectile::SMaterialLookUp::eType_Water ) )
			{
				if(pierceabilityParams.DestroyOnWaterImpact())
				{
					DestroyAtHitPosition(pCollision->pt);
				}
				else
				{
					EmitUnderwaterTracer(pCollision->pt, pCollision->pt + (pCollision->vloc[0].GetNormalizedSafe() * 100.0f));
				}
			}
			else if (m_pAmmoParams->bounceableBullet == 0)
			{
				float bouncy, friction;
				uint32 pierceabilityMat;
				gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
				pierceabilityMat &= sf_pierceable_mask;
				
				const bool terrainHit = (pCollision->idCollider == -1);

				bool thouShallNotPass = terrainHit;
				if (!CheckAnyProjectileFlags(ePFlag_ownerIsPlayer))
					thouShallNotPass = thouShallNotPass || pierceabilityParams.DestroyOnImpact(pierceabilityMat);

				if (!thouShallNotPass)
					HandlePierceableSurface(pCollision, pTarget, hitDir, bProcessCollisionEvent);

				const bool destroy = thouShallNotPass || ShouldDestroyBullet();
				if (destroy)
				{
					DestroyAtHitPosition(pCollision->pt);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CBullet::EmitUnderwaterTracer(const Vec3& pos, const Vec3& destination)
{
	const STrailParams* pTrail = m_pAmmoParams->pTrailUnderWater;

	if (!pTrail)
		return;

	CTracerManager::STracerParams params;
	params.position = pos;
	params.destination = destination;
	params.geometry = NULL;
	params.effect = pTrail->effect.c_str();
	params.speed = 35.0f;
	params.lifetime = 2.5f;
	params.delayBeforeDestroy = 0.f;

	g_pGame->GetWeaponSystem()->GetTracerManager().EmitTracer(params);
}

//////////////////////////////////////////////////////////////////////////
bool CBullet::FilterFriendlyAIHit(IEntity* pHitTarget)
{
	bool bResult = false;

	if (!gEnv->bMultiplayer && pHitTarget)
	{
		const bool bIsClient = (m_ownerId == g_pGame->GetIGameFramework()->GetClientActorId());
		IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_ownerId);

		//Filter client hits against friendly AI
		if (pOwnerEntity && bIsClient)
		{
			IAIObject *pOwnerAI = pOwnerEntity->GetAI();
			IAIObject *pTargetAI = pHitTarget->GetAI();
			if (pOwnerAI && pTargetAI && !pTargetAI->IsHostile(pOwnerAI))
			{
				const bool bEnableFriendlyHit = g_pGameCVars->g_enableFriendlyPlayerHits != 0;
				if (!bEnableFriendlyHit)
				{
					g_pGame->GetGameRules()->SetEntityToIgnore(pHitTarget->GetId());
					bResult = true;
				}
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
float CBullet::GetFinalDamage( const Vec3& hitPos ) const
{
	float damage = (float)m_damage - m_accumulatedDamageFallOffAfterPenetration;

	if(m_damageFallOffAmount > 0.f)
	{
		float distTravelledSq = (m_initial_pos - hitPos).GetLengthSquared();
		bool fallof = distTravelledSq > (m_damageFallOffStart*m_damageFallOffStart);
		bool pointBlank = distTravelledSq < (m_pointBlankFalloffDistance*m_pointBlankFalloffDistance) && m_ownerIsPlayer;
		float distTravelled = (fallof || pointBlank) ? sqrt_tpl(distTravelledSq) : 0.0f;

		if (fallof)
		{
			distTravelled -= m_damageFallOffStart;
			damage -= distTravelled * m_damageFallOffAmount;
		}

		if (pointBlank)
		{
			damage *= Projectile::GetPointBlankMultiplierAtRange(distTravelled, m_pointBlankDistance, m_pointBlankFalloffDistance, m_pointBlankAmount);
		}
	}

	damage = max(damage, m_damageFalloffMin);

	return damage;
}


//////////////////////////////////////////////////////////////////////////
void CBullet::HandlePierceableSurface( const EventPhysCollision* pCollision, IEntity* pHitTarget, const Vec3& hitDirection, bool bProcessedCollisionEvent )
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	const SPierceabilityParams& pierceabilityParams = m_pAmmoParams->pierceabilityParams;
	
	const int maxPenetrationCount = 4;
	const float entryAngleDot = pCollision->n.Dot(hitDirection);
	bool backFace = (entryAngleDot >= 0);

#ifdef DEBUG_BULLET_PENETRATION
	bool debugBulletPenetration = (g_pGameCVars->g_bulletPenetrationDebug != 0);
#endif

	if (backFace == false)
	{
		//Front face hit, accumulate damage falloff after penetration
		float bouncy, friction;
		uint32 pierceabilityMat;
		gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
		pierceabilityMat &= sf_pierceable_mask;

#ifdef DEBUG_BULLET_PENETRATION
		const float damageBeforePenetration = GetDamageAfterPenetrationFallOff();
#endif

		m_penetrationCount++;

		//1- Check if collided surface might stop the bullet
		const bool collisionStopsBullet = (!bProcessedCollisionEvent) || 
			(pCollision->idCollider == -1) || 
			((int16)pierceabilityMat <= GetBulletPierceability()) || 
			(m_penetrationCount >= maxPenetrationCount);
		
		if (collisionStopsBullet)
		{
#ifdef DEBUG_BULLET_PENETRATION
			if (debugBulletPenetration)
			{
				s_debugBulletPenetration.AddBulletHit(pCollision->pt, hitDirection, damageBeforePenetration, (pCollision->idCollider == -1) ? -1 : pierceabilityMat, false, true, false);
			}
#endif
			m_accumulatedDamageFallOffAfterPenetration += (float)m_damage;
			return;
		}

		//2- If not stopped, add fall off damage, and see if can still penetrate
		m_accumulatedDamageFallOffAfterPenetration += (float)m_damage * (pierceabilityParams.GetDamageFallOffForPierceability(pierceabilityMat) * 0.01f);
		
		bool needsBackFaceCheck = (GetDamageAfterPenetrationFallOff() > 0.0f) && pierceabilityParams.SurfaceRequiresBackFaceCheck(pierceabilityMat);

#ifdef DEBUG_BULLET_PENETRATION
		if (debugBulletPenetration)
		{
			if (ShouldDestroyBullet())
			{
				s_debugBulletPenetration.AddBulletHit(pCollision->pt, hitDirection, damageBeforePenetration, pierceabilityMat, false, true, false);
			}
		}
#endif

		if (needsBackFaceCheck)
		{
			//3- Raytrace backwards, to check thickness & exit point if any
			const float angleFactor = 1.0f/max(0.2f, -entryAngleDot);
			const float distCheck = pierceabilityParams.maxPenetrationThickness * angleFactor;

			SBackHitInfo hit;
			bool exitPointFound = RayTraceGeometry(pCollision, pCollision->pt + (hitDirection * (distCheck + 0.035f)), -hitDirection * distCheck ,&hit);

			if (exitPointFound)
			{
				//Exit point found
				if(ShouldSpawnBackSideEffect(pHitTarget))
				{
					//Spawn effect
					IMaterialEffects* pMaterialEffects = g_pGame->GetIGameFramework()->GetIMaterialEffects();
					TMFXEffectId effectId = pMaterialEffects->GetEffectId(GetEntity()->GetClass(), pCollision->idmat[1]);
					if (effectId != InvalidEffectId)
					{
						SMFXRunTimeEffectParams params;
						params.src = GetEntityId();
						params.trg = pHitTarget ? pHitTarget->GetId() : 0;
						params.srcSurfaceId = pCollision->idmat[0];
						params.trgSurfaceId = pCollision->idmat[1]; 
						//params.soundSemantic = eSoundSemantic_Physics_Collision;
						params.srcRenderNode = (pCollision->iForeignData[0] == PHYS_FOREIGN_ID_STATIC) ? (IRenderNode*)pCollision->pForeignData[0] : NULL;
						params.trgRenderNode = (pCollision->iForeignData[1] == PHYS_FOREIGN_ID_STATIC) ? (IRenderNode*)pCollision->pForeignData[1] : NULL;
						params.pos = hit.pt;
						params.normal = hitDirection; //Use bullet direction, more readable for exits than normal
						params.partID = pCollision->partid[1];
						params.dir[0] = -hitDirection;
						params.playflags = eMFXPF_All&(~eMFXPF_Audio); //Do not play the sound on backface
						params.playflags &= ~eMFXPF_Decal; //We disable also decals, since hit.pt is not refined with render mesh
						params.fDecalPlacementTestMaxSize = pCollision->fDecalPlacementTestMaxSize;

						pMaterialEffects->ExecuteEffect(effectId, params);
					}
				}

#ifdef DEBUG_BULLET_PENETRATION
				if (debugBulletPenetration)
				{
					s_debugBulletPenetration.AddBulletHit(pCollision->pt, hitDirection, damageBeforePenetration, pierceabilityMat, false, false, false);
					s_debugBulletPenetration.AddBulletHit(hit.pt, hitDirection, GetDamageAfterPenetrationFallOff(), pierceabilityMat, true, false, false);
				}
#endif
			}
			else
			{
#ifdef DEBUG_BULLET_PENETRATION
				if (debugBulletPenetration)
				{
					s_debugBulletPenetration.AddBulletHit(pCollision->pt, hitDirection, damageBeforePenetration, pierceabilityMat, false, true, true);
				}
#endif
				//Surface must be too thick, add enough fall off to destroy the bullet
				m_accumulatedDamageFallOffAfterPenetration += (float)m_damage;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CBullet::ShouldSpawnBackSideEffect(IEntity* pHitTarget)
{
	if(m_backSideEffectsDisabled)
		return false;

	if(m_ownerIsPlayer && pHitTarget)
	{
		IActor* pTarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pHitTarget->GetId());
		if(pTarget)
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if (pGameRules && pGameRules->GetFriendlyFireRatio() <= 0.f)
			{		
				//if friendly fire is off don't spawn blood effect on teammates
				return !pTarget->IsFriendlyEntity(m_ownerId);
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CBullet::DestroyAtHitPosition(const Vec3& hitPosition)
{
	GetEntity()->SetPos(hitPosition);
	Destroy();
}

//////////////////////////////////////////////////////////////////////////
bool CBullet::ShouldDestroyBullet() const
{
	return (GetDamageAfterPenetrationFallOff() <= 0.0f);
}

//////////////////////////////////////////////////////////////////////////
void CBullet::ReInitFromPool()
{
	BaseClass::ReInitFromPool();

	m_damageCap = FLT_MAX;
	m_accumulatedDamageFallOffAfterPenetration = 0.0f;
	m_penetrationCount = 0;
	m_alive = true;
	m_hitActors.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CBullet::IsAlive() const
{
	return m_alive;
}

//////////////////////////////////////////////////////////////////////////
void CBullet::SetUpParticleParams(IEntity* pOwnerEntity, uint8 pierceabilityModifier)
{
	CRY_ASSERT(m_pPhysicalEntity);

	pe_params_particle pparams;
	pparams.pColliderToIgnore = pOwnerEntity ? pOwnerEntity->GetPhysics() : NULL;
	if (m_pAmmoParams)
	{
		pparams.iPierceability = max(0, min(m_pAmmoParams->pParticleParams->iPierceability + pierceabilityModifier, (int)sf_max_pierceable));
		m_bulletPierceability = pparams.iPierceability;
	}
	m_pPhysicalEntity->SetParams(&pparams);
}

//////////////////////////////////////////////////////////////////////////

bool CBullet::RayTraceGeometry( const EventPhysCollision* pCollision, const Vec3& pos, const Vec3& hitDirection, SBackHitInfo* pBackHitInfo )
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	bool exitPointFound = false;

	IPhysicalEntity* pCollider = pCollision->pEntity[1];
	assert(pCollider);

	pe_params_part partParams;
	partParams.partid = pCollision->partid[1];
	pe_status_pos posStatus;

	if (pCollider->GetParams(&partParams) && pCollider->GetStatus(&posStatus))
	{
		if (partParams.pPhysGeom && partParams.pPhysGeom->pGeom)
		{
			geom_world_data geomWorldData;
			geomWorldData.R = Matrix33(posStatus.q*partParams.q);
			geomWorldData.offset = posStatus.pos + (posStatus.q * partParams.pos);
			geomWorldData.scale = posStatus.scale * partParams.scale;

			geom_contact *pContacts;
			intersection_params intersectionParams;
			IGeometry* pRayGeometry = s_pRayWrapper->GetRay(pos, hitDirection);
			const Vec3 hitDirectionNormalized = hitDirection.GetNormalized();

			{ WriteLockCond lock; 
			const int contactCount = partParams.pPhysGeom->pGeom->IntersectLocked(pRayGeometry,&geomWorldData,0,&intersectionParams,pContacts,lock);
			if (contactCount > 0)
			{
				float bestDistance = 10.0f;
				
				for (int i = (contactCount-1); (i >= 0) && (pContacts[i].t < bestDistance) && ((pContacts[i].n*hitDirectionNormalized) < 0); i--)
				{
					bestDistance = (float)pContacts[i].t;
					pBackHitInfo->pt = pContacts[i].pt;
					exitPointFound = true;
				}
			}
			} // lock
		}
	}

	s_pRayWrapper->ResetRay();

	return exitPointFound;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_BULLET_PENETRATION

void SDebugBulletPenetration::AddBulletHit( const Vec3& hitPosition, const Vec3& hitDirection, float currentDamage, int8 surfacePierceability, bool isBackFace, bool stoppedBullet, bool tooThick )
{
	if (m_nextHit == MAX_DEBUG_BULLET_HITS)
	{
		m_nextHit = 0;
	}

	m_hitsList[m_nextHit].hitPosition = hitPosition;
	m_hitsList[m_nextHit].bulletDirection = hitDirection;
	m_hitsList[m_nextHit].damage = currentDamage;
	m_hitsList[m_nextHit].isBackFaceHit = isBackFace;
	m_hitsList[m_nextHit].stoppedBullet = stoppedBullet;
	m_hitsList[m_nextHit].tooThick = tooThick;
	m_hitsList[m_nextHit].surfacePierceability = surfacePierceability;
	m_hitsList[m_nextHit].lifeTime = (g_pGameCVars->g_bulletPenetrationDebugTimeout > 0.0f) ? g_pGameCVars->g_bulletPenetrationDebugTimeout : DEFAULT_DEBUG_BULLET_HIT_LIFETIME;

	m_nextHit++;
}

//////////////////////////////////////////////////////////////////////////
void SDebugBulletPenetration::Update(float frameTime)
{
	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetCullMode(e_CullModeNone); 
	pRenderAux->SetRenderFlags(newFlags);

	const float baseDebugTimeOut = (g_pGameCVars->g_bulletPenetrationDebugTimeout > 0.0f) ? g_pGameCVars->g_bulletPenetrationDebugTimeout : DEFAULT_DEBUG_BULLET_HIT_LIFETIME;

	for (int i = 0; i < MAX_DEBUG_BULLET_HITS; ++i)
	{
		SDebugBulletHit& currentHit = m_hitsList[i];

		if (currentHit.lifeTime <= 0.0f)
		{
			continue;
		}

		currentHit.lifeTime -= frameTime;

		//const float alpha = powf((currentHit.lifeTime / baseDebugTimeOut), 4.0f);
		// avoid powf whenever possible, for such simple cases, can do with 2 muls
		float alpha = (currentHit.lifeTime / baseDebugTimeOut);
		alpha *= alpha;
		alpha *= alpha;

		const ColorB red(255, 0, 0, (uint8)(192 * alpha)), green(0, 255, 0, (uint8)(192 * alpha));
		const ColorB& hitColor = currentHit.stoppedBullet ? red : green;
		const Vec3 coneBase = currentHit.isBackFaceHit ? (currentHit.hitPosition + (currentHit.bulletDirection * 0.3f)) : (currentHit.hitPosition - (currentHit.bulletDirection * 0.2f)) ;
		const Vec3 lineEnd = (coneBase - (currentHit.bulletDirection * 0.3f));
		pRenderAux->DrawCone(coneBase, currentHit.bulletDirection, 0.12f, 0.2f, hitColor);
		pRenderAux->DrawLine(coneBase, hitColor, lineEnd, hitColor, 3.0f);

		const Vec3 baseText = (currentHit.isBackFaceHit) ? coneBase + (0.2f * currentHit.bulletDirection)  : lineEnd - (0.3f * currentHit.bulletDirection);
		const Vec3 textLineOffset(0.0f, 0.0f, 0.14f);
		const float textColor[4] = {1.0f, 1.0f, 1.0f, alpha};

		IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * 2.0f), 1.25f, textColor, true, false, "Damage: %.1f", currentHit.damage);
		IRenderAuxText::DrawLabelExF(baseText - (textLineOffset * 3.0f), 1.25f, textColor, true, false, "Pierceability: %d", currentHit.surfacePierceability);
		IRenderAuxText::DrawLabelEx (baseText - (textLineOffset * 4.0f), 1.25f, textColor, true, false, GetPenetrationLevelByPierceability(currentHit.surfacePierceability));
		IRenderAuxText::DrawLabelEx (baseText - (textLineOffset * 5.0f), 1.25f, textColor, true, false, currentHit.tooThick ? "Too thick!" : "------");

	}

	pRenderAux->SetRenderFlags(oldFlags);
}

//////////////////////////////////////////////////////////////////////////
const char* SDebugBulletPenetration::GetPenetrationLevelByPierceability( int8 surfacePierceability ) const
{
	if (surfacePierceability == -1)
	{
		return "Terrain";
	}
	else if (surfacePierceability == 0)
	{
		return "Non pierceable";
	}
	else if (surfacePierceability < 4)
	{
		return "Level 1";
	}
	else if (surfacePierceability < 7)
	{
		return "Level 2";
	}
	else if (surfacePierceability < 10)
	{
		return "Level 3";
	}

	return "-----";
}

#endif
