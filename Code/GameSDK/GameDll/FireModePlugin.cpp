// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fire Mode Plugins

*************************************************************************/

#include "StdAfx.h"
#include "FireModePlugin.h"

#include "FireMode.h"
#include "Weapon.h"
#include "VehicleWeapon.h"
#include "Actor.h"
#include "Player.h"
#include <CryParticleSystem/ParticleParams.h>
#include "EntityUtility/EntityEffects.h"
#include "ScreenEffects.h"

CRY_IMPLEMENT_GTI_BASE(IFireModePlugin);


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CRY_IMPLEMENT_GTI(CFireModePlugin_Overheat, IFireModePlugin);

//------------------------------------------------------------------------
CFireModePlugin_Overheat::CFireModePlugin_Overheat()
: m_heat(0.f)
, m_overheat(0.f)
, m_nextHeatTime(0.f)
, m_heatEffectId(0)
, m_firedThisFrame(false)
, m_isCoolingDown(false)
{
}

//------------------------------------------------------------------------
void CFireModePlugin_Overheat::OnShoot()
{
	m_firedThisFrame = true;
}

//------------------------------------------------------------------------
bool CFireModePlugin_Overheat::Update(float frameTime, uint32 frameId)
{
	CWeapon* pWeapon = m_pOwnerFiremode->GetWeapon();

	CRY_ASSERT(pWeapon);

	CActor *pActor = pWeapon->GetOwnerActor();
	bool isOwnerAIControlled = pActor ? !pActor->IsPlayer() : false;

	if(isOwnerAIControlled)
		return false;

	float oldheat = m_heat;

	if (m_overheat > 0.0f)
	{
		m_overheat -= frameTime;
		if (m_overheat <= 0.0f)
		{
			m_overheat = 0.0f;
			FragmentID fragment = pWeapon->GetFragmentID(m_pParams->cooldown.c_str());
			pWeapon->PlayAction(fragment);				
		}
	}
	else
	{
		float add=0.0f;
		float sub=0.0f;

		if (m_firedThisFrame)
		{
			add = m_pParams->attack;

			if(pActor && pActor->GetLinkedVehicle())
			{
				const static float k_VehicleCombatMode_ScaleOverheat = 0.5f;
				add = m_pParams->attack * k_VehicleCombatMode_ScaleOverheat;	
			}

		}
		else if (m_pOwnerFiremode->GetNextShotTime()<=0.0001f && !m_pOwnerFiremode->IsFiring())
		{
			sub=frameTime / m_pParams->decay;
		}

		m_heat = clamp_tpl(m_heat + (add-sub), 0.0f, 1.0f);

		if (m_heat >= 0.999f && oldheat<0.999f)
		{
			m_overheat = m_pParams->duration;
			m_isCoolingDown = true;

			CWeapon* const pWeapon = m_pOwnerFiremode->GetWeapon();
			if (pWeapon->IsVehicleWeapon())
			{
				CVehicleWeapon* const pVehicleWeapon = static_cast<CVehicleWeapon*>(pWeapon);
				IVehicle* const pVehicle = pVehicleWeapon->GetVehicle();
				CPlayer* const pPlayer = pVehicleWeapon->GetOwnerPlayer();
				if (pVehicle && pPlayer)
				{
					pVehicle->OnAction(eVAI_Attack2, eAAM_OnRelease, 0.0f, pPlayer->GetEntityId());
				}
			}
			else
			{
				m_pOwnerFiremode->StopFire();
			}

			FragmentID fragment = pWeapon->GetFragmentID(m_pParams->overheating.c_str());
			pWeapon->PlayAction(fragment);

			int slot = pWeapon->GetStats().fp ? eIGS_FirstPerson : eIGS_ThirdPerson;
			if (!m_pParams->effect[slot].empty())
			{
				if (m_heatEffectId)
				{
					pWeapon->DetachEffect(m_heatEffectId);
					m_heatEffectId = 0;
				}

				m_heatEffectId = pWeapon->AttachEffect(slot, false, m_pParams->effect[slot].c_str(), m_pParams->helper[slot].c_str());
			}
		}
		else if(m_heat <= m_pParams->refire_heat)
		{
			m_isCoolingDown = false;
		}
	}

	m_firedThisFrame = false;

	return (m_heat > 0.0001f);
}

//------------------------------------------------------------------------
void CFireModePlugin_Overheat::Activate(bool activate)
{
	CWeapon* pWeapon = m_pOwnerFiremode->GetWeapon();
		
	CRY_ASSERT(pWeapon);

	if(activate)
	{
		m_heat = 0.f;
		m_overheat = 0.f;

		if(m_nextHeatTime > 0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			float dt = m_nextHeatTime - time.GetSeconds();
			if(dt > 0.0f)
			{
				m_heat = min(dt,1.0f);
			}
			if(dt > 1.0f)
			{
				m_overheat = dt - 1.0f;
			}
			m_nextHeatTime = 0.0f;
		}
	}
	else
	{
		if(m_heatEffectId)
		{
			pWeapon->DetachEffect(m_heatEffectId);
			m_heatEffectId = 0;
		}

		m_nextHeatTime = 0.0f;
		if(m_heat>0.0f)
		{
			CTimeValue time = gEnv->pTimer->GetFrameStartTime();
			m_nextHeatTime = time.GetSeconds() + m_heat + m_overheat;
		}

		m_heat = 0.f;
		m_overheat = 0.f;
	}

	m_firedThisFrame = false;
}

//------------------------------------------------------------------------
bool CFireModePlugin_Overheat::AllowFire() const
{
	return m_overheat <= 0.f && (!m_isCoolingDown || m_heat <= m_pParams->refire_heat);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CRY_IMPLEMENT_GTI(CFireModePlugin_Reject, IFireModePlugin);

//------------------------------------------------------------------------
CFireModePlugin_Reject::CFireModePlugin_Reject()
: m_shellHelperVelocity(ZERO)
, m_lastShellFXPosition(ZERO)
, m_shellFXSpeed(0.f)
{
};

//------------------------------------------------------------------------
void CFireModePlugin_Reject::Activate(bool activate)
{
	if (m_shellFXSpeed != 0.0f && !activate)
	{
		const SEffectParams& rejectEffect = m_pParams->rejectEffect;

		IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(rejectEffect.effect[0].c_str());
		if (pParticleEffect)
		{
			ParticleParams particleParams = pParticleEffect->GetParticleParams();
			particleParams.fSpeed = m_shellFXSpeed;
			pParticleEffect->SetParticleParams(particleParams);
		}
		
		pParticleEffect = gEnv->pParticleManager->FindEffect(rejectEffect.effect[1].c_str());
		if (pParticleEffect)
		{
			ParticleParams particleParams = pParticleEffect->GetParticleParams();
			particleParams.fSpeed = m_shellFXSpeed;
			pParticleEffect->SetParticleParams(particleParams);
		}
		
		m_shellFXSpeed = 0.0f;
	}
}

//------------------------------------------------------------------------
void CFireModePlugin_Reject::OnShoot()
{
	const SEffectParams& rejectEffect = m_pParams->rejectEffect;
	CWeapon* pWeapon = m_pOwnerFiremode->GetWeapon();

	const int slot = pWeapon->GetStats().fp ? 0 : 1;
	const bool doRejectEffect = (g_pGameCVars->i_rejecteffects != 0) && (!rejectEffect.effect[slot].empty());

	if (doRejectEffect)
	{
		//First check if we can skip/cull the effect (not for the local player)
		if (!pWeapon->IsOwnerClient())
		{
			const CCamera& camera = gEnv->p3DEngine->GetRenderingCamera();
			const Vec3 cameraPos = camera.GetPosition();
			const Vec3 effectPosition = pWeapon->GetWorldPos();

			const float fDist2 = (cameraPos-effectPosition).len2();

			// Too far, skip
			if (fDist2 > g_pGameCVars->g_rejectEffectCullDistance)			
			{
				return; 
			}

			// Not in the view ?
			if(g_pGameCVars->g_rejectEffectVisibilityCull)
			{
				Sphere emitterSphere(effectPosition, 2.0f);
				if(camera.IsSphereVisible_F(emitterSphere) == false)
				{
					return;
				}
			}
		}

		IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(rejectEffect.effect[slot].c_str());

		if (m_shellFXSpeed == 0.0f)
		{
			
			if (pParticleEffect)
			{
				const ParticleParams& particleParams = pParticleEffect->GetParticleParams();
				m_shellFXSpeed = particleParams.fSpeed.GetMaxValue();
			}
		}
		const Vec3 shellDirection = pWeapon->GetSlotHelperRotation(slot, rejectEffect.helper[slot].c_str(), true).GetColumn1();
		const Vec3 shellVelocity = m_shellHelperVelocity + (shellDirection * m_shellFXSpeed);

		EntityEffects::SEffectSpawnParams spawnParams(ZERO, shellVelocity.GetNormalized(), rejectEffect.scale[slot], shellVelocity.GetLength(), false);
		EntityEffects::SpawnParticleWithEntity(pWeapon->GetEntity(), slot, pParticleEffect, rejectEffect.helper[slot].c_str(), spawnParams);
	}
}

//------------------------------------------------------------------------
bool CFireModePlugin_Reject::Update(float frameTime, uint32 frameId)
{
	CWeapon* pWeapon = m_pOwnerFiremode->GetWeapon();

	if(pWeapon->IsOwnerClient())
	{
		const int slot = pWeapon->GetStats().fp ? 0 : 1;
		const SEffectParams& rejectEffect = m_pParams->rejectEffect;
		Vec3 lastShellFXPosition = m_lastShellFXPosition;

		const Vec3 helperLocalPosition = pWeapon->GetSlotHelperPos(slot, rejectEffect.helper[slot].c_str(), true);
		
		if (lastShellFXPosition.IsZero())
		{
			lastShellFXPosition = helperLocalPosition;
		}
		
		const Vec3 positionDifferential = helperLocalPosition - lastShellFXPosition;
		m_shellHelperVelocity = positionDifferential * (1.0f / frameTime);

		m_lastShellFXPosition = helperLocalPosition;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CRY_IMPLEMENT_GTI(CFireModePlugin_RecoilShake, IFireModePlugin);

void CFireModePlugin_RecoilShake::OnShoot()
{
	const bool clientShot = m_pOwnerFiremode->GetWeapon()->IsOwnerClient();
	if(clientShot == false)
		return;

	const SFireModePlugin_RecoilShakeParams::SRecoilShakeParams& params = m_pParams->m_recoilShake;
	CScreenEffects* pScreenEffects = g_pGame->GetScreenEffects();
	if(pScreenEffects != NULL)
	{
		pScreenEffects->CamShake( params.m_rotateShake, params.m_shiftShake, params.m_frequency, params.m_shakeTime, params.m_randomness, CScreenEffects::eCS_GID_Weapon );
	}
}

void CFireModePlugin_RecoilShake::OnReplayShoot()
{
	OnShoot();
}