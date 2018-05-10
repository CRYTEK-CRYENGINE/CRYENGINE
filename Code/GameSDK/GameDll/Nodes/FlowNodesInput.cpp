// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************
   -------------------------------------------------------------------------

   -------------------------------------------------------------------------
   History:
   - 10:3:2009  : Created by Jan Müller, integrated from G04

 *************************************************************************/

#include "StdAfx.h"
#include "FlowGameSDKBaseNode.h"
#include "UI/UIManager.h"
#include "Player.h"

// 360 keys enum
enum EXBoxKey
{
	// Invalid - Not an XBox key
	eXBK_Invalid = -1,

	// Keep in same order as EKeyID (KI_XINPUT_BASE+value)
	eXBK_DPadUp = 0,
	eXBK_DPadDown,
	eXBK_DPadLeft,
	eXBK_DPadRight,
	eXBK_Start,
	eXBK_Back,
	eXBK_ThumbL,
	eXBK_ThumbR,
	eXBK_ShoulderL,
	eXBK_ShoulderR,
	eXBK_A,
	eXBK_B,
	eXBK_X,
	eXBK_Y,
	eXBK_TriggerL,
	eXBK_TriggerR,
	eXBK_ThumbLX,
	eXBK_ThumbLY,
	eXBK_ThumbLUp,
	eXBK_ThumbLDown,
	eXBK_ThumbLLeft,
	eXBK_ThumbLRight,
	eXBK_ThumbRX,
	eXBK_ThumbRY,
	eXBK_ThumbRUp,
	eXBK_ThumbRDown,
	eXBK_ThumbRLeft,
	eXBK_ThumbRRight,
	eXBK_TriggerLBtn,
	eXBK_TriggerRBtn,
};

#if CRY_PLATFORM_ORBIS
static int OrbisKeyTable[] =
{
	eKI_Orbis_Up,           // 0:DPadUp
	eKI_Orbis_Down,         // 1:DPadDown
	eKI_Orbis_Left,         // 2:DPadLeft
	eKI_Orbis_Right,        // 3:DPadRight
	eKI_Orbis_Options,      // 4:Options
	-1,                     // 5
	eKI_Orbis_StickLX,      // 6:ThumbL
	eKI_Orbis_StickRY,      // 7:ThumbR
	eKI_Orbis_L1,           // 8:ShoulderL
	eKI_Orbis_R1,           // 9:ShoulderR
	eKI_Orbis_Cross,        // 10:A
	eKI_Orbis_Circle,       // 11:B
	eKI_Orbis_Square,       // 12:X
	eKI_Orbis_Triangle,     // 13:Y
	eKI_Orbis_LeftTrigger,  // 14: TriggerL
	eKI_Orbis_RightTrigger, // 15: TriggerR
	-1,                     // 16
	-1,                     // 17
	-1,                     // 18
	-1,                     // 19
	-1,                     // 20
	-1,                     // 21
	-1,                     // 22
	-1,                     // 23
	-1,                     // 26
	-1,                     // 27
	eKI_Orbis_L2,           // 28:TriggerLBtn
	eKI_Orbis_R2,           // 29:TriggerRBtn
};

#endif

#define XBoxKeyEnum    "enum_int:DPadUp=0,DPadDown=1,DPadLeft=2,DPadRight=3,Start=4,Back=5,ThumbL=6,ThumbR=7,ShoulderL=8,ShoulderR=9,A=10,B=11,X=12,Y=13,TriggerL=28,TriggerR=29"
#define XBoxAnalogEnum "enum_int:ThumbL=6,ThumbR=7,TriggerL=14,TriggerR=15"

//////////////////////////////////////////////////////////////////////////
class CG4FlowNode_XBoxKey : public CFlowGameSDKBaseNode<eNCT_Instanced>, public IInputEventListener
{
public:
	CG4FlowNode_XBoxKey(SActivationInfo* pActInfo) : m_bActive(false)
	{
	}

	~CG4FlowNode_XBoxKey()
	{
		Register(false);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CG4FlowNode_XBoxKey(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bActive", m_bActive);
		if (ser.IsReading())
		{
			m_actInfo = *pActInfo;
			Register(m_bActive);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_Key,
		EIP_NonDevMode,
	};

	enum EOutputPorts
	{
		EOP_Pressed = 0,
		EOP_Released,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",      _HELP("Enable reporting")),
			InputPortConfig_Void("Disable",     _HELP("Disable reporting")),
			InputPortConfig<int>("Key",         0,                          _HELP("XBoxOne controller key"),                                                    NULL, _UICONFIG(XBoxKeyEnum)),
			InputPortConfig<bool>("NonDevMode", false,                      _HELP("If set to true, can be used in Non-Devmode as well [Debugging backdoor]")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Pressed",  _HELP("Called when key is pressed")),
			OutputPortConfig_Void("Released", _HELP("Called when key is released")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get input from XBox 360 controller. EntityInput is used in multiplayer");
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (InputEntityIsLocalPlayer(pActInfo))
		{
			switch (event)
			{
			case eFE_Initialize:
				{
					m_actInfo = *pActInfo;
					Register(true);
				}
				break;

			case eFE_Activate:
				{
					m_actInfo = *pActInfo;
					if (IsPortActive(pActInfo, EIP_Enable))
					{
						Register(true);
					}
					if (IsPortActive(pActInfo, EIP_Disable))
					{
						Register(false);
					}
				}
				break;
			}
		}
	}

	void Register(bool bRegister)
	{
		if (IInput* pInput = gEnv->pInput)
		{
			if (true == bRegister)
			{
				const bool bAllowedInNonDevMode = GetPortBool(&m_actInfo, EIP_NonDevMode);
				if (gEnv->pSystem->IsDevMode() || bAllowedInNonDevMode)
					pInput->AddEventListener(this);
			}
			else
				pInput->RemoveEventListener(this);

			m_bActive = bRegister;
		}
		else
			m_bActive = false;
	}

	int TranslateKey(int nKeyId)
	{
		if (nKeyId >= eKI_XI_DPadUp && nKeyId <= eKI_XI_Disconnect)
			return nKeyId - KI_XINPUT_BASE;
		return eXBK_Invalid;
	}

	// ~IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& event)
	{
		if (true == m_bActive)
		{
#if CRY_PLATFORM_ORBIS
			int nThisKey = event.keyId;
			int nKey = -1;
			int nInput = GetPortInt(&m_actInfo, EIP_Key);
			int tableSize = CRY_ARRAY_COUNT(OrbisKeyTable);
			if (nInput >= 0 && nInput < tableSize)
				nKey = OrbisKeyTable[nInput];
#else
			// Translate key, check value
			const int nThisKey = TranslateKey(event.keyId);
			const int nKey = GetPortInt(&m_actInfo, EIP_Key);
#endif
			if (nKey == nThisKey)
			{
				// Return based on state
				if (eIS_Pressed == event.state)
					ActivateOutput(&m_actInfo, EOP_Pressed, true);
				else if (eIS_Released == event.state)
					ActivateOutput(&m_actInfo, EOP_Released, true);
			}
		}

		// Let other listeners handle it
		return false;
	}

private:
	bool            m_bActive; // TRUE when node is enabled
	SActivationInfo m_actInfo; // Activation info instance
};

//////////////////////////////////////////////////////////////////////////
class CG4FlowNode_XBoxAnalog : public CFlowGameSDKBaseNode<eNCT_Instanced>, public IInputEventListener
{
public:
	CG4FlowNode_XBoxAnalog(SActivationInfo* pActInfo) : m_bActive(false)
	{
	}

	~CG4FlowNode_XBoxAnalog()
	{
		Register(false);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CG4FlowNode_XBoxAnalog(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_bActive", m_bActive);
		if (ser.IsReading())
		{
			m_actInfo = *pActInfo;
			Register(m_bActive);
		}
	}

	enum EInputPorts
	{
		EIP_Enable = 0,
		EIP_Disable,
		EIP_Key,
		EIP_NonDevMode,
	};

	enum EOutputPorts
	{
		EOP_ChangedX = 0,
		EOP_ChangedY,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Enable",      _HELP("Enable reporting")),
			InputPortConfig_Void("Disable",     _HELP("Disable reporting")),
			InputPortConfig<int>("Key",         0,                          _HELP("XBoxOne controller key"),                                                    NULL, _UICONFIG(XBoxAnalogEnum)),
			InputPortConfig<bool>("NonDevMode", false,                      _HELP("If set to true, can be used in Non-Devmode as well [Debugging backdoor]")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] = {
			OutputPortConfig<float>("ChangedX", _HELP("Called when analog changes in X (trigger info sent out here as well)")),
			OutputPortConfig<float>("ChangedY", _HELP("Called when analog changes in Y")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Get analog input from XBox 360 controller. Note: Expensive!  Note2: entity input is used in multiplayer");
		config.SetCategory(EFLN_DEBUG);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (InputEntityIsLocalPlayer(pActInfo))
		{
			switch (event)
			{
			case eFE_Initialize:
				{
					m_actInfo = *pActInfo;
					Register(true);
				}
				break;

			case eFE_Activate:
				{
					m_actInfo = *pActInfo;
					if (IsPortActive(pActInfo, EIP_Enable))
					{
						Register(true);
					}
					if (IsPortActive(pActInfo, EIP_Disable))
					{
						Register(false);
					}
				}
				break;
			}
		}
	}

	void Register(bool bRegister)
	{
		if (IInput* pInput = gEnv->pInput)
		{
			if (true == bRegister)
			{
				const bool bAllowedInNonDevMode = GetPortBool(&m_actInfo, EIP_NonDevMode);
				if (gEnv->pSystem->IsDevMode() || bAllowedInNonDevMode)
					pInput->AddEventListener(this);
			}
			else
				pInput->RemoveEventListener(this);

			m_bActive = bRegister;
		}
		else
			m_bActive = false;
	}

	EXBoxKey TranslateKeyIntoFGInputEnum(int nKeyId, bool& isXAxis)
	{
		isXAxis = true;
		switch (nKeyId)
		{

#if CRY_PLATFORM_ORBIS
		// the values in the Input port are shared between all versions, that is why we translate here into xbox constants
		case eKI_Orbis_StickLY:
			isXAxis = false;
		case eKI_Orbis_StickLX:
			return eXBK_ThumbL;
			break;

		case eKI_Orbis_StickRY:
			isXAxis = false;
		case eKI_Orbis_StickRX:
			return eXBK_ThumbR;
			break;

		case eKI_Orbis_LeftTrigger:
			return eXBK_TriggerL;
			break;

		case eKI_Orbis_RightTrigger:
			return eXBK_TriggerR;
			break;
#else
		case eKI_XI_ThumbLY:
			isXAxis = false;
		case eKI_XI_ThumbLX:
			return eXBK_ThumbL;
			break;

		case eKI_XI_ThumbRY:
			isXAxis = false;
		case eKI_XI_ThumbRX:
			return eXBK_ThumbR;
			break;

		case eKI_XI_TriggerL:
			return eXBK_TriggerL;
			break;

		case eKI_XI_TriggerR:
			return eXBK_TriggerR;
			break;
#endif
		}
		return eXBK_Invalid;
	}

	// ~IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& event)
	{
		if (true == m_bActive)
		{
			bool isXAxis = false;
			const EXBoxKey nPressedKey = TranslateKeyIntoFGInputEnum(event.keyId, isXAxis);
			const EXBoxKey nExpectedKey = EXBoxKey(GetPortInt(&m_actInfo, EIP_Key));
			if (nExpectedKey == nPressedKey)
			{
				if (isXAxis)
					ActivateOutput(&m_actInfo, EOP_ChangedX, event.value);
				else
					ActivateOutput(&m_actInfo, EOP_ChangedY, event.value);
			}
		}

		// Let other listeners handle it
		return false;
	}

private:
	bool            m_bActive; // TRUE when node is enabled
	SActivationInfo m_actInfo; // Activation info instance
};

//////////////////////////////////////////////////////////////////////////
class CG4FlowNode_InputControlScheme : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CG4FlowNode_InputControlScheme(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		Get = 0
	};
	enum EOutPorts
	{
		Keyboard = 0, // Mouse is available but focus is on keyboard
		KeyboardMouse,
		XBoxOneController,
		PS4Controller,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_AnyType("Check", _HELP("Triggers a check of the current input control scheme")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig_AnyType("Keyboard",          _HELP("Outputs the signal from Check input if the current control scheme is keyboard (Focus on keyboard")),
			OutputPortConfig_AnyType("KeyboardMouse",     _HELP("Outputs the signal from Check input if the current control scheme is keyboardmouse (Focus on mouse with keyboard")),
			OutputPortConfig_AnyType("XBoxOneController", _HELP("Outputs the signal from Check input if the current control scheme is XBoxOne controller")),
			OutputPortConfig_AnyType("PS4Controller",     _HELP("Outputs the signal from Check input if the current control scheme is PS4 controller")),
			{ 0 }
		};
		config.sDescription = _HELP("Provides branching logic for different input control schemes");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			if (!IsPortActive(pActInfo, Get))
				return;

			EControlScheme curControlScheme = g_pGame->GetUI()->GetCurControlScheme();

			switch (curControlScheme)
			{
			case eControlScheme_Keyboard:
				ActivateOutput(pActInfo, Keyboard, GetPortAny(pActInfo, Get));
				break;
			case eControlScheme_KeyboardMouse:
				ActivateOutput(pActInfo, KeyboardMouse, GetPortAny(pActInfo, Get));
				break;
			case eControlScheme_XBoxOneController:
				ActivateOutput(pActInfo, XBoxOneController, GetPortAny(pActInfo, Get));
				break;
			case eControlScheme_PS4Controller:
				ActivateOutput(pActInfo, PS4Controller, GetPortAny(pActInfo, Get));
				break;

			default:
				GameWarning("CFlowNode_InputControlScheme::ProcessEvent: Failed to activate output port, unhandled control scheme type");
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
class CG4FlowNode_InputControlSchemeListener : public CFlowBaseNode<eNCT_Instanced>, public IUIControlSchemeListener
{
public:
	CG4FlowNode_InputControlSchemeListener(SActivationInfo* pActInfo)
		: m_listening(false)
	{
		m_actInfo = *pActInfo;
	}
	~CG4FlowNode_InputControlSchemeListener()
	{
		if (m_listening)
		{
			g_pGame->GetUI()->UnregisterControlSchemeListener(this);
		}
	}

	enum EInPorts
	{
		StartListening = 0,
		StopListening
	};
	enum EOutPorts
	{
		Keyboard = 0, // Mouse is available but focus is on keyboard
		KeyboardMouse,
		XBoxOneController,
		PS4Controller,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] =
		{
			InputPortConfig_AnyType("StartListening", _HELP("Starts listening to control scheme changes. Always triggers once on start.")),
			InputPortConfig_AnyType("StopListening",  _HELP("Starts listening to control scheme changes.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] =
		{
			OutputPortConfig_AnyType("Keyboard",          _HELP("Outputs the signal from Check input if the current control scheme is keyboard (Focus on keyboard")),
			OutputPortConfig_AnyType("KeyboardMouse",     _HELP("Outputs the signal from Check input if the current control scheme is keyboardmouse (Focus on mouse with keyboard")),
			OutputPortConfig_AnyType("XBoxOneController", _HELP("Outputs the signal from Check input if the current control scheme is XBoxOne controller")),
			OutputPortConfig_AnyType("PS4Controller",     _HELP("Outputs the signal from Check input if the current control scheme is PS4 controller")),
			{ 0 }
		};
		config.sDescription = _HELP("Provides branching logic for different input control schemes");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_listening", m_listening);
		if (ser.IsReading())
		{
			g_pGame->GetUI()->RegisterControlSchemeListener(this);
		}
	}

	virtual bool OnControlSchemeChanged(const EControlScheme controlScheme)
	{
		switch (controlScheme)
		{
		case eControlScheme_Keyboard:
			ActivateOutput(&m_actInfo, Keyboard, true);
			break;
		case eControlScheme_KeyboardMouse:
			ActivateOutput(&m_actInfo, KeyboardMouse, true);
			break;
		case eControlScheme_XBoxOneController:
			ActivateOutput(&m_actInfo, XBoxOneController, true);
			break;
		case eControlScheme_PS4Controller:
			ActivateOutput(&m_actInfo, PS4Controller, true);
			break;
		}
		return false;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
		{
			m_actInfo = *pActInfo;
		}
		if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, StartListening))
			{
				g_pGame->GetUI()->RegisterControlSchemeListener(this);
				m_listening = true;

				EControlScheme curControlScheme = g_pGame->GetUI()->GetCurControlScheme();
				OnControlSchemeChanged(curControlScheme);

			}
			else if (IsPortActive(pActInfo, StopListening))
			{
				g_pGame->GetUI()->UnregisterControlSchemeListener(this);
				m_listening = false;
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CG4FlowNode_InputControlSchemeListener(pActInfo);
	}

	bool            m_listening;
	SActivationInfo m_actInfo;
};

//////////////////////////////////////////////////////////////////////////
class CFlowNode_MoveOverlay : public CFlowBaseNode<eNCT_Singleton>
{
	enum EInputs
	{
		eIP_ENABLE = 0,
		eIP_DISABLE,
		eIP_MOVE_X,
		eIP_MOVE_Y,
		eIP_WEIGHT
	};

public:
	CFlowNode_MoveOverlay(SActivationInfo* pActInfo){}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] =
		{
			InputPortConfig_Void("Enable",   _HELP("Enables Overlay and Sets the parameters")),
			InputPortConfig_Void("Disable",  _HELP("Disables the overlay")),
			InputPortConfig<float>("MoveX",  0.f,                                              _HELP("Input Left/Right (positive is left)"),                                  0, _UICONFIG("v_min=-1,v_max=1")),
			InputPortConfig<float>("MoveY",  0.5f,                                             _HELP("Input Forward/Backward (positive is forward)"),                         0, _UICONFIG("v_min=-1,v_max=1")),
			InputPortConfig<float>("Weight", 0.5f,                                             _HELP("Tells how strong the overlay is. (0 = no Overlay, 1 = Full Overlay)"),  0, _UICONFIG("v_min=0,v_max=1")),
			{ 0 }
		};
		static const SOutputPortConfig outputs[] =
		{
			{ 0 }
		};

		config.nFlags &= ~EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Inputs movement that overlays with the players input and mixes the movements.");
		config.SetCategory(EFLN_APPROVED);
	}

	//////////////////////////////////////////////////////////////////////////
	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			DisableOverlay();
			break;

		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, eIP_ENABLE))
				{
					SendOverlayData(pActInfo);
				}
				else if (IsPortActive(pActInfo, eIP_DISABLE))
				{
					DisableOverlay();
				}
			}
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void DisableOverlay()
	{
		CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pClientActor)
		{
			if (pClientActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
				IPlayerInput* pPlayerInput = pClientPlayer->GetPlayerInput();

				if (pPlayerInput)
				{
					pPlayerInput->OnAction(CCryName("move_overlay_disable"), eAAM_OnPress, 0.f);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SendOverlayData(SActivationInfo* pActInfo)
	{
		CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pClientActor)
		{
			if (pClientActor->GetActorClass() == CPlayer::GetActorClassType())
			{
				CPlayer* pClientPlayer = static_cast<CPlayer*>(pClientActor);
				IPlayerInput* pPlayerInput = pClientPlayer->GetPlayerInput();

				if (pPlayerInput)
				{
					const float moveX = GetPortFloat(pActInfo, eIP_MOVE_X);
					const float moveY = GetPortFloat(pActInfo, eIP_MOVE_Y);
					const float weight = GetPortFloat(pActInfo, eIP_WEIGHT);

					pPlayerInput->OnAction(CCryName("move_overlay_enable"), eAAM_OnHold, 0.f);
					pPlayerInput->OnAction(CCryName("move_overlay_x"), eAAM_OnHold, moveX);
					pPlayerInput->OnAction(CCryName("move_overlay_y"), eAAM_OnHold, moveY);
					pPlayerInput->OnAction(CCryName("move_overlay_weight"), eAAM_OnHold, weight);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//////////////////////////////////////////////////////////////////////////
// Register nodes

REGISTER_FLOW_NODE("Debug:InputXboxKey", CG4FlowNode_XBoxKey);
REGISTER_FLOW_NODE("Debug:InputXboxAnalog", CG4FlowNode_XBoxAnalog);
REGISTER_FLOW_NODE("Input:MoveOverlay", CFlowNode_MoveOverlay);
REGISTER_FLOW_NODE("Input:ControlScheme", CG4FlowNode_InputControlScheme);
REGISTER_FLOW_NODE("Input:ControlSchemeListener", CG4FlowNode_InputControlSchemeListener);
