// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Continuous health effect running for the local player

-------------------------------------------------------------------------
History:
- 10:05:2010   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "PlayerHealthEffect.h"
#include "Actor.h"
#include "GameCVars.h"
#include "ReplayActor.h"
#include "RecordingSystem.h"
#include "Effects/GameEffects/ParameterGameEffect.h"

REGISTER_DATA_CALLBACKS(CPlayerHealthGameEffect::LoadStaticData,CPlayerHealthGameEffect::ReleaseStaticData,CPlayerHealthGameEffect::ReloadStaticData,PlayerHealthData);

namespace
{


	const char* radialBlurMaskName = "Textures/weapons/damage_blurmask.dds";


}

struct SPlayerHealthGameEffectData
{
	SPlayerHealthGameEffectData()
	{
		highHealth = 0.7f;
		lowHealth = 0.4f;
		nearDeath = 0.1f;
		lowHealthBloodIntensity = 0.6f;
		radialBlurOscillationTime = 0.48f;
		minIllumination = 0.5f;
		maxIllumination = 2.0f;
		maxContrast = 1.8f;
		minSaturation = -1.0f;
		mercyTimeBlood = 1.0f;
		mercyTimeRadialBlur = 1.0f;
	}

	float		highHealth;
	float		lowHealth;
	float		nearDeath;
	float		lowHealthBloodIntensity;
	float		radialBlurOscillationTime;
	float		minIllumination;
	float		maxIllumination;
	float		maxContrast;
	float		minSaturation;
	float		mercyTimeBlood;
	float		mercyTimeRadialBlur;
};
static SPlayerHealthGameEffectData s_playerHealthGEData;

void CPlayerHealthGameEffect::LoadStaticData(IItemParamsNode* pRootNode)
{
	const IItemParamsNode* pEffectNode = pRootNode->GetChild("PlayerHealthEffect");

	if(pEffectNode)
	{
		const IItemParamsNode* pParamsNode = pEffectNode->GetChild("params");
		if(pParamsNode)
		{
			pParamsNode->GetAttribute("highHealth",s_playerHealthGEData.highHealth);
			pParamsNode->GetAttribute("lowHealth",s_playerHealthGEData.lowHealth);
			pParamsNode->GetAttribute("nearDeath",s_playerHealthGEData.nearDeath);
			pParamsNode->GetAttribute("lowHealthBloodIntensity",s_playerHealthGEData.lowHealthBloodIntensity);
			pParamsNode->GetAttribute("radialBlurOscillationTime",s_playerHealthGEData.radialBlurOscillationTime);
			pParamsNode->GetAttribute("minIllumination",s_playerHealthGEData.minIllumination);
			pParamsNode->GetAttribute("maxIllumination",s_playerHealthGEData.maxIllumination);
			pParamsNode->GetAttribute("maxContrast",s_playerHealthGEData.maxContrast);
			pParamsNode->GetAttribute("minSaturation",s_playerHealthGEData.minSaturation);
			pParamsNode->GetAttribute("mercyTimeBlood",s_playerHealthGEData.mercyTimeBlood);
			pParamsNode->GetAttribute("mercyTimeRadialBlur",s_playerHealthGEData.mercyTimeRadialBlur);

			// Avoid divisions by 0
			s_playerHealthGEData.radialBlurOscillationTime = max(s_playerHealthGEData.radialBlurOscillationTime,0.0001f);
		}
	}
}

void CPlayerHealthGameEffect::ReloadStaticData(IItemParamsNode* pRootNode)
{
	ReleaseStaticData();
	LoadStaticData(pRootNode);
}

void CPlayerHealthGameEffect::ReleaseStaticData()
{

}

CPlayerHealthGameEffect::CPlayerHealthGameEffect()
: m_playerHealthEffectId(InvalidEffectId)
, m_playerDeathEffectId(InvalidEffectId)
, m_killerId(0)
, m_screenBloodIntensity(0.0f)
, m_radialBlurIntensity(0.0f)
, m_radialBlurTime(0.0f)
, m_lastEffectIntensity(1.0f)
, m_lastRadialBlurIntensity(0.0f)
, m_baseContrast(1.f)
, m_baseBrightness(1.f)
, m_useBlurMask(false)
, m_isExternallyControlled(false)
, m_externallySetEffectIntensity(0.f)
{
	m_useNewEffect = (g_pGameCVars->pl_health.enableNewHUDEffect != 0);
}

void CPlayerHealthGameEffect::Initialise(const SGameEffectParams* gameEffectParams)
{
	BaseEffectClass::Initialise(gameEffectParams);

	SPlayerHealthGameEffectParams playerHealthGameEffectParams;
	if(gameEffectParams)
	{
		playerHealthGameEffectParams = *(SPlayerHealthGameEffectParams*)(gameEffectParams);
	}

	m_playerHealthEffectId = playerHealthGameEffectParams.playerHealthEffectId;
	m_playerDeathEffectId = playerHealthGameEffectParams.playerDeathEffectId;
	if (playerHealthGameEffectParams.pClientActor)
	{
		m_clientActor = playerHealthGameEffectParams.pClientActor->GetWeakPtr();
	}
	
	FX_ASSERT_MESSAGE(m_playerHealthEffectId, "Player health effect initialised without a screen effect");
}

void CPlayerHealthGameEffect::Update(float frameTime)
{
	BaseEffectClass::Update(frameTime);

	CActorPtr pClientActor = m_clientActor.lock();
	if (pClientActor)
	{
		const float maxHealth = pClientActor->GetMaxHealth();
		const float currentHealth = pClientActor->GetHealth();
		float killerHealthPercentage = 0.0f;
		bool isDead = true;

		CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
		const bool isKillCamReplay = pRecordingSystem != NULL && pRecordingSystem->IsPlayingBack();

		CRY_ASSERT(maxHealth > 0.0f);

		if (isKillCamReplay)
		{
			CReplayActor* pKiller = pRecordingSystem->GetReplayActor(m_killerId, true);
			if (pKiller)
			{
				killerHealthPercentage = pKiller->GetHealthPercentage() / 100.0f;
				isDead = ((pKiller->m_flags & eRAF_Dead) != 0);
			}
		}
		else
		{
			if (m_isExternallyControlled)
			{
				killerHealthPercentage = 1.f - m_externallySetEffectIntensity;
				isDead = false;
			}
			else
			{
				killerHealthPercentage = currentHealth * (float)__fres(maxHealth);
				isDead = pClientActor->IsDead();
			}
		}

		if (!isDead)
		{
			const float minIntensity = g_pGameCVars->pl_health.minimalHudEffect ? 0.2f : 0.0f;
			const float effectIntensity = CLAMP(killerHealthPercentage, minIntensity, 1.0f);

			if (!m_useNewEffect)
			{
				IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

				SMFXCustomParamValue intensity;
				intensity.fValue = clamp_tpl(1.0f - effectIntensity, 0.0f, 1.0f);
				pMaterialEffects->SetCustomParameter(m_playerHealthEffectId, "Intensity", intensity); // Use intensity param to pass x pos
			}
			else
			{
				UpdateHealthReadibility(effectIntensity, frameTime);

				if (m_lastRegisteredHit.IsValid())
				{
					IRenderer* pRenderer = gEnv->pRenderer;
					if (pRenderer)
					{
						pRenderer->EF_SetPostEffectParam("HUDHitEffect_Amount", 10);
						pRenderer->EF_SetPostEffectParamVec4("HUDHitEffect_HitDirection", Vec4(m_lastRegisteredHit.m_hitDirection.x, m_lastRegisteredHit.m_hitDirection.y, m_lastRegisteredHit.m_hitDirection.z, 1));
						pRenderer->EF_SetPostEffectParam("HUDHitEffect_HitStrength", m_lastRegisteredHit.m_hitStrength);
						pRenderer->EF_SetPostEffectParam("HUDHitEffect_HitSpeed", m_lastRegisteredHit.m_hitSpeed);
					}

					m_lastRegisteredHit.Invalidate();
				}
			}
		}
	}
}

void CPlayerHealthGameEffect::SetActive(bool isActive)
{
	BaseEffectClass::SetActive(isActive);
	m_isExternallyControlled = false;

	IRenderer* pRenderer = gEnv->pRenderer;

	if (pRenderer)
	{
		pRenderer->EF_SetPostEffectParam("ScreenBlood_Amount", 0.0f, true);
		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_Amount", 0.0f, true);
		pRenderer->EF_SetPostEffectParam("Global_User_Brightness", m_baseBrightness);
		pRenderer->EF_SetPostEffectParam("Global_User_Contrast", m_baseContrast);
	}
	CParameterGameEffect * pParameterGameEffect = g_pGame->GetParameterGameEffect();
	FX_ASSERT_MESSAGE (pParameterGameEffect, "Pointer to ParameterGameEffect is NULL");
	if (pParameterGameEffect)
	{
		pParameterGameEffect->SetSaturationAmount(1.f, CParameterGameEffect::eSEU_PlayerHealth);
	}

	m_screenBloodIntensity = 0.0f;
	m_radialBlurIntensity = 0.0f;
}

void CPlayerHealthGameEffect::Start()
{
	if (!m_useNewEffect)
	{
		CActorPtr pClientActor = m_clientActor.lock();
		if (pClientActor && (pClientActor->IsDead() || pClientActor->GetSpectatorMode() !=  CActor::eASM_None))
		{
			// Restart the dead effect only
			OnKill();
		}
		else if (m_playerHealthEffectId != InvalidEffectId)
		{
			IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();
			assert(pMaterialEffects);

			SMFXRunTimeEffectParams effectParams;
			effectParams.pos = GetISystem()->GetViewCamera().GetPosition();
			//effectParams.soundSemantic = eSoundSemantic_None;

			pMaterialEffects->ExecuteEffect(m_playerHealthEffectId, effectParams);

			SMFXCustomParamValue intensity;
			intensity.fValue = 0.0f;
			pMaterialEffects->SetCustomParameter(m_playerHealthEffectId, "Intensity", intensity); 
		}
	}
	else
	{
		if (gEnv->pRenderer)
		{
			gEnv->pRenderer->EF_SetPostEffectParam("HUDHitEffect_HealthLevel", 1.0f, true);
			gEnv->pRenderer->EF_SetPostEffectParam("HUDHitEffect_Amount", 0);
		}
		m_lastRegisteredHit.Invalidate();
	}

	SetActive(true);
}

void CPlayerHealthGameEffect::Stop()
{
	if (!m_useNewEffect)
	{
		IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();
		assert(pMaterialEffects);

		if (m_playerHealthEffectId != InvalidEffectId)
		{
			SMFXCustomParamValue intensity;
			intensity.fValue = 0.0f;
			pMaterialEffects->SetCustomParameter(m_playerHealthEffectId, "Intensity", intensity);

			pMaterialEffects->StopEffect(m_playerHealthEffectId);
		}

		if (m_playerDeathEffectId != InvalidEffectId)
		{
			pMaterialEffects->StopEffect(m_playerDeathEffectId);
		}
	}
	else
	{
		m_lastRegisteredHit.Invalidate();
	}

	SetActive(false);
}

void CPlayerHealthGameEffect::ReStart()
{
	Stop();
	Start();
}

void CPlayerHealthGameEffect::InitialSetup(CActor* pClientActor)
{
	IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

	SPlayerHealthGameEffectParams healthEffectParams;
	healthEffectParams.playerHealthEffectId = pMaterialEffects->GetEffectIdByName("player_fx", "player_damage");
	healthEffectParams.playerDeathEffectId = pMaterialEffects->GetEffectIdByName("player_fx", "player_death");
	healthEffectParams.pClientActor = pClientActor;

	if (gEnv->pRenderer)
	{
		m_blurMaskTexture = gEnv->pRenderer->EF_LoadTexture(radialBlurMaskName);
	}

	if (m_blurMaskTexture)
	{
		m_blurMaskTexture->Release();
		m_useBlurMask=true;
	}
	else
	{
		m_useBlurMask=false;
	}

	Initialise(&healthEffectParams);
}

void CPlayerHealthGameEffect::OnKill()
{
	Stop();
	if (!m_useNewEffect)
	{
		if (m_playerDeathEffectId != InvalidEffectId && !gEnv->bMultiplayer)
		{
			IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

			SMFXRunTimeEffectParams effectParams;
			effectParams.pos = GetISystem()->GetViewCamera().GetPosition();
			//effectParams.soundSemantic = eSoundSemantic_None;

			pMaterialEffects->ExecuteEffect(m_playerDeathEffectId, effectParams);
		}
	}
	else
	{
		if (gEnv->pRenderer)
		{
			gEnv->pRenderer->EF_SetPostEffectParam("HUDHitEffect_HealthLevel", 0.0f, true);
		}
	}
}

void CPlayerHealthGameEffect::OnHit( const Vec3& hitDirection, const float hitStrength, const float hitSpeed )
{
	m_lastRegisteredHit.m_hitDirection = hitDirection;
	m_lastRegisteredHit.m_hitStrength = hitStrength;
	m_lastRegisteredHit.m_hitSpeed = hitSpeed;
	m_lastRegisteredHit.Validate();

	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if (pRecordingSystem)
	{
		pRecordingSystem->OnPlayerHealthEffect(m_lastRegisteredHit);
	}
}


void CPlayerHealthGameEffect::UpdateHealthReadibility(float effectIntensity, float frameTime)
{
	IRenderer* pRenderer = gEnv->pRenderer;
	if (!pRenderer)
	{
		return;
	}
	CRY_ASSERT(pRenderer);

	const float highHealth = s_playerHealthGEData.highHealth;
	const float lowHealth = s_playerHealthGEData.lowHealth;
	const float nearDeath = s_playerHealthGEData.nearDeath;
	const float lowHealthBloodIntensity = s_playerHealthGEData.lowHealthBloodIntensity;
	const float radialBlurOscillationTime = s_playerHealthGEData.radialBlurOscillationTime;
	const float minIllumination = s_playerHealthGEData.minIllumination;
	const float maxIllumination = s_playerHealthGEData.maxIllumination;
	const float maxContrast = s_playerHealthGEData.maxContrast;
	const float minSaturation = s_playerHealthGEData.minSaturation;
	const float interpolateVelocity = (effectIntensity > m_lastEffectIntensity) ? 1.0f : 12.0f;

	float targetScreenBlood;
	float targetRadialBlur;

	if (effectIntensity <= nearDeath)
	{
		targetScreenBlood = s_playerHealthGEData.mercyTimeBlood;
		targetRadialBlur = s_playerHealthGEData.mercyTimeRadialBlur;
	}
	else if (effectIntensity <= lowHealth)
	{
		targetScreenBlood = lowHealthBloodIntensity;
		targetRadialBlur = 0.0f;
	}
	else
	{
		const float tangent = lowHealthBloodIntensity / max((highHealth-lowHealth),0.0001f);
		targetScreenBlood = max(0.0f, -tangent*effectIntensity + tangent*highHealth);
		targetRadialBlur = 0.0f;
	}

	Interpolate(m_screenBloodIntensity, targetScreenBlood, interpolateVelocity, frameTime);
	Interpolate(m_radialBlurIntensity, targetRadialBlur, interpolateVelocity, frameTime);

	pRenderer->EF_SetPostEffectParam("ScreenBlood_Amount", m_screenBloodIntensity, true);

	if (m_radialBlurIntensity > 0.05f)
	{
		m_radialBlurTime =
			fmodf(m_radialBlurTime + frameTime, radialBlurOscillationTime);
		float normalizedBlurTime = m_radialBlurTime / radialBlurOscillationTime;
		float radialBlurTime = powf(normalizedBlurTime, 0.25f);
		float radialBlurOscilation = -cosf(radialBlurTime*2*3.1415f)*0.5f+0.5f;

		float radialBlurAmount = m_radialBlurIntensity * (radialBlurOscilation * 0.5f + 0.5f);
		float brightness = LERP(minIllumination, maxIllumination, (radialBlurOscilation * 0.5f + 0.5f));
		float contrast = m_radialBlurIntensity * ((1.0f - radialBlurOscilation) * 0.75f + 0.25f);
		float saturation = radialBlurAmount;

		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_Amount", radialBlurAmount, true);
		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_ScreenPosX", 0.5f, true);
		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_ScreenPosY", 0.5f, true);
		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_Radius", 1.5f, true);
		if (m_useBlurMask)
		{
			pRenderer->EF_SetPostEffectParamString(
				"FilterRadialBlurring_MaskTexName",
				radialBlurMaskName);
		}

		pRenderer->EF_SetPostEffectParam("Global_User_Brightness", m_baseBrightness*LERP(1.0f, brightness, m_radialBlurIntensity));
		pRenderer->EF_SetPostEffectParam("Global_User_Contrast", m_baseContrast*LERP(1.0f, brightness, contrast));
		CParameterGameEffect * pParameterGameEffect = g_pGame->GetParameterGameEffect();
		if (pParameterGameEffect)
		{
			pParameterGameEffect->SetSaturationAmount(LERP(1.0f, minSaturation, saturation), CParameterGameEffect::eSEU_PlayerHealth);
		}
	}
	else if (m_lastRadialBlurIntensity > 1e-6f)
	{
		pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_Amount", 0.0f, true);

		pRenderer->EF_SetPostEffectParam("Global_User_Brightness", m_baseBrightness);
		pRenderer->EF_SetPostEffectParam("Global_User_Contrast", m_baseContrast);
		CParameterGameEffect * pParameterGameEffect = g_pGame->GetParameterGameEffect();
		if (pParameterGameEffect)
		{
			pParameterGameEffect->SetSaturationAmount(1.f, CParameterGameEffect::eSEU_PlayerHealth);
		}
	}

	m_lastEffectIntensity = effectIntensity;
	m_lastRadialBlurIntensity = m_radialBlurIntensity;
}

void CPlayerHealthGameEffect::ResetRenderParameters()
{
	if (gEnv->pRenderer)
	{
		gEnv->pRenderer->EF_SetPostEffectParam("ScreenBlood_Amount", 0.0f, true);
		gEnv->pRenderer->EF_SetPostEffectParam("FilterRadialBlurring_Amount", 0.0f, true);
		gEnv->pRenderer->EF_SetPostEffectParam("Global_User_Brightness", m_baseBrightness);
		gEnv->pRenderer->EF_SetPostEffectParam("Global_User_Contrast", m_baseContrast);
	}
}

void CPlayerHealthGameEffect::EnableExternalControl( bool enable )
{
	m_isExternallyControlled = enable;
}

// will only be used when m_isExternallyControlled==true
void CPlayerHealthGameEffect::ExternalSetEffectIntensity( float intensity )
{ 
	m_externallySetEffectIntensity = intensity; 
}
