// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIUtilNodes.cpp
//  Version:     v1.00
//  Created:     24/4/212 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIUtilNodes.h"
#include "FlashUI.h"

//------------------------------------------------------------------------------------------------------
void CFlashUIPlatformNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("get", "Get current platform"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("IsPc",      _HELP("Triggered on PC")),
		OutputPortConfig_Void("IsXBoxOne", _HELP("Triggered on XBox One")),
		OutputPortConfig_Void("IsPS4",     _HELP("Triggered on PS4")),
		OutputPortConfig_Void("IsAndroid", _HELP("Triggered on Android")),
		OutputPortConfig_Void("IsConsole", _HELP("Triggered on Consoles")),
		{ 0 }
	};

	config.sDescription = "Node to get current platform, works also with selected platform of UIEmulator";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIPlatformNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Get))
	{
		if (gEnv->IsEditor())
		{
			if (gEnv->pFlashUI)
			{
				IFlashUI::EPlatformUI platform = ((CFlashUI*)gEnv->pFlashUI)->GetCurrentPlatform();
				switch (platform)
				{
				case IFlashUI::ePUI_PC:
					ActivateOutput(pActInfo, eO_IsPc, true);
					break;
				case IFlashUI::ePUI_Orbis:
					ActivateOutput(pActInfo, eO_IsOrbis, true);
					ActivateOutput(pActInfo, eO_IsConsole, true);
					break;
				case IFlashUI::ePUI_Durango:
					ActivateOutput(pActInfo, eO_IsDurango, true);
					ActivateOutput(pActInfo, eO_IsConsole, true);
					break;
				case IFlashUI::ePUI_Android:
					ActivateOutput(pActInfo, eO_IsAndroid, true);
					ActivateOutput(pActInfo, eO_IsConsole, true);
					break;
				}
			}
		}
		else
		{
#if CRY_PLATFORM_DURANGO
			ActivateOutput(pActInfo, eO_IsDurango, true);
			ActivateOutput(pActInfo, eO_IsConsole, true);
#elif CRY_PLATFORM_ORBIS
			ActivateOutput(pActInfo, eO_IsOrbis, true);
			ActivateOutput(pActInfo, eO_IsConsole, true);
#elif CRY_PLATFORM_ANDROID
			ActivateOutput(pActInfo, eO_IsAndroid, true);
			ActivateOutput(pActInfo, eO_IsConsole, true);
#else
			ActivateOutput(pActInfo, eO_IsPc, true);
#endif
		}
	}
}

//--------------------------------------------------------------------------------------------
void CFlashUIDelayNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputs[] = {
		InputPortConfig_AnyType("in",   _HELP("Value to be passed after [Delay] time"), _HELP("In")),
		InputPortConfig<float>("delay", 1.0f,                                           _HELP("Delay time in seconds"),_HELP("Delay")),
		{ 0 }
	};

	static const SOutputPortConfig outputs[] = {
		OutputPortConfig_AnyType("out", _HELP("Out")),
		{ 0 }
	};

	config.sDescription = _HELP("This node will delay passing the signal from [In] to [Out] for the specified number of seconds in [Delay]");
	config.pInputPorts = inputs;
	config.pOutputPorts = outputs;
	config.SetCategory(EFLN_APPROVED);

}

void CFlashUIDelayNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		break;
	case eFE_Activate:
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		m_fTime = gEnv->pTimer->GetAsyncCurTime();
		break;
	case eFE_Update:
		if (m_fTime + GetPortFloat(pActInfo, eI_Delay) < gEnv->pTimer->GetAsyncCurTime())
		{
			ActivateOutput(pActInfo, eO_Done, GetPortAny(pActInfo, eI_Start));
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}
		break;
	}
}

//------------------------------------------------------------------------------------------------------
REGISTER_FLOW_NODE("UI:Util:Platform", CFlashUIPlatformNode);
REGISTER_FLOW_NODE("UI:Util:UIDelay", CFlashUIDelayNode);
