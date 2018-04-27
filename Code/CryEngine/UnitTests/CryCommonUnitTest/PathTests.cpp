// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryString/CryPath.h>

template<typename TString>
void TestUnixPathA()
{
	REQUIRE(PathUtil::ToUnixPath(TString("some\\path\\etc")) == "some/path/etc");
	REQUIRE(PathUtil::ToUnixPath(TString("some/path\\etc")) == "some/path/etc");
	REQUIRE(PathUtil::ToUnixPath(TString("some\\path/etc")) == "some/path/etc");
	REQUIRE(PathUtil::ToUnixPath(TString("some\\path\\")) == "some/path/");
	REQUIRE(PathUtil::ToUnixPath(TString("somepath")) == "somepath");
	REQUIRE(PathUtil::ToUnixPath(TString("\\")) == "/");
	REQUIRE(PathUtil::ToUnixPath(TString("C:\\a.b\\")) == "C:/a.b/");
	REQUIRE(PathUtil::ToUnixPath(TString("")) == TString(""));
}

template<typename TString>
void TestUnixPathW()
{
	REQUIRE(PathUtil::ToUnixPath(TString(L"C:\\中文路径\\")) == TString(L"C:/中文路径/"));
}

TEST(CryPathTest, ToUnixPath)
{
	TestUnixPathA<string>();
	TestUnixPathA<CryStackStringT<char, 512>>();
	TestUnixPathA<CryFixedStringT<512>>();
	TestUnixPathW<wstring>();
	TestUnixPathW<CryStackStringT<wchar_t, 512>>();
	TestUnixPathW<CryFixedWStringT<512>>();
}

TEST(CryPathTest, SimplifyFilePath)
{
	// Check a number of invalid outputs and too-short-buffer scenarios.
	// They should do the same for Windows and Posix.
	{
		for (int i = 0; i < 2; ++i)
		{
			PathUtil::EPathStyle style = (i == 0 ? PathUtil::ePathStyle_Windows : PathUtil::ePathStyle_Posix);
			char buf[1];
			bool result = PathUtil::SimplifyFilePath("foo", buf, 1, style);
			REQUIRE(!result && !*buf);

			result = PathUtil::SimplifyFilePath(nullptr, buf, 1, style);
			REQUIRE(!result && !*buf);

			result = PathUtil::SimplifyFilePath("foo", nullptr, 9001, style);
			REQUIRE(!result);

			result = PathUtil::SimplifyFilePath("foo", buf, 0, style);
			REQUIRE(!result);

			result = PathUtil::SimplifyFilePath("", buf, 1, style);
			REQUIRE(!result && !*buf);
		}
	}

	// tests with sufficient buffer
	{
		struct
		{
			const char* szInput;
			bool        isWinExpected;
			const char* szWinExpected;
			bool        isUnixExpected;
			const char* szUnixExpected;
		}
		items[] =
		{
			//                               WINAPI                                 POSIX
			// input                         success   output                       success   output
			{ "",                            false,    "",                          false,    "" },
			{ "c:",                          true,     "c:",                        true,     "c:" },// c: and c:. counts as a regular folder name in posix
			{ "c:.",                         true,     "c:",                        true,     "c:." },
			{ "c:..",                        true,     "c:..",                      true,     "c:.." },
			{ "c:abc/.",                     true,     "c:abc",                     true,     "c:abc" },
			{ "c:abc/..",                    true,     "c:",                        true,     "" },
			{ "c:abc/../..",                 true,     "c:..",                      true,     ".." },
			{ "c:\\",                        true,     "c:\\",                      true,     "c:" },
			{ ":\\.",                        false,    "",                          true,     ":" },
			{ "abc:\\.",                     false,    "",                          true,     "abc:" },
			{ "c:\\.",                       true,     "c:\\",                      true,     "c:" },
			{ "c:\\..",                      false,    "",                          true,     "" },
			{ "c:/abc/.",                    true,     "c:\\abc",                   true,     "c:/abc" },
			{ "c:/abc/..",                   true,     "c:\\",                      true,     "c:" },
			{ "c:/abc\\..\\..",              false,    "",                          true,     "" },
			{ "\\\\storage\\.",              true,     "\\\\storage",               false,    "" },
			{ "\\\\storage\\..",             false,    "",                          false,    "" },
			{ "\\\\storage\\abc\\..\\",      true,     "\\\\storage",               false,    "" },
			{ "c:/should/be/unchanged",      true,     "c:\\should\\be\\unchanged", true,     "c:/should/be/unchanged" },
			{ "c:\\disappear/..\\remain",    true,     "c:\\remain",                true,     "c:/remain" },
			{ "c:\\disappear\\..\\remain",   true,     "c:\\remain",                true,     "c:/remain" },
			{ "/disappear/../remain",        true,     "\\remain",                  true,     "/remain" },
			{ "c:/trailing/slash/gone/",     true,     "c:\\trailing\\slash\\gone", true,     "c:/trailing/slash/gone" },
			{ "c:\\trailing\\slash\\gone\\", true,     "c:\\trailing\\slash\\gone", true,     "c:/trailing/slash/gone" },
			{ "/trailing/slash/gone/",       true,     "\\trailing\\slash\\gone",   true,     "/trailing/slash/gone" },
			{ ".",                           true,     ".",                         true,     "." },
			{ "..",                          true,     "..",                        true,     ".." },
			{ "..\\..",                      true,     "..\\..",                    true,     "../.." },
			{ "\\..\\..\\",                  false,    "",                          false,    "" },
			{ "../.\\..",                    true,     "..\\..",                    true,     "../.." },
			{ "\\\\.\\myfile",               false,    "",                          false,    "" },
			{ "/some/rooted/path",           true,     "\\some\\rooted\\path",      true,     "/some/rooted/path" },
			{ "\\\\s2\\..\\s1",              false,    "",                          false,    "" },
			{ "/",                           true,     "\\",                        true,     "/" },
			{ "/.",                          true,     "\\",                        true,     "/" },
			{ "/..",                         false,    "",                          false,    "" },
			{ "/abc/.",                      true,     "\\abc",                     true,     "/abc" },
			{ "/abc/..",                     true,     "\\",                        true,     "/" },
			{ "/abc/../..",                  false,    "",                          false,    "" },
			{ "../abc",                      true,     "..\\abc",                   true,     "../abc" },
			{ "../../abc",                   true,     "..\\..\\abc",               true,     "../../abc" },
			{ "bar/../../../abc",            true,     "..\\..\\abc",               true,     "../../abc" },
			{ "/abc/foo/bar",                true,     "\\abc\\foo\\bar",           true,     "/abc/foo/bar" },
			{ "abc",                         true,     "abc",                       true,     "abc" },
			{ "abc/.",                       true,     "abc",                       true,     "abc" },
			{ "abc/..",                      true,     "",                          true,     "" },
			{ "abc/../..",                   true,     "..",                        true,     ".." },
			{ "abc/foo/bar",                 true,     "abc\\foo\\bar",             true,     "abc/foo/bar" },
			{ "/foo//bar",                   true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ "/foo/bar//",                  true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ "/foo/////bar////",            true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ "\\foo\\\\bar",                true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ "\\foo\\bar\\\\",              true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ "\\foo\\\\\\\\\\bar\\\\\\",    true,     "\\foo\\bar",                true,     "/foo/bar" },
			{ ".\\/system.cfg",              true,     "system.cfg",                true,     "system.cfg" },
			{ ".\\/./system.cfg",            true,     "system.cfg",                true,     "system.cfg" },
			{ ".\\/./%USER%/game.cfg",       true,     "%USER%\\game.cfg",          true,     "%USER%/game.cfg" },
		};
		const size_t numItems = CRY_ARRAY_COUNT(items);

		const size_t bufLength = 100;
		char buf[bufLength];

		for (size_t i = 0; i < numItems; ++i)
		{
			bool isWinResultSuccess = PathUtil::SimplifyFilePath(items[i].szInput, buf, bufLength, PathUtil::ePathStyle_Windows);
			bool matchesWin = strcmp(buf, items[i].szWinExpected) == 0;
			bool isUnixResultSuccess = PathUtil::SimplifyFilePath(items[i].szInput, buf, bufLength, PathUtil::ePathStyle_Posix);
			bool matchesUnix = strcmp(buf, items[i].szUnixExpected) == 0;
			REQUIRE(isWinResultSuccess == items[i].isWinExpected);
			REQUIRE(isUnixResultSuccess == items[i].isUnixExpected);
			REQUIRE(matchesWin);
			REQUIRE(matchesUnix);
		}
	}

	// test complex case
	{
		const size_t bufLength = 100;
		char buf[bufLength];

		const char* const szComplex1 = "foo/bar/../baz/./../../../../././hi/.";
		const char* const szResult1 = "../../hi";
		bool result = PathUtil::SimplifyFilePath(szComplex1, buf, bufLength, PathUtil::ePathStyle_Posix);
		REQUIRE(result && strcmp(buf, szResult1) == 0);

		const char* const szComplex2 = "c:/foo/bar/./disappear/disappear/disappear/../.\\../../../baz\\";
		const char* const szResult2 = "c:\\foo\\baz";
		result = PathUtil::SimplifyFilePath(szComplex2, buf, bufLength, PathUtil::ePathStyle_Windows);
		REQUIRE(result && strcmp(buf, szResult2) == 0);
	}
}
