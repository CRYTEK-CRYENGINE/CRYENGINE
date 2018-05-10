// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIBaseNode.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIBaseNode.h"
#include "FlashUI.h"

int CUIFGStackMan::m_Size = 0;
bool CUIFGStackMan::m_bEnabled = true;
#ifdef ENABLE_UISTACK_DEBUGGING
std::map<int, const char*> CUIFGStackMan::m_debugInfo;
#endif

// ---------------------------------------------------------------
// ---------------------------------------------------------------
// ---------------------------------------------------------------
CFlashUIBaseNode::CFlashUIBaseNode()
	: m_pElement(NULL)
{
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateInstanceIdPort()
{
	return InputPortConfig<int>("instanceID", -1, "Instance ID for the element, -1 = all instances (lazy init), -2 = all initialized instances");
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateElementsPort()
{
	return InputPortConfig<string>("uiElements_Element", "", _HELP("Element"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateVariablesPort()
{
	return InputPortConfig<string>("uiVariables_Variable", "", _HELP("Variable"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateArraysPort()
{
	return InputPortConfig<string>("uiArrays_Array", "", _HELP("Array"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateMovieClipsPort()
{
	return InputPortConfig<string>("uiMovieclips_MovieClips", "", _HELP("MovieClip"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateMovieClipsParentPort()
{
	return InputPortConfig<string>("uiMovieclips_Parent", _HELP("Parent MovieClip, can be selected from the list or a dynamic instance name of another dynamic create MC"), "ParentMC");
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateMovieClipTmplPort()
{
	return InputPortConfig<string>("uiTemplates_Template", "", _HELP("MovieClip template"));
}
// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateVariablesForTmplPort()
{
	return InputPortConfig<string>("uiVariablesTmpl_Variable", "", _HELP("Variable"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateArraysForTmplPort()
{
	return InputPortConfig<string>("uiArraysTmpl_Array", "", _HELP("Array"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateMovieClipsForTmplPort()
{
	return InputPortConfig<string>("uiMovieclipsTmpl_MovieClips", "", _HELP("MovieClip"));
}

// ---------------------------------------------------------------
SInputPortConfig CFlashUIBaseNode::CreateTmplInstanceNamePort()
{
	return InputPortConfig<string>("InstanceName", "Instance name of the used template");
}

// ---------------------------------------------------------------
void CFlashUIBaseNode::UpdateUIElement(const string& sName, SActivationInfo* pActInfo)
{
	if (gEnv->pFlashUI)
	{
		m_pElement = gEnv->pFlashUI->GetUIElement(sName);

		if (!m_pElement)
		{
			UIACTION_WARNING("FG: UIElement \"%s\" does not exist, referenced at node \"%s\"", sName.c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
		}
	}
}

// ---------------------------------------------------------------
IUIElement* CFlashUIBaseNode::GetInstance(int instanceID)
{
	if (instanceID < 0)
		instanceID = 0;

	if (m_pElement)
	{
		return m_pElement->GetInstance((uint) instanceID);
	}

	return NULL;
}

// ---------------------------------------------------------------
// ---------------------------------------------------------------
// ---------------------------------------------------------------
void CFlashUIBaseNodeDynPorts::AddParamInputPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SInputPortConfig>& ports)
{
	for (TUIParams::const_iterator iter = eventDesc.Params.begin(); iter != eventDesc.Params.end(); ++iter)
	{
		switch (iter->eType)
		{
		case SUIParameterDesc::eUIPT_Bool:
			ports.push_back(InputPortConfig<bool>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_Int:
			ports.push_back(InputPortConfig<int>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_Float:
			ports.push_back(InputPortConfig<float>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_Vec3:
			ports.push_back(InputPortConfig<Vec3>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_String:
			ports.push_back(InputPortConfig<string>(iter->sDisplayName, iter->sDesc));
			break;
		default:
			ports.push_back(InputPortConfig_AnyType(iter->sDisplayName, iter->sDesc));
			break;
		}
	}
}

// ---------------------------------------------------------------
void CFlashUIBaseNodeDynPorts::AddParamOutputPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SOutputPortConfig>& ports)
{
	for (TUIParams::const_iterator iter = eventDesc.Params.begin(); iter != eventDesc.Params.end(); ++iter)
	{
		switch (iter->eType)
		{
		case SUIParameterDesc::eUIPT_Bool:
			ports.push_back(OutputPortConfig<bool>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_Int:
			ports.push_back(OutputPortConfig<int>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_Float:
			ports.push_back(OutputPortConfig<float>(iter->sDisplayName, iter->sDesc));
			break;
		case SUIParameterDesc::eUIPT_String:
		case SUIParameterDesc::eUIPT_WString:
			ports.push_back(OutputPortConfig<string>(iter->sDisplayName, iter->sDesc));
			break;
		default:
			ports.push_back(OutputPortConfig_AnyType(iter->sDisplayName, iter->sDesc));
			break;
		}
	}
}

// ---------------------------------------------------------------
void CFlashUIBaseNodeDynPorts::AddCheckPorts(const SUIEventDesc::SEvtParams& eventDesc, std::vector<SInputPortConfig>& ports)
{
	m_enumStr = "enum_int:<TriggerAlways>=-1";
	int idx = 0;
	for (TUIParams::const_iterator iter = eventDesc.Params.begin(); iter != eventDesc.Params.end(); ++iter, ++idx)
	{
		m_enumStr += ",";
		m_enumStr += iter->sDisplayName;
		m_enumStr += "=";
		m_enumStr += idx;
	}
	ports.push_back(InputPortConfig<int>("Port", -1, _HELP("If a port is selected, the node will only trigger if the selected \"Port\" has \"Idx\" value"), 0, _UICONFIG(m_enumStr.c_str())));
	ports.push_back(InputPortConfig<string>("Idx", "Value that is compated with current value of output on selected \"Port\""));
}

// ---------------------------------------------------------------
void CFlashUIBaseNodeDynPorts::GetDynInput(SUIArguments& args, const SUIParameterDesc& desc, SActivationInfo* pActInfo, int port)
{
	switch (desc.eType)
	{
	case SUIParameterDesc::eUIPT_Bool:
		args.AddArgument(GetPortBool(pActInfo, port));
		break;
	case SUIParameterDesc::eUIPT_Int:
		args.AddArgument(GetPortInt(pActInfo, port));
		break;
	case SUIParameterDesc::eUIPT_Float:
		args.AddArgument(GetPortFloat(pActInfo, port));
		break;
	case SUIParameterDesc::eUIPT_String:
		args.AddArgument(GetPortString(pActInfo, port));
		break;
	case SUIParameterDesc::eUIPT_WString:
		args.AddArgument(GetPortString(pActInfo, port), eUIDT_WString);
		break;
	case SUIParameterDesc::eUIPT_Vec3:
		args.AddArgument(GetPortVec3(pActInfo, port));
		break;
	default:
		{
			string val;
			const TFlowInputData data = GetPortAny(pActInfo, port);
			data.GetValueWithConversion(val);
			args.AddArgument(val, eUIDT_Any);
		}
		break;
	}
}

// ---------------------------------------------------------------
void CFlashUIBaseNodeDynPorts::ActivateDynOutput(const TUIData& arg, const SUIParameterDesc& desc, SActivationInfo* pActInfo, int port)
{
	switch (desc.eType)
	{
	case SUIParameterDesc::eUIPT_Bool:
		{
			bool val;
			const bool ok = arg.GetValueWithConversion(val);
			if (!ok) UIACTION_WARNING("FG: Node \"%s\" received wrong data on typed input!", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			ActivateOutput(pActInfo, port, val);
		}
		break;
	case SUIParameterDesc::eUIPT_Int:
		{
			int val;
			const bool ok = arg.GetValueWithConversion(val);
			if (!ok) UIACTION_WARNING("FG: Node \"%s\" received wrong data on typed input!", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			ActivateOutput(pActInfo, port, val);
		}
		break;
	case SUIParameterDesc::eUIPT_Float:
		{
			float val;
			const bool ok = arg.GetValueWithConversion(val);
			if (!ok) UIACTION_WARNING("FG: Node \"%s\" received wrong data on typed input!", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			ActivateOutput(pActInfo, port, val);
		}
		break;
	case SUIParameterDesc::eUIPT_String:
	case SUIParameterDesc::eUIPT_WString:
	default:
		{
			string val;
			const bool ok = arg.GetValueWithConversion(val);
			if (!ok) UIACTION_WARNING("FG: Node \"%s\" received wrong data on typed input!", pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			ActivateOutput(pActInfo, port, val);
		}
		break;
	}
}
