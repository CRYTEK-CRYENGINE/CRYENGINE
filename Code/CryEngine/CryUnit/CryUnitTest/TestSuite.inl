// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../TestFailedException.h"

// -----------------------------------------------------------------------------

CRY_TEST_WITH_FIXTURE(TestSuiteRegisterOneTest, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	m_TestSuite.Run(*m_listener);

	ASSERT_ARE_EQUAL(m_TestSuite.GetNumberOfTests(), 1u);
}

CRY_TEST_WITH_FIXTURE(TestSuiteRegisterTwoTests, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(m_TestSuite.GetNumberOfTests(), 2u);
}

CRY_TEST_WITH_FIXTURE(TestTestRun, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(GetListener().numberOfTestsRun, 1);
}

CRY_TEST_WITH_FIXTURE(TestSuiteRunTwoSuccessfulTests, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(GetListener().numberOfTestsPassed, 2);
}

CRY_TEST_WITH_FIXTURE(TestSuiteRunOneSuccessfulTestOneFailingTest, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyTest::Info());
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(GetListener().numberOfTestsRun, 2);
	ASSERT_ARE_EQUAL(GetListener().numberOfTestsPassed, 1);
	ASSERT_ARE_EQUAL(GetListener().numberOfTestsFailed, 1);
}

CRY_TEST_WITH_FIXTURE(TestSuiteGetFailingTestName, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(GetListener().numberOfTestsFailed, 1);
	ASSERT_ARE_EQUAL(GetListener().testFailedNames[0].GetInternalData(), "FailingDummyTest");
}

CRY_TEST_WITH_FIXTURE(TestSuiteGetTwoFailingTestNames, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());
	m_TestSuite.RegisterTestCase(CFailingDummyTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(GetListener().numberOfTestsFailed, 2);
	ASSERT_ARE_EQUAL(GetListener().testFailedNames[0].GetInternalData(), "FailingDummyTest");
	ASSERT_ARE_EQUAL(GetListener().testFailedNames[1].GetInternalData(), "FailingDummyTest");
}

CRY_TEST_WITH_FIXTURE(TestSuiteTypeIsUnitTest, TestCryUnitFixture)
{
	CryUnit::ITestSuite* testSuite = CryUnit::ITestSuite::Create("name", CryUnit::ITestSuite::UNIT_TEST);

	ASSERT_ARE_EQUAL(testSuite->GetType(), CryUnit::ITestSuite::UNIT_TEST);

	testSuite->Release();
}

CRY_TEST_WITH_FIXTURE(TestSuiteTypeIsSmokeTest, TestCryUnitFixture)
{
	CryUnit::ITestSuite* testSuite = CryUnit::ITestSuite::Create("name", CryUnit::ITestSuite::SMOKE_TEST);

	ASSERT_ARE_EQUAL(testSuite->GetType(), CryUnit::ITestSuite::SMOKE_TEST);

	testSuite->Release();
}

// -----------------------------------------------------------------------------

class CSetUpTearDownDummyTest : public TestCaseDummyBase<CSetUpTearDownDummyTest>
{
public:
	static const char* GetTestName()
	{
		return "SetUpTearDownDummyTest";
	}

	void SetUp()
	{
		m_isSetupExectuted = true;
	}

	void TearDown()
	{
		m_isTearDownExectuted = true;
	}

	virtual void TestBody()
	{
		CryUnit::AssertIsTrue(true, "", 0);
	}

	static bool m_isSetupExectuted;
	static bool m_isTearDownExectuted;
};

bool CSetUpTearDownDummyTest::m_isSetupExectuted = false;
bool CSetUpTearDownDummyTest::m_isTearDownExectuted = false;

CRY_TEST_WITH_FIXTURE(TestSetupIsExecuted, CTestSuiteTestFixture)
{
	CSetUpTearDownDummyTest::m_isSetupExectuted = false;
	CSetUpTearDownDummyTest::m_isTearDownExectuted = false;

	m_TestSuite.RegisterTestCase(CSetUpTearDownDummyTest::Info());
	RunTests();

	ASSERT_IS_TRUE(CSetUpTearDownDummyTest::m_isSetupExectuted);
};

CRY_TEST_WITH_FIXTURE(TestTearDownIsExecuted, CTestSuiteTestFixture)
{
	CSetUpTearDownDummyTest::m_isSetupExectuted = false;
	CSetUpTearDownDummyTest::m_isTearDownExectuted = false;

	m_TestSuite.RegisterTestCase(CSetUpTearDownDummyTest::Info());
	RunTests();

	ASSERT_IS_TRUE(CSetUpTearDownDummyTest::m_isTearDownExectuted);
};

CRY_TEST_WITH_FIXTURE(TestFindTest, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CSetUpTearDownDummyTest::Info());
	ASSERT_IS_NOT_NULL(m_TestSuite.GetTestInfoByName(CSetUpTearDownDummyTest::Info().Name))
};

CRY_TEST_WITH_FIXTURE(TestFindTestFails, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CSetUpTearDownDummyTest::Info());
	ASSERT_IS_NULL(m_TestSuite.GetTestInfoByName("NonValidTestCaseName"));
};

CRY_TEST_WITH_FIXTURE(TestTestRunWhenTestDisabled, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CPassingDummyDisabledTest::Info());

	RunTests();

	ASSERT_ARE_EQUAL(0, GetListener().numberOfTestsRun);
	ASSERT_ARE_EQUAL(1, GetListener().numberOfTestsSkipped);
}

template<typename Derived>
class CrashingTestBase : public TestCaseDummyBase<Derived>
{
public:
	static const char* GetTestName()
	{
		return "CrashingTest";
	}

	void ThrowException()
	{
#ifndef PS3
		throw "Bye!";
#endif
	}

	virtual void TestBody() {}
};

class CrashingTest : public CrashingTestBase<CrashingTest>
{
public:
	virtual void TestBody()
	{
		ThrowException();
	}
};

class CrashingSetupTest : public CrashingTestBase<CrashingSetupTest>
{
public:
	virtual void SetUp()
	{
		ThrowException();
	}
};

class CrashingTearDownTest : public CrashingTestBase<CrashingTearDownTest>
{
public:
	virtual void TearDown()
	{
		ThrowException();
	}
};

class CrashingCtorTest : public CrashingTestBase<CrashingCtorTest>
{
public:
	CrashingCtorTest()
	{
		ThrowException();
	}
};

CRY_TEST_WITH_FIXTURE(TestCrashingTestCatched, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CrashingTest::Info());

	RunTests();
	ASSERT_ARE_EQUAL("Unhandled exception", m_listener->testFailedErrorMessage[0].GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestCrashingSetupCatched, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CrashingSetupTest::Info());

	RunTests();
	ASSERT_ARE_EQUAL("Unhandled exception", m_listener->testFailedErrorMessage[0].GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestCrashingTearDownCatched, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CrashingTearDownTest::Info());

	RunTests();
	ASSERT_ARE_EQUAL("TearDown failed", m_listener->testFailedErrorMessage[0].GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestCrashingCTorDownCatched, CTestSuiteTestFixture)
{
	m_TestSuite.RegisterTestCase(CrashingCtorTest::Info());

	RunTests();
	ASSERT_ARE_EQUAL("CreateTest failed", m_listener->testFailedErrorMessage[0].GetInternalData());
}

#ifndef PS3
CRY_TEST_WITH_FIXTURE(TestCrashingTestProtectorDisabled, CTestSuiteTestFixture)
{
	bool exceptionThrown = false;
	try
	{
		m_TestSuite.DisableProtector();
		m_TestSuite.RegisterTestCase(CrashingTest::Info());

		RunTests();
	}
	catch (...)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestCrashingSetupProtectorDisabled, CTestSuiteTestFixture)
{
	bool exceptionThrown = false;
	try
	{
		m_TestSuite.DisableProtector();
		m_TestSuite.RegisterTestCase(CrashingSetupTest::Info());

		RunTests();
	}
	catch (...)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestCrashingTearDownProtectorDisabled, CTestSuiteTestFixture)
{
	bool exceptionThrown = false;
	try
	{
		m_TestSuite.DisableProtector();
		m_TestSuite.RegisterTestCase(CrashingTearDownTest::Info());

		RunTests();
	}
	catch (...)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestCrashingCTorDownProtectorDisabled, CTestSuiteTestFixture)
{
	bool exceptionThrown = false;
	try
	{
		m_TestSuite.DisableProtector();
		m_TestSuite.RegisterTestCase(CrashingCtorTest::Info());

		RunTests();
	}
	catch (...)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}
#endif
