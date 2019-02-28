// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../TestFailedException.h"

#ifdef WIN32

CRY_TEST_WITH_FIXTURE(TestAssertAreEqual, TestCryUnitFixture)
{
	CryUnit::AssertAreEqual(10, 10, "", 0);
}

CRY_TEST_WITH_FIXTURE(TestAssertAreEqualForChars, TestCryUnitFixture)
{
	CryUnit::AssertAreEqual('a', 'a', "", 0);
}

CRY_TEST_WITH_FIXTURE(TestAssertAreNotEqual, TestCryUnitFixture)
{
	bool test_failed = false;
	try
	{
		CryUnit::AssertAreEqual(5, 10, "", 0);
	}
	catch (...)
	{
		test_failed = true;
	}

	ASSERT_IS_TRUE(test_failed);
}

CRY_TEST_WITH_FIXTURE(TestAssertAreEqualForCStrings, TestCryUnitFixture)
{
	const char str1[] = "XXX";
	const char str2[] = "XXX";
	ASSERT_ARE_EQUAL(str1, str2);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsTrue, TestCryUnitFixture)
{
	ASSERT_IS_TRUE(true);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsFalse, TestCryUnitFixture)
{
	ASSERT_IS_FALSE(false);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotTrue, TestCryUnitFixture)
{
	bool exceptionThrown = false;
	try
	{
		ASSERT_IS_TRUE(false);
	}
	catch (...)
	{
		exceptionThrown = true;
	}

	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotFalse, TestCryUnitFixture)
{
	bool exceptionThrown = false;
	try
	{
		ASSERT_IS_FALSE(true);
	}
	catch (...)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFail, TestCryUnitFixture)
{
	bool failExceptionThrown = false;
	try
	{
		CRY_UNIT_FAIL();
	}
	catch (const CryUnit::ITestFailedException&)
	{
		failExceptionThrown = true;
	}

	ASSERT_IS_TRUE(failExceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFailWithMessageForCString, TestCryUnitFixture)
{
	bool failExceptionThrown = false;
	const char* expectedMessage = "FailMessage";
	try
	{
		CRY_UNIT_FAIL_MESSAGE(expectedMessage);
	}
	catch (const CryUnit::ITestFailedException& testFailedException)
	{
		failExceptionThrown = true;
		ASSERT_ARE_EQUAL(expectedMessage, testFailedException.GetMessage());
	}

	ASSERT_IS_TRUE(failExceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFailWithMessage, TestCryUnitFixture)
{
	bool failExceptionThrown = false;
	const char* expectedMessage = "FailMessage";
	try
	{
		CRY_UNIT_FAIL_MESSAGE(expectedMessage);
	}
	catch (const CryUnit::ITestFailedException& testFailedException)
	{
		failExceptionThrown = true;
		ASSERT_ARE_EQUAL(expectedMessage, testFailedException.GetMessage());
	}

	ASSERT_IS_TRUE(failExceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFailWithMessageAndErrorString, TestCryUnitFixture)
{
	const char expectedMessage[] = "ErrorMessage";
	try
	{
		CRY_UNIT_FAIL_MESSAGE(expectedMessage);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL(expectedMessage, exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertAreEqualGivesCorrectErrorMessageForInts, TestCryUnitFixture)
{
	try
	{
		ASSERT_ARE_EQUAL(2, 3);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("AssertAreEqual failed: Expected: '2', got: '3'", exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertAreNotEqualGivesCorrectErrorMessageForInts, TestCryUnitFixture)
{
	try
	{
		ASSERT_ARE_NOT_EQUAL(2, 2);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("AssertAreNotEqual failed: Expected to not be: '2'", exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertAreEqualGivesCorrectErrorMessageForStrings, TestCryUnitFixture)
{
	try
	{
		ASSERT_ARE_EQUAL("2", "3");
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("AssertAreEqual failed: Expected: '2', got: '3'", exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertAreNotEqualGivesCorrectErrorMessageForStrings, TestCryUnitFixture)
{
	try
	{
		ASSERT_ARE_NOT_EQUAL("2", "2");
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("AssertAreNotEqual failed: Expected to not be: '2'", exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertIsTrueGivesCorrectErrorMessageForInts, TestCryUnitFixture)
{
	try
	{
		ASSERT_IS_TRUE(false);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("AssertIsTrue failed.", exception.GetMessage());
		return;
	}

	CRY_UNIT_FAIL();
}

CRY_TEST_WITH_FIXTURE(TestAssertIsTrueGetsCorrectFileNameAndLineWhenFailing, TestCryUnitFixture)
{
	try
	{
		CryUnit::AssertIsTrue(false, "file", 10);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		ASSERT_ARE_EQUAL("file", exception.GetFilePath());
		ASSERT_ARE_EQUAL(10, exception.GetFileLine());
	}
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNullMacro, TestCryUnitFixture)
{
	ASSERT_IS_NULL((void*)0);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNullMacroFail, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		ASSERT_IS_NULL((void*)1);
	}
	catch (const CryUnit::ITestFailedException&)
	{
		exceptionThrown = true;
	}

	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotNullMacro, TestCryUnitFixture)
{
	ASSERT_IS_NOT_NULL((void*)1);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotNullMacroFail, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		ASSERT_IS_NOT_NULL((void*)0);
	}
	catch (const CryUnit::ITestFailedException&)
	{
		exceptionThrown = true;
	}

	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNullGetsCorrectFileNameAndLineWhenFailing, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		CryUnit::AssertIsNull((void*)1, "file", 10);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		exceptionThrown = true;
		ASSERT_ARE_EQUAL("file", exception.GetFilePath());
		ASSERT_ARE_EQUAL(10, exception.GetFileLine());
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotNullGetsCorrectFileNameAndLineWhenFailing, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		CryUnit::AssertIsNotNull((void*)0, "file", 10);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		exceptionThrown = true;
		ASSERT_ARE_EQUAL("file", exception.GetFilePath());
		ASSERT_ARE_EQUAL(10, exception.GetFileLine());
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNullFailureMessage, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		ASSERT_IS_NULL((void*)1);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		exceptionThrown = true;
		ASSERT_ARE_EQUAL("AssertIsNull failed: Pointer is not NULL", exception.GetMessage());

	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestAssertIsNotNullFailureMessage, TestCryUnitFixture)
{
	bool exceptionThrown = false;

	try
	{
		ASSERT_IS_NOT_NULL((void*)0);
	}
	catch (const CryUnit::ITestFailedException& exception)
	{
		exceptionThrown = true;
		ASSERT_ARE_EQUAL("AssertIsNotNull failed: Pointer is NULL", exception.GetMessage());
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFailMessageWithMultibyteString, TestCryUnitFixture)
{
	bool failExceptionThrown = false;
	try
	{
		CryUnit::Fail("", "message", "", 0);
	}
	catch (const CryUnit::ITestFailedException& testFailedException)
	{
		failExceptionThrown = true;
		ASSERT_ARE_EQUAL("", testFailedException.GetCondition());
		ASSERT_ARE_EQUAL("message", testFailedException.GetMessage());
		ASSERT_ARE_EQUAL("", testFailedException.GetFilePath());
		ASSERT_ARE_EQUAL(0, testFailedException.GetFileLine());

	}

	ASSERT_IS_TRUE(failExceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFailErrorMessageFileLineWithMultibyteString, TestCryUnitFixture)
{
	bool failExceptionThrown = false;
	try
	{
		CryUnit::Fail("condition", "message", "file", 1);
	}
	catch (const CryUnit::ITestFailedException& testFailedException)
	{
		failExceptionThrown = true;
		ASSERT_ARE_EQUAL("condition", testFailedException.GetCondition());
		ASSERT_ARE_EQUAL("message", testFailedException.GetMessage());
		ASSERT_ARE_EQUAL("file", testFailedException.GetFilePath());
		ASSERT_ARE_EQUAL(1, testFailedException.GetFileLine());
	}

	ASSERT_IS_TRUE(failExceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFloatAreEqualWithEpsilon, TestCryUnitFixture)
{
	float actual = 1.559f;
	float expected = 1.561f;
	float epsilon = 0.01f;

	CryUnit::AssertFloatAreEqual(expected, actual, epsilon, "", 0);
}

CRY_TEST_WITH_FIXTURE(TestFloatAreEqualWithEpsilonFails, TestCryUnitFixture)
{
	float actual = 1.559f;
	float expected = 1.561f;
	float epsilon = 0.001f;

	bool exceptionThrown = false;

	try
	{
		CryUnit::AssertFloatAreEqual(expected, actual, epsilon, "", 0);
	}
	catch (const CryUnit::ITestFailedException&)
	{
		exceptionThrown = true;
	}
	ASSERT_IS_TRUE(exceptionThrown);
}

CRY_TEST_WITH_FIXTURE(TestFloatAreEqualFailFileNameLine, TestCryUnitFixture)
{
	float actual = 1.559f;
	float expected = 1.561f;
	float epsilon = 0.001f;

	try
	{
		CryUnit::AssertFloatAreEqual(expected, actual, epsilon, "FileName", 12);
		CRY_UNIT_FAIL();
	}
	catch (const CryUnit::ITestFailedException& testFailedException)
	{
		ASSERT_ARE_EQUAL("", testFailedException.GetCondition());
		ASSERT_ARE_EQUAL("AssertFloatAreEqual failed: Expected: '1.56', got: '1.56'", testFailedException.GetMessage());
		ASSERT_ARE_EQUAL("FileName", testFailedException.GetFilePath());
		ASSERT_ARE_EQUAL(12, testFailedException.GetFileLine());
	}
}

#endif
