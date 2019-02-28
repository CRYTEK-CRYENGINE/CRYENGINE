// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Kinect (Xbox One) Flowgraph nodes. These nodes can be used
              directly or as an example to build more complex Kinect
              functionality (e.g. add UI-prompts to speech recognition)
   -------------------------------------------------------------------------
   History:

*************************************************************************/

#include "StdAfx.h"

#include <CryFlowGraph/IFlowBaseNode.h>

// ------------------------------------------------------------------------
class CSpeechRecognitionIsEnabledNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CSpeechRecognitionIsEnabledNode(SActivationInfo* pActInfo)
	{
	}

	~CSpeechRecognitionIsEnabledNode()
	{
	}
	//////////////////////////////////////////////////////////////////////////

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eI_IsEnabled = 0,
		eI_IsGrammarInitialised,
	};

	enum EOutputs
	{
		eO_True = 0,
		eO_False,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("IsEnabled",            _HELP("Is Kinect Speech Recognition Enabled?")),
			InputPortConfig_AnyType("IsGrammarInitialised", _HELP("Are Kinect Grammar(s) properly loaded and initialised?")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("True",  _HELP("True")),
			OutputPortConfig_Void("False", _HELP("False")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Queries the availability of Kinect Speech Recognition and its grammars (Xbox One)");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
#if CRY_PLATFORM_DURANGO
			IKinectInput* pKinect = gEnv->pSystem->GetIInput()->GetKinectInput();
			if (IsPortActive(pActInfo, eI_IsEnabled))
			{
				const bool ok = pKinect && pKinect->IsEnabled();
				ActivateOutput(pActInfo, ok ? eO_True : eO_False, true);
			}
			else if (IsPortActive(pActInfo, eI_IsGrammarInitialised))
			{
				const bool ok = pKinect && pKinect->IsGrammarInitialized();
				ActivateOutput(pActInfo, ok ? eO_True : eO_False, true);
			}
#else
			ActivateOutput(pActInfo, eO_False, true);
#endif
		}
	}
};

// ------------------------------------------------------------------------
class CSpeechRecognitionSetEnableNode : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CSpeechRecognitionSetEnableNode(SActivationInfo* pActInfo)
	{
	}

	~CSpeechRecognitionSetEnableNode()
	{
	}
	//////////////////////////////////////////////////////////////////////////

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eI_Enable = 0,
		eI_Disable,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("Enable",  _HELP("Enables Kinect Speech Recognition (Xbox One)")),
			InputPortConfig_AnyType("Disable", _HELP("Disables Kinect Speech Recognition (Xbox One)")),
			{ 0 }
		};
		config.pInputPorts = inputs;
		config.sDescription = _HELP("Enables/Disables Kinect Speech Recognition (Xbox One)");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, eI_Enable))
			{
				if (gEnv->pSystem->GetIInput()->GetKinectInput())
					gEnv->pSystem->GetIInput()->GetKinectInput()->SpeechEnable();
			}
			if (IsPortActive(pActInfo, eI_Disable))
			{
				if (gEnv->pSystem->GetIInput()->GetKinectInput())
					gEnv->pSystem->GetIInput()->GetKinectInput()->SpeechDisable();
			}
		}
	}
};

// ------------------------------------------------------------------------
class CSpeechRecognitionListenerNode : public CFlowBaseNode<eNCT_Instanced>, public IKinectInputAudioListener
{
public:
	CSpeechRecognitionListenerNode(SActivationInfo* pActInfo)
		: m_active(false)
	{
	}

	~CSpeechRecognitionListenerNode()
	{
#if CRY_PLATFORM_DURANGO
		if (gEnv->pSystem->GetIInput()->GetKinectInput())
		{
			gEnv->pSystem->GetIInput()->GetKinectInput()->UnregisterKinectAudioListener(this);
		}
#endif
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CSpeechRecognitionListenerNode(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
	}
	//////////////////////////////////////////////////////////////////////////

	virtual void OnVoiceCommand(const char* voiceCommand)
	{
		if (m_active)
		{
			const string& command = GetPortString(&m_actInfo, eI_Command);
			if (command == voiceCommand)
			{
				// Note: the voieCommand will be the TAG field of the command input in the GRXML file
				// This is so to make sure that the recognition is independent from the language
				ActivateOutput(&m_actInfo, eO_Active, true);
			}
		}
	}

	virtual void OnGrammarChange(const char* grammarName, eGrammaChange change) {};

	void         GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eI_Enable = 0,
		eI_Disable,
		eI_Command,
	};

	enum EOutputs
	{
		eO_Active
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("Enable",    _HELP("Enables the node")),
			InputPortConfig_AnyType("Disable",   _HELP("Disables the node")),
			InputPortConfig<string>("CommandID", _HELP("Voice command ID to listen for (Xbox One) - This is the TAG filed of the item in the GRXML file")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Active", _HELP("The selected voice command has been triggered (Xbox One)")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Listener for Kinect Voice Commands (Xbox One)");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_active = false;
				m_actInfo = *pActInfo;  // Keep a copy of the activation info to be able to use the output ports
#if CRY_PLATFORM_DURANGO
				// Register listener
				if (gEnv->pSystem->GetIInput()->GetKinectInput())
				{
					gEnv->pSystem->GetIInput()->GetKinectInput()->RegisterKinectAudioListener(this, "CSpeechRecognitionListenerNode");
				}
#endif

			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eI_Enable))
				{
					m_active = true;
				}
				if (IsPortActive(pActInfo, eI_Disable))
				{
					m_active = false;
				}
			}
			break;
		}
	}

private:
	bool            m_active;
	SActivationInfo m_actInfo;
};

// ------------------------------------------------------------------------
class CSpeechRecognitionSetEnableGrammarNode : public CFlowBaseNode<eNCT_Instanced>, public IKinectInputAudioListener
{
public:
	CSpeechRecognitionSetEnableGrammarNode(SActivationInfo* pActInfo)
	{
	}

	~CSpeechRecognitionSetEnableGrammarNode()
	{
#if CRY_PLATFORM_DURANGO
		if (gEnv->pSystem->GetIInput()->GetKinectInput())
		{
			gEnv->pSystem->GetIInput()->GetKinectInput()->UnregisterKinectAudioListener(this);
		}
#endif
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CSpeechRecognitionSetEnableGrammarNode(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
	}
	//////////////////////////////////////////////////////////////////////////

	virtual void OnVoiceCommand(const char* voiceCommand) {}
	virtual void OnGrammarChange(const char* grammarName, eGrammaChange change)
	{
		const string& myGrammarName = GetPortString(&m_actInfo, eI_GrammarName);

		if (myGrammarName == grammarName)
		{
			EOutputs outputPort = change == eGC_Enabled ? eO_Enabled :
			                      change == eGC_Disabled ? eO_Disabled : eO_NotLoaded;

			ActivateOutput(&m_actInfo, outputPort, true);
		}
	}

	void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	enum EInputs
	{
		eI_Enable = 0,
		eI_Disable,
		eI_GrammarName,
	};

	enum EOutputs
	{
		eO_Enabled = 0,
		eO_Disabled,
		eO_NotLoaded
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_AnyType("Enable",      _HELP("Enables a particular grammar (Xbox One)")),
			InputPortConfig_AnyType("Disable",     _HELP("Disables a particular grammar (Xbox One)")),
			InputPortConfig<string>("GrammarName", _HELP("Name of the grammar to be enabled/disabled (Xbox One)")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Enabled",   _HELP("The selected grammar has been Enabled (Xbox One)")),
			OutputPortConfig_Void("Disabled",  _HELP("The selected grammar has been Disabled (Xbox One)")),
			OutputPortConfig_Void("NotLoaded", _HELP("The selected grammar is not loaded (Xbox One)")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Enables/Disables the selected Grammar for Kinect Speech Recognition (Xbox One)");
		config.SetCategory(EFLN_APPROVED);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				m_actInfo = *pActInfo;  // Keep a copy of the activation info to be able to use the output ports
#if CRY_PLATFORM_DURANGO
				// Register listener
				IKinectInput* pKinectInput = gEnv->pSystem->GetIInput()->GetKinectInput();
				if (pKinectInput)
				{
					pKinectInput->RegisterKinectAudioListener(this, "CSpeechRecognitionSetEnableGrammarNode");
				}
#endif

			}
			break;
		case eFE_Activate:
			{
#if CRY_PLATFORM_DURANGO
				if (IKinectInput* pKinectInput = gEnv->pSystem->GetIInput()->GetKinectInput())
				{
					const string& grammarName = GetPortString(pActInfo, eI_GrammarName);
					if (IsPortActive(pActInfo, eI_Enable))
					{
						pKinectInput->SetEnableGrammar(grammarName, true);
					}
					else if (IsPortActive(pActInfo, eI_Disable))
					{
						pKinectInput->SetEnableGrammar(grammarName, false);
					}
				}
#else
				// If not Durango trigger corresponding output immediately
				if (IsPortActive(pActInfo, eI_Enable))
				{
					ActivateOutput(pActInfo, eO_Enabled, true);
				}
				else if (IsPortActive(pActInfo, eI_Disable))
				{
					ActivateOutput(pActInfo, eO_Disabled, true);
				}
#endif
			}
		}
	}

private:
	SActivationInfo m_actInfo;
};

REGISTER_FLOW_NODE("Input:SpeechRecognitionEnabled", CSpeechRecognitionIsEnabledNode);
REGISTER_FLOW_NODE("Input:SpeechRecognition", CSpeechRecognitionSetEnableNode);
REGISTER_FLOW_NODE("Input:SpeechRecognitionListener", CSpeechRecognitionListenerNode);
REGISTER_FLOW_NODE("Input:GrammarManagement", CSpeechRecognitionSetEnableGrammarNode);
