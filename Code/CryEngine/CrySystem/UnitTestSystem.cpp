// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! Implementation of the CryEngine Unit Testing framework
//! Also contains core engine tests

#include "StdAfx.h"
#include "UnitTestSystem.h"
#include <CryString/StringUtils.h>  // cry_strXXX()
#include <CryCore/CryCustomTypes.h> // CRY_ARRAY_COUNT

#if !defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
	#include <setjmp.h>
#endif

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include "UnitTestExcelReporter.h"

using namespace CryUnitTest;

#if !defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
jmp_buf* GetAssertJmpBuf()
{
	static jmp_buf s_jmpBuf;
	return &s_jmpBuf;
}
#endif

struct SAutoTestsContext
{
	float               fStartTime     = 0;
	float               fCurrTime      = 0;
	string              sSuiteName;    
	string              sTestName;     
	int                 waitAfterMSec  = 0;
	int                 iter           = -1;
	int                 runCount       = 1;
	SUnitTestRunContext context;
};

//////////////////////////////////////////////////////////////////////////

CUnitTestManager::CUnitTestManager(ILog& logToUse)
	: m_log(logToUse)
{
	m_pAutoTestsContext = stl::make_unique<SAutoTestsContext>();
	CRY_ASSERT(m_pAutoTestsContext != nullptr);

	// Listen to asserts and fatal errors, turn them into unit test failures during the test
	if (!gEnv->pSystem->RegisterErrorObserver(this))
	{
		CRY_ASSERT_MESSAGE(false, "Unit test manager failed to register error system callback");
	}

	// Register tests defined in this module
	CreateTests("CrySystem");
}

//Empty destructor here to enable m_pAutoTestsContext forward declaration
CUnitTestManager::~CUnitTestManager()
{
	// Clean up listening to asserts and fatal errors
	if (!gEnv->pSystem->UnregisterErrorObserver(this))
	{
		CRY_ASSERT_MESSAGE(false, "Unit test manager failed to unregister error system callback");
	}
}

IUnitTest* CUnitTestManager::GetTestInstance(const CUnitTestInfo& info)
{
	for (auto& pt : m_tests)
	{
		if (pt->GetInfo() == info)
		{
			return pt.get();
		}
	}

	auto pTest = stl::make_unique<CUnitTest>(info);
	CUnitTest* pTestRaw = pTest.get();
	m_tests.push_back(std::move(pTest));
	return pTestRaw;
}

int CUnitTestManager::RunAllTests(EReporterType reporterType)
{
	CryLogAlways("Running all unit tests...");//this gets output to main log. details are output to test log.
	SUnitTestRunContext context;
	StartTesting(context, reporterType);

	for (auto& pt : m_tests)
	{
		RunTest(*pt, context);
	}

	EndTesting(context);
	CryLogAlways("Running all unit tests done.");
	return (context.failedTestCount == 0) ? 0 : 1; //non-zero for error exit code
}

void CUnitTestManager::RunAutoTests(const char* szSuiteName, const char* szTestName)
{
	// prepare auto tests context
	// tests actually will be called during Update call
	m_pAutoTestsContext->fStartTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
	m_pAutoTestsContext->fCurrTime = m_pAutoTestsContext->fStartTime;
	m_pAutoTestsContext->sSuiteName = szSuiteName;
	m_pAutoTestsContext->sTestName = szTestName;
	m_pAutoTestsContext->waitAfterMSec = 0;
	m_pAutoTestsContext->iter = 0;

	// Note this requires cvar "ats_loop" be defined to work.
	m_pAutoTestsContext->runCount = max(gEnv->pConsole->GetCVar("ats_loop")->GetIVal(), 1);

	StartTesting(m_pAutoTestsContext->context, EReporterType::Excel);
}

void CUnitTestManager::Update()
{
	if (m_pAutoTestsContext->iter != -1)
	{
		m_pAutoTestsContext->fCurrTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();

		if ((m_pAutoTestsContext->fCurrTime - m_pAutoTestsContext->fStartTime) > m_pAutoTestsContext->waitAfterMSec)
		{
			bool wasFound = false;

			for (size_t i = m_pAutoTestsContext->iter; i < m_tests.size(); i++)
			{
				if (IsTestMatch(*m_tests[i], m_pAutoTestsContext->sSuiteName, m_pAutoTestsContext->sTestName))
				{
					RunTest(*m_tests[i], m_pAutoTestsContext->context);

					const SAutoTestInfo& info = m_tests[i]->GetAutoTestInfo();
					m_pAutoTestsContext->waitAfterMSec = info.waitMSec;
					m_pAutoTestsContext->iter = i;

					if (info.runNextTest)
						m_pAutoTestsContext->iter++;

					wasFound = true;
					break;
				}
			}
			if (!wasFound)
			{
				// no tests were found so stop testing
				m_pAutoTestsContext->runCount--;

				if (0 == m_pAutoTestsContext->runCount)
				{
					EndTesting(m_pAutoTestsContext->context);
					m_pAutoTestsContext->iter = -1;

					if (gEnv->pConsole->GetCVar("ats_exit")->GetIVal())// Note this requires cvar "ats_exit" be defined to work.
						exit(0);
				}
				else
				{
					m_pAutoTestsContext->waitAfterMSec = 0;
					m_pAutoTestsContext->iter = 0;
				}
			}
			m_pAutoTestsContext->fStartTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
			m_pAutoTestsContext->fCurrTime = m_pAutoTestsContext->fStartTime;
		}
	}
}

void CUnitTestManager::StartTesting(SUnitTestRunContext& context, EReporterType reporterToUse)
{
	context.testCount = 0;
	context.failedTestCount = 0;
	context.succedTestCount = 0;

	switch (reporterToUse)
	{
	case EReporterType::Excel:
		context.pReporter = stl::make_unique<CUnitTestExcelReporter>(m_log);
		break;
	case EReporterType::ExcelWithNotification:
		context.pReporter = stl::make_unique<CUnitTestExcelNotificationReporter>(m_log);
		break;
	case EReporterType::Minimal:
		context.pReporter = stl::make_unique<CMinimalLogUnitTestReporter>(m_log);
		break;
	case EReporterType::Regular:
		context.pReporter = stl::make_unique<CLogUnitTestReporter>(m_log);
		break;
	default:
		CRY_ASSERT(false);
		break;
	}
	CRY_ASSERT(context.pReporter != nullptr);

	context.pReporter->OnStartTesting(context);
}

void CUnitTestManager::EndTesting(SUnitTestRunContext& context)
{
	context.pReporter->OnFinishTesting(context);
}

void CUnitTestManager::RunTest(CUnitTest& test, SUnitTestRunContext& context)
{
	bool bFail = false;

	test.Init();
	context.pReporter->OnSingleTestStart(test);
	context.testCount++;

	CTimeValue t0 = gEnv->pTimer->GetAsyncTime();

#if defined(CRY_UNIT_TESTING)
	m_bRunningTest = true;
	m_failureMsg.clear();

	#if !defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
	if (setjmp(*GetAssertJmpBuf()) == 0)
	#else
	try
	#endif
	{
		test.Run();
		context.succedTestCount++;
	}
	#if !defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
	else
	{
		context.failedTestCount++;
		bFail = true;
	}
	#else
	catch (CryUnitTest::assert_exception const& e)
	{
		context.failedTestCount++;
		bFail = true;
		m_failureMsg = e.what();

		// copy filename and line number of unit test assert
		// will be used later for test reporting
		test.m_info.SetFileName(e.m_filename);
		test.m_info.SetLineNumber(e.m_lineNumber);
	}
	catch (std::exception const& e)
	{
		context.failedTestCount++;

		bFail = true;
		m_failureMsg.Format("Unhandled exception: %s", e.what());
	}
	catch (...)
	{
		context.failedTestCount++;

		bFail = true;
		m_failureMsg = "Crash";
	}
	#endif

	m_bRunningTest = false;
#endif
	CTimeValue t1 = gEnv->pTimer->GetAsyncTime();

	float fRunTimeInMs = (t1 - t0).GetMilliSeconds();

	context.pReporter->OnSingleTestFinish(test, fRunTimeInMs, !bFail, m_failureMsg.c_str());

	test.Done();
}

void CUnitTestManager::SetExceptionCause(const char* szExpression, const char* szFile, int line)
{
	if (m_bRunningTest)
	{
#ifdef USE_CRY_ASSERT
		// Reset assert flag to not block other updates depending on the flag, e.g. editor update, 
		// in case we do not immediately quit after the unit test.
		// The flag has to be set here, since the majority of engine code does not use exceptions
		// but we are using exception or longjmp here, so it no longer returns to the assert caller
		// which would otherwise reset the flag.
		gEnv->stoppedOnAssert = false;
#endif

		m_failureMsg = szExpression;
#if defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
		throw CryUnitTest::assert_exception(szExpression, szFile, line);
#else
		longjmp(*GetAssertJmpBuf(), 1);
#endif
	}
}

bool CUnitTestManager::IsTestMatch(const CUnitTest& test, const string& sSuiteName, const string& sTestName) const
{
	bool isMatch = true; // by default test is match

	if (sSuiteName != "" && sSuiteName != "*")
	{
		isMatch &= (sSuiteName == test.GetInfo().GetSuite());
	}
	if (sTestName != "" && sTestName != "*")
	{
		isMatch &= (sTestName == test.GetInfo().GetName());
	}
	return isMatch;
}

void CryUnitTest::CUnitTestManager::OnAssert(const char* szCondition, const char* szMessage, const char* szFileName, unsigned int fileLineNumber)
{
	if (m_bRunningTest)
	{
		string sCause;
		if (szMessage != nullptr && strlen(szMessage))
		{
			sCause.Format("Assert: condition \"%s\" failed with \"%s\"", szCondition, szMessage);
		}
		else
		{
			sCause.Format("Assert: \"%s\"", szCondition);
		}
		SetExceptionCause(sCause.c_str(), szFileName, fileLineNumber);
	}
}

void CryUnitTest::CUnitTestManager::OnFatalError(const char* szMessage)
{
	if (m_bRunningTest)
	{
		string sCause;
		sCause.Format("Fatal Error: %s", szMessage);
		SetExceptionCause(sCause.c_str(), "", 0);
	}
}

//////////////////////////////////////////////////////////////////////////

void CryUnitTest::CLogUnitTestReporter::OnStartTesting(const SUnitTestRunContext& context)
{
	m_log.Log("UnitTesting Started");
}

void CryUnitTest::CLogUnitTestReporter::OnFinishTesting(const SUnitTestRunContext& context)
{
	m_log.Log("UnitTesting Finished");
}

void CryUnitTest::CLogUnitTestReporter::OnSingleTestStart(const IUnitTest& test)
{
	auto& info = test.GetInfo();
	m_log.Log("UnitTestStart:  [%s]%s:%s", info.GetModule(), info.GetSuite(), info.GetName());
}

void CryUnitTest::CLogUnitTestReporter::OnSingleTestFinish(const IUnitTest& test, float fRunTimeInMs, bool bSuccess, char const* szFailureDescription)
{
	auto& info = test.GetInfo();
	if (bSuccess)
		m_log.Log("UnitTestFinish: [%s]%s:%s | OK (%3.2fms)", info.GetModule(), info.GetSuite(), info.GetName(), fRunTimeInMs);
	else
		m_log.Log("UnitTestFinish: [%s]%s:%s | FAIL (%s)", info.GetModule(), info.GetSuite(), info.GetName(), szFailureDescription);
}

//////////////////////////////////////////////////////////////////////////

void CryUnitTest::CMinimalLogUnitTestReporter::OnStartTesting(const SUnitTestRunContext& context)
{
	m_nRunTests = 0;
	m_nSucceededTests = 0;
	m_nFailedTests = 0;
	m_fTimeTaken = 0.f;
}

void CryUnitTest::CMinimalLogUnitTestReporter::OnFinishTesting(const SUnitTestRunContext& context)
{
	m_log.Log("UnitTesting Finished Tests: %d Succeeded: %d, Failed: %d, Time: %5.2f ms", m_nRunTests, m_nSucceededTests, m_nFailedTests, m_fTimeTaken);
}

void CryUnitTest::CMinimalLogUnitTestReporter::OnSingleTestFinish(const IUnitTest& test, float fRunTimeInMs, bool bSuccess, char const* szFailureDescription)
{
	++m_nRunTests;
	if (bSuccess)
		++m_nSucceededTests;
	else
		++m_nFailedTests;
	m_fTimeTaken += fRunTimeInMs;

	if (!bSuccess)
	{
		auto& info = test.GetInfo();
		m_log.Log("-- FAIL (%s): [%s]%s:%s (%s:%d)", szFailureDescription, info.GetModule(), info.GetSuite(), info.GetName(), info.GetFileName(), info.GetLineNumber());
	}
}
