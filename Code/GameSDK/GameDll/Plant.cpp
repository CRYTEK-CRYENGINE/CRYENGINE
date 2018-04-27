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
#include "Plant.h"
#include "Item.h"
#include "Weapon.h"
#include "Projectile.h"
#include "Player.h"
#include "Actor.h"
#include "Game.h"
#include "C4.h"

#include "Single.h"
#include "WeaponSystem.h"
#include "FireModeParams.h"

#include "PlayerStateEvents.h"

CRY_IMPLEMENT_GTI(CPlant, CFireMode);


//------------------------------------------------------------------------
struct CPlant::MidPlantAction
{
	MidPlantAction(CPlant *_plant): pPlant(_plant) {};
	CPlant *pPlant;

	void execute(CItem *_this)
	{
		CActor* pActor = pPlant->m_pWeapon->GetOwnerActor();
		
		if(!gEnv->bMultiplayer || (pActor && pActor->IsClient()))
		{
			Vec3 pos;
			Vec3 dir(FORWARD_DIRECTION);
			Vec3 vel(0,0,0);

			if(pPlant->GetPlantingParameters(pos, dir, vel))
				pPlant->Plant(pos, dir, vel);
		}

		pPlant->m_pWeapon->HideItem(true);
		pPlant->m_pWeapon->RequireUpdate(eIUS_FireMode);			
	}
};

//------------------------------------------------------------------------
struct CPlant::EndPlantAction
{
	EndPlantAction(CPlant *_plant): pPlant(_plant) {};
	CPlant *pPlant;

	void execute(CItem *_this)
	{
		pPlant->m_pWeapon->SetBusy(false);
		pPlant->m_planting = false;

		pPlant->CheckAmmo();

		if(!pPlant->OutOfAmmo())
		{
			pPlant->m_pWeapon->PlayAction(pPlant->GetFragmentIds().fromPlant, 0,false,CItem::eIPAF_Default);
		}
	}
};

//------------------------------------------------------------------------
struct CPlant::StartPlantAction
{
	StartPlantAction(CPlant *_plant): pPlant(_plant) {};
	CPlant *pPlant;

	void execute(CItem *_this)
	{
		pPlant->m_pWeapon->PlayAction(_this->GetFragmentIds().plant, 0,false,CItem::eIPAF_Default);

		uint32 animLength = pPlant->m_pWeapon->GetCurrentAnimationTime(eIGS_Owner);

		pPlant->m_pWeapon->GetScheduler()->TimerAction(uint32(pPlant->m_fireParams->plantparams.delay*1000.f), CSchedulerAction<MidPlantAction>::Create(pPlant), false);
		pPlant->m_pWeapon->GetScheduler()->TimerAction(animLength, CSchedulerAction<EndPlantAction>::Create(pPlant), false);
	}
};

//------------------------------------------------------------------------
CPlant::CPlant() : m_planting(false), m_pressing(false)
{
}

//------------------------------------------------------------------------
CPlant::~CPlant()
{
}

//-----------------------------------------------------------------------
void CPlant::PostInit()
{
	CacheAmmoGeometry();
}

//-----------------------------------------------------------------------
void CPlant::CacheAmmoGeometry()
{
	if(m_fireParams->plantparams.ammo_type_class)
	{
		if(const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(m_fireParams->plantparams.ammo_type_class))
		{
			pAmmoParams->CacheResources();
		}
	}
}

//------------------------------------------------------------------------
void CPlant::UpdateFPView(float frameTime)
{
}

//------------------------------------------------------------------------
void CPlant::Activate(bool activate)
{
	BaseClass::Activate(activate);

	m_planting = false;
	m_pressing = false;

	if(activate && m_pWeapon->GetOwnerActor())
	{
		CheckAmmo();
	}
}

//------------------------------------------------------------------------
bool CPlant::OutOfAmmo() const
{
	if (m_fireParams->plantparams.clip_size!=0)
		return m_fireParams->plantparams.ammo_type_class && m_fireParams->plantparams.clip_size != -1 && m_pWeapon->GetAmmoCount(m_fireParams->plantparams.ammo_type_class)<1;

	return m_fireParams->plantparams.ammo_type_class && m_pWeapon->GetInventoryAmmoCount(m_fireParams->plantparams.ammo_type_class)<1;
}

//------------------------------------------------------------------------
bool CPlant::LowAmmo(float thresholdPerCent) const
{
	int clipSize = m_fireParams->plantparams.clip_size;
	if (clipSize!=0)
		return m_fireParams->plantparams.ammo_type_class && clipSize != -1 && m_pWeapon->GetAmmoCount(m_fireParams->plantparams.ammo_type_class) < (int)(thresholdPerCent*clipSize);

	return m_fireParams->plantparams.ammo_type_class && (m_pWeapon->GetInventoryAmmoCount(m_fireParams->plantparams.ammo_type_class) < (int)(thresholdPerCent*clipSize));
}


//------------------------------------------------------------------------
void CPlant::NetShoot(const Vec3 &hit, int ph)
{
	NetShootEx(ZERO, ZERO, ZERO, hit, 1.0f, ph);
}

//------------------------------------------------------------------------
void CPlant::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
	Plant(pos, dir, vel, true, ph);
}

//------------------------------------------------------------------------
void CPlant::NetStartFire()
{
	StartFire();
}

//------------------------------------------------------------------------
void CPlant::NetStopFire()
{
	StopFire();
}

//------------------------------------------------------------------------
IEntityClass* CPlant::GetAmmoType() const
{
	return m_fireParams->plantparams.ammo_type_class;
}

//------------------------------------------------------------------------
int CPlant::GetDamage() const
{
	return m_fireParams->plantparams.damage;
}

//------------------------------------------------------------------------
bool CPlant::GetPlantingParameters(Vec3& pos, Vec3& dir, Vec3& vel) const
{
	// returns true if firing is allowed
	const float fBackwardAdjustment = 0.4f; //To ensure we throw from within our capsule - avoid throwing through walls

	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		IMovementController *pMC = pActor->GetMovementController();
		SMovementState info;
		if (pMC)
			pMC->GetMovementState(info);

		pos = m_pWeapon->GetWorldPos();

		if (pMC)
			dir = info.eyeDirection;
		else
			dir = pActor->GetEntity()->GetWorldRotation().GetColumn1();

		if(gEnv->bMultiplayer)
		{
			pos -= dir * fBackwardAdjustment;
		}

		if (IPhysicalEntity *pPE = pActor->GetEntity()->GetPhysics())
		{
			pe_status_dynamics sv;
			if (pPE->GetStatus(&sv))
			{
				if (sv.v.len2()>0.01f)
				{
					float dot=sv.v.GetNormalized().Dot(dir);
					if (dot<0.0f)
						dot=0.0f;
					vel=sv.v*dot;
				}
			}
		}

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
bool CPlant::CanFire(bool considerAmmo/* =true */) const
{
	return !m_planting && !m_pressing && !m_pWeapon->IsBusy() && (!considerAmmo || !OutOfAmmo());
}

//------------------------------------------------------------------------
void CPlant::StartFire()
{
	CActor* pOwner = m_pWeapon->GetOwnerActor();

	if (pOwner && pOwner->IsClient() && !CanFire(true))
	{
		return;
	}

	CPlayer *ownerPlayer = m_pWeapon->GetOwnerPlayer();
	if (ownerPlayer)
	{
		ownerPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);

	m_planting = true;
	m_pWeapon->SetBusy(true);
	m_pWeapon->PlayAction(GetFragmentIds().intoPlant, 0,false,CItem::eIPAF_Default);
	
	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner), CSchedulerAction<StartPlantAction>::Create(this), false);

	m_pWeapon->RequestStartFire();
}

//------------------------------------------------------------------------
void CPlant::StopFire()
{
	m_pressing=false;

	m_pWeapon->RequestStopFire();
}

//------------------------------------------------------------------------
bool CPlant::IsSilenced() const
{
	return m_fireParams->plantparams.is_silenced;
}

//------------------------------------------------------------------------
void CPlant::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget()!=eST_Network)
	{
		ser.Value("projectiles", m_projectiles);
	}
}

//------------------------------------------------------------------------
void CPlant::Plant(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, bool net, int ph)
{
	IEntityClass* ammo = m_fireParams->plantparams.ammo_type_class;
	int weaponAmmoCount=m_pWeapon->GetAmmoCount(ammo);
	int inventoryAmmoCount=m_pWeapon->GetInventoryAmmoCount(ammo);

	int ammoCount = weaponAmmoCount;
	if (m_fireParams->plantparams.clip_size==0)
		ammoCount = inventoryAmmoCount; 

	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(ammo, net);
	CActor *pActor=m_pWeapon->GetOwnerActor();
	if (pAmmo)
	{
		pAmmo->SetParams(CProjectile::SProjectileDesc(m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_fireParams->plantparams.damage, 0.f, 0.f, 0.f, 0, 0, false));
		//pAmmo->SetDestination(m_pWeapon->GetDestination());
		
		pAmmo->Launch(pos, dir, vel);
		
		EntityId projectileId = pAmmo->GetEntity()->GetId();

		if (projectileId && !m_fireParams->plantparams.simple && m_pWeapon->IsServer())
		{
			if (pActor)
			{
				SetProjectileId(projectileId);
			}
		}
	}

	ammoCount--;
	if (m_fireParams->plantparams.clip_size != -1)
	{
		if (m_fireParams->plantparams.clip_size!=0)
			m_pWeapon->SetAmmoCount(ammo, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
	}

	if (pAmmo && ph && pActor)
	{
		pAmmo->GetGameObject()->RegisterAsValidated(pActor->GetGameObject(), ph);
		pAmmo->GetGameObject()->BindToNetwork();
	}
	else if (pAmmo && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer && pActor && pActor->IsClient())
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	OnShoot(m_pWeapon->GetOwnerId(), pAmmo?pAmmo->GetEntity()->GetId():0, GetAmmoType(), pos, dir, vel);

	if (!net)
		m_pWeapon->RequestShoot(ammo, pos, dir, vel, ZERO, 1.0f, pAmmo? pAmmo->GetGameObject()->GetPredictionHandle() : 0, true);

}

//========================================================================
void CPlant::SetProjectileId(EntityId id)
{
	if(id)
	{
		bool changed = m_projectiles.empty();

		stl::push_back_unique(m_projectiles,id);

		if(changed)
		{
			CHANGED_NETWORK_STATE(m_pWeapon, CC4::ASPECT_DETONATE);
		}
	}
}

//====================================================================
EntityId CPlant::GetProjectileId() const
{
	if(!m_projectiles.empty())
		return m_projectiles.back();

	return 0;
}
//=====================================================================
EntityId CPlant::RemoveProjectileId() 
{
	EntityId id = 0;
	if(!m_projectiles.empty())
	{
		id= m_projectiles.back();
		m_projectiles.pop_back();

		if(m_projectiles.empty())
		{
			CHANGED_NETWORK_STATE(m_pWeapon, CC4::ASPECT_DETONATE);
		}
	}
	return id;
}

//------------------------------------------------------------------------
int CPlant::GetNumProjectiles() const
{
	return m_projectiles.size();
}

//------------------------------------------------------------------------
void CPlant::CheckAmmo()
{
	if(m_pWeapon->GetOwnerActor())
	{
		bool noAmmo = OutOfAmmo();
		m_pWeapon->HideItem(noAmmo);
	}
}

//------------------------------------------------------------------------
void CPlant::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddContainer(m_projectiles);	
	BaseClass::GetInternalMemoryUsage(s);		// collect memory of parent class
}

//------------------------------------------------------------------------
void CPlant::OnZoomStateChanged()
{

}

int CPlant::GetAmmoCount() const
{
	return m_pWeapon->GetAmmoCount(m_fireParams->plantparams.ammo_type_class);
}

int CPlant::GetClipSize() const
{
	return m_fireParams->plantparams.clip_size;
}
