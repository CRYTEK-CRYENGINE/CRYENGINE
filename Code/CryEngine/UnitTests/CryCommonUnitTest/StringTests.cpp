// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryString/CryString.h>
#include <string>

TEST(CryStringTest, Construction)
{
	const char* szStr = nullptr;
	EXPECT_NO_FATAL_FAILURE({ string str = szStr; });
}

TEST(CryStringTest, FindLastOf)
{
	//////////////////////////////////////////////////////////////////////////
	// Based on MS documentation of find_last_of
	string strTestFindLastOfOverload1("abcd-1234-abcd-1234");
	string strTestFindLastOfOverload2("ABCD-1234-ABCD-1234");
	string strTestFindLastOfOverload3("456-EFG-456-EFG");
	string strTestFindLastOfOverload4("12-ab-12-ab");

	const char* szCstr2 = "B1";
	const char* szCstr2b = "D2";
	const char* szCstr3a = "5EZZZZZZ";
	string str4a("ba3");
	string str4b("a2");

	size_t nPosition(string::npos);

	nPosition = strTestFindLastOfOverload1.find_last_of('d', 14);
	REQUIRE(nPosition == 13);

	nPosition = strTestFindLastOfOverload2.find_last_of(szCstr2, 12);
	REQUIRE(nPosition == 11);

	nPosition = strTestFindLastOfOverload2.find_last_of(szCstr2b);
	REQUIRE(nPosition == 16);

	nPosition = strTestFindLastOfOverload3.find_last_of(szCstr3a, 8, 8);
	REQUIRE(nPosition == 4);

	nPosition = strTestFindLastOfOverload4.find_last_of(str4a, 8);
	REQUIRE(nPosition == 4);

	nPosition = strTestFindLastOfOverload4.find_last_of(str4b);
	REQUIRE(nPosition == 9);
}

TEST(CryStringTest, FindLastNotOf)
{
	//////////////////////////////////////////////////////////////////////////
	// Based on MS documentation of find_last_not_of
	string strTestFindLastNotOfOverload1("dddd-1dd4-abdd");
	string strTestFindLastNotOfOverload2("BBB-1111");
	string strTestFindLastNotOfOverload3("444-555-GGG");
	string strTestFindLastNotOfOverload4("12-ab-12-ab");

	const char* szCstr2NF = "B1";
	const char* szCstr3aNF = "45G";
	const char* szCstr3bNF = "45G";

	size_t position(string::npos);

	string str4aNF("b-a");
	string str4bNF("12");

	size_t nPosition3A(string::npos);

	position = strTestFindLastNotOfOverload1.find_last_not_of('d', 7);
	REQUIRE(position == 5);

	position = strTestFindLastNotOfOverload1.find_last_not_of("d");
	REQUIRE(position == 11);

	position = strTestFindLastNotOfOverload2.find_last_not_of(szCstr2NF, 6);
	REQUIRE(position == 3);

	position = strTestFindLastNotOfOverload3.find_last_not_of(szCstr3aNF);
	REQUIRE(position == 7);

	position = strTestFindLastNotOfOverload3.find_last_not_of(szCstr3bNF, 6, 3);    //nPosition - 1 );
	REQUIRE(position == 3);

	position = strTestFindLastNotOfOverload4.find_last_not_of(str4aNF, 5);
	REQUIRE(position == 1);

	position = strTestFindLastNotOfOverload4.find_last_not_of(str4bNF);
	REQUIRE(position == 10);
}

TEST(CryFixedStringTest, All)
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
	REQUIRE(str1 == "abcd");

	str2 = "efg";
	REQUIRE(str2 == "efg");

	str2 = str1;
	REQUIRE(str2 == "abcd");

	str1 += "XY";
	REQUIRE(str1 == "abcdXY");

	str2 += "efghijk";
	REQUIRE(str2 == "abcdefghijk");

	str1.replace("bc", "");
	REQUIRE(str1 == "adXY");

	str1.replace("XY", "1234");
	REQUIRE(str1 == "ad1234");

	str1.replace("1234", "1234567890");
	REQUIRE(str1 == "ad1234567890");

	str1.reserve(200);
	REQUIRE(str1 == "ad1234567890");
	REQUIRE(str1.capacity() == 200);
	str1.reserve(0);
	REQUIRE(str1 == "ad1234567890");
	REQUIRE(str1.capacity() == str1.length());

	str1.erase(7); // doesn't change capacity
	REQUIRE(str1 == "ad12345");

	str4.assign("abc");
	REQUIRE(str4 == "abc");
	str4.reserve(9);
	REQUIRE(str4.capacity() >= 9);  // capacity is always >= MAX_SIZE-1
	str4.reserve(0);
	REQUIRE(str4.capacity() >= 9);  // capacity is always >= MAX_SIZE-1

	size_t idx = str1.find("123");
	REQUIRE(idx == 2);

	idx = str1.find("123", 3);
	REQUIRE(idx == str1.npos);

	wstr1 = L"abc";
	REQUIRE(wstr1 == L"abc");
	REQUIRE(wstr1.compare(L"aBc") > 0);
	REQUIRE(wstr1.compare(L"babc") < 0);
	REQUIRE(wstr1.compareNoCase(L"aBc") == 0);
	str1.Format("This is a %s %ls with %d params", "mixed", L"string", 3);
	str2.Format("This is a %ls %s with %d params", L"mixed", "string", 3);
	REQUIRE(str1 == "This is a mixed string with 3 params");
	REQUIRE(str1 == str2);
}

