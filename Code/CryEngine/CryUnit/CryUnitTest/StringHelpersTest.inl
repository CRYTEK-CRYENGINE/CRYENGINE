// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySystem/CryUnitTest.h>
#include "../CryUnitAsserts.h"
#include "../StringHelpers.h"
using namespace CryUnit::StringHelpers;

CRY_TEST_WITH_FIXTURE(StringBufferAppendInt, TestCryUnitFixture)
{
	StringBuffer stringBuffer;
	stringBuffer.Append(12);
	ASSERT_ARE_EQUAL("12", stringBuffer.ToString());
}

CRY_TEST_WITH_FIXTURE(StringBufferAppendUnsinged, TestCryUnitFixture)
{
	StringBuffer stringBuffer;
	stringBuffer.Append(12u);
	ASSERT_ARE_EQUAL("12", stringBuffer.ToString());
}

CRY_TEST_WITH_FIXTURE(StringBufferAppendCharString, TestCryUnitFixture)
{
	StringBuffer stringBuffer;
	stringBuffer.Append("str");
	ASSERT_ARE_EQUAL("str", stringBuffer.ToString());
}

CRY_TEST_WITH_FIXTURE(StringBufferAppendFloat, TestCryUnitFixture)
{
	StringBuffer stringBuffer;
	stringBuffer.Append(12.01f);
	ASSERT_ARE_EQUAL("12.01", stringBuffer.ToString());
}

CRY_TEST_WITH_FIXTURE(StringBufferAppendMixed, TestCryUnitFixture)
{
	StringBuffer stringBuffer;
	stringBuffer.Append(12.01f);
	stringBuffer.Append(" ");
	stringBuffer.Append("done");
	ASSERT_ARE_EQUAL("12.01 done", stringBuffer.ToString());
}
