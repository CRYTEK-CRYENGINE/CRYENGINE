// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../GameAIEnv.h"
#include "RadioChatterModule.h"

#include <CryFlowGraph/IFlowBaseNode.h>

class RadioChatterFlowNode : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_Set,
		IN_Enable,
		IN_Disable,
		IN_SoundName,
		IN_MinimumSilenceDuration,
		IN_MaximumSilenceDuration,
	};

	enum EOutputs
	{
		OUT_Set,
		OUT_Enabled,
		OUT_Disabled,
	};

	RadioChatterFlowNode(SActivationInfo* pActInfo)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new RadioChatterFlowNode(pActInfo); }

	void                 GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Set",                  _HELP("Triggering this will apply your sound name and duration to the radio chatter system. (Note that this is automatically done if you trigger Enable.)")),
			InputPortConfig_Void("Enable",               _HELP("Enable Radio Chatter and apply the sound name and duration.")),
			InputPortConfig_Void("Disable",              _HELP("Disable Radio Chatter.")),
			InputPortConfig<string>("sound_Sound",       _HELP("The chatter sound to be used. (Leave empty for default.)")),
			InputPortConfig<float>("SilenceDurationMin", -1.0f,                                                                                                                                                       _HELP("The silence duration used between chatter will be randomly picked within the interval of the specific minimum and the maximum duration. (Set either one of them to -1.0 to use default properties in the system.)")),
			InputPortConfig<float>("SilenceDurationMax", -1.0f,                                                                                                                                                       _HELP("The silence duration used between chatter will be randomly picked within the interval of the specific minimum and the maximum duration. (Set either one of them to -1.0 to use default properties in the system.)")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] = {
			OutputPortConfig_AnyType("Set",      _HELP("The properties have been applied.")),
			OutputPortConfig_AnyType("Enabled",  _HELP("Radio Chatter was enabled and the properties were applied.")),
			OutputPortConfig_AnyType("Disabled", _HELP("Radio Chatter was disabled.")),
			{ 0 }
		};
		config.nFlags |= EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.sDescription = _HELP("Radio Chatter refers to the noises emitted from soldiers walkie-talkies. A sound will be emitted from a soldier near the camera. The interval is randomized between the minimum/maximum silence time.");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			RadioChatterModule* radioChatterModule = gGameAIEnv.radioChatterModule;

			if (IsPortActive(pActInfo, IN_Enable))
			{
				radioChatterModule->SetEnabled(true);
				ActivateOutput(pActInfo, OUT_Enabled, true);
			}

			if (IsPortActive(pActInfo, IN_Disable))
			{
				radioChatterModule->SetEnabled(false);
				ActivateOutput(pActInfo, OUT_Disabled, true);
			}

			if (IsPortActive(pActInfo, IN_Set) || IsPortActive(pActInfo, IN_Enable))
			{
				const float minimumSilenceDuration = GetPortFloat(pActInfo, IN_MinimumSilenceDuration);
				const float maximumSilenceDuration = GetPortFloat(pActInfo, IN_MaximumSilenceDuration);
				const string& chatterSoundName = GetPortString(pActInfo, IN_SoundName);

				if (minimumSilenceDuration < 0.0f || maximumSilenceDuration < 0.0f)
					radioChatterModule->SetDefaultSilenceDuration();
				else
					radioChatterModule->SetSilenceDuration(minimumSilenceDuration, maximumSilenceDuration);

				if (chatterSoundName.empty())
					radioChatterModule->SetDefaultSound();
				else
					radioChatterModule->SetSound(chatterSoundName);

				ActivateOutput(pActInfo, OUT_Set, true);
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("AI:RadioChatter", RadioChatterFlowNode);
