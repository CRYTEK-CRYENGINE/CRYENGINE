// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "ModelToHudFlowNodes.h"
#include <CrySystem/Scaleform/IFlashUI.h>


CFlowSetupModelPostRender::CFlowSetupModelPostRender( SActivationInfo * pActInfo )
{
	characterModelIndex = CMenuRender3DModelMgr::kAddedModelIndex_Invalid;
	playerModelName = "";
}

IFlowNodePtr CFlowSetupModelPostRender::Clone(SActivationInfo *pActInfo)
{
	return new CFlowSetupModelPostRender(pActInfo);
}

void CFlowSetupModelPostRender::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_Void("Start", _HELP("Start rendering")),
		InputPortConfig_Void("Shutdown", _HELP("Shutdown")),
		InputPortConfig<string>( "uiMovieclips_MovieClip",_HELP("MovieClip to attach the texture to") ),
		InputPortConfig<Vec3>( "AmbientLightColor",_HELP("") ),
		InputPortConfig<float>( "AmbientLightStrength", 0.8f, _HELP("") ),
		InputPortConfig<Vec3>( "LightColor1", Vec3(5.f, 6.f, 5.5f), _HELP("") ),
		InputPortConfig<Vec3>( "LightColor2", Vec3(0.7f, 0.7f, 0.7f), _HELP("") ),
		InputPortConfig<Vec3>( "LightColor3", Vec3(0.5f, 1.0f, 0.7f), _HELP("") ),
		InputPortConfig<float>( "DebugScale", 0.0f, _HELP("0 = off, otherwise 0 - 1 to visualize the debugview with UVmap") ),
		{0}
	};
	static const SOutputPortConfig outputs[] = {
		{0}
	};    
	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.sDescription = _HELP("Add 3d model to HUD RT");
	config.SetCategory(EFLN_APPROVED);
}

void CFlowSetupModelPostRender::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	switch (event)
	{
	case eFE_Initialize:
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, IN_SHUTDOWN))
		{
			CMenuRender3DModelMgr::Release(true);

			ITexture *tex = NULL;
			//unbind tex from MC
			if(gEnv->pConsole->GetCVar("r_UsePersistentRTForModelHUD")->GetIVal() > 0)		
				tex  = gEnv->pRenderer->EF_LoadTexture("$ModelHUD");
			else
				tex  = gEnv->pRenderer->EF_LoadTexture("$BackBuffer");

			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(sUIElement.c_str());
			IUIElement* pUIElement = pElement ? pElement->GetInstance(0) : NULL;
			if(pUIElement)
				pUIElement->UnloadTexFromMc(sMovieClipName.c_str(), tex);

			return;
		}
		
		if (IsPortActive(pActInfo, IN_START))
		{
			INDENT_LOG_DURING_SCOPE();

			// Setup scene settings
			CMenuRender3DModelMgr::SSceneSettings sceneSettings;
			sceneSettings.fovScale = 0.85f;
			sceneSettings.fadeInSpeed = 0.01f;
			sceneSettings.flashEdgeFadeScale = 0.0f;
			sceneSettings.ambientLight = Vec4(GetPortVec3(pActInfo, IN_AMBIENTLIGHTCOLOR), GetPortFloat(pActInfo, IN_AMBIENTLIGHTSTRENGTH));
			sceneSettings.lights.resize(3);
			sceneSettings.lights[0].pos.Set(-25.f, -10.f, 30.f);
			sceneSettings.lights[0].color = GetPortVec3(pActInfo, IN_LIGHTCOLOR1);
			sceneSettings.lights[0].specular = 4.f;
			sceneSettings.lights[0].radius = 400.f;
			sceneSettings.lights[1].pos.Set(25.f, -4.f, 30.f);
			sceneSettings.lights[1].color = GetPortVec3(pActInfo, IN_LIGHTCOLOR2);
			sceneSettings.lights[1].specular = 10.f;
			sceneSettings.lights[1].radius = 400.f;
			sceneSettings.lights[2].pos.Set(60.f, 40.f, 10.f);
			sceneSettings.lights[2].color = GetPortVec3(pActInfo, IN_LIGHTCOLOR3);
			sceneSettings.lights[2].specular = 10.f;
			sceneSettings.lights[2].radius = 400.f;

			//Create scene
			CMenuRender3DModelMgr *renderModels = new CMenuRender3DModelMgr();
			renderModels->SetSceneSettings(sceneSettings);

			ITexture *tex = NULL;
			//Fetch texture and send to movieclip
			if(gEnv->pConsole->GetCVar("r_UsePersistentRTForModelHUD")->GetIVal() > 0)		
				tex  = gEnv->pRenderer->EF_LoadTexture("$ModelHUD");
			else
				tex  = gEnv->pRenderer->EF_LoadTexture("$BackBuffer");

			string sStr = GetPortString(pActInfo, IN_MC);
			string::size_type sPos = sStr.find( ':' );
			sUIElement = sStr.substr( 0, sPos );
			sMovieClipName = sStr.substr( sPos + 1 );

			IUIElement* pElement = gEnv->pFlashUI->GetUIElement(sUIElement.c_str());
			IUIElement* pUIElement = pElement ? pElement->GetInstance(0) : NULL;
			if(pUIElement)
				pUIElement->LoadTexIntoMc(sMovieClipName.c_str(), tex);
			else
				CryWarning(VALIDATOR_MODULE_FLOWGRAPH, VALIDATOR_WARNING, "CFlowAdd3DModelToHUD: Movieclip not found");

			//check debug info
			float debugscale = GetPortFloat(pActInfo, IN_DEBUGSCALE);
			ICVar* cv_debugRender = gEnv->pConsole->GetCVar("g_post3DRendererDebug");
			if(debugscale > 0.0f)
			{
				if(cv_debugRender)
				{
					cv_debugRender->Set(2);
					CMenuRender3DModelMgr::GetInstance()->SetDebugScale(debugscale);
				}
			}
			else
			{
				cv_debugRender->Set(0);
			}
			
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------
void CFlowSetupModelPostRender::GetMemoryUsage(ICrySizer * s) const
{
	s->Add(*this);
}

//--------------------------------------------------------------------------------------------
CFlowAddModelToPostRender::CFlowAddModelToPostRender( SActivationInfo * pActInfo )
{
	characterModelIndex = CMenuRender3DModelMgr::kAddedModelIndex_Invalid;
	playerModelName = "";
}

IFlowNodePtr CFlowAddModelToPostRender::Clone(SActivationInfo *pActInfo)
{
	return new CFlowAddModelToPostRender(pActInfo);
}

void CFlowAddModelToPostRender::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_Void("Add", _HELP("Start rendering")),
		InputPortConfig<string>( "Model",_HELP("Path to cdf file of model") ),
		InputPortConfig<float>( "Scale", 1.0f, _HELP("Scale of the model") ),
		InputPortConfig<string>( "Animation",_HELP("Name of the animation") ),
		InputPortConfig<float>( "PlaySpeed", 1.0f, _HELP("speed of the animation") ),
		InputPortConfig<Vec3>( "EntityPos", Vec3(0.1f, 0.0f, -0.2f), _HELP("") ),
		InputPortConfig<Vec3>( "EntityRot", Vec3(0.f, 0.f, 3.5f), _HELP("") ),
		InputPortConfig<Vec3>( "EntityContinuousRot",_HELP("For showcasing the model") ),
		InputPortConfig<Vec3>( "ScreenUV",_HELP("") ),
		InputPortConfig<Vec3>( "ScreenU2V2",_HELP("") ),
		{0}
	};

	static const SOutputPortConfig outputs[] = {
		{0}
	};    

	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.sDescription = _HELP("Add 3d model to HUD RT");
	config.SetCategory(EFLN_APPROVED);
}

void CFlowAddModelToPostRender::ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
{
	switch (event)
	{
	case eFE_Initialize:
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, IN_ADD))
		{
			INDENT_LOG_DURING_SCOPE();
			//Create scene
			CMenuRender3DModelMgr *renderModels = CMenuRender3DModelMgr::GetInstance();
			
			if (renderModels)
			{
				//Add model with animation if needed
				const float playSpeed = GetPortFloat(pActInfo, IN_ANIM_SPEED);
				const string usePlayerModelName = GetPortString(pActInfo, IN_MODEL);
				const string useAnimation = GetPortString(pActInfo, IN_ANIM);

				CMenuRender3DModelMgr::SModelParams params;
				params.pFilename = usePlayerModelName.c_str();
				params.posOffset = GetPortVec3(pActInfo, IN_ENTITYPOS);
				params.rot = Ang3(GetPortVec3(pActInfo, IN_ENTITYROT));
				params.continuousRot = Ang3(GetPortVec3(pActInfo, IN_ENTITYCONTROT));
				params.scale = GetPortFloat(pActInfo, IN_SCALE);
				params.pName = "char";
				params.screenRect[0] = GetPortVec3(pActInfo, IN_SCREENUV).x;
				params.screenRect[1] = GetPortVec3(pActInfo, IN_SCREENUV).y;
				params.screenRect[2] = GetPortVec3(pActInfo, IN_SCREENU2V2).x;
				params.screenRect[3] = GetPortVec3(pActInfo, IN_SCREENU2V2).y;

				if(!usePlayerModelName.empty())
					characterModelIndex = renderModels->AddModel(params);

				if(!useAnimation.empty())
					renderModels->UpdateAnim(characterModelIndex, useAnimation, playSpeed);
			}
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------
void CFlowAddModelToPostRender::GetMemoryUsage(ICrySizer * s) const
{
	s->Add(*this);
}

//--------------------------------------------------------------------------------------------
REGISTER_FLOW_NODE( "HUD:SetupModelPostRender", CFlowSetupModelPostRender);
REGISTER_FLOW_NODE( "HUD:AddModelToPostRender", CFlowAddModelToPostRender);
