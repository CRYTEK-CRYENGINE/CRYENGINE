// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 08:04:2010   Created by Will Wilson
*************************************************************************/

#include "StdAfx.h"
#include "FeatureTestMgr.h"
#include "AutoTester.h"
#include "FeatureTester.h"

#if ENABLE_FEATURE_TESTER_MGR

class CFeatureTestMgrArgumentAutoComplete : public IConsoleArgumentAutoComplete
{
public:
	virtual int GetCount() const
	{
		CFeatureTestMgr *pFtMgr = GetFeatureTestMgr();
		return pFtMgr ? pFtMgr->m_featureTests.size() : 0;
	}

	virtual const char* GetValue(int nIndex) const
	{
		if (nIndex < 0 || nIndex >= GetCount())
		{
			return NULL;
		}

		CFeatureTestMgr *pFtMgr = GetFeatureTestMgr();
		if (!pFtMgr)
		{
			return NULL;
		}

		IFeatureTest* pFeatureTest = pFtMgr->m_featureTests[nIndex].m_pTest;
		return pFeatureTest ? pFeatureTest->Name() : NULL;
	}

private:
	CFeatureTestMgr* GetFeatureTestMgr() const
	{
#if ENABLE_FEATURE_TESTER
		CFeatureTester* pFTester = CFeatureTester::GetInstance();

		if (pFTester)
		{
			return &pFTester->GetMapFeatureTestMgr();
		}
#endif
		return NULL;
	}
};

static CFeatureTestMgrArgumentAutoComplete s_featureTestMgrArgumentAutoComplete;

CFeatureTestMgr::CFeatureTestMgr()
:	m_runningTestIndex(),
	m_pRunningTest(),
	m_pAutoTester(),
	m_bRunning(),
	m_bPendingRunAll(),
	m_bPendingQuickload(),
	m_bPendingLevelReload(),
	m_bHasQuickloaded(),
	m_timeWaitedForScheduled(0.0f),
	m_bWaiting(false),
	m_timeoutScheduled(0.0f),
	m_bTestManifestWritten(false)
{
	IConsole* pConsole = GetISystem()->GetIConsole();

	if (pConsole)
	{
		REGISTER_COMMAND("ft_map_runAll", CmdMapRunAll, VF_CHEAT, "FEATURE TESTER: Run all enabled map feature tests");
		REGISTER_COMMAND("ft_map_forceRun", CmdMapForceRun, VF_CHEAT, "FEATURE TESTER: Force run of the provided test (even if it's not ready)");
		pConsole->RegisterAutoComplete("ft_map_forceRun", &s_featureTestMgrArgumentAutoComplete);
	}
}

CFeatureTestMgr::~CFeatureTestMgr()
{
	IConsole* pConsole = GetISystem()->GetIConsole();

	if (pConsole)
	{
		pConsole->RemoveCommand("ft_map_runAll");
		pConsole->RemoveCommand("ft_map_forceRun");
	}
}

/// Registers a feature test (does not take ownership of test)
void CFeatureTestMgr::RegisterFeatureTest(IFeatureTest* pFeatureTest)
{
	assert(pFeatureTest);
	if (pFeatureTest)
	{
		if (!stl::push_back_unique(m_featureTests, pFeatureTest))
		{
			CryLog("Feature test case already registered: %s", pFeatureTest->Name());
		}
	}
}

/// Unregisters a feature test
void CFeatureTestMgr::UnregisterFeatureTest(IFeatureTest* pFeatureTest)
{
	assert(pFeatureTest);
	if (pFeatureTest)
	{
		if (pFeatureTest == m_pRunningTest)
			m_pRunningTest = NULL;

		if (!stl::find_and_erase(m_featureTests, pFeatureTest))
		{
			CryLog("Tried to unregister an unknown feature test: %s", pFeatureTest->Name());
		}
	}
}

/// Runs all registered tests (if they meet their dependencies)
void CFeatureTestMgr::RunAll()
{
	// Writing the list of the active registered tests. 
	// This can be useful to later check, when a crash occurs, 
	// which tests were executed and which are skipped 
	// (We will have no valid results for them)
	{
		if(m_pAutoTester && !m_bTestManifestWritten)
		{
			XmlNodeRef testManifest = GetISystem()->CreateXmlNode("testManifest");
			testManifest->setTag("testmanifest");		

			CryLogAlways("About to dump out the testmanifest xml...");

			for (TFeatureTestVec::iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
			{
				FeatureTestState& fTest = *iter;
				XmlNodeRef testDescrNode = fTest.m_pTest->XmlDescription();
				if(testDescrNode)
				{
					testManifest->addChild(testDescrNode);
				}
				
			}

			m_pAutoTester->WriteTestManifest(testManifest);
			m_bTestManifestWritten = true;
		}
	}



	if (!IsRunning())
	{
		// Ensure all tests are cleaned up and scheduled to run
		ResetAllTests(eFTS_Scheduled);

		if (StartNextTest() || WaitingForScheduledTests())
		{
			CryLog("Running all map feature tests...");
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "No tests available to run!");
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tests are already running, can't start more until tests are complete.");
	}
}

/// Force running of the defined test case (ignores dependencies)
void CFeatureTestMgr::ForceRun(const char* testNameFilter)
{
	if (!IsRunning())
	{
		// Ensure no other tests run apart from the selected one
		ResetAllTests(eFTS_Disabled);

		size_t firstTestIndex = ~0;

		for (TFeatureTestVec::iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
		{
			FeatureTestState& ftState = *iter;

			const char* testName = ftState.m_pTest->Name();

			// If test name contains the filter string
			if (strstr(testName, testNameFilter) != NULL)
			{
				ftState.m_state = eFTS_Scheduled;
				
				CryLogAlways("Scheduling test: %s", testName);

				// Store the first valid test found as the one to run first
				if (firstTestIndex == ~0)
					firstTestIndex = std::distance(m_featureTests.begin(), iter);
			}
		}

		if (firstTestIndex != ~0)
		{
			FeatureTestState& ftState = m_featureTests[firstTestIndex];
			ftState.m_state = eFTS_Running;
			m_runningTestIndex = firstTestIndex;
			m_pRunningTest = ftState.m_pTest;
			m_bRunning = true;

			CryLogAlways("Forcefully running Map Tests: %s", m_pRunningTest->Name());

			const float currTime = gEnv->pTimer->GetCurrTime();

			// If test doesn't start
			if (!m_pRunningTest->Start())
			{
				// Reset state
				m_bRunning = false;
				ResetAllTests(eFTS_Disabled);
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Unable to find test(s) to run: %s", testNameFilter);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tests are already running, can't start: %s", testNameFilter);
	}
}

bool CFeatureTestMgr::WaitingForScheduledTests()
{
	//If timeout has not yet been reached
	if( m_timeWaitedForScheduled < m_timeoutScheduled )
	{
		//If there is any test scheduled and still waiting then return true;
		for (TFeatureTestVec::const_iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
		{
			const FeatureTestState& ftState = *iter;
			if(ftState.m_state == eFTS_Scheduled)
			{
				m_bWaiting = true;
				return true;
			}
		}
	}

	m_bWaiting = false;
	return false;
}

/// Updates testing state
void CFeatureTestMgr::Update(float deltaTime)
{
	if (m_bPendingLevelReload)
	{
		m_bPendingLevelReload = false;
		m_bHasQuickloaded = false;
		CryLogAlways("Reloading level before starting map tests.");
		gEnv->pConsole->ExecuteString("map");
		return;
	}

	if (m_bPendingQuickload)
	{
		m_bPendingQuickload = false;

		bool bAllowQuickload = true;

		if (gEnv->IsEditor())
		{
			ICVar* pg_allowSaveLoadInEditor = gEnv->pConsole->GetCVar("g_allowSaveLoadInEditor");
			const bool bAllowSaveInEditor = pg_allowSaveLoadInEditor != NULL && (pg_allowSaveLoadInEditor->GetIVal() != 0);
			if (!bAllowSaveInEditor)
			{
				CryLogAlways("Ignoring quickload tests in editor");
				bAllowQuickload = false;
			}
		}

		if (bAllowQuickload)
		{
			QuickloadReportResults();
			return;
		}
	}

	// WORKAROUND: Auto-tester sends run all request before FG tests have loaded, so we wait for them to register here
	if (m_bPendingRunAll)
	{
		// Have any feature tests registered yet?
		if (!m_featureTests.empty())
		{
			// Initiate the RunAll!
			m_bPendingRunAll = false;
			RunAll();
		}
	}

	// If running tests
	if (m_bRunning)
	{
		// If a test is in progress
		if (m_pRunningTest)
		{
			m_pRunningTest->Update(deltaTime);
		}
		else	// We don't have a current test
		{
			// Get one from the scheduled list
			if (!StartNextTest() && !WaitingForScheduledTests())
			{
				CryLogAlways("Finished running map tests!");

				//reset g_testStatus if it was set by tester
				if (g_pGameCVars->g_testStatus != 0)
				{
					CryLog("Resetting g_testStatus to notify tester.");
					g_pGameCVars->g_testStatus = 0;
				}
			}
		}
	}
	else
	{
		//If we have tests scheduled but nothing is running, then wait for a time out

		if(WaitingForScheduledTests())
		{
			m_timeWaitedForScheduled += deltaTime;
			if(m_timeWaitedForScheduled >= m_timeoutScheduled)
			{
				m_bWaiting = false;
				CryLogAlways("More feature tests were scheduled, but exceeded wait time:%.2f!", m_timeoutScheduled);
			}
			else
			{
				//Try to start the next test again in case conditions are now met
				StartNextTest();
			}
		}
	}
}

/// Called when a test run is done with its results.
void CFeatureTestMgr::OnTestResults(const char* testName, const char* testDesc, const char* failureMsg, float duration, const char* owners)
{
	CryLogAlways("Finished running test \"%s\" in %fs: %s", testName, duration, (!failureMsg ? "Succeeded" : failureMsg));

	if (m_pAutoTester)
	{
		const char* testGroupName = m_bHasQuickloaded ? "FG Tests (after quickload)" : "FG Tests";
		m_pAutoTester->AddSimpleTestCase(testGroupName, testName, duration, failureMsg, owners);
		m_pAutoTester->WriteResults(m_pAutoTester->kWriteResultsFlag_unfinished);
	}
}

/// Called on completion of a feature test
void CFeatureTestMgr::OnTestFinished(IFeatureTest* pFeatureTest)
{
	assert(m_pRunningTest == pFeatureTest);

	if (m_pRunningTest == pFeatureTest)
	{
		if (m_runningTestIndex < m_featureTests.size())
		{
			FeatureTestState& ftState = m_featureTests[m_runningTestIndex];

			ftState.m_state = eFTS_Finished;
		}

		// Indicate it's time for the next test (resolved on update)
		m_pRunningTest = NULL;	// OR: Dispense with update and just call here?
	}
}

/// Used to reset internal state in the event of being interrupted (i.e. by editor/game transitions)
void CFeatureTestMgr::Reset()
{
	m_runningTestIndex = 0;
	m_pRunningTest = NULL;
	m_bRunning = false;
	m_bPendingRunAll = false;
	m_pAutoTester = NULL;
	m_timeWaitedForScheduled = 0.0f;
	m_bWaiting = false;
	m_timeoutScheduled = 0.0f;
}

/// Looks for a test to run, and runs it, otherwise returns false
bool CFeatureTestMgr::StartNextTest()
{
	// Ensure previous test is complete first
	assert(!m_pRunningTest);

	for (TFeatureTestVec::iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
	{
		FeatureTestState& ftState = *iter;

		// TODO: Check category, other filters?

		// Find the first test hasn't been run and has no dependencies
		if (ftState.m_state == eFTS_Scheduled && ftState.m_pTest->ReadyToRun())
		{
			ftState.m_state = eFTS_Running;
			m_runningTestIndex = std::distance(m_featureTests.begin(), iter);
			m_pRunningTest = ftState.m_pTest;

			CryLogAlways("Running Map Test: %s", m_pRunningTest->Name());

			// Run it!
			if (m_pRunningTest->Start())
			{
				break;
			}
			else
			{
				OnTestResults(m_pRunningTest->Name(), "UNKNOWN", "Failed to start", 0.0f);
				m_pRunningTest = NULL;
			}
		}
	}

	m_bRunning = m_pRunningTest != NULL;

	// Returns false when no more tests are available to run
	return m_bRunning;
}

/// Returns the index of the first test found with the given name or ~0
size_t CFeatureTestMgr::FindTest(const char* name) const
{
	size_t resultIndex = ~0;

	for (TFeatureTestVec::const_iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
	{
		const FeatureTestState& ftState = *iter;
		const char* testName = ftState.m_pTest->Name();
		if (stricmp(name, testName) == 0)
		{
			resultIndex = std::distance(m_featureTests.begin(), iter);
			break;
		}
	}

	return resultIndex;
}

/// Resets all tests to to the given state
void CFeatureTestMgr::ResetAllTests(EFTState resetState)
{
	if (m_bRunning)
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Resetting tests while testing was running - may have undesired side effects!");

	m_runningTestIndex = 0;
	m_pRunningTest = NULL;

	for (TFeatureTestVec::iterator iter(m_featureTests.begin()); iter != m_featureTests.end(); ++iter)
	{
		FeatureTestState& ftState = *iter;

		ftState.m_state = resetState;
	}
}

/// Writes the success or otherwise of a quickload "test" case
void CFeatureTestMgr::QuickloadReportResults()
{
	m_bHasQuickloaded = true;
	CryLogAlways("Performing quickload before starting map tests.");
	const float startTime = gEnv->pTimer->GetCurrTime();
	const bool success = g_pGame->LoadLastSave();
	const float duration = gEnv->pTimer->GetCurrTime() - startTime;

	const char* const failureMessage = success ? NULL : "Couldn't load last save";
	OnTestResults("Quickload", "Quickload before running tests", failureMessage, duration);
}

// Console commands
void CFeatureTestMgr::CmdMapRunAll(IConsoleCmdArgs *pArgs)
{
#if ENABLE_FEATURE_TESTER
	CFeatureTester* pFTester = CFeatureTester::GetInstance();

	bool reloadLevel = false;
	bool quickload = false;

	int argCount = pArgs->GetArgCount();
	for (int argIndex = 0; argIndex < argCount; ++argIndex)
	{
		const char* arg = pArgs->GetArg(argIndex);
		if (!stricmp("reloadlevel", arg))
		{
			reloadLevel = true;
		}
		if (!stricmp("quickload", arg))
		{
			quickload = true;
		}
	}

	if (pFTester)
		pFTester->GetMapFeatureTestMgr().ScheduleRunAll(reloadLevel, quickload, 0.0f);
#else
	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Feature testing not enabled in this build.");
#endif
}

void CFeatureTestMgr::CmdMapForceRun(IConsoleCmdArgs *pArgs)
{
#if ENABLE_FEATURE_TESTER
	CFeatureTester* pFTester = CFeatureTester::GetInstance();

	if (pFTester)
	{
		CFeatureTestMgr& ftMgr = pFTester->GetMapFeatureTestMgr();

		if (pArgs->GetArgCount() == 2)
		{
			const char* ftName = pArgs->GetArg(1);
			ftMgr.ForceRun(ftName);
		}
		else
		{
			CryLogAlways("Usage: %s <testname>", pArgs->GetArg(0));
		}
	}
#else
	CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Feature testing not enabled in this build.");
#endif
}

#endif // ENABLE_FEATURE_TESTER_MGR