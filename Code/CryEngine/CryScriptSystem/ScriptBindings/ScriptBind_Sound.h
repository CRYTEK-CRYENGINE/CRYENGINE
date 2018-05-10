// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>

/*! <remarks>These function will never be called from C-Code. They're script-exclusive.</remarks>*/
class CScriptBind_Sound final : public CScriptableBase
{
public:

	CScriptBind_Sound(IScriptSystem* pScriptSystem, ISystem* pSystem);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>Sound.GetAudioTriggerID(char const* const szName)</code>
	//! <description>Get the trigger CryAudio::ControlId (wrapped into a ScriptHandle).</description>
	//!		<param name="szName">unique name of an audio trigger</param>
	//! <returns>ScriptHandle with the CryAudio::ControlId value, or nil if the szName is not found.</returns>
	int GetAudioTriggerID(IFunctionHandler* pH, char const* const szName);

	//! <code>Sound.GetAudioSwitchID(char const* const szName)</code>
	//! <description>Get the switch CryAudio::ControlId (wrapped into a ScriptHandle).</description>
	//!		<param name="szName">unique name of an audio switch</param>
	//! <returns>ScriptHandle with the CryAudio::ControlId value, or nil if the szName is not found.</returns>
	int GetAudioSwitchID(IFunctionHandler* pH, char const* const szName);

	//! <code>Sound.GetAudioSwitchStateID(ScriptHandle const hSwitchID, char const* const szName)</code>
	//! <description>Get the SwitchState TAudioSwitchStatelID (wrapped into a ScriptHandle).</description>
	//!		<param name="hSwitchID">the switch ID handle</param>
	//!		<param name="szName">unique name of an audio switch state</param>
	//! <returns>ScriptHandle with the TAudioSwitchStateID value, or nil if the szName is not found.</returns>
	int GetAudioSwitchStateID(IFunctionHandler* pH, ScriptHandle const hSwitchID, char const* const szName);

	//! <code>Sound.GetAudioRtpcID(char const* const szName)</code>
	//! <description>Get the RTPC CryAudio::ControlId (wrapped into a ScriptHandle).</description>
	//!		<param name="szName">unique name of an audio RTPC</param>
	//! <returns>ScriptHandle with the CryAudio::ControlId value, or nil if the szName is not found.</returns>
	int GetAudioRtpcID(IFunctionHandler* pH, char const* const szName);

	//! <code>Sound.GetAudioEnvironmentID(char const* const szName)</code>
	//! <description>Get the Audio Environment TAudioEnvironmentID (wrapped into a ScriptHandle).</description>
	//!		<param name="szName">unique name of an Audio Environment</param>
	//! <returns>ScriptHandle with the TAudioEnvironmentID value, or nil if the szName is not found.</returns>
	int GetAudioEnvironmentID(IFunctionHandler* pH, char const* const szName);

	//! <code>Sound.SetAudioRtpcValue( hRtpcID, fValue )</code>
	//! <description>Globally sets the specified audio RTPC to the specified value</description>
	//!		<param name="hParameterId">the parameter ID handle</param>
	//!		<param name="value">the parameter value</param>
	//! <returns>nil</returns>
	int SetAudioRtpcValue(IFunctionHandler* pH, ScriptHandle const hParameterId, float const value);

	//! <code>Sound.GetAudioTriggerRadius( hTriggerID )</code>
	//! <description>Get the activity radius for the passed in trigger</description>
	//!		<param name="hTriggerID">the audio trigger ID handle</param>
	//! <returns>ScriptHandle with the radius value (float), or nil if the hTriggerID is not found</returns>
	int GetAudioTriggerRadius(IFunctionHandler* pH, ScriptHandle const hTriggerID);
};
