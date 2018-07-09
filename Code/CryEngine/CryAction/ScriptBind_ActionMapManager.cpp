// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$

   -------------------------------------------------------------------------
   History:
   - 8:11:2004   16:49 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ScriptBind_ActionMapManager.h"
#include "ActionMapManager.h"

//------------------------------------------------------------------------
CScriptBind_ActionMapManager::CScriptBind_ActionMapManager(ISystem* pSystem, CActionMapManager* pActionMapManager)
	: m_pSystem(pSystem),
	m_pManager(pActionMapManager)
{
	Init(gEnv->pScriptSystem, m_pSystem);
	SetGlobalName("ActionMapManager");

	RegisterGlobals();
	RegisterMethods();
}

//------------------------------------------------------------------------
CScriptBind_ActionMapManager::~CScriptBind_ActionMapManager()
{
}

//------------------------------------------------------------------------
void CScriptBind_ActionMapManager::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_ActionMapManager::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_ActionMapManager::

	SCRIPT_REG_TEMPLFUNC(EnableActionFilter, "name, enable");
	SCRIPT_REG_TEMPLFUNC(EnableActionMap, "name, enable");
	SCRIPT_REG_TEMPLFUNC(EnableActionMapManager, "enable, resetStateOnDisable");
	SCRIPT_REG_TEMPLFUNC(LoadFromXML, "name");
	SCRIPT_REG_TEMPLFUNC(InitActionMaps, "path");
	SCRIPT_REG_TEMPLFUNC(SetDefaultActionEntity, "id, updateAll");
	SCRIPT_REG_TEMPLFUNC(GetDefaultActionEntity, "");
	SCRIPT_REG_TEMPLFUNC(LoadControllerLayoutFile, "layoutName");
	SCRIPT_REG_TEMPLFUNC(IsFilterEnabled, "filterName");
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::EnableActionFilter(IFunctionHandler* pH, const char* name, bool enable)
{
	m_pManager->EnableFilter(name, enable);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::EnableActionMap(IFunctionHandler* pH, const char* name, bool enable)
{
	m_pManager->EnableActionMap(name, enable);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::EnableActionMapManager(IFunctionHandler* pH, bool enable, bool resetStateOnDisable)
{
	m_pManager->Enable(enable, resetStateOnDisable);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::LoadFromXML(IFunctionHandler* pH, const char* name)
{
	XmlNodeRef rootNode = m_pSystem->LoadXmlFromFile(name);
	m_pManager->LoadFromXML(rootNode);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::InitActionMaps(IFunctionHandler* pH, const char* path)
{
	m_pManager->InitActionMaps(path);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::SetDefaultActionEntity(IFunctionHandler* pH, EntityId id, bool updateAll)
{
	m_pManager->SetDefaultActionEntity(id, updateAll);
	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::GetDefaultActionEntity(IFunctionHandler* pH)
{
	return pH->EndFunction(m_pManager->GetDefaultActionEntity());
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::LoadControllerLayoutFile(IFunctionHandler* pH, const char* layoutName)
{
	return pH->EndFunction(m_pManager->LoadControllerLayoutFile(layoutName));
}

//------------------------------------------------------------------------
int CScriptBind_ActionMapManager::IsFilterEnabled(IFunctionHandler* pH, const char* filterName)
{
	return pH->EndFunction(m_pManager->IsFilterEnabled(filterName));
}
