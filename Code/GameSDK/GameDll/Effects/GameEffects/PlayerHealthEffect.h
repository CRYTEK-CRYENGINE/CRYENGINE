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
#pragma once

#ifndef _PLAYERHEALTH_GAME_EFFECT_
#define _PLAYERHEALTH_GAME_EFFECT_

#include "GameEffect.h"
#include <CryAction/IMaterialEffects.h>
#include "Effects/GameEffectsSystem.h"

class CActor;
DECLARE_SHARED_POINTERS(CActor);

class CParameterGameEffect;

class CPlayerHealthGameEffect : public CGameEffect
{
	typedef CGameEffect BaseEffectClass;

public:
	struct SQueuedHit
	{
		SQueuedHit()
			: m_valid(false)
			, m_hitDirection(FORWARD_DIRECTION)
			, m_hitStrength(0.0f)
			, m_hitSpeed(1.0f)
		{

		}

		ILINE void Validate() { m_valid = true; }
		ILINE void Invalidate() { m_valid = false; }
		ILINE bool IsValid() const { return m_valid; }

		Vec3	m_hitDirection;
		float	m_hitStrength;
		float	m_hitSpeed;
	
	private:
		bool	m_valid;
	};

public:
	CPlayerHealthGameEffect();

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual void	Update(float frameTime) override;
	virtual void	SetActive(bool isActive) override;

	void InitialSetup(CActor* pClientActor);

	ILINE void SetKillCamData(const EntityId id, const float brightness, const float contrast) {m_killerId = id; m_baseBrightness = brightness; m_baseContrast = contrast; } // For killcam replay

	void ReStart();
	void Start();
	void Stop();
	void OnKill();
	void OnHit(const Vec3& hitDirection, const float hitStrength, const float hitSpeed);

	void EnableExternalControl( bool enable );
	void ExternalSetEffectIntensity( float intensity );

	virtual void GetMemoryUsage( ICrySizer *pSizer ) const override
	{
		pSizer->AddObject(this, sizeof(*this));	
	}

	virtual const char* GetName() const override { return "Player Health"; }
	virtual void ResetRenderParameters() override;

	static void LoadStaticData(IItemParamsNode* pRootNode);
	static void ReloadStaticData(IItemParamsNode* pRootNode);
	static void ReleaseStaticData();

private:
	void UpdateHealthReadibility(float effectIntensity, float frameTime);

	TMFXEffectId							m_playerHealthEffectId;
	TMFXEffectId							m_playerDeathEffectId;
	CActorWeakPtr							m_clientActor;

	SQueuedHit								m_lastRegisteredHit;
	EntityId									m_killerId; // For killcam replay
	_smart_ptr<ITexture>					m_blurMaskTexture;

	float									m_screenBloodIntensity;
	float									m_radialBlurIntensity;
	float									m_radialBlurTime;
	float									m_lastEffectIntensity;
	float									m_lastRadialBlurIntensity;

	float									m_baseContrast;
	float									m_baseBrightness;

	float									m_externallySetEffectIntensity;

	bool									m_useNewEffect;
	bool									m_useBlurMask;
	bool									m_isExternallyControlled;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SPlayerHealthGameEffectParams : public SGameEffectParams
{
	SPlayerHealthGameEffectParams()
		: playerHealthEffectId(InvalidEffectId)
		, playerDeathEffectId(InvalidEffectId)
		, pClientActor(NULL)
	{
		
	}

	TMFXEffectId		playerHealthEffectId;
	TMFXEffectId		playerDeathEffectId;
	CActor*					pClientActor;
};

#endif // _PLAYERHEALTH_GAME_EFFECT_