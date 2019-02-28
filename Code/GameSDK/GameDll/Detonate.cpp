// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "Detonate.h"
#include "WeaponSystem.h"
#include "Item.h"
#include "Weapon.h"
#include "Actor.h"
#include "Projectile.h"
#include "Binocular.h"
#include "C4.h"

#include "WeaponSharedParams.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"


CRY_IMPLEMENT_GTI(CDetonate, CSingle);


//------------------------------------------------------------------------
CDetonate::CDetonate()
{
	m_canDetonate = false;
	m_detonationTimer = 0.0f;
}

//------------------------------------------------------------------------
CDetonate::~CDetonate()
{
}

//------------------------------------------------------------------------
struct CDetonate::ExplodeAction
{
	ExplodeAction(CDetonate *_detonate): pDetonate(_detonate) {};
	CDetonate *pDetonate;

	void execute(CItem *_this)
	{
		bool outOfAmmo = pDetonate->m_pWeapon->OutOfAmmo(false);

		if (outOfAmmo)
		{
			pDetonate->OutOfAmmoExplode();
		}
	}
};

//------------------------------------------------------------------------
struct CDetonate::DropRemoveAction
{
	DropRemoveAction(CDetonate *_detonate): pDetonate(_detonate) {};
	CDetonate *pDetonate;

	void execute(CItem *_this)
	{
		pDetonate->DropRemoveItem();
	}
};

//------------------------------------------------------------------------
void CDetonate::Update(float frameTime, uint32 frameId)
{
	BaseClass::Update(frameTime, frameId);

	if (m_detonationTimer>0.0f)
	{
		m_detonationTimer-=frameTime;

		if (m_detonationTimer<=0.0f)
		{
			m_detonationTimer=0.0f;

			CCCPOINT(DetonateFireMode_TimerHasReachedZero);
			bool detonated = Detonate();

			if (detonated && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->IsClient())
				m_pWeapon->GetScheduler()->TimerAction(uint32(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner)), CSchedulerAction<ExplodeAction>::Create(this), false);
		}
		else
			m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
void CDetonate::Activate(bool activate)
{
	BaseClass::Activate(activate);
	
	m_detonationTimer = 0.0f;
}

//------------------------------------------------------------------------
bool CDetonate::CanReload() const
{
	return false;
}

//------------------------------------------------------------------------
bool CDetonate::CanFire(bool considerAmmo) const
{
	bool detonateAllowed = false;

	if(m_detonationTimer<=0.0f)
	{
		if(gEnv->bServer)
		{
			if(IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
			{
				detonateAllowed = pFM->GetProjectileId() != 0;
			}
		}
		else
		{
			detonateAllowed = m_canDetonate;
		}
	}
	
	return detonateAllowed;
}

//------------------------------------------------------------------------
void CDetonate::StartFire()
{
	if (CanFire(false))
	{
		CActor *pOwner=m_pWeapon->GetOwnerActor();

		CCCPOINT(DetonateFireMode_StartFireOK);
		m_pWeapon->RequireUpdate(eIUS_FireMode);
		m_detonationTimer = 0.1f;
		m_pWeapon->PlayAction(GetFragmentIds().fire);
		m_pWeapon->RequestDetonate();
	}
	else
	{
#if !defined(_RELEASE)
		IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode());
		EntityId projectileId = pFM ? pFM->GetProjectileId() : 0;
		IEntity * projectile = gEnv->pEntitySystem->GetEntity(projectileId);

		CryLog ("[Detonate] Failure to detonate %s '%s' (timer = %.4f, can detonate = %s, fire mode = '%s') projectile = %u (%s '%s')",
						m_pWeapon->GetEntity()->GetClass()->GetName(),
						m_pWeapon->GetEntity()->GetName(),
						m_detonationTimer,
						m_canDetonate ? "TRUE" : "FALSE",
						pFM ? pFM->GetName() : "NONE",
						projectileId,
						projectile ? projectile->GetClass()->GetName() : "NONE",
						projectile ? projectile->GetName() : "N/A");
#endif

		CCCPOINT_IF(m_detonationTimer > 0.0f, DetonateFireMode_CannotFire_TimerNotReachedZero);
		CCCPOINT(DetonateFireMode_CannotFire);
	}
}

//------------------------------------------------------------------------
const char *CDetonate::GetCrosshair() const
{
	return "";
}

//------------------------------------------------------------------------
void CDetonate::OutOfAmmoExplode()
{
	CActor *pOwner = m_pWeapon->GetOwnerActor();

	if (!pOwner)
		return;

	if (gEnv->bMultiplayer)
	{
		pOwner->SelectLastItem(true, true);
	}
	else
	{
		m_pWeapon->PlayAction(GetFragmentIds().deselect);
		m_pWeapon->GetScheduler()->TimerAction(uint32(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner)), CSchedulerAction<DropRemoveAction>::Create(this), false);
	}
}

//------------------------------------------------------------------------
void CDetonate::DropRemoveItem()
{
	m_pWeapon->Drop(1.0f, true);
	m_pWeapon->RemoveEntity();
}

//------------------------------------------------------------------------
bool CDetonate::Detonate(bool net)
{
	bool detonatedAll = true;

	if (m_pWeapon->IsServer())
	{
		CActor *pOwner=m_pWeapon->GetOwnerActor();
		if (!pOwner)
			return false;

		if (IFireMode* pFM = m_pWeapon->GetFireMode(m_pWeapon->GetCurrentFireMode()))
		{
			std::vector<EntityId> undetonatedList;
			undetonatedList.clear();

			while(EntityId projectileId = pFM->RemoveProjectileId())
			{
				if (CProjectile *pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(projectileId))
				{
					if(pProjectile->Detonate())
					{
						CCCPOINT(DetonateFireMode_ProjectileHasBeenDetonated);
						g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, pProjectile->GetEntity()->GetClass()->GetName(), 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
					}
					else
					{
						CCCPOINT(DetonateFireMode_ProjectileFailedToDetonate);
						stl::push_back_unique(undetonatedList, projectileId);
					}
				}
			}

			while(!undetonatedList.empty())
			{
				pFM->SetProjectileId(undetonatedList.back());
				undetonatedList.pop_back();
				detonatedAll = false;
			}
		}
	}

	return detonatedAll;
}

//------------------------------------------------------------------------
void CDetonate::NetShoot(const Vec3 &hit, int ph)
{
	Detonate(true);	
}

void CDetonate::SetCanDetonate( bool canDet )
{
	m_canDetonate = canDet;
}
