// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! Implementation of the CryEngine Unit Testing framework

#pragma once

#include <CrySystem/CryUnitTest.h>
#include <CrySystem/ILog.h>
#include <CryString/CryString.h>

struct SAutoTestsContext;

namespace CryUnitTest
{

class CUnitTest final : public IUnitTest
{
public:
	CUnitTest(CUnitTestInfo info) : m_info(std::move(info)) {}
	virtual const CUnitTestInfo& GetInfo() const override { return m_info; };
	virtual const SAutoTestInfo& GetAutoTestInfo() const override
	{
		return m_info.GetTest().m_autoTestInfo;
	}
	virtual void Init() override
	{
		m_info.GetTest().Init();
	};
	virtual void Run() override
	{
		m_info.GetTest().Run();
	};
	virtual void Done() override
	{
		m_info.GetTest().Done();
	};

	CUnitTestInfo m_info;
};

class CLogUnitTestReporter : public IUnitTestReporter
{
public:
	CLogUnitTestReporter(ILog& log) : m_log(log) {}
private:
	ILog& m_log;
private:
	virtual void OnStartTesting(const SUnitTestRunContext& context) override;
	virtual void OnFinishTesting(const SUnitTestRunContext& context) override;
	virtual void OnSingleTestStart(const IUnitTest& test) override;
	virtual void OnSingleTestFinish(const IUnitTest& test, float fRunTimeInMs, bool bSuccess, char const* szFailureDescription) override;
};

class CMinimalLogUnitTestReporter : public IUnitTestReporter
{
public:
	CMinimalLogUnitTestReporter(ILog& log) : m_log(log) {}
private:
	ILog& m_log;
	int   m_nRunTests = 0;
	int   m_nSucceededTests = 0;
	int   m_nFailedTests = 0;
	float m_fTimeTaken = 0.f;

private:
	virtual void OnStartTesting(const SUnitTestRunContext& context) override;
	virtual void OnFinishTesting(const SUnitTestRunContext& context) override;
	virtual void OnSingleTestStart(const IUnitTest& test) override {}
	virtual void OnSingleTestFinish(const IUnitTest& test, float fRunTimeInMs, bool bSuccess, char const* szFailureDescription) override;
};

class CUnitTestManager : public IUnitTestManager, IErrorObserver
{
public:
	CUnitTestManager(ILog& logToUse);
	virtual ~CUnitTestManager();

public:
	virtual IUnitTest* GetTestInstance(const CUnitTestInfo& info) override;
	virtual int        RunAllTests(EReporterType reporterType) override;
	virtual void       RunAutoTests(const char* szSuiteName, const char* szTestName) override; //!< Currently not in use
	virtual void       Update() override;                                                      //!< Currently not in use

	virtual void       SetExceptionCause(const char* szExpression, const char* szFile, int line) override;
private:
	void               StartTesting(SUnitTestRunContext& context, EReporterType reporterToUse);
	void               EndTesting(SUnitTestRunContext& context);
	void               RunTest(CUnitTest& test, SUnitTestRunContext& context);
	bool               IsTestMatch(const CUnitTest& Test, const string& sSuiteName, const string& sTestName) const;

	//! Implement IErrorObserver
	virtual void       OnAssert(const char* szCondition, const char* szMessage, const char* szFileName, unsigned int fileLineNumber) override;
	virtual void       OnFatalError(const char* szMessage) override;

private:
	ILog&                                   m_log;
	std::vector<std::unique_ptr<CUnitTest>> m_tests;
	string                                  m_failureMsg;
	std::unique_ptr<SAutoTestsContext>      m_pAutoTestsContext;
	bool m_bRunningTest = false;
};
} // namespace CryUnitTest
