// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIMenuEvents.cpp
//  Version:     v1.00
//  Created:     21/11/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIMenuEvents.h"

#include "Game.h"
#include <ILevelSystem.h>

////////////////////////////////////////////////////////////////////////////
CUIMenuEvents::CUIMenuEvents()
: m_pUIEvents(NULL)
, m_pUIFunctions(NULL)
, m_bIsIngameMenuStarted(false)
{
}

////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::InitEventSystem()
{
	if (!gEnv->pFlashUI) return;

	// events to send from this class to UI flowgraphs
	m_pUIFunctions = gEnv->pFlashUI->CreateEventSystem("MenuEvents", IUIEventSystem::eEST_SYSTEM_TO_UI);
	m_eventSender.Init(m_pUIFunctions);

	{
		SUIEventDesc eventDesc("OnStartIngameMenu", "Triggered if the IngameMenu should be displayed");
		m_eventSender.RegisterEvent<eUIE_StartIngameMenu>(eventDesc);
	}

	{
		SUIEventDesc eventDesc("OnStopIngameMenu", "Triggered if the IngameMenu should be hidden");
		m_eventSender.RegisterEvent<eUIE_StopIngameMenu>(eventDesc);
	}


	// events that can be sent from UI flowgraphs to this class
	m_pUIEvents = gEnv->pFlashUI->CreateEventSystem("MenuEvents", IUIEventSystem::eEST_UI_TO_SYSTEM);
	m_eventDispatcher.Init(m_pUIEvents, this, "CUIMenuEvents");
	{
		SUIEventDesc eventDesc("DisplayIngameMenu", "Call this to Display or Hide the IngameMenu");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Bool>("Display", "True or false");
		m_eventDispatcher.RegisterEvent(eventDesc, &CUIMenuEvents::DisplayIngameMenu);
	}

	gEnv->pFlashUI->RegisterModule(this, "CUIMenuEvents");
}

////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::UnloadEventSystem()
{
	if (gEnv->pFlashUI)
		gEnv->pFlashUI->UnregisterModule(this);
}

////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::Reset()
{
	DisplayIngameMenu(false);
}

////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::DisplayIngameMenu(bool bDisplay)
{
	if (m_bIsIngameMenuStarted == bDisplay) return;

	m_bIsIngameMenuStarted = bDisplay;
	if (bDisplay)
		StartIngameMenu();
	else
		StopIngameMenu();
}

////////////////////////////////////////////////////////////////////////////
// ui events
////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::StartIngameMenu()
{
	if (!gEnv->bMultiplayer && !gEnv->IsEditor())
	{
		g_pGame->GetIGameFramework()->PauseGame( true, true );
	}

	IActionMapManager* pAmMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
	if (pAmMgr)
	{
		pAmMgr->Enable(true);
		pAmMgr->EnableFilter("only_ui", true);
	}

	m_eventSender.SendEvent<eUIE_StartIngameMenu>();
}

////////////////////////////////////////////////////////////////////////////
void CUIMenuEvents::StopIngameMenu()
{
	if (!gEnv->bMultiplayer && !gEnv->IsEditor())
	{
		g_pGame->GetIGameFramework()->PauseGame( false, true );
	}

	IActionMapManager* pAmMgr = g_pGame->GetIGameFramework()->GetIActionMapManager();
	if (pAmMgr)
	{
		pAmMgr->EnableFilter("only_ui", false);
	}

	m_eventSender.SendEvent<eUIE_StopIngameMenu>();
}

////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM( CUIMenuEvents );