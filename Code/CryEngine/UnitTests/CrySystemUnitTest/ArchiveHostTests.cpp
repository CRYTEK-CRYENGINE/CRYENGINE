// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include <UnitTest.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/StringList.h>
#include <CrySerialization/SmartPtr.h>
#include <memory>
#include <CryCore/smartptr.h>
#include <Serialization/ArchiveHost.h>

struct SMember
{
	string name;
	float  weight;

	SMember()
		: weight(0.0f) {}

	void CheckEquality(const SMember& copy) const
	{
		REQUIRE(name == copy.name);
		REQUIRE(weight == copy.weight);
	}

	void Change(int index)
	{
		name = "Changed name ";
		name += (index % 10) + '0';
		weight = float(index);
	}

	void Serialize(Serialization::IArchive& ar)
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

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(baseMember, "baseMember");
	}

	virtual void CheckEquality(const CPolyBase* copy) const
	{
		REQUIRE(baseMember == copy->baseMember);
	}

	virtual bool IsDerivedA() const { return false; }
	virtual bool IsDerivedB() const { return false; }
protected:
	string baseMember;
};

class CPolyDerivedA : public CPolyBase
{
public:
	void Serialize(Serialization::IArchive& ar) override
	{
		CPolyBase::Serialize(ar);
		ar(derivedMember, "derivedMember");
	}

	bool IsDerivedA() const override { return true; }

	void CheckEquality(const CPolyBase* copyBase) const override
	{
		REQUIRE(copyBase->IsDerivedA());
		const CPolyDerivedA* copy = (CPolyDerivedA*)copyBase;
		REQUIRE(derivedMember == copy->derivedMember);

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

	void Serialize(Serialization::IArchive& ar) override
	{
		CPolyBase::Serialize(ar);
		ar(derivedMember, "derivedMember");
	}

	void CheckEquality(const CPolyBase* copyBase) const override
	{
		REQUIRE(copyBase->IsDerivedB());
		const CPolyDerivedB* copy = (const CPolyDerivedB*)copyBase;
		REQUIRE(derivedMember == copy->derivedMember);

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

	void Serialize(Serialization::IArchive& ar)
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
		REQUIRE(m_bool == rhs.m_bool);
		REQUIRE(m_char == rhs.m_char);
		REQUIRE(m_int8 == rhs.m_int8);
		REQUIRE(m_uint8 == rhs.m_uint8);
		REQUIRE(m_int16 == rhs.m_int16);
		REQUIRE(m_uint16 == rhs.m_uint16);
		REQUIRE(m_int32 == rhs.m_int32);
		REQUIRE(m_uint32 == rhs.m_uint32);
		REQUIRE(m_int64 == rhs.m_int64);
		REQUIRE(m_uint64 == rhs.m_uint64);
		REQUIRE(m_float == rhs.m_float);
		REQUIRE(m_double == rhs.m_double);
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

	void Serialize(Serialization::IArchive& ar)
	{
		ar(name, "name");
		ar(polyPtr, "polyPtr");
		ar(polyVector, "polyVector");
		ar(members, "members");
		{
			Serialization::StringListValue value(stringList, stringList[index]);
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
		REQUIRE(name == copy.name);
		REQUIRE(index == copy.index);

		REQUIRE(polyPtr != nullptr);
		REQUIRE(copy.polyPtr != nullptr);
		polyPtr->CheckEquality(copy.polyPtr);

		REQUIRE(members.size() == copy.members.size());
		for (size_t i = 0; i < members.size(); ++i)
		{
			members[i].CheckEquality(copy.members[i]);
		}

		REQUIRE(polyVector.size() == copy.polyVector.size());
		for (size_t i = 0; i < polyVector.size(); ++i)
		{
			if (polyVector[i] == 0)
			{
				REQUIRE(copy.polyVector[i] == nullptr);
				continue;
			}
			REQUIRE(copy.polyVector[i] != nullptr);
			polyVector[i]->CheckEquality(copy.polyVector[i]);
		}

		const size_t arrayLen = CRY_ARRAY_COUNT(array);
		for (size_t i = 0; i < arrayLen; ++i)
		{
			array[i].CheckEquality(copy.array[i]);
		}

		numericTypes.CheckEquality(copy.numericTypes);

		REQUIRE(stringToInt.size() == copy.stringToInt.size());
		for (size_t i = 0; i < stringToInt.size(); ++i)
		{
			REQUIRE(stringToInt[i] == copy.stringToInt[i]);
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

	Serialization::StringListStatic     stringList;
	std::vector<_smart_ptr<CPolyBase>>  polyVector;
	_smart_ptr<CPolyBase>               polyPtr;

	std::map<string, SMember>           stringToStructMap;

	SMember                             array[5];
};

SERIALIZATION_CLASS_NAME(CPolyBase, CPolyBase, "base", "Base")
SERIALIZATION_CLASS_NAME(CPolyBase, CPolyDerivedA, "derived_a", "Derived A")
SERIALIZATION_CLASS_NAME(CPolyBase, CPolyDerivedB, "derived_b", "Derived B")

TEST(ArchiveHostTest, JsonBasicTypes)
{
	std::unique_ptr<Serialization::IArchiveHost> host(Serialization::CreateArchiveHost());

	DynArray<char> bufChanged;
	CComplexClass objChanged;
	objChanged.Change();
	host->SaveJsonBuffer(bufChanged, Serialization::SStruct(objChanged));
	REQUIRE(!bufChanged.empty());

	DynArray<char> bufResaved;
	{
		CComplexClass obj;

		REQUIRE(host->LoadJsonBuffer(Serialization::SStruct(obj), bufChanged.data(), bufChanged.size()));
		REQUIRE(host->SaveJsonBuffer(bufResaved, Serialization::SStruct(obj)));
		REQUIRE(!bufResaved.empty());

		obj.CheckEquality(objChanged);
	}
	REQUIRE(bufChanged.size() == bufResaved.size());
	for (size_t i = 0; i < bufChanged.size(); ++i)
		REQUIRE(bufChanged[i] == bufResaved[i]);
}

// Expose an enumeration that can be saved to disk
enum class EMyEnum
{
	Value = 1,
	UnusedValue,
};

// Reflect the enum values to the serialization library
// This is required in order to serialize custom enums
YASLI_ENUM_BEGIN(EMyEnum, "MyEnum")
YASLI_ENUM(EMyEnum::Value, "Value", "Value")
YASLI_ENUM(EMyEnum::UnusedValue, "UnusedValue", "UnusedValue")
YASLI_ENUM_END()

TEST(ArchiveHostTest, SerializeJson)
{
	std::unique_ptr<Serialization::IArchiveHost> host(Serialization::CreateArchiveHost());

	struct SNativeJsonRepresentation
	{
		string myString;
		EMyEnum myEnum;
		std::vector<int> myVector;
		std::map<string, string> myMap;
		// Also test loading into map from legacy format
		std::map<string, string> myMapLegacy;

		void Serialize(Serialization::IArchive& ar)
		{
			ar(myString, "myString");
			ar(myEnum, "myEnum");
			ar(myVector, "myVector");
			ar(myMap, "myMap");
			ar(myMapLegacy, "myMapLegacy");
		}
	};

	const string parsedJson = "{"
		"\"myString\": \"STR\","
		"\"myEnum\": \"Value\","
		"\"myVector\": ["
			"0,"
			"1"
		"],"
		"\"myMap\": {"
			"\"first\": \"1\","
			"\"second\": \"2\""
		"},"
		"\"myMapLegacy\": ["
			"{ \"name\": \"0\", \"value\": \"1337\" },"
			"{ \"name\": \"1\", \"value\": \"9001\" }"
		"]"
	"}";

	SNativeJsonRepresentation parsedObject;

	// Validate that loading succeeded
	const bool wasParsed = host->LoadJsonBuffer(Serialization::SStruct(parsedObject), parsedJson.data(), parsedJson.length());
	REQUIRE(wasParsed);
	REQUIRE(parsedObject.myString == "STR");
	REQUIRE(parsedObject.myEnum == EMyEnum::Value);
	REQUIRE(parsedObject.myVector.size() == 2);
	REQUIRE(parsedObject.myVector[0] == 0);
	REQUIRE(parsedObject.myVector[1] == 1);
	REQUIRE(parsedObject.myMap.find("first") != parsedObject.myMap.end());
	REQUIRE(parsedObject.myMap["first"] == "1");
	REQUIRE(parsedObject.myMap.find("second") != parsedObject.myMap.end());
	REQUIRE(parsedObject.myMap["second"] == "2");
	REQUIRE(parsedObject.myMapLegacy.find("0") != parsedObject.myMapLegacy.end());
	REQUIRE(parsedObject.myMapLegacy["0"] == "1337");
	REQUIRE(parsedObject.myMapLegacy.find("1") != parsedObject.myMapLegacy.end());
	REQUIRE(parsedObject.myMapLegacy["1"] == "9001");

	// Validate that loading succeeds
	DynArray<char> buffer;
	host->SaveJsonBuffer(buffer, Serialization::SStruct(parsedObject));
	buffer.push_back('\0');

	const string expectedJson = "{"
		"\n\t\"myString\": \"STR\","
		"\n\t\"myEnum\": \"Value\","
		"\n\t\"myVector\": [ 0, 1 ],"
		"\n\t\"myMap\": { \"first\": \"1\", \"second\": \"2\" },"
		"\n\t\"myMapLegacy\": { \"0\": \"1337\", \"1\": \"9001\" }"
	"\n}";

	REQUIRE(string(buffer.data()) == expectedJson);
}

TEST(ArchiveHostTest, BinBasicTypes)
{
	std::unique_ptr<Serialization::IArchiveHost> host(Serialization::CreateArchiveHost());

	DynArray<char> bufChanged;
	CComplexClass objChanged;
	objChanged.Change();
	host->SaveBinaryBuffer(bufChanged, Serialization::SStruct(objChanged));
	REQUIRE(!bufChanged.empty());

	DynArray<char> bufResaved;
	{
		CComplexClass obj;

		REQUIRE(host->LoadBinaryBuffer(Serialization::SStruct(obj), bufChanged.data(), bufChanged.size()));
		REQUIRE(host->SaveBinaryBuffer(bufResaved, Serialization::SStruct(obj)));
		REQUIRE(!bufResaved.empty());

		obj.CheckEquality(objChanged);
	}
	REQUIRE(bufChanged.size() == bufResaved.size());
	for (size_t i = 0; i < bufChanged.size(); ++i)
		REQUIRE(bufChanged[i] == bufResaved[i]);
}
