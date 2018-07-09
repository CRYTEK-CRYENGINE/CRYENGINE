// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Script Binding for the Vehicle System

   -------------------------------------------------------------------------
   History:
   - 26:04:2005   : Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include <CryAnimation/ICryAnimation.h>
#include "ActorSystem.h"
#include <CryGame/IGameFramework.h>
#include "VehicleSystem.h"
#include "ScriptBind_VehicleSystem.h"
#include "VehicleSystem/VehicleViewThirdPerson.h"

//------------------------------------------------------------------------
CScriptBind_VehicleSystem::CScriptBind_VehicleSystem(ISystem* pSystem, CVehicleSystem* vehicleSystem)
{
	m_pVehicleSystem = vehicleSystem;

	Init(gEnv->pScriptSystem, pSystem);
	SetGlobalName("Vehicle");

	RegisterGlobals();
	RegisterMethods();

	gEnv->pScriptSystem->ExecuteFile("Scripts/Entities/Vehicles/VehicleSystem.lua");
}

//------------------------------------------------------------------------
CScriptBind_VehicleSystem::~CScriptBind_VehicleSystem()
{
}

//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::RegisterGlobals()
{
}

//------------------------------------------------------------------------
void CScriptBind_VehicleSystem::RegisterMethods()
{
#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_VehicleSystem::

	SCRIPT_REG_FUNC(GetVehicleImplementations);
	SCRIPT_REG_TEMPLFUNC(GetOptionalScript, "vehicleName");
	SCRIPT_REG_TEMPLFUNC(ReloadSystem, "");
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::GetVehicleImplementations(IFunctionHandler* pH)
{
	SmartScriptTable tableImpls(m_pSS->CreateTable());
	SVehicleImpls impls;

	m_pVehicleSystem->GetVehicleImplementations(impls);

	//CryLog("Scriptbind got %i vehicles", impls.Count());
	for (int i = 0; i < impls.Count(); ++i)
	{
		tableImpls->SetAt(i + 1, impls.GetAt(i).c_str());
	}
	return pH->EndFunction(tableImpls);
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::GetOptionalScript(IFunctionHandler* pH, char* vehicleName)
{
	char scriptName[1024] = { 0 };

	if (m_pVehicleSystem->GetOptionalScript(vehicleName, scriptName, sizeof(scriptName)))
		return pH->EndFunction(scriptName);

	return pH->EndFunction();
}

//------------------------------------------------------------------------
int CScriptBind_VehicleSystem::ReloadSystem(IFunctionHandler* pH)
{
	m_pVehicleSystem->ReloadSystem();

	return pH->EndFunction();
}
