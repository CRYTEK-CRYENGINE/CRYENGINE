// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryEdit.h"

#include "LevelEditor/NewLevelDialog.h"
#include "Mannequin/MannequinChangeMonitor.h"
#include "Objects/Group.h"
#include "Objects/SelectionGroup.h"
#include "QT/QtMainFrame.h"
#include "Terrain/Heightmap.h"
#include "Terrain/TerrainManager.h"
#include "Util/BoostPythonHelpers.h"
#include "Util/EditorAutoLevelLoadTest.h"
#include "Util/Ruler.h"

#include "CryEditDoc.h"
#include "GameEngine.h"
#include "GameExporter.h"
#include "GameResourcesExporter.h"
#include "IBackgroundScheduleManager.h"
#include "IDevManager.h"
#include "IEditorImpl.h"
#include "LevelIndependentFileMan.h"
#include "LogFile.h"
#include "MainThreadWorker.h"
#include "Mission.h"
#include "ObjectCloneTool.h"
#include "PluginManager.h"
#include "ProcessInfo.h"
#include "ResourceCompilerHelpers.h"
#include "SplashScreen.h"
#include "ViewManager.h"

#include <AssetSystem/AssetManager.h>
#include <Util/IndexedFiles.h>

#include <FileUtils.h>
#include <IObjectManager.h>
#include <ModelViewport.h>
#include <Notifications/NotificationCenter.h>
#include <PathUtils.h>
#include <QFullScreenWidget.h>
#include <Util/TempFileHelper.h>

#include <CrySandbox/IEditorGame.h>
#include <CrySystem/Testing/CryTest.h>
#include <CrySystem/Testing/CryTestCommands.h>
#include <CryString/CryWinStringUtils.h>
#include <IGameRulesSystem.h>

#include <QDir>

namespace
{

string PyGetGameFolder()
{
	string gameFolder = PathUtil::Make(PathUtil::GetEnginePath(), PathUtil::GetGameFolder()).c_str();
	return gameFolder;
}

void PyOpenLevel(const char* pLevelName)
{
	CCryEditApp::GetInstance()->LoadLevel(pLevelName);
}

void PyOpenLevelNoPrompt(const char* pLevelName)
{
	CCryEditApp::GetInstance()->DiscardLevelChanges();
	PyOpenLevel(pLevelName);
}

int PyCreateLevel(const char* levelName, int resolution, float unitSize, bool bUseTerrain)
{
	CAssetType* pLevelType = GetIEditorImpl()->GetAssetManager()->FindAssetType("Level");
	CRY_ASSERT(pLevelType);
	const string levelPath = string().Format("%s.level.cryasset", levelName);

	CLevelType::SLevelCreateParams params {};
	params.resolution = resolution;
	params.unitSize = unitSize;
	params.bUseTerrain = bUseTerrain;
	return pLevelType->Create(levelPath, &params) ? CCryEditApp::ECLR_OK : CCryEditApp::ECLR_DIR_CREATION_FAILED;
}

const char* PyGetCurrentLevelName()
{
	return GetIEditorImpl()->GetGameEngine()->GetLevelName();
}

const char* PyGetCurrentLevelPath()
{
	return GetIEditorImpl()->GetGameEngine()->GetLevelPath();
}

void Command_UnloadPlugins()
{
	GetIEditorImpl()->GetPluginManager()->UnloadAllPlugins();
}

void Command_LoadPlugins()
{
	Command_UnloadPlugins();

	char path[MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(path), path);
	const string executableFilePath = PathUtil::Make(path, "EditorPlugins\\*.dll");
	GetIEditorImpl()->GetPluginManager()->LoadPlugins(executableFilePath.c_str());
}

boost::python::tuple PyGetCurrentViewPosition()
{
	Vec3 pos = GetIEditorImpl()->GetSystem()->GetViewCamera().GetPosition();
	return boost::python::make_tuple(pos.x, pos.y, pos.z);
}

boost::python::tuple PyGetCurrentViewRotation()
{
	Ang3 ang = RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(GetIEditorImpl()->GetSystem()->GetViewCamera().GetMatrix())));
	return boost::python::make_tuple(ang.x, ang.y, ang.z);
}

void PySetCurrentViewPosition(float x, float y, float z)
{
	CViewport* pRenderViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
	if (pRenderViewport)
	{
		Matrix34 tm = pRenderViewport->GetViewTM();
		tm.SetTranslation(Vec3(x, y, z));
		pRenderViewport->SetViewTM(tm);
	}
}

void PySetCurrentViewRotation(float x, float y, float z)
{
	CViewport* pRenderViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
	if (pRenderViewport)
	{
		Matrix34 tm = pRenderViewport->GetViewTM();
		tm.SetRotationXYZ(Ang3(DEG2RAD(x), DEG2RAD(y), DEG2RAD(z)), tm.GetTranslation());
		pRenderViewport->SetViewTM(tm);
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenLevel, general, open_level,
                                     "Opens a level.",
                                     "general.open_level(str levelName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenLevelNoPrompt, general, open_level_no_prompt,
                                     "Opens a level. Doesn't prompt user about saving a modified level",
                                     "general.open_level_no_prompt(str levelName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyCreateLevel, general, create_level,
                                     "Creates a level with the parameters of 'levelName', 'resolution', 'unitSize' and 'bUseTerrain'.",
                                     "general.create_level(str levelName, int resolution, float unitSize, bool useTerrain)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetGameFolder, general, get_game_folder,
                                     "Gets the path to the Game folder of current project.",
                                     "general.get_game_folder()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCurrentLevelName, general, get_current_level_name,
                                     "Gets the name of the current level.",
                                     "general.get_current_level_name()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCurrentLevelPath, general, get_current_level_path,
                                     "Gets the fully specified path of the current level.",
                                     "general.get_current_level_path()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Command_LoadPlugins, general, load_all_plugins,
                                     "Loads all available plugins.",
                                     "general.load_all_plugins()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Command_UnloadPlugins, general, unload_all_plugins,
                                     "Unloads all available plugins.",
                                     "general.unload_all_plugins()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCurrentViewPosition, general, get_current_view_position,
                                          "Returns the position of the current view as a list of 3 floats.",
                                          "general.get_current_view_position()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCurrentViewRotation, general, get_current_view_rotation,
                                          "Returns the rotation of the current view as a list of 3 floats, each of which represents x, y, z Euler angles.",
                                          "general.get_current_view_rotation()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetCurrentViewPosition, general, set_current_view_position,
                                     "Sets the position of the current view as given x, y, z coordinates.",
                                     "general.set_current_view_position(float xValue, float yValue, float zValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetCurrentViewRotation, general, set_current_view_rotation,
                                     "Sets the rotation of the current view as given x, y, z Euler angles.",
                                     "general.set_current_view_rotation(float xValue, float yValue, float zValue)");

//////////////////////////////////////////////////////////////////////////
// The one and only CCryEditApp object
//////////////////////////////////////////////////////////////////////////
CCryEditApp theApp;

enum
{
	DefaultExportSettings_ExportToPC      = true,
	DefaultExportSettings_ExportToConsole = false,
};

const UINT kDisplayMenuIndex          = 3;
const UINT kRememberLocationMenuIndex = 12;
const UINT kGotoLocationMenuIndex     = 13;

#define ERROR_LEN 256

namespace
{
CCryEditApp* s_pCCryEditApp = 0;
}
CCryEditApp* CCryEditApp::GetInstance()
{
	return s_pCCryEditApp;
}

CCryEditApp::CCryEditApp()
{
	s_pCCryEditApp = this;
	m_mutexApplication = NULL;

	cry_strcpy(m_sPreviewFile, "");

#ifdef _DEBUG
	int tmpDbgFlag;
	tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	// Clear the upper 16 bits and OR in the desired frequency
	tmpDbgFlag = (tmpDbgFlag & 0x0000FFFF) | (32768 << 16);
	//tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
	_CrtSetDbgFlag(tmpDbgFlag);

	// Check heap every
	//_CrtSetBreakAlloc(119065);
#endif
}

CCryEditApp::~CCryEditApp()
{
}

class CEditCommandLineInfo
{
public:
	int    m_paramNum;
	int    m_exportParamNum;
	bool   m_bTest;
	bool   m_bAutoLoadLevel;
	bool   m_bExport;
	bool   m_bMatEditMode;
	bool   m_bExportTexture;
	bool   m_bPrecacheShaders;
	bool   m_bPrecacheShadersLevels;
	bool   m_bPrecacheShaderList;
	bool   m_bStatsShaders;
	bool   m_bStatsShaderList;
	bool   m_bMergeShaders;
	bool   m_bExportAI;
	bool   m_bConsoleMode;
	bool   m_bDeveloperMode;
	bool   m_bMemReplay;
	bool   m_bRunPythonScript;
	string m_file;
	string m_gameCmdLine;
	string m_swCmdLine;
	string m_strFileName;

	CEditCommandLineInfo()
		: m_exportParamNum(-1)
		, m_paramNum(0)
		, m_bExport(false)
		, m_bMatEditMode(false)
		, m_bPrecacheShaders(false)
		, m_bPrecacheShadersLevels(false)
		, m_bPrecacheShaderList(false)
		, m_bStatsShaders(false)
		, m_bStatsShaderList(false)
		, m_bMergeShaders(false)
		, m_bTest(false)
		, m_bAutoLoadLevel(false)
		, m_bExportTexture(false)
		, m_bExportAI(false)
		, m_bConsoleMode(false)
		, m_bDeveloperMode(false)
		, m_bMemReplay(false)
		, m_bRunPythonScript(false)
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (!argv)
			return;

		for (int i = 0; i < argc; ++i)
		{
			if ((*argv[i] == L'-') || (*argv[i] == L'/'))
			{
				ParseParam(argv[i] + 1, true);
			}
			else if (m_strFileName.IsEmpty()) // first non - flag parameter on the command line.
			{
				m_strFileName = CryStringUtils::WStrToUTF8(argv[i]);
			}
			else
			{
				ParseParam(argv[i], false);
			}
		}

		LocalFree(argv);
	}

private:
	void ParseParam(LPWSTR lpszParam, BOOL bFlag)
	{
		if (bFlag && wcsicmp(lpszParam, L"export") == 0)
		{
			m_exportParamNum = m_paramNum;
			m_bExport = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"exportTexture") == 0)
		{
			m_exportParamNum = m_paramNum;
			m_bExportTexture = true;
			m_bExport = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"exportAI") == 0)
		{
			m_exportParamNum = m_paramNum;
			m_bExportAI = true;
			m_bExport = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"test") == 0)
		{
			m_bTest = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"auto_level_load") == 0)
		{
			m_bAutoLoadLevel = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"PrecacheShaders") == 0)
		{
			m_bPrecacheShaders = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"PrecacheShadersLevels") == 0)
		{
			m_bPrecacheShadersLevels = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"PrecacheShaderList") == 0)
		{
			m_bPrecacheShaderList = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"StatsShaders") == 0)
		{
			m_bStatsShaders = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"StatsShaderList") == 0)
		{
			m_bStatsShaderList = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"MergeShaders") == 0)
		{
			m_bMergeShaders = true;
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"VTUNE") == 0)
		{
			m_gameCmdLine += " -VTUNE";
			return;
		}
		else if (bFlag && wcsicmp(lpszParam, L"MatEdit") == 0)
		{
			m_bMatEditMode = true;
		}
		else if (bFlag && wcsicmp(lpszParam, L"BatchMode") == 0)
		{
			m_bConsoleMode = true;
		}
		else if (bFlag && wcsicmp(lpszParam, L"devmode") == 0)
		{
			m_bDeveloperMode = true;
		}
		else if (bFlag && wcsicmp(lpszParam, L"memreplay") == 0)
		{
			m_bMemReplay = true;
		}
		else if (bFlag && wcsicmp(lpszParam, L"runpython") == 0)
		{
			m_bRunPythonScript = true;
		}

		if (!bFlag)
		{
			// otherwise it thinks that command parameters included between [] is a filename and tries to load it.
			if (lpszParam[0] == L'[')
				return;

			m_file = CryStringUtils::WStrToUTF8(lpszParam);
		}
		m_paramNum++;
	}
};

//////////////////////////////////////////////////////////////////////////
// CTheApp::FirstInstance
//		FirstInstance checks for an existing instance of the application.
//		If one is found, it is activated.
//
//    This function uses a technique similar to that described in KB
//    article Q141752	to locate the previous instance of the application. .
BOOL CCryEditApp::FirstInstance(bool bForceNewInstance)
{
	CWnd* pwndFirst = CWnd::FindWindow(_T("CryEditorClass"), NULL);
	if (pwndFirst && !bForceNewInstance)
	{
		// another instance is already running - activate it
		CWnd* pwndPopup = pwndFirst->GetLastActivePopup();
		pwndFirst->SetForegroundWindow();
		if (pwndFirst->IsIconic())
			pwndFirst->ShowWindow(SW_SHOWNORMAL);
		if (pwndFirst != pwndPopup)
			pwndPopup->SetForegroundWindow();

		if (m_bPreviewMode)
		{
			// IF in preview mode send this window copy data message to load new preview file.
			COPYDATASTRUCT cd;
			ZeroStruct(cd);
			cd.dwData = 100;
			cd.cbData = strlen(m_sPreviewFile);
			cd.lpData = m_sPreviewFile;
			pwndFirst->SendMessage(WM_COPYDATA, 0, (LPARAM)&cd);
		}
		return FALSE;
	}
	else
	{
		// this is the first instance
		// Register your unique class name that you wish to use
		WNDCLASS wndcls;
		ZeroStruct(wndcls);
		wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		//wndcls.lpfnWndProc = EditorWndProc;
		wndcls.hInstance = AfxGetInstanceHandle();
		//wndcls.hIcon = LoadIcon(0,IDR_MAINFRAME); // or load a different icon.
		//wndcls.hCursor = LoadCursor(0,IDC_ARROW);
		wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wndcls.lpszMenuName = NULL;

		// Specify your own class name for using FindWindow later
		wndcls.lpszClassName = _T("CryEditorClass");

		// Register the new class and exit if it fails
		if (!AfxRegisterClass(&wndcls))
		{
			TRACE("Class Registration Failed\n");
			return FALSE;
		}

		return TRUE;
	}
}

void CCryEditApp::InitFromCommandLine(CEditCommandLineInfo& cmdInfo)
{
	//! Setup flags from command line
	if (cmdInfo.m_bPrecacheShaders || cmdInfo.m_bPrecacheShadersLevels || cmdInfo.m_bMergeShaders
	    || cmdInfo.m_bPrecacheShaderList || cmdInfo.m_bStatsShaderList || cmdInfo.m_bStatsShaders)
	{
		m_bPreviewMode = true;
		m_bConsoleMode = true;
		m_bTestMode = true;
	}

	m_bConsoleMode |= cmdInfo.m_bConsoleMode;
	m_bTestMode |= cmdInfo.m_bTest;

	m_bPrecacheShaderList = cmdInfo.m_bPrecacheShaderList;
	m_bStatsShaderList = cmdInfo.m_bStatsShaderList;
	m_bStatsShaders = cmdInfo.m_bStatsShaders;
	m_bPrecacheShaders = cmdInfo.m_bPrecacheShaders;
	m_bPrecacheShadersLevels = cmdInfo.m_bPrecacheShadersLevels;
	m_bMergeShaders = cmdInfo.m_bMergeShaders;
	m_bExportMode = cmdInfo.m_bExport;
	m_bRunPythonScript = cmdInfo.m_bRunPythonScript;

	m_pEditor->SetMatEditMode(cmdInfo.m_bMatEditMode);

	if (m_bExportMode)
	{
		m_exportFile = cmdInfo.m_file;
		m_bTestMode = true;
	}

	// Do we have a passed filename ?
	if (!cmdInfo.m_strFileName.IsEmpty())
	{
		if (!cmdInfo.m_bRunPythonScript && CModelViewport::IsPreviewableFileType(cmdInfo.m_strFileName.GetString()))
		{
			m_bPreviewMode = true;
			cry_strcpy(m_sPreviewFile, cmdInfo.m_strFileName);
		}
	}

	if (cmdInfo.m_bAutoLoadLevel)
	{
		m_bLevelLoadTestMode = true;
		gEnv->bUnattendedMode = true;
		CEditorAutoLevelLoadTest::Instance();
	}

	if (cmdInfo.m_bMemReplay)
	{
		CryGetIMemReplay()->Start();
	}
}

namespace
{

// Callback class for initialization messages from system.
struct SInitializeUIInfo : IInitializeUIInfo
{
	virtual void SetInfoText(const char* text) override
	{
		SplashScreen::SetText(text);
	}

	static SInitializeUIInfo& GetInstance()
	{
		static SInitializeUIInfo theInstance;
		return theInstance;
	}
};

} // namespace

std::unique_ptr<CGameEngine> CCryEditApp::InitGameSystem()
{
	bool bShaderCacheGen = m_bPrecacheShaderList | m_bPrecacheShaders | m_bPrecacheShadersLevels;

	std::unique_ptr<CGameEngine> pGameEngine = stl::make_unique<CGameEngine>();
	if (!pGameEngine->Init(m_bPreviewMode, m_bTestMode, bShaderCacheGen, CryStringUtils::ANSIToUTF8(GetCommandLineA()).c_str(), &SInitializeUIInfo::GetInstance()))
	{
		return nullptr;
	}

	return pGameEngine;
}

bool CCryEditApp::CheckIfAlreadyRunning()
{
	bool bForceNewInstance = false;

	if (!m_bPreviewMode)
	{
		m_mutexApplication = CreateMutex(nullptr, TRUE, "CrytekApplication");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			const QString title = "CRYENGINE Sandbox is already running";
			const QString message = "Running multiple instances of the CRYENGINE Sandbox at once is not supported.\nDo you want to open a new instance anyway?";

			if (CQuestionDialog::SWarning(title, message, QDialogButtonBox::Yes | QDialogButtonBox::No) != QDialogButtonBox::Yes)
			{
				return false;
			}

			bForceNewInstance = true;
		}
	}

	// Shader precaching may start multiple editor copies
	if (!FirstInstance(bForceNewInstance) && !m_bPrecacheShaderList)
	{
		return false;
	}

	return true;
}

void CCryEditApp::InitPlugins()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	SplashScreen::SetText("Loading Plugins...");
	// Load the plugins
	{
		Command_LoadPlugins();
	}
}

void CCryEditApp::InitLevel(CEditCommandLineInfo& cmdInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	if (m_bPreviewMode)
	{
		GetIEditorImpl()->EnableAcceleratos(false);

		// Load geometry object.
		if (!cmdInfo.m_strFileName.IsEmpty())
		{
			LoadFile(cmdInfo.m_strFileName);
		}
	}
	else if (m_bExportMode && !m_exportFile.IsEmpty())
	{
		GetIEditorImpl()->SetModifiedFlag(FALSE);
		CCryEditDoc* pDocument = LoadLevel(m_exportFile);
		if (pDocument)
		{
			GetIEditorImpl()->SetModifiedFlag(FALSE);
			if (cmdInfo.m_bExportAI)
			{
				GetIEditorImpl()->GetGameEngine()->GenerateAiAll(eExp_AI_All);
			}
			ExportLevel(cmdInfo.m_bExport, cmdInfo.m_bExportTexture, true);
			// Terminate process.
			CLogFile::WriteLine("Editor: Terminate Process after export");
		}
		exit(0);
	}
	else if (stricmp(PathUtil::GetExt(cmdInfo.m_strFileName.GetString()), CLevelType::GetFileExtensionStatic()) == 0)
	{
		CCryEditDoc* pDocument = LoadLevel(cmdInfo.m_strFileName);
		if (pDocument)
		{
			GetIEditorImpl()->SetModifiedFlag(false);
		}
	}
}

BOOL CCryEditApp::InitConsole()
{
	if (m_bPrecacheShaderList)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShaderList");
		return FALSE;
	}
	else if (m_bStatsShaderList)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_StatsShaderList");
		return FALSE;
	}
	else if (m_bStatsShaders)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_StatsShaders");
		return FALSE;
	}
	else if (m_bPrecacheShaders)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShaders");
		return FALSE;
	}
	else if (m_bPrecacheShadersLevels)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_PrecacheShadersLevels");
		return FALSE;
	}
	else if (m_bMergeShaders)
	{
		GetIEditorImpl()->GetSystem()->GetIConsole()->ExecuteString("r_MergeShaders");
		return FALSE;
	}

	// Execute editor specific config.
	gEnv->pConsole->ExecuteString("exec editor.cfg");

	return TRUE;
}

void CCryEditApp::RunInitPythonScript(CEditCommandLineInfo& cmdInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (cmdInfo.m_bRunPythonScript)
	{
		GetIEditorImpl()->ExecuteCommand("general.run_file '%s'", cmdInfo.m_strFileName);
	}
}

bool CCryEditApp::InitInstance()
{
	////////////////////////////////////////////////////////////////////////
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.
	////////////////////////////////////////////////////////////////////////
	CProcessInfo::LoadPSApi();

	InitCommonControls();    // initialize common control library

	// Init COM services
	CoInitialize(NULL);
	// Initialize RichEditCtrl.
	AfxInitRichEdit2();

	// Initialize GDI+
	Gdiplus::GdiplusStartupInput gdiplusstartupinput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusstartupinput, NULL);
	
	// Check for 32bpp
	if (::GetDeviceCaps(GetDC(NULL), BITSPIXEL) != 32)
		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr("WARNING: Your desktop is not set to 32bpp, this might result in unexpected behavior" \
		  "of the editor. Please set your desktop to 32bpp !"));

	if (!CheckIfAlreadyRunning())
	{
		return false;
	}

	if (!CResourceCompilerVersion::CheckIfValid())
	{
		return false;
	}

	auto pGameEngine = InitGameSystem();
	if (!pGameEngine)
	{
		return false;
	}

	CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() after system");

	m_pEditor = new CEditorImpl(pGameEngine.release());

	const IDevManager* pDevManager = GetIEditorImpl()->GetDevManager();
	if (!pDevManager->HasValidDefaultLicense())
	{
		return false;
	}

	CEditCommandLineInfo cmdInfo;
	InitFromCommandLine(cmdInfo);

	m_pEditor->Init();

	SplashScreen::SetVersion(m_pEditor->GetFileVersion());

	// check here again for standard dev manager which requires the console to be initialized
	if (!pDevManager->HasValidDefaultLicense())
	{
		return false;
	}

	// Create Sandbox user folder if necessary
	GetISystem()->GetIPak()->MakeDir("%USER%/Sandbox");

	InitPlugins();

	//Must be initialized after plugins
	GetIEditorImpl()->GetAssetManager()->Init();

	m_pChangeMonitor = new CMannequinChangeMonitor();

	CCryEditDoc* pDoc = new CCryEditDoc;
	GetIEditorImpl()->SetDocument(pDoc);

	if (IsInRegularEditorMode())
	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() - File Indexing");

		CIndexedFiles::Create();

		if (gEnv->pConsole->GetCVar("ed_indexfiles")->GetIVal())
		{
			Log("Started game resource files indexing...");
			CIndexedFiles::StartFileIndexing();
		}
		else
		{
			Log("Game resource files indexing is disabled.");
		}
	}

	if (!GetIEditorImpl()->IsInMatEditMode())
		m_pEditor->InitFinished();

	InitLevel(cmdInfo);

	const ICmdLineArg* pCommandArg = GetISystem()->GetICmdLine()->FindArg(eCLAT_Pre, "edCommand");
	if (nullptr != pCommandArg)
	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() - Execute Command");
		GetIEditorImpl()->ExecuteCommand(pCommandArg->GetValue());
	}

	if (!m_bConsoleMode && !m_bPreviewMode)
	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() - Update Views");
		GetIEditorImpl()->UpdateViews();
	}

	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() - Init Console");
		if (!InitConsole())
		{
			return true;
		}
	}

	{
		CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "CCryEditApp::InitInstance() - Load Python Plugins");
		RunInitPythonScript(cmdInfo);
		PyScript::LoadPythonPlugins();
	}

	return true;
}

bool CCryEditApp::PostInit()
{
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_SANDBOX_POST_INIT_DONE, 0, 0);
	return true;
}

void CCryEditApp::DiscardLevelChanges()
{
	GetIEditorImpl()->GetDocument()->SetModifiedFlag(FALSE);
}

void CCryEditApp::LoadFile(const string& fileName)
{
	//CEditCommandLineInfo cmdLine;
	//ProcessCommandLine(cmdinfo);

	//bool bBuilding = false;
	//string file = cmdLine.SpanExcluding()
	if (GetIEditorImpl()->GetViewManager()->GetViewCount() == 0)
		return;
	CViewport* vp = GetIEditorImpl()->GetViewManager()->GetView(0);
	if (vp->GetType() == ET_ViewportModel)
	{
		((CModelViewport*)vp)->LoadObject(fileName, 1);
	}

	GetIEditorImpl()->SetModifiedFlag(FALSE);
}

inline void ExtractMenuName(string& str)
{
	// eliminate &
	int pos = str.Find('&');
	if (pos >= 0)
	{
		str = str.Left(pos) + str.Right(str.GetLength() - pos - 1);
	}
	// cut the string
	for (int i = 0; i < str.GetLength(); i++)
		if (str[i] == 9)
			str = str.Left(i);
}

int CCryEditApp::ExitInstance()
{
	SAFE_DELETE(m_pChangeMonitor);

	CIndexedFiles::Destroy();

	if (GetIEditorImpl())
		GetIEditorImpl()->Notify(eNotify_OnQuit);

	m_bExiting = true;

	HEAP_CHECK

	//////////////////////////////////////////////////////////////////////////
	// Quick end for editor.
	if (gEnv && gEnv->pSystem)
		gEnv->pSystem->Quit();
	//////////////////////////////////////////////////////////////////////////

	if (m_pEditor)
	{
		delete m_pEditor;
		m_pEditor = 0;
	}

	CoUninitialize();

	// save accelerator manager configuration.
	//m_AccelManager.SaveOnExit();

	CProcessInfo::UnloadPSApi();

	Gdiplus::GdiplusShutdown(m_gdiplusToken);

	if (m_mutexApplication)
		CloseHandle(m_mutexApplication);

	return 0;
}

bool CCryEditApp::IsAnyWindowActive()
{
	return CEditorMainFrame::GetInstance()->GetToolManager()->isAnyWindowActive() ||
	       QFullScreenWidget::IsFullScreenWindowActive() ||
	       nullptr != QApplication::activeWindow() ||
	       CEditorMainFrame::GetInstance()->isActiveWindow();
}

bool CCryEditApp::IdleProcessing(bool bBackgroundUpdate)
{
	if (m_suspendUpdate)
		return false;

	////////////////////////////////////////////////////////////////////////
	// Call the update function of the engine
	////////////////////////////////////////////////////////////////////////
	if (m_bTestMode && !bBackgroundUpdate)
	{
		// Terminate process.
		CLogFile::WriteLine("Editor: Terminate Process");
		exit(0);
	}

	bool bIsAppWindow = IsAnyWindowActive();
	bool bActive = false;
	bool res = false;
	if (bIsAppWindow || m_bForceProcessIdle || m_bKeepEditorActive)
	{
		res = true;
		bActive = true;
	}
	m_bForceProcessIdle = false;

	// focus changed
	if (m_bPrevActive != bActive)
		GetIEditorImpl()->GetSystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, bActive, 0);

	// process the work schedule - regardless if the app is active or not
	GetIEditorImpl()->GetBackgroundScheduleManager()->Update();

	// if there are active schedules keep updating the application
	if (GetIEditorImpl()->GetBackgroundScheduleManager()->GetNumSchedules() > 0)
	{
		bActive = true;
	}

	// Process main thread tasks, regardless whether the app is active or not.
	if (CMainThreadWorker::GetInstance().TryExecuteNextTask())
	{
		bActive = true;
	}

	m_bPrevActive = bActive;

	if (bActive || (bBackgroundUpdate && !bIsAppWindow))
	{
		// Start profiling frame.

		if (GetIEditorImpl()->IsInGameMode())
		{
			// Update Game
			GetIEditorImpl()->GetGameEngine()->Update();
		}
		else
		{
			GetIEditorImpl()->GetSystem()->GetProfilingSystem()->StartFrame();
			if (!GetIEditorImpl()->IsInMatEditMode())
			{
				//Engine update is disabled during level load
				//This condition will not always hold true and we should aim to remove this,
				//as many things may depend on the engine systems being updated during editor time, no matter what we are doing with level load
				//This was added to temporarily prevent an assert in the entity system
				//TODO: Remove this when the engine supports update during loading better
				if (!GetIEditorImpl()->GetDocument()->IsLevelBeingLoaded())
				{
					GetIEditorImpl()->GetGameEngine()->Update();
				}

				if (m_pEditor)
				{
					m_pEditor->Update();
				}

				// synchronize all animations to ensure that their computation has finished
				GetIEditorImpl()->GetSystem()->GetIAnimationSystem()->SyncAllAnimations();
			}

			GetIEditorImpl()->Notify(eNotify_OnIdleUpdate);
			GetIEditorImpl()->GetSystem()->GetProfilingSystem()->EndFrame();
		}

	}
	else if (GetIEditorImpl()->GetSystem() && GetIEditorImpl()->GetSystem()->GetILog())
		GetIEditorImpl()->GetSystem()->GetILog()->Update(); // print messages from other threads

	return res;
}

void CCryEditApp::ExportLevel(bool bExportToGame, bool bExportTexture, bool bAutoExport)
{
	if (bExportTexture)
	{
		CGameExporter gameExporter;
		gameExporter.SetAutoExportMode(bAutoExport);
		gameExporter.Export(eExp_SurfaceTexture);
	}
	else if (bExportToGame)
	{
		CGameExporter gameExporter;
		gameExporter.SetAutoExportMode(bAutoExport);
		gameExporter.Export();
	}
}

void CCryEditApp::OnEditDuplicate()
{
	if (GetIEditorImpl()->GetObjectManager()->GetSelection()->IsEmpty())
	{
		GetIEditorImpl()->GetNotificationCenter()->ShowInfo(QObject::tr("Command failed"), QObject::tr("Cannot duplicate, there are no selected objects"));
		return;
	}

	CEditTool* tool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
	if (tool && tool->IsKindOf(RUNTIME_CLASS(CObjectCloneTool)))
	{
		((CObjectCloneTool*)tool)->Accept();
	}

	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(new CObjectCloneTool);
	GetIEditorImpl()->SetModifiedFlag();
}

void CCryEditApp::OnScriptEditScript()
{
	// Let the user choose a LUA script file to edit
	string file;
	if (CFileUtil::SelectSingleFile(EFILE_TYPE_ANY, file, "Lua scripts (*.lua)|*.lua"))
	{
		CFileUtil::EditTextFile(file);
	}
}

void CCryEditApp::OnFileEditLogFile()
{
	CFileUtil::EditTextFile(CLogFile::GetLogFileName(), 0, CFileUtil::FILE_TYPE_SCRIPT, false);
}

void CCryEditApp::OnUndo()
{
	GetIEditorImpl()->GetIUndoManager()->Undo();
}

void CCryEditApp::OnRedo()
{
	GetIEditorImpl()->GetIUndoManager()->Redo();
}

CCryEditApp::ECreateLevelResult CCryEditApp::CreateLevel(const string& levelName, int resolution, float unitSize, bool bUseTerrain)
{
	ICryPak* const pCryPak = GetIEditorImpl()->GetSystem()->GetIPak();

	LOADING_TIME_PROFILE_AUTO_SESSION("sanbox_create_new_level");
	const string currentLevel = GetIEditorImpl()->GetLevelFolder();
	if (!currentLevel.IsEmpty())
	{
		pCryPak->ClosePacks(PathUtil::Make(currentLevel, "*.*"));
	}

	const string levelFileName = PathUtil::GetFile(PathUtil::RemoveSlash(levelName));
	const string levelPath = string().Format("%s/%s", PathUtil::GetGameFolder().c_str(), levelName.c_str());
	const string filename = PathUtil::Make(levelPath, levelFileName, CLevelType::GetFileExtensionStatic());

	if (pCryPak->IsFolder(levelPath.c_str()))
	{
		// User already saw all confirmation dialogs. Deleting level folder without prompt
		const QString absLevelDirectory = QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), levelName));

		QDir dirToDelete(absLevelDirectory);
		if (!dirToDelete.removeRecursively())
		{
			return ECLR_DIR_CREATION_FAILED;
		}
	}

	CLogFile::WriteLine("Creating level directory");
	if (!pCryPak->MakeDir(levelPath.c_str()))
	{
		return ECLR_DIR_CREATION_FAILED;
	}

	// First cleanup previous document
	GetIEditorImpl()->CloseDocument();
	// Create empty document
	CCryEditDoc* pDoc = new CCryEditDoc();
	GetIEditorImpl()->SetDocument(pDoc);

	if (CViewport* pViewPort = GetIEditorImpl()->GetActiveView())
	{
		pViewPort->Update();
	}

	GetIEditorImpl()->GetGameEngine()->SetLevelPath(levelPath);
	gEnv->pGameFramework->SetEditorLevel(levelName, levelPath);
	pDoc->InitEmptyLevel(resolution, unitSize, bUseTerrain);
	CProgressNotification notification("Creating Level", QtUtil::ToQString(levelName));

	// Save the document to this folder
	pDoc->SetPathName(filename.GetBuffer());

	if (pDoc->Save())
	{
		// Create sample Objectives.xml in level data.
		CreateSampleMissionObjectives();

		CGameExporter gameExporter;
		gameExporter.Export();

		GetIEditorImpl()->GetGameEngine()->LoadLevel(levelPath, GetIEditorImpl()->GetGameEngine()->GetMissionName(), true, true);

		if (auto* pEditorGame = GetIEditorImpl()->GetGameEngine()->GetIEditorGame())
		{
			pEditorGame->OnAfterLevelLoad(GetIEditorImpl()->GetGameEngine()->GetLevelName(), GetIEditorImpl()->GetGameEngine()->GetLevelPath());
		}

		GetIEditorImpl()->GetHeightmap()->InitTerrain();

		// we need to reload environment after terrain creation in order for water to be initialized
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
	}

	pDoc->SetDocumentReady(true);

	return ECLR_OK;
}

CCryEditDoc* CCryEditApp::LoadLevel(const char* lpszFileName)//this path is a pak file path but make this more robust
{
	CCryEditDoc* pDoc = GetIEditorImpl()->GetDocument();
	if (pDoc && pDoc->IsLevelBeingLoaded())
	{
		return nullptr;
	}

	if (GetIEditorImpl()->GetLevelIndependentFileMan()->PromptChangedFiles() && GetIEditorImpl()->GetDocument()->CanClose())
	{
		CProgressNotification notification("Loading Level", QString(lpszFileName));
		// disable commands while loading. Since we process events this can trigger actions
		// that tweak incomplete object state.
		GetIEditorImpl()->EnableAcceleratos(false);

		if (pDoc)
		{
			delete pDoc;
		}
		pDoc = new CCryEditDoc;

		const string newPath = PathUtil::AbsolutePathToCryPakPath(lpszFileName);
		pDoc->OnOpenDocument(newPath);

		// Re-enable accelerators here
		GetIEditorImpl()->EnableAcceleratos(true);

		pDoc->SetLastLoadedLevelName(newPath);
	}

	return pDoc;
}

void CCryEditApp::OnResourcesReduceworkingset()
{
	SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
}

void CCryEditApp::CreateSampleMissionObjectives()
{
	CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + CMission::GetObjectivesFileName());

	XmlNodeRef root = XmlHelpers::CreateXmlNode("Root");

	XmlNodeRef levelNode = XmlHelpers::CreateXmlNode("Level");
	levelNode->setAttr("Name", GetIEditorImpl()->GetGameEngine()->GetLevelName());
	root->addChild(levelNode);

	XmlNodeRef missionNode = XmlHelpers::CreateXmlNode("Mission_01");
	missionNode->setAttr("Name", "Example Primary Mission");
	missionNode->setAttr("Description", "Description of Example Primary Mission.");
	levelNode->addChild(missionNode);

	missionNode = XmlHelpers::CreateXmlNode("Mission_01b");
	missionNode->setAttr("Name", "Example Secondary Mission");
	missionNode->setAttr("Description", "Description of Example Secondary Mission.");
	missionNode->setAttr("Secondary", "true");
	levelNode->addChild(missionNode);

	root->saveToFile(helper.GetTempFilePath());
	helper.UpdateFile(false);
}

void CCryEditApp::OnExportIndoors()
{
	//CBrushIndoor *indoor = GetIEditorImpl()->GetObjectManager()->GetCurrentIndoor();
	//CBrushExporter exp;
	//exp.Export( indoor, "C:\\MasterCD\\Objects\\Indoor.bld" );
}

void CCryEditApp::OnViewCycle2dviewport()
{
	GetIEditorImpl()->GetViewManager()->Cycle2DViewport();
}

void CCryEditApp::OnRuler()
{
	CRuler* pRuler = GetIEditorImpl()->GetRuler();
	pRuler->SetActive(!pRuler->IsActive());
}

void CCryEditApp::OnFileSavelevelresources()
{
	CGameResourcesExporter saver;
	saver.GatherAllLoadedResources();
	saver.ChooseDirectoryAndSave();
}

bool CCryEditApp::IsInRegularEditorMode()
{
	return !IsInTestMode() && !IsInPreviewMode()
	       && !IsInExportMode() && !IsInConsoleMode() && !IsInLevelLoadTestMode();
}

namespace
{
bool g_runScriptResult = false;     // true -> success, false -> failure

void PySetResultToSuccess()
{
	g_runScriptResult = true;
}

void PySetResultToFailure()
{
	g_runScriptResult = false;
}

void PyIdleWait(double timeInSec)
{
	clock_t start = clock();
	do
	{
		MSG msg;
		while (FALSE != ::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}
	while ((double)(clock() - start) / CLOCKS_PER_SEC < timeInSec);
}
}

void CCryEditApp::SubObjectModeVertex()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (pSelection->GetCount() != 1)
		return;

	CBaseObject* pSelObject = pSelection->GetObject(0);

	if (pSelObject->GetType() == OBJTYPE_BRUSH)
	{
		GetIEditorImpl()->ExecuteCommand("edit_mode.select_vertex");
	}
	else if (pSelObject->GetType() == OBJTYPE_SOLID)
	{
		PyScript::Execute("designer.select_vertexmode()");
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
	}
}

void CCryEditApp::SubObjectModeEdge()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (pSelection->GetCount() != 1)
		return;

	CBaseObject* pSelObject = pSelection->GetObject(0);

	if (pSelObject->GetType() == OBJTYPE_BRUSH)
	{
		GetIEditorImpl()->ExecuteCommand("edit_mode.select_edge");
	}
	else if (pSelObject->GetType() == OBJTYPE_SOLID)
	{
		PyScript::Execute("designer.select_edgemode()");
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
	}
}

void CCryEditApp::SubObjectModeFace()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (pSelection->GetCount() != 1)
		return;

	CBaseObject* pSelObject = pSelection->GetObject(0);

	if (pSelObject->GetType() == OBJTYPE_BRUSH)
	{
		GetIEditorImpl()->ExecuteCommand("edit_mode.select_face");
	}
	else if (pSelObject->GetType() == OBJTYPE_SOLID)
	{
		PyScript::Execute("designer.select_facemode()");
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
	}
}

void CCryEditApp::SubObjectModePivot()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (pSelection->GetCount() != 1)
		return;

	CBaseObject* pSelObject = pSelection->GetObject(0);

	if (pSelObject->GetType() == OBJTYPE_SOLID)
	{
		PyScript::Execute("designer.select_pivotmode()");
	}
	else
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("The current selected object(s) do(es)n't support the mesh editing"));
	}
}

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetResultToSuccess, general, set_result_to_success,
                                          "Sets the result of a script execution to success. Used only for Sandbox AutoTests.",
                                          "general.set_result_to_success()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetResultToFailure, general, set_result_to_failure,
                                          "Sets the result of a script execution to failure. Used only for Sandbox AutoTests.",
                                          "general.set_result_to_failure()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyIdleWait, general, idle_wait,
                                          "Waits idling for a given seconds. Primarily used for auto-testing.",
                                          "general.idle_wait(double time)");

CRY_TEST(EditorCreateLevelTest, editor = true, game = false, timeout = 60.f)
{
	auto KillLevel = []
	{
		CryPathString fullPath;
		gEnv->pCryPak->AdjustFileName("examplelevel", fullPath, ICryPak::FOPEN_ONDISK);
		FileUtils::RemoveDirectory(fullPath);

		gEnv->pCryPak->RemoveFile("examplelevel.level.cryasset");
	};

	commands =
	{
		KillLevel,
		[] {
			GetIEditor()->ExecuteCommand("general.create_level 'examplelevel' '1024' '1.0f' 'true'");
		},
		CryTest::CCommandWait(3.0f),
		[] {
			CHeightmap* heightMap = GetIEditorImpl()->GetTerrainManager()->GetHeightmap();
			CRY_TEST_ASSERT(heightMap);
			CRY_TEST_ASSERT(heightMap->GetHeight() == 1024);
			CRY_TEST_ASSERT(heightMap->GetWidth() == 1024);
			CRY_TEST_ASSERT(heightMap->GetUnitSize() == 1.0f);
		},
	};
}
