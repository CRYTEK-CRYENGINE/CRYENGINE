// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <CryNetwork/IServiceNetwork.h>
#include <CryNetwork/IRemoteCommand.h>
#include <CryMath/ISplines.h>
#include "LiveCreateManager.h"
#include "LiveCreateHost.h"

#include "LiveCreate_System.h"
#include <CryAISystem/IAISystem.h>

#ifndef NO_LIVECREATE

namespace LiveCreate
{

IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetCameraPosition);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetCameraFOV);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EnableLiveCreate);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_DisableLiveCreate);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_EnableCameraSync);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_DisableCameraSync);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetCVar);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDay);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayValue);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayFull);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetEnvironmentFull);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetArchetypesFull);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SetParticlesFull);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_FileSynced);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SyncSelection);
IMPLEMENT_LIVECREATE_COMMAND(CLiveCreateCmd_SyncLayerVisibility);

bool bIsCameraSyncEnabled = false;

void CLiveCreateCmd_SetCameraPosition::Execute()
{
	if (bIsCameraSyncEnabled)
	{
		// prepare view parameters
		IView* pView = GetActiveView();
		if (NULL != pView)
		{
			SViewParams params = *pView->GetCurrentParams();
			params.position = m_position;
			params.rotation = m_rotation;
			params.ResetBlending();

			// update camera entity
			IEntity* pCameraEntity = GetCameraEntity();
			if (NULL != pCameraEntity)
			{
				pCameraEntity->SetPos(m_position);
				pCameraEntity->SetRotation(m_rotation);
			}

			IViewSystem* pViewSystem = GetViewSystem();
			pViewSystem->SetOverrideCameraRotation(true, m_rotation);
			pViewSystem->GetActiveView()->SetCurrentParams(params);
			pViewSystem->GetActiveView()->ResetShaking();
		}
	}
}

void CLiveCreateCmd_SetCameraFOV::Execute()
{
	if (bIsCameraSyncEnabled)
	{
		// prepare view parameters
		IView* pView = GetActiveView();
		if (NULL != pView)
		{
			SViewParams params = *pView->GetCurrentParams();
			params.fov = m_fov;
			params.ResetBlending();

			IViewSystem* pViewSystem = GetViewSystem();
			pViewSystem->GetActiveView()->SetCurrentParams(params);
			pViewSystem->GetActiveView()->ResetShaking();
		}
	}
}

void CLiveCreateCmd_EnableLiveCreate::Execute()
{
	gEnv->pConsole->ExecuteString("g_godMode 1");
	gEnv->pConsole->ExecuteString("sys_ai 0");
	gEnv->pConsole->ExecuteString("AI_IgnorePlayer 1");
	gEnv->pConsole->ExecuteString("AI_NoUpdate 1");
	gEnv->pConsole->ExecuteString("mov_NoCutscenes 1");
	gEnv->pConsole->ExecuteString("e_ObjectLayersActivation 0");
	gEnv->pConsole->ExecuteString("e_ObjectLayersActivationPhysics 0");
	gEnv->pMovieSystem->StopAllCutScenes();
	gEnv->pMovieSystem->StopAllSequences();
	gEnv->p3DEngine->ResetPostEffects();

	// show layers
	for (uint32 i = 0; i < m_visibleLayers.size(); ++i)
	{
		const string& layerName = m_visibleLayers[i];

		gEnv->pEntitySystem->ToggleLayerVisibility(layerName.c_str(), true);

		const int layerIndex = gEnv->pEntitySystem->GetLayerId(layerName.c_str());
		if (-1 != layerIndex)
		{
			const bool bCheckLayerActivation = false;
			gEnv->p3DEngine->ActivateObjectsLayer((uint16)layerIndex, true, true, true, true, "ALL_LAYERS", NULL, bCheckLayerActivation);
		}

		if (gEnv->pAISystem)
		{
			gEnv->pAISystem->LayerEnabled(layerName.c_str(), true, false);
		}
	}

	// hide layers
	for (uint32 i = 0; i < m_hiddenLayers.size(); ++i)
	{
		const string& layerName = m_hiddenLayers[i];

		gEnv->pEntitySystem->ToggleLayerVisibility(layerName.c_str(), false);

		const int layerIndex = gEnv->pEntitySystem->GetLayerId(layerName.c_str());
		if (-1 != layerIndex)
		{
			const bool bCheckLayerActivation = false;
			gEnv->p3DEngine->ActivateObjectsLayer((uint16)layerIndex, false, true, true, true, "ALL_LAYERS", NULL, bCheckLayerActivation);
		}
	}
}

void CLiveCreateCmd_DisableLiveCreate::Execute()
{
	gEnv->pConsole->ExecuteString("g_godMode 0");
	gEnv->pConsole->ExecuteString("sys_ai 1");
	gEnv->pConsole->ExecuteString("AI_IgnorePlayer 0");
	gEnv->pConsole->ExecuteString("AI_NoUpdate 0");
	gEnv->pConsole->ExecuteString("mov_NoCutscenes 0");
	gEnv->pConsole->ExecuteString("e_ObjectLayersActivation 1");
	gEnv->pConsole->ExecuteString("e_ObjectLayersActivationPhysics 1");
	gEnv->p3DEngine->ResetPostEffects();

	if (m_bRestorePhysicsState)
	{
		gEnv->pEntitySystem->ResumePhysicsForSuppressedEntities(m_bWakePhysicalObjects);
	}

	CHost* pHost = static_cast<CHost*>(gEnv->pLiveCreateHost);
	pHost->ClearSelectionData();
}

void CLiveCreateCmd_EnableCameraSync::Execute()
{
	if (!bIsCameraSyncEnabled)
	{
		gEnv->pConsole->ExecuteString("p_fly_mode 1");
		gEnv->pConsole->ExecuteString("g_detachCamera 1");
		gEnv->pMovieSystem->StopAllCutScenes();
		gEnv->pMovieSystem->StopAllSequences();
		gEnv->p3DEngine->ResetPostEffects();

		IEntity* pPlayerEntity = GetPlayerEntity();
		if (NULL != pPlayerEntity)
		{
			pPlayerEntity->Hide(true);
		}

		// prepare view parameters
		IView* pView = GetActiveView();
		if (NULL != pView)
		{
			SViewParams params = *pView->GetCurrentParams();
			params.position = m_position;
			params.rotation = m_rotation;
			params.ResetBlending();

			// update camera entity
			IEntity* pCameraEntity = GetCameraEntity();
			if (NULL != pCameraEntity)
			{
				pCameraEntity->SetPos(m_position);
				pCameraEntity->SetRotation(m_rotation);
			}

			IViewSystem* pViewSystem = GetViewSystem();
			pViewSystem->SetOverrideCameraRotation(true, m_rotation);
			pViewSystem->GetActiveView()->SetCurrentParams(params);
			pViewSystem->GetActiveView()->ResetShaking();
		}

		bIsCameraSyncEnabled = true;
	}
}

void CLiveCreateCmd_DisableCameraSync::Execute()
{
	if (bIsCameraSyncEnabled)
	{
		gEnv->pConsole->ExecuteString("p_fly_mode 0");
		gEnv->pConsole->ExecuteString("g_detachCamera 0");
		gEnv->p3DEngine->ResetPostEffects();

		IEntity* pPlayerEntity = GetPlayerEntity();
		if (NULL != pPlayerEntity)
		{
			pPlayerEntity->Hide(false);
		}

		IViewSystem* pViewSystem = GetViewSystem();
		if (NULL != pViewSystem)
		{
			pViewSystem->SetOverrideCameraRotation(false, Quat());
			pViewSystem->GetActiveView()->ResetShaking();
		}

		const SViewParams* pViewParams = pViewSystem->GetActiveView()->GetCurrentParams();
		;
		pPlayerEntity->SetPosRotScale(
		  pViewParams->position,
		  pViewParams->rotation,
		  Vec3(1, 1, 1),
		  ENTITY_XFORM_EDITOR | ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL);

		gEnv->pEntitySystem->ResumePhysicsForSuppressedEntities(false);

		bIsCameraSyncEnabled = false;
	}
}

void CLiveCreateCmd_SetCVar::Execute()
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(m_name);
	if (NULL != pCVar)
	{
		pCVar->Set(m_value);
	}
}

void CLiveCreateCmd_SetTimeOfDay::Execute()
{
	gEnv->p3DEngine->GetTimeOfDay()->SetTime(m_time, false);

	CHost* pHost = static_cast<CHost*>(gEnv->pLiveCreateHost);
	pHost->RequestTimeOfDayUpdate();
}

void CLiveCreateCmd_SetTimeOfDayValue::Execute()
{
	ITimeOfDay::SVariableInfo varInfo;
	if (gEnv->p3DEngine->GetTimeOfDay()->GetVariableInfo(m_index, varInfo))
	{
		if (m_name == varInfo.displayName)
		{
			XmlNodeRef xml = gEnv->pSystem->LoadXmlFromBuffer(m_data.c_str(), m_data.length());
			if (xml)
			{
				varInfo.pInterpolator->SerializeSpline(xml, true);

				CHost* pHost = static_cast<CHost*>(gEnv->pLiveCreateHost);
				pHost->RequestTimeOfDayUpdate();
			}
		}
	}
}

void CLiveCreateCmd_SetTimeOfDayFull::Execute()
{
	XmlNodeRef node = gEnv->pSystem->LoadXmlFromBuffer((const char*)&m_data[0], m_data.size());
	if (node)
	{
		gEnv->p3DEngine->GetTimeOfDay()->Serialize(node, true);

		CHost* pHost = static_cast<CHost*>(gEnv->pLiveCreateHost);
		pHost->RequestTimeOfDayUpdate();
	}
}

void CLiveCreateCmd_SetEnvironmentFull::Execute()
{
	XmlNodeRef node = gEnv->pSystem->LoadXmlFromBuffer((const char*)&m_data[0], m_data.size());
	if (node)
	{
		// Flush rendering thread :( another length stuff that is unfortunately needed here because it crashes without it
		gEnv->pRenderer->FlushRTCommands(true, true, true);

		// Reload environment settings
		gEnv->p3DEngine->LoadEnvironmentSettingsFromXML(node);
	}
}

void CLiveCreateCmd_SetArchetypesFull::Execute()
{
	XmlNodeRef node = gEnv->pSystem->LoadXmlFromBuffer((const char*)&m_data[0], m_data.size());
	if (node)
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		const int numChildren = node->getChildCount();
		for (int i = 0; i < numChildren; ++i)
		{
			XmlNodeRef entityNode = node->getChild(i);
			if (entityNode->isTag("EntityPrototype"))
			{
				pEntitySystem->LoadEntityArchetype(entityNode);
			}
		}
	}
}

void CLiveCreateCmd_SetParticlesFull::Execute()
{
	XmlNodeRef node = gEnv->pSystem->LoadXmlFromBuffer((const char*)&m_data[0], m_data.size());
	if (node)
	{
		XmlNodeRef oParticlesLibrary = node->findChild("ParticlesLibrary");
		if (!oParticlesLibrary)
		{
			return;
		}

		const int nNumberOfChildren = oParticlesLibrary->getChildCount();
		for (int nCurrentChild = 0; nCurrentChild < nNumberOfChildren; ++nCurrentChild)
		{
			XmlString strLibraryName;
			XmlNodeRef oLibrary = oParticlesLibrary->getChild(nCurrentChild);
			if (oLibrary->isTag("Library"))
			{
				continue;
			}
			else if (!oLibrary->getAttr("name", strLibraryName))
			{
				continue;
			}

	#if CRY_PLATFORM_WINDOWS
		#undef LoadLibrary
	#endif
			gEnv->pParticleManager->LoadLibrary(strLibraryName.c_str(), oLibrary, true);
		}
	}
}

void CLiveCreateCmd_FileSynced::Execute()
{
	string str = PathUtil::MakeGamePath(m_fileName);
	string fileExt = PathUtil::GetExt(m_fileName.c_str());

	str.MakeLower();

	if (fileExt == "mtl")
	{
		XmlNodeRef xml = gEnv->pSystem->LoadXmlFromFile(m_fileName.c_str());
		gEnv->p3DEngine->GetMaterialManager()->LoadMaterialFromXml(str.c_str(), xml);
	}
	else
	{
		gEnv->pRenderer->EF_ReloadFile(str.c_str());
	}
}

void CLiveCreateCmd_SyncSelection::Execute()
{
	CHost* pHost = static_cast<CHost*>(gEnv->pLiveCreateHost);
	pHost->SetSelectionData(m_selection);
}

void CLiveCreateCmd_SyncLayerVisibility::Execute()
{
	if (gEnv->pEntitySystem)
	{
		// Show/Hide entities
		gEnv->pEntitySystem->ToggleLayerVisibility(m_layerName.c_str(), m_bIsVisible);

		// Show/Hide object geometry
		const int layerIndex = gEnv->pEntitySystem->GetLayerId(m_layerName.c_str());
		if (-1 != layerIndex)
		{
			const bool bCheckLayerActivation = false;
			gEnv->p3DEngine->ActivateObjectsLayer((uint16)layerIndex, m_bIsVisible, true, true, true, "ALL_LAYERS", NULL, bCheckLayerActivation);
		}

		if (m_bIsVisible && gEnv->pAISystem)
		{
			gEnv->pAISystem->LayerEnabled(m_layerName.c_str(), true, false);
		}
	}
}

}

#endif
