// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIElementNodes.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIElementNodes.h"
#include "FlashUIElement.h"
#include "FlashUI.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIVariableBaseNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateVariablesPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Set",     "Set value"),
		InputPortConfig_Void("Get",     "Get value"),
		InputPortConfig_AnyType("Value","Value to set"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SInputPortConfig in_config_tmpl[] = {
		CreateVariablesForTmplPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("Set",     "Set value"),
		InputPortConfig_Void("Get",     "Get value"),
		InputPortConfig_AnyType("Value","Value to set"),
		InputPortConfig_Void(0),
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",    "On set value"),
		OutputPortConfig_AnyType("Value", "Value"),
		{ 0 }
	};

	config.pInputPorts = IsTemplate() ? in_config_tmpl : in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Access to Variables";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIVariableBaseNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIVariable)), pActInfo, m_isTemplate);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, GetInputPort(eI_UIVariable)))
		{
			UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIVariable)), pActInfo, m_isTemplate);
		}

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, GetInputPort(eI_TemplateInstanceName)), pActInfo))
			return;

		const int instanceId = GetPortInt(pActInfo, GetInputPort(eI_InstanceID));
		if (IsPortActive(pActInfo, GetInputPort(eI_Set)))
		{
			const TFlowInputData& data = GetPortAny(pActInfo, GetInputPort(eI_Value));
			TUIData value;
			ConvertToUIData(data, value, pActInfo);

			SPerInstanceCall1<const TUIData&> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIVariableBaseNode::SetVariable), value);

			ActivateOutput(pActInfo, eO_OnSet, true);
		}
		if (IsPortActive(pActInfo, GetInputPort(eI_Get)))
		{
			TUIData out;

			SPerInstanceCall1<TUIData&> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIVariableBaseNode::GetVariable), out, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get Variable for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetElement()->GetName(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}

			string res;
			out.GetValueWithConversion(res);
			ActivateOutput(pActInfo, eO_Value, res);
		}
	}

}

////////////////////////////////////////////////////////////////////////////
void CFlashUIVariableBaseNode::ConvertToUIData(const TFlowInputData& in, TUIData& out, SActivationInfo* pActInfo)
{
	if (!GetObjectDesc())
	{
		UIACTION_WARNING("FG: No valid Variable \"%s\"! (Referenced at node \"%s\")", GetPortString(pActInfo, GetInputPort(eI_UIVariable)).c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
		return;
	}

	bool ok = false;
	const char* vartype = "any";
	switch (GetObjectDesc()->eType)
	{
	case SUIParameterDesc::eUIPT_Bool:
		{
			bool value;
			ok = in.GetValueWithConversion(value);
			out = TUIData(value);
			vartype = "bool";
		}
		break;
	case SUIParameterDesc::eUIPT_Int:
		{
			int value;
			ok = in.GetValueWithConversion(value);
			out = TUIData(value);
			vartype = "int";
		}
		break;
	case SUIParameterDesc::eUIPT_Float:
		{
			float value;
			ok = in.GetValueWithConversion(value);
			out = TUIData(value);
			vartype = "float";
		}
		break;
	case SUIParameterDesc::eUIPT_String: // fall through, just change the type desc
		vartype = "string";
	case SUIParameterDesc::eUIPT_Any:
	default:
		{
			string value;
			ok = in.GetValueWithConversion(value);
			out = TUIData(value);
		}
		break;
	}

	if (!ok)
	{
		UIACTION_WARNING("FG: UIElement \"%s\" Variable \"%s\" expected type \"%s\" given value was not compatible! (Referenced at node \"%s\")", GetElement()->GetName(), GetObjectDesc()->sDisplayName, vartype, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIArrayBaseNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateArraysPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Set",     "Set value"),
		InputPortConfig_Void("Get",     "Get value"),
		InputPortConfig<string>("Value","Value to set as string list"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SInputPortConfig in_config_tmpl[] = {
		CreateArraysForTmplPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("Set",     "Set value"),
		InputPortConfig_Void("Get",     "Get value"),
		InputPortConfig<string>("Value","Value to set as string list"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",    "On set value"),
		OutputPortConfig<string>("Value", "Value as string list"),
		{ 0 }
	};

	config.pInputPorts = IsTemplate() ? in_config_tmpl : in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Access to Arrays";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIArrayBaseNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIArray)), pActInfo, m_isTemplate);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, GetInputPort(eI_UIArray)))
		{
			UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIArray)), pActInfo, m_isTemplate);
		}

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, GetInputPort(eI_TemplateInstanceName)), pActInfo))
			return;

		if (GetElement())
		{
			const int instanceId = GetPortInt(pActInfo, GetInputPort(eI_InstanceID));
			if (IsPortActive(pActInfo, GetInputPort(eI_Set)))
			{
				SUIArguments values(GetPortString(pActInfo, GetInputPort(eI_Value)).c_str());

				SPerInstanceCall1<const SUIArguments&> caller;
				caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIArrayBaseNode::SetArray), values);

				ActivateOutput(pActInfo, eO_OnSet, true);
			}
			else if (IsPortActive(pActInfo, GetInputPort(eI_Get)))
			{
				SUIArguments out;

				SPerInstanceCall1<SUIArguments&> caller;
				if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIArrayBaseNode::GetArray), out, false))
				{
					UIACTION_WARNING("FG: UIElement \"%s\" called get Array for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetElement()->GetName(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				}

				ActivateOutput(pActInfo, eO_Value, string(out.GetAsString()));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("UI:Variable:Var", CFlashUIVariableNode);
REGISTER_FLOW_NODE("UI:Variable:Array", CFlashUIArrayNode);
REGISTER_FLOW_NODE("UI:Template:Variable:Var", CFlashUITmplVariableNode);
REGISTER_FLOW_NODE("UI:Template:Variable:Array", CFlashUITmplArrayNode);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIGotoAndPlayBaseNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateMovieClipsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("GotoAndPlay", "GotoAndPlay to frame"),
		InputPortConfig_Void("GotoAndStop", "GotoAndStop to frame"),
		InputPortConfig<int>("FrameId",     -1,                                              "Frame Number, -1 = use FrameName"),
		InputPortConfig<string>("FrameName","FrameName, only used if FrameId is set to -1"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SInputPortConfig in_config_tmpl[] = {
		CreateMovieClipsForTmplPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("GotoAndPlay", "GotoAndPlay to frame"),
		InputPortConfig_Void("GotoAndStop", "GotoAndStop to frame"),
		InputPortConfig<int>("FrameId",     -1,                                              "Frame Number, -1 = use FrameName"),
		InputPortConfig<string>("FrameName","FrameName, only used if FrameId is set to -1"),
		InputPortConfig_Void(0),
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnGotoAndPlay", "On GotoAndPlay"),
		OutputPortConfig_Void("OnGotoAndStop", "On GotoAndStop"),
		{ 0 }
	};

	config.pInputPorts = IsTemplate() ? in_config_tmpl : in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Access to MovieClips";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIGotoAndPlayBaseNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, GetInputPort(eI_UIMovieClip)))
		{
			UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
		}

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, GetInputPort(eI_TemplateInstanceName)), pActInfo))
			return;

		if (GetElement())
		{
			const int frameId = GetPortInt(pActInfo, GetInputPort(eI_FrameNum));
			const string frameName = GetPortString(pActInfo, GetInputPort(eI_FrameName));
			const int instanceId = GetPortInt(pActInfo, GetInputPort(eI_InstanceID));
			if (IsPortActive(pActInfo, GetInputPort(eI_GotoAndPlay)))
			{
				SPerInstanceCall2<int, const char*> caller;
				caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIGotoAndPlayBaseNode::GotoAndPlay), frameId, frameName);

				ActivateOutput(pActInfo, eO_OnGotoAndPlay, true);
			}
			else if (IsPortActive(pActInfo, GetInputPort(eI_GotoAndStop)))
			{
				SPerInstanceCall2<int, const char*> caller;
				caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIGotoAndPlayBaseNode::GotoAndStop), frameId, frameName);

				ActivateOutput(pActInfo, eO_OnGotoAndStop, true);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIGotoAndPlayBaseNode::GotoAndPlay(IUIElement* pElement, int frameId, const char* frameName)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		if (frameId > -1)
			pMc->GotoAndPlay(frameId);
		else
			pMc->GotoAndPlay(frameName);
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIGotoAndPlayBaseNode::GotoAndStop(IUIElement* pElement, int frameId, const char* frameName)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		if (frameId > -1)
			pMc->GotoAndStop(frameId);
		else
			pMc->GotoAndStop(frameName);
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIMCVisibleBaseNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateMovieClipsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Set",     "Set visible/alpha state"),
		InputPortConfig_Void("Get",     "Get visible/alpha state"),
		InputPortConfig<bool>("Visible",true,                       "Visible true/false"),
		InputPortConfig<float>("Alpha", "Alpha (0-1)"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SInputPortConfig in_config_tmpl[] = {
		CreateMovieClipsForTmplPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("Set",     "Set visible/alpha state"),
		InputPortConfig_Void("Get",     "Get visible/alpha state"),
		InputPortConfig<bool>("Visible",true,                       "Visible true/false"),
		InputPortConfig<float>("Alpha", "Alpha (0-1)"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",      "On set visible/alpha state"),
		OutputPortConfig<bool>("IsVisible", "Visible true/false"),
		OutputPortConfig<float>("AlphaVal", "Current Alpha value (0-1)"),
		{ 0 }
	};

	config.pInputPorts = IsTemplate() ? in_config_tmpl : in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Visible/Alpha access to MovieClips";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCVisibleBaseNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, GetInputPort(eI_UIMovieClip)))
		{
			UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
		}

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, GetInputPort(eI_TemplateInstanceName)), pActInfo))
			return;

		if (GetElement())
		{
			const int instanceId = GetPortInt(pActInfo, GetInputPort(eI_InstanceID));
			if (IsPortActive(pActInfo, GetInputPort(eI_Set)))
			{
				const bool bVis = GetPortBool(pActInfo, GetInputPort(eI_Visible));
				const float alpha = clamp_tpl(GetPortFloat(pActInfo, GetInputPort(eI_Alpha)), 0.0f, 1.f);

				SPerInstanceCall2<bool, float> caller;
				caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCVisibleBaseNode::SetValues), bVis, alpha);

				ActivateOutput(pActInfo, eO_OnSet, true);
				ActivateOutput(pActInfo, eO_Visible, bVis);
				ActivateOutput(pActInfo, eO_Alpha, alpha);
			}
			else if (IsPortActive(pActInfo, GetInputPort(eI_Get)))
			{
				bool bVis = false;
				float alpha = 0.f;

				SPerInstanceCall2<bool&, float&> caller;
				if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCVisibleBaseNode::GetValues), bVis, alpha, false))
				{
					UIACTION_WARNING("FG: UIElement \"%s\" called get Array for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetElement()->GetName(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				}

				ActivateOutput(pActInfo, eO_Visible, bVis);
				ActivateOutput(pActInfo, eO_Alpha, alpha);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCVisibleBaseNode::SetValues(IUIElement* pElement, bool bVisible, float alpha)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		SFlashDisplayInfo info;
		pMc->GetDisplayInfo(info);
		info.SetVisible(bVisible);
		info.SetAlpha(alpha * 100.f);
		pMc->SetDisplayInfo(info);
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCVisibleBaseNode::GetValues(IUIElement* pElement, bool& bVisible, float& alpha)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		SFlashDisplayInfo info;
		pMc->GetDisplayInfo(info);
		bVisible = info.GetVisible();
		alpha = info.GetAlpha() / 100.f;
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIMCPosRotScaleBaseNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateMovieClipsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Set",   "Set visible/alpha state"),
		InputPortConfig_Void("Get",   "Get visible/alpha state"),
		InputPortConfig<Vec3>("Pos",  Vec3(0,                     0,  0), "Position"),
		InputPortConfig<Vec3>("Rot",  Vec3(0,                     0,  0), "Rotation"),
		InputPortConfig<Vec3>("Scale",Vec3(1,                     1,  1), "Scale"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SInputPortConfig in_config_tmpl[] = {
		CreateMovieClipsForTmplPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("Set",   "Set visible/alpha state"),
		InputPortConfig_Void("Get",   "Get visible/alpha state"),
		InputPortConfig<Vec3>("Pos",  Vec3(0,                     0,  0), "Position"),
		InputPortConfig<Vec3>("Rot",  Vec3(0,                     0,  0), "Rotation"),
		InputPortConfig<Vec3>("Scale",Vec3(1,                     1,  1), "Scale"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",  "On set visible/alpha state"),
		OutputPortConfig<Vec3>("Pos",   "Position"),
		OutputPortConfig<Vec3>("Rot",   "Rotation"),
		OutputPortConfig<Vec3>("Scale", "Scale"),
		{ 0 }
	};

	config.pInputPorts = IsTemplate() ? in_config_tmpl : in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Pos/Rot/Scale access to MovieClips";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCPosRotScaleBaseNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, GetInputPort(eI_UIMovieClip)))
		{
			UpdateObjectDesc(GetPortString(pActInfo, GetInputPort(eI_UIMovieClip)), pActInfo, m_isTemplate);
		}

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, GetInputPort(eI_TemplateInstanceName)), pActInfo))
			return;

		if (GetElement())
		{
			const int instanceId = GetPortInt(pActInfo, GetInputPort(eI_InstanceID));
			if (IsPortActive(pActInfo, GetInputPort(eI_Set)))
			{
				const Vec3 pos = GetPortVec3(pActInfo, GetInputPort(eI_Pos));
				const Vec3 rot = GetPortVec3(pActInfo, GetInputPort(eI_Rot));
				const Vec3 scale = GetPortVec3(pActInfo, GetInputPort(eI_Scale));

				SPerInstanceCall3<const Vec3&, const Vec3&, const Vec3&> caller;
				caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCPosRotScaleBaseNode::SetValues), pos, rot, scale);

				ActivateOutput(pActInfo, eO_OnSet, true);
				ActivateOutput(pActInfo, eO_Pos, pos);
				ActivateOutput(pActInfo, eO_Rot, rot);
				ActivateOutput(pActInfo, eO_Scale, scale);
			}
			else if (IsPortActive(pActInfo, GetInputPort(eI_Get)))
			{
				Vec3 pos;
				Vec3 rot;
				Vec3 scale;

				SPerInstanceCall3<Vec3&, Vec3&, Vec3&> caller;
				if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCPosRotScaleBaseNode::GetValues), pos, rot, scale, false))
				{
					UIACTION_WARNING("FG: UIElement \"%s\" called get PosRotScale for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetElement()->GetName(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				}

				ActivateOutput(pActInfo, eO_Pos, pos);
				ActivateOutput(pActInfo, eO_Rot, rot);
				ActivateOutput(pActInfo, eO_Scale, scale);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCPosRotScaleBaseNode::SetValues(IUIElement* pElement, const Vec3& vPos, const Vec3& vRot, const Vec3& vScale)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		SFlashDisplayInfo info;
		pMc->GetDisplayInfo(info);

		info.SetX(vPos.x);
		info.SetY(vPos.y);
		info.SetZ(vPos.z);

		info.SetRotation(vRot.z);
		info.SetXRotation(vRot.x);
		info.SetYRotation(vRot.y);

		info.SetXScale(vScale.x * 100.f);
		info.SetYScale(vScale.y * 100.f);
		info.SetZScale(vScale.z * 100.f);

		pMc->SetDisplayInfo(info);
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCPosRotScaleBaseNode::GetValues(IUIElement* pElement, Vec3& vPos, Vec3& vRot, Vec3& vScale)
{
	IFlashVariableObject* pMc = pElement->GetMovieClip(GetObjectDesc(), GetTmplDesc());
	if (pMc)
	{
		SFlashDisplayInfo info;
		pMc->GetDisplayInfo(info);

		vPos.x = info.GetX();
		vPos.y = info.GetY();
		vPos.z = info.GetZ();

		vRot.x = info.GetXRotation();
		vRot.y = info.GetYRotation();
		vRot.z = info.GetRotation();

		vScale.x = info.GetXScale() / 100.f;
		vScale.y = info.GetYScale() / 100.f;
		vScale.z = info.GetZScale() / 100.f;
	}
}

////////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("UI:MovieClip:GotoAndPlay", CFlashUIGotoAndPlayNode);
REGISTER_FLOW_NODE("UI:MovieClip:Visible", CFlashUIMCVisibleNode);
REGISTER_FLOW_NODE("UI:MovieClip:PosRotScale", CFlashUIMCPosRotScaleNode);
REGISTER_FLOW_NODE("UI:Template:MovieClip:GotoAndPlay", CFlashUIGotoAndPlayTmplNode);
REGISTER_FLOW_NODE("UI:Template:MovieClip:Visible", CFlashUIMCVisibleTmplNode);
REGISTER_FLOW_NODE("UI:Template:MovieClip:PosRotScale", CFlashUIMCPosRotScaleTmplNode);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIMCTemplateCreateNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CFlashUIBaseNode::CreateMovieClipTmplPort(),
		CFlashUIBaseNode::CreateMovieClipsParentPort(),
		CFlashUIBaseNode::CreateInstanceIdPort(),
		InputPortConfig<string>("InstanceName",        "New instance name for this MC (if empty it will auto create a new name)"),
		InputPortConfig_Void("Create",                 "Creates a new MovieClip"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnCreated",       "Triggered once the MovieClip was removed"),
		OutputPortConfig<string>("InstanceName", "The new name of the instance"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Create a MovieClip and attaches it to the given Parent MovieClip";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCTemplateCreateNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		m_TmplDescHelper.UpdateObjectDesc(GetPortString(pActInfo, eI_UIMovieClipTmpl), pActInfo, true);
		m_ParentDescHelper.UpdateObjectDesc(GetPortString(pActInfo, eI_UIParentMovieClip), pActInfo, false);

		if (GetParentDesc() && m_TmplDescHelper.GetElement() != m_ParentDescHelper.GetElement())
		{
			UIACTION_WARNING("FG: Can't attach \"%s\" on parent MC \"%s\" (they must be part of the same UIElement!), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIMovieClipTmpl).c_str(), GetPortString(pActInfo, eI_UIParentMovieClip).c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			m_ParentDescHelper.Reset();
		}
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, eI_UIMovieClipTmpl))
		{
			m_TmplDescHelper.UpdateObjectDesc(GetPortString(pActInfo, eI_UIMovieClipTmpl), pActInfo, true);
		}

		IUIElement* pElement = GetElement();
		if (IsPortActive(pActInfo, eI_UIParentMovieClip))
		{
			m_ParentDescHelper.UpdateObjectDesc(GetPortString(pActInfo, eI_UIParentMovieClip), pActInfo, false);

			if (GetParentDesc() && m_TmplDescHelper.GetElement() != m_ParentDescHelper.GetElement())
			{
				UIACTION_WARNING("FG: Can't attach \"%s\" on parent MC \"%s\" (they must be part of the same UIElement!), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIMovieClipTmpl).c_str(), GetPortString(pActInfo, eI_UIParentMovieClip).c_str(), pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
				m_ParentDescHelper.Reset();
			}
		}

		// if m_pCurrentParen was null we also allow to pass a template name!
		if (GetParentDesc() == NULL)
		{
			if (!m_ParentDescHelper.UpdateTmplDesc(GetPortString(pActInfo, eI_UIParentMovieClip), pActInfo))
				return;
		}

		if (GetElement() && IsPortActive(pActInfo, eI_Create))
		{
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);
			string name = GetPortString(pActInfo, eI_NewInstanceName);

			SPerInstanceCall3<const SUIMovieClipDesc*, const SUIMovieClipDesc*, string&> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCTemplateCreateNode::CreateMoviclip), GetTemplateDesc(), GetParentDesc(), name, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get CreateMovieClip for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetElement()->GetName(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}

			ActivateOutput(pActInfo, eO_InstanceName, name);
			ActivateOutput(pActInfo, eO_OnCreate, true);
		}
	}

}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCTemplateCreateNode::CreateMoviclip(IUIElement* pElement, const SUIMovieClipDesc* pTemplate, const SUIMovieClipDesc* pParent, string& newname)
{
	const SUIMovieClipDesc* pNewMC = NULL;
	pElement->CreateMovieClip(pNewMC, pTemplate, pParent, newname.length() > 0 ? newname.c_str() : NULL);
	if (pNewMC)
	{
		newname = pNewMC->sDisplayName;
	}
	else
	{
		UIACTION_WARNING("FG: UIElement \"%s\" could not create MovieClip with name \"%s\" (MovieClip with same name already exists!)", pElement->GetName(), newname.c_str());
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CFlashUIMCTemplateRemoveNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		CreateTmplInstanceNamePort(),
		InputPortConfig_Void("Remove","Removes the given MovieClip"),
		InputPortConfig_Void(0),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnRemove", "Triggered once the MovieClip was removed"),
		{ 0 }
	};

	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.sDescription = "Removes a MovieClip";
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIMCTemplateRemoveNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Initialize)
	{
		UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
	}
	else if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, eI_UIElement))
		{
			UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
		}

		if (!UpdateTmplDesc(GetPortString(pActInfo, eI_TemplateInstanceName), pActInfo))
			return;

		if (GetElement() && IsPortActive(pActInfo, eI_Remove))
		{
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

			SPerInstanceCall1<const SUIMovieClipDesc*> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIMCTemplateRemoveNode::RemoveMoviclip), GetTmplDesc(true), false);

			ActivateOutput(pActInfo, eO_OnRemove, true);
		}
	}

}

////////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("UI:Template:CreateMovieClip", CFlashUIMCTemplateCreateNode);
REGISTER_FLOW_NODE("UI:Template:RemoveMovieClip", CFlashUIMCTemplateRemoveNode);

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
CFlashUIEventNode::CFlashUIEventNode(IUIElement* pUIElement, string sCategory, const SUIEventDesc* pEventDesc)
	: CFlashUIBaseElementNode(pUIElement, sCategory)
	, m_iCurrInstanceId(-1)
{
	CRY_ASSERT_MESSAGE(pEventDesc, "NULL pointer passed!");
	m_eventDesc = *pEventDesc;

	//inputs
	m_inPorts.push_back(CFlashUIBaseNode::CreateInstanceIdPort());
	AddCheckPorts(m_eventDesc.InputParams, m_inPorts);
	m_inPorts.push_back(InputPortConfig_Void(NULL));

	// outputs
	m_outPorts.push_back(OutputPortConfig_Void("onEvent", "Event trigger"));
	m_outPorts.push_back(OutputPortConfig<int>("instanceID", "The instance ID of the element that raised the event"));
	AddParamOutputPorts(m_eventDesc.InputParams, m_outPorts);
	if (m_eventDesc.InputParams.IsDynamic)
		m_outPorts.push_back(OutputPortConfig<string>("Array", m_eventDesc.InputParams.sDynamicDesc, m_eventDesc.InputParams.sDynamicName));
	m_outPorts.push_back(OutputPortConfig_Void(NULL));
}

////////////////////////////////////////////////////////////////////////////
IFlowNodePtr CFlashUIEventNode::Clone(SActivationInfo* pActInfo)
{
	return IFlowNodePtr(new CFlashUIEventNode(m_pElement, GetCategory(), &m_eventDesc));
}

////////////////////////////////////////////////////////////////////////////
CFlashUIEventNode::~CFlashUIEventNode()
{
	m_events.clear();
	ClearListener();
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::GetConfiguration(SFlowNodeConfig& config)
{
	config.pInputPorts = &m_inPorts[0];
	config.pOutputPorts = &m_outPorts[0];
	config.sDescription = m_eventDesc.sDesc;
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			m_events.clear();
			m_events.init(pActInfo->pGraph);
			ClearListener();
			m_iCurrInstanceId = GetPortInt(pActInfo, eI_InstanceID);
			if (m_iCurrInstanceId < -1)
				m_iCurrInstanceId = -1;

			SPerInstanceCall0 caller;
			caller.Execute(m_pElement, m_iCurrInstanceId, functor(*this, &CFlashUIEventNode::RegisterAsListener));
		}
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, eI_InstanceID))
		{
			m_iCurrInstanceId = GetPortInt(pActInfo, eI_InstanceID);
			if (m_iCurrInstanceId < -1)
				m_iCurrInstanceId = -1;
			ClearListener();

			SPerInstanceCall0 caller;
			caller.Execute(m_pElement, m_iCurrInstanceId, functor(*this, &CFlashUIEventNode::RegisterAsListener));
		}
		break;
	case eFE_Update:
		FlushNextEvent(pActInfo);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance)
{
	if (m_iCurrInstanceId < 0)
		RegisterAsListener(pNewInstance);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::OnUIEvent(IUIElement* pSender, const SUIEventDesc& event, const SUIArguments& args)
{
	if (m_eventDesc == event)
	{
		UI_STACK_PUSH(m_events, std::make_pair(args, pSender->GetInstanceID()), "OnFlashEvent %s@%i %s (%s)", pSender->GetName(), pSender->GetInstanceID(), event.sDisplayName, args.GetAsString());
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::ClearListener()
{
	IUIElementIteratorPtr elements = m_pElement->GetInstances();
	while (IUIElement* pElement = elements->Next())
		pElement->RemoveEventListener(this);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIEventNode::FlushNextEvent(SActivationInfo* pActInfo)
{
	if (m_events.size() > 0)
	{
		const std::pair<SUIArguments, int>& data = m_events.get();
		const SUIArguments& args = data.first;
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
			string val;

			int i = 0;
			for (; i < end; ++i)
			{
				CRY_ASSERT_MESSAGE(i < args.GetArgCount(), "UIEvent received wrong number of arguments!");
				ActivateDynOutput(i < args.GetArgCount() ? args.GetArg(i) : TUIData(string("")), m_eventDesc.InputParams.Params[i], pActInfo, i + 2);
			}
			if (m_eventDesc.InputParams.IsDynamic)
			{
				SUIArguments dynarg;
				for (; i < args.GetArgCount(); ++i)
				{
					if (args.GetArg(i, val))
						dynarg.AddArgument(val);
				}
				ActivateOutput(pActInfo, end + eO_DynamicPorts, string(dynarg.GetAsString()));
			}
			ActivateOutput(pActInfo, eO_OnEvent, true);
			ActivateOutput(pActInfo, eO_OnInstanceId, data.second);
		}
		m_events.pop();
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
CFlashUIFunctionNode::CFlashUIFunctionNode(IUIElement* pUIElement, string sCategory, const SUIEventDesc* pFuncDesc, bool isTemplate)
	: CFlashUIBaseElementNode(pUIElement, sCategory)
	, m_pTmplDesc(NULL)
	, m_isTemplate(isTemplate)
{
	CRY_ASSERT_MESSAGE(pFuncDesc, "NULL pointer passed!");
	m_funcDesc = *pFuncDesc;

	m_inPorts.push_back(InputPortConfig_Void("Call", "Calls the function"));
	m_inPorts.push_back(CFlashUIBaseNode::CreateInstanceIdPort());
	if (isTemplate)
		m_inPorts.push_back(CFlashUIBaseNode::CreateTmplInstanceNamePort());

	AddParamInputPorts(m_funcDesc.InputParams, m_inPorts);

	m_inPorts.push_back(InputPortConfig_Void(NULL));

}

////////////////////////////////////////////////////////////////////////////
IFlowNodePtr CFlashUIFunctionNode::Clone(SActivationInfo* pActInfo)
{
	return IFlowNodePtr(new CFlashUIFunctionNode(m_pElement, GetCategory(), &m_funcDesc, m_isTemplate));
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIFunctionNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnCall",       "On call"),
		OutputPortConfig_AnyType("ReturnVal", "Return value of this function"),
		{ 0 }
	};

	config.pInputPorts = &m_inPorts[0];
	config.pOutputPorts = out_config;
	config.sDescription = m_funcDesc.sDesc;
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIFunctionNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate && IsPortActive(pActInfo, eI_Call))
	{
		int port = GetDynStartPort();
		SUIArguments args;
		for (TUIParams::const_iterator iter = m_funcDesc.InputParams.Params.begin(); iter != m_funcDesc.InputParams.Params.end(); ++iter)
			GetDynInput(args, *iter, pActInfo, port++);

		TUIData res;
		res.Set(string());
		const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

		if (IsTemplate() && !UpdateTmplDesc(GetPortString(pActInfo, eI_TemplateInstanceName), pActInfo))
			return;

		SPerInstanceCall2<const SUIArguments&, TUIData&> caller;
		caller.Execute(m_pElement, instanceId, functor(*this, &CFlashUIFunctionNode::CallFunction), args, res);

		string out;
		res.GetValueWithConversion(out);
		ActivateOutput(pActInfo, eO_RetVal, out);

		ActivateOutput(pActInfo, eO_OnCall, true);
	}
}

////////////////////////////////////////////////////////////////////////////
CFlashUIFunctionNode::~CFlashUIFunctionNode()
{
	// Note: m_funcDesc is invalid if this is called due to a reload in the editor
	// Do not try to access it here (points to old UIElement-memory)
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
CFlashUIElementListenerNode::CFlashUIElementListenerNode(SActivationInfo* pActInfo)
	: m_iCurrInstanceId(-1)
{
}

////////////////////////////////////////////////////////////////////////////
CFlashUIElementListenerNode::~CFlashUIElementListenerNode()
{
	m_events.clear();
	ClearListener();
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("instanceID",          "Instance ID for the element that fired the event"),
		OutputPortConfig_Void("onInit",              "Triggered when flash element was initialized"),
		OutputPortConfig_Void("onUnload",            "Triggered when unloaded"),
		OutputPortConfig_Void("onShow",              "Triggered when displayed"),
		OutputPortConfig_Void("onHide",              "Triggered when hided"),
		OutputPortConfig_Void("onNewInstance",       "Triggered when new instance was created"),
		OutputPortConfig_Void("onInstanceDestroyed", "Triggered when instance was destroyed"),
		{ 0 }
	};

	config.sDescription = "Node receive notifications about display/hide/load/unload of an UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			m_events.clear();
			m_events.init(pActInfo->pGraph);
			ClearListener();
			UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
			m_iCurrInstanceId = GetPortInt(pActInfo, eI_InstanceID);
			if (m_iCurrInstanceId < -1)
				m_iCurrInstanceId = -1;

			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), m_iCurrInstanceId, functor(*this, &CFlashUIElementListenerNode::RegisterAsListener));
		}
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, eI_InstanceID) || IsPortActive(pActInfo, eI_UIElement))
		{
			ClearListener();
			UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
			m_iCurrInstanceId = GetPortInt(pActInfo, eI_InstanceID);
			if (m_iCurrInstanceId < -1)
				m_iCurrInstanceId = -1;

			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), m_iCurrInstanceId, functor(*this, &CFlashUIElementListenerNode::RegisterAsListener));
		}
		break;
	case eFE_Update:
		FlushNextEvent(pActInfo);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::FlushNextEvent(SActivationInfo* pActInfo)
{
	if (m_events.size() > 0)
	{
		const std::pair<EOutputs, int>& data = m_events.get();
		ActivateOutput(pActInfo, eO_InstanceId, data.second);
		ActivateOutput(pActInfo, data.first, true);
		m_events.pop();
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::OnInit(IUIElement* pSender, IFlashPlayer* pFlashPlayer)
{
	UI_STACK_PUSH(m_events, std::make_pair(eO_OnInit, pSender->GetInstanceID()), "OnInit %s@%i", pSender->GetName(), pSender->GetInstanceID());
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::OnUnload(IUIElement* pSender)
{
	UI_STACK_PUSH(m_events, std::make_pair(eO_OnUnload, pSender->GetInstanceID()), "OnUnload %s@%i", pSender->GetName(), pSender->GetInstanceID());
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::OnSetVisible(IUIElement* pSender, bool bVisible)
{
	EOutputs port = bVisible ? eO_OnShow : eO_OnHide;
	UI_STACK_PUSH(m_events, std::make_pair(port, pSender->GetInstanceID()), "OnSetVisible %s@%i %s", pSender->GetName(), pSender->GetInstanceID(), bVisible ? "visible" : "hide");
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance)
{
	RegisterAsListener(pNewInstance);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementListenerNode::ClearListener()
{
	if (GetElement())
	{
		IUIElementIteratorPtr elements = GetElement()->GetInstances();
		while (IUIElement* pElement = elements->Next())
			pElement->RemoveEventListener(this);
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
CFlashUIElementInstanceNode::~CFlashUIElementInstanceNode()
{
	m_events.clear();
	if (GetElement()) GetElement()->RemoveEventListener(this);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementInstanceNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Destroy","Destroy UIElement instance"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig<int>("instanceID",          "Instance ID for the element that fired the event"),
		OutputPortConfig_Void("onNewInstance",       "Triggered when new instance was created"),
		OutputPortConfig_Void("onInstanceDestroyed", "Triggered when instance was destroyed"),
		{ 0 }
	};

	config.sDescription = "Node to delete instances of UIElements and receive notifications about new/destroyed instances of an UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementInstanceNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		m_events.clear();
		m_events.init(pActInfo->pGraph);
		if (GetElement()) GetElement()->RemoveEventListener(this);
		UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
		if (GetElement()) GetElement()->AddEventListener(this, "CFlashUIElementInstanceNode");
		break;
	case eFE_Activate:
		if (IsPortActive(pActInfo, eI_InstanceID) || IsPortActive(pActInfo, eI_UIElement))
		{
			if (GetElement()) GetElement()->RemoveEventListener(this);
			UpdateUIElement(GetPortString(pActInfo, eI_UIElement), pActInfo);
			if (GetElement()) GetElement()->AddEventListener(this, "CFlashUIElementInstanceNode");
		}
		if (IsPortActive(pActInfo, eI_DestroyInstance))
		{
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);
			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIElementInstanceNode::DestroyInstance));
		}
		break;
	case eFE_Update:
		FlushNextEvent(pActInfo);
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementInstanceNode::FlushNextEvent(SActivationInfo* pActInfo)
{
	if (m_events.size() > 0)
	{
		const std::pair<EOutputs, int>& data = m_events.get();
		ActivateOutput(pActInfo, eO_InstanceId, data.second);
		ActivateOutput(pActInfo, data.first, true);
		m_events.pop();
	}
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementInstanceNode::OnInstanceCreated(IUIElement* pSender, IUIElement* pNewInstance)
{
	pNewInstance->AddEventListener(this, "CFlashUIElementInstanceNode");
	UI_STACK_PUSH(m_events, std::make_pair(eO_OnNewInstance, pNewInstance->GetInstanceID()), "OnInstanceCreated %s@%i", pSender->GetName(), pNewInstance->GetInstanceID());
}

////////////////////////////////////////////////////////////////////////////
void CFlashUIElementInstanceNode::OnInstanceDestroyed(IUIElement* pSender, IUIElement* pDeletedInstance)
{
	UI_STACK_PUSH(m_events, std::make_pair(eO_OnInstanceDestroyed, pDeletedInstance->GetInstanceID()), "OnInstanceDestroyed %s@%i", pSender->GetName(), pDeletedInstance->GetInstanceID());
}

////////////////////////////////////////////////////////////////////////////
REGISTER_FLOW_NODE("UI:Display:UIElementListener", CFlashUIElementListenerNode);
REGISTER_FLOW_NODE("UI:Display:UIElementInstance", CFlashUIElementInstanceNode);
