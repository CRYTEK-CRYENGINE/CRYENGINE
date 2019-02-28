// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DebugCallStack.h
//  Version:     v1.00
//  Created:     3/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DebugCallStack_h__
#define __DebugCallStack_h__

#include "IDebugCallStack.h"

#if CRY_PLATFORM_WINDOWS

//! Limits the maximal number of functions in call stack.
const int MAX_DEBUG_STACK_ENTRIES_FILE_DUMP = 12;

struct ISystem;

//!============================================================================
//!
//! DebugCallStack class, capture call stack information from symbol files.
//!
//!============================================================================
class DebugCallStack : public IDebugCallStack
{
	friend class CCaptureCrashScreenShot;
public:
	DebugCallStack();
	virtual ~DebugCallStack();

	ISystem*       GetSystem() { return m_pSystem; };
	//! Dumps Current Call Stack to log.
	void           LogMemCallstackFile(int memSize);
	void           SetMemLogFile(bool open, const char* filename);

	virtual void   CollectCurrentCallStack(int maxStackEntries = MAX_DEBUG_STACK_ENTRIES);
	virtual int    CollectCallStackFrames(void** pCallstack, int maxStackEntries);
	virtual int    CollectCallStack(HANDLE thread, void** pCallstack, int maxStackEntries);
	virtual string GetModuleNameForAddr(void* addr);
	virtual bool   GetProcNameForAddr(void* addr, string& procName, void*& baseAddr, string& filename, int& line);
	virtual string GetCurrentFilename();
	virtual void   InitSymbols() { initSymbols(); }
	virtual void   DoneSymbols() { doneSymbols(); }
	virtual void   FatalError(const char* message);

	void           installErrorHandler(ISystem* pSystem);
	void           uninstallErrorHandler();
	virtual int    handleException(EXCEPTION_POINTERS* exception_pointer);

	virtual void   ReportBug(const char*);

	void           dumpCallStack(std::vector<string>& functions);

	void           SetUserDialogEnable(const bool bUserDialogEnable);

	void           PrintThreadCallstack(const threadID nThreadId, FILE* f);

	// Simulates generation of the crash report.
	void           GenerateCrashReport();

	// Creates a minimal necessary crash reporting, without UI.
	void           MinimalExceptionReport(EXCEPTION_POINTERS* exception_pointer);

	// Register CVars needed for debug call stack.
	void           RegisterCVars();

	typedef std::map<void*, string> TModules;
protected:
	bool                    initSymbols();
	void                    doneSymbols();

	static void             RemoveOldFiles();
	static void             MoveFile(const char* szFileNameOld, const char* szFileNameNew);
	static void             RemoveFile(const char* szFileName);

	NO_INLINE void          FillStackTrace(int maxStackEntries = MAX_DEBUG_STACK_ENTRIES, int skipNumFunctions = 0, HANDLE hThread = GetCurrentThread());

	static int              PrintException(EXCEPTION_POINTERS* exception_pointer);
	static INT_PTR CALLBACK ExceptionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

	int                     updateCallStack(EXCEPTION_POINTERS* exception_pointer);
	void                    LogExceptionInfo(EXCEPTION_POINTERS* exception_pointer);

	void                    WriteErrorLog( const char *filename,const char *writeString );
	void                    CaptureScreenshot();

	bool                    BackupCurrentLevel();
	int                     SubmitBug(EXCEPTION_POINTERS* exception_pointer);
	void                    ResetFPU(EXCEPTION_POINTERS* pex);

	static string           LookupFunctionName(void* address, bool fileInfo);
	static bool             LookupFunctionName(void* address, bool fileInfo, string& proc, string& file, int& line, void*& baseAddr);

	static const int s_iCallStackSize = 32768;

	char             m_excLine[256];
	char             m_excModule[128];

	char             m_excDesc[MAX_WARNING_LENGTH];
	char             m_excCode[MAX_WARNING_LENGTH];
	char             m_excAddr[80];
	char             m_excCallstack[s_iCallStackSize];

	bool             m_symbols;
	bool             m_bCrash;
	const char*      m_szBugMessage;

	ISystem*         m_pSystem;

	CONTEXT          m_context;

	TModules         m_modules;

	LPTOP_LEVEL_EXCEPTION_FILTER m_previousHandler;

	string           m_outputPath;
};

#ifdef CRY_USE_CRASHRPT

#include "CrashRpt.h"
class CCrashRpt
{
public:
	static void RegisterCVars();

	static bool InstallHandler();

	static void UninstallHandler();

	static int CALLBACK CrashCallback(CR_CRASH_CALLBACK_INFO* pInfo);

	static void CmdGenerateCrashReport(IConsoleCmdArgs*);

	static void FatalError();

	static void ReInstallCrashRptHandler(ICVar*);

	static SFileVersion GetSystemVersionInfo();

};
#endif // CRY_USE_CRASHRPT

#endif // CRY_PLATFORM_WINDOWS

#endif // __DebugCallStack_h__
