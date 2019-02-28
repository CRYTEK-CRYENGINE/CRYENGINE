// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(CRY_PLATFORM)
#error CRY_PLATFORM is not defined, probably #include "stdafx.h" is missing.
#endif

#if defined(CRY_ENABLE_RC_HELPER)

#undef RC_EXECUTABLE
#define RC_EXECUTABLE "rc.exe"

#include "ResourceCompilerHelper.h"
#include "EngineSettingsManager.h"
#include <CrySystem/File/LineStreamBuffer.h>
#include <CrySystem/CryUtils.h>

#pragma warning (disable:4312)

#include <CryCore/Platform/CryLibrary.h>
#include <shellapi.h> // ShellExecuteW()
#include <CryCore/Assert/CryAssert.h>

#include <stdio.h>
#include <memory> // std::unique_ptr for CryTIFPlugin.

// pseudo-variable that represents the DOS header of the module
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace
{

	class RcLock
	{
	public:
		RcLock()
		{
			InitializeCriticalSection(&m_cs);
		}
		~RcLock()
		{
			DeleteCriticalSection(&m_cs);
		}

		void Lock()
		{
			EnterCriticalSection(&m_cs);
		}
		void Unlock()
		{
			LeaveCriticalSection(&m_cs);
		}

	private:
		CRITICAL_SECTION m_cs;
	};

	template<class LockClass>
	class RcAutoLock
	{
	public:
		RcAutoLock(LockClass& lock)
			: m_lock(lock)
		{
			m_lock.Lock();
		}
		~RcAutoLock()
		{
			m_lock.Unlock();
		}

	private:
		RcAutoLock();
		RcAutoLock(const RcAutoLock<LockClass>&);
		RcAutoLock<LockClass>& operator=(const RcAutoLock<LockClass>&);

	private:
		LockClass& m_lock;
	};

	class CRcCancellationToken
	{
	public:
		void SetHandle(HANDLE handle)
		{
			RcAutoLock<RcLock> lock(m_lock);
			m_handle = handle;
		}

		void CancelProcess()
		{
			RcAutoLock<RcLock> lock(m_lock);
			if (m_handle)
			{
				TerminateProcess(m_handle, eRcExitCode_Crash);
				WaitForSingleObject(m_handle, INFINITE);
			}
		}

	private:
		HANDLE m_handle = 0;
		RcLock m_lock;
	};

	static bool CallProcessHelper(const wchar_t* szStartingDirectory, const wchar_t* szCommandLine, bool bShowWindow, LineStreamBuffer *pListener, int& exitCode, void* pEnvironment, CRcCancellationToken* pCancellationToken)
	{
		if (pCancellationToken)
		{
			pCancellationToken->SetHandle(0);
		}

		HANDLE hChildStdOutRd, hChildStdOutWr;
		HANDLE hChildStdInRd, hChildStdInWr;
		HANDLE handlesToInherit[2] = {};
		PROCESS_INFORMATION pi = { 0 };

		// Create a pipe to read the stdout of the RC.
		typedef std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, void(*)(LPPROC_THREAD_ATTRIBUTE_LIST)> AttributeListPtr;
		AttributeListPtr pAttributeList(nullptr, [](LPPROC_THREAD_ATTRIBUTE_LIST p) {});

		SECURITY_ATTRIBUTES saAttr = { 0 };
		if (pListener)
		{
			saAttr.bInheritHandle = TRUE;
			saAttr.lpSecurityDescriptor = 0;
			CreatePipe(&hChildStdOutRd, &hChildStdOutWr, &saAttr, 0);
			SetHandleInformation(hChildStdOutRd, HANDLE_FLAG_INHERIT, 0); // Need to do this according to MSDN
			CreatePipe(&hChildStdInRd, &hChildStdInWr, &saAttr, 0);
			SetHandleInformation(hChildStdInWr, HANDLE_FLAG_INHERIT, 0); // Need to do this according to MSDN

																		 // We do not want the RC process to inherit any handlers except a couple of std pipes to prevent cases when
																		 // deleting and renaming files in the engine can fail because there is more than one handle open to the same file.
			handlesToInherit[0] = hChildStdOutWr;
			handlesToInherit[1] = hChildStdInRd;
			SIZE_T size = 0;
			InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
			AttributeListPtr p(reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size)), [](LPPROC_THREAD_ATTRIBUTE_LIST p)
			{
				DeleteProcThreadAttributeList(p);
				HeapFree(GetProcessHeap(), 0, p);
			});
			if (p && InitializeProcThreadAttributeList(p.get(), 1, 0, &size))
			{
				UpdateProcThreadAttribute(p.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handlesToInherit, sizeof(handlesToInherit), nullptr, nullptr);
				pAttributeList = std::move(p);
			}
			CRY_ASSERT(pAttributeList);
		}

		STARTUPINFOEXW siex = { 0 };
		siex.StartupInfo.cb = sizeof(siex);
		siex.lpAttributeList = pAttributeList.get();
		siex.StartupInfo.dwX = 100;
		siex.StartupInfo.dwY = 100;
		if (pListener)
		{
			siex.StartupInfo.hStdError = hChildStdOutWr;
			siex.StartupInfo.hStdOutput = hChildStdOutWr;
			siex.StartupInfo.hStdInput = hChildStdInRd;
			siex.StartupInfo.dwFlags = STARTF_USEPOSITION | STARTF_USESTDHANDLES;
		}
		else
		{
			siex.StartupInfo.dwFlags = STARTF_USEPOSITION;
		}

		const DWORD creationFlags = (bShowWindow ? 0 : CREATE_NO_WINDOW) | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;

		SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> wRemoteCmdLine;
		wRemoteCmdLine.append(szCommandLine);

		if (!CreateProcessW(
			nullptr,                                   // No module name (use command line).
			const_cast<wchar_t*>(wRemoteCmdLine.c_str()),// Command line.
			nullptr,                                   // Process handle not inheritable.
			nullptr,                                   // Thread handle not inheritable.
			pListener ? true : false,                   // Handle inheritance.
			creationFlags,        // creation flags.
			pEnvironment,                              // if null, use parent's environment block.
			szStartingDirectory,                       // Set starting directory.
			&siex.StartupInfo,                         // Pointer to STARTUPINFO structure.
			&pi))                                      // Pointer to PROCESS_INFORMATION structure.
		{
			// The following  code block is commented out instead of being deleted
			// because it's good to have at hand for a debugging session.
#if 0
			const size_t charsInMessageBuffer = 32768;   // msdn about FormatMessage(): "The output buffer cannot be larger than 64K bytes."
			wchar_t szMessageBuffer[charsInMessageBuffer] = L"";
			FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szMessageBuffer, charsInMessageBuffer, NULL);
			GetCurrentDirectoryW(charsInMessageBuffer, szMessageBuffer);
#endif
			return false;
		}

		if (pCancellationToken)
		{
			pCancellationToken->SetHandle(pi.hProcess);
		}

		bool bFailedToReadOutput = false;

		if (pListener)
		{
			// Close the pipe that writes to the child process, since we don't actually have any input for it.
			CloseHandle(hChildStdInWr);

			// Read all the output from the child process.
			CloseHandle(hChildStdOutWr);
			for (;; )
			{
				char buffer[2048];
				DWORD bytesRead;
				if (!ReadFile(hChildStdOutRd, buffer, sizeof(buffer), &bytesRead, NULL) || (bytesRead == 0))
				{
					break;
				}
				pListener->HandleText(buffer, bytesRead);
			}
			CloseHandle(hChildStdOutRd);
			CloseHandle(hChildStdInRd);

			bFailedToReadOutput = pListener->IsTruncated();
		}

		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		if (pCancellationToken)
		{
			pCancellationToken->SetHandle(0);
		}

		DWORD processExitcode = 0;
		if (bFailedToReadOutput || GetExitCodeProcess(pi.hProcess, &processExitcode) == 0)
		{
			exitCode = eRcExitCode_Error;
		}

		exitCode = (int)processExitcode;

		// Close process and thread handles.
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		return true;
	}

	CRcCancellationToken s_rcCancellationToken;

}

//////////////////////////////////////////////////////////////////////////
// Modified version of of FileUtil::DirectoryExists() from Code/Tools/CryCommonTools/FileUtil.h
//
// Examples of paths that will return true:
//   "existing_dir", "existing_dir/", "existing_dir/subdir", "e:", "e://", "e:.", "e:/a", ".", "..", "//storage/builds"
// Examples of paths that will return false:
//   "", "//storage", "//storage/.", "nonexisting_dir", "f:/" (if f: drive doesn't exist)
static bool DirectoryExists(const wchar_t* szPathPart0, const wchar_t* szPathPart1 = 0)
{
	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 2> dir;

	if (szPathPart0 && szPathPart0[0])
	{
		dir.append(szPathPart0);
	}
	if (szPathPart1 && szPathPart1[0])
	{
		dir.append(szPathPart1);
	}

	const DWORD dwAttr = GetFileAttributesW(dir.c_str());
	return (dwAttr != INVALID_FILE_ATTRIBUTES) && ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

//////////////////////////////////////////////////////////////////////////
static bool IsRelativePath(const char* p)
{
	if (!p || !p[0])
	{
		return true;
	}
	return p[0] != '/' && p[0] != '\\' && !strchr(p, ':');
}

//////////////////////////////////////////////////////////////////////////
static void ShowMessageBoxRcNotFound(const wchar_t* const szCmdLine, const wchar_t* const szDir)
{
	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 4 + 150> tmp;

	if (szCmdLine && szCmdLine[0])
	{
		tmp.append(szCmdLine);
		tmp.append(L"\n\n");
	}
	if (szDir && szDir[0])
	{
		tmp.append(szDir);
		tmp.append(L"\n\n");
	}
	tmp.append(L"\n\n");
	tmp.append(L"ResourceCompiler (Tools\\rc\\rc.exe) was not found.\n\nPlease run Tools\\SettingsMgr.exe.");

	MessageBoxW(0, tmp.c_str(), L"Error", MB_ICONERROR | MB_OK);
}

//////////////////////////////////////////////////////////////////////////
static void CryFindEngineToolsFolder(unsigned int nEngineToolsPathSize, wchar_t* szEngineToolsPath)
{
	const size_t len = ::GetModuleFileNameW(CryGetCurrentModule(), szEngineToolsPath, nEngineToolsPathSize);

	if (len <= 0)
	{
		szEngineToolsPath[0] = L'\0';
		return;
	}

	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 2> filename;

	while (wchar_t* pathEnd = wcsrchr(szEngineToolsPath, L'\\'))
	{
		pathEnd[0] = L'\0';

		filename.clear();
		filename.append(szEngineToolsPath);
		filename.append(L"/Tools");

		if (filename.length() < nEngineToolsPathSize)
		{
			const DWORD dwAttr = ::GetFileAttributesW(filename.c_str());
			if (dwAttr != INVALID_FILE_ATTRIBUTES && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				memcpy(szEngineToolsPath, filename.c_str(), sizeof(szEngineToolsPath[0]) * (filename.length() + 1));
				return;
			}
		}
	}

	szEngineToolsPath[0] = L'\0';
}


//////////////////////////////////////////////////////////////////////////
namespace
{
	class ResourceCompilerLineHandler
	{
	public:
		ResourceCompilerLineHandler(IResourceCompilerListener* listener)
			: m_listener(listener)
		{
		}

		void HandleLine(const char* line)
		{
			if (!m_listener || !line)
			{
				return;
			}

			// check the first three characters to see if it's a warning or error.
			bool bHasPrefix;
			IResourceCompilerListener::MessageSeverity severity;
			if ((line[0] == 'E') && (line[1] == ':') && (line[2] == ' '))
			{
				bHasPrefix = true;
				severity = IResourceCompilerListener::MessageSeverity_Error;
				line += 3;  // skip the prefix
			}
			else if ((line[0] == 'W') && (line[1] == ':') && (line[2] == ' '))
			{
				bHasPrefix = true;
				severity = IResourceCompilerListener::MessageSeverity_Warning;
				line += 3;  // skip the prefix
			}
			else if ((line[0] == ' ') && (line[1] == ' ') && (line[2] == ' '))
			{
				bHasPrefix = true;
				severity = IResourceCompilerListener::MessageSeverity_Info;
				line += 3;  // skip the prefix
			}
			else
			{
				bHasPrefix = false;
				severity = IResourceCompilerListener::MessageSeverity_Info;
			}

			if (bHasPrefix)
			{
				// skip thread info "%d>", if present
				{
					const char* p = line;
					while (*p == ' ')
					{
						++p;
					}
					if (isdigit(*p))
					{
						while (isdigit(*p))
						{
							++p;
						}
						if (*p == '>')
						{
							line = p + 1;
						}
					}
				}

				// skip time info "%d:%d", if present
				{
					const char* p = line;
					while (*p == ' ')
					{
						++p;
					}
					if (isdigit(*p))
					{
						while (isdigit(*p))
						{
							++p;
						}
						if (*p == ':')
						{
							++p;
							if (isdigit(*p))
							{
								while (isdigit(*p))
								{
									++p;
								}
								while (*p == ' ')
								{
									++p;
								}
								line = p;
							}
						}
					}
				}
			}

			m_listener->OnRCMessage(severity, line);
		}

	private:
		IResourceCompilerListener* m_listener;
	};
}

//////////////////////////////////////////////////////////////////////////
int CResourceCompilerHelper::GetResourceCompilerConfigPath(char* outBuffer, size_t outBufferCount, CResourceCompilerHelper::ERcExePath rcExePath)
{
	bool dirFound = true;
	CSettingsManagerTools smTools = CSettingsManagerTools();
	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> rcIniPath;
	{
		wchar_t pathBuffer[512];
		switch (rcExePath)
		{
		case eRcExePath_registry:
			smTools.GetRootPathUtf16(true, SettingsManagerHelpers::CWCharBuffer(pathBuffer, sizeof(pathBuffer)));
			if (!pathBuffer[0])
			{
				dirFound = false;
			}
			else 
			{
				rcIniPath.append(&pathBuffer[0]);
				rcIniPath.append(L"/Tools/rc");
			}

			break;
		case eRcExePath_editor:
			CryFindEngineToolsFolder(CRY_ARRAY_COUNT(pathBuffer), pathBuffer);
			if (!pathBuffer[0])
			{
				dirFound = false;
			}
			else 
			{
				rcIniPath.append(&pathBuffer[0]);
				rcIniPath.append(L"/rc");
			}

			break;
		default:
			dirFound = false;
		}
	}
	if (!dirFound)
	{
		return snprintf(outBuffer, outBufferCount, "");
	}
	rcIniPath.appendAscii("/");
	rcIniPath.appendAscii("rc.ini");
	char buffer[MAX_PATH];

	SettingsManagerHelpers::GetAsciiFilename(rcIniPath.c_str(), SettingsManagerHelpers::CCharBuffer(buffer, sizeof(buffer)));

	return snprintf(outBuffer, outBufferCount, "%s", buffer);
}

//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::CallProcess(const wchar_t* szStartingDirectory, const wchar_t* szCommandLine, bool bShowWindow, LineStreamBuffer *pListener, int& exitCode, void* pEnvironment)
{
	return CallProcessHelper(szStartingDirectory, szCommandLine, bShowWindow, pListener, exitCode, pEnvironment, nullptr);
}

//////////////////////////////////////////////////////////////////////////
CResourceCompilerHelper::ERcCallResult CResourceCompilerHelper::CallResourceCompiler(
	const char* szFileName,
	const char* szAdditionalSettings,
	IResourceCompilerListener* listener,
	bool bMayShowWindow,
	CResourceCompilerHelper::ERcExePath rcExePath,
	bool bSilent,
	bool bNoUserDialog,
	const wchar_t* szWorkingDirectory)
{
	// make command for execution
	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> wRemoteCmdLine;
	CSettingsManagerTools smTools = CSettingsManagerTools();

	if (!szAdditionalSettings)
	{
		szAdditionalSettings = "";
	}

	SettingsManagerHelpers::CFixedString<wchar_t, MAX_PATH * 3> rcDirectory;
	{
		wchar_t pathBuffer[512];
		switch (rcExePath)
		{
		case eRcExePath_registry:
			smTools.GetRootPathUtf16(true, SettingsManagerHelpers::CWCharBuffer(pathBuffer, sizeof(pathBuffer)));
			if (!pathBuffer[0])
			{
				return eRcCallResult_notFound;
			}
			rcDirectory.append(&pathBuffer[0]);
			rcDirectory.append(L"/Tools/rc");
			break;
		case eRcExePath_editor:
			CryFindEngineToolsFolder(CRY_ARRAY_COUNT(pathBuffer), pathBuffer);
			if (!pathBuffer[0])
			{
				return eRcCallResult_notFound;
			}
			rcDirectory.append(&pathBuffer[0]);
			rcDirectory.append(L"/rc");
			break;
		default:
			return eRcCallResult_notFound;
		}
	}

	wchar_t szRegSettingsBuffer[1024];
	smTools.GetEngineSettingsManager()->GetValueByRef("RC_Parameters", SettingsManagerHelpers::CWCharBuffer(szRegSettingsBuffer, sizeof(szRegSettingsBuffer)));

	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.append(rcDirectory.c_str());
	wRemoteCmdLine.appendAscii("/");
	wRemoteCmdLine.appendAscii(RC_EXECUTABLE);
	wRemoteCmdLine.appendAscii("\"");

	if (!szFileName)
	{
		wRemoteCmdLine.appendAscii(" /userdialog=0 ");
		wRemoteCmdLine.appendAscii(szAdditionalSettings);
		wRemoteCmdLine.appendAscii(" ");
		wRemoteCmdLine.append(szRegSettingsBuffer);
	}
	else
	{
		wRemoteCmdLine.appendAscii(" \"");
		wRemoteCmdLine.appendAscii(szFileName);
		wRemoteCmdLine.appendAscii("\"");
		wRemoteCmdLine.appendAscii(bNoUserDialog ? " /userdialog=0 " : " /userdialog=1 ");
		wRemoteCmdLine.appendAscii(szAdditionalSettings);
		wRemoteCmdLine.appendAscii(" ");
		wRemoteCmdLine.append(szRegSettingsBuffer);
	}

	const wchar_t* const szStartingDirectory = szWorkingDirectory ? szWorkingDirectory : rcDirectory.c_str();

	bool bShowWindow;
	if (bMayShowWindow)
	{
		wchar_t buffer[20];
		smTools.GetEngineSettingsManager()->GetValueByRef("ShowWindow", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer)));
		bShowWindow = (wcscmp(buffer, L"true") == 0);
	}
	else
	{
		bShowWindow = false;
	}

	bool bSucceed = true;
	int exitCode;
	if (listener)
	{
		ResourceCompilerLineHandler lineHandler(listener);
		LineStreamBuffer lineBuffer(&lineHandler, &ResourceCompilerLineHandler::HandleLine);

		bSucceed = CallProcessHelper(
			szStartingDirectory,
			const_cast<wchar_t*>(wRemoteCmdLine.c_str()),
			bShowWindow,
			&lineBuffer,
			exitCode,
			nullptr,
			&s_rcCancellationToken);
	}
	else
	{
		bSucceed = CallProcessHelper(
			szStartingDirectory,
			const_cast<wchar_t*>(wRemoteCmdLine.c_str()),
			bShowWindow,
			nullptr,
			exitCode,
			nullptr,
			&s_rcCancellationToken);
	}

	if (!bSucceed)
	{
		if (!bSilent)
		{
			ShowMessageBoxRcNotFound(wRemoteCmdLine.c_str(), szStartingDirectory);
		}

		return eRcCallResult_notFound;
	}


	switch (exitCode)
	{
	case eRcExitCode_Success:
	case eRcExitCode_UserFixing:
		return eRcCallResult_success;
	case eRcExitCode_Crash:
		return eRcCallResult_crash;
	default:
		return eRcCallResult_error;
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceCompilerHelper::TerminateCalledResourceCompiler()
{
	s_rcCancellationToken.CancelProcess();
}

//////////////////////////////////////////////////////////////////////////
ERcExitCode CResourceCompilerHelper::InvokeResourceCompiler(const char* szSrcFilePath, const char* szDstFilePath, const bool bUserDialog, const bool bRefresh)
{
	ERcExitCode eRet = eRcExitCode_Pending;

	// make command for execution
	wchar_t szProjectDir[512];
	GetCurrentDirectoryW(CRY_ARRAY_COUNT(szProjectDir), szProjectDir);

	SettingsManagerHelpers::CFixedString<wchar_t, 512> wRemoteCmdLine;
	SettingsManagerHelpers::CFixedString<wchar_t, 512> wDir;
	CSettingsManagerTools smTools = CSettingsManagerTools();

	wchar_t szToolsDir[512];
	CryFindEngineToolsFolder(CRY_ARRAY_COUNT(szToolsDir), szToolsDir);
	if (!szToolsDir[0])
	{
		return eRcExitCode_FatalError;
	}

	wchar_t szRegSettingsBuffer[1024];
	smTools.GetEngineSettingsManager()->GetValueByRef("RC_Parameters", SettingsManagerHelpers::CWCharBuffer(szRegSettingsBuffer, sizeof(szRegSettingsBuffer)));

	// RC path
	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.append(szToolsDir);
	wRemoteCmdLine.appendAscii("/rc/");
	wRemoteCmdLine.appendAscii(RC_EXECUTABLE);
	wRemoteCmdLine.appendAscii("\"");

	// Parameters
	wRemoteCmdLine.appendAscii(" \"");
	if (IsRelativePath(szSrcFilePath))
	{
		wRemoteCmdLine.append(szProjectDir);
		wRemoteCmdLine.appendAscii("\\");
	}
	wRemoteCmdLine.appendAscii(szSrcFilePath);
	wRemoteCmdLine.appendAscii("\" ");
	wRemoteCmdLine.appendAscii(bUserDialog ? "/userdialog=1 " : "/userdialog=0 ");
	wRemoteCmdLine.appendAscii(bRefresh ? "/refresh=1 " : "/refresh=0 ");
	wRemoteCmdLine.append(szRegSettingsBuffer);

	// make it write to a filename of our choice
	char szDstFilename[512];
	char szDstPath[512];

	RemovePath(szDstFilePath, szDstFilename, CRY_ARRAY_COUNT(szDstFilename));
	RemoveFilename(szDstFilePath, szDstPath, CRY_ARRAY_COUNT(szDstPath));

	wRemoteCmdLine.appendAscii(" \"/overwritefilename=");
	wRemoteCmdLine.appendAscii(szDstFilename);
	wRemoteCmdLine.appendAscii("\"");
	wRemoteCmdLine.appendAscii(" \"/targetroot=");
	if (IsRelativePath(szDstPath))
	{
		wRemoteCmdLine.append(szProjectDir);
		wRemoteCmdLine.appendAscii("\\");
	}
	wRemoteCmdLine.appendAscii(szDstPath);
	wRemoteCmdLine.appendAscii("\"");

	wDir.append(szToolsDir);
	wDir.appendAscii("\\rc");

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwX = 100;
	si.dwY = 100;
	si.dwFlags = STARTF_USEPOSITION;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

#if defined(DEBUG) && !defined(NDEBUG) && defined(_RENDERER)
	extern ILog* iLog;

	char tmp1[512];
	char tmp2[512];
	SettingsManagerHelpers::CCharBuffer dst1(tmp1, 512);
	SettingsManagerHelpers::ConvertUtf16ToUtf8(wDir.c_str(), dst1);
	SettingsManagerHelpers::CCharBuffer dst2(tmp2, 512);
	SettingsManagerHelpers::ConvertUtf16ToUtf8(wRemoteCmdLine.c_str(), dst2);

	iLog->Log("Debug: RC: dir \"%s\", cmd \"%s\"\n", tmp1, tmp2);
#endif

	if (!CreateProcessW(
		NULL,                                                               // No module name (use command line).
		const_cast<wchar_t*>(wRemoteCmdLine.c_str()),                       // Command line.
		NULL,                                                               // Process handle not inheritable.
		NULL,                                                               // Thread handle not inheritable.
		FALSE,                                                              // Set handle inheritance to FALSE.
		BELOW_NORMAL_PRIORITY_CLASS + (bUserDialog ? 0 : CREATE_NO_WINDOW), // creation flags.
		NULL,                                                               // Use parent's environment block.
		wDir.c_str(),                                                       // Set starting directory.
		&si,                                                                // Pointer to STARTUPINFO structure.
		&pi))                                                               // Pointer to PROCESS_INFORMATION structure.
	{
		eRet = eRcExitCode_FatalError;
	}
	else
	{
		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD exitCode;
		if (GetExitCodeProcess(pi.hProcess, &exitCode) == 0)
		{
			eRet = eRcExitCode_Error;
		}
		else
		{
			eRet = (ERcExitCode)exitCode;
		}
	}

	// Close process and thread handles.
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return eRet;
}

//////////////////////////////////////////////////////////////////////////
const char* CResourceCompilerHelper::GetCallResultDescription(ERcCallResult result)
{
	switch (result)
	{
	case eRcCallResult_success:
		return "Success.";
	case eRcCallResult_notFound:
		return "ResourceCompiler executable was not found.";
	case eRcCallResult_error:
		return "ResourceCompiler exited with an error.";
	case eRcCallResult_crash:
		return "ResourceCompiler crashed! Please report this. Include source asset and this log in the report.";
	case eRcCallResult_queued:
		return "ResourceCompiler added to the background-processing queue.";
	default:
		return "Unexpected failure in ResourceCompilerHelper.";
	}
}
#endif //(CRY_ENABLE_RC_HELPER)

#if 0
//////////////////////////////////////////////////////////////////////////
const char* CResourceCompilerHelper::GetExtension(const char* in)
{
	const size_t len = strlen(in);
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

// little helper function (to stay independent)
void CResourceCompilerHelper::ReplaceExtension(const char* path, const char* new_ext, char* buffer, size_t bufferSizeInBytes)
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

	cry_strcpy(buffer, bufferSizeInBytes, p.c_str());
}

void CResourceCompilerHelper::GetOutputFilename(const char* szFilePath, char* buffer, size_t bufferSizeInBytes)
{
	const char* const ext = GetExtension(szFilePath);

	if (ext)
	{
		if (stricmp(ext, "tif") == 0 ||
			stricmp(ext, "hdr") == 0)
		{
			ReplaceExtension(szFilePath, "dds", buffer, bufferSizeInBytes);
			return;
		}
	}

	cry_strcpy(buffer, bufferSizeInBytes, szFilePath);
}

bool CResourceCompilerHelper::IsImageFormat(const char* szExtension)
{
	if (szExtension)
	{
		if (stricmp(szExtension, "dds") == 0 ||    // DirectX surface format
			stricmp(szExtension, "hdr") == 0)      // RGBE high dynamic range image
		{
			return true;
		}
	}

	return false;
}
#endif
