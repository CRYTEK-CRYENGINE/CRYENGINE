// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _KILL_CAM_GAME_EFFECT_
#define _KILL_CAM_GAME_EFFECT_

#pragma once

// Includes
#include "GameEffect.h"
#include "Effects/GameEffectsSystem.h"
#include "Effects/HUDEventListeners/LetterBoxHudEventListener.h"
#include "Effects/Tools/CVarActivationSystem.h"
#include "Effects/Tools/PostEffectActivationSystem.h"

//==================================================================================================
// Name: CKillCamGameEffect
// Desc: Kill Cam visual effect
// Author: James Chilvers
//==================================================================================================
class CKillCamGameEffect : public CGameEffect
{
private:
	//--------------------------------------------------------------------------------------------------
	// Name: SKillCamGameEffectData
	// Desc: Data loaded from xml to control kill cam game effect
	//--------------------------------------------------------------------------------------------------
	struct SEffectData
	{
		SEffectData()
			: chromaFadeTimeInv(0.0f)
			, fadeOutTimeInv(0.0f)
			, baseContrast(1.0f)
			, baseBrightness(1.0f)
			, isInitialised(false)
		{}

		CCVarActivationSystem cvarActivationSystem;
		CPostEffectActivationSystem postEffectActivationSystem;
		SLetterBoxParams letterBox;
		float chromaFadeTimeInv;
		float fadeOutTimeInv;
		float baseContrast;
		float baseBrightness;
		bool isInitialised;
	};

public:
	enum EKillCamEffectMode
	{
		eKCEM_KillCam=0,
		eKCEM_KillerCam,
		eKCEM_IntroCam,
		eKCEM_TotalModes
	};

public:
	CKillCamGameEffect();
	~CKillCamGameEffect();

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual void	Release() override;
	virtual void	SetActive(bool isActive) override;
	virtual void	Update(float frameTime) override;

	virtual const char* GetName() const override;
	virtual void ResetRenderParameters() override;

	ILINE void SetCurrentMode(EKillCamEffectMode mode) { if(m_currentMode!=mode && IsFlagSet(GAME_EFFECT_ACTIVE)){ SetActive(false); } m_currentMode=mode; }
	ILINE void SetActiveIfCurrentMode(EKillCamEffectMode mode, bool active) { if( m_currentMode==mode ) { SetActive(active); } }

	ILINE void SetRemainingTime( const float remainingTime ) { m_remainingTime = remainingTime; }
	ILINE float GetFadeOutTimeInv( ) const { return s_data[m_currentMode].fadeOutTimeInv; }

#if DEBUG_GAME_FX_SYSTEM
	static void  DebugOnInputEvent(int keyId);
	static void	 DebugDisplay(const Vec2& textStartPos,float textSize,float textYStep);
#endif
	static void LoadStaticData(IItemParamsNode* rootNode);
	static void ReloadStaticData(IItemParamsNode* rootNode);
	static void ReleaseStaticData();

	static float GetBaseBrightness(const EKillCamEffectMode mode) { return s_data[mode].baseBrightness; }
	static float GetBaseContrast(const EKillCamEffectMode mode) { return s_data[mode].baseContrast; }

private:
	static void LoadStaticModeData(const IItemParamsNode* paramNode, SEffectData& data);

	void Start();
	void Stop();

	CLetterBoxHudEventListener m_letterBox[eKCEM_TotalModes];
	EKillCamEffectMode m_currentMode;
	float m_activeTime;
	float m_remainingTime;
	float m_originalBrightness;

	static SEffectData s_data[eKCEM_TotalModes];
};//------------------------------------------------------------------------------------------------

struct SKillCamGameEffectParams : public SGameEffectParams
{
	SKillCamGameEffectParams()
	{
		autoUpdatesWhenActive = false;
	}
};//------------------------------------------------------------------------------------------------

#endif // _KILL_CAM_GAME_EFFECT_
