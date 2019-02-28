// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PathHelpers.h"
#include "StringHelpers.h"
#include "Util.h"

#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING
#include <CryCore/Platform/CryWindows.h>   // MAX_PATH
#include <CryString/UnicodeFunctions.h>
#include <CryString/CryPath.h>

template <class TS>
static inline TS GetDirectory_Tpl(const TS& path)
{
	static const TS::value_type separators[] = { '/', '\\', ':', 0 };
	const size_t pos = path.find_last_of(separators);

	if (pos == TS::npos)
	{
		return TS();
	}

	if (path[pos] == ':' || pos == 0 || path[pos - 1] == ':')
	{
		return path.substr(0, pos + 1);
	}

	// Handle paths like "\\machine"
	if (pos == 1 && (path[0] == '/' || path[0] == '\\'))
	{
		return path;
	}

	return path.substr(0, pos);
}

string PathHelpers::GetDirectory(const string& path)
{
	return  GetDirectory_Tpl(path);
}

wstring PathHelpers::GetDirectory(const wstring& path)
{
	return  GetDirectory_Tpl(path);
}

template <class TS>
static inline TS RemoveDuplicateSeparators_Tpl(const TS& path)
{
	if (path.length() <= 1)
	{
		return path;
	}

	TS ret;
	ret.reserve(path.length());

	const TS::value_type* p = path.c_str();

	// We start from the second char just to avoid damaging UNC paths with double backslash at the beginning (e.g. "\\Server04\file.txt")
	ret += *p++;

	while (*p)
	{
		ret += *p++;
		if (p[-1] == '\\' || p[-1] == '/')
		{
			while (*p == '\\' || *p == '/')
			{
				++p;
			}
		}
	}

	return ret;
}

string PathHelpers::RemoveDuplicateSeparators(const string& path)
{
	return RemoveDuplicateSeparators_Tpl(path);
}

wstring PathHelpers::RemoveDuplicateSeparators(const wstring& path)
{
	return RemoveDuplicateSeparators_Tpl(path);
}

template <class TS>
static inline bool IsRelative_Tpl(const TS& path)
{
	if (path.empty())
	{
		return true;
	}
	return path[0] != '/' && path[0] != '\\' && path.find(':') == TS::npos;
}

bool PathHelpers::IsRelative(const string& path)
{
	return IsRelative_Tpl(path);
}

bool PathHelpers::IsRelative(const wstring& path)
{
	return IsRelative_Tpl(path);
}

string PathHelpers::GetAbsolutePath(const char* pPath)
{
	wstring widePath;
	Unicode::Convert<Unicode::eEncoding_UTF16, Unicode::eEncoding_UTF8>(widePath, pPath);

	enum { kBufferLen = MAX_PATH };
	wchar_t bufferWchars[kBufferLen];

	if (!_wfullpath(bufferWchars, widePath.c_str(), kBufferLen))
	{
		return string();
	}

	string result;
	Unicode::Convert<Unicode::eEncoding_UTF8, Unicode::eEncoding_UTF16>(result, bufferWchars);
	return result;
}

string PathHelpers::GetAbsolutePath(const wchar_t* pPath)
{
	enum { kBufferLen = MAX_PATH };
	wchar_t bufferWchars[kBufferLen];

	if (!_wfullpath(bufferWchars, pPath, kBufferLen))
	{
		return string();
	}

	return Unicode::Convert<string>(bufferWchars);
}

string PathHelpers::GetShortestRelativePath(const string& baseFolder, const string& dependentPath)
{
	const string d = GetAbsolutePath(dependentPath.c_str());
	if (d.empty())
	{
		return PathHelpers::CanonicalizePath(dependentPath);
	}

	const string b = GetAbsolutePath(baseFolder.c_str());
	if (b.empty())
	{
		return PathHelpers::CanonicalizePath(dependentPath);
	}

	const string b2 = PathUtil::AddSlash(b);
	if (StringHelpers::StartsWithIgnoreCase(d, b2))
	{
		const size_t len = d.length() - b2.length();
		// note: len == 0 is possible in case of "C:\" and "C:\".
		return (len == 0) ? string(".") : d.substr(b2.length(), len);
	}

	std::vector<string> p0;
	StringHelpers::Split(b2, string("\\"), true, p0);
	std::vector<string> p1;
	StringHelpers::Split(d, string("\\"), true, p1);

	if (!StringHelpers::EqualsIgnoreCase(p0[0], p1[0]))
	{
		// got different drive letters
		return PathHelpers::CanonicalizePath(dependentPath);
	}

	if (StringHelpers::EqualsIgnoreCase(d, b))
	{
		// exactly same path
		return string(".");
	}

	// Search for first non-matching component
	for (int i = 1; i < (int)p0.size(); ++i)
	{
		if (StringHelpers::EqualsIgnoreCase(p0[i], p1[i]))
		{
			continue;
		}

		string s;
		s.reserve(Util::getMax(d.length(), b.length()));
		for (int j = i; j < (int)p0.size(); ++j)
		{
			if (!p0[j].empty())
			{
				s.append("..\\");
			}
		}
		for (int j = i; j < (int)p1.size(); ++j)
		{
			s.append(p1[j]);
			if (j + 1 < (int)p1.size())
			{
				s.push_back('\\');
			}
		}
		return s;
	}

	assert(0);
	return string();
}


string PathHelpers::CanonicalizePath( const string& path )
{
	string result = PathUtil::RemoveSlash(path);
	// remove .\ or ./ at the path beginning.
	if (result.length() > 2)
	{
		if (result[0] == '.' && (result[1] == '\\' || result[1] == '/'))
		{
			result = result.substr(2);
		}
	}

	return result;
}
