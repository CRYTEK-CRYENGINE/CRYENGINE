// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUIActionEvents.cpp
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "FlashUIActionEvents.h"

#include <ILevelSystem.h>
#include <IPlayerProfiles.h>

CFlashUIActionEvents::CFlashUIActionEvents()
	: m_pGameFramework(NULL)
	, m_pLevelSystem(NULL)
	, m_pUIEvents(NULL)
	, m_pUIFunctions(NULL)
{
	m_pGameFramework = gEnv->pGameFramework;
	CRY_ASSERT_MESSAGE(m_pGameFramework, "IGameFramework is not initialized, crash can follow!");

	m_pLevelSystem = m_pGameFramework ? m_pGameFramework->GetILevelSystem() : NULL;
	CRY_ASSERT_MESSAGE(m_pLevelSystem, "ILevelSystem is not initialized, crash can follow!");

	CRY_ASSERT_MESSAGE(gEnv->pFlashUI, "No FlashUI extension available!");
	if (gEnv->pFlashUI)
	{
		// system events (notify ui)
		m_pUIFunctions = gEnv->pFlashUI->CreateEventSystem("System", IUIEventSystem::eEST_SYSTEM_TO_UI);
		m_eventSender.Init(m_pUIFunctions);
		{
			SUIEventDesc eventDesc("OnSystemStarted", "Triggered if system is started (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_Bool>("AutoLevelLoad", "True if launcher loads a level after startup");
			eventDesc.SetDynamic("Levels", "Array with all available levels");
			m_eventSender.RegisterEvent<eUIE_OnSystemStarted>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnSystemShutdown", "OnSystemShutdown", "Triggered if system shuts down (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnSystemShutdown>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnLoadingStart", "OnLoadingStart", "Triggered if level is loading (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Level", "Localization label of the level name");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Video", "Loading screen video file");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Image", "Loading screen image");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Text", "Loading screen text");
			m_eventSender.RegisterEvent<eUIE_OnLoadingStart>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnLoadingProgress", "OnLoadingProgress", "Triggered during loading progress (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_Int>("Progress", "Loading Progress");
			m_eventSender.RegisterEvent<eUIE_OnLoadingProgress>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnLoadingComplete", "OnLoadingComplete", "Triggered if loading is complete (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Level", "Localization label of the level name");
			m_eventSender.RegisterEvent<eUIE_OnLoadingComplete>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnLoadingError", "OnLoadingError", "Triggered if an error occurred during loading (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnLoadingError>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnGameplayStarted", "OnGameplayStarted", "Triggered if gameplay starts (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnGameplayStarted>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnGameplayEnded", "OnGameplayEnded", "Triggered if gameplay ends (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnGameplayEnded>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnUnloadStart", "OnUnloadStart", "Triggered if level unload starts (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Level", "Localization label of the level name");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Image", "Unloading screen image");
			m_eventSender.RegisterEvent<eUIE_OnUnloadStart>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnUnloadComplete", "OnUnloadComplete", "Triggered if level unload is completed (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnUnloadComplete>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnConnect", "OnConnect", "Triggered if connect to a server (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Server", "Name of server");
			m_eventSender.RegisterEvent<eUIE_OnConnect>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnDisconnect", "OnDisconnect", "Triggered if connect to a server (Not in Editor!)");
			eventDesc.AddParam<SUIParameterDesc::eUIPT_String>("Error", "Error Message");
			m_eventSender.RegisterEvent<eUIE_OnDisconnect>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnGamePause", "OnGamePause", "Triggered game is paused (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnGamePause>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnGameResume", "OnGameResume", "Triggered if game is resumed (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnGameResume>(eventDesc);
		}

		{
			SUIEventDesc eventDesc("OnReload", "OnReload", "Triggered if ui is reloaded (Not in Editor!)");
			m_eventSender.RegisterEvent<eUIE_OnReload>(eventDesc);
		}

		// system functions (called by ui)
		m_pUIEvents = gEnv->pFlashUI->CreateEventSystem("System", IUIEventSystem::eEST_UI_TO_SYSTEM);
		m_eventDispatcher.Init(m_pUIEvents, this, "CFlashUIActionEvents");
		{
			SUIEventDesc eventDesc("UnloadAllElements", "UnloadAllElements", "Unloads all UI Elements (Skip Elements that are defined in the Array input)");
			eventDesc.SetDynamic("Elements", "List of Elements that are excluded (Separated by '|')");
			m_eventDispatcher.RegisterEvent(eventDesc, &CFlashUIActionEvents::OnUnloadAllElements);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
CFlashUIActionEvents::~CFlashUIActionEvents()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// UI Functions //////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnSystemStart()
{
	if (gEnv->IsEditor()) return;

	bool bMapCommand = false;

	if (gEnv->pSystem)
	{
		const ICmdLineArg* pMapArg = gEnv->pSystem->GetICmdLine()->FindArg(eCLAT_Post, "map");
		bMapCommand = pMapArg != NULL;
	}

	SUIArguments args;
	args.AddArgument(bMapCommand);

	if (m_pLevelSystem)
	{
		int i = 0;
		while (ILevelInfo* pLevelInfo = m_pLevelSystem->GetLevelInfo(i++))
			args.AddArgument(pLevelInfo->GetName());
	}

	m_eventSender.SendEvent<eUIE_OnSystemStarted>(args);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnSystemShutdown()
{
	m_eventSender.SendEvent<eUIE_OnSystemShutdown>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnLoadingStart(ILevelInfo* pLevel)
{
	if (pLevel && !gEnv->IsEditor())
	{
		string videoFile;
		string imageFile;
		string text;
		bool ok = pLevel->GetAttribute("loading_video", videoFile);
		ok &= pLevel->GetAttribute("loading_image", imageFile);
		ok &= pLevel->GetAttribute("loading_text", text);
		if (!ok)
		{
			gEnv->pLog->LogWarning("Level does not have loading screen info");
		}

		m_eventSender.SendEvent<eUIE_OnLoadingStart>(pLevel->GetDisplayName(), videoFile, imageFile, text);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnLoadingProgress(ILevelInfo* pLevel, int progressAmount)
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnLoadingProgress>(progressAmount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnLoadingComplete()
{
	if (gEnv->IsEditor()) return;

	const ILevelInfo* pLevelInfo = m_pLevelSystem ? m_pLevelSystem->GetCurrentLevel() : NULL;
	string label = pLevelInfo ? pLevelInfo->GetDisplayName() : "<UNDEFINED>";

	m_eventSender.SendEvent<eUIE_OnLoadingComplete>(label);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnLoadingError(ILevelInfo* pLevel, const char* error)
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnLoadingError>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnGameplayStarted()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnGameplayStarted>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnGameplayEnded()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnGameplayEnded>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// @param1: Level
//
void CFlashUIActionEvents::OnLevelUnload()
{
	if (gEnv->IsEditor()) return;

	const ILevelInfo* pLevelInfo = m_pLevelSystem ? m_pLevelSystem->GetCurrentLevel() : NULL;
	string label = pLevelInfo ? pLevelInfo->GetDisplayName() : "<UNDEFINED>";
	string imageFile;

	if (pLevelInfo)
	{
		pLevelInfo->GetAttribute("unloading_image", imageFile);
		if (imageFile.empty())
			pLevelInfo->GetAttribute("loading_image", imageFile);
	}

	m_eventSender.SendEvent<eUIE_OnUnloadStart>(label, imageFile);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CFlashUIActionEvents::OnUnloadComplete()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnUnloadComplete>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// @param1: Server
//
void CFlashUIActionEvents::OnConnect(const char* server)
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnConnect>(server);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// @param1: Error
//
void CFlashUIActionEvents::OnDisconnect(const char* error)
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnDisconnect>(error);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CFlashUIActionEvents::OnGameplayPaused()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnGamePause>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void CFlashUIActionEvents::OnGameplayResumed()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnGameResume>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnReload()
{
	if (gEnv->IsEditor()) return;

	m_eventSender.SendEvent<eUIE_OnReload>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////// UI Events ///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFlashUIActionEvents::OnUnloadAllElements(const SUIArguments& elements)
{
	if (!gEnv->pFlashUI) return;

	for (int i = 0; i < gEnv->pFlashUI->GetUIElementCount(); ++i)
	{
		IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
		bool bSkipElement = false;
		for (int k = 0; k < elements.GetArgCount(); ++k)
		{
			string element;
			elements.GetArg(k, element);
			if (element == pElement->GetName())
			{
				bSkipElement = true;
				break;
			}
		}
		if (!bSkipElement)
		{
			IUIElementIteratorPtr instances = pElement->GetInstances();
			while (IUIElement* pInstance = instances->Next())
			{
				pInstance->Unload();
			}
		}
	}
}
