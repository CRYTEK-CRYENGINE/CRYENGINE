// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __STRINGHELPERS_H__
#define __STRINGHELPERS_H__

namespace StringHelpers
{
	int Compare(const string& str0, const string& str1);
	int Compare(const wstring& str0, const wstring& str1);

	int CompareIgnoreCase(const string& str0, const string& str1);
	int CompareIgnoreCase(const wstring& str0, const wstring& str1);

	bool Equals(const string& str0, const string& str1);
	bool Equals(const wstring& str0, const wstring& str1);

	bool EqualsIgnoreCase(const string& str0, const string& str1);
	bool EqualsIgnoreCase(const wstring& str0, const wstring& str1);

	bool StartsWith(const string& str, const string& pattern);
	bool StartsWith(const wstring& str, const wstring& pattern);

	bool StartsWithIgnoreCase(const string& str, const string& pattern);
	bool StartsWithIgnoreCase(const wstring& str, const wstring& pattern);

	bool EndsWith(const string& str, const string& pattern);
	bool EndsWith(const wstring& str, const wstring& pattern);

	bool EndsWithIgnoreCase(const string& str, const string& pattern);
	bool EndsWithIgnoreCase(const wstring& str, const wstring& pattern);

	bool Contains(const string& str, const string& pattern);
	bool Contains(const wstring& str, const wstring& pattern);

	bool ContainsIgnoreCase(const string& str, const string& pattern);
	bool ContainsIgnoreCase(const wstring& str, const wstring& pattern);

	bool MatchesWildcards(const string& str, const string& wildcards);
	bool MatchesWildcards(const wstring& str, const wstring& wildcards);

	bool MatchesWildcardsIgnoreCase(const string& str, const string& wildcards);
	bool MatchesWildcardsIgnoreCase(const wstring& str, const wstring& wildcards);

	bool MatchesWildcardsIgnoreCaseExt(const string& str, const string& wildcards, std::vector<string>& wildcardMatches);
	bool MatchesWildcardsIgnoreCaseExt(const wstring& str, const wstring& wildcards, std::vector<wstring>& wildcardMatches);

	string TrimLeft(const string& s);
	wstring TrimLeft(const wstring& s);

	string TrimRight(const string& s);
	wstring TrimRight(const wstring& s);

	string Trim(const string& s);
	wstring Trim(const wstring& s);

	string RemoveDuplicateSpaces(const string& s);
	wstring RemoveDuplicateSpaces(const wstring& s);

	string MakeLowerCase(const string& s);
	wstring MakeLowerCase(const wstring& s);

	string MakeUpperCase(const string& s);
	wstring MakeUpperCase(const wstring& s);

	string Replace(const string& s, char oldChar, char newChar);
	wstring Replace(const wstring& s, wchar_t oldChar, wchar_t newChar);

	template <typename O, typename I> O ConvertString(const I& in)
	{
		O out;
		ConvertStringByRef(out, in);
		return out;
	}

	void ConvertStringByRef(string& out, const string& in);
	void ConvertStringByRef(wstring& out, const string& in);
	void ConvertStringByRef(string& out, const wstring& in);
	void ConvertStringByRef(wstring& out, const wstring& in);

	void Split(const string& str, const string& separator, bool bReturnEmptyPartsToo, std::vector<string>& outParts);
	void Split(const wstring& str, const wstring& separator, bool bReturnEmptyPartsToo, std::vector<wstring>& outParts);

	void SplitByAnyOf(const string& str, const string& separators, bool bReturnEmptyPartsToo, std::vector<string>& outParts);
	void SplitByAnyOf(const wstring& str, const wstring& separators, bool bReturnEmptyPartsToo, std::vector<wstring>& outParts);

	//////////////////////////////////////////////////////////////////////////

	string FormatVA(const char* const format, va_list parg);
	string Format(const char* const format, ...);

	//////////////////////////////////////////////////////////////////////////

	void SafeCopy(char* const pDstBuffer, const size_t dstBufferSizeInBytes, const char* const pSrc);
	void SafeCopy(wchar_t* const pDstBuffer, const size_t dstBufferSizeInBytes, const wchar_t* const pSrc);

	void SafeCopyPadZeros(char* const pDstBuffer, const size_t dstBufferSizeInBytes, const char* const pSrc);
	void SafeCopyPadZeros(wchar_t* const pDstBuffer, const size_t dstBufferSizeInBytes, const wchar_t* const pSrc);

	//////////////////////////////////////////////////////////////////////////

	// ASCII
	// ANSI (system default Windows ANSI code page)
	// UTF-8
	// UTF-16

	// ASCII -> UTF-16

	bool Utf16ContainsAsciiOnly(const wchar_t* wstr);
	string ConvertAsciiUtf16ToAscii(const wchar_t* wstr);
	wstring ConvertAsciiToUtf16(const char* str);

	// UTF-8 <-> UTF-16

	wstring ConvertUtf8ToUtf16(const char* str);
	string ConvertUtf16ToUtf8(const wchar_t* wstr);

	inline string ConvertUtfToUtf8(const char* str)
	{
		return string(str);
	}

	inline string ConvertUtfToUtf8(const wchar_t* wstr)
	{
		return ConvertUtf16ToUtf8(wstr);
	}

	inline wstring ConvertUtfToUtf16(const char* str)
	{
		return ConvertUtf8ToUtf16(str);
	}

	inline wstring ConvertUtfToUtf16(const wchar_t* wstr)
	{
		return wstring(wstr);
	}

	// ANSI <-> UTF-8, UTF-16

	wstring ConvertAnsiToUtf16(const char* str);
	string ConvertUtf16ToAnsi(const wchar_t* wstr, char badChar);

	inline string ConvertAnsiToUtf8(const char* str)
	{
		return ConvertUtf16ToUtf8(ConvertAnsiToUtf16(str).c_str());
	}
	inline string ConvertUtf8ToAnsi(const char* str, char badChar)
	{
		return ConvertUtf16ToAnsi(ConvertUtf8ToUtf16(str).c_str(), badChar);
	}

	// ANSI -> ASCII
	string ConvertAnsiToAscii(const char* str, char badChar);
}
#endif //__STRINGHELPERS_H__
