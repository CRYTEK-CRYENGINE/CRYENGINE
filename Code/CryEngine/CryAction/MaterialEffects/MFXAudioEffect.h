// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ----------------------------------------------------------------------------------------
//  File name:   MFXAudioEffect.h
//  Description: MFX system subclass which takes care of interfacing with audio system
//
////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MFX_AUDIO_H_
#define _MFX_AUDIO_H_

#pragma once

#include "MFXEffectBase.h"
#include <CryAudio/IAudioInterfacesCommonData.h>

struct SAudioTriggerWrapper
{
	SAudioTriggerWrapper()
		: m_triggerID(CryAudio::InvalidControlId)
	{
	}

	void                Init(const char* triggerName);

	CryAudio::ControlId GetTriggerId() const
	{
		return m_triggerID;
	}

	bool IsValid() const
	{
		return (m_triggerID != CryAudio::InvalidControlId);
	}

	const char* GetTriggerName() const
	{
#if defined(MATERIAL_EFFECTS_DEBUG)
		return m_triggerName.c_str();
#else
		return "Trigger Unknown";
#endif
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
#if defined(MATERIAL_EFFECTS_DEBUG)
		pSizer->Add(m_triggerName);
#endif
	}

private:

#if defined(MATERIAL_EFFECTS_DEBUG)
	string m_triggerName;
#endif
	CryAudio::ControlId m_triggerID;
};

struct SAudioSwitchWrapper
{
	SAudioSwitchWrapper()
		: m_switchID(CryAudio::InvalidControlId)
		, m_switchStateID(CryAudio::InvalidSwitchStateId)
	{
	}

	void                Init(const char* switchName, const char* switchStateName);

	CryAudio::ControlId GetSwitchId() const
	{
		return m_switchID;
	}

	CryAudio::SwitchStateId GetSwitchStateId() const
	{
		return m_switchStateID;
	}

	bool IsValid() const
	{
		return (m_switchID != CryAudio::InvalidControlId) && (m_switchStateID != CryAudio::InvalidSwitchStateId);
	}

	const char* GetSwitchName() const
	{
#if defined(MATERIAL_EFFECTS_DEBUG)
		return m_switchName.c_str();
#else
		return "Switch Unknown";
#endif
	}

	const char* GetSwitchStateName() const
	{
#if defined(MATERIAL_EFFECTS_DEBUG)
		return m_switchStateName.c_str();
#else
		return "Switch State Unknown";
#endif
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
#if defined(MATERIAL_EFFECTS_DEBUG)
		pSizer->Add(m_switchName);
		pSizer->Add(m_switchStateName);
#endif
	}

private:
#if defined(MATERIAL_EFFECTS_DEBUG)
	string m_switchName;
	string m_switchStateName;
#endif
	CryAudio::ControlId m_switchID;
	CryAudio::SwitchStateId m_switchStateID;
};

//////////////////////////////////////////////////////////////////////////

struct SMFXAudioEffectParams
{
	typedef std::vector<SAudioSwitchWrapper> TSwitches;

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(&trigger);
		pSizer->Add(triggerSwitches);
	}

	SAudioTriggerWrapper trigger;
	TSwitches            triggerSwitches;
};

class CMFXAudioEffect :
	public CMFXEffectBase
{
public:
	CMFXAudioEffect();

	// IMFXEffect
	virtual void Execute(const SMFXRunTimeEffectParams& params) override;
	virtual void LoadParamsFromXml(const XmlNodeRef& paramsNode) override;
	virtual void GetResources(SMFXResourceList& resourceList) const override;
	virtual void GetMemoryUsage(ICrySizer* pSizer) const override;
	//~IMFXEffect

protected:

	SMFXAudioEffectParams m_audioParams;
};

#endif // _MFX_AUDIO_H_
