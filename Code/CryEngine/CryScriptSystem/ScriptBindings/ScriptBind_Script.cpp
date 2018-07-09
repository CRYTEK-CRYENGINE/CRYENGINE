// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_Script.cpp
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ScriptBind_Script.h"
#include "../ScriptTimerMgr.h"
#include "../ScriptSystem.h"
#include <CrySystem/ILog.h>
#include <CryEntitySystem/IEntitySystem.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CScriptBind_Script::CScriptBind_Script(IScriptSystem* pScriptSystem, ISystem* pSystem)
{
	CScriptableBase::Init(pScriptSystem, pSystem);
	SetGlobalName("Script");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Script::

	SCRIPT_REG_FUNC(ReloadScripts);
	SCRIPT_REG_FUNC(ReloadScript);
	SCRIPT_REG_TEMPLFUNC(ReloadEntityScript, "className");
	SCRIPT_REG_FUNC(LoadScript);
	SCRIPT_REG_FUNC(UnloadScript);
	SCRIPT_REG_FUNC(DumpLoadedScripts);
	SCRIPT_REG_TEMPLFUNC(SetTimer, "nMilliseconds,Function");
	SCRIPT_REG_TEMPLFUNC(SetTimerForFunction, "nMilliseconds,Function");
	SCRIPT_REG_TEMPLFUNC(KillTimer, "nTimerId");
}

CScriptBind_Script::~CScriptBind_Script()
{
}

/*!reload all previosly loaded scripts
 */
int CScriptBind_Script::ReloadScripts(IFunctionHandler* pH)
{
	SCRIPT_CHECK_PARAMETERS(0);
	m_pSS->ReloadScripts();
	return pH->EndFunction();
}

/*!reload a specified script. If the script wasn't loaded at least once before the function will fail
   @param sFileName path of the script that has to be reloaded
 */
int CScriptBind_Script::ReloadScript(IFunctionHandler* pH)
{
	const char* sFileName;
	if (!pH->GetParams(sFileName))
		return pH->EndFunction();

	m_pSS->ExecuteFile(sFileName, true, gEnv->IsEditor()); // Force reload if in editor mode

	return pH->EndFunction();
}

int CScriptBind_Script::ReloadEntityScript(IFunctionHandler* pH, const char* className)
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	IEntityClass* pClass = pEntitySystem->GetClassRegistry()->FindClass(className);

	if (pClass)
	{
		pClass->LoadScript(false);
	}

	return pH->EndFunction();
}

/*!load a specified script
   @param sFileName path of the script that has to be loaded
 */
int CScriptBind_Script::LoadScript(IFunctionHandler* pH)
{
	bool bReload = false;
	bool bRaiseError = true;

	if (pH->GetParamCount() >= 3)
	{
		pH->GetParam(3, bRaiseError);
	}
	if (pH->GetParamCount() >= 2)
	{
		pH->GetParam(2, bReload);
	}

	const char* sScriptFile;
	pH->GetParam(1, sScriptFile);

	if (m_pSS->ExecuteFile(sScriptFile, bRaiseError, bReload))
		return pH->EndFunction(1);
	else
		return pH->EndFunction();
}

/*!unload script from the "loaded scripts map" so if this script is loaded again
   the Script system will reloadit. this function doesn't
   involve the LUA VM so the resources allocated by the script will not be released
   unloading the script
   @param sFileName path of the script that has to be loaded
 */
int CScriptBind_Script::UnloadScript(IFunctionHandler* pH)
{
	const char* sScriptFile;
	if (!pH->GetParams(sScriptFile))
		return pH->EndFunction();

	m_pSS->UnloadScript(sScriptFile);
	return pH->EndFunction();
}

/*!Dump all loaded scripts path calling IScriptSystemSink::OnLoadedScriptDump
   @see IScriptSystemSink
 */
int CScriptBind_Script::DumpLoadedScripts(IFunctionHandler* pH)
{
	m_pSS->DumpLoadedScripts();
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::SetTimer(IFunctionHandler* pH, int nMilliseconds, HSCRIPTFUNCTION hFunc)
{
	SmartScriptTable pUserData;
	bool bUpdateDuringPause = false;
	if (pH->GetParamCount() > 2)
	{
		pH->GetParam(3, pUserData);
	}
	if (pH->GetParamCount() > 3)
	{
		pH->GetParam(4, bUpdateDuringPause);
	}
	ScriptTimer timer;
	timer.bUpdateDuringPause = bUpdateDuringPause;
	timer.sFuncName[0] = 0;
	timer.pScriptFunction = hFunc;
	timer.pUserData = pUserData;
	timer.nMillis = nMilliseconds;

	int nTimerId = ((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->AddTimer(timer);
	ScriptHandle timerHandle;
	timerHandle.n = nTimerId;

	return pH->EndFunction(timerHandle);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::SetTimerForFunction(IFunctionHandler* pH, int nMilliseconds, const char* sFunctionName)
{
	SmartScriptTable pUserData;
	bool bUpdateDuringPause = false;
	if (pH->GetParamCount() > 2)
	{
		pH->GetParam(3, pUserData);
	}
	if (pH->GetParamCount() > 3)
	{
		pH->GetParam(4, bUpdateDuringPause);
	}
	ScriptTimer timer;
	timer.bUpdateDuringPause = bUpdateDuringPause;
	if (strlen(sFunctionName) > (sizeof(timer.sFuncName) - 1))
	{
		gEnv->pLog->LogError("SetTimerForFunction: Function name too long - %s", sFunctionName);
	}
	else
	{
		cry_strcpy(timer.sFuncName, sFunctionName);
	}
	timer.pScriptFunction = 0;
	timer.pUserData = pUserData;
	timer.nMillis = nMilliseconds;

	int nTimerId = ((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->AddTimer(timer);
	ScriptHandle timerHandle;
	timerHandle.n = nTimerId;

	return pH->EndFunction(timerHandle);
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Script::KillTimer(IFunctionHandler* pH, ScriptHandle nTimerId)
{
	int nid = (int)nTimerId.n;
	((CScriptSystem*)m_pSS)->GetScriptTimerMgr()->RemoveTimer(nid);
	return pH->EndFunction();
}
