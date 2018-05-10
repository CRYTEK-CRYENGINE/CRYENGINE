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
#include "Rapid.h"
#include "Actor.h"
#include "Player.h"
#include "Game.h"
#include "GameCVars.h"
#include <IViewSystem.h>

#include "WeaponSharedParams.h"
#include "ScreenEffects.h"
#include "Battlechatter.h"
#include "GameRules.h"
#include "Utility/CryWatch.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleWeapon.h"

CRY_IMPLEMENT_GTI(CRapid, CSingle);

//------------------------------------------------------------------------
CRapid::CRapid()
	: m_queueRapidFireAction(false)
	, m_speed(0.0f)
	, m_acceleration(0.0f)
	, m_rotation_angle(0.0f)
	, m_rapidFlags(eRapidFlag_none)
{
}

//------------------------------------------------------------------------
void CRapid::Activate(bool activate)
{
	BaseClass::Activate(activate);

	if (m_pBarrelSpinAction)
	{
		m_pBarrelSpinAction->Stop();
		m_pBarrelSpinAction = 0;
	}
	if (activate)
	{
		m_pBarrelSpinAction = GetWeapon()->PlayAction(GetFragmentIds().barrel_spin);
	}

	m_rotation_angle = 0.0f;
	m_speed = 0.0f;
	m_rapidFlags &= ~(eRapidFlag_accelerating|eRapidFlag_decelerating|eRapidFlag_startedFiring);
	m_acceleration = 0.0f;

	// initialize rotation xforms
	UpdateRotation(0.0f);

		Firing(false);
}

//------------------------------------------------------------------------
void CRapid::Update(float frameTime, uint32 frameId)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	PlayStopRapidFireIfNeeded();

	BaseClass::Update(frameTime, frameId);

	if (m_speed <= 0.0f && m_acceleration < 0.0001f)
	{
		FinishDeceleration();
		return;
	}

	CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
	const bool isOwnerClient = pOwnerActor ? pOwnerActor->IsClient() : false;
	const bool isOwnerPlayer = pOwnerActor ? pOwnerActor->IsPlayer() : false;

	m_pWeapon->RequireUpdate(eIUS_FireMode);

	m_speed = m_speed + m_acceleration*frameTime;

	if (m_speed > m_fireParams->rapidparams.max_speed)
	{
		m_speed = m_fireParams->rapidparams.max_speed;
		m_rapidFlags &= ~eRapidFlag_accelerating;
	}

	if ((m_speed >= m_fireParams->rapidparams.min_speed) && !(m_rapidFlags & eRapidFlag_decelerating))
	{
		float dt = 1.0f;
		if (fabs_tpl(m_speed)>0.001f && fabs_tpl(m_fireParams->rapidparams.max_speed)>0.001f)
		{
			dt = m_speed * (float)__fres(m_fireParams->rapidparams.max_speed);
		}
		CRY_ASSERT(m_fireParams->fireparams.rate > 0);
		
		m_next_shot_dt = 60.0f* (float)__fres((m_fireParams->fireparams.rate*dt));

		if (CanFire(false))
		{
			if (!OutOfAmmo())
			{
				const bool firing = (m_rapidFlags & eRapidFlag_netShooting) || Shoot(true, m_fireParams->fireparams.autoReload);
				Firing(firing);
			}
			else
			{
				StopFire();
			}
		}
	}
	else if (m_firing)
	{
		StopFire();
		if (OutOfAmmo() && isOwnerPlayer)
		{
			m_pWeapon->Reload();
		}
	}

	if ((m_speed < m_fireParams->rapidparams.min_speed) && (m_acceleration < 0.0f) && !(m_rapidFlags & eRapidFlag_decelerating))
		Accelerate(m_fireParams->rapidparams.deceleration);

	UpdateRotation(frameTime);
	UpdateFiring(pOwnerActor, isOwnerClient, isOwnerPlayer, frameTime);
}

//------------------------------------------------------------------------
void CRapid::StartReload(int zoomed)
{
	if (IsFiring())
		Accelerate(m_fireParams->rapidparams.deceleration);
	Firing(false);
	PlayStopRapidFireIfNeeded();

	BaseClass::StartReload(zoomed);
}

//------------------------------------------------------------------------
void CRapid::StartFire()
{
	if (m_pWeapon->IsBusy() || !CanFire(true))
	{
		//Clip empty sound
		if(!CanFire(true) && !m_reloading)
		{
			int ammoCount = m_pWeapon->GetAmmoCount(m_fireParams->fireparams.ammo_type_class);

			if (GetClipSize()==0)
				ammoCount = m_pWeapon->GetInventoryAmmoCount(m_fireParams->fireparams.ammo_type_class);

			if(ammoCount<=0)
			{
				m_pWeapon->PlayAction(GetFragmentIds().empty_clip);
				m_pWeapon->OnFireWhenOutOfAmmo();
			}
		}
		return;
	}

	m_rapidFlags &= ~eRapidFlag_netShooting;

	if ((m_rapidFlags & (eRapidFlag_decelerating | eRapidFlag_accelerating)) == 0)
	{
		m_pWeapon->EnableUpdate(true, eIUS_FireMode);
	}

	//SpinUpEffect(true);
	Accelerate(m_fireParams->rapidparams.acceleration);

	m_rapidFlags |= eRapidFlag_startedFiring;
	m_firstFire = true;

	m_startFiringTime = gEnv->pTimer->GetAsyncTime();

	m_muzzleEffect.StartFire(this);

	m_pWeapon->RequestStartFire();
}

//------------------------------------------------------------------------
void CRapid::StopFire()
{
	bool acceleratingOrDecelerating = (m_rapidFlags & (eRapidFlag_accelerating|eRapidFlag_decelerating)) != 0;
	if (!m_firing && (m_reloading || !acceleratingOrDecelerating))
		return;

	m_firing = false;
	m_rapidFlags &= ~(eRapidFlag_startedFiring|eRapidFlag_rapidFiring);

  if(m_acceleration >= 0.0f)
  {
	  Accelerate(m_fireParams->rapidparams.deceleration);

    if (m_pWeapon->IsDestroyed())
      FinishDeceleration();
  }

  SpinUpEffect(false);
	SmokeEffect();
	
	const bool shouldTriggerStopFiringAnim =
		m_fireParams->rapidparams.min_firingTimeToStop==0.0f ||
		((gEnv->pTimer->GetAsyncTime().GetSeconds() - m_startFiringTime.GetSeconds()) > m_fireParams->rapidparams.min_firingTimeToStop);
	if (shouldTriggerStopFiringAnim)
	{
		m_queueRapidFireAction = true;
	}

	m_startFiringTime.SetSeconds(0.0f);

	m_muzzleEffect.StopFire(this);

	m_pWeapon->RequestStopFire();
}

//------------------------------------------------------------------------
void CRapid::NetStartFire()
{
	m_rapidFlags |= eRapidFlag_netShooting;
	
	m_pWeapon->EnableUpdate(true, eIUS_FireMode);

	//SpinUpEffect(true);
	Accelerate(m_fireParams->rapidparams.acceleration);
}

//------------------------------------------------------------------------
void CRapid::NetStopFire()
{
	if(m_acceleration >= 0.0f)
	{
		Accelerate(m_fireParams->rapidparams.deceleration);

	  if (m_pWeapon->IsDestroyed())
		  FinishDeceleration();
	}
	
	SpinUpEffect(false);

	if(m_firing)
		SmokeEffect();

	m_firing = false;
	m_rapidFlags &= ~(eRapidFlag_startedFiring|eRapidFlag_rapidFiring|eRapidFlag_netShooting);

	m_queueRapidFireAction = true;
	m_startFiringTime.SetSeconds(0.0f);
}

float CRapid::GetSpinUpTime() const
{
	return m_fireParams->rapidparams.min_speed/m_fireParams->rapidparams.acceleration;
}

//------------------------------------------------------------------------
int CRapid::GetShootAmmoCost(bool playerIsShooter)
{
	int cost = 1;

	if (m_fireParams->fireparams.fake_fire_rate && playerIsShooter)
	{
		if(!gEnv->bMultiplayer)
		{
			cost += cry_random(0, m_fireParams->fireparams.fake_fire_rate - 1);
		}
		else
		{
			// we want this predictable and fair in MP. 
			cost += m_fireParams->fireparams.fake_fire_rate; 
		}
	}

	return cost;
}

//------------------------------------------------------------------------
void CRapid::Accelerate(float acc)
{
	m_acceleration = acc;

	if (acc > 0.0f)
	{
		if (!IsFiring())
		{
			SpinUpEffect(true);
		}

		m_rapidFlags |= eRapidFlag_accelerating;
		m_rapidFlags &= ~eRapidFlag_decelerating;

		int flags = CItem::eIPAF_Default;
		m_pWeapon->PlayAction(GetFragmentIds().spin_up, 0, false, flags);
	}
	else
	{
		m_rapidFlags |= eRapidFlag_decelerating;
		m_rapidFlags &= ~eRapidFlag_accelerating;

		if(m_speed>0.0f)
		{
			m_pWeapon->PlayAction(GetFragmentIds().spin_down);

			if(m_firing)
			{
				m_pWeapon->PlayAction(GetFragmentIds().spin_down_tail);
			}
		}
		
		if (IsFiring())
		{
		  SpinUpEffect(false);
		}
	}
}

//------------------------------------------------------------------------
void CRapid::FinishDeceleration()
{
  m_rapidFlags &= ~eRapidFlag_decelerating;
  m_speed = 0.0f;
  m_acceleration = 0.0f;
  
  m_pWeapon->EnableUpdate(false, eIUS_FireMode);  
}

//------------------------------------------------------------------------
void CRapid::Firing(bool firing)
{
	SpinUpEffect(false);

	if (m_firing != firing)
	{
		if (m_firing)
			StopFire();
	}
	m_firing = firing;
}

//------------------------------------------------------------------------
void CRapid::UpdateRotation(float frameTime)
{
	if (m_pBarrelSpinAction)
	{
		m_pBarrelSpinAction->SetParam(CItem::sActionParamCRCs.spinSpeed, m_speed);
	}
}

//------------------------------------------------------------------------
void CRapid::UpdateFiring(CActor* pOwnerActor, bool ownerIsClient, bool ownerIsPlayer, float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_speed >= 0.00001f)
	{			
		const uint32 decelAndRapid = eRapidFlag_decelerating|eRapidFlag_rapidFiring;

		if ((m_speed >= m_fireParams->rapidparams.min_speed) && !(m_rapidFlags & decelAndRapid))
		{
			m_rapidFlags |= eRapidFlag_rapidFiring;

			uint32 flags = CItem::eIPAF_Default;
			float speedOverride = -1.0f;
			PlayRapidFire(speedOverride, (flags & CItem::eIPAF_ConcentratedFire) != 0);		
		}
		else if ((m_speed < m_fireParams->rapidparams.min_speed) && ((m_rapidFlags & decelAndRapid) ==  decelAndRapid))
		{
			m_rapidFlags &= ~eRapidFlag_rapidFiring;
		}
	}

	if(ownerIsClient && gEnv->bMultiplayer)
	{
		CRY_ASSERT(pOwnerActor);

		const CTimeValue currentTime = gEnv->pTimer->GetFrameStartTime();
		const float startFiringSeconds = m_startFiringTime.GetSeconds();
		const float secondsFiring = gEnv->pTimer->GetAsyncTime().GetSeconds() - startFiringSeconds;
		if(startFiringSeconds != 0.0f && secondsFiring > 3.0f)
		{
			NET_BATTLECHATTER(BC_Battlecry, static_cast<CPlayer*>(pOwnerActor));
		}
	}
}

//------------------------------------------------------------------------
bool CRapid::AllowZoom() const
{
	return true; //!m_firing && !m_startedToFire;
}

void CRapid::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));	
	BaseClass::GetInternalMemoryUsage(s);		// collect memory of parent class
}

void CRapid::PlayRapidFire(float speedOverride, bool concentratedFire)
{
	int fragmentID = m_pWeapon->GetFragmentID("rapid_fire");
	CRapidFireAction* pAction = new CRapidFireAction(PP_PlayerAction, fragmentID, this);
		
	pAction->SetParam(CItem::sActionParamCRCs.ammoLeft, GetAmmoSoundParam());
			
	m_pWeapon->PlayFragment(pAction, speedOverride, -1.f, GetFireAnimationWeight(), GetFireFFeedbackWeight(), concentratedFire);
}

float CRapid::GetAmmoSoundParam()
{
	float ammo = 1.0f;
	int clipSize = GetClipSize();
	int ammoCount = GetAmmoCount();
	if(clipSize > 0 && ammoCount >= 0)
	{
		ammo = (float) ammoCount / clipSize;
	}

#if !defined(_RELEASE)
	if (g_pGameCVars->i_debug_sounds)
	{
		CryWatch("ammo_left param %.2f", ammo);
	}
#endif

	return ammo;
};

void CRapid::PlayStopRapidFireIfNeeded()
{
	if (m_queueRapidFireAction)
	{
		// Workaround to try not have a stop_rapid_fire being queued the same frame than a start_rapid_fire, since we're skipping some sounds getting started.
		// TODO: Remove once the action resolve in mannequin handles the case differently.
		m_pWeapon->PlayAction(
			GetFragmentIds().stop_rapid_fire, 0, false, CItem::eIPAF_Default,
			-1.0f, GetFireAnimationWeight(), GetFireFFeedbackWeight());
		m_queueRapidFireAction = false;
	}
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

IAction::EStatus CRapidFireAction::Update(float timePassed)
{
	if(!m_pFireMode || !m_pFireMode->IsFiring())
	{
		m_eStatus = Finished;
	}
	else
	{
		m_animWeight = m_pFireMode->GetFireAnimationWeight();
		SetParam(CItem::sActionParamCRCs.ammoLeft, m_pFireMode->GetAmmoSoundParam());
		SetParam(CItem::sActionParamCRCs.fired, m_pFireMode->Fired());
		SetParam(CItem::sActionParamCRCs.firstFire, m_pFireMode->FirstFire());

		if (GetRootScope().IsDifferent(m_fragmentID, m_fragTags, m_subContext))
		{
			SetFragment(m_fragmentID, m_fragTags);
		}
	}

	return BaseClass::Update(timePassed);
}