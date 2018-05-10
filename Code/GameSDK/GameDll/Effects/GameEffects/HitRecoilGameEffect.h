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

#pragma once

#ifndef HIT_RECOIL_GAME_EFFECT_H
#define HIT_RECOIL_GAME_EFFECT_H

#include "GameEffect.h"
#include <IForceFeedbackSystem.h>

class CPlayer;
class CProjectile;
struct HitInfo;



struct SHitRecoilGameEffectParams : public SGameEffectParams
{
	SHitRecoilGameEffectParams() {}
};



class CHitRecoilGameEffect : public CGameEffect
{
private:
	struct SCameraShake
	{
		SCameraShake();

		float m_rollIntensity;
		float m_pitchIntensity;
		float m_shiftIntensity;
		float m_curveAttack;
		float m_time;
		float m_doubleAttack;
		float m_doubleAttackTime;
	};

	struct SForceFeedback
	{
		SForceFeedback();

		ForceFeedbackFxId m_fxId;
		float m_delay;
		float m_weight;
	};

	struct SHitRecoilParams
	{
		SHitRecoilParams();

		SCameraShake m_cameraShake;
		SForceFeedback m_forceFeedback;
		float m_minDamage;
		float m_maxDamage;
		float m_filterDelay;

		void GetMemoryUsage( ICrySizer *pSizer ) const{}
	};

	struct SCameraShakeParams
	{
		SCameraShakeParams() {}
		SCameraShakeParams(CPlayer* _pPlayer, const SCameraShake& _cameraShake, float _intensity, Vec3 _damageDirection);

		CPlayer* pPlayer;
		const SCameraShake* pCameraShake;
		float intensity;
		Vec3 damageDirection;
	};

public:
	CHitRecoilGameEffect();

	virtual void Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual void Update(float frameTime) override;
	virtual void AddHit(CPlayer* pPlayer, IEntityClass* pProjectileClass, float damage, int damageTypeId, const Vec3& damageDirection);

	void Reset(const IItemParamsNode* pRootNode);

	static int GetHitRecoilId(const string& name);

	virtual void GetMemoryUsage( ICrySizer *pSizer ) const override
	{		
		pSizer->AddContainer(m_hitRecoilParams);
		pSizer->AddContainer(m_hitTypeToRecoil);		
	}

	virtual const char* GetName() const override { return "Hit Recoil"; }

private:
	void NormalCameraShakeAttack();
	void DoublePreCameraShakeAttack(float intensity, float time);
	void CamShake(float intensity, float attackTime, float decayTime);
	void ForceFeedback(const SForceFeedback& feedback, float intensity);
	void ResetRenderParameters() override { }

	typedef std::map<int, SHitRecoilParams> THitRecoilParamMap;
	THitRecoilParamMap m_hitRecoilParams;
	typedef std::map<int, int> THitTypeToRecoilMap;
	THitTypeToRecoilMap m_hitTypeToRecoil;

	SCameraShakeParams m_cameraShakeParams;
	float m_timeOutCounter;
	float m_doubleAttackDelay;
	bool m_doubleAttack;
};


#endif

