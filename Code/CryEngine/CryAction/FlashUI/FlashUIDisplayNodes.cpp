// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIDisplayNodes.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIDisplayNodes.h"
#include "FlashUI.h"

// --------------------------------------------------------------
void CFlashUIDisplayNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("show",              "Display UI element"),
		InputPortConfig_Void("hide",              "Hide UI element"),
		InputPortConfig_Void("unload",            "Unload UI element"),
		InputPortConfig_Void("requestHide",       "Send request for hide to flash asset, to allow fade out"),
		InputPortConfig_Void("init",              "Inits the flash file"),
		InputPortConfig_Void("reload",            "Reloads the flash file"),
		InputPortConfig_Void("unloadBootstrapper","Unloads the bootstrapper for this UI Element. Will unload all Instances of this element"),
		InputPortConfig_Void("reloadBootstrapper","Reloads the bootstrapper for this UI Element. Will reload all Instances of this element"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("onShow",               "Triggered when display was called"),
		OutputPortConfig_Void("onHide",               "Triggered when hide was called"),
		OutputPortConfig_Void("onUnload",             "Triggered when unload was called"),
		OutputPortConfig_Void("onRequestHide",        "Triggered when hide was requested"),
		OutputPortConfig_Void("onInit",               "Triggered when init was called"),
		OutputPortConfig_Void("onReload",             "Triggered when reload was called"),
		OutputPortConfig_Void("onUnloadBootstrapper", "Triggered when unload bootstrapper was called"),
		OutputPortConfig_Void("onReloadBootstrapper", "Triggered when reload bootstrapper was called"),
		{ 0 }
	};

	config.sDescription = "Node to display/hide/reload UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIDisplayNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

		if (IsPortActive(pActInfo, eI_Show))
		{
			SPerInstanceCall1<bool> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::SetVisible), true);
			ActivateOutput(pActInfo, eO_OnShow, true);
		}

		if (IsPortActive(pActInfo, eI_Hide))
		{
			SPerInstanceCall1<bool> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::SetVisible), false);
			ActivateOutput(pActInfo, eO_OnHide, true);
		}

		if (IsPortActive(pActInfo, eI_Unload))
		{
			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::Unload));
			ActivateOutput(pActInfo, eO_OnUnload, true);
		}

		if (IsPortActive(pActInfo, eI_RequestHide))
		{
			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::RequestHide));
			ActivateOutput(pActInfo, eO_OnRequestHide, true);
		}

		if (IsPortActive(pActInfo, eI_Init))
		{
			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::Init));
			ActivateOutput(pActInfo, eO_OnInit, true);
		}

		if (IsPortActive(pActInfo, eI_Reload))
		{
			SPerInstanceCall0 caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayNode::Reload));
			ActivateOutput(pActInfo, eO_OnReload, true);
		}

		if (IsPortActive(pActInfo, eI_UnloadBootstrapper))
		{
			if (GetElement()) GetElement()->UnloadBootStrapper();
			ActivateOutput(pActInfo, eO_OnUnloadBootstrapper, true);
		}

		if (IsPortActive(pActInfo, eI_ReloadBootstrapper))
		{
			if (GetElement()) GetElement()->ReloadBootStrapper();
			ActivateOutput(pActInfo, eO_OnReloadBootstrapper, true);
		}
	}
}

//------------------------------------------------------------------------------------------------------
void CFlashUIAdvanceNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Advance","Advance the flash player"),
		InputPortConfig<float>("Delta",0,                           "Delta time for advance"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("onAdvance", "Triggered when advance was called"),
		{ 0 }
	};
	config.sDescription = "Node to advance a UIElement";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIAdvanceNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		if (IsPortActive(pActInfo, eI_Advance))
		{
			const float delta = GetPortFloat(pActInfo, eI_Delta);
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

			SPerInstanceCall1<float> caller1;
			caller1.Execute(GetElement(), instanceId, functor(*this, &CFlashUIAdvanceNode::Advance), delta);

			ActivateOutput(pActInfo, eO_OnAdvance, true);
		}
	}
}

void CFlashUIAdvanceNode::Advance(IUIElement* pInstance, float delta)
{
	// update call will not lazy init! so do it here
	if (!pInstance->IsInit())
		pInstance->Init();
	pInstance->Update(delta);
}

//------------------------------------------------------------------------------------------------------
void CFlashUIScreenPosNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Get",            "Trigger the node"),
		InputPortConfig<float>("PX",           0,                   "Input screen x-pos (0-1)"),
		InputPortConfig<float>("PY",           0,                   "Input screen y-pos (0-1)"),
		InputPortConfig<bool>("StageScaleMode",false,               "If flash asset uses stage.scaleMode this must be true,\nthe movieclip must sit in another mc that is manually located in the center of the screen"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("onGet", "Triggered when advance was called"),
		OutputPortConfig<float>("PX",  "Screen x-pos for selected Flash asset"),
		OutputPortConfig<float>("PY",  "Screen y-pos for selected Flash asset"),
		{ 0 }
	};
	config.sDescription = "Node to convert a screen position (Value 0-1) to a actual X,Y position in the flash asset";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIScreenPosNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		if (IsPortActive(pActInfo, eI_Get))
		{
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);
			float px = GetPortFloat(pActInfo, eI_PX);
			float py = GetPortFloat(pActInfo, eI_PY);
			const bool scaleMode = GetPortBool(pActInfo, eI_ScaleMode);

			SPerInstanceCall3<float&, float&, bool> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIScreenPosNode::GetScreenPos), px, py, scaleMode, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get screenpos for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIElement).c_str(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}

			ActivateOutput(pActInfo, eO_OnGet, true);
			ActivateOutput(pActInfo, eO_PX, px);
			ActivateOutput(pActInfo, eO_PY, py);
		}
	}
}

void CFlashUIScreenPosNode::GetScreenPos(IUIElement* pInstance, float& px /*in/out*/, float& py /*in/out*/, bool bScaleMode)
{
	pInstance->ScreenToFlash(px, py, px, py, bScaleMode);
}

//------------------------------------------------------------------------------------------------------
void CFlashUIWorldScreenPosNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("Get",            "Trigger the node"),
		InputPortConfig<Vec3>("WorldPos",      "Input world pos"),
		InputPortConfig<Vec3>("Offset",        "Offset of the world pos"),
		InputPortConfig<bool>("StageScaleMode",false,                      "If flash asset uses stage.scaleMode this must be true,\nthe movieclip must sit in another mc that is manually located in the center of the screen"),
		{ 0 }
	};
	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("onGet",      "Triggered when advance was called"),
		OutputPortConfig<Vec3>("ScreenPos", "Screen in selected selected Flash asset"),
		OutputPortConfig<float>("Scale",    "If you use 2.5D mode this will return scale for your movieclip simulate the depth."),
		OutputPortConfig<bool>("OnScreen",  "True if on screen otherwise false"),
		OutputPortConfig<Vec3>("Border",    "If not on screen x=-1 means left from screen x=1 right from screen, y=-1 top of screen y=1 below the screen"),
		{ 0 }
	};
	config.sDescription = "Node to convert a world position to a actual X,Y,Z and Scale value in the flash asset";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIWorldScreenPosNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		if (IsPortActive(pActInfo, eI_Get))
		{
			const int instanceId = GetPortInt(pActInfo, eI_InstanceID);
			Vec3 worldPos = GetPortVec3(pActInfo, eI_WorldPos);
			Vec3 offset = GetPortVec3(pActInfo, eI_Offset);
			const bool scaleMode = GetPortBool(pActInfo, eI_ScaleMode);

			SPerInstanceCall3<Vec3&, Vec3&, bool> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIWorldScreenPosNode::GetScreenPosFromWorld), worldPos, offset, scaleMode, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get screenpos for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIElement).c_str(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}

			ActivateOutput(pActInfo, eO_ScreenPos, worldPos);
			ActivateOutput(pActInfo, eO_Scale, offset.z);
			ActivateOutput(pActInfo, eO_IsOnScreen, offset.x == 0 && offset.y == 0);
			offset.z = 0;
			ActivateOutput(pActInfo, eO_OverScanBorder, offset);
			ActivateOutput(pActInfo, eO_OnGet, true);
		}
	}
}

void CFlashUIWorldScreenPosNode::GetScreenPosFromWorld(IUIElement* pInstance, Vec3& pos /*in/out*/, Vec3& offset /*in/out*/, bool bScaleMode)
{
	// get current camera matrix
	const CCamera& cam = GetISystem()->GetViewCamera();
	const Matrix34& camMat = cam.GetMatrix();

	// add offset to position
	const Vec3 vFaceingPos = camMat.GetTranslation() - camMat.GetColumn1() * 1000.f;
	const Vec3 vDir = (pos - vFaceingPos).GetNormalizedFast();
	const Vec3 vOffsetX = vDir.Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalizedFast() * offset.x;
	const Vec3 vOffsetY = vDir * offset.y;
	const Vec3 vOffsetZ = Vec3(0, 0, offset.z);
	const Vec3 vOffset = vOffsetX + vOffsetY + vOffsetZ;
	pos += vOffset;

	Vec2 borders;
	float scale;
	Vec3 flashPos;
	pInstance->WorldToFlash(camMat, pos, flashPos, borders, scale, bScaleMode);

	// return flashpos in pos
	pos = flashPos;

	// store overflow in offset x/y and scale in z
	offset.x = borders.x;
	offset.y = borders.y;
	offset.z = scale;
}

//------------------------------------------------------------------------------------------------------
void CFlashUIDisplayConfigNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("get",             "Get configuration"),
		InputPortConfig_Void("set",             "set configuration"),

		InputPortConfig<bool>("cursor",         "Enables mouse cursor"),
		InputPortConfig<bool>("mouseEvents",    "Enables mouse events"),
		InputPortConfig<bool>("keyEvents",      "Enables mouse events"),
		InputPortConfig<bool>("consoleMouse",   "Enables use controller stick as mouse on console (only if \"mouseEvents\" is enabled)"),
		InputPortConfig<bool>("consoleCursor",  "Enables cursor on console (only if \"cursor\" is enabled)"),
		InputPortConfig<bool>("controllerInput","Enables controller input"),
		InputPortConfig<bool>("eventsExclusive","If set to true no other elements will receive events if this element receives them first"),
		InputPortConfig<bool>("fixedProjDepth", "If set to true this element will use pseudo 3D mode. The _z value of each movieclip will only affect its size to give the feeling of \"correct\" depth"),

		InputPortConfig<bool>("forceNoUnload",  "If set to true this element will not be unloaded on level unload (flag will be applied to all instances!)"),

		InputPortConfig<float>("alpha",         "Alpha"),
		InputPortConfig<int>("layer",           "layer of the element"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",              _HELP("Triggered on set")),
		OutputPortConfig_Void("OnGet",              _HELP("Triggered on get")),
		OutputPortConfig<bool>("isVisible",         "Current menu state"),

		OutputPortConfig<bool>("hasCursor",         "Current cursor state"),
		OutputPortConfig<bool>("hasMouseEvents",    "Current mouse event state"),
		OutputPortConfig<bool>("hasKeyEvents",      "Current key event state"),
		OutputPortConfig<bool>("isConsoleMouse",    "Current console mouse state"),
		OutputPortConfig<bool>("isConsoleCursor",   "Current console cursor state"),
		OutputPortConfig<bool>("isControllerInput", "Current controller input state"),
		OutputPortConfig<bool>("isEventsExclusive", "Current event exclusive state"),
		OutputPortConfig<bool>("isFixedProjDepth",  "Current fixedProjDepth state"),

		OutputPortConfig<bool>("isForceNoUnload",   "Current forceNoUnload state"),

		OutputPortConfig<float>("alpha",            "Current alpha value"),
		OutputPortConfig<int>("layer",              "layer of the element"),
		{ 0 }
	};

	config.sDescription = "Node to setup flags for UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIDisplayConfigNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

		if (IsPortActive(pActInfo, eI_Get))
		{
			SPerInstanceCall1<SActivationInfo*> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayConfigNode::GetFromInstance), pActInfo, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get flags for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIElement).c_str(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}
			ActivateOutput(pActInfo, eO_OnGet, true);
		}

		if (IsPortActive(pActInfo, eI_Set))
		{
			const float alpha = GetPortFloat(pActInfo, eI_Alpha);
			const int layer = GetPortInt(pActInfo, eI_Layer);
			const uint64 flags = GetFlags(pActInfo);

			SPerInstanceCall3<uint64, float, int> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIDisplayConfigNode::SetToInstance), flags, alpha, layer);
			ActivateOutput(pActInfo, eO_OnSet, true);
		}
	}
}

void CFlashUIDisplayConfigNode::GetFromInstance(IUIElement* pInstance, SActivationInfo* pActInfo)
{
	ActivateOutput(pActInfo, eO_IsCursorEnabled, pInstance->HasFlag(IUIElement::eFUI_HARDWARECURSOR));
	ActivateOutput(pActInfo, eO_IsMouseEvents, pInstance->HasFlag(IUIElement::eFUI_MOUSEEVENTS));
	ActivateOutput(pActInfo, eO_IsKeyEvents, pInstance->HasFlag(IUIElement::eFUI_KEYEVENTS));
	ActivateOutput(pActInfo, eO_IsConsole_Mouse, pInstance->HasFlag(IUIElement::eFUI_CONSOLE_MOUSE));
	ActivateOutput(pActInfo, eO_IsConsole_Cursor, pInstance->HasFlag(IUIElement::eFUI_CONSOLE_CURSOR));
	ActivateOutput(pActInfo, eO_IsController_Input, pInstance->HasFlag(IUIElement::eFUI_CONTROLLER_INPUT));
	ActivateOutput(pActInfo, eO_IsEvents_Exclusive, pInstance->HasFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE));
	ActivateOutput(pActInfo, eO_IsFixedProjDepth, pInstance->HasFlag(IUIElement::eFUI_FIXED_PROJ_DEPTH));

	ActivateOutput(pActInfo, eO_IsForce_NoUnload, pInstance->HasFlag(IUIElement::eFUI_FORCE_NO_UNLOAD));

	ActivateOutput(pActInfo, eO_IsVisible, pInstance->IsVisible());
	ActivateOutput(pActInfo, eO_Alpha, pInstance->GetAlpha());
	ActivateOutput(pActInfo, eO_Layer, pInstance->GetLayer());
}

void CFlashUIDisplayConfigNode::SetToInstance(IUIElement* pInstance, uint64 flags, float alpha, int layer)
{
	// todo: optimize don't call GetPortBool for each instance!
	pInstance->SetFlag(IUIElement::eFUI_HARDWARECURSOR, (flags& IUIElement::eFUI_HARDWARECURSOR) != 0);
	pInstance->SetFlag(IUIElement::eFUI_MOUSEEVENTS, (flags& IUIElement::eFUI_MOUSEEVENTS) != 0);
	pInstance->SetFlag(IUIElement::eFUI_KEYEVENTS, (flags& IUIElement::eFUI_KEYEVENTS) != 0);
	pInstance->SetFlag(IUIElement::eFUI_CONSOLE_MOUSE, (flags& IUIElement::eFUI_CONSOLE_MOUSE) != 0);
	pInstance->SetFlag(IUIElement::eFUI_CONSOLE_CURSOR, (flags& IUIElement::eFUI_CONSOLE_CURSOR) != 0);
	pInstance->SetFlag(IUIElement::eFUI_CONTROLLER_INPUT, (flags& IUIElement::eFUI_CONTROLLER_INPUT) != 0);
	pInstance->SetFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE, (flags& IUIElement::eFUI_EVENTS_EXCLUSIVE) != 0);
	pInstance->SetFlag(IUIElement::eFUI_FIXED_PROJ_DEPTH, (flags& IUIElement::eFUI_FIXED_PROJ_DEPTH) != 0);

	pInstance->SetFlag(IUIElement::eFUI_FORCE_NO_UNLOAD, (flags& IUIElement::eFUI_FORCE_NO_UNLOAD) != 0);

	pInstance->SetAlpha(alpha);
	pInstance->SetLayer(layer);
}

uint64 CFlashUIDisplayConfigNode::GetFlags(SActivationInfo* pActInfo)
{
	uint64 flags = 0;
	flags |= GetPortBool(pActInfo, eI_CursorEnabled) ? IUIElement::eFUI_HARDWARECURSOR : 0;
	flags |= GetPortBool(pActInfo, eI_MouseEventsEnabled) ? IUIElement::eFUI_MOUSEEVENTS : 0;
	flags |= GetPortBool(pActInfo, eI_KeyEventsEnabled) ? IUIElement::eFUI_KEYEVENTS : 0;
	flags |= GetPortBool(pActInfo, eI_Console_Mouse) ? IUIElement::eFUI_CONSOLE_MOUSE : 0;
	flags |= GetPortBool(pActInfo, eI_Console_Cursor) ? IUIElement::eFUI_CONSOLE_CURSOR : 0;
	flags |= GetPortBool(pActInfo, eI_Controller_Input) ? IUIElement::eFUI_CONTROLLER_INPUT : 0;
	flags |= GetPortBool(pActInfo, eI_Events_Exclusive) ? IUIElement::eFUI_EVENTS_EXCLUSIVE : 0;
	flags |= GetPortBool(pActInfo, eI_FixedProjDepth) ? IUIElement::eFUI_FIXED_PROJ_DEPTH : 0;

	flags |= GetPortBool(pActInfo, eI_Force_NoUnload) ? IUIElement::eFUI_FORCE_NO_UNLOAD : 0;

	return flags;
}

//------------------------------------------------------------------------------------------------------
void CFlashUILayerNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("get",  "Get configuration"),
		InputPortConfig_Void("set",  "set configuration"),
		InputPortConfig<int>("layer","layer of the element"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet", _HELP("Triggered on set")),
		OutputPortConfig_Void("OnGet", _HELP("Triggered on get")),
		OutputPortConfig<int>("layer", "layer of the element"),
		{ 0 }
	};

	config.sDescription = "Node to setup layer of UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUILayerNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		const int instanceId = GetPortInt(pActInfo, eI_InstanceID);

		if (IsPortActive(pActInfo, eI_Get))
		{
			int layer = 0;

			SPerInstanceCall1<int&> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUILayerNode::GetLayer), layer, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" called get layer for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIElement).c_str(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}

			ActivateOutput(pActInfo, eO_Layer, layer);
			ActivateOutput(pActInfo, eO_OnGet, true);
		}

		if (IsPortActive(pActInfo, eI_Set))
		{
			const int layer = GetPortInt(pActInfo, eI_Layer);

			SPerInstanceCall1<int> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUILayerNode::SetLayer), layer);

			ActivateOutput(pActInfo, eO_OnSet, true);
		}
	}
}

//------------------------------------------------------------------------------------------------------
void CFlashUIConstraintsNode::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig in_config[] = {
		CreateElementsPort(),
		CreateInstanceIdPort(),
		InputPortConfig_Void("set",      "Set configuration"),
		InputPortConfig_Void("get",      "Get configuration"),
		InputPortConfig<int>("type",     1,                                             _HELP("Set positioning type (fixed=fixed position, dynamic=fit screen but keep aspect ratio, fullscreen=scale to fullscreen) "),0, _UICONFIG("enum_int:Fixed=0,Fullscreen=1,Dynamic=2")),
		InputPortConfig<int>("left",     "Set left position (fixed mode)"),
		InputPortConfig<int>("top",      "Set top position (fixed mode)"),
		InputPortConfig<int>("width",    "Set width (fixed mode)"),
		InputPortConfig<int>("height",   "Set height (fixed mode)"),
		InputPortConfig<bool>("scale",   "Set scale"),
		InputPortConfig<int>("hAlign",   1,                                             _HELP("Set horizontal align (dynamic mode)"),                                                                  0, _UICONFIG("enum_int:Left=0,Middle=1,Right=2")),
		InputPortConfig<int>("vAlign",   1,                                             _HELP("Set vertical align (dynamic mode)"),                                                                    0, _UICONFIG("enum_int:Top=0,Middle=1,Bottom=2")),
		InputPortConfig<bool>("maximize","Set if element is maximized (dynamic mode)"),
		{ 0 }
	};

	static const SOutputPortConfig out_config[] = {
		OutputPortConfig_Void("OnSet",     _HELP("Triggered on set")),
		OutputPortConfig_Void("OnGet",     _HELP("Triggered on get")),
		OutputPortConfig<int>("type",      _HELP("Get positioning type (0=fixed position, 1=scaled to fit screen)")),
		OutputPortConfig<int>("left",      "Get left position"),
		OutputPortConfig<int>("top",       "Get top position"),
		OutputPortConfig<int>("width",     "Get width"),
		OutputPortConfig<int>("height",    "Get height"),
		OutputPortConfig<bool>("scale",    "Get scale"),
		OutputPortConfig<int>("hAlign",    "Get horizontal align (0:Left, 1:Middle, 2:Right)"),
		OutputPortConfig<int>("vAlign",    "Get vertical align (0:Bottom, 1:Middle 2:Top"),
		OutputPortConfig<bool>("maximize", "Get maximize"),
		{ 0 }
	};

	config.sDescription = "Node to setup constraints for UIElements";
	config.pInputPorts = in_config;
	config.pOutputPorts = out_config;
	config.SetCategory(EFLN_APPROVED);
}

void CFlashUIConstraintsNode::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
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

		const int instanceId = GetPortInt(pActInfo, eI_InstanceID);
		if (IsPortActive(pActInfo, eI_Get))
		{
			IUIElement::SUIConstraints constraints;
			SPerInstanceCall1<IUIElement::SUIConstraints&> caller;
			if (!caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIConstraintsNode::GetConstraints), constraints, false))
			{
				UIACTION_WARNING("FG: UIElement \"%s\" can't get Constraints for multiple instances! (passed instanceId %i), referenced at node \"%s\"", GetPortString(pActInfo, eI_UIElement).c_str(), instanceId, pActInfo->pGraph->GetNodeTypeName(pActInfo->myID));
			}
			ActivateOutput(pActInfo, eO_PosType, (int) constraints.eType);
			ActivateOutput(pActInfo, eO_Left, constraints.iLeft);
			ActivateOutput(pActInfo, eO_Top, constraints.iTop);
			ActivateOutput(pActInfo, eO_Width, constraints.iWidth);
			ActivateOutput(pActInfo, eO_Height, constraints.iHeight);
			ActivateOutput(pActInfo, eO_Scale, constraints.bScale);
			ActivateOutput(pActInfo, eO_HAlign, (int) constraints.eHAlign);
			ActivateOutput(pActInfo, eO_VAlign, (int) constraints.eVAlign);
			ActivateOutput(pActInfo, eO_Maximize, constraints.bMax);
			ActivateOutput(pActInfo, eO_OnGet, true);
		}

		if (IsPortActive(pActInfo, eI_Set))
		{
			IUIElement::SUIConstraints constraints;
			constraints.eType = (IUIElement::SUIConstraints::EPositionType) GetPortInt(pActInfo, eI_PosType);
			constraints.iLeft = GetPortInt(pActInfo, eI_Left);
			constraints.iTop = GetPortInt(pActInfo, eI_Top);
			constraints.iWidth = GetPortInt(pActInfo, eI_Width);
			constraints.iHeight = GetPortInt(pActInfo, eI_Height);
			constraints.eHAlign = (IUIElement::SUIConstraints::EPositionAlign) GetPortInt(pActInfo, eI_HAlign);
			constraints.eVAlign = (IUIElement::SUIConstraints::EPositionAlign) GetPortInt(pActInfo, eI_VAlign);
			constraints.bScale = GetPortBool(pActInfo, eI_Scale);
			constraints.bMax = GetPortBool(pActInfo, eI_Maximize);

			SPerInstanceCall1<const IUIElement::SUIConstraints&> caller;
			caller.Execute(GetElement(), instanceId, functor(*this, &CFlashUIConstraintsNode::SetConstraints), constraints);

			ActivateOutput(pActInfo, eO_OnSet, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
class CFlowRayToFlashSpace : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowRayToFlashSpace(SActivationInfo* pActInfo) {}

	virtual ~CFlowRayToFlashSpace() {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowRayToFlashSpace(pActInfo);
	}

	void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
	}

	enum EInputPorts
	{
		EIP_Cast = 0,
		EIP_RayPos,
		EIP_RayDir,
		EIP_CallFunction,
		EIP_SkipEntity,
	};

	enum EOutputPorts
	{
		EOP_Success = 0,
		EOP_Failed,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = {
			InputPortConfig_Void("Cast",            _HELP("Cast test ray")),
			InputPortConfig<Vec3>("Pos",            _HELP("Ray pos")),
			InputPortConfig<Vec3>("Dir",            _HELP("Ray Dir")),
			InputPortConfig<string>("FunctionName", _HELP("This function will be called in ActionScript (signature: myFunc = function(int, int))")),
			InputPortConfig<EntityId>("SkipEntity", _HELP("Optional: Entity to skip")),
			{ 0 }
		};

		static const SOutputPortConfig outputs[] = {
			OutputPortConfig_Void("Success", _HELP("Triggers if succeeded")),
			OutputPortConfig_Void("Failed",  _HELP("Triggers if something went wrong(no hit or wrong entity)")),
			{ 0 }
		};

		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Casts a ray, finds the hitposition in flash space and calls a function");
		config.SetCategory(EFLN_ADVANCED);
	}

	bool RayIntersectMesh(IRenderMesh* pMesh, IMaterial* pMaterial, IUIElement* pElement, const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2)
	{
		bool hasHit = false;
		// get triangle that was hit
		pMesh->LockForThreadAccess();
		const int numIndices = pMesh->GetIndicesCount();
		const int numVertices = pMesh->GetVerticesCount();
		if (numIndices || numVertices)
		{
			// TODO: this could be optimized, e.g cache triangles and uv's and use octree to find triangles
			strided_pointer<Vec3> pPos;
			strided_pointer<Vec2> pUV;
			pPos.data = (Vec3*)pMesh->GetPosPtr(pPos.iStride, FSL_READ);
			pUV.data = (Vec2*)pMesh->GetUVPtr(pUV.iStride, FSL_READ);
			const vtx_idx* pIndices = pMesh->GetIndexPtr(FSL_READ);

			const TRenderChunkArray& Chunks = pMesh->GetChunks();
			const int nChunkCount = Chunks.size();
			for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
			{
				const CRenderChunk* pChunk = &Chunks[nChunkId];
				if ((pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
					continue;

				int lastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
				for (int i = pChunk->nFirstIndexId; i < lastIndex; i += 3)
				{
					const Vec3& v0 = pPos[pIndices[i]];
					const Vec3& v1 = pPos[pIndices[i + 1]];
					const Vec3& v2 = pPos[pIndices[i + 2]];

					if (Intersect::Ray_Triangle(ray, v0, v2, v1, hitpos))  // only front face
					{
						uv0 = pUV[pIndices[i]];
						uv1 = pUV[pIndices[i + 1]];
						uv2 = pUV[pIndices[i + 2]];
						p0 = v0;
						p1 = v1;
						p2 = v2;
						hasHit = true;
						nChunkId = nChunkCount; // break outer loop
						break;
					}
				}
			}
		}

		pMesh->UnlockStream(VSF_GENERAL);
		pMesh->UnlockIndexStream();
		pMesh->UnLockForThreadAccess();

		return hasHit;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			break;
		case eFE_Activate:
			IGameFramework* pGameFramework = gEnv->pGameFramework;

			if (IsPortActive(pActInfo, EIP_Cast))
			{
				// setup ray + optionally skip 1 entity
				ray_hit rayHit;
				static const float maxRayDist = 100.f;
				const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
				IPhysicalEntity* skipList[1];
				int skipCount = 0;
				IEntity* skipEntity = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, EIP_SkipEntity));
				if (skipEntity)
				{
					skipList[0] = skipEntity->GetPhysics();
					skipCount = 1;
				}

				Vec3 rayPos = GetPortVec3(pActInfo, EIP_RayPos);
				Vec3 rayDir = GetPortVec3(pActInfo, EIP_RayDir);

				// Check if the ray hits an entity
				if (gEnv->pSystem->GetIPhysicalWorld()->RayWorldIntersection(rayPos, rayDir * 100, ent_all, flags, &rayHit, 1, skipList, skipCount))
				{
					int type = rayHit.pCollider->GetiForeignData();

					if (type == PHYS_FOREIGN_ID_ENTITY)
					{
						IEntity* pEntity = (IEntity*)rayHit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
						IEntityRender* pIEntityRender = pEntity ? pEntity->GetRenderInterface() : 0;

						// Get the renderproxy, and use it to check if the material is a DynTex, and get the UIElement if so
						
						{
							IRenderNode* pRenderNode = pIEntityRender->GetRenderNode();
							IMaterial* pMaterial = pIEntityRender->GetRenderMaterial();
							SEfResTexture* texture = 0;
							if (pMaterial && pMaterial->GetShaderItem().m_pShaderResources)
								texture = pMaterial->GetShaderItem().m_pShaderResources->GetTexture(EFTT_DIFFUSE);
							IUIElement* pElement = texture ? gEnv->pFlashUI->GetUIElementByInstanceStr(texture->m_Name) : 0;

							if (pElement && pRenderNode)
							{
								int m_dynTexGeomSlot = 0;
								IStatObj* pObj = pRenderNode->GetEntityStatObj(m_dynTexGeomSlot);

								// result
								bool hasHit = false;
								Vec2 uv0, uv1, uv2;
								Vec3 p0, p1, p2;
								Vec3 hitpos;

								// calculate ray dir
								CCamera cam = GetISystem()->GetViewCamera();
								if (pEntity->GetSlotFlags(m_dynTexGeomSlot) & ENTITY_SLOT_RENDER_NEAREST)
								{
									ICVar* r_drawnearfov = gEnv->pConsole->GetCVar("r_DrawNearFoV");
									assert(r_drawnearfov);
									cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), DEG2RAD(r_drawnearfov->GetFVal()), cam.GetNearPlane(), cam.GetFarPlane(), cam.GetPixelAspectRatio());
								}

								Vec3 vPos0 = rayPos;
								Vec3 vPos1 = rayPos + rayDir;

								// translate into object space
								const Matrix34 m = pEntity->GetWorldTM().GetInverted();
								vPos0 = m * vPos0;
								vPos1 = m * vPos1;

								// walk through all sub objects
								const int objCount = pObj->GetSubObjectCount();
								for (int obj = 0; obj <= objCount && !hasHit; ++obj)
								{
									Vec3 vP0, vP1;
									IStatObj* pSubObj = NULL;

									if (obj == objCount)
									{
										vP0 = vPos0;
										vP1 = vPos1;
										pSubObj = pObj;
									}
									else
									{
										IStatObj::SSubObject* pSub = pObj->GetSubObject(obj);
										const Matrix34 mm = pSub->tm.GetInverted();
										vP0 = mm * vPos0;
										vP1 = mm * vPos1;
										pSubObj = pSub->pStatObj;
									}

									IRenderMesh* pMesh = pSubObj ? pSubObj->GetRenderMesh() : NULL;
									if (pMesh)
									{
										const Ray ray(vP0, (vP1 - vP0).GetNormalized() * maxRayDist);
										hasHit = RayIntersectMesh(pMesh, pMaterial, pElement, ray, hitpos, p0, p1, p2, uv0, uv1, uv2);
									}
								}

								// skip if not hit
								if (!hasHit)
								{
									ActivateOutput(pActInfo, EOP_Failed, 1);
									return;
								}

								// calculate vectors from hitpos to vertices p0, p1 and p2:
								const Vec3 v0 = p0 - hitpos;
								const Vec3 v1 = p1 - hitpos;
								const Vec3 v2 = p2 - hitpos;

								// calculate factors
								const float h = (p0 - p1).Cross(p0 - p2).GetLength();
								const float f0 = v1.Cross(v2).GetLength() / h;
								const float f1 = v2.Cross(v0).GetLength() / h;
								const float f2 = v0.Cross(v1).GetLength() / h;

								// find the uv corresponding to hitpos
								Vec3 uv = uv0 * f0 + uv1 * f1 + uv2 * f2;

								// translate to flash space
								int x, y, width, height;
								float aspect;
								pElement->GetFlashPlayer()->GetViewport(x, y, width, height, aspect);
								int iX = int_round(uv.x * (float)width);
								int iY = int_round(uv.y * (float)height);

								// call the function provided if it is present in the UIElement description
								string funcName = GetPortString(pActInfo, EIP_CallFunction);
								const SUIEventDesc* eventDesc = pElement->GetFunctionDesc(funcName);
								if (eventDesc)
								{
									SUIArguments arg;
									arg.AddArgument(iX);
									arg.AddArgument(iY);
									pElement->CallFunction(eventDesc->sName, arg);
								}

								ActivateOutput(pActInfo, EOP_Success, 1);
							}
						}
					}
				}

				ActivateOutput(pActInfo, EOP_Failed, 1);
			}

			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

//------------------------------------------------------------------------------------------------------
REGISTER_FLOW_NODE("UI:Display:Display", CFlashUIDisplayNode);
REGISTER_FLOW_NODE("UI:Display:Advance", CFlashUIAdvanceNode);
REGISTER_FLOW_NODE("UI:Display:ScreenPos", CFlashUIScreenPosNode);
REGISTER_FLOW_NODE("UI:Display:WorldScreenPos", CFlashUIWorldScreenPosNode);
REGISTER_FLOW_NODE("UI:Display:Config", CFlashUIDisplayConfigNode);
REGISTER_FLOW_NODE("UI:Display:Constraints", CFlashUIConstraintsNode);
REGISTER_FLOW_NODE("UI:Display:Layer", CFlashUILayerNode);
REGISTER_FLOW_NODE("UI:Display:RayToFlashSpace", CFlowRayToFlashSpace);
