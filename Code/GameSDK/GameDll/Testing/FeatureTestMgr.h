// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 08:04:2010   Created by Will Wilson
*************************************************************************/

#pragma once

// in sync with ENABLE_FEATURE_TESTER
#if defined(_RELEASE) && !defined(PERFORMANCE_BUILD)
	#define ENABLE_FEATURE_TESTER_MGR 0
#else
	#define ENABLE_FEATURE_TESTER_MGR 1
#endif

// Forward decl.
class CAutoTester;

/// Interface for a feature test for use with CFeatureTestMgr
class IFeatureTest
{
public:
	virtual ~IFeatureTest(){}
	/// Indicates all dependencies are met and this test is ready to run
	virtual bool ReadyToRun() const = 0;

	/// Runs the test
	virtual bool Start() = 0;

	/// Used to update any time dependent state (such as timeouts)
	virtual void Update(float currTime) = 0;

	/// Called to cleanup test state once the test is complete
	virtual void Cleanup() = 0;

	/// Returns the name of the test
	virtual const char* Name() = 0;

	/// Returns the xml description of the test
	///(also containing info about all of the attached entities)
	virtual const XmlNodeRef XmlDescription() = 0;
};

#if ENABLE_FEATURE_TESTER_MGR

enum EFTState
{
	eFTS_Disabled = 0,    /// Not set to run during this test
	eFTS_Scheduled,       /// Scheduled to run once dependencies are met
	eFTS_Running,         /// This is the currently running test
	eFTS_Finished,        /// Test finished
};

// Simple manager for handling map feature testing
class CFeatureTestMgr
{
public:
	friend class CFeatureTestMgrArgumentAutoComplete;

	CFeatureTestMgr();
	~CFeatureTestMgr();

	/// Registers a feature test (does not take ownership of test)
	void RegisterFeatureTest(IFeatureTest* pFeatureTest);

	/// Unregisters a feature test
	void UnregisterFeatureTest(IFeatureTest* pFeatureTest);

	/// Runs all registered tests (if they meet their dependencies)
	void RunAll();

	/// Force running of the defined test case (ignores dependencies)
	void ForceRun(const char* testName);

	/// Updates testing state
	void Update(float deltaTime);

	bool IsRunning() const { return m_bRunning || m_bPendingRunAll || m_bWaiting; }

	/// Called when a test run is done with its results.
	void OnTestResults(const char* testName, const char* testDesc, const char* failureMsg, float duration, const char* owners = NULL);

	/// Called on completion of a feature test.
	void OnTestFinished(IFeatureTest* pFeatureTest);

	/// Used to set the output for tests
	void SetAutoTester(CAutoTester* pAutoTester)
	{
		m_pAutoTester = pAutoTester;
	}

	/// Used to reset internal state in the event of being interrupted (i.e. by editor/game transitions)
	void Reset();

	// Workaround to allow run all command to wait until the level has successfully loaded
	void ScheduleRunAll(bool reloadLevel, bool quickload, float timeoutScheduled)
	{
		m_bPendingLevelReload = reloadLevel;
		m_bPendingQuickload = quickload;
		m_timeoutScheduled = timeoutScheduled;
		m_bPendingRunAll = true;
	}

protected:
	/// Looks for a scheduled test to run, and runs it, otherwise returns false
	bool StartNextTest();

	/// Returns the index of the first test found with the given name or ~0
	size_t FindTest(const char* name) const;

	/// Resets all tests to the given state and performs a cleanup
	void ResetAllTests(EFTState resetState);

	/// Loads the latest save, and reports success or failure as a test result
	void QuickloadReportResults();

	// Console commands
	static void CmdMapRunAll(IConsoleCmdArgs* pArgs);
	static void CmdMapForceRun(IConsoleCmdArgs* pArgs);

private:
	struct FeatureTestState
	{
		FeatureTestState(IFeatureTest* pFT = NULL)
			: m_pTest(pFT),
			m_state(eFTS_Disabled)
		{}

		bool operator==(const FeatureTestState& other) const { return m_pTest == other.m_pTest; }

		IFeatureTest* m_pTest;
		EFTState      m_state;
	};

	//Private method to check if the manager is waiting for scheduled tests to become ready
	bool WaitingForScheduledTests();

	typedef std::vector<FeatureTestState> TFeatureTestVec;

	TFeatureTestVec m_featureTests;           /// All registered tests (elements not owned by this)
	size_t          m_runningTestIndex;       /// Current index into m_featureTests for the running test
	IFeatureTest*   m_pRunningTest;           /// Current test case in progress
	float           m_timeoutScheduled;       /// Timeout to wait for on scheduled tests
	float           m_timeWaitedForScheduled; /// Time waited for scheduled tests that have not yet met start criteria
	CAutoTester*    m_pAutoTester;            /// Auto-test used to output results
	bool            m_bRunning;
	bool            m_bWaiting;               /// Tracks if scheduled tests are still waiting for start conditions
	bool            m_bPendingRunAll;         /// Workaround: Indicates a ft_map_runAll is pending (allows waiting for level load to complete)
	bool            m_bPendingQuickload;      /// Perform a quickload before performing any tests
	bool            m_bPendingLevelReload;    /// Reload the level before performing any tests
	bool            m_bHasQuickloaded;
	bool            m_bTestManifestWritten;
};

#endif //__FEATURETESTMGR_H__