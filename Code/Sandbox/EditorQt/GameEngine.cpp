// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameEngine.h"
#include "IEditorImpl.h"
#include "CryEditDoc.h"
#include "Objects/EntityScript.h"
#include "Objects/AIWave.h"
#include "Geometry/EdMesh.h"
#include "Mission.h"
#include "Terrain/SurfaceType.h"
#include "Terrain/Heightmap.h"
#include "Terrain/TerrainGrid.h"
#include "TerrainLighting.h"
#include "ViewManager.h"
#include "SplashScreen.h"
#include "AI/AIManager.h"
#include "AI/CoverSurfaceManager.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/AICoverSurface.h"
#include "AI/NavDataGeneration/Navigation.h"
#include "Material/MaterialManager.h"
#include "Particles/ParticleManager.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraphModuleManager.h"
#include "HyperGraph/Controls/FlowGraphDebuggerEditor.h"
#include "UIEnumsDatabase.h"
#include "Util/Ruler.h"
#include "CustomActions/CustomActionsEditorManager.h"
#include "Material/MaterialFXGraphMan.h"
#include <CryAISystem/IAgent.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryMovie/IMovieSystem.h>
#include <CryInput/IInput.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/File/ICryPak.h>
#include <CryPhysics/IPhysics.h>
#include <CrySandbox/IEditorGame.h>
#include <CrySystem/ITimer.h>
#include <CryFlowGraph/IFlowSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameFramework.h>
#include <CryInput/IHardwareMouse.h>
#include <CryAudio/Dialog/IDialogSystem.h>
#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <CryPhysics/IDeferredCollisionEvent.h>
#include <CryCore/Platform/platform_impl.inl>
#include "Prefabs/PrefabManager.h"
#include "Prefabs/PrefabEvents.h"
#include "UI\UIManager.h"
#include <Cry3DEngine/ITimeOfDay.h>
#include "QT/Widgets/QWaitProgress.h"
#include "Controls/QuestionDialog.h"
#include "Notifications/NotificationCenter.h"
#include "ICommandManager.h"
#include "Preferences/ViewportPreferences.h"
#include "GameExporter.h"
#include <RenderViewport.h>
#include <CrySystem/ICryPluginManager.h>

// added just because of the suspending/resuming of engine update, should be removed once we have msgboxes in a separate process
#include "CryEdit.h"

#include <../CryAction/IGameRulesSystem.h>
#include <../CryAction/ILevelSystem.h>

// Implementation of System Callback structure.
struct SSystemUserCallback : public ISystemUserCallback
{
	SSystemUserCallback(IInitializeUIInfo* logo) { m_pLogo = logo; };
	virtual void OnSystemConnect(ISystem* pSystem)
	{
		ModuleInitISystem(pSystem, "Editor");
	}

	virtual bool OnSaveDocument()
	{
		return GetIEditorImpl()->SaveDocumentBackup();
	}

	virtual void OnProcessSwitch()
	{
		if (GetIEditorImpl()->IsInGameMode())
			GetIEditorImpl()->SetInGameMode(false);
	}

	virtual void OnInitProgress(const char* sProgressMsg)
	{
		if (m_pLogo)
			m_pLogo->SetInfoText(sProgressMsg);
	}

	struct SEditorSuspendUpdateScopeHelper
	{
		SEditorSuspendUpdateScopeHelper()
		{
			CCryEditApp::GetInstance()->SuspendUpdate();
		}

		~SEditorSuspendUpdateScopeHelper()
		{
			CCryEditApp::GetInstance()->ResumeUpdate();
		}
	};

	virtual EQuestionResult ShowMessage(const char* text, const char* caption, EMessageBox uType)
	{
		SEditorSuspendUpdateScopeHelper suspendUpdateScope;

		switch (uType)
		{
		case eMB_Error:
			CQuestionDialog::SCritical(caption, text);
			return eQR_None;

		case eMB_YesCancel:
			if (QDialogButtonBox::Yes == CQuestionDialog::SQuestion(caption, text))
				return eQR_Yes;
			return eQR_Cancel;

		case eMB_YesNoCancel:
			QDialogButtonBox::StandardButton res = CQuestionDialog::SQuestion(caption, text, QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
			if (QDialogButtonBox::Yes == res)
				return eQR_Yes;
			return QDialogButtonBox::No == res ? eQR_No : eQR_Cancel;
		}

		CQuestionDialog::SWarning(caption, text);

		return eQR_None;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer)
	{
		GetIEditorImpl()->GetMemoryUsage(pSizer);
	}

private:
	IInitializeUIInfo* m_pLogo;
};

// Implements EntitySystemSink for InGame mode.
struct SInGameEntitySystemListener : public IEntitySystemSink
{
	SInGameEntitySystemListener()
	{
	}

	~SInGameEntitySystemListener()
	{
		// Remove all remaining entities from entity system.
		IEntitySystem* pEntitySystem = GetIEditorImpl()->GetSystem()->GetIEntitySystem();

		for (std::set<int>::iterator it = m_entities.begin(); it != m_entities.end(); ++it)
		{
			EntityId entityId = *it;
			IEntity* pEntity = pEntitySystem->GetEntity(entityId);
			if (pEntity)
			{
				IEntity* pParent = pEntity->GetParent();
				if (pParent)
				{
					// Childs of irremovable entity are also not deleted (Needed for vehicles weapons for example)
					if (pParent->GetFlags() & ENTITY_FLAG_UNREMOVABLE)
						continue;
				}
			}
			pEntitySystem->RemoveEntity(*it, true);
		}
	}

	virtual bool OnBeforeSpawn(SEntitySpawnParams& params)
	{
		return true;
	}

	virtual void OnSpawn(IEntity* e, SEntitySpawnParams& params)
	{
		//if (params.ed.ClassId!=0 && ed.ClassId!=PLAYER_CLASS_ID) // Ignore MainPlayer
		if (!(e->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
		{
			m_entities.insert(e->GetId());
		}
	}

	virtual bool OnRemove(IEntity* e)
	{
		if (!(e->GetFlags() & ENTITY_FLAG_UNREMOVABLE))
			m_entities.erase(e->GetId());
		return true;
	}

	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params)
	{
		CRY_ASSERT_MESSAGE(false, "Editor should not be receiving entity reused events from IEntitySystemSink, investigate this.");
	}

private:
	// Ids of all spawned entities.
	std::set<int> m_entities;
};

namespace
{
SInGameEntitySystemListener* s_InGameEntityListener = 0;
};

// Timur.
// This is FarCry.exe authentication function, this code is not for public release!!
static void GameSystemAuthCheckFunction(void* data)
{
	// src and trg can be the same pointer (in place encryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit:  int key[4] = {n1,n2,n3,n4};
	// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE(src, trg, len, key) {                                                                                      \
  unsigned int* v = (src), * w = (trg), * k = (key), nlen = (len) >> 3;                                                       \
  unsigned int delta = 0x9E3779B9, a = k[0], b = k[1], c = k[2], d = k[3];                                                    \
  while (nlen--) {                                                                                                            \
    unsigned int y = v[0], z = v[1], n = 32, sum = 0;                                                                         \
    while (n-- > 0) { sum += delta; y += (z << 4) + a ^ z + sum ^ (z >> 5) + b; z += (y << 4) + c ^ y + sum ^ (y >> 5) + d; } \
    w[0] = y; w[1] = z; v += 2, w += 2; }                                                                                     \
}

	// src and trg can be the same pointer (in place decryption)
	// len must be in bytes and must be multiple of 8 byts (64bits).
	// key is 128bit: int key[4] = {n1,n2,n3,n4};
	// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE(src, trg, len, key) {                                                                                      \
  unsigned int* v = (src), * w = (trg), * k = (key), nlen = (len) >> 3;                                                       \
  unsigned int delta = 0x9E3779B9, a = k[0], b = k[1], c = k[2], d = k[3];                                                    \
  while (nlen--) {                                                                                                            \
    unsigned int y = v[0], z = v[1], sum = 0xC6EF3720, n = 32;                                                                \
    while (n-- > 0) { z -= (y << 4) + c ^ y + sum ^ (y >> 5) + d; y -= (z << 4) + a ^ z + sum ^ (z >> 5) + b; sum -= delta; } \
    w[0] = y; w[1] = z; v += 2, w += 2; }                                                                                     \
}

	// Data assumed to be 32 bytes.
	int key1[4] = { 1178362782, 223786232, 371615531, 90884141 };
	TEA_DECODE((unsigned int*)data, (unsigned int*)data, 32, (unsigned int*)key1);
	int key2[4] = { 89158165, 1389745433, 971685123, 785741042 };
	TEA_ENCODE((unsigned int*)data, (unsigned int*)data, 32, (unsigned int*)key2);
}

static void KeepEditorActiveChanged(ICVar* pKeepEditorActive)
{
	const int cvarKeepEditorActive = pKeepEditorActive->GetIVal();
	CCryEditApp::GetInstance()->KeepEditorActive(cvarKeepEditorActive);
}

// This class implements calls from the game to the editor
class CGameToEditorInterface : public IGameToEditorInterface
{
	virtual void SetUIEnums(const char* sEnumName, const char** sStringsArray, int nStringCount)
	{
		std::vector<string> sStringsList;

		for (int i = 0; i < nStringCount; ++i)
		{
			stl::push_back_unique(sStringsList, sStringsArray[i]);
		}

		GetIEditorImpl()->GetUIEnumsDatabase()->SetEnumStrings(sEnumName, sStringsList);
	}
};

CGameEngine::CGameEngine()
	: m_bIgnoreUpdates(false)
	, m_pEditorGame(0)
	, m_ePendingGameMode(ePGM_NotPending)
	, m_listeners(0)
{
	m_pISystem = nullptr;
	m_pNavigation = nullptr;
	m_bLevelLoaded = false;
	m_bInGameMode = false;
	m_bGameModeSuspended = false;
	m_bSimulationMode = false;
	m_bSimulationModeAI = false;
	m_bSyncPlayerPosition = true;
	m_bUpdateFlowSystem = false;
	m_bJustCreated = false;
	m_pGameToEditorInterface = new CGameToEditorInterface;
	m_levelName = "Untitled";
	m_playerViewTM.SetIdentity();
}

CGameEngine::~CGameEngine()
{
	gViewportSelectionPreferences.signalSettingsChanged.DisconnectObject(this);

	if (CEditorImpl* pEditorImpl = GetIEditorImpl())
	{
		pEditorImpl->UnregisterNotifyListener(this);
	}

	if (m_pISystem != nullptr)
	{
		m_pISystem->GetIMovieSystem()->SetCallback(NULL);
	}

	CEntityScriptRegistry::Release();
	CEdMesh::ReleaseAll();

	delete m_pNavigation;

	if (m_pEditorGame)
	{
		m_pEditorGame->Shutdown();
	}

	m_pISystem = nullptr;

	/*
	   if (m_hSystemHandle)
	    FreeLibrary(m_hSystemHandle);
	 */
	delete m_pGameToEditorInterface;
	delete m_pSystemUserCallback;
}

static int ed_killmemory_size;
static int ed_indexfiles;

void KillMemory(IConsoleCmdArgs* /* pArgs */)
{
	while (true)
	{
		const int kLimit = 10000000;
		int size;

		if (ed_killmemory_size > 0)
		{
			size = ed_killmemory_size;
		}
		else
		{
			size = rand() * rand();
			size = size > kLimit ? kLimit : size;
		}

		uint8* alloc = new uint8[size];
	}
}

void DisableGameMode(IConsoleCmdArgs* /* pArgs */)
{
	GetIEditorImpl()->GetGameEngine()->SetGameMode(false);
}

static void CmdGotoEditor(IConsoleCmdArgs* pArgs)
{
	// feature is mostly useful for QA purposes, this works with the game "goto" command
	// this console command actually is used by the game command, the editor command shouldn't be used by the user
	int iArgCount = pArgs->GetArgCount();

	CViewManager* pViewManager = GetIEditorImpl()->GetViewManager();
	CViewport* pRenderViewport = pViewManager->GetGameViewport();
	if (!pRenderViewport) return;

	float x, y, z, wx, wy, wz;

	if (iArgCount == 7
	    && sscanf(pArgs->GetArg(1), "%f", &x) == 1
	    && sscanf(pArgs->GetArg(2), "%f", &y) == 1
	    && sscanf(pArgs->GetArg(3), "%f", &z) == 1
	    && sscanf(pArgs->GetArg(4), "%f", &wx) == 1
	    && sscanf(pArgs->GetArg(5), "%f", &wy) == 1
	    && sscanf(pArgs->GetArg(6), "%f", &wz) == 1)
	{
		Matrix34 tm = pRenderViewport->GetViewTM();

		tm.SetTranslation(Vec3(x, y, z));
		tm.SetRotation33(Matrix33::CreateRotationXYZ(DEG2RAD(Ang3(wx, wy, wz))));
		pRenderViewport->SetViewTM(tm);
	}
}

bool CGameEngine::Init(
  bool bPreviewMode,
  bool bTestMode,
  bool bShaderCacheGen,
  const char* sInCmdLine,
  IInitializeUIInfo* logo)
{
	m_pSystemUserCallback = new SSystemUserCallback(logo);

	SSystemInitParams startupParams;

	startupParams.bEditor = true;
	startupParams.bPreview = bPreviewMode;

	if (AfxGetMainWnd())
	{
		startupParams.hWnd = AfxGetMainWnd()->GetSafeHwnd();
	}

	startupParams.sLogFileName = "Editor.log";
	startupParams.pUserCallback = m_pSystemUserCallback;

	if (sInCmdLine)
	{
		cry_strcpy(startupParams.szSystemCmdLine, sInCmdLine);
		if (strstr(sInCmdLine, "-export"))
			startupParams.bUnattendedMode = true;
	}

	startupParams.pCheckFunc = &GameSystemAuthCheckFunction;

	if (bShaderCacheGen)
		startupParams.bSkipFont = true;

	// We do this manually in the Editor
	startupParams.bExecuteCommandLine = false;

	if (!CryInitializeEngine(startupParams, true))
	{
		return false;
	}

	m_pISystem = startupParams.pSystem;

	m_pNavigation = new CNavigation(gEnv->pSystem);
	m_pNavigation->Init();

	if (gEnv
	    && gEnv->p3DEngine
	    && gEnv->p3DEngine->GetTimeOfDay())
	{
		gEnv->p3DEngine->GetTimeOfDay()->BeginEditMode();
	}

	if (gEnv && gEnv->pMovieSystem)
	{
		gEnv->pMovieSystem->EnablePhysicsEvents(m_bSimulationMode);
	}

	auto highlightUpdateFunctor = [ = ]
	{
		ColorF highlightColor(gViewportSelectionPreferences.geometryHighlightColor.toVec4() / 255.0f);
		ColorF selectionColor(gViewportSelectionPreferences.geometrySelectionColor.toVec4() / 255.0f);
		highlightColor.a = selectionColor.a = gViewportSelectionPreferences.geomAlpha;

		gEnv->pRenderer->SetHighlightColor(highlightColor);
		gEnv->pRenderer->SetSelectionColor(selectionColor);
		// outlines should be even to avoid only one side getting a highlight
		gEnv->pRenderer->SetHighlightParams(2 * gViewportSelectionPreferences.outlinePixelWidth, gViewportSelectionPreferences.outlineGhostAlpha);
	};

	// Connect settings updates to renderer viewport updates
	gViewportSelectionPreferences.signalSettingsChanged.Connect(highlightUpdateFunctor, reinterpret_cast<uintptr_t>(this));

	// call once to update colors
	highlightUpdateFunctor();

	CLogFile::AboutSystem();
	REGISTER_CVAR(ed_killmemory_size, -1, VF_DUMPTODISK, "Sets the testing allocation size. -1 for random");
	REGISTER_CVAR(ed_indexfiles, 1, VF_DUMPTODISK, "Index game resource files, 0 - inactive, 1 - active");
	REGISTER_CVAR2_CB("ed_keepEditorActive", &keepEditorActive, 0, VF_NULL, "Keep the editor active, even if no focus is set", KeepEditorActiveChanged);
	REGISTER_INT("ed_useDevManager", 1, VF_INVISIBLE, "Use DevManager with sandbox editor");
	REGISTER_COMMAND("ed_killmemory", KillMemory, VF_NULL, "");
	REGISTER_COMMAND("ed_goto", CmdGotoEditor, VF_CHEAT, "Internal command, used by the 'GOTO' console command\n");
	REGISTER_COMMAND("ed_disable_game_mode", DisableGameMode, VF_NULL, "Will go back to editor mode if in game mode.\n");

	return true;
}

void CGameEngine::InitAdditionalEngineComponents()
{
	LOADING_TIME_PROFILE_SECTION;

	if (HMODULE hGameModule = (HMODULE)gEnv->pGameFramework->GetGameModuleHandle())
	{
		if (IEditorGame::TEntryFunction CreateEditorGame = (IEditorGame::TEntryFunction)GetProcAddress(hGameModule, "CreateEditorGame"))
		{
			m_pEditorGame = CreateEditorGame();

			if (m_pEditorGame == nullptr)
			{
				Error("Failed to create editor game from game dll!");
			}
		}
	}

	// Has to be done regardless of game dll being loaded or not, as we need to create a game context
	InitializeEditorGame();

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - lua entity scripts");
		// Execute Editor.lua override file.
		IScriptSystem* pScriptSystem = m_pISystem->GetIScriptSystem();
		pScriptSystem->ExecuteFile("Editor.Lua", false);

		SplashScreen::SetText("Loading Entity Scripts...");
		CEntityScriptRegistry::Instance()->LoadScripts();
	}

	CEditorImpl* const pEditor = GetIEditorImpl();

	if (pEditor->GetFlowGraphManager())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - FG init");
		SplashScreen::SetText("Loading Flowgraph...");
		pEditor->GetFlowGraphManager()->Init();
	}

	if (pEditor->GetMatFxGraphManager())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Material FX init");
		SplashScreen::SetText("Loading Material Effects Flowgraphs...");
		pEditor->GetMatFxGraphManager()->Init();
	}

	if (pEditor->GetFlowGraphDebuggerEditor())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init FG debugger");
		SplashScreen::SetText("Initializing Flowgraph Debugger...");
		pEditor->GetFlowGraphDebuggerEditor()->Init();
	}

	if (pEditor->GetFlowGraphModuleManager())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init FG modules");
		SplashScreen::SetText("Initializing Flowgraph Module Manager...");
		pEditor->GetFlowGraphModuleManager()->Init();
	}

	// Initialize prefab events after flowgraphmanager to avoid handling creation of flow node prototypes
	CPrefabManager* const pPrefabManager = pEditor->GetPrefabManager();
	if (pPrefabManager)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init Prefab events");
		SplashScreen::SetText("Initializing Prefab Events...");
		CPrefabEvents* pPrefabEvents = pPrefabManager->GetPrefabEvents();
		CRY_ASSERT(pPrefabEvents != NULL);
		const bool bResult = pPrefabEvents->Init();
		if (!bResult)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "CGameEngine::InitGame: Failed to init prefab events");
		}
	}

	if (pEditor->GetAI())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init AI Actions, behaviours, smart objects");
		SplashScreen::SetText("Initializing AI Actions and Smart Objects...");
		pEditor->GetAI()->Init(m_pISystem);
	}

	if (pEditor->GetUIManager())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init UI Actions");
		SplashScreen::SetText("Loading UI Actions...");
		pEditor->GetUIManager()->Init();
	}

	CCustomActionsEditorManager* const pCustomActionsManager = pEditor->GetCustomActionManager();
	if (pCustomActionsManager)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::InitAdditionalEngineComponents - Init UI Actions");
		SplashScreen::SetText("Loading Custom Actions...");
		pCustomActionsManager->Init(m_pISystem);
	}

	// in editor we do it later, bExecuteCommandLine was set to false
	m_pISystem->ExecuteCommandLine();
}

void CGameEngine::InitializeEditorGame()
{
	LOADING_TIME_PROFILE_SECTION;
	if (m_pEditorGame != nullptr)
	{
		m_pEditorGame->Init(m_pISystem, m_pGameToEditorInterface);
	}

	gEnv->pGameFramework->InitEditor(m_pGameToEditorInterface);

	gEnv->bServer = true;
	gEnv->bMultiplayer = false;

#if CRY_PLATFORM_DESKTOP
	gEnv->SetIsClient(true);
#endif

	StartGameContext();
}

bool CGameEngine::StartGameContext()
{
	LOADING_TIME_PROFILE_SECTION;
	if (gEnv->pGameFramework->StartedGameContext())
		return true;

	SGameContextParams ctx;

	SGameStartParams gameParams;
	gameParams.flags = eGSF_Server
	                   | eGSF_NoSpawnPlayer
	                   | eGSF_Client
	                   | eGSF_NoLevelLoading
	                   | eGSF_BlockingClientConnect
	                   | eGSF_NoGameRules
	                   | eGSF_NoQueries;

	gameParams.flags |= eGSF_LocalOnly;

	gameParams.connectionString = "";
	gameParams.hostname = "localhost";
	gameParams.port = 0xed17;
	gameParams.pContextParams = &ctx;
	gameParams.maxPlayers = 1;

	if (gEnv->pGameFramework->StartGameContext(&gameParams))
	{
		return true;
	}

	return false;
}

void CGameEngine::NotifyGameModeChange(bool bGameMode)
{
	if (!bGameMode || StartGameContext())
	{
		gEnv->pGameFramework->OnEditorSetGameMode(bGameMode);

		if (m_pEditorGame != nullptr)
		{
			m_pEditorGame->SetGameMode(bGameMode);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed configuring net context");
	}
}

void CGameEngine::SetLevelPath(const string& path)
{
	m_levelPath = PathUtil::ToUnixPath(PathUtil::RemoveSlash(path.GetString())).c_str();
	m_levelName = m_levelPath.Mid(m_levelPath.ReverseFind('/') + 1);

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->SetLevelPath(m_levelPath);

	if (gEnv->pAISystem)
		gEnv->pAISystem->SetLevelPath(m_levelPath);
}

void CGameEngine::SetMissionName(const string& mission)
{
	m_missionName = mission;
}

bool CGameEngine::LoadLevel(
  const string& levelPath,
  const string& mission,
  bool bDeleteAIGraph,
  bool bReleaseResources)
{
	LOADING_TIME_PROFILE_SECTION(GetIEditorImpl()->GetSystem());
	m_bLevelLoaded = false;
	m_missionName = mission;
	CryLog("Loading map '%s' into engine...", (const char*)m_levelPath);
	// Switch the current directory back to the Master CD folder first.
	// The engine might have trouble to find some files when the current
	// directory is wrong
	SetCurrentDirectory(GetIEditorImpl()->GetMasterCDFolder());

	string pakFile = m_levelPath + "/*.pak";

	// Open Pak file for this level.
	if (!m_pISystem->GetIPak()->OpenPacks(pakFile))
	{
		//CryWarning( VALIDATOR_MODULE_GAME,VALIDATOR_WARNING,"Level Pack File %s Not Found",sPak1.c_str() );
	}

	// Initialize physics grid.
	if (bReleaseResources)
	{
		SSectorInfo si;

		GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);
		float terrainSize = si.sectorSize * si.numSectors;

		if (m_pISystem->GetIPhysicalWorld())
		{
			float fCellSize = terrainSize > 2048 ? terrainSize * (1.0f / 1024) : 2.0f;

			if (ICVar* pCvar = m_pISystem->GetIConsole()->GetCVar("e_PhysMinCellSize"))
				fCellSize = max(fCellSize, (float)pCvar->GetIVal());

			int log2PODGridSize = 0;

			if (fCellSize == 2.0f)
				log2PODGridSize = 2;
			else if (fCellSize == 4.0f)
				log2PODGridSize = 1;

			m_pISystem->GetIPhysicalWorld()->SetupEntityGrid(2, Vec3(0, 0, 0), terrainSize / fCellSize, terrainSize / fCellSize, fCellSize, fCellSize, log2PODGridSize);
		}

		// Resize proximity grid in entity system.
		if (bReleaseResources)
		{
			if (gEnv->pEntitySystem)
				gEnv->pEntitySystem->ResizeProximityGrid(terrainSize, terrainSize);
		}
	}

	// Load level in 3d engine.
	if (!gEnv->p3DEngine->InitLevelForEditor(m_levelPath, m_missionName))
	{
		CLogFile::WriteLine("ERROR: Can't load level !");

		CQuestionDialog::SCritical(QObject::tr("Error"), QObject::tr("ERROR: Can't load level !"));
		return false;
	}

	REINST("notify audio of level loading start?");

	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_REFRESH);

	if (gEnv->pAISystem && bDeleteAIGraph)
	{
		gEnv->pAISystem->FlushSystemNavigation();
	}

	m_bLevelLoaded = true;

	if (!bReleaseResources)
		ReloadEnvironment();

	if (GetIEditorImpl()->GetMatFxGraphManager())
	{
		GetIEditorImpl()->GetMatFxGraphManager()->ReloadFXGraphs();
	}

	return true;
}

bool CGameEngine::ReloadLevel()
{
	if (!LoadLevel(GetLevelPath(), GetMissionName(), false, false))
		return false;

	return true;
}

bool CGameEngine::LoadAI(const string& levelName, const string& missionName)
{
	if (!gEnv->pAISystem)
		return false;

	if (!IsLevelLoaded())
		return false;

	float fStartTime = m_pISystem->GetITimer()->GetAsyncCurTime();
	CryLog("Loading AI data %s, %s", (const char*)levelName, (const char*)missionName);
	gEnv->pAISystem->FlushSystemNavigation();

	// Load only navmesh data, all other working systems are currently restored when editor objects are loaded
	gEnv->pAISystem->LoadLevelData(levelName, missionName, eAILoadDataFlag_MNM);

	CryLog("Finished Loading AI data in %6.3f secs", m_pISystem->GetITimer()->GetAsyncCurTime() - fStartTime);

	return true;
}

bool CGameEngine::FinishLoadAI()
{
	if (!gEnv->pAISystem)
		return false;

	if (!IsLevelLoaded())
		return false;

	if (INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem())
	{
		pNavigationSystem->RemoveLoadedMeshesWithoutRegisteredAreas();
	}

	return true;
}

bool CGameEngine::LoadMission(const string& mission)
{
	if (!IsLevelLoaded())
		return false;

	if (mission != m_missionName)
	{
		m_missionName = mission;
		gEnv->p3DEngine->LoadMissionDataFromXMLNode(m_missionName);
	}

	return true;
}

bool CGameEngine::ReloadEnvironment()
{
	if (!gEnv->p3DEngine)
		return false;

	if (!IsLevelLoaded() && !m_bJustCreated)
		return false;

	if (!GetIEditorImpl()->GetDocument())
		return false;

	XmlNodeRef env = XmlHelpers::CreateXmlNode("Environment");
	CXmlTemplate::SetValues(GetIEditorImpl()->GetDocument()->GetEnvironmentTemplate(), env);

	// Notify mission that environment may be changed.
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->OnEnvironmentChange();
	//! Add lighting node to environment settings.
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetLighting()->Serialize(env, false);

	string xmlStr = env->getXML();

	// Reload level data in engine.
	gEnv->p3DEngine->LoadEnvironmentSettingsFromXML(env);

	return true;
}

void CGameEngine::SwitchToInGame()
{
	if (gEnv->p3DEngine)
	{
		gEnv->p3DEngine->ResetPostEffects();
	}

	GetIEditorImpl()->Notify(eNotify_OnBeginGameMode);

	m_pISystem->SetThreadState(ESubsys_Physics, false);

	if (gEnv->p3DEngine)
	{
		gEnv->p3DEngine->ResetParticlesAndDecals();
	}

	if (GetIEditorImpl()->GetObjectManager() && GetIEditorImpl()->GetObjectManager()->GetLayersManager())
		GetIEditorImpl()->GetObjectManager()->GetLayersManager()->SetGameMode(true);

	//! Notify entities so that they get in the proper state before the layers collect these states.
	GetIEditor()->GetObjectManager()->SendEvent(EVENT_PREINGAME);

	if (GetIEditorImpl()->GetFlowGraphManager())
		GetIEditorImpl()->GetFlowGraphManager()->OnEnteringGameMode(true);

	GetIEditorImpl()->GetAI()->OnEnterGameMode(true);

	if (gEnv->pDynamicResponseSystem)
	{
		gEnv->pDynamicResponseSystem->Reset();
	}

	m_pISystem->GetIMovieSystem()->EnablePhysicsEvents(true);
	m_bInGameMode = true;

	NotifyGameModeChange(true);

	CRuler* pRuler = GetIEditorImpl()->GetRuler();
	if (pRuler)
	{
		pRuler->SetActive(false);
	}

	GetIEditorImpl()->GetAI()->SaveAndReloadActionGraphs();

	CCustomActionsEditorManager* pCustomActionsManager = GetIEditorImpl()->GetCustomActionManager();
	if (pCustomActionsManager)
	{
		pCustomActionsManager->SaveAndReloadCustomActionGraphs();
	}

	gEnv->p3DEngine->GetTimeOfDay()->EndEditMode();
	HideLocalPlayer(false);

	bool bHandledPlayerPositionChange = false;

	if (m_pEditorGame)
	{
		if (m_pEditorGame->SetPlayerPosAng(m_playerViewTM.GetTranslation(), m_playerViewTM.TransformVector(FORWARD_DIRECTION)))
		{
			if (auto* pPlayerEntity = GetPlayerEntity())
			{
				pPlayerEntity->InvalidateTM({ ENTITY_XFORM_POS, ENTITY_XFORM_ROT });
			}

			bHandledPlayerPositionChange = true;
		}
	}

	if (!bHandledPlayerPositionChange)
	{
		if (IEntity* pPlayerEntity = gEnv->pGameFramework->GetClientEntity())
		{
			pPlayerEntity->SetWorldTM(m_playerViewTM);
		}
	}

	// Disable accelerators.
	GetIEditorImpl()->EnableAcceleratos(false);
	// Reset physics state before switching to game.
	m_pISystem->GetIPhysicalWorld()->ResetDynamicEntities();
	// Reset mission script.
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->ResetScript();
	//! Send event to switch into game.
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_INGAME);

	// reset all agents in aisystem
	// (MATT) Needs to occur after ObjectManager because AI now needs control over AI enabling on start {2008/09/22}
	if (m_pISystem->GetAISystem())
	{
		m_pISystem->GetAISystem()->Reset(IAISystem::RESET_ENTER_GAME);
	}

	// When the player starts the game inside an area trigger, it will get
	// triggered.
	gEnv->pEntitySystem->ResetAreas();

	// Register in game entitysystem listener.
	if (!s_InGameEntityListener)
	{
		s_InGameEntityListener = new SInGameEntitySystemListener;
		gEnv->pEntitySystem->AddSink(s_InGameEntityListener, IEntitySystem::OnSpawn | IEntitySystem::OnRemove);
	}

	gEnv->pEntitySystem->OnEditorSimulationModeChanged(IEntitySystem::EEditorSimulationMode::InGame);

	m_pISystem->GetIMovieSystem()->Reset(true, false);

	INotificationCenter* pNotificationCenter = GetIEditorImpl()->GetNotificationCenter();
	QAction* suspendAction = GetIEditorImpl()->GetICommandManager()->GetAction("general.suspend_game_input");
	QString shortcut = suspendAction->shortcut().toString();
	pNotificationCenter->ShowInfo("Starting Game", QString("Press ") + shortcut + " for mouse control");
}

void CGameEngine::SwitchToInEditor()
{
	// if game has been suspended resume first to reset all state as needed
	if (m_bGameModeSuspended)
	{
		ToggleGameInputSuspended();
	}

	m_pISystem->GetIMovieSystem()->Reset(false, false);

	m_pISystem->SetThreadState(ESubsys_Physics, false);

	if (gEnv->p3DEngine)
	{
		// Reset 3d engine effects
		gEnv->p3DEngine->ResetPostEffects();
		gEnv->p3DEngine->ResetParticlesAndDecals();
	}

	if (GetIEditorImpl()->GetObjectManager() && GetIEditorImpl()->GetObjectManager()->GetLayersManager())
		GetIEditorImpl()->GetObjectManager()->GetLayersManager()->SetGameMode(false);

	if (GetIEditorImpl()->GetFlowGraphManager())
		GetIEditorImpl()->GetFlowGraphManager()->OnEnteringGameMode(false);

	GetIEditorImpl()->GetAI()->OnEnterGameMode(false);

	m_pISystem->GetIMovieSystem()->EnablePhysicsEvents(m_bSimulationMode);
	gEnv->p3DEngine->GetTimeOfDay()->BeginEditMode();

	if (gEnv->pDynamicResponseSystem)
		gEnv->pDynamicResponseSystem->Reset(DRS::IDynamicResponseSystem::eResetHint_ResetAllResponses | DRS::IDynamicResponseSystem::eResetHint_Speaker);

	// this has to be done before the RemoveSink() call, or else some entities may not be removed
	gEnv->p3DEngine->GetDeferredPhysicsEventManager()->ClearDeferredEvents();
	if (s_InGameEntityListener)
	{
		// Unregister ingame entitysystem listener, and kill all remaining entities.
		gEnv->pEntitySystem->RemoveSink(s_InGameEntityListener);
		delete s_InGameEntityListener;
		s_InGameEntityListener = 0;
	}
	// Enable accelerators.
	GetIEditorImpl()->EnableAcceleratos(true);
	// Reset mission script.
	GetIEditorImpl()->GetDocument()->GetCurrentMission()->ResetScript();

	if (GetIEditorImpl()->Get3DEngine()->IsTerrainHightMapModifiedByGame())
		GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain();

	// reset movie-system
	m_pISystem->GetIPhysicalWorld()->ResetDynamicEntities();

	gEnv->pEntitySystem->OnEditorSimulationModeChanged(IEntitySystem::EEditorSimulationMode::Editing);

	// clean up AI System
	if (m_pISystem->GetAISystem())
		m_pISystem->GetAISystem()->Reset(IAISystem::RESET_EXIT_GAME);

	m_bInGameMode = false;

	// [Anton] - order changed, see comments for CGameEngine::SetSimulationMode
	//! Send event to switch out of game.
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_OUTOFGAME);

	// Hide all drawing of character.
	HideLocalPlayer(true);

	NotifyGameModeChange(false);

	m_playerViewTM = m_pISystem->GetViewCamera().GetMatrix();

	// Out of game in Editor mode.
	CViewport* pGameViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
	if (pGameViewport)
	{
		pGameViewport->SetViewTM(m_playerViewTM);
	}

	GetIEditorImpl()->Notify(eNotify_OnEndGameMode);
}

void CGameEngine::RequestSetGameMode(bool inGame)
{
	m_ePendingGameMode = inGame ? ePGM_SwitchToInGame : ePGM_SwitchToInEditor;
}

void CGameEngine::SetGameMode(bool bInGame)
{
	if (m_bInGameMode == bInGame)
		return;

	if (!GetIEditorImpl()->GetDocument())
		return;

	LOADING_TIME_PROFILE_AUTO_SESSION(bInGame ? "SetGameModeGame" : "SetGameModeEditor");
	LOADING_TIME_PROFILE_SECTION;

	// Enables engine to know about that.
	gEnv->SetIsEditorGameMode(bInGame);

	// Ignore updates while changing in and out of game mode
	m_bIgnoreUpdates = true;
	LockResources();

	CViewport* pGameViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();
	QWidget* pViewWidget = nullptr;

	if (pGameViewport)
	{
		// Make sure the viewport's window is active and has focus before we capture the input below
		pViewWidget = pGameViewport->GetViewWidget();

		pViewWidget->activateWindow();
		pViewWidget->setFocus();
	}

	if (bInGame)
	{
		SwitchToInGame();
	}
	else
	{
		SwitchToInEditor();
	}

	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_PHYSICS_APPLYSTATE);

	// Enables engine to know about that.
	if (pGameViewport)
	{
		HWND hWnd = (HWND)pViewWidget->window()->winId();

		// marcok: setting exclusive mode based on whether we are inGame
		// fixes mouse sometimes getting stuck in the editor. However, doing
		// this for the keyboard disables editor keys when you go out of
		// gamemode
		// m_ISystem->GetIInput()->SetExclusiveMode( eDI_Mouse, inGame, hWnd );
		m_pISystem->GetIInput()->SetExclusiveMode(eIDT_Keyboard, false, hWnd);
		m_pISystem->GetIInput()->ClearKeyState();

		// Julien: we now use hardware mouse, it doesn't change anything
		// for keyboard but mouse should be fixed now.
		if (gEnv->pHardwareMouse)
		{
			// this is the actual handle to the viewport rendering context, because the hWnd is actually
			// handle to some parent window in the hierarchy; we cannot use hWnd bacause mouse cursor
			// should be confined exactly to the rendering context
			gEnv->pHardwareMouse->SetConfinedWnd(bInGame ? pGameViewport->GetCWnd()->m_hWnd : nullptr);
			gEnv->pHardwareMouse->SetGameMode(bInGame);
		}
	}

	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED, bInGame, 0);

	UnlockResources();
	m_bIgnoreUpdates = false;
}

void CGameEngine::SetSimulationMode(bool enabled, bool bOnlyPhysics)
{
	if (m_bSimulationMode == enabled)
		return;

	gEnv->SetIsEditorSimulationMode(enabled);
	m_pISystem->GetIMovieSystem()->EnablePhysicsEvents(enabled);

	if (!bOnlyPhysics)
		LockResources();

	if (enabled)
	{
		CRuler* pRuler = GetIEditorImpl()->GetRuler();
		if (pRuler)
		{
			pRuler->SetActive(false);
		}

		GetIEditorImpl()->Notify(eNotify_OnBeginSimulationMode);

		if (!bOnlyPhysics)
		{
			GetIEditorImpl()->GetAI()->SaveAndReloadActionGraphs();

			CCustomActionsEditorManager* pCustomActionsManager = GetIEditorImpl()->GetCustomActionManager();
			if (pCustomActionsManager)
			{
				pCustomActionsManager->SaveAndReloadCustomActionGraphs();
			}
		}
	}
	else
		GetIEditorImpl()->Notify(eNotify_OnEndSimulationMode);

	m_bSimulationMode = enabled;
	m_pISystem->SetThreadState(ESubsys_Physics, false);

	if (!bOnlyPhysics)
		m_bSimulationModeAI = enabled;

	if (m_bSimulationMode)
	{
		if (!bOnlyPhysics)
		{
			if (m_pISystem->GetI3DEngine())
				m_pISystem->GetI3DEngine()->ResetPostEffects();

			// make sure FlowGraph is initialized
			if (m_pISystem->GetIFlowSystem())
				m_pISystem->GetIFlowSystem()->Reset(false);

			GetIEditorImpl()->SetConsoleVar("ai_ignoreplayer", 1);
			//GetIEditorImpl()->SetConsoleVar( "ai_soundperception",0 );
		}

		// [Anton] the order of the next 3 calls changed, since, EVENT_INGAME loads physics state (if any),
		// and Reset should be called before it
		m_pISystem->GetIPhysicalWorld()->ResetDynamicEntities();
		GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_INGAME);

		if (!bOnlyPhysics && m_pISystem->GetAISystem())
		{
			// (MATT) Had to move this here because AI system now needs control over entities enabled on start {2008/09/22}
			// When turning physics on, emulate out of game event.
			m_pISystem->GetAISystem()->Reset(IAISystem::RESET_ENTER_GAME);
		}

		if (!s_InGameEntityListener)
		{
			// Register in game entitysystem listener.
			s_InGameEntityListener = new SInGameEntitySystemListener;
			gEnv->pEntitySystem->AddSink(s_InGameEntityListener, IEntitySystem::OnSpawn | IEntitySystem::OnRemove);
		}
	}
	else
	{
		if (!bOnlyPhysics)
		{
			GetIEditorImpl()->SetConsoleVar("ai_ignoreplayer", 0);
			//GetIEditorImpl()->SetConsoleVar( "ai_soundperception",1 );

			if (m_pISystem->GetI3DEngine())
				m_pISystem->GetI3DEngine()->ResetPostEffects();
		}

		m_pISystem->GetIPhysicalWorld()->ResetDynamicEntities();

		if (!bOnlyPhysics)
		{
			if (GetIEditorImpl()->Get3DEngine()->IsTerrainHightMapModifiedByGame())
				GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain();
		}

		GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_OUTOFGAME);

		if (s_InGameEntityListener)
		{
			// Unregister ingame entitysystem listener, and kill all remaining entities.
			gEnv->pEntitySystem->RemoveSink(s_InGameEntityListener);
			delete s_InGameEntityListener;
			s_InGameEntityListener = 0;
		}
	}

	if (!m_bSimulationMode && !bOnlyPhysics)
	{
		// When turning physics off, emulate out of game event.
		if (m_pISystem->GetAISystem())
			m_pISystem->GetAISystem()->Reset(IAISystem::RESET_EXIT_GAME);
	}

	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_PHYSICS_APPLYSTATE);

	if (!bOnlyPhysics)
	{
		gEnv->pEntitySystem->OnEditorSimulationModeChanged(enabled ? IEntitySystem::EEditorSimulationMode::Simulation : IEntitySystem::EEditorSimulationMode::Editing);
	}

	if (gEnv->pGameFramework)
	{
		gEnv->pGameFramework->OnEditorSetGameMode(enabled + 2);
	}

	if (!bOnlyPhysics)
		UnlockResources();

	if (AfxGetMainWnd())
	{
		m_pISystem->GetIInput()->SetExclusiveMode(eIDT_Keyboard, false, AfxGetMainWnd()->GetSafeHwnd());
		m_pISystem->GetIInput()->ClearKeyState();
		AfxGetMainWnd()->SetFocus();
	}
}

void CGameEngine::LoadAINavigationData()
{
	if (!gEnv->pAISystem)
		return;

	CWaitProgress wait("Loading AI Navigation");
	CWaitCursor waitCursor;

	CryLog("Loading AI navigation data for Level:%s Mission:%s", (const char*)m_levelPath, (const char*)m_missionName);
	gEnv->pAISystem->FlushSystemNavigation();
	gEnv->pAISystem->LoadNavigationData(m_levelPath, m_missionName);
}

void CGameEngine::GenerateAiAll(uint32 flags)
{
	if (!gEnv->pAISystem)
		return;

	CWaitProgress wait("Generating All AI Annotations");
	CWaitCursor waitCursor;

	wait.Step(1);

	gEnv->pAISystem->FlushSystemNavigation();

	wait.Step(5);

	string fileNameAreas;
	fileNameAreas.Format("%s/areas%s.bai", m_levelPath, m_missionName);
	m_pNavigation->WriteAreasIntoFile(fileNameAreas.GetString());

	wait.Step(25);
	if (flags & eExp_AI_CoverSurfaces)
	{
		GetIEditorImpl()->GetGameEngine()->GenerateAICoverSurfaces();
	}

	wait.Step(40);
	if (flags & eExp_AI_MNM)
	{
		GenerateAINavigationMesh();
	}

	wait.Step(95);
	CryLog("Validating SmartObjects");

	// quick temp code to validate smart objects on level export
	{
		static CObjectClassDesc* pClass = GetIEditorImpl()->GetObjectManager()->FindClass("SmartObject");
		CRY_ASSERT_MESSAGE(pClass != nullptr, "SmartObject class desc not found");
		//this code now assumes SmartObjects are declared and are inherited from CEntityObject.h.
		//See SmartObject.h in the SmartObjectsEditor plugin where it will now reside.

		string error;
		CBaseObjectsArray objects;
		GetIEditorImpl()->GetObjectManager()->GetObjects(objects);

		CBaseObjectsArray::iterator it, itEnd = objects.end();
		for (it = objects.begin(); it != itEnd; ++it)
		{
			CBaseObject* pBaseObject = *it;
			if (pBaseObject->GetClassDesc() == pClass)
			{
				CEntityObject* pSOEntity = (CEntityObject*)pBaseObject;

				if (!gEnv->pAISystem->GetSmartObjectManager()->ValidateSOClassTemplate(pSOEntity->GetIEntity()))
				{
					const Vec3 pos = pSOEntity->GetWorldPos();

					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "SmartObject '%s' at (%.2f, %.2f, %.2f) is invalid!",
					           (const char*)pSOEntity->GetName(), pos.x, pos.y, pos.z);
				}
			}
		}
	}

	wait.Step(100);
}

void CGameEngine::GenerateAINavigationMesh()
{
	if (!gEnv->pAISystem)
		return;

	CWaitProgress wait("Generating AI MNM navigation meshes");
	string mnmFilename;

	// The NavMesh thread must be running for this step, so temporarily enable it again
	gEnv->pAISystem->GetNavigationSystem()->RestartNavigationUpdate();
	gEnv->pAISystem->GetNavigationSystem()->ProcessQueuedMeshUpdates();
	gEnv->pAISystem->GetNavigationSystem()->PauseNavigationUpdate();

	mnmFilename.Format("%s/mnmnav%s.bai", m_levelPath.GetString(), m_missionName.GetString());

	AILogProgress("Now writing to %s.", mnmFilename.GetString());

	gEnv->pAISystem->GetNavigationSystem()->SaveToFile(mnmFilename);
}

void CGameEngine::ValidateAINavigation()
{
	if (!gEnv->pAISystem)
		return;

	CWaitProgress wait("Validating AI navigation");
	CWaitCursor waitCursor;

	m_pNavigation->ValidateNavigation();
}

void CGameEngine::ClearAllAINavigation()
{
	if (!gEnv->pAISystem)
		return;

	CWaitProgress wait("Clearing all AI navigation data");
	CWaitCursor waitCursor;

	wait.Step(1);
	// Flush old navigation data
	gEnv->pAISystem->FlushSystemNavigation();
	wait.Step(20);
	gEnv->pAISystem->LoadNavigationData(m_levelPath, m_missionName);
	wait.Step(100);
}

void CGameEngine::GenerateAICoverSurfaces()
{
	if (!gEnv->pAISystem)
		return;

	CCoverSurfaceManager* coverSurfaceManager = GetIEditorImpl()->GetAI()->GetCoverSurfaceManager();
	const CCoverSurfaceManager::SurfaceObjects& surfaceObjects = coverSurfaceManager->GetSurfaceObjects();
	uint32 surfaceObjectCount = surfaceObjects.size();

	if (surfaceObjectCount)
	{
		{
			CWaitProgress wait("Generating AI Cover Surfaces");
			CWaitCursor waitCursor;

			wait.Step(0);

			float stepInc = 100.5f / (float)surfaceObjectCount;
			float step = 0.0f;

			GetIEditorImpl()->GetAI()->GetCoverSurfaceManager()->ClearGameSurfaces();
			CCoverSurfaceManager::SurfaceObjects::const_iterator it = surfaceObjects.begin();
			CCoverSurfaceManager::SurfaceObjects::const_iterator end = surfaceObjects.end();

			for (; it != end; ++it)
			{
				CAICoverSurface* coverSurfaceObject = *it;
				coverSurfaceObject->Generate();
				step += stepInc;
				wait.Step((uint32)step);
			}
		}

		{
			CWaitProgress wait("Validating AI Cover Surfaces");
			CWaitCursor waitCursor;

			wait.Step(0);

			float stepInc = 100.5f / (float)surfaceObjectCount;
			float step = 0.0f;

			CCoverSurfaceManager::SurfaceObjects::const_iterator it = surfaceObjects.begin();
			CCoverSurfaceManager::SurfaceObjects::const_iterator end = surfaceObjects.end();

			for (; it != end; ++it)
			{
				CAICoverSurface* coverSurfaceObject = *it;
				coverSurfaceObject->ValidateGenerated();

				step += stepInc;
				wait.Step((uint32)step);
			}
		}
	}

	char fileName[1024];
	cry_sprintf(fileName, "%s\\cover%s.bai", (LPCTSTR)m_levelPath, (LPCTSTR)m_missionName);
	AILogProgress("Now writing to %s.", fileName);
	GetIEditorImpl()->GetAI()->GetCoverSurfaceManager()->WriteToFile(fileName);
}

void CGameEngine::ExportAiData(
  const char* navFileName,
  const char* areasFileName,
  const char* roadsFileName,
  const char* vertsFileName,
  const char* volumeFileName,
  const char* flightFileName)
{
	m_pNavigation->ExportData(
	  navFileName,
	  areasFileName,
	  roadsFileName,
	  vertsFileName,
	  volumeFileName,
	  flightFileName);
}

void CGameEngine::ResetResources()
{
	LOADING_TIME_PROFILE_SECTION;
	if (gEnv->pEntitySystem)
	{
		/// Delete all entities.
		gEnv->pEntitySystem->Reset();

		// In editor we are at the same time client and server.
		gEnv->bServer = true;
		gEnv->bMultiplayer = false;
		gEnv->bHostMigrating = false;
		gEnv->SetIsClient(true);
	}

	if (gEnv->p3DEngine)
	{
		gEnv->p3DEngine->UnloadLevel();
	}

	if (gEnv->pPhysicalWorld)
	{
		// Initialize default entity grid in physics
		m_pISystem->GetIPhysicalWorld()->SetupEntityGrid(2, Vec3(0, 0, 0), 128, 128, 4, 4, 1);
	}

	if (gEnv->pGameFramework->StartedGameContext())
	{
		gEnv->pGameFramework->EndGameContext();
	}
}

void CGameEngine::SetPlayerEquipPack(const char* sEqipPackName)
{
	//TODO: remove if not used, mission related
}

void CGameEngine::ReloadResourceFile(const string& filename)
{
	GetIEditorImpl()->GetRenderer()->EF_ReloadFile(filename);
}

void CGameEngine::SetPlayerViewMatrix(const Matrix34& tm, bool bEyePos)
{
	m_playerViewTM = tm;

	if (m_bSyncPlayerPosition)
	{
		if (m_pEditorGame)
		{
			const Vec3 oPosition(m_playerViewTM.GetTranslation());
			const Vec3 oDirection(m_playerViewTM.TransformVector(FORWARD_DIRECTION));
			m_pEditorGame->SetPlayerPosAng(oPosition, oDirection);
		}
		else if (IEntity* pEntity = gEnv->pGameFramework->GetClientEntity())
		{
			pEntity->SetWorldTM(m_playerViewTM);
		}
	}
}

void CGameEngine::HideLocalPlayer(bool bHide)
{
	if (auto* pActorEntity = gEnv->pGameFramework->GetClientEntity())
	{
		pActorEntity->Hide(bHide);
	}
}

void CGameEngine::SyncPlayerPosition(bool bEnable)
{
	m_bSyncPlayerPosition = bEnable;

	if (m_bSyncPlayerPosition)
	{
		SetPlayerViewMatrix(m_playerViewTM);
	}
}

void CGameEngine::SetCurrentMOD(const char* sMod)
{
	m_MOD = sMod;
}

const char* CGameEngine::GetCurrentMOD() const
{
	return m_MOD;
}

void CGameEngine::Update()
{
	if (m_bIgnoreUpdates)
	{
		return;
	}

	GetIEditorImpl()->GetAI()->EarlyUpdate();

	switch (m_ePendingGameMode)
	{
	case ePGM_SwitchToInGame:
		SetGameMode(true);
		m_ePendingGameMode = ePGM_NotPending;
		break;

	case ePGM_SwitchToInEditor:
		SetGameMode(false);
		m_ePendingGameMode = ePGM_NotPending;
		break;
	}

	if (m_bInGameMode)
	{
		QWidget* pFocusWidget = QApplication::focusWidget();
		CViewport* pRenderViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();

		bool bReadOnlyConsole = pRenderViewport ? pFocusWidget != pRenderViewport->GetViewWidget() : true;
		gEnv->pConsole->SetReadOnly(bReadOnlyConsole);

		gEnv->pSystem->DoFrame((static_cast<CRenderViewport*>(pRenderViewport))->GetDisplayContext().GetDisplayContextKey());

		// TODO: still necessary after AVI recording removal?
		if (pRenderViewport)
		{
			// Make sure we at least try to update game viewport (Needed for AVI recording).
			pRenderViewport->Update();
		}

		GetIEditorImpl()->GetAI()->Update(0);
	}
	else
	{
		for (CListenerSet<IGameEngineListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnPreEditorUpdate();
		}

		gEnv->GetJobManager()->SetFrameStartTime(gEnv->pTimer->GetAsyncTime());

		Cry::IPluginManager* const pPluginManager = gEnv->pSystem->GetIPluginManager();
		pPluginManager->UpdateBeforeSystem();

		CEnumFlags<ESystemUpdateFlags> updateFlags = ESYSUPDATE_EDITOR;

		if (!m_bSimulationMode)
			updateFlags |= ESYSUPDATE_IGNORE_PHYSICS;

		if (!m_bSimulationModeAI)
			updateFlags |= ESYSUPDATE_IGNORE_AI;

		bool bUpdateAIPhysics = GetSimulationMode() || m_bUpdateFlowSystem;

		if (bUpdateAIPhysics)
			updateFlags |= ESYSUPDATE_EDITOR_AI_PHYSICS;

		GetIEditorImpl()->GetSystem()->Update(updateFlags);

		// Update flow system in simulation mode.
		if (bUpdateAIPhysics)
		{
			if (IFlowSystem* pFlowSystem = GetIFlowSystem())
			{
				pFlowSystem->Update();
			}

			if (IDialogSystem* pDialogSystem = gEnv->pGameFramework->GetIDialogSystem())
			{
				pDialogSystem->Update(gEnv->pTimer->GetFrameTime());
			}
			const CRenderViewport* gameViewport = static_cast<CRenderViewport*>(GetIEditorImpl()->GetViewManager()->GetGameViewport());
			CRY_ASSERT(gameViewport);
			gEnv->pSystem->DoFrame(gameViewport->GetDisplayContext().GetDisplayContextKey(), updateFlags);
		}

		pPluginManager->UpdateAfterSystem();
		pPluginManager->UpdateBeforeFinalizeCamera();
		pPluginManager->UpdateBeforeRender();
		pPluginManager->UpdateAfterRender();
		pPluginManager->UpdateAfterRenderSubmit();

		GetIEditorImpl()->GetAI()->Update(updateFlags.UnderlyingValue());

		for (CListenerSet<IGameEngineListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
		{
			notifier->OnPostEditorUpdate();
		}
	}

	GetIEditorImpl()->GetAI()->LateUpdate();
}

void CGameEngine::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneOpen:
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::OnEditorNotifyEvent() OnBeginNewScene/SceneOpen");

			StartGameContext();

			if (m_pEditorGame)
				m_pEditorGame->OnBeforeLevelLoad();

			auto* pGameRulesVar = gEnv->pConsole->GetCVar("sv_gamerules");

			gEnv->pGameFramework->GetIGameRulesSystem()->CreateGameRules(pGameRulesVar->GetString());
			gEnv->pGameFramework->GetILevelSystem()->OnLoadingStart(0);

			if (!gEnv->pGameFramework->BlockingSpawnPlayer())
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed spawning player or waiting for InGame");
			}
		}
		break;
	case eNotify_OnEndSceneOpen:
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::OnEditorNotifyEvent() OnEndSceneOpen");
			if (m_pEditorGame)
			{
				// This method must be called so we will have a player to start game mode later.
				m_pEditorGame->OnAfterLevelLoad(m_levelName, m_levelPath);
			}

			gEnv->pGameFramework->GetILevelSystem()->Rescan("levels", ILevelSystem::TAG_MAIN);

			ILevelInfo* pLevel = gEnv->pGameFramework->GetILevelSystem()->SetEditorLoadedLevel(m_levelName);
			if (pLevel)
			{
				gEnv->pGameFramework->GetILevelSystem()->OnLoadingComplete(pLevel);
			}
		}
	case eNotify_OnEndNewScene: // intentional fall-through?
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::OnEditorNotifyEvent() OnEndNewScene");

			if (m_pEditorGame)
			{
				m_pEditorGame->OnAfterLevelInit(m_levelName, m_levelPath);
			}

			HideLocalPlayer(true);
			SetPlayerViewMatrix(m_playerViewTM); // Sync initial player position

			if (gEnv->p3DEngine)
			{
				gEnv->p3DEngine->PostLoadLevel();
			}
		}
		break;
	case eNotify_OnClearLevelContents:
		{
			LOADING_TIME_PROFILE_SECTION_NAMED("CGameEngine::OnEditorNotifyEvent() OnClearLevelContents");

			SetGameMode(false);

			if (m_pEditorGame)
				m_pEditorGame->OnCloseLevel();
		}
		break;
	case eNotify_OnDisplayRenderUpdate:
		//Note: this seems to be only useful for helpers and only for GameSDK specific helpers,
		//probably not necessary as each game DLL could simply listen to editor events in its editor plyugin
		if (GetIEditorGame())
			GetIEditorGame()->OnDisplayRenderUpdated(GetIEditor()->IsHelpersDisplayed());
		break;
	}
}

IEntity* CGameEngine::GetPlayerEntity()
{
	return gEnv->pGameFramework->GetClientEntity();
}

IFlowSystem* CGameEngine::GetIFlowSystem() const
{
	if (gEnv->pGameFramework)
	{
		return gEnv->pGameFramework->GetIFlowSystem();
	}

	return nullptr;
}

IGameTokenSystem* CGameEngine::GetIGameTokenSystem() const
{
	if (gEnv->pGameFramework)
	{
		return gEnv->pGameFramework->GetIGameTokenSystem();
	}

	return nullptr;
}

IEquipmentSystemInterface* CGameEngine::GetIEquipmentSystemInterface() const
{
	if (m_pEditorGame)
		return m_pEditorGame->GetIEquipmentSystemInterface();

	return nullptr;
}

void CGameEngine::LockResources()
{
	gEnv->p3DEngine->LockCGFResources();
}

void CGameEngine::UnlockResources()
{
	gEnv->p3DEngine->UnlockCGFResources();
}

bool CGameEngine::BuildEntitySerializationList(XmlNodeRef output)
{
	return m_pEditorGame ? m_pEditorGame->BuildEntitySerializationList(output) : true;
}

void CGameEngine::OnTerrainModified(const Vec2& modPosition, float modAreaRadius, bool bFullTerrain, bool bGeometryModified)
{
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

	if (pNavigationSystem)
	{
		// Only report elevationModifications and local modifications, not a change in the full terrain (probably happening during initialization)
		if (bGeometryModified && !bFullTerrain)
		{
			const Vec2 offset(modAreaRadius * 1.5f, modAreaRadius * 1.5f);
			AABB updateBox;
			updateBox.min = modPosition - offset;
			updateBox.max = modPosition + offset;
			const float terrainHeight1 = gEnv->p3DEngine->GetTerrainElevation(updateBox.min.x, updateBox.min.y);
			const float terrainHeight2 = gEnv->p3DEngine->GetTerrainElevation(updateBox.max.x, updateBox.max.y);
			const float terrainHeight3 = gEnv->p3DEngine->GetTerrainElevation(modPosition.x, modPosition.y);

			updateBox.min.z = min(terrainHeight1, min(terrainHeight2, terrainHeight3)) - (modAreaRadius * 2.0f);
			updateBox.max.z = max(terrainHeight1, max(terrainHeight2, terrainHeight3)) + (modAreaRadius * 2.0f);

			GetIEditorImpl()->GetAIManager()->OnAreaModified(updateBox);
		}
	}
}

void CGameEngine::AddListener(IGameEngineListener* pListener)
{
	m_listeners.Add(pListener);
}

void CGameEngine::RemoveListener(IGameEngineListener* pListener)
{
	m_listeners.Remove(pListener);
}

void CGameEngine::ToggleGameInputSuspended()
{
	if (!m_bInGameMode)
	{
		return;
	}

	// toggle the mode
	m_bGameModeSuspended ^= true;

	CViewport* pGameViewport = GetIEditorImpl()->GetViewManager()->GetGameViewport();

	if (!pGameViewport)
		return;

	QWidget* pViewWidget = pGameViewport->GetViewWidget();
	HWND hWnd = (HWND)pViewWidget->window()->winId();

	if (m_bGameModeSuspended)
	{
		// reenable accelerators and actions for editor
		GetIEditorImpl()->EnableAcceleratos(true);

		m_pISystem->GetIInput()->SetExclusiveMode(eIDT_Keyboard, false, hWnd);
		m_pISystem->GetIInput()->ClearKeyState();

		if (gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->SetGameMode(false);
		}
		pGameViewport->SetCurrentCursor(STD_CURSOR_DEFAULT);

		gEnv->pInput->EnableDevice(eIDT_Keyboard, false);
		gEnv->pInput->EnableDevice(eIDT_Mouse, false);
	}
	else
	{
		pViewWidget->activateWindow();
		pViewWidget->setFocus();

		// disable accelerators and actions for editor
		GetIEditorImpl()->EnableAcceleratos(false);

		m_pISystem->GetIInput()->SetExclusiveMode(eIDT_Keyboard, false, hWnd);
		m_pISystem->GetIInput()->ClearKeyState();

		if (gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->SetGameMode(true);
		}
		pGameViewport->SetCurrentCursor(STD_CURSOR_CRYSIS);

		gEnv->pInput->EnableDevice(eIDT_Keyboard, true);
		gEnv->pInput->EnableDevice(eIDT_Mouse, true);
	}
}

bool CGameEngine::IsGameInputSuspended()
{
	return m_bGameModeSuspended;
}

