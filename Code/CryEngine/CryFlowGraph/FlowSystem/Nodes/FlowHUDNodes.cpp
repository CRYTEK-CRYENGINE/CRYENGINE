// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
#include "FlowFrameworkBaseNode.h"

#include <CryString/StringUtils.h>
#include "FlowSystem/FlowSystemCVars.h"

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

			if (CFlowSystemCVars::Get().fg_DisableHUDText == 0)
			{
				IRenderer* pRenderer = gEnv->pRenderer;

				// Get correct coordinates
				float x = GetPortFloat(pActInfo, INP_X);
				float y = GetPortFloat(pActInfo, INP_Y);

				int aspectRatioFlag = 0;

				if ((x < 1.f || y < 1.f) && gEnv->pRenderer)
				{
					const int screenWidth  = std::max(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceX(), 1);
					const int screenHeight = std::max(IRenderAuxGeom::GetAux()->GetCamera().GetViewSurfaceZ(), 1);

					if (x < 1.f)
						x *= (float)screenWidth;
					if (y < 1.f)
						y *= (float)screenHeight;

					aspectRatioFlag = pRenderer->IsStereoEnabled() || (screenWidth < screenHeight) ? 0 : eDrawText_800x600;
				}

				SDrawTextInfo ti;
				float scale = GetPortFloat(pActInfo, INP_FontSize);
				int flags = eDrawText_2D | aspectRatioFlag | eDrawText_FixedSize | (GetPortBool(pActInfo, INP_Centered) ? eDrawText_Center | eDrawText_CenterV : 0);
				Vec3 color = GetPortVec3(pActInfo, INP_Color);

				IRenderAuxText::DrawText(Vec3(x, y, 0.5f), scale, color, flags, GetPortString(pActInfo, INP_Message).c_str());
			}
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

protected:
	bool InputEntityIsLocalPlayer(const IFlowNode::SActivationInfo* const pActInfo) const
	{
		bool bRet = true;

		if (pActInfo->pEntity)
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pActInfo->pEntity->GetId());
			if (pActor != gEnv->pGameFramework->GetClientActor())
			{
				bRet = false;
			}
		}
		else if (gEnv->bMultiplayer)
		{
			bRet = false;
		}

		return bRet;
	}


	bool  m_isVisible;
	bool  m_isPermanent;
	float m_showTimeLeft;

};

REGISTER_FLOW_NODE("Debug:DisplayMessage", CFlowNode_DisplayDebugMessage);
