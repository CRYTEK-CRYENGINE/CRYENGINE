// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIInput.cpp
//  Version:     v1.00
//  Created:     17/9/2010 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIInput.h"
#include "UIMenuEvents.h"
#include "UIManager.h"

#include "Game.h"
#include "GameActions.h"

TActionHandler<CUIInput> CUIInput::s_actionHandler;

///////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIInput::CUIInput()
	: m_pUIFunctions(nullptr)
	, m_pUIEvents(nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIInput::~CUIInput()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::Shutdown()
{
	s_actionHandler.Clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::InitEventSystem()
{
	if ( !gEnv->pFlashUI
		|| !g_pGame->GetIGameFramework() 
		|| !g_pGame->GetIGameFramework()->GetIActionMapManager() )
	{
		return;
	}

	// set up the handlers
	if (s_actionHandler.GetNumHandlers() == 0)
	{
		#define ADD_HANDLER(action, func) s_actionHandler.AddHandler(actions.action, &CUIInput::func)
		const CGameActions& actions = g_pGame->Actions();

		ADD_HANDLER(ui_toggle_pause, OnActionTogglePause);
		ADD_HANDLER(ui_start_pause, OnActionStartPause);

		ADD_HANDLER(ui_up, OnActionUp);
		ADD_HANDLER(ui_down, OnActionDown);
		ADD_HANDLER(ui_left, OnActionLeft);	
		ADD_HANDLER(ui_right, OnActionRight);

		ADD_HANDLER(ui_click, OnActionClick);	
		ADD_HANDLER(ui_back, OnActionBack);	

		ADD_HANDLER(ui_confirm, OnActionConfirm);	
		ADD_HANDLER(ui_reset, OnActionReset);	

		#undef ADD_HANDLER
	}

	IActionMapManager* pAMMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
	if (pAMMgr)
	{
		pAMMgr->AddAlwaysActionListener(shared_from_this());
	}

	// ui events (sent to ui)
	m_pUIFunctions = gEnv->pFlashUI->CreateEventSystem( "Input", IUIEventSystem::eEST_SYSTEM_TO_UI );
	m_eventSender.Init(m_pUIFunctions);
	{
		SUIEventDesc eventDesc("OnKeyboardDone", "triggered once keyboard is done");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("String", "String of keyboard input");
		m_eventSender.RegisterEvent<eUIE_OnVirtKeyboardDone>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("OnKeyboardCancelled", "triggered once keyboard is cancelled");
		m_eventSender.RegisterEvent<eUIE_OnVirtKeyboardCancelled>(eventDesc);
	}
	
	{
		SUIEventDesc eventDesc("OnExclusiveControllerDisconnected", "triggered once the exclusive controller got disconnected");
		m_eventSender.RegisterEvent<eUIE_OnExclusiveControllerDisconnected>(eventDesc);
	}

	// ui events (called from ui)
	m_pUIEvents = gEnv->pFlashUI->CreateEventSystem( "Input", IUIEventSystem::eEST_UI_TO_SYSTEM );
	m_eventDispatcher.Init(m_pUIEvents, this, "UIInput");
	{
		SUIEventDesc eventDesc("ShowVirtualKeyboard", "Displays the virtual keyboard");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Title", "Title for the virtual keyboard");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Value", "Initial string of virtual keyboard");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("MaxChars", "Maximum chars");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIInput::OnDisplayVirtualKeyboard );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::UnloadEventSystem()
{
	if (g_pGame && g_pGame->GetIGameFramework())
	{
		IActionMapManager* pAMMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
		if (pAMMgr)
		{
			pAMMgr->RemoveAlwaysActionListener(shared_from_this());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::KeyboardCancelled()
{
	m_eventSender.SendEvent<eUIE_OnVirtKeyboardCancelled>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::KeyboardFinished(const char *pInString)
{
	m_eventSender.SendEvent<eUIE_OnVirtKeyboardDone>(pInString);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// UI Functions ////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::OnDisplayVirtualKeyboard( const char* title, const char* initialStr, int maxchars )
{
	gEnv->pFlashUI->DisplayVirtualKeyboard(IPlatformOS::KbdFlag_Default, title, initialStr, maxchars, this );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// Actions /////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::OnActionInput( const ActionId& action, int activationMode, float value )
{
	s_actionHandler.Dispatch( this, 0, action, activationMode, value );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnAction( const ActionId& action, int activationMode, float value, const SInputEvent &inputEvent )
{
	return s_actionHandler.Dispatch( this, 0, action, activationMode, value );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionTogglePause(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	CUIMenuEvents* pMenuEvents = UIEvents::Get<CUIMenuEvents>();
	if (g_pGame->GetIGameFramework()->IsGameStarted() && pMenuEvents)
	{
		const bool bIsIngameMenu = pMenuEvents->IsIngameMenuStarted();
		pMenuEvents->DisplayIngameMenu(!bIsIngameMenu);
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionStartPause(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	CUIMenuEvents* pMenuEvents = UIEvents::Get<CUIMenuEvents>();
	if (g_pGame->GetIGameFramework()->IsGameStarted() && pMenuEvents && !pMenuEvents->IsIngameMenuStarted())
	{
			pMenuEvents->DisplayIngameMenu(true);
	}
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SEND_CONTROLLER_EVENT(evt, value)   if ( gEnv->pFlashUI ) \
	{ \
		switch (activationMode) \
		{ \
		case eAAM_OnPress:   gEnv->pFlashUI->DispatchControllerEvent( IUIElement::evt, IUIElement::eCIS_OnPress, value ); break; \
		case eAAM_OnRelease: gEnv->pFlashUI->DispatchControllerEvent( IUIElement::evt, IUIElement::eCIS_OnRelease, value ); break;\
		case eAAM_Always: \
		case eAAM_OnHold:    gEnv->pFlashUI->DispatchControllerEvent( IUIElement::evt, IUIElement::eCIS_OnPress, value ); \
												gEnv->pFlashUI->DispatchControllerEvent( IUIElement::evt, IUIElement::eCIS_OnRelease, value ); break; \
		} \
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionUp(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Up, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionDown(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Down, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionLeft(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Left, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionRight(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Right, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionClick(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Action, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionBack(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Back, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionConfirm(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Button3, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIInput::OnActionReset(EntityId entityId, const ActionId& actionId, int activationMode, float value)
{
	SEND_CONTROLLER_EVENT(eCIE_Button4, value);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIInput::ExclusiveControllerDisconnected()
{
	m_eventSender.SendEvent<eUIE_OnExclusiveControllerDisconnected>();
}

#undef SEND_CONTROLLER_EVENT

///////////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM( CUIInput );
