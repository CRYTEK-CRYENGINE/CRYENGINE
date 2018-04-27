// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins
-  18:07:2008		Slightly Refactored (cleaned-up): Benito G.R.

*************************************************************************/
#include "StdAfx.h"
#include "Throw.h"
#include "Actor.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "Game.h"
#include "Projectile.h"
#include "WeaponSystem.h"
#include "GameRules.h"
#include "Battlechatter.h"
#include "IInteractor.h"

#include "WeaponSharedParams.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "PlayerAnimation.h"


#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDUtils.h"

#define MAX_TRAJECTORY_TIME 5.0f


CRY_IMPLEMENT_GTI(CThrow, CSingle);


static Vec3 s_trajectoryPart[MAX_TRAJECTORY_SAMPLES];


//------------------------------------------------------------------------
CThrow::CThrowFiringLocator::CThrowFiringLocator()
: m_eyeDir(ZERO)
, m_eyePos(ZERO)
{

}


void CThrow::CThrowFiringLocator::SetValues(const Vec3& pos, const Vec3& dir)
{
	m_eyePos = pos;
	m_eyeDir = dir;
}


bool CThrow::CThrowFiringLocator::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	const Vec3 cross = m_eyeDir.cross(Vec3(0.0f, 0.0f, 1.0f));
	pos = m_eyePos + cross*1.0f;
	return true;
}

bool CThrow::CThrowFiringLocator::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	dir = m_eyeDir;
	return true;
}

//------------------------------------------------------------------------
CThrow::CThrow()
: m_predictedPos(ZERO),
	m_predictedDir(ZERO),
	m_predictedHit(ZERO),
	m_predicted(false),
	m_primed(false),
	m_throwing(false),
	m_primeTime(0.0f),
	m_projectileLifeTime(0.0f),
	m_firedTime(-1.0f),
	m_startFireTime(0.0f),
	m_explodeTime(0.0f),
	m_trajectoryLength(0)
{
}

//------------------------------------------------------------------------
CThrow::~CThrow()
{
	m_pWeapon->SetFiringLocator(NULL);
}

//------------------------------------------------------------------
void CThrow::SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams)
{
	SetProjectileSpeedScale(launchParams.fSpeedScale);
}

//------------------------------------------------------------------------
void CThrow::Activate(bool activate)
{
	m_primed = false;
	m_primeTime = m_startFireTime = 0.0f;
	m_firing = false;
	m_throwing = false;
	m_predicted = false;
	
	m_pWeapon->SetFiringLocator(&m_throwFiringLocator);

	if(activate)
	{
		CheckAmmo();

		if(const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(GetAmmoType()))
		{
			m_projectileLifeTime = pAmmoParams->lifetime;
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
	else
	{

		CActor* pOwner = m_pWeapon->GetOwnerActor();

		if(pOwner && pOwner->IsClient() && pOwner->IsPlayer())
		{
			static_cast<CPlayer*>(pOwner)->UnlockInteractor(m_pWeapon->GetEntityId());
		}
	}

	BaseClass::Activate(activate);
}

//------------------------------------------------------------------------
void CThrow::Update(float frameTime, uint32 frameId)
{
	BaseClass::Update(frameTime, frameId);

	CActor* pActor = m_pWeapon->GetOwnerActor();
	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);
		m_throwFiringLocator.SetValues(info.eyePosition, info.eyeDirection);
	}

	if(gEnv->bServer && pActor && pActor->IsPlayer() && m_primed && IsReadyToThrow() && m_fireParams->throwparams.prime_timer && (m_projectileLifeTime > 0.0f))
	{
		float primedTime = gEnv->pTimer->GetAsyncCurTime() - m_primeTime;
		if(primedTime >= m_projectileLifeTime)
		{
			CCCPOINT(Throw_AutomaticThrow);
			m_firing = false;
			m_primed = false;

			const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(m_fireParams->fireparams.ammo_type_class);
			
			if(pAmmoParams)
			{
				const SExplosionParams* pExplosionParams = pAmmoParams->pExplosion;
				if (pExplosionParams)
				{
					EntityId ownerId = pActor->GetEntityId();
					EntityId weaponId = m_pWeapon->GetEntityId();
					CGameRules *pGameRules = g_pGame->GetGameRules();
					Vec3 pos = m_pWeapon->GetWorldPos();
					float fDamage = (float) m_fireParams->fireparams.damage;

					uint16 projectileNetClassId = 0;

					if(g_pGame->GetIGameFramework()->GetNetworkSafeClassId(projectileNetClassId, m_fireParams->fireparams.ammo_type_class->GetName()))
					{
						SProjectileExplosionParams pep(
							ownerId,
							weaponId,
							0,
							0,
							pos,
							-FORWARD_DIRECTION,
							FORWARD_DIRECTION,
							ZERO,
							fDamage,
							projectileNetClassId,
							false,
							false);

						pGameRules->ProjectileExplosion(pep);
					}
		
					CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
					CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

					//apply a hit to the owner as well to ensure they are always killed
					HitInfo hitInfo(ownerId, ownerId, m_pWeapon->GetEntityId(), fDamage, 0.0f, 0, -1, m_fireParams->fireparams.hitTypeId, pos, ZERO, ZERO);

					hitInfo.remote = false;
					hitInfo.bulletType = -1;

					pGameRules->ClientHit(hitInfo);

					SHUDEvent hudEvent(eHUDEvent_OnGrenadeThrow);
					hudEvent.AddData(false);
					CHUDEventDispatcher::CallEvent(hudEvent);
				}
			}

			//Decrease ammo count too
			if (m_fireParams->fireparams.clip_size != -1)
			{
				IEntityClass* pAmmoClass = m_fireParams->fireparams.ammo_type_class;

				const int weaponAmmoCount = m_pWeapon->GetAmmoCount(pAmmoClass);
				const int inventoryAmmoCount = m_pWeapon->GetInventoryAmmoCount(pAmmoClass);

				m_pWeapon->SetAmmoCount(pAmmoClass, (inventoryAmmoCount > 0) ? weaponAmmoCount : weaponAmmoCount - 1);
				m_pWeapon->SetInventoryAmmoCount(pAmmoClass, (inventoryAmmoCount > 0) ? inventoryAmmoCount -1 : inventoryAmmoCount);
			}

			bool bRevertToLastItem = gEnv->bMultiplayer || CheckAmmo();
			if(pActor && pActor->IsClient() && bRevertToLastItem)
			{
				CCCPOINT(Throw_SwitchBack);
				pActor->SelectLastItem(true, true);
			}
			else
			{
				m_pWeapon->PlayAction(GetFragmentIds().Select);
			}
		}

		if(g_pGameCVars->i_debug_projectiles > 0)
		{
			const Vec3 helperPos = m_pWeapon->GetSlotHelperPos(m_pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson, "grenade_term"/*m_fireParams->fireparams.helper->c_str()*/, true);
			IRenderAuxText::DrawLabelF(helperPos, 3.0f, "%.2f", (m_projectileLifeTime - primedTime));
		}

		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}

	m_predicted = false;

	if(g_pGameCVars->i_grenade_showTrajectory && m_fireParams->throwparams.display_trajectory && pActor && pActor->IsClient() && m_pWeapon->IsSelected())
	{
		if(m_firedTime>=0.0f)
		{
			const float timeSinceFire = (gEnv->pTimer->GetAsyncCurTime() - m_firedTime);
			const float timeLeft = (m_projectileLifeTime - (m_firedTime - m_primeTime));
			if(timeSinceFire>timeLeft)
			{
				m_firedTime = -1.0f;
			}
		}
		CalculateTrajectory();
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//----------------------------------------------------------------------
struct CThrow::PrimeAction
{
	PrimeAction(CThrow* _pThrow) { pThrow = _pThrow; }
	CThrow* pThrow;

	void execute(CItem* _this)
	{
		if(!pThrow->m_primed && pThrow->m_firing)
		{
			pThrow->m_primed = true;
			pThrow->m_primeTime = gEnv->pTimer->GetAsyncCurTime();
			_this->PlayAction(pThrow->GetFragmentIds().primed_loop);
		}
	}
};

//----------------------------------------------------------------------
struct CThrow::HoldAction
{
	HoldAction(CThrow* _pThrow) { pThrow = _pThrow; }
	CThrow* pThrow;

	void execute(CItem* _this)
	{
		if(!pThrow->m_throwing && pThrow->m_firing)
		{
			_this->PlayAction(pThrow->GetFragmentIds().hold, 0, true);
		}
	}
};

void CThrow::Prime()
{
	if(!m_fireParams->throwparams.prime_enabled)
		return;

	if(!IsReadyToFire())
	{
		CPlayer *ownerPlayer = m_pWeapon->GetOwnerPlayer();
		if (ownerPlayer)
		{
			ownerPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
		}

		return;
	}

	if(!m_primed)
	{
		if(!IsReadyToThrow())
		{
			CCCPOINT(Throw_Prime);
			StartFire();
		}
	}
}

//------------------------------------------------------------------------
bool CThrow::CanReload() const
{
	return BaseClass::CanReload();
}

//------------------------------------------------------------------------
bool CThrow::IsReadyToFire() const
{
	CPlayer *ownerPlayer = m_pWeapon->GetOwnerPlayer();
	return CanFire(true) && !m_firing && !m_throwing && (!ownerPlayer || !ownerPlayer->IsSliding());
}

//------------------------------------------------------------------------
void CThrow::StartFireInternal()
{
	CCCPOINT(Throw_Start);

	m_firing = true;
	m_throwing = false;
	m_primed = false;
	m_startFireTime = gEnv->pTimer->GetAsyncCurTime();

	CActor *pOwner = m_pWeapon->GetOwnerActor();

	if (m_fireParams->throwparams.prime_enabled)
	{
		m_pWeapon->PlayAction(GetFragmentIds().prime);
		m_pWeapon->GetScheduler()->TimerAction((uint32)(m_fireParams->throwparams.prime_delay * 1000.0f), CSchedulerAction<PrimeAction>::Create(this), false);
		m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner) - 100, CSchedulerAction<HoldAction>::Create(this), false);

		if (m_fireParams->throwparams.crosshairblink_enabled && pOwner && pOwner->IsClient())
		{
			float fGrenadeLifeTime = -1.f;
			const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(GetAmmoType());
			if(pAmmoParams)
			{
				fGrenadeLifeTime = pAmmoParams->lifetime;
			}
			SHUDEvent hudEvent(eHUDEvent_OnGrenadeThrow);
			hudEvent.AddData(true);//Grenade thrown
			hudEvent.AddData(m_projectileLifeTime);
			CHUDEventDispatcher::CallEvent(hudEvent);
		}
	}
	else
	{
		m_pWeapon->PlayAction(GetFragmentIds().pre_fire);
		m_pWeapon->PlayAction(GetFragmentIds().hold);
	}		

	if(pOwner)
	{
		pOwner->SetTag(PlayerMannequin.tagIDs.throwing, true);

		if(pOwner->IsClient())
		{
			pOwner->LockInteractor(m_pWeapon->GetEntityId(), true);
		}
	}
		
	m_pWeapon->RequestStartFire();

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CThrow::StartFire()
{
	if (IsReadyToFire())
	{
		StartFireInternal();
	}
	else if (!CanFire(true))
	{ 
		// reload if possible.
		if( CanReload() )
		{
			//Auto reload
			m_pWeapon->Reload();
		}
		return;
	}
}

//------------------------------------------------------------------------
void CThrow::StopFire()
{
	if (IsReadyToThrow())
	{
		CCCPOINT(Throw_Stop);
		DoThrow();

		m_pWeapon->RequestStopFire();

		m_pWeapon->RequireUpdate(eIUS_FireMode);

		if (m_fireParams->throwparams.crosshairblink_enabled)
		{
			SHUDEvent hudEvent(eHUDEvent_OnGrenadeThrow);
			hudEvent.AddData(false);
			CHUDEventDispatcher::CallEvent(hudEvent);
		}

		CActor* pOwner = m_pWeapon->GetOwnerActor();

		if(pOwner && pOwner->IsClient() && pOwner->IsPlayer())
		{
			static_cast<CPlayer*>(pOwner)->UnlockInteractor(m_pWeapon->GetEntityId());
		}
	}
}

//------------------------------------------------------------------------
void CThrow::NetStartFire()
{
	CCCPOINT(Throw_NetStart);
	StartFireInternal();
}

//------------------------------------------------------------------------
void CThrow::NetStopFire()
{
	if (IsReadyToThrow())
	{
		CCCPOINT(Throw_NetStop);
		DoThrow();
		m_pWeapon->RequireUpdate(eIUS_FireMode);
	}
}

//------------------------------------------------------------------------
bool CThrow::CheckAmmo()
{
	bool hide = false;
	const int clipSize = m_fireParams->fireparams.clip_size;
	IEntityClass* pAmmoClass = m_fireParams->fireparams.ammo_type_class;

	if (m_pWeapon->GetOwner() && clipSize >= 0)
	{
		const int inventoryAmmoCount = m_pWeapon->GetInventoryAmmoCount(pAmmoClass);
		const int weaponAmmoCount = m_pWeapon->GetAmmoCount(pAmmoClass);

		if (clipSize > 0)
		{
			hide = ((weaponAmmoCount + inventoryAmmoCount) <= 0);

			//Refresh in clip ammo if needed
			if (gEnv->bServer)
			{
				if ((inventoryAmmoCount > 0) && (weaponAmmoCount == 0))
				{
					int refillAmmo = min(clipSize, inventoryAmmoCount);
					m_pWeapon->SetAmmoCount(pAmmoClass, refillAmmo);
					m_pWeapon->SetInventoryAmmoCount(pAmmoClass, inventoryAmmoCount - refillAmmo);
				}
			}
		}
		else
		{
			hide = (inventoryAmmoCount <= 0);
		}
	}

	m_pWeapon->HideItem(hide);

	return hide;
}

//------------------------------------------------------------------------
struct CThrow::ThrowAction
{
	ThrowAction(CThrow *_throw): pThrow(_throw) {};
	CThrow *pThrow;

	void execute(CItem *_this)
	{
		CActor* pOwner = pThrow->m_pWeapon->GetOwnerActor();

#if (USE_DEDICATED_INPUT )
		bool shoot = gEnv->bMultiplayer ? pOwner && (strcmp(pOwner->GetEntity()->GetClass()->GetName(), "DummyPlayer") == 0 || pOwner->IsClient()) : true;
#else
		bool shoot = gEnv->bMultiplayer ? pOwner && pOwner->IsClient() : true;
#endif
		
		if(shoot)
		{
			pThrow->Shoot(true);
		}
	}
};

struct CThrow::FinishAction
{
	FinishAction(CThrow *_throw): pThrow(_throw) {};
	CThrow *pThrow;

	void execute(CItem *_this)
	{
		pThrow->m_throwing = false;
		pThrow->m_primed = false;
		bool bRevertToLastItem = gEnv->bMultiplayer || pThrow->CheckAmmo();

		CActor* pOwner = pThrow->m_pWeapon->GetOwnerActor();

		if(pOwner)
		{
			pOwner->SetTag(PlayerMannequin.tagIDs.throwing, false);

			if(pOwner->IsClient() && bRevertToLastItem)
			{
				CCCPOINT(Throw_SwitchBack);
				pOwner->SelectLastItem(true, true);
			}
			else
			{
				_this->PlayAction(_this->GetFragmentIds().Select);
			}
		}
	}
};

struct CThrow::ShowItemAction
{
	ShowItemAction(CThrow *_throw): pThrow(_throw) {};
	CThrow *pThrow;

	void execute(CItem *_this)
	{
		pThrow->m_pWeapon->HideItem(false);
	}
};

//------------------------------------------------------
void CThrow::DoThrow()
{
	if (CActor *pActor = m_pWeapon->GetOwnerActor())
	{
		BATTLECHATTER(BC_GrenadeThrow, pActor->GetEntityId());
	}

	m_startFireTime = 0.0f;
	m_throwing = true;
	m_firing = false;

	m_pWeapon->PlayAction(GetFragmentIds().Throw, 0, false, CItem::eIPAF_Default);
	m_pWeapon->GetScheduler()->TimerAction((uint32)(m_fireParams->throwparams.throw_delay * 1000.0f), CSchedulerAction<ThrowAction>::Create(this), false);
	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner), CSchedulerAction<FinishAction>::Create(this), false);
}

//-----------------------------------------------------
bool CThrow::Shoot(bool resetAnimation, bool autoreload, bool isRemote)
{
	CCCPOINT(Throw_Shoot);
	//Grenade speed scale is always one (for player)
	if(CActor *pOwner = static_cast<CActor*>(m_pWeapon->GetOwnerActor()))
	{
		if(pOwner->IsDead() || pOwner->GetGameObject()->GetAspectProfile(eEA_Physics)==eAP_Sleep)
			return false; //Do not throw grenade if player is death (AI "ghost grenades")
	}

	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool clientIsShooter = pActor ? pActor->IsClient() : false;

	Vec3 hit, pos, dir;

	if(m_predicted)
	{
		hit = m_predictedHit;
		pos = m_predictedPos;
		dir = m_predictedDir;
	}
	else
	{
		bool bHit = false;
		ray_hit rayhit;	 
		rayhit.pCollider = 0;

		hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
		pos = (pActor && pActor->IsPlayer()) ? m_pWeapon->GetWorldPos() : GetFiringPos(hit);
		dir = ApplySpread(GetFiringDir(hit, pos), GetSpread());
	}

	Vec3 vel = GetFiringVelocity(dir);

	ShootInternal(hit, pos, dir, vel, clientIsShooter);

	return true;
}


void CThrow::ShootInternal(Vec3 hit, Vec3 pos, Vec3 dir, Vec3 vel, bool clientIsShooter)
{
	float newLifeTime = m_primed && m_fireParams->throwparams.prime_timer ? m_projectileLifeTime - (gEnv->pTimer->GetAsyncCurTime() - m_primeTime) : m_projectileLifeTime;
	newLifeTime = max(newLifeTime, 0.001f);
	if (m_projectileLifeTime == 0.0f)
		newLifeTime = 0.0f;

	m_pWeapon->HideItem(true);

	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;

	CProjectile *pAmmo = m_throwing || gEnv->bServer ? m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, false) : NULL;
	if (pAmmo)
	{

		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

		pAmmo->SetParams(CProjectile::SProjectileDesc(
			m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), (int)m_fireParams->fireparams.damage,
			0.f, 0.f, 0.f, m_fireParams->fireparams.hitTypeId,
			m_fireParams->fireparams.bullet_pierceability_modifier, false));
		// this must be done after owner is set
		pAmmo->InitWithAI();

		pAmmo->SetDestination(m_pWeapon->GetDestination());

		OnSpawnProjectile(pAmmo);
		pAmmo->Launch(pos, dir, vel, m_speed_scale);

		pAmmo->SetLifeTime(newLifeTime);
		
		m_projectileId = pAmmo->GetEntity()->GetId();
	}

	if (pAmmo && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer && clientIsShooter)
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	m_fired = true;
	m_firedTime = gEnv->pTimer->GetAsyncCurTime();
	m_next_shot += m_next_shot_dt;

	if (m_fireParams->fireparams.clip_size != -1)
	{
		int weaponAmmoCount = m_pWeapon->GetAmmoCount(ammo);
		int inventoryAmmoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

		m_pWeapon->SetAmmoCount(ammo, (inventoryAmmoCount > 0) ? weaponAmmoCount : weaponAmmoCount - 1);
		m_pWeapon->SetInventoryAmmoCount(ammo, (inventoryAmmoCount > 0) ? inventoryAmmoCount -1 : inventoryAmmoCount);
	}

	OnShoot(m_pWeapon->GetOwnerId(), pAmmo?pAmmo->GetEntity()->GetId():0, ammo, pos, dir, vel);

	m_pWeapon->RequestShoot(ammo, pos, dir, vel, hit, newLifeTime, pAmmo? pAmmo->GetGameObject()->GetPredictionHandle() : 0, false);
	
}

//------------------------------------------------------------------------
void CThrow::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle)
{
	CCCPOINT(Throw_NetShoot);
	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;

	CActor* pActor = m_pWeapon->GetOwnerActor();

	extra = max(extra, 0.001f);
	
	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, true);
	if (pAmmo)
	{
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");
		
		pAmmo->SetParams(CProjectile::SProjectileDesc(
			m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(),
			m_fireParams->fireparams.damage, 0.f, 0.f, 0.f, m_fireParams->fireparams.hitTypeId,
			m_fireParams->fireparams.bullet_pierceability_modifier, false));

		pAmmo->SetDestination(m_pWeapon->GetDestination());

		pAmmo->SetRemote(true);

		pAmmo->Launch(pos, dir, vel, m_speed_scale);
		pAmmo->SetLifeTime(extra);

		m_projectileId = pAmmo->GetEntity()->GetId();
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	m_fired = true;
	m_next_shot = 0.0f;

	int clipSize = GetClipSize();

	if (m_pWeapon->IsServer() && clipSize != -1)
	{
		int weaponAmmoCount = m_pWeapon->GetAmmoCount(ammo);
		int inventoryAmmoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

		m_pWeapon->SetAmmoCount(ammo, (inventoryAmmoCount > 0) ? weaponAmmoCount : weaponAmmoCount - 1);
		m_pWeapon->SetInventoryAmmoCount(ammo, (inventoryAmmoCount > 0) ? inventoryAmmoCount -1 : inventoryAmmoCount);
	}

	OnShoot(m_pWeapon->GetOwnerId(), pAmmo?pAmmo->GetEntity()->GetId():0, ammo, pos, dir, vel);

	if (OutOfAmmo())
		m_pWeapon->OnOutOfAmmo(ammo);

	if (pAmmo && predictionHandle && pActor)
	{
		pAmmo->GetGameObject()->RegisterAsValidated(pActor->GetGameObject(), predictionHandle);
		pAmmo->GetGameObject()->BindToNetwork();
	}
	else if (pAmmo && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer && pActor && pActor->IsClient())
	{
		pAmmo->GetGameObject()->BindToNetwork();
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CThrow::ReplayShoot()
{
	CCCPOINT(CThrow_ReplayShoot);
	// Don't need to actually spawn a projectile because the entity recording system will recreate that
	// Animations will also be recorded and played back through the animation recording system
	
	// Hide the item
	m_pWeapon->HideItem(true);
	// Then show it again after the animation is finished
	m_pWeapon->GetScheduler()->TimerAction(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner), CSchedulerAction<ShowItemAction>::Create(this), false);
	OnReplayShoot();
}

//---------------------------------------------------
void CThrow::CalculateTrajectory()
{
	bool bHit = false;
	ray_hit rayhit;	 
	rayhit.pCollider = 0;

	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
	
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = GetFiringDir(hit, pos);
	Vec3 vel = GetFiringVelocity(dir);

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	IPhysicalEntity* pOwnerPhysics = pOwner->GetEntity()->GetPhysics();

	Vec3 predictedPosOut;
	float predictedSpeedOut = 0.f;
	m_trajectoryLength = MAX_TRAJECTORY_SAMPLES;

	const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(GetAmmoType());
	if (pAmmoParams && m_pWeapon->PredictProjectileHit(pOwnerPhysics, pos, dir, vel, m_speed_scale * pAmmoParams->speed,
		predictedPosOut, predictedSpeedOut, m_trajectory, &m_trajectoryLength, g_pGameCVars->i_grenade_trajectory_resolution))
	{
		float explodeTime = m_primed ? (m_projectileLifeTime - (gEnv->pTimer->GetAsyncCurTime() - m_primeTime)) : m_projectileLifeTime;

		if(m_firedTime > 0.0f)
		{
			explodeTime = (m_projectileLifeTime - (m_firedTime - m_primeTime));
		}

		if(m_explodeTime!=explodeTime)
		{
			m_explodeTime = explodeTime;
			SHUDEvent timeEvent(eHUDEvent_OnTrajectoryTimeChanged);
			timeEvent.AddData(SHUDEventData(explodeTime));
			CHUDEventDispatcher::CallEvent(timeEvent);
		}

		if(explodeTime>0.0f && g_pGameCVars->i_grenade_trajectory_useGeometry==0)
		{
			RenderTrajectory(m_trajectory, m_trajectoryLength, explodeTime);
		}

		m_predicted = true;
		
		m_predictedPos = pos;
		m_predictedDir = dir;
		m_predictedHit = hit;
	}
	else
	{
		m_explodeTime = 0.0f;
	}
}
//-----------------------------------------------------
void CThrow::CheckNearMisses(const Vec3 &probableHit, const Vec3 &pos, const Vec3 &dir, float range, float radius)
{

}

//-----------------------------------------------------
bool CThrow::OutOfAmmo() const
{
	const int clipSize = GetClipSize();
	IEntityClass* pAmmoClass = m_fireParams->fireparams.ammo_type_class;
	const bool enabled = IsEnabled();

	//If usable
	if (enabled && pAmmoClass)
	{
		if (clipSize > 0)
		{
			return ((m_pWeapon->GetAmmoCount(pAmmoClass) + m_pWeapon->GetInventoryAmmoCount(pAmmoClass)) < 1);
		}
		else if (clipSize == 0)
		{
			return (m_pWeapon->GetInventoryAmmoCount(pAmmoClass) < 1);
		}

		return false; //Negative clip size
	}

	return false;
}

//-----------------------------------------------------
void CThrow::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));	
	BaseClass::GetInternalMemoryUsage(s);		// collect memory of parent class
}

//------------------------------------------------------
void CThrow::FumbleGrenade()
{
	//drop the grenade if the grenade priming is interrupted by certain player actions
	if(IsReadyToThrow() && m_pWeapon->IsOwnerClient())
	{
		float velocity = 0.f;

		if(const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(GetAmmoType()))
		{
			velocity = pAmmoParams->speed;
		}

		ShootInternal(Vec3(0.f, 0.f, 0.f),  m_pWeapon->GetWorldPos(), Vec3(0.f, 0.f, -1.f), Vec3(0.f, 0.f, velocity), true);
	}
}

void CThrow::RenderTrajectory(const Vec3* trajectory, unsigned int sampleCount, const float maxTime)
{
	ColorB color = CHUDUtils::GetHUDColor();
	
	const float fade = maxTime / m_projectileLifeTime;

	color.g = int_round((float)color.g * fade);
	color.b = int_round((float)color.b * fade);
	color.r += int_round((float)(255-color.r) * (1.0f-fade));
	color.a = int_round((float)color.a * 0.5f);

	IRenderAuxGeom* pRenderer = gEnv->pRenderer->GetIRenderAuxGeom(); 
	SAuxGeomRenderFlags oldFlags = pRenderer->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetCullMode(e_CullModeNone);
	pRenderer->SetRenderFlags(newFlags);

	const float dashLength = g_pGameCVars->i_grenade_trajectory_dashes;
	const float gapLength = g_pGameCVars->i_grenade_trajectory_gaps;
	const float partLength = dashLength+gapLength;

	float dash = 0.0f;
	float gap = 0.0f;
	bool isDash = true;
	int currentChunkSize = 0;
	float time = 0.0f;
	float timeStep = g_pGameCVars->i_grenade_trajectory_resolution;

	for(unsigned int i=0; (i<sampleCount)&&(time<maxTime); i+=1, time+=timeStep)
	{
		float dist = 0.0f;
		if(i>0)
		{
			dist = (trajectory[i-1] - trajectory[i]).GetLength();
		}
		
		if(isDash)
		{
			float test = dash + dist;
			if(test < dashLength)
			{
				s_trajectoryPart[currentChunkSize] = trajectory[i];
				++currentChunkSize;
				dash = test;
			}
			else
			{
				const float rest = (dashLength - dash);
				if(i>0)
				{
					s_trajectoryPart[currentChunkSize] = trajectory[i-1] + ((trajectory[i] - trajectory[i-1]) * rest);
					++currentChunkSize;
					pRenderer->DrawPolyline(s_trajectoryPart, currentChunkSize, false, color, 10.0f);
				}

				currentChunkSize = 0;

				dash = 0.0f;

				isDash = false;
				test = dist - rest;

				if(test > gapLength)
				{
					//the current part is longer than the gap, split up this part and start dash again
					if(i>0)
					{
						const float newStart = (rest + gapLength);
						s_trajectoryPart[currentChunkSize] = trajectory[i-1] + ((trajectory[i] - trajectory[i-1]) * newStart);
						++currentChunkSize;
					}
					isDash = true;
				}
				else
				{
					gap = test;
				}
			}
		}
		else
		{
			float test = gap + dist;
			if(test < gapLength)
			{
				gap = test;
			}
			else
			{
				const float rest = (gapLength - gap);
				if(i>0)
				{
					s_trajectoryPart[currentChunkSize] = trajectory[i-1] + ((trajectory[i] - trajectory[i-1]) * rest);
					++currentChunkSize;
				}

				gap = 0.0f;
				dash = dist - rest;
				isDash = true;
			}

		}
	}

	if(currentChunkSize)
	{
		pRenderer->DrawPolyline(s_trajectoryPart, currentChunkSize, false, color, 10.0f);
	}

	pRenderer->SetRenderFlags(oldFlags);
}
