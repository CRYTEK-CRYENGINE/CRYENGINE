// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIDisplayNodes.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUIDisplayNodes_H__
#define __FlashUIDisplayNodes_H__

#include "FlashUIBaseNode.h"

class CFlashUIDisplayNode : public CFlashUIBaseNode
{
public:
	CFlashUIDisplayNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIDisplayNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIDisplayNode(pActInfo); }

private:
	void SetVisible(IUIElement* pInstance, bool visible) { pInstance->SetVisible(visible); }
	void Unload(IUIElement* pInstance)                   { pInstance->Unload(); }
	void RequestHide(IUIElement* pInstance)              { pInstance->RequestHide(); }
	void Init(IUIElement* pInstance)                     { pInstance->Reload(); }
	void Reload(IUIElement* pInstance)                   { pInstance->Reload(); }

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Show,
		eI_Hide,
		eI_Unload,
		eI_RequestHide,
		eI_Init,
		eI_Reload,
		eI_UnloadBootstrapper,
		eI_ReloadBootstrapper,
	};
	enum EOutputs
	{
		eO_OnShow = 0,
		eO_OnHide,
		eO_OnUnload,
		eO_OnRequestHide,
		eO_OnInit,
		eO_OnReload,
		eO_OnUnloadBootstrapper,
		eO_OnReloadBootstrapper,
	};
};

// --------------------------------------------------------------
class CFlashUIAdvanceNode : public CFlashUIBaseNode
{
public:
	CFlashUIAdvanceNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIAdvanceNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIAdvanceNode(pActInfo); }

private:
	void Advance(IUIElement* pInstance, float delta);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Advance,
		eI_Delta,
	};
	enum EOutputs
	{
		eO_OnAdvance = 0,
	};
};

// --------------------------------------------------------------
class CFlashUIScreenPosNode : public CFlashUIBaseNode
{
public:
	CFlashUIScreenPosNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIScreenPosNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIScreenPosNode(pActInfo); }

private:
	void GetScreenPos(IUIElement* pInstance, float& px /*in/out*/, float& py /*in/out*/, bool bScaleMode);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Get,
		eI_PX,
		eI_PY,
		eI_ScaleMode,
	};
	enum EOutputs
	{
		eO_OnGet = 0,
		eO_PX,
		eO_PY,
	};
};

// --------------------------------------------------------------
class CFlashUIWorldScreenPosNode : public CFlashUIBaseNode
{
public:
	CFlashUIWorldScreenPosNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIWorldScreenPosNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIWorldScreenPosNode(pActInfo); }

private:
	void GetScreenPosFromWorld(IUIElement* pInstance, Vec3& pos /*in/out*/, Vec3& offset /*in/out*/, bool bScaleMode);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Get,
		eI_WorldPos,
		eI_Offset,
		eI_ScaleMode,
	};
	enum EOutputs
	{
		eO_OnGet = 0,
		eO_ScreenPos,
		eO_Scale,
		eO_IsOnScreen,
		eO_OverScanBorder,
	};
};

// --------------------------------------------------------------
class CFlashUIDisplayConfigNode : public CFlashUIBaseNode
{
public:
	CFlashUIDisplayConfigNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIDisplayConfigNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIDisplayConfigNode(pActInfo); }

private:
	void   GetFromInstance(IUIElement* pInstance, SActivationInfo* pActInfo);
	void   SetToInstance(IUIElement* pInstance, uint64 flags, float alpha, int layer);
	uint64 GetFlags(SActivationInfo* pActInfo);

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Get,
		eI_Set,

		eI_CursorEnabled,
		eI_MouseEventsEnabled,
		eI_KeyEventsEnabled,
		eI_Console_Mouse,
		eI_Console_Cursor,
		eI_Controller_Input,
		eI_Events_Exclusive,
		eI_FixedProjDepth,

		eI_Force_NoUnload,

		eI_Alpha,
		eI_Layer,
	};
	enum EOutputs
	{
		eO_OnSet = 0,
		eO_OnGet,
		eO_IsVisible,

		eO_IsCursorEnabled,
		eO_IsMouseEvents,
		eO_IsKeyEvents,
		eO_IsConsole_Mouse,
		eO_IsConsole_Cursor,
		eO_IsController_Input,
		eO_IsEvents_Exclusive,
		eO_IsFixedProjDepth,

		eO_IsForce_NoUnload,

		eO_Alpha,
		eO_Layer,
	};
};

// --------------------------------------------------------------
class CFlashUILayerNode : public CFlashUIBaseNode
{
public:
	CFlashUILayerNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUILayerNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUILayerNode(pActInfo); }

private:
	void GetLayer(IUIElement* pInstance, int& layer) { layer = pInstance->GetLayer(); }
	void SetLayer(IUIElement* pInstance, int layer)  { pInstance->SetLayer(layer); }

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Get,
		eI_Set,
		eI_Layer,
	};
	enum EOutputs
	{
		eO_OnSet = 0,
		eO_OnGet,
		eO_Layer,
	};
};

// --------------------------------------------------------------
class CFlashUIConstraintsNode : public CFlashUIBaseNode
{
public:
	CFlashUIConstraintsNode(SActivationInfo* pActInfo) {}
	virtual ~CFlashUIConstraintsNode() {}

	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo) { return new CFlashUIConstraintsNode(pActInfo); }

private:
	void GetConstraints(IUIElement* pInstance, IUIElement::SUIConstraints& constraints)       { constraints = pInstance->GetConstraints(); }
	void SetConstraints(IUIElement* pInstance, const IUIElement::SUIConstraints& constraints) { pInstance->SetConstraints(constraints); }

private:
	enum EInputs
	{
		eI_UIElement = 0,
		eI_InstanceID,
		eI_Set,
		eI_Get,
		eI_PosType,
		eI_Left,
		eI_Top,
		eI_Width,
		eI_Height,
		eI_Scale,
		eI_HAlign,
		eI_VAlign,
		eI_Maximize,
	};
	enum EOutputs
	{
		eO_OnSet = 0,
		eO_OnGet,
		eO_PosType,
		eO_Left,
		eO_Top,
		eO_Width,
		eO_Height,
		eO_Scale,
		eO_HAlign,
		eO_VAlign,
		eO_Maximize,
	};
};

#endif // #ifndef __FlashUIDisplayNodes_H__
