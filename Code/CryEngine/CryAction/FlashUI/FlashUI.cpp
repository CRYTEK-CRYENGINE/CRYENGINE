// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlashUI.h"
#include "FlashUIElement.h"
#include "FlashUIAction.h"
#include "FlashUIElementNodes.h"
#include "FlashUIEventNodes.h"
#include "FlashUIActionEvents.h"

#include <IPlayerProfiles.h>
#include <CryString/StringUtils.h>
#include <CrySystem/VR/IHMDManager.h>
#include <CryRenderer/IStereoRenderer.h>
#include <CryRenderer/IScaleform.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>

#define FLASH_UIELEMENTS_FOLDER "UIElements"
#define FLASH_UIACTION_FOLDER   "UIActions"
#define FLASH_PRELOAD_XML       "TexturePreload.xml"

AllocateConstIntCVar(CFlashUI, CV_gfx_draw);
AllocateConstIntCVar(CFlashUI, CV_gfx_debugdraw);
AllocateConstIntCVar(CFlashUI, CV_gfx_uiaction_enable);
AllocateConstIntCVar(CFlashUI, CV_gfx_uiaction_log);
AllocateConstIntCVar(CFlashUI, CV_gfx_loadtimethread);
AllocateConstIntCVar(CFlashUI, CV_gfx_reloadonlanguagechange);
AllocateConstIntCVar(CFlashUI, CV_gfx_uievents_editorenabled);
AllocateConstIntCVar(CFlashUI, CV_gfx_ampserver);
int CFlashUI::CV_gfx_enabled;
float CFlashUI::CV_gfx_inputevents_triggerstart;
float CFlashUI::CV_gfx_inputevents_triggerrepeat;

ICVar* CFlashUI::CV_gfx_uiaction_log_filter;
ICVar* CFlashUI::CV_gfx_uiaction_folder;

//------------------------------------------------------------------------------------
string CreateDisplayName(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char result[1024];
	cry_vsprintf(result, format, args);
	va_end(args);
	return result;
}

#if !defined (_RELEASE) || defined(ENABLE_UISTACK_DEBUGGING)
void RenderDebugInfo();
#endif
#ifdef ENABLE_UISTACK_DEBUGGING
void RenderStackDebugInfo(bool, const char*, int);
#endif
//-------------------------------------------------------------------
CRYREGISTER_SINGLETON_CLASS(CFlashUI)

CFlashUI::CFlashUI()
	: m_pUIActionManager(new CUIActionManager())
	, m_iWidth(1)
	, m_iHeight(1)
	, m_bLoadtimeThread(false)
	, m_bSortedElementsInvalidated(true)
	, m_ScreenSizeCB(NULL)
	, m_LogCallback(NULL)
	, m_plattformCallback(NULL)
	, m_pFlashUIActionEvents(NULL)
	, m_systemState(eSS_NoLevel)
	, m_fLastAdvance(0)
	, m_modules(8)
	, m_lastTimeTriggered(0)
	, m_bHudVisible(true)
{
	DefineConstIntCVarName("gfx_draw", CV_gfx_draw, 1, VF_NULL, "Draw UI Elements");
	DefineConstIntCVarName("gfx_debugdraw", CV_gfx_debugdraw, 0, VF_NULL, "Display UI Elements debug info.\n" \
	  "0=Disabled"                                                                                            \
	  "1=UIElements"                                                                                          \
	  "2=UIActions"                                                                                           \
	  "4=UIActions"                                                                                           \
	  "12=UIStack per UI FG");
	DefineConstIntCVarName("gfx_uiaction_log", CV_gfx_uiaction_log, 0, VF_NULL, "Log UI Actions");
	DefineConstIntCVarName("gfx_uiaction_enable", CV_gfx_uiaction_enable, 1, VF_NULL, "Enables UI Actions");
	DefineConstIntCVarName("gfx_loadtimethread", CV_gfx_loadtimethread, 1, VF_NULL, "Enables threaded rendering while loading");
	DefineConstIntCVarName("gfx_reloadonlanguagechange", CV_gfx_reloadonlanguagechange, 1, VF_NULL, "Automatically reloads all UIElements on language change");
	DefineConstIntCVarName("gfx_uievents_editorenabled", CV_gfx_uievents_editorenabled, 1, VF_NULL, "Enabled UI->System events in editor (Disabled per default! handle with care!)");
	DefineConstIntCVarName("gfx_ampserver", CV_gfx_ampserver, 0, VF_NULL, "Enables AMP flash profiling");

	REGISTER_CVAR3("gfx_enabled", CV_gfx_enabled, 1, VF_NULL, "Enables the general FlashUI.");

	REGISTER_CVAR2("gfx_inputevents_triggerstart", &CV_gfx_inputevents_triggerstart, 0.3f, VF_NULL, "Time in seconds to wait until input key triggering starts");
	REGISTER_CVAR2("gfx_inputevents_triggerrepeat", &CV_gfx_inputevents_triggerrepeat, 0.05f, VF_NULL, "Time in seconds to wait between each input key trigger");

	CV_gfx_uiaction_log_filter = REGISTER_STRING("gfx_uiaction_log_filter", "", VF_NULL, "Filter for logging\n" \
	  "<string> only log messages\n"                                                                            \
	  "-<string> don't log message\n"                                                                           \
	  "<filter1>|<filter2> to use more filters");
	CV_gfx_uiaction_folder = REGISTER_STRING("gfx_uiaction_folder", "Libs/UI/", VF_NULL, "Default folder for UIActions");

	REGISTER_COMMAND("gfx_reload_all", CFlashUI::ReloadAllElements, VF_NULL, "Reloads all UI Elements");
}

//-------------------------------------------------------------------
void CFlashUI::Init()
{
	LOADING_TIME_PROFILE_SECTION;
	if (CV_gfx_enabled)
	{
		m_pFlashUIActionEvents = new CFlashUIActionEvents();

		LoadElements();
		CreateNodes();
	}
}

//-------------------------------------------------------------------
bool CFlashUI::PostInit()
{
	LOADING_TIME_PROFILE_SECTION;
	bool res = true;

	if (CV_gfx_enabled)
	{
		LoadActions();

		for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
		{
			res &= (*iter)->Init(false);
		}
		for (TUIActionsLookup::iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
		{
			res &= (*iter)->Init();
		}

		RegisterListeners();

		for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
			notifier->Init();

		PreloadTextures();
	}
	return res;
}

//-------------------------------------------------------------------
void CFlashUI::Shutdown()
{
	StopRenderThread();

	for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
		notifier->Shutdown();

	if (gEnv->pHardwareMouse && gEnv->pInput)
	{
		gEnv->pHardwareMouse->RemoveListener(this);
		gEnv->pInput->RemoveEventListener(this);
	}
	ClearNodes();
	ClearActions();
	ClearElements();
	UnregisterListeners();

	for (TUIEventSystemMap::const_iterator iter = m_eventSystemsUiToSys.begin(); iter != m_eventSystemsUiToSys.end(); ++iter)
		delete iter->second;

	for (TUIEventSystemMap::const_iterator iter = m_eventSystemsSysToUi.begin(); iter != m_eventSystemsSysToUi.end(); ++iter)
		delete iter->second;

	SAFE_DELETE(m_pUIActionManager);
	SAFE_DELETE(m_pFlashUIActionEvents);

	gEnv->pFlashUI = nullptr;
}

//------------------------------------------------------------------------------------
void CFlashUI::RegisterListeners()
{
	CRY_ASSERT_MESSAGE(gEnv->pHardwareMouse && gEnv->pInput, "Unable to register as input listener!");
	if (gEnv->pHardwareMouse && gEnv->pInput)
	{
		gEnv->pHardwareMouse->AddListener(this);
		gEnv->pInput->AddEventListener(this);
	}

	CRY_ASSERT_MESSAGE(gEnv->pGameFramework, "Unable to register as Framework listener!");
	if (gEnv->pGameFramework)
	{
		gEnv->pGameFramework->RegisterListener(this, "FlashUI", FRAMEWORKLISTENERPRIORITY_HUD);
		CRY_ASSERT_MESSAGE(gEnv->pGameFramework->GetILevelSystem(), "Unable to register as levelsystem listener!");
		if (gEnv->pGameFramework->GetILevelSystem())
		{
			gEnv->pGameFramework->GetILevelSystem()->AddListener(this);
		}
	}

	CRY_ASSERT_MESSAGE(gEnv->pSystem, "Unable to register as system listener!");
	if (gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CFlashUI");
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::UnregisterListeners()
{
	if (gEnv->pGameFramework)
	{
		gEnv->pGameFramework->UnregisterListener(this);
		if (gEnv->pGameFramework->GetILevelSystem())
			gEnv->pGameFramework->GetILevelSystem()->RemoveListener(this);
	}

	if (gEnv->pSystem)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	if (gEnv->pHardwareMouse && gEnv->pInput)
	{
		gEnv->pHardwareMouse->RemoveListener(this);
		gEnv->pInput->RemoveEventListener(this);
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::Reload()
{
	LOADING_TIME_PROFILE_SECTION;
	UIACTION_LOG("FlashUI reload start");

	ReloadAllBootStrapper();
	ResetActions();
	ReloadScripts();

	for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
		notifier->Reload();

	gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_FRONTEND_RELOADED, 0, 0);

	UIACTION_LOG("FlashUI reload end");
}

//-------------------------------------------------------------------
void CFlashUI::ReloadAll()
{
	if (gEnv->IsEditor())
	{
		for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
			if (!notifier->EditorAllowReload())
				return;

		LoadElements();
		CreateNodes();
		gEnv->pFlowSystem->ReloadAllNodeTypes();
		LoadActions();
		Reload();

		for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
			notifier->EditorReload();

		return;
	}

	Reload();
}

//-------------------------------------------------------------------
void CFlashUI::Update(float fDeltaTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_bLoadtimeThread)
		return;

	const bool isEditor = gEnv->IsEditor();

	if (!isEditor || gEnv->pGameFramework->IsGameStarted())
	{
		if (CV_gfx_reloadonlanguagechange == 1)
			CheckLanguageChanged();

		CheckResolutionChange();
	}

	for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
		notifier->UpdateModule(fDeltaTime);

	const TSortedElementList& sortedElements = GetSortedElements();

	for (TSortedElementList::const_iterator iter = sortedElements.begin(); iter != sortedElements.end(); ++iter)
	{
		// UIElements with eFUI_NO_AUTO_UPDATE flag are advanced & rendered manually.
		if (iter->second->HasFlag(IUIElement::eFUI_NO_AUTO_UPDATE))
			continue;

		iter->second->Update(fDeltaTime);
		if (CV_gfx_draw == 1)
		{
			if (!m_bHudVisible && iter->second->HasFlag(IUIElement::eFUI_IS_HUD))
				continue;

			iter->second->Render();
		}
	}

	ResetDirtyFlags();

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	// amp server support
	if (gEnv && gEnv->pScaleformHelper)
	{
		static bool ampEnabled = false;
		if (CV_gfx_ampserver == 1)
		{
			if (!ampEnabled)
			{
				gEnv->pScaleformHelper->SetAmpEnabled(true);
				ampEnabled = true;
			}
		}
		else if (ampEnabled)
		{
			gEnv->pScaleformHelper->SetAmpEnabled(false);
			ampEnabled = false;
		}

		// always tick amp if we compiled support in, else memory will accumulate and never be freed
		gEnv->pScaleformHelper->AmpAdvanceFrame();
	}

#endif

#if !defined (_RELEASE) || defined(ENABLE_UISTACK_DEBUGGING)
	// debug info
	if (CV_gfx_debugdraw > 0)
	{
		RenderDebugInfo();
	}
#endif
}

//------------------------------------------------------------------------------------
void CFlashUI::InvalidateSortedElements()
{
	m_bSortedElementsInvalidated = true;
}

//------------------------------------------------------------------------------------
const CFlashUI::TSortedElementList& CFlashUI::GetSortedElements()
{
	if (m_bSortedElementsInvalidated)
		UpdateSortedElements();
	return m_sortedElements;
}

//------------------------------------------------------------------------------------
void CFlashUI::UpdateSortedElements()
{
	m_sortedElements.clear();
	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
	{
		IUIElementIteratorPtr instances = (*iter)->GetInstances();
		while (IUIElement* pInstance = instances->Next())
		{
			if (pInstance->IsVisible())
			{
				m_sortedElements.insert(std::make_pair(pInstance->GetLayer(), pInstance));
			}
		}
	}
	m_bSortedElementsInvalidated = false;
}

//------------------------------------------------------------------------------------
void CFlashUI::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	LOADING_TIME_PROFILE_SECTION;
	switch (event)
	{
	case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
		{
			m_pFlashUIActionEvents->OnSystemStart();
			UpdateFG();
			m_systemState = eSS_NoLevel;
		}
		break;

	case ESYSTEM_EVENT_FAST_SHUTDOWN:
		{
			StopRenderThread();
			ReleasePreloadedTextures(true);
			m_pFlashUIActionEvents->OnSystemShutdown();
			UpdateFG();
			m_systemState = eSS_NoLevel;
		}
		break;

	case ESYSTEM_EVENT_GAME_PAUSED:
		{
			if (m_systemState == eSS_GameStarted)
			{
				m_pFlashUIActionEvents->OnGameplayPaused();
			}
		}
		break;

	case ESYSTEM_EVENT_GAME_RESUMED:
		{
			if (m_systemState == eSS_GameStarted)
			{
				m_pFlashUIActionEvents->OnGameplayResumed();
			}
		}
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
		{
			m_fLastAdvance = gEnv->pTimer->GetAsyncCurTime();
			if (m_systemState == eSS_NoLevel)
			{
				m_pFlashUIActionEvents->OnLoadingStart((ILevelInfo*)wparam);
				UpdateFG();
				StartRenderThread();
			}

			m_systemState = eSS_Loading;
		}
		break;

	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
		{
			if (m_systemState == eSS_LoadingDone)
			{
				m_pFlashUIActionEvents->OnGameplayStarted();
			}
			m_systemState = eSS_GameStarted;
		}
		break;

	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		{
			if (m_systemState == eSS_GameStarted)
			{
				m_pFlashUIActionEvents->OnLevelUnload();
				UpdateFG();
				Update(0.1f);
				StartRenderThread();

				UIACTION_LOG("FlashUI unload start");
				for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
				{
					CFlashUIElement* pElement = (CFlashUIElement*)*iter;
					if (pElement->HasFlag(IUIElement::eFUI_FORCE_NO_UNLOAD))
						continue;
					pElement->Unload(true);
				}
				for (TUIModules::Notifier notifier(m_modules); notifier.IsValid(); notifier.Next())
					notifier->Reset();

				ResetActions();
				ReleasePreloadedTextures();

				UIACTION_LOG("FlashUI unload end");
			}
			m_systemState = eSS_Unloading;
		}
		break;

	case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
		{
			if (m_systemState == eSS_Unloading)
			{
				StopRenderThread();
				PreloadTextures();

				m_pFlashUIActionEvents->OnUnloadComplete();
				UpdateFG();

			}
			m_systemState = eSS_NoLevel;
		}
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			StopRenderThread();

			if (m_systemState == eSS_Loading)
			{
				m_pFlashUIActionEvents->OnLoadingComplete();
				UpdateFG();
			}

			ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->GetCurrentLevel();
			PreloadTextures(pLevel ? pLevel->GetName() : "");

			m_systemState = eSS_LoadingDone;

			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_GAMEPLAY_START, 0, 0);
		}
		break;

	case ESYSTEM_EVENT_FRONTEND_RELOADED:
		{
			m_pFlashUIActionEvents->OnReload();
		}
		break;
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::OnActionEvent(const SActionEvent& event)
{
	switch (event.m_event)
	{
	case eAE_disconnected:
		{
			m_pFlashUIActionEvents->OnDisconnect(event.m_description);
			if (m_systemState == eSS_GameStarted)
				m_pFlashUIActionEvents->OnGameplayEnded();

			UpdateFG();
		}
		break;

	case eAE_connected:
		{
			m_pFlashUIActionEvents->OnConnect(event.m_description);
			UpdateFG();
		}
		break;
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::OnLevelNotFound(const char* levelName)
{
	StopRenderThread();

	if (m_systemState == eSS_Loading)
	{
		string msg;
		msg.Format("Level %s not found", levelName);
		m_pFlashUIActionEvents->OnLoadingError(NULL, msg.c_str());
		UpdateFG();
	}
	m_systemState = eSS_NoLevel;
}

//------------------------------------------------------------------------------------
void CFlashUI::OnLoadingError(ILevelInfo* pLevel, const char* error)
{
	StopRenderThread();

	if (m_systemState == eSS_Loading)
	{
		m_pFlashUIActionEvents->OnLoadingError(pLevel, error);
		UpdateFG();
	}
	m_systemState = eSS_NoLevel;
}

//------------------------------------------------------------------------------------
void CFlashUI::OnLoadingProgress(ILevelInfo* pLevel, int progressAmount)
{
	if (m_systemState == eSS_Loading)
	{
		if (CV_gfx_loadtimethread == 0 && m_bLoadtimeThread == false)
		{
			uint32 nRecursionLevel = 0;
			gEnv->pRenderer->EF_Query(EFQ_RecurseLevel, nRecursionLevel);
			const bool bStandAlone = (nRecursionLevel <= 0);
			if (bStandAlone)
			{
				gEnv->pSystem->RenderBegin(SDisplayContextKey{});
			}

			const float currTime = gEnv->pTimer->GetAsyncCurTime();
			OnPostUpdate(currTime - m_fLastAdvance);
			m_fLastAdvance = currTime;

			if (bStandAlone)
				gEnv->pSystem->RenderEnd();
		}
		m_pFlashUIActionEvents->OnLoadingProgress(pLevel, progressAmount);
		UpdateFG();
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::LoadtimeUpdate(float fDeltaTime)
{
	LOADING_TIME_PROFILE_SECTION

	if (m_bSortedElementsInvalidated)
	{
		m_loadtimePlayerList.clear();

		for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
		{
			IUIElementIteratorPtr instances = (*iter)->GetInstances();
			while (IUIElement* pInstance = instances->Next())
			{
				if (pInstance->IsVisible())
					m_loadtimePlayerList.emplace_back(pInstance->GetFlashPlayer());
			}
		}

		m_bSortedElementsInvalidated = false;
	}

	for (TPlayerList::const_iterator it = m_loadtimePlayerList.begin(); it != m_loadtimePlayerList.end(); ++it)
	{
		(*it)->Advance(fDeltaTime);
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::LoadtimeRender()
{
	LOADING_TIME_PROFILE_SECTION

	if (CV_gfx_draw == 1)
	{
		IStereoRenderer* stereoRenderer = gEnv->pRenderer->GetIStereoRenderer();

		if (stereoRenderer->GetStereoEnabled())
			stereoRenderer->PrepareFrame();

		for (auto &pFlashPlayer : m_loadtimePlayerList)
		{
			auto p = pFlashPlayer;
			p->SetClearFlags(FRT_CLEAR_COLOR, Clr_Transparent);
			gEnv->pRenderer->FlashRenderPlayer(std::move(p));
		}

		if (stereoRenderer->GetStereoEnabled())
		{
			if (!stereoRenderer->IsMenuModeEnabled())
				stereoRenderer->DisplaySocialScreen();
			stereoRenderer->SubmitFrameToHMD();
		}
	}
}

//------------------------------------------------------------------------------------
#ifdef ENABLE_UISTACK_DEBUGGING
	#define DEBUG_FLUSH_STACK(loop)      RenderStackDebugInfo(true, "Finished", loop);
	#define DEBUG_ADD_STACK(label, loop) RenderStackDebugInfo(false, label, loop);
#else
	#define DEBUG_FLUSH_STACK
	#define DEBUG_ADD_STACK(label, loop)
#endif
void CFlashUI::UpdateFG()
{
	LOADING_TIME_PROFILE_SECTION;

	static bool isUpdating = false;
	CRY_ASSERT_MESSAGE(!isUpdating, "Recursive loop: trying to update UIAction FlowGraphs within update loop!");
	if (!isUpdating)
	{
		isUpdating = true;
		int loops = 0;
		do
		{
			m_pUIActionManager->Update();// preupdate
			DEBUG_ADD_STACK("Events and Actions", loops);

			int i = 0;
			while (CFlashUIAction* pAction = (CFlashUIAction*)GetUIAction(i++))
			{
				pAction->Update();
				if (CV_gfx_debugdraw & 8)
				{
					DEBUG_ADD_STACK(string().Format("FG %s", pAction->GetName()), loops);
				}
			}
			if (!(CV_gfx_debugdraw & 8))
			{
				DEBUG_ADD_STACK("Flowgraphs", loops);
			}

			m_pUIActionManager->Update();// post update
			DEBUG_ADD_STACK("PostUpdate", loops);

			if (++loops > 255)
			{
				CRY_ASSERT_MESSAGE(false, "stack constantly gets new events within this update loop, should not happen!");
				UIACTION_ERROR("Some UIActions are causing infinitive loop!");
				break;
			}
		}
		while (CUIFGStackMan::GetTotalStackSize() > 0);
		DEBUG_FLUSH_STACK(loops);
		isUpdating = false;
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::EnableEventStack(bool bEnable)
{
	CRY_ASSERT_MESSAGE(gEnv->IsEditor(), "Only allowed for editor!");
	if (gEnv->IsEditor())
	{
		CUIFGStackMan::SetEnabled(bEnable);
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::RegisterModule(IUIModule* pModule, const char* name)
{
	const bool ok = m_modules.Add(pModule, name);
	CRY_ASSERT_MESSAGE(ok, "Module already registered!");
}

//------------------------------------------------------------------------------------
void CFlashUI::UnregisterModule(IUIModule* pModule)
{
	CRY_ASSERT_MESSAGE(m_modules.Contains(pModule), "Module was never registered or already unregistered!");
	m_modules.Remove(pModule);
}

void CFlashUI::SetHudElementsVisible(bool bVisible)
{
	ICVar* pHudHidden = gEnv->pConsole->GetCVar("hud_hide");
	if (!pHudHidden)
	{
		m_bHudVisible = bVisible;
	}
	else
	{
		bool bHudHideOverride = pHudHidden->GetIVal() > 0;
		if (bHudHideOverride)
		{
			m_bHudVisible = false;
		}
		else
		{
			m_bHudVisible = bVisible;
		}
	}
}

//-------------------------------------------------------------------
void CFlashUI::OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (gEnv->pConsole->GetStatus())   // disable UI inputs when console is open
		return;
	int iButton = 0;
	SFlashCursorEvent::ECursorState evt;
	switch (eHardwareMouseEvent)
	{
	case HARDWAREMOUSEEVENT_RBUTTONDOWN:
		iButton = 1;
	case HARDWAREMOUSEEVENT_LBUTTONDOWN:
		evt = SFlashCursorEvent::eCursorPressed;
		break;
	case HARDWAREMOUSEEVENT_RBUTTONUP:
	case HARDWAREMOUSEEVENT_RBUTTONDOUBLECLICK: // IFlashPlayer does not support double click atm
		iButton = 1;
	case HARDWAREMOUSEEVENT_LBUTTONUP:
	case HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK: // IFlashPlayer does not support double click atm
		evt = SFlashCursorEvent::eCursorReleased;
		break;
	case HARDWAREMOUSEEVENT_MOVE:
		evt = SFlashCursorEvent::eCursorMoved;
		break;
	case HARDWAREMOUSEEVENT_WHEEL:
		evt = SFlashCursorEvent::eWheel;
		break;
	default:
		return;
	}
	SendFlashMouseEvent(evt, iX, iY, iButton, wheelDelta / 120.0f);
}

//-------------------------------------------------------------------
bool CFlashUI::OnInputEvent(const SInputEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (gEnv->pConsole->GetStatus())   // disable UI inputs when console is open
		return false;

	if (event.state == eIS_Down)
	{
		switch (event.keyId)
		{
		case eKI_LShift:
		case eKI_RShift:
		case eKI_LAlt:
		case eKI_RAlt:
		case eKI_LCtrl:
		case eKI_RCtrl:
			break;
		default:
			TriggerEvent(event);
		}
		return false;
	}

	if (event.state == eIS_Pressed || event.state == eIS_Released)
	{
		m_lastTimeTriggered = gEnv->pTimer->GetAsyncCurTime() + CV_gfx_inputevents_triggerstart;

		SFlashKeyEvent evt = MapToFlashKeyEvent(event);

		const TSortedElementList& sortedElements = GetSortedElements();

		for (TSortedElementList::const_reverse_iterator iter = sortedElements.rbegin(); iter != sortedElements.rend(); ++iter)
		{
			if (iter->second->HasFlag(IUIElement::eFUI_KEYEVENTS))
			{
				iter->second->SendKeyEvent(evt);
				if (iter->second->HasFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE))
					return false;
			}
		}
	}
	return false;
}

//------------------------------------------------------------------------------------
bool CFlashUI::OnInputEventUI(const SUnicodeEvent& event)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (gEnv->pConsole->GetStatus())   // disable UI inputs when console is open
		return false;

	SFlashCharEvent charEvent(event.inputChar);
	const TSortedElementList& sortedElements = GetSortedElements();

	for (TSortedElementList::const_reverse_iterator iter = sortedElements.rbegin(); iter != sortedElements.rend(); ++iter)
	{
		if (iter->second->HasFlag(IUIElement::eFUI_KEYEVENTS))
		{
			iter->second->SendCharEvent(charEvent);
			if (iter->second->HasFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE))
				return false;
		}
	}
	return false;
}

//-------------------------------------------------------------------
void CFlashUI::TriggerEvent(const SInputEvent& event)
{
	const float currtime = gEnv->pTimer->GetAsyncCurTime();
	if (currtime > m_lastTimeTriggered + CV_gfx_inputevents_triggerrepeat)
	{
		SInputEvent evt = event;
		evt.state = eIS_Released;
		OnInputEvent(evt);
		evt.state = eIS_Pressed;
		OnInputEvent(evt);
		m_lastTimeTriggered = currtime;
	}
}

//-------------------------------------------------------------------
void CFlashUI::DispatchControllerEvent(IUIElement::EControllerInputEvent event, IUIElement::EControllerInputState state, float value)
{
	if (IsVirtualKeyboardRunning()) return;

	if (event == IUIElement::eCIE_Action)
	{
		CreateMouseClick(state);
	}

	const TSortedElementList& sortedElements = GetSortedElements();

	for (TSortedElementList::const_reverse_iterator iter = sortedElements.rbegin(); iter != sortedElements.rend(); ++iter)
	{
		if (iter->second->HasFlag(IUIElement::eFUI_CONTROLLER_INPUT) && !iter->second->IsHiding())
		{
			iter->second->SendControllerEvent(event, state, value);
			if (iter->second->HasFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE))
				return;
		}
	}
}

//-------------------------------------------------------------------
void CFlashUI::CreateMouseClick(IUIElement::EControllerInputState state)
{
	SFlashCursorEvent::ECursorState evt;

	switch (state)
	{
	case IUIElement::eCIS_OnPress:
		evt = SFlashCursorEvent::eCursorPressed;
		break;
	case IUIElement::eCIS_OnRelease:
		evt = SFlashCursorEvent::eCursorReleased;
		break;
	default:
		return;
	}

	float fX, fY;
	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&fX, &fY);
	SendFlashMouseEvent(evt, (int) fX, (int) fY, 0, 0.f, true);
}

//-------------------------------------------------------------------
void CFlashUI::SendFlashMouseEvent(SFlashCursorEvent::ECursorState evt, int iX, int iY, int iButton /*= 0*/, float fWheel /*= 0.f*/, bool bFromController /*= false*/)
{
	const TSortedElementList& sortedElements = GetSortedElements();

	for (TSortedElementList::const_reverse_iterator iter = sortedElements.rbegin(); iter != sortedElements.rend(); ++iter)
	{
		if (iter->second->HasFlag(IUIElement::eFUI_MOUSEEVENTS) && (!bFromController || !iter->second->HasFlag(IUIElement::eFUI_CONTROLLER_INPUT))
#if !CRY_PLATFORM_DESKTOP
		    && iter->second->HasFlag(IUIElement::eFUI_CONSOLE_MOUSE)
#elif !defined(_RELEASE)
		    && (!gEnv->IsEditor() || GetCurrentPlatform() == IFlashUI::ePUI_PC || iter->second->HasFlag(IUIElement::eFUI_CONSOLE_MOUSE))
#endif
		    && !iter->second->IsHiding())
		{
			iter->second->SendCursorEvent(evt, iX, iY, iButton, fWheel);
			if (iter->second->HasFlag(IUIElement::eFUI_EVENTS_EXCLUSIVE))
				return;
		}
	}
}

//-------------------------------------------------------------------
bool CFlashUI::DisplayVirtualKeyboard(unsigned int flags, const char* title, const char* initialInput, int maxInputLength, IVirtualKeyboardEvents* pInCallback)
{
	IPlatformOS* pPlatformOS = gEnv->pSystem->GetPlatformOS();
	if (pPlatformOS && gEnv->pGameFramework)
	{
		unsigned int controllerIdx = gEnv->pGameFramework->GetIPlayerProfileManager()->GetExclusiveControllerDeviceIndex();
		if (controllerIdx != INVALID_CONTROLLER_INDEX)
			return pPlatformOS->KeyboardStart(controllerIdx, flags, title, initialInput, maxInputLength, pInCallback);
	}
	return false;
}

//-------------------------------------------------------------------
bool CFlashUI::IsVirtualKeyboardRunning()
{
	IPlatformOS* pPlatformOS = gEnv->pSystem->GetPlatformOS();
	if (pPlatformOS)
	{
		return pPlatformOS->KeyboardIsRunning();
	}
	return false;
}

//-------------------------------------------------------------------
bool CFlashUI::CancelVirtualKeyboard()
{
	IPlatformOS* pPlatformOS = gEnv->pSystem->GetPlatformOS();
	if (pPlatformOS)
	{
		return pPlatformOS->KeyboardCancel();
	}
	return false;
}

//-------------------------------------------------------------------
IUIActionManager* CFlashUI::GetUIActionManager() const
{
	return m_pUIActionManager;
}

//-------------------------------------------------------------------
void CFlashUI::LoadElements()
{
	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
		((CFlashUIElement*)*iter)->SetValid(false);

	string folder;
	folder.Format("%s/%s", CV_gfx_uiaction_folder->GetString(), FLASH_UIELEMENTS_FOLDER);
	LoadFromFile(folder.c_str(), "*.xml", &CFlashUI::LoadElementsFromFile);
	InvalidateSortedElements();

	//Reset the dirty flags so we don't
	//unload what we are about to load
	ResetDirtyFlags();
}

//-------------------------------------------------------------------
void CFlashUI::LoadActions()
{
	for (TUIActionsLookup::iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
		((CFlashUIAction*)*iter)->SetValid(false);

	string folder;
	folder.Format("%s/%s", CV_gfx_uiaction_folder->GetString(), FLASH_UIACTION_FOLDER);
	LoadFromFile(folder.c_str(), "*.xml", &CFlashUI::LoadFGActionFromFile);
	LoadFromFile(folder.c_str(), "*.lua", &CFlashUI::LoadLuaActionFromFile);
}

//-------------------------------------------------------------------
template<EUIObjectType T> bool IsTemplate()                     { return false; }
template<> bool                IsTemplate<eUOT_MovieClipTmpl>() { return true; }

template<EUIObjectType Type, class Item>
static void CreateMCFunctionNodes(std::vector<CFlashUiFlowNodeFactory*>& nodelist, IUIElement* pElement, Item* pItem, const string& path, bool fill = false)
{
	// prevent to fill "first row"
	if (fill)
	{
		const int fctcount = SUIGetDesc<eUOT_Functions, Item, int>::GetCount(pItem);
		for (int i = 0; i < fctcount; ++i)
		{
			const SUIEventDesc* pDesc = SUIGetDesc<eUOT_Functions, Item, int>::GetDesc(pItem, i);

			string category = path;
			category += ":";
			category += pDesc->sDisplayName;

			CFlashUIFunctionNode* pNode = new CFlashUIFunctionNode(pElement, category, pDesc, IsTemplate<Type>());
			CFlashUiFlowNodeFactory* pFlowNode = new CFlashUiFlowNodeFactory(pNode->GetCategory(), pNode);
			nodelist.push_back(pFlowNode);
		}
	}

	const int subcount = SUIGetDesc<Type, Item, int>::GetCount(pItem);
	for (int i = 0; i < subcount; ++i)
	{
		const SUIMovieClipDesc* pDesc = SUIGetDesc<Type, Item, int>::GetDesc(pItem, i);
		string newpath = path;
		newpath += ":";
		newpath += pDesc->sDisplayName;
		CreateMCFunctionNodes<eUOT_MovieClip>(nodelist, pElement, pDesc, newpath, true);
	}
}

//-------------------------------------------------------------------
void CFlashUI::CreateNodes()
{
	// create element nodes
	ClearNodes();

	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
	{
		if (!(*iter)->IsValid()) continue;

		int i = 0;
		while (const SUIEventDesc* pDesc = (*iter)->GetEventDesc(i++))
		{
			CFlashUIEventNode* pNode = new CFlashUIEventNode(*iter, CreateDisplayName("UI:Events:%s:%s", (*iter)->GetName(), pDesc->sDisplayName), pDesc);
			CFlashUiFlowNodeFactory* pFlowNode = new CFlashUiFlowNodeFactory(pNode->GetCategory(), pNode);
			m_UINodes.push_back(pFlowNode);
		}
		i = 0;
		while (const SUIEventDesc* pDesc = (*iter)->GetFunctionDesc(i++))
		{
			CFlashUIFunctionNode* pNode = new CFlashUIFunctionNode(*iter, CreateDisplayName("UI:Functions:%s:%s", (*iter)->GetName(), pDesc->sDisplayName), pDesc, false);
			CFlashUiFlowNodeFactory* pFlowNode = new CFlashUiFlowNodeFactory(pNode->GetCategory(), pNode);
			m_UINodes.push_back(pFlowNode);
		}

		CreateMCFunctionNodes<eUOT_MovieClip>(m_UINodes, *iter, *iter, CreateDisplayName("UI:Functions:%s", (*iter)->GetName()));
		CreateMCFunctionNodes<eUOT_MovieClipTmpl>(m_UINodes, *iter, *iter, CreateDisplayName("UI:Template:Functions:%s", (*iter)->GetName()));
	}

	// create event system nodes for eEST_SYSTEM_TO_UI
	IUIEventSystemIteratorPtr eventIter = CreateEventSystemIterator(IUIEventSystem::eEST_SYSTEM_TO_UI);
	string name;
	while (IUIEventSystem* pEventSystem = eventIter->Next(name))
	{
		int i = 0;
		while (const SUIEventDesc* pDesc = pEventSystem->GetEventDesc(i++))
		{
			CFlashUIBaseNodeCategory* pNode = new CFlashUIEventSystemEventNode(pEventSystem, CreateDisplayName("UI:Events:%s:%s", name.c_str(), pDesc->sDisplayName), pDesc);
			CFlashUiFlowNodeFactory* pFlowNode = new CFlashUiFlowNodeFactory(pNode->GetCategory(), pNode);
			m_UINodes.push_back(pFlowNode);
		}
	}

	// create event system nodes for eEST_UI_TO_SYSTEM
	eventIter = CreateEventSystemIterator(IUIEventSystem::eEST_UI_TO_SYSTEM);
	while (IUIEventSystem* pEventSystem = eventIter->Next(name))
	{
		int i = 0;
		while (const SUIEventDesc* pDesc = pEventSystem->GetEventDesc(i++))
		{
			CFlashUIBaseNodeCategory* pNode = new CFlashUIEventSystemFunctionNode(pEventSystem, CreateDisplayName("UI:Functions:%s:%s", name.c_str(), pDesc->sDisplayName), pDesc);
			CFlashUiFlowNodeFactory* pFlowNode = new CFlashUiFlowNodeFactory(pNode->GetCategory(), pNode);
			m_UINodes.push_back(pFlowNode);
		}
	}

	// Do nodes registration with flow graph system
	for (auto pNodeFactory : m_UINodes)
	{
		gEnv->pFlowSystem->RegisterType(pNodeFactory->GetNodeTypeName(),pNodeFactory);
	}
}

//-------------------------------------------------------------------
bool CFlashUI::LoadElementsFromFile(const char* sFileName)
{
	UIACTION_LOG("FlashUI parse element XML file %s", sFileName);
	XmlNodeRef xmlNode = GetISystem()->LoadXmlFromFile(sFileName);
	if (xmlNode)
	{
		const char* groupname = xmlNode->getAttr("name");

		for (int i = 0; i < xmlNode->getChildCount(); ++i)
		{
			XmlNodeRef element = xmlNode->getChild(i);

			const char* name = element->getAttr("name");
			if (name == NULL) continue;

			IUIElement* pElement = GetUIElement(name);
			if (!pElement)
			{
				UIACTION_LOG("FlashUI create UIElement %s", name);
				pElement = new CFlashUIElement(this);
				pElement->SetName(name);
				m_elements.push_back(pElement);
			}
			UIACTION_LOG("FlashUI read UIElement %s from xml %s", name, sFileName);
			if (!pElement->Serialize(element, true))
			{
				UIACTION_ERROR("FlashUI failed to load UIElement %s from xml %s", name, sFileName);
				return false;
			}

			pElement->SetGroupName(groupname ? groupname : "");
		}
		return true;
	}

	UIACTION_ERROR("Error in element xml file %s", sFileName);
	return false;
}

//-------------------------------------------------------------------
bool CFlashUI::LoadActionFromFile(const char* sFileName, IUIAction::EUIActionType type)
{
	switch (type)
	{
	case IUIAction::eUIAT_FlowGraph:
		return LoadFGActionFromFile(sFileName);
	case IUIAction::eUIAT_LuaScript:
		return LoadLuaActionFromFile(sFileName);
	}
	return false;
}
//-------------------------------------------------------------------
bool CFlashUI::LoadFGActionFromFile(const char* sFileName)
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(sFileName);
	if (root)
	{
		IUIAction* pAction = GetUIAction(PathUtil::GetFileName(sFileName));
		if (!pAction)
		{
			UIACTION_LOG("FlashUI create FG UIAction from xml %s", sFileName);
			pAction = new CFlashUIAction(IUIAction::eUIAT_FlowGraph);
			pAction->SetName(PathUtil::GetFileName(sFileName));
			m_actions.push_back(pAction);
		}
		if (pAction->GetType() == IUIAction::eUIAT_FlowGraph)
		{
			UIACTION_LOG("FlashUI read FG UIAction from xml %s", sFileName);
			if (!pAction->Serialize(root, true))
			{
				UIACTION_ERROR("FlashUI failed to load FG UIAction from xml %s", sFileName);
				return false;
			}
		}
		else
		{
			UIACTION_ERROR("FlashUI try to load UIAction FlowGraph from xml %s but Lua UI Action already exist with same name!", sFileName);
			return false;
		}
		return true;
	}
	UIACTION_ERROR("Error in ui action xml file %s", sFileName);
	return false;
}

//-------------------------------------------------------------------
bool CFlashUI::LoadLuaActionFromFile(const char* sFileName)
{
	IUIAction* pAction = GetUIAction(PathUtil::GetFileName(sFileName));
	if (!pAction)
	{
		UIACTION_LOG("FlashUI create Lua UIAction from lau %s", sFileName);
		pAction = new CFlashUIAction(IUIAction::eUIAT_LuaScript);
		pAction->SetName(PathUtil::GetFileName(sFileName));
		m_actions.push_back(pAction);
	}
	if (pAction->GetType() == IUIAction::eUIAT_LuaScript)
	{
		UIACTION_LOG("FlashUI read Lua UIAction from xml %s", sFileName);
		if (!pAction->Serialize(sFileName, true))
		{
			UIACTION_ERROR("FlashUI failed to load Lua UIAction from script %s", sFileName);
			return false;
		}
	}
	else
	{
		UIACTION_ERROR("FlashUI try to load UIAction Lua script %s but Flowgraph UI Action already exist with same name!", sFileName);
		return false;
	}
	return true;
}

//-------------------------------------------------------------------
void CFlashUI::ClearElements()
{
	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
		delete *iter;

	m_elements.clear();
}

//-------------------------------------------------------------------
void CFlashUI::ClearActions()
{
	m_pUIActionManager->Init();

	for (TUIActionsLookup::iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
		delete *iter;

	m_actions.clear();
}

//-------------------------------------------------------------------
void CFlashUI::ResetActions()
{
	m_pUIActionManager->Init();
	for (TUIActionsLookup::iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
	{
		(*iter)->Init();
	}
}

//-------------------------------------------------------------------
void CFlashUI::ReloadScripts()
{
	for (TUIActionsLookup::iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
	{
		CFlashUIAction* pAction = (CFlashUIAction*)*iter;
		pAction->ReloadScript();
	}
}

//-------------------------------------------------------------------
void CFlashUI::ClearNodes()
{
	for (auto pNodeFactory : m_UINodes)
	{
		gEnv->pFlowSystem->UnregisterType(pNodeFactory->GetNodeTypeName());
	}

	m_UINodes.clear();
}

//-------------------------------------------------------------------
void CFlashUI::LoadFromFile(const char* pFolderName, const char* pSearch, bool (CFlashUI::* fhFileLoader)(const char*))
{
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	string search = string(pFolderName) + "/";
	search += pSearch;
	intptr_t handle = pCryPak->FindFirst(search.c_str(), &fd);
	if (handle != -1)
	{
		int res = 0;
		do
		{
			cry_strcpy(filename, pFolderName);
			cry_strcat(filename, "/");
			cry_strcat(filename, fd.name);

			(this->*fhFileLoader)(filename);

			res = pCryPak->FindNext(handle, &fd);
		}
		while (res >= 0);
		pCryPak->FindClose(handle);
	}
}

//-------------------------------------------------------------------
void CFlashUI::PreloadTextures(const char* pLevelName)
{
	LOADING_TIME_PROFILE_SECTION;

	ReleasePreloadedTextures(false);

	string file;
	file.Format("%s/%s", CV_gfx_uiaction_folder->GetString(), FLASH_PRELOAD_XML);
	if (!gEnv->pCryPak->IsFileExist(file))
		return;

	XmlNodeRef xmlNode = GetISystem()->LoadXmlFromFile(file.c_str());

	string XMLLevelName(pLevelName);
	XMLLevelName.replace("/", "_");

	if (xmlNode)
	{
		XmlNodeRef commonNode = xmlNode->findChild("common");
		PreloadTexturesFromNode(commonNode);
		if (pLevelName)
		{
			XmlNodeRef levels = xmlNode->findChild("levels");
			if (levels)
			{
				XmlNodeRef commonLevelNode = levels->findChild("common");
				PreloadTexturesFromNode(commonLevelNode);
				XmlNodeRef levelNode = levels->findChild(XMLLevelName.c_str());
				PreloadTexturesFromNode(levelNode);
			}
		}
		else
		{
			XmlNodeRef levelNode = xmlNode->findChild("nolevel");
			PreloadTexturesFromNode(levelNode);
		}
	}
}

void CFlashUI::PreloadTexturesFromNode(const XmlNodeRef& node)
{
	if (node)
	{
		for (int i = 0; i < node->getChildCount(); ++i)
		{
			const char* pFile = node->getChild(i)->getAttr("file");
			if (pFile)
			{
				string path = PathUtil::GetPathWithoutFilename(pFile);
				string file = PathUtil::GetFile(pFile);
				LoadFromFile(path, file, &CFlashUI::PreloadTexture);
			}
		}
	}
}

bool CFlashUI::PreloadTexture(const char* pFileName)
{
	gEnv->pLog->UpdateLoadingScreen(0); // CFlashUI::PreloadTextures() might take a long time
	                                    // poll system events here

	stack_string sFile = pFileName;

	//#if !CRY_PLATFORM_DESKTOP
	if (sFile.find(".0") != string::npos)
		sFile = sFile.substr(0, sFile.find(".0"));
	//#endif

	if (sFile.find(".dds") == sFile.length() - 4
	    || sFile.find(".tif") == sFile.length() - 4)
	{
		if (gEnv && gEnv->pRenderer)
		{
			PathUtil::ToUnixPath(sFile);
			while (sFile.find("//") != stack_string::npos)
				sFile.replace("//", "/");

			ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(sFile.c_str(), FT_DONT_STREAM | FT_NOMIPS);
			m_preloadedTextures[pTexture] = sFile.c_str();
			UIACTION_LOG("Preload Texture: %s %s", sFile.c_str(), pTexture ? "done!" : "failed!");
		}
	}
	return true;
}

//-------------------------------------------------------------------
void CFlashUI::ReleasePreloadedTextures(bool bReleaseTextures /*= true*/)
{
	if (bReleaseTextures)
	{
		for (TTextureMap::iterator it = m_preloadedTextures.begin(); it != m_preloadedTextures.end(); ++it)
		{
			it->first->Release();
		}
	}
	m_preloadedTextures.clear();
	UIACTION_LOG("Released preloaded texture");
}

//-------------------------------------------------------------------
bool CFlashUI::UseSharedRT(const char* instanceStr, bool defVal) const
{
	IUIElement* pElement = GetUIElementByInstanceStr(instanceStr);
	return pElement ? pElement->HasFlag(IUIElement::eFUI_SHARED_RT) : defVal;
}

//-------------------------------------------------------------------
void CFlashUI::CheckPreloadedTexture(ITexture* pTexture) const
{
#if defined(UIACTION_LOGGING)
	TTextureMap::const_iterator it = m_preloadedTextures.find(pTexture);
	if (it != m_preloadedTextures.end())
	{
		UIACTION_LOG("Using preloaded texture: \"%s\"", it->second.c_str());
	}
	else
	{
		UIACTION_WARNING("Using not preloaded Texture \"%s\"", pTexture->GetName());
	}
#endif
}

//-------------------------------------------------------------------
void CFlashUI::GetMemoryStatistics(ICrySizer* s) const
{
	SIZER_SUBCOMPONENT_NAME(s, "FlashUI");
	s->AddObject(this, sizeof(*this));
	s->AddContainer(m_preloadedTextures);
	s->AddContainer(m_UINodes);

	{
		SIZER_SUBCOMPONENT_NAME(s, "Elements");
		s->AddContainer(m_elements);
		for (TUIElementsLookup::const_iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
		{
			s->AddObject(*iter);
		}
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "Actions");
		s->AddContainer(m_actions);
		for (TUIActionsLookup::const_iterator iter = m_actions.begin(); iter != m_actions.end(); ++iter)
		{
			s->AddObject(*iter);
		}
	}

	{
		SIZER_SUBCOMPONENT_NAME(s, "EventSystems");
		s->AddContainer(m_eventSystemsUiToSys);
		s->AddContainer(m_eventSystemsSysToUi);

		for (TUIEventSystemMap::const_iterator iter = m_eventSystemsUiToSys.begin(); iter != m_eventSystemsUiToSys.end(); ++iter)
		{
			s->AddObject(iter->second);
		}
		for (TUIEventSystemMap::const_iterator iter = m_eventSystemsSysToUi.begin(); iter != m_eventSystemsSysToUi.end(); ++iter)
		{
			s->AddObject(iter->second);
		}
	}

	s->Add(*m_pUIActionManager);
	s->Add(*m_pFlashUIActionEvents);
}

//-------------------------------------------------------------------
IUIEventSystem* CFlashUI::CreateEventSystem(const char* sName, IUIEventSystem::EEventSystemType eType)
{
	IUIEventSystem* pResult = GetEventSystem(sName, eType);
	if (pResult)
		return pResult;

	TUIEventSystemMap* pMap = GetEventSystemMap(eType);
	CFlashUIEventSystem* pEventSystem = new CFlashUIEventSystem(sName, eType);
	pMap->insert(std::make_pair(sName, pEventSystem));
	return pEventSystem;
}

//------------------------------------------------------------------------------------
IUIEventSystem* CFlashUI::GetEventSystem(const char* name, IUIEventSystem::EEventSystemType eType)
{
	TUIEventSystemMap* pMap = GetEventSystemMap(eType);
	TUIEventSystemMap::iterator it = pMap->find(name);
	if (it != pMap->end())
		return it->second;

	return 0;
}

//------------------------------------------------------------------------------------
TUIEventSystemMap* CFlashUI::GetEventSystemMap(IUIEventSystem::EEventSystemType eType)
{
	TUIEventSystemMap* pMap = &m_eventSystemsSysToUi;
	;
	switch (eType)
	{
	case IUIEventSystem::eEST_SYSTEM_TO_UI:
		pMap = &m_eventSystemsSysToUi;
		break;
	case IUIEventSystem::eEST_UI_TO_SYSTEM:
		pMap = &m_eventSystemsUiToSys;
		break;
	default:
		CRY_ASSERT_MESSAGE(false, "Invalid IUIEventSystem::EEventSystemType type");
	}
	return pMap;
}

//------------------------------------------------------------------------------------
void CFlashUI::StartRenderThread()
{
#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP
	const bool bMultiThreaded = true;
#else
	static ICVar* pMT = gEnv->pConsole->GetCVar("r_MultiThreaded");
	const bool bMultiThreaded = pMT && pMT->GetIVal() > 0;
#endif
	static ICVar* pAsyncLoad = gEnv->pConsole->GetCVar("g_asynclevelload");
	const bool bAsyncLoad = pAsyncLoad && pAsyncLoad->GetIVal() > 0;
	if (CV_gfx_loadtimethread == 1 && !gEnv->IsEditor() && m_bLoadtimeThread == false && bMultiThreaded && !bAsyncLoad)
	{
		const TSortedElementList& activeElements = GetSortedElements();
		for (TSortedElementList::const_iterator it = activeElements.begin(); it != activeElements.end(); ++it)
		{
			m_loadtimePlayerList.emplace_back(it->second->GetFlashPlayer());
		}
		m_bLoadtimeThread = true;
		gEnv->pCryPak->LockReadIO(true);
		gEnv->pRenderer->StartLoadtimeFlashPlayback(this);
		UIACTION_LOG("Loadtime Render Thread: Started");
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::StopRenderThread()
{
	if (m_bLoadtimeThread == true)
	{
		gEnv->pRenderer->StopLoadtimeFlashPlayback();
		m_loadtimePlayerList.clear();
		gEnv->pCryPak->LockReadIO(false);
		m_bLoadtimeThread = false;
		UIACTION_LOG("Loadtime Render Thread: Stopped");
	}
}

//------------------------------------------------------------------------------------
IUIEventSystemIteratorPtr CFlashUI::CreateEventSystemIterator(IUIEventSystem::EEventSystemType eType)
{
	TUIEventSystemMap* pMap = GetEventSystemMap(eType);
	IUIEventSystemIteratorPtr iter = new CUIEventSystemIterator(pMap);
	iter->Release();
	return iter;
}

//-------------------------------------------------------------------
std::pair<string, int> CFlashUI::GetUIIdentifiersByInstanceStr(const char* sUIInstanceStr) const
{
	std::pair<string, int> result = std::make_pair<string, int>("", 0);

	CRY_ASSERT(sUIInstanceStr != nullptr);
	if (sUIInstanceStr == nullptr)
		return result;

	string tmpName(sUIInstanceStr);
	PathUtil::RemoveExtension(tmpName);

	char name[_MAX_PATH];
	cry_strcpy(name, tmpName.c_str());

	const char* pExt = PathUtil::GetExt(sUIInstanceStr);
	if (*pExt != '\0' && strcmpi(pExt, "ui"))
		return result;

	uint instanceId = 0;

	int str_length = strlen(name);
	int index = 0;
	int id_index = -1;
	while (name[index] != '\0')
	{
		if (name[index] == '@')
		{
			name[index] = '\0';
			id_index = index + 1;
		}

		index++;
	}

	result.first = name;

	if (id_index != -1 && id_index < str_length)
	{
		result.second = atoi(name + id_index);
	}

	return result;
}

std::pair<IUIElement*, IUIElement*> CFlashUI::GetUIElementsByInstanceStr(const char* sUIInstanceStr) const
{
	std::pair<IUIElement*, IUIElement*> result = std::make_pair<IUIElement*, IUIElement*>(nullptr, nullptr);
	std::pair<string, int> identifiers = GetUIIdentifiersByInstanceStr(sUIInstanceStr);

	if (identifiers.first == "")
		return result;

	if (result.second = result.first = GetUIElement(identifiers.first))
	{
		result.second = result.first->GetInstance(identifiers.second);
	}

	return result;
}

IUIElement* CFlashUI::GetUIElementByInstanceStr(const char* sUIInstanceStr) const
{
	return CFlashUI::GetUIElementsByInstanceStr(sUIInstanceStr).second;
}

//-------------------------------------------------------------------
SFlashKeyEvent CFlashUI::MapToFlashKeyEvent(const SInputEvent& inputEvent)
{
	SFlashKeyEvent::EKeyCode keyCode(SFlashKeyEvent::VoidSymbol);

	EKeyId keyId(inputEvent.keyId);
	switch (keyId)
	{
	case eKI_Backspace:
		keyCode = SFlashKeyEvent::Backspace;
		break;
	case eKI_Tab:
		keyCode = SFlashKeyEvent::Tab;
		break;
	case eKI_Enter:
		keyCode = SFlashKeyEvent::Return;
		break;
	case eKI_LShift:
	case eKI_RShift:
		keyCode = SFlashKeyEvent::Shift;
		break;
	case eKI_LCtrl:
	case eKI_RCtrl:
		keyCode = SFlashKeyEvent::Control;
		break;
	case eKI_LAlt:
	case eKI_RAlt:
		keyCode = SFlashKeyEvent::Alt;
		break;
	case eKI_Escape:
		keyCode = SFlashKeyEvent::Escape;
		break;
	case eKI_PgUp:
		keyCode = SFlashKeyEvent::PageUp;
		break;
	case eKI_PgDn:
		keyCode = SFlashKeyEvent::PageDown;
		break;
	case eKI_End:
		keyCode = SFlashKeyEvent::End;
		break;
	case eKI_Home:
		keyCode = SFlashKeyEvent::Home;
		break;
	case eKI_Left:
		keyCode = SFlashKeyEvent::Left;
		break;
	case eKI_Up:
		keyCode = SFlashKeyEvent::Up;
		break;
	case eKI_Right:
		keyCode = SFlashKeyEvent::Right;
		break;
	case eKI_Down:
		keyCode = SFlashKeyEvent::Down;
		break;
	case eKI_Insert:
		keyCode = SFlashKeyEvent::Insert;
		break;
	case eKI_Delete:
		keyCode = SFlashKeyEvent::Delete;
		break;
	}

	unsigned char specialKeyState(0);
	specialKeyState |= ((inputEvent.modifiers & (eMM_LShift | eMM_RShift)) != 0) ? SFlashKeyEvent::eShiftPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & (eMM_LCtrl | eMM_RCtrl)) != 0) ? SFlashKeyEvent::eCtrlPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & (eMM_LAlt | eMM_RAlt)) != 0) ? SFlashKeyEvent::eAltPressed : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_CapsLock) != 0) ? SFlashKeyEvent::eCapsToggled : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_NumLock) != 0) ? SFlashKeyEvent::eNumToggled : 0;
	specialKeyState |= ((inputEvent.modifiers & eMM_ScrollLock) != 0) ? SFlashKeyEvent::eScrollToggled : 0;

	return SFlashKeyEvent(inputEvent.state == eIS_Pressed ? SFlashKeyEvent::eKeyDown : SFlashKeyEvent::eKeyUp, keyCode, specialKeyState, 0, 0);
}

//------------------------------------------------------------------------------------
void CFlashUI::CheckLanguageChanged()
{
	static ICVar* pLanguage = gEnv->pConsole->GetCVar("g_language");
	if (pLanguage)
	{
		static string sLanguage = pLanguage->GetString();
		if (sLanguage != pLanguage->GetString())
		{
			ReloadAll();
			sLanguage = pLanguage->GetString();
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::CheckResolutionChange()
{
	int width = 0, height = 0;
	if (!gEnv->IsEditor() && gEnv->pRenderer)
	{
		width  = gEnv->pRenderer->GetOverlayWidth();
		height = gEnv->pRenderer->GetOverlayHeight();
	}
	else
	{
		GetScreenSize(width, height);
	}

	const bool bResChanged = width != m_iWidth || height != m_iHeight;
	m_iWidth = width;
	m_iHeight = height;

	if (bResChanged)
	{
		const TSortedElementList& sortedElements = GetSortedElements();

		for (TSortedElementList::const_iterator iter = sortedElements.begin(); iter != sortedElements.end(); ++iter)
			iter->second->UpdateViewPort();

		// fix for mouse cursor trapped in old resolution on switch to FS
		std::map<IUIElement*, bool> bCursorMap;
		for (TSortedElementList::const_iterator iter = sortedElements.begin(); iter != sortedElements.end(); ++iter)
		{
			bCursorMap[iter->second] = iter->second->HasFlag(IUIElement::eFUI_HARDWARECURSOR);
			iter->second->SetFlag(IUIElement::eFUI_HARDWARECURSOR, false);
		}
		for (TSortedElementList::const_iterator iter = sortedElements.begin(); iter != sortedElements.end(); ++iter)
		{
			iter->second->SetFlag(IUIElement::eFUI_HARDWARECURSOR, bCursorMap[iter->second]);
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::GetScreenSize(int& width, int& height)
{
	CRY_ASSERT_MESSAGE(gEnv->IsEditor(), "Only allowed for editor!");
	if (m_ScreenSizeCB)
	{
		m_ScreenSizeCB(width, height);
	}
	else if (gEnv->pRenderer)
	{
		width  = gEnv->pRenderer->GetOverlayWidth();
		height = gEnv->pRenderer->GetOverlayHeight();
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::SetEditorScreenSizeCallback(TEditorScreenSizeCallback& cb)
{
	m_ScreenSizeCB = cb;
}

//------------------------------------------------------------------------------------
void CFlashUI::RemoveEditorScreenSizeCallback()
{
	m_ScreenSizeCB = NULL;
}

//------------------------------------------------------------------------------------
void CFlashUI::SetEditorUILogEventCallback(TEditorUILogEventCallback& cb)
{
	m_LogCallback = cb;
}

void CFlashUI::RemoveEditorUILogEventCallback()
{
	m_LogCallback = NULL;
}

//------------------------------------------------------------------------------------
IFlashUI::EPlatformUI CFlashUI::GetCurrentPlatform()
{
	CRY_ASSERT_MESSAGE(gEnv->IsEditor(), "Only allowed for editor!");

	if (m_plattformCallback)
	{
		return m_plattformCallback();
	}

	return IFlashUI::ePUI_PC;
}

//------------------------------------------------------------------------------------
void CFlashUI::SetEditorPlatformCallback(TEditorPlatformCallback& cb)
{
	m_plattformCallback = cb;
}

//------------------------------------------------------------------------------------
void CFlashUI::RemoveEditorPlatformCallback()
{
	m_plattformCallback = NULL;
}

//------------------------------------------------------------------------------------
void CFlashUI::ReloadAllBootStrapper()
{
	std::map<IUIElement*, std::pair<bool, bool>> visMap;
	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
	{
		IUIElementIteratorPtr instances = (*iter)->GetInstances();
		while (IUIElement* pInstance = instances->Next())
		{
			visMap[pInstance].first = pInstance->IsInit();
			visMap[pInstance].second = pInstance->IsVisible();
			pInstance->UnloadBootStrapper();
		}
	}
	for (TUIElementsLookup::iterator iter = m_elements.begin(); iter != m_elements.end(); ++iter)
	{
		IUIElementIteratorPtr instances = (*iter)->GetInstances();
		while (IUIElement* pInstance = instances->Next())
		{
			pInstance->Init(visMap[pInstance].first);
			pInstance->SetVisible(visMap[pInstance].second);
		}
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::ReloadAllElements(IConsoleCmdArgs* /* pArgs */)
{
	if (gEnv->pFlashUI)
	{
		((CFlashUI*)gEnv->pFlashUI)->ReloadAll();
	}
}

//------------------------------------------------------------------------------------
void CFlashUI::LogUIAction(ELogEventLevel level, const char* format, ...)
{
#if defined(UIACTION_LOGGING)
	if (!CV_gfx_uiaction_log)
		return;

	va_list args;
	va_start(args, format);
	char tmp[1024];
	cry_vsprintf(tmp, format, args);
	va_end(args);

	if (gEnv->IsEditor())
	{
		if (((CFlashUI*)gEnv->pFlashUI)->m_LogCallback && (CheckFilter(tmp) || level != IFlashUI::eLEL_Log))
		{
			IFlashUI::SUILogEvent evt;
			evt.Message = tmp;
			evt.Level = level;
			((CFlashUI*)gEnv->pFlashUI)->m_LogCallback(evt);
		}
	}

	if (level == IFlashUI::eLEL_Warning)
	{
		gEnv->pLog->LogWarning("$7[UIAction] $6%s", tmp);
	}
	else if (level == IFlashUI::eLEL_Error)
	{
		gEnv->pLog->LogError("$7[UIAction] $4%s", tmp);
	}
	else
	{
		if (CheckFilter(tmp))
		{
			CryLogAlways("$7[UIAction] $1%s", tmp);
		}
	}
#endif
}

//------------------------------------------------------------------------------------
bool CFlashUI::CheckFilter(const string& str)
{
	string msg = str;
	msg.MakeUpper();
	if (CV_gfx_uiaction_log_filter)
	{
		string filter = CV_gfx_uiaction_log_filter->GetString();
		filter.MakeUpper();
		SUIArguments filterlist;
		filterlist.SetDelimiter("|");
		filterlist.SetArguments(filter.c_str());

		const int filter_count = filterlist.GetArgCount();
		for (int i = 0; i < filter_count; ++i)
		{
			string filterEntry;
			filterlist.GetArg(i, filterEntry);
			if (filterEntry.substr(0, 1) == "-")
			{
				if (msg.find(filterEntry.substr(1)) != string::npos)
					return false;
			}
		}

		bool bHasFilter = false;
		for (int i = 0; i < filter_count; ++i)
		{
			string filterEntry;
			filterlist.GetArg(i, filterEntry);
			if (filterEntry.substr(0, 1) != "-")
			{
				if (msg.find(filterEntry) != string::npos)
				{
					return true;
				}
				else
				{
					bHasFilter = true;
				}
			}
		}
		return !bHasFilter;
	}
	return true;
}

//------------------------------------------------------------------------------------
void CFlashUI::ResetDirtyFlags()
{
	if (m_elements.size() > 0)
	{
		auto flash_player = m_elements[0]->GetFlashPlayer();
		if (flash_player)
			flash_player->ResetDirtyFlags();
	}
}

//------------------------------------------------------------------------------------
#if !defined (_RELEASE) || defined(ENABLE_UISTACK_DEBUGGING)
void Draw2dLabel(float x, float y, float fontSize, const float* pColor, const char* pText)
{
	IRenderAuxText::DrawText(Vec3(x, y, 0.5f), fontSize, pColor, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace, pText);
}

void DrawTextLabelEx(float x, float y, float size, const float* pColor, const char* pFormat, ...) PRINTF_PARAMS(5, 6);
void DrawTextLabelEx(float x, float y, float size, const float* pColor, const char* pFormat, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, pFormat);
	cry_vsprintf(buffer, pFormat, args);
	va_end(args);

	Draw2dLabel(x, y, size, pColor, buffer);
}

	#define DrawTextLabel(x, y, pColor, ...) DrawTextLabelEx(x, y, 1.4f, pColor, __VA_ARGS__)

void RenderDebugInfo()
{
	if (gEnv->pFlashUI && gEnv->pSystem)
	{
		const float dy = 15;
		const float dy_space = 5;
		static const float colorWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		static const float colorBlue[4] = { 0.3f, 0.8f, 1.0f, 1.0f };
		static const float colorDarkBlue[4] = { 0.3f, 0.4f, 0.5f, 1.0f };
		static const float colorGray[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
		static const float colorGreen[4] = { 0.3f, 1.0f, 0.8f, 1.0f };
		static const float colorRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

		static ICVar* pFlashInfo = gEnv->pConsole->GetCVar("sys_flash_info");
		float py = 20 + (pFlashInfo ? pFlashInfo->GetIVal() > 0 ? 400.f : 0 : 0);

		if (CFlashUI::CV_gfx_debugdraw & 1)
		{
			const float col1 = 10;
			const float col2 = 420;
			const float col3 = 480;
			const float col4 = 600;
			const float col5 = 700;
			const float col6 = 800;

			DrawTextLabel(col1, py, colorWhite, "UIElement");
			DrawTextLabel(col2, py, colorWhite, "Refs");
			DrawTextLabel(col3, py, colorWhite, "Instance");
			DrawTextLabel(col4, py, colorWhite, "Init");
			DrawTextLabel(col5, py, colorWhite, "Visible");
			DrawTextLabel(col6, py, colorWhite, "Memory/Textures");
			py += dy + dy_space;

			std::map<IUIElement*, bool> m_collectedMap;

			// display normal elements
			for (int i = 0; i < gEnv->pFlashUI->GetUIElementCount(); ++i)
			{
				ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
				IUIElement* pElement = gEnv->pFlashUI->GetUIElement(i);
				pElement->GetMemoryUsage(pSizer);
				IUIElementIteratorPtr instances = pElement->GetInstances();
				const float* color = pElement->IsValid() ? colorGreen : colorRed;
				DrawTextLabel(col1, py, color, "%s%s (%s)", pElement->IsValid() ? "" : "INVALID! ", pElement->GetName(), pElement->GetFlashFile());
				DrawTextLabel(col3, py, color, "Count: %i", instances->GetCount());
				DrawTextLabel(col6, py, color, "%4d KB", static_cast<uint32>(pSizer->GetTotalSize() >> 10));
				pSizer->Release();
				py += dy;

				while (IUIElement* pInstance = instances->Next())
				{
					color = pInstance->IsValid() ? pInstance->IsInit() ? pInstance->IsVisible() ? colorBlue : colorDarkBlue : colorGray : colorRed;
					DrawTextLabel(col1, py, color, "%s%s@%i", pInstance->IsValid() ? "" : "INVALID! ", pInstance->GetName(), pInstance->GetInstanceID());
					DrawTextLabel(col2, py, color, "%i", ((CFlashUIElement*)pInstance)->m_refCount);
					DrawTextLabel(col3, py, color, "ID: %i", pInstance->GetInstanceID());
					DrawTextLabel(col4, py, color, "%s ", pInstance->IsInit() ? "true" : "false");
					DrawTextLabel(col5, py, color, "%s ", pInstance->IsVisible() ? "true" : "false");
					DrawTextLabel(col6, py, color, "DynTex: %i ", pInstance->GetNumExtTextures());
					py += dy;
					m_collectedMap[pInstance] = true;
				}
				py += dy_space;
			}
			py += dy;

			// display leaking elements
			for (CFlashUIElement::TElementInstanceList::const_iterator it = CFlashUIElement::s_ElementDebugList.begin(); it != CFlashUIElement::s_ElementDebugList.end(); ++it)
			{
				CFlashUIElement* pElement = *it;
				if (m_collectedMap[pElement])
					continue;

				DrawTextLabel(col1, py, colorRed, "%s@%i", pElement->GetName(), pElement->GetInstanceID());
				DrawTextLabel(col2, py, colorRed, "%i", pElement->m_refCount);
				DrawTextLabel(col3, py, colorRed, "ID: %i", pElement->GetInstanceID());
				DrawTextLabel(col4, py, colorRed, "LEAKING!!!");
				DrawTextLabel(col6, py, colorRed, "DynTex: %i ", pElement->GetNumExtTextures());
				py += dy;
			}
			py += dy;
		}
		if (CFlashUI::CV_gfx_debugdraw & 2)
		{
			const float col1 = 10;
			const float col2 = 480;
			const float col3 = 600;
			const float col4 = 700;

			DrawTextLabel(col1, py, colorWhite, "UIAction");
			DrawTextLabel(col2, py, colorWhite, "Enabled");
			DrawTextLabel(col3, py, colorWhite, "Type");
			DrawTextLabel(col4, py, colorWhite, "Memory");
			py += dy + dy_space;

			std::list<IUIAction*> actions[2];

			for (int i = 0; i < gEnv->pFlashUI->GetUIActionCount(); ++i)
			{
				IUIAction* pAction = gEnv->pFlashUI->GetUIAction(i);
				if (pAction->IsEnabled())
					actions[0].push_back(pAction);
				else
					actions[1].push_back(pAction);
			}

			for (int i = 0; i < 2; ++i)
			{
				for (std::list<IUIAction*>::iterator it = actions[i].begin(); it != actions[i].end(); ++it)
				{
					ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
					IUIAction* pAction = *it;
					const float* color = pAction->IsValid() ? i == 0 ? colorBlue : colorDarkBlue : colorRed;
					pAction->GetMemoryUsage(pSizer);
					DrawTextLabel(col1, py, color, "%s", pAction->GetName());
					DrawTextLabel(col2, py, color, "%s", pAction->IsValid() ? pAction->IsEnabled() ? "true" : "false" : "INVALID!");
					DrawTextLabel(col3, py, color, "%s", pAction->GetType() == IUIAction::eUIAT_FlowGraph ? "FlowGraph" : "Lua Script");
					DrawTextLabel(col4, py, color, "%4d KB", static_cast<uint32>(pSizer->GetTotalSize() >> 10));
					pSizer->Release();
					py += dy;
				}
				py += dy_space;
			}
			py += dy;
		}

		// Force post effect active
		gEnv->p3DEngine->SetPostEffectParam("Post3DRenderer_Active", 1.0f, true);

		// Get post3DRenderer texture
		static ITexture* pPost3DRendererTexture = NULL;
		if (pPost3DRendererTexture == NULL)
		{
			pPost3DRendererTexture = gEnv->pRenderer->EF_LoadTexture("$BackBuffer");
		}

		// Render debug view
		if (pPost3DRendererTexture != NULL)
		{
			IRenderAuxImage::Draw2dImage(0, 0, 200, 200, pPost3DRendererTexture->GetTextureID());
		}
		/////////////////
	}
}
#endif

#ifdef ENABLE_UISTACK_DEBUGGING

struct SStackInfo
{
	enum EType
	{
		eDefault = 0,
		eAdded,
		eStack,
		eRemoved,
	};
	SStackInfo(float t = 0) : time(t), index(-1), type(eDefault), id(-1) {}
	SStackInfo(const char* str, float t) : info(str), time(t), index(-1), type(eDefault), id(-1) {}
	SStackInfo(const char* str, float t, int i, EType tp, int idx) : info(str), time(t), index(i), type(tp), id(idx) {}
	string info;
	float  time;
	int    index;
	EType  type;
	int    id;
};

void RenderStackDebugInfo(bool render, const char* label, int loop)
{
	if (CFlashUI::CV_gfx_debugdraw & 4)
	{
		static bool empty = true;
		static std::deque<SStackInfo> stacklist;
		static std::vector<int> currStack;
		static std::map<int, const char*> lastStack;

		const float time = gEnv->pTimer->GetAsyncCurTime();

		const std::map<int, const char*>& stack = CUIFGStackMan::GetStack();
		if (lastStack != stack)
		{
			lastStack = stack;

			if (!stack.empty() || !currStack.empty())
			{
				empty = false;
				stacklist.push_front(SStackInfo(time));
				stacklist.push_front(SStackInfo(string().Format("%i: %s", loop, label), time));

				int removed = 0;
				for (std::vector<int>::iterator it = currStack.begin(); it != currStack.end(); )
				{
					if (stack.find(*it) == stack.end())
					{
						for (std::deque<SStackInfo>::const_iterator it2 = stacklist.begin(), end2 = stacklist.end(); it2 != end2; ++it2)
						{
							if (it2->id == *it)
							{
								stacklist.push_front(SStackInfo(it2->info.c_str(), time, removed++, SStackInfo::eRemoved, it2->id));
								break;
							}
						}
						it = currStack.erase(it);
					}
					else
						++it;
				}

				int stackNum = 0;
				for (std::map<int, const char*>::const_iterator it = stack.begin(), end = stack.end(); it != end; ++it)
				{
					SStackInfo::EType type = SStackInfo::eStack;
					if (!stl::find(currStack, it->first))
					{
						currStack.push_back(it->first);
						type = SStackInfo::eAdded;
					}
					stacklist.push_front(SStackInfo(it->second, time, stackNum++, type, it->first));
				}
			}
		}

		if (!render)
			return;

		if (!empty)
		{
			stacklist.push_front(SStackInfo("-------------------------------------------------------", time));
			empty = true;
		}

		const float displayTime = 20;
		const int itemsPerRow = 140;

		const float dy = 8;
		const float dy_space = 5;
		const float dx = 400;
		float colorWhite[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float colorBlue[4] = { 0.3f, 0.8f, 1.0f, 1.0f };
		float colorGreen[4] = { 0.3f, 1.0f, 0.8f, 1.0f };
		float colorRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

		const float col1 = 10;
		const float col2 = 40;
		const float col3 = 50;

		float py = 0;
		float px = 0;
		const int count = stacklist.size();
		int round = 0;

		colorWhite[3] = 1.f;
		for (int i = 0; i <= count / itemsPerRow; ++i)
		{
			DrawTextLabelEx(col1 + i * dx, py, 1.1f, colorWhite, "   Id");
			DrawTextLabelEx(col3 + i * dx, py, 1.1f, colorWhite, "Stack");
		}
		py += dy + dy_space;

		for (int i = 0; i < count; ++i)
		{
			float alpha = (displayTime + stacklist[i].time - time) / displayTime;
			if (alpha <= 0.05)
			{
				stacklist.resize(i);
				break;
			}
			colorWhite[3] = colorBlue[3] = colorGreen[3] = colorRed[3] = alpha;

			float* color = stacklist[i].type == SStackInfo::eAdded ? colorGreen : stacklist[i].type == SStackInfo::eRemoved ? colorRed : colorBlue;

			if (stacklist[i].index >= 0)
			{
				DrawTextLabelEx(col1 + px, py, 0.9f, color, "%4i", stacklist[i].id & 511);
				DrawTextLabelEx(col2 + px, py, 0.9f, color, "%s", stacklist[i].type == SStackInfo::eAdded ? "+" : stacklist[i].type == SStackInfo::eRemoved ? "-" : "");
				DrawTextLabelEx(col3 + px, py, 0.9f, color, "%s", stacklist[i].info.c_str());
			}
			else
			{
				DrawTextLabelEx(col1 + px, py, 0.9f, colorWhite, "%s", stacklist[i].info.c_str());
			}

			if (stacklist[i].info == "")
				py += dy_space;
			else
				py += dy;

			if ((i + 1) % itemsPerRow == 0)
			{
				px += dx;
				py = dy + dy_space;
			}
		}
	}
}
#endif
//------------------------------------------------------------------------------------
