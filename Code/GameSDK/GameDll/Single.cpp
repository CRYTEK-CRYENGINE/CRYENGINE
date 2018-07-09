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
#include "Single.h"

#include "Item.h"
#include "Weapon.h"
#include "Projectile.h"
#include "Actor.h"
#include "Player.h"
#include "PlayerStateEvents.h"
#include "Game.h"
#include "GameCVars.h"
#include "WeaponSystem.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <IVehicleSystem.h>
#include <CryAction/IMaterialEffects.h>
#include "GameRules.h"
#include <CryMath/Cry_GeoDistance.h>


#include "IronSight.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>	

#include "WeaponSharedParams.h"
#include "ScreenEffects.h"

#include "Audio/AudioSignalPlayer.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "AI/GameAIEnv.h"
#include "AI/HazardModule/HazardModule.h"

#include "ItemAnimation.h"
#include "Melee.h"

#include <IPerceptionManager.h>

CRY_IMPLEMENT_GTI(CSingle, CFireMode);

#pragma warning(disable: 4244)

#if defined(_DEBUG)
	#define DEBUG_AIMING_AND_SHOOTING
#endif

#ifdef DEBUG_AIMING_AND_SHOOTING

//---------------------------------------------------------------------------
// TODO remove when aiming/fire direction is working
// debugging aiming dir
struct DBG_shoot{
	Vec3	src;
	Vec3	dst;
};

const	int	DGB_ShotCounter(3);
int	DGB_curIdx(-1);
int	DGB_curLimit(-1);
DBG_shoot DGB_shots[DGB_ShotCounter];
// remove over
//---------------------------------------------------------------------------

#endif

// SProbableHitInfo
SProbableHitInfo::SProbableHitInfo()
: m_queuedRayID(0)
, m_hit(0.0f, 0.0f, 0.0f)
, m_state(eProbableHitDeferredState_none)
{

}

SProbableHitInfo::~SProbableHitInfo()
{
	CancelPendingRay();
}

void SProbableHitInfo::OnDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(rayID == m_queuedRayID);

	if(result.hitCount > 0)
	{
		m_hit = result.hits[0].pt;
	}

	m_queuedRayID = 0;

	m_state = eProbableHitDeferredState_done;
}

void SProbableHitInfo::CancelPendingRay()
{
	if (m_queuedRayID != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRayID);
	}
	m_queuedRayID = 0;
}

//------------------------------------------------------------------------
CSingle::CSingle()
: m_projectileId(0),
	m_spinUpTimer(0.0f),
	m_speed_scale(1.0f),
	m_spinUpEffectId(0),
	m_spinUpTime(0),
	m_next_shot(0),
	m_next_shot_dt(0),
	m_emptyclip(false),
	m_reloading(false),
	m_firing(false),
	m_fired(false),
	m_firstFire(false),
	m_barrelId(0),
	m_reloadCancelled(false),
	m_reloadStartFrame(0),
	m_reloadPending(false),
	m_autoReloading(false),
	m_firePending(false),
	m_cocking(false),
	m_smokeEffectId(0),
	m_saved_next_shot(0.0f),
	m_fireAnimationWeight(1.0f)
{
}

//------------------------------------------------------------------------
CSingle::~CSingle()
{
	m_fireParams = 0;
}

//------------------------------------------------------------------------
void CSingle::InitFireMode(IWeapon* pWeapon, const SParentFireModeParams* pParams)
{
	BaseClass::InitFireMode(pWeapon, pParams);

	m_recoil.Init(m_pWeapon, this);
}

//----------------------------------------------------------------------
void CSingle::PostInit()
{
	CacheAmmoGeometry();
}

//------------------------------------------------------------------------
void CSingle::Update(float frameTime, uint32 frameId)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	bool keepUpdating = false;
	CActor* pOwnerActor = m_pWeapon->GetOwnerActor();

	if (m_firePending)
	{
		StartFire();
		if (!m_firePending)
			StopFire();
	}

	if (m_spinUpTime>0.0f)
	{
		m_spinUpTime -= frameTime;
		if (m_spinUpTime<=0.0f)
		{
			m_spinUpTime=0.0f;
			Shoot(true, m_fireParams->fireparams.autoReload);
		}

		keepUpdating=true;
	}
	else if (m_next_shot >= 0.0f)
	{
		m_next_shot -= frameTime;
		if (m_next_shot<=0.0f)
		{
			CMelee* pMeleeMode = m_pWeapon->GetMelee();
			
			if(!m_reloading && !(pMeleeMode && pMeleeMode->IsAttacking()) && !m_pWeapon->IsSelecting())
			{
				m_pWeapon->SetBusy(false);
				m_pWeapon->ForcePendingActions(CWeapon::eWeaponAction_Fire);
			}
		}

		keepUpdating=true;
	}

	if (m_pWeapon->IsBusy())
		keepUpdating = true;

	if (IsReadyToFire())
		m_pWeapon->OnReadyToFire();

	// update spinup effect
	if (m_spinUpTimer>0.0f)
	{
		m_spinUpTimer -= frameTime;
		if (m_spinUpTimer <= 0.0f)
		{
			m_spinUpTimer = 0.0f;
			if (m_spinUpEffectId)
				SpinUpEffect(false);
		}

		keepUpdating=true;
	}

	m_recoil.SetSpreadMultiplier(CalculateSpreadMultiplier(pOwnerActor));
	m_recoil.SetRecoilMultiplier(CalculateRecoilMultiplier(pOwnerActor));
	m_recoil.Update(pOwnerActor, frameTime, m_fired, frameId, m_firstFire);
	
	uint i = 0;

 	while(i < m_queuedProbableHits.size())
	{
		SProbableHitInfo *probableHit = m_queuedProbableHits[i];
		
		if(probableHit->m_state >= eProbableHitDeferredState_done)
		{
			NetShootDeferred(probableHit->m_hit);
			probableHit->m_state = eProbableHitDeferredState_none;
			
			m_queuedProbableHits.removeAt(i);
		}
		else
		{
			i++;
		}
	}

	keepUpdating |= (m_queuedProbableHits.size() > 0);

	if (keepUpdating)
		m_pWeapon->RequireUpdate(eIUS_FireMode);

#ifndef _RELEASE
	if(g_pGameCVars->i_debug_spread > 1 && m_pWeapon->IsSelected())
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode); //Force updating to keep debug spread watch active
	}
#endif

	BaseClass::Update(frameTime, frameId);

	m_firstFire = m_firstFire && !m_fired;
	m_fired = false;

	if (m_HazardID.IsDefined())
	{		
		SyncHazardAreaInFrontOfWeapon();
	}

#ifdef DEBUG_AIMING_AND_SHOOTING
	//---------------------------------------------------------------------------
	// TODO remove when aiming/fire direction is working
	// debugging aiming dir

	static ICVar* pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");
	if(pAimDebug->GetIVal()!=0)
	{
		const ColorF	queueFireCol( .4f, 1.0f, 0.4f, 1.0f );
		for(int dbgIdx(0);dbgIdx<DGB_curLimit; ++dbgIdx)
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( DGB_shots[dbgIdx].src, queueFireCol, DGB_shots[dbgIdx].dst, queueFireCol );
	}	
#endif
}

#ifndef _RELEASE
//---------------------------------------------------------------------------
void CSingle::DebugUpdate(float frameTime) const
{
	m_recoil.DebugUpdate(frameTime);
}
#endif

//---------------------------------------------------------------------------
void CSingle::UpdateFPView(float frameTime)
{
	UpdateFireAnimationWeight(frameTime);
}

//------------------------------------------------------------------------
void CSingle::ResetParams()
{
	BaseClass::ResetParams();

	ResetRecoilMod();
	ResetSpreadMod();
	m_muzzleEffect.Initialize(this);
	// Detach emitters so that if the particle effect changes it will take effect
	m_muzzleEffect.DetachEmitters(this, m_barrelId);
}

//------------------------------------------------------------------------
void CSingle::Activate(bool activate)
{
	BaseClass::Activate(activate);

	m_fired = m_firstFire = m_firing = m_reloading = m_emptyclip = m_cocking = false;
	m_next_shot = 0.0f;
	m_spinUpTime = 0.0f;

	if (m_fireParams->fireparams.rate > 0.0f)
		m_next_shot_dt = 60.0f/m_fireParams->fireparams.rate;
	else
		m_next_shot_dt = 0.001f;

	m_barrelId = 0;
  
	m_reloadPending = false;
	m_autoReloading = false;

	if (!activate && m_smokeEffectId)
	{
		m_pWeapon->DetachEffect(m_smokeEffectId);
		m_smokeEffectId = 0;
	}

	m_reloadCancelled = false;

	if (!activate)
		m_muzzleEffect.DetachEmitters(this, m_barrelId);
	else if (m_pWeapon->GetStats().fp)
		m_muzzleEffect.AttachEmitters(this, m_barrelId);
  	
	SpinUpEffect(false);

	ResetRecoil();  

	if(m_pWeapon->IsZoomed() || m_pWeapon->IsZoomingInOrOut())
	{
		if(activate)
		{
			if(IZoomMode* pZm = m_pWeapon->GetZoomMode(m_pWeapon->GetCurrentZoomMode()))
				pZm->ApplyZoomMod(this, m_pWeapon->GetZoomTransition());
		}
		else
		{
			ResetRecoilMod();
			ResetSpreadMod();
		}
	}

	if(activate && (m_pWeapon->IsOwnerClient() || gEnv->bServer))
	{
		if((OutOfAmmo() || m_pWeapon->ReloadWhenSelected()) && CanReload())
		{
			if(m_pWeapon->IsSelecting())
			{
				m_pWeapon->ForceSelectActionComplete();
			}

			m_pWeapon->Reload();
		}
	}

#ifndef _RELEASE
	if(g_pGameCVars->i_debug_spread > 1)
	{
		m_pWeapon->RequireUpdate(eIUS_FireMode); //Force updating to show debug spread watch value
	}
#endif
}

void CSingle::OnEnterFirstPerson()
{
	m_muzzleEffect.DetachEmitters(this, m_barrelId);
	m_muzzleEffect.AttachEmitters(this, m_barrelId);
}

//------------------------------------------------------------------------
int CSingle::GetAmmoCount() const
{
	return m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class);
}

//------------------------------------------------------------------------
int CSingle::GetClipSize() const
{
	const int clipSize = m_fireParams->fireparams.clip_size;
	if(!gEnv->bMultiplayer && clipSize > 0)
	{
		if(CActor* pOwner = m_pWeapon->GetOwnerActor())
		{
			if (!pOwner->IsPlayer())
			{
				return (m_fireParams->fireparams.ai_infiniteAmmo == false) ? clipSize : -1;
			}
		}
	}

	return clipSize;
}

//------------------------------------------------------------------------
int CSingle::GetChamberSize() const
{
	return m_fireParams->fireparams.bullet_chamber;
}

//------------------------------------------------------------------------
bool CSingle::OutOfAmmo() const
{
	const int clipSize = GetClipSize();

	// We want to make sure that some firemodes can never be used by the player 
	// so that they are only accessible to the AI. This will make it possible to
	// make AI entities shoot harder/more accurate/faster/whatever when they 
	// are angry/'buffed'/whatever.	(Ab)using the 'enabled' property here to 
	// keep it compatible with multiplayer code.	
	bool isEnabled = true;
	const CActor* owner = m_pWeapon->GetOwnerActor();
	if (owner != NULL)
	{
		if (owner->IsPlayer())
		{
			isEnabled = IsEnabled();
		}
	}

	if (clipSize != 0)
	{
		return !isEnabled || (m_fireParams->fireparams.ammo_type_class && clipSize != -1 && m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class) < 1);
	}

	return !isEnabled || (m_fireParams->fireparams.ammo_type_class && (m_pWeapon->GetInventoryAmmoCount(m_fireParams->fireparams.ammo_type_class) < 1));
}

//------------------------------------------------------------------------
bool CSingle::LowAmmo(float thresholdPerCent) const
{
	const int clipSize = GetClipSize();
	if (clipSize != 0)
	{
		return m_fireParams->fireparams.ammo_type_class && clipSize != -1 && m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class) < (int)(thresholdPerCent*clipSize);
	}

	return m_fireParams->fireparams.ammo_type_class && (m_pWeapon->GetInventoryAmmoCount(m_fireParams->fireparams.ammo_type_class) < (int)(thresholdPerCent*clipSize));
}

//------------------------------------------------------------------------
bool CSingle::CanReload() const
{
	const int clipSize = GetClipSize();

	CActor * pActor = m_pWeapon->GetOwnerActor();
	const bool isAI = pActor ? !pActor->IsPlayer() : false;

	if (clipSize != 0)
	{
		return !m_reloading && !m_reloadPending && (GetAmmoCount() < clipSize) && 
			((m_pWeapon->GetInventoryAmmoCount(m_fireParams->fireparams.ammo_type_class) > 0) || isAI);
	}

	return false;
}

bool CSingle::IsReloading(bool includePending)
{
	return m_reloading || (includePending && (m_reloadPending || m_autoReloading));
}

//------------------------------------------------------------------------
void CSingle::Reload(int zoomed)
{
	if(!gEnv->bMultiplayer || strlen(g_pGameCVars->g_forceHeavyWeapon->GetString()) == 0)
	{
		StartReload(zoomed);
	}
	else
	{
		FillAmmo(true);
	}
}

//------------------------------------------------------------------------
bool CSingle::CanFire(bool considerAmmo) const
{
	bool notOutOfAmmo =
		!considerAmmo ||
		!OutOfAmmo() ||
		!m_fireParams->fireparams.ammo_type_class ||
		GetClipSize() == -1;

	return
		!m_reloading &&
		!m_reloadPending &&
		(m_next_shot <= 0.0f) &&
		(m_spinUpTime <= 0.0f) &&
		PluginsAllowFire() &&
		!m_pWeapon->IsBusy() &&
		notOutOfAmmo;
}

//------------------------------------------------------------------------
void CSingle::StartFire()
{
	CCCPOINT(Single_StartFire);

	if (m_pWeapon->IsBusy())
	{
		int ammoCount = GetAmmoCount();
		if (inrange(m_next_shot, 0.0f, m_fireParams->fireparams.stabilization) && ammoCount>0)
		{
			m_firePending = true;
		}

		return;
	}

	if (!m_firing && m_next_shot < 0.f)
	{
		m_next_shot = 0.0f;
	}

	if (m_fireParams->fireparams.spin_up_time>0.0f)
	{
		m_firing = true;
		m_spinUpTime = m_fireParams->fireparams.spin_up_time;

		m_pWeapon->PlayAction(GetFragmentIds().spin_up);
		SpinUpEffect(true);
	}
	else
	{
		m_firing = Shoot(true, m_fireParams->fireparams.autoReload);
		if (m_firing)
			m_pWeapon->SetBusy(true);
	}

	m_muzzleEffect.StartFire(this);

	m_pWeapon->RequireUpdate(eIUS_FireMode);	

	RegisterHazardAreaInFrontOfWeapon();	
}

//------------------------------------------------------------------------
void CSingle::StopFire()
{
	if (m_firing)
	{
		CCCPOINT(Single_StopFire);

		m_muzzleEffect.StopFire(this);
		SmokeEffect();

		m_firing = false;
	}

	UnregisterHazardAreaInFrontOfWeapon();
}

void CSingle::StopPendingFire()
{
	m_firePending = false;
}

bool CSingle::IsSilenced() const
{
	return m_fireParams->fireparams.is_silenced;
}

//------------------------------------------------------------------------
IEntityClass* CSingle::GetAmmoType() const
{
	return m_fireParams->fireparams.ammo_type_class;
}

//------------------------------------------------------------------------
const SAmmoParams* CSingle::GetAmmoParams() const
{
	IEntityClass *pAmmoClass = GetAmmoType();

	return (pAmmoClass ? g_pGame->GetWeaponSystem()->GetAmmoParams(pAmmoClass) : NULL);
}

//------------------------------------------------------------------------
float CSingle::GetSpinUpTime() const
{
	return m_fireParams->fireparams.spin_up_time;
}

//------------------------------------------------------------------------
float CSingle::GetNextShotTime() const
{
  return m_next_shot;
}

//------------------------------------------------------------------------
void CSingle::SetNextShotTime(float time)	
{
	CActor *pActor = m_pWeapon->GetOwnerActor();
	if (pActor && pActor->IsPlayer())
	{
		time *= GetTimeBetweenShotsMultiplier(static_cast<CPlayer*>(pActor));
	}

	m_next_shot = time;
	if (time>0.0f)
		m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
float CSingle::GetFireRate() const
{
  return m_fireParams->fireparams.rate;
}

//------------------------------------------------------------------------
struct CSingle::FillAmmoAction
{
	FillAmmoAction(CSingle *_single, int reloadStartFrame):
	single(_single), _reloadStartFrame(reloadStartFrame){};
	
	CSingle *single;
	int _reloadStartFrame;

	void execute(CItem *_this)
	{
		const bool fromInventory = true;
		if(single->m_reloadStartFrame == _reloadStartFrame)
			single->FillAmmo(fromInventory);
	}
};

//------------------------------------------------------------------------
struct CSingle::EndReloadAction
{
	EndReloadAction(CSingle *_single, int zoomed, int reloadStartFrame):
	single(_single), _zoomed(zoomed), _reloadStartFrame(reloadStartFrame){};

	CSingle *single;
	int _zoomed;
	int _reloadStartFrame;

	void execute(CItem *_this)
	{
		if(single->m_reloadStartFrame == _reloadStartFrame)
			single->EndReload(_zoomed);
	}
};

void CSingle::CancelReload()
{
	m_reloadCancelled = true;
	m_reloadPending = false;
	EndReload(0);
}



void CSingle::PlayShootAction(int ammoCount)
{
	CActor *pActor = m_pWeapon->GetOwnerActor();
	const bool playerIsShooter = pActor ? pActor->IsPlayer() : false;
	int flags = CItem::eIPAF_Default;

	float speedOverride = -1.0f;

	if (IsProceduralRecoilEnabled() && pActor)
	{
		pActor->ProceduralRecoil(m_fireParams->proceduralRecoilParams.duration, m_fireParams->proceduralRecoilParams.strength, m_fireParams->proceduralRecoilParams.kickIn,m_fireParams->proceduralRecoilParams.arms);
	}

	m_pWeapon->PlayAction(GetFragmentIds().fire, 0, false, flags, speedOverride, GetFireAnimationWeight(), GetFireFFeedbackWeight());
}



void CSingle::StartReload(int zoomed)
{
	CCCPOINT(Single_StartReload);

	CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
	CPlayer *ownerPlayer = m_pWeapon->GetOwnerPlayer();
	if (ownerPlayer)
	{
		ownerPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
	}

	m_reloading = true;
	if (zoomed != 0)
		m_pWeapon->ExitZoom(true);
	m_pWeapon->SetBusy(true);
	
	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;

	//When interrupting reload to melee, scheduled reload action can get a bit "confused"
	//This way we can verify that the scheduled EndReloadAction matches this StartReload call... 
	m_reloadStartFrame = gEnv->nMainFrameID;

	int ammoCount = GetAmmoCount();

	const float speedOverride = GetReloadSpeedMultiplier(pOwnerActor);

	const CTagDefinition* pTagDefinition = NULL;
	int fragmentID = m_pWeapon->GetFragmentID("reload", &pTagDefinition);
	if(fragmentID != FRAGMENT_ID_INVALID)
	{
		TagState actionTags = TAG_STATE_EMPTY;
		if(pTagDefinition)
		{
			CTagState fragTags(*pTagDefinition);
			SetReloadFragmentTags(fragTags, ammoCount);
			actionTags = fragTags.GetMask();
		}
						
		CItemAction* pAction = new CItemAction(PP_PlayerAction, fragmentID, actionTags);
		m_pWeapon->PlayFragment(pAction, speedOverride);
	}
	
	uint32 endReloadTime = 0;
	uint32 fillAmmoTime = 0;
	if (pOwnerActor && pOwnerActor->IsPlayer())
	{
		uint32 animTime = m_pWeapon->GetCurrentAnimationTime(eIGS_Owner);
		endReloadTime = uint32(animTime * m_fireParams->fireparams.endReloadFraction);
		fillAmmoTime = uint32(animTime * m_fireParams->fireparams.fillAmmoReloadFraction);
	}
	else
	{
		endReloadTime = (uint32)(GetShared()->fireparams.ai_reload_time * 1000.0f);
		fillAmmoTime = endReloadTime;
	}
	m_pWeapon->GetScheduler()->TimerAction(fillAmmoTime, CSchedulerAction<FillAmmoAction>::Create(FillAmmoAction(this, m_reloadStartFrame)), false);
	m_pWeapon->GetScheduler()->TimerAction(endReloadTime, CSchedulerAction<EndReloadAction>::Create(EndReloadAction(this, zoomed, m_reloadStartFrame)), false);
	
	m_pWeapon->OnStartReload(m_pWeapon->GetOwnerId(), ammo);

	//Proper end reload timing for MP only (for clients, not host)
	if (gEnv->IsClient() && !gEnv->bServer)
	{
		if (pOwnerActor && pOwnerActor->IsClient() && !gEnv->bServer)
			m_reloadPending=true;
	}
}

//------------------------------------------------------------------------
void CSingle::SetReloadFragmentTags(CTagState& fragTags, int ammoCount)
{
	m_pWeapon->SetAmmoCountFragmentTags(fragTags, ammoCount);
}

//------------------------------------------------------------------------
void CSingle::EndReload(int zoomed)
{
	CCCPOINT(Single_EndReload);
	m_reloading = false;
	m_emptyclip = false;
	m_spinUpTime = m_firing?m_fireParams->fireparams.spin_up_time:0.0f;

	if (m_pWeapon->IsServer() && !m_reloadCancelled)
	{	
		m_pWeapon->SendEndReload();

		m_pWeapon->GetGameObject()->Pulse('bang');
	}

	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;
	m_pWeapon->OnEndReload(m_pWeapon->GetOwnerId(), ammo);

	m_reloadStartFrame = 0;
	m_reloadCancelled = false;
	m_pWeapon->SetBusy(false);
	const uint8 blockedActions = ( gEnv->bMultiplayer ? (CWeapon::eWeaponAction_Reload|CWeapon::eWeaponAction_Fire) : CWeapon::eWeaponAction_Reload );
	m_pWeapon->ForcePendingActions(blockedActions);
}

//------------------------------------------------------------------------
bool CSingle::FillAmmo(bool fromInventory)
{
	CCCPOINT(Single_FillAmmo);

	IEntityClass* ammo = m_fireParams->fireparams.ammo_type_class;
	bool ammoFilledUp = true;

	const int currentAmmoCount = m_pWeapon->GetAmmoCount(ammo);
	const int chamberSize = GetChamberSize();
	const int clipSize = GetClipSize();

	if (m_pWeapon->IsServer() && !m_reloadCancelled && fromInventory)
	{
		CActor * pOwnerActor = m_pWeapon->GetOwnerActor();
		bool ai = pOwnerActor ? !pOwnerActor->IsPlayer() : false;
		int inventoryCount = m_pWeapon->GetInventoryAmmoCount(ammo);
		int giveAmmo = 0;

		if (ai)
		{
			giveAmmo = max(clipSize - currentAmmoCount, 0);
		}
		else
		{
			int freeSpace = (clipSize + chamberSize) - currentAmmoCount;
			giveAmmo = min(inventoryCount, min(clipSize, freeSpace));
		}

		m_pWeapon->SetAmmoCount(ammo, currentAmmoCount + giveAmmo);
		if ((m_fireParams->fireparams.max_clips != -1) && !ai)
			m_pWeapon->SetInventoryAmmoCount(ammo, inventoryCount-giveAmmo);

		const char* ammoName = ammo ? ammo->GetName() : "";

		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponReload, ammoName, giveAmmo, (void *)(EXPAND_PTR)(m_pWeapon->GetEntityId())));
	}
	else if (!fromInventory && m_pWeapon->IsServer())
	{
		int fillAmmount = (clipSize + chamberSize);
		if (currentAmmoCount >= fillAmmount)
			ammoFilledUp = false;
		m_pWeapon->SetAmmoCount(ammo, fillAmmount);
	}

	return ammoFilledUp;
}

//------------------------------------------------------------------------
class CSingle::ScheduleAutoReload
{
public:
	ScheduleAutoReload(CSingle* pSingle, CWeapon* pWeapon)
	{
		_pThis = pSingle;
		_pWeapon = pWeapon;
	}
	void execute(CItem *item) 
	{
		_pThis->m_autoReloading = false;
		_pWeapon->SetBusy(false);
		_pWeapon->OnFireWhenOutOfAmmo();
	}
private:
	CSingle* _pThis;
	CWeapon* _pWeapon;
};

bool CSingle::Shoot(bool resetAnimation, bool autoreload, bool isRemote)
{
	CCCPOINT(Single_Shoot);
	IEntityClass* pAmmoClass = m_fireParams->fireparams.ammo_type_class;

	const EntityId ownerEntityId = m_pWeapon->GetOwnerId();
	CActor *pActor = m_pWeapon->GetOwnerActor();
	
	const bool playerIsShooter = pActor ? pActor->IsPlayer() : false;
	const bool clientIsShooter = pActor ? pActor->IsClient() : false;

	const int clipSize = GetClipSize();
	int ammoCount = (clipSize != 0) ? m_pWeapon->GetAmmoCount(pAmmoClass) : m_pWeapon->GetInventoryAmmoCount(pAmmoClass);

	m_firePending = false;
	if (!CanFire(true))
	{
		if ((ammoCount <= 0) && !m_reloading && !m_reloadPending)
		{
			m_pWeapon->PlayAction(GetFragmentIds().empty_clip);
			m_pWeapon->OnFireWhenOutOfAmmo();
		}
		return false;
	}

	bool bHit = false;
	ray_hit rayhit;	 
	rayhit.pCollider = 0;

	Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
	Vec3 pos = GetFiringPos(hit);
	Vec3 dir = GetFiringDir(hit, pos);		//Spread was already applied if required in GetProbableHit( )
	Vec3 vel = GetFiringVelocity(dir);  

	PlayShootAction(ammoCount);

	// debug

/*
	DebugShoot shoot;
	shoot.pos=pos;
	shoot.dir=dir;
	shoot.hit=hit;
	g_shoots.push_back(shoot);*/
	
	CheckNearMisses(hit, pos, dir, (hit-pos).len(), 1.0f);

	int ammoCost = GetShootAmmoCost(playerIsShooter);
	ammoCost = min(ammoCount, ammoCost);

	EntityId ammoEntityId = 0;
	int ammoPredicitonHandle = 0;
	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, false);
	if (pAmmo)
	{
		ammoEntityId = pAmmo->GetEntityId();
		ammoPredicitonHandle = pAmmo->GetGameObject()->GetPredictionHandle();

		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

		CProjectile::SProjectileDesc projectileDesc(
			ownerEntityId, m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), GetDamage(), m_fireParams->fireparams.damage_drop_min_distance,
			m_fireParams->fireparams.damage_drop_per_meter, m_fireParams->fireparams.damage_drop_min_damage, m_fireParams->fireparams.hitTypeId,
			m_fireParams->fireparams.bullet_pierceability_modifier, m_pWeapon->IsZoomed());
		projectileDesc.pointBlankAmount = m_fireParams->fireparams.point_blank_amount;
		projectileDesc.pointBlankDistance = m_fireParams->fireparams.point_blank_distance;
		projectileDesc.pointBlankFalloffDistance = m_fireParams->fireparams.point_blank_falloff_distance;
		if (m_fireParams->fireparams.ignore_damage_falloff)
			projectileDesc.damageFallOffAmount = 0.0f;
		pAmmo->SetParams(projectileDesc);
		// this must be done after owner is set
		pAmmo->InitWithAI();
    
		pAmmo->SetDestination(m_pWeapon->GetDestination());

		const float speedScale = (float)__fsel(-GetShared()->fireparams.speed_override, 1.0f, GetShared()->fireparams.speed_override * __fres(GetAmmoParams()->speed));

		OnSpawnProjectile(pAmmo);

		pAmmo->Launch(pos, dir, vel, m_speed_scale * speedScale);
		pAmmo->CreateBulletTrail(hit);
		pAmmo->SetKnocksTargetInfo( m_fireParams );
   
		pAmmo->SetRemote(isRemote || (!gEnv->bServer && m_pWeapon->IsProxyWeapon()));
		pAmmo->SetFiredViaProxy(m_pWeapon->IsProxyWeapon());

		const STracerParams * tracerParams = &m_fireParams->tracerparams;

		int frequency = tracerParams->frequency;

		bool emit = false;
		if (frequency > 0)
		{
			if(m_pWeapon->GetStats().fp)
				emit = (!tracerParams->geometryFP.empty() || !tracerParams->effectFP.empty()) && ((ammoCount == clipSize) || (ammoCount%frequency == 0));
			else
				emit = (!tracerParams->geometry.empty() || !tracerParams->effect.empty()) && ((ammoCount == clipSize) || (ammoCount%frequency==0));
		}

		if (emit)
		{
			EmitTracer(pos, hit, tracerParams, pAmmo);
		}

		m_projectileId = pAmmo->GetEntityId();

		if (clientIsShooter && pAmmo->IsPredicted() && gEnv->IsClient() && gEnv->bServer)
		{
			pAmmo->GetGameObject()->BindToNetwork();
		}

		pAmmo->SetAmmoCost(ammoCost);
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = pAmmoClass != NULL ? pAmmoClass->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	}

	m_muzzleEffect.Shoot(this, hit, m_barrelId);
	RecoilImpulse(pos, dir);

	m_fired = true;
	SetNextShotTime(m_next_shot + m_next_shot_dt);

	if (++m_barrelId == m_fireParams->fireparams.barrel_count)
	{
		m_barrelId = 0;
	}
	
	ammoCount -= ammoCost;
	if (ammoCount <= m_fireParams->fireparams.minimum_ammo_count)
		ammoCount = 0;

	if (clipSize != -1)
	{
		if (clipSize!=0)
			m_pWeapon->SetAmmoCount(pAmmoClass, ammoCount);
		else
			m_pWeapon->SetInventoryAmmoCount(pAmmoClass, ammoCount);
	}

	const EntityId shooterEntityId = ownerEntityId ? ownerEntityId : m_pWeapon->GetHostId();
	OnShoot(shooterEntityId, ammoEntityId, pAmmoClass, pos, dir, vel);

	const SThermalVisionParams& thermalParams = m_fireParams->thermalVisionParams;
	m_pWeapon->AddShootHeatPulse(pActor, thermalParams.weapon_shootHeatPulse, thermalParams.weapon_shootHeatPulseTime,
	thermalParams.owner_shootHeatPulse, thermalParams.owner_shootHeatPulseTime);
	if (OutOfAmmo())
	{
		OnOutOfAmmo(pActor, autoreload);
	}

	//CryLog("RequestShoot - pos(%f,%f,%f), dir(%f,%f,%f), hit(%f,%f,%f)", pos.x, pos.y, pos.z, dir.x, dir.y, dir.z, hit.x, hit.y, hit.z);
	m_pWeapon->RequestShoot(pAmmoClass, pos, dir, vel, hit, m_speed_scale, ammoPredicitonHandle, false);

#ifdef DEBUG_AIMING_AND_SHOOTING

	static ICVar* pAimDebug = gEnv->pConsole->GetCVar("g_aimdebug");
	if (pAimDebug->GetIVal()) 
	{
		IPersistantDebug* pDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
		pDebug->Begin("CSingle::Shoot", false);
		pDebug->AddSphere(hit, 0.05f, ColorF(0,0,1,1), 10.f);
		pDebug->AddDirection(pos, 0.25f, dir, ColorF(0,0,1,1), 1.f);
	}

	//---------------------------------------------------------------------------
	// TODO remove when aiming/fire direction is working
	// debugging aiming dir
	if(++DGB_curLimit>DGB_ShotCounter)	DGB_curLimit = DGB_ShotCounter;
	if(++DGB_curIdx>=DGB_ShotCounter)	DGB_curIdx = 0;
	DGB_shots[DGB_curIdx].dst=pos+dir*200.f;
	DGB_shots[DGB_curIdx].src=pos;
	//---------------------------------------------------------------------------
#endif

	return true;
}

//------------------------------------------------------------------------
int CSingle::GetShootAmmoCost(bool playerIsShooter)
{
	int cost = 1;

	if (m_fireParams->fireparams.fake_fire_rate && playerIsShooter)
		cost += m_fireParams->fireparams.fake_fire_rate; 

	return cost;
}

//------------------------------------------------------------------------
bool CSingle::ShootFromHelper(const Vec3 &eyepos, const Vec3 &probableHit) const
{
	Vec3 dp(eyepos-probableHit);
	return dp.len2()>(WEAPON_HIT_MIN_DISTANCE*WEAPON_HIT_MIN_DISTANCE);
}

//------------------------------------------------------------------------
bool CSingle::HasFireHelper() const
{ 
  return !m_fireParams->fireparams.helper[m_pWeapon->GetStats().fp?0:1].empty();
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFireHelperPos() const
{
  if (HasFireHelper())
  {
    int id = m_pWeapon->GetStats().fp?0:1;
    int slot = id ? eIGS_ThirdPerson: eIGS_FirstPerson;

    return m_pWeapon->GetSlotHelperPos(slot, m_fireParams->fireparams.helper[id].c_str(), true);
  }

  return Vec3(ZERO);
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFireHelperDir() const
{
  if (HasFireHelper())
  {
    int id = m_pWeapon->GetStats().fp?0:1;
    int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;

    return m_pWeapon->GetSlotHelperRotation(slot, m_fireParams->fireparams.helper[id].c_str(), true).GetColumn(1);
  }  

  return FORWARD_DIRECTION;
}

namespace
{
  IPhysicalEntity* GetSkipPhysics(IEntity* pEntity)
  {
    IPhysicalEntity* pPhysics = pEntity->GetPhysics();    
    if (pPhysics && pPhysics->GetType() == PE_LIVING)
    {
      if (ICharacterInstance* pCharacter = pEntity->GetCharacter(0)) 
      {
        if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
          pPhysics = pCharPhys;
      }
    }    
    return pPhysics;
  }
}

//------------------------------------------------------------------------
void CSingle::GetSkipEntities(CWeapon* pWeapon, PhysSkipList& skipList)
{
	IEntity* weaponEntity = pWeapon->GetEntity();

	if (IPhysicalEntity* weaponPhysEntity = weaponEntity->GetPhysics())
	{
		stl::push_back_unique(skipList, weaponPhysEntity);
	}

	if (CActor *pActor = pWeapon->GetOwnerActor())
	{
		if(IPhysicalEntity* pActorSkipPhysics = GetSkipPhysics(pActor->GetEntity()))
			stl::push_back_unique(skipList, pActorSkipPhysics);

		if (IVehicle* pVehicle = pActor->GetLinkedVehicle())
		{
			IEntity* pVehicleEntity = pVehicle->GetEntity();

			if(IPhysicalEntity* pVehicleSkipPhysics = pVehicleEntity->GetPhysics())
				stl::push_back_unique(skipList, pVehicleSkipPhysics);

			int childCount = pVehicleEntity->GetChildCount(); 
			for (int i = 0; i < childCount; ++i)
			{
				if (IPhysicalEntity* pPhysics = GetSkipPhysics(pVehicleEntity->GetChild(i)))        
					stl::push_back_unique(skipList, pPhysics);
			}
		}
	}
}

//------------------------------------------------------------------------
Vec3 CSingle::GetProbableHit(float maxRayLength, bool *pbHit, ray_hit *pHit) const
{
	static Vec3 pos(ZERO), dir(FORWARD_DIRECTION);

	CActor *pActor = m_pWeapon->GetOwnerActor();
	IEntity *pWeaponEntity = m_pWeapon->GetEntity();

	static PhysSkipList skipList;
	skipList.clear();
	GetSkipEntities(m_pWeapon, skipList);

	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();      
	if (pLocator)
	{
		Vec3 hit(ZERO);
		if (pLocator->GetProbableHit(m_pWeapon->GetEntityId(), this, hit))
			return hit;
	}

	float rayLength = maxRayLength;
	const SAmmoParams *pAmmoParams = GetAmmoParams();
	if (pAmmoParams)
	{
		//Benito - It could happen that the lifetime/speed are zero, so ensure at least some minimum distance check
		float maxSpeed = pAmmoParams->speed;
		if (pAmmoParams->pHomingParams)
		{
			maxSpeed = max(maxSpeed, pAmmoParams->pHomingParams->m_maxSpeed);
		}
		rayLength = clamp_tpl(maxSpeed * pAmmoParams->lifetime, 5.0f, rayLength);
	}

	bool requiresProbableHitTest = false;

	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);

		pos = info.weaponPosition;

		if (!pActor->IsPlayer())
		{
			dir = (info.fireTarget-pos).normalized();
		}
		else
		{
			dir = info.fireDirection;
			requiresProbableHitTest = true; // Only players use probably hit info, AI uses fire target always
		}

		CRY_ASSERT(dir.IsUnit());

		ApplyAutoAim(dir, pos);
		dir = ApplySpread(dir, GetSpread()) * rayLength;
	}
	else
	{ 
		// fallback    
		pos = GetFiringPos(Vec3Constants<float>::fVec3_Zero);
		dir = rayLength * GetFiringDir(Vec3Constants<float>::fVec3_Zero, pos);
	}

	if (requiresProbableHitTest)
	{
		static ray_hit hit;	

		// use the ammo's pierceability
		uint32 flags=(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit|rwi_colltype_any|rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces;
		uint8 pierceability = 8;
		if (pAmmoParams && pAmmoParams->pParticleParams && !is_unused(pAmmoParams->pParticleParams->iPierceability))
		{
			pierceability=pAmmoParams->pParticleParams->iPierceability;
		}
		flags |= pierceability;

		if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, ent_all, flags, &hit, 1, !skipList.empty() ? &skipList[0] : NULL, skipList.size()))
		{
			if (pbHit)
				*pbHit=true;
			if (pHit)
				*pHit=hit;

			// players in vehicles need to check position isn't behind target (since info.weaponPosition is 
			//	actually the camera pos - argh...)
			if(pbHit && *pbHit && pActor && pActor->GetLinkedVehicle())
			{
				Matrix34 tm = m_pWeapon->GetWorldTM();
				Vec3 wppos = tm.GetTranslation();

				// find the plane perpendicular to the aim direction that passes through the weapon position
				Vec3 n = dir.GetNormalizedSafe();
				float d = -(n.Dot(wppos));
				Plane	plane(n, d);
				float dist = plane.DistFromPlane(pos);

				if(dist < 0.0f)
				{
					// now do a new intersection test forwards from the point where the previous rwi intersected the plane...
					Vec3 newPos = pos - dist * n;
					if (gEnv->pPhysicalWorld->RayWorldIntersection(newPos, dir, ent_all,
						rwi_stop_at_pierceable|rwi_ignore_back_faces, &hit, 1, !skipList.empty() ? &skipList[0] : NULL, skipList.size()))
					{
						*pbHit=true;
						if (pHit)
							*pHit=hit;
					}
				}
			}

			return hit.pt;
		}
	}


	if (pbHit)
		*pbHit=false;

	return pos + dir;
}

//------------------------------------------------------------------------
void CSingle::DeferGetProbableHit(float maxRayLength)
{
	static Vec3 pos(ZERO), dir(FORWARD_DIRECTION);
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IEntity *pWeaponEntity = m_pWeapon->GetEntity();

	static PhysSkipList skipList;
	skipList.clear();
	GetSkipEntities(m_pWeapon, skipList);

	float rayLength = maxRayLength;
	const SAmmoParams *pAmmoParams = GetAmmoParams();
	if (pAmmoParams)
	{
		//Benito - It could happen that the lifetime/speed are zero, so ensure at least some minimum distance check
		float maxSpeed = pAmmoParams->speed;
		if (pAmmoParams->pHomingParams)
		{
			maxSpeed = max(maxSpeed, pAmmoParams->pHomingParams->m_maxSpeed);
		}
		rayLength = clamp_tpl(maxSpeed * pAmmoParams->lifetime, 5.0f, rayLength);
	}

	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	if (pMC)
	{ 
		SMovementState info;
		pMC->GetMovementState(info);

		pos = info.weaponPosition;

		if (!pActor->IsPlayer())
		{      
			dir = (info.fireTarget-pos).normalized();
		}
		else
		{
			dir = info.fireDirection;
			CRY_ASSERT(dir.IsUnit());
		}
		dir = ApplySpread(dir, GetSpread()) * rayLength;
	}
	else
	{ 
		// fallback    
		pos = GetFiringPos(Vec3Constants<float>::fVec3_Zero);
		dir = rayLength * GetFiringDir(Vec3Constants<float>::fVec3_Zero, pos);
	}

	static ray_hit hit;

	uint32 flags=(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit|rwi_colltype_any|rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces;
	uint8 pierceability=8;
	if (pAmmoParams && pAmmoParams->pParticleParams && !is_unused(pAmmoParams->pParticleParams->iPierceability))
		pierceability=pAmmoParams->pParticleParams->iPierceability;
	flags |= pierceability;

	SProbableHitInfo *probableHit = NULL;
	for(int i=0; i < MAX_PROBABLE_HITS; i++)
	{
		if(m_probableHits[i].m_state == eProbableHitDeferredState_none)
		{
			probableHit = &m_probableHits[i];
			break;
		}
	}

	CRY_ASSERT(probableHit);

	if(probableHit)	// maybe too many shots during a client's loading screen?
	{
		probableHit->m_hit = pos + dir;	// default value, if we hit nothing

		CRY_ASSERT(probableHit->m_queuedRayID == 0);
		CRY_ASSERT(m_queuedProbableHits.size() < m_queuedProbableHits.max_size());

		probableHit->m_queuedRayID = g_pGame->GetRayCaster().Queue(
			RayCastRequest::MediumPriority,
			RayCastRequest(pos, dir,
			ent_all,
			flags,
			!skipList.empty() ? &skipList[0] : NULL,
			skipList.size()),
			functor(*probableHit, &SProbableHitInfo::OnDataReceived));

		probableHit->m_state = eProbableHitDeferredState_dispatched;

		m_queuedProbableHits.push_back(probableHit);
	}
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFiringPos(const Vec3 &probableHit) const
{
  static Vec3 pos;
	
  IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();
	if (pLocator)
  { 
		if (pLocator->GetFiringPos(m_pWeapon->GetEntityId(), this, pos))
      return pos;
  }
	
  int id = m_pWeapon->GetStats().fp ? 0 : 1;
  int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;
  
  pos = m_pWeapon->GetWorldPos();
  
	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	
  if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);

		pos = info.weaponPosition;

		// FIXME
		// should be getting it from MovementCotroller (same for AIProxy::QueryBodyInfo)
		// update: now AI always should be using the fire_pos from movement controller
		if (/*pActor->IsPlayer() && */(HasFireHelper() && ShootFromHelper(pos, probableHit)))
		{
			// FIXME
			// making fire pos be at eye when animation is not updated (otherwise shooting from ground)
			bool	isCharacterVisible(false);
			
			IEntity *pEntity(pActor->GetEntity());
			ICharacterInstance * pCharacter(pEntity->GetCharacter(0));

			if(pCharacter && pCharacter->IsCharacterVisible()!=0)
				isCharacterVisible = true;

			if(isCharacterVisible)
				pos = m_pWeapon->GetSlotHelperPos(slot, m_fireParams->fireparams.helper[id].c_str(), true);
		}
	}
  else
  {
    // when no MC, fall back to helper
    if (HasFireHelper())
    {
      pos = m_pWeapon->GetSlotHelperPos(slot, m_fireParams->fireparams.helper[id].c_str(), true);
    }
  }

	return pos;
}


//------------------------------------------------------------------------
Vec3 CSingle::GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const
{
	Vec3 dir(FORWARD_DIRECTION);
	
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();
	if (pLocator)
	{	 
		if (pLocator->GetFiringDir(m_pWeapon->GetEntityId(), this, dir, probableHit, firingPos))
			return dir;		
	}

	int id = m_pWeapon->GetStats().fp?0:1;
	int slot = id ? eIGS_ThirdPerson : eIGS_FirstPerson;

	dir = m_pWeapon->GetWorldTM().GetColumn1();

	CActor *pActor = m_pWeapon->GetOwnerActor();
	IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
	if (pMC)
	{
		SMovementState info;
		pMC->GetMovementState(info);

		dir = info.fireDirection;

		if (HasFireHelper() && ShootFromHelper(info.weaponPosition, probableHit))
		{
			if (!pActor->IsPlayer())
			{
				dir = (info.fireTarget-firingPos).normalized();
			}
			else if(!probableHit.IsZero())
			{
				dir = (probableHit-firingPos).normalized();
			}
		}
		else if (!pActor->IsThirdPerson())
		{
			if(!probableHit.IsZero())
			{
				//This direction contains already the spread calculation
				dir = (probableHit-firingPos).normalized();
			}
		}
	}  
	else if (HasFireHelper())
	{ 
		// if no MC, fall back to helper 
		 dir = m_pWeapon->GetSlotHelperRotation(slot, m_fireParams->fireparams.helper[id].c_str(), true).GetColumn(1);
	}
  
	return dir;
}

//------------------------------------------------------------------------
Vec3 CSingle::GetFiringVelocity(const Vec3 &dir) const
{
	IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator();
	if (pLocator)
  {
    Vec3 vel;
    if (pLocator->GetFiringVelocity(m_pWeapon->GetEntityId(), this, vel, dir))
      return vel;
  }

	CActor *pActor=m_pWeapon->GetOwnerActor();
	if (pActor)
	{
		IPhysicalEntity *pPE=pActor->GetEntity()->GetPhysics();
		if (pPE)
		{
			pe_status_dynamics sv;
			if (pPE->GetStatus(&sv))
			{
				if (sv.v.len2()>0.01f)
				{
					float dot = max(sv.v.GetNormalized().Dot(dir), 0.0f);

					return sv.v*dot;
				}
			}
		}
	}

	return Vec3(0,0,0);
}

//------------------------------------------------------------------------
Vec3 CSingle::ApplySpread(const Vec3 &dir, float spread, int quadrant) const
{
	//Claire: Spread can now be quadrant based to allow us to get a more even spread on the shotgun
	//Quad 0 = +x, +z
	//Quad 1 = +x, -z
	//Quad 2 = -x, -z
	//Quad 3 = -x, +z

	Ang3 angles=Ang3::GetAnglesXYZ(Matrix33::CreateRotationVDir(dir));
	float rx = 0.f;
	float rz = 0.f;

	if(quadrant < 0)
	{
		CCCPOINT(Single_ApplySpreadRandom);

		rx = cry_random(-0.5f, 0.5f);
		rz = cry_random(-0.5f, 0.5f);
	}
	else
	{
		CCCPOINT(Single_ApplySpreadQuadrant);
		CRY_ASSERT_MESSAGE(quadrant < 4, "Invalid quadrant provided to apply spread");

		rx = cry_random(0.0f, 0.5f);
		rz = cry_random(0.0f, 0.5f);

		if(quadrant > 1)
		{
			rx *= -1.f; 
		}

		if(quadrant == 1 || quadrant == 2)
		{
			rz *= -1.f;
		}
	}
	
	angles.x+=rx*DEG2RAD(spread);
	angles.z+=rz*DEG2RAD(spread);

	return Matrix33::CreateRotationXYZ(angles).GetColumn(1).normalized();
}

//------------------------------------------------------------------------
void CSingle::ApplyAutoAim( Vec3 &rDir, const Vec3 &pos ) const
{
	AlterFiringDirection(pos,rDir);
}

//------------------------------------------------------------------------
Vec3 CSingle::GetTracerPos(const Vec3 &firingPos, const STracerParams* useTracerParams)
{
	int id=m_pWeapon->GetStats().fp?0:1;
	int slot=id ? eIGS_ThirdPerson : eIGS_FirstPerson;
	const char * helper = useTracerParams->helper[id].c_str();

	if (!helper[0])
		return firingPos;

	return m_pWeapon->GetSlotHelperPos(slot, helper, true);
}



void CSingle::SmokeEffect(bool effect)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (effect)
	{		
		Vec3 dir(0,1.0f,0);
		CActor *pActor = m_pWeapon->GetOwnerActor();
		IMovementController * pMC = pActor ? pActor->GetMovementController() : 0;
		if (pMC)
		{
			SMovementState info;
			pMC->GetMovementState(info);
			dir = info.aimDirection;
		}

		int slot = m_pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if(m_smokeEffectId)
		{
			m_pWeapon->DetachEffect(m_smokeEffectId);
			m_smokeEffectId = 0;
		}

		if (!m_fireParams->muzzlesmoke.effect[id].empty())
		{		
			m_smokeEffectId = m_pWeapon->AttachEffect(slot, m_fireParams->muzzlesmoke.helperFromAccessory, m_fireParams->muzzlesmoke.effect[id].c_str(), m_fireParams->muzzlesmoke.helper[id].c_str());
		}
	}
}

//------------------------------------------------------------------------
void CSingle::SpinUpEffect(bool attach)
{ 
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if(m_spinUpEffectId)
	{
		m_pWeapon->DetachEffect(m_spinUpEffectId);
		m_spinUpEffectId=0;
	}

	if (attach)
	{
		int slot = m_pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
		int id = m_pWeapon->GetStats().fp ? 0 : 1;

		if (!m_fireParams->spinup.effect[0].empty() || !m_fireParams->spinup.effect[1].empty())
		{
			m_spinUpEffectId = m_pWeapon->AttachEffect(slot, m_fireParams->spinup.helperFromAccessory, m_fireParams->spinup.effect[id].c_str(), 
			m_fireParams->spinup.helper[id].c_str(), Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_OneY, 1.0f, false);
		}

		m_spinUpTimer = (uint32)(m_fireParams->spinup.time[id]);
	}
}

//------------------------------------------------------------------------
int CSingle::GetDamage() const
{
	CActor* pActor = m_pWeapon->GetOwnerActor();
	const bool playerIsShooter = pActor ? pActor->IsPlayer() : false;

	int damage = m_fireParams->fireparams.damage;
	if (!playerIsShooter)
		damage += m_fireParams->fireparams.npc_additional_damage;

	const bool canOvercharge = m_pWeapon->GetSharedItemParams()->params.can_overcharge;
	if (canOvercharge)
	{
		const float overchargeModifier = pActor ? pActor->GetOverchargeDamageScale() : 1.0f;
		damage = (int)(damage * overchargeModifier);
	}

	return damage;
}

//------------------------------------------------------------------------
bool CSingle::DoesFireRayIntersectFrustum(const Vec3& vPos, bool& firePosInFrustum)
{
	if(g_pGameCVars->g_mpCullShootProbablyHits)
	{
		// If the firing position is inside the frustum, then the ray definitely intersects with the frustum
		firePosInFrustum = (gEnv->p3DEngine->GetRenderingCamera().IsPointVisible(vPos)==CULL_OVERLAP);
	}
	else
	{
		firePosInFrustum = true; // Set to true so culling is off
	}

	if(firePosInFrustum)
	{
		return true; // If the fire pos is in frustum, then ray definitely intersects frustum
	}
	else
	{
		Vec3 vDir = GetFiringDir(Vec3Constants<float>::fVec3_Zero, vPos); // Get direction without spread applied
		Ray fireRay(vPos,vDir);
		Vec3 collisionPoint;
		const bool isSingleSided = false;
		bool rayIntersectsPlane = false;
		const Plane* frustumPlane = NULL;
		int planeIntersectCount = 0;
		Vec3 intersectedPlanePoints[FRUSTUM_PLANES];

		// Check ray against every frustum plane
		for(int p=0; p<FRUSTUM_PLANES; p++)
		{
			frustumPlane = gEnv->p3DEngine->GetRenderingCamera().GetFrustumPlane(p);
			rayIntersectsPlane = Intersect::Ray_Plane(fireRay,*frustumPlane,collisionPoint,isSingleSided);
			if(rayIntersectsPlane)
			{
				intersectedPlanePoints[planeIntersectCount] = collisionPoint;
				planeIntersectCount++;
			}
		}

		// If there is more than 1 collision then the ray does collide with the frustum, use the middle point between each collision and
		// test if that point is inside the frustum
		bool averagePointInFrustum = false;
		Vec3 averagePoint;
		if(planeIntersectCount > 1)
		{
			for(int p=0; p<planeIntersectCount; p++)
			{
				for(int c=0; c<planeIntersectCount; c++)
				{
					if(c!=p)
					{
						averagePoint = LERP(intersectedPlanePoints[p],intersectedPlanePoints[c],0.5f);
						averagePointInFrustum = (gEnv->p3DEngine->GetRenderingCamera().IsPointVisible(averagePoint) == CULL_OVERLAP);
						if(averagePointInFrustum)
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CSingle::DoesFireLineSegmentIntersectFrustum(const Vec3& start, const Vec3& end)
{
	Lineseg lineSeg(start,end);
	const Plane* frustumPlane = NULL;
	
	Vec3 collisionPoint;
	const bool isSingleSided = false;

	// Test line segment against each frustum plane
	for(int p=0; p<FRUSTUM_PLANES; p++)
	{
		frustumPlane = gEnv->p3DEngine->GetRenderingCamera().GetFrustumPlane(p);

		if(Intersect::Segment_Plane(lineSeg,*frustumPlane,collisionPoint,isSingleSided))
		{
			return true;
		}
	}
	
	return false;
}
void CSingle::NetShoot(const Vec3 &hit, int predictionHandle)
{
	Vec3 vPos = GetFiringPos(hit);
	Vec3 vHit(hit);

	bool firePosInFrustum = false;
	bool visible = DoesFireRayIntersectFrustum(vPos,firePosInFrustum);

	if(visible)
	{
		m_muzzleEffect.Shoot(this, vHit, m_barrelId);

		IWeaponFiringLocator *pLocator = m_pWeapon->GetFiringLocator(); 

		if(pLocator && pLocator->GetProbableHit(m_pWeapon->GetEntityId(), this, vHit))
		{
			vPos = GetFiringPos(vHit); // recalc pos now we have an actual hit
		}
		else 
		{
			vHit = hit; //reset in case GetProbableHit altered vHit but returned false
			if(hit.IsZeroFast())
			{
				DeferGetProbableHit(WEAPON_HIT_RANGE);
				m_pWeapon->RequireUpdate(eIUS_FireMode);
				return;
			}
		}
	}
	else
	{
		vHit.zero();
	}
	
	Vec3 vDir = GetFiringDir(vHit, vPos);
	Vec3 vVel = GetFiringVelocity(vDir);

	NetShootEx(vPos, vDir, vVel, vHit, 1.0f, predictionHandle);
}

void CSingle::NetShootDeferred(const Vec3 &inHit)
{
	Vec3 vHit(inHit);
	Vec3 vPos = GetFiringPos(vHit);
	Vec3 vDir = GetFiringDir(vHit, vPos);		//Spread was already applied if required in DeferGetProbableHit( )
	Vec3 vVel = GetFiringVelocity(vDir);

	// if not visible and not intersecting the fustrum
	if(!(gEnv->p3DEngine->GetRenderingCamera().IsPointVisible(vPos) == CULL_OVERLAP) && DoesFireLineSegmentIntersectFrustum(vPos,vHit) == false)
	{
		vHit.zero(); // Line segment is not in frustum, so zero out the hit so no effects are spawned for it
	}

	NetShootEx(vPos, vDir, vVel, vHit, 1.0f, 0);
}

//------------------------------------------------------------------------
void CSingle::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle)
{
	CCCPOINT(Single_NetShoot);

	IEntityClass* ammo		= m_fireParams->fireparams.ammo_type_class;
	CActor* pActor				= m_pWeapon->GetOwnerActor();
	int clipSize					= GetClipSize();
	int weaponAmmoCount = m_pWeapon->GetAmmoCount(ammo);
	int inventoryAmmoCount = m_pWeapon->GetInventoryAmmoCount(ammo);
	int ammoCount					= (clipSize == 0) ? inventoryAmmoCount : weaponAmmoCount;
	bool playerIsShooter	= pActor ? pActor->IsPlayer() : false;
	FragmentID action		= (ammoCount == 1 || m_fireParams->fireparams.no_cock) ? GetFragmentIds().fire : GetFragmentIds().fire_cock;

	int ammoCost = m_fireParams->fireparams.fake_fire_rate ? m_fireParams->fireparams.fake_fire_rate : 1;
	ammoCost = min(ammoCount, ammoCost);

	if (IsProceduralRecoilEnabled() && pActor)
	{
		pActor->ProceduralRecoil(m_fireParams->proceduralRecoilParams.duration, m_fireParams->proceduralRecoilParams.strength, m_fireParams->proceduralRecoilParams.kickIn,m_fireParams->proceduralRecoilParams.arms);
	}

	m_pWeapon->PlayAction(action, 0, false, CItem::eIPAF_Default);

	bool isVisibleToClient = (hit.IsZero() == false);
	
	CProjectile *pAmmo = m_pWeapon->SpawnAmmo(m_fireParams->fireparams.spawn_ammo_class, true);
	if (pAmmo)
	{	
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId, string().Format("Invalid hit type '%s' in fire params for '%s'", m_fireParams->fireparams.hit_type.c_str(), m_pWeapon->GetEntity()->GetName()));
		CRY_ASSERT_MESSAGE(m_fireParams->fireparams.hitTypeId == g_pGame->GetGameRules()->GetHitTypeId(m_fireParams->fireparams.hit_type.c_str()), "Sanity Check Failed: Stored hit type id does not match the type string, possibly CacheResources wasn't called on this weapon type");

		CProjectile::SProjectileDesc projectileDesc(
			m_pWeapon->GetOwnerId(), m_pWeapon->GetHostId(), m_pWeapon->GetEntityId(), m_fireParams->fireparams.damage,
			m_fireParams->fireparams.damage_drop_min_distance, m_fireParams->fireparams.damage_drop_per_meter, m_fireParams->fireparams.damage_drop_min_damage,
			m_fireParams->fireparams.hitTypeId, m_fireParams->fireparams.bullet_pierceability_modifier, m_pWeapon->IsZoomed());
		projectileDesc.pointBlankAmount = m_fireParams->fireparams.point_blank_amount;
		projectileDesc.pointBlankDistance = m_fireParams->fireparams.point_blank_distance;
		projectileDesc.pointBlankFalloffDistance = m_fireParams->fireparams.point_blank_falloff_distance;
		if (m_fireParams->fireparams.ignore_damage_falloff)
			projectileDesc.damageFallOffAmount = 0.0f;
		pAmmo->SetParams(projectileDesc);
		
		pAmmo->SetDestination(m_pWeapon->GetDestination());

		pAmmo->SetRemote(true);

		m_speed_scale=extra;
		pAmmo->Launch(pos, dir, vel, m_speed_scale);
		pAmmo->CreateBulletTrail(hit);

		const STracerParams * tracerParams = &m_fireParams->tracerparams;
		bool emit = (tracerParams->frequency > 0) ? (!tracerParams->geometry.empty() || !tracerParams->effect.empty()) && (ammoCount == clipSize || ((ammoCount % tracerParams->frequency) == 0)) : false;

		if ((emit) && (isVisibleToClient))
		{
			EmitTracer(pos, hit, tracerParams, pAmmo);
		}

		m_projectileId = pAmmo->GetEntity()->GetId();

		pAmmo->SetAmmoCost(ammoCost);
	}

	if (m_pWeapon->IsServer())
	{
		const char *ammoName = ammo != NULL ? ammo->GetName() : NULL;
		g_pGame->GetIGameFramework()->GetIGameplayRecorder()->Event(m_pWeapon->GetOwner(), GameplayEvent(eGE_WeaponShot, ammoName, 1, (void *)(EXPAND_PTR)m_pWeapon->GetEntityId()));
	
	}

  RecoilImpulse(pos, dir);

	m_fired = true;
	m_next_shot = 0.0f;

  if (++m_barrelId == m_fireParams->fireparams.barrel_count)
    m_barrelId = 0;

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

	OnShoot(m_pWeapon->GetOwnerId(), pAmmo?pAmmo->GetEntity()->GetId():0, ammo, pos, dir, vel);

	const SThermalVisionParams& thermalParams = m_fireParams->thermalVisionParams;
	m_pWeapon->AddShootHeatPulse(pActor, thermalParams.weapon_shootHeatPulse, thermalParams.weapon_shootHeatPulseTime,
		thermalParams.owner_shootHeatPulse, thermalParams.owner_shootHeatPulseTime);

	if (OutOfAmmo())
		m_pWeapon->OnOutOfAmmo(ammo);

	if (pAmmo && predictionHandle && pActor)
	{
		pAmmo->GetGameObject()->RegisterAsValidated(pActor->GetGameObject(), predictionHandle);
		pAmmo->GetGameObject()->BindToNetwork();
	}

	m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CSingle::ReplayShoot()
{
	CCCPOINT(Single_ReplayShoot);

	Vec3 pos = GetFiringPos(ZERO);
	bool firePosInFrustum = false;
	if(DoesFireRayIntersectFrustum(pos, firePosInFrustum))
	{
		bool bHit = false;
		ray_hit rayhit;	 
		rayhit.pCollider = 0;

		Vec3 hit = GetProbableHit(WEAPON_HIT_RANGE, &bHit, &rayhit);
		pos = GetFiringPos(hit); // Recalc pos now we have a hit position

		if (firePosInFrustum || DoesFireLineSegmentIntersectFrustum(pos, hit))
		{
			Vec3 dir = GetFiringDir(hit, pos);
			Vec3 vel = GetFiringVelocity(dir);

			const STracerParams * tracerParams = &m_fireParams->tracerparams;
			if (tracerParams->frequency > 0 && (!tracerParams->geometry.empty() || !tracerParams->effect.empty()))
			{
				if(cry_random(0, tracerParams->frequency - 1) == 0)
				{
					EmitTracer(pos, hit, tracerParams, NULL);
				}
			}

			m_muzzleEffect.Shoot(this, hit, m_barrelId);
	
			if (++m_barrelId == m_fireParams->fireparams.barrel_count)
				m_barrelId = 0;
		}
	}

	OnReplayShoot();
}

//------------------------------------------------------------------------
void CSingle::RecoilImpulse(const Vec3& firingPos, const Vec3& firingDir)
{
	m_recoil.RecoilImpulse(firingPos, firingDir);
}

//------------------------------------------------------------------------
void CSingle::CheckNearMisses(const Vec3 &probableHit, const Vec3 &pos, const Vec3 &dir, float range, float radius)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	if (IPerceptionManager::GetInstance())
	{
		EntityId ownerId = m_pWeapon->GetOwnerId();
		if ((ownerId != 0) && !GetShared()->fireparams.is_silenced)
		{
			// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
			CActor* pActor = m_pWeapon->GetOwnerActor();
			IVehicle* pVehicle = pActor ? pActor->GetLinkedVehicle() : NULL;
			if (pVehicle)
			{
				ownerId = pVehicle->GetEntityId();
			}

			SAIStimulus stim(AISTIM_BULLET_WHIZZ, 0, ownerId, 0, pos, dir, range);
			IPerceptionManager::GetInstance()->RegisterStimulus(stim);
		}
	}
}

//-----------------------------------------------------------------------
void CSingle::CacheAmmoGeometry()
{
	const SAmmoParams *pAmmoParams = GetAmmoParams();
	if (pAmmoParams)
	{
		pAmmoParams->CacheResources();
	}
}

//------------------------------------------------------------------------
float CSingle::GetProjectileFiringAngle(float v, float g, float x, float y)
{
	float angle=0.0,t,a;

	// for simple stuff has pow of 2, always use x * x plz
	// also always use powf versus pow (last is double precision..)

	// Avoid square root in script
	float d = sqrt_tpl(max(0.0f,powf(v,4)-2*g*y*( v * v )- (g * g) * (x *x)));
	if(d>=0)
	{
		a= (v * v)-g*y;
		if (a-d>0) {
			t=sqrt_tpl(2*(a-d)/(g * g));
			angle = (float)acos_tpl(x/(v*t));	
			float y_test;
			y_test=float(-v*sin_tpl(angle)*t-g*( (t * t) *0.5f));
			if (fabsf(y-y_test)<0.02f)
				return RAD2DEG(-angle);
			y_test=float(v*sin_tpl(angle)*t-g * ( (t * t) *0.5f));
			if (fabsf(y-y_test)<0.02f)
				return RAD2DEG(angle);
		}
		t = sqrt_tpl(2*(a+d)/(g * g));
		angle = (float)acos_tpl(x/(v*t));	
		float y_test=float(v*sin_tpl(angle)*t-g*( (t * t) * 0.5f));

		if (fabsf(y-y_test)<0.02f)
			return RAD2DEG(angle);

		return 0;
	}
	return 0;
}
//------------------------------------------------------------------------
void CSingle::Cancel()
{

}
//------------------------------------------------------------------------
bool CSingle::AllowZoom() const
{
	return !m_cocking;
}
//------------------------------------------------------------------------

void CSingle::GetMemoryUsage(ICrySizer * s) const
{	
	s->AddObject(this, sizeof(*this));
	GetInternalMemoryUsage(s);
}

void CSingle::GetInternalMemoryUsage(ICrySizer * s) const
{
	s->AddObject(m_recoil);	
	s->AddObject(m_muzzleEffect);	
	//s->AddObject(m_queuedProbableHits);			
	BaseClass::GetInternalMemoryUsage(s); // collect memory of parent class
}


//-------------------------------------------------------------------------------


void CSingle::PatchSpreadMod(const SSpreadModParams &sSMP, float modMultiplier)
{
	m_recoil.PatchSpreadMod(sSMP, m_fireParams->spreadparamsCopy, modMultiplier);
}


void CSingle::ResetSpreadMod()
{
	m_recoil.ResetSpreadMod(m_fireParams->spreadparamsCopy);
}


void CSingle::PatchRecoilMod(const SRecoilModParams &sRMP, float modMultiplier)
{
	m_recoil.PatchRecoilMod(sRMP, m_fireParams->recoilparamsCopy, modMultiplier);
}


void CSingle::ResetRecoilMod()
{
	m_recoil.ResetRecoilMod(m_fireParams->recoilparamsCopy, &m_fireParams->recoilHints);
}

//-----------------------------------------------------------------------------
EntityId CSingle::RemoveProjectileId()
{
	EntityId ret = m_projectileId;
	m_projectileId = 0;
	return ret;
}

//--------------------------------------------------------------
void CSingle::EmitTracer(const Vec3& pos, const Vec3& destination, const STracerParams * useTracerParams, CProjectile* pProjectile)
{
	CCCPOINT(Single_EmitTracer);

	CTracerManager::STracerParams params;
	params.position = GetTracerPos(pos, useTracerParams);
	params.destination = destination;

	if(m_pWeapon->GetStats().fp)
	{
		Vec3 dir = (destination-params.position);
		
		const float length = dir.NormalizeSafe();
		
		if(length < (2.0f))
			return;

		params.position += (dir * 2.0f);
		params.effect = useTracerParams->effectFP.c_str();
		params.geometry = useTracerParams->geometryFP.c_str();
		params.speed = useTracerParams->speedFP;
		params.scale = useTracerParams->thicknessFP;
	}
	else
	{
		params.effect = useTracerParams->effect.c_str();
		params.geometry = useTracerParams->geometry.c_str();
		params.speed = useTracerParams->speed;
		params.scale = useTracerParams->thickness;
	}

	params.slideFraction = useTracerParams->slideFraction;
	params.lifetime = useTracerParams->lifetime;
	params.delayBeforeDestroy = useTracerParams->delayBeforeDestroy;

	const int tracerIdx = g_pGame->GetWeaponSystem()->GetTracerManager().EmitTracer(params, pProjectile ? pProjectile->GetEntityId() : 0);

	if (pProjectile)
	{
		pProjectile->BindToTracer(tracerIdx);
	}
}

//----------------------------------------------------
void CSingle::OnZoomStateChanged()
{

}

//----------------------------------------------------
bool CSingle::DampRecoilEffects() const
{
	return m_pWeapon->IsZoomStable();
}

//----------------------------------------------------
void CSingle::UpdateFireAnimationWeight(float frameTime)
{
	const SFireModeParams* pShared = GetShared();
	float desireAnimationWeight = 1.0f;

	if (DampRecoilEffects())
	{
		desireAnimationWeight *= pShared->fireparams.holdbreath_fire_anim_damp;
	}

	if(m_pWeapon->IsZoomed() || m_pWeapon->IsZoomingIn())
	{
		desireAnimationWeight *= pShared->fireparams.ironsight_fire_anim_damp;
	}
	
	desireAnimationWeight *= pShared->fireparams.fire_anim_damp;

	Interpolate(m_fireAnimationWeight, desireAnimationWeight, 4.0f, frameTime);
}

//----------------------------------------------------
float CSingle::GetFireAnimationWeight() const
{
	return m_fireAnimationWeight;
}

//----------------------------------------------------
float CSingle::GetFireFFeedbackWeight() const
{
	if (!DampRecoilEffects())
		return 1.0f;
	return GetShared()->fireparams.holdbreath_ffeedback_damp;
}

//----------------------------------------------------
void CSingle::SetProjectileLaunchParams(const SProjectileLaunchParams &launchParams)
{
	SetProjectileSpeedScale(launchParams.fSpeedScale);
}

//----------------------------------------------------
float CSingle::CalculateSpreadMultiplier(CActor* pOwnerActor) const
{
	float spreadMultiplier = 1.f;
	if (pOwnerActor && pOwnerActor->IsPlayer())
	{
		CPlayer * pPlayer = static_cast<CPlayer*>(pOwnerActor);
		spreadMultiplier *= pPlayer->GetModifiableValues().GetValue(kPMV_WeaponHipSpreadMultiplier);
		
		if (pPlayer->IsSliding())
		{
			spreadMultiplier *= g_pGame->GetCVars()->slide_spread;
		}
	}
	return spreadMultiplier;
}

//----------------------------------------------------
float CSingle::CalculateRecoilMultiplier(CActor* pOwnerActor) const
{
	if (pOwnerActor && pOwnerActor->IsPlayer() && GetWeapon()->IsZoomed())
	{
		return static_cast<CPlayer*>(pOwnerActor)->GetModifiableValues().GetValue(kPMV_WeaponSightRecoilMultiplier);
	}

	return 1.f;
}


//----------------------------------------------------
void CSingle::SetProjectileSpeedScale( float fSpeedScale )
{
	m_speed_scale = fSpeedScale;
}

// ===========================================================================
//	Get the hazard area descriptor data.
//
//	Returns:	The hazard area descriptor data (or NULL if it could not be
//				obtained).
//
const SHazardDescriptor* CSingle::GetHazardDescriptor() const
{
	const CWeapon* weapon = GetWeapon();
	if (weapon == NULL)
	{
		return NULL;
	}
	const CWeaponSharedParams* sharedParams = weapon->GetWeaponSharedParams();
	if (sharedParams == NULL)
	{
		return NULL;
	}
	return &(sharedParams->hazardWeaponDescriptor);
}


// ===========================================================================
//	Query if we should generate a hazard area in front of the projectile.h
//
//	Returns:	True if a hazard area should be generated; otherwise false.
//
bool CSingle::ShouldGenerateHazardArea() const
{
	const SHazardDescriptor* hazardDesc = GetHazardDescriptor();
	if (hazardDesc == NULL || !gGameAIEnv.hazardModule )
	{
		return false;
	}
	return hazardDesc->m_DefinedFlag;	
}


// ===========================================================================
//	Retrieve the position and direction of the hazardous area in front of
//	the weapon.
//
//	Out:	The start position of the area (in world-space) (NULL is
//			invalid!)
//	Out:	The aiming direction normal vector (in world-space) (NULL
//			is invalid!)
//
void CSingle::RetrieveHazardAreaPoseInFrontOfWeapon(
	Vec3* hazardStartPos, Vec3* hazardForwardNormal) const
{
	const SHazardDescriptor* hazardDesc = GetHazardDescriptor();
	assert(hazardDesc != NULL);

	bool hitFlag = false;
	ray_hit rayHit;	 
	rayHit.pCollider = 0;

	Vec3 hitPos = GetProbableHit(WEAPON_HIT_RANGE, &hitFlag, &rayHit);
	Vec3 firingPos = GetFiringPos(hitPos);	
	Vec3 forwardNormal = GetFiringDir(hitPos, firingPos);
	*hazardStartPos = firingPos + (forwardNormal * hazardDesc->descriptor.startPosNudgeOffset);
	*hazardForwardNormal = forwardNormal;	
}


// ===========================================================================
//	Register a temporary hazard area in front of the weapon so that
//	actors get a chance to react while the weapon is charging up (if needed).
//
void CSingle::RegisterHazardAreaInFrontOfWeapon()
{
	UnregisterHazardAreaInFrontOfWeapon(); // (Just in case).

	if (!ShouldGenerateHazardArea())
	{
		return;
	}	

	const SHazardDescriptor* hazardDesc = GetHazardDescriptor();
	assert(hazardDesc != NULL);
	
	const CWeapon* weapon = GetWeapon();
	IEntity* weaponOwnerEntity = NULL;
	if (weapon != NULL)
	{
		weaponOwnerEntity = weapon->GetOwner();
	}	

	HazardSystem::HazardSetup setup;
	if (weaponOwnerEntity != NULL)
	{
		setup.m_OriginEntityID   = weaponOwnerEntity->GetId();
	}	
	setup.m_ExpireDelay          = -1.0f;	// (Never expire automatically).
	setup.m_WarnOriginEntityFlag = false;	// The entity knows it is firing its own weapon, 
											// so no need to send signals back.
	
	const HazardWeaponDescriptor& hazardWeaponDesc = hazardDesc->descriptor;

	HazardSystem::HazardProjectile context;
	RetrieveHazardAreaPoseInFrontOfWeapon(
		&context.m_Pos,
		&context.m_MoveNormal);
	context.m_Radius                    = hazardWeaponDesc.hazardRadius;
	context.m_MaxScanDistance			= hazardWeaponDesc.maxHazardDistance;
	context.m_MaxPosDeviationDistance	= hazardWeaponDesc.maxHazardApproxPosDeviation;
	context.m_MaxAngleDeviationRad      = DEG2RAD(hazardWeaponDesc.maxHazardApproxAngleDeviationDeg);
	if (weapon != NULL)
	{
		context.m_IgnoredWeaponEntityID = weapon->GetEntityId();
	}
	m_HazardID = gGameAIEnv.hazardModule->ReportHazard(setup, context);	
}


// ===========================================================================
//	Report the area in front of the weapon as a hazardous area (if needed).
//
void CSingle::SyncHazardAreaInFrontOfWeapon()
{	
	Vec3 hazardStartPos;
	Vec3 hazardForwardNormal;
	RetrieveHazardAreaPoseInFrontOfWeapon(&hazardStartPos, &hazardForwardNormal);
	gGameAIEnv.hazardModule->ModifyHazard(
		m_HazardID, 
		hazardStartPos, hazardForwardNormal);
}


// ===========================================================================
//	Unregister the hazardous area in front of the weapon (if needed).
//
void CSingle::UnregisterHazardAreaInFrontOfWeapon()
{
	if (!m_HazardID.IsDefined())
	{
		return;
	}

	gGameAIEnv.hazardModule->ExpireHazard(m_HazardID);
	m_HazardID.Undefine();
}

float CSingle::GetReloadSpeedMultiplier( const CActor* pOwnerActor ) const
{
	float multiplier = 1.f;

	if (pOwnerActor)
	{
		multiplier *= pOwnerActor->GetReloadSpeedScale();
	}

	const CItem::TAccessoryArray& accessories = m_pWeapon->GetAccessories();
	const int numAccessories = accessories.size();
	for (int i = 0; i < numAccessories; i++)
	{
		const SAccessoryParams* pParams = m_pWeapon->GetAccessoryParams(accessories[i].pClass);
		multiplier *= pParams->reloadSpeedMultiplier;
	}

	return multiplier;
}

void CSingle::OnHostMigrationCompleted()
{
	// Check if we're currently reloading, if we are then force reset the flag and ask the server to fill our 
	// ammo.
	
	// Flag reset is because the flag only ever gets set/reset on clients in response to a serialise from the 
	// server, it never gets changed on the server itself.  If we leave the flag alone then we can get stuck 
	// unable to fire or zoom.

	// Ask for more ammo from the server because the new server may not had received the message saying we were
	// trying to reload since it wasn't server at the time, it could also have got past the point where refilling
	// should happen while it was still a client.  Either situation would result in no ammo being given.

	if (m_reloadPending && (m_pWeapon->GetOwnerId() == gEnv->pGameFramework->GetClientActorId()))
	{
		m_reloadPending = false;

		if (m_pWeapon->IsServer())
		{
			FillAmmo(true);
		}
		else
		{
			CWeapon::SvRequestInstantReloadParams params;
			params.fireModeId = m_pWeapon->GetCurrentFireMode();
			m_pWeapon->GetGameObject()->InvokeRMI(CWeapon::SvRequestInstantReload(), params, eRMI_ToServer);
		}
	}
}

void CSingle::OnOutOfAmmo(const CActor* pActor, bool autoReload)
{
	IEntityClass *pAmmoClass = GetAmmoType();
	IF_LIKELY(pAmmoClass)
	{
		m_pWeapon->OnOutOfAmmo(pAmmoClass);
	}
	m_pWeapon->PlayAction(GetFragmentIds().shot_last_bullet);

	if (autoReload && (!pActor || pActor->IsClient()))
	{
		m_pWeapon->SetBusy(true);
		const int curTime = (int)(m_pWeapon->GetParams().autoReloadDelay * 1000.0f);
		m_pWeapon->GetScheduler()->TimerAction(curTime, CSchedulerAction<ScheduleAutoReload>::Create(ScheduleAutoReload(this, m_pWeapon)), false);
		m_autoReloading = true;
	}
}
