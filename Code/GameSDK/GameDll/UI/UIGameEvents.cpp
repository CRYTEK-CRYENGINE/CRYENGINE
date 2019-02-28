// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIGameEvents.cpp
//  Version:     v1.00
//  Created:     19/03/2012 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIGameEvents.h"

#include <CryGame/IGameFramework.h>
#include "Game.h"
#include "Actor.h"
#include "GameRules.h"

////////////////////////////////////////////////////////////////////////////
CUIGameEvents::CUIGameEvents()
	: m_pUIEvents(NULL)
	, m_pGameFramework(NULL)
	, m_pLevelSystem(NULL)
{
}

////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::InitEventSystem()
{
	if (!gEnv->pFlashUI)
		return;

	m_pGameFramework = gEnv->pGameFramework;
	m_pLevelSystem = m_pGameFramework ? m_pGameFramework->GetILevelSystem() : NULL;

	assert(m_pLevelSystem && m_pGameFramework);
	if (!m_pLevelSystem || !m_pGameFramework)
		return;

	// events that can be sent from UI flowgraphs to this class
	m_pUIEvents = gEnv->pFlashUI->CreateEventSystem("Game", IUIEventSystem::eEST_UI_TO_SYSTEM);
	m_eventDispatcher.Init(m_pUIEvents, this, "CUIGameEvents");
	{
		SUIEventDesc eventDesc("LoadLevel", "Loads a level");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Level", "Name of the Level");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Bool>  ("Server", "If true, load as Server");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("GameRules", "Name of the gamerules that should be used");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnLoadLevel );
	}

	{
		SUIEventDesc eventDesc("ReloadLevel", "Reload current level");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnReloadLevel );
	}

	{
		SUIEventDesc eventDesc("SaveGame", "Quicksave current game");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Bool>("Resume", "If true, game will be resumed if game was paused");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnSaveGame );
	}

	{
		SUIEventDesc eventDesc("LoadGame", "Quickload current game");
		eventDesc.AddParam<SUIParameterDesc::eUIPT_Bool>("Resume", "If true, game will be resumed if game was paused");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnLoadGame );
	}

	{
		SUIEventDesc eventDesc("PauseGame", "Pause the game (does not pause in mp)");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnPauseGame );
	}

	{
		SUIEventDesc eventDesc("ResumeGame", "Resumes the game (does not pause in mp)");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnResumeGame );
	}

	{
		SUIEventDesc eventDesc("ExitGame", "Quit the game");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnExitGame );
	}

	{
		SUIEventDesc eventDesc("StartGame", "Starts the current game (sends the GAMEPLAY START event)");
		m_eventDispatcher.RegisterEvent( eventDesc, &CUIGameEvents::OnStartGame );
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::UnloadEventSystem()
{
}


////////////////////////////////////////////////////////////////////////////
// ui events
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnLoadLevel( const char* mapname, bool isServer, const char* gamerules )
{
	if (gEnv->IsEditor()) return;

	ICVar* pGameRulesVar = gEnv->pConsole->GetCVar("sv_GameRules");
	if (pGameRulesVar) pGameRulesVar->Set( gamerules );
	m_pGameFramework->ExecuteCommandNextFrame(string().Format("map %s%s", mapname, isServer ? " s" : ""));
	if ( m_pGameFramework->IsGamePaused() )
		m_pGameFramework->PauseGame(false, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnReloadLevel()
{
	if (gEnv->IsEditor()) return;

	ILevelInfo* pLevel = m_pLevelSystem->GetCurrentLevel();
	if (pLevel)
	{
		m_pGameFramework->ExecuteCommandNextFrame(string().Format("map %s", pLevel->GetName()));
		if ( m_pGameFramework->IsGamePaused() )
			m_pGameFramework->PauseGame(false, true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnSaveGame( bool shouldResume )
{
	if (gEnv->IsEditor()) return;

	ILevelInfo* pLevel = m_pLevelSystem->GetCurrentLevel();
	if (pLevel)
	{
		m_pGameFramework->SaveGame(pLevel->GetPath(), true);
		if ( shouldResume && m_pGameFramework->IsGamePaused() )
			m_pGameFramework->PauseGame(false, true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnLoadGame( bool shouldResume )
{
	if (gEnv->IsEditor()) return;

	ILevelInfo* pLevel = m_pLevelSystem->GetCurrentLevel();
	if (pLevel)
	{
		m_pGameFramework->LoadGame(pLevel->GetPath(), true);
		if ( shouldResume && m_pGameFramework->IsGamePaused() )
			m_pGameFramework->PauseGame(false, true);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnPauseGame()
{
	if (gEnv->IsEditor()) return;

	m_pGameFramework->PauseGame(true, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnResumeGame()
{
	if (gEnv->IsEditor()) return;

	m_pGameFramework->PauseGame(false, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnExitGame()
{
	if (gEnv->IsEditor()) return;

	gEnv->pSystem->Quit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIGameEvents::OnStartGame()
{
	if (gEnv->IsEditor()) return;

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_GAMEPLAY_START, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM( CUIGameEvents );
