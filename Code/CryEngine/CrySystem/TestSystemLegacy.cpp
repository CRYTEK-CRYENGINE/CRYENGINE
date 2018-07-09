// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/ISystem.h>                      // ISystem
#include <CryEntitySystem/IEntity.h>                // EntityId
#include <CrySystem/ITimer.h>                       // ITimer
#include <CryEntitySystem/IEntitySystem.h>          // IEntityIt
#include <CrySystem/IConsole.h>                     // IConsole
#include <Cry3DEngine/I3DEngine.h>                  // I3DEngine
#include <CryGame/IGameFramework.h>                 // IGameFramework
#include "TestSystemLegacy.h"
#include "DebugCallStack.h"                 // DebugCallStack

#include <../CryAction/ILevelSystem.h>                   // ILevelSystemListener

extern int CryMemoryGetAllocatedSize();

CTestSystemLegacy::CTestSystemLegacy(ISystem* pSystem)
	: m_log(pSystem)
	, m_unitTestManager(m_log)
{
}

//////////////////////////////////////////////////////////////////////////
/*static*/ void CTestSystemLegacy::InitCommands()
{
	REGISTER_COMMAND("RunUnitTests", RunUnitTests, VF_INVISIBLE | VF_CHEAT, "Execute a set of unit tests");
}

//////////////////////////////////////////////////////////////////////////
/*static*/ void CTestSystemLegacy::RunUnitTests(IConsoleCmdArgs* pArgs)
{
	int nExitCode = gEnv->pSystem->GetITestSystem()->GetIUnitTestManager()->RunAllTests(CryUnitTest::EReporterType::Excel);

	// Check for "noquit" option.
	for (int a = 1; a < pArgs->GetArgCount(); a++)
		if (!strcmpi(pArgs->GetArg(a), "noquit"))
			return;

	// Exit application at the testing end.
	exit(nExitCode);
}

//////////////////////////////////////////////////////////////////////////
void CTestSystemLegacy::QuitInNSeconds(const float fInNSeconds)
{
	if (fInNSeconds > 0)
		m_log.Log("QuitInNSeconds() requests quit in %f sec", fInNSeconds);

	m_fQuitInNSeconds = fInNSeconds;
}

//////////////////////////////////////////////////////////////////////////
void CTestSystemLegacy::InitLog()
{
	m_log.SetFileName("%USER%/TestResults/TestLog.log");
}

class CLevelListener : public ILevelSystemListener
{
public:
	CLevelListener(CTestSystemLegacy& rTestSystem) : m_rTestSystem(rTestSystem)
	{
		m_LevelStartTime = gEnv->pTimer->GetAsyncTime();
	}

	// interface ILevelSystemListener -------------------------------

	virtual void OnLevelNotFound(const char* levelName)
	{
		m_rTestSystem.GetILog()->LogError("CLevelListener '%s' level not found", levelName);
		Quit();
	}
	virtual void OnLoadingStart(ILevelInfo* pLevel)              {}
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) {};
	virtual void OnLoadingComplete(ILevelInfo* pLevel)
	{
		m_rTestSystem.LogLevelStats();
		Quit();
	}

	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error)
	{
		m_rTestSystem.GetILog()->LogError("TestSystem '%s' loading error:'%s'", pLevel->GetName(), error);
		Quit();
	}
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) {}
	virtual void OnUnloadComplete(ILevelInfo* pLevel)                      {}

	// ------------------------------------------------------------------

	void Quit()
	{
		CTimeValue time = gEnv->pTimer->GetAsyncTime() - m_LevelStartTime;

		m_rTestSystem.GetILog()->Log("   Time since level start: %u min %u sec", ((uint32)time.GetSeconds()) / 60, ((uint32)time.GetSeconds()) % 60);

		gEnv->pConsole->ExecuteString("quit");
	}

	// ------------------------------------------------------------------

	CTestSystemLegacy& m_rTestSystem;                   //
	CTimeValue         m_LevelStartTime;                // absolute time at level start
};

void CTestSystemLegacy::ApplicationTest(const char* szParam)
{
	m_sParameter = szParam;
	m_iRenderPause = 0;

	if (!szParam)
	{
		m_log.LogError("ApplicationTest() parameter missing");
		return;
	}

	if (m_sParameter.CompareNoCase("LevelStats") == 0)
	{
		if (gEnv->pGameFramework)
		{
			static CLevelListener listener(*this);

			if (m_bFirstUpdate)
			{
				DeactivateCrashDialog();
				gEnv->pGameFramework->GetILevelSystem()->AddListener(&listener);
			}
		}
	}

	gEnv->pConsole->ShowConsole(false);

	m_bApplicationTest = true;

	//	m_log.Log("ApplicationTest '%s'",szParam);
}

void CTestSystemLegacy::LogLevelStats()
{
	// get version
	const SFileVersion& ver = GetISystem()->GetFileVersion();
	char sVersion[128];
	ver.ToString(sVersion);

	m_log.Log("   LevelStats Level='%s'   Ver=%s", gEnv->pGameFramework->GetLevelName(), sVersion);

	{
		// copied from CBudgetingSystem::MonitorSystemMemory
		int memUsageInMB_Engine(CryMemoryGetAllocatedSize());
		int memUsageInMB_SysCopyMeshes = 0;
		int memUsageInMB_SysCopyTextures = 0;
		gEnv->pRenderer->EF_Query(EFQ_Alloc_APIMesh, memUsageInMB_SysCopyMeshes);
		gEnv->pRenderer->EF_Query(EFQ_Alloc_APITextures, memUsageInMB_SysCopyTextures);

		int memUsageInMB(RoundToClosestMB((size_t) memUsageInMB_Engine +
		                                  (size_t) memUsageInMB_SysCopyMeshes + (size_t) memUsageInMB_SysCopyTextures));

		memUsageInMB_Engine = RoundToClosestMB(memUsageInMB_Engine);
		memUsageInMB_SysCopyMeshes = RoundToClosestMB(memUsageInMB_SysCopyMeshes);
		memUsageInMB_SysCopyTextures = RoundToClosestMB(memUsageInMB_SysCopyTextures);

		m_log.Log("   System memory: %d MB [%d MB (engine), %d MB (managed textures), %d MB (managed meshes)]",
		               memUsageInMB, memUsageInMB_Engine, memUsageInMB_SysCopyTextures, memUsageInMB_SysCopyMeshes);
	}

	gEnv->pConsole->ExecuteString("SaveLevelStats");
}

void CTestSystemLegacy::Update()
{
	if (m_fQuitInNSeconds > 0.0f)
	{
		int iSec = (int)m_fQuitInNSeconds;

		m_fQuitInNSeconds -= gEnv->pTimer->GetFrameTime();

		if (m_fQuitInNSeconds <= 0.0f)
		{
			gEnv->pConsole->ExecuteString("ExitOnQuit 1");
			gEnv->pSystem->Quit();
		}
		else
		{
			if (iSec != (int)m_fQuitInNSeconds)
				gEnv->pLog->Log("quit in %d seconds ...", iSec);
		}
	}

	if (!m_bApplicationTest)
		return;

	if (m_sParameter.empty())
		return;

	if (m_bFirstUpdate)
	{
		if (m_sParameter.CompareNoCase("TimeDemo") == 0)
		{
			gEnv->pConsole->ExecuteString("exec TimeDemo.exc");
		}
	}

	m_bFirstUpdate = false;
}

void CTestSystemLegacy::DeactivateCrashDialog()
{
#ifndef _DEBUG
	#if CRY_PLATFORM_WINDOWS
	((DebugCallStack*)IDebugCallStack::instance())->SetUserDialogEnable(false);
	#endif
#endif
}

void CTestSystemLegacy::BeforeRender()
{
	const int iPauseTime = 20;

	if (m_sParameter.empty())
		return;

	I3DEngine* p3DEngine = gEnv->p3DEngine;
	IRenderer* pRenderer = gEnv->pRenderer;

	if (m_sParameter.CompareNoCase("PrecacheCameraScreenshots") == 0)
	{
		assert(p3DEngine);
		assert(pRenderer);

		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		IEntityClass* pPrecacheCameraClass = pEntitySystem->GetClassRegistry()->FindClass("PrecacheCamera");
		static IEntityItPtr pEntityIter = 0;

		if (m_iRenderPause != 0)
			--m_iRenderPause;
		else
		{
			if (!pEntityIter)
			{
				pEntityIter = pEntitySystem->GetEntityIterator();
			}
			else pEntityIter->Next();

			m_iRenderPause = iPauseTime;                                // wait 20 frames
		}

		IEntity* pEntity = 0;
		if (pEntityIter)
		{
			pEntity = pEntityIter->This();
		}

		// find next of the right type
		while (pEntity && pEntity->GetClass() != pPrecacheCameraClass)
		{
			pEntityIter->Next();
			pEntity = pEntityIter->This();
		}

		if (!pEntity)
		{
			m_sParameter = "";                                          // stop processing
			m_iRenderPause = 0;
			return;
		}

		const char* szDirectory = "TestSystemOutput";

		if (m_iRenderPause == iPauseTime - 1)
			ScreenShot(szDirectory, (string(pEntity->GetName()) + "_immediate.bmp").c_str());

		if (m_iRenderPause == 0)
			ScreenShot(szDirectory, (string(pEntity->GetName()) + "_later.bmp").c_str());

		// setup camera
		CCamera cam = GetISystem()->GetViewCamera();
		Matrix34 mat = pEntity->GetWorldTM();
		cam.SetMatrix(mat);
		cam.SetFrustum(pRenderer->GetWidth(), pRenderer->GetHeight(), gf_PI / 2, cam.GetNearPlane(), cam.GetFarPlane());

		GetISystem()->SetViewCamera(cam);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTestSystemLegacy::ScreenShot(const char* szDirectory, const char* szFilename)
{
	assert(szDirectory);
	assert(szFilename);

	CryCreateDirectory(szDirectory);  // ensure the directory is existing - might not be needed

	// directory is generated automatically
	gEnv->pRenderer->ScreenShot(string(szDirectory) + "/" + szFilename);

	m_log.Log("Generated screen shot '%s/%s'", szDirectory, szFilename);
}

//////////////////////////////////////////////////////////////////////////
void CTestSystemLegacy::SetTimeDemoInfo(STimeDemoInfo* pTimeDemoInfo)
{
	m_timeDemoInfo = *pTimeDemoInfo;
}

//////////////////////////////////////////////////////////////////////////
STimeDemoInfo* CTestSystemLegacy::GetTimeDemoInfo()
{
	return &m_timeDemoInfo;
}
