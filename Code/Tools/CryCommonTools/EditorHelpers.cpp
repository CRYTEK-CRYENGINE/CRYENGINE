// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "EditorHelpers.h"

#include "FileUtil.h"
#include "PathHelpers.h"
#include "StringHelpers.h"

#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <CryCore/Platform/CryWindows.h>
#include <CryString/CryPath.h>
#include <shellapi.h>
#include <shlobj.h>  // SHGetSpecialFolderPath

static bool ReadFileW(string& result, const wstring& filePath)
{
	result.clear();

	FILE* const file = _wfopen(filePath.c_str(), L"rb");
	if (!file)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size == 0)
	{
		return false;
	}

	result.resize(size);

	const bool bOk = fread((void*)result.data(), size, 1, file) == 1;

	fclose(file);

	if (!bOk)
	{
		return false;
	}

	return true;
}

static string GetLastSessionCmdLine()
{
	wchar_t pathBuffer[MAX_PATH];
	memset(pathBuffer, 0, sizeof(pathBuffer));
	SHGetSpecialFolderPathW(nullptr, pathBuffer, CSIDL_APPDATA, false);
	const wstring appData = pathBuffer;

	const wstring filePath = appData + L"\\Crytek\\CryENGINE\\LastSession.json";
	string content;
	if (!ReadFileW(content, filePath))
	{
		return string();
	}

	// Read Json.

	// We're only interested in a single string of the Json file, so very simple
	// parsing is sufficient.

	string key("\"cmdline\":");
	size_t cur = content.find(key);
	if (cur == string::npos)
	{
		return string();
	}

	cur += key.length();

	// Skip whitespace.
	while (isspace(content[cur])) cur++;

	// Expecting string literal.
	if (content[cur++] != '"')
	{
		return string();
	}

	string cmdline;
	while (cur < content.length())
	{
		const char c = content[cur];

		// Unescape next character
		if (c == '\\')
		{
			if (cur + 1 < content.length())
			{
				cmdline += content[++cur];
			}
		}
		else if (c == '"')
		{
			return cmdline;
		}
		else
		{
			cmdline += c;
		}

		cur++;
	}

	return string();
}

static string FindEditorExecutable(CSettingsManagerTools& smt)
{
	wchar_t buffer[1024];

	smt.GetRootPathUtf16(true, SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer)));

	if (buffer[0] <= 0)
	{
		::MessageBoxA(NULL, "Can't run the Editor.\nPlease setup correct CryENGINE root path by using Tools\\SettingsMgr.exe.", "Error", MB_ICONERROR | MB_OK);
		smt.CallSettingsManagerExe(0);
		smt.GetRootPathUtf16(true, SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer)));

		if (buffer[0] <= 0)
		{
			return string();
		}
	}

	string bufferString;
	Unicode::Convert(bufferString, buffer);

	std::vector<string> results;
	{
		std::vector<string> dirsToIgnore;
		dirsToIgnore.push_back(string("BinTemp"));
		dirsToIgnore.push_back(string("Code"));
		dirsToIgnore.push_back(string("Tools"));

		FileUtil::FindFiles(results, 1, bufferString, "", true, "Sandbox.exe", dirsToIgnore);
	}

	if (results.empty())
	{
		::MessageBoxA(NULL, "Can't find the Editor.\nPlease setup correct CryENGINE root path by using Tools\\SettingsMgr.exe.", "Error", MB_ICONERROR | MB_OK);
		return string();
	}

	return PathUtil::Make(bufferString, results[0]);
}


bool EditorHelpers::CallEditor(void** pEditorWindow, void* hParent, const char* szWindowName, const wchar_t* szOptions)
{
	// First, try to load commandline from last session.
	string commandLine = GetLastSessionCmdLine();
	if (!commandLine.empty())
	{
		wstring extCommandLine;
		StringHelpers::ConvertStringByRef(extCommandLine, commandLine);
		extCommandLine += L' ';
		extCommandLine += szOptions;

		STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));
		if (CreateProcessW(NULL, (LPWSTR)extCommandLine.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		{
			pEditorWindow = 0;
			return true;
		}
	}

   	CSettingsManagerTools smt;
	
	// DEPRECATED: The material editor widget cannot be identified by window title anymore in SandboxQt.
	HWND window = ::FindWindowA(NULL, szWindowName);
	if (window)
	{
		*pEditorWindow = window;
		return true;
	}
	else
	{
		*pEditorWindow = 0;

		string editorPath = FindEditorExecutable(smt);

		if (!editorPath.empty())
		{
			wstring weditorPath;
			Unicode::Convert(weditorPath, editorPath);

			const INT_PTR hIns = (INT_PTR)::ShellExecuteW(NULL, L"open", weditorPath.c_str(), szOptions, NULL, SW_SHOWNORMAL);
			if (hIns > 32)
			{
				return true;
			}
			else
			{
				::MessageBoxA(0,"Sandbox.exe was not found.\n\nPlease verify CryENGINE root path.","Error",MB_ICONERROR|MB_OK);
				smt.CallSettingsManagerExe(hParent);
				editorPath = FindEditorExecutable(smt);

				Unicode::Convert(weditorPath, editorPath);
				::ShellExecuteW(NULL, L"open", weditorPath.c_str(), szOptions, NULL, SW_SHOWNORMAL);
			}
		}
	}

	return false;
}
