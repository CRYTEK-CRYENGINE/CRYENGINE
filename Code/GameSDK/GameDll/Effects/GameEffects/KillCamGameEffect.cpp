// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Kill Cam visual effect.

// Includes
#include "StdAfx.h"
#include "KillCamGameEffect.h"
#include "ItemParams.h"
#include "Effects/Tools/CVarActivationSystem.h"
#include "Effects/Tools/PostEffectActivationSystem.h"
#include "Network/Lobby/GameLobby.h"
#include "UI/UIManager.h"
#include "UI/UIHUD3D.h"

REGISTER_EFFECT_DEBUG_DATA(CKillCamGameEffect::DebugOnInputEvent,CKillCamGameEffect::DebugDisplay,KillCam);
REGISTER_DATA_CALLBACKS(CKillCamGameEffect::LoadStaticData,CKillCamGameEffect::ReleaseStaticData,CKillCamGameEffect::ReloadStaticData,KillCamData);

//--------------------------------------------------------------------------------------------------
// Desc: Static data
//--------------------------------------------------------------------------------------------------
const char* KILL_CAM_GAME_EFFECT_NAME = "Kill Cam";
const char* KILL_CAM_GAME_EFFECT_MODE_NAMES[] =
{
	"KillCam",
	"KillerCam",
	"IntroCam"
};
CKillCamGameEffect::SEffectData CKillCamGameEffect::s_data[eKCEM_TotalModes];


//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CKillCamGameEffect
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CKillCamGameEffect::CKillCamGameEffect()
	: m_currentMode(eKCEM_KillCam)
	, m_remainingTime(0.0f)
	, m_activeTime(0.0f)
	, m_originalBrightness(1.0f)
{

}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CKillCamGameEffect
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CKillCamGameEffect::~CKillCamGameEffect()
{

}
//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Initialise
// Desc: Initializes game effect
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::Initialise(const SGameEffectParams* gameEffectParams)
{
	SKillCamGameEffectParams killCamGameEffectParams;
	if(gameEffectParams)
	{
		killCamGameEffectParams = *(SKillCamGameEffectParams*)(gameEffectParams);
	}

	CGameEffect::Initialise(&killCamGameEffectParams);

	for( int i=0; i<eKCEM_TotalModes; i++ )
	{
		m_letterBox[i].Initialise(&s_data[i].letterBox);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Release
// Desc: Releases game effect
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::Release()
{
	SetActive(false);

	CGameEffect::Release();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Update
// Desc: Updates the Effect.
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::Update( float frameTime )
{
	CGameEffect::Update(frameTime);

	if(IsFlagSet(GAME_EFFECT_ACTIVE))
	{
		m_activeTime+=frameTime;
		const SEffectData& data = s_data[m_currentMode];
		if(data.chromaFadeTimeInv>0.f)
		{
			const float scale = clamp_tpl( 1.f-(m_activeTime*data.chromaFadeTimeInv), 0.f, 1.f );
			if(scale>0.01f)
			{
				static float chromabase = 0.1f;
				static float chromaStrength = 2.5f;
				static const Vec4 chroma(chromabase,chromabase,chromabase,chromaStrength);
				static float colorbase = 1.2f;
				static float grain = 2.f;
				static const Vec4 color(colorbase,colorbase,colorbase,grain);

				gEnv->pRenderer->EF_SetPostEffectParam("FilterKillCamera_Active", 1.f, true);
				gEnv->pRenderer->EF_SetPostEffectParamVec4("FilterKillCamera_ChromaShift", chroma*scale, true);
				gEnv->pRenderer->EF_SetPostEffectParamVec4("FilterKillCamera_ColorScale", color*scale, true);
			}
			else
			{
				gEnv->pRenderer->EF_SetPostEffectParamVec4("FilterKillCamera_ChromaShift", Vec4(0.f,0.f,0.f,0.f), true);
				gEnv->pRenderer->EF_SetPostEffectParamVec4("FilterKillCamera_ColorScale", Vec4(0.f,0.f,0.f,0.f), true);
			}
		}

		if(data.fadeOutTimeInv>0.f)
		{
			if(m_remainingTime>=0.f)
			{
				m_remainingTime = (float)__fsel(m_remainingTime-frameTime,m_remainingTime-frameTime,0.f);
				const float blackOut = SmoothBlendValue(clamp_tpl(1.f-(m_remainingTime*data.fadeOutTimeInv),0.f,1.f));
				const float brightness = LERP(m_originalBrightness,0.f,blackOut);
				gEnv->pRenderer->EF_SetPostEffectParam("Global_User_Brightness", brightness, true);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// Name: GetName
// Desc: Gets effect's name
//--------------------------------------------------------------------------------------------------
const char* CKillCamGameEffect::GetName() const
{
	return KILL_CAM_GAME_EFFECT_NAME;
}//-------------------------------------------------------------------------------------------------

void CKillCamGameEffect::ResetRenderParameters()
{
	gEnv->pRenderer->EF_SetPostEffectParam("FilterKillCamera_Active", 0.0f, true);
}

//--------------------------------------------------------------------------------------------------
// Name: SetActive
// Desc: Sets active status of effect
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::SetActive(bool isActive)
{
	if(s_data[m_currentMode].isInitialised && gEnv->pRenderer)
	{
		CUIHUD3D *pHUD = UIEvents::Get<CUIHUD3D>();
		if(pHUD)
		{
			pHUD->SetVisible(!isActive);
		}

		if(isActive)
		{
			Start();
		}
		else
		{
			Stop();
		}
		CGameEffect::SetActive(isActive);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Start
// Desc: Starts effect
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::Start()
{
	if(IsFlagSet(GAME_EFFECT_ACTIVE) == false)
	{
		m_letterBox[m_currentMode].Register();
		m_activeTime = 0.f;
		m_remainingTime = -1.f;
		s_data[m_currentMode].cvarActivationSystem.StoreCurrentValues();
		s_data[m_currentMode].cvarActivationSystem.SetCVarsActive(true);
	}
	// Outside to re-apply if already started.
	// This fixes an issue where the reset of all fx is deferred and overwrites these.
	s_data[m_currentMode].postEffectActivationSystem.SetPostEffectsActive(true);
	if(s_data[m_currentMode].fadeOutTimeInv>0.f)
	{
		gEnv->pRenderer->EF_GetPostEffectParam("Global_User_Brightness", m_originalBrightness);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: Stop
// Desc: Stops effect
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::Stop()
{
	if(IsFlagSet(GAME_EFFECT_ACTIVE))
	{
		const int curMode = m_currentMode;
		m_letterBox[curMode].UnRegister();
		s_data[curMode].cvarActivationSystem.SetCVarsActive(false);
		s_data[curMode].postEffectActivationSystem.SetPostEffectsActive(false);
	}
}//-------------------------------------------------------------------------------------------------



//////////////////////////////////////////////////////////////////////////
//STATIC
//--------------------------------------------------------------------------------------------------
// Name: LoadStaticData
// Desc: Loads static data for KillCam
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::LoadStaticData(IItemParamsNode* rootNode)
{
	CGameLobby* pGameLobby = g_pGame->GetGameLobby();
	const char * pLevelName = pGameLobby ? pGameLobby->GetCurrentLevelName() : "";
	const int children = rootNode->GetChildCount();
	for( int m=0; m<eKCEM_TotalModes; m++ )
	{
		for( int i=0; i<children; i++ )
		{
			const IItemParamsNode* paramNode = rootNode->GetChild(i);
			if(!stricmp(paramNode->GetName(), KILL_CAM_GAME_EFFECT_MODE_NAMES[m]))
			{
				if(pLevelName)
				{
					if(const char* pSpecificLevel = paramNode->GetAttribute("level"))
					{
						if(stricmp(pSpecificLevel, pLevelName))
						{
							continue;
						}
					}
				}
				LoadStaticModeData(paramNode, s_data[m]);
			}
		}
	}
}
//--------------------------------------------------------------------------------------------------
// Name: LoadStaticModeData
// Desc: Loads static data effect mode.
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::LoadStaticModeData( const IItemParamsNode* paramNode, SEffectData& data )
{
	if(paramNode)
	{
		// Initialise CVars
		data.cvarActivationSystem.Initialise(paramNode->GetChild("CVars"));

		// Find the Contrast and Brightness PostEffect values.
		data.baseContrast = data.baseBrightness = 1.f;
		if(const IItemParamsNode* pPostEffectsNode = paramNode->GetChild("PostEffects"))
		{
			const int childCount = pPostEffectsNode->GetChildCount();

			int paramIndex=0;
			for(int i=0; i<childCount; i++)
			{
				const IItemParamsNode* pChildNode = pPostEffectsNode->GetChild(i);
				{
					if(!strcmp(pChildNode->GetName(),"Global_User_Contrast"))
					{
						pChildNode->GetAttribute("activeValue", data.baseContrast);
					}
					else if(!strcmp(pChildNode->GetName(),"Global_User_Brightness"))
					{
						pChildNode->GetAttribute("activeValue", data.baseBrightness);
					}
				}
			}

			// Initialise post effects
			data.postEffectActivationSystem.Initialise(pPostEffectsNode);
		}


		// Initialise letter box
		const IItemParamsNode* letterBoxXmlNode = paramNode->GetChild("letterBox");
		if(letterBoxXmlNode)
		{
			letterBoxXmlNode->GetAttribute("scale",data.letterBox.scale);

			letterBoxXmlNode->GetAttribute("red",data.letterBox.color.r);
			letterBoxXmlNode->GetAttribute("green",data.letterBox.color.g);
			letterBoxXmlNode->GetAttribute("blue",data.letterBox.color.b);
			letterBoxXmlNode->GetAttribute("alpha",data.letterBox.color.a);
		}

		// Chroma Fade in.
		data.chromaFadeTimeInv = 0.f;
		if(paramNode->GetAttribute("chromaFadeTime", data.chromaFadeTimeInv)) { data.chromaFadeTimeInv=__fres(data.chromaFadeTimeInv); }

		// Fade Out Time.
		data.fadeOutTimeInv = 0.f;
		if(paramNode->GetAttribute("fadeOutTime", data.fadeOutTimeInv)) { data.fadeOutTimeInv=__fres(data.fadeOutTimeInv); }

		data.isInitialised = true;
	}

}
//--------------------------------------------------------------------------------------------------
// Name: ReloadStaticData
// Desc: Reloads static data
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::ReloadStaticData(IItemParamsNode* rootNode)
{
	ReleaseStaticData();
	LoadStaticData(rootNode);

#if DEBUG_GAME_FX_SYSTEM
	// Data has been reloaded, so re-initialse debug effect with new data
	CKillCamGameEffect* pDebugKillCamEffect = (CKillCamGameEffect*)GAME_FX_SYSTEM.GetDebugEffect(KILL_CAM_GAME_EFFECT_NAME);
	if(pDebugKillCamEffect && pDebugKillCamEffect->IsFlagSet(GAME_EFFECT_REGISTERED))
	{
		pDebugKillCamEffect->Initialise();
		// Re-activate effect if currently active, so HUD Letter box will register correctly
		if(pDebugKillCamEffect->IsFlagSet(GAME_EFFECT_ACTIVE))
		{
			pDebugKillCamEffect->SetActive(false);
			pDebugKillCamEffect->SetActive(true);
		}
	}
#endif
}
//--------------------------------------------------------------------------------------------------
// Name: ReleaseStaticData
// Desc: Releases static data
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::ReleaseStaticData()
{
	for( int i=0; i<eKCEM_TotalModes; i++ )
	{
		if(s_data[i].isInitialised)
		{
			s_data[i].cvarActivationSystem.Release();
			s_data[i].postEffectActivationSystem.Release();
			s_data[i].isInitialised = false;
		}
	}
}
//-------------------------------------------------------------------------------------------------





//////////////////////////////////////////////////////////////////////////
// DEBUG
#if DEBUG_GAME_FX_SYSTEM
static int debugMode = 0;

//--------------------------------------------------------------------------------------------------
// Name: DebugOnInputEvent
// Desc: Called when input events happen in debug builds
//--------------------------------------------------------------------------------------------------
void CKillCamGameEffect::DebugOnInputEvent(int keyId)
{
	if(s_data[debugMode].isInitialised)
	{
		CKillCamGameEffect* pKillCamEffect = (CKillCamGameEffect*)GAME_FX_SYSTEM.GetDebugEffect(KILL_CAM_GAME_EFFECT_NAME);

		// Create debug effect for development
		if(pKillCamEffect == NULL)
		{
			SKillCamGameEffectParams killCamInitParams;
			killCamInitParams.autoDelete = true;

			pKillCamEffect = new CKillCamGameEffect;
			pKillCamEffect->Initialise(&killCamInitParams);
			pKillCamEffect->SetFlag(GAME_EFFECT_DEBUG_EFFECT,true);
		}

		// Read input
		if(pKillCamEffect && !pKillCamEffect->IsFlagSet(GAME_EFFECT_RELEASED))
		{
			switch(keyId)
			{
				case eKI_NP_1:
				{
					pKillCamEffect->SetActive(true);
					break;
				}
				case eKI_NP_2:
				{
					pKillCamEffect->SetActive(false);
					break;
				}
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: DebugDisplay
// Desc: Display when this effect is selected to debug through the game effects system
//--------------------------------------------------------------------------------------------------

#include <CryRenderer/IRenderAuxGeom.h>

void CKillCamGameEffect::DebugDisplay(const Vec2& textStartPos,float textSize,float textYStep)
{
	ColorF textCol(1.0f,1.0f,0.0f,1.0f);
	Vec2 currentTextPos = textStartPos;

	if(s_data[debugMode].isInitialised)
	{
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Turn on: NumPad 1");
		currentTextPos.y += textYStep;
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Turn off: NumPad 2");
	}
	else
	{
		IRenderAuxText::Draw2dLabel(currentTextPos.x,currentTextPos.y,textSize,&textCol.r,false,"Effect failed to load data");
	}
}//-------------------------------------------------------------------------------------------------

#endif
