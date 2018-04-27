// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Manages the value of the game effects' parameters when more than one module is trying to
// modify it at the same time.

// Includes
#include "StdAfx.h"
#include "ParameterGameEffect.h"

#include "ItemParams.h"
#include "RecordingSystem.h"

const char* PARAMETER_GAME_EFFECT_NAME = "ParameterGameEffect";

//--------------------------------------------------------------------------------------------------
// Name: CParameterGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CParameterGameEffect::CParameterGameEffect()
{
	for (int i = 0; i < eSEU_NUMTYPES; i++)
	{
		m_saturationExecutionData.push_back(SSaturationData());
	}

	m_saturationExecutionData[eSEU_Intro].m_amount = -1.0f; // disable by default
}

//--------------------------------------------------------------------------------------------------
// Name: SetActive
// Desc: Sets active status of effect
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::SetActive(bool isActive)
{
	const bool bActive = IsFlagSet(GAME_EFFECT_ACTIVE);
	if (bActive != isActive)
	{
		if (isActive)
		{
			CGameEffect::SetActive(true);
		}
		else
		{
			float currentSaturation;
			gEnv->p3DEngine->GetPostEffectParam("Global_User_Saturation", currentSaturation);

			if (currentSaturation >0.99f)
			{
				CGameEffect::SetActive(false);
			}
			else
			{
				gEnv->p3DEngine->SetPostEffectParam("Global_User_Saturation", 1.0f);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// Name: GetName
// Desc: Gets effect's name
//--------------------------------------------------------------------------------------------------
const char* CParameterGameEffect::GetName() const
{
	return PARAMETER_GAME_EFFECT_NAME;
}

//--------------------------------------------------------------------------------------------------
// Name: UpdateSaturation
// Desc: Updates saturation effect
//--------------------------------------------------------------------------------------------------
bool CParameterGameEffect::UpdateSaturation(float frameTime)
{
	float minimumSaturation = 1.f;
	for (unsigned int i = 0; i < eSEU_NUMTYPES; i++)
	{
		if(i != eSEU_Intro)
		{
			float saturationAmount = m_saturationExecutionData[i].m_amount;
			
			if (i == eSEU_LeavingBattleArea)
			{
				CRecordingSystem * pRecordingSystem = g_pGame->GetRecordingSystem();
				if (pRecordingSystem && pRecordingSystem->IsPlayingBack())
				{
					saturationAmount = 1.f;
				}
			}

			minimumSaturation = min(minimumSaturation, saturationAmount);	
		}
	}

	// Intro has priority (-1.0f amount for intro means disabled)
	if(m_saturationExecutionData[eSEU_Intro].m_amount > -1.0f)
	{
		minimumSaturation = m_saturationExecutionData[eSEU_Intro].m_amount;
	}

	gEnv->p3DEngine->SetPostEffectParam("Global_User_Saturation", minimumSaturation);

	return (minimumSaturation!=1.f);
}

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates effect
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::Update(float frameTime)
{
	// Have at least 1 active usage of effect otherwise would never enter this function

	CGameEffect::Update(frameTime);

	bool desaturationActive = UpdateSaturation(frameTime);

	SetActive(desaturationActive);
}

//--------------------------------------------------------------------------------------------------
// Name: SetSaturationAmount
// Desc: Sets the saturation amount for a given usage
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::SetSaturationAmount(const float fAmount, const ESaturationEffectUsage usage)
{
	CRY_ASSERT(usage >= 0 && usage < eSEU_NUMTYPES);

	SSaturationData& executionData = m_saturationExecutionData[usage];

	executionData.m_amount = fAmount;

	SetActive(IsFlagSet(GAME_EFFECT_ACTIVE) || (fAmount!=1.f));
}

//--------------------------------------------------------------------------------------------------
// Name: Reset
// Desc: Sets the effects data to default values
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::Reset()
{
	ResetSaturation();
}

//--------------------------------------------------------------------------------------------------
// Name: ResetRenderParameters
// Desc: Sets the effects data to default values
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::ResetRenderParameters()
{
	Reset();
}

//--------------------------------------------------------------------------------------------------
// Name: Reset
// Desc: Sets the saturation data to default values
//--------------------------------------------------------------------------------------------------
void CParameterGameEffect::ResetSaturation()
{
	for (int i=0; i<eSEU_NUMTYPES; ++i)
	{
		m_saturationExecutionData[i].m_amount = 1.f;
	}

	// Always disable intro saturation by default
	m_saturationExecutionData[eSEU_Intro].m_amount = -1.0f;
}
