// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

//! Defines interfaces and macros for Cry Unit Tests.
//! CRY_UNIT_TEST_SUITE:           A suite name to group tests locally together.
//! CRY_UNIT_TEST_FIXTURE:         A fixture (intermediate base class) as the container of variables and methods shared by tests.
//! CRY_UNIT_TEST:                 A standard unit test block.
//! CRY_UNIT_TEST_WITH_FIXTURE:    A unit test with a fixture as the base class.
//! CRY_UNIT_TEST_ASSERT:          Fails and reports if the specified expression evaluates to false.
//! CRY_UNIT_TEST_CHECK_CLOSE:     Fails and reports if the specified floating point values are not equal with respect to epsilon.
//! CRY_UNIT_TEST_CHECK_EQUAL:     Fails and reports if the specified values are not equal.
//! CRY_UNIT_TEST_CHECK_DIFFERENT: Fails and reports if the specified values are not different.
//!
//! Usage of comparison checks should be favored over the generic CRY_UNIT_TEST_ASSERT because of detailed output in the report.

#pragma once

#include <CrySystem/ITestSystem.h>
#include <CryString/StringUtils.h>
#include <CryCore/StaticInstanceList.h>
#include <type_traits>
#if defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
	#include <exception>
#endif

namespace CryUnitTest
{
struct STest;
struct IUnitTestReporter;

//! Describes in what way the test result is reported
enum class EReporterType
{
	Excel,                  //!< Writes multiple excel documents for detailed results
	ExcelWithNotification,  //!< In addition to excel document, also opens the document locally when any test fails
	Minimal,                //!< Writes minimal logs
	Regular,                //!< Writes detailed logs for every test
};

class CUnitTestInfo final
{
public:
	//! Instantiates test info bound to a test instance
	CUnitTestInfo(STest& test)
		: m_test(test)
	{
	}

	STest& GetTest() const { return m_test; }

	//! Sets and gets basic information for the test
	void        SetSuite(const char* suiteName)   { m_sSuite = suiteName; }
	const char* GetSuite() const                  { return m_sSuite.c_str(); }

	void        SetName(const char* name)         { m_sName = name; }
	const char* GetName() const                   { return m_sName.c_str(); }

	void        SetFileName(const char* fileName) { m_sFileName = fileName; }
	const char* GetFileName() const               { return m_sFileName.c_str(); }

	void        SetLineNumber(int lineNumber)     { m_LineNumber = lineNumber; }
	int         GetLineNumber() const             { return m_LineNumber; }

	//! Sets and gets module (dll name) information for the test.
	//! Stores CryString in the back because the raw literal is not persistent across modules.
	void        SetModule(const char* newModuleName) { m_sModule = newModuleName; }
	const char* GetModule() const                    { return m_sModule.c_str(); }

	//! Equality comparison. Comparing related test instances is sufficient.
	friend bool operator==(const CUnitTestInfo& lhs, const CUnitTestInfo& rhs)
	{
		return &lhs.m_test == &rhs.m_test;
	}

private:
	STest& m_test;
	string m_sSuite;
	string m_sName;
	string m_sFileName;
	int    m_LineNumber = 0;
	string m_sModule;
};

//! Defines info needed for script bound auto testing.
//! Currently not in use
struct SAutoTestInfo
{
	bool        runNextTest = true;  //!< To organize auto tests cycle.
	int         waitMSec = 0;        //!< Identify waiting period after each auto test.
	const char* szTaskName = 0;      //!< Current test task.
};

//! Base class for all user tests expanded through macros
struct STest
{
	virtual ~STest(){}

	//! Must be implemented by the test creator.
	virtual void Run() = 0;

	//! Optional methods called by the system at the beginning of the testing and at the end of the testing.
	virtual void Init() {};
	virtual void Done() {};

	CUnitTestInfo m_unitTestInfo { *this };
	SAutoTestInfo m_autoTestInfo;//!< Currently not in use
};

struct SUnitTestRunContext
{
	int testCount = 0;
	int failedTestCount = 0;
	int succedTestCount = 0;
	std::unique_ptr<IUnitTestReporter> pReporter;
};

struct IUnitTest
{
	virtual ~IUnitTest(){}
	virtual const CUnitTestInfo& GetInfo() const = 0;
	virtual const SAutoTestInfo& GetAutoTestInfo() const = 0;
	virtual void                 Init() = 0;
	virtual void                 Run() = 0;
	virtual void                 Done() = 0;
};

struct IUnitTestReporter
{
	virtual ~IUnitTestReporter(){}

	//! Notify reporter the test system started
	virtual void OnStartTesting(const SUnitTestRunContext& context) = 0;

	//! Notify reporter the test system finished
	virtual void OnFinishTesting(const SUnitTestRunContext& context) = 0;

	//! Notify reporter one test started
	virtual void OnSingleTestStart(const IUnitTest& test) = 0;

	//! Notify reporter one test finished, along with necessary results
	virtual void OnSingleTestFinish(const IUnitTest& test, float fRunTimeInMs, bool bSuccess, char const* szFailureDescription) = 0;
};

struct IUnitTestManager
{
	virtual ~IUnitTestManager(){}

	virtual IUnitTest* GetTestInstance(const CUnitTestInfo& info) = 0;

	//! Runs all test instances and returns exit code.
	//! \return 0 if all succeeded, non-zero when at least one test failed.
	virtual int  RunAllTests(EReporterType reporterType) = 0;

	virtual void RunAutoTests(const char* sSuiteName, const char* sTestName) = 0; //!< Currently not in use
	virtual void Update() = 0;//! Currently not in use

	//! 'callback' for failed tests, to prevent storing information in the exception, allowing use of setjmp/longjmp.
	virtual void SetExceptionCause(const char* expression, const char* file, int line) = 0;

	//! Helper called on module initialization. Do not use directly.
	void CreateTests(const char* moduleName);
};

#if defined(CRY_UNIT_TESTING_USE_EXCEPTIONS)
//! Helper classes.
class assert_exception
	: public std::exception
{
public:
	virtual ~assert_exception() throw() {}
	char m_description[256];
	char m_filename[256];
	int  m_lineNumber;
public:
	assert_exception(char const* szDescription, char const* szFilename, int lineNumber)
	{
		cry_strcpy(m_description, szDescription);
		cry_strcpy(m_filename, szFilename);
		m_lineNumber = lineNumber;
	}
	virtual char const* what() const throw()override { return m_description; };
};
#endif

//! Helper class whose sole purpose is to register static test instances defined in separate files to the system.
struct SUnitTestRegistrar : public CStaticInstanceList<SUnitTestRegistrar>
{
	SUnitTestRegistrar(STest& test, const char* suite, const char* name, const char* filename, int line)
		: test(test)
	{
		test.m_unitTestInfo.SetSuite(suite);
		test.m_unitTestInfo.SetName(name);
		test.m_unitTestInfo.SetFileName(filename);
		test.m_unitTestInfo.SetLineNumber(line);
	}

	STest& test;
};

inline void IUnitTestManager::CreateTests(const char* moduleName)
{
	for (SUnitTestRegistrar* pTestRegistrar = SUnitTestRegistrar::GetFirstInstance();
	     pTestRegistrar != nullptr;
	     pTestRegistrar = pTestRegistrar->GetNextInstance())
	{
		pTestRegistrar->test.m_unitTestInfo.SetModule(moduleName);
		GetTestInstance(pTestRegistrar->test.m_unitTestInfo);
	}
}

} // namespace CryUnitTest

//! Global Suite for all tests that do not specify suite.
namespace CryUnitTestSuite
{
inline const char* GetSuiteName() { return ""; }
}

//! Implementation, do not use directly
namespace CryUnitTestImpl
{
ILINE string FormatVar(float val)          { return string().Format("<float> %f", val); }
ILINE string FormatVar(double val)         { return string().Format("<double> %f", val); }
ILINE string FormatVar(int8 val)           { return string().Format("<int8> %d", val); }
ILINE string FormatVar(uint8 val)          { return string().Format("<uint8> %u (0x%x)", val, val); }
ILINE string FormatVar(int16 val)          { return string().Format("<int16> %d", val); }
ILINE string FormatVar(uint16 val)         { return string().Format("<uint16> %u (0x%x)", val, val); }
ILINE string FormatVar(int32 val)          { return string().Format("<int32> %d", val); }
ILINE string FormatVar(uint32 val)         { return string().Format("<uint32> %u (0x%x)", val, val); }
ILINE string FormatVar(int64 val)          { return string().Format("<int64> %" PRId64, val); }
ILINE string FormatVar(uint64 val)         { return string().Format("<uint64> %" PRIu64 " (0x" PRIx64 ")", val, val); }
ILINE string FormatVar(const char* val)    { return string().Format("\"%s\"", val); }
ILINE string FormatVar(const wchar_t* val) { return string().Format("\"%ls\"", val); }
ILINE string FormatVar(const string& val)  { return string().Format("\"%s\"", val.c_str()); }
ILINE string FormatVar(const wstring& val) { return string().Format("\"%ls\"", val.c_str()); }
ILINE string FormatVar(std::nullptr_t)     { return "nullptr"; }

template<typename T>
ILINE
typename std::enable_if<std::is_pointer<T>::value, string>::type
FormatVar(T val)
{
	return string().Format("<pointer> %p", val);
}

//! Fall-back overload for unsupported types: dump bytes
//! Taking const reference because not all types can be copied or copied without side effect.
template<typename T>
ILINE 
typename std::enable_if<!std::is_pointer<T>::value, string>::type
FormatVar(const T& val)
{
	string result = "<unknown> ";
	const char* separator = "";
	for (const uint8* p = reinterpret_cast<const uint8*>(std::addressof(val)), * end = p + sizeof(T);
	     p != end; ++p)
	{
		result.Append(separator);
		result.AppendFormat("%02x", *p);
		separator = " ";
	}
	return result;
}

//! Equality comparison
template<typename T, typename U>
ILINE bool AreEqual(T&& t, U&& u) { return t == u; }

//! Inequality comparison, note this is necessary as it is more accurate to use operator!= for the types rather than the negation of '=='
template<typename T, typename U>
ILINE bool AreInequal(T&& t, U&& u) { return t != u; }

//! Equality comparison overloaded for types that do not compare equality with '==', such as raw string.
ILINE bool AreEqual(const char* str1, const char* str2) { return strcmp(str1, str2) == 0; }

template<std::size_t M, std::size_t N>
ILINE bool AreEqual(const char(&str1)[M], const char(&str2)[N]) { return strcmp(str1, str2) == 0; }

ILINE bool AreInequal(const char* str1, const char* str2)       { return strcmp(str1, str2) != 0; }

template<std::size_t M, std::size_t N>
ILINE bool AreInequal(const char(&str1)[M], const char(&str2)[N]) { return strcmp(str1, str2) != 0; }

}

//! Specifies a new test.
#define CRY_UNIT_TEST(ClassName)                                                                                                                                           \
  class ClassName : public CryUnitTest::STest                                                                                                                              \
  {                                                                                                                                                                        \
    virtual void Run();                                                                                                                                                    \
  };                                                                                                                                                                       \
  ClassName auto_unittest_instance_ ## ClassName;                                                                                                                          \
  CryUnitTest::SUnitTestRegistrar autoreg_unittest_ ## ClassName(auto_unittest_instance_ ## ClassName, CryUnitTestSuite::GetSuiteName(), # ClassName, __FILE__, __LINE__); \
  void ClassName::Run()

//! Defines a fixture (intermediate base class) as the container of variables and methods shared by tests.
#define CRY_UNIT_TEST_FIXTURE(FixureName) \
  struct FixureName : public CryUnitTest::STest

//! Specifies a new test with a fixture as the base class.
#define CRY_UNIT_TEST_WITH_FIXTURE(ClassName, FixtureName)                                                                                                                 \
  class ClassName : public FixtureName                                                                                                                                     \
  {                                                                                                                                                                        \
    virtual void Run();                                                                                                                                                    \
  };                                                                                                                                                                       \
  ClassName auto_unittest_instance_ ## ClassName;                                                                                                                          \
  CryUnitTest::SUnitTestRegistrar autoreg_unittest_ ## ClassName(auto_unittest_instance_ ## ClassName, CryUnitTestSuite::GetSuiteName(), # ClassName, __FILE__, __LINE__); \
  void ClassName::Run()

//! Specifies a suite name to group tests locally together.
#define CRY_UNIT_TEST_SUITE(SuiteName)                      \
  namespace SuiteName {                                     \
  namespace CryUnitTestSuite {                              \
  inline char const* GetSuiteName() { return # SuiteName; } \
  }                                                         \
  }                                                         \
  namespace SuiteName

//! Fails and reports if the specified expression evaluates to false.
#define CRY_UNIT_TEST_ASSERT(condition)                                                                          \
  do                                                                                                             \
  {                                                                                                              \
  if (!(condition))                                                                                              \
  {                                                                                                              \
    CryDebugBreak();                                                                                             \
    gEnv->pSystem->GetITestSystem()->GetIUnitTestManager()->SetExceptionCause( # condition, __FILE__, __LINE__); \
  }                                                                                                              \
  } while (0)

//! Fails and reports if the specified floating point values are not equal with respect to epsilon.
#define CRY_UNIT_TEST_CHECK_CLOSE(valueA, valueB, epsilon)                                                            \
  do                                                                                                                  \
  {                                                                                                                   \
    if (!(IsEquivalent(valueA, valueB, epsilon)))                                                                     \
    {                                                                                                                 \
      CryDebugBreak();                                                                                                \
      string message = # valueA " != " # valueB " with epsilon " # epsilon " [";                                      \
      message.append(CryUnitTestImpl::FormatVar(valueA).c_str()).append(" != ");                                      \
      message.append(CryUnitTestImpl::FormatVar(valueB).c_str()).append("]");                                         \
      gEnv->pSystem->GetITestSystem()->GetIUnitTestManager()->SetExceptionCause(message.c_str(), __FILE__, __LINE__); \
    }                                                                                                                 \
  } while (0)

//! Fails and reports if the specified values are not equal.
#define CRY_UNIT_TEST_CHECK_EQUAL(valueA, valueB)                                                                     \
  do                                                                                                                  \
  {                                                                                                                   \
    if (!CryUnitTestImpl::AreEqual(valueA, valueB))                                                                   \
    {                                                                                                                 \
      CryDebugBreak();                                                                                                \
      string message = # valueA " != " # valueB " [";                                                                 \
      message.append(CryUnitTestImpl::FormatVar(valueA).c_str()).append(" != ");                                      \
      message.append(CryUnitTestImpl::FormatVar(valueB).c_str()).append("]");                                         \
      gEnv->pSystem->GetITestSystem()->GetIUnitTestManager()->SetExceptionCause(message.c_str(), __FILE__, __LINE__); \
    }                                                                                                                 \
  } while (0)

//! Fails and reports if the specified values are not different.
#define CRY_UNIT_TEST_CHECK_DIFFERENT(valueA, valueB)                                                                 \
  do                                                                                                                  \
  {                                                                                                                   \
    if (!CryUnitTestImpl::AreInequal(valueA, valueB))                                                                 \
    {                                                                                                                 \
      CryDebugBreak();                                                                                                \
      string message = # valueA " == " # valueB " [";                                                                 \
      message.append(CryUnitTestImpl::FormatVar(valueA).c_str()).append(" != ");                                      \
      message.append(CryUnitTestImpl::FormatVar(valueB).c_str()).append("]");                                         \
      gEnv->pSystem->GetITestSystem()->GetIUnitTestManager()->SetExceptionCause(message.c_str(), __FILE__, __LINE__); \
    }                                                                                                                 \
  } while (0)
//The last line must exist to compile on gcc!

//! \endcond