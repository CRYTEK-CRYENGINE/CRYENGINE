// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#if !defined(CRY_TESTING_USE_EXCEPTIONS)
	#include <setjmp.h>
#endif
#include <CrySystem/ISystem.h>
#include <CrySystem/ConsoleRegistration.h>
#include <CryGame/IGameFramework.h>
#include <CryCore/optional.h>
#include "UnitTestExcelReporter.h"
#include "TestSystem.h"

namespace CryTest
{

class CTestRecordFile
{
public:
	CTestRecordFile(const char* path)
		: m_Path(path)
	{
		FILE* record = gEnv->pCryPak->FOpen(m_Path, "r");
		if (record)
		{
			char testName[256] = {};
			gEnv->pCryPak->FGets(testName, 256, record);
			if (*testName)
			{
				m_LastRunningTest = string(testName);
			}
			gEnv->pCryPak->FClose(record);
		}
	}

	~CTestRecordFile()
	{
		CleanUp();
	}

	stl::optional<string> GetLastTestName() const { return m_LastRunningTest; }
	void                  SetLastTestName(string name)
	{
		FILE* record = gEnv->pCryPak->FOpen(m_Path, "w");
		if (record)
		{
			m_LastRunningTest = name;
			gEnv->pCryPak->FPrintf(record, "%s", name.c_str());
			gEnv->pCryPak->FClose(record);
		}
	}

private:
	void CleanUp()
	{
		if (gEnv && gEnv->pCryPak)
		{
			gEnv->pCryPak->RemoveFile(m_Path);
		}
	}

	const char*           m_Path = nullptr;
	stl::optional<string> m_LastRunningTest;
};

struct SCryTestArgumentAutoComplete : public IConsoleArgumentAutoComplete
{
	// Gets number of matches for the argument to auto complete.
	virtual int GetCount() const override
	{
		return testNames.size();
	}

	// Gets argument value by index, nIndex must be in 0 <= nIndex < GetCount()
	virtual const char* GetValue(int nIndex) const override
	{
		return testNames[nIndex];
	}

	void Add(CTestFactory* pFactory)
	{
		auto& info = pFactory->GetTestInfo();
		string name;
		if (!info.module.empty())
			name += info.module;
		if (!info.suite.empty())
			name += "." + info.suite;
		name += "." + info.name;
		if (!name.empty())
			testNames.push_back(name);
	}

	std::vector<string> testNames;
};

static SCryTestArgumentAutoComplete sCryTestArgAutoComplete;

static constexpr float FreezeTimeOut = 120.f;

class CTestSystem::CTestInstance
{
public:
	CTestInstance(CTestFactory* pTestFactory)
	{
		m_currentTestInfo = pTestFactory->GetTestInfo();
		m_pCurrentTest = pTestFactory->CreateTest();
		m_TimeOut = pTestFactory->GetTimeOut();
	}
	void  Init()           { m_pCurrentTest->Init(); }
	void  Done()           { m_pCurrentTest->Done(); }
	void  Run()            { m_pCurrentTest->Run(); }
	bool  UpdateCommands() { return m_pCurrentTest->UpdateCommands(); }
	float GetTimeOut() const
	{
		return m_TimeOut;
	}
	void StartCountingCurrentTestTime()
	{
		m_currentTestStartTime = std::chrono::system_clock::now();
	}
	std::chrono::system_clock::duration GetCurrentTestElapsed()
	{
		return std::chrono::system_clock::now() - m_currentTestStartTime;
	}
	const STestInfo&           GetTestInfo() const    { return m_currentTestInfo; }
	void                       AddFailure(SError err) { m_currentTestFailures.push_back(err); }
	const std::vector<SError>& GetFailures() const    { return m_currentTestFailures; }

private:

	std::unique_ptr<CTest>                m_pCurrentTest;
	float                                 m_TimeOut;
	std::chrono::system_clock::time_point m_currentTestStartTime;
	STestInfo                             m_currentTestInfo;
	std::vector<SError>                   m_currentTestFailures;
};

CTestSystem::CTestSystem(ISystem* pSystem)
	: m_log(pSystem)
{
	// Listen to asserts and fatal errors, turn them into test failures during the test
	if (!gEnv->pSystem->RegisterErrorObserver(this))
	{
		CRY_ASSERT_MESSAGE(false, "Test system failed to register error system callback");
	}

	// Spawn a thread to help figuring out time out or hang
	if (!gEnv->pThreadManager->SpawnThread(this, "Test System"))
	{
		CRY_ASSERT_MESSAGE(false, "Error spawning test system watch thread.");
	}

	// Register tests defined in this module
	for (CTestFactory* pFactory = CTestFactory::GetFirstInstance();
	     pFactory != nullptr;
	     pFactory = pFactory->GetNextInstance())
	{
		pFactory->SetModuleName("CrySystem");
		AddFactory(pFactory);
	}
}

CTestSystem::~CTestSystem()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);

	// Clean up listening to asserts and fatal errors
	if (!gEnv->pSystem->UnregisterErrorObserver(this))
	{
		CRY_ASSERT_MESSAGE(false, "Test system failed to unregister error system callback");
	}
}

//////////////////////////////////////////////////////////////////////////
void RunTests(IConsoleCmdArgs* pArgs)
{
#ifdef CRY_TESTING
	string singleTestName = nullptr;

	for (int i = 1; i < pArgs->GetArgCount(); i++)
	{
		string testName = pArgs->GetArg(i);
		size_t lastDot = testName.find_last_of('.');
		CRY_ASSERT(lastDot != string::npos);
		singleTestName = testName.substr(lastDot + 1);
		break;   //supports one test name as argument
	}

	ITestSystem* pTestSystem = gEnv->pSystem->GetITestSystem();
	pTestSystem->SetQuitAfterTests(false);
	if (!singleTestName.empty())
		pTestSystem->RunSingle(singleTestName.c_str());
	else
		pTestSystem->Run();
#endif // CRY_TESTING
}

//////////////////////////////////////////////////////////////////////////
/*static*/ void CTestSystem::InitCommands()
{
	REGISTER_COMMAND("crytest", RunTests, VF_INVISIBLE | VF_CHEAT, "Execute a set of crytests");
	gEnv->pConsole->RegisterAutoComplete("crytest", &sCryTestArgAutoComplete);
}

//////////////////////////////////////////////////////////////////////////
void CTestSystem::InitLog()
{
	m_log.SetFileName("%USER%/TestResults/TestLog.log");
}

//////////////////////////////////////////////////////////////////////////
ILog* CTestSystem::GetLog()
{
	return &m_log;
}

#if !defined(CRY_TESTING_USE_EXCEPTIONS)
jmp_buf* GetAssertJmpBuf()
{
	static jmp_buf s_jmpBuf;
	return &s_jmpBuf;
}
#endif

template<typename F>
stl::optional<SError> TryRun(F&& function)
{
#if defined(CRY_TESTING_USE_EXCEPTIONS)
	try
	{
		function();
		return {};
	}
	catch (assert_exception const& e)
	{
		return SError{
						 e.what(), e.m_filename, e.m_lineNumber };
	}
	catch (std::exception const& e)
	{
		string failureMessage;
		failureMessage.Format("Unhandled exception: %s", e.what());
		return SError{
						 failureMessage, "Unknown", 0 };
	}
#else
	if (setjmp(*GetAssertJmpBuf()) == 0)
	{
		function();
		return {};
	}
	else
	{
		return SError{
						 "Crash", "Unknown", 0 };
	}
#endif // defined(CRY_TESTING_USE_EXCEPTIONS)
}

void CTestSystem::Update()
{
	if (m_isRunningTest)
	{
		if (!m_remainingTestFactories.empty() && !m_currentTestInstance)
		{
			CTestFactory* pTestFactory = m_remainingTestFactories.front();
			CRY_ASSERT(pTestFactory);
			const bool isEnabledForGame = pTestFactory->IsEnabledForGame();
			const bool isEnabledForEditor = pTestFactory->IsEnabledForEditor();
			const bool isEditor = gEnv->IsEditor();
			if ((isEnabledForGame && !isEditor) || (isEnabledForEditor && isEditor))
			{
				m_currentTestInstance = stl::make_unique<CTestInstance>(pTestFactory);
				m_remainingTestFactories.pop();
				m_pTestRecordFile->SetLastTestName(m_currentTestInstance->GetTestInfo().name);
				if (auto pReporter = m_pReporter.lock())
					pReporter->SaveTemporaryReport();
				if (!InitTest())  //critical errors
				{
					FinishTest();
				}
			}
			else   // test is skipped
			{
				// in the editor, warn user for running game-only tests. 
				if (!isEnabledForEditor && isEditor)
				{
					CryLogAlways("Test %s is marked as disabled for Sandbox", pTestFactory->GetTestInfo().name.c_str());
				}
				m_remainingTestFactories.pop();
			}
		}

		if (m_currentTestInstance)
		{
			bool isDone = false;
			TryRun([&]
				{
					isDone = m_currentTestInstance->UpdateCommands();
				});

			//check timeout
			std::chrono::duration<float> runTimeSeconds = m_currentTestInstance->GetCurrentTestElapsed();
			if (m_currentTestInstance->GetTimeOut() > 0 && runTimeSeconds.count() > m_currentTestInstance->GetTimeOut())
			{
				ReportNonCriticalError("Time out", "", 0);
				isDone = true;
			}

			if (isDone)
			{
				FinishTest();
			}
		}

		if (m_remainingTestFactories.empty() && !m_currentTestInstance)
		{
			if (auto pReporter = m_pReporter.lock())
				pReporter->OnFinishTesting(m_testContext, m_openReport);
			CryLogAlways("Running all tests done.");
			m_pTestRecordFile.reset();
			m_isRunningTest = false;
			RestoreAssertDialogSetting();
			if (m_wantQuitAfterTestsDone)
			{
				gEnv->pSystem->Quit();
			}
		}
	}
}

template<typename F> void CTestSystem::RunTestsFilter(F&& predicate)
{
	CRY_ASSERT(!m_isRunningTest);
	m_pTestRecordFile = stl::make_unique<CTestRecordFile>("%USER%/TestResults/TestRecord.txt");

	m_isRunningTest = true;                     //before spawning tests because we'd like to catch errors during test spawns too

	stl::optional<string> lastErrorTest = m_pTestRecordFile->GetLastTestName();

	//If the system doesn't inject a reporter, we create one
	if (!m_pReporter.lock())
	{
		m_pExcelReporter = std::make_shared<CTestExcelReporter>(m_log);
		m_pReporter = m_pExcelReporter;
	}
	m_testContext = {};

	if (auto pReporter = m_pReporter.lock())
		pReporter->OnStartTesting(m_testContext);

	auto testStartIter = m_testFactories.begin();
	if (lastErrorTest)
	{
		string lastErrorTestName = lastErrorTest.value();
		testStartIter = std::find_if(m_testFactories.begin(), m_testFactories.end(), [&lastErrorTestName](CTestFactory* pFactory)
		{
			return pFactory->GetTestInfo().name == lastErrorTestName;
		});

		STestInfo lastErrorTestInfo = (*testStartIter)->GetTestInfo();

		++testStartIter;   //bypasses the error test
		if (auto pReporter = m_pReporter.lock())
		{
			pReporter->RecoverTemporaryReport();

			//If the recovered report doesn't cover the unfinished test, it has crashed
			if (!pReporter->HasTest(lastErrorTestInfo))
			{
				std::vector<SError> errors;
				errors.push_back(SError{ "Incomplete from previous run", nullptr, 0 });
				pReporter->OnSingleTestFinish(lastErrorTestInfo, 0, false, errors);
			}
		}
		
	}

	for (auto iter = testStartIter; iter != m_testFactories.end(); ++iter)
	{
		if (predicate(*iter))
			m_remainingTestFactories.push(*iter);
	}
}

void CTestSystem::Run()
{
	CryLogAlways("Running all tests..."); //this gets output to main log. details are output to test log.
	SaveAndDisableAssertDialogSetting();
	RunTestsFilter([](CTestFactory*) { return true; });
}

void CTestSystem::RunSingle(const char* testName)
{
	CryLogAlways("Running test %s", testName); //this gets output to main log. details are output to test log.
	SaveAndDisableAssertDialogSetting();
	RunTestsFilter([testName](CTestFactory* pFactory) 
	{ 
		return stricmp(pFactory->GetTestInfo().name.c_str(), testName) == 0; 
	});
}

void CTestSystem::RunTestsByName(const DynArray<string>& names)
{
	CRY_ASSERT(!names.empty());
	CryLogAlways("Running following tests:");

#if !defined(EXCLUDE_NORMAL_LOG)
	for (const string& name : names)
	{
		CryLogAlways(name);
	}
#endif
	SaveAndDisableAssertDialogSetting();
	std::set<string> remainingNames;
	for (const string& name : names)
	{
		remainingNames.insert(name);
	}
	RunTestsFilter([&](CTestFactory* pFactory)
	{
		for (const string& name : remainingNames)
		{
			if (stricmp(pFactory->GetTestInfo().name.c_str(), name) == 0)
			{
				remainingNames.erase(name);
				return true;
			}
		}
		return false;
	});
}

bool CTestSystem::InitTest()
{
	if (auto pReporter = m_pReporter.lock())
		pReporter->OnSingleTestStart(m_currentTestInstance->GetTestInfo());
	m_testContext.testCount++;

	m_currentTestInstance->StartCountingCurrentTestTime();

	stl::optional<SError> err = TryRun([&]
		{
			m_currentTestInstance->Init();
			m_currentTestInstance->Run();
		});

	if (err)
	{
		m_testContext.failedTestCount++;
		m_currentTestInstance->AddFailure(err.value());
		return false;
	}

	return true;
}

void CTestSystem::FinishTest()
{
	bool failed = false;
	if (m_currentTestInstance->GetFailures().size())
	{
		m_testContext.failedTestCount++;
		failed = true;
	}
	else
	{
		m_testContext.succedTestCount++;
	}

	stl::optional<SError> err = TryRun([&]
		{
			m_currentTestInstance->Done();
		});

	std::chrono::duration<float, std::milli> runTime = m_currentTestInstance->GetCurrentTestElapsed();
	if (auto pReporter = m_pReporter.lock())
		pReporter->OnSingleTestFinish(m_currentTestInstance->GetTestInfo(), runTime.count(), !failed, m_currentTestInstance->GetFailures());
	m_currentTestInstance.reset();
}

void CTestSystem::SignalStopWork()
{
	m_wantQuit = true;
}

void CTestSystem::SaveAndDisableAssertDialogSetting()
{
#ifdef USE_CRY_ASSERT
	m_wasShowAssertDialog = Cry::Assert::ShowDialogOnAssert();
	Cry::Assert::ShowDialogOnAssert(false);
#endif
}

void CTestSystem::RestoreAssertDialogSetting()
{
#ifdef USE_CRY_ASSERT
	Cry::Assert::ShowDialogOnAssert(m_wasShowAssertDialog);
#endif
}

void CTestSystem::SetReporter(const std::shared_ptr<ITestReporter>& reporter)
{
	m_pReporter = reporter;
}

void CTestSystem::ThreadEntry()
{
	while (!m_wantQuit)
	{
		uint64 updateCounter = gEnv->pSystem->GetUpdateCounter();
		std::chrono::duration<float> timeSinceHeartBeat = std::chrono::system_clock::now() - m_heartBeatTime;
		if (m_currentTestInstance && updateCounter == m_lastUpdateCounter && timeSinceHeartBeat.count() > FreezeTimeOut)
		{
			//record timeout and quit
			ReportNonCriticalError("Time out", "", 0);
			std::chrono::duration<float, std::milli> runTime = m_currentTestInstance->GetCurrentTestElapsed();
			if (auto pReporter = m_pReporter.lock())
				pReporter->OnSingleTestFinish(m_currentTestInstance->GetTestInfo(), runTime.count(), false, m_currentTestInstance->GetFailures());

			SignalStopWork();
			gEnv->pSystem->Quit();
		}
		m_lastUpdateCounter = updateCounter;
		CrySleep(100);
	}
}

void CTestSystem::ReportCriticalError(const char* szExpression, const char* szFile, int line)
{
	if (m_isRunningTest)
	{
#ifdef USE_CRY_ASSERT
		// Reset assert flag to not block other updates depending on the flag, e.g. editor update,
		// in case we do not immediately quit after the test.
		// The flag has to be set here, since the majority of engine code does not use exceptions
		// but we are using exception or longjmp here, so it no longer returns to the assert caller
		// which would otherwise reset the flag.
		Cry::Assert::Detail::IsInAssert(false);
#endif
		m_currentTestInstance->AddFailure(SError{ szExpression, szFile, line });
#if defined(CRY_TESTING_USE_EXCEPTIONS)
		throw assert_exception(szExpression, szFile, line);
#else
		longjmp(*GetAssertJmpBuf(), 1);
#endif
	}
}

void CTestSystem::ReportNonCriticalError(const char* szExpression, const char* szFile, int line)
{
	// This can be called from CRY_ASSERT hook, so we must handle the case when 
	// m_currentTestInstance is not available
	if (m_isRunningTest && m_currentTestInstance != nullptr)
	{
		m_currentTestInstance->AddFailure(SError{ szExpression, szFile, line });
	}
}

const DynArray<CTestFactory*>& CTestSystem::GetFactories() const
{
	return m_testFactories;
}

void CTestSystem::AddFactory(CTestFactory* pFactory)
{
	pFactory->SetupAttributes();
	m_testFactories.push_back(pFactory);
	sCryTestArgAutoComplete.Add(pFactory);
}

void CTestSystem::OnAssert(const char* szCondition, const char* szMessage, const char* szFileName, unsigned int fileLineNumber)
{
	if (m_isRunningTest)
	{
		string cause;
		if (szMessage != nullptr && strlen(szMessage))
		{
			cause.Format("Assert: condition \"%s\" failed with \"%s\"", szCondition, szMessage);
		}
		else
		{
			cause.Format("Assert: \"%s\"", szCondition);
		}
		ReportNonCriticalError(cause.c_str(), szFileName, fileLineNumber);
	}
}

void CTestSystem::OnFatalError(const char* szMessage)
{
	if (m_isRunningTest)
	{
		string cause;
		cause.Format("Fatal Error: %s", szMessage);
		ReportCriticalError(cause.c_str(), "", 0);
	}
}

} // namespace CryTest
