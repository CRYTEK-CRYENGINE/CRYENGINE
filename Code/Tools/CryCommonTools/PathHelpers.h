// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PATH_H__
#define __PATH_H__

namespace PathHelpers
{
	string FindExtension(const string& path);
	wstring FindExtension(const wstring& path);

	string ReplaceExtension(const string& path, const string& newExtension);
	wstring ReplaceExtension(const wstring& path, const wstring& newExtension);

	string RemoveExtension(const string& path);
	wstring RemoveExtension(const wstring& path);

	// "abc/def/ghi" -> "abc/def"
	// "abc/def/ghi/" -> "abc/def/ghi"
	// "/" -> "/"
	string GetDirectory(const string& path);
	wstring GetDirectory(const wstring& path);

	string GetFilename(const string& path);
	wstring GetFilename(const wstring& path);

	string AddSeparator(const string& path);
	wstring AddSeparator(const wstring& path);

	string RemoveSeparator(const string& path);
	wstring RemoveSeparator(const wstring& path);

	string RemoveDuplicateSeparators(const string& path);
	wstring RemoveDuplicateSeparators(const wstring& path);

	// It's not allowed to pass an absolute path in path2.
	// Join(GetDirectory(fname), GetFilename(fname)) returns fname.
	string Join(const string& path1, const string& path2);
	wstring Join(const wstring& path1, const wstring& path2);

	bool IsRelative(const string& path);
	bool IsRelative(const wstring& path);

	string ToUnixPath(const string& path);
	wstring ToUnixPath(const wstring& path);

	string ToDosPath(const string& path);
	wstring ToDosPath(const wstring& path);

	// char* pPath: in ASCII or UTF-8 encoding
	// wchar_t* pPath: in UTF-16 encoding
	// Non-ASCII components of pPath (everything from &pPath[0] to last non-ASCII
	// part, inclusively) should exist on disk, otherwise an empty string is returned.
	string GetAsciiPath(const char* pPath);
	string GetAsciiPath(const wchar_t* pPath);

	// pPath passed should be in ASCII or UTF-8 encoding
	string GetAbsoluteAsciiPath(const char* pPath);
	// pPath passed should be in UTF-16 encoding
	string GetAbsoluteAsciiPath(const wchar_t* pPath);

	// baseFolder and dependentPath passed should be in ASCII or UTF-8 encoding
	string GetShortestRelativeAsciiPath(const string& baseFolder, const string& dependentPath);

	string CanonicalizePath(const string& path);
}

#endif //__PATH_H__
