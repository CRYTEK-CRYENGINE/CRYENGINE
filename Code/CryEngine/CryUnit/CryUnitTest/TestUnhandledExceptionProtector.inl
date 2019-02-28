// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../CryUnitString.h"

class TestListener : public CryUnit::ITestListener
{
public:
	TestListener()
		: m_testFailedCalled(false)
	{

	}

	virtual void TestSuiteRun(const CryUnit::ITestSuite& testSuite) {}
	virtual void TestSuiteEnd(const CryUnit::ITestSuite& testSuite) {}
	virtual void StartTestRun(const CryUnit::STestInfo& testInfo)   {}
	virtual void TestRun(const CryUnit::STestInfo& testInfo)        {}
	virtual void TestPassed(const CryUnit::STestInfo& testInfo)     {}
	virtual void TestSkipped(const CryUnit::STestInfo& testInfo)    {}

	virtual void TestFailed(const CryUnit::STestInfo& testInfo, const CryUnit::SFailureInfo& failureInfo)
	{
		m_testFailedCalled = true;
		m_testName.Assign(testInfo.Name);
		m_message.Assign(failureInfo.Message);
		m_fileName.Assign(failureInfo.FileName);
		m_line = failureInfo.FileLineNumber;
	}

	const bool TestFailedCalled() const
	{
		return m_testFailedCalled;
	}

	const char* TestName() const
	{
		return m_testName.GetInternalData();
	}

	const char* Message() const
	{
		return m_message.GetInternalData();
	}

	const char* FileName() const
	{
		return m_fileName.GetInternalData();
	}

	const int Line() const
	{
		return m_line;
	}

private:
	bool            m_testFailedCalled;
	CryUnit::String m_testName;
	CryUnit::String m_message;
	CryUnit::String m_fileName;
	int             m_line;
};

class ThrowingExceptionProtector : public CryUnit::UnhandledExceptionProtector
{
public:
	ThrowingExceptionProtector(const CryUnit::STestInfo& testInfo, CryUnit::ITestListener& listener, const char* message)
		: CryUnit::UnhandledExceptionProtector(testInfo, listener, message)
	{

	}

	virtual void Protected()
	{
#ifndef PS3
		throw "bye!";
#endif
	}
};

class NotThrowingExceptionProtector : public CryUnit::UnhandledExceptionProtector
{
public:
	NotThrowingExceptionProtector(const CryUnit::STestInfo& testInfo, CryUnit::ITestListener& listener, const char* message)
		: CryUnit::UnhandledExceptionProtector(testInfo, listener, message)
		, m_called(false)
	{

	}

	virtual void Protected()
	{
		m_called = true;
	}

	const bool Called() const
	{
		return m_called;
	}

private:
	bool m_called;
};

CRY_TEST_WITH_FIXTURE(TestWhenUnhandledExceptionProtectorCatches, TestCryUnitFixture)
{
	TestListener listener;

	ThrowingExceptionProtector throwingExceptionProtector(CreateEmptyTestInfo(), listener, "");
	throwingExceptionProtector.Execute();

	ASSERT_IS_TRUE(listener.TestFailedCalled());
}

CRY_TEST_WITH_FIXTURE(TestParametersWhenUnhandledExceptionProtectorCatches, TestCryUnitFixture)
{
	CryUnit::STestInfo testInfo("TestName", "", NULL, true, "FileName", 12);
	TestListener listener;

	ThrowingExceptionProtector throwingExceptionProtector(testInfo, listener, "Message");
	throwingExceptionProtector.Execute();

	ASSERT_ARE_EQUAL(testInfo.Name, listener.TestName());
	ASSERT_ARE_EQUAL(testInfo.FileName, listener.FileName());
	ASSERT_ARE_EQUAL(testInfo.FileLineNumber, listener.Line());
}

CRY_TEST_WITH_FIXTURE(TestUnhandledExceptionProtectorNotCrashing, TestCryUnitFixture)
{
	TestListener listener;

	NotThrowingExceptionProtector notThrowingExceptionProtector(CreateEmptyTestInfo(), listener, "");
	notThrowingExceptionProtector.Execute();

	ASSERT_IS_FALSE(listener.TestFailedCalled());
}

CRY_TEST_WITH_FIXTURE(TestUnhandledExceptionProtectorProtectedMethodCalled, TestCryUnitFixture)
{
	TestListener listener;

	NotThrowingExceptionProtector notThrowingExceptionProtector(CreateEmptyTestInfo(), listener, "");
	notThrowingExceptionProtector.Execute();

	ASSERT_IS_TRUE(notThrowingExceptionProtector.Called());
}
#ifndef PS3
CRY_TEST_WITH_FIXTURE(TestUnhandledExceptionProtectorExcecuteDisabled, TestCryUnitFixture)
{
	TestListener listener;
	ThrowingExceptionProtector throwingExceptionProtector(CreateEmptyTestInfo(), listener, "");
	bool exceptionThrown = false;
	try
	{
		throwingExceptionProtector.Execute();
	}
	catch (...)
	{
		exceptionThrown = true;
	}

	ASSERT_IS_FALSE(exceptionThrown);
	ASSERT_IS_TRUE(listener.TestFailedCalled());
}
#endif
