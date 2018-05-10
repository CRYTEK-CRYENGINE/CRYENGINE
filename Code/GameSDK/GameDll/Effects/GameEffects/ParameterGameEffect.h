// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _PARAMETER_GAME_EFFECT_
#define _PARAMETER_GAME_EFFECT_

#pragma once



// Includes
#include "GameEffect.h"
#include "Effects/GameEffectsSystem.h"

// Forward declarations

//==================================================================================================
// Name: CParameterGameEffect
// Desc: Manages the value of the game effects' parameters when more than one module is trying to
//			modify it at the same time
// Author: Sergi Juarez
//==================================================================================================
class CParameterGameEffect : public CGameEffect
{
public:
	CParameterGameEffect();

	
	virtual void	SetActive(bool isActive) override;
	virtual void	Update(float frameTime) override;
	virtual const char* GetName() const override;
	        void	Reset();
	virtual void ResetRenderParameters() override;

//************************************************************************************
//		SATURATION EFFECT--
//************************************************************************************
public:
	enum ESaturationEffectUsage
	{
		eSEU_PreMatch = 0,
		eSEU_LeavingBattleArea,
		eSEU_PlayerHealth,
		eSEU_Intro,
		eSEU_NUMTYPES,
	};

	void SetSaturationAmount(const float fAmount, const ESaturationEffectUsage usage);

private:
	bool UpdateSaturation(float frameTime);
	void ResetSaturation();

private:
	struct SSaturationData 
	{
		float m_amount;

		SSaturationData(): m_amount(1.f){}
	};

	typedef CryFixedArray<SSaturationData, eSEU_NUMTYPES> TSaturationEffectExecutionData;
	TSaturationEffectExecutionData m_saturationExecutionData;
//************************************************************************************
//		--SATURATION EFFECT
//************************************************************************************


};

#endif // _SCENEBLUR_GAME_EFFECT_