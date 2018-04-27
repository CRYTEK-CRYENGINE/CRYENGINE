// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PATH_H__
#define __PATH_H__

namespace PathHelpers
{
	// "abc/def/ghi" -> "abc/def"
	// "abc/def/ghi/" -> "abc/def/ghi"
	// "/" -> "/"
	string GetDirectory(const string& path);
	wstring GetDirectory(const wstring& path);

	string RemoveDuplicateSeparators(const string& path);
	wstring RemoveDuplicateSeparators(const wstring& path);

	bool IsRelative(const string& path);
	bool IsRelative(const wstring& path);

	string GetAbsolutePath(const char* pPath);
	string GetAbsolutePath(const wchar_t* pPath);

	// baseFolder and dependentPath passed should be in ASCII or UTF-8 encoding
	string GetShortestRelativePath(const string& baseFolder, const string& dependentPath);

	string CanonicalizePath(const string& path);
}

#endif //__PATH_H__
