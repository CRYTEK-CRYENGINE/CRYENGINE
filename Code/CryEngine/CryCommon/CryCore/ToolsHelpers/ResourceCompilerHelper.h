// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SettingsManagerHelpers.h"

class LineStreamBuffer;

enum ERcExitCode
{
	eRcExitCode_Success    = 0, //!< Must be 0.
	eRcExitCode_Error      = 1,
	eRcExitCode_FatalError = 100,
	eRcExitCode_Crash      = 101,
	eRcExitCode_UserFixing = 200,
	eRcExitCode_Pending    = 666,
};

//! Listener for synchronous resource-compilation.
//! Connects the listener to the output pipe of the RC.
struct IResourceCompilerListener
{
public:
	//! FbxImportDialog relies on this enum being in the order from most verbose to least verbose.
	enum MessageSeverity
	{
		MessageSeverity_Debug = 0,
		MessageSeverity_Info,
		MessageSeverity_Warning,
		MessageSeverity_Error
	};

	virtual void OnRCMessage(MessageSeverity severity, const char* text) = 0;
	virtual ~IResourceCompilerListener(){}
};

//! Provides settings and functions to make calls to RC.
class CResourceCompilerHelper
{
public:
	enum ERcCallResult
	{
		eRcCallResult_success,
		eRcCallResult_notFound,
		eRcCallResult_error,
		eRcCallResult_crash,
		eRcCallResult_queued
	};

	enum ERcExePath
	{
		eRcExePath_editor,
		eRcExePath_registry,
	};

#if defined(CRY_ENABLE_RC_HELPER)

	//! Returns a file path to RC's INI file.
	//! \return The number of characters written. If both buffer and bufferCount are 0, no characters are written to the buffer
	//!		and the return value is number of characters required to write the output.
	static int GetResourceCompilerConfigPath(char* buffer, size_t bufferCount, ERcExePath rcExePath = eRcExePath_registry);

	// Arguments:
	//   szFileName null terminated file path (0 can be used to test for rc.exe existence)
	//   szAdditionalSettings - 0 or e.g. "/refresh" or "/refresh /xyz=56"
	static ERcCallResult CallResourceCompiler(
	  const char* szFileName = 0,
	  const char* szAdditionalSettings = 0,
	  IResourceCompilerListener* listener = 0,
	  bool bMayShowWindow = true,
	  ERcExePath rcExePath = eRcExePath_registry,
	  bool bSilent = false,
	  bool bNoUserDialog = false,
	  const wchar_t* szWorkingDirectory = 0);

	static void        TerminateCalledResourceCompiler();

	static ERcExitCode InvokeResourceCompiler(
	  const char* szSrcFilePath,
	  const char* szDstFilePath,
	  const bool bUserDialog,
	  const bool bRefresh);
	static const char* GetCallResultDescription(ERcCallResult result);

	static bool CallProcess(
		const wchar_t* szStartingDirectory, 
		const wchar_t* szCommandLine, 
		bool bShowWindow, 
		LineStreamBuffer* pListener, 
		int& exitCode, 
		void* pEnvironment);

#endif // CRY_ENABLE_RC_HELPER

public:
	//! Little helper function (to stay independent).
	static const char* GetExtension(const char* in)
	{
		const size_t len = SettingsManagerHelpers::Utils::strlen(in);
		for (const char* p = in + len - 1; p >= in; --p)
		{
			switch (*p)
			{
			case ':':
			case '/':
			case '\\':
				// we've reached a path separator - it means there's no extension in this name
				return 0;
			case '.':
				// there's an extension in this file name
				return p + 1;
			}
		}
		return 0;
	}

	//! Little helper function (to stay independent).
	static void ReplaceExtension(const char* path, const char* new_ext, char* buffer, size_t bufferSizeInBytes)
	{
		const char* const ext = GetExtension(path);

		SettingsManagerHelpers::CFixedString<char, 512> p;
		if (ext)
		{
			p.set(path, ext - path);
			p.append(new_ext);
		}
		else
		{
			p.set(path);
			p.append(".");
			p.append(new_ext);
		}

		SettingsManagerHelpers::Utils::strcpy_with_clamp(buffer, bufferSizeInBytes, p.c_str());
	}

	//! Little helper function (to stay independent).
	static void RemovePath(const char* szFilePath, char* buffer, size_t bufferSizeInBytes)
	{
		const char* out = szFilePath;
		const char* chk = strrchr(out, '\\');
		if (chk)
		{
			out = chk + 1;
		}

		chk = strrchr(out, '/');
		if (chk)
		{
			out = chk + 1;
		}

		SettingsManagerHelpers::Utils::strcpy_with_clamp(buffer, bufferSizeInBytes, out);
	}

	//! Little helper function (to stay independent).
	static void RemoveFilename(const char* szFilePath, char* buffer, size_t bufferSizeInBytes)
	{
		SettingsManagerHelpers::Utils::strcpy_with_clamp(buffer, bufferSizeInBytes, szFilePath);

		char* out = buffer;
		char* chk = strrchr(out, '\\');
		if (chk)
		{
			out = chk;
		}

		chk = strrchr(out, '/');
		if (chk)
		{
			out = chk;
		}

		*out = '\0';
	}

public:
	//! \param szFilePath Could be source or destination filename.
	static void GetOutputFilename(const char* szFilePath, char* buffer, size_t bufferSizeInBytes)
	{
		const char* const ext = GetExtension(szFilePath);

		if (ext)
		{
			if (_stricmp(ext, "tif") == 0 ||
			    _stricmp(ext, "hdr") == 0)
			{
				ReplaceExtension(szFilePath, "dds", buffer, bufferSizeInBytes);
				return;
			}
		}

		SettingsManagerHelpers::Utils::strcpy_with_clamp(buffer, bufferSizeInBytes, szFilePath);
	}
};
