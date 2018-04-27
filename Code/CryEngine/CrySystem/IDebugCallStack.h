// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   Description:
   A multiplatform base class for handling errors and collecting call stacks

   -------------------------------------------------------------------------
   History:
   - 12:10:2009	: Created by Alex McCarthy
*************************************************************************/

#ifndef __I_DEBUG_CALLSTACK_H__
#define __I_DEBUG_CALLSTACK_H__

#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_ORBIS
struct EXCEPTION_POINTERS;
#endif
//! Limits the maximal number of functions in call stack.
enum {MAX_DEBUG_STACK_ENTRIES = 80};

class IDebugCallStack
{
public:
	// Returns single instance of DebugStack
	static IDebugCallStack* instance();

	virtual int             handleException(EXCEPTION_POINTERS* exception_pointer)                 { return 0; }
	virtual void            CollectCurrentCallStack(int maxStackEntries = MAX_DEBUG_STACK_ENTRIES) {}
	// Collects the low level callstack frames.
	// Returns number of collected stack frames.
	virtual int CollectCallStackFrames(void** pCallstack, int maxStackEntries) { return 0; }

	// collects low level callstack for given thread handle
	virtual int CollectCallStack(HANDLE thread, void** pCallstack, int maxStackEntries) { return 0; }

	// returns the module name of a given address
	virtual string GetModuleNameForAddr(void* addr) { return "[unknown]"; }

	// returns the function name of a given address together with source file and line number (if available) of a given address
	virtual bool GetProcNameForAddr(void* addr, string& procName, void*& baseAddr, string& filename, int& line)
	{
		filename = "[unknown]";
		line = 0;
		baseAddr = addr;
#if CRY_PLATFORM_64BIT
		procName.Format("[%016llX]", (uint64) addr);
#else
		procName.Format("[%08X]", (uint32) addr);
#endif
		return false;
	}

	// returns current filename
	virtual string GetCurrentFilename() { return "[unknown]"; }

	// initialize symbols
	virtual void InitSymbols() {}
	virtual void DoneSymbols() {}

	//! Dumps Current Call Stack to log.
	virtual void LogCallstack();
	//triggers a fatal error, so the DebugCallstack can create the error.log and terminate the application
	virtual void FatalError(const char*);

	//Reports a bug and continues execution
	virtual void ReportBug(const char*) {}

	virtual void FileCreationCallback(void (* postBackupProcess)());

	//! Get current call stack information.
	void getCallStack(std::vector<string>& functions) { functions = m_functions; }

protected:
	IDebugCallStack();
	virtual ~IDebugCallStack() {}

	static const char* TranslateExceptionCode(DWORD dwExcept);
	static void        PutVersion(char* str);

	static void        WriteLineToLog(const char* format, ...);

	static void        Screenshot(const char* szFileName);

	bool                     m_bIsFatalError;
	static const char* const s_szFatalErrorCode;

	std::vector<string>      m_functions;
	void                     (* m_postBackupProcess)();
};

#endif // __I_DEBUG_CALLSTACK_H__
