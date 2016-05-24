// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowHUDNodes.cpp
//  Version:     v1.00
//  Created:     July 4th 2005 by Julien.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FlowBaseNode.h"
#include <CryAction.h>
#include <IGameObjectSystem.h>
#include <CryString/StringUtils.h>
#include <IActorSystem.h>
#include <PersistantDebug.h>
#include "CryActionCVars.h"

// display an instruction message in the HUD
class CFlowNode_DisplayInstructionMessage : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DisplayInstructionMessage(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			// a "void" port that can be pulsed to display the message
			InputPortConfig_Void("display",         _HELP("Connect event here to display the message")),
			// this string port is the message that should be displayed
			InputPortConfig<string>("text_message", _HELP("Display this message on the hud")),
			// this floating point input port is how long the message should be displayed
			InputPortConfig<float>("displayTime",   10.0f,                                              _HELP("How long to display message")),
			{ 0 }
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_OBSOLETE);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		// if the event is activate, and the 0th port ("display") has been activated,
		// display the message
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{
				static const ColorF col(175.0f / 255.0f, 218.0f / 255.0f, 154.0f / 255.0f, 1.0f);
				CCryAction::GetCryAction()->GetIPersistantDebug()->Add2DText(GetPortString(pActInfo, 1).c_str(), 2.f, col, GetPortFloat(pActInfo, 2));
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

// display an info message in the HUD
class CFlowNode_DisplayInfoMessage : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_DisplayInfoMessage(SActivationInfo* pActInfo)
	{
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			// a "void" port that can be pulsed to display the message
			InputPortConfig_Void("display",       _HELP("Connect event here to display the message")),
			// this string port is the message that should be displayed
			InputPortConfig<string>("message",    _HELP("Display this message on the hud")),
			// this floating point input port is how long the message should be displayed
			InputPortConfig<float>("displayTime", 10.0f,                                              _HELP("How long to display message")),
			{ 0 }
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_OBSOLETE);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		// if the event is activate, and the 0th port ("display") has been activated,
		// display the message
		if (event == eFE_Activate && IsPortActive(pActInfo, 0))
		{
			if (CFlowSystemCVars::Get().m_noDebugText == 0)
			{
				static const ColorF col(175.0f / 255.0f, 218.0f / 255.0f, 154.0f / 255.0f, 1.0f);
				CCryAction::GetCryAction()->GetIPersistantDebug()->Add2DText(GetPortString(pActInfo, 1).c_str(), 2.f, col, GetPortFloat(pActInfo, 2));
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

// display a debug message in the HUD
class CFlowNode_DisplayDebugMessage : public CFlowBaseNode<eNCT_Instanced>
{
	enum
	{
		INP_Show = 0,
		INP_Hide,
		INP_Message,
		INP_DisplayTime,
		INP_X,
		INP_Y,
		INP_FontSize,
		INP_Color,
		INP_Centered,
	};

	enum
	{
		OUT_Show = 0,
		OUT_Hide
	};

public:
	CFlowNode_DisplayDebugMessage(SActivationInfo* pActInfo)
		: m_isVisible(false)
		, m_isPermanent(false)
		, m_showTimeLeft(0)
	{

	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_DisplayDebugMessage(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.Value("m_isVisible", m_isVisible);
		ser.Value("m_isPermanent", m_isPermanent);
		ser.Value("m_showTimeLeft", m_showTimeLeft);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_AnyType("Show",       _HELP("Show message")),
			InputPortConfig_AnyType("Hide",       _HELP("Hide message")),
			InputPortConfig<string>("message",    _HELP("Display this message on the hud")),
			InputPortConfig<float>("DisplayTime", 0.f,                                      _HELP("How much time the message will be visible. 0 = permanently visible.")),

			// this floating point input port is the x position where the message should be displayed
			InputPortConfig<float>("posX",        50.0f,                                    _HELP("Input x text position")),
			// this floating point input port is the y position where the message should be displayed
			InputPortConfig<float>("posY",        50.0f,                                    _HELP("Input y text position")),
			// this floating point input port is the font size of the message that should be displayed
			InputPortConfig<float>("fontSize",    2.0f,                                     _HELP("Input font size")),
			InputPortConfig<Vec3>("clr_Color",    Vec3(1.f,                                 1.f,                                               1.f),0, _HELP("color")),
			InputPortConfig<bool>("centered",     false,                                    _HELP("centers the text around the coordinates")),
			{ 0 }
		};
		static const SOutputPortConfig out_ports[] =
		{
			OutputPortConfig_AnyType("Show", _HELP("")),
			OutputPortConfig_AnyType("Hide", _HELP("")),
			{ 0 }
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_ports;
		config.pOutputPorts = out_ports;
		config.SetCategory(EFLN_DEBUG);
		config.sDescription = _HELP("If an entity is not provided, the local player will be used instead");
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (CFlowSystemCVars::Get().m_noDebugText != 0)
			return;
		if (!InputEntityIsLocalPlayer(pActInfo))
			return;

		switch (event)
		{
		case eFE_Initialize:
			m_isPermanent = false;
			m_isVisible = false;
			m_showTimeLeft = 0;
			break;

		case eFE_Activate:
			if (IsPortActive(pActInfo, INP_Show))
			{
				m_showTimeLeft = GetPortFloat(pActInfo, INP_DisplayTime);
				m_isPermanent = m_showTimeLeft == 0;
				if (!m_isVisible)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					m_isVisible = true;
				}
				ActivateOutput(pActInfo, OUT_Show, true);
			}

			if (IsPortActive(pActInfo, INP_Hide) && m_isVisible)
			{
				m_isVisible = false;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				ActivateOutput(pActInfo, OUT_Hide, true);
			}

			if (IsPortActive(pActInfo, INP_DisplayTime))
			{
				m_showTimeLeft = GetPortFloat(pActInfo, INP_DisplayTime);
				m_isPermanent = m_showTimeLeft == 0;
			}
			break;

		case eFE_Update:
			if (!m_isPermanent)
			{
				m_showTimeLeft -= gEnv->pTimer->GetFrameTime();
				if (m_showTimeLeft <= 0)
				{
					m_isVisible = false;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
					ActivateOutput(pActInfo, OUT_Hide, true);
				}
			}

			if (CCryActionCVars::Get().cl_DisableHUDText == 0)
			{
				IRenderer* pRenderer = gEnv->pRenderer;

				// Get correct coordinates
				float x = GetPortFloat(pActInfo, INP_X);
				float y = GetPortFloat(pActInfo, INP_Y);

				int aspectRatioFlag = 0;

				if (x < 1.f || y < 1.f)
				{
					int screenX, screenY, screenWidth, screenHeight;
					pRenderer->GetViewport(&screenX, &screenY, &screenWidth, &screenHeight);

					if (x < 1.f)
						x *= (float)screenWidth;
					if (y < 1.f)
						y *= (float)screenHeight;

					aspectRatioFlag = pRenderer->IsStereoEnabled() || (screenWidth < screenHeight) ? 0 : eDrawText_800x600;
				}

				SDrawTextInfo ti;
				ti.xscale = ti.yscale = GetPortFloat(pActInfo, INP_FontSize);
				ti.flags = eDrawText_2D | aspectRatioFlag | eDrawText_FixedSize;
				Vec3 color = GetPortVec3(pActInfo, INP_Color);
				ti.color[0] = color.x;
				ti.color[1] = color.y;
				ti.color[2] = color.z;

				ti.flags |= GetPortBool(pActInfo, INP_Centered) ? eDrawText_Center | eDrawText_CenterV : 0;

				pRenderer->DrawTextQueued(Vec3(x, y, 0.5f), ti, GetPortString(pActInfo, INP_Message).c_str());
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	bool  m_isVisible;
	bool  m_isPermanent;
	float m_showTimeLeft;

};

// display a debug message in the HUD
class CFlowNode_DisplayTimedDebugMessage : public CFlowBaseNode<eNCT_Instanced>
{
	CTimeValue m_endTime;

public:
	CFlowNode_DisplayTimedDebugMessage(SActivationInfo* pActInfo)
	{
	}

	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_DisplayTimedDebugMessage(pActInfo);
	}

	void Serialize(SActivationInfo*, TSerialize ser)
	{
		ser.Value("m_endTime", m_endTime);
	}

	void GetConfiguration(SFlowNodeConfig& config)
	{
		// declare input ports
		static const SInputPortConfig in_ports[] =
		{
			InputPortConfig_Void("Trigger",       _HELP("Trigger this port to display message")),
			InputPortConfig<string>("Message",    _HELP("Message to display")),
			InputPortConfig<float>("DisplayTime", 2.0f,                                          _HELP("How long to display in seconds (<=0.0 means forever)")),
			InputPortConfig<float>("PosX",        50.0f,                                         _HELP("X Position of text")),
			InputPortConfig<float>("PosY",        50.0f,                                         _HELP("Y Position of text")),
			InputPortConfig<float>("FontSize",    2.0f,                                          _HELP("Input font size")),
			{ 0 }
		};
		// we set pointers in "config" here to specify which input and output ports the node contains
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Display a debug [Message] for [DisplayTime] seconds\rIf an entity is not provided, the local player will be used instead");
		config.pInputPorts = in_ports;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_DEBUG);
	}

	void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{

		if (event == eFE_Initialize)
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}

		if (InputEntityIsLocalPlayer(pActInfo))
		{
			if (event == eFE_Activate)
			{
				if (CFlowSystemCVars::Get().m_noDebugText == 0)
				{
					if (IsPortActive(pActInfo, 0))
					{
						m_endTime = gEnv->pTimer->GetFrameStartTime() + GetPortFloat(pActInfo, 2);
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
					}
				}
			}
			else if (event == eFE_Update)
			{
				if (CFlowSystemCVars::Get().m_noDebugText == 0)
				{
					if (CCryActionCVars::Get().cl_DisableHUDText == 0)
					{
						IRenderer* pRenderer = gEnv->pRenderer;

						// Get correct coordinates
						float x = GetPortFloat(pActInfo, 3);
						float y = GetPortFloat(pActInfo, 4);
						if (x < 1.f || y < 1.f)
						{
							int screenX, screenY, screenWidth, screenHeight;
							pRenderer->GetViewport(&screenX, &screenY, &screenWidth, &screenHeight);
							if (x < 1.f)
								x *= (float)screenWidth;
							if (y < 1.f)
								y *= (float)screenHeight;
						}

						float drawColor[4] = { 1, 1, 1, 1 };
						pRenderer->Draw2dLabel(x,
						                       y,
						                       GetPortFloat(pActInfo, 5),
						                       drawColor,
						                       false,
						                       "%s", GetPortString(pActInfo, 1).c_str());
					}
				}
				// check if we should show forever...
				if (GetPortFloat(pActInfo, 2) > 0.0f)
				{
					// no, so check if time-out
					CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();
					if (curTime >= m_endTime)
						pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
				}
			}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

REGISTER_FLOW_NODE("HUD:DisplayInstructionMessage", CFlowNode_DisplayInstructionMessage);
REGISTER_FLOW_NODE("HUD:DisplayInfoMessage", CFlowNode_DisplayInfoMessage);
REGISTER_FLOW_NODE("HUD:DisplayDebugMessage", CFlowNode_DisplayDebugMessage);
REGISTER_FLOW_NODE("HUD:DisplayTimedDebugMessage", CFlowNode_DisplayTimedDebugMessage);
