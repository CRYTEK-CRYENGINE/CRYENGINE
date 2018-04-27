// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScriptBind_Sound.h"
#include <CryAudio/IAudioSystem.h>

CScriptBind_Sound::CScriptBind_Sound(IScriptSystem* pScriptSystem, ISystem* pSystem)
{
	CScriptableBase::Init(pScriptSystem, pSystem);
	SetGlobalName("Sound");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &CScriptBind_Sound::

	// AudioSystem
	SCRIPT_REG_TEMPLFUNC(GetAudioTriggerID, "sTriggerName");
	SCRIPT_REG_TEMPLFUNC(GetAudioSwitchID, "sSwitchName");
	SCRIPT_REG_TEMPLFUNC(GetAudioSwitchStateID, "hSwitchID, sStateName");
	SCRIPT_REG_TEMPLFUNC(GetAudioRtpcID, "sRtpcName");
	SCRIPT_REG_TEMPLFUNC(GetAudioEnvironmentID, "sEnvironmentName");
	SCRIPT_REG_TEMPLFUNC(SetAudioRtpcValue, "hRtpcID, fValue");
	SCRIPT_REG_TEMPLFUNC(GetAudioTriggerRadius, "triggerId");
}

///////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioTriggerID(IFunctionHandler* pH, char const* const szName)
{
	if ((szName != nullptr) && (szName[0] != '\0'))
	{
		CryAudio::ControlId const triggerId = CryAudio::StringToId(szName);
		return pH->EndFunction(IntToHandle(triggerId));
	}

	return pH->EndFunction();
}

///////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioSwitchID(IFunctionHandler* pH, char const* const szName)
{
	if ((szName != nullptr) && (szName[0] != '\0'))
	{
		CryAudio::ControlId const switchId = CryAudio::StringToId(szName);
		return pH->EndFunction(IntToHandle(switchId));
	}

	return pH->EndFunction();
}

///////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioSwitchStateID(IFunctionHandler* pH, ScriptHandle const hSwitchID, char const* const szName)
{
	if ((szName != nullptr) && (szName[0] != '\0'))
	{
		CryAudio::SwitchStateId const switchStateId = CryAudio::StringToId(szName);
		return pH->EndFunction(IntToHandle(switchStateId));
	}

	return pH->EndFunction();
}

///////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioRtpcID(IFunctionHandler* pH, char const* const szName)
{
	if ((szName != nullptr) && (szName[0] != '\0'))
	{
		CryAudio::ControlId const parameterId = CryAudio::StringToId(szName);
		return pH->EndFunction(IntToHandle(parameterId));
	}

	return pH->EndFunction();
}

///////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioEnvironmentID(IFunctionHandler* pH, char const* const szName)
{
	if ((szName != nullptr) && (szName[0] != '\0'))
	{
		CryAudio::EnvironmentId const environmentId = CryAudio::StringToId(szName);
		return pH->EndFunction(IntToHandle(environmentId));
	}

	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::SetAudioRtpcValue(IFunctionHandler* pH, ScriptHandle const hParameterId, float const value)
{
	gEnv->pAudioSystem->SetParameter(HandleToInt<CryAudio::ControlId>(hParameterId), value);
	return pH->EndFunction();
}

//////////////////////////////////////////////////////////////////////////
int CScriptBind_Sound::GetAudioTriggerRadius(IFunctionHandler* pH, ScriptHandle const hTriggerID)
{
	CryAudio::STriggerData data;
	gEnv->pAudioSystem->GetTriggerData(HandleToInt<CryAudio::ControlId>(hTriggerID), data);
	return pH->EndFunction(data.radius);
}
