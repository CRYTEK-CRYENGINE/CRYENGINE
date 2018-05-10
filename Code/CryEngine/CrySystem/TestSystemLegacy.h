// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ITestSystem.h> // ITestSystem
#include "Log.h"                   // CLog
#include "UnitTestSystem.h"        // CUnitTestManager

struct ISystem;
struct IConsole;
struct IConsoleCmdArgs;

// needed for external test application
class CTestSystemLegacy : public ITestSystem
{
public:

	//! constructs test system that outputs to dedicated log
	CTestSystemLegacy(ISystem* pSystem);

	//! initializes supported console commands. Must be called after the console is set up.
	static void InitCommands();

	// interface ITestSystem -----------------------------------------------
	virtual void                           ApplicationTest(const char* szParam) override;
	virtual void                           Update() override;
	virtual void                           BeforeRender() override;
	virtual void                           AfterRender()  override {}
	virtual void                           InitLog() override;
	virtual ILog*                          GetILog() override      { return &m_log; }
	virtual void                           Release() override      { delete this; }
	virtual void                           SetTimeDemoInfo(STimeDemoInfo* pTimeDemoInfo) override;
	virtual STimeDemoInfo*                 GetTimeDemoInfo() override;
	virtual void                           QuitInNSeconds(const float fInNSeconds) override;
	virtual CryUnitTest::IUnitTestManager* GetIUnitTestManager()  override { return &m_unitTestManager; };

private: // --------------------------------------------------------------

	// TGA screenshot
	void        ScreenShot(const char* szDirectory, const char* szFilename);
	//
	void        LogLevelStats();
	// useful when running through a lot of tests
	void        DeactivateCrashDialog();

	static void RunUnitTests(IConsoleCmdArgs* pArgs);

private: // --------------------------------------------------------------
	friend class CLevelListener;

	CLog                          m_log;
	STimeDemoInfo                 m_timeDemoInfo;
	CryUnitTest::CUnitTestManager m_unitTestManager;

	string                        m_sParameter;            // "" if not in test mode
	int                           m_iRenderPause = 0;      // counts down every render to delay some processing
	float                         m_fQuitInNSeconds = 0.f; // <=0 means it's deactivated
	bool                          m_bFirstUpdate = true;
	bool                          m_bApplicationTest = false;
};
