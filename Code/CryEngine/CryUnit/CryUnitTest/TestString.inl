// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../CryUnitString.h"

CRY_TEST_WITH_FIXTURE(TestEmptyStringByDefault, TestCryUnitFixture)
{
	CryUnit::String str;
	ASSERT_ARE_EQUAL("", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestEmptyStringIfConstructorWithEmptyString, TestCryUnitFixture)
{
	CryUnit::String str("");
	ASSERT_ARE_EQUAL("", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestConstructorWithSomeString, TestCryUnitFixture)
{
	CryUnit::String str("test");
	ASSERT_ARE_EQUAL("test", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAppendEmptyString, TestCryUnitFixture)
{
	CryUnit::String str("test");
	str.Append("");

	ASSERT_ARE_EQUAL("test", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAppendString, TestCryUnitFixture)
{
	CryUnit::String str("test");
	str.Append(" trying to make it bigger");
	ASSERT_ARE_EQUAL("test trying to make it bigger", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestCopyEmptyString, TestCryUnitFixture)
{
	CryUnit::String emptyStr1;
	CryUnit::String emptyStr2(emptyStr1);

	ASSERT_ARE_EQUAL("", emptyStr2.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestCopyString, TestCryUnitFixture)
{
	CryUnit::String str1("test");
	CryUnit::String str2(str1);

	ASSERT_ARE_EQUAL("test", str2.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAssignOperatorEmptyString, TestCryUnitFixture)
{
	CryUnit::String emptyStr1("old");
	CryUnit::String emptyStr2;
	emptyStr1 = emptyStr2;

	ASSERT_ARE_EQUAL("", emptyStr1.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAssignOperatorString, TestCryUnitFixture)
{
	CryUnit::String str1("test with a quite bigger string");
	CryUnit::String str2("old");
	str2 = str1;

	ASSERT_ARE_EQUAL("test with a quite bigger string", str2.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestStringSelfAssignment, TestCryUnitFixture)
{
	CryUnit::String str1("test");
	str1 = str1;

	ASSERT_ARE_EQUAL("test", str1.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAssignEmptyString, TestCryUnitFixture)
{
	CryUnit::String emptyStr("old");
	emptyStr.Assign("");

	ASSERT_ARE_EQUAL("", emptyStr.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestAssignString, TestCryUnitFixture)
{
	CryUnit::String str;
	str.Assign("test with a quite bigger string");

	ASSERT_ARE_EQUAL("test with a quite bigger string", str.GetInternalData());
}

CRY_TEST_WITH_FIXTURE(TestEmptyStringsAreEqual, TestCryUnitFixture)
{
	CryUnit::String str1;
	CryUnit::String str2;

	ASSERT_ARE_EQUAL(str1, str2);
}

CRY_TEST_WITH_FIXTURE(TestEqualStringsAreEqual, TestCryUnitFixture)
{
	CryUnit::String str1("str");
	CryUnit::String str2("str");

	ASSERT_ARE_EQUAL(str1, str2);
}

CRY_TEST_WITH_FIXTURE(TestSameStringsAreEqual, TestCryUnitFixture)
{
	CryUnit::String str("str");

	ASSERT_ARE_EQUAL(str, str);
}

CRY_TEST_WITH_FIXTURE(TestNotEqualStringsAreNotEqual, TestCryUnitFixture)
{
	CryUnit::String str1("str1");
	CryUnit::String str2("str2");

	ASSERT_ARE_NOT_EQUAL(str1, str2);
}
