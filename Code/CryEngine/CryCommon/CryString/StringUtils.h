// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryStringUtils.h"
#include "CryString.h"
#include "UnicodeFunctions.h"

#ifndef NOT_USE_CRY_STRING
#include <CryCore/Platform/CryWindows.h>
#endif

#include <CryCore/CryCrc32.h>

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <ctype.h>
#endif

#include <algorithm>  // std::replace, std::min

namespace CryStringUtils
{

enum
{
	CRY_DEFAULT_HASH_SEED = 40503,    //!< This is a large 16 bit prime number (perfect for seeding).
};

//! Converts all ASCII characters to lower case.
//! This function is ASCII-only and locale agnostic.
//! \note Any non-ASCII characters are left unchanged.
inline string toLower(const string& str)
{
	string temp = str;

#ifndef NOT_USE_CRY_STRING
	temp.MakeLower();
#else
	std::transform(temp.begin(), temp.end(), temp.begin(), toLowerAscii); // STL MakeLower
#endif

	return temp;
}

//! Converts all ASCII characters to upper case.
//! This function is ASCII-only and locale agnostic.
//! \note Any non-ASCII characters are left unchanged.
inline string toUpper(const string& str)
{
	string temp = str;

#ifndef NOT_USE_CRY_STRING
	temp.MakeUpper();
#else
	std::transform(temp.begin(), temp.end(), temp.begin(), toUpperAscii); // STL MakeLower
#endif

	return temp;
}

//! Works like strstr, but is case-insensitive.
//! This function does not perform Unicode collation and uses the current CRT locale to perform case conversion.
inline const char* stristr(const char* szString, const char* szSubstring)
{
	int nSuperstringLength = (int)strlen(szString);
	int nSubstringLength = (int)strlen(szSubstring);

	for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
	{
		if (strnicmp(szString + nSubstringPos, szSubstring, nSubstringLength) == 0)
			return szString + nSubstringPos;
	}
	return NULL;
}

//! Converts the number to a string.
//! These functions are Unicode agnostic and locale agnostic (integral) or uses the current CRT locale (float, vector, quat, matrix).
inline string toString(unsigned nNumber)
{
	char szNumber[16];
	cry_sprintf(szNumber, "%u", nNumber);
	return szNumber;
}

inline string toString(signed int nNumber)
{
	char szNumber[16];
	cry_sprintf(szNumber, "%d", nNumber);
	return szNumber;
}

inline string toString(float nNumber)
{
	char szNumber[128];
	cry_sprintf(szNumber, "%f", nNumber);
	return szNumber;
}

inline string toString(bool nNumber)
{
	char szNumber[4];
	cry_sprintf(szNumber, "%i", (int)nNumber);
	return szNumber;
}

#ifdef MATRIX_H
inline string toString(const Matrix44& m)
{
	char szBuf[512];
	cry_sprintf(szBuf, "{%g,%g,%g,%g}{%g,%g,%g,%g}{%g,%g,%g,%g}{%g,%g,%g,%g}",
	            m(0, 0), m(0, 1), m(0, 2), m(0, 3),
	            m(1, 0), m(1, 1), m(1, 2), m(1, 3),
	            m(2, 0), m(2, 1), m(2, 2), m(2, 3),
	            m(3, 0), m(3, 1), m(3, 2), m(3, 3));
	return szBuf;
}
#endif

#ifdef _CRYQUAT_H
inline string toString(const CryQuat& q)
{
	char szBuf[256];
	cry_sprintf(szBuf, "{%g,{%g,%g,%g}}", q.w, q.v.x, q.v.y, q.v.z);
	return szBuf;
}
#endif

#ifdef VECTOR_H
inline string toString(const Vec3& v)
{
	char szBuf[128];
	cry_sprintf(szBuf, "{%g,%g,%g}", v.x, v.y, v.z);
	return szBuf;
}
#endif

//! Does the same as strstr, but the szString is allowed to be no more than the specified size.
//! This function is Unicode agnostic and locale agnostic.
inline const char* strnstr(const char* szString, const char* szSubstring, int nSuperstringLength)
{
	int nSubstringLength = (int)strlen(szSubstring);
	if (!nSubstringLength)
		return szString;

	for (int nSubstringPos = 0; szString[nSubstringPos] && nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
	{
		if (strncmp(szString + nSubstringPos, szSubstring, nSubstringLength) == 0)
			return szString + nSubstringPos;
	}
	return NULL;
}

//! Supports wildcard ? (matches one code-point) and * (matches zero or more code-points).
//! This function is Unicode aware and locale agnostic.
//! \note ANSI input is not supported, ASCII is fine since it's a subset of UTF-8.
//! \return true if the string matches the wildcard.
inline bool MatchWildcard(const char* szString, const char* szWildcard)
{
	return CryStringUtils_Internal::MatchesWildcards_Tpl<CryStringUtils_Internal::SCharComparatorCaseSensitive>(szString, szWildcard);
}

//! Supports wildcard ? (matches one code-point) and * (matches zero or more code-points).
//! This function is Unicode aware and uses the "C" locale for case comparison.
//! \note ANSI input is not supported, ASCII is fine since it's a subset of UTF-8.
//! \return true if the string matches the wildcard.
inline bool MatchWildcardIgnoreCase(const char* szString, const char* szWildcard)
{
	return CryStringUtils_Internal::MatchesWildcards_Tpl<CryStringUtils_Internal::SCharComparatorCaseInsensitive>(szString, szWildcard);
}

//! Calculates a hash value for a given string.
inline uint32 CalculateHash(const char* str)
{
	return CCrc32::Compute(str);
}

//! Calculates a hash value for the lower case version of a given string.
inline uint32 CalculateHashLowerCase(const char* str)
{
	return CCrc32::ComputeLowercase(str);
}

//! This function is Unicode and locale agnostic.
inline uint32 HashStringSeed(const char* string, const uint32 seed)
{
	// A string hash taken from the FRD/Crysis2 (game) code with low probability of clashes.
	// Recommend you use the CRY_DEFAULT_HASH_SEED (see HashString).
	const char* p;
	uint32 hash = seed;
	for (p = string; *p != '\0'; p++)
	{
		hash += *p;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

//! This function is ASCII-only and uses the standard "C" locale for case conversion.
inline uint32 HashStringLowerSeed(const char* string, const uint32 seed)
{
	// Computes the hash of 'string' converted to lower case.
	// Also see the comment in HashStringSeed.
	const char* p;
	uint32 hash = seed;
	for (p = string; *p != '\0'; p++)
	{
		hash += toLowerAscii(*p);
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

//! This function is Unicode agnostic and locale agnostic.
inline uint32 HashString(const char* string)
{
	return HashStringSeed(string, CRY_DEFAULT_HASH_SEED);
}

//! This function is ASCII-only and uses the standard "C" locale for case conversion.
inline uint32 HashStringLower(const char* string)
{
	return HashStringLowerSeed(string, CRY_DEFAULT_HASH_SEED);
}

//! Converts all ASCII characters in a string to lower case - avoids memory allocation.
//! This function is ASCII-only (Unicode remains unchanged) and uses the "C" locale for case conversion (A-Z only).
inline void toLowerInplace(string& str)
{
#ifndef NOT_USE_CRY_STRING
	str.MakeLower();
#else
	std::transform(str.begin(), str.end(), str.begin(), toLowerAscii); // STL MakeLower
#endif
}

//! Converts all characters in a null-terminated string to lower case - avoids memory allocation.
//! This function is ASCII-only (Unicode remains unchanged) and uses the "C" locale for case conversion (A-Z only).
inline void toLowerInplace(char* str)
{
	for (char* s = str; *s != 0; s++)
	{
		*s = toLowerAscii(*s);
	}
}

#ifndef NOT_USE_CRY_STRING

//! Converts a wide string (can be UTF-16 or UTF-32 depending on platform) to UTF-8.
//! This function is Unicode aware and locale agnostic.
template<typename T>
inline void WStrToUTF8(const wchar_t* str, T& dstr)
{
	string utf8;
	Unicode::Convert(utf8, str);

	// Note: This function expects T to have assign(ptr, len) function
	dstr.assign(utf8.c_str(), utf8.length());
}

//! Converts a wide string (can be UTF-16 or UTF-32 depending on platform) to UTF-8.
//! This function is Unicode aware and locale agnostic.
inline string WStrToUTF8(const wchar_t* str)
{
	return Unicode::Convert<string>(str);
}

//! Converts an UTF-8 string to wide string (can be UTF-16 or UTF-32 depending on platform).
//! This function is Unicode aware and locale agnostic.
template<typename T>
inline void UTF8ToWStr(const char* str, T& dstr)
{
	wstring wide;
	Unicode::Convert(wide, str);

	// Note: This function expects T to have assign(ptr, len) function
	dstr.assign(wide.c_str(), wide.length());
}

//! Converts an UTF-8 string to wide string (can be UTF-16 or UTF-32 depending on platform).
//! This function is Unicode aware and locale agnostic.
inline wstring UTF8ToWStr(const char* str)
{
	return Unicode::Convert<wstring>(str);
}


//! Converts an UTF-8 string to wide string (can be UTF-16 or UTF-32 depending on platform).
//! This function is Unicode aware and locale agnostic.
inline wstring UTF8ToWStrSafe(const char* szString)
{
	return Unicode::ConvertSafe<Unicode::eErrorRecovery_FallbackWin1252ThenReplace, wstring>(szString);
}

#ifdef CRY_PLATFORM_WINAPI
//! Converts a string from the local Windows codepage to UTF-8.
inline string ANSIToUTF8(const char* str)
{
	int wideLen = MultiByteToWideChar(CP_ACP, 0, str, -1, 0, 0);
	wchar_t* unicode = (wchar_t*)malloc(wideLen * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, str, -1, unicode, wideLen);
	string utf = CryStringUtils::WStrToUTF8(unicode);
	free(unicode);
	return utf;
}
#endif


#endif // NOT_USE_CRY_STRING

//! The type used to parse a yes/no string.
enum YesNoType
{
	nYNT_Yes,
	nYNT_No,
	nYNT_Invalid
};

//! Parse the yes/no string.
//! This function only accepts ASCII input, on Unicode content will return nYNT_Invalid.
inline YesNoType toYesNoType(const char* szString)
{
	if (!stricmp(szString, "yes")
	    || !stricmp(szString, "enable")
	    || !stricmp(szString, "true")
	    || !stricmp(szString, "1"))
		return nYNT_Yes;
	if (!stricmp(szString, "no")
	    || !stricmp(szString, "disable")
	    || !stricmp(szString, "false")
	    || !stricmp(szString, "0"))
		return nYNT_No;
	return nYNT_Invalid;
}

//! This function checks if the provided filename is valid.
//! This function only accepts ASCII input, on Unicode content will return false.
inline bool IsValidFileName(const char* fileName)
{
	size_t i = 0;
	for (;; )
	{
		const char c = fileName[i++];
		if (c == 0)
		{
			return true;
		}
		if (!((c >= '0' && c <= '9')
		      || (c >= 'A' && c <= 'Z')
		      || (c >= 'a' && c <= 'z')
		      || c == '.' || c == '-' || c == '_'))
		{
			return false;
		}
	}
}

/**************************************************************************
 *_splitpath() - split a path name into its individual components
 *
 * Purpose:
 *       to split a path name into its individual components
 *
 * Entry:
 *       path  - pointer to path name to be parsed
 *       drive - pointer to buffer for drive component, if any
 *       dir   - pointer to buffer for subdirectory component, if any
 *       fname - pointer to buffer for file base name component, if any
 *       ext   - pointer to buffer for file name extension component, if any
 *
 * Exit:
 *       drive - pointer to drive string.  Includes ':' if a drive was given.
 *       dir   - pointer to subdirectory string. Includes leading and trailing '/' or '\', if any.
 *       fname - pointer to file base name
 *       ext   - pointer to file extension, if any.  Includes leading '.'.
 *
 *******************************************************************************/
ILINE void portable_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	char* p;
	char* last_slash = NULL, * dot = NULL;
	unsigned len;
	PREFAST_ASSUME(path);

	/* we assume that the path argument has the following form, where any
	 * or all of the components may be missing.
	 *
	 *  <drive><dir><fname><ext>
	 *
	 * and each of the components has the following expected form(s)
	 *
	 *  drive:
	 *  0 to _MAX_DRIVE-1 characters, the last of which, if any, is a
	 *  ':'
	 *  dir:
	 *  0 to _MAX_DIR-1 characters in the form of an absolute path
	 *  (leading '/' or '\') or relative path, the last of which, if
	 *  any, must be a '/' or '\'.  E.g -
	 *  absolute path:
	 *      \top\next\last\     ; or
	 *      /top/next/last/
	 *  relative path:
	 *      top\next\last\  ; or
	 *      top/next/last/
	 *  Mixed use of '/' and '\' within a path is also tolerated
	 *  fname:
	 *  0 to _MAX_FNAME-1 characters not including the '.' character
	 *  ext:
	 *  0 to _MAX_EXT-1 characters where, if any, the first must be a
	 *  '.'
	 *
	 */

	/* extract drive letter and :, if any */

	if ((strlen(path) >= (_MAX_DRIVE - 2)) && (*(path + _MAX_DRIVE - 2) == (':')))
	{
		if (drive)
		{
			cry_strcpy(drive, _MAX_DRIVE, path);
		}
		path += _MAX_DRIVE - 1;
	}
	else if (drive)
	{
		*drive = ('\0');
	}

	/* extract path string, if any.  Path now points to the first character
	 * of the path, if any, or the filename or extension, if no path was
	 * specified.  Scan ahead for the last occurence, if any, of a '/' or
	 * '\' path separator character.  If none is found, there is no path.
	 * We will also note the last '.' character found, if any, to aid in
	 * handling the extension.
	 */

	for (last_slash = NULL, p = (char*)path; *p; p++)
	{
		if (*p == ('/') || *p == ('\\'))
			/* point to one beyond for later copy */
			last_slash = p + 1;
		else if (*p == ('.'))
			dot = p;
	}

	if (last_slash)
	{

		/* found a path - copy up through last_slash or max. characters
		 * allowed, whichever is smaller
		 */

		if (dir)
		{
			len = (std::min)((unsigned int)(((char*)last_slash - (char*)path) / sizeof(char)), (unsigned int)(_MAX_DIR - 1));
			memcpy(dir, path, len);
			*(dir + len) = ('\0');
		}
		path = last_slash;
	}
	else if (dir)
	{

		/* no path found */

		*dir = ('\0');
	}

	/* extract file name and extension, if any.  Path now points to the
	 * first character of the file name, if any, or the extension if no
	 * file name was given.  Dot points to the '.' beginning the extension,
	 * if any.
	 */

	if (dot && (dot >= path))
	{
		/* found the marker for an extension - copy the file name up to
		 * the '.'.
		 */
		if (fname)
		{
			len = (std::min)((unsigned int)(((char*)dot - (char*)path) / sizeof(char)), (unsigned int)(_MAX_FNAME - 1));
			memcpy(fname, path, len);
			*(fname + len) = ('\0');
		}
		/* now we can get the extension - remember that p still points
		 * to the terminating nul character of path.
		 */
		if (ext)
		{
			len = (std::min)((unsigned int)(((char*)p - (char*)dot) / sizeof(char)), (unsigned int)(_MAX_EXT - 1));
			memcpy(ext, dot, len);
			*(ext + len) = ('\0');
		}
	}
	else
	{
		/* found no extension, give empty extension and copy rest of
		 * string into fname.
		 */
		if (fname)
		{
			len = (std::min)((unsigned int)(((char*)p - (char*)path) / sizeof(char)), (unsigned int)(_MAX_FNAME - 1));
			memcpy(fname, path, len);
			*(fname + len) = ('\0');
		}
		if (ext)
		{
			*ext = ('\0');
		}
	}
}

/**************************************************************************
 * void _makepath() - build path name from components
 *
 * Purpose:
 *       create a path name from its individual components
 *
 * Entry:
 *       char *path  - pointer to buffer for constructed path
 *       char *drive - pointer to drive component, may or may not contain trailing ':'
 *       char *dir   - pointer to subdirectory component, may or may not include leading and/or trailing '/' or '\' characters
 *       char *fname - pointer to file base name component
 *       char *ext   - pointer to extension component, may or may not contain a leading '.'.
 *
 * Exit:
 *       path - pointer to constructed path name
 *
 *******************************************************************************/
ILINE void portable_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
	const char* p;

	/* we assume that the arguments are in the following form (although we
	 * do not diagnose invalid arguments or illegal filenames (such as
	 * names longer than 8.3 or with illegal characters in them)
	 *
	 *  drive:
	 *      A           ; or
	 *      A:
	 *  dir:
	 *      \top\next\last\     ; or
	 *      /top/next/last/     ; or
	 *      either of the above forms with either/both the leading
	 *      and trailing / or \ removed.  Mixed use of '/' and '\' is
	 *      also tolerated
	 *  fname:
	 *      any valid file name
	 *  ext:
	 *      any valid extension (none if empty or null )
	 */

	/* copy drive */

	if (drive && *drive)
	{
		*path++ = *drive;
		*path++ = (':');
	}

	/* copy dir */

	if ((p = dir) && *p)
	{
		do
		{
			*path++ = *p++;
		}
		while (*p);
		if (*(p - 1) != '/' && *(p - 1) != ('\\'))
		{
			*path++ = ('\\');
		}
	}

	/* copy fname */

	if (p = fname)
	{
		while (*p)
		{
			*path++ = *p++;
		}
	}

	/* copy ext, including 0-terminator - check to see if a '.' needs
	 * to be inserted.
	 */

	if (p = ext)
	{
		if (*p && *p != ('.'))
		{
			*path++ = ('.');
		}
		while (*path++ = *p++)
			;
	}
	else
	{
		/* better add the 0-terminator */
		*path = ('\0');
	}
}

namespace Wildcards
{
enum class EConstraintType : int32
{
	Invalid = -1,
	Literal,
	WildCardStar,
	WildCardQuestionMark,
};

struct SConstraintDesc
{
	inline SConstraintDesc(EConstraintType constraintType = EConstraintType::Invalid)
		: constraintType(constraintType)
		, literalLength(0)
		, szLiteral(nullptr)
	{
	}
	inline SConstraintDesc(const char* szLiteralStart, const char* szLiteralEnd)
		: constraintType(EConstraintType::Literal)
		, literalLength(static_cast<uint32>(szLiteralEnd - szLiteralStart))
		, szLiteral(szLiteralStart)
	{
	}
	EConstraintType constraintType;
	uint32          literalLength;
	const char*     szLiteral;
};

typedef std::vector<SConstraintDesc> TConstraintDescs;

struct SDescriptor
{
	inline SDescriptor() {}
	inline SDescriptor(const char* szPattern) { Build(szPattern); }
	inline void   Build(const char* szPattern);
	inline size_t GetWildCardCount() const;

private:
	inline void ProcessPendingLiteral(const char* pCurrentPos, const char* pLastPos);

public:
	TConstraintDescs constraintDescs;
};

struct SConstraintMatch
{
	inline SConstraintMatch()
		: szStart(nullptr)
		, size(0)
	{
	}
	const char* szStart;
	uint32      size;
};

typedef std::vector<SConstraintMatch> TConstraintMatches;

class CEvaluationResult
{
public:
	inline bool                      Process(const SDescriptor& desc, const char* const szString, const bool bCaseSensitive);
	inline bool                      Process(const SDescriptor& desc, const char* const szString, const size_t stringSize, const bool bCaseSensitive);

	inline void                      Clear()            { m_matches.clear(); }
	inline const TConstraintMatches& GetMatches() const { return m_matches; }

private:
	inline bool MatchWithFollowers(const SDescriptor& desc, const char* const szString, const int index, const size_t stringSize, const bool bCaseSensitive);
	inline bool CheckLiteral(const char* const szLiteral, const uint32 literalLength, const char* const szString, const bool bCaseSensitive);

	TConstraintMatches m_matches;
};

template<typename TPatternStringStorage>
class CProcessor
{
public:
	inline void Reset(const char* szPattern)
	{
		if (m_pattern != szPattern)
		{
			m_pattern = szPattern;
			m_descriptor.Build(m_pattern.c_str());
		}
	}

	inline bool Process(CEvaluationResult& outResult, const char* szStr, const bool bCaseSensitive) const
	{
		outResult.Clear();
		return outResult.Process(m_descriptor, szStr, bCaseSensitive);
	}

	inline bool Process(CEvaluationResult& outResult, const char* szStr, const size_t stringSize, const bool bCaseSensitive) const
	{
		outResult.Clear();
		return outResult.Process(m_descriptor, szStr, stringSize, bCaseSensitive);
	}

	inline const SDescriptor& GetDescriptor() const { return m_descriptor; }

	template<typename TFunc>
	inline void ForEachWildCardResult(const CEvaluationResult& result, const TFunc& func);

private:
	TPatternStringStorage m_pattern;
	SDescriptor           m_descriptor;
};

// SDescriptor implementation
void SDescriptor::Build(const char* szPattern)
{
	constraintDescs.clear();
	const char* pCurrentPos = szPattern;
	const char* pLastPos = pCurrentPos;
	while (*pCurrentPos)
	{
		switch (*pCurrentPos)
		{
		case '?':
			ProcessPendingLiteral(pCurrentPos, pLastPos);
			constraintDescs.emplace_back(EConstraintType::WildCardQuestionMark);
			pLastPos = ++pCurrentPos;
			break;
		case '*':
			ProcessPendingLiteral(pCurrentPos, pLastPos);
			constraintDescs.emplace_back(EConstraintType::WildCardStar);
			pLastPos = ++pCurrentPos;
			break;
		default:
			++pCurrentPos;
			break;
		}
	}
	ProcessPendingLiteral(pCurrentPos, pLastPos);
}

size_t SDescriptor::GetWildCardCount() const
{
	size_t result = 0;
	for (const SConstraintDesc& desc : constraintDescs)
	{
		if ((desc.constraintType == EConstraintType::WildCardStar) || (desc.constraintType == EConstraintType::WildCardQuestionMark))
		{
			++result;
		}
	}
	return result;
}

void SDescriptor::ProcessPendingLiteral(const char* pCurrentPos, const char* pLastPos)
{
	if (pCurrentPos != pLastPos)
	{
		constraintDescs.emplace_back(pLastPos, pCurrentPos);
	}
}

// CEvaluationResult implementation
bool CEvaluationResult::Process(const SDescriptor& desc, const char* const szString, const bool bCaseSensitive)
{
	size_t stringSize = strlen(szString);
	return Process(desc, szString, stringSize, bCaseSensitive);
}

bool CEvaluationResult::Process(const SDescriptor& desc, const char* const szString, const size_t stringSize, const bool bCaseSensitive)
{
	m_matches.clear();
	m_matches.resize(desc.constraintDescs.size());

	const bool bSuccess = MatchWithFollowers(desc, szString, 0, stringSize, bCaseSensitive);
	if (!bSuccess)
	{
		m_matches.clear();
	}
	return bSuccess;
}

bool CEvaluationResult::MatchWithFollowers(const SDescriptor& desc, const char* const szString, const int index, const size_t stringSize, const bool bCaseSensitive)
{
	bool bSuccess = false;
	if (index >= static_cast<int>(m_matches.size()))
	{
		bSuccess = (stringSize == 0);   // only succeed if the constraint matched perfectly with the end of the string
	}
	else
	{
		const SConstraintDesc& currentDesc = desc.constraintDescs[index];
		SConstraintMatch& currentMatchResult = m_matches[index];
		currentMatchResult.szStart = szString;

		const int nextIndex = index + 1;
		switch (currentDesc.constraintType)
		{
		case EConstraintType::Literal:
			if ((currentDesc.literalLength <= stringSize)
			    && CheckLiteral(currentDesc.szLiteral, currentDesc.literalLength, szString, bCaseSensitive))
			{
				const size_t remainingSize = stringSize - currentDesc.literalLength;
				currentMatchResult.size = currentDesc.literalLength;
				bSuccess = MatchWithFollowers(desc, szString + currentDesc.literalLength, nextIndex, remainingSize, bCaseSensitive);
			}
			break;
		case EConstraintType::WildCardStar:
			currentMatchResult.size = 0;
			if (index >= static_cast<int>(desc.constraintDescs.size()))
			{
				currentMatchResult.size = strlen(szString);
			}
			else
			{
				size_t remainingSize = stringSize;
				while ((!(bSuccess = MatchWithFollowers(desc, &szString[currentMatchResult.size], nextIndex, remainingSize, bCaseSensitive)))
				       && (szString[currentMatchResult.size]))
				{
					++currentMatchResult.size;
					--remainingSize;
				}
			}
			break;
		case EConstraintType::WildCardQuestionMark:
			if (*szString)
			{
				currentMatchResult.size = 1;
				bSuccess = MatchWithFollowers(desc, szString + 1, nextIndex, stringSize - 1, bCaseSensitive);
			}
			break;
		default:
			break;
		}
	}
	return bSuccess;
}

bool CEvaluationResult::CheckLiteral(const char* const szLiteral, const uint32 literalLength, const char* const szString, const bool bCaseSensitive)
{
	bool bResult = true;
	if (bCaseSensitive)
	{
		for (uint32 i = 0; i < literalLength; ++i)
		{
			if (szLiteral[i] != szString[i])
			{
				bResult = false;
				break;
			}
		}
	}
	else
	{
		for (uint32 i = 0; i < literalLength; ++i)
		{
			if (CryStringUtils::toLowerAscii(szLiteral[i]) != CryStringUtils::toLowerAscii(szString[i]))
			{
				bResult = false;
				break;
			}
		}
	}
	return bResult;
}

// CProcessor implementation
template<typename TPatternStringStorage>
template<typename TFunc>
void CProcessor<TPatternStringStorage >::ForEachWildCardResult(const CEvaluationResult& result, const TFunc& func)
{
	const TConstraintMatches& matches = result.GetMatches();
	const size_t count = matches.size();
	CRY_ASSERT_MESSAGE(count == m_descriptor.constraintDescs.size(), "Descriptor and result are out of sync. Either the last Process call failed or the result structure has already been reused after it.");
	if (count == m_descriptor.constraintDescs.size())
	{
		for (size_t i = 0; i < count; ++i)
		{
			switch (m_descriptor.constraintDescs[i].constraintType)
			{
			case EConstraintType::WildCardStar:
			case EConstraintType::WildCardQuestionMark:
				func(matches[i]);
				break;
			default:
				break;
			}
		}
	}
}
}

} // namespace CryStringUtils
