// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$

	-------------------------------------------------------------------------
	History:
	- 15:2:2006   12:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Weapon.h"
#include "Actor.h"
#include "Game.h"
#include "GameRules.h"
#include "FireMode.h"
#include "Melee.h"

#include "UI/HUD/HUDEventDispatcher.h"

/*
#define CHECK_OWNER_REQUEST()	\
	{ \
		uint16 channelId=m_pGameFramework->GetGameChannelId(pNetChannel);	\
		IActor *pOwnerActor=GetOwnerActor(); \
		if (!pOwnerActor || pOwnerActor->GetChannelId()!=channelId) \
		{ \
			CryLogAlways("[gamenet] Disconnecting %s. Bogus weapon action '%s' request! %s %d!=%d (%s!=%s)", \
			pNetChannel->GetName(), __FUNCTION__, pOwnerActor?pOwnerActor->GetEntity()->GetName():"null", \
			pOwnerActor?pOwnerActor->GetChannelId():0, channelId,\
			pOwnerActor?pOwnerActor->GetEntity()->GetName():"null", \
			m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId)?m_pGameFramework->GetIActorSystem()->GetActorByChannelId(channelId)->GetEntity()->GetName():"null"); \

			return false; \
		} \
	} \
*/

//------------------------------------------------------------------------
int CWeapon::NetGetCurrentAmmoCount() const
{
	if (!m_fm)
		return 0;

	return GetAmmoCount(m_fm->GetAmmoType());
}

//------------------------------------------------------------------------
void CWeapon::NetSetCurrentAmmoCount(int count)
{
	if (!m_fm)
		return;

	SetAmmoCount(m_fm->GetAmmoType(), count);
}

int CWeapon::GetReloadState() const
{
	return m_reloadState;
}

void CWeapon::SvSetReloadState(int state)
{
	m_reloadState = state;
	CHANGED_NETWORK_STATE(this, ASPECT_RELOAD);
}

void CWeapon::ClSetReloadState(int state)
{
	assert(!gEnv->bServer);

	if(m_fm)
	{
		IActor *pActor = GetOwnerActor();
		bool ownerIsLocal = pActor && pActor->IsClient();

		switch(state)
		{
		case eNRS_NoReload:
			{
				if(IsReloading())
					m_fm->NetEndReload();
				m_reloadState = state;
				break;
			}

		case eNRS_StartReload:
			{
				if(!IsReloading(false))
				{
					m_fm->Reload(m_zm ? m_zm->GetCurrentStep() : 0); 
				}
				m_reloadState = eNRS_StartReload;
				break;
			}

		case eNRS_EndReload:
			{
				if(ownerIsLocal)
				{
					m_fm->NetEndReload();
				}
				m_reloadState = eNRS_NoReload;
				break;
			}

		case eNRS_CancelReload:
			{
				if(!ownerIsLocal)
				{
					m_fm->CancelReload();
				}
				else
				{
					m_fm->NetEndReload();
				}

				m_reloadState = eNRS_NoReload;
				break;
			}

		default:
			{
				break;
			}
		}
	}
}

void CWeapon::SvCancelReload()
{
	if(m_fm)
	{
		m_fm->CancelReload();
		SvSetReloadState(eNRS_CancelReload);
	}
}

void CWeapon::NetStateSent()
{
	if(m_reloadState == eNRS_EndReload || m_reloadState == eNRS_CancelReload)
	{
		m_reloadState = eNRS_NoReload;
	}
}

void CWeapon::NetUpdateFireMode(SEntityUpdateContext& ctx)
{
	// CGunTurret and CVehicleWeapon overide NetAllowUpdate to perform their own checks.
	if(NetAllowUpdate(true))
	{
		m_netNextShot -= ctx.fFrameTime;

		if(IsReloading())
			return;	// reloading, bail

		if((!m_isFiringStarted) && (m_isFiring || m_shootCounter > 0)) 
		{
			m_isFiringStarted = true;
			m_netNextShot = 0.f;
			NetStartFire();
			EnableUpdate(true, eIUS_FireMode);
		}

		if(m_fm)
		{
			if(m_shootCounter > 0 && m_netNextShot <= 0.0f)
			{
				// Aside from the prediction handle, needed for the server, NetShoot/Ex parameters
				// are no longer used, these will need removing when the client->server RMI's are tided up
				m_fm->NetShoot(Vec3(0.f, 0.f, 0.f), 0);
				m_shootCounter--;

				//if fireRate == 0.0f, set m_netNextShot to 0.0f, otherwise increment by 60.f / fireRate.
				//	fres used to avoid microcoded instructions, fsel to avoid branching - Rich S
				const float fRawFireRate		= m_fm->GetFireRate();
				const float fFireRateSelect = -fabsf(fRawFireRate);
				const float fFireRateForDiv = (float)__fsel(fFireRateSelect, 1.0f, fRawFireRate);
				const float fNextShot				= (float)__fsel(fFireRateSelect, 0.0f, m_netNextShot + (60.f * __fres(fFireRateForDiv)));
				m_netNextShot = fNextShot;
			}
		}

		if(m_isFiringStarted && !m_isFiring && m_shootCounter <= 0)
		{
			m_isFiringStarted = false;
			NetStopFire();
			EnableUpdate(false, eIUS_FireMode);
		}

		// this needs to happen here, or NetStopFire interrupts the animation
		if(m_doMelee && m_melee)
		{
			m_melee->NetAttack();
			m_doMelee= false;
		}
	}

}

//------------------------------------------------------------------------
bool CWeapon::NetAllowUpdate(bool requireActor)
{
	if (!gEnv->bServer)
	{
		IActor *pActor = GetOwnerActor();
		
		if(!pActor && requireActor)
		{
			return false;
		}

		if (!pActor || !pActor->IsClient())
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CWeapon::NetShoot(const Vec3 &hit, int predictionHandle, int fireModeId)
{
	IFireMode *pFM = GetFireMode(fireModeId);
	if (pFM)
	{
		pFM->NetShoot(hit, predictionHandle);
	}
}

//------------------------------------------------------------------------
void CWeapon::NetShootEx(const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle, int fireModeId)
{
	IFireMode *pFM = GetFireMode(fireModeId);
	if (pFM)
	{
		pFM->NetShootEx(pos, dir, vel, hit, extra, predictionHandle);
	}
}

//------------------------------------------------------------------------
void CWeapon::NetSetIsFiring(bool isFiring)
{
	m_isFiring = isFiring;
	if(gEnv->bServer)
		CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
}

//------------------------------------------------------------------------
void CWeapon::NetStartFire()
{
	if (m_fm)
		m_fm->NetStartFire();
}

//------------------------------------------------------------------------
void CWeapon::NetStopFire()
{
	if (m_fm)
		m_fm->NetStopFire();
}

//------------------------------------------------------------------------
void CWeapon::NetStartMeleeAttack(bool boostedAttack, int8 attackIndex /*= -1*/)
{
	if(m_melee)
	{
		BoostMelee(boostedAttack);
		m_melee->NetAttack();
	}
}

//------------------------------------------------------------------------
void CWeapon::NetZoom(float fov)
{
	if (CActor *pOwner=GetOwnerActor())
	{
		if (pOwner->IsClient())
			return;

		pOwner->GetActorParams().viewFoVScale = fov;
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestShoot(IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel, const Vec3 &hit, float extra, int predictionHandle, bool forceExtended)
{
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		if (IsServerSpawn(pAmmoType) || forceExtended)
			GetGameObject()->InvokeRMI(CWeapon::SvRequestShootEx(), SvRequestShootExParams(pos, dir, vel, hit, extra, predictionHandle, m_firemode), eRMI_ToServer);
		else
			GetGameObject()->InvokeRMI(CWeapon::SvRequestShoot(), SvRequestShootParams(hit, m_firemode), eRMI_ToServer);

		m_expended_ammo++;
		m_fireCounter++;
	}
	else if (IsServer())
	{
		m_fireCounter++;
		CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestStartFire()
{
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		GetGameObject()->InvokeRMI(CWeapon::SvRequestStartFire(), DefaultParams(), eRMI_ToServer);
	}
	else if (IsServer())
	{
		NetSetIsFiring(true);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestStartMeleeAttack(bool weaponMelee, bool boostedAttack, int8 attackIndex /*= -1*/)
{
	
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		GetGameObject()->InvokeRMI(CWeapon::SvRequestStartMeleeAttack(), MeleeRMIParams(boostedAttack,attackIndex), eRMI_ToServer);
	
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponComboDebugEnabled)
		{
			CryLogAlways("COMBO - RequestStartMeleeAttack - called by NON SERVER client with attack index [%d]",attackIndex);
			CryLogAlways("COMBO - RequestStartMeleeAttack - calling SvRequestStartMeleeAttack");
		}
#endif //#ifndef _RELEASE

	}
	else if (IsServer())
	{
		m_attackIndex		= attackIndex; 
		m_meleeCounter	= (m_meleeCounter + 1) % kMeleeCounterMax;
#ifndef _RELEASE
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponComboDebugEnabled)
		{
			CryLogAlways("COMBO - RequestStartMeleeAttack - called by server, about to netserialising to clients w A.Index. [%d]", m_attackIndex);
		}
#endif //#ifndef _RELEASE
		CHANGED_NETWORK_STATE(this, ASPECT_MELEE);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestStopFire()
{
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		GetGameObject()->InvokeRMI(CWeapon::SvRequestStopFire(), DefaultParams(), eRMI_ToServer);
	}
	else if (IsServer())
	{
		NetSetIsFiring(false);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestReload()
{
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		GetGameObject()->InvokeRMI(SvRequestReload(), DefaultParams(), eRMI_ToServer);
	}
	else if (IsServer())
	{
		SvSetReloadState(eNRS_StartReload);
	}
}

//-----------------------------------------------------------------------
void CWeapon::RequestCancelReload()
{
	CActor *pActor=GetOwnerActor();

	if (!gEnv->bServer && pActor && pActor->IsClient())
	{
		if(m_fm)
		{
			m_fm->CancelReload();
		}
		GetGameObject()->InvokeRMI(SvRequestCancelReload(), DefaultParams(), eRMI_ToServer);
	}
	else if (IsServer())
	{
		SvCancelReload();
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestFireMode(int fmId)
{
	if(fmId==GetCurrentFireMode())
		return;

	CActor *pActor=GetOwnerActor();

	if (gEnv->IsClient() && pActor && pActor->IsClient())
	{
		float animationTime = GetCurrentAnimationTime(eIGS_Owner)/1000.0f;
		m_switchFireModeTimeStap = gEnv->pTimer->GetCurrTime() + animationTime;

		if(gEnv->bServer)
		{
			SetCurrentFireMode(fmId);
		}
		else
		{
			if(m_fm)
			{
				m_fm->Activate(false);
			}
			GetGameObject()->InvokeRMI(SvRequestFireMode(), SvRequestFireModeParams(fmId), eRMI_ToServer);
		}	
		StartVerificationSample(gEnv->pTimer->GetAsyncCurTime());
		SHUDEvent event(eHUDEvent_OnWeaponFireModeChanged);
		event.AddData(SHUDEventData(fmId));
		CHUDEventDispatcher::CallEvent(event);
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestWeaponRaised(bool raise)
{
	if(gEnv->bMultiplayer)
	{
		CActor* pActor = GetOwnerActor();
		if(pActor && pActor->IsClient())
		{
			if (gEnv->bServer)
				CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
			else
				GetGameObject()->InvokeRMI(SvRequestWeaponRaised(), WeaponRaiseParams(raise), eRMI_ToServer);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::RequestSetZoomState(bool zoomed)
{
	if(gEnv->bMultiplayer)
	{
		CActor* pActor = GetOwnerActor();
		if(pActor && pActor->IsClient())
		{
			if (gEnv->bServer)
				CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
			else
				GetGameObject()->InvokeRMI(SvRequestSetZoomState(), ZoomStateParams(zoomed), eRMI_ToServer);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::SendEndReload()
{
	SvSetReloadState(eNRS_EndReload);
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStartFire)
{
	CHECK_OWNER_REQUEST();

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
	{
		NetSetIsFiring(true);
		NetStartFire();
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStopFire)
{
	CHECK_OWNER_REQUEST();

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
	{
		NetSetIsFiring(false);
		NetStopFire();
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestShoot)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->IsDead())
		ok=false;

	if (ok)
	{
		m_fireCounter++;
		m_expended_ammo++;

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
		{
			NetShoot(params.hit, 0, params.fireModeId);
		}

		CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestShootEx)
{
	CHECK_OWNER_REQUEST();

	bool ok=true;
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->IsDead())
	{
		ok=false;
	}

	if (ok)
	{
		m_fireCounter++;
		m_expended_ammo++;

		IActor *pLocalActor=m_pGameFramework->GetClientActor();
		bool isLocal = pLocalActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

		if (!isLocal)
		{
			NetShootEx(params.pos, params.dir, params.vel, params.hit, params.extra, params.predictionHandle, params.fireModeId);
		}

		CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
	}
	else
	{
		if(params.predictionHandle)
		{
			CGameRules::SPredictionParams predictionParams(params.predictionHandle);
			g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::ClPredictionFailed(), predictionParams, eRMI_ToClientChannel, m_pGameFramework->GetGameChannelId(pNetChannel));
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestStartMeleeAttack)
{
	CHECK_OWNER_REQUEST();

	CActor *pActor=GetActorByNetChannel(pNetChannel);
	IActor *pLocalActor=m_pGameFramework->GetClientActor();
	bool isLocal = pLocalActor && pActor && (pLocalActor->GetChannelId() == pActor->GetChannelId());

	if (!isLocal)
	{
		NetStartMeleeAttack(params.boostedAttack, params.attackIndex);
	}

	m_attackIndex		= params.attackIndex; 
	m_meleeCounter	= (m_meleeCounter + 1) % kMeleeCounterMax;
	CHANGED_NETWORK_STATE(this, ASPECT_MELEE);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestFireMode)
{
	CHECK_OWNER_REQUEST();

	SetCurrentFireMode(params.id);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestReload)
{
	CHECK_OWNER_REQUEST();
	
	CActor *pActor=GetActorByNetChannel(pNetChannel);
	if (!pActor || pActor->IsDead())
	{
		SvSetReloadState(eNRS_CancelReload);
	}
	else if(!IsSelected())
	{
		//If the weapon isn't selected but the client is requesting a reload, they probably have finished equipping the weapon
		//	while the server is catching up. This can be due to latency or mismatched 1P/3P animation lengths.
		m_bReloadWhenSelected = true;
	}
	else
	{
		SvSetReloadState(eNRS_StartReload);

		if(m_fm)
			m_fm->Reload(0);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestCancelReload)
{
	CHECK_OWNER_REQUEST();

	SvCancelReload();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestWeaponRaised)
{
	CHECK_OWNER_REQUEST();

	CHANGED_NETWORK_STATE(this, ASPECT_STREAM);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestSetZoomState)
{
	CHECK_OWNER_REQUEST();

	if (params.zoomed)
		StartZoom(m_owner.GetId(), 1);
	else
		StopZoom(m_owner.GetId());

	CHANGED_NETWORK_STATE(this, ASPECT_STREAM);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvStartedCharging)
{
	CHECK_OWNER_REQUEST();

	if(CFireMode* pFireMode = static_cast<CFireMode*>(GetFireMode(GetCurrentFireMode())))
	{
		pFireMode->NetSetCharging(true);

		CHANGED_NETWORK_STATE(this, ASPECT_CHARGING);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CWeapon, SvRequestInstantReload)
{
	CHECK_OWNER_REQUEST();

	CFireMode *pFireMode = GetCFireMode(params.fireModeId);
	if (pFireMode)
	{
		pFireMode->FillAmmo(true);
	}

	return true;
}
