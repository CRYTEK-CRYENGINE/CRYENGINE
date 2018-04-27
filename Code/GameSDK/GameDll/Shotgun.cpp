// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Shotgun.h"
#include "Game.h"
#include "WeaponSystem.h"
#include "Actor.h"
#include "Player.h"
#include "Projectile.h"
#include "GameRules.h"

#include "WeaponSharedParams.h"
#include "UI/UICVars.h"
#include "UI/UIManager.h"

#include  "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "ItemAnimation.h"

#include <IForceFeedbackSystem.h>


CRY_IMPLEMENT_GTI(CShotgun, CSingle);


void CShotgun::Activate(bool activate)
{
	CSingle::Activate(activate);

	m_next_shot = 0.0f;
	m_reload_pump = true;
	m_load_shell_on_end = false;
	m_break_reload = false;
	m_reload_was_broken = false;
	m_pWeapon->RequireUpdate(eIUS_FireMode);
	m_shotIndex = 0;
}

// Move shotgun into reloading pose
struct CShotgun::BeginReloadLoop
{
	BeginReloadLoop(CShotgun *_shotty, int _zoomed): shotty(_shotty), zoomed(_zoomed) {};
	CShotgun *shotty;
	int zoomed;

	void execute(CItem *_this)
	{
		shotty->ReloadShell(zoomed);
	}
};

void CShotgun::StartReload(int zoomed)
{
	//If reload was broken to shoot, do not start a reload before shooting
	if(m_break_reload)
		return;

	int clipSize = GetClipSize();
	int ammoCount = m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class);
	if ((ammoCount >= clipSize) || m_reloading)
		return;

	CActor* pActor =  m_pWeapon->GetOwnerActor();

	m_max_shells = clipSize - ammoCount;

	m_reload_was_broken = false;
	m_reloading = true;
	if (zoomed)
		m_pWeapon->ExitZoom(true);

	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;
	m_pWeapon->OnStartReload(m_pWeapon->GetOwnerId(), ammo);

	m_pWeapon->PlayAction(GetFragmentIds().begin_reload, 0, false, CItem::eIPAF_Default, -1);

	m_reload_pump = ammoCount > 0 ? false : true;
	uint32 animTime = m_pWeapon->GetCurrentAnimationTime(eIGS_Owner);
	if(animTime==0)
		animTime = 500; //For DS
	m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<BeginReloadLoop>::Create(BeginReloadLoop(this, zoomed)), false);
}

// Reload shells
class CShotgun::PartialReloadAction
{
public:
	PartialReloadAction(CWeapon *_wep, int _zoomed)
		: pWep(_wep)
		, rzoomed(_zoomed)
	{
	}
	void execute(CItem *_this)
	{
		CShotgun *fm = (CShotgun *)pWep->GetFireMode(pWep->GetCurrentFireMode());

		if(fm->m_reload_was_broken)
			return;

		IEntityClass* pAmmoType = fm->GetAmmoType();

		if (pWep->IsServer())
		{
			const int ammoCount = pWep->GetAmmoCount(pAmmoType);
			const int inventoryCount = pWep->GetInventoryAmmoCount(pAmmoType);

			const int refill = fm->m_fireParams->shotgunparams.partial_reload ? 1 : min(inventoryCount, fm->GetClipSize() - ammoCount);

			pWep->SetAmmoCount(pAmmoType, ammoCount + refill);
			pWep->SetInventoryAmmoCount(pAmmoType, pWep->GetInventoryAmmoCount(pAmmoType) - refill);
		}

		CCCPOINT(Shotgun_ReloadShell);
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(pWep->GetOwner(), GameplayEvent(eGE_WeaponReload, pAmmoType->GetName(), 1, (void *)(EXPAND_PTR)(pWep->GetEntityId())));

		if (!fm->m_break_reload)
			fm->ReloadShell(rzoomed);
		else
			fm->EndReload(rzoomed);
	}
private:
	CWeapon *pWep;
	int rzoomed;
};

void CShotgun::ReloadShell(int zoomed)
{
	if(m_reload_was_broken)
		return;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	bool isAI = pOwner && (pOwner->IsPlayer() == false);
	int ammoCount = m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class);
	if ((ammoCount < GetClipSize()) && (m_max_shells>0) &&
		(isAI || (m_pWeapon->GetInventoryAmmoCount(m_fireParams->fireparams.ammo_type_class) > (m_load_shell_on_end ? 1 : 0))) ) // AI has unlimited ammo
	{
		m_max_shells --;
		
		const float speedOverride = GetReloadSpeedMultiplier(pOwner);

		// reload a shell
		m_pWeapon->PlayAction(GetFragmentIds().reload_shell, 0, false, CItem::eIPAF_Default,speedOverride);		
		uint32 animTime = m_pWeapon->GetCurrentAnimationTime(eIGS_Owner);
		if(animTime==0)
			animTime = 530; //For DS
		m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<PartialReloadAction>::Create(PartialReloadAction(m_pWeapon, zoomed)), false);
		// call this again
	}
	else
	{
		EndReload(zoomed);
	}
}

// Move shotgun out of reloading pose
class CShotgun::ReloadEndAction
{
public:
	ReloadEndAction(CWeapon *_wep, int _zoomed)
	{
		pWep = _wep;
		rzoomed = _zoomed;
	}
	void execute(CItem *_this)
	{
		CShotgun *fm = (CShotgun *)pWep->GetFireMode(pWep->GetCurrentFireMode());
		IEntityClass* ammo = fm->GetAmmoType();
		pWep->OnEndReload(pWep->GetOwnerId(), ammo);

		pWep->SetBusy(false);

		fm->m_reloading = false;
		fm->m_emptyclip = false;
		fm->m_spinUpTime = fm->m_firing ? fm->m_fireParams->fireparams.spin_up_time:0.0f;

		if (fm->m_break_reload)
		{
			fm->m_break_reload=false;
		}
		else if(fm->m_load_shell_on_end && pWep->IsServer())
		{
			IEntityClass* pAmmoType = fm->GetAmmoType();
			const int ammoCount = pWep->GetAmmoCount(pAmmoType);
			const int inventoryCount = pWep->GetInventoryAmmoCount(pAmmoType);

			pWep->SetAmmoCount(pAmmoType, ammoCount + 1);
			pWep->SetInventoryAmmoCount(pAmmoType, inventoryCount - 1);

			fm->m_load_shell_on_end = false;
		}
	}
private:
	CWeapon *pWep;
	int		rzoomed;
};

void CShotgun::EndReload(int zoomed)
{
	CActor *pActor = m_pWeapon->GetOwnerActor();

	float speedOverride = 1.0f;
	if (m_reload_was_broken)
	{
		speedOverride = m_fireParams->shotgunparams.endReloadSpeedOverride;
	}
	else
	{
		speedOverride = GetReloadSpeedMultiplier(pActor);
	}


	uint32 animTime = 100;
	uint32 reloadBreakTime = uint32(m_fireParams->shotgunparams.reloadBreakTime * 1000);

	const CTagDefinition* pTagDefinition = NULL;
	int fragmentID = m_pWeapon->GetFragmentID("exit_reload", &pTagDefinition);
	if(fragmentID != FRAGMENT_ID_INVALID)
	{
		TagState actionTags = TAG_STATE_EMPTY;
		if(pTagDefinition && m_reload_pump && !m_reload_was_broken)
		{
			CTagState fragTags(*pTagDefinition);
				
			fragTags.SetByCRC(CItem::sFragmentTagCRCs.ammo_empty, true);

			actionTags = fragTags.GetMask();
		}

		CItemAction* pAction = new CItemAction(PP_PlayerAction, fragmentID, actionTags);
		m_pWeapon->PlayFragment(pAction, speedOverride);
	}

	animTime = m_pWeapon->GetCurrentAnimationTime(eIGS_Owner);

	if(!m_reload_was_broken)
		m_pWeapon->GetScheduler()->TimerAction(animTime, CSchedulerAction<ReloadEndAction>::Create(ReloadEndAction(m_pWeapon, zoomed)), false);
	else
		m_pWeapon->GetScheduler()->TimerAction(reloadBreakTime, CSchedulerAction<ReloadEndAction>::Create(ReloadEndAction(m_pWeapon, zoomed)), false);

	m_pWeapon->SendEndReload();
}

bool CShotgun::CanFire(bool considerAmmo) const
{
	return (m_next_shot<=0.0f) && (m_spinUpTime<=0.0f) &&
		!m_pWeapon->IsBusy() && !m_pWeapon->IsSwitchingFireMode() && (!considerAmmo || !OutOfAmmo() || !m_fireParams->fireparams.ammo_type_class || GetClipSize() == -1)
		&& (!m_reloading || m_fireParams->shotgunparams.partial_reload) && !m_pWeapon->IsLowered();
}

class CShotgun::ScheduleReload
{
public:
	ScheduleReload(CShotgun* pShotgun, CWeapon* pWeapon)
	{
		_pThis = pShotgun;
		_pWeapon = pWeapon;
	}
	void execute(CItem *item) 
	{
		_pThis->m_autoReloading = false;
		_pWeapon->OnFireWhenOutOfAmmo();
	}
private:
	CShotgun* _pThis;
	CWeapon* _pWeapon;
};

bool CShotgun::Shoot(bool resetAnimation, bool autoreload/* =true */, bool isRemote)
{
	CCCPOINT(Shotgun_TryShoot);

	m_firePending = false;
	m_shotIndex++;

	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;
	int ammoCount = m_pWeapon->GetAmmoCount(ammo);

	int clipSize = GetClipSize();
	if (clipSize == 0)
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor ? pActor->IsPlayer() : false;
	bool shooterIsClient = pActor ? pActor->IsClient() : false;

	if (!CanFire(true))
	{
		if ((ammoCount <= 0) && (!m_reloading))
		{
			m_pWeapon->PlayAction(GetFragmentIds().empty_clip);
			m_pWeapon->OnFireWhenOutOfAmmo();
			CCCPOINT(Shotgun_TryShootWhileOutOfAmmo);
		}
		else
		{
			CCCPOINT(Shotgun_TryShootWhileCannotBeFired);
		}
		return false;
	}

	if (m_reloading)
	{
		if(m_pWeapon->IsBusy())
			m_pWeapon->SetBusy(false);
		
		if(CanFire(true) && !m_break_reload)
		{
			m_break_reload = true;
			m_pWeapon->RequestCancelReload();
		}
		CCCPOINT(Shotgun_TryShootWhileReloading);
		return false;
	}

	uint32 flags = CItem::eIPAF_Default;
	if (IsProceduralRecoilEnabled() && pActor)
	{
		pActor->ProceduralRecoil(m_fireParams->proceduralRecoilParams.duration, m_fireParams->proceduralRecoilParams.strength, m_fireParams->proceduralRecoilParams.kickIn, m_fireParams->proceduralRecoilParams.arms);
	}

	float speedOverride = -1.f;

	m_pWeapon->PlayAction(GetFragmentIds().fire, 0, false, flags, speedOverride);

	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE);
	Vec3 pos = GetFiringPos(hit);
	Vec3 fdir = GetFiringDir(hit, pos);
	Vec3 vel = GetFiringVelocity(fdir);
	Vec3 dir;
	const float hitDist = hit.GetDistance(pos);

	CheckNearMisses(hit, pos, fdir, WEAPON_HIT_RANGE, m_fireParams->shotgunparams.spread);
	
	CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
	CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

	int quad = cry_random(0, 3);
	const int numPellets = m_fireParams->shotgunparams.pellets;

	std::vector<CProjectile*> projList;
	projList.reserve(numPellets);

	int ammoCost = (m_fireParams->fireparams.fake_fire_rate && playerIsShooter) ? m_fireParams->fireparams.fake_fire_rate : 1;
	ammoCost = min(ammoCost, ammoCount);

	EntityId firstAmmoId = 0;

	// SHOT HERE
	for (int i = 0; i < numPellets; i++)
	{
		CProjectile *pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, false);
		if (pAmmo)
		{
			if(!firstAmmoId)
			{
				firstAmmoId = pAmmo->GetEntityId();
			}

			projList.push_back(pAmmo);

			dir = ApplySpread(fdir, m_fireParams->shotgunparams.spread, quad);  
			quad = (quad+1)%4;
			
			int pelletDamage = m_fireParams->shotgunparams.pelletdamage;
			if (!playerIsShooter)
				pelletDamage += m_fireParams->shotgunparams.npc_additional_damage;

			const bool canOvercharge = m_pWeapon->GetSharedItemParams()->params.can_overcharge;
			const float overchargeModifier = pActor ? pActor->GetOverchargeDamageScale() : 1.0f;
			if (canOvercharge)
			{
				pelletDamage = int(pelletDamage * overchargeModifier);
			}

			CProjectile::SProjectileDesc projectileDesc(
				m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), pelletDamage, m_fireParams->fireparams.damage_drop_min_distance,
				m_fireParams->fireparams.damage_drop_per_meter, m_fireParams->fireparams.damage_drop_min_damage, m_fireParams->fireparams.hitTypeId, m_fireParams->fireparams.bullet_pierceability_modifier, m_pWeapon->IsZoomed());
			projectileDesc.pointBlankAmount = m_fireParams->fireparams.point_blank_amount;
			projectileDesc.pointBlankDistance = m_fireParams->fireparams.point_blank_distance;
			projectileDesc.pointBlankFalloffDistance = m_fireParams->fireparams.point_blank_falloff_distance;
			if (m_fireParams->fireparams.ignore_damage_falloff)
				projectileDesc.damageFallOffAmount = 0.0f;
			
			const Vec3 pelletDestination = pos + (dir * hitDist);

			pAmmo->SetParams(projectileDesc);
			pAmmo->SetDestination(m_pWeapon->GetDestination());
			pAmmo->Launch(pos, dir, vel);
			pAmmo->CreateBulletTrail( pelletDestination );
			pAmmo->SetKnocksTargetInfo( GetShared() );

			if ((!m_fireParams->tracerparams.geometry.empty() || !m_fireParams->tracerparams.effect.empty()) && ((ammoCount == clipSize) || (ammoCount%m_fireParams->tracerparams.frequency==0)))
			{
				EmitTracer(pos, pelletDestination, &m_fireParams->tracerparams, pAmmo);
			}

			if(shooterIsClient)
			{
				pAmmo->RegisterLinkedProjectile(m_shotIndex);
				
				if(gEnv->bMultiplayer)
				{
					float damageCap = g_pGameCVars->pl_shotgunDamageCap;
					pAmmo->SetDamageCap(damageCap);
				}
			}
			
			m_projectileId = pAmmo->GetEntity()->GetId();

			pAmmo->SetAmmoCost(ammoCost);
		}
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, m_fireParams->shotgunparams.pellets, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	m_muzzleEffect.Shoot(this, hit, m_barrelId);

	m_fired = true;
	SetNextShotTime(m_next_shot + m_next_shot_dt);

	ammoCount -= ammoCost;

	if (ammoCount < m_fireParams->fireparams.minimum_ammo_count)
		ammoCount = 0;

	if (clipSize != -1)
	{
		if (clipSize != 0)
			m_pWeapon->SetAmmoCount(ammo, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
	}

	OnShoot(m_pWeapon->GetOwnerId(), firstAmmoId, ammo, pos, dir, vel);

	const SThermalVisionParams& thermalParams = m_fireParams->thermalVisionParams;
	m_pWeapon->AddShootHeatPulse(pActor, thermalParams.weapon_shootHeatPulse, thermalParams.weapon_shootHeatPulseTime,
		thermalParams.owner_shootHeatPulse, thermalParams.owner_shootHeatPulseTime);

	if (OutOfAmmo())
	{
		m_pWeapon->OnOutOfAmmo(ammo);
		if (autoreload)
		{
			uint32 scheduleTime = max(m_pWeapon->GetCurrentAnimationTime(eIGS_Owner), (uint)(m_next_shot*1000));
			m_pWeapon->GetScheduler()->TimerAction(scheduleTime, CSchedulerAction<ScheduleReload>::Create(ScheduleReload(this, m_pWeapon)), false);
			m_autoReloading = true;
		}
	}

	m_pWeapon->RequestShoot(ammo, pos, dir, vel, hit, 1.0f, 0, false);

	CCCPOINT(Shotgun_Fired);

	return true;
}

//------------------------------------------------------------------------
void CShotgun::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int ph)
{
	CCCPOINT(Shotgun_NetShoot);

	assert(0 == ph);
	
	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;
	FragmentID action = m_fireParams->fireparams.no_cock ? GetFragmentIds().fire : GetFragmentIds().fire_cock;

	CActor *pActor = m_pWeapon->GetOwnerActor();
	bool playerIsShooter = pActor?pActor->IsPlayer():false;

	int ammoCount = m_pWeapon->GetAmmoCount(ammo);
	int clipSize = GetClipSize();
	if (clipSize == 0)
		ammoCount = m_pWeapon->GetInventoryAmmoCount(ammo);

	if (ammoCount == 1)
		action = GetFragmentIds().fire;

	if (IsProceduralRecoilEnabled() && pActor)
	{
		pActor->ProceduralRecoil(m_fireParams->proceduralRecoilParams.duration, m_fireParams->proceduralRecoilParams.strength, m_fireParams->proceduralRecoilParams.kickIn,m_fireParams->proceduralRecoilParams.arms);
	}

	m_pWeapon->PlayAction(action, 0, false, CItem::eIPAF_Default);

	Vec3 pdir;

	int quad = cry_random(0, 3);

	CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
	CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

	int ammoCost = m_fireParams->fireparams.fake_fire_rate ? m_fireParams->fireparams.fake_fire_rate : 1;
	ammoCost = min(ammoCost, ammoCount);

	// SHOT HERE
	for (int i = 0; i < m_fireParams->shotgunparams.pellets; i++)
	{
		CProjectile *pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, true);
		if (pAmmo)
		{
			pdir = ApplySpread(dir, m_fireParams->shotgunparams.spread, quad);
			quad = (quad+1)%4;
		
			CProjectile::SProjectileDesc projectileDesc(
				m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_fireParams->shotgunparams.pelletdamage, m_fireParams->fireparams.damage_drop_min_distance,
				m_fireParams->fireparams.damage_drop_min_damage, m_fireParams->fireparams.damage_drop_per_meter, m_fireParams->fireparams.hitTypeId, m_fireParams->fireparams.bullet_pierceability_modifier, m_pWeapon->IsZoomed());
			projectileDesc.pointBlankAmount = m_fireParams->fireparams.point_blank_amount;
			projectileDesc.pointBlankDistance = m_fireParams->fireparams.point_blank_distance;
			projectileDesc.pointBlankFalloffDistance = m_fireParams->fireparams.point_blank_falloff_distance;
			if (m_fireParams->fireparams.ignore_damage_falloff)
				projectileDesc.damageFallOffAmount = 0.0f;
			pAmmo->SetParams(projectileDesc);
			pAmmo->SetDestination(m_pWeapon->GetDestination());
			pAmmo->SetRemote(true);
			pAmmo->Launch(pos, pdir, vel);

			bool emit = false;
			if(m_pWeapon->GetStats().fp)
				emit = (!m_fireParams->tracerparams.geometryFP.empty() || !m_fireParams->tracerparams.effectFP.empty()) && ((ammoCount == clipSize) || (ammoCount%m_fireParams->tracerparams.frequency==0));
			else
				emit = (!m_fireParams->tracerparams.geometry.empty() || !m_fireParams->tracerparams.effect.empty()) && ((ammoCount == clipSize) || (ammoCount%m_fireParams->tracerparams.frequency==0));

			if (emit)
			{
				EmitTracer(pos, hit, &m_fireParams->tracerparams, pAmmo);
			}

			m_projectileId = pAmmo->GetEntity()->GetId();

			pAmmo->SetAmmoCost(ammoCost);
		}
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, m_fireParams->shotgunparams.pellets, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	m_muzzleEffect.Shoot(this, hit, m_barrelId);

	m_fired = true;
	m_next_shot = 0.0f;

	ammoCount -= ammoCost;

	if (m_pWeapon->IsServer())
	{
		if (clipSize != -1)
		{
			if (clipSize != 0)
				m_pWeapon->SetAmmoCount(ammo, ammoCount);
			else
				m_pWeapon->SetInventoryAmmoCount(ammo, ammoCount);
		}
	}

	OnShoot(m_pWeapon->GetOwnerId(), 0, ammo, pos, dir, vel);

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//---------------------------------------------------------------------
void CShotgun::CancelReload()
{
	if(!m_reload_was_broken)
	{
		m_reload_was_broken = true;
		m_pWeapon->GetScheduler()->Reset();
		EndReload(0);
	}
}

//------------------------------------------------
void CShotgun::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));	
	GetInternalMemoryUsage(s);
}
void CShotgun::GetInternalMemoryUsage(ICrySizer * s) const
{
	CSingle::GetInternalMemoryUsage(s);		// collect memory of parent class
}

//------------------------------------------------
float CShotgun::GetSpreadForHUD() const
{
	return m_fireParams->shotgunparams.spread * g_pGame->GetUI()->GetCVars()->hud_Crosshair_shotgun_spreadMultiplier;
}
