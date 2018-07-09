// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:

-------------------------------------------------------------------------
History:
- 17:11:2009: Created by Filipe Amim

*************************************************************************/

#include "StdAfx.h"
#include "Effects/GameEffectsSystem.h"
#include "HitRecoilGameEffect.h"
#include "GameRules.h"
#include "Player.h"
#include "Weapon.h"
#include "AmmoParams.h"
#include "Projectile.h"
#include "WeaponSystem.h"
#include "ScreenEffects.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

namespace
{


	class CHitRecoilNameRegistration
	{
	public:
		static const int maxNumEntries = 16;
		static const int stringSize = 16;

		typedef CryFixedStringT<stringSize> TString;
		typedef CryFixedArray<TString, maxNumEntries> TStringArray;

	public:

		int GetNameId(const string& name)
		{
			TString cmpString = name.c_str();

			for (size_t i = 0; i < m_nameRegistration.size(); ++i)
			{
				if (m_nameRegistration[i] == cmpString)
					return i;
			}

			m_nameRegistration.push_back(cmpString);

			return m_nameRegistration.size() - 1;
		}

		CryFixedStringT<stringSize> GetNameFromId(int id)
		{
			if (id < 0 || id >= (int)m_nameRegistration.size())
				return "";
			return m_nameRegistration[id];
		}

	private:
		TStringArray m_nameRegistration;
	};



	CHitRecoilNameRegistration& GetHitRecoilNameRegistration()
	{
		static CHitRecoilNameRegistration hitRecoilNameRegistration;
		return hitRecoilNameRegistration;
	}


	template<typename T>
	T Sign(T v)
	{
		return v < 0 ? (T)-1 : v > 0 ? (T)1 : 0;
	}


	// curve attack for [-1, 1] range
	float CurveAttack(float linearVal, float curvePower)
	{
		return pow_tpl(linearVal*0.5f+0.5f, curvePower)*2.0f-1.0f;
	}


}



CHitRecoilGameEffect::SCameraShakeParams::SCameraShakeParams(CPlayer* _pPlayer, const SCameraShake& _cameraShake, float _intensity, Vec3 _damageDirection)
	:	pPlayer(_pPlayer)
	,	pCameraShake(&_cameraShake)
	,	intensity(_intensity)
	,	damageDirection(_damageDirection)
{
}


CHitRecoilGameEffect::SCameraShake::SCameraShake()
	:	m_pitchIntensity(0.0f)
	,	m_shiftIntensity(1.0f)
	,	m_rollIntensity(0.0f)
	,	m_curveAttack(1.0f)
	,	m_time(0.0f)
	,	m_doubleAttack(0.0f)
	,	m_doubleAttackTime(0.0f)
{
}



CHitRecoilGameEffect::SForceFeedback::SForceFeedback()
	:	m_fxId(0)
	,	m_delay(0.0f)
	,	m_weight(1.0f)
{
}



CHitRecoilGameEffect::SHitRecoilParams::SHitRecoilParams()
	:	m_minDamage(0.0f)
	,	m_maxDamage(1.0f)
	,	m_filterDelay(0.0f)
{
}



CHitRecoilGameEffect::CHitRecoilGameEffect()
	:	m_timeOutCounter(0.0f)
	,	m_doubleAttackDelay(0.0f)
	,	m_doubleAttack(false)
{
}



void CHitRecoilGameEffect::Initialise(const SGameEffectParams* gameEffectParams)
{
	CGameEffect::Initialise(gameEffectParams);
}



void CHitRecoilGameEffect::Update(float frameTime)
{
	m_timeOutCounter -= frameTime;
	m_doubleAttackDelay -= frameTime;

	if (m_doubleAttackDelay < 0.0f && m_doubleAttack)
		NormalCameraShakeAttack();

	if (m_timeOutCounter < 0.0f)
		SetActive(false);
}



void CHitRecoilGameEffect::AddHit(CPlayer* pPlayer, IEntityClass *pProjectileClass, float damage, int damageTypeId, const Vec3& damageDirection)
{
	if (m_timeOutCounter > 0)
		return;

	int hitRecoilType = -1;
	if (pProjectileClass)
	{
		const SAmmoParams *projectileParams = g_pGame->GetWeaponSystem()->GetAmmoParams(pProjectileClass);
		hitRecoilType = projectileParams->m_hitRecoilId;
	}
	else
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		
		if(damageTypeId != 0)
		{
			THitTypeToRecoilMap::iterator it = m_hitTypeToRecoil.find(damageTypeId);
			if (it == m_hitTypeToRecoil.end())
				return;

			hitRecoilType = it->second;
		}
	}

	THitRecoilParamMap::iterator it = m_hitRecoilParams.find(hitRecoilType);
	if (it == m_hitRecoilParams.end())
		return;
	const SHitRecoilParams& hitRecoilParams = it->second;

	const float effectIntensity = SATURATE((damage - hitRecoilParams.m_minDamage) / (hitRecoilParams.m_maxDamage - hitRecoilParams.m_minDamage));
	const float fovEffectIntensity = pPlayer->GetActorParams().viewFoVScale;

	m_cameraShakeParams = SCameraShakeParams(pPlayer, hitRecoilParams.m_cameraShake, effectIntensity*fovEffectIntensity, damageDirection);

	if (it->second.m_cameraShake.m_doubleAttack > 0.0f)
		DoublePreCameraShakeAttack(it->second.m_cameraShake.m_doubleAttack, it->second.m_cameraShake.m_doubleAttackTime);
	else
		NormalCameraShakeAttack();
	ForceFeedback(hitRecoilParams.m_forceFeedback, effectIntensity);

	m_timeOutCounter += hitRecoilParams.m_filterDelay;

	SetActive(true);

#ifndef _RELEASE
	if (g_pGameCVars->pl_debug_hit_recoil)
	{
		const string& name = GetHitRecoilNameRegistration().GetNameFromId(hitRecoilType);
		gEnv->pLog->Log("Hit Recoil '%s', damage='%f', intensity='%f'", name.c_str(), damage, effectIntensity);
	}
#endif
}



void CHitRecoilGameEffect::NormalCameraShakeAttack()
{
	const float attackFraction = 0.25f;
	float attackTime = m_cameraShakeParams.pCameraShake->m_time * attackFraction;
	float decayTime = m_cameraShakeParams.pCameraShake->m_time * (1.0f - attackFraction);
	CamShake(1.0f, std::abs(attackTime), std::abs(decayTime));
	m_doubleAttack = false;
}


void CHitRecoilGameEffect::DoublePreCameraShakeAttack(float intensity, float time)
{
	m_doubleAttackDelay = time;
	m_doubleAttack = true;
	CamShake(-intensity, time*0.5f, time*0.5f);
}



void CHitRecoilGameEffect::CamShake(float intensity, float attackTime, float decayTime)
{
	const Vec3 localRecoilDirection = -(Matrix33(m_cameraShakeParams.pPlayer->GetEntity()->GetWorldTM().GetInverted()) * m_cameraShakeParams.damageDirection);
	const Vec3 shakeShiftVec = Vec3(localRecoilDirection.x, 0, 0) * m_cameraShakeParams.pCameraShake->m_shiftIntensity * m_cameraShakeParams.intensity;
	Ang3 shakeRotationAngles(ZERO);
	const float fSign = (float)__fsel(localRecoilDirection.y, 1.0f, -1.0f);
	shakeRotationAngles.x = CurveAttack(fabsf(localRecoilDirection.y), m_cameraShakeParams.pCameraShake->m_curveAttack) * fSign;
	shakeRotationAngles.y = (1.0f-fabsf(shakeRotationAngles.x)) * Sign(-localRecoilDirection.x);
	shakeRotationAngles.x *= m_cameraShakeParams.pCameraShake->m_pitchIntensity * m_cameraShakeParams.intensity;
	shakeRotationAngles.y *= m_cameraShakeParams.pCameraShake->m_rollIntensity * m_cameraShakeParams.intensity;

	IView* pView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
	if (pView)
	{
		IView::SShakeParams shakeParams;
		shakeParams.shakeAngle = shakeRotationAngles*intensity;
		shakeParams.shakeShift = shakeShiftVec*intensity;
		shakeParams.fadeInDuration = attackTime;
		shakeParams.fadeOutDuration = decayTime;
		shakeParams.bFlipVec = false;
		shakeParams.shakeID = CScreenEffects::eCS_GID_HitRecoil;
		pView->SetViewShakeEx(shakeParams);
	}

	CCCPOINT(HitRecoil_CameraShake);
}



void CHitRecoilGameEffect::ForceFeedback(const SForceFeedback& feedback, float intensity)
{
	IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem();
	assert(pForceFeedback);
	SForceFeedbackRuntimeParams runtimeParams(clamp_tpl(intensity * feedback.m_weight, 0.0f, 1.0f), feedback.m_delay);
	pForceFeedback->PlayForceFeedbackEffect(feedback.m_fxId, runtimeParams);
}



int CHitRecoilGameEffect::GetHitRecoilId(const string& name)
{
	return GetHitRecoilNameRegistration().GetNameId(name);
}



void CHitRecoilGameEffect::Reset(const IItemParamsNode* pRootNode)
{
	const IItemParamsNode* pParams = pRootNode->GetChild("HitRecoilTable");
	if (!pParams)
		return;

	CGameRules *pGameRules = g_pGame->GetGameRules();

#ifdef _DEBUG
	// This code calls pGameRules->GetHitTypeId which (in debug builds) asserts if it fails to find the hit type specified.
	// Calling this stops the asserts from happening... [TF]

	bool old = CGameRules::DbgSetAssertOnFailureToFindHitType(false);
#endif

	IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem();
	assert(pForceFeedback);

	for (int i = 0; i < pParams->GetChildCount(); ++i)
	{
		const IItemParamsNode* pHitRecoil = pParams->GetChild(i);
		if (strcmp(pHitRecoil->GetName(), "HitRecoil") != 0)
			continue;

		string name;
		name = pHitRecoil->GetAttribute("name");
		int id = GetHitRecoilId(name);
		SHitRecoilParams hitRecoilParams;

		pHitRecoil->GetAttribute("shakeCurveAttack", hitRecoilParams.m_cameraShake.m_curveAttack);
		pHitRecoil->GetAttribute("shakeShiftIntensity", hitRecoilParams.m_cameraShake.m_shiftIntensity);
		pHitRecoil->GetAttribute("shakePitchIntensity", hitRecoilParams.m_cameraShake.m_pitchIntensity);
		pHitRecoil->GetAttribute("shakeRollIntensity", hitRecoilParams.m_cameraShake.m_rollIntensity);
		pHitRecoil->GetAttribute("shakeTime", hitRecoilParams.m_cameraShake.m_time);
		pHitRecoil->GetAttribute("doubleAttack", hitRecoilParams.m_cameraShake.m_doubleAttack);
		pHitRecoil->GetAttribute("doubleAttackTime", hitRecoilParams.m_cameraShake.m_doubleAttackTime);
		pHitRecoil->GetAttribute("ffWeight", hitRecoilParams.m_forceFeedback.m_weight);
		pHitRecoil->GetAttribute("ffDelay", hitRecoilParams.m_forceFeedback.m_delay);
		hitRecoilParams.m_forceFeedback.m_fxId = pForceFeedback->GetEffectIdByName(pHitRecoil->GetAttributeSafe("ffEffectName"));
		pHitRecoil->GetAttribute("minDamage", hitRecoilParams.m_minDamage);
		pHitRecoil->GetAttribute("maxDamage", hitRecoilParams.m_maxDamage);
		pHitRecoil->GetAttribute("filterDelay", hitRecoilParams.m_filterDelay);

		m_hitRecoilParams[id] = hitRecoilParams;

		if(pGameRules)
		{
			int hitTypeId = pGameRules->GetHitTypeId(name.c_str());
			if(hitTypeId != 0)
			{
				m_hitTypeToRecoil[hitTypeId] = id;
			}
		}
	}

#ifdef _DEBUG
	CGameRules::DbgSetAssertOnFailureToFindHitType(old);
#endif
}
