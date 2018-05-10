// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowInputNode.cpp
//  Version:     v1.00
//  Created:     13/10/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//  24.09.2012 - Dario Sancho
//               * Added new node ForceFeedbackTriggerTweaker (for Durango)
//
//  28.08.2015 - Marco Hopp
//               * Deprecated Input:ActionMapManager, Input:ActionListener, Input:ActionFilter
//               * new versions of those nodes can be found in FlowActionMapNodes.cpp
//               * TODO: Remove this code at next major release
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "FlowFrameworkBaseNode.h"
#include <CryInput/IInput.h>

#include <CryString/StringUtils.h>

#include "FlowSystem/FlowSystemCVars.h"

class CFlowNode_InputKey : public CFlowBaseNode<eNCT_Instanced>, public IInputEventListener
{
	enum INPUTS
	{
		EIP_ENABLE = 0,
		EIP_DISABLE,
		EIP_KEY,
		EIP_NONDEVMODE,
	};

	enum OUTPUTS
	{
		EOP_PRESSED,
		EOP_RELEASED,
	};

public:
	CFlowNode_InputKey(SActivationInfo* pActInfo) : m_bActive(false), m_bAllowedInNonDevMode(false)
	{
		m_eventKeyName.resize(64); // avoid delete/re-newing during execution - assuming no action keys are longer than 64 char
	}

	~CFlowNode_InputKey()
	{
		Register(false, 0);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",      _HELP("Trigger to enable. Default: Already enabled.")),
			InputPortConfig_Void("Disable",     _HELP("Trigger to disable")),
			InputPortConfig<string>("Key",      _HELP("Key name. Leave empty for any.")),
			InputPortConfig<bool>("NonDevMode", false,                                                 _HELP("If set to true, can be used in Non-Devmode as well [Debugging backdoor]")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			OutputPortConfig<string>("Pressed",  _HELP("Key pressed.")),
			OutputPortConfig<string>("Released", _HELP("Key released.")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to catch key inputs. Use only for debugging. It is enabled by default. Entity Input need to be used in multiplayer.");
		config.SetCategory(EFLN_DEBUG);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_InputKey(pActInfo);
	}

	void Register(bool bRegister, SActivationInfo* pActInfo)
	{
		IInput* pInput = gEnv->pInput;
		if (pInput)
		{
			if (bRegister)
			{
				assert(pActInfo != 0);
				m_bAllowedInNonDevMode = GetPortBool(pActInfo, EIP_NONDEVMODE);
				if (gEnv->pSystem->IsDevMode() || m_bAllowedInNonDevMode)
					pInput->AddEventListener(this);
			}
			else
				pInput->RemoveEventListener(this);
		}
		m_bActive = bRegister;
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bActive", m_bActive);
		if (ser.IsReading())
		{
			Register(m_bActive, pActInfo);
		}
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (InputEntityIsLocalPlayer(pActInfo))
		{
			switch (event)
			{
			case eFE_Initialize:
				m_actInfo = *pActInfo;
				Register(true, pActInfo);
				break;
			case eFE_Activate:
				m_actInfo = *pActInfo;
				if (IsPortActive(pActInfo, EIP_DISABLE))
					Register(false, pActInfo);
				if (IsPortActive(pActInfo, EIP_ENABLE))
					Register(true, pActInfo);
				break;
			}
		}
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		// Kevin - Ignore if console is up
		if (gEnv->pConsole->IsOpened())
			return false;

		m_eventKeyName = event.keyName.c_str();

		const string& keyName = GetPortString(&m_actInfo, EIP_KEY);

		if (keyName.empty() == false)
		{
			if (stricmp(keyName.c_str(), m_eventKeyName.c_str()) != 0)
				return false;
		}

		if (gEnv->pSystem->IsDevMode() && !gEnv->IsEditor() && CFlowSystemCVars::Get().g_disableInputKeyFlowNodeInDevMode != 0 && !m_bAllowedInNonDevMode)
			return false;

		const bool nullInputValue = (fabs_tpl(event.value) < 0.02f);
		if ((event.state == eIS_Pressed) || ((event.state == eIS_Changed) && !nullInputValue))
			ActivateOutput(&m_actInfo, EOP_PRESSED, m_eventKeyName);
		else if ((event.state == eIS_Released) || ((event.state == eIS_Changed) && !nullInputValue))
			ActivateOutput(&m_actInfo, EOP_RELEASED, m_eventKeyName);

		// return false, so other listeners get notification as well
		return false;
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	SActivationInfo m_actInfo;
	bool            m_bActive;
	bool            m_bAllowedInNonDevMode;
	string          m_eventKeyName; // string is necessary for ActivateOutput, and it allocates in heap - so we use a member var for it
};

//////////////////////////////////////////////////////////////////////////

class CFlowNode_ForceFeedback : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_Controller = 0,
		eIP_EffectName,
		eIP_Play,
		eIP_Intensity,
		eIP_Delay,
		eIP_Stop,
		eIP_StopAll
	};

public:
	CFlowNode_ForceFeedback(SActivationInfo* pActInfo){}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<int>("Controller", _HELP("Number of the controller that should rumble" ), _HELP("Controller Number"), _UICONFIG("enum_int:1=0,2=1,3=2,4=3")),
			InputPortConfig<string>("EffectName", _HELP("Name of the force feedback effect to be played/stopped" ), _HELP("Effect Name"), _UICONFIG("enum_global:forceFeedback")),
			InputPortConfig_Void("Play", _HELP("Play the specified effect")),
			InputPortConfig<float>("Intensity", 1.f, _HELP("Intensity factor applied to the effect being played")),
			InputPortConfig<float>("Delay", 0.f, _HELP("Time delay to start the effect")),
			InputPortConfig_Void("Stop", _HELP("Stop the specified effect")),
			InputPortConfig_Void("StopAll", _HELP("Stop all currently playing force feedback effects")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("FlowNode to play/stop force feedback effects");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (InputEntityIsLocalPlayer(pActInfo))
				{
					IForceFeedbackSystem* pForceFeedback = gEnv->pGameFramework->GetIForceFeedbackSystem();

					if (IsPortActive(pActInfo, eIP_Play))
					{
						gEnv->pInput->ForceFeedbackSetDeviceIndex(GetPortInt(pActInfo,eIP_Controller));
						const string effectName = GetPortString(pActInfo, eIP_EffectName).c_str();
						ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName(effectName);
						const float intensity = GetPortFloat(pActInfo, eIP_Intensity);
						const float delay = GetPortFloat(pActInfo, eIP_Delay);
						pForceFeedback->PlayForceFeedbackEffect(fxId, SForceFeedbackRuntimeParams(intensity, delay));
					}
					if (IsPortActive(pActInfo, eIP_Stop))
					{
						const string effectName = GetPortString(pActInfo, eIP_EffectName);
						ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName(effectName);
						pForceFeedback->StopForceFeedbackEffect(fxId);
					}
					if (IsPortActive(pActInfo, eIP_StopAll))
					{
						pForceFeedback->StopAllEffects();
					}
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_ForceFeedbackTweaker : public CFlowBaseNode<eNCT_Singleton>
{
	enum INPUTS
	{
		eIP_LowPassMultiplier,
		eIP_HighPassMultiplier,
		eIP_Start,
		eIP_Stop,
	};

public:
	CFlowNode_ForceFeedbackTweaker(SActivationInfo* pActInfo){}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig<float>("LowPass",     1.f,                                             _HELP("Multiplier applied to the low frequencey effect being played")),
			InputPortConfig<float>("HighPass",    1.f,                                             _HELP("Multiplier applied to the high frequencey effect being played")),
			InputPortConfig_AnyType("Activate",   _HELP("activate the forcefeedback tweaker")),
			InputPortConfig_AnyType("Deactivate", _HELP("deactivate the force feedback tweaker")),
			{ 0 }
		};

		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.sDescription = _HELP("FlowNode to control individual high and low frequency force feedback effect");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_Start))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
				if (IsPortActive(pActInfo, eIP_Stop))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
				break;
			}

		case eFE_Update:
			{
				IForceFeedbackSystem* pForceFeedback = gEnv->pGameFramework->GetIForceFeedbackSystem();
				if (pForceFeedback)
				{
					pForceFeedback->AddFrameCustomForceFeedback(GetPortFloat(pActInfo, eIP_HighPassMultiplier), GetPortFloat(pActInfo, eIP_LowPassMultiplier));
				}
				break;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("Debug:InputKey", CFlowNode_InputKey);
REGISTER_FLOW_NODE("Game:ForceFeedback", CFlowNode_ForceFeedback);
REGISTER_FLOW_NODE("Game:ForceFeedbackTweaker", CFlowNode_ForceFeedbackTweaker);
