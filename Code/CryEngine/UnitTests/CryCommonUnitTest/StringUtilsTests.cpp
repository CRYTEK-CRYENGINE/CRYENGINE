// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <vector>
#include <CryString/CryStringUtils.h>

TEST(CryStringUtilsTest, Strcpy)
{
	REQUIRE(!cry_strcpy(nullptr, 0, nullptr));
	REQUIRE(!cry_strcpy(nullptr, 0, nullptr, 0));
	REQUIRE(!cry_strcpy(nullptr, 1, nullptr));
	REQUIRE(!cry_strcpy(nullptr, 1, nullptr, 1));
	REQUIRE(!cry_strcpy(nullptr, 1, ""));
	REQUIRE(!cry_strcpy(nullptr, 1, "", 1));

	char bf[4];
	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 0, ""));
	REQUIRE(!memcmp(bf, "abcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 0, "", 1));
	REQUIRE(!memcmp(bf, "abcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 1, nullptr));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 1, nullptr, 0));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, nullptr));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 3, "qwerty"));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 3, "qwerty", 4));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, 3, "qwerty", 3));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, 3, "qwerty", 2));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, 3, "qwerty", 1));
	REQUIRE(!memcmp(bf, "q\000cd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, 3, "qwerty", 0));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, "qwerty"));
	REQUIRE(!memcmp(bf, "qwe\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcpy(bf, "qwerty", 4));
	REQUIRE(!memcmp(bf, "qwe\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, "qwerty", 3));
	REQUIRE(!memcmp(bf, "qwe\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, "qwerty", 2));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, "qwe"));
	REQUIRE(!memcmp(bf, "qwe\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, "qwe", 4));
	REQUIRE(!memcmp(bf, "qwe\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, "qw", 3));
	REQUIRE(!memcmp(bf, "qw\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, sizeof(bf), "q"));
	REQUIRE(!memcmp(bf, "q\000cd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcpy(bf, sizeof(bf), "q", 2));
	REQUIRE(!memcmp(bf, "q\000cd", 4));
}

TEST(CryStringUtilsTest, Strcat)
{
	REQUIRE(!cry_strcat(0, 0, 0));
	REQUIRE(!cry_strcat(0, 0, 0, 0));
	REQUIRE(!cry_strcat(0, 1, 0));
	REQUIRE(!cry_strcat(0, 1, 0, 0));
	REQUIRE(!cry_strcat(0, 1, ""));
	REQUIRE(!cry_strcat(0, 1, "", 1));

	char bf[4];

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 0, "xy"));
	REQUIRE(!memcmp(bf, "abcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 0, "xy", 3));
	REQUIRE(!memcmp(bf, "abcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 0, "xy", 0));
	REQUIRE(!memcmp(bf, "abcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 1, "xyz"));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 1, "xyz", 4));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 1, "xyz", 1));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcat(bf, 1, "xyz", 0));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, 1, 0, 0));
	REQUIRE(!memcmp(bf, "\000bcd", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(!cry_strcat(bf, 3, "xyz"));
	REQUIRE(!memcmp(bf, "ax\000d", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(!cry_strcat(bf, 3, "xyz", 4));
	REQUIRE(!memcmp(bf, "ax\000d", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(!cry_strcat(bf, 3, "xyz", 2));
	REQUIRE(!memcmp(bf, "ax\000d", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(cry_strcat(bf, 3, "xyz", 1));
	REQUIRE(!memcmp(bf, "ax\000d", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, "xyz"));
	REQUIRE(!memcmp(bf, "abc\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, "xyz", 4));
	REQUIRE(!memcmp(bf, "abc\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(!cry_strcat(bf, "xyz", 1));
	REQUIRE(!memcmp(bf, "abc\000", 4));

	memcpy(bf, "abcd", 4);
	REQUIRE(cry_strcat(bf, "xyz", 0));
	REQUIRE(!memcmp(bf, "abc\000", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(!cry_strcat(bf, "xyz"));
	REQUIRE(!memcmp(bf, "abx\000", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(!cry_strcat(bf, "xyz", 4));
	REQUIRE(!memcmp(bf, "abx\000", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(cry_strcat(bf, "xyz", 1));
	REQUIRE(!memcmp(bf, "abx\000", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(cry_strcat(bf, "xyz", 0));
	REQUIRE(!memcmp(bf, "ab\000d", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(!cry_strcat(bf, 0, 0));
	REQUIRE(!memcmp(bf, "ab\000d", 4));

	memcpy(bf, "ab\000d", 4);
	REQUIRE(!cry_strcat(bf, 0, 1));
	REQUIRE(!memcmp(bf, "ab\000d", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(cry_strcat(bf, sizeof(bf), "xy"));
	REQUIRE(!memcmp(bf, "axy\000", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(cry_strcat(bf, sizeof(bf), "xy", 3));
	REQUIRE(!memcmp(bf, "axy\000", 4));

	memcpy(bf, "a\000cd", 4);
	REQUIRE(cry_strcat(bf, sizeof(bf), "xy", 1));
	REQUIRE(!memcmp(bf, "ax\000d", 4));
}

TEST(CryStringUtilsTest, Sprintf)
{
	const auto test = [](const bool bNull, int charCount, const char* src, const char* initBuffer, const char* resultBuffer, bool resultBool)
	{
		char buffer[3] = { initBuffer[0], initBuffer[1], initBuffer[2] };
		REQUIRE(charCount >= 0);
		REQUIRE(charCount <= CRY_ARRAY_COUNT(buffer));
		const bool b = cry_sprintf(bNull ? nullptr : buffer, charCount * sizeof(buffer[0]), src);
		REQUIRE(b == resultBool);
		REQUIRE(!memcmp(buffer, resultBuffer, sizeof(buffer)));
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

TEST(CryStringUtilsTest, ComputeLengthFormatted)
{
	const auto test = [](int length, std::vector<char>& c)
	{
		c.resize(length + 1);
		const int count = CryStringUtils_Internal::compute_length_formatted("%*s", length, "");
		REQUIRE(count == length);
		REQUIRE(cry_sprintf(c.data(), (length + 1) * sizeof(c[0]), "%*s", length, ""));
		REQUIRE(strlen(c.data()) == length);
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
