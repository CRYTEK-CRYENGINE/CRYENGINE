// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIActionNodes.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIActionNodes.h"
#include "FlashUIBaseNode.h"
#include "FlashUI.h"

//--------------------------------------------------------------------------------------------
CFlashUIActionBaseNode::CFlashUIActionBaseNode(const char* name)
	: m_pAction(NULL)
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->GetUIActionManager()->AddListener(this, name);
}

CFlashUIActionBaseNode::~CFlashUIActionBaseNode()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->GetUIActionManager()->RemoveListener(this);
}

void CFlashUIActionBaseNode::UpdateAction(const char* sName, bool bStrict)
{
	if (gEnv->pFlashUI)
		m_pAction = gEnv->pFlashUI->GetUIAction(sName);

	if (!m_pAction && bStrict)
	{
		UIACTION_ERROR("FG: UIAction \"%s\" does not exist", sName);
	}
}

void CFlashUIActionBaseNode::UpdateAction(IFlowGraph* pGraph)
{
	if (gEnv->pFlashUI)
	{
		int i = 0;
		while (IUIAction* pAction = gEnv->pFlashUI->GetUIAction(i++))
		{
			if (pAction->GetType() == IUIAction::eUIAT_FlowGraph && pAction->GetFlowGraph() == pGraph)
			{
				m_pAction = pAction;
				break;
			}
		}
	}
	if (!m_pAction)
	{
		UIACTION_WARNING("FG: UI:Action:Start/End nodes only allowed within UIAction graph!");
	}
}

//--------------------------------------------------------------------------------------------
CFlashUIStartActionNode::~CFlashUIStartActionNode()
{
	m_stack.clear();
}

void CFlashUIStartActionNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<bool>("UseAsState", false, "If this is set to true, the Flowgraph is disabled per default. It will be enabled once it gets started.", "DisableAction"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("StartAction", "Triggered if someone starts the action"),
		OutputPortConfig<string>("Args",     "Comma separated argument string"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Start node for UI Action";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIStartActionNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		UpdateAction(pActInfo->pGraph);
		m_stack.clear();
		m_stack.init(pActInfo->pGraph);
		if (m_pAction)
		{
			CRY_ASSERT_MESSAGE(gEnv->pFlashUI, "No FlashUI extension available!");
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			if (GetPortBool(pActInfo, eI_UseAsState))
				gEnv->pFlashUI->GetUIActionManager()->EnableAction(m_pAction, false);
		}
		break;
	case eFE_Update:
		FlushNextAction(pActInfo);
		break;
	}
}

void CFlashUIStartActionNode::OnStart(IUIAction* pAction, const SUIArguments& args)
{
	if (m_pAction != NULL && pAction == m_pAction)
	{
		UI_STACK_PUSH(m_stack, std::make_pair(pAction, args), "Start UIAction %s (%s)", pAction->GetName(), args.GetAsString());
	}
}

void CFlashUIStartActionNode::FlushNextAction(SActivationInfo* pActInfo)
{
	if (m_stack.size() > 0)
	{
		const SUIArguments& args = m_stack.get().second;
		ActivateOutput(pActInfo, eO_OnActionStart, true);
		ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
		m_stack.pop();
	}
}

//--------------------------------------------------------------------------------------------
void CFlashUIEndActionNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("EndAction",   "Trigger to announce that action is finished"),
		InputPortConfig<bool>("UseAsState", false,                                         "If this is set to true, the end node will disable this UIAction flowgraph","DisableAction"),
		InputPortConfig<string>("Args",     "Comma separated argument string"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = 0;
	config.sDescription = "End node for UI Action";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIEndActionNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		UpdateAction(pActInfo->pGraph);
		break;
	case eFE_Activate:
		if (gEnv->pFlashUI && m_pAction && IsPortActive(pActInfo, eI_OnActionEnd))
		{
			SUIArguments args(GetPortString(pActInfo, eI_Args).c_str(), true);
			gEnv->pFlashUI->GetUIActionManager()->EndAction(m_pAction, args);

			if (GetPortBool(pActInfo, eI_UseAsState))
			{
				gEnv->pFlashUI->GetUIActionManager()->EnableAction(m_pAction, false);
			}
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------
CFlashUIActionNode::~CFlashUIActionNode()
{
	m_startStack.clear();
	m_endStack.clear();
	m_selfEndStack.clear();
}

void CFlashUIActionNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<string>("uiActions_UIAction", "Name of UI action",                _HELP("UIAction")),
		InputPortConfig<bool>("Strict",               false,                              "If true this node will log an error if the UIAction does not exist, otherwise it can be used in a more loose way."),
		InputPortConfig_Void("Start",                 "Start UI action"),
		InputPortConfig<string>("Args",               "Comma separated argument string"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnStart",    "Triggered if this node starts the action"),
		OutputPortConfig_Void("OnEnd",      "Triggered if action is stopped and was started by this node"),
		OutputPortConfig_Void("OnStartAll", "Always triggered if the action started"),
		OutputPortConfig_Void("OnEndAll",   "Always triggered if action is stopped"),
		OutputPortConfig<string>("Args",    "Comma separated argument string"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Controls an UI Action";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIActionNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		m_bWasStarted = false;
		m_startStack.clear();
		m_endStack.clear();
		m_selfEndStack.clear();
		m_startStack.init(pActInfo->pGraph);
		m_endStack.init(pActInfo->pGraph);
		m_selfEndStack.init(pActInfo->pGraph);
		UpdateAction(GetPortString(pActInfo, eI_UIAction).c_str(), GetPortBool(pActInfo, eI_Strict));
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		break;
	case eFE_Activate:
		UpdateAction(GetPortString(pActInfo, eI_UIAction).c_str(), GetPortBool(pActInfo, eI_Strict));

		if (gEnv->pFlashUI && m_pAction)
		{
			if (IsPortActive(pActInfo, eI_StartAction))
			{
				SUIArguments args(GetPortString(pActInfo, eI_Args).c_str(), true);
				gEnv->pFlashUI->GetUIActionManager()->StartAction(m_pAction, args);
				ActivateOutput(pActInfo, eO_UIOnStart, true);
				m_bWasStarted = true;
			}
		}
		break;
	case eFE_Update:
		FlushNextAction(pActInfo);
		break;
	}
}

void CFlashUIActionNode::OnStart(IUIAction* pAction, const SUIArguments& args)
{
	if (m_pAction != NULL && pAction == m_pAction)
	{
		UI_STACK_PUSH(m_startStack, std::make_pair(pAction, args), "OnStart UIAction %s (%s)", pAction->GetName(), args.GetAsString());
	}
}

void CFlashUIActionNode::OnEnd(IUIAction* pAction, const SUIArguments& args)
{
	if (m_pAction != NULL && pAction == m_pAction)
	{
		UI_STACK_PUSH(m_endStack, std::make_pair(pAction, args), "OnEnd UIAction %s (%s)", pAction->GetName(), args.GetAsString());
		if (m_bWasStarted)
		{
			m_bWasStarted = false;
			UI_STACK_PUSH(m_selfEndStack, std::make_pair(pAction, args), "OnEnd (self) UIAction %s (%s)", pAction->GetName(), args.GetAsString());
		}
	}
}

void CFlashUIActionNode::FlushNextAction(SActivationInfo* pActInfo)
{
	if (m_startStack.size() > 0)
	{
		const SUIArguments& args = m_startStack.get().second;
		ActivateOutput(pActInfo, eO_UIOnStartAll, true);
		ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
		m_startStack.pop();
	}
	else if (m_endStack.size() > 0)
	{
		const SUIArguments& args = m_endStack.get().second;
		ActivateOutput(pActInfo, eO_UIOnEndAll, true);
		ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
		m_endStack.pop();
	}
	else if (m_selfEndStack.size() > 0)
	{
		const SUIArguments& args = m_selfEndStack.get().second;
		ActivateOutput(pActInfo, eO_UIOnEnd, true);
		ActivateOutput(pActInfo, eO_Args, string(args.GetAsString()));
		m_selfEndStack.pop();
	}
}

//--------------------------------------------------------------------------------------------
REGISTER_FLOW_NODE("UI:Action:Start", CFlashUIStartActionNode);
REGISTER_FLOW_NODE("UI:Action:End", CFlashUIEndActionNode);
REGISTER_FLOW_NODE("UI:Action:Control", CFlashUIActionNode);
