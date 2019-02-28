// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Manages effects for scene blur - The effect needs to be managed in 1 global place to stop
// different game features fighting over setting the values.

// Includes
#include "StdAfx.h"
#include "SceneBlurGameEffect.h"

#include "ItemParams.h"

REGISTER_EFFECT_DEBUG_DATA(CSceneBlurGameEffect::DebugOnInputEvent,CSceneBlurGameEffect::DebugDisplay,SceneBlur);
REGISTER_DATA_CALLBACKS(CSceneBlurGameEffect::LoadStaticData,CSceneBlurGameEffect::ReleaseStaticData,CSceneBlurGameEffect::ReloadStaticData,SceneBlurData);

const char* SCENEBLUR_GAME_EFFECT_NAME = "SceneBlur";

//--------------------------------------------------------------------------------------------------
// Name: SSceneBlurGameEffectData
// Desc: Data loaded from xml to control game effect
//--------------------------------------------------------------------------------------------------
struct SSceneBlurGameEffectData
{
	struct SEffectBlendParams
	{
		SEffectBlendParams()
			: blurAmount(0.0f)
			, enterDelayDuration(0.0f)
			, enterInterpolateDuration(0.0f)
			, exitInterpolateDuration(0.0f)
		{
		}

		float blurAmount;
		float enterDelayDuration;
		float enterInterpolateDuration;
		float exitInterpolateDuration;
	};

	typedef CryFixedArray<SEffectBlendParams, NUMBLENDEDSCENEBLURUSAGES> TSceneBlurBlendedEffectData; // Could use eGameEffectUsage_NUMTYPES but uses extra memory which won't be used for non blended effects

	SSceneBlurGameEffectData()
	{
		for (int i = 0; i < NUMBLENDEDSCENEBLURUSAGES; i++)
		{
			blendedEffectData.push_back(SEffectBlendParams());
		}

		isInitialised = false;
	}

	void LoadEffectParamData(const IItemParamsNode* pParamNode, const CSceneBlurGameEffect::EGameEffectUsage effectUsage)
	{
		SSceneBlurGameEffectData::SEffectBlendParams& effectParams = blendedEffectData[effectUsage];

		CRY_ASSERT(pParamNode);
		if(pParamNode)
		{
			pParamNode->GetAttribute("blurAmount",effectParams.blurAmount);
			pParamNode->GetAttribute("enterDelayDuration",effectParams.enterDelayDuration);
			pParamNode->GetAttribute("enterInterpolateDuration",effectParams.enterInterpolateDuration);
			pParamNode->GetAttribute("exitInterpolateDuration",effectParams.exitInterpolateDuration);
		}
	}

	TSceneBlurBlendedEffectData blendedEffectData;
	bool												isInitialised;
};
static SSceneBlurGameEffectData s_sceneBlurGEData;

//--------------------------------------------------------------------------------------------------
// Name: CSceneBlurGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CSceneBlurGameEffect::CSceneBlurGameEffect()
{
	for (int i = 0; i < eGameEffectUsage_NUMTYPES; i++)
	{
		m_executionData.push_back(SGameEffectExecutionData());
	}
}

//--------------------------------------------------------------------------------------------------
// Name: ~CSceneBlurGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CSceneBlurGameEffect::~CSceneBlurGameEffect()
{
	
}

//--------------------------------------------------------------------------------------------------
// Name: LoadStaticData
// Desc: Loads static data for effect
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::LoadStaticData(IItemParamsNode* pRootNode)
{
	const IItemParamsNode* pParamNode = pRootNode->GetChild("SceneBlur");

	if(pParamNode)
	{
		// Interest videos params
		const IItemParamsNode* pSubNode = pParamNode->GetChild("InterestVideos");
		s_sceneBlurGEData.LoadEffectParamData(pSubNode, eGameEffectUsage_InterestVideos);
		s_sceneBlurGEData.isInitialised = true;
	}
}

//--------------------------------------------------------------------------------------------------
// Name: ReloadStaticData
// Desc: Reloads static data
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::ReloadStaticData(IItemParamsNode* pRootNode)
{
	ReleaseStaticData();
	LoadStaticData(pRootNode);

#if DEBUG_GAME_FX_SYSTEM
	// Data has been reloaded, so re-initialse debug effect with new data
	CSceneBlurGameEffect* pDebugEffect = (CSceneBlurGameEffect*)GAME_FX_SYSTEM.GetDebugEffect(SCENEBLUR_GAME_EFFECT_NAME);
	if(pDebugEffect && pDebugEffect->IsFlagSet(GAME_EFFECT_REGISTERED))
	{
		pDebugEffect->Initialise();
		// Re-activate effect if currently active, so it will register correctly
		if(pDebugEffect->IsFlagSet(GAME_EFFECT_ACTIVE))
		{
			pDebugEffect->SetActive(false);
			pDebugEffect->SetActive(true);
		}
	}
#endif
}

//--------------------------------------------------------------------------------------------------
// Name: ReleaseStaticData
// Desc: Releases static data
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::ReleaseStaticData()
{
	if(s_sceneBlurGEData.isInitialised)
	{
		s_sceneBlurGEData.isInitialised = false;
	}
}

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initializes game effect
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::Initialise(const SGameEffectParams* pGameEffectParams)
{
	CGameEffect::Initialise(pGameEffectParams);
}

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases game effect
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::Release()
{
	SetActive(false);

	CGameEffect::Release();
}

//--------------------------------------------------------------------------------------------------
// Name: SetActive
// Desc: Sets active status of effect
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::SetActive(bool isActive)
{
	const bool bActive = IsFlagSet(GAME_EFFECT_ACTIVE);
	if (bActive != isActive)
	{
		if(s_sceneBlurGEData.isInitialised)
		{
			CGameEffect::SetActive(isActive);

			if (!isActive)
			{
				gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", 0.0f);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// Name: GetName
// Desc: Gets effect's name
//--------------------------------------------------------------------------------------------------
const char* CSceneBlurGameEffect::GetName() const
{
	return SCENEBLUR_GAME_EFFECT_NAME;
}

void CSceneBlurGameEffect::ResetRenderParameters()
{
	gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", 0.0f);
}

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates effect
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::Update(float frameTime)
{
	// Have at least 1 active usage of effect otherwise would never enter this function

	CGameEffect::Update(frameTime);
	
	float fBlurAmount = 0.0f;
	float fCurBlurAmount = 0.0f;
	bool bActive = false;
	for (int i = 0; i < NUMBLENDEDSCENEBLURUSAGES; i++)
	{
		SGameEffectExecutionData& executionData = m_executionData[i];
		SSceneBlurGameEffectData::SEffectBlendParams& effectBlendParams = s_sceneBlurGEData.blendedEffectData[i];
		
		switch (executionData.m_effectState)
		{
		case eGameEffectBlendableState_InitialDelay:
			{
				const float fUpdatedTime = executionData.m_fUpdateTime += frameTime;
				if (fUpdatedTime >= effectBlendParams.enterDelayDuration)
				{
					executionData.m_fUpdateTime = 0.0f;
					executionData.m_effectState = eGameEffectBlendableState_BlendingTo;
				}

				bActive = true;
			}
			break;
		case eGameEffectBlendableState_BlendingTo:
			{
				const float fUpdatedTime = executionData.m_fUpdateTime += frameTime;

				const float fBlendDuration = effectBlendParams.enterInterpolateDuration;
				float fTimeFactor = 1.0f;
				if (fBlendDuration > 0.0f) // Prevent divide by 0
				{
					fTimeFactor = CLAMP((fUpdatedTime / fBlendDuration), 0.0f, 1.0f);
				}

				fCurBlurAmount = LERP(0.0f, executionData.m_fBlurAmount, fTimeFactor);

				if (fUpdatedTime >= fBlendDuration)
				{
					executionData.m_fUpdateTime = 0.0f;
					executionData.m_effectState = eGameEffectBlendableState_Hold;
				}

				bActive = true;
			}
			break;
		case eGameEffectBlendableState_Hold:
			{
				fCurBlurAmount = executionData.m_fBlurAmount;
				bActive = true;
			}
			break;
		case eGameEffectBlendableState_BlendingFrom:
			{
				const float fUpdatedTime = executionData.m_fUpdateTime += frameTime;

				const float fBlendDuration = effectBlendParams.exitInterpolateDuration;
				float fTimeFactor = 1.0f;
				if (fBlendDuration > 0.0f) // Prevent divide by 0
				{
					fTimeFactor = CLAMP((fUpdatedTime / fBlendDuration), 0.0f, 1.0f);
				}

				fCurBlurAmount = LERP(executionData.m_fBlurAmount, 0.0f, fTimeFactor);

				if (fUpdatedTime >= fBlendDuration)
				{
					executionData.m_fUpdateTime = 0.0f;
					executionData.m_effectState = eGameEffectBlendableState_None;
				}

				bActive = true;
			}
			break;
		}

		fBlurAmount = max(fBlurAmount, fCurBlurAmount);
	}

	// Now go through the non blended usages
	for (int i = NUMBLENDEDSCENEBLURUSAGES; i < eGameEffectUsage_NUMTYPES; i++)
	{
		SGameEffectExecutionData& executionData = m_executionData[i];

		if (executionData.m_effectState == eGameEffectBlendableState_Hold)
		{
			fBlurAmount = max(fBlurAmount, executionData.m_fBlurAmount);
			bActive = true;
		}
	}


	if (bActive)
	{
		gEnv->p3DEngine->SetPostEffectParam("FilterBlurring_Amount", fBlurAmount);
	}
	else
	{
		SetActive(false);
	}
}

//--------------------------------------------------------------------------------------------------
// Name: SetBlurAmount
// Desc: Sets blur amount from value
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::SetBlurAmount(const float fAmount, const EGameEffectUsage usage)
{
	CRY_ASSERT(usage >= 0 && usage < eGameEffectUsage_NUMTYPES);

	SGameEffectExecutionData& executionData = m_executionData[usage];

	executionData.m_fBlurAmount = fAmount;
	executionData.m_fUpdateTime = 0.0f;

	if (fAmount > FLT_EPSILON)
	{
		SetActive(true);
		executionData.m_effectState = eGameEffectBlendableState_Hold;
	}
	else
	{
		executionData.m_effectState = eGameEffectBlendableState_None;
	}
}

//--------------------------------------------------------------------------------------------------
// Name: SetBlurAmountFromData
// Desc: Sets blur amount from xml data
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::SetBlurAmountFromData(const bool bEnable, const EGameEffectUsage usage)
{
	CRY_ASSERT(usage >= 0 && usage < eGameEffectUsage_NUMTYPES);

	SGameEffectExecutionData& executionData = m_executionData[usage];
	executionData.m_fUpdateTime = 0.0f;
}

#if DEBUG_GAME_FX_SYSTEM
//--------------------------------------------------------------------------------------------------
// Name: DebugOnInputEvent
// Desc: Called when input events happen in debug builds
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::DebugOnInputEvent(int keyId)
{
	if(s_sceneBlurGEData.isInitialised)
	{
		CSceneBlurGameEffect* pEffect = (CSceneBlurGameEffect*)GAME_FX_SYSTEM.GetDebugEffect(SCENEBLUR_GAME_EFFECT_NAME);

		// Read input
		switch(keyId)
		{
		case eKI_NP_1:
			{
				// Create debug effect for development
				if(pEffect == NULL)
				{
					pEffect = new CSceneBlurGameEffect;
					pEffect->Initialise();
					pEffect->SetFlag(GAME_EFFECT_DEBUG_EFFECT,true);
				}
				break;
			}
		case eKI_NP_2:
			{
				SAFE_DELETE_GAME_EFFECT(pEffect);
				break;
			}
		case eKI_NP_4:
			{
				if(pEffect)
					pEffect->SetActive(true);
				break;
			}
		case eKI_NP_5:
			{
				if(pEffect)
					pEffect->SetActive(false);
				break;
			}
		}
	}
}


#include <CryRenderer/IRenderAuxGeom.h>

//--------------------------------------------------------------------------------------------------
// Name: DebugDisplay
// Desc: Display when this effect is selected to debug through the game effects system
//--------------------------------------------------------------------------------------------------
void CSceneBlurGameEffect::DebugDisplay(const Vec2& textStartPos,float textSize,float textYStep)
{
	ColorF textCol(1.0f,1.0f,0.0f,1.0f);
	Vec2 currentTextPos = textStartPos;

	if(s_sceneBlurGEData.isInitialised)
	{
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Create: NumPad 1");
		currentTextPos.y += textYStep;
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Destroy: NumPad 2");
		currentTextPos.y += textYStep;
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Turn on: NumPad 4");
		currentTextPos.y += textYStep;
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Turn off: NumPad 5");
		currentTextPos.y += textYStep;
	}
	else
	{
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Effect failed to load data");
	}
}

#endif
