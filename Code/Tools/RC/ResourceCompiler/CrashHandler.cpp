// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CrashHandler.cpp
//  Version:     v1.00
//  Created:     1/10/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Based on debugcallstack.cpp
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CrashHandler.h"

#if CRY_PLATFORM_WINDOWS

#include <CrySystem/IConsole.h>
#include "IRCLog.h"
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#pragma comment(lib, "version.lib")

#include <CryCore/Platform/CryWindows.h>  // needed for DbgHelp.h

#include <DbgHelp.h>
#pragma comment(lib, "dbghelp")
#pragma warning(disable: 4244)

#define MAX_SYMBOL_LENGTH 512
#define MAX_LOGLINE_LENGTH 4096

#define DBGHELP_DLL_NAME "dbghelp.dll"

#pragma region // FixedString ----------------------------------------------

template <uint BUFFER_SIZE>
class FixedString
{
public:
	FixedString();
	~FixedString();

	bool appendString(const char* str);
	bool appendVA(const char* format, va_list parg);
	bool append(const char* format, ...);

	void clear();
	bool empty() const;
	const char* c_str() const;

private:
	char m_buffer[BUFFER_SIZE];
	uint m_size;       // including trailing 0
	bool m_bOverflow;
};


template <uint BUFFER_SIZE>
FixedString<BUFFER_SIZE>::FixedString()
	: m_size(1)
	, m_bOverflow(false)
{
	m_buffer[0] = 0;
}


template <uint BUFFER_SIZE>
FixedString<BUFFER_SIZE>::~FixedString()
{
}


template <uint BUFFER_SIZE>
bool FixedString<BUFFER_SIZE>::appendString(const char* const str)
{
	if (m_bOverflow)
	{
		return false;
	}

	if (!str || !str[0])
	{
		return true;
	}

	m_bOverflow = !cry_strcpy(m_buffer + m_size - 1, BUFFER_SIZE - m_size + 1, str);

	return !m_bOverflow;
}


template <uint BUFFER_SIZE>
bool FixedString<BUFFER_SIZE>::appendVA(const char* const format, va_list args)
{
	if (m_bOverflow)
	{
		return false;
	}

	if (!format || !format[0])
	{
		return true;
	}

	m_bOverflow = !cry_vsprintf(m_buffer + m_size - 1, BUFFER_SIZE - m_size + 1, format, args);

	return !m_bOverflow;
}


template <uint BUFFER_SIZE>
bool FixedString<BUFFER_SIZE>::append(const char* const format, ...)
{
	va_list args;
	va_start(args,format);
	appendVA(format, args);
	va_end(args);

	return !m_bOverflow;
}


template <uint BUFFER_SIZE>
void FixedString<BUFFER_SIZE>::clear()
{
	m_size = 1;
	m_buffer[0] = 0;
}


template <uint BUFFER_SIZE>
bool FixedString<BUFFER_SIZE>::empty() const
{
	return m_size <= 1;
}


template <uint BUFFER_SIZE>
const char* FixedString<BUFFER_SIZE>::c_str() const
{
	return &m_buffer[0];
}

#pragma endregion


static CrashHandler* s_pInstance = 0;
static FixedString<MAX_PATH> s_logFilename0;
static FixedString<MAX_PATH> s_logFilename1;
static FixedString<MAX_PATH> s_dumpFilename;


//=============================================================================
LONG __stdcall UnhandledExceptionHandler(EXCEPTION_POINTERS* pex)
{
	if (s_pInstance)
	{
		return s_pInstance->HandleException(pex);
	}
	else
	{
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

void CrashHandler::LogLine(ELogDestination logDestination, const char* format, ...) const
{
	if (logDestination == eNull)
	{
		return;
	}

	if (format == 0)
	{
		return;
	}

	FixedString<MAX_LOGLINE_LENGTH> str;

	va_list parg;
	va_start(parg, format);
	str.appendVA(format, parg);
	va_end(parg);

	if (logDestination == eDefaultLog)
	{
		RCLogError("%s", str.c_str());
	}
	else
	{
		if (!s_logFilename0.empty())
		{
			FILE* f = 0;
			fopen_s(&f, s_logFilename0.c_str(), "a+t");
			if (f)
			{
				fputs(str.c_str(), f);
				fputs("\n", f);
				fflush(f);
				fclose(f);
			}
		}

		if (!s_logFilename1.empty())
		{
			FILE* f = 0;
			fopen_s(&f, s_logFilename1.c_str(), "a+t");
			if (f)
			{
				fputs(str.c_str(), f);
				fputs("\n", f);
				fflush(f);
				fclose(f);
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
// Sets up the symbols for functions in the debug file.
//------------------------------------------------------------------------------------------------------------------------
CrashHandler::CrashHandler()
{
	m_excModule[0] = 0;

	m_bSymbols = false;
	m_hThread = 0;

	s_logFilename0.clear();
	s_logFilename1.clear();
	s_dumpFilename.clear();

	s_pInstance = this;
	m_prevExceptionHandler = (void*)SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}

CrashHandler::~CrashHandler()
{
}

void CrashHandler::SetFiles(const char* const a_logFilename0, const char* const a_logFilename1, const char* const a_dumpFilename)
{
	s_logFilename0.clear();
	s_logFilename1.clear();
	s_dumpFilename.clear();

	if (a_logFilename0 && a_logFilename0[0])
	{
		s_logFilename0.appendString(a_logFilename0);
	}

	if (a_logFilename1 && a_logFilename1[0])
	{
		s_logFilename1.appendString(a_logFilename1);
	}

	if (a_dumpFilename && a_dumpFilename[0])
	{
		s_dumpFilename.appendString(a_dumpFilename);
	}
}

bool CrashHandler::InitSymbols(ELogDestination logDestination)
{
#ifdef WIN98
	return false;
#else
	if (m_bSymbols)
	{	
		return true;
	}
	
	{
		const HMODULE dbgHelpDll = GetModuleHandle(DBGHELP_DLL_NAME);

		char fullpath[MAX_PATH];
		if (!GetModuleFileName(dbgHelpDll, fullpath, sizeof(fullpath)))
		{
			LogLine(logDestination, DBGHELP_DLL_NAME " - unable to obtain module name");
		}
		else
		{
			DWORD dwHandle;
			const int verSize = GetFileVersionInfoSize(fullpath, &dwHandle);
			if (verSize > 0)
			{
				char ver[1024*8];
				GetFileVersionInfo(fullpath, dwHandle, sizeof(ver), ver);
				unsigned int len;
				VS_FIXEDFILEINFO *vinfo;
				VerQueryValue(ver, "\\", (void**)&vinfo, &len);

				int fv[4];
				
				fv[0] = vinfo->dwFileVersionLS & 0xFFFF;
				fv[1] = vinfo->dwFileVersionLS >> 16;
				fv[2] = vinfo->dwFileVersionMS & 0xFFFF;
				fv[3] = vinfo->dwFileVersionMS >> 16;

				LogLine(logDestination, DBGHELP_DLL_NAME " version %d.%d.%d.%d", fv[3], fv[2], fv[1], fv[0]);
			}
			else
			{
				LogLine(logDestination, DBGHELP_DLL_NAME " version is unknown");
			}
		}
	}

	SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_INCLUDE_32BIT_MODULES|SYMOPT_LOAD_ANYTHING|SYMOPT_LOAD_LINES);
		
	// Get module file name.
	char fullpath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, fullpath, sizeof(fullpath));
	fullpath[sizeof(fullpath) - 1] = 0;

	// Convert it into search path for symbols.
	FixedString<MAX_PATH*2> pathname;
	char directory[MAX_PATH];
	char fname[MAX_PATH];
	char drive[MAX_PATH];
	_splitpath(fullpath, drive, directory, fname, NULL);
	pathname.append("%s%s", drive, directory);		
	pathname.appendString(";.;");  // append the current directory to build a search path for SymInitialize

	const HANDLE hProcess = GetCurrentProcess();

	int result = SymInitialize(hProcess, pathname.c_str(), TRUE);
	if (result) 
	{
		pathname.clear();
		pathname.append("%s%s", drive, directory);		
		FixedString<MAX_PATH*2> pdb;
		pdb.append("%s.pdb", fname);
		char res_pdb[MAX_PATH];
		if (SearchTreeForFile(pathname.c_str(), pdb.c_str(), res_pdb)) 
		{
			LogLine(logDestination, "SearchTreeForFile: success.");
		}
		else
		{
			LogLine(logDestination, "SearchTreeForFile: failed");
		}
	} 
	else 
	{
		result = SymInitialize(hProcess, pathname.c_str() ,FALSE);
		if (!result)
		{
			LogLine(logDestination, "SymInitialize: failed");
		}
		else
		{
			LogLine(logDestination, "SymInitialize: success");
		}
	}

	m_bSymbols = (result != 0);

	return result != 0;
#endif
}

void CrashHandler::DoneSymbols()
{
#ifndef WIN98
	if (m_bSymbols) 
	{
		SymCleanup(GetCurrentProcess());
		m_bSymbols = false;
	}
#endif
}

//------------------------------------------------------------------------------------------------------------------------
int CrashHandler::UpdateCallStack(EXCEPTION_POINTERS* const pex)
{
	static int callCount = 0;
	if (++callCount > 1)
	{
		// uninstall our exception handler.
		SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)m_prevExceptionHandler);

		// Immediate termination of process.
		abort();
	}

	//! Find Name of .DLL or .EXE from Exception address.
	SettingsManagerHelpers::Utils::strcpy_with_clamp(m_excModule, sizeof(m_excModule), "<Unknown>");

	if (m_bSymbols)
	{
		const HANDLE process = GetCurrentProcess();
		const DWORD64 dwAddr = SymGetModuleBase64(process, (DWORD64)pex->ExceptionRecord->ExceptionAddress);
		if (dwAddr) 
		{
			char fullpath[MAX_PATH];
			if (GetModuleFileName((HMODULE)dwAddr, fullpath, sizeof(fullpath))) 
			{
				char fdrive[MAX_PATH];
				char fdir[MAX_PATH];
				char fname[MAX_PATH];
				char fext[MAX_PATH];
				_splitpath(fullpath, fdrive, fdir, fname, fext);
				_makepath(fullpath, NULL, NULL, fname, fext);

				SettingsManagerHelpers::Utils::strcpy_with_clamp(m_excModule, sizeof(m_excModule), fullpath);
			}
		}
	}

	// Fill stack trace info.
	m_context = *pex->ContextRecord;
	FillStackTrace();

	return EXCEPTION_CONTINUE_EXECUTION;
}

//------------------------------------------------------------------------------------------------------------------------
static void LookupFunctionName(void* pointer, bool fileInfo, FixedString<MAX_SYMBOL_LENGTH+MAX_PATH+50>& res)
{
	res.clear();

#ifndef WIN98
	const HANDLE process = GetCurrentProcess();

	char symbolBuf[sizeof(SYMBOL_INFO)+MAX_SYMBOL_LENGTH+1];
	memset(symbolBuf, 0, sizeof(symbolBuf));
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;

	DWORD displacement = 0;
	DWORD64 displacement64 = 0;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
	if (SymFromAddr(process,(DWORD64)pointer,&displacement64,pSymbol))
	{
		res.append("%s()", pSymbol->Name);
	}

	if (fileInfo)
	{
		// Lookup Line in source file.
		IMAGEHLP_LINE64 lineImg;
		memset(&lineImg,0,sizeof(lineImg));
		lineImg.SizeOfStruct = sizeof(lineImg);

		if (SymGetLineFromAddr64(process,(DWORD_PTR)pointer, &displacement, &lineImg))
		{
			char file[MAX_PATH];
			char fname[MAX_PATH];
			char fext[MAX_PATH];
			_splitpath(lineImg.FileName,NULL,NULL,fname,fext);
			_makepath(file,NULL,NULL,fname,fext);
			res.append("  [%s:%i]", file, lineImg.LineNumber);
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
void CrashHandler::FillStackTrace(int maxStackEntries, int skipNumFunctions, HANDLE hThread)
{
	STACKFRAME64 stack_frame;
	memset(&stack_frame, 0, sizeof(stack_frame));
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrReturn.Mode = AddrModeFlat;
	stack_frame.AddrBStore.Mode = AddrModeFlat;

	DWORD MachineType = IMAGE_FILE_MACHINE_I386;

#if defined(_M_IX86)
	MachineType                   = IMAGE_FILE_MACHINE_I386;
	stack_frame.AddrPC.Offset     = m_context.Eip;
	stack_frame.AddrStack.Offset  = m_context.Esp;
	stack_frame.AddrFrame.Offset  = m_context.Ebp;
#elif defined(_M_X64)
	MachineType                   = IMAGE_FILE_MACHINE_AMD64;
	stack_frame.AddrPC.Offset     = m_context.Rip;
	stack_frame.AddrStack.Offset  = m_context.Rsp;
	stack_frame.AddrFrame.Offset  = m_context.Rdi;
#endif

	m_functions.clear();

	// While there are still functions on the stack... 

	const HANDLE hProcess = GetCurrentProcess();
	BOOL bRet = TRUE;

	for (int count = 0; count < maxStackEntries && bRet == TRUE; ++count)
	{
		bRet = StackWalk64(MachineType, hProcess, hThread, &stack_frame, &m_context,NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);

		if (count < skipNumFunctions)
		{
			continue;
		}

		FixedString<MAX_SYMBOL_LENGTH+MAX_PATH+50> functionName;
		if (m_bSymbols)
		{
			LookupFunctionName((void*)stack_frame.AddrPC.Offset, true, functionName);
		} 
		if (functionName.empty())
		{
			const DWORD64 p = (DWORD64)stack_frame.AddrPC.Offset;
			functionName.append("function=0x%p",p);
		}
		m_functions.push_back(string(functionName.c_str()));
	}
}

//////////////////////////////////////////////////////////////////////////
int	CrashHandler::HandleException(EXCEPTION_POINTERS* const pex)
{
	// uninstall our exception handler.
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)m_prevExceptionHandler);

	static bool firstTime = true;
	if (!firstTime)
	{
		LogLine(eCrashLog, "Critical Exception! Called Multiple Times!");
		return EXCEPTION_EXECUTE_HANDLER;
	}
	firstTime = false;

	// Print exception info
	{
		LogLine(eCrashLog, "");
		LogLine(eCrashLog, "<CRITICAL EXCEPTION>");
		LogLine(eCrashLog, 
			"Exception: 0x%08X, at Address: 0x%04X:0x%p",
			pex->ExceptionRecord->ExceptionCode,
			pex->ContextRecord->SegCs,
			pex->ExceptionRecord->ExceptionAddress);
	}

	LogLine(eCrashLog, "calling InitSymbols...");

	if (InitSymbols(eCrashLog))
	{
		LogLine(eCrashLog, "InitSymbols: success");
		LogLine(eCrashLog, "symbols: %s", (m_bSymbols ? "loaded" : "failed to load"));
		LogLine(eCrashLog, "");

		UpdateCallStack(pex);

		LogExceptionInfo(pex);

		DoneSymbols();

		fprintf(stderr, "Quitting because of a crash.\r\n");

		exit(eRcExitCode_Crash);
	}
	else
	{
		LogLine(eCrashLog, "InitSymbols: failed");
		fprintf(stderr, "Quitting because of a crash. Note: failed to print call stack.\r\n");

		exit(eRcExitCode_Crash);
	}

	return 0;	// never executed due to exit() calls above. just to keep compiler happy
}


//////////////////////////////////////////////////////////////////////////
void CrashHandler::LogCallStack(ELogDestination logDestination, const std::vector<string> &funcs)
{
	LogLine(logDestination, "=============================================================================");
	const uint len = funcs.size();
	for (uint i = 0; i < funcs.size(); ++i) 
	{
		const char* str = funcs[i].c_str();
		LogLine(logDestination, "%2d) %s", len-i, str);
	}
	LogLine(logDestination, "=============================================================================");
}

//////////////////////////////////////////////////////////////////////////
void CrashHandler::LogCallStack()
{
	if (!InitSymbols(eDefaultLog))
	{
		return;
	}

	m_functions.clear();

	memset(&m_context, 0, sizeof(m_context));
	m_context.ContextFlags = CONTEXT_FULL;

	__try
	{
		RaiseException(0xF001, 0, 0, 0);
	}
	__except(m_context = *(GetExceptionInformation())->ContextRecord, EXCEPTION_EXECUTE_HANDLER)
	{
		const int nSkipNumFunctions = 3;
		FillStackTrace(MAX_DEBUG_STACK_ENTRIES, nSkipNumFunctions);
	}

	LogCallStack(eDefaultLog, m_functions);
}

//////////////////////////////////////////////////////////////////////////
static const char* TranslateExceptionCode(DWORD dwExcept)
{
	switch (dwExcept)
	{
	case EXCEPTION_ACCESS_VIOLATION :        return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT";
	case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_FLT_DENORMAL_OPERAND :    return "EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK: 	     return "EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW:	         return "EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_PRIV_INSTRUCTION:	     return "EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR:	         return "EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_ILLEGAL_INSTRUCTION:	     return "EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_GUARD_PAGE:               return "EXCEPTION_GUARD_PAGE";
	case EXCEPTION_INVALID_HANDLE:           return "EXCEPTION_INVALID_HANDLE";

	case STATUS_FLOAT_MULTIPLE_FAULTS:	     return "STATUS_FLOAT_MULTIPLE_FAULTS";
	case STATUS_FLOAT_MULTIPLE_TRAPS:	     return "STATUS_FLOAT_MULTIPLE_TRAPS";
		
	default: return "UnknownExceptionCode";
	}
}

void PutVersion(FixedString<1024>& str);

//////////////////////////////////////////////////////////////////////////
void CrashHandler::LogExceptionInfo(EXCEPTION_POINTERS* const pex)
{
	{
		FixedString<1024> versionString;
		PutVersion(versionString);
		LogLine(eCrashLog, "%s", versionString.c_str());
	}

	FixedString<256> descString;

	if (pex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) 
	{
		if (pex->ExceptionRecord->NumberParameters > 1) 
		{
			const int iswrite = pex->ExceptionRecord->ExceptionInformation[0];
			const DWORD64 accessAddr = pex->ExceptionRecord->ExceptionInformation[1];
			if (iswrite) 
			{
				descString.append("Attempt to write data to address 0x%08p\r\nThe memory could not be \"written\"", accessAddr);
			} 
			else 
			{
				descString.append("Attempt to read from address 0x%08p\r\nThe memory could not be \"read\"", accessAddr);
			}
		}
	}

	LogLine(eCrashLog, "Exception Code: 0x%08X", pex->ExceptionRecord->ExceptionCode);
	LogLine(eCrashLog, "Exception Addr: 0x%04X:0x%p", pex->ContextRecord->SegCs, pex->ExceptionRecord->ExceptionAddress);
	LogLine(eCrashLog, "Exception Module: %s", m_excModule);
	LogLine(eCrashLog, "Exception Name:   %s", TranslateExceptionCode(pex->ExceptionRecord->ExceptionCode));
	LogLine(eCrashLog, "Exception Description: %s", descString.c_str());
	LogCallStack(eCrashLog, m_functions);

	if (!s_dumpFilename.empty())
	{
		WriteMinidump(pex);
	}
}


static void PutVersion(FixedString<1024>& str)
{
	int fv[4];
	int pv[4];
	
	{
		char exe[MAX_PATH];
		GetModuleFileName(NULL, exe, sizeof(exe));

		DWORD dwHandle;
		const int verSize = GetFileVersionInfoSize(exe, &dwHandle);
		if (verSize > 0)
		{
			char ver[1024*8];
			GetFileVersionInfo(exe, dwHandle, sizeof(ver), ver);
			unsigned int len;
			VS_FIXEDFILEINFO* vinfo;
			VerQueryValue(ver, "\\", (void**)&vinfo, &len);
		
			fv[0] = vinfo->dwFileVersionLS & 0xFFFF;
			fv[1] = vinfo->dwFileVersionLS >> 16;
			fv[2] = vinfo->dwFileVersionMS & 0xFFFF;
			fv[3] = vinfo->dwFileVersionMS >> 16;
		
			pv[0] = vinfo->dwProductVersionLS & 0xFFFF;
			pv[1] = vinfo->dwProductVersionLS >> 16;
			pv[2] = vinfo->dwProductVersionMS & 0xFFFF;
			pv[3] = vinfo->dwProductVersionMS >> 16;
		}
	}

	char timeBuffer[256];
	{
		//! Get time.
		time_t ltime;
		time(&ltime);
		tm *today = localtime(&ltime);

		//! Use strftime to build a customized time string.
		strftime(timeBuffer, sizeof(timeBuffer), "Logged at %#c\n", today);
	}

	str.appendString(timeBuffer);
	str.append("FileVersion: %d.%d.%d.%d\n", fv[3], fv[2], fv[1], fv[0]);
	str.append("ProductVersion: %d.%d.%d.%d\n", pv[3], pv[2], pv[1], pv[0]);
}


void CrashHandler::WriteMinidump(EXCEPTION_POINTERS* const pExceptionInfo)
{
	// firstly see if DBGHELP_DLL_NAME is around and has the function we need
	// look nearby the EXE first, as the one in System32 might be old 
	// (e.g. Windows 2000)
	HMODULE hDll = NULL;
	char szDbgHelpPath[MAX_PATH*2];

	if (GetModuleFileName(NULL, szDbgHelpPath, MAX_PATH))
	{
		char *pSlash = strrchr(szDbgHelpPath, '\\');
		if (pSlash)
		{
			strcpy(pSlash+1, DBGHELP_DLL_NAME);
			hDll = ::LoadLibrary(szDbgHelpPath);
		}
	}

	if (hDll==NULL)
	{
		// load any version we can
		hDll = ::LoadLibrary(DBGHELP_DLL_NAME);
	}

	LPCTSTR szResult = NULL;
	FixedString<MAX_PATH + 200> strResult;

	if (hDll)
	{
		// based on DbgHelp.h
		typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
			CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
			CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
			CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDll, "MiniDumpWriteDump");
		if (pDump)
		{
			{
				// create the file
				HANDLE hFile = ::CreateFile(s_dumpFilename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL, NULL);

				if (hFile != INVALID_HANDLE_VALUE)
				{
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

					ExInfo.ThreadId = ::GetCurrentThreadId();
					ExInfo.ExceptionPointers = pExceptionInfo;
					ExInfo.ClientPointers = NULL;

					// write the dump
					MINIDUMP_TYPE const mdumpValue = (MINIDUMP_TYPE)
						( MiniDumpNormal                            // include call stack, thread info, etc
						| MiniDumpWithIndirectlyReferencedMemory    // try to find pointers on the stack and dump memory near where they're pointing
						| MiniDumpWithDataSegs );                   // dump global variables (like gEnv)
					BOOL const bOK = pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdumpValue , &ExInfo, NULL, NULL);
					if (bOK)
					{
						strResult.append("Saved crash dump file to '%s'", s_dumpFilename.c_str());
					}
					else
					{
						strResult.append("Failed to save crash dump file to '%s' (error %d)", s_dumpFilename.c_str(), GetLastError());
					}
					::CloseHandle(hFile);
				}
				else
				{
					strResult.append("Failed to create crash dump file '%s' (error %d)", s_dumpFilename.c_str(), GetLastError());
				}
			}
		}
		else
		{
			strResult.append("%s", "Failed to save crash dump file because " DBGHELP_DLL_NAME " is too old");
		}
	}
	else
	{
		strResult.append("%s", "Failed to save crash dump file because " DBGHELP_DLL_NAME " is not found");
	}

	if (!strResult.empty())
	{
		fprintf(stderr, "%s\r\n", strResult.c_str());
	}
}


#endif // CRY_PLATFORM_WINDOWS
