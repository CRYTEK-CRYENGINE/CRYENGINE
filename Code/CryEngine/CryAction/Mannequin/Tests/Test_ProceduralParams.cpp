// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <ICryMannequin.h>
#include <Mannequin/Serialization.h>
#include <CrySystem/CryUnitTest.h>

namespace mannequin
{
namespace test
{
struct STestParams
	: public IProceduralParams
{
	STestParams(const string& paramName, const int param)
		: m_paramName(paramName)
		, m_param(param)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(m_param, m_paramName.c_str(), m_paramName.c_str());
	}

	const string m_paramName;
	int          m_param;
};
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(IProceduralParams_OperatorEquals_Self)
{
	test::STestParams param("Param", 3);
	CRY_UNIT_TEST_ASSERT(param == param);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(IProceduralParams_OperatorEquals_DifferentInstancesEqual)
{
	test::STestParams param1("Param", 3);
	test::STestParams param2("Param", 3);
	CRY_UNIT_TEST_ASSERT(param1 == param2);
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(IProceduralParams_OperatorEquals_DifferentParamNames)
{
	test::STestParams param1("Param1", 3);
	test::STestParams param2("Param2", 3);
	CRY_UNIT_TEST_ASSERT(!(param1 == param2));
}

//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(IProceduralParams_OperatorEquals_DifferentParamValues)
{
	test::STestParams param1("Param", 3);
	test::STestParams param2("Param", 2);
	CRY_UNIT_TEST_ASSERT(!(param1 == param2));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CRY_UNIT_TEST(IProceduralParams_StringWrapper_EmptyConstructor)
{
	const IProceduralParams::StringWrapper s;
	CRY_UNIT_TEST_ASSERT(string("") == s.c_str());
}

CRY_UNIT_TEST(IProceduralParams_StringWrapper_Constructor)
{
	const IProceduralParams::StringWrapper s("test");
	CRY_UNIT_TEST_ASSERT(string("test") == s.c_str());
}

CRY_UNIT_TEST(IProceduralParams_StringWrapper_OperatorEqual)
{
	IProceduralParams::StringWrapper s("test");
	CRY_UNIT_TEST_ASSERT(string("test") == s.c_str());

	s = "t";
	CRY_UNIT_TEST_ASSERT(string("t") == s.c_str());

	s = "test";
	CRY_UNIT_TEST_ASSERT(string("test") == s.c_str());

	IProceduralParams::StringWrapper s2;
	CRY_UNIT_TEST_ASSERT(string("") == s2.c_str());

	s2 = s = IProceduralParams::StringWrapper("test2");
	CRY_UNIT_TEST_ASSERT(string("test2") == s.c_str());
	CRY_UNIT_TEST_ASSERT(string("test2") == s2.c_str());
}

CRY_UNIT_TEST(IProceduralParams_StringWrapper_GetLength)
{
	IProceduralParams::StringWrapper s("test");
	CRY_UNIT_TEST_ASSERT(4 == s.GetLength());

	s = "t";
	CRY_UNIT_TEST_ASSERT(1 == s.GetLength());

	s = "test";
	CRY_UNIT_TEST_ASSERT(4 == s.GetLength());

	const IProceduralParams::StringWrapper s2("test2");
	CRY_UNIT_TEST_ASSERT(5 == s2.GetLength());
}

CRY_UNIT_TEST(IProceduralParams_StringWrapper_IsEmpty)
{
	{
		IProceduralParams::StringWrapper s("test");
		CRY_UNIT_TEST_ASSERT(!s.IsEmpty());

		s = "";
		CRY_UNIT_TEST_ASSERT(s.IsEmpty());

		s = "test";
		CRY_UNIT_TEST_ASSERT(!s.IsEmpty());
	}

	{
		const IProceduralParams::StringWrapper s("test");
		CRY_UNIT_TEST_ASSERT(!s.IsEmpty());
	}

	{
		const IProceduralParams::StringWrapper s("");
		CRY_UNIT_TEST_ASSERT(s.IsEmpty());
	}

	{
		const IProceduralParams::StringWrapper s;
		CRY_UNIT_TEST_ASSERT(s.IsEmpty());
	}
}
}
