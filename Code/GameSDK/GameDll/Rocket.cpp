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
#include "Rocket.h"
#include "Game.h"
#include "Bullet.h"
#include "GameRules.h"
#include "Actor.h"

//------------------------------------------------------------------------
CRocket::CRocket()
: m_launchLoc(0,0,0)
,	m_detonatorFired(false)
{
}

//------------------------------------------------------------------------
CRocket::~CRocket()
{
	AutoDropOwnerWeapon();
}

//------------------------------------------------------------------------
void CRocket::HandleEvent(const SGameObjectEvent &event)
{
	if (CheckAnyProjectileFlags(ePFlag_destroying))
		return;

	CProjectile::HandleEvent(event);

	if ((!gEnv->bServer && m_pAmmoParams->serverSpawn) || IsDemoPlayback())
		return;

	if (event.event == eGFE_OnCollision && !m_detonatorFired)
	{		
		if (EventPhysCollision *pCollision = reinterpret_cast<EventPhysCollision*>(event.ptr))
		{
			if (m_pAmmoParams->safeExplosion>0.0f)
			{
				const Vec3 referencePosition = pCollision->pt;
				const float dp2 = (m_launchLoc - referencePosition).len2();
				if (dp2 <= (m_pAmmoParams->safeExplosion * m_pAmmoParams->safeExplosion))
				{
					AutoDropOwnerWeapon();
					m_detonatorFired = true;
					DisableTrail();

					ProcessFailedDetonation();
					return;
				}
			}

			if (pCollision->pEntity[0]->GetType()==PE_PARTICLE)
			{
				float bouncy, friction;
				uint32	pierceabilityMat;
				gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
				pierceabilityMat &= sf_pierceable_mask;

				pe_params_particle params;
				
				if(pCollision->pEntity[0]->GetParams(&params)==0)
					SetDefaultParticleParams(&params);
				
				if( (params.velocity>1.0f) && (s_materialLookup.IsMaterial( pCollision->idmat[1] , CProjectile::SMaterialLookUp::eType_Water ) == false) 
					&& (!pCollision->pEntity[1] || (pCollision->pEntity[1]->GetType() != PE_LIVING && pCollision->pEntity[1]->GetType() != PE_ARTICULATED)))
				{
					if((int)pierceabilityMat > params.iPierceability)
						return;
				}
			}

			AutoDropOwnerWeapon();
			m_detonatorFired = true;

		  IEntity* pTarget = pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1] : 0;

		  CProjectile::SExplodeDesc explodeDesc(true);
		  explodeDesc.impact = true;
		  explodeDesc.pos = pCollision->pt;
		  explodeDesc.normal = pCollision->n;
		  explodeDesc.vel = pCollision->vloc[0];
		  explodeDesc.targetId = pTarget?pTarget->GetId():0;
		  Explode(explodeDesc);
	  }
  }
}

//------------------------------------------------------------------------
void CRocket::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);

	m_launchLoc=pos;

	if(CWeapon* pWeapon = GetWeapon())
	{
		if(pWeapon->IsAutoDroppable())
			pWeapon->AddFiredRocket();
	}
}



void CRocket::AutoDropOwnerWeapon()
{
	CWeapon* pWeapon = GetWeapon();
	if(!m_detonatorFired && pWeapon)
	{
		if(pWeapon->IsAutoDroppable())
			pWeapon->AutoDrop();
	}
}



void CRocket::EnableTrail()
{
	TrailEffect(true);
	TrailSound(true);

	bool isAccThrustAllowed = true;
	if(!gEnv->bMultiplayer)
	{
		// If it's single player and AI then we don't allow to enable the trail since
		// it will make the shot diverging from the prediction
		const CActor* pOwnerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));
		isAccThrustAllowed = pOwnerActor && pOwnerActor->IsPlayer();
	}

	if(isAccThrustAllowed)
	{
		pe_params_particle params;
		params.accThrust = m_pAmmoParams->pParticleParams->accThrust;
		GetEntity()->GetPhysics()->SetParams(&params);
	}
}



void CRocket::DisableTrail()
{
	TrailEffect(false);
	TrailSound(false);

	pe_params_particle params;
	params.accThrust = 0.0;
	GetEntity()->GetPhysics()->SetParams(&params);
}



bool CRocket::ShouldCollisionsDamageTarget() const
{
	return !m_detonatorFired;
}



void CRocket::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_TIMER && event.nParam[0] == ePTIMER_LIFETIME)
	{
		if (!m_detonatorFired)
			Explode(true);
		else
			Destroy();
	}
	else
	{
		return CProjectile::ProcessEvent(event);
	}
}
