// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios
// -------------------------------------------------------------------------
//  Created:     10/10/2001 by Timur.
//  Description: IEditor interface implementation.
//
////////////////////////////////////////////////////////////////////////////
#include "IEditor.h"
#include <CryInput/IInput.h>
#include <CryCore/Containers/CryListenerSet.h>
#include "Version.h"
#include "Util/XmlTemplate.h"
#include <CryMath/Cry_Geo.h>
#include "IUndoManager.h"

#define GET_PLUGIN_ID_FROM_MENU_ID(ID)     (((ID) & 0x000000FF))
#define GET_UI_ELEMENT_ID_FROM_MENU_ID(ID) ((((ID) & 0x0000FF00) >> 8))

class CObjectManager;
class CGameEngine;
class EditorScriptEnvironment;
class CExportManager;
class CErrorsDlg;
class CLensFlareManager;
class CVegetationMap;
class CIconManager;
class CBackgroundTaskManager;
class CTrackViewSequenceManager;
class CMaterialFXGraphMan;
class CEditorFileMonitor;
class CTerrainManager;
class IPythonManager;
class CEditorPythonManager;
class CPolledKeyManager;
class CGizmoManager;
struct IDevManager;
class CClassFactory;
class CAssetManager;
class CEditorCommandManager;
struct SGizmoParameters;
struct IGizmoManager;
class CBaseObject;
class CViewManager;
class CCryEditDoc;
class CSelectionGroup;
class CEditTool;
class CAnimationContext;
class CGameEngine;
struct IIconManager;
class CEntityPrototypeManager;
class CMaterialManager;
class CMaterail;
class CEntityPrototype;
class CParticleManager;
class CPrefabManager;
class CBroadcastManager;
class CGameTokenManager;
class CLensFlareManager;
class CPluginManager;
class CEAXPresetManager;
class CBaseLibraryItem;
class CBaseLibraryDialog;
class ICommandManager;
class CAIManager;
class INotificationCenter;
class CPersonalizationManager;
class CEditorCommandManager;
class CHeightmap;
class IPythonManager;
class CEditorPythonManager;
class CHyperGraphManager;
class CFlowGraphManager;
class CConsoleSynchronization;
class CUIEnumsDatabase;
struct ISourceControl;
struct IEditorClassFactory;
struct IDataBaseItem;
struct IUIEvent;
struct ITransformManipulator;
struct IDataBaseManager;
class IFacialEditor;
class CDialog;
class C3DConnexionDriver;
class CRuler;
class CCustomActionsEditorManager;
struct IExportManager;
struct IDevManager;
class CDisplaySettings;
struct SGizmoParameters;
class CFlowGraphDebuggerEditor;
class CEditorFlowGraphModuleManager;
class CLevelIndependentFileMan;
class CViewport;
class CUIManager;
struct IResourceSelectorHost;
class CGameExporter;
struct SPythonCommand;
struct SPythonModule;
class QWidget;
class CConfigurationManager;
class CTrayArea;
struct IGizmoManager;
struct IBackgroundScheduleManager;
struct IBackgroundTaskManagerListener;
class IPane;
class CMainThreadWorker;

struct ISystem;
struct IGame;
struct I3DEngine;
struct IRenderer;
struct AABB;
struct IUriEventListener;
struct SEditorSettings;
enum EModifiedModule;

enum EEditorPathName
{
	EDITOR_PATH_OBJECTS,
	EDITOR_PATH_TEXTURES,
	EDITOR_PATH_SOUNDS,
	EDITOR_PATH_MATERIALS,
	EDITOR_PATH_UI_ICONS,
	EDITOR_PATH_LAST
};

namespace BackgroundScheduleManager
{
class CScheduleManager;
}

namespace BackgroundTaskManager
{
class CTaskManager;
class CBackgroundTasksListener;
}

//! Callback class passed to PickObject.
struct IPickObjectCallback
{
	//! Called when object picked.
	virtual void OnPick(CBaseObject* picked) = 0;
	//! Called when pick mode cancelled.
	virtual void OnCancelPick() = 0;
	//! Return true if specified object is pickable.
	virtual bool OnPickFilter(CBaseObject* filterObject) { return true; };
};

class SANDBOX_API CEditorImpl : public IEditor, public ISystemEventListener
{
public:
	CEditorImpl(CGameEngine* ge);
	~CEditorImpl();

	//! This will be called after most of the other systems have initialized. If something depends on something else being initialized,
	//! move the dependent code into Init()
	void                         Init();

	IEditorClassFactory*         GetClassFactory();
	CPersonalizationManager*     GetPersonalizationManager() { return m_pPersonalizationManager; }
	virtual CPreferences*        GetPreferences() override   { return m_pPreferences; }
	CEditorCommandManager*       GetCommandManager()         { return m_pCommandManager; }
	ICommandManager*             GetICommandManager();
	static bool                  OnFilterInputEvent(SInputEvent* pInput);
	virtual CTrayArea*           GetTrayArea()           { return m_pTrayArea; }
	virtual ILevelEditor*        GetLevelEditor() override;
	virtual INotificationCenter* GetNotificationCenter() { return m_pNotificationCenter; }
	void                         ExecuteCommand(const char* sCommand, ...);
	CAssetManager*               GetAssetManager()       { return m_pAssetManager; };
	IPythonManager*              GetIPythonManager();
	CEditorPythonManager*        GetPythonManager()      { return m_pPythonManager; };
	void                         SetDocument(CCryEditDoc* pDoc);
	void                         CloseDocument();
	CCryEditDoc*                 GetDocument() const;
	bool                         IsDocumentReady() const;
	virtual bool                 IsMainFrameClosing() const;
	bool                         IsLevelExported() const;
	bool                         SetLevelExported(bool boExported = true);
	void                         InitFinished();
	bool                         IsInitialized() const { return m_bInitialized; }
	bool                         SaveDocument();
	bool                         SaveDocumentBackup();
	ISystem*                     GetSystem() override;
	I3DEngine*                   Get3DEngine() override;
	IRenderer*                   GetRenderer() override;
	void                         WriteToConsole(const char* pszString);

	void                         SetConsoleVar(const char* var, float value);
	void                         SetConsoleStringVar(const char* var, const char* value);

	float                        GetConsoleVar(const char* var);
	//! Query main window of the editor
	HWND                         GetEditorMainWnd()
	{
		if (AfxGetMainWnd())
		{
			return AfxGetMainWnd()->m_hWnd;
		}

		return 0;
	};

	string              GetMasterCDFolder();
	virtual const char* GetLevelName() override;
	virtual const char* GetLevelPath() override;
	string              GetLevelFolder();
	string              GetLevelDataFolder();
	const char*         GetUserFolder();
	virtual bool        IsInGameMode();
	virtual void        SetInGameMode(bool inGame);
	virtual bool        IsInTestMode();
	virtual bool        IsInPreviewMode() override;
	virtual bool        IsInConsolewMode();
	virtual bool        IsInLevelLoadTestMode();
	virtual bool        IsInMatEditMode() { return m_bMatEditMode; }

	//! Enables/Disable updates of editor.
	void                            EnableUpdate(bool enable) { m_bUpdates = enable; };
	void                            EnableAcceleratos(bool bEnable);
	CGameEngine*                    GetGameEngine()           { return m_pGameEngine; };
	void                            SetModifiedFlag(bool modified = true);

	//! Creates a new object. 
	//! bInteractive: when true, will force visibility of the object type, will show and unfreeze the layer if necessary and select the object, 
	//! intended for immediate interaction of the object
	virtual CBaseObject*            NewObject(const char* type, const char* file = nullptr, bool bInteractive = false) override;

	void                            DeleteObject(CBaseObject* obj);
	CBaseObject*                    CloneObject(CBaseObject* obj);
	void                            StartObjectCreation(const char* type, const char* file = nullptr) override;
	IObjectManager*                 GetObjectManager();
	IGizmoManager*                  GetGizmoManager();
	const CSelectionGroup*          GetSelection() const;
	const ISelectionGroup*          GetISelectionGroup() const override;
	int                             ClearSelection();
	CBaseObject*                    GetSelectedObject();
	void                            SelectObject(CBaseObject* obj);
	void                            SelectObjects(std::vector<CBaseObject*> objects);
	void                            LockSelection(bool bLock);
	bool                            IsSelectionLocked();
	void                            PickObject(IPickObjectCallback* callback, CRuntimeClass* targetClass = 0, bool bMultipick = false);
	void                            CancelPick();
	bool                            IsPicking();
	IDataBaseManager*               GetDBItemManager(EDataBaseItemType itemType);
	CEntityPrototypeManager*        GetEntityProtManager()      { return m_pEntityManager; };
	CMaterialManager*               GetMaterialManager()        { return m_pMaterialManager; };
	CParticleManager*               GetParticleManager()        { return m_particleManager; };
	CPrefabManager*                 GetPrefabManager()          { return m_pPrefabManager; };
	virtual CBroadcastManager*      GetGlobalBroadcastManager() { return m_pGlobalBroadcastManager; }
	CGameTokenManager*              GetGameTokenManager()       { return m_pGameTokenManager; };
	CLensFlareManager*              GetLensFlareManager()       { return m_pLensFlareManager; };

	IBackgroundTaskManager*         GetBackgroundTaskManager();
	IBackgroundScheduleManager*     GetBackgroundScheduleManager();
	IBackgroundTaskManagerListener* GetIBackgroundTaskManagerListener();

	IFileChangeMonitor*             GetFileMonitor();
	IIconManager*                   GetIconManager();
	float                           GetTerrainElevation(float x, float y);
	Vec3                            GetTerrainNormal(const Vec3& pos);
	CHeightmap*                     GetHeightmap();
	CVegetationMap*                 GetVegetationMap();
	//////////////////////////////////////////////////////////////////////////
	// Special FG
	//////////////////////////////////////////////////////////////////////////
	CEditorFlowGraphModuleManager* GetFlowGraphModuleManager()  { return m_pFlowGraphModuleManager; }
	CFlowGraphDebuggerEditor*      GetFlowGraphDebuggerEditor() { return m_pFlowGraphDebuggerEditor; }
	CMaterialFXGraphMan*           GetMatFxGraphManager()       { return m_pMatFxGraphManager; }

	virtual IAIManager*            GetAIManager() override;
	CAIManager*                    GetAI() { return m_pAIManager; }

	//////////////////////////////////////////////////////////////////////////
	// UI
	//////////////////////////////////////////////////////////////////////////
	CUIManager* GetUIManager() { return m_pUIManager; }

	//////////////////////////////////////////////////////////////////////////
	CCustomActionsEditorManager* GetCustomActionManager();
	IMovieSystem*                GetMovieSystem()
	{
		if (m_pSystem)
			return m_pSystem->GetIMovieSystem();
		return NULL;
	};

	CPluginManager*                  GetPluginManager()  { return m_pPluginManager; }
	CTerrainManager*                 GetTerrainManager() { return m_pTerrainManager; }
	CViewManager*                    GetViewManager() override;
	virtual IViewportManager*        GetViewportManager() override;
	CViewport*                       GetActiveView();
	IDisplayViewport*                GetActiveDisplayViewport();
	void                             SetActiveView(CViewport* viewport);

	CLevelIndependentFileMan*        GetLevelIndependentFileMan() { return m_pLevelIndependentFileMan; }

	void                             UpdateViews(int flags = 0xFFFFFFFF, AABB* updateRegion = NULL);
	void                             ResetViews();
	void                             UpdateSequencer(bool bOnlyKeys = false);
	void                             SetSelectedRegion(const AABB& box) override;
	void                             GetSelectedRegion(AABB& box) override;
	CRuler*                          GetRuler() override { return m_pRuler; }
	bool                             AddToolbarItem(uint8 iId, IUIEvent* pIHandler);
	void                             SetDataModified();

	void                             SetEditMode(int editMode);
	int                              GetEditMode();
	void                             SetEditTool(CEditTool* tool, bool bStopCurrentTool = true);
	void                             SetEditTool(const string& sEditToolName, bool bStopCurrentTool = true);
	//! Returns current edit tool.
	CEditTool*                       GetEditTool();
	void                             SetAxisConstrains(AxisConstrains axis);
	AxisConstrains                   GetAxisConstrains();
	void                             SetAxisVectorLock(bool bAxisVectorLock) { m_bAxisVectorLock = bAxisVectorLock; }
	bool                             IsAxisVectorLocked()                    { return m_bAxisVectorLock; }
	//! Snap mode flags
	virtual uint16                   GetSnapMode();
	//! Terrain snapping
	virtual void                     EnableSnapToTerrain(bool bEnable);
	virtual bool                     IsSnapToTerrainEnabled() const;
	//! Surface normal snapping
	virtual void                     EnableSnapToNormal(bool bEnable);
	virtual bool                     IsSnapToNormalEnabled() const;
	//! Geometry snapping
	virtual void                     EnableSnapToGeometry(bool bEnable);
	virtual bool                     IsSnapToGeometryEnabled() const override;
	virtual bool                     IsHelpersDisplayed() const;
	virtual void                     EnableHelpersDisplay(bool bEnable);
	virtual bool                     IsPivotSnappingEnabled() const;
	virtual void                     EnablePivotSnapping(bool bEnable);
	void                             SetReferenceCoordSys(RefCoordSys refCoords);
	RefCoordSys                      GetReferenceCoordSys();
	XmlNodeRef                       FindTemplate(const string& templateName);
	void                             AddTemplate(const string& templateName, XmlNodeRef& tmpl);
	virtual void                     OpenAndFocusDataBase(EDataBaseItemType type, IDataBaseItem* pItem) override;
	CBaseLibraryDialog*              OpenDataBaseLibrary(EDataBaseItemType type, IDataBaseItem* pItem = NULL);
	CWnd*                            OpenView(const char* sViewClassName) override;
	CWnd*                            FindView(const char* sViewClassName) override;
	IPane*                           CreateDockable(const char* className) override;
	void                             RaiseDockable(IPane* pPane) override;
	QWidget*                         CreatePreviewWidget(const QString& file, QWidget* pParent = nullptr) override;
	virtual void                     PostOnMainThread(std::function<void()> task) override;
	bool                             CloseView(const char* sViewClassName);
	bool                             SelectColor(COLORREF& color, CWnd* parent = 0);
	void                             Update();
	Version                          GetFileVersion()    { return m_fileVersion; };
	Version                          GetProductVersion() { return m_productVersion; };
	//! Get shader enumerator.
	IUndoManager*                    GetIUndoManager() { return m_pUndoManager; };
	//! Retrieve current animation context.
	void                             Notify(EEditorNotifyEvent event);
	void                             RegisterNotifyListener(IEditorNotifyListener* listener);
	void                             UnregisterNotifyListener(IEditorNotifyListener* listener);

	//! Retrieve interface to the source control.
	ISourceControl*                  GetSourceControl();
	//! Get project manager interface
	virtual IProjectManager*         GetProjectManager() override;
	//! Retrieve true if source control is provided and enabled in settings
	virtual bool                     IsSourceControlAvailable() override;
	//! Setup Material Editor mode
	void                             SetMatEditMode(bool bIsMatEditMode);
	CFlowGraphManager*               GetFlowGraphManager() override { return m_pFlowGraphManager; };
	CUIEnumsDatabase*                GetUIEnumsDatabase()           { return m_pUIEnumsDatabase; };
	void                             AddUIEnums();
	void                             GetMemoryUsage(ICrySizer* pSizer);
	void                             ReduceMemory();
	// Get Export manager
	IExportManager*                  GetExportManager();
	// Get Dev manager
	IDevManager*                     GetDevManager() const { return m_pDevManager; }
	// Set current configuration spec of the editor.
	void                             SetEditorConfigSpec(ESystemConfigSpec spec);
	ESystemConfigSpec                GetEditorConfigSpec() const;
	void                             ReloadTemplates();
	IResourceSelectorHost*           GetResourceSelectorHost() { return m_pResourceSelectorHost.get(); }
	virtual void                     ShowStatusText(bool bEnable);
	virtual FileSystem::CEnumerator* GetFileSystemEnumerator();

	virtual void                     OnObjectContextMenuOpened(CPopupMenuItem* pMenu, const CBaseObject* pObject);

	virtual void                     RegisterObjectContextMenuExtension(TContextMenuExtensionFunc func);

	virtual void                     SetCurrentMissionTime(float time);
	virtual float                    GetCurrentMissionTime();

	//ISystemEventListener interface
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	//~ISystemEventListener interface

	//IUriEventListener interface
	virtual void RegisterURIListener(IUriEventListener* pListener, const char* szSchema);
	virtual void UnregisterURIListener(IUriEventListener* pListener, const char* szSchema);
	//~IUriEventListener interface

	virtual void           RequestScriptReload(const EReloadScriptsType& kind);

	void                   AddNativeHandler(uintptr_t id, std::function<bool(void*, long*)> callback);
	void                   RemoveNativeHandler(uintptr_t id);

	virtual void           EnableDevMode(bool bEnable) { bDevMode = bEnable; }
	virtual bool           IsDevModeEnabled() const    { return bDevMode; }

	virtual bool           IsUpdateSuspended() const override;
	virtual bool           IsGameInputSuspended() override;
	virtual void           ToggleGameInputSuspended() override;

	CConfigurationManager* GetConfigurationManager() const { return m_pConfigurationManager; }

	// CBaseObject hacks
	virtual bool             PickObject(const Vec3& vWorldRaySrc, const Vec3& vWorldRayDir, SRayHitInfo& outHitInfo, CBaseObject* pObject);
	virtual void             AddWaitProgress(CWaitProgress* pProgress);
	virtual void             RemoveWaitProgress(CWaitProgress* pProgress);
	virtual void             RegisterEntity(IRenderNode* pRenderNode);
	virtual void             UnRegisterEntityAsJob(IRenderNode* pRenderNode);
	virtual void             SyncPrefabCPrefabObject(CBaseObject* oObject, const SObjectChangedContext& context);
	virtual bool             IsModifyInProgressCPrefabObject(CBaseObject* oObject);
	virtual void             OnGroupMake() override;
	virtual void             OnGroupAttach() override;
	virtual void             OnGroupDetach() override;
	virtual void             OnGroupDetachToRoot() override;
	virtual void             OnLinkTo() override;
	virtual void             OnUnLink() override;
	virtual void             OnPrefabMake();
	virtual IEditorMaterial* LoadMaterial(const string& name);
	virtual void             OnRequestMaterial(IMaterial* pMatInfo);
	virtual bool             IsCGroup(CBaseObject* pObject);
	virtual bool             IsCPrefabObject(CBaseObject* pObject);
	virtual bool             IsGroupOpen(CBaseObject* pObject);
	virtual void             OpenGroup(CBaseObject* pObject) override;
	virtual void             ResumeUpdateCGroup(CBaseObject* pObject);
	virtual bool             SuspendUpdateCGroup(CBaseObject* pObject);
	virtual void             RemoveMemberCGroup(CBaseObject* pGroup, CBaseObject* pObject);
	virtual void             RegisterDeprecatedPropertyEditor(int propertyType, std::function<bool(const string& /*oldValue*/, string& /*newValueOut*/)>& handler) override;
	bool                     EditDeprecatedProperty(int propertyType, const string& oldValue, string& newValueOut);
	bool                     CanEditDeprecatedProperty(int propertyType);

	//This method only exists to avoid dependency to MFCToolsPlugin
	virtual bool ScanDirectory(const string& path, const string& fileSpec, std::vector<string>& filesOut, bool recursive = true) override;
	virtual void SetPlayerViewMatrix(const Matrix34& tm, bool bEyePos = true);

protected:
	typedef std::map<string, CListenerSet<IUriEventListener*>> TURIListeners;

	void DetectVersion();
	void SetMasterCDFolder();
	void OnObjectHideMaskChanged();

	//! List of all notify listeners.
	std::list<IEditorNotifyListener*> m_listeners;

	int                               m_objectHideMask;
	EEditMode                         m_currEditMode;
	EEditMode                         m_prevEditMode;
	EOperationMode                    m_operationMode;
	ISystem*                          m_pSystem;
	CClassFactory*                    m_pClassFactory;
	CEditorCommandManager*            m_pCommandManager;
	CAssetManager*                    m_pAssetManager;
	CTrayArea*                        m_pTrayArea;
	INotificationCenter*              m_pNotificationCenter;
	CPersonalizationManager*          m_pPersonalizationManager;
	CPreferences*                     m_pPreferences;
	CEditorPythonManager*             m_pPythonManager;
	CPolledKeyManager*                m_pPolledKeyManager;
	CObjectManager*                   m_pObjectManager;
	CGizmoManager*                    m_pGizmoManager;
	CPluginManager*                   m_pPluginManager;
	CViewManager*                     m_pViewManager;
	IUndoManager*                     m_pUndoManager;
	AABB                              m_selectedRegion;
	AxisConstrains                    m_lastAxis[16];
	RefCoordSys                       m_lastCoordSys[16];
	uint16                            m_snapModeFlags;
	bool                              m_areHelpersEnabled;
	bool                              m_bPivotSnappingEnabled;
	bool                              m_bAxisVectorLock;
	bool                              m_bUpdates;
	Version                           m_fileVersion;
	Version                           m_productVersion;
	CXmlTemplateRegistry              m_templateRegistry;
	_smart_ptr<CEditTool>             m_pEditTool;
	CIconManager*                     m_pIconManager;
	wstring                           m_masterCDFolder;
	string                            m_userFolder;
	bool                              m_bSelectionLocked;
	CEditTool*                        m_pPickTool;
	CAIManager*                       m_pAIManager;
	CUIManager*                       m_pUIManager;
	CCustomActionsEditorManager*      m_pCustomActionsManager;
	CEditorFlowGraphModuleManager*    m_pFlowGraphModuleManager;
	CMaterialFXGraphMan*              m_pMatFxGraphManager;
	CFlowGraphDebuggerEditor*         m_pFlowGraphDebuggerEditor;
	CGameEngine*                      m_pGameEngine;
	CEntityPrototypeManager*          m_pEntityManager;
	CMaterialManager*                 m_pMaterialManager;
	CParticleManager*                 m_particleManager;
	CPrefabManager*                   m_pPrefabManager;
	CBroadcastManager*                m_pGlobalBroadcastManager;
	CGameTokenManager*                m_pGameTokenManager;
	CLensFlareManager*                m_pLensFlareManager;
	//! Global instance of error report class.
	//! Source control interface.
	ISourceControl*           m_pSourceControl;
	CFlowGraphManager*        m_pFlowGraphManager;

	CUIEnumsDatabase*         m_pUIEnumsDatabase;
	//! Currently used ruler
	CRuler*                   m_pRuler;
	EditorScriptEnvironment*  m_pScriptEnv;
	//! CConsole Synchronization
	CConsoleSynchronization*  m_pConsoleSync;

	CLevelIndependentFileMan* m_pLevelIndependentFileMan;

	//! Export manager for exporting objects and a terrain from the game to DCC tools
	CExportManager*                                                m_pExportManager;
	IDevManager*                                                   m_pDevManager;
	CTerrainManager*                                               m_pTerrainManager;
	CVegetationMap*                                                m_pVegetationMap;
	CConfigurationManager*                                         m_pConfigurationManager;
	CMainThreadWorker*                                             m_pMainThreadWorker;
	std::auto_ptr<BackgroundTaskManager::CTaskManager>             m_pBackgroundTaskManager;
	std::auto_ptr<BackgroundScheduleManager::CScheduleManager>     m_pBackgroundScheduleManager;
	std::auto_ptr<BackgroundTaskManager::CBackgroundTasksListener> m_pBGTasks;
	std::auto_ptr<CEditorFileMonitor>                              m_pEditorFileMonitor;
	std::auto_ptr<IResourceSelectorHost>                           m_pResourceSelectorHost;
	std::unique_ptr<FileSystem::CEnumerator>                       m_pFileSystemEnumerator;
	string m_selectFileBuffer;
	string m_levelNameBuffer;
	std::map<int, std::function<bool(const string&, string&)>> m_deprecatedPropertyEditors;

	//! True if the editor is in material edit mode. Fast preview of materials.
	//! In this mode only very limited functionality is available.
	bool m_bMatEditMode;
	bool m_bShowStatusText;
	int  doc_use_subfolder_for_crash_backup;
	bool m_bInitialized;

	// NOTE: Closing is the process of shutting down the user interface. Exiting is the process of shutting down the entire application.
	// Closing happens before exiting.
	// TODO: Refactor this and keep only one of these.
	bool m_bExiting;

	static void CmdPy(IConsoleCmdArgs* pArgs);

	std::vector<TContextMenuExtensionFunc> m_objectContextMenuExtensions;
	TURIListeners                          m_uriListeners;

	std::set<EReloadScriptsType>           m_reloadScriptsQueue;

	ESystemConfigSpec                      editorConfigSpec;

	bool bDevMode;
};

CEditorImpl* GetIEditorImpl();

