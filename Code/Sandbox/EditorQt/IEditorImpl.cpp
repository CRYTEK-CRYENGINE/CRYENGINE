// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IEditorImpl.h"

#include "AI/AIManager.h"
#include "Commands/CommandManager.h"
#include "Commands/PolledKeyManager.h"
#include "Commands/PythonManager.h"
#include "CustomActions/CustomActionsEditorManager.h"
#include "Export/ExportManager.h"
#include "FileSystem/FileSystem_Enumerator.h"
#include "GameTokens/GameTokenManager.h"
#include "HyperGraph/Controls/FlowGraphDebuggerEditor.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphModuleManager.h"
#include "LensFlareEditor/LensFlareManager.h"
#include "Material/Material.h"
#include "Material/MaterialBrowser.h"
#include "Material/MaterialFXGraphMan.h"
#include "Material/MaterialManager.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/ObjectManager.h"
#include "Objects/PrefabObject.h"
#include "Particles/ParticleManager.h"
#include "Prefabs/PrefabManager.h"
#include "QT/QtMainFrame.h"
#include "QT/QToolTabManager.h"
#include "QT/Widgets/QPreviewWidget.h"
#include "Script/ScriptEnvironment.h"
#include "Terrain/TerrainManager.h"
#include "UI/UIManager.h"
#include "Undo/Undo.h"
#include "Util/Ruler.h"
#include "Vegetation/VegetationMap.h"

#include "BackgroundScheduleManager.h"
#include "BackgroundTaskManager.h"
#include "BaseLibraryDialog.h"
#include "ClassFactory.h"
#include "CryEdit.h"
#include "CryEditDoc.h"
#include "DataBaseDialog.h"
#include "EditorFileMonitor.h"
#include "EntityPrototypeManager.h"
#include "GameEngine.h"
#include "IconManager.h"
#include "IDevManager.h"
#include "LogFile.h"
#include "MainThreadWorker.h"
#include "Mission.h"
#include "ObjectCreateTool.h"
#include "PhysTool.h"
#include "PluginManager.h"
#include "ResourceSelectorHost.h"
#include "SurfaceInfoPicker.h"
#include "ViewManager.h"

// MFC
#include <Dialogs/CustomColorDialog.h>
#include <MFCToolsPlugin.h>

// EditorCommon
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/Browser/AssetModel.h>
#include <ConfigurationManager.h>
#include <EditorCommonInit.h>
#include <EditorFramework/BroadcastManager.h>
#include <EditorFramework/PersonalizationManager.h>
#include <EditorFramework/Preferences.h>
#include <EditorFramework/ToolBar/ToolBarService.h>
#include <EditorFramework/TrayArea.h>
#include <Gizmos/GizmoManager.h>
#include <ISourceControl.h>
#include <LevelEditor/Tools/ObjectMode.h>
#include <Notifications/NotificationCenterImpl.h>
#include <PathUtils.h>
#include <Preferences/GeneralPreferences.h>
#include <Preferences/ViewportPreferences.h>
#include <UIEnumsDatabase.h>
#include <CrySystem/ConsoleRegistration.h>

#include <CrySandbox/CryInterop.h>
#include <CrySandbox/IEditorGame.h>

LINK_SYSTEM_LIBRARY("version.lib")

// even in Release mode, the editor will return its heap, because there's no Profile build configuration for the editor
#ifdef _RELEASE
	#undef _RELEASE
#endif
#include <CryInput/IInput.h>
#include <CryCore/CrtDebugStats.h>

static CCryEditDoc * theDocument;
static CEditorImpl* s_pEditor = nullptr;

IEditor*     GetIEditor() { return s_pEditor; }

CEditorImpl* GetIEditorImpl()
{
	return s_pEditor;
}

CEditorImpl::CEditorImpl(CGameEngine* ge)
	: m_bInitialized(false)
	, m_objectHideMask(0)
	, editorConfigSpec(CONFIG_MEDIUM_SPEC)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	//This is dangerous and should (in theory) be set at the end of the constructor for safety, after everything is properly initialized.
	//Code within this scope can use GetIEditorImpl() at their own risk
	s_pEditor = this;

	//init game engine first as there may be a lot of things depending on pSystem or gEnv.
	m_pSystem = ge->GetSystem();
	m_pGameEngine = ge;
	RegisterNotifyListener(m_pGameEngine);

	EditorCommon::SetIEditor(this);
	MFCToolsPlugin::SetEditor(s_pEditor);

	m_pLevelIndependentFileMan = new CLevelIndependentFileMan;
	m_pLevelEditorSharedState.reset(new CLevelEditorSharedState);
	m_pExportManager = nullptr;
	SetMasterCDFolder();
	m_bExiting = false;
	m_pClassFactory = CClassFactory::Instance();

	m_pGlobalBroadcastManager = new CBroadcastManager;
	m_pNotificationCenter = new CNotificationCenter;
	m_pTrayArea = new CTrayArea;
	m_pToolBarService = new CToolBarService;
	m_pCommandManager = new CEditorCommandManager;
	m_pPersonalizationManager = new CPersonalizationManager;
	m_pPreferences = new CPreferences;
	CAutoRegisterPreferencesHelper::RegisterAll();
	m_pPythonManager = new CEditorPythonManager;
	m_pPythonManager->Init();//must be initialized before plugins are initialized
	m_pAssetManager = new CAssetManager;
	CAssetModel::CAutoRegisterColumn::RegisterAll();
	m_pPolledKeyManager = new CPolledKeyManager;
	m_pEditorFileMonitor.reset(new CEditorFileMonitor);
	m_pBackgroundTaskManager.reset(new BackgroundTaskManager::CTaskManager);
	m_pBackgroundScheduleManager.reset(new BackgroundScheduleManager::CScheduleManager);
	m_pBGTasks.reset(new BackgroundTaskManager::CBackgroundTasksListener);

	m_pUIEnumsDatabase = new CUIEnumsDatabase;
	m_pPluginManager = new CPluginManager;
	m_pTerrainManager = new CTerrainManager;
	m_pVegetationMap = new CVegetationMap;
	m_pObjectManager = new CObjectManager;
	m_pGizmoManager = new CGizmoManager;
	m_pViewManager = new CViewManager;
	m_pIconManager = new CIconManager;
	m_pUndoManager = new CUndoManager;
	m_pAIManager = new CAIManager;
	m_pUIManager = new CUIManager;
	m_pCustomActionsManager = new CCustomActionsEditorManager;
	m_pMaterialManager = new CMaterialManager;
	m_pEntityManager = new CEntityPrototypeManager;
	m_particleManager = new CParticleManager;
	m_pPrefabManager = new CPrefabManager;
	m_pGameTokenManager = new CGameTokenManager;
	m_pFlowGraphManager = new CFlowGraphManager;

	m_pLensFlareManager = new CLensFlareManager;
	m_pFlowGraphModuleManager = new CEditorFlowGraphModuleManager;
	m_pFlowGraphDebuggerEditor = new CFlowGraphDebuggerEditor;
	m_pMatFxGraphManager = new CMaterialFXGraphMan;
	m_pDevManager = new CDevManager;
	m_pSourceControl = nullptr;
	m_pScriptEnv = new EditorScriptEnvironment();

	m_pResourceSelectorHost.reset(CreateResourceSelectorHost());
	CAutoRegisterResourceSelector::RegisterAll();

	m_pRuler = new CRuler;
	m_pConfigurationManager = new CConfigurationManager;

	m_pMainThreadWorker = new CMainThreadWorker;

	m_bUpdates = true;

	m_bSelectionLocked = false;

	m_bMatEditMode = false;
	m_bShowStatusText = true;

	doc_use_subfolder_for_crash_backup = 0;

	CPhysPullTool::RegisterCVars();

	MFCToolsPlugin::Initialize();
	EditorCommon::Initialize();

	m_pGameEngine->InitAdditionalEngineComponents();
}

void CEditorImpl::Init()
{
	m_pCommandManager->RegisterAutoCommands();
	AddUIEnums();

	if (gEnv->pRenderer)
		gEnv->pRenderer->AddAsyncTextureCompileListener(m_pBGTasks.get());

	m_pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CEditorImpl");

	gEnv->pInput->OnFilterInputEvent = OnFilterInputEvent;

	CAutoRegisterClassHelper::RegisterAll();

	m_pPolledKeyManager->Init();

	m_pBackgroundTaskManager->AddListener(m_pBGTasks.get(), "Editor");

	auto projectPath = PathUtil::GetGameProjectAssetsPath();
	m_pFileSystemEnumerator.reset(new FileSystem::CEnumerator(projectPath.c_str()));

	m_pNotificationCenter->Init();

	DetectVersion();

	m_templateRegistry.LoadTemplates("%EDITOR%");

	m_pConfigurationManager->Init();
}

CEditorImpl::~CEditorImpl()
{
	m_pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	m_bExiting = true; // Can't save level after this point (while Crash)
	SAFE_DELETE(m_pScriptEnv);
	SAFE_RELEASE(m_pSourceControl);
	SAFE_DELETE(m_pGameTokenManager);

	SAFE_DELETE(m_pMatFxGraphManager);
	SAFE_DELETE(m_pFlowGraphModuleManager);

	if (m_pFlowGraphDebuggerEditor)
	{
		m_pFlowGraphDebuggerEditor->Shutdown();
		SAFE_DELETE(m_pFlowGraphDebuggerEditor);
	}

	SAFE_DELETE(m_particleManager)
	SAFE_DELETE(m_pEntityManager)
	SAFE_DELETE(m_pMaterialManager)
	SAFE_DELETE(m_pIconManager)
	SAFE_DELETE(m_pViewManager)
	SAFE_DELETE(m_pObjectManager)  // relies on prefab manager
	SAFE_DELETE(m_pPrefabManager); // relies on flowgraphmanager
	SAFE_DELETE(m_pGizmoManager)
	SAFE_DELETE(m_pFlowGraphManager);
	SAFE_DELETE(m_pVegetationMap);
	SAFE_DELETE(m_pTerrainManager);
	// AI should be destroyed after the object manager, as the objects may
	// refer to AI components.
	SAFE_DELETE(m_pAIManager)
	SAFE_DELETE(m_pUIManager)
	SAFE_DELETE(m_pCustomActionsManager)
	SAFE_DELETE(m_pPluginManager)
	SAFE_DELETE(m_pRuler)
	SAFE_DELETE(m_pPreferences)
	SAFE_DELETE(m_pPersonalizationManager)
	SAFE_DELETE(m_pCommandManager)
	SAFE_DELETE(m_pToolBarService)
	SAFE_DELETE(m_pTrayArea)
	SAFE_DELETE(m_pNotificationCenter)
	SAFE_DELETE(m_pPythonManager)
	SAFE_DELETE(m_pPolledKeyManager)

	CAutoRegisterClassHelper::UnregisterAll();
	SAFE_DELETE(m_pClassFactory)

	SAFE_DELETE(m_pUIEnumsDatabase)
	SAFE_DELETE(m_pExportManager);
	SAFE_DELETE(m_pDevManager);
	SAFE_DELETE(m_pConfigurationManager);
	// Game engine should be among the last things to be destroyed, as it
	// destroys the engine.
	SAFE_DELETE(m_pLevelIndependentFileMan);

	UnregisterNotifyListener(m_pGameEngine);
	SAFE_DELETE(m_pGameEngine);

	m_pGlobalBroadcastManager->deleteLater();

	EditorCommon::Deinitialize();
	MFCToolsPlugin::Deinitialize();
	s_pEditor = nullptr;
}

bool CEditorImpl::IsMainFrameClosing() const
{
	return CEditorMainFrame::GetInstance()->IsClosing();
}

void CEditorImpl::SetMasterCDFolder()
{
	m_masterCDFolder = wstring(L".");
}

bool CEditorImpl::OnFilterInputEvent(SInputEvent* pInput)
{
	CryInputEvent event(pInput);
	event.SendToKeyboardFocus();
	return event.isAccepted();
}

ILevelEditor* CEditorImpl::GetLevelEditor()
{
	return CEditorMainFrame::GetInstance()->GetLevelEditor();
}

void CEditorImpl::ExecuteCommand(const char* sCommand, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, sCommand);
	cry_vsprintf(buffer, sCommand, args);
	va_end(args);
	m_pCommandManager->Execute(buffer);
}

IPythonManager* CEditorImpl::GetIPythonManager()
{
	return m_pPythonManager;
}

ICommandManager* CEditorImpl::GetICommandManager()
{
	return m_pCommandManager;
}

void CEditorImpl::WriteToConsole(const char* pszString)
{
	CLogFile::WriteLine(pszString);
}

const ISelectionGroup* CEditorImpl::GetISelectionGroup() const
{
	return GetSelection();
}

void CEditorImpl::Update()
{
	if (!m_bUpdates)
		return;

	// Make sure this is not called recursively
	m_bUpdates = false;

	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	m_pRuler->Update();

	//@FIXME: Restore this latter.
	//if (GetGameEngine() && GetGameEngine()->IsLevelLoaded())
	{
		m_pObjectManager->Update();
	}
	if (IsInPreviewMode())
	{
		SetModifiedFlag(FALSE);
	}

	if (m_pGameEngine != nullptr)
	{
		IEditorGame* pEditorGame = m_pGameEngine->GetIEditorGame();
		if (pEditorGame != nullptr)
		{
			auto* pActiveViewport = GetActiveDisplayViewport();
			pEditorGame->UpdateHelpers(pActiveViewport ? pActiveViewport->GetHelperSettings().enabled : false);
		}
	}

	for (EReloadScriptsType kind : m_reloadScriptsQueue)
	{
		WPARAM p;
		switch (kind)
		{
		case eReloadScriptsType_Actor:
			p = ID_RELOAD_ACTOR_SCRIPTS;
			break;
		case eReloadScriptsType_AI:
			p = ID_RELOAD_AI_SCRIPTS;
			break;
		case eReloadScriptsType_Entity:
			p = ID_RELOAD_ENTITY_SCRIPTS;
			break;
		case eReloadScriptsType_Item:
			p = ID_RELOAD_ITEM_SCRIPTS;
			break;
		case eReloadScriptsType_UI:
			p = ID_RELOAD_UI_SCRIPTS;
			break;
		default:
			continue;
		}
		AfxGetMainWnd()->SendMessage(WM_COMMAND, p, 0);
	}
	m_reloadScriptsQueue.clear();

	m_bUpdates = true;
}

ISystem* CEditorImpl::GetSystem()
{
	return m_pSystem;
}

I3DEngine* CEditorImpl::Get3DEngine()
{
	if (gEnv)
		return gEnv->p3DEngine;
	return 0;
}

IRenderer* CEditorImpl::GetRenderer()
{
	if (gEnv)
		return gEnv->pRenderer;
	return 0;
}

IEditorClassFactory* CEditorImpl::GetClassFactory()
{
	return m_pClassFactory;
}

CCryEditDoc* CEditorImpl::GetDocument() const
{
	return theDocument;
}

void CEditorImpl::SetDocument(CCryEditDoc* pDoc)
{
	theDocument = pDoc;
}

void CEditorImpl::CloseDocument()
{
	if (theDocument)
	{
		CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
		Notify(eNotify_OnBeginSceneClose);
		theDocument->DeleteContents();
		delete theDocument;
		// might be better to just recreate an empty document instead to make sure no invalid access happens
		theDocument = nullptr;
		Notify(eNotify_OnEndSceneClose);
	}
}

void CEditorImpl::SetModifiedFlag(bool modified)
{
	if (GetDocument() && GetDocument()->IsDocumentReady())
	{
		GetDocument()->SetModifiedFlag(modified);

		if (modified)
		{
			GetDocument()->SetLevelExported(false);
		}
	}
}

bool CEditorImpl::IsLevelExported() const
{
	CCryEditDoc* pDoc = GetDocument();

	if (pDoc)
	{
		return pDoc->IsLevelExported();
	}

	return false;
}

bool CEditorImpl::SetLevelExported(bool boExported)
{
	if (GetDocument())
	{
		GetDocument()->SetLevelExported(boExported);
		return true;
	}
	return false;
}

bool CEditorImpl::SaveDocument()
{
	if (m_bExiting)
		return false;

	if (GetDocument())
		return GetDocument()->Save();
	else
		return false;
}

bool CEditorImpl::SaveDocumentBackup()
{
	if (m_bExiting)
		return false;

	// Assets backup
	if (doc_use_subfolder_for_crash_backup)
	{
		const CTime theTime = CTime::GetCurrentTime();
		const string backupFolder = PathUtil::Make(PathUtil::GetUserSandboxFolder(), theTime.Format("CrashBackup/%Y.%m.%d [%H-%M.%S]"));
		GetAssetManager()->SaveBackup(backupFolder);
	}
	else
	{
		GetAssetManager()->SaveAll(nullptr);
	}

	const char* const CRASH_FOLDER = "/.CRASH_BACKUP/";
	const char* const CRASH_FILE_PREFIX = "CRASH_BACKUP_";

	if (CCryEditDoc* pDocument = GetDocument())
	{
		bool bShouldCreateBackup = true;
		if (doc_use_subfolder_for_crash_backup)
		{
			string sLevelFolder = pDocument->GetPathName().GetString(); // copy the string so we can modify it
			if (sLevelFolder.GetLength() != 0)
			{
				IObjectManager* pObjectManager = GetObjectManager();
				if (!pObjectManager)
				{
					return false;
				}

				CObjectLayerManager* pObjectLayerManager = pObjectManager->GetLayersManager();
				if (!pObjectLayerManager)
				{
					return false;
				}

				string backupFolder = sLevelFolder.GetString(); // get the modifiable string buffer
				backupFolder = PathUtil::GetParentDirectory(backupFolder);
				backupFolder.Append(CRASH_FOLDER);

				if (::CreateDirectory(backupFolder, NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
				{
					string oldLayerPath = pObjectLayerManager->GetLayersPath();
					string newLayerPath(CRASH_FOLDER);
					newLayerPath.Append(oldLayerPath);
					pObjectLayerManager->SetLayersPath(newLayerPath); // make sure layers are saved in the crash folder, not the current folder
					string fullLayerPath(backupFolder);
					fullLayerPath.Append(oldLayerPath);
					if (::CreateDirectory(fullLayerPath.GetString(), NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
					{
						string sLevelName = pDocument->GetPathName(); // copy the string so we can modify it
						sLevelName = PathUtil::GetFile(sLevelName);
						char szCrashBackupName[MAX_PATH];
						cry_sprintf(szCrashBackupName, "%s%s%s", backupFolder.GetString(), CRASH_FILE_PREFIX, sLevelName.GetString());

						pDocument->SetPathName(szCrashBackupName, false);
					}
					else
					{
						bShouldCreateBackup = false;
					}

					pObjectLayerManager->SetLayersPath(oldLayerPath);
				}
				else
				{
					bShouldCreateBackup = false;
				}
			}
		}

		if (bShouldCreateBackup)
		{
			return pDocument->Save();
		}
	}

	return false;
}

string CEditorImpl::GetMasterCDFolder()
{
	return CryStringUtils::WStrToUTF8(m_masterCDFolder.c_str());
}

string CEditorImpl::GetLevelFolder()
{
	return GetGameEngine()->GetLevelPath();
}

const char* CEditorImpl::GetLevelName()
{
	m_levelNameBuffer = GetGameEngine()->GetLevelName();
	return m_levelNameBuffer.GetBuffer();
}

const char* CEditorImpl::GetLevelPath()
{
	return GetLevelFolder();
}

string CEditorImpl::GetLevelDataFolder()
{
	return PathUtil::AddSlash(PathUtil::Make(GetGameEngine()->GetLevelPath(), "LevelData")).c_str();
}

const char* CEditorImpl::GetUserFolder()
{
	m_userFolder = string(PathUtil::GetUserSandboxFolder().c_str());
	return m_userFolder;
}

void CEditorImpl::SetDataModified()
{
	GetDocument()->SetModifiedFlag(TRUE);
}

CBaseObject* CEditorImpl::NewObject(const char* type, const char* file /*=nullptr*/, bool bInteractive /*= false*/)
{
	CUndo undo("Create new object");

	CBaseObject* pObject = GetObjectManager()->NewObject(type, 0, file);
	if (pObject)
	{
		if (bInteractive)
		{
			Matrix34 objectTM;
			objectTM.SetIdentity();
			pObject->SetLocalTM(objectTM);

			// if this object type was hidden by category, re-display it.
			int hideMask = gViewportDebugPreferences.GetObjectHideMask();
			hideMask = hideMask & ~(pObject->GetType());
			gViewportDebugPreferences.SetObjectHideMask(hideMask);

			// Enable display of current layer.
			CObjectLayer* pLayer = m_pObjectManager->GetLayersManager()->GetCurrentLayer();
			assert(pLayer);
			pLayer->SetFrozen(false);
			pLayer->SetVisible(true);
			pLayer->SetModified();

			m_pObjectManager->SelectObject(pObject);
		}

		SetModifiedFlag();
		return pObject;
	}
	else
	{
		return nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DeleteObject(CBaseObject* obj)
{
	SetModifiedFlag();
	GetObjectManager()->DeleteObject(obj);
}

CBaseObject* CEditorImpl::CloneObject(CBaseObject* obj)
{
	SetModifiedFlag();
	return GetObjectManager()->CloneObject(obj);
}

bool CEditorImpl::StartObjectCreation(const char* type, const char* file /*= nullptr*/)
{
	if (!GetDocument()->IsDocumentReady())
	{
		return false;
	}

	CObjectCreateTool* tool = new CObjectCreateTool();
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(tool);
	tool->SelectObjectToCreate(type, file);

	return true;
}

CBaseObject* CEditorImpl::GetSelectedObject()
{
	if (m_pObjectManager->GetSelection()->GetCount() != 1)
		return 0;
	return m_pObjectManager->GetSelection()->GetObject(0);
}

IObjectManager* CEditorImpl::GetObjectManager()
{
	return m_pObjectManager;
}

IGizmoManager* CEditorImpl::GetGizmoManager()
{
	return m_pGizmoManager;
}

const CSelectionGroup* CEditorImpl::GetSelection() const
{
	return m_pObjectManager->GetSelection();
}

void CEditorImpl::LockSelection(bool bLock)
{
	// Selection must be not empty to enable selection lock.
	if (!GetSelection()->IsEmpty())
		m_bSelectionLocked = bLock;
	else
		m_bSelectionLocked = false;
}

bool CEditorImpl::IsSelectionLocked()
{
	return m_bSelectionLocked;
}

bool CEditorImpl::IsUpdateSuspended() const
{
	return CCryEditApp::GetInstance()->IsUpdateSuspended();
}

bool CEditorImpl::IsGameInputSuspended()
{
	return GetGameEngine()->IsGameInputSuspended();
}

void CEditorImpl::ToggleGameInputSuspended()
{
	return GetGameEngine()->ToggleGameInputSuspended();
}

CLevelEditorSharedState* CEditorImpl::GetLevelEditorSharedState()
{
	return m_pLevelEditorSharedState.get();
}

CViewManager* CEditorImpl::GetViewManager()
{
	return m_pViewManager;
}

IViewportManager* CEditorImpl::GetViewportManager()
{
	return static_cast<IViewportManager*>(GetViewManager());
}

CViewport* CEditorImpl::GetActiveView()
{
	return GetViewManager()->GetActiveViewport();
}

IDisplayViewport* CEditorImpl::GetActiveDisplayViewport()
{
	return GetViewManager()->GetActiveViewport();
}

void CEditorImpl::SetActiveView(CViewport* viewport)
{
	m_pViewManager->SelectViewport(viewport);
}

void CEditorImpl::UpdateViews(int flags, AABB* updateRegion)
{
	AABB prevRegion = m_pViewManager->GetUpdateRegion();
	if (updateRegion)
		m_pViewManager->SetUpdateRegion(*updateRegion);
	m_pViewManager->UpdateViews(flags);
	if (updateRegion)
		m_pViewManager->SetUpdateRegion(prevRegion);
}

void CEditorImpl::UpdateSequencer(bool bOnlyKeys)
{
	if (bOnlyKeys)
		Notify(eNotify_OnUpdateSequencerKeys);
	else
		Notify(eNotify_OnUpdateSequencer);
}

void CEditorImpl::ResetViews()
{
	m_pViewManager->ResetViews();
}

IIconManager* CEditorImpl::GetIconManager()
{
	return m_pIconManager;
}

IBackgroundTaskManager* CEditorImpl::GetBackgroundTaskManager()
{
	return m_pBackgroundTaskManager.get();
}

IBackgroundScheduleManager* CEditorImpl::GetBackgroundScheduleManager()
{
	return m_pBackgroundScheduleManager.get();
}

IBackgroundTaskManagerListener* CEditorImpl::GetIBackgroundTaskManagerListener()
{
	return m_pBGTasks.get();
}

IFileChangeMonitor* CEditorImpl::GetFileMonitor()
{
	return m_pEditorFileMonitor.get();
}

float CEditorImpl::GetTerrainElevation(float x, float y)
{
	I3DEngine* engine = m_pSystem->GetI3DEngine();
	if (!engine)
		return 0;
	return engine->GetTerrainElevation(x, y);
}

Vec3 CEditorImpl::GetTerrainNormal(const Vec3& pos)
{
	I3DEngine* pEngine = m_pSystem->GetI3DEngine();
	if (!pEngine)
		return Vec3();

	return pEngine->GetTerrainSurfaceNormal(pos);
}

CHeightmap* CEditorImpl::GetHeightmap()
{
	assert(m_pTerrainManager);
	return m_pTerrainManager->GetHeightmap();
}

CVegetationMap* CEditorImpl::GetVegetationMap()
{
	return m_pVegetationMap;
}

CWnd* CEditorImpl::OpenView(const char* szViewClassName)
{
	return CTabPaneManager::GetInstance()->OpenMFCPane(szViewClassName);
}

CWnd* CEditorImpl::FindView(const char* szViewClassName)
{
	return CTabPaneManager::GetInstance()->FindMFCPane(szViewClassName);
}

IPane* CEditorImpl::CreateDockable(const char* szClassName)
{
	return CTabPaneManager::GetInstance()->CreatePane(szClassName);
}

IPane* CEditorImpl::FindDockable(const char* szClassName)
{
	return CTabPaneManager::GetInstance()->FindPaneByClass(szClassName);
}

std::vector<IPane*> CEditorImpl::FindAllDockables(const char* szClassName)
{
	return CTabPaneManager::GetInstance()->FindAllPanelsByClass(szClassName);
}

IPane* CEditorImpl::FindDockableIf(const std::function<bool(IPane*, const string& /*className*/)>& predicate)
{
	return CTabPaneManager::GetInstance()->FindPane(predicate);
}

void CEditorImpl::RaiseDockable(IPane* pPane)
{
	CTabPaneManager::GetInstance()->BringToFront(pPane);
}

QWidget* CEditorImpl::CreatePreviewWidget(const QString& file, QWidget* pParent)
{
	const static QStringList validGeometryExtensions = []()
	{
		QStringList result;
		result << QStringLiteral(CRY_SKEL_FILE_EXT)
		       << QStringLiteral(CRY_SKIN_FILE_EXT)
		       << QStringLiteral(CRY_CHARACTER_DEFINITION_FILE_EXT)
		       << QStringLiteral(CRY_ANIM_GEOMETRY_FILE_EXT)
		       << QStringLiteral(CRY_GEOMETRY_FILE_EXT);
		return result;
	} ();

	const QFileInfo info(file);
	if (validGeometryExtensions.contains(info.suffix()))
	{
		return new QPreviewWidget(file, pParent);
	}
	else if (info.suffix() == CRY_MATERIAL_FILE_EXT)
	{
		return new QMaterialPreview(file, pParent);
	}
	else if (info.suffix() == "dds")
	{
		return new QTexturePreview(file, pParent);
	}

	return nullptr;
}

void CEditorImpl::PostOnMainThread(std::function<void()> task)
{
	if (task)
	{
		CMainThreadWorker::GetInstance().AddTask(std::move(task));
	}
}

IDataBaseManager* CEditorImpl::GetDBItemManager(EDataBaseItemType itemType)
{
	switch (itemType)
	{
	case EDB_TYPE_MATERIAL:
		return m_pMaterialManager;
	case EDB_TYPE_ENTITY_ARCHETYPE:
		return m_pEntityManager;
	case EDB_TYPE_GAMETOKEN:
		return m_pGameTokenManager;
	case EDB_TYPE_PARTICLE:
		return m_particleManager;
	}
	return 0;
}

CBaseLibraryDialog* CEditorImpl::OpenDataBaseLibrary(EDataBaseItemType type, IDataBaseItem* pItem)
{
	if (pItem)
		type = pItem->GetType();

	CWnd* pWnd = NULL;
	if (type == EDB_TYPE_MATERIAL)
	{
		pWnd = OpenView("Material Editor Legacy");
	}
	else
	{
		pWnd = OpenView("DataBase View");
	}

	IDataBaseManager* pManager = GetDBItemManager(type);
	if (pManager)
		pManager->SetSelectedItem(pItem);

	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CBaseLibraryDialog)))
	{
		return (CBaseLibraryDialog*)pWnd;
	}
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CDataBaseDialog)))
	{
		CDataBaseDialog* dlgDB = (CDataBaseDialog*)pWnd;
		CDataBaseDialogPage* pPage = dlgDB->SelectDialog(type, pItem);
		if (pPage && pPage->IsKindOf(RUNTIME_CLASS(CBaseLibraryDialog)))
		{
			return (CBaseLibraryDialog*)pPage;
		}
	}
	return 0;
}

bool CEditorImpl::SelectColor(COLORREF& color, CWnd* parent)
{
	COLORREF col = color;
	CCustomColorDialog dlg(col, CC_FULLOPEN, parent);
	if (dlg.DoModal() == IDOK)
	{
		color = dlg.GetColor();
		return true;
	}
	return false;
}

void CEditorImpl::SetInGameMode(bool inGame)
{
	static bool bWasInSimulationMode(false);

	if (inGame)
	{
		bWasInSimulationMode = GetIEditorImpl()->GetGameEngine()->GetSimulationMode();
		GetIEditorImpl()->GetGameEngine()->SetSimulationMode(false);
		GetIEditorImpl()->GetCommandManager()->Execute("game.enter");
	}
	else
	{
		GetIEditorImpl()->GetCommandManager()->Execute("game.exit");
		GetIEditorImpl()->GetGameEngine()->SetSimulationMode(bWasInSimulationMode);
	}
}

bool CEditorImpl::IsInGameMode()
{
	if (m_pGameEngine)
		return m_pGameEngine->IsInGameMode();
	return false;
}

bool CEditorImpl::IsInTestMode()
{
	return CCryEditApp::GetInstance()->IsInTestMode();
}

bool CEditorImpl::IsInConsolewMode()
{
	return CCryEditApp::GetInstance()->IsInConsoleMode();
}

bool CEditorImpl::IsInLevelLoadTestMode()
{
	return CCryEditApp::GetInstance()->IsInLevelLoadTestMode();
}

bool CEditorImpl::IsInPreviewMode()
{
	return CCryEditApp::GetInstance()->IsInPreviewMode();
}

void CEditorImpl::EnableAcceleratos(bool bEnable)
{
	m_pCommandManager->SetEditorUIActionsEnabled(bEnable);
}

void CEditorImpl::DetectVersion()
{
	char exe[_MAX_PATH];
	DWORD dwHandle;
	UINT len;

	char ver[1024 * 8];

	GetModuleFileName(NULL, exe, _MAX_PATH);

	int verSize = GetFileVersionInfoSize(exe, &dwHandle);
	if (verSize > 0)
	{
		GetFileVersionInfo(exe, dwHandle, 1024 * 8, ver);
		VS_FIXEDFILEINFO* vinfo;
		VerQueryValue(ver, "\\", (void**)&vinfo, &len);

		m_fileVersion.v[0] = vinfo->dwFileVersionLS & 0xFFFF;
		m_fileVersion.v[1] = vinfo->dwFileVersionLS >> 16;
		m_fileVersion.v[2] = vinfo->dwFileVersionMS & 0xFFFF;
		m_fileVersion.v[3] = vinfo->dwFileVersionMS >> 16;

		m_productVersion.v[0] = vinfo->dwProductVersionLS & 0xFFFF;
		m_productVersion.v[1] = vinfo->dwProductVersionLS >> 16;
		m_productVersion.v[2] = vinfo->dwProductVersionMS & 0xFFFF;
		m_productVersion.v[3] = vinfo->dwProductVersionMS >> 16;
	}
}

XmlNodeRef CEditorImpl::FindTemplate(const string& templateName)
{
	return m_templateRegistry.FindTemplate(templateName);
}

void CEditorImpl::AddTemplate(const string& templateName, XmlNodeRef& tmpl)
{
	m_templateRegistry.AddTemplate(templateName, tmpl);
}

void CEditorImpl::OpenAndFocusDataBase(EDataBaseItemType type, IDataBaseItem* pItem)
{
	OpenDataBaseLibrary(type, pItem);
}

void CEditorImpl::SetConsoleVar(const char* var, const int value)
{
	ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
	if (ivar)
		ivar->Set(value);
}

void CEditorImpl::SetConsoleVar(const char* var, const float value)
{
	ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
	if (ivar)
		ivar->Set(value);
}

void CEditorImpl::SetConsoleStringVar(const char* var, const char* value)
{
	ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
	if (ivar)
		ivar->Set(value);
}

float CEditorImpl::GetConsoleVar(const char* var)
{
	ICVar* ivar = GetSystem()->GetIConsole()->GetCVar(var);
	if (ivar)
	{
		return ivar->GetFVal();
	}
	return 0;
}

IAIManager* CEditorImpl::GetAIManager()
{
	return m_pAIManager;
}

CCustomActionsEditorManager* CEditorImpl::GetCustomActionManager()
{
	return m_pCustomActionsManager;
}

void CEditorImpl::OnObjectHideMaskChanged()
{
	int hideMask = gViewportDebugPreferences.GetObjectHideMask();
	int maskChanges = m_objectHideMask ^ hideMask;

	if (maskChanges & (OBJTYPE_ENTITY | OBJTYPE_PREFAB | OBJTYPE_GROUP | OBJTYPE_BRUSH | OBJTYPE_GEOMCACHE))
		gEnv->p3DEngine->OnObjectModified(NULL, ERF_CASTSHADOWMAPS);

	GetIEditorImpl()->Notify(eNotify_OnObjectHideMaskChange);

	m_objectHideMask = hideMask;
}

void CEditorImpl::Notify(EEditorNotifyEvent event)
{
	if (m_bExiting)
		return;

	std::list<IEditorNotifyListener*>::iterator it = m_listeners.begin();
	while (it != m_listeners.end())
	{
		(*it++)->OnEditorNotifyEvent(event);
	}
}

void CEditorImpl::RegisterAllObjectModeSubTools()
{
	CAutoRegisterObjectModeSubToolHelper::RegisterAll();
}

void CEditorImpl::UnRegisterAllObjectModeSubTools()
{
	CAutoRegisterObjectModeSubToolHelper::UnregisterAll();
}

void CEditorImpl::RegisterNotifyListener(IEditorNotifyListener* listener)
{
	listener->m_bIsRegistered = true;
	stl::push_back_unique(m_listeners, listener);
}

void CEditorImpl::UnregisterNotifyListener(IEditorNotifyListener* listener)
{
	m_listeners.remove(listener);
	listener->m_bIsRegistered = false;
}

ISourceControl* CEditorImpl::GetSourceControl()
{
	if (!gEditorGeneralPreferences.enableSourceControl())
		return nullptr;

	if (m_pSourceControl)
		return m_pSourceControl;

	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_SCM_PROVIDER, classes);
	for (int i = 0; i < classes.size(); i++)
	{
		IClassDesc* pClass = classes[i];
		ISourceControl* pSCM = (ISourceControl*)pClass;
		if (pSCM)
		{
			m_pSourceControl = pSCM;
			return m_pSourceControl;
		}
	}

	return 0;
}

Cry::IProjectManager* CEditorImpl::GetProjectManager()
{
	return m_pSystem->GetIProjectManager();
}

bool CEditorImpl::IsSourceControlAvailable()
{
	if (gEditorGeneralPreferences.enableSourceControl() && GetSourceControl())
		return true;

	return false;
}

void CEditorImpl::SetMatEditMode(bool bIsMatEditMode)
{
	m_bMatEditMode = bIsMatEditMode;
}

void CEditorImpl::ShowStatusText(bool bEnable)
{
	m_bShowStatusText = bEnable;
}

FileSystem::CEnumerator* CEditorImpl::GetFileSystemEnumerator()
{
	return m_pFileSystemEnumerator.get();
}

void CEditorImpl::GetMemoryUsage(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "Editor");

	if (GetDocument())
	{
		SIZER_COMPONENT_NAME(pSizer, "Document");

		GetDocument()->GetMemoryUsage(pSizer);
	}

	if (m_pVegetationMap)
		m_pVegetationMap->GetMemoryUsage(pSizer);
}

void CEditorImpl::ReduceMemory()
{
	GetIEditorImpl()->GetIUndoManager()->ClearRedoStack();
	GetIEditorImpl()->GetIUndoManager()->ClearUndoStack();
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_FREE_GAME_DATA);

	HANDLE hHeap = GetProcessHeap();

	if (hHeap)
	{
		uint64 maxsize = (uint64)HeapCompact(hHeap, 0);
		CryLogAlways("Max Free Memory Block = %I64d Kb", maxsize / 1024);
	}
}

IExportManager* CEditorImpl::GetExportManager()
{
	if (!m_pExportManager)
		m_pExportManager = new CExportManager();

	return m_pExportManager;
}

void CEditorImpl::AddUIEnums()
{
	// Spec settings for shadow casting lights
	string SpecString[4];
	std::vector<string> types;
	types.push_back("Never=0");
	SpecString[0].Format("VeryHigh Spec=%d", CONFIG_VERYHIGH_SPEC);
	types.push_back(SpecString[0].c_str());
	SpecString[1].Format("High Spec=%d", CONFIG_HIGH_SPEC);
	types.push_back(SpecString[1].c_str());
	SpecString[2].Format("Medium Spec=%d", CONFIG_MEDIUM_SPEC);
	types.push_back(SpecString[2].c_str());
	SpecString[3].Format("Low Spec=%d", CONFIG_LOW_SPEC);
	types.push_back(SpecString[3].c_str());
	m_pUIEnumsDatabase->SetEnumStrings("CastShadows", types);

	// Power-of-two percentages
	string percentStringPOT[5];
	types.clear();
	percentStringPOT[0].Format("Default=%d", 0);
	types.push_back(percentStringPOT[0].c_str());
	percentStringPOT[1].Format("12.5=%d", 1);
	types.push_back(percentStringPOT[1].c_str());
	percentStringPOT[2].Format("25=%d", 2);
	types.push_back(percentStringPOT[2].c_str());
	percentStringPOT[3].Format("50=%d", 3);
	types.push_back(percentStringPOT[3].c_str());
	percentStringPOT[4].Format("100=%d", 4);
	types.push_back(percentStringPOT[4].c_str());
	m_pUIEnumsDatabase->SetEnumStrings("ShadowMinResPercent", types);
}

void CEditorImpl::SetEditorConfigSpec(ESystemConfigSpec spec)
{
	editorConfigSpec = spec;
	if (m_pSystem->GetConfigSpec(true) != spec)
	{
		m_pSystem->SetConfigSpec(spec, true);
		editorConfigSpec = m_pSystem->GetConfigSpec(true);
		GetObjectManager()->SendEvent(EVENT_CONFIG_SPEC_CHANGE);
		if (m_pVegetationMap)
			m_pVegetationMap->UpdateConfigSpec();
		Notify(eNotify_EditorConfigSpecChanged);
	}
	SET_PERSONALIZATION_PROPERTY(CEditorImpl, "configSpec", editorConfigSpec);
}

ESystemConfigSpec CEditorImpl::GetEditorConfigSpec() const
{
	return editorConfigSpec;
}

void CEditorImpl::InitFinished()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (!m_bInitialized)
	{
		m_bInitialized = true;
		Notify(eNotify_OnInit);

		REGISTER_CVAR(doc_use_subfolder_for_crash_backup, 0, VF_NULL, "When saving a level after the editor has crashed, save to a sub-folder instead of overwriting the level");
		REGISTER_COMMAND("py", CmdPy, 0, "Execute a Python code snippet.");

		// Let system wide listeners know about this as well.
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_ON_INIT, 0, 0);

		if (m_pPersonalizationManager->HasProperty("CEditorImpl", "configSpec"))
			editorConfigSpec = (ESystemConfigSpec)GET_PERSONALIZATION_PROPERTY(CEditorImpl, "configSpec").toInt();
		SetEditorConfigSpec(editorConfigSpec);

		m_pNotificationCenter->Init();
		m_pPreferences->Init();
		gViewportDebugPreferences.objectHideMaskChanged.Connect(this, &CEditorImpl::OnObjectHideMaskChanged);
		m_pObjectManager->LoadClassTemplates("%EDITOR%");
	}
}

void CEditorImpl::ReloadTemplates()
{
	m_templateRegistry.LoadTemplates("%EDITOR%");
}

void CEditorImpl::CmdPy(IConsoleCmdArgs* pArgs)
{
	// Execute the given script command.
	string scriptCmd = pArgs->GetCommandLine();

	scriptCmd = scriptCmd.Right(scriptCmd.GetLength() - 2); // The part of the text after the 'py'
	scriptCmd = scriptCmd.Trim();
	PyRun_SimpleString(scriptCmd.GetString());
	PyErr_Print();
}

void CEditorImpl::OnObjectContextMenuOpened(CPopupMenuItem* pMenu, const CBaseObject* pObject)
{
	for (auto iter = m_objectContextMenuExtensions.begin(); iter != m_objectContextMenuExtensions.end(); ++iter)
	{
		(*iter)(pMenu, pObject);
	}
}

void CEditorImpl::RegisterObjectContextMenuExtension(TContextMenuExtensionFunc func)
{
	m_objectContextMenuExtensions.push_back(func);
}

void CEditorImpl::SetCurrentMissionTime(float time)
{
	if (CMission* pMission = GetIEditorImpl()->GetDocument()->GetCurrentMission())
		pMission->SetTime(time);
}

float CEditorImpl::GetCurrentMissionTime()
{
	if (CMission* pMission = GetIEditorImpl()->GetDocument()->GetCurrentMission())
		return pMission->GetTime();
	return 12.0f; // return default value noon as 12 hours
}

void CEditorImpl::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	if (event == ESYSTEM_EVENT_URI)
	{
		CCryURI uri((const char*)wparam);
		string uriSchema = uri.GetSchema();

		TURIListeners::iterator it = m_uriListeners.find(uriSchema);
		if (it != m_uriListeners.end())
		{
			for (CListenerSet<IUriEventListener*>::Notifier notifier(it->second); notifier.IsValid(); notifier.Next())
			{
				notifier->OnUriReceived(uri.GetUri());
			}
		}
	}
}

void CEditorImpl::RegisterURIListener(IUriEventListener* pListener, const char* szSchema)
{
	TURIListeners::iterator it = m_uriListeners.find(szSchema);
	if (it != m_uriListeners.end())
	{
		it->second.Add(pListener);
	}
	else
	{
		string schamaName = szSchema;
		CListenerSet<IUriEventListener*> uriListener(4);
		uriListener.Add(pListener);
		m_uriListeners.insert(std::make_pair(schamaName, uriListener));
	}
}

void CEditorImpl::UnregisterURIListener(IUriEventListener* pListener, const char* szSchema)
{
	TURIListeners::iterator it = m_uriListeners.find(szSchema);
	if (it != m_uriListeners.end())
	{
		if (it->second.Contains(pListener))
		{
			it->second.Remove(pListener);
		}
	}
}
void CEditorImpl::RequestScriptReload(const EReloadScriptsType& kind)
{
	m_reloadScriptsQueue.insert(kind);
}
void CEditorImpl::AddNativeHandler(uintptr_t id, std::function<bool(void*, long*)> callback)
{
	CEditorMainFrame::GetInstance()->m_loopHandler.AddNativeHandler(id, callback);
}

void CEditorImpl::RemoveNativeHandler(uintptr_t id)
{
	CEditorMainFrame::GetInstance()->m_loopHandler.RemoveNativeHandler(id);
}
//////////////////////////////////////////////////////////////////////////

bool CEditorImpl::IsDocumentReady() const
{
	return GetDocument()->IsDocumentReady();
}

void CEditorImpl::RemoveMemberCGroup(CBaseObject* pGroup, CBaseObject* pObject)
{
	CRY_ASSERT(IsCGroup(pGroup));

	//Take a faster code path when closing
	bool bIsClosing = m_bExiting || GetIEditor()->GetDocument()->IsClosing();

	((CGroup*)pGroup)->RemoveMember(pObject, !bIsClosing, !bIsClosing);
}

bool CEditorImpl::IsCGroup(CBaseObject* pObject)
{
	return pObject->IsKindOf(RUNTIME_CLASS(CGroup));
}

bool CEditorImpl::IsCPrefabObject(CBaseObject* pObject)
{
	return pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject));
}

void CEditorImpl::RegisterEntity(IRenderNode* pRenderNode)
{
	Get3DEngine()->RegisterEntity(pRenderNode);
}

void CEditorImpl::UnRegisterEntityAsJob(IRenderNode* pRenderNode)
{
	GetIEditorImpl()->Get3DEngine()->UnRegisterEntityAsJob(pRenderNode);
}

void CEditorImpl::SyncPrefabCPrefabObject(CBaseObject* pObject, const SObjectChangedContext& context)
{
	((CPrefabObject*)pObject)->SyncPrefab(context);
}

bool CEditorImpl::IsModifyInProgressCPrefabObject(CBaseObject* pObject)
{
	return ((CPrefabObject*)pObject)->IsModifyInProgress();
}

bool CEditorImpl::IsGroupOpen(CBaseObject* pObject)
{
	CGroup* pGroup = (CGroup*)pObject;
	return pGroup->IsOpen();
}

void CEditorImpl::OpenGroup(CBaseObject* pObject)
{
	if (!pObject || (!IsCGroup(pObject) && !IsCPrefabObject(pObject)))
		return;

	CGroup* pGroup = static_cast<CGroup*>(pObject);
	if (!pGroup->IsOpen())
		pGroup->Open();
}

void CEditorImpl::ResumeUpdateCGroup(CBaseObject* pObject)
{
	((CGroup*)pObject)->ResumeUpdate();
}

bool CEditorImpl::SuspendUpdateCGroup(CBaseObject* pObject)
{
	return ((CGroup*)pObject)->SuspendUpdate();
}

IEditorMaterial* CEditorImpl::LoadMaterial(const string& name)
{
	if (GetMaterialManager())
		return GetMaterialManager()->LoadMaterial(name);
	return nullptr;
}

void CEditorImpl::OnRequestMaterial(IMaterial* pMatInfo)
{
	if (GetMaterialManager())
		GetMaterialManager()->OnRequestMaterial(pMatInfo);
}

bool CEditorImpl::PickObject(const Vec3& vWorldRaySrc, const Vec3& vWorldRayDir, SRayHitInfo& outHitInfo, CBaseObject* pObject)
{
	CSurfaceInfoPicker picker;
	return picker.PickObject(vWorldRaySrc, vWorldRayDir, outHitInfo, pObject);
}

void CEditorImpl::AddWaitProgress(CWaitProgress* pProgress)
{
	if (CEditorMainFrame::GetInstance())
		CEditorMainFrame::GetInstance()->AddWaitProgress(pProgress);
}

void CEditorImpl::RemoveWaitProgress(CWaitProgress* pProgress)
{
	if (CEditorMainFrame::GetInstance())
		CEditorMainFrame::GetInstance()->AddWaitProgress(pProgress);
}

void CEditorImpl::RegisterDeprecatedPropertyEditor(int propertyType, std::function<bool(const string&, string&)>& handler)
{
	m_deprecatedPropertyEditors[propertyType] = handler;
}

bool CEditorImpl::EditDeprecatedProperty(int propertyType, const string& oldValue, string& newValueOut)
{
	auto handler = m_deprecatedPropertyEditors[propertyType];
	if (handler)
	{
		return handler(oldValue, newValueOut);
	}
	return false;
}

bool CEditorImpl::CanEditDeprecatedProperty(int propertyType)
{
	auto handler = m_deprecatedPropertyEditors[propertyType];
	if (handler)
		return true;
	else
		return false;
}

bool CEditorImpl::ScanDirectory(const string& path, const string& fileSpec, std::vector<string>& filesOut, bool recursive /*= true*/)
{
	CFileUtil::FileArray result;
	bool res = CFileUtil::ScanDirectory(CString(path), fileSpec, result, recursive);
	if (res && !result.empty())
	{
		for (auto& file : result)
		{
			filesOut.push_back(file.filename.GetBuffer());
		}
	}
	return res;
}

void CEditorImpl::SetPlayerViewMatrix(const Matrix34& tm, bool bEyePos /*= true*/)
{
	if (GetGameEngine())
		GetGameEngine()->SetPlayerViewMatrix(tm, bEyePos);
}
