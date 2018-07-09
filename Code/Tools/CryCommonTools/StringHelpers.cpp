// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StringHelpers.h"
#include "Util.h"
#include <CryString/StringUtils.h> // From CryCommon
#include <CryString/UnicodeFunctions.h>
#include <cctype>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>   // WideCharToMultibyte(), CP_UTF8, etc.


static inline char ToLower(const char c)
{
	return tolower(c);
}

static inline wchar_t ToLower(const wchar_t c)
{
	return towlower(c);
}


static inline char ToUpper(const char c)
{
	return toupper(c);
}

static inline wchar_t ToUpper(const wchar_t c)
{
	return towupper(c);
}


int StringHelpers::Compare(const string& str0, const string& str1)
{
	const size_t minLength = Util::getMin(str0.length(), str1.length());
	const int result = std::memcmp(str0.c_str(), str1.c_str(), minLength);
	if (result)
	{
		return result;
	}
	else
	{
		return (str0.length() == str1.length())
			? 0
			: ((str0.length() < str1.length()) ? -1 : +1);
	}
}

int StringHelpers::Compare(const wstring& str0, const wstring& str1)
{
	const size_t minLength = Util::getMin(str0.length(), str1.length());
	for (size_t i = 0; i < minLength; ++i)
	{
		const wchar_t c0 = str0[i];
		const wchar_t c1 = str1[i];
		if (c0 != c1)
		{
			return (c0 < c1) ? -1 : 1;
		}
	}

	return (str0.length() == str1.length())
		? 0
		: ((str0.length() < str1.length()) ? -1 : +1);
}


int StringHelpers::CompareIgnoreCase(const string& str0, const string& str1)
{
	const size_t minLength = Util::getMin(str0.length(), str1.length());
	const int result = memicmp(str0.c_str(), str1.c_str(), minLength);
	if (result)
	{
		return result;
	}
	else
	{
		return (str0.length() == str1.length())
			? 0
			: ((str0.length() < str1.length()) ? -1 : +1);
	}
}

int StringHelpers::CompareIgnoreCase(const wstring& str0, const wstring& str1)
{
	const size_t minLength = Util::getMin(str0.length(), str1.length());
	for (size_t i = 0; i < minLength; ++i)
	{
		const wchar_t c0 = towlower(str0[i]);
		const wchar_t c1 = towlower(str1[i]);
		if (c0 != c1)
		{
			return (c0 < c1) ? -1 : 1;
		}
	}

	return (str0.length() == str1.length())
		? 0
		: ((str0.length() < str1.length()) ? -1 : +1);
}


bool StringHelpers::Equals(const string& str0, const string& str1)
{
	if (str0.length() != str1.length())
	{
		return false;
	}
	return std::memcmp(str0.c_str(), str1.c_str(), str1.length()) == 0;
}

bool StringHelpers::Equals(const wstring& str0, const wstring& str1)
{
	if (str0.length() != str1.length())
	{
		return false;
	}
	return std::memcmp(str0.c_str(), str1.c_str(), str1.length() * sizeof(wstring::value_type)) == 0;
}


bool StringHelpers::EqualsIgnoreCase(const string& str0, const string& str1)
{
	if (str0.length() != str1.length())
	{
		return false;
	}
	return memicmp(str0.c_str(), str1.c_str(), str1.length()) == 0;
}

bool StringHelpers::EqualsIgnoreCase(const wstring& str0, const wstring& str1)
{
	const size_t str1Length = str1.length();
	if (str0.length() != str1Length)
	{
		return false;
	}
	for (size_t i = 0; i < str1Length; ++i)
	{
		if (towlower(str0[i]) != towlower(str1[i]))
		{
			return false;
		}
	}
	return true;
}


bool StringHelpers::StartsWith(const string& str, const string& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return std::memcmp(str.c_str(), pattern.c_str(), pattern.length()) == 0;
}

bool StringHelpers::StartsWith(const wstring& str, const wstring& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return std::memcmp(str.c_str(), pattern.c_str(), pattern.length() * sizeof(wstring::value_type)) == 0;
}


bool StringHelpers::StartsWithIgnoreCase(const string& str, const string& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return memicmp(str.c_str(), pattern.c_str(), pattern.length()) == 0;
}

bool StringHelpers::StartsWithIgnoreCase(const wstring& str, const wstring& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	for (size_t i = 0; i < patternLength; ++i)
	{
		if (towlower(str[i]) != towlower(pattern[i]))
		{
			return false;
		}
	}
	return true;
}


bool StringHelpers::EndsWith(const string& str, const string& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return std::memcmp(str.c_str() + str.length() - pattern.length(), pattern.c_str(), pattern.length()) == 0;
}

bool StringHelpers::EndsWith(const wstring& str, const wstring& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return std::memcmp(str.c_str() + str.length() - pattern.length(), pattern.c_str(), pattern.length() * sizeof(wstring::value_type)) == 0;
}


bool StringHelpers::EndsWithIgnoreCase(const string& str, const string& pattern)
{
	if (str.length() < pattern.length())
	{
		return false;
	}
	return memicmp(str.c_str() + str.length() - pattern.length(), pattern.c_str(), pattern.length()) == 0;
}

bool StringHelpers::EndsWithIgnoreCase(const wstring& str, const wstring& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	for (size_t i = str.length() - patternLength, j = 0; i < patternLength; ++i, ++j)
	{
		if (towlower(str[i]) != towlower(pattern[j]))
		{
			return false;
		}
	}
	return true;
}


bool StringHelpers::Contains(const string& str, const string& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	const size_t n = str.length() - patternLength + 1;
	for (size_t i = 0; i < n; ++i)
	{
		if (std::memcmp(str.c_str() + i, pattern.c_str(), patternLength) == 0)
		{
			return true;
		}
	}
	return false;
}

bool StringHelpers::Contains(const wstring& str, const wstring& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	const size_t n = str.length() - patternLength + 1;
	for (size_t i = 0; i < n; ++i)
	{
		if (std::memcmp(str.c_str() + i, pattern.c_str(), patternLength * sizeof(wstring::value_type)) == 0)
		{
			return true;
		}
	}
	return false;
}


bool StringHelpers::ContainsIgnoreCase(const string& str, const string& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	const size_t n = str.length() - patternLength + 1;
	for (size_t i = 0; i < n; ++i)
	{
		if (memicmp(str.c_str() + i, pattern.c_str(), patternLength) == 0)
		{
			return true;
		}
	}
	return false;
}

bool StringHelpers::ContainsIgnoreCase(const wstring& str, const wstring& pattern)
{
	const size_t patternLength = pattern.length();
	if (str.length() < patternLength)
	{
		return false;
	}
	const size_t n = str.length() - patternLength + 1;
	for (size_t i = 0; i < n; ++i)
	{
		for (size_t j = 0; j < patternLength; ++j)
		{
			if (towlower(str[i + j]) != towlower(pattern[j]))
			{
				return false;
			}
		}
	}
	return false;
}

bool StringHelpers::MatchesWildcards(const string& str, const string& wildcards)
{
	using namespace CryStringUtils_Internal;
	return MatchesWildcards_Tpl<SCharComparatorCaseSensitive>(str.c_str(), wildcards.c_str());
}

bool StringHelpers::MatchesWildcards(const wstring& str, const wstring& wildcards)
{
	using namespace CryStringUtils_Internal;
	return MatchesWildcards_Tpl<SCharComparatorCaseSensitive>(str.c_str(), wildcards.c_str());
}

bool StringHelpers::MatchesWildcardsIgnoreCase(const string& str, const string& wildcards)
{
	using namespace CryStringUtils_Internal;
	return MatchesWildcards_Tpl<SCharComparatorCaseInsensitive>(str.c_str(), wildcards.c_str());
}

bool StringHelpers::MatchesWildcardsIgnoreCase(const wstring& str, const wstring& wildcards)
{
	using namespace CryStringUtils_Internal;
	return MatchesWildcards_Tpl<SCharComparatorCaseInsensitive>(str.c_str(), wildcards.c_str());
}


template <class TS>
static inline bool MatchesWildcardsIgnoreCaseExt_Tpl(const TS& str, const TS& wildcards, std::vector<TS>& wildcardMatches)
{
	const TS::value_type* savedStrBegin = 0;
	const TS::value_type* savedStrEnd = 0;
	const TS::value_type* savedWild = 0;
	size_t savedWildCount;

	const TS::value_type* pStr = str.c_str();
	const TS::value_type* pWild = wildcards.c_str();
	size_t wildCount = 0;

	while ((*pStr) && (*pWild != '*')) 
	{
		if (*pWild == '?')
		{
			wildcardMatches.push_back(TS(pStr, pStr + 1));
			++wildCount;
		}
		else if (ToLower(*pWild) != ToLower(*pStr))
		{
			wildcardMatches.resize(wildcardMatches.size() - wildCount);
			return false;
		}
		++pWild;
		++pStr;
	}

	while (*pStr) 
	{
		if (*pWild == '*') 
		{
			if (!pWild[1]) 
			{
				wildcardMatches.push_back(TS(pStr));
				return true;
			}
			savedWild = pWild++;
			savedStrBegin = pStr;
			savedStrEnd = pStr;
			savedWildCount = wildCount;
			wildcardMatches.push_back(TS(savedStrBegin, savedStrEnd));
			++wildCount;
		} 
		else if (*pWild == '?')
		{
			wildcardMatches.push_back(TS(pStr, pStr + 1));
			++wildCount;
			++pWild;
			++pStr;
		} 
		else if (ToLower(*pWild) == ToLower(*pStr)) 
		{
			++pWild;
			++pStr;
		} 
		else 
		{
			wildcardMatches.resize(wildcardMatches.size() - (wildCount - savedWildCount));
			wildCount = savedWildCount;
			++savedStrEnd;
			pWild = savedWild + 1;
			pStr = savedStrEnd;
			wildcardMatches.push_back(TS(savedStrBegin, savedStrEnd));
			++wildCount;
		}
	}

	while (*pWild == '*') 
	{
		++pWild;
		wildcardMatches.push_back(TS());
		++wildCount;
	}

	if (*pWild)
	{
		wildcardMatches.resize(wildcardMatches.size() - wildCount);
		return false;
	}

	return true;
}

bool StringHelpers::MatchesWildcardsIgnoreCaseExt(const string& str, const string& wildcards, std::vector<string>& wildcardMatches)
{
	return MatchesWildcardsIgnoreCaseExt_Tpl(str, wildcards, wildcardMatches);
}

bool StringHelpers::MatchesWildcardsIgnoreCaseExt(const wstring& str, const wstring& wildcards, std::vector<wstring>& wildcardMatches)
{
	return MatchesWildcardsIgnoreCaseExt_Tpl(str, wildcards, wildcardMatches);
}


string StringHelpers::TrimLeft(const string& s)
{
	const size_t first = s.find_first_not_of(" \r\t");
	return (first == s.npos) ? string() : s.substr(first);
}

wstring StringHelpers::TrimLeft(const wstring& s)
{
	const size_t first = s.find_first_not_of(L" \r\t");
	return (first == s.npos) ? wstring() : s.substr(first);
}


string StringHelpers::TrimRight(const string& s)
{
	const size_t last = s.find_last_not_of(" \r\t");
	return (last == s.npos) ? s : s.substr(0,last+1);
}

wstring StringHelpers::TrimRight(const wstring& s)
{
	const size_t last = s.find_last_not_of(L" \r\t");
	return (last == s.npos) ? s : s.substr(0,last+1);
}


string StringHelpers::Trim(const string& s)
{
	return TrimLeft(TrimRight(s));
}

wstring StringHelpers::Trim(const wstring& s)
{
	return TrimLeft(TrimRight(s));
}


template <class TS>
static inline TS RemoveDuplicateSpaces_Tpl(const TS& s)
{
	TS res;
	bool spaceFound = false;

	for (size_t i = 0, n = s.length(); i < n; ++i)
	{
		if ((s[i]==' ') || (s[i]=='\r') || (s[i]=='\t'))
		{
			spaceFound = true;
		}
		else
		{
			if (spaceFound)
			{
				res += ' ';
				spaceFound = false;
			}
			res += s[i];
		}
	}

	if (spaceFound)
	{
		res += ' ';
	}
	return res;
}

string StringHelpers::RemoveDuplicateSpaces(const string& s)
{
	return RemoveDuplicateSpaces_Tpl(s);
}

wstring StringHelpers::RemoveDuplicateSpaces(const wstring& s)
{
	return RemoveDuplicateSpaces_Tpl(s);
}


template <class TS>
static inline TS MakeLowerCase_Tpl(const TS& s)
{
	TS copy;
	copy.reserve(s.length());
	for (TS::const_iterator it = s.begin(), end = s.end(); it != end; ++it)
	{
		copy.append(1, ToLower(*it));
	}
	return copy;
}

string StringHelpers::MakeLowerCase(const string& s)
{
	return MakeLowerCase_Tpl(s);
}

wstring StringHelpers::MakeLowerCase(const wstring& s)
{
	return MakeLowerCase_Tpl(s);
}


template <class TS>
static inline TS MakeUpperCase_Tpl(const TS& s)
{
	TS copy;
	copy.reserve(s.length());
	for (TS::const_iterator it = s.begin(), end = s.end(); it != end; ++it)
	{
		copy.append(1, ToUpper(*it));
	}
	return copy;
}

string StringHelpers::MakeUpperCase(const string& s)
{
	return MakeUpperCase_Tpl(s);
}

wstring StringHelpers::MakeUpperCase(const wstring& s)
{
	return MakeUpperCase_Tpl(s);
}


template <class TS>
static inline TS Replace_Tpl(const TS& s, const typename TS::value_type oldChar, const typename TS::value_type newChar)
{
	TS copy;
	copy.reserve(s.length());
	for (TS::const_iterator it = s.begin(), end = s.end(); it != end; ++it)
	{
		const TS::value_type c = (*it);
		copy.append(1, ((c == oldChar) ? newChar : c));
	}
	return copy;
}

string StringHelpers::Replace(const string& s, char oldChar, char newChar)
{
	return Replace_Tpl(s, oldChar, newChar);
}

wstring StringHelpers::Replace(const wstring& s, wchar_t oldChar, wchar_t newChar)
{
	return Replace_Tpl(s, oldChar, newChar);
}


void StringHelpers::ConvertStringByRef(string& out, const string& in)
{
	out = in;
}

void StringHelpers::ConvertStringByRef(wstring& out, const string& in)
{
	Unicode::Convert(out, in);
}

void StringHelpers::ConvertStringByRef(string& out, const wstring& in)
{
	Unicode::Convert(out, in);
}

void StringHelpers::ConvertStringByRef(wstring& out, const wstring& in)
{
	out = in;
}


template <class TS>
static inline void Split_Tpl(const TS& str, const TS& separator, bool bReturnEmptyPartsToo, std::vector<TS>& outParts)
{
	if (str.empty())
	{
		return;
	}

	if (separator.empty())
	{
		for (size_t i = 0; i < str.length(); ++i)
		{
			outParts.push_back(str.substr(i, 1));
		}
		return;		
	}

	size_t partStart = 0;

	for (;;)
	{
		const size_t pos = str.find(separator, partStart);
		if (pos == TS::npos)
		{
			break;
		}
		if (bReturnEmptyPartsToo || (pos > partStart))
		{
			outParts.push_back(str.substr(partStart, pos - partStart));
		}
		partStart = pos + separator.length();
	}

	if (bReturnEmptyPartsToo || (partStart < str.length()))
	{
		outParts.push_back(str.substr(partStart, str.length() - partStart));
	}
}


template <class TS>
static inline void SplitByAnyOf_Tpl(const TS& str, const TS& separators, bool bReturnEmptyPartsToo, std::vector<TS>& outParts)
{
	if (str.empty())
	{
		return;
	}

	if (separators.empty())
	{
		for (size_t i = 0; i < str.length(); ++i)
		{
			outParts.push_back(str.substr(i, 1));
		}
		return;		
	}

	size_t partStart = 0;

	for (;;)
	{
		const size_t pos = str.find_first_of(separators, partStart);
		if (pos == TS::npos)
		{
			break;
		}
		if (bReturnEmptyPartsToo || (pos > partStart))
		{
			outParts.push_back(str.substr(partStart, pos - partStart));
		}
		partStart = pos + 1;
	}

	if (bReturnEmptyPartsToo || (partStart < str.length()))
	{
		outParts.push_back(str.substr(partStart, str.length() - partStart));
	}
}



void StringHelpers::Split(const string& str, const string& separator, bool bReturnEmptyPartsToo, std::vector<string>& outParts)
{
	Split_Tpl(str, separator, bReturnEmptyPartsToo, outParts);
}

void StringHelpers::Split(const wstring& str, const wstring& separator, bool bReturnEmptyPartsToo, std::vector<wstring>& outParts)
{
	Split_Tpl(str, separator, bReturnEmptyPartsToo, outParts);
}


void StringHelpers::SplitByAnyOf(const string& str, const string& separators, bool bReturnEmptyPartsToo, std::vector<string>& outParts)
{
	SplitByAnyOf_Tpl(str, separators, bReturnEmptyPartsToo, outParts);
}

void StringHelpers::SplitByAnyOf(const wstring& str, const wstring& separators, bool bReturnEmptyPartsToo, std::vector<wstring>& outParts)
{
	SplitByAnyOf_Tpl(str, separators, bReturnEmptyPartsToo, outParts);
}

//////////////////////////////////////////////////////////////////////////

string StringHelpers::FormatVA(const char* const format, va_list args)
{
#if CRY_COMPILER_MSVC && CRY_COMPILER_VERSION <= 1700
	// Visual Studio 2012 and older don't have va_copy()
	const int n = CryStringUtils_Internal::compute_length_formatted_va(format, args);
#else
	va_list argsCopy;
	va_copy(argsCopy, args);
	const int n = CryStringUtils_Internal::compute_length_formatted_va(format, argsCopy);
	va_end(argsCopy);
#endif
	if (n < 0)
	{
		return string();
	}
	std::vector<char> bf;
	bf.resize(n + 1);
	cry_vsprintf(&bf[0], n + 1, format, args);
	return string(&bf[0]);
}

string StringHelpers::Format(const char* const format, ...)
{
	va_list args;
	va_start(args, format);
	const string result = FormatVA(format, args);
	va_end(args);
	return result;
}

//////////////////////////////////////////////////////////////////////////

template <class TC>
static inline void SafeCopy_Tpl(TC* const pDstBuffer, const size_t dstBufferSizeInBytes, const TC* const pSrc)
{
	if (dstBufferSizeInBytes >= sizeof(TC))
	{
		const size_t n = dstBufferSizeInBytes / sizeof(TC) - 1;
		size_t i;
		for (i = 0; i < n && pSrc[i]; ++i)
		{
			pDstBuffer[i] = pSrc[i];
		}
		pDstBuffer[i] = 0;
	}
}

template <class TC>
static inline void SafeCopyPadZeros_Tpl(TC* const pDstBuffer, const size_t dstBufferSizeInBytes, const TC* const pSrc)
{
	if (dstBufferSizeInBytes > 0)
	{
		const size_t n = (dstBufferSizeInBytes < sizeof(TC)) ? 0 : dstBufferSizeInBytes / sizeof(TC) - 1;
		size_t i;
		for (i = 0; i < n && pSrc[i]; ++i)
		{
			pDstBuffer[i] = pSrc[i];
		}
		memset(&pDstBuffer[i], 0, dstBufferSizeInBytes - i * sizeof(TC));
	}
}


void StringHelpers::SafeCopy(char* const pDstBuffer, const size_t dstBufferSizeInBytes, const char* const pSrc)
{
	SafeCopy_Tpl<char>(pDstBuffer, dstBufferSizeInBytes, pSrc);
}
	
void StringHelpers::SafeCopy(wchar_t* const pDstBuffer, const size_t dstBufferSizeInBytes, const wchar_t* const pSrc)
{
	SafeCopy_Tpl<wchar_t>(pDstBuffer, dstBufferSizeInBytes, pSrc);
}


void StringHelpers::SafeCopyPadZeros(char* const pDstBuffer, const size_t dstBufferSizeInBytes, const char* const pSrc)
{
	SafeCopyPadZeros_Tpl<char>(pDstBuffer, dstBufferSizeInBytes, pSrc);
}

void StringHelpers::SafeCopyPadZeros(wchar_t* const pDstBuffer, const size_t dstBufferSizeInBytes, const wchar_t* const pSrc)
{
	SafeCopyPadZeros_Tpl<wchar_t>(pDstBuffer, dstBufferSizeInBytes, pSrc);
}

//////////////////////////////////////////////////////////////////////////

bool StringHelpers::Utf16ContainsAsciiOnly(const wchar_t* wstr)
{
	while (*wstr)
	{
		if (*wstr > 127 || *wstr < 0)
		{
			return false;
		}
		++wstr;
	}
	return true;
}


string StringHelpers::ConvertAsciiUtf16ToAscii(const wchar_t* wstr)
{
	const size_t len = wcslen(wstr);

	string result;
	result.reserve(len);

	for (size_t i = 0; i < len; ++i)
	{
		result.push_back(wstr[i] & 0x7F);
	}

	return result;
}


wstring StringHelpers::ConvertAsciiToUtf16(const char* str)
{
	const size_t len = strlen(str);

	wstring result;
	result.reserve(len);

	for (size_t i = 0; i < len; ++i)
	{
		result.push_back(str[i] & 0x7F);
	}

	return result;
}


static string ConvertUtf16ToMultibyte(const wchar_t* wstr, uint codePage, char badChar)
{
	if (wstr[0] == 0)
	{
		return string();
	}

	const size_t len = wcslen(wstr);

	// Request needed buffer size, in bytes
	int neededByteCount = WideCharToMultiByte(
		codePage, 
		0, 
		wstr,
		len,
		0,
		0,
		((badChar && codePage != CP_UTF8) ? &badChar : NULL),
		NULL);
	if (neededByteCount <= 0)
	{
		return string();
	}
	++neededByteCount; // extra space for terminating zero

	std::vector<char> buffer(neededByteCount);

	const int byteCount = WideCharToMultiByte(
		codePage, 
		0, 
		wstr,
		len,
		&buffer[0], // output buffer
		neededByteCount - 1,  // size of the output buffer in bytes
		((badChar && codePage != CP_UTF8) ? &badChar : NULL),
		NULL);
	if (byteCount != neededByteCount - 1)
	{
		return string();
	}

	buffer[byteCount] = 0;

	return string(&buffer[0]);
}


static wstring ConvertMultibyteToUtf16(const char* str, uint codePage)
{
	if (str[0] == 0)
	{
		return wstring();
	}

	const size_t len = strlen(str);

	// Request needed buffer size, in characters
	int neededCharCount = MultiByteToWideChar(
		codePage, 
		0, 
		str,
		len,
		0,
		0);
	if (neededCharCount <= 0)
	{
		return wstring();
	}
	++neededCharCount; // extra space for terminating zero

	std::vector<wchar_t> wbuffer(neededCharCount);

	const int charCount = MultiByteToWideChar(
		codePage, 
		0, 
		str,
		len,
		&wbuffer[0], // output buffer
		neededCharCount - 1);  // size of the output buffer in characters
	if (charCount != neededCharCount - 1)
	{
		return wstring();
	}

	wbuffer[charCount] = 0;

	return wstring(&wbuffer[0]);
}


wstring StringHelpers::ConvertUtf8ToUtf16(const char* str)
{
	return ConvertMultibyteToUtf16(str, CP_UTF8);
}


string StringHelpers::ConvertUtf16ToUtf8(const wchar_t* wstr)
{
	return ConvertUtf16ToMultibyte(wstr, CP_UTF8, 0);
}


wstring StringHelpers::ConvertAnsiToUtf16(const char* str)
{
	return ConvertMultibyteToUtf16(str, CP_ACP);
}


string StringHelpers::ConvertUtf16ToAnsi(const wchar_t* wstr, char badChar)
{
	return ConvertUtf16ToMultibyte(wstr, CP_ACP, badChar);
}


string StringHelpers::ConvertAnsiToAscii(const char* str, char badChar)
{
	const size_t len = strlen(str);

	string result;
	result.reserve(len);

	for (size_t i = 0; i < len; ++i)
	{
		char c = str[i];
		if (c < 0 || c > 127)
		{
			c = badChar;
		}
		result.push_back(c);
	}

	return result;
}

// eof
