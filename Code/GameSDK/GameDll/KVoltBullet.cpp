// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: KVolt bullet

-------------------------------------------------------------------------
History:
- 13:05:2009   15:00 : Created by Claire Allan

***********************************************************************/
#include "StdAfx.h"
#include "KVoltBullet.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "Actor.h"
#include "GameRules.h"
#include "Player.h"
#include "WeaponSystem.h"
#include "ItemResourceCache.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

//------------------------------------------------------------------------
CKVoltBullet::CKVoltBullet()
{  
}


//------------------------------------------------------------------------
CKVoltBullet::~CKVoltBullet()
{
	Destroy();
}

//------------------------------------------------------------------------
bool CKVoltBullet::Init(IGameObject *pGameObject)
{
	if(inherited::Init(pGameObject))
	{
		return (m_pAmmoParams->pKvoltParams != NULL);
	}

	return false;
}

//------------------------------------------------------------------------
void CKVoltBullet::HandleEvent(const SGameObjectEvent& event)
{
	if(event.event == eGFE_OnCollision)
	{
		EventPhysCollision *pCollision = (EventPhysCollision* )event.ptr;
		m_last = pCollision->pt;

		// switch collision target id's to ensure target is not the bullet itself
		int trgId = 1;
		int srcId = 0;
		IPhysicalEntity *pTarget = pCollision->pEntity[trgId];

		if (pTarget == GetEntity()->GetPhysics())
		{
			trgId = 0;
			srcId = 1;
			pTarget = pCollision->pEntity[trgId];
		}

		//Do not stick to breakable glass
		if(ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(pCollision->idmat[trgId]))
		{
			if(pSurfaceType->GetBreakability()==1)
			{
				return;
			}
		}

		Trigger(pCollision->pt);
		
		if(!IsRemote())
		{
			IEntity *pTargetEntity = pTarget ? gEnv->pEntitySystem->GetEntityFromPhysics(pTarget) : 0;
			EntityId targetId =  pTargetEntity ? pTargetEntity->GetId() : 0;
			Vec3 dir(0, 0, 0);
			
			if (pCollision->vloc[srcId].GetLengthSquared() > 1e-6f)
			{
				dir = pCollision->vloc[srcId].GetNormalized();
			}

			DamageEnemiesInRange(m_pAmmoParams->pKvoltParams->damageRadius, pCollision->pt, targetId);
			
			if(gEnv->bMultiplayer)
			{
				CGameRules * pGameRules = g_pGame->GetGameRules();
				int nOwnerTeam = pGameRules->GetTeam(m_ownerId), nTargetTeam = pGameRules->GetTeam(targetId);

				if(nOwnerTeam == 0 || nTargetTeam != nOwnerTeam)
				{
					ProcessDamage(targetId, m_damage * m_pAmmoParams->pKvoltParams->directHitDamageMultiplier, pCollision->partid[trgId], pCollision->pt, dir);
				}
			}
			else
			{
				CActor* pOwner = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));

				if(!pOwner || !pOwner->IsFriendlyEntity(targetId, false))
				{
					ProcessDamage(targetId, m_damage * m_pAmmoParams->pKvoltParams->directHitDamageMultiplier, pCollision->partid[trgId], pCollision->pt, dir);
				}
			}			
		}

		if (!gEnv->bMultiplayer && GetAmmoParams().pElectricProjectileParams)
		{
			SElectricHitTarget target(GetEntity()->GetPhysics(), pCollision);
			ProcessElectricHit(target);
		}
	}

	inherited::HandleEvent(event);
}

//-------------------------------------------------------------------------
void CKVoltBullet::Trigger(Vec3 collisionPos)
{
	CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));
	bool friendly = pActor != NULL && pActor->IsFriendlyEntity(g_pGame->GetIGameFramework()->GetClientActorId());

	CItemParticleEffectCache& particleCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache();

	IParticleEffect* pHitEffect = particleCache.GetCachedParticle(friendly ? m_pAmmoParams->pKvoltParams->friendlyEffect.c_str() : m_pAmmoParams->pKvoltParams->enemyEffect.c_str());

	if(pHitEffect)
	{
		CCCPOINT(KVolt_SpawnDestroyParticle);
		pHitEffect->Spawn(true, IParticleEffect::ParticleLoc(collisionPos));
	}

	//Update entity position before destroy for bullet threat trails
	GetEntity()->SetPos(collisionPos);	
	Destroy();
}

//-----------------------------------------------------------------------
void CKVoltBullet::DamageEnemiesInRange(float range, Vec3 pos, EntityId ignoreId)
{
	if(range == 0.0f)	//This is the current value in MP.
		return;

	TActorIds actorIds;
	GetActorsInArea(actorIds, range, pos);

	for (int i = 0, size = actorIds.size(); i < size; ++i)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(actorIds[i]);

		if (pEntity)
		{
			CCCPOINT(KVolt_DamageEnemy);

			EntityId targetId = pEntity->GetId();

			if(targetId != ignoreId)
			{
				CActor* pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(targetId));

				//We don't want the kvolt killing people inside vehicles in MP
				if (pActor != NULL && !pActor->IsDead() && !pActor->IsFriendlyEntity(m_ownerId) && (!gEnv->bMultiplayer || !pActor->GetLinkedVehicle()))
				{
					Vec3 entityPos = pEntity->GetWorldPos();
					Vec3 dir = entityPos - m_initial_pos;

					ProcessDamage(targetId, (float)m_damage, -1, entityPos, dir);
				}
			}
		}
	}
}

void CKVoltBullet::ProcessDamage(EntityId targetId, float damage, int hitPartId, Vec3 pos, Vec3 dir)
{
	if(targetId)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();

		HitInfo hitInfo(m_ownerId, targetId, m_weaponId, damage, 0.0f, 0, hitPartId, m_hitTypeId, pos, dir, -dir);

		hitInfo.remote = IsRemote(); 
		hitInfo.projectileId = GetEntityId();
		hitInfo.bulletType = m_pAmmoParams->bulletType;

		pGameRules->ClientHit(hitInfo);

		ReportHit(targetId);
	}
}