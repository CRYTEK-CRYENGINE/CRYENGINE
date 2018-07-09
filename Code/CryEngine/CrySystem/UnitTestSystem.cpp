// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! Implementation of the CryEngine Unit Testing framework
//! Also contains core engine tests

#include "StdAfx.h"
#include "UnitTestSystem.h"
#include <CryMemory/HeapAllocator.h>
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

//////////////////////////////////////////////////////////////////////////

#if defined(CRY_UNIT_TESTING)

CRY_UNIT_TEST_SUITE(BITFIDDLING)
{

	CRY_UNIT_TEST(CUT_BITFIDDLING)
	{
		for (uint32 i = 0, j = ~0U; i < 32; ++i, j >>= 1)
			CRY_UNIT_TEST_ASSERT(countLeadingZeros32(j) == i);
		for (uint32 i = 0, j = ~0U; i < 32; ++i, j <<= 1)
			CRY_UNIT_TEST_ASSERT(countTrailingZeros32(j) == i);

		for (uint64 i = 0, j = ~0ULL; i < 64; ++i, j >>= 1)
			CRY_UNIT_TEST_ASSERT(countLeadingZeros64(j) == i);
		for (uint64 i = 0, j = ~0ULL; i < 64; ++i, j <<= 1)
			CRY_UNIT_TEST_ASSERT(countTrailingZeros64(j) == i);

		for (uint8 i = 7, j = 0x80; i > 0; --i, j >>= 1)
		{
			CRY_UNIT_TEST_ASSERT(IntegerLog2        (uint8(j           )) ==  i     );
			CRY_UNIT_TEST_ASSERT(IntegerLog2        (uint8(j + (j >> 1))) ==  i     );
			CRY_UNIT_TEST_ASSERT(IntegerLog2_RoundUp(uint8(j           )) ==  i     );
			CRY_UNIT_TEST_ASSERT(IntegerLog2_RoundUp(uint8(j + (j >> 1))) == (i + 1));
		}
	}
}

CRY_UNIT_TEST_SUITE(Math)
{
	using namespace crymath;
	const float HALF_EPSILON = 0.00025f;

	CRndGen MathRand;  // Static generator for reproducible sequence

	float RandomExp(float fMaxExp)
	{
		float f = pow(10.0f, MathRand.GetRandom(-fMaxExp, fMaxExp));
		if (MathRand.GetRandom(0, 1))
			f = -f;
		assert(IsValid(f));
		return f;
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real), S in, S res)
	{
		Real out = func(convert<Real>(in));
		CRY_UNIT_TEST_ASSERT(All(out == convert<Real>(res)));
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real, Real), S a, S b, S res)
	{
		Real out = func(convert<Real>(a), convert<Real>(b));
		CRY_UNIT_TEST_ASSERT(All(out == convert<Real>(res)));
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real, Real, Real), S a, S b, S c, S res)
	{
		Real out = func(convert<Real>(a), convert<Real>(b), convert<Real>(c));
		CRY_UNIT_TEST_ASSERT(All(out == convert<Real>(res)));
	}

	template<typename Real, typename S>
	bool IsEquiv(Real a, Real b, S epsilon)
	{
		return NumberValid(a) && NumberValid(b) && IsEquivalent(a, b, -epsilon); // Relative comparison
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real), S in, S res, S epsilon)
	{
		Real out = func(convert<Real>(in));
		CRY_UNIT_TEST_ASSERT(IsEquiv(out, convert<Real>(res), epsilon));
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real, Real), S a, S b, S res, S epsilon)
	{
		Real out = func(convert<Real>(a), convert<Real>(b));
		CRY_UNIT_TEST_ASSERT(IsEquiv(out, convert<Real>(res), epsilon));
	}

	template<typename Real, typename S>
	void TestFunction(Real (* func)(Real, Real, Real), S a, S b, S c, S res, S epsilon)
	{
		Real out = func(convert<Real>(a), convert<Real>(b), convert<Real>(c));
		CRY_UNIT_TEST_ASSERT(IsEquiv(out, convert<Real>(res), epsilon));
	}

	template<typename Real, typename S>
	void FunctionApproxTest(Real (* func)(Real), Real (* func_fast)(Real), S in, S res)
	{
		TestFunction(func, in, res, FLT_EPSILON * 2.0f);
		TestFunction(func_fast, in, res, HALF_EPSILON * 2.0f);
	}

	template<typename Real, typename Int, typename Uint>
	void FunctionTest()
	{
		// Check trunc, floor, ceil
		float fdata[] = { 0.0f, 257.85f, 124.35f, 73.0f, 0.806228399f, 0.2537399f, -0.123432f, -0.986552f, -5.0f, -63.4f, -102.7f, 1e-18f, -4.567e8f };

		for (float f : fdata)
		{
			float res;

			res = float(int(f));
			TestFunction<Real>(trunc, f, res);

			res = std::floor(f);
			TestFunction<Real>(floor, f, res);

			res = std::ceil(f);
			TestFunction<Real>(ceil, f, res);

			res = f<0.0f ? 0.0f : f> 1.0f ? 1.0f : f;
			TestFunction<Real>(saturate, f, res);

			res = f < 0.0f ? -f : f;
			TestFunction<Real>(abs, f, res);

			res = f < 0.0f ? -1.0f : 1.0f;
			TestFunction<Real>(signnz, f, res);

			res = f<0.0f ? -1.0f : f> 0.0f ? 1.0f : 0.0f;
			TestFunction<Real>(sign, f, res);

			FunctionApproxTest<Real>(sqrt, sqrt_fast, f * f, abs(f));

			if (f != 0.0f)
			{
				FunctionApproxTest<Real>(rcp, rcp_fast, f, 1.0f / f);
				FunctionApproxTest<Real>(rsqrt, rsqrt_fast, 1.0f / (f * f), abs(f));
			}
		}

		TestFunction<Real>(max, 257.35f, 510.0f, 510.0f);
		TestFunction<Real>(max, -102.0f, -201.4f, -102.0f);

		TestFunction<Real>(mod, 3.0f, 1.0f, 0.0f, FLT_EPSILON * 4);
		TestFunction<Real>(mod, 0.75f, 0.5f, 0.25f, FLT_EPSILON * 4);
		TestFunction<Real>(mod, -0.6f, 0.5f, -0.1f, FLT_EPSILON * 4);
		TestFunction<Real>(mod, -0.2f, -0.3f, -0.2f, FLT_EPSILON * 4);

		TestFunction<Real>(wrap, -6.0f, -1.0f, 3.0f, 2.0f, FLT_EPSILON * 4);
		TestFunction<Real>(wrap, -6.3f, 0.0f, 1.0f, 0.7f, FLT_EPSILON * 4);
		TestFunction<Real>(wrap, 4.8f, -1.0f, 1.0f, 0.8f, FLT_EPSILON * 4);

		TestFunction<Int>(min, -3, 1, -3);
		TestFunction<Int>(max, -3, 1, 1);

		TestFunction<Uint>(min, 8u, 3u, 3u);
		TestFunction<Uint>(min, 8u, 0x9FFFFFFFu, 8u);
		TestFunction<Uint>(max, 8u, 0x9FFFFFFFu, 0x9FFFFFFFu);
		TestFunction<Uint>(max, 0x90000000u, 0xA0000000u, 0xA0000000u);
	}

	#if CRY_PLATFORM_SSE2

	template<typename V>
	void ConvertTest()
	{
		using T = scalar_t<V>;
		V v = convert<V>(T(0), T(1), T(2), T(3));
		CRY_UNIT_TEST_ASSERT(NumberValid(v));
		CRY_UNIT_TEST_ASSERT(get_element<0>(v) == T(0));
		CRY_UNIT_TEST_ASSERT(get_element<3>(v) == T(3));
		v = convert<V>(T(1), 0);
		CRY_UNIT_TEST_ASSERT(All(v == convert<V>(T(1), T(0), T(0), T(0))));
	}

		#define ELEMENT_OPERATE(V, a, op, b) convert<V>( \
		  get_element<0>(a) op get_element<0>(b),        \
		  get_element<1>(a) op get_element<1>(b),        \
		  get_element<2>(a) op get_element<2>(b),        \
		  get_element<3>(a) op get_element<3>(b))        \

	template<typename V>
	void TestIntOperators(V a, V b)
	{
		CRY_UNIT_TEST_ASSERT(All((a + b) == ELEMENT_OPERATE(V, a, +, b)));
		CRY_UNIT_TEST_ASSERT(All((a - b) == ELEMENT_OPERATE(V, a, -, b)));
		CRY_UNIT_TEST_ASSERT(All((a * b) == ELEMENT_OPERATE(V, a, *, b)));
		CRY_UNIT_TEST_ASSERT(All((a / b) == ELEMENT_OPERATE(V, a, /, b)));
		CRY_UNIT_TEST_ASSERT(All((a % b) == ELEMENT_OPERATE(V, a, %, b)));
		CRY_UNIT_TEST_ASSERT(All((a & b) == ELEMENT_OPERATE(V, a, &, b)));
		CRY_UNIT_TEST_ASSERT(All((a | b) == ELEMENT_OPERATE(V, a, |, b)));
		CRY_UNIT_TEST_ASSERT(All((a ^ b) == ELEMENT_OPERATE(V, a, ^, b)));
		CRY_UNIT_TEST_ASSERT(All((a << Scalar(3)) == ELEMENT_OPERATE(V, a, <<, convert<V>(3))));
		CRY_UNIT_TEST_ASSERT(All((a >> Scalar(2)) == ELEMENT_OPERATE(V, a, >>, convert<V>(2))));
	}

	template<typename V>
	void TestFloatOperators(V a, V b)
	{
		CRY_UNIT_TEST_ASSERT(All((a + b) == ELEMENT_OPERATE(V, a, +, b)));
		CRY_UNIT_TEST_ASSERT(All((a - b) == ELEMENT_OPERATE(V, a, -, b)));
		CRY_UNIT_TEST_ASSERT(All((a * b) == ELEMENT_OPERATE(V, a, *, b)));
		CRY_UNIT_TEST_ASSERT(IsEquiv(a / b, ELEMENT_OPERATE(V, a, /, b), FLT_EPSILON));
	}

	void OperatorTest()
	{
		i32v4 idata[6] = {
			convert<i32v4>(1,    -2,    3,   -4),     convert<i32v4>(-2,    -1,     5,    6),
			convert<i32v4>(7,    -12,   -13, 24),     convert<i32v4>(-2,    -5,     5,    6),
			convert<i32v4>(7654, -1234, -13, 249999), convert<i32v4>(-2373, -59999, 5432, 62)
		};

		for (int i = 0; i < 6; i += 2)
		{
			i32v4 a = idata[i];
			i32v4 b = idata[i + 1];

			TestIntOperators(a, b);
			TestIntOperators(convert<u32v4>(abs(a)), convert<u32v4>(abs(b)));
			TestIntOperators(convert<u32v4>(a), convert<u32v4>(b));
			TestFloatOperators(convert<f32v4>(a), convert<f32v4>(b));
		}
	}

	template<typename V>
	void HorizontalTest()
	{
		i32v4 idata[] = {
			convert<i32v4>(1,  -2,  3,   -4),
			convert<i32v4>(-2, -1,  5,   6),
			convert<i32v4>(7,  -12, -13, 24),
			convert<i32v4>(-2, -5,  5,   6)
		};

		for (i32v4 d : idata)
		{
			V v = convert<V>(d);

			auto h_sum = get_element<0>(v) + get_element<1>(v) + get_element<2>(v) + get_element<3>(v);
			CRY_UNIT_TEST_ASSERT(All(hsumv(v) == to_v4(h_sum)));

			auto h_min = min(get_element<0>(v), min(get_element<1>(v), min(get_element<2>(v), get_element<3>(v))));
			CRY_UNIT_TEST_ASSERT(All(hminv(v) == to_v4(h_min)));

			auto h_max = max(get_element<0>(v), max(get_element<1>(v), max(get_element<2>(v), get_element<3>(v))));
			CRY_UNIT_TEST_ASSERT(All(hmaxv(v) == to_v4(h_max)));
		}
	}

	template<typename V>
	void CompareTest()
	{
		CRY_UNIT_TEST_ASSERT(All(convert<V>(-3, -5, 6, 9) == convert<V>(-3, -5, 6, 9)));
		CRY_UNIT_TEST_ASSERT(Any(convert<V>(-3, -5, 6, 9) != convert<V>(-3, 5, 6, 9)));
		CRY_UNIT_TEST_ASSERT(All(convert<V>(-3, -5, 6, 9) < convert<V>(4, -2, 7, 10)));
		CRY_UNIT_TEST_ASSERT(All(convert<V>(-3, -5, 6, 9) <= convert<V>(4, -2, 6, 10)));
		CRY_UNIT_TEST_ASSERT(All(convert<V>(-3, -5, 6, 9) > convert<V>(-4, -7, -8, 0)));
		CRY_UNIT_TEST_ASSERT(All(convert<V>(-3, -5, 6, 9) >= convert<V>(-3, -6, 6, 8)));
	}
	void UCompareTest()
	{
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(1, 2, 3, 4) == convert<u32v4>(1, 2, 3, 4)));
		CRY_UNIT_TEST_ASSERT(!All(convert<u32v4>(1, 2, 3, 4) == convert<u32v4>(0, 2, 3, 4)));
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(1, 2, 3, 4) != convert<u32v4>(4, 3, 2, 1)));
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(0, 2, 3, 4) < convert<u32v4>(2u, 4u, 8u, 0x99999999)));
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(0, 2, 3, 4) <= convert<u32v4>(2u, 2u, 3u, 0x99999999)));
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(1u, 2u, 3u, 0x80000000) > convert<u32v4>(0, 1, 2, 3)));
		CRY_UNIT_TEST_ASSERT(All(convert<u32v4>(1u, 2u, 3u, 0x80000000) >= convert<u32v4>(1, 2, 3, 4)));
	}

	void SelectTest()
	{
		CRY_UNIT_TEST_ASSERT(if_else(3 == 4, 1.0f, 2.0f) == 2.0f);
		CRY_UNIT_TEST_ASSERT(if_else_zero(7.0f != 5.0f, 9) == 9);
		CRY_UNIT_TEST_ASSERT(__fsel(-7.0f, -5.0f, -3.0f) == -3.0f);
		CRY_UNIT_TEST_ASSERT(All(if_else(
		                           convert<f32v4>(0.5f, 0.0f, -7.0f, -0.0f) >= convert<f32v4>(2.0f, -4.0f, -5.0f, 0.0f),
		                           convert<i32v4>(-3, 3, -5, 0),
		                           convert<i32v4>(4, 40, -1, -7)
		                           ) == convert<i32v4>(4, 3, -1, 0)));
		CRY_UNIT_TEST_ASSERT(All(if_else_zero(
		                           convert<i32v4>(3, -7, 0, 4) >= convert<i32v4>(2, 7, -1, -8),
		                           convert<f32v4>(-3.0f, 3.0f, -3.0f, 0.9f)
		                           ) == convert<f32v4>(-3.0f, 0.0f, -3.0f, 0.9f)));
		CRY_UNIT_TEST_ASSERT(All(__fsel(
		                           convert<f32v4>(0.5f, 0.0f, -7.0f, -0.0f),
		                           convert<f32v4>(2.0f, -4.0f, -5.0f, 2.5f),
		                           convert<f32v4>(-3.0f, 3.0f, -3.0f, 0.9f)
		                           ) == convert<f32v4>(2.0f, -4.0f, -3.0f, 2.5f)));
	}

	#endif // CRY_PLATFORM_SSE2

	template<typename F> NO_INLINE
	void QuadraticTest(int mode = 2)
	{
		F maxErrAbs = 0, maxErrRel = 0;

		for (int i = 0; i < 240; ++i)
		{
			F c0 = RandomExp(6.0f) * (i % 5);
			F c1 = RandomExp(6.0f) * (i % 4);
			F c2 = RandomExp(6.0f) * (i % 3);

			F r[2];
			int n = solve_quadratic(c2, c1, c0, r);
			while (--n >= 0)
			{
				F err = abs(c0 + r[n] * (c1 + r[n] * c2));
				F mag = abs(r[n] * c1) + abs(r[n] * r[n] * c2 * F(2));

				if (err > maxErrAbs)
					maxErrAbs = err;
				if (err > mag * maxErrRel)
				{
					maxErrRel = err / mag;
					CRY_UNIT_TEST_ASSERT(err <= mag * std::numeric_limits<F>::epsilon() * F(5));
				}
			}
		}
	}

	CRY_UNIT_TEST(Math)
	{
		QuadraticTest<f32>();
		QuadraticTest<f64>();
		FunctionTest<float, int, uint>();
	#if CRY_PLATFORM_SSE2
		FunctionTest<f32v4, i32v4, u32v4>();
		ConvertTest<f32v4>();
		ConvertTest<i32v4>();
		OperatorTest();
		HorizontalTest<f32v4>();
		HorizontalTest<i32v4>();
		HorizontalTest<u32v4>();
		CompareTest<f32v4>();
		CompareTest<i32v4>();
		UCompareTest();
		SelectTest();
	#endif
	}
}

#ifdef CRY_HARDWARE_VECTOR4

// Enable this macro only to get vector timing results. Normally off, as it slows down compilation.
// #define VECTOR_PROFILE

CRY_UNIT_TEST_SUITE(MathVector)
{
	float RandomFloat()
	{
		return Math::RandomExp(3.0f);
	}
	Vec4 RandomVec4()
	{
		return Vec4(RandomFloat(), RandomFloat(), RandomFloat(), RandomFloat());
	}
	Matrix44 RandomMatrix44()
	{
		return Matrix44(RandomVec4(), RandomVec4(), RandomVec4(), RandomVec4());
	}

	static const int VCount = 256;
	static const int VRep = 1024;

	template<typename Vec, typename Mat>
	struct Element
	{
		float f0;
		Vec v0, v1;
		Mat m0, m1;

		Element() {}

		Element(int)
			: f0(RandomFloat())
			, v0(RandomVec4()), v1(RandomVec4())
			, m0(RandomMatrix44()), m1(RandomMatrix44()) {}

		template<typename Vec2, typename Mat2>
		Element(const Element<Vec2, Mat2>& in)
			: f0(in.f0)
			, v0(in.v0), v1(in.v1)
			, m0(in.m0), m1(in.m1)
		{}
	};

	std::vector<cstr> TestNames;

	template<typename Elem>
	struct Tester
	{
		cstr name;
		std::vector<int64> times;
		Elem elems[VCount];

		void Log() const
		{
			uint32 divide = VCount * VRep;
			#if CRY_PLATFORM_WINDOWS
				// Ticks are kilocycles
				divide /= 1000;
			#endif

			CryLog("Timings for %s", name);
			for (int i = 0; i < TestNames.size(); ++i)
			{
				uint32 ticks = uint32((times[i] + divide/2) / divide);
				CryLog(" %3u: %s", ticks, TestNames[i]);
			}
		}
	};

	template<typename Elem1, typename Elem2, typename Code1, typename Code2, typename E>
	void VerifyCode(cstr message, Tester<Elem1>& tester1, Tester<Elem2>& tester2, Code1 code1, Code2 code2, E tolerance)
	{
		for (int i = 0; i < VCount; ++i)
		{
			auto res1 = code1(tester1.elems[i]);
			auto res2 = code2(tester2.elems[i]);
			CRY_ASSERT_MESSAGE(IsEquivalent(res1, res2, tolerance), message, i);
		}
	}

	enum class TestMode { Verify, Prime, Profile };

	using Element4H = Element<Vec4,  Matrix44>;   Tester<Element4H> test4H = {"Vec4H, Matrix44H"};
	using Element4  = Element<Vec4f, Matrix44f>;  Tester<Element4>  test4  = {"Vec4, Matrix44"};
	using Element3H = Element<Vec3,  Matrix34>;   Tester<Element3H> test3H = {"Vec3, Matrix34H"};
	using Element3  = Element<Vec3,  Matrix34f>;  Tester<Element3>  test3  = {"Vec3, Matrix34"};

	void VectorTest(TestMode mode)
	{
	#ifdef VECTOR_PROFILE
		#define	VECTOR_PROFILE_TESTER(tester, code) { \
			auto& e = tester.elems[0]; \
			typedef decltype(code) Result; \
			Result results[VCount]; \
			int i = 0; \
			int64 time = CryGetTicks(); \
			for (auto& e: tester.elems) \
				results[i++] = code; \
			time = CryGetTicks() - time; \
			if (mode == TestMode::Prime) \
				tester.times.resize(TestNames.size()); \
			if (mode == TestMode::Profile) \
				tester.times[stat] += time; \
		}

		#define	VECTOR_PROFILE_CODE(code)  { \
			VECTOR_PROFILE_TESTER(test4H, code) \
			VECTOR_PROFILE_TESTER(test4,  code) \
			VECTOR_PROFILE_TESTER(test3H, code) \
			VECTOR_PROFILE_TESTER(test3,  code) \
			++stat; \
		}

	#else
		#define	VECTOR_PROFILE_CODE(code)
	#endif

		#define	VECTOR_TEST_CODE(code, tolerance) \
			if (mode == TestMode::Verify) \
			{ \
				if (add_names) TestNames.push_back(#code); \
				VerifyCode<Element4H, Element4>("mismatch 4/4H #%d: " #code, test4H, test4, [](Element4H& e) { return (code); }, [](Element4& e) { return (code); }, tolerance); \
				VerifyCode<Element3H, Element3>("mismatch 3/3H #%d: " #code, test3H, test3, [](Element3H& e) { return (code); }, [](Element3& e) { return (code); }, tolerance); \
			} \
			else \
			{ \
				VECTOR_PROFILE_CODE(code) \
			} \

		static const float Tolerance = -1e-5f;
		bool add_names = TestNames.empty();

		int stat = 0;
		VECTOR_TEST_CODE(e.v0, 0.0f);
		VECTOR_TEST_CODE(e.v0 == e.v1, bool());
		VECTOR_TEST_CODE(IsEquivalent(e.v0, e.v1, Tolerance), bool());
		VECTOR_TEST_CODE(IsEquivalent(e.v0, e.v1, Tolerance), bool());
		VECTOR_TEST_CODE(e.v0 | e.v1, Tolerance);
		VECTOR_TEST_CODE(e.v0 + e.v1, 0.0f);
		VECTOR_TEST_CODE(e.v0 * e.f0, Tolerance);
		VECTOR_TEST_CODE(e.v0.GetLength(), Tolerance);
		VECTOR_TEST_CODE(e.v0.GetNormalized(), Tolerance);
		VECTOR_TEST_CODE(e.v0.ProjectionOn(e.v1), Tolerance);

		VECTOR_TEST_CODE(e.m0, 0.0f);
		VECTOR_TEST_CODE(e.m0 == e.m1, bool());
		VECTOR_TEST_CODE(IsEquivalent(e.m0, e.m1, Tolerance), bool());
		VECTOR_TEST_CODE(IsEquivalent(e.m0, e.m1, Tolerance), bool());
		VECTOR_TEST_CODE(e.m0.GetTransposed(), 0.0f);
		VECTOR_TEST_CODE(e.m0.GetInverted(), Tolerance);
		VECTOR_TEST_CODE(e.m0.Determinant(), Tolerance);

		VECTOR_TEST_CODE(e.v0 * e.m0, Tolerance);
		VECTOR_TEST_CODE(e.m0 * e.v0, Tolerance);
		VECTOR_TEST_CODE(e.m0.TransformVector(e.v0), Tolerance);
		VECTOR_TEST_CODE(e.m0.TransformPoint(e.v0), Tolerance);
		VECTOR_TEST_CODE(e.m0 * e.m1, Tolerance);
	}

	CRY_UNIT_TEST(Vector)
	{
		// Init test to equivalent values
		for (int i = 0; i < VCount; ++i)
		{
			Construct(test4H.elems[i], 1);
			test4.elems[i]  = test4H.elems[i];
			test3H.elems[i] = test4H.elems[i];
			test3.elems[i]  = test3H.elems[i];
		}

		VectorTest(TestMode::Verify);

		#ifdef VECTOR_PROFILE
			VectorTest(TestMode::Prime);
			for (int i = 0; i < VRep; ++i)
			{
				VectorTest(TestMode::Profile);
			}

			test4H.Log();
			test4.Log();
			test3H.Log();
			test3.Log();
		#endif
	}
}

#endif // CRY_HARDWARE_VECTOR4


CRY_UNIT_TEST_SUITE(Strings)
{
	CRY_UNIT_TEST(CUT_Strings)
	{
		bool bOk;
		char bf[4];

		// cry_strcpy()

		bOk = cry_strcpy(0, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcpy(0, 0, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcpy(0, 1, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcpy(0, 1, 0, 1);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcpy(0, 1, "");
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcpy(0, 1, "", 1);
		CRY_UNIT_TEST_ASSERT(!bOk);

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 0, "");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 0, "", 1);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 1, 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 1, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty", 3);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty", 2);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty", 1);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "q\000cd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, 3, "qwerty", 0);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwerty");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "qwe\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwerty", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "qwe\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwerty", 3);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qwe\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwerty", 2);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwe");
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qwe\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qwe", 4);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qwe\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, "qw", 3);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "qw\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, sizeof(bf), "q");
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "q\000cd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcpy(bf, sizeof(bf), "q", 2);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "q\000cd", 4));

		// cry_strcat()

		bOk = cry_strcat(0, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcat(0, 0, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcat(0, 1, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcat(0, 1, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcat(0, 1, "");
		CRY_UNIT_TEST_ASSERT(!bOk);

		bOk = cry_strcat(0, 1, "", 1);
		CRY_UNIT_TEST_ASSERT(!bOk);

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 0, "xy");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 0, "xy", 3);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 0, "xy", 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 1, "xyz");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 1, "xyz", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 1, "xyz", 1);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 1, "xyz", 0);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, 1, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "\000bcd", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, 3, "xyz");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "ax\000d", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, 3, "xyz", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "ax\000d", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, 3, "xyz", 2);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "ax\000d", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, 3, "xyz", 1);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "ax\000d", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, "xyz");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abc\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, "xyz", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abc\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, "xyz", 1);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abc\000", 4));

		memcpy(bf, "abcd", 4);
		bOk = cry_strcat(bf, "xyz", 0);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "abc\000", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, "xyz");
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abx\000", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, "xyz", 4);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "abx\000", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, "xyz", 1);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "abx\000", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, "xyz", 0);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "ab\000d", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, 0, 0);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "ab\000d", 4));

		memcpy(bf, "ab\000d", 4);
		bOk = cry_strcat(bf, 0, 1);
		CRY_UNIT_TEST_ASSERT(!bOk && !memcmp(bf, "ab\000d", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, sizeof(bf), "xy");
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "axy\000", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, sizeof(bf), "xy", 3);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "axy\000", 4));

		memcpy(bf, "a\000cd", 4);
		bOk = cry_strcat(bf, sizeof(bf), "xy", 1);
		CRY_UNIT_TEST_ASSERT(bOk && !memcmp(bf, "ax\000d", 4));

		// cry_sprintf()
		{
			const auto test = [](const bool bNull, int charCount, const char* src, const char* initBuffer, const char* resultBuffer, bool resultBool)
			{
				char buffer[3] = { initBuffer[0], initBuffer[1], initBuffer[2] };
				CRY_ASSERT(charCount >= 0 && charCount <= CRY_ARRAY_COUNT(buffer));
				const bool b = cry_sprintf(bNull ? nullptr : buffer, charCount * sizeof(buffer[0]), src);
				CRY_UNIT_TEST_ASSERT(b == resultBool && !memcmp(buffer, resultBuffer, sizeof(buffer)));
			};

			test(true, 0, 0, "abc", "abc", false);
			test(true, 1, 0, "abc", "abc", false);
			test(false, 0, 0, "abc", "abc", false);
			test(false, 1, 0, "abc", "\000bc", false);

			test(true, 0, "", "abc", "abc", false);
			test(true, 1, "", "abc", "abc", false);
			test(true, 0, "x", "abc", "abc", false);
			test(true, 1, "x", "abc", "abc", false);
			test(true, 2, "x", "abc", "abc", false);

			test(false, 0, "", "abc", "abc", false);
			test(false, 1, "", "abc", "\000bc", true);
			test(false, 0, "x", "abc", "abc", false);
			test(false, 1, "x", "abc", "\000bc", false);
			test(false, 2, "x", "abc", "x\000c", true);
			test(false, 3, "x", "abc", "x\000c", true);
			test(false, 2, "xy", "abc", "x\000c", false);
			test(false, 3, "xy", "abc", "xy\000", true);
		}

		// CryStringUtils_Internal::compute_length_formatted()
		{
			const auto test = [](int length, std::vector<char>& c)
			{
				c.resize(length + 1);
				const int count = CryStringUtils_Internal::compute_length_formatted("%*s", length, "");
				CRY_UNIT_TEST_ASSERT(count == length);
				const bool b = cry_sprintf(c.data(), (length + 1) * sizeof(c[0]), "%*s", length, "");
				CRY_UNIT_TEST_ASSERT(b && strlen(c.data()) == length);
			};

			std::vector<char> c;
			c.reserve(4096);

			test(0, c);
			test(1, c);
			test(2, c);
			test(64, c);
			test(127, c);
			test(128, c);
			test(512, c);
			test(4095, c);
		}
	}
}

CRY_UNIT_TEST_SUITE(CryString)
{
	CRY_UNIT_TEST(CUT_CryString)
	{
		//////////////////////////////////////////////////////////////////////////
		// Based on MS documentation of find_last_of
		string strTestFindLastOfOverload1("abcd-1234-abcd-1234");
		string strTestFindLastOfOverload2("ABCD-1234-ABCD-1234");
		string strTestFindLastOfOverload3("456-EFG-456-EFG");
		string strTestFindLastOfOverload4("12-ab-12-ab");

		const char* cstr2 = "B1";
		const char* cstr2b = "D2";
		const char* cstr3a = "5EZZZZZZ";
		string str4a("ba3");
		string str4b("a2");

		size_t nPosition(string::npos);

		nPosition = strTestFindLastOfOverload1.find_last_of('d', 14);
		CRY_UNIT_TEST_ASSERT(nPosition == 13);

		nPosition = strTestFindLastOfOverload2.find_last_of(cstr2, 12);
		CRY_UNIT_TEST_ASSERT(nPosition == 11);

		nPosition = strTestFindLastOfOverload2.find_last_of(cstr2b);
		CRY_UNIT_TEST_ASSERT(nPosition == 16);

		nPosition = strTestFindLastOfOverload3.find_last_of(cstr3a, 8, 8);
		CRY_UNIT_TEST_ASSERT(nPosition == 4);

		nPosition = strTestFindLastOfOverload4.find_last_of(str4a, 8);
		CRY_UNIT_TEST_ASSERT(nPosition == 4);

		nPosition = strTestFindLastOfOverload4.find_last_of(str4b);
		CRY_UNIT_TEST_ASSERT(nPosition == 9);

		//////////////////////////////////////////////////////////////////////////
		// Based on MS documentation of find_last_not_of
		string strTestFindLastNotOfOverload1("dddd-1dd4-abdd");
		string strTestFindLastNotOfOverload2("BBB-1111");
		string strTestFindLastNotOfOverload3("444-555-GGG");
		string strTestFindLastNotOfOverload4("12-ab-12-ab");

		const char* cstr2NF = "B1";
		const char* cstr3aNF = "45G";
		const char* cstr3bNF = "45G";

		string str4aNF("b-a");
		string str4bNF("12");

		size_t nPosition3A(string::npos);

		nPosition = strTestFindLastNotOfOverload1.find_last_not_of('d', 7);
		CRY_UNIT_TEST_ASSERT(nPosition == 5);

		nPosition = strTestFindLastNotOfOverload1.find_last_not_of("d");
		CRY_UNIT_TEST_ASSERT(nPosition == 11);

		nPosition = strTestFindLastNotOfOverload2.find_last_not_of(cstr2NF, 6);
		CRY_UNIT_TEST_ASSERT(nPosition == 3);

		nPosition = strTestFindLastNotOfOverload3.find_last_not_of(cstr3aNF);
		CRY_UNIT_TEST_ASSERT(nPosition == 7);

		nPosition = strTestFindLastNotOfOverload3.find_last_not_of(cstr3bNF, 6, 3);    //nPosition - 1 );
		CRY_UNIT_TEST_ASSERT(nPosition == 3);

		nPosition = strTestFindLastNotOfOverload4.find_last_not_of(str4aNF, 5);
		CRY_UNIT_TEST_ASSERT(nPosition == 1);

		nPosition = strTestFindLastNotOfOverload4.find_last_not_of(str4bNF);
		CRY_UNIT_TEST_ASSERT(nPosition == 10);
	}
}

CRY_UNIT_TEST_SUITE(FixedString)
{
	CRY_UNIT_TEST(CUT_FixedString)
	{
		CryStackStringT<char, 10> str1;
		CryStackStringT<char, 10> str2;
		CryStackStringT<char, 4> str3;
		CryStackStringT<char, 10> str4;
		CryStackStringT<wchar_t, 16> wstr1;
		CryStackStringT<wchar_t, 255> wstr2;
		CryFixedStringT<100> fixedString100;
		CryFixedStringT<200> fixedString200;

		typedef CryStackStringT<char, 10> T;
		T* pStr = new T;
		*pStr = "adads";
		delete pStr;

		str1 = "abcd";
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "abcd");

		str2 = "efg";
		CRY_UNIT_TEST_CHECK_EQUAL(str2, "efg");

		str2 = str1;
		CRY_UNIT_TEST_CHECK_EQUAL(str2, "abcd");

		str1 += "XY";
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "abcdXY");

		str2 += "efghijk";
		CRY_UNIT_TEST_CHECK_EQUAL(str2, "abcdefghijk");

		str1.replace("bc", "");
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "adXY");

		str1.replace("XY", "1234");
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "ad1234");

		str1.replace("1234", "1234567890");
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "ad1234567890");

		str1.reserve(200);
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "ad1234567890");
		CRY_UNIT_TEST_ASSERT(str1.capacity() == 200);
		str1.reserve(0);
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "ad1234567890");
		CRY_UNIT_TEST_ASSERT(str1.capacity() == str1.length());

		str1.erase(7); // doesn't change capacity
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "ad12345");

		str4.assign("abc");
		CRY_UNIT_TEST_CHECK_EQUAL(str4, "abc");
		str4.reserve(9);
		CRY_UNIT_TEST_ASSERT(str4.capacity() >= 9);  // capacity is always >= MAX_SIZE-1
		str4.reserve(0);
		CRY_UNIT_TEST_ASSERT(str4.capacity() >= 9);  // capacity is always >= MAX_SIZE-1

		size_t idx = str1.find("123");
		CRY_UNIT_TEST_ASSERT(idx == 2);

		idx = str1.find("123", 3);
		CRY_UNIT_TEST_ASSERT(idx == str1.npos);

		wstr1 = L"abc";
		CRY_UNIT_TEST_CHECK_EQUAL(wstr1, L"abc");
		CRY_UNIT_TEST_ASSERT(wstr1.compare(L"aBc") > 0);
		CRY_UNIT_TEST_ASSERT(wstr1.compare(L"babc") < 0);
		CRY_UNIT_TEST_ASSERT(wstr1.compareNoCase(L"aBc") == 0);
		str1.Format("This is a %s %ls with %d params", "mixed", L"string", 3);
		str2.Format("This is a %ls %s with %d params", L"mixed", "string", 3);
		CRY_UNIT_TEST_CHECK_EQUAL(str1, "This is a mixed string with 3 params");
		CRY_UNIT_TEST_CHECK_EQUAL(str1, str2);
	}
}

//////////////////////////////////////////////////////////////////////////
// Unit Testing of aligned_vector
//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(CUT_AlignedVector)
{
	stl::aligned_vector<int, 16> vec;

	vec.push_back(1);
	vec.push_back(2);
	vec.push_back(3);

	CRY_UNIT_TEST_ASSERT(vec.size() == 3);
	CRY_UNIT_TEST_ASSERT(((INT_PTR)(&vec[0]) % 16) == 0);
}

CRY_UNIT_TEST_SUITE(SmartPointer)
{
	bool gHasCalledDtor = false;

	template<typename TargetType>
	class I : public TargetType
	{
	public:
		virtual ~I() {
			gHasCalledDtor = true;
		}
		virtual int GetValue() const = 0;
	};

	template<typename TargetType>
	class C : public I<TargetType>
	{
		int value = 0;
	public:

		C() = default;
		C(const C&) = default;
		explicit C(int _val) : value(_val) {}

		virtual int GetValue() const override
		{
			return value;
		}
	};

	CRY_UNIT_TEST(CUT_SmartPtr)
	{
		typedef _smart_ptr<I<_i_reference_target_t>> Ptr;
		gHasCalledDtor = false;
		{
			Ptr ptr = new C<_i_reference_target_t>();
			CRY_UNIT_TEST_CHECK_EQUAL(ptr->UseCount(), 1);
			{
				Ptr ptr2 = ptr;
				CRY_UNIT_TEST_CHECK_EQUAL(ptr->UseCount(), 2);
				CRY_UNIT_TEST_CHECK_EQUAL(ptr2->UseCount(), 2);

				Ptr ptr3 = nullptr;
				ptr3 = ptr2;
				CRY_UNIT_TEST_CHECK_EQUAL(ptr->UseCount(), 3);
				CRY_UNIT_TEST_CHECK_EQUAL(ptr2->UseCount(), 3);
				CRY_UNIT_TEST_CHECK_EQUAL(ptr3->UseCount(), 3);
			}
			CRY_UNIT_TEST_CHECK_EQUAL(ptr->UseCount(), 1);
		}
		CRY_UNIT_TEST_ASSERT(gHasCalledDtor);
	}

	CRY_UNIT_TEST(CUT_SmartPtr_Copy)
	{
		typedef C<_i_reference_target_t> MyType;
		typedef _smart_ptr<MyType> Ptr;
		Ptr ptr1 = new MyType(42);
		Ptr ptr2 = new MyType(0x1337);
		Ptr ptr3 = ptr2;
		CRY_UNIT_TEST_CHECK_EQUAL(ptr1->UseCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr2->UseCount(), 2);

		//when object pointed by _smart_ptr gets assigned, it should copy the value but not the refcount
		*ptr2 = *ptr1;
		CRY_UNIT_TEST_CHECK_EQUAL(ptr1->UseCount(), 1);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr2->UseCount(), 2);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr1->GetValue(), 42);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr2->GetValue(), 42);

		//when a new object is copy constructed, it should copy the value but not the refcount
		_smart_ptr<I<_i_reference_target_t>> ptr4 = new MyType(*ptr2);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr4->GetValue(), 42);
		CRY_UNIT_TEST_CHECK_EQUAL(ptr4->UseCount(), 1);
	}
}

struct Counts
{
	int construct = 0, copy_init = 0, copy_assign = 0, move_init = 0, move_assign = 0, destruct = 0;

	Counts()
	{}

	Counts(int a, int b, int c, int d, int e, int f)
		: construct(a), copy_init(b), copy_assign(c), move_init(d), move_assign(e), destruct(f) {}

	bool operator==(const Counts& o) const { return !memcmp(this, &o, sizeof(*this)); }
};
static Counts s_counts;
static Counts delta_counts()
{
	Counts delta = s_counts;
	s_counts = Counts();
	return delta;
}

struct Tracker
{
	int res;

	Tracker(int r = 0)
	{
		s_counts.construct += r;
		res = r;
	}
	Tracker(const Tracker& in)
	{
		s_counts.copy_init += in.res;
		res = in.res;
	}
	Tracker(Tracker&& in)
	{
		s_counts.move_init += in.res;
		res = in.res;
		in.res = 0;
	}
	void operator=(const Tracker& in)
	{
		s_counts.copy_assign += res + in.res;
		res = in.res;
	}
	void operator=(Tracker&& in)
	{
		s_counts.move_assign += res + in.res;
		res = in.res;
		in.res = 0;
	}
	~Tracker()
	{
		s_counts.destruct += res;
		res = 0;
	}
};

CRY_UNIT_TEST(CUT_DynArray)
{
	DynArray<string, uint> aus;
	DynArray<string, uint>::difference_type dif = -1;
	CRY_UNIT_TEST_ASSERT(dif < 0);

	// Test all constructors
	const int ais[] = { 11, 7, 5, 3, 2, 1, 0 };
	DynArray<int> ai;
	CRY_UNIT_TEST_ASSERT(ai.size() == 0);
	DynArray<int> ai1(4);
	CRY_UNIT_TEST_ASSERT(ai1.size() == 4);
	DynArray<int> ai3(6, 0);
	CRY_UNIT_TEST_ASSERT(ai3.size() == 6);
	DynArray<int> ai4(ai3);
	CRY_UNIT_TEST_ASSERT(ai4.size() == 6);
	DynArray<int> ai6(ais);
	CRY_UNIT_TEST_ASSERT(ai6.size() == 7);
	DynArray<int> ai7(ais + 1, ais + 5);
	CRY_UNIT_TEST_ASSERT(ai7.size() == 4);

	ai.push_back(3);
	ai.insert(&ai[0], 1, 1);
	ai.insert(1, 1, 2);
	ai.insert(0, 0);

	for (int i = 0; i < 4; i++)
		CRY_UNIT_TEST_ASSERT(ai[i] == i);

	ai.push_back(5u);
	CRY_UNIT_TEST_ASSERT(ai.size() == 5);
	ai.push_back(ai);
	CRY_UNIT_TEST_ASSERT(ai.size() == 10);
	ai.push_back(ArrayT(ais));
	CRY_UNIT_TEST_ASSERT(ai.size() == 17);

	std::basic_string<char> bstr;

	const int nStrs = 11;
	string Strs[nStrs] = { "nought", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };

	DynArray<string> s;
	for (int i = 0; i < nStrs; i += 2)
		s.push_back(Strs[i]);
	for (int i = 1; i < nStrs; i += 2)
		s.insert(i, Strs[i]);
	s.push_back(string("eleven"));
	s.push_back("twelve");
	s.push_back("thirteen");
	CRY_UNIT_TEST_ASSERT(s.size() == 14);
	for (int i = 0; i < nStrs; i++)
		CRY_UNIT_TEST_ASSERT(s[i] == Strs[i]);

	DynArray<string> s2 = s;
	s.erase(5, 2);
	CRY_UNIT_TEST_ASSERT(s.size() == 12);

	s.insert(3, &Strs[5], &Strs[8]);

	s2 = s2(3, 4);
	CRY_UNIT_TEST_ASSERT(s2.size() == 4);

	s2.assign("single");

	DynArray<CryStackStringT<char, 8>> sf;
	sf.push_back("little");
	sf.push_back("Bigger one");
	sf.insert(1, "medium");
	sf.erase(0);
	CRY_UNIT_TEST_ASSERT(sf[0] == "medium");
	CRY_UNIT_TEST_ASSERT(sf[1] == "Bigger one");

	{
		// construct, copy_init, copy_assign, move_init, move_assign, destruct;
		typedef LocalDynArray<Tracker, 4> TrackerArray;
		TrackerArray at(3, 1);                            // construct init argument, move-init first element, copy-init 2 elements
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(1, 2, 0, 1, 0, 0));
		at.erase(1);                                      // destruct 1 element, move-init 1 element
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(0, 0, 0, 1, 0, 1));

		// Move forwarding
		at.reserve(6);                                    // move_init 2
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(0, 0, 0, 2, 0, 0));
		at.append(Tracker(1));                            // construct, move_init
		at.push_back(Tracker(1));                         // construct, move_init
		at += Tracker(1);                                 // construct, move_init
		at.insert(1, Tracker(1));                         // construct, move_init, move_init * 4
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(4, 0, 0, 8, 0, 0));

		DynArray<TrackerArray> aas(3, at);                // copy init 18 elements
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(0, 18, 0, 0, 0, 0));
		aas.erase(0);                                     // destruct 6 elements, move element arrays (no element construction or destruction)
		CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(0, 0, 0, 0, 0, 6));
	}                                                   // destruct 12 elements from aas and 6 from at
	CRY_UNIT_TEST_ASSERT(delta_counts() == Counts(0, 0, 0, 0, 0, 18));
	int construct = 0, copy_init = 0, copy_assign = 0, move_init = 0, move_assign = 0, destruct = 0;
}

CRY_UNIT_TEST(CUT_HeapAlloc)
{
	typedef stl::HeapAllocator<stl::PSyncNone> THeap;
	THeap beep;

	for (int i = 0; i < 8; ++i)
	{
		THeap::Array<float> af(beep, i + 77);
		af.push_back(3.14f);
		THeap::Array<bool, uint, 16> ab(beep, i*5);
		THeap::Array<int> ai(beep);
		ai.reserve(9111);
		THeap::Array<string> as(beep, 8);
		ai.push_back(-7);
		ai.push_back(-6);
		af.erase(30, 40);
		if (i == 2)
			ai.resize(0);
		as[0] = "nil";
		as[7] = "sevven";
		if (i == 4)
			as.clear();
	}
	CRY_UNIT_TEST_ASSERT(beep.GetTotalMemory().nUsed == 0);
}

CRY_UNIT_TEST(CUT_SimplifyFilePath)
{
	// Check a number of invalid outputs and too-short-buffer scenarios.
	// They should do the same for Windows and Posix.
	{
		for (int i = 0; i < 2; ++i)
		{
			PathUtil::EPathStyle style = (i == 0 ? PathUtil::ePathStyle_Windows : PathUtil::ePathStyle_Posix);
			char buf[1];
			bool result = PathUtil::SimplifyFilePath("foo", buf, 1, style);
			CRY_UNIT_TEST_ASSERT(!result && !*buf);

			result = PathUtil::SimplifyFilePath(nullptr, buf, 1, style);
			CRY_UNIT_TEST_ASSERT(!result && !*buf);

			result = PathUtil::SimplifyFilePath("foo", nullptr, 9001, style);
			CRY_UNIT_TEST_ASSERT(!result);

			result = PathUtil::SimplifyFilePath("foo", buf, 0, style);
			CRY_UNIT_TEST_ASSERT(!result);

			result = PathUtil::SimplifyFilePath("", buf, 1, style);
			CRY_UNIT_TEST_ASSERT(!result && !*buf);
		}
	}

	// tests with sufficient buffer
	{
		struct
		{
			const char* szInput;
			bool        bWinExpected;
			const char* szWinExpected;
			bool        bUnixExpected;
			const char* szUnixExpected;
		}
		items[] =
		{
			//                              WINAPI                                   POSIX
			// input                        success   output                         success   output
			{ "",                            false, "",                          false, ""                       },
			{ "c:",                          true,  "c:",                        true,  "c:"                     },// c: and c:. counts as a regular folder name in posix
			{ "c:.",                         true,  "c:",                        true,  "c:."                    },
			{ "c:..",                        true,  "c:..",                      true,  "c:.."                   },
			{ "c:abc/.",                     true,  "c:abc",                     true,  "c:abc"                  },
			{ "c:abc/..",                    true,  "c:",                        true,  ""                       },
			{ "c:abc/../..",                 true,  "c:..",                      true,  ".."                     },
			{ "c:\\",                        true,  "c:\\",                      true,  "c:"                     },
			{ ":\\.",                        false, "",                          true,  ":"                      },
			{ "abc:\\.",                     false, "",                          true,  "abc:"                   },
			{ "c:\\.",                       true,  "c:\\",                      true,  "c:"                     },
			{ "c:\\..",                      false, "",                          true,  ""                       },
			{ "c:/abc/.",                    true,  "c:\\abc",                   true,  "c:/abc"                 },
			{ "c:/abc/..",                   true,  "c:\\",                      true,  "c:"                     },
			{ "c:/abc\\..\\..",              false, "",                          true,  ""                       },
			{ "\\\\storage\\.",              true,  "\\\\storage",               false, ""                       },
			{ "\\\\storage\\..",             false, "",                          false, ""                       },
			{ "\\\\storage\\abc\\..\\",      true,  "\\\\storage",               false, ""                       },
			{ "c:/should/be/unchanged",      true,  "c:\\should\\be\\unchanged", true,  "c:/should/be/unchanged" },
			{ "c:\\disappear/..\\remain",    true,  "c:\\remain",                true,  "c:/remain"              },
			{ "c:\\disappear\\..\\remain",   true,  "c:\\remain",                true,  "c:/remain"              },
			{ "/disappear/../remain",        true,  "\\remain",                  true,  "/remain"                },
			{ "c:/trailing/slash/gone/",     true,  "c:\\trailing\\slash\\gone", true,  "c:/trailing/slash/gone" },
			{ "c:\\trailing\\slash\\gone\\", true,  "c:\\trailing\\slash\\gone", true,  "c:/trailing/slash/gone" },
			{ "/trailing/slash/gone/",       true,  "\\trailing\\slash\\gone",   true,  "/trailing/slash/gone"   },
			{ ".",                           true,  ".",                         true,  "."                      },
			{ "..",                          true,  "..",                        true,  ".."                     },
			{ "..\\..",                      true,  "..\\..",                    true,  "../.."                  },
			{ "\\..\\..\\",                  false, "",                          false, ""                       },
			{ "../.\\..",                    true,  "..\\..",                    true,  "../.."                  },
			{ "\\\\.\\myfile",               false, "",                          false, ""                       },
			{ "/some/rooted/path",           true,  "\\some\\rooted\\path",      true,  "/some/rooted/path"      },
			{ "\\\\s2\\..\\s1",              false, "",                          false, ""                       },
			{ "/",                           true,  "\\",                        true,  "/"                      },
			{ "/.",                          true,  "\\",                        true,  "/"                      },
			{ "/..",                         false, "",                          false, ""                       },
			{ "/abc/.",                      true,  "\\abc",                     true,  "/abc"                   },
			{ "/abc/..",                     true,  "\\",                        true,  "/"                      },
			{ "/abc/../..",                  false, "",                          false, ""                       },
			{ "../abc",                      true,  "..\\abc",                   true,  "../abc"                 },
			{ "../../abc",                   true,  "..\\..\\abc",               true,  "../../abc"              },
			{ "bar/../../../abc",            true,  "..\\..\\abc",               true,  "../../abc"              },
			{ "/abc/foo/bar",                true,  "\\abc\\foo\\bar",           true,  "/abc/foo/bar"           },
			{ "abc",                         true,  "abc",                       true,  "abc"                    },
			{ "abc/.",                       true,  "abc",                       true,  "abc"                    },
			{ "abc/..",                      true,  "",                          true,  ""                       },
			{ "abc/../..",                   true,  "..",                        true,  ".."                     },
			{ "abc/foo/bar",                 true,  "abc\\foo\\bar",             true,  "abc/foo/bar"            },
			{ "/foo//bar",                   true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ "/foo/bar//",                  true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ "/foo/////bar////",            true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ "\\foo\\\\bar",                true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ "\\foo\\bar\\\\",              true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ "\\foo\\\\\\\\\\bar\\\\\\",    true,  "\\foo\\bar",                true,  "/foo/bar"               },
			{ ".\\/system.cfg",              true,  "system.cfg",                true,  "system.cfg"             },
			{ ".\\/./system.cfg",            true,  "system.cfg",                true,  "system.cfg"             },
			{ ".\\/./%USER%/game.cfg",       true,  "%USER%\\game.cfg",          true,  "%USER%/game.cfg"        },
		};
		const size_t numItems = CRY_ARRAY_COUNT(items);

		const size_t bufLength = 100;
		char buf[bufLength];

		for (size_t i = 0; i < numItems; ++i)
		{
			bool bWinResult = PathUtil::SimplifyFilePath(items[i].szInput, buf, bufLength, PathUtil::ePathStyle_Windows);
			bool bWinMatches = strcmp(buf, items[i].szWinExpected) == 0;
			bool bUnixResult = PathUtil::SimplifyFilePath(items[i].szInput, buf, bufLength, PathUtil::ePathStyle_Posix);
			bool bUnixMatches = strcmp(buf, items[i].szUnixExpected) == 0;
			CRY_UNIT_TEST_CHECK_EQUAL(bWinResult, items[i].bWinExpected);
			CRY_UNIT_TEST_CHECK_EQUAL(bUnixResult, items[i].bUnixExpected);
			CRY_UNIT_TEST_CHECK_EQUAL(bWinMatches, true);
			CRY_UNIT_TEST_CHECK_EQUAL(bUnixMatches, true);
		}
	}

	// test complex case
	{
		const size_t bufLength = 100;
		char buf[bufLength];

		const char* const szComplex1 = "foo/bar/../baz/./../../../../././hi/.";
		const char* const szResult1 = "../../hi";
		bool result = PathUtil::SimplifyFilePath(szComplex1, buf, bufLength, PathUtil::ePathStyle_Posix);
		CRY_UNIT_TEST_ASSERT(result && strcmp(buf, szResult1) == 0);

		const char* const szComplex2 = "c:/foo/bar/./disappear/disappear/disappear/../.\\../../../baz\\";
		const char* const szResult2 = "c:\\foo\\baz";
		result = PathUtil::SimplifyFilePath(szComplex2, buf, bufLength, PathUtil::ePathStyle_Windows);
		CRY_UNIT_TEST_ASSERT(result && strcmp(buf, szResult2) == 0);
	}
}

	#include <CryCore/RingBuffer.h>

CRY_UNIT_TEST(CUT_RingBuffer)
{
	{
		CRingBuffer<int, 256> buffer;
		CRY_UNIT_TEST_ASSERT(buffer.empty() && buffer.size() == 0 && buffer.full() == false);

		// 0, 1, 2, 3
		buffer.push_back(2);
		buffer.push_front(1);
		buffer.push_front_overwrite(0);
		buffer.push_back_overwrite(3);

		CRY_UNIT_TEST_ASSERT(buffer.front() == 0);
		CRY_UNIT_TEST_ASSERT(buffer.back() == 3);

		buffer.pop_front();
		buffer.pop_back();

		CRY_UNIT_TEST_ASSERT(buffer.size() == 2);
		CRY_UNIT_TEST_ASSERT(buffer.front() == 1);
		CRY_UNIT_TEST_ASSERT(buffer.back() == 2);
	}

	{
		CRingBuffer<float, 3> buffer;

		// 0, 1, 2
		buffer.push_back(0.0f);
		buffer.push_back(1.0f);
		buffer.push_back(2.0f);
		CRY_UNIT_TEST_ASSERT(buffer.full());

		// 3, 1, 2
		buffer.pop_front();
		buffer.push_front(3.0f);
		bool bPushed = buffer.push_front(4.0f);
		CRY_UNIT_TEST_ASSERT(!bPushed && buffer.front() != 4.0f);

		// 4, 3, 1
		buffer.push_front_overwrite(4.0f);
		CRY_UNIT_TEST_ASSERT(buffer.front() == 4.0f);
		CRY_UNIT_TEST_ASSERT(buffer.back() == 1.0f);

		// nop
		bPushed = buffer.push_back(5.0f);
		CRY_UNIT_TEST_ASSERT(!bPushed && buffer.back() != 5.0f);

		// 3, 1, 5
		buffer.push_back_overwrite(5.0f);
		CRY_UNIT_TEST_ASSERT(buffer.front() == 3.0f);
		CRY_UNIT_TEST_ASSERT(buffer.back() == 5.0f);
	}
}

	#include <CryCore/CryVariant.h>

CRY_UNIT_TEST(CUT_Variant)
{
	// Default initialization
	{
		CryVariant<string, Vec3, short, int> v;
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v));
		CRY_UNIT_TEST_ASSERT(v.index() == 0);
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v) == string());
	}

	// Emplace & Get
	{
		CryVariant<int, string> v;

		v.emplace<int>(5);
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get<0>(v) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<int>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<int>(&v)) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<string>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<0>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<0>(&v)) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<1>(&v) == nullptr);

		v.emplace<string>("Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get<1>(v) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get_if<int>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<string>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<string>(&v)) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get_if<0>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<1>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<1>(&v)) == "Hello World");

		v.emplace<0>(5);
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get<0>(v) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<int>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<int>(&v)) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<string>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<0>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<0>(&v)) == 5);
		CRY_UNIT_TEST_ASSERT(stl::get_if<1>(&v) == nullptr);

		v.emplace<1>("Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get<1>(v) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get_if<int>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<string>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<string>(&v)) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::get_if<0>(&v) == nullptr);
		CRY_UNIT_TEST_ASSERT(stl::get_if<1>(&v) != nullptr);
		CRY_UNIT_TEST_ASSERT((*stl::get_if<1>(&v)) == "Hello World");
	}

	// Equals
	{
		CryVariant<bool, int> v1, v2;

		v1.emplace<bool>(true);
		v2.emplace<bool>(true);
		CRY_UNIT_TEST_ASSERT(v1 == v2);
		CRY_UNIT_TEST_ASSERT(!(v1 != v2));

		v1.emplace<bool>(true);
		v2.emplace<bool>(false);
		CRY_UNIT_TEST_ASSERT(!(v1 == v2));
		CRY_UNIT_TEST_ASSERT(v1 != v2);

		v1.emplace<bool>(true);
		v2.emplace<int>(1);
		CRY_UNIT_TEST_ASSERT(!(v1 == v2));
		CRY_UNIT_TEST_ASSERT(v1 != v2);
	}

	// Less (used for sorting in stl containers)
	{
		CryVariant<bool, int> v1, v2;

		v1.emplace<int>(0);
		v2.emplace<int>(5);
		CRY_UNIT_TEST_ASSERT(v1 < v2);
		CRY_UNIT_TEST_ASSERT(!(v2 < v1));

		v1.emplace<int>(0);
		v2.emplace<int>(0);
		CRY_UNIT_TEST_ASSERT(!(v1 < v2));
		CRY_UNIT_TEST_ASSERT(!(v2 < v1));

		v1.emplace<bool>(true);
		v2.emplace<int>(0);
		CRY_UNIT_TEST_ASSERT(v1 < v2);
		CRY_UNIT_TEST_ASSERT(!(v2 < v1));
	}

	// Copy & Move
	{
		CryVariant<int, string> v1, v2;

		v1.emplace<int>(0);
		v2.emplace<int>(1);
		v1 = v2;
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v1) == 1);

		v1.emplace<string>("Hello");
		v2.emplace<string>("World");
		v1 = v2;
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v1) == "World");

		v1.emplace<int>(0);
		v2.emplace<string>("Hello World");
		v1 = v2;
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v1));
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v1) == "Hello World");

		v1.emplace<int>(0);
		v2.emplace<int>(1);
		v1 = std::move(v2);
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v1) == 1);

		v1.emplace<string>("Hello");
		v2.emplace<string>("World");
		v1 = std::move(v2);
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v1) == "World");

		v1.emplace<int>(0);
		v2.emplace<string>("Hello World");
		v1 = std::move(v2);
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v1));
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v1) == "Hello World");

		v1.emplace<string>("Hello World");
		CryVariant<int, string> v3(v1);
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v3));
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v3) == "Hello World");

		v1.emplace<string>("Hello World");
		CryVariant<int, string> v4(std::move(v1));
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v4));
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v4) == "Hello World");
	}

	// Swap
	{
		CryVariant<int, string> v1, v2;

		v1.emplace<int>(0);
		v2.emplace<int>(1);
		v1.swap(v2);
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<int>(v1));
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v1) == 1);
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<int>(v2));
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v2) == 0);

		v1.emplace<int>(0);
		v2.emplace<string>("Hello World");
		v1.swap(v2);
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<string>(v1));
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v1) == "Hello World");
		CRY_UNIT_TEST_ASSERT(stl::holds_alternative<int>(v2));
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v2) == 0);
	}

	// Visit
	{
		auto visitor = [](CryVariant<int, string>& v)
		{
			if (stl::holds_alternative<int>(v))
				v.emplace<int>(stl::get<int>(v) * 2);
			else
				v.emplace<string>(stl::get<string>(v) + " " + stl::get<string>(v));
		};

		CryVariant<int, string> v1, v2;
		v1.emplace<int>(5);
		v2.emplace<string>("Hello World");

		stl::visit(visitor, v1);
		stl::visit(visitor, v2);
		CRY_UNIT_TEST_ASSERT(stl::get<int>(v1) == 10);
		CRY_UNIT_TEST_ASSERT(stl::get<string>(v2) == "Hello World Hello World");
	}
}

CRY_UNIT_TEST(CUT_CRYGUID)
{
	CryGUID guid;

	// Test that CryGUID constructor initialize it to null
	CRY_UNIT_TEST_ASSERT(guid.IsNull());

	guid = "296708CE-F570-4263-B067-C6D8B15990BD"_cry_guid;

	// Verify that all string-based ways to create CryGUID return the same result
	CRY_UNIT_TEST_CHECK_EQUAL(guid, CryGUID::FromString("296708CE-F570-4263-B067-C6D8B15990BD"));

	// Test that GUID specified in string with or without brackets work reliably
	CRY_UNIT_TEST_CHECK_EQUAL(guid, "{296708CE-F570-4263-B067-C6D8B15990BD}"_cry_guid);
	CRY_UNIT_TEST_CHECK_EQUAL(guid, CryGUID::FromString("{296708CE-F570-4263-B067-C6D8B15990BD}"));

	// Verify that CryGUID is case insensitive
	CRY_UNIT_TEST_CHECK_EQUAL(guid, "296708ce-f570-4263-b067-c6d8b15990bd"_cry_guid);
	CRY_UNIT_TEST_CHECK_EQUAL(guid, "{296708ce-f570-4263-b067-c6d8b15990bd}"_cry_guid);

	// Test back conversion from GUID to string
	char str[64];
	guid.ToString(str);
	CRY_UNIT_TEST_CHECK_EQUAL(CryStringUtils::toUpper(str), "296708CE-F570-4263-B067-C6D8B15990BD");
	CRY_UNIT_TEST_CHECK_EQUAL(CryStringUtils::toUpper(guid.ToString()), "296708CE-F570-4263-B067-C6D8B15990BD");
}

#endif //CRY_UNIT_TESTING
