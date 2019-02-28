// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "MaterialSender.h"

class CMatEditMainDlg;
class CCryEditDoc;
class CEditCommandLineInfo;
class SplashScreen;
struct mg_connection;
struct mg_request_info;
struct mg_context;
struct SGameExporterSettings;

class SANDBOX_API CCryEditApp
{
public:
	enum ECreateLevelResult
	{
		ECLR_OK = 0,
		ECLR_ALREADY_EXISTS,
		ECLR_DIR_CREATION_FAILED
	};

	CCryEditApp();
	~CCryEditApp();

	static CCryEditApp*          GetInstance();

	void                         LoadFile(const string& fileName);
	void                         ForceNextIdleProcessing()      { m_bForceProcessIdle = true; }
	void                         KeepEditorActive(bool bActive) { m_bKeepEditorActive = bActive; };
	bool                         IsInTestMode()                 { return m_bTestMode; };
	bool                         IsInPreviewMode()              { return m_bPreviewMode; };
	bool                         IsInExportMode()               { return m_bExportMode; };
	bool                         IsInConsoleMode()              { return m_bConsoleMode; };
	bool                         IsInLevelLoadTestMode()        { return m_bLevelLoadTestMode; }
	bool                         IsInRegularEditorMode();
	bool                         IsExiting() const              { return m_bExiting; }

	ECreateLevelResult           CreateLevel(const string& levelName, int resolution, float unitSize, bool bUseTerrain);
	BOOL                         FirstInstance(bool bForceNewInstance = false);
	void                         InitFromCommandLine(CEditCommandLineInfo& cmdInfo);
	bool                         CheckIfAlreadyRunning();
	std::unique_ptr<CGameEngine> InitGameSystem();
	void                         InitPlugins();
	void                         InitLevel(CEditCommandLineInfo& cmdInfo);
	BOOL                         InitConsole();
	bool                         IdleProcessing(bool bBackground);
	bool                         IsAnyWindowActive();
	void                         RunInitPythonScript(CEditCommandLineInfo& cmdInfo);

	void                         DiscardLevelChanges();
	CCryEditDoc*                 LoadLevel(const char* lpszFileName);

	// don't use these suspending functions - they are used to hack freezing msgboxes during a crash, it will be removed
	void SuspendUpdate()           { m_suspendUpdate = true; }
	void ResumeUpdate()            { m_suspendUpdate = false; }
	bool IsUpdateSuspended() const { return m_suspendUpdate; }

public:
	bool InitInstance();
	bool PostInit();
	int  ExitInstance();

	// Implementation
	void SaveTagLocations();
	void OnExportSelectedObjects();
	void OnExportToEngine();
	void OnViewSwitchToGame();
	void OnEditSelectNone();
	void OnScriptCompileScript();
	void OnScriptEditScript();
	void OnEditmodeMove();
	void OnEditmodeRotate();
	void OnEditmodeScale();
	void OnEditToolLink();
	void OnEditToolUnlink();
	void OnEditmodeSelect();
	void OnUndo();
	void OnEditDuplicate();
	void OnSelectionSave();
	void OnSelectionLoad();
	void OnGotoSelected();
	void OnAlignObject();
	void OnAlignToGrid();
	void OnGroupAttach();
	void OnGroupClose();
	// Detaches selection from group, the object will be placed relative to the group's
	// parent (or layer if the group doesn't have a parent)
	void OnGroupDetach();
	// Detaches selection from group, the object will placed directly on it's owning layer
	void OnGroupDetachToRoot();
	void OnGroupMake();
	void OnGroupOpen();
	void OnGroupUngroup();
	void OnLockSelection();
	void OnFileEditLogFile();
	void OnReloadTextures();
	void OnReloadArchetypes();
	void OnReloadAllScripts();
	void OnReloadEntityScripts();
	void OnReloadItemScripts();
	void OnReloadAIScripts();
	void OnReloadUIScripts();
	void OnReloadGeometry();
	void OnRedo();
	void OnSwitchPhysics();
	void OnSyncPlayer();
	void OnResourcesReduceworkingset();
	void OnToolsUpdateProcVegetation();
	void ToggleHelpersDisplay();
	void OnCycleDisplayInfo();

private:
	void CreateSampleMissionObjectives();
	void TagLocation(int index);
	void GotoTagLocation(int index);
	void LoadTagLocations();
	bool UserExportToGame(bool bNoMsgBox = true);
	bool DoExportToGame(uint32 flags, const SGameExporterSettings& exportSettings, bool showMsgBox = false);

	class CEditorImpl* m_pEditor;
	//! True if editor is in test mode.
	//! Test mode is a special mode enabled when Editor ran with /test command line.
	//! In this mode editor starts up, but exit immediately after all initialization.
	bool   m_bTestMode;
	bool   m_bPrecacheShaderList;
	bool   m_bPrecacheShaders;
	bool   m_bPrecacheShadersLevels;
	bool   m_bMergeShaders;
	bool   m_bStatsShaderList;
	bool   m_bStatsShaders;
	//! In this mode editor will load specified level file, export it, and then close.
	bool   m_bExportMode;
	string m_exportFile;
	//! If application exiting.
	bool   m_bExiting;
	//! True if editor is in preview mode.
	//! In this mode only very limited functionality is available and only for fast preview of models.
	bool      m_bPreviewMode;
	// Only console window is created.
	bool      m_bConsoleMode;
	// Level load test mode
	bool      m_bLevelLoadTestMode;
	//! Current file in preview mode.
	char      m_sPreviewFile[_MAX_PATH];
	//! True if "/runpython" was passed as a flag.
	bool      m_bRunPythonScript;
	//! In this mode, editor will load world segments and process command for each batch
	Vec3      m_tagLocations[12];
	Ang3      m_tagAngles[12];
	Vec2i     m_tagSegmentsXY[12];
	float     m_fastRotateAngle;
	float     m_moveSpeedStep;

	ULONG_PTR m_gdiplusToken;
	HANDLE    m_mutexApplication;
	//! was the editor active in the previous frame ... needed to detect if the game lost focus and
	//! dispatch proper SystemEvent (needed to release input keys)
	bool m_bPrevActive;
	// If this flag is set, the next OnIdle() will update, even if the app is in the background, and then
	// this flag will be reset.
	bool                           m_bForceProcessIdle;
	// Keep the editor alive, even if no focus is set
	bool                           m_bKeepEditorActive;
	bool                           m_suspendUpdate;
	string                         m_lastOpenLevelPath;

	int                            m_initSegmentsToOpen;

	class CMannequinChangeMonitor* m_pChangeMonitor;

public:
	void OnEditHide();
	void OnEditUnhideall();
	void OnEditFreeze();
	void OnEditUnfreezeall();
	void OnWireframe();
	void OnPointmode();

	// Tag Locations.
	void OnTagLocation1();
	void OnTagLocation2();
	void OnTagLocation3();
	void OnTagLocation4();
	void OnTagLocation5();
	void OnTagLocation6();
	void OnTagLocation7();
	void OnTagLocation8();
	void OnTagLocation9();
	void OnTagLocation10();
	void OnTagLocation11();
	void OnTagLocation12();

	void OnGotoLocation1();
	void OnGotoLocation2();
	void OnGotoLocation3();
	void OnGotoLocation4();
	void OnGotoLocation5();
	void OnGotoLocation6();
	void OnGotoLocation7();
	void OnGotoLocation8();
	void OnGotoLocation9();
	void OnGotoLocation10();
	void OnGotoLocation11();
	void OnGotoLocation12();
	void OnToolsLogMemoryUsage();
	void OnExportIndoors();
	void OnViewCycle2dviewport();
	void OnDisplayGotoPosition();
	void OnRuler();
	void OnRotateselectionXaxis();
	void OnRotateselectionYaxis();
	void OnRotateselectionZaxis();
	void OnRotateselectionRotateangle();
	void OnMaterialAssigncurrent();
	void OnMaterialResettodefault();
	void OnMaterialGetmaterial();
	void OnPhysicsGetState();
	void OnPhysicsResetState();
	void OnPhysicsSimulateObjects();
	void OnFileSavelevelresources();
	void OnValidateLevel();
	void OnValidateObjectPositions();
	void OnEditInvertselection();
	void OnMaterialPicktool();
	void OnResolveMissingObjects();
	void OnPhysicsGenerateJoints();
	void OnSwitchToDefaultCamera();
	void OnSwitchToSequenceCamera();
	void OnSwitchToSelectedcamera();
	void OnSwitchcameraNext();

public:
	void        ExportLevel(bool bExportToGame, bool bExportTexture, bool bAutoExport);
	static bool Command_ExportToEngine();
	static void SubObjectModeVertex();
	static void SubObjectModeEdge();
	static void SubObjectModeFace();
	static void SubObjectModePivot();

	void        OnFileExportOcclusionMesh();
};

