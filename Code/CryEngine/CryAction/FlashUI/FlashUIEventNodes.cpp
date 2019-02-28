// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIEventNodes.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIEventNodes.h"
#include "FlashUIBaseNode.h"

//--------------------------------------------------------------------------------------------
CFlashUIEventSystemFunctionNode::CFlashUIEventSystemFunctionNode(IUIEventSystem* pSystem, string sCategory, const SUIEventDesc* pEventDesc)
	: CFlashUIBaseNodeDynPorts(sCategory)
	, m_pSystem(pSystem)
{
	CRY_ASSERT_MESSAGE(m_pSystem && pEventDesc, "NULL pointer passed!");

	m_eventDesc = *pEventDesc;
	m_iEventId = m_pSystem->GetEventId(pEventDesc->sName);

	m_inPorts.push_back(InputPortConfig_Void("send", "Send the event to the engine"));
	AddParamInputPorts(m_eventDesc.InputParams, m_inPorts);
	if (m_eventDesc.InputParams.IsDynamic)
		m_inPorts.push_back(InputPortConfig<string>("Array", m_eventDesc.InputParams.sDynamicDesc, m_eventDesc.InputParams.sDynamicName));
	m_inPorts.push_back(InputPortConfig_Void(NULL));

	m_outPorts.push_back(OutputPortConfig_Void("OnEvent", "Triggered once the event was sent"));
	AddParamOutputPorts(m_eventDesc.OutputParams, m_outPorts);
	if (m_eventDesc.OutputParams.IsDynamic)
		m_outPorts.push_back(OutputPortConfig<string>("Array", m_eventDesc.OutputParams.sDynamicDesc, m_eventDesc.OutputParams.sDynamicName));
	m_outPorts.push_back(OutputPortConfig_Void(NULL));
}

CFlashUIEventSystemFunctionNode::~CFlashUIEventSystemFunctionNode()
{
}

IFlowNodePtr CFlashUIEventSystemFunctionNode::Clone(SActivationInfo* pActInfo)
{
	return IFlowNodePtr(new CFlashUIEventSystemFunctionNode(m_pSystem, GetCategory(), &m_eventDesc));
}

void CFlashUIEventSystemFunctionNode::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = &m_inPorts[0];
	config.pOutputPorts = &m_outPorts[0];
	config.sDescription = m_eventDesc.sDesc;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIEventSystemFunctionNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		if (IsPortActive(pActInfo, eI_Send))
		{
			int port = 1;
			SUIArguments args;
			for (TUIParams::const_iterator iter = m_eventDesc.InputParams.Params.begin(); iter != m_eventDesc.InputParams.Params.end(); ++iter)
				GetDynInput(args, *iter, pActInfo, port++);

			if (m_eventDesc.InputParams.IsDynamic)
			{
				string val;
				const TFlowInputData& data = GetPortAny(pActInfo, port++);
				data.GetValueWithConversion(val);
				args.AddArguments(val.c_str());
			}
			SUIArgumentsRet res = m_pSystem->SendEvent(SUIEvent(m_iEventId, args));

			int end = m_eventDesc.OutputParams.Params.size();
			int i = 0;
			for (; i < end; ++i)
			{
				CRY_ASSERT_MESSAGE(i < res.GetArgCount(), "UIEvent returns wrong number of arguments!");
				ActivateDynOutput(i < res.GetArgCount() ? res.GetArg(i) : TUIData(string("")), m_eventDesc.OutputParams.Params[i], pActInfo, i + 1);
			}
			if (m_eventDesc.OutputParams.IsDynamic)
			{
				string val;
				SUIArguments dynarg;
				for (; i < res.GetArgCount(); ++i)
				{
					if (res.GetArg(i, val))
						dynarg.AddArgument(val);
				}
				ActivateOutput(pActInfo, end + 1, string(dynarg.GetAsString()));
			}
			ActivateOutput(pActInfo, eO_OnEvent, true);
		}
		break;
	}
}

//--------------------------------------------------------------------------------------------
CFlashUIEventSystemEventNode::CFlashUIEventSystemEventNode(IUIEventSystem* pSystem, string sCategory, const SUIEventDesc* pEventDesc)
	: CFlashUIBaseNodeDynPorts(sCategory)
	, m_pSystem(pSystem)
{
	CRY_ASSERT_MESSAGE(m_pSystem && pEventDesc, "NULL pointer passed!");

	m_iEventId = m_pSystem->GetEventId(pEventDesc->sName);
	m_eventDesc = *pEventDesc;

	//inputs
	AddCheckPorts(m_eventDesc.InputParams, m_inPorts);
	m_inPorts.push_back(InputPortConfig_Void(NULL));

	// outputs
	m_outPorts.push_back(OutputPortConfig_Void("onEvent", "On event"));
	AddParamOutputPorts(m_eventDesc.InputParams, m_outPorts);
	if (m_eventDesc.InputParams.IsDynamic)
		m_outPorts.push_back(OutputPortConfig<string>("Array", m_eventDesc.InputParams.sDynamicDesc, m_eventDesc.InputParams.sDynamicName));
	m_outPorts.push_back(OutputPortConfig_Void(NULL));

	m_pSystem->RegisterListener(this, "CFlashUIEventSystemEventNode");
}

CFlashUIEventSystemEventNode::~CFlashUIEventSystemEventNode()
{
	m_events.clear();
	if (m_pSystem)
		m_pSystem->UnregisterListener(this);
}

IFlowNodePtr CFlashUIEventSystemEventNode::Clone(SActivationInfo* pActInfo)
{
	return IFlowNodePtr(new CFlashUIEventSystemEventNode(m_pSystem, GetCategory(), &m_eventDesc));
}

void CFlashUIEventSystemEventNode::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = &m_inPorts[0];
	config.pOutputPorts = &m_outPorts[0];
	config.sDescription = m_eventDesc.sDesc;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIEventSystemEventNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		m_events.clear();
		m_events.init(pActInfo->pGraph);
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
	}
	else if (event == eFE_Update)
	{
		FlushNextEvent(pActInfo);
	}
}

SUIArgumentsRet CFlashUIEventSystemEventNode::OnEvent(const SUIEvent& event)
{
	if (m_iEventId == event.event)
	{
		UI_STACK_PUSH(m_events, event.args, "OnEvent %s (%s)", m_eventDesc.sDisplayName, event.args.GetAsString());
	}
	return SUIArguments();
}

void CFlashUIEventSystemEventNode::FlushNextEvent(SActivationInfo* pActInfo)
{
	if (m_events.size() > 0)
	{
		const SUIArguments& args = m_events.get();
		bool bTriggerEvent = true;
		const int checkValue = GetPortInt(pActInfo, eI_CheckPort);

		if (checkValue >= 0)
		{
			bTriggerEvent = false;
			CRY_ASSERT_MESSAGE(checkValue < args.GetArgCount(), "Port does not exist!");
			if (checkValue < args.GetArgCount())
			{
				string val = GetPortString(pActInfo, eI_CheckValue);
				string compstr;
				args.GetArg(checkValue).GetValueWithConversion(compstr);
				bTriggerEvent = val == compstr;
			}
		}

		if (bTriggerEvent)
		{
			int end = m_eventDesc.InputParams.Params.size();
			int i = 0;
			for (; i < end; ++i)
			{
				CRY_ASSERT_MESSAGE(i < args.GetArgCount(), "UIEvent received wrong number of arguments!");
				ActivateDynOutput(i < args.GetArgCount() ? args.GetArg(i) : TUIData(string("")), m_eventDesc.InputParams.Params[i], pActInfo, i + 1);
			}
			if (m_eventDesc.InputParams.IsDynamic)
			{
				string val;
				SUIArguments dynarg;
				for (; i < args.GetArgCount(); ++i)
				{
					if (args.GetArg(i, val))
						dynarg.AddArgument(val);
				}
				ActivateOutput(pActInfo, end + 1, string(dynarg.GetAsString()));
			}
			ActivateOutput(pActInfo, eO_OnEvent, true);
		}
		m_events.pop();
	}
}
