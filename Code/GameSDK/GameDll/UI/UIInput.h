// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIInput.h
//  Version:     v1.00
//  Created:     17/9/2010 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UIInput_H__
#define __UIInput_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <IActionMapManager.h>

class CUIInput 
	: public IUIGameEventSystem
	, public IBlockingActionListener
	, public IVirtualKeyboardEvents
	, public std::enable_shared_from_this<CUIInput>
{
public:
	CUIInput();
	~CUIInput();

	static void Shutdown();

	// IUIGameEventSystem
	UIEVENTSYSTEM( "UIInput" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;

	void OnActionInput( const ActionId& action, int activationMode, float value );

	// IBlockingActionListener
	virtual bool OnAction( const ActionId& action, int activationMode, float value, const SInputEvent &inputEvent ) override;
	// ~IBlockingActionListener

	// IVirtualKeyboardEvents
	virtual void KeyboardCancelled() override;
	virtual void KeyboardFinished(const char *pInString) override;
	// ~IVirtualKeyboardEvents

	void ExclusiveControllerDisconnected();

private:
	// ui events
	void OnDisplayVirtualKeyboard( const char* title, const char* initialStr, int maxchars );

	// actions
	bool OnActionTogglePause(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionStartPause(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionUp(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionDown(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionRight(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionClick(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionBack(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionConfirm(EntityId entityId, const ActionId& actionId, int activationMode, float value);
	bool OnActionReset(EntityId entityId, const ActionId& actionId, int activationMode, float value);

private:
	enum EUIEvent
	{
		eUIE_OnVirtKeyboardDone,
		eUIE_OnVirtKeyboardCancelled,
		eUIE_OnExclusiveControllerDisconnected,
	};

	SUIEventReceiverDispatcher<CUIInput> m_eventDispatcher;
	SUIEventSenderDispatcher<EUIEvent> m_eventSender;
	IUIEventSystem* m_pUIEvents;
	IUIEventSystem* m_pUIFunctions;

	static TActionHandler<CUIInput>	s_actionHandler;
	std::map< EUIEvent, uint > m_eventMap;
};


#endif