// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 14:02:2006   11:29 : Created by AlexL
	- 04:04:2006	 17:30 : Extended by Jan Müller

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_HUD.h"
#include "Game.h"
#include "GameRules.h"

#include "UI/UIManager.h"
#include "HUDEventDispatcher.h"
#include "HUDMissionObjectiveSystem.h"
#include "HUDEventWrapper.h"
#include "HUDSilhouettes.h"
#include "HUDUtils.h"

//------------------------------------------------------------------------
CScriptBind_HUD::CScriptBind_HUD(ISystem *pSystem, IGameFramework *pGameFramework)
: m_pSystem(pSystem),
	m_pGameFW(pGameFramework)
{
	Init(pSystem->GetIScriptSystem(), m_pSystem);
	SetGlobalName("HUD");

	RegisterMethods();
	RegisterGlobals();
}

//------------------------------------------------------------------------
CScriptBind_HUD::~CScriptBind_HUD()
{
}

//------------------------------------------------------------------------
void CScriptBind_HUD::RegisterGlobals()
{
	m_pSS->SetGlobalValue("MO_DEACTIVATED", CHUDMissionObjective::DEACTIVATED);
	m_pSS->SetGlobalValue("MO_COMPLETED", CHUDMissionObjective::COMPLETED);
	m_pSS->SetGlobalValue("MO_FAILED", CHUDMissionObjective::FAILED);
	m_pSS->SetGlobalValue("MO_ACTIVATED", CHUDMissionObjective::ACTIVATED);

	m_pSS->SetGlobalValue("eBLE_Information", 2);
	m_pSS->SetGlobalValue("eBLE_Currency", 1);
	m_pSS->SetGlobalValue("eBLE_Warning", 3);
	m_pSS->SetGlobalValue("eBLE_System", 0);

}

//------------------------------------------------------------------------
void CScriptBind_HUD::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_HUD::

	SCRIPT_REG_TEMPLFUNC(SetObjectiveStatus,"objective,status,silent");
	SCRIPT_REG_TEMPLFUNC(SetObjectiveEntity,"objective,entityName");
	SCRIPT_REG_TEMPLFUNC(ClearObjectiveEntity,"objective");
	SCRIPT_REG_TEMPLFUNC(AddEntityToRadar, "entityId");
	SCRIPT_REG_TEMPLFUNC(RemoveEntityFromRadar, "entityId");
	SCRIPT_REG_TEMPLFUNC(OnGameStatusUpdate, "goodBad, msg");
	SCRIPT_REG_TEMPLFUNC(RemoveObjective, "entityId");


#undef SCRIPT_REG_CLASSNAME
}

//------------------------------------------------------------------------
int CScriptBind_HUD::SetObjectiveStatus(IFunctionHandler *pH,const char* pObjectiveID, int status, bool silent)
{
	if(CHUDMissionObjectiveSystem* pMOSystem = g_pGame->GetMOSystem())
	{
		CHUDMissionObjective*	pObj = pMOSystem->GetMissionObjective(pObjectiveID);
		if (pObj)
		{
			pObj->SetSilent(silent);
			pObj->SetStatus((CHUDMissionObjective::HUDMissionStatus)status);
		}
		else if(status!=CHUDMissionObjective::FIRST)
		{
			GameWarning("CScriptBind_HUD::Tried to access non existing MissionObjective '%s'", pObjectiveID);
		}
	}
	return pH->EndFunction();  
}

//------------------------------------------------------------------------
int CScriptBind_HUD::SetObjectiveEntity(IFunctionHandler *pH,const char* pObjectiveID,const char* entityName)
{
	if(CHUDMissionObjectiveSystem* pMOSystem = g_pGame->GetMOSystem())
	{
		CHUDMissionObjective*	pObj = pMOSystem->GetMissionObjective(pObjectiveID);
		if (pObj)
			pObj->SetTrackedEntity(entityName);
		else
			GameWarning("CScriptBind_HUD::SetObjectiveEntity Tried to access non existing MissionObjective '%s'", pObjectiveID);
	}
	return pH->EndFunction();  
}

//------------------------------------------------------------------------
int CScriptBind_HUD::ClearObjectiveEntity(IFunctionHandler *pH,const char* pObjectiveID)
{
	if(CHUDMissionObjectiveSystem* pMOSystem = g_pGame->GetMOSystem())
	{
		CHUDMissionObjective*	pObj = pMOSystem->GetMissionObjective(pObjectiveID);
		if (pObj)
			pObj->ClearTrackedEntity();
		else
			GameWarning("CScriptBind_HUD::ClearObjectiveEntity Tried to access non existing MissionObjective '%s'", pObjectiveID);
	}
	return pH->EndFunction();  
}

//------------------------------------------------------------------------
int CScriptBind_HUD::AddEntityToRadar(IFunctionHandler *pH, ScriptHandle entityId)
{
	SHUDEvent hudevent(eHUDEvent_AddEntity);
	hudevent.AddData(SHUDEventData((int)entityId.n));
	CHUDEventDispatcher::CallEvent(hudevent);

	return pH->EndFunction();
}

int CScriptBind_HUD::RemoveEntityFromRadar(IFunctionHandler *pH, ScriptHandle entityId)
{
	SHUDEvent hudevent(eHUDEvent_RemoveEntity);
	hudevent.AddData(SHUDEventData((int)entityId.n));
	CHUDEventDispatcher::CallEvent(hudevent);
	return pH->EndFunction();
}

int CScriptBind_HUD::AddEntitySilhouette(IFunctionHandler *pH, ScriptHandle entityId, float r, float g, float b, float a)
{
	g_pGame->GetUI()->GetSilhouettes()->SetSilhouette(gEnv->pEntitySystem->GetEntity((EntityId)entityId.n), r, g, b, a, -1);
	return pH->EndFunction();
}

int CScriptBind_HUD::OnGameStatusUpdate(IFunctionHandler *pH, int goodBad, const char *msg)
{
	const char *localisedMessage = CHUDUtils::LocalizeString(msg);
	assert(goodBad >= eGBNFLP_Neutral);
	assert(goodBad < eGBNFLP_Bad);
	SHUDEventWrapper::OnGameStatusUpdate((EGoodBadNeutralForLocalPlayer)goodBad, localisedMessage);
	return pH->EndFunction();
}

int CScriptBind_HUD::RemoveObjective(IFunctionHandler *pH, ScriptHandle entityId)
{
	SHUDEventWrapper::OnRemoveObjective((EntityId)entityId.n);
	return pH->EndFunction();
}

