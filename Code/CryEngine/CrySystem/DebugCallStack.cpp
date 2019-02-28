// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   debugcallstack.cpp
//  Version:     v1.00
//  Created:     1/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "DebugCallStack.h"
#include <CryThreading/IThreadManager.h>

#include <mutex>
#include <condition_variable>
#include <chrono>

#if CRY_PLATFORM_WINDOWS

	#include <CrySystem/IConsole.h>
	#include <CryScriptSystem/IScriptSystem.h>
	#include "JiraClient.h"
	#include "System.h"
	#include <CryCore/CryCustomTypes.h>

	#include "resource.h"
LINK_SYSTEM_LIBRARY("version.lib")

	#include <CryCore/Platform/CryWindows.h>
	#include <dbghelp.h> // requires <windows.h>
	#pragma comment( lib, "dbghelp" )
	#pragma warning(push)
	#pragma warning(disable: 4244) //conversion' conversion from 'type1' to 'type2', possible loss of data

	#ifdef CRY_USE_CRASHRPT
		#include <CrashRpt.h>
		bool g_bCrashRptInstalled = false;
	#endif // CRY_USE_CRASHRPT

	#define MAX_PATH_LENGTH   1024
	#define MAX_SYMBOL_LENGTH 512

//! Needs one external of DLL handle.
extern HMODULE gDLLHandle;
static FILE* gMemAllocFile;

static HWND hwndException = 0;
static bool g_bUserDialog = true;     // true=on crash show dialog box, false=supress user interaction

static int         PrintException(EXCEPTION_POINTERS* pex);

static bool        IsFloatingPointException(EXCEPTION_POINTERS* pex);

extern LONG WINAPI CryEngineExceptionFilterWER(struct _EXCEPTION_POINTERS* pExceptionPointers);
extern LONG WINAPI CryEngineExceptionFilterMiniDump(struct _EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath, MINIDUMP_TYPE mdumpValue);

//=============================================================================
CONTEXT CaptureCurrentContext()
{
	CONTEXT context;
	memset(&context, 0, sizeof(context));
	context.ContextFlags = CONTEXT_FULL;
	RtlCaptureContext(&context);

	return context;
}

static volatile int s_exception_handler_lock = 0;
LONG __stdcall CryUnhandledExceptionHandler(EXCEPTION_POINTERS* pex)
{
	CrySpinLock(&s_exception_handler_lock, 0, 1);
	return DebugCallStack::instance()->handleException(pex);
}

BOOL CALLBACK EnumModules(
  PCSTR ModuleName,
  DWORD64 BaseOfDll,
  PVOID UserContext)
{
	DebugCallStack::TModules& modules = *static_cast<DebugCallStack::TModules*>(UserContext);
	modules[(void*)BaseOfDll] = ModuleName;

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
class CCaptureCrashScreenShot : public IThread
{
public:
	void ThreadEntry()
	{
		m_threadId = CryGetCurrentThreadId();

		for (; !m_interrupt_flag;)
		{
			// Wait for work
			{
				std::unique_lock<std::mutex> l(m);
				m_cvSignal.wait(l, [this]() { return m_interrupt_flag || m_signal_flag; });
			}

			if (!m_interrupt_flag)
				IDebugCallStack::Screenshot("error.jpg");

			// Signal capture end
			{
				std::unique_lock<std::mutex> l(m);
				m_signal_flag = false;
				m_cvCapture.notify_all();
			}
		}
	}

	// Signal interrupt
	void SignalStopWork()
	{
		std::unique_lock<std::mutex> l(m);
		m_interrupt_flag = true;
		m_cvSignal.notify_one();
		m_cvCapture.notify_all();
	}

	// Signal capture, and wait for completion.
	// Returns true if captured, false if interrupted or timed-out.
	template <class Rep, class Period>
	bool SignalCaptureAndWait(const std::chrono::duration<Rep, Period> &duration = std::chrono::steady_clock::duration::max())
	{
		{
			// Notify worker
			std::unique_lock<std::mutex> l(m);
			m_signal_flag = true;
			m_cvSignal.notify_one();
		}

		{
			// Wait
			std::unique_lock<std::mutex> l(m);
			m_cvCapture.wait_for(l, duration, [this]() { return m_interrupt_flag || !m_signal_flag; });
			return !m_signal_flag;
		}
	}

	const threadID& GetThreadId() const noexcept { return m_threadId; }

private:
	std::mutex              m;
	std::condition_variable m_cvSignal, m_cvCapture;

	bool                    m_interrupt_flag = false;
	bool                    m_signal_flag = false;
	threadID                m_threadId = THREADID_NULL;
};

CCaptureCrashScreenShot g_screenShotThread;

MINIDUMP_TYPE GetMiniDumpType()
{
	switch (g_cvars.sys_dump_type)
	{
	case 0:
		return (MINIDUMP_TYPE)(MiniDumpValidTypeFlags + 1); // guaranteed to be invalid
		break;
	case 1:
		return MiniDumpNormal;
		break;
	case 2:
		return (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs);
		break;
	case 3:
		return MiniDumpWithFullMemory;
		break;
	default:
		return (MINIDUMP_TYPE)g_cvars.sys_dump_type;
		break;
	}
}

	#ifdef CRY_USE_CRASHRPT
struct CCrashRptCVars
{
	int    sys_crashrpt = 0;
	ICVar* sys_crashrpt_server = nullptr;
	ICVar* sys_crashrpt_privacypolicy = nullptr;
	ICVar* sys_crashrpt_email = nullptr;
	ICVar* sys_crashrpt_appname = nullptr;
	ICVar* sys_crashrpt_appversion = nullptr;
} g_crashrpt_cvars;

void CCrashRpt::RegisterCVars()
{
	REGISTER_CVAR2("sys_crashrpt", &g_crashrpt_cvars.sys_crashrpt, g_crashrpt_cvars.sys_crashrpt, VF_NULL, "Enable CrashRpt crash reporting library.");
	g_crashrpt_cvars.sys_crashrpt_server = REGISTER_STRING_CB("sys_crashrpt_server", "http://localhost:80/crashrpt/crashrpt.php", VF_NULL, "CrashRpt server url address for crash submission", &ReInstallCrashRptHandler);
	g_crashrpt_cvars.sys_crashrpt_privacypolicy = REGISTER_STRING_CB("sys_crashrpt_privacypolicy", "privacy link", VF_NULL, "CrashRpt privacy policy description url", &ReInstallCrashRptHandler);
	g_crashrpt_cvars.sys_crashrpt_email = REGISTER_STRING_CB("sys_crashrpt_email", "", VF_NULL, "CrashRpt default submission e-mail", &ReInstallCrashRptHandler);
	g_crashrpt_cvars.sys_crashrpt_appname = REGISTER_STRING_CB("sys_crashrpt_appname", "", VF_NULL, "CrashRpt application name (ex: CRYENGINE)", &ReInstallCrashRptHandler);
	g_crashrpt_cvars.sys_crashrpt_appversion = REGISTER_STRING_CB("sys_crashrpt_appversion", "", VF_NULL, "CrashRpt optional application version", &ReInstallCrashRptHandler);
	REGISTER_COMMAND("sys_crashrpt_generate", CmdGenerateCrashReport, VF_CHEAT, "Forces CrashRpt report to be generated");

	// Reinstall crash handler with updated cvar values
	ReInstallCrashRptHandler(0);
}

bool CCrashRpt::InstallHandler()
{
	g_bCrashRptInstalled = false;
	if (!g_crashrpt_cvars.sys_crashrpt)
		return false;

	char appVersionBuffer[256];

	// Define CrashRpt configuration parameters
	CR_INSTALL_INFOA info;
	memset(&info, 0, sizeof(CR_INSTALL_INFO));
	info.cb = sizeof(CR_INSTALL_INFO);
	info.pszAppName = NULL; //NULL == Use exe filname _T("CRYENGINE");
	info.pszAppVersion = NULL; //NULL == Extract from the executable  _T("1.0.0");
	MINIDUMP_TYPE mdumpType = GetMiniDumpType();
	if (mdumpType == (MINIDUMP_TYPE)(mdumpType & MiniDumpValidTypeFlags))
	{
		info.uMiniDumpType = mdumpType;
	}
	if (g_crashrpt_cvars.sys_crashrpt_appname && 0 != strlen(g_crashrpt_cvars.sys_crashrpt_appname->GetString()))
	{
		info.pszAppName = g_crashrpt_cvars.sys_crashrpt_appname->GetString();
	}
	if (g_crashrpt_cvars.sys_crashrpt_appversion && 0 != strlen(g_crashrpt_cvars.sys_crashrpt_appversion->GetString()))
	{
		info.pszAppVersion = g_crashrpt_cvars.sys_crashrpt_appversion->GetString();
	}
	else
	{
#if CRY_PLATFORM_WINDOWS
		SFileVersion ver = GetSystemVersionInfo();
		ver.ToString(appVersionBuffer);
		info.pszAppVersion = appVersionBuffer;
#endif //CRY_PLATFORM_WINDOWS
	}
	info.pszEmailSubject = NULL;
	if (g_crashrpt_cvars.sys_crashrpt_email)
	{
		info.pszEmailTo = g_crashrpt_cvars.sys_crashrpt_email->GetString();
	}
	if (g_crashrpt_cvars.sys_crashrpt_server)
	{
		info.pszUrl = g_crashrpt_cvars.sys_crashrpt_server->GetString();
	}
	info.uPriorities[CR_HTTP] = 3;  // First try send report over HTTP
	info.uPriorities[CR_SMTP] = 2;  // Second try send report over SMTP
	info.uPriorities[CR_SMAPI] = 1; // Third try send report over Simple MAPI
									// Install all available exception handlers
	info.dwFlags |= CR_INST_ALL_POSSIBLE_HANDLERS;
	info.dwFlags |= CR_INST_SHOW_ADDITIONAL_INFO_FIELDS;
	info.dwFlags |= CR_INST_AUTO_THREAD_HANDLERS;

	// Define the Privacy Policy URL
	if (g_crashrpt_cvars.sys_crashrpt_privacypolicy)
	{
		info.pszPrivacyPolicyURL = g_crashrpt_cvars.sys_crashrpt_privacypolicy->GetString();
	}

	// Install crash reporting
	int nResult = crInstallA(&info);
	if (nResult != 0)
	{
		// Something goes wrong. Get error message.
		TCHAR szErrorMsg[512] = "";
		crGetLastErrorMsg(szErrorMsg, 512);
		CryLogAlways("%s\n", szErrorMsg);
		return false;
	}
	g_bCrashRptInstalled = true;

	// Take screenshot of the app window at the moment of crash
	//crAddScreenshot2(CR_AS_MAIN_WINDOW | CR_AS_USE_JPEG_FORMAT, 95);

	// Add our log file to the error report
	const char* logFileName = gEnv->pLog->GetFileName();
	if (logFileName)
	{
		crAddFile2(logFileName, NULL, "Log File", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE);
	}
	else
	{
		crAddFile2("game.log", NULL, "Game Log File", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE);
		crAddFile2("editor.log", NULL, "Editor Log File", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE);
	}
	crAddFile2("error.log", NULL, "Error log", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE);
	crAddFile2("error.jpg", NULL, "Screenshot", CR_AF_TAKE_ORIGINAL_FILE | CR_AF_MISSING_FILE_OK | CR_AF_ALLOW_DELETE);

	// Set crash callback function
	crSetCrashCallback(&CrashCallback, NULL);

	return true;
}

void CCrashRpt::UninstallHandler()
{
	if (g_bCrashRptInstalled)
	{
		crUninstall();
	}
	g_bCrashRptInstalled = false;
}

int CALLBACK CCrashRpt::CrashCallback(CR_CRASH_CALLBACK_INFO* pInfo)
{
	static volatile bool s_bHandleExceptionInProgressLock = false;
	if (s_bHandleExceptionInProgressLock)
	{
		return CR_CB_CANCEL;
	}

	switch (pInfo->nStage)
	{
	case CR_CB_STAGE_PREPARE:
	{
		if (gEnv && gEnv->pLog)
		{
			s_bHandleExceptionInProgressLock = true;
			((DebugCallStack*)DebugCallStack::instance())->MinimalExceptionReport(pInfo->pExceptionInfo->pexcptrs);
			s_bHandleExceptionInProgressLock = false;
		}
	}
	break;
	case CR_CB_STAGE_FINISH:
		break;
	}

	// Proceed with crash report generation.
	// This return code also makes CrashRpt to not call this callback function for
	// the next crash report generation stage.
	return CR_CB_NOTIFY_NEXT_STAGE;
}

void CCrashRpt::CmdGenerateCrashReport(IConsoleCmdArgs*)
{
	CR_EXCEPTION_INFO ei;
	memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
	ei.cb = sizeof(CR_EXCEPTION_INFO);
	ei.exctype = CR_SEH_EXCEPTION;
	ei.code = 1234;
	ei.pexcptrs = NULL;
	ei.bManual = TRUE;

	int result = crGenerateErrorReport(&ei);
	if (result != 0)
	{
		// If goes here, crGenerateErrorReport() has failed
		// Get the last error message
		char szErrorMsg[256];
		crGetLastErrorMsg(szErrorMsg, 256);
		CryLogAlways("%s", szErrorMsg);
	}
}

void CCrashRpt::FatalError()
{
	CR_EXCEPTION_INFO ei;
	memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
	ei.cb = sizeof(CR_EXCEPTION_INFO);
	ei.exctype = CR_CPP_TERMINATE_CALL;
	ei.code = 1;
	ei.pexcptrs = NULL;
	ei.bManual = TRUE;
	crGenerateErrorReport(&ei);
	_exit(1); // Immediate termination of process.
}

void CCrashRpt::ReInstallCrashRptHandler(ICVar*)
{
	UninstallHandler();
	InstallHandler();
}

SFileVersion CCrashRpt::GetSystemVersionInfo()
{
	SFileVersion productVersion;
#if CRY_PLATFORM_WINDOWS
	char moduleName[_MAX_PATH];

	char ver[1024 * 8];

	GetModuleFileName(NULL, moduleName, _MAX_PATH);  //retrieves the PATH for the current module
#ifndef _LIB
	cry_strcpy(moduleName, "CrySystem.dll"); // we want to version from the system dll
#endif //_LIB

	DWORD dwHandle(0);
	int verSize = GetFileVersionInfoSize(moduleName, &dwHandle);
	if (verSize > 0)
	{
		GetFileVersionInfo(moduleName, dwHandle, 1024 * 8, ver);
		VS_FIXEDFILEINFO* vinfo;
		UINT len(0);
		VerQueryValue(ver, "\\", (void**)&vinfo, &len);

		const uint32 verIndices[4] = { 0, 1, 2, 3 };

		productVersion.v[verIndices[0]] = vinfo->dwFileVersionLS & 0xFFFF;
		productVersion.v[verIndices[1]] = vinfo->dwFileVersionLS >> 16;
		productVersion.v[verIndices[2]] = vinfo->dwFileVersionMS & 0xFFFF;
		productVersion.v[verIndices[3]] = vinfo->dwFileVersionMS >> 16;
	}
#endif //CRY_PLATFORM_WINDOWS
	return productVersion;
}

	#endif //CRY_USE_CRASHRPT

//=============================================================================
// Class Statics
//=============================================================================

// Return single instance of class.
IDebugCallStack* IDebugCallStack::instance()
{
	static DebugCallStack sInstance;
	return &sInstance;
}

//------------------------------------------------------------------------------------------------------------------------
// Sets up the symbols for functions in the debug file.
//------------------------------------------------------------------------------------------------------------------------
DebugCallStack::DebugCallStack()
	: m_pSystem(0)
	, m_symbols(false)
	, m_bCrash(false)
	, m_szBugMessage(NULL)
	, m_previousHandler(nullptr)
{
	RemoveOldFiles();
	if (gEnv && gEnv->pThreadManager)
	{
		// As we want to avoid allocating memory during exception handling
		// spawn thread here when it is safe for the thread manager to allocate new memory when spawning the thread
		if (!gEnv->pThreadManager->SpawnThread(&g_screenShotThread, "CaptureCrashScreenShot"))
		{
			CryFatalError("Error spawning \"CaptureCrashScreenShot\" thread.");
		}
	}

	m_outputPath = "";
#if defined(DEDICATED_SERVER)
	m_outputPath = gEnv->pSystem->GetRootFolder();
#endif // defined(DEDICATED_SERVER)
}

DebugCallStack::~DebugCallStack()
{
	if (gMemAllocFile)
		fclose(gMemAllocFile);
}

/*
   BOOL CALLBACK func_PSYM_ENUMSOURCFILES_CALLBACK( PSOURCEFILE pSourceFile, PVOID UserContext )
   {
   CryLogAlways( pSourceFile->FileName );
   return TRUE;
   }

   BOOL CALLBACK func_PSYM_ENUMMODULES_CALLBACK64(
                                        PSTR ModuleName,
                                        DWORD64 BaseOfDll,
                                        PVOID UserContext
                                        )
   {
   CryLogAlways( "<SymModule> %s: %x",ModuleName,(uint32)BaseOfDll );
   return TRUE;
   }

   BOOL CALLBACK func_PSYM_ENUMERATESYMBOLS_CALLBACK(
   PSYMBOL_INFO  pSymInfo,
   ULONG         SymbolSize,
   PVOID         UserContext
   )
   {
   CryLogAlways( "<Symbol> %08X Size=%08X  :%s",(uint32)pSymInfo->Address,(uint32)pSymInfo->Size,pSymInfo->Name );
   return TRUE;
   }
 */

bool DebugCallStack::initSymbols()
{
	#ifndef WIN98
	if (m_symbols) return true;

	char fullpath[MAX_PATH_LENGTH + 1];
	char pathname[MAX_PATH_LENGTH + 1];
	char fname[MAX_PATH_LENGTH + 1];
	char directory[MAX_PATH_LENGTH + 1];
	char drive[10];

	{
		// Print dbghelp version.
		HMODULE dbgHelpDll = GetModuleHandle("dbghelp.dll");

		char ver[1024 * 8];
		GetModuleFileName(dbgHelpDll, fullpath, _MAX_PATH);
		int fv[4];

		DWORD dwHandle;
		int verSize = GetFileVersionInfoSize(fullpath, &dwHandle);
		if (verSize > 0)
		{
			unsigned int len;
			GetFileVersionInfo(fullpath, dwHandle, 1024 * 8, ver);
			VS_FIXEDFILEINFO* vinfo;
			VerQueryValue(ver, "\\", (void**)&vinfo, &len);

			fv[0] = vinfo->dwFileVersionLS & 0xFFFF;
			fv[1] = vinfo->dwFileVersionLS >> 16;
			fv[2] = vinfo->dwFileVersionMS & 0xFFFF;
			fv[3] = vinfo->dwFileVersionMS >> 16;

			//			WriteLineToLog( "dbghelp.dll version %d.%d.%d.%d",fv[3],fv[2],fv[1],fv[0] );
		}
	}

	//	SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_OMAP_FIND_NEAREST|SYMOPT_INCLUDE_32BIT_MODULES);
	//DWORD res1 = SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_OMAP_FIND_NEAREST);

	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_LOAD_ANYTHING | SYMOPT_LOAD_LINES);

	HANDLE hProcess = GetCurrentProcess();

	// Get module file name.
	GetModuleFileName(NULL, fullpath, MAX_PATH_LENGTH);

	// Convert it into search path for symbols.
	cry_strcpy(pathname, fullpath);
	_splitpath(pathname, drive, directory, fname, NULL);
	cry_sprintf(pathname, "%s%s", drive, directory);

	// Append the current directory to build a search path forSymInit
	cry_strcat(pathname, ";.;");

	int result = 0;

	m_symbols = false;

	result = SymInitialize(hProcess, pathname, TRUE);
	if (result)
	{
		//HMODULE hMod = GetModuleHandle( "imagehlp" );
		//SymGetLineFromAddrPtr = (SymGetLineFromAddrFunction)GetProcAddress( hMod,"SymGetLineFromAddr" );

		SymSetSearchPath(hProcess, pathname);
		SymRefreshModuleList(hProcess);
		SymEnumerateModules64(hProcess, EnumModules, &m_modules);

		char pdb[MAX_PATH_LENGTH + 1];
		char res_pdb[MAX_PATH_LENGTH + 1];
		cry_sprintf(pdb, "%s.pdb", fname);
		cry_sprintf(pathname, "%s%s", drive, directory);
		if (SearchTreeForFile(pathname, pdb, res_pdb))
		{
			m_symbols = true;
		}
	}
	else
	{
		result = SymInitialize(hProcess, pathname, FALSE);
		if (!result)
		{
			WriteLineToLog("<CrySystem> SymInitialize failed");
		}
	}
	#else
	return false;
	#endif

	return result != 0;
}

void DebugCallStack::doneSymbols()
{
	#ifndef WIN98
	if (m_symbols)
	{
		SymCleanup(GetCurrentProcess());
	}
	m_symbols = false;
	#endif
}

void DebugCallStack::RemoveOldFiles()
{
	string baseName;

	struct stat fileStat;
	if (stat("error.log", &fileStat)>=0 && fileStat.st_mtime)
	{
		tm* today = localtime(&fileStat.st_mtime);
		if (today)
		{
			char s[128];
			strftime(s, 128, "%d %b %y (%H %M %S)", today);
			baseName = "error_" + string(s);
		}
		else
		{
			baseName = "error";
		}
	}
	else
	{
		baseName = "error";
	}

	baseName = PathUtil::Make("LogBackups", baseName);
	string logDest = baseName + ".log";
	string jpgDest = baseName + ".jpg";
	string dmpDest = baseName + ".dmp";

	MoveFile("error.log", logDest.c_str());
	MoveFile("error.jpg", jpgDest.c_str());
	MoveFile("error.dmp", dmpDest.c_str());
}

void DebugCallStack::MoveFile(const char* szFileNameOld, const char* szFileNameNew)
{
	FILE* const pFile = fopen(szFileNameOld, "r");

	if (pFile)
	{
		fclose(pFile);

		RemoveFile(szFileNameNew);

		WriteLineToLog("Moving file \"%s\" to \"%s\"...", szFileNameOld, szFileNameNew);
		if (rename(szFileNameOld, szFileNameNew) == 0)
		{
			WriteLineToLog("File successfully moved.");
		}
		else
		{
			WriteLineToLog("Couldn't move file!");
			RemoveFile(szFileNameOld);
		}
	}
}

void DebugCallStack::RemoveFile(const char* szFileName)
{
	FILE* const pFile = fopen(szFileName, "r");

	if (pFile)
	{
		fclose(pFile);

		WriteLineToLog("Removing file \"%s\"...", szFileName);
		if (remove(szFileName) == 0)
		{
			WriteLineToLog("File successfully removed.");
		}
		else
		{
			WriteLineToLog("Couldn't remove file!");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::CollectCurrentCallStack(int maxStackEntries)
{
	if (!initSymbols())
		return;

	m_functions.clear();

	memset(&m_context, 0, sizeof(m_context));
	m_context.ContextFlags = CONTEXT_FULL;

	GetThreadContext(GetCurrentThread(), &m_context);

	const int nSkipNumFunctions = 2;
	FillStackTrace(maxStackEntries, nSkipNumFunctions);
}

//------------------------------------------------------------------------------------------------------------------------
static int callCount = 0;
int DebugCallStack::updateCallStack(EXCEPTION_POINTERS* pex)
{
	if (callCount > 0)
	{
		_exit(1); // Immediate termination of process.
	}
	callCount++;

	HANDLE process = GetCurrentProcess();

	//! Find source line at exception address.
	//m_excLine = lookupFunctionName( (void*)pex->ExceptionRecord->ExceptionAddress,true );

	//! Find Name of .DLL from Exception address.
	cry_strcpy(m_excModule, "<Unknown>");

	if (m_symbols && pex)
	{
		DWORD64 dwAddr = SymGetModuleBase64(process, (DWORD64)pex->ExceptionRecord->ExceptionAddress);
		if (dwAddr)
		{
			char szBuff[MAX_PATH_LENGTH];
			if (GetModuleFileName((HMODULE)dwAddr, szBuff, MAX_PATH_LENGTH))
			{
				cry_strcpy(m_excModule, szBuff);

				char fdir[_MAX_PATH];
				char fdrive[_MAX_PATH];
				char file[_MAX_PATH];
				char fext[_MAX_PATH];
				_splitpath(m_excModule, fdrive, fdir, file, fext);
				_makepath(fdir, NULL, NULL, file, fext);

				cry_strcpy(m_excModule, fdir);
			}
		}
	}

	// Fill stack trace info.
	if (pex)
	{
		m_context = *pex->ContextRecord;
	}
	FillStackTrace();

	return EXCEPTION_CONTINUE_EXECUTION;
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::FillStackTrace(int maxStackEntries, int skipNumFunctions, HANDLE hThread)
{
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	int count;
	STACKFRAME64 stack_frame;
	BOOL b_ret = TRUE; //Setup stack frame
	memset(&stack_frame, 0, sizeof(stack_frame));
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrReturn.Mode = AddrModeFlat;
	stack_frame.AddrBStore.Mode = AddrModeFlat;

	DWORD MachineType = IMAGE_FILE_MACHINE_I386;

	#if defined(_M_IX86)
	MachineType = IMAGE_FILE_MACHINE_I386;
	stack_frame.AddrPC.Offset = m_context.Eip;
	stack_frame.AddrStack.Offset = m_context.Esp;
	stack_frame.AddrFrame.Offset = m_context.Ebp;
	#elif defined(_M_X64)
	MachineType = IMAGE_FILE_MACHINE_AMD64;
	stack_frame.AddrPC.Offset = m_context.Rip;
	stack_frame.AddrStack.Offset = m_context.Rsp;
	stack_frame.AddrFrame.Offset = m_context.Rdi;
	#endif

	m_functions.clear();

	//WriteLineToLog( "Start StackWalk" );
	//WriteLineToLog( "eip=%p, esp=%p, ebp=%p",m_context.Eip,m_context.Esp,m_context.Ebp );

	//While there are still functions on the stack..
	for (count = 0; count < maxStackEntries && b_ret == TRUE; count++)
	{
		b_ret = StackWalk64(MachineType, GetCurrentProcess(), hThread, &stack_frame, &m_context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);

		if (count < skipNumFunctions)
			continue;

		if (m_symbols)
		{
			string funcName = LookupFunctionName((void*)stack_frame.AddrPC.Offset, true);
			if (!funcName.empty())
			{
				m_functions.push_back(funcName);
			}
			else
			{
				void* p = (void*)(uintptr_t)stack_frame.AddrPC.Offset;
				char str[80];
				cry_sprintf(str, "function=0x%p", p);
				m_functions.push_back(str);
			}
		}
		else
		{
			void* p = (void*)(uintptr_t)stack_frame.AddrPC.Offset;
			char str[80];
			cry_sprintf(str, "function=0x%p", p);
			m_functions.push_back(str);
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
string DebugCallStack::LookupFunctionName(void* address, bool fileInfo)
{
	string fileName, symName;
	int lineNumber;
	void* baseAddr;
	LookupFunctionName(address, fileInfo, symName, fileName, lineNumber, baseAddr);
	symName += "()";
	if (fileInfo)
	{
		char lineNum[1024];
		itoa(lineNumber, lineNum, 10);
		string path;

		char file[1024];
		char fname[1024];
		char fext[1024];
		_splitpath(fileName.c_str(), NULL, NULL, fname, fext);
		_makepath(file, NULL, NULL, fname, fext);
		symName += string("  [") + file + ":" + lineNum + "]";
	}
	return symName;
}

bool DebugCallStack::LookupFunctionName(void* address, bool fileInfo, string& proc, string& file, int& line, void*& baseAddr)
{
	proc = "";
	file = "";
	line = 0;
	baseAddr = address;
	#ifndef WIN98
	HANDLE process = GetCurrentProcess();
	char symbolBuf[sizeof(SYMBOL_INFO) + MAX_SYMBOL_LENGTH + 1];
	memset(symbolBuf, 0, sizeof(symbolBuf));
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;

	DWORD displacement = 0;
	DWORD64 displacement64 = 0;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
	if (SymFromAddr(process, (DWORD64)address, &displacement64, pSymbol))
	{
		proc = string(pSymbol->Name);
		baseAddr = (void*)((UINT_PTR)address - displacement64);
	}
	else
	{
		#if defined(_M_IX86)
		proc.Format("[%08X]", address);
		#elif defined(_M_X64)
		proc.Format("[%016llX]", address);
		#endif
		return false;
	}

	if (fileInfo)
	{
		// Lookup Line in source file.
		IMAGEHLP_LINE64 lineImg;
		memset(&lineImg, 0, sizeof(lineImg));
		lineImg.SizeOfStruct = sizeof(lineImg);

		if (SymGetLineFromAddr64(process, (DWORD_PTR)address, &displacement, &lineImg))
		{
			file = lineImg.FileName;
			line = lineImg.LineNumber;
		}
		return true;
	}
	#endif

	return false;
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::FatalError(const char* message)
{
#ifdef CRY_USE_CRASHRPT
	if (g_bCrashRptInstalled)
	{
		m_bIsFatalError = true;
		WriteLineToLog(message);
		CCrashRpt::FatalError();
	}
	else
		IDebugCallStack::FatalError(message);
#else
		IDebugCallStack::FatalError(message);
#endif // CRY_USE_CRASHRPT
}

void DebugCallStack::installErrorHandler(ISystem* pSystem)
{
	m_pSystem = pSystem;

	bool bInstallHandler = true;

#ifdef CRY_USE_CRASHRPT
		if (CCrashRpt::InstallHandler())
		{
			bInstallHandler = false;
		}
	#endif

	if (bInstallHandler)
	{
		m_previousHandler = SetUnhandledExceptionFilter(CryUnhandledExceptionHandler);
	}
}

void DebugCallStack::uninstallErrorHandler()
{
	#ifdef CRY_USE_CRASHRPT
		CCrashRpt::UninstallHandler();
	#endif

	if (m_previousHandler)
	{
		SetUnhandledExceptionFilter(m_previousHandler);
		m_previousHandler = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::SetUserDialogEnable(const bool bUserDialogEnable)
{
	g_bUserDialog = bUserDialogEnable;
}

//////////////////////////////////////////////////////////////////////////
namespace
{
void SuspendAnyThread(threadID nThreadId, void* pData)
{
	HANDLE hHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, nThreadId);
	SuspendThread(hHandle);
	CloseHandle(hHandle);
}

void ResumeAnyThread(threadID nThreadId, void* pData)
{
	HANDLE hHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, nThreadId);
	ResumeThread(hHandle);
	CloseHandle(hHandle);
}

void SuspendTargetThread(threadID nThreadId, void* pData)
{
	if (nThreadId == *static_cast<threadID*>(pData))
	{
		HANDLE hHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, nThreadId);
		SuspendThread(hHandle);
		CloseHandle(hHandle);
	}
}

void ResumeTargetThread(threadID nThreadId, void* pData)
{
	if (nThreadId == *static_cast<threadID*>(pData))
	{
		HANDLE hHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, nThreadId);
		ResumeThread(hHandle);
		CloseHandle(hHandle);
	}
}

struct PrintCallstackOfTargetThreadDesc
{
	DebugCallStack* pDebugCallStack;
	FILE*           pOutputFile;
};

void PrintCallstackOfTargetThread(threadID nThreadId, void* pData)
{
	PrintCallstackOfTargetThreadDesc* pDesc = (PrintCallstackOfTargetThreadDesc*)pData;
	pDesc->pDebugCallStack->PrintThreadCallstack(nThreadId, pDesc->pOutputFile);
}
}
//////////////////////////////////////////////////////////////////////////
int DebugCallStack::handleException(EXCEPTION_POINTERS* exception_pointer)
{
	if (gEnv == NULL)
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}

#ifdef USE_CRY_ASSERT
	gEnv->ignoreAllAsserts = true;
#endif

	gEnv->pLog->FlushAndClose();

	ResetFPU(exception_pointer);
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	if (g_cvars.sys_WER)
		return CryEngineExceptionFilterWER(exception_pointer);

	if (g_cvars.sys_no_crash_dialog)
	{
		DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
		SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);
	}

	m_bCrash = true;

	if (g_cvars.sys_no_crash_dialog)
	{
		DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX);
		SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX);
	}

	static bool firstTime = true;

	// Wait a reasonable amount of time for the render thread to reach a safe point
	const uint timeoutMs = 200;

	if (gEnv->pRenderer)
		gEnv->pRenderer->StopRendererAtFrameEnd(timeoutMs);

	// Ensure all threads have finished writing to log before suspending them.
	// Otherwise we run the risk of suspending a thread which is holding a WinApi lock
	// resulting in a deadlock when we attempt to log to file from this thread.
	gEnv->pLog->ThreadExclusiveLogAccess(true);

	// Suspend all threads but this one
	if (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break)
		gEnv->pThreadManager->ForEachOtherThread(SuspendAnyThread);

	// Ensure logging is enabled
	gEnv->pLog->SetVerbosity(4);
	gEnv->pLog->SetLogMode(eLogMode_AppCrash); // Log straight to file

	if (!firstTime)
	{
		WriteLineToLog("Critical Exception! Called Multiple Times!");
		// Exception called more then once.
		return EXCEPTION_EXECUTE_HANDLER;
	}

	// Print exception info:
	{
		char excCode[80];
		char excAddr[80];
		WriteLineToLog("<CRITICAL EXCEPTION>");
		cry_sprintf(excAddr, "0x%04X:0x%p", exception_pointer->ContextRecord->SegCs, exception_pointer->ExceptionRecord->ExceptionAddress);
		cry_sprintf(excCode, "0x%08X", exception_pointer->ExceptionRecord->ExceptionCode);
		WriteLineToLog("Exception: %s, at Address: %s", excCode, excAddr);

		if (CSystem* pSystem = (CSystem*)GetSystem())
		{
			if (const char* pLoadingProfilerCallstack = pSystem->GetLoadingProfilerCallstack())
				if (pLoadingProfilerCallstack[0])
					WriteLineToLog("<CrySystem> LoadingProfilerCallstack: %s", pLoadingProfilerCallstack);
		}

		{
			IMemoryManager::SProcessMemInfo memInfo;
			if (gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memInfo))
			{
				uint32 nMemUsage = (uint32)(memInfo.PagefileUsage / (1024 * 1024));
				WriteLineToLog("Virtual memory usage: %dMb", nMemUsage);
			}
			gEnv->szDebugStatus[SSystemGlobalEnvironment::MAX_DEBUG_STRING_LENGTH - 1] = '\0';
			WriteLineToLog("Debug Status: %s", gEnv->szDebugStatus);
		}

		if (gEnv->pRenderer)
		{
			ID3DDebugMessage* pMsg = 0;
			gEnv->pRenderer->EF_Query(EFQ_GetLastD3DDebugMessage, pMsg);
			if (pMsg)
			{
				const char* pStr = pMsg->GetMsg();
				WriteLineToLog("Last D3D debug message: %s", pStr ? pStr : "#unknown#");
				SAFE_RELEASE(pMsg);
			}
		}
	}

	firstTime = false;

	const int ret = SubmitBug(exception_pointer);

	if (ret != IDB_IGNORE)
		CryEngineExceptionFilterWER(exception_pointer);

	if (exception_pointer->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
	{
		// This is non continuable exception.
		_exit(1); // Immediate termination of process.
	}

	if (ret == IDB_EXIT)
	{
		_exit(1);  // Immediate termination of process.
	}
	else if (ret == IDB_IGNORE)
	{
	#if CRY_PLATFORM_32BIT
		exception_pointer->ContextRecord->FloatSave.StatusWord &= ~31;
		exception_pointer->ContextRecord->FloatSave.ControlWord |= 7;
		(*(WORD*)(exception_pointer->ContextRecord->ExtendedRegisters + 24) &= 31) |= 0x1F80;
	#else
		exception_pointer->ContextRecord->FltSave.StatusWord &= ~31;
		exception_pointer->ContextRecord->FltSave.ControlWord |= 7;
		(exception_pointer->ContextRecord->FltSave.MxCsr &= 31) |= 0x1F80;
	#endif
		firstTime = true;
		callCount = 0;

		// Resume all threads
		if (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break)
			gEnv->pThreadManager->ForEachOtherThread(ResumeAnyThread);

		// Resume render thread
		gEnv->pRenderer->ResumeRendererFromFrameEnd();

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	// Continue;
	return EXCEPTION_EXECUTE_HANDLER;
}

void DebugCallStack::ReportBug(const char* szErrorMessage)
{
	WriteLineToLog("Reporting bug: %s", szErrorMessage);

	m_szBugMessage = szErrorMessage;
	m_context = CaptureCurrentContext();
	SubmitBug(NULL);
	--callCount;
	m_szBugMessage = NULL;
}

void DebugCallStack::dumpCallStack(std::vector<string>& funcs)
{
	WriteLineToLog("=============================================================================");
	int len = (int)funcs.size();
	for (int i = 0; i < len; i++)
	{
		const char* str = funcs[i].c_str();
		WriteLineToLog("%2d) %s", len - i, str);
	}
	WriteLineToLog("=============================================================================");
}

void DebugCallStack::LogMemCallstackFile(int memSize)
{
	if (!gMemAllocFile)
		return;

	CollectCurrentCallStack(MAX_DEBUG_STACK_ENTRIES_FILE_DUMP);   // is updating m_functions

	char buffer[16];
	itoa(memSize, buffer, 10);
	CryFixedStringT<64> temp("*** Memory allocation for ");
	temp.append(buffer);
	temp.append(" bytes ");
	int frame = gEnv->pRenderer->GetFrameID(false);
	itoa(frame, buffer, 10);
	temp.append("in frame ");
	temp.append(buffer);
	temp.append("****\n");
	fwrite(temp.c_str(), temp.size(), 1, gMemAllocFile);
	int len = (int)m_functions.size();
	for (int i = 0; i < len; i++)
	{
		const char* str = m_functions[i].c_str();
		itoa(len - i, buffer, 10);
		temp = buffer;
		temp.append(" ");
		temp.append(str);
		temp.append("\n");
		fwrite(temp.c_str(), temp.size(), 1, gMemAllocFile);
	}
	temp = "=============================================================================\n";
	fwrite(temp.c_str(), temp.size(), 1, gMemAllocFile);
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::SetMemLogFile(bool open, const char* filename)
{
	if (open)
	{
		if (!gMemAllocFile)
			gMemAllocFile = fopen("memallocfile.txt", "wb");
		assert(gMemAllocFile);
	}
	else if (gMemAllocFile)
	{
		fclose(gMemAllocFile);
		gMemAllocFile = NULL;
	}
}

void ReportJiraBug()
{
	// (Kevin) - Acknowledging the cvar or user settings to suppress the dialog box messages for when crashes occur.
	//	In the future, a separate cvar should control if Jira client can submit a crash bug silently. (24/08/09)
	if (g_cvars.sys_no_crash_dialog != 0 || !g_bUserDialog)
	{
		return;
	}

	if (!CJiraClient::ReportBug())
	{
		CryMessageBox("Error running jira crash handler: bug submission failed.", "Bug submission failed", eMB_Error);
	}
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::LogExceptionInfo(EXCEPTION_POINTERS* pex)
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	static char errorString[s_iCallStackSize];
	errorString[0] = 0;

	// Time and Version.
	char versionbuf[1024];
	cry_strcpy(versionbuf, "");
	PutVersion(versionbuf);
	cry_strcat(errorString, versionbuf);
	cry_strcat(errorString, "\n");

	char excCode[MAX_WARNING_LENGTH];
	char excAddr[80];
	char desc[1024];
	char excDesc[MAX_WARNING_LENGTH];

	// make sure the mouse cursor is visible
	if (gEnv->pInput)
	{
		gEnv->pInput->ShowCursor(true);
	}

	const char* excName;
	if (m_bIsFatalError || !pex)
	{
		const char* const szMessage = m_bIsFatalError ? s_szFatalErrorCode : m_szBugMessage;
		excName = szMessage;
		cry_strcpy(excCode, szMessage);
		cry_strcpy(excAddr, "");
		cry_strcpy(desc, "");
		cry_strcpy(m_excModule, "");
		cry_strcpy(excDesc, szMessage);
	}
	else
	{
		cry_sprintf(excAddr, "0x%04X:0x%p", pex->ContextRecord->SegCs, pex->ExceptionRecord->ExceptionAddress);
		cry_sprintf(excCode, "0x%08X", pex->ExceptionRecord->ExceptionCode);
		excName = TranslateExceptionCode(pex->ExceptionRecord->ExceptionCode);
		cry_strcpy(desc, "");
		cry_sprintf(excDesc, "%s\r\n%s", excName, desc);

		if (pex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		{
			if (pex->ExceptionRecord->NumberParameters > 1)
			{
				int iswrite = pex->ExceptionRecord->ExceptionInformation[0];
				void* accessAddr = (void*)(uintptr_t)pex->ExceptionRecord->ExceptionInformation[1];
				if (iswrite)
				{
					cry_sprintf(desc, "Attempt to write data to address 0x%08p\r\nThe memory could not be \"written\"", accessAddr);
				}
				else
				{
					cry_sprintf(desc, "Attempt to read from address 0x%08p\r\nThe memory could not be \"read\"", accessAddr);
				}
			}
		}
	}

	WriteLineToLog("Exception Code: %s", excCode);
	WriteLineToLog("Exception Addr: %s", excAddr);
	WriteLineToLog("Exception Module: %s", m_excModule);
	WriteLineToLog("Exception Name  : %s", excName);
	WriteLineToLog("Exception Description: %s", desc);

	cry_strcpy(m_excDesc, excDesc);
	cry_strcpy(m_excAddr, excAddr);
	cry_strcpy(m_excCode, excCode);

	char errs[32768];
	cry_sprintf(errs, "Exception Code: %s\nException Addr: %s\nException Module: %s\nException Description: %s, %s\n",
	            excCode, excAddr, m_excModule, excName, desc);

	char tempString[256];

	IMemoryManager::SProcessMemInfo memInfo;
	if (gEnv->pSystem->GetIMemoryManager()->GetProcessMemInfo(memInfo))
	{
		char memoryString[256];
		double MB = 1024 * 1024;
		cry_sprintf(memoryString, "Memory in use: %3.1fMB\n", (double)(memInfo.PagefileUsage) / MB);
		cry_strcat(errs, memoryString);
	}
	{
		gEnv->szDebugStatus[SSystemGlobalEnvironment::MAX_DEBUG_STRING_LENGTH - 1] = '\0';
		cry_sprintf(tempString, "Debug Status: %s\n", gEnv->szDebugStatus);
		cry_strcat(errs, tempString);

		cry_sprintf(tempString, "Out of Memory: %d\n", gEnv->bIsOutOfMemory);
		cry_strcat(errs, tempString);
	}

	{
		threadID mainThread = 0;
		threadID renderThread = 0;

		if (gEnv->pRenderer)
		{
			gEnv->pRenderer->GetThreadIDs(mainThread, renderThread);
		}
		else
		{
			mainThread = gEnv->mMainThreadId;
		}

		cry_sprintf(tempString, "MainThread Id: %d\n", mainThread);
		cry_strcat(errs, tempString);

		if (renderThread)
		{
			cry_sprintf(tempString, "RenderThread Id: %d\n", renderThread);
			cry_strcat(errs, tempString);
		}

		threadID nThreadId = GetCurrentThreadId();
		const char* cThreadName = gEnv->pThreadManager->GetThreadName(nThreadId);

		cry_sprintf(tempString, "[Crashed] Thread Id: %d\n", nThreadId);
		cry_strcat(errs, tempString);

		cry_sprintf(tempString, "[Crashed] ThreadName: %s\n", cThreadName);
		cry_strcat(errs, tempString);

		cry_sprintf(tempString, "\n[Crashed] Call Stack: [\"%s\" [%ld]]\n", (strcmp("", cThreadName) != 0 ? cThreadName : "ThreadNameUnknown"), nThreadId);
		cry_strcat(errs, tempString);
	}

	// Write to log
	WriteLineToLog(errs);

	std::vector<string> funcs;
	if (gEnv->bIsOutOfMemory)
	{
		cry_strcat(errs, "1) OUT_OF_MEMORY()\n");
	}
	else
	{
		getCallStack(funcs);
		dumpCallStack(funcs);
		// Fill call stack.
		char str[s_iCallStackSize];
		cry_strcpy(str, "");
		for (size_t i = 0; i < funcs.size(); i++)
		{
			char temp[s_iCallStackSize];
			cry_sprintf(temp, " %2" PRISIZE_T ") %s", funcs.size() - i, funcs[i].c_str());
			cry_strcat(str, temp);
			cry_strcat(str, "\r\n");
			cry_strcat(errs, temp);
			cry_strcat(errs, "\n");
		}
		cry_strcpy(m_excCallstack, str);
	}

	cry_strcat(errorString, errs);

	stack_string errorlogFilename = PathUtil::Make(stack_string(m_outputPath), stack_string("error.log"));

	WriteErrorLog(errorlogFilename.c_str(), errorString);
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::MinimalExceptionReport(EXCEPTION_POINTERS* exception_pointer)
{
	if (!gEnv || !gEnv->pLog)
		return;

	int prev_sys_no_crash_dialog = g_cvars.sys_no_crash_dialog;

#ifdef USE_CRY_ASSERT
	gEnv->ignoreAllAsserts = true;
	gEnv->cryAssertLevel = ECryAssertLevel::Disabled;
#endif

	g_cvars.sys_no_crash_dialog = 1;

	CrySpinLock(&s_exception_handler_lock, 0, 1);

	gEnv->pLog->FlushAndClose();

	if (gEnv->pCryPak)
	{
		gEnv->pCryPak->DisableRuntimeFileAccess(false);
	}

	ResetFPU(exception_pointer);
	SCOPED_DISABLE_FLOAT_EXCEPTIONS();
	if (gEnv->pRenderer)
		gEnv->pRenderer->StopRendererAtFrameEnd(200);

	// Ensure all threads have finished writing to log before suspending them.
	// Otherwise we run the risk of suspending a thread which is holding a WinApi lock
	// resulting in a deadlock when we attempt to log to file from this thread.
	gEnv->pLog->ThreadExclusiveLogAccess(true);

	// Suspend all threads but this one
	if (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break)
		gEnv->pThreadManager->ForEachOtherThread(SuspendAnyThread);

	// Ensure logging is enabled
	gEnv->pLog->SetVerbosity(4);
	gEnv->pLog->SetLogMode(eLogMode_AppCrash); // Log straight to file


	if (initSymbols())
	{
		// Rise exception to call updateCallStack method.
		updateCallStack(exception_pointer);
		
		LogExceptionInfo(exception_pointer);

		doneSymbols();
	}

	if (gEnv->pRenderer)
	{
		threadID renderThread = 0, mainThread = 0;
		gEnv->pRenderer->GetThreadIDs(mainThread, renderThread);

		// Resume the screenshot and render thread
		gEnv->pThreadManager->ForEachOtherThread(ResumeTargetThread, (void*)(&g_screenShotThread.GetThreadId()));
		gEnv->pThreadManager->ForEachOtherThread(ResumeTargetThread, (void*)(&renderThread));

		gEnv->pLog->ThreadExclusiveLogAccess(false);
		CaptureScreenshot();
	}

	// If in full screen minimize render window
	{
		ICVar* pFullscreen = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("r_Fullscreen") : 0;
		if (pFullscreen && pFullscreen->GetIVal() != 0 && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
			::PostMessage((HWND)gEnv->pRenderer->GetHWND(), WM_SYSCOMMAND, SC_MINIMIZE, 0);
	}

	g_cvars.sys_no_crash_dialog = prev_sys_no_crash_dialog;
	const bool bQuitting = !gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting();
	if (g_cvars.sys_no_crash_dialog == 0 && g_bUserDialog && gEnv->IsEditor() && !bQuitting && exception_pointer)
	{
		EQuestionResult res = CryMessageBox("WARNING!\n\nThe engine / game / editor crashed and is now unstable.\r\nSaving may cause level corruption or further crashes.\r\n\r\nProceed with Save ? ", "Crash", eMB_YesCancel);
		if (res == eQR_Yes)
		{
			// Make one additional backup.
			if (BackupCurrentLevel())
			{
				CryMessageBox("Level has been successfully saved!\r\nPress Ok to terminate Editor.", "Save");
			}
			else
			{
				CryMessageBox("Error saving level.\r\nPress Ok to terminate Editor.", "Save", eMB_Error);
			}
		}
	}

	CrySpinLock(&s_exception_handler_lock, 1, 0);
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::WriteErrorLog( const char *filename,const char *errorString )
{
	FILE* f = fopen(filename, "wt");
	if (f)
	{
		fwrite(errorString, strlen(errorString), 1, f);
		if (!gEnv->bIsOutOfMemory)
		{
			if (gEnv && gEnv->pSystem || (g_cvars.sys_dump_aux_threads | g_cvars.sys_keyboard_break))
			{
				// Print info about other threads (current crashed thread call stack is already in errorString)
				PrintCallstackOfTargetThreadDesc desc = { this, f };
				gEnv->pThreadManager->ForEachOtherThread(PrintCallstackOfTargetThread, &desc);
			}
		}
		fflush(f);
		fclose(f);
	}
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::CaptureScreenshot()
{
#if !defined(DEDICATED_SERVER)
	if (!gEnv->pRenderer)
		return;

	if (!gEnv->pLog)
		return;

	gEnv->pLog->SetLogMode(eLogMode_AppCrash); // Log straight to file

	// Allow screenshot thread to write to log, too
	gEnv->pLog->ThreadExclusiveLogAccess(false);

	// Notify and wait for screenshot thread
	if (!g_screenShotThread.SignalCaptureAndWait(std::chrono::seconds(2)))
	{
		WriteLineToLog("----- FAILED TO GET SCREENSHOT -----");
	}


	// Re-enable exclusive logging for this thread
	gEnv->pLog->ThreadExclusiveLogAccess(true);

#endif // !defined(DEDICATED_SERVER)
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::PrintThreadCallstack(const threadID nThreadId, FILE* f)
{
	char errorString[s_iCallStackSize];
	char tmpLine[512];
	const char* cThreadName = gEnv->pThreadManager->GetThreadName(nThreadId);
	cry_sprintf(errorString, "\n\nCall Stack: [\"%s\" [%ld]]\n", (strcmp("", cThreadName) != 0 ? cThreadName : "ThreadNameUnknown"), nThreadId);

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, TRUE, nThreadId);

	if (hThread)
	{
		std::vector<string> funcs;

		memset(&m_context, 0, sizeof(m_context));
		m_context.ContextFlags = CONTEXT_FULL;
		GetThreadContext(hThread, &m_context);

		const int nSkipNumFunctions = 0;
		FillStackTrace(10, nSkipNumFunctions, hThread);
		getCallStack(funcs);
		for (uint32 i = 0; i < funcs.size(); i++)
		{
			cry_sprintf(tmpLine, "%2" PRISIZE_T ") %s\n ", funcs.size() - i, funcs[i].c_str());
			cry_strcat(errorString, tmpLine);
		}

		CloseHandle(hThread);
	}
	else
	{
		cry_strcat(errorString, "\tError: Unable to obtain callstack");
	}

	fprintf(f, "%s", errorString);
	WriteLineToLog(errorString);
}

void DebugCallStack::GenerateCrashReport()
{
#ifdef CRY_USE_CRASHRPT
	CCrashRpt::CmdGenerateCrashReport(0);
#endif
}

//////////////////////////////////////////////////////////////////////////
void DebugCallStack::RegisterCVars()
{
	#if defined DEDICATED_SERVER
		const int DEFAULT_DUMP_TYPE = 3;
	#else
		const int DEFAULT_DUMP_TYPE = 1;
	#endif

	#ifdef CRY_USE_CRASHRPT
	CCrashRpt::RegisterCVars();
	REGISTER_CVAR2_CB("sys_dump_type", &g_cvars.sys_dump_type, DEFAULT_DUMP_TYPE, VF_NULL,
		"Specifies type of crash dump to create - see MINIDUMP_TYPE in dbghelp.h for full list of values\n"
		"0: Do not create a minidump (not valid if using CrashRpt)\n"
		"1: Create a small minidump (stacktrace)\n"
		"2: Create a medium minidump (+ some variables)\n"
		"3: Create a full minidump (+ all memory)\n", 
		&CCrashRpt::ReInstallCrashRptHandler
	);
	#else
	REGISTER_CVAR2("sys_dump_type", &g_cvars.sys_dump_type, DEFAULT_DUMP_TYPE, VF_NULL,
		"Specifies type of crash dump to create - see MINIDUMP_TYPE in dbghelp.h for full list of values\n"
		"0: Do not create a minidump (not valid if using CrashRpt)\n"
		"1: Create a small minidump (stacktrace)\n"
		"2: Create a medium minidump (+ some variables)\n"
		"3: Create a full minidump (+ all memory)\n"
	);
	#endif


}

INT_PTR CALLBACK DebugCallStack::ExceptionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static EXCEPTION_POINTERS* pex;

	static char errorString[32768] = "";

	switch (message)
	{
	case WM_INITDIALOG:
		{
			pex = (EXCEPTION_POINTERS*)lParam;
			HWND h;

			if (pex->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
			{
				// Disable continue button for non continuable exceptions.
				//h = GetDlgItem( hwndDlg,IDB_CONTINUE );
				//if (h) EnableWindow( h,FALSE );
			}

			DebugCallStack* pDCS = static_cast<DebugCallStack*>(DebugCallStack::instance());

			h = GetDlgItem(hwndDlg, IDC_EXCEPTION_DESC);
			if (h) SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excDesc);

			h = GetDlgItem(hwndDlg, IDC_EXCEPTION_CODE);
			if (h) SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excCode);

			h = GetDlgItem(hwndDlg, IDC_EXCEPTION_MODULE);
			if (h) SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excModule);

			h = GetDlgItem(hwndDlg, IDC_EXCEPTION_ADDRESS);
			if (h) SendMessage(h, EM_REPLACESEL, FALSE, (LONG_PTR)pDCS->m_excAddr);

			// Fill call stack.
			HWND callStack = GetDlgItem(hwndDlg, IDC_CALLSTACK);
			if (callStack)
			{
				SendMessage(callStack, WM_SETTEXT, FALSE, (LPARAM)pDCS->m_excCallstack);
			}

			if (hwndException)
			{
				DestroyWindow(hwndException);
				hwndException = 0;
			}

			if (IsFloatingPointException(pex))
			{
				EnableWindow(GetDlgItem(hwndDlg, IDB_IGNORE), TRUE);
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDB_EXIT:
		case IDB_IGNORE:
			// Fall through.

			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
	}
	return FALSE;
}

bool DebugCallStack::BackupCurrentLevel()
{
	CSystem* pSystem = static_cast<CSystem*>(m_pSystem);
	if (pSystem && pSystem->GetUserCallback())
	{
		return pSystem->GetUserCallback()->OnSaveDocument();
	}

	return false;
}

int DebugCallStack::SubmitBug(EXCEPTION_POINTERS* exception_pointer)
{
	int ret = IDB_EXIT;

	assert(!hwndException);

	//hwndException = CreateDialog( gDLLHandle,MAKEINTRESOURCE(IDD_EXCEPTION),NULL,NULL );

	RemoveOldFiles();

#if defined(DEDICATED_SERVER)
	string fileName(PathUtil::Make(m_outputPath.c_str(),"error.log"));
	string backupPath = PathUtil::ToUnixPath(PathUtil::AddSlash(PathUtil::Make(m_outputPath.c_str(),"DumpBackups")));
	CryCreateDirectory(backupPath.c_str());

	struct stat fileInfo;
	string timeStamp;

	if (stat(fileName.c_str(), &fileInfo) == 0)
	{
		// Backup log
		tm* creationTime = localtime(&fileInfo.st_mtime);
		char tempBuffer[32];
		strftime(tempBuffer, CRY_ARRAY_COUNT(tempBuffer), "%d %b %Y (%H %M %S)", creationTime);
		timeStamp = tempBuffer;

		string backupFileName = backupPath + timeStamp + " error.log";
		CopyFile(fileName.c_str(), backupFileName.c_str(), true);
	}
#endif // defined(DEDICATED_SERVER)


	if (!initSymbols())
		return ret;

	// Rise exception to call updateCallStack method.
	updateCallStack(exception_pointer);

	LogExceptionInfo(exception_pointer);

	if (gEnv->pRenderer)
	{
		threadID renderThread = 0, mainThread = 0;
		gEnv->pRenderer->GetThreadIDs(mainThread, renderThread);

		// Resume the screenshot and render thread
		gEnv->pThreadManager->ForEachOtherThread(ResumeTargetThread, (void*)(&g_screenShotThread.GetThreadId()));
		gEnv->pThreadManager->ForEachOtherThread(ResumeTargetThread, (void*)(&renderThread));

		gEnv->pLog->ThreadExclusiveLogAccess(false);
		CaptureScreenshot();
	}

	// If in full screen minimize render window
	{
		ICVar* pFullscreen = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("r_Fullscreen") : 0;
		if (pFullscreen && pFullscreen->GetIVal() != 0 && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
			::PostMessage((HWND)gEnv->pRenderer->GetHWND(), WM_SYSCOMMAND, SC_MINIMIZE, 0);
	}

	if (exception_pointer)
	{
		MINIDUMP_TYPE mdumpType = GetMiniDumpType();

		if (mdumpType == (MINIDUMP_TYPE)(mdumpType & MiniDumpValidTypeFlags))
		{
			stack_string fileName = "error.dmp";
#if defined(DEDICATED_SERVER)
			if (stat(fileName.c_str(), &fileInfo) == 0)
			{
				// Backup dump (use timestamp from error.log if available)
				if (timeStamp.empty())
				{
					tm* creationTime = localtime(&fileInfo.st_mtime);
					char tempBuffer[32];
					strftime(tempBuffer, CRY_ARRAY_COUNT(tempBuffer), "%d %b %Y (%H %M %S)", creationTime);
					timeStamp = tempBuffer;
				}

				string backupFileName = backupPath + timeStamp + " error.dmp";
				CopyFile(fileName.c_str(), backupFileName.c_str(), true);
			}
#endif // defined(DEDICATED_SERVER)

			CryEngineExceptionFilterMiniDump(exception_pointer, fileName.c_str(), mdumpType);
		}
	}

	//if no crash dialog don't even submit the bug
	if (m_postBackupProcess && g_cvars.sys_no_crash_dialog == 0 && g_bUserDialog)
	{
		m_postBackupProcess();
	}
	else
	{
		ReportJiraBug();
	}

	const bool bQuitting = !gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting();

	if (g_cvars.sys_no_crash_dialog == 0 && g_bUserDialog && gEnv->IsEditor() && !bQuitting && exception_pointer)
	{
		EQuestionResult res = CryMessageBox("WARNING!\n\nThe engine / game / editor crashed and is now unstable.\r\nSaving may cause level corruption or further crashes.\r\n\r\nProceed with Save ? ", "Crash", eMB_YesCancel);
		if (res == eQR_Yes)
		{
			// Make one additional backup.
			if (BackupCurrentLevel())
			{
				CryMessageBox("Level has been successfully saved!\r\nPress Ok to terminate Editor.", "Save");
			}
			else
			{
				CryMessageBox("Error saving level.\r\nPress Ok to terminate Editor.", "Save", eMB_Error);
			}
		}
		TerminateProcess(GetCurrentProcess(), 1);
	}

	if (g_cvars.sys_no_crash_dialog != 0 || !g_bUserDialog)
	{
		_exit(1); // Immediate termination of process.
	}

	if (IsFloatingPointException(exception_pointer))
	{
		//! Print exception dialog.
		ret = PrintException(exception_pointer);
	}

	doneSymbols();

	return ret;
}

void DebugCallStack::ResetFPU(EXCEPTION_POINTERS* pex)
{
	if (IsFloatingPointException(pex))
	{
		// How to reset FPU: http://www.experts-exchange.com/Programming/System/Windows__Programming/Q_10310953.html
		_clearfp();
	#if CRY_PLATFORM_32BIT
		pex->ContextRecord->FloatSave.ControlWord |= 0x2F;
		pex->ContextRecord->FloatSave.StatusWord &= ~0x8080;
	#endif
	}
}

//////////////////////////////////////////////////////////////////////////
int __cdecl WalkStackFrames(CONTEXT& context, void** pCallstack, int maxStackEntries)
{
	int count;
	BOOL b_ret = TRUE; //Setup stack frame

	HANDLE hThread = GetCurrentThread();

	STACKFRAME64 stack_frame;

	memset(&stack_frame, 0, sizeof(stack_frame));
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrReturn.Mode = AddrModeFlat;
	stack_frame.AddrBStore.Mode = AddrModeFlat;

	DWORD MachineType = IMAGE_FILE_MACHINE_I386;

	#if defined(_M_IX86)
	MachineType = IMAGE_FILE_MACHINE_I386;
	stack_frame.AddrPC.Offset = context.Eip;
	stack_frame.AddrStack.Offset = context.Esp;
	stack_frame.AddrFrame.Offset = context.Ebp;
	#elif defined(_M_X64)
	MachineType = IMAGE_FILE_MACHINE_AMD64;
	stack_frame.AddrPC.Offset = context.Rip;
	stack_frame.AddrStack.Offset = context.Rsp;
	stack_frame.AddrFrame.Offset = context.Rdi;
	#endif

	//While there are still functions on the stack..
	for (count = 0; count < maxStackEntries && b_ret == TRUE; count++)
	{
		b_ret = StackWalk64(MachineType, GetCurrentProcess(), hThread, &stack_frame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);
		pCallstack[count] = (void*)(stack_frame.AddrPC.Offset);
	}
	return count;
}

//////////////////////////////////////////////////////////////////////////
int DebugCallStack::CollectCallStackFrames(void** pCallstack, int maxStackEntries)
{
	if (!m_symbols)
	{
		if (!initSymbols())
			return 0;
	}

	CONTEXT context = CaptureCurrentContext();

	int count = WalkStackFrames(context, pCallstack, maxStackEntries);
	return count;
}

int DebugCallStack::CollectCallStack(HANDLE thread, void** pCallstack, int maxStackEntries)
{
	if (!m_symbols)
	{
		if (!initSymbols())
			return 0;
	}

	CONTEXT context;
	memset(&context, 0, sizeof(context));
	#if defined(_M_IX86)
	context.ContextFlags = CONTEXT_i386 | CONTEXT_FULL;
	#elif defined(_M_X64)
	context.ContextFlags = CONTEXT_AMD64 | CONTEXT_FULL;
	#endif
	int prev_priority = GetThreadPriority(thread);
	SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
	BOOL result = GetThreadContext(thread, &context);
	::SetThreadPriority(thread, prev_priority);
	return WalkStackFrames(context, pCallstack, maxStackEntries);
}

string DebugCallStack::GetModuleNameForAddr(void* addr)
{
	if (m_modules.empty())
		return "[unknown]";

	if (addr < m_modules.begin()->first)
		return "[unknown]";

	TModules::const_iterator it = m_modules.begin();
	TModules::const_iterator end = m_modules.end();
	for (; ++it != end; )
	{
		if (addr < it->first)
			return (--it)->second;
	}

	//if address is higher than the last module, we simply assume it is in the last module.
	return m_modules.rbegin()->second;
}

bool DebugCallStack::GetProcNameForAddr(void* addr, string& procName, void*& baseAddr, string& filename, int& line)
{
	return LookupFunctionName(addr, true, procName, filename, line, baseAddr);
}

string DebugCallStack::GetCurrentFilename()
{
	char fullpath[MAX_PATH_LENGTH + 1];
	GetModuleFileName(NULL, fullpath, MAX_PATH_LENGTH);
	return fullpath;
}

static bool IsFloatingPointException(EXCEPTION_POINTERS* pex)
{
	if (!pex)
	{
		return false;
	}

	DWORD exceptionCode = pex->ExceptionRecord->ExceptionCode;
	switch (exceptionCode)
	{
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
	case STATUS_FLOAT_MULTIPLE_FAULTS:
	case STATUS_FLOAT_MULTIPLE_TRAPS:
		return true;

	default:
		return false;
	}
}

int DebugCallStack::PrintException(EXCEPTION_POINTERS* exception_pointer)
{
	return DialogBoxParam(gDLLHandle, MAKEINTRESOURCE(IDD_CRITICAL_ERROR), NULL, DebugCallStack::ExceptionDialogProc, (LPARAM)exception_pointer);
}

	#pragma warning(pop)

#else
void MarkThisThreadForDebugging(const char*) {}
void UnmarkThisThreadFromDebugging()         {}
void UpdateFPExceptionsMaskForThreads()      {}
#endif // CRY_PLATFORM_WINDOWS
