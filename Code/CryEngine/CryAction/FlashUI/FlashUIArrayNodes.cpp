// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIArrayNodes.cpp
//  Version:     v1.00
//  Created:     21/4/2011 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIArrayNodes.h"

#include <CrySystem/Scaleform/IFlashUI.h>

// --------------------------------------------------------------
void CFlashUIToArrayNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Set",     "Set this array"),
		InputPortConfig<int>("Count",   4,                _HELP("Number of elements for this array"),0, _UICONFIG("enum_int:1,2,3,4")),
		InputPortConfig_AnyType("Val1", "First value"),
		InputPortConfig_AnyType("Val2", "Second value"),
		InputPortConfig_AnyType("Val3", "Third value"),
		InputPortConfig_AnyType("Val4", "Fourth value"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",    "On Set this array"),
		OutputPortConfig<string>("Array", UIARGS_DEFAULT_DELIMITER_NAME " separated list"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Create an UI Array";
	config.SetCategory(EFLN_APPROVED);

}

void CFlashUIToArrayNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Set))
	{
		int count = GetPortInt(pActInfo, eI_Count);
		int port = eI_Val1;

		SUIArguments args;
		for (int i = 0; i < count; ++i)
		{
			TFlowInputData data = GetPortAny(pActInfo, port + i);
			string val;
			data.GetValueWithConversion(val);
			args.AddArgument(val);
		}

		ActivateOutput(pActInfo, eO_OnSet, true);
		ActivateOutput(pActInfo, eO_ArgList, string(args.GetAsString()));
	}
}

// --------------------------------------------------------------
void CFlashUIFromArrayNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig<string>("Array", UIARGS_DEFAULT_DELIMITER_NAME " separated string"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("Count",       "Number of elements"),
		OutputPortConfig_AnyType("Val1",     "First value"),
		OutputPortConfig_AnyType("Val2",     "Second value"),
		OutputPortConfig_AnyType("Val3",     "Third value"),
		OutputPortConfig_AnyType("Val4",     "Fourth value"),
		OutputPortConfig<string>("LeftOver", "If Array has more than four elements, this port returns the \"loeftover\" array"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Get Values from UI Array";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIFromArrayNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Array))
	{
		SUIArguments args(GetPortString(pActInfo, eI_Array).c_str());
		ActivateOutput(pActInfo, eO_Count, args.GetArgCount());

		SUIArguments leftOver;
		int port = eO_Val1;

		for (int i = 0; i < args.GetArgCount(); ++i)
		{
			string arg;
			args.GetArg(i, arg);
			if (port + i < eO_LeftOver)
			{
				ActivateOutput(pActInfo, port + i, arg);
			}
			else
			{
				leftOver.AddArgument(arg);
			}
		}
		if (leftOver.GetArgCount() > 0)
		{
			ActivateOutput(pActInfo, eO_LeftOver, string(leftOver.GetAsString()));
		}
	}
}

// --------------------------------------------------------------
void CFlashUIFromArrayExNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Get",      "Get the value at \"Index\""),
		InputPortConfig<string>("Array", UIARGS_DEFAULT_DELIMITER_NAME " separated string"),
		InputPortConfig<int>("Index",    "Index"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_AnyType("Value", "Value of element at \"Index\""),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Get specific Value from UI Array";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIFromArrayExNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Get))
	{
		SUIArguments args(GetPortString(pActInfo, eI_Array).c_str());
		int index = GetPortInt(pActInfo, eI_Index);
		string arg;
		if (args.GetArg(index, arg))
		{
			ActivateOutput(pActInfo, eO_Val, arg);
		}
	}
}

// --------------------------------------------------------------
void CFlashUIArrayConcatNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		InputPortConfig_Void("Merge",     "Merge the arrays"),
		InputPortConfig<string>("Array1", UIARGS_DEFAULT_DELIMITER_NAME " separated string"),
		InputPortConfig<string>("Array2", UIARGS_DEFAULT_DELIMITER_NAME " separated string"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnMerge",  "On merged the arrays"),
		OutputPortConfig<string>("Array", UIARGS_DEFAULT_DELIMITER_NAME " separated list"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Merge two Arrays";
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIArrayConcatNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Set))
	{
		SUIArguments newArray(GetPortString(pActInfo, eI_Arr1).c_str());
		newArray.AddArguments(GetPortString(pActInfo, eI_Arr2).c_str());
		ActivateOutput(pActInfo, eO_OnSet, true);
		ActivateOutput(pActInfo, eO_ArgList, string(newArray.GetAsString()));
	}

}

REGISTER_FLOW_NODE("UI:Util:ToArray", CFlashUIToArrayNode);
REGISTER_FLOW_NODE("UI:Util:FromArray", CFlashUIFromArrayNode);
REGISTER_FLOW_NODE("UI:Util:FromArrayByIndex", CFlashUIFromArrayExNode);
REGISTER_FLOW_NODE("UI:Util:MergeArrays", CFlashUIArrayConcatNode);
