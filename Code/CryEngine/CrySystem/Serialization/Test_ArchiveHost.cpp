// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ArchiveHost.h"
#include <CrySystem/CryUnitTest.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/SmartPtr.h>
#include <memory>

#if defined(CRY_UNIT_TESTING)
namespace Serialization
{
struct SMember
{
	string name;
	float  weight;

	SMember()
		: weight(0.0f) {}

	void CheckEquality(const SMember& copy) const
	{
		CRY_UNIT_TEST_ASSERT(name == copy.name);
		CRY_UNIT_TEST_ASSERT(weight == copy.weight);
	}

	void Change(int index)
	{
		name = "Changed name ";
		name += (index % 10) + '0';
		weight = float(index);
	}

	void Serialize(IArchive& ar)
	{
		ar(name, "name");
		ar(weight, "weight");
	}
};

class CPolyBase : public _i_reference_target_t
{
public:
	CPolyBase()
	{
		baseMember = "Regular base member";
	}

	virtual void Change()
	{
		baseMember = "Changed base member";
	}

	virtual void Serialize(IArchive& ar)
	{
		ar(baseMember, "baseMember");
	}

	virtual void CheckEquality(const CPolyBase* copy) const
	{
		CRY_UNIT_TEST_ASSERT(baseMember == copy->baseMember);
	}

	virtual bool IsDerivedA() const { return false; }
	virtual bool IsDerivedB() const { return false; }
protected:
	string baseMember;
};

class CPolyDerivedA : public CPolyBase
{
public:
	void Serialize(IArchive& ar) override
	{
		CPolyBase::Serialize(ar);
		ar(derivedMember, "derivedMember");
	}

	bool IsDerivedA() const override { return true; }

	void CheckEquality(const CPolyBase* copyBase) const override
	{
		CRY_UNIT_TEST_ASSERT(copyBase->IsDerivedA());
		const CPolyDerivedA* copy = (CPolyDerivedA*)copyBase;
		CRY_UNIT_TEST_ASSERT(derivedMember == copy->derivedMember);

		CPolyBase::CheckEquality(copyBase);
	}
protected:
	string derivedMember;
};

class CPolyDerivedB : public CPolyBase
{
public:
	CPolyDerivedB()
		: derivedMember("B Derived")
	{
	}

	bool IsDerivedB() const override { return true; }

	void Serialize(IArchive& ar) override
	{
		CPolyBase::Serialize(ar);
		ar(derivedMember, "derivedMember");
	}

	void CheckEquality(const CPolyBase* copyBase) const override
	{
		CRY_UNIT_TEST_ASSERT(copyBase->IsDerivedB());
		const CPolyDerivedB* copy = (const CPolyDerivedB*)copyBase;
		CRY_UNIT_TEST_ASSERT(derivedMember == copy->derivedMember);

		CPolyBase::CheckEquality(copyBase);
	}
protected:
	string derivedMember;
};

struct SNumericTypes
{
	SNumericTypes()
		: m_bool(false)
		, m_char(0)
		, m_int8(0)
		, m_uint8(0)
		, m_int16(0)
		, m_uint16(0)
		, m_int32(0)
		, m_uint32(0)
		, m_int64(0)
		, m_uint64(0)
		, m_float(0.0f)
		, m_double(0.0)
	{
	}

	void Change()
	{
		m_bool = true;
		m_char = -1;
		m_int8 = -2;
		m_uint8 = 0xff - 3;
		m_int16 = -6;
		m_uint16 = 0xff - 7;
		m_int32 = -4;
		m_uint32 = -5;
		m_int64 = -8ll;
		m_uint64 = 9ull;
		m_float = -10.0f;
		m_double = -11.0;
	}

	void Serialize(IArchive& ar)
	{
		ar(m_bool, "bool");
		ar(m_char, "char");
		ar(m_int8, "int8");
		ar(m_uint8, "uint8");
		ar(m_int16, "int16");
		ar(m_uint16, "uint16");
		ar(m_int32, "int32");
		ar(m_uint32, "uint32");
		ar(m_int64, "int64");
		ar(m_uint64, "uint64");
		ar(m_float, "float");
		ar(m_double, "double");
	}

	void CheckEquality(const SNumericTypes& rhs) const
	{
		CRY_UNIT_TEST_ASSERT(m_bool == rhs.m_bool);
		CRY_UNIT_TEST_ASSERT(m_char == rhs.m_char);
		CRY_UNIT_TEST_ASSERT(m_int8 == rhs.m_int8);
		CRY_UNIT_TEST_ASSERT(m_uint8 == rhs.m_uint8);
		CRY_UNIT_TEST_ASSERT(m_int16 == rhs.m_int16);
		CRY_UNIT_TEST_ASSERT(m_uint16 == rhs.m_uint16);
		CRY_UNIT_TEST_ASSERT(m_int32 == rhs.m_int32);
		CRY_UNIT_TEST_ASSERT(m_uint32 == rhs.m_uint32);
		CRY_UNIT_TEST_ASSERT(m_int64 == rhs.m_int64);
		CRY_UNIT_TEST_ASSERT(m_uint64 == rhs.m_uint64);
		CRY_UNIT_TEST_ASSERT(m_float == rhs.m_float);
		CRY_UNIT_TEST_ASSERT(m_double == rhs.m_double);
	}

	bool   m_bool;

	char   m_char;
	int8   m_int8;
	uint8  m_uint8;

	int16  m_int16;
	uint16 m_uint16;

	int32  m_int32;
	uint32 m_uint32;

	int64  m_int64;
	uint64 m_uint64;

	float  m_float;
	double m_double;
};

class CComplexClass
{
public:
	CComplexClass()
		: index(0)
	{
		name = "Foo";
		stringList.push_back("Choice 1");
		stringList.push_back("Choice 2");
		stringList.push_back("Choice 3");

		polyPtr.reset(new CPolyDerivedA());

		polyVector.push_back(new CPolyDerivedB);
		polyVector.push_back(new CPolyBase);

		SMember& a = stringToStructMap["a"];
		a.name = "A";
		SMember& b = stringToStructMap["b"];
		b.name = "B";

		members.resize(13);

		intToString.push_back(std::make_pair(1, "one"));
		intToString.push_back(std::make_pair(2, "two"));
		intToString.push_back(std::make_pair(3, "three"));
		stringToInt.push_back(std::make_pair("one", 1));
		stringToInt.push_back(std::make_pair("two", 2));
		stringToInt.push_back(std::make_pair("three", 3));
	}

	void Change()
	{
		name = "Slightly changed name";
		index = 2;
		polyPtr.reset(new CPolyDerivedB());
		polyPtr->Change();

		for (size_t i = 0; i < members.size(); ++i)
			members[i].Change(int(i));

		members.erase(members.begin());

		for (size_t i = 0; i < polyVector.size(); ++i)
			polyVector[i]->Change();

		polyVector.resize(4);
		polyVector.push_back(new CPolyBase());
		polyVector[4]->Change();

		const size_t arrayLen = CRY_ARRAY_COUNT(array);
		for (size_t i = 0; i < arrayLen; ++i)
			array[i].Change(int(arrayLen - i));

		numericTypes.Change();

		vectorOfStrings.push_back("str1");
		vectorOfStrings.push_back("2str");
		vectorOfStrings.push_back("thirdstr");

		stringToStructMap.erase("a");
		SMember& c = stringToStructMap["c"];
		c.name = "C";

		intToString.push_back(std::make_pair(4, "four"));
		stringToInt.push_back(std::make_pair("four", 4));
	}

	void Serialize(IArchive& ar)
	{
		ar(name, "name");
		ar(polyPtr, "polyPtr");
		ar(polyVector, "polyVector");
		ar(members, "members");
		{
			StringListValue value(stringList, stringList[index]);
			ar(value, "stringList");
			index = value.index();
			if (index == -1)
				index = 0;
		}
		ar(array, "array");
		ar(numericTypes, "numericTypes");
		ar(vectorOfStrings, "vectorOfStrings");
		ar(stringToInt, "stringToInt");
	}

	void CheckEquality(const CComplexClass& copy) const
	{
		CRY_UNIT_TEST_ASSERT(name == copy.name);
		CRY_UNIT_TEST_ASSERT(index == copy.index);

		CRY_UNIT_TEST_ASSERT(polyPtr != 0);
		CRY_UNIT_TEST_ASSERT(copy.polyPtr != 0);
		polyPtr->CheckEquality(copy.polyPtr);

		CRY_UNIT_TEST_ASSERT(members.size() == copy.members.size());
		for (size_t i = 0; i < members.size(); ++i)
		{
			members[i].CheckEquality(copy.members[i]);
		}

		CRY_UNIT_TEST_ASSERT(polyVector.size() == copy.polyVector.size());
		for (size_t i = 0; i < polyVector.size(); ++i)
		{
			if (polyVector[i] == 0)
			{
				CRY_UNIT_TEST_ASSERT(copy.polyVector[i] == 0);
				continue;
			}
			CRY_UNIT_TEST_ASSERT(copy.polyVector[i] != 0);
			polyVector[i]->CheckEquality(copy.polyVector[i]);
		}

		const size_t arrayLen = CRY_ARRAY_COUNT(array);
		for (size_t i = 0; i < arrayLen; ++i)
		{
			array[i].CheckEquality(copy.array[i]);
		}

		numericTypes.CheckEquality(copy.numericTypes);

		CRY_UNIT_TEST_ASSERT(stringToInt.size() == copy.stringToInt.size());
		for (size_t i = 0; i < stringToInt.size(); ++i)
		{
			CRY_UNIT_TEST_ASSERT(stringToInt[i] == copy.stringToInt[i]);
		}
	}
protected:
	string                              name;
	typedef std::vector<SMember> Members;
	std::vector<string>                 vectorOfStrings;
	std::vector<std::pair<int, string>> intToString;
	std::vector<std::pair<string, int>> stringToInt;
	Members                             members;
	int32                               index;
	SNumericTypes                       numericTypes;

	StringListStatic                    stringList;
	std::vector<_smart_ptr<CPolyBase>>  polyVector;
	_smart_ptr<CPolyBase>               polyPtr;

	std::map<string, SMember>           stringToStructMap;

	SMember                             array[5];
};

SERIALIZATION_CLASS_NAME(CPolyBase, CPolyBase, "base", "Base")
SERIALIZATION_CLASS_NAME(CPolyBase, CPolyDerivedA, "derived_a", "Derived A")
SERIALIZATION_CLASS_NAME(CPolyBase, CPolyDerivedB, "derived_b", "Derived B")

CRY_UNIT_TEST_SUITE(ArchiveHost)
{
	CRY_UNIT_TEST(JsonBasicTypes)
	{
		std::unique_ptr<IArchiveHost> host(CreateArchiveHost());

		DynArray<char> bufChanged;
		CComplexClass objChanged;
		objChanged.Change();
		host->SaveJsonBuffer(bufChanged, SStruct(objChanged));
		CRY_UNIT_TEST_ASSERT(!bufChanged.empty());

		DynArray<char> bufResaved;
		{
			CComplexClass obj;

			CRY_UNIT_TEST_ASSERT(host->LoadJsonBuffer(SStruct(obj), bufChanged.data(), bufChanged.size()));
			CRY_UNIT_TEST_ASSERT(host->SaveJsonBuffer(bufResaved, SStruct(obj)));
			CRY_UNIT_TEST_ASSERT(!bufResaved.empty());

			obj.CheckEquality(objChanged);
		}
		CRY_UNIT_TEST_ASSERT(bufChanged.size() == bufResaved.size());
		for (size_t i = 0; i < bufChanged.size(); ++i)
			CRY_UNIT_TEST_ASSERT(bufChanged[i] == bufResaved[i]);
	}

	CRY_UNIT_TEST(BinBasicTypes)
	{
		std::unique_ptr<IArchiveHost> host(CreateArchiveHost());

		DynArray<char> bufChanged;
		CComplexClass objChanged;
		objChanged.Change();
		host->SaveBinaryBuffer(bufChanged, SStruct(objChanged));
		CRY_UNIT_TEST_ASSERT(!bufChanged.empty());

		DynArray<char> bufResaved;
		{
			CComplexClass obj;

			CRY_UNIT_TEST_ASSERT(host->LoadBinaryBuffer(SStruct(obj), bufChanged.data(), bufChanged.size()));
			CRY_UNIT_TEST_ASSERT(host->SaveBinaryBuffer(bufResaved, SStruct(obj)));
			CRY_UNIT_TEST_ASSERT(!bufResaved.empty());

			obj.CheckEquality(objChanged);
		}
		CRY_UNIT_TEST_ASSERT(bufChanged.size() == bufResaved.size());
		for (size_t i = 0; i < bufChanged.size(); ++i)
			CRY_UNIT_TEST_ASSERT(bufChanged[i] == bufResaved[i]);
	}
}
}
#endif
