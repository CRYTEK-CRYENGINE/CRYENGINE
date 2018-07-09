// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "TestSuiteTestFixture.h"
#include "../CryUnitString.h"

CRY_TEST_FIXTURE(CTestListenerFixture, CTestSuiteTestFixtureBase, TestCryUnitSuite)
{
	class CMockTestListener : public CryUnit::ITestListener
	{
	public:
		CMockTestListener()
			: m_TestSuiteIsRun(false)
			, m_TestSuiteEndIsCalled(false)
			, m_Line(0)
			, m_StartRunTestInfo(CreateEmptyTestInfo())
			, m_RunTestInfo(CreateEmptyTestInfo())
			, m_PassedTestInfo(CreateEmptyTestInfo())
			, m_FailedTestInfo(CreateEmptyTestInfo())
			, m_TestSkipped(CreateEmptyTestInfo())
		{
		}

		virtual void TestSuiteRun(const CryUnit::ITestSuite& testSuite)
		{
			m_TestSuiteIsRun = true;
			m_TestSuiteRunName.Assign(testSuite.GetName());
		}

		virtual void TestSuiteEnd(const CryUnit::ITestSuite& testSuite)
		{
			m_TestSuiteEndIsCalled = true;
		}

		virtual void StartTestRun(const CryUnit::STestInfo& testInfo)
		{
			m_StartRunTestInfo = testInfo;
		}

		virtual void TestRun(const CryUnit::STestInfo& testInfo)
		{
			m_RunTestInfo = testInfo;
		}

		virtual void TestPassed(const CryUnit::STestInfo& testInfo)
		{
			m_PassedTestInfo = testInfo;
		}

		virtual void TestFailed(const CryUnit::STestInfo& testInfo, const CryUnit::SFailureInfo& failureInfo)
		{
			m_FailedTestInfo = testInfo;
			m_ErrorMessage.Assign(failureInfo.Message);
			m_FileName.Assign(failureInfo.FileName);
			m_Line = failureInfo.FileLineNumber;
		}

		virtual void TestSkipped(const CryUnit::STestInfo& testInfo)
		{
			m_TestSkipped = testInfo;
		}

		bool               m_TestSuiteIsRun;
		bool               m_TestSuiteEndIsCalled;

		CryUnit::String    m_TestSuiteRunName;
		CryUnit::String    m_ErrorMessage;
		CryUnit::String    m_FileName;

		int                m_Line;

		CryUnit::STestInfo m_StartRunTestInfo;
		CryUnit::STestInfo m_RunTestInfo;
		CryUnit::STestInfo m_PassedTestInfo;
		CryUnit::STestInfo m_FailedTestInfo;
		CryUnit::STestInfo m_TestSkipped;
	};

public:
	virtual void SetUp()
	{
		m_listener = new CMockTestListener();
	}

	virtual void TearDown()
	{
		delete m_listener;
	}

	virtual CMockTestListener& GetListener()
	{
		return *m_listener;
	}

	CMockTestListener* m_listener;
};

// -----------------------------------------------------------------------------

CRY_TEST_WITH_FIXTURE(GetNameFromListenerForPassingTest, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(CPassingDummyTest::Info().Name, GetListener().m_PassedTestInfo.Name);
}

CRY_TEST_WITH_FIXTURE(GetNameFromListenerForRunTest, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(CPassingDummyTest::Info().Name, GetListener().m_RunTestInfo.Name);
}

CRY_TEST_WITH_FIXTURE(GetNameFromListenerForFailedTest, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(GetListener().m_FailedTestInfo.Name, CFailingDummyTest::Info().Name);
}

CRY_TEST_WITH_FIXTURE(TestSuiteStartRunIsNotifiedToListener, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(GetListener().m_StartRunTestInfo.Name, CFailingDummyTest::Info().Name);
}

CRY_TEST_WITH_FIXTURE(TestSuiteRunIsNotifiedToListener, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());
	RunTests();

	ASSERT_IS_TRUE(GetListener().m_TestSuiteIsRun);
}

CRY_TEST_WITH_FIXTURE(GetErrorMessageFromListenerForFailedTest, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CFailingEqualityDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(GetListener().m_ErrorMessage.GetInternalData(), "AssertAreEqual failed: Expected: '2', got: '3'");
}

CRY_TEST_WITH_FIXTURE(GetFileNameFromListenerForFailedTest, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CFailingEqualityDummyTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(GetListener().m_FileName.GetInternalData(), "file");
	ASSERT_ARE_EQUAL(GetListener().m_Line, 10);
}

CRY_TEST_WITH_FIXTURE(TestSuiteEndIsNotifiedFromListener, CTestListenerFixture)
{
	RunTests();
	ASSERT_IS_TRUE(GetListener().m_TestSuiteEndIsCalled);
}

CRY_TEST_WITH_FIXTURE(GetTestSuiteNameFromListener, CTestListenerFixture)
{
	RunTests();
	ASSERT_ARE_EQUAL(GetListener().m_TestSuiteRunName.GetInternalData(), m_TestSuite.GetName());
}

CRY_TEST_WITH_FIXTURE(TestSuiteRunTestSkippedIsNotifiedToListener, CTestListenerFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyDisabledTest::Info());
	RunTests();

	ASSERT_ARE_EQUAL(CPassingDummyDisabledTest::Info().Name, GetListener().m_TestSkipped.Name);
}
