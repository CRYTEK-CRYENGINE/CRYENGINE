// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>

// -----------------------------------------------------------------------------

CryUnit::ITestCase* DummyTestFactoryFunction()
{
	return 0;
}

CRY_TEST_WITH_FIXTURE(TestTestInfoName, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo("TestName", 0, 0, false, 0, 0);

	ASSERT_ARE_EQUAL("TestName", testInfo.Name);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoFixture, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, "FixtureName", 0, false, 0, 0);

	ASSERT_ARE_EQUAL("FixtureName", testInfo.Fixture);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoTestFactory, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, DummyTestFactoryFunction, false, 0, 0);

	ASSERT_ARE_EQUAL((const void*)DummyTestFactoryFunction, (const void*)testInfo.TestFactory);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoEnabled, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, true, 0, 0);

	ASSERT_IS_TRUE(testInfo.Enabled);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoFileName, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, "FileName", 0);

	ASSERT_ARE_EQUAL("FileName", testInfo.FileName);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoFileLineNumber, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 1);

	ASSERT_ARE_EQUAL(1, testInfo.FileLineNumber);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoNameNull, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 0);

	ASSERT_IS_NULL(testInfo.Name);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoFixtureNull, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 0);

	ASSERT_IS_NULL(testInfo.Fixture);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoTestFactoryNull, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 0);

	ASSERT_IS_NULL((const void*)testInfo.TestFactory);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoDisabled, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 0);

	ASSERT_IS_FALSE(testInfo.Enabled);
}

CRY_TEST_WITH_FIXTURE(TestTestInfoFileNameNULL, CTestSuiteTestFixture)
{
	CryUnit::STestInfo testInfo(0, 0, 0, false, 0, 0);

	ASSERT_IS_NULL(testInfo.FileName);
}
