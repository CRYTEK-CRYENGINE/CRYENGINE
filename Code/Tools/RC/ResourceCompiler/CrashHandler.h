// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   CrashHandler.h
//  Version:     v1.00
//  Created:     3/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Based on DebugCallStack code.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CRASHHANDLER_H_
#define _CRASHHANDLER_H_

#if _MSC_VER > 1000
#pragma once
#endif

#if CRY_PLATFORM_WINDOWS


//!============================================================================
//!
//! CrashHandler class, captures call stack information from symbol files,
//! writes callstack & minidump files.
//!
//!============================================================================
class CrashHandler
{
private:
	enum ELogDestination
	{
		eDefaultLog,
		eCrashLog,
		eNull
	};

public:
	CrashHandler();
	~CrashHandler();

	//! Sets files used for logging and crash dumping.
	void SetFiles(const char* logFilename0, const char* logFilename1, const char* dumpFilename);

	//! Dumps Current Call Stack to log.
	void LogCallStack();

	//! Exception handler.
	int HandleException(EXCEPTION_POINTERS* pex);

private:
	void LogLine(ELogDestination logDestination, const char* format, ...) const;

	bool InitSymbols(ELogDestination logDestination);
	void DoneSymbols();

	void LogCallStack(ELogDestination logDestination, const std::vector<string> &funcs);
	int  UpdateCallStack(EXCEPTION_POINTERS* pex);
	NO_INLINE void FillStackTrace(int maxStackEntries = MAX_DEBUG_STACK_ENTRIES, int skipNumFunctions = 0, HANDLE hThread = GetCurrentThread());
	void LogExceptionInfo(EXCEPTION_POINTERS* pex);

	void WriteMinidump(EXCEPTION_POINTERS* pex);

private:
	//! Limits the maximal number of functions in call stack.
	static const int MAX_DEBUG_STACK_ENTRIES = 30;

	std::vector<string> m_functions;

	char m_excModule[MAX_PATH];

	bool m_bSymbols;

	HANDLE m_hThread;
	CONTEXT m_context;

	void* m_prevExceptionHandler;
};

#endif // CRY_PLATFORM_WINDOWS

#endif // _RCCRASHHANDLER_H_