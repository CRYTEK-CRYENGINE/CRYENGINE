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
	void OnScriptCompileScript();
	void OnScriptEditScript();
	void OnUndo();
	void OnEditDuplicate();
	void OnFileEditLogFile();
	void OnRedo();
	void OnResourcesReduceworkingset();

private:
	void CreateSampleMissionObjectives();

	class CEditorImpl* m_pEditor { nullptr };
	//! True if editor is in test mode.
	//! Test mode is a special mode enabled when Editor ran with /test command line.
	//! In this mode editor starts up, but exit immediately after all initialization.
	bool   m_bTestMode { false };
	bool   m_bPrecacheShaderList { false };
	bool   m_bPrecacheShaders { false };
	bool   m_bPrecacheShadersLevels { false };
	bool   m_bMergeShaders { false };
	bool   m_bStatsShaderList { false };
	bool   m_bStatsShaders { false };
	//! In this mode editor will load specified level file, export it, and then close.
	bool   m_bExportMode { false };
	string m_exportFile;
	//! If application exiting.
	bool   m_bExiting { false };
	//! True if editor is in preview mode.
	//! In this mode only very limited functionality is available and only for fast preview of models.
	bool      m_bPreviewMode { false };
	// Only console window is created.
	bool      m_bConsoleMode { false };
	// Level load test mode
	bool      m_bLevelLoadTestMode { false };
	//! Current file in preview mode.
	char      m_sPreviewFile[_MAX_PATH];
	//! True if "/runpython" was passed as a flag.
	bool      m_bRunPythonScript { false };
	float     m_moveSpeedStep { 0.1f };

	ULONG_PTR m_gdiplusToken { 0 };
	HANDLE    m_mutexApplication;
	//! was the editor active in the previous frame ... needed to detect if the game lost focus and
	//! dispatch proper SystemEvent (needed to release input keys)
	bool m_bPrevActive { false };
	// If this flag is set, the next OnIdle() will update, even if the app is in the background, and then
	// this flag will be reset.
	bool                           m_bForceProcessIdle { false };
	// Keep the editor alive, even if no focus is set
	bool                           m_bKeepEditorActive { false };
	bool                           m_suspendUpdate { false };

	class CMannequinChangeMonitor* m_pChangeMonitor { nullptr };

public:
	void OnExportIndoors();
	void OnViewCycle2dviewport();
	void OnRuler();
	void OnFileSavelevelresources();

public:
	void        ExportLevel(bool bExportToGame, bool bExportTexture, bool bAutoExport);
	static void SubObjectModeVertex();
	static void SubObjectModeEdge();
	static void SubObjectModeFace();
	static void SubObjectModePivot();
};

