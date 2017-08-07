// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PathHelpers.h"
#include "StringHelpers.h"
#include "Util.h"

#define CRY_SUPPRESS_CRYENGINE_WINDOWS_FUNCTION_RENAMING
#include <CryCore/Platform/CryWindows.h>   // MAX_PATH


// Returns position of last extension in last name (string::npos if not found)
// note: returns string::npos for names starting from '.' and having no
// '.' later (for example 'aaa/.ccc', 'a:.abc', '.rc')
template <class TS>
static inline size_t findExtensionPosition_Tpl(const TS& path)
{
	const size_t dotPos = path.rfind('.');
	if (dotPos == TS::npos)
	{
		return TS::npos;
	}

	static const TS::value_type separators[] = { '\\', '/', ':', 0 };
	const size_t separatorPos = path.find_last_of(separators);
	if (separatorPos != TS::npos)
	{
		if (separatorPos + 1 >= dotPos)
		{
			return TS::npos;
		}		
	}
	else if (dotPos == 0)
	{
		return TS::npos;
	}

	return dotPos + 1;
}

static size_t findExtensionPosition(const string& path)
{
	return findExtensionPosition_Tpl(path);
}

static size_t findExtensionPosition(const wstring& path)
{
	return findExtensionPosition_Tpl(path);
}


string PathHelpers::FindExtension(const string& path)
{
	const size_t extPos = findExtensionPosition(path);
	return (extPos == string::npos) ? string() : path.substr(extPos, string::npos);
}

wstring PathHelpers::FindExtension(const wstring& path)
{
	const size_t extPos = findExtensionPosition(path);
	return (extPos == wstring::npos) ? wstring() : path.substr(extPos, wstring::npos);
}


template <class TS>
static inline TS ReplaceExtension_Tpl(const TS& path, const TS& newExtension)
{
	if (path.empty())
	{
		return TS();
	}

	if (newExtension.empty())
	{
		return PathHelpers::RemoveExtension(path);
	}

	const TS::value_type last = path[path.length() - 1];
	if ((last == '\\') || (last == '/') || (last == ':') || (last == '.'))
	{
		return path;
	}

	const size_t extPos = findExtensionPosition(path);
	static const TS::value_type dot[] = { '.', 0 };
	return ((extPos == TS::npos) ? path + dot : path.substr(0, extPos)) + newExtension;
}

string PathHelpers::ReplaceExtension(const string& path, const string& newExtension)
{
	return ReplaceExtension_Tpl(path, newExtension);
}

wstring PathHelpers::ReplaceExtension(const wstring& path, const wstring& newExtension)
{
	return ReplaceExtension_Tpl(path, newExtension);
}


string PathHelpers::RemoveExtension(const string& path)
{
	const size_t extPos = findExtensionPosition(path);
	return (extPos == string::npos) ? path : path.substr(0, extPos - 1);
}

wstring PathHelpers::RemoveExtension(const wstring& path)
{
	const size_t extPos = findExtensionPosition(path);
	return (extPos == wstring::npos) ? path : path.substr(0, extPos - 1);
}


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
static inline TS GetFilename_Tpl(const TS& path)
{
	static const TS::value_type separators[] = { '/', '\\', ':', 0 };
	const size_t pos = path.find_last_of(separators);

	if (pos == TS::npos)
	{
		return path;
	}

	// Handle paths like "\\machine"
	if (pos == 1 && (path[0] == '/' || path[0] == '\\'))
	{
		return TS();
	}

	return path.substr(pos + 1, TS::npos);
}

string PathHelpers::GetFilename(const string& path)
{
	return GetFilename_Tpl(path);
}

wstring PathHelpers::GetFilename(const wstring& path)
{
	return GetFilename_Tpl(path);
}


template <class TS>
static inline TS AddSeparator_Tpl(const TS& path)
{
	if (path.empty())
	{
		return TS();
	}
	const TS::value_type last = path[path.length() - 1];
	if (last == '/' || last == '\\' || last == ':')
	{
		return path;
	}
	static const TS::value_type separator[] = { '\\', 0 };
	return path + separator;
}

string PathHelpers::AddSeparator(const string& path)
{
	return AddSeparator_Tpl(path);
}

wstring PathHelpers::AddSeparator(const wstring& path)
{
	return AddSeparator_Tpl(path);
}


template <class TS>
static inline TS RemoveSeparator_Tpl(const TS& path)
{
	if (path.empty())
	{
		return TS();
	}
	const TS::value_type last = path[path.length() - 1];
	if ((last == '/' || last == '\\') && path.length() > 1 && path[path.length() - 2] != ':')
	{
		return path.substr(0, path.length() - 1);
	}
	return path;
}

string PathHelpers::RemoveSeparator(const string& path)
{
	return RemoveSeparator_Tpl(path);
}

wstring PathHelpers::RemoveSeparator(const wstring& path)
{
	return RemoveSeparator_Tpl(path);
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
static inline TS Join_Tpl(const TS& path1, const TS& path2)
{
	if (path1.empty())
	{
		return path2;
	}
	if (path2.empty())
	{
		return path1;
	}

	if (!PathHelpers::IsRelative(path2))
	{
		assert(0 && "Join(): path2 is not relative");
		return TS();
	}

	const TS::value_type last = path1[path1.length() - 1];
	if (last == '/' || last == '\\' || last == ':')
	{
		return path1 + path2;
	}
	static const TS::value_type separator[] = { '\\', 0 };
	return path1 + separator + path2;
}


string PathHelpers::Join(const string& path1, const string& path2)
{
	return Join_Tpl(path1, path2);
}

wstring PathHelpers::Join(const wstring& path1, const wstring& path2)
{
	return Join_Tpl(path1, path2);
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


string PathHelpers::ToUnixPath(const string& path)
{
	return StringHelpers::Replace(path, '\\', '/');
}

wstring PathHelpers::ToUnixPath(const wstring& path)
{
	wstring s(path);
	std::replace(s.begin(), s.end(), L'\\', L'/');
	return s;
}


string PathHelpers::ToDosPath(const string& path)
{
	return StringHelpers::Replace(path, '/', '\\');
}

wstring PathHelpers::ToDosPath(const wstring& path)
{
	wstring s(path);
	std::replace(s.begin(), s.end(), L'/', L'\\');
	return s;
}


string PathHelpers::GetAsciiPath(const char* pPath)
{
	const wstring w = StringHelpers::ConvertUtf8ToUtf16(pPath);
	return GetAsciiPath(w.c_str());
}

string PathHelpers::GetAsciiPath(const wchar_t* pPath)
{
	if (!pPath[0])
	{
		return string();
	}

	wstring w = ToDosPath(RemoveSeparator(wstring(pPath)));

	if (StringHelpers::Utf16ContainsAsciiOnly(w.c_str()))
	{
		return StringHelpers::ConvertAsciiUtf16ToAscii(w.c_str());
	}

	// The path is non-ASCII, so let's resort to using short
	// filenames where needed (short names are always ASCII-only)

	// Long names components
	std::vector<wstring> p0;
	StringHelpers::Split(w, wstring(L"\\"), true, p0);

	// find last component that is not in ASCII char set
	int lastNonAscii;
	for (lastNonAscii = (int)p0.size() - 1; lastNonAscii >= 0; --lastNonAscii)
	{
		if (!StringHelpers::Utf16ContainsAsciiOnly(p0[lastNonAscii].c_str()))
		{
			break;
		}
	}
	assert(lastNonAscii >= 0);

	string res;
	res.reserve(w.length());

	w.clear();
	for (int i = 0; i <= lastNonAscii; ++i)
	{
		w.append(p0[i]);
		if (i < lastNonAscii)
		{
			w.push_back('\\');
		}
	}

	enum { kBufferLen = MAX_PATH };
	wchar_t bufferWchars[kBufferLen];

	const int charCount = GetShortPathNameW(w.c_str(), bufferWchars, kBufferLen);
	if (charCount <= 0 || charCount >= kBufferLen)
	{
		return string();
	}
	// Paranoid
	if (!StringHelpers::Utf16ContainsAsciiOnly(bufferWchars))
	{
		assert(0);
		return string();
	}

	// Short names components
	std::vector<wstring> p1;
	StringHelpers::Split(wstring(bufferWchars), wstring(L"\\"), true, p1);

	for (size_t i = 0; i < (int)p0.size(); ++i)
	{
		if (!p0[i].empty())
		{
			const wstring& p = 
				(i > lastNonAscii || StringHelpers::Utf16ContainsAsciiOnly(p0[i].c_str()))
				? p0[i]
				: p1[i];
			res.append(StringHelpers::ConvertAsciiUtf16ToAscii(p.c_str()));
		}
		if (i + 1 < (int)p0.size())
		{
			res.push_back('\\');
		}
	}

	return res;
}


string PathHelpers::GetAbsoluteAsciiPath(const char* pPath)
{
	const wstring w = StringHelpers::ConvertUtf8ToUtf16(pPath);
	return GetAbsoluteAsciiPath(w.c_str());
}

string PathHelpers::GetAbsoluteAsciiPath(const wchar_t* pPath)
{
	enum { kBufferLen = MAX_PATH };
	wchar_t bufferWchars[kBufferLen];

	if (!_wfullpath(bufferWchars, pPath, kBufferLen))
	{
		return string();
	}

	return GetAsciiPath(bufferWchars);
}


string PathHelpers::GetShortestRelativeAsciiPath(const string& baseFolder, const string& dependentPath)
{
	const string d = GetAbsoluteAsciiPath(dependentPath.c_str());
	if (d.empty())
	{
		return PathHelpers::CanonicalizePath(dependentPath);
	}

	const string b = GetAbsoluteAsciiPath(baseFolder.c_str());
	if (b.empty())
	{
		return PathHelpers::CanonicalizePath(dependentPath);
	}

	const string b2 = AddSeparator(b);
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
	string result = RemoveSeparator(path);
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
