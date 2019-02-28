// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIManager.cpp
//  Version:     v1.00
//  Created:     08/8/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "UIManager.h"
#include "UIMenuEvents.h"

#include <CryGame/IGame.h>
#include <CryGame/IGameFramework.h>
#include <CrySystem/Scaleform/IFlashUI.h>

#include "Game.h"
#include "WarningsManager.h"
#include "ProfileOptions.h"
#include "UICVars.h"
#include "Utils/ScreenLayoutManager.h"
#include "HUD/HUDEventDispatcher.h"
#include "HUD/HUDSilhouettes.h"
#include "HUD/HUDMissionObjectiveSystem.h"
#include "Graphics/2DRenderUtils.h"
#include "GameRulesTypes.h"
#include "UIInput.h"

IUIEventSystemFactory* IUIEventSystemFactory::s_pFirst = NULL;
IUIEventSystemFactory* IUIEventSystemFactory::s_pLast;

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// CTor/DTor ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
CUIManager::CUIManager()
	: m_bRegistered(false)
	, m_controlSchemeListeners(1)
	, m_pWarningManager(NULL)
	, m_pOptions(NULL)
	, m_p2DRendUtils(NULL)
	, m_pScreenLayoutMan(NULL)
	, m_pHudSilhouettes(NULL)
	, m_pCVars(NULL)
	, m_pMOSystem(NULL)
	, m_soundListener(INVALID_ENTITYID)
	, m_curControlScheme(eControlScheme_NotSpecified)
{
}

/////////////////////////////////////////////////////////////////////////////////////
CUIManager::~CUIManager()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::Init()
{
	CHUDEventDispatcher::SetUpEventListener();
	SHUDEvent::InitDataStack();

	m_pWarningManager = new CWarningsManager();
	m_pOptions = new CProfileOptions();

	if (gEnv->pRenderer)
	{
		m_pScreenLayoutMan = new ScreenLayoutManager();
		m_p2DRendUtils = new C2DRenderUtils(m_pScreenLayoutMan);
	}
	m_pHudSilhouettes = new CHUDSilhouettes();
	m_pCVars = new CUICVars();
	m_pMOSystem = new CHUDMissionObjectiveSystem();

	m_pCVars->RegisterConsoleCommandsAndVars();
	
	IUIEventSystemFactory* pFactory = IUIEventSystemFactory::GetFirst();
	while (pFactory)
	{
		TUIEventSystemPtr pGameEvent = pFactory->Create();
		CRY_ASSERT_MESSAGE(pGameEvent != NULL, "Invalid IUIEventSystemFactory!");
		const char* name = pGameEvent->GetTypeName();
		TUIEventSystems::const_iterator it = m_EventSystems.find(name);
		if(it == m_EventSystems.end())
		{
			m_EventSystems[name] = pGameEvent;
		}
		else
		{
			string str;
			str.Format("IUIGameEventSystem \"%s\" already exists!", name);
			CRY_ASSERT_MESSAGE(false, str.c_str());
		}
		pFactory = pFactory->GetNext();
	}

	TUIEventSystems::const_iterator it = m_EventSystems.begin();
	TUIEventSystems::const_iterator end = m_EventSystems.end();
	for (;it != end; ++it)
	{
		it->second->InitEventSystem();
	}

	InitSound();

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener( this, "CUIManager");
	g_pGame->GetIGameFramework()->RegisterListener(this, "CUIManager", FRAMEWORKLISTENERPRIORITY_HUD);
	m_bRegistered = true;
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::Shutdown()
{
	TUIEventSystems::const_iterator it = m_EventSystems.begin();
	TUIEventSystems::const_iterator end = m_EventSystems.end();
	for (;it != end; ++it)
		it->second->UnloadEventSystem();
	m_EventSystems.clear();

	if (m_bRegistered)
	{
		if (gEnv->pSystem && gEnv->pSystem->GetISystemEventDispatcher())
			gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener( this );
		if (g_pGame && g_pGame->GetIGameFramework())
			g_pGame->GetIGameFramework()->UnregisterListener(this);
	}

	SAFE_DELETE(m_pWarningManager);
	SAFE_DELETE(m_pOptions);
	SAFE_DELETE(m_p2DRendUtils);
	SAFE_DELETE(m_pScreenLayoutMan);
	SAFE_DELETE(m_pHudSilhouettes);
	SAFE_DELETE(m_pMOSystem);
	
	m_pCVars->UnregisterConsoleCommandsAndVars();
	SAFE_DELETE(m_pCVars);

	m_bRegistered = false;	

	CUIInput::Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::GetMemoryUsage( ICrySizer *pSizer ) const
{
	SIZER_SUBCOMPONENT_NAME(pSizer, "CUIManager");
	pSizer->AddObject(this, sizeof(*this));

	pSizer->Add( *m_pWarningManager );
	pSizer->Add( *m_pOptions );
	pSizer->Add( *m_p2DRendUtils );
	pSizer->AddObject( m_pScreenLayoutMan );
	pSizer->AddObject( m_pHudSilhouettes );
	pSizer->AddObject( m_pCVars );
	pSizer->AddObject( m_pMOSystem );
	
	// TODO
// 	TUIEventSystems::const_iterator it = m_EventSystems.begin();
// 	TUIEventSystems::const_iterator end = m_EventSystems.end();
// 	for (;it != end; ++it)
// 		it->second->GetMemoryUsage(pSizer);

}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::InitGameType(bool multiplayer, bool fromInit)
{
	// TODO?
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::PostSerialize()
{
	// TODO?
}

/////////////////////////////////////////////////////////////////////////////////////
IUIGameEventSystem* CUIManager::GetUIEventSystem(const char* type) const
{
	TUIEventSystems::const_iterator it = m_EventSystems.find(type);
	assert(it != m_EventSystems.end());
	return it != m_EventSystems.end() ? it->second.get() : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::OnPostUpdate(float fDeltaTime)
{
	TUIEventSystems::const_iterator it = m_EventSystems.begin();
	TUIEventSystems::const_iterator end = m_EventSystems.end();
	for (;it != end; ++it)
	{
		it->second->OnUpdate(fDeltaTime);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ProcessViewParams(const SViewParams &viewParams)
{
	TUIEventSystems::const_iterator it = m_EventSystems.begin();
	TUIEventSystems::const_iterator end = m_EventSystems.end();
	for (;it != end; ++it)
	{
		it->second->UpdateView(viewParams);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
bool CUIManager::RegisterControlSchemeListener(IUIControlSchemeListener* pListener)
{
	return m_controlSchemeListeners.Add(pListener);
}

/////////////////////////////////////////////////////////////////////////////////////
bool CUIManager::UnregisterControlSchemeListener(IUIControlSchemeListener* pListener)
{
	if (m_controlSchemeListeners.Contains(pListener))
	{
		m_controlSchemeListeners.Remove(pListener);
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ClearControlSchemeListeners()
{
	m_controlSchemeListeners.Clear();
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::SetDefaultControlScheme()
{
	EControlScheme defaultControlScheme;

#if CRY_PLATFORM_ORBIS
	defaultControlScheme = eControlScheme_PS4Controller;
#elif CRY_PLATFORM_DURANGO
	defaultControlScheme = eControlScheme_XBoxOneController;
#else
	defaultControlScheme = eControlScheme_Keyboard;
#endif

	SetCurControlScheme(defaultControlScheme);
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::SetCurControlScheme( const EControlScheme controlScheme )
{
	if (GetCurControlScheme() == controlScheme)
		return;

	const EControlScheme prevControlScheme = m_curControlScheme;
	m_curControlScheme = controlScheme;

	SHUDEvent hudEvent(eHUDEvent_OnControlSchemeSwitch);
	hudEvent.AddData(SHUDEventData((int)controlScheme));
	hudEvent.AddData(SHUDEventData((int)prevControlScheme));
	CHUDEventDispatcher::CallEvent(hudEvent);

	// Notify listeners (Msg3D entities use this currently)
	for (TUIControlSchemeListeners::Notifier notifier(m_controlSchemeListeners); notifier.IsValid(); notifier.Next())
	{
		bool bHandled = notifier->OnControlSchemeChanged(controlScheme);
		if (bHandled) // Allow blocking
		{
			break;
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::OnSystemEvent( ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam )
{

	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		InitSound();
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
		ShutdownSound();
		break;
	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
	case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		{
			if (event == ESYSTEM_EVENT_LEVEL_GAMEPLAY_START || wparam == 1)
			{
				ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
				if(pLevel)
				{
					m_pMOSystem->LoadLevelObjectives(pLevel->GetPath(), true);
				}
			}
			if (event == ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED)
			{
				// Make sure the menu is ready to be called
				CUIMenuEvents* pMenuEvents = UIEvents::Get<CUIMenuEvents>();
				if (gEnv && gEnv->pGameFramework && gEnv->pGameFramework->IsGameStarted() && pMenuEvents && pMenuEvents->IsIngameMenuStarted())
					pMenuEvents->DisplayIngameMenu(false);
			}
		}
		break;
	}
}


/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::InitSound()
{
	REINST("UI listener");
	if (m_soundListener == INVALID_ENTITYID)
	{
		//m_soundListener = gEnv->pSoundSystem->CreateListener();
	}
	
	if (m_soundListener != INVALID_ENTITYID)
	{
		/*IAudioListener* const pListener = gEnv->pSoundSystem->GetListener(m_soundListener);

		if (pListener)
		{
			pListener->SetRecordLevel(1.0f);
			pListener->SetActive(true);
		}*/
	}
}

void CUIManager::ShutdownSound()
{
	/*if (m_soundListener != INVALID_ENTITYID)
	{
	gEnv->pSoundSystem->RemoveListener(m_soundListener);
	m_soundListener = INVALID_ENTITYID;
	}*/
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ActivateState(const char* state)
{

}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ActivateStateImmediate(const char* state)
{

}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ActivateDefaultState()
{

}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ActivateDefaultStateImmediate()
{

}

/////////////////////////////////////////////////////////////////////////////////////
bool CUIManager::IsLoading()
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
bool CUIManager::IsInMenu()
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
bool CUIManager::IsPreGameDone()
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
void CUIManager::ForceCompletePreGame()
{
}

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
EGameRulesMissionObjectives SGameRulesMissionObjectiveInfo::GetIconId(const char* nameOrNumber)
{
	const int nameAsNumber = atoi(nameOrNumber);
	if( INT_MAX != nameAsNumber && INT_MIN != nameAsNumber && 0 != nameAsNumber )
	{// we have a number
		return static_cast<EGameRulesMissionObjectives>(nameAsNumber);
	}	

	return EGRMO_Unknown;
}

