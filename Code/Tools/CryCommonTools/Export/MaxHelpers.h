// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MAXHELPERS_H__
#define __MAXHELPERS_H__

#include "PathHelpers.h"
#include "StringHelpers.h"


namespace MaxHelpers
{
	enum { kBadChar = '_' };

#if !defined(MAX_PRODUCT_VERSION_MAJOR)
	#error MAX_PRODUCT_VERSION_MAJOR is undefined
#elif (MAX_PRODUCT_VERSION_MAJOR >= 15)
	static_assert(sizeof(MCHAR) == 2, "Invalid type size!");
	#define MAX_MCHAR_SIZE 2
	typedef wstring MaxCompatibleString;
#elif (MAX_PRODUCT_VERSION_MAJOR >= 12)
	static_assert(sizeof(MCHAR) == 1, "Invalid type size!");
	#define MAX_MCHAR_SIZE 1
	typedef string MaxCompatibleString;
#else
	#error 3dsMax 2009 and older are not supported anymore
#endif

	inline string CreateAsciiString(const char* s_ansi)
	{
		return StringHelpers::ConvertAnsiToAscii(s_ansi, kBadChar);
	}


	inline string CreateAsciiString(const wchar_t* s_utf16)
	{
		const string s_ansi = StringHelpers::ConvertUtf16ToAnsi(s_utf16, kBadChar);
		return CreateAsciiString(s_ansi.c_str());
	}


	inline string CreateUtf8String(const char* s_ansi)
	{
		return StringHelpers::ConvertAnsiToUtf8(s_ansi);
	}


	inline string CreateUtf8String(const wchar_t* s_utf16)
	{
		return StringHelpers::ConvertUtf16ToUtf8(s_utf16);
	}


	inline string CreateTidyAsciiNodeName(const char* s_ansi)
	{
		const size_t len = strlen(s_ansi);

		string res;
		res.reserve(len);

		for (size_t i = 0; i < len; ++i)
		{
			char c = s_ansi[i];
			if (c < ' ' || c >= 127)
			{
				c = kBadChar;
			}
			res.append(1, c);
		}
		return res;
	}


	inline string CreateTidyAsciiNodeName(const wchar_t* s_utf16)
	{
		const string s_ansi = StringHelpers::ConvertUtf16ToAnsi(s_utf16, kBadChar);
		return CreateTidyAsciiNodeName(s_ansi.c_str());;
	}


	inline MSTR CreateMaxStringFromAscii(const char* s_ascii)
	{
#if (MAX_MCHAR_SIZE == 2)
		return MSTR(StringHelpers::ConvertAsciiToUtf16(s_ascii).c_str());
#else
		return MSTR(s_ascii);
#endif
	}


	inline MaxCompatibleString CreateMaxCompatibleStringFromAscii(const char* s_ascii)
	{
#if (MAX_MCHAR_SIZE == 2)
		return StringHelpers::ConvertAsciiToUtf16(s_ascii);
#else
		return MaxCompatibleString(s_ascii);
#endif
	}


	inline string GetAbsoluteAsciiPath(const char* s_ansi)
	{
		if (!s_ansi || !s_ansi[0])
		{
			return string();
		}

		return CreateAsciiString(PathHelpers::GetAbsolutePath(StringHelpers::ConvertAnsiToUtf16(s_ansi).c_str()).c_str());
	}


	inline string GetAbsoluteAsciiPath(const wchar_t* s_utf16)
	{
		if (!s_utf16 || !s_utf16[0])
		{
			return string();
		}
		return CreateAsciiString(PathHelpers::GetAbsolutePath(s_utf16).c_str());
	}
}

#endif
