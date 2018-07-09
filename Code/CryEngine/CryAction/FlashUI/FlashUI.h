// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlashUI.h
//  Version:     v1.00
//  Created:     10/9/2010 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __FlashUI_H__
#define __FlashUI_H__

#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryExtension/ClassWeaver.h>
#include <CryInput/IHardwareMouse.h>
#include <CryInput/IInput.h>
#include <CryGame/IGameFramework.h>
#include <ILevelSystem.h>
#include "FlashUIEventSystem.h"

#if !defined (_RELEASE) || defined(RELEASE_LOGGING)
	#define UIACTION_LOGGING
#endif

#if defined (UIACTION_LOGGING)
	#define UIACTION_LOG(...)     { if (CFlashUI::CV_gfx_uiaction_log) CFlashUI::LogUIAction(IFlashUI::eLEL_Log, __VA_ARGS__); }
	#define UIACTION_WARNING(...) { CFlashUI::LogUIAction(IFlashUI::eLEL_Warning, __VA_ARGS__); }
	#define UIACTION_ERROR(...)   { CFlashUI::LogUIAction(IFlashUI::eLEL_Error, __VA_ARGS__); }
#else
	#define UIACTION_LOG     (void)
	#define UIACTION_WARNING (void)
	#define UIACTION_ERROR   (void)
#endif

class CFlashUiFlowNodeFactory;
struct CUIActionManager;
class CFlashUIActionEvents;

class CFlashUI
	: public IFlashUI
	  , public IHardwareMouseEventListener
	  , public IInputEventListener
	  , public IGameFrameworkListener
	  , public ILevelSystemListener
	  , public ISystemEventListener
	  , public ILoadtimeCallback
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IFlashUI)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CFlashUI, "FlashUI", "35ae7f0f-bb13-437b-9c5f-fcd2568616a5"_cry_guid)

	CFlashUI();
	virtual ~CFlashUI() {}

public:
	// IFlashUI
	virtual void                      Init() override;
	virtual bool                      PostInit() override;
	virtual void                      Update(float fDeltatime) override;
	virtual void                      Reload() override;
	virtual void                      ClearUIActions() override { ClearActions(); }
	virtual void                      Shutdown() override;

	virtual bool                      LoadElementsFromFile(const char* sFileName) override;
	virtual bool                      LoadActionFromFile(const char* sFileName, IUIAction::EUIActionType type) override;

	virtual IUIElement*               GetUIElement(const char* sName) const override { return const_cast<IUIElement*>(m_elements(sName)); }
	virtual IUIElement*               GetUIElement(int index) const override         { return index < m_elements.size() ? const_cast<IUIElement*>(m_elements[index]) : NULL; }
	virtual int                       GetUIElementCount() const override             { return m_elements.size(); }

	virtual                        IUIElement*  GetUIElementByInstanceStr(const char* sUIInstanceStr) const override;
	virtual std::pair<IUIElement*, IUIElement*> GetUIElementsByInstanceStr(const char* sUIInstanceStr) const override;
	virtual std::pair<string, int>              GetUIIdentifiersByInstanceStr(const char* sUIInstanceStr) const override;

	virtual IUIAction*                GetUIAction(const char* sName) const override { return const_cast<IUIAction*>(m_actions(sName)); }
	virtual IUIAction*                GetUIAction(int index) const override         { return index < m_actions.size() ? const_cast<IUIAction*>(m_actions[index]) : NULL; }
	virtual int                       GetUIActionCount() const override             { return m_actions.size(); }

	virtual IUIActionManager*         GetUIActionManager() const override;
	virtual void                      UpdateFG() override;
	virtual void                      EnableEventStack(bool bEnable) override;
	virtual void                      RegisterModule(IUIModule* pModule, const char* name) override;
	virtual void                      UnregisterModule(IUIModule* pModule) override;

	virtual void                      SetHudElementsVisible(bool bVisible) override;

	virtual IUIEventSystem*           CreateEventSystem(const char* sName, IUIEventSystem::EEventSystemType eType) override;
	virtual IUIEventSystem*           GetEventSystem(const char* name, IUIEventSystem::EEventSystemType eType) override;
	virtual IUIEventSystemIteratorPtr CreateEventSystemIterator(IUIEventSystem::EEventSystemType eType) override;

	virtual void                      DispatchControllerEvent(IUIElement::EControllerInputEvent event, IUIElement::EControllerInputState state, float value) override;
	virtual void                      SendFlashMouseEvent(SFlashCursorEvent::ECursorState evt, int iX, int iY, int iButton = 0, float wheel = 0.f, bool bFromController = false) override;
	virtual bool                      DisplayVirtualKeyboard(unsigned int flags, const char* title, const char* initialInput, int maxInputLength, IVirtualKeyboardEvents* pInCallback) override;
	virtual bool                      IsVirtualKeyboardRunning() override;
	virtual bool                      CancelVirtualKeyboard() override;

	virtual void                      GetScreenSize(int& width, int& height) override;
	virtual void                      SetEditorScreenSizeCallback(TEditorScreenSizeCallback& cb) override;
	virtual void                      RemoveEditorScreenSizeCallback() override;

	virtual void                      SetEditorUILogEventCallback(TEditorUILogEventCallback& cb) override;
	virtual void                      RemoveEditorUILogEventCallback() override;

	virtual void                      SetEditorPlatformCallback(TEditorPlatformCallback& cb) override;
	virtual void                      RemoveEditorPlatformCallback() override;

	virtual bool                      UseSharedRT(const char* instanceStr, bool defVal) const override;

	virtual void                      CheckPreloadedTexture(ITexture* pTexture) const override;

	virtual void                      GetMemoryStatistics(ICrySizer* s) const override;

#if !defined(_LIB)
	virtual SUIItemLookupSet_Impl<SUIParameterDesc>* CreateLookupParameter() override { return new SUIItemLookupSet_Impl<SUIParameterDesc>(); };
	virtual SUIItemLookupSet_Impl<SUIMovieClipDesc>* CreateLookupMovieClip() override { return new SUIItemLookupSet_Impl<SUIMovieClipDesc>(); };
	virtual SUIItemLookupSet_Impl<SUIEventDesc>*     CreateLookupEvent() override { return new SUIItemLookupSet_Impl<SUIEventDesc>(); };
#endif
	// ~IFlashUI

	// IHardwareMouseEventListener
	void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta) override;
	// ~IHardwareMouseEventListener

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& event) override;
	virtual bool OnInputEventUI(const SUnicodeEvent& event) override;
	// ~IInputEventListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime) override    {}
	virtual void OnSaveGame(ISaveGame* pSaveGame) override  {}
	virtual void OnLoadGame(ILoadGame* pLoadGame) override  {}
	virtual void OnLevelEnd(const char* nextLevel) override {}
	virtual void OnActionEvent(const SActionEvent& event) override;
	// ~IGameFrameworkListener

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName) override;
	virtual void OnLoadingStart(ILevelInfo* pLevel) override              {}
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) override {}
	virtual void OnLoadingComplete(ILevelInfo* pLevel) override           {}
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) override;
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override;
	virtual void OnUnloadComplete(ILevelInfo* pLevel) override {}
	// ~ILevelSystemListener

	// ILoadtimeCallback
	virtual void LoadtimeUpdate(float fDeltaTime) override;
	virtual void LoadtimeRender() override;
	// ~ILoadtimeCallback

	// logging
	static void LogUIAction(ELogEventLevel level, const char* format, ...) PRINTF_PARAMS(2, 3);

	// cvars
	DeclareStaticConstIntCVar(CV_gfx_draw, 1);
	DeclareStaticConstIntCVar(CV_gfx_debugdraw, 0);
	DeclareStaticConstIntCVar(CV_gfx_uiaction_log, 0);
	DeclareStaticConstIntCVar(CV_gfx_uiaction_enable, 1);
	DeclareStaticConstIntCVar(CV_gfx_loadtimethread, 1);
	DeclareStaticConstIntCVar(CV_gfx_reloadonlanguagechange, 1);
	DeclareStaticConstIntCVar(CV_gfx_uievents_editorenabled, 1);
	DeclareStaticConstIntCVar(CV_gfx_ampserver, 0);
	static int    CV_gfx_enabled;
	static float  CV_gfx_inputevents_triggerstart;
	static float  CV_gfx_inputevents_triggerrepeat;
	static ICVar* CV_gfx_uiaction_log_filter;
	static ICVar* CV_gfx_uiaction_folder;

	static void ReloadAllElements(IConsoleCmdArgs* /* pArgs */);

	void        InvalidateSortedElements();

	bool        IsLoadtimeThread() const { return m_bLoadtimeThread; };

	EPlatformUI GetCurrentPlatform();

private:
	CFlashUI(const CFlashUI&) : m_modules(8) {}

	// cppcheck-suppress operatorEqVarError
	void operator=(const CFlashUI&) {}

	void RegisterListeners();
	void UnregisterListeners();

	void ReloadAll();

	void LoadElements();
	void ClearElements();

	void LoadActions();
	void ClearActions();

	void ResetActions();
	void ReloadScripts();

	void CreateNodes();
	void ClearNodes();

	void LoadFromFile(const char* sFolderName, const char* pSearch, bool (CFlashUI::* fhFileLoader)(const char*));
	bool LoadFGActionFromFile(const char* sFileName);
	bool LoadLuaActionFromFile(const char* sFileName);

	void PreloadTextures(const char* pLevelName = NULL);
	void PreloadTexturesFromNode(const XmlNodeRef& node);
	bool PreloadTexture(const char* pFileName);
	void ReleasePreloadedTextures(bool bReleaseTextures = true);

	typedef std::multimap<int, IUIElement*> TSortedElementList;
	inline const TSortedElementList& GetSortedElements();
	inline void                      UpdateSortedElements();

	void                             CreateMouseClick(IUIElement::EControllerInputState state);

	void                             TriggerEvent(const SInputEvent& event);

	SFlashKeyEvent                   MapToFlashKeyEvent(const SInputEvent& inputEvent);

	TUIEventSystemMap*               GetEventSystemMap(IUIEventSystem::EEventSystemType eType);

	void                             StartRenderThread();
	void                             StopRenderThread();

	inline void                      CheckLanguageChanged();
	inline void                      CheckResolutionChange();

	void                             ReloadAllBootStrapper();
	void                             ResetDirtyFlags();

	static bool                      CheckFilter(const string& str);

private:
	CFlashUIActionEvents* m_pFlashUIActionEvents;

	TUIEventSystemMap     m_eventSystemsUiToSys;
	TUIEventSystemMap     m_eventSystemsSysToUi;

	TUIElementsLookup     m_elements;
	TUIActionsLookup      m_actions;

	TSortedElementList    m_sortedElements;
	bool                  m_bSortedElementsInvalidated;

	bool                  m_bLoadtimeThread;
	typedef std::vector<std::shared_ptr<IFlashPlayer>> TPlayerList;
	TPlayerList           m_loadtimePlayerList;

	std::vector<CFlashUiFlowNodeFactory*> m_UINodes;

	typedef std::map<ITexture*, string> TTextureMap;
	TTextureMap       m_preloadedTextures;

	CUIActionManager* m_pUIActionManager;

	typedef CListenerSet<IUIModule*> TUIModules;
	TUIModules                m_modules;

	int                       m_iWidth;
	int                       m_iHeight;
	TEditorScreenSizeCallback m_ScreenSizeCB;
	TEditorUILogEventCallback m_LogCallback;
	TEditorPlatformCallback   m_plattformCallback;

	enum ESystemState
	{
		eSS_NoLevel,
		eSS_Loading,
		eSS_LoadingDone,
		eSS_GameStarted,
		eSS_Unloading,
	};

	ESystemState m_systemState;
	float        m_fLastAdvance;
	float        m_lastTimeTriggered;
	bool         m_bHudVisible;
};

#endif // #ifndef __FlashUI_H__
