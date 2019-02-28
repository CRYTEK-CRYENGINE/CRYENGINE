// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	CVars for the FlowSystem
   -------------------------------------------------------------------------
   History:
   - 02:03:2006  12:00 : Created by AlexL

*************************************************************************/

#include "StdAfx.h"
#include "FlowSystemCVars.h"
#include "Inspectors/FlowFilters.h"
#include <CryFlowGraph/IFlowSystem.h>

#include <CryAISystem/IAgent.h>

CFlowSystemCVars* CFlowSystemCVars::s_pThis = 0;

namespace
{
IFlowGraphInspector::IFilterPtr pTheFilter;

void SetCurrentFilter(IFlowGraphInspector::IFilterPtr pFilter)
{
	static IFlowSystem* pFlowSystem = gEnv->pFlowSystem;
	IFlowGraphInspectorPtr pInspector = pFlowSystem->GetDefaultInspector();
	if (pTheFilter)
		pInspector->RemoveFilter(pTheFilter);
	pTheFilter = pFilter;
	if (pTheFilter)
	{
		pInspector->AddFilter(pTheFilter);
		pFlowSystem->EnableInspecting(true);
	}
}

const char* MakeEmptyIfAny(const char* param)
{
	if (param && stricmp(param, "_ANY") == 0)
		return "";
	return param;
}

EntityId GetEntityIdByName(const char* entityName)
{
	if (entityName == 0 || entityName[0] == '\0')
		return 0;
	static IEntitySystem* pEntitySys = gEnv->pEntitySystem;
	IEntity* pEntity = pEntitySys->FindEntityByName(entityName);
	if (pEntity != 0)
		return pEntity->GetId();

	GameWarning("Entity '%s' not found", entityName);
	return 0;
}

// called when ConsoleVariable fg_Inspector changes
void Inspector(IConsoleCmdArgs* args)
{
	const int argCount = args->GetArgCount();
	if (argCount < 2 || argCount > 6)
	{
		GameWarning("Usage: %s [0/1]", args->GetArg(0));
		return;
	}
	int mode = atoi(args->GetArg(1));
	gEnv->pFlowSystem->EnableInspecting(mode == 0 ? false : true);
	SetCurrentFilter(0);    // clear all filters
}

void InspectAction(IConsoleCmdArgs* args)
{
	const int argCount = args->GetArgCount();
	if (argCount < 2 || argCount > 6)
	{
		GameWarning("Usage: %s [ActionName] [UserClass] [ObjectClass] [UserEntityName] [ObjectEntityName]", args->GetArg(0));
		return;
	}

	// at least one arg
	const char* actionName = args->GetArg(1);
	bool isAnyAction = stricmp(actionName, "_ANY") == 0;

	if (!isAnyAction && gEnv->pAISystem && gEnv->pAISystem->GetAIActionManager()->GetAIAction(actionName) == 0)
	{
		GameWarning("AIAction '%s' not found", actionName);
		return;
	}

	const char* userClass = 0;
	const char* objectClass = 0;
	const char* userEntity = 0;
	const char* objectEntity = 0;
	if (argCount > 2)
		userClass = args->GetArg(2);
	if (argCount > 3)
		objectClass = args->GetArg(3);
	if (argCount > 4)
		userEntity = args->GetArg(4);
	if (argCount > 5)
		objectEntity = args->GetArg(5);

	actionName = MakeEmptyIfAny(actionName);
	userClass = MakeEmptyIfAny(userClass);
	objectClass = MakeEmptyIfAny(objectClass);
	userEntity = MakeEmptyIfAny(userEntity);
	objectEntity = MakeEmptyIfAny(objectEntity);

	EntityId userId = GetEntityIdByName(userEntity);
	EntityId objectId = GetEntityIdByName(objectEntity);

	CFlowFilterAIAction* pFilter = new CFlowFilterAIAction();
	pFilter->SetAction(actionName);
	pFilter->SetParams(userClass, objectClass, userId, objectId);
	SetCurrentFilter(pFilter);
}

void InspectEntity(IConsoleCmdArgs* args)
{
	const int argCount = args->GetArgCount();
	if (argCount < 2 || argCount > 2)
	{
		GameWarning("Usage: %s [EntityName]", args->GetArg(0));
		return;
	}

	// at least one arg
	const char* entityName = args->GetArg(1);
	if (entityName[0] == '\0')
	{
		SetCurrentFilter(0);
		return;
	}
	EntityId entityId = 0;
	if (entityName[0] == '#' && entityName[1] == '#')
		entityId = atoi(entityName + 2);
	else
		entityId = GetEntityIdByName(entityName);
	if (entityId == 0)
		return;

	CFlowFilterEntity* pFilter = new CFlowFilterEntity();
	pFilter->SetEntity(entityId);
	SetCurrentFilter(pFilter);
}
};

CFlowSystemCVars::CFlowSystemCVars()
{
	assert(s_pThis == 0);
	s_pThis = this;

	IConsole* pConsole = gEnv->pConsole;

	REGISTER_COMMAND("fg_Inspector", Inspector, 0,
	                 "Toggles FlowGraph System DefaultInspector.\n"
	                 "Usage: fg_Inspector [0/1]\n"
	                 "Default is 0 (off).");
	REGISTER_COMMAND("fg_InspectEntity", InspectEntity, 0, "Inspects the specified Entity graph");
	REGISTER_COMMAND("fg_InspectAction", InspectAction, 0, "Inspects the specified AIAction graph");
	REGISTER_CVAR2("fg_SystemEnable", &this->m_enableUpdates, 1, VF_CHEAT, "Toggles FlowGraph System Updates.\nUsage: fg_SystemEnable [0/1]\nDefault is 1 (on).");
	REGISTER_CVAR2("fg_profile", &this->m_profile, 0, 0, "Flow graph profiling enable/disable");
	REGISTER_CVAR2("fg_inspectorLog", &this->m_inspectorLog, 0, 0, "Log inspector on console.");

#ifdef _RELEASE
	REGISTER_CVAR2("fg_abortOnLoadError", &this->m_abortOnLoadError, 0, VF_NULL, "Abort on load error of flowgraphs\n2:abort, 1:dialog, 0:log only");
	REGISTER_CVAR2("fg_noDebugText", &this->m_noDebugText, 1, VF_NULL, "Don't show debug text [0/1]\nDefault is 0 (show debug text).");
#else
	REGISTER_CVAR2("fg_abortOnLoadError", &this->m_abortOnLoadError, 1, VF_NULL, "Abort on load error of flowgraphs\n2:abort, 1:dialog, 0:log only");
	REGISTER_CVAR2("fg_noDebugText", &this->m_noDebugText, 0, VF_NULL, "Don't show debug text [0/1]\nDefault is 0 (show debug text).");
#endif

	REGISTER_CVAR2("fg_iEnableFlowgraphNodeDebugging", &this->m_enableFlowgraphNodeDebugging, 0, VF_DUMPTODISK, "Toggles visual flowgraph debugging.");
	REGISTER_CVAR2("fg_iDebugNextStep", &this->m_debugNextStep, 0, VF_DUMPTODISK, "Update flowgraph debug by one step.");

	// Disable HUD debug text
	REGISTER_CVAR2("cl_DisableHUDText", &fg_DisableHUDText, 0, 0, "Force disable all output from HUD Debug text nodes");

	REGISTER_CVAR(g_disableInputKeyFlowNodeInDevMode, 0, VF_NULL, "disables input Key flownodes even in dev mode. Pure game only, does not affect editor.");

	REGISTER_CVAR(g_disableSequencePlayback, 0, VF_NULL, "disable movie sequence playback");
}

CFlowSystemCVars::~CFlowSystemCVars()
{
	assert(s_pThis != 0);
	s_pThis = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->RemoveCommand("fg_Inspector");
	pConsole->RemoveCommand("fg_InspectEntity");
	pConsole->RemoveCommand("fg_InspectAction");

	pConsole->UnregisterVariable("fg_SystemEnable", true);
	pConsole->UnregisterVariable("fg_profile", true);
	pConsole->UnregisterVariable("fg_abortOnLoadError", true);
	pConsole->UnregisterVariable("fg_inspectorLog", true);
	pConsole->UnregisterVariable("fg_noDebugText", true);

	pConsole->UnregisterVariable("fg_iEnableFlowgraphNodeDebugging", true);
	pConsole->UnregisterVariable("fg_iDebugNextStep", true);
	pConsole->UnregisterVariable("fg_DisableHUDText", true);
	pConsole->UnregisterVariable("g_disableInputKeyFlowNodeInDevMode", true);
	pConsole->UnregisterVariable("g_disableSequencePlayback", true);

	pConsole->UnregisterVariable("cl_DisableHUDText", true);
}
