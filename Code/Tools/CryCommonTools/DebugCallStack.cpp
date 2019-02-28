// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugCallStack.h"

#include "ILogger.h"
#include <time.h>
#include <crtdbg.h>

#include "resource.h"
#include "ModuleHelpers.h"
#include "StringHelpers.h"
#include <CryString/StringUtils.h>

#pragma comment(lib, "version.lib")


#include <CryCore/Platform/CryWindows.h>   // needed for DbgHelp.h
#include "DbgHelp.h"
#pragma comment(lib, "dbghelp")
#pragma warning(disable: 4244)

#include "Psapi.h"
typedef BOOL (WINAPI *GetProcessMemoryInfoProc)( HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD );

#define MAX_PATH_LENGTH 1024
#define MAX_SYMBOL_LENGTH 512

static HWND hwndException = 0;

void PutVersion(char* str, size_t sizeInBytes);

LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
	DebugCallStack::instance()->handleException(ExceptionInfo);
	return EXCEPTION_CONTINUE_EXECUTION;
}

const char* TranslateExceptionCode( DWORD dwExcept )
{
	switch (dwExcept)
	{
	case EXCEPTION_ACCESS_VIOLATION :	return "EXCEPTION_ACCESS_VIOLATION"; break ;
	case EXCEPTION_DATATYPE_MISALIGNMENT : return "EXCEPTION_DATATYPE_MISALIGNMENT"; break ;
	case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";	break ;
	case EXCEPTION_SINGLE_STEP:	return "EXCEPTION_SINGLE_STEP";	break ;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"; break ;
	case EXCEPTION_FLT_DENORMAL_OPERAND :	return "EXCEPTION_FLT_DENORMAL_OPERAND"; break ;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO"; break ;
	case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";	break ;
	case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION"; break ;
	case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW"; break ;
	case EXCEPTION_FLT_STACK_CHECK: 	return "EXCEPTION_FLT_STACK_CHECK";	break ;
	case EXCEPTION_FLT_UNDERFLOW:	return "EXCEPTION_FLT_UNDERFLOW";	break ;
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";break ;
	case EXCEPTION_INT_OVERFLOW:return "EXCEPTION_INT_OVERFLOW";break ;
	case EXCEPTION_PRIV_INSTRUCTION:	return "EXCEPTION_PRIV_INSTRUCTION";	break ;
	case EXCEPTION_IN_PAGE_ERROR:	return "EXCEPTION_IN_PAGE_ERROR";	break ;
	case EXCEPTION_ILLEGAL_INSTRUCTION:	return "EXCEPTION_ILLEGAL_INSTRUCTION";	break ;
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:	return "EXCEPTION_NONCONTINUABLE_EXCEPTION";	break ;
	case EXCEPTION_STACK_OVERFLOW:	return "EXCEPTION_STACK_OVERFLOW";	break ;
	case EXCEPTION_INVALID_DISPOSITION:	return "EXCEPTION_INVALID_DISPOSITION";	break ;
	case EXCEPTION_GUARD_PAGE:	return "EXCEPTION_GUARD_PAGE";	break ;
	case EXCEPTION_INVALID_HANDLE:	return "EXCEPTION_INVALID_HANDLE";	break ;
		
	default:
		return "Unknown";
		break;
	}
}

void DebugCallStack::SetLog(ILogger* log)
{
	m_pLog = log;
}

void DebugCallStack::PrintException(EXCEPTION_POINTERS* pex)
{
	// Time and Version.
	char versionbuf[1024];
	PutVersion(versionbuf, sizeof(versionbuf));
	m_pLog->Log(ILogger::eSeverity_Error, "%s", versionbuf);

	//! Get call stack functions.
	DebugCallStack *cs = DebugCallStack::instance();
	std::vector<string> funcs;
	cs->getCallStack( funcs );

	// Init dialog.
	char excCode[80];
	char excAddr[80];
	sprintf( excAddr,"0x%04X:0x%p",pex->ContextRecord->SegCs,pex->ExceptionRecord->ExceptionAddress );
	sprintf( excCode,"0x%08X",pex->ExceptionRecord->ExceptionCode );
	string moduleName = DebugCallStack::instance()->getExceptionModule();
	const char *excModule = moduleName.c_str();

	char desc[1024];
	const char *excName = TranslateExceptionCode(pex->ExceptionRecord->ExceptionCode);

	if (pex->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		if (pex->ExceptionRecord->NumberParameters > 1) {
			int iswrite = pex->ExceptionRecord->ExceptionInformation[0];
			void* accessAddr = (void*)pex->ExceptionRecord->ExceptionInformation[1];
			if (iswrite) {
				sprintf( desc,"Attempt to write data to address 0x%08p - The memory could not be \"written\"",accessAddr );
			} else {
				sprintf( desc,"Attempt to read from address 0x%08p - The memory could not be \"read\"",accessAddr );
			}
		}
	}
	m_pLog->Log(ILogger::eSeverity_Error, "Exception Code: %s", excCode);
	m_pLog->Log(ILogger::eSeverity_Error, "Exception Addr: %s", excAddr);
	m_pLog->Log(ILogger::eSeverity_Error, "Exception Module: %s", excModule);
	m_pLog->Log(ILogger::eSeverity_Error, "Exception Type: %s", excName);
	m_pLog->Log(ILogger::eSeverity_Error, "Exception Description: %s", desc);

	m_pLog->Log(ILogger::eSeverity_Error, "Call Stack Trace:");

	// Call stack.
	for (unsigned int i = 0; i < funcs.size(); i++) 
	{
		m_pLog->Log(ILogger::eSeverity_Error, "%2d) %s", funcs.size()-i,(const char*)funcs[i].c_str());
	}
}

//=============================================================================
// Class Statics
//=============================================================================
DebugCallStack* DebugCallStack::m_instance = 0;

// Return single instance of class.
DebugCallStack* DebugCallStack::instance()
{
	if (!m_instance) {
		m_instance = new DebugCallStack;
	}
	return m_instance;
}

//------------------------------------------------------------------------------------------------------------------------
// Sets up the symbols for functions in the debug file.
//------------------------------------------------------------------------------------------------------------------------
DebugCallStack::DebugCallStack()
: m_pLog(0), m_pVectoredExceptionHandlerHandle(0), m_symbols(false)
{
}

DebugCallStack::~DebugCallStack()
{
}

bool DebugCallStack::initSymbols()
{
	if (m_symbols) return true;
	
	char pathname[MAX_PATH_LENGTH+1];
	char fname[MAX_PATH_LENGTH+1];
	char directory[MAX_PATH_LENGTH+1];
	char drive[10];

	SymSetOptions(SYMOPT_UNDNAME|SYMOPT_DEFERRED_LOADS|SYMOPT_INCLUDE_32BIT_MODULES|SYMOPT_LOAD_ANYTHING|SYMOPT_LOAD_LINES);
		
	HANDLE hProcess = GetCurrentProcess();
	
	// Get module file name.
	std::string moduleName = StringHelpers::ConvertString<string>(ModuleHelpers::GetCurrentModulePath(ModuleHelpers::CurrentModuleSpecifier_Library));

	// Convert it into search path for symbols.
	_splitpath(moduleName.c_str(), drive, directory, fname, NULL);
	sprintf(pathname, "%s%s", drive, directory);

	//// Get module file name.
	//GetModuleFileName( NULL, fullpath, MAX_PATH_LENGTH );

	//// Convert it into search path for symbols.
	//cry_strcpy( pathname,fullpath );
	//_splitpath( pathname, drive, directory, fname, NULL );
	//sprintf( pathname, "%s%s", drive,directory );
	
	// Append the current directory to build a search path forSymInit
	cry_strcat( pathname, ";.;" );

	int result = SymInitialize( hProcess,pathname,TRUE );
	if (result) 
	{
		char pdb[MAX_PATH_LENGTH+1];
		char res_pdb[MAX_PATH_LENGTH+1];
		sprintf( pdb, "%s.pdb",fname );
		sprintf( pathname, "%s%s", drive,directory );
		bool ok = (SearchTreeForFile( pathname,pdb,res_pdb ) != FALSE);
		m_pLog->Log(ILogger::eSeverity_Info, "SearchTreeForFile('%s','%s') %s", pathname, pdb, (ok ? "succeeded" : "failed"));
	} 
	else 
	{
		result = SymInitialize( hProcess,pathname,FALSE );
		if (!result)
		{
			m_pLog->Log(ILogger::eSeverity_Error, "SymInitialize failed");
		}
	}

	m_symbols = (result != 0);

	return result != 0;
}

void	DebugCallStack::doneSymbols()
{
	if (m_symbols) {
		SymCleanup( GetCurrentProcess() );
	}
	m_symbols = false;
}

void DebugCallStack::getCallStack( std::vector<string> &functions )
{
	functions = m_functions;
}

extern "C" void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)
#pragma auto_inline(off)
DWORD_PTR GetProgramCounter()
{
	return (DWORD_PTR)_ReturnAddress();
}
#pragma auto_inline(on)

//------------------------------------------------------------------------------------------------------------------------
int DebugCallStack::updateCallStack( void *exception_pointer )
{
	EXCEPTION_POINTERS *pex = (EXCEPTION_POINTERS*)exception_pointer;

	HANDLE process = GetCurrentProcess();

	//! Find Name of .DLL from Exception address.
	cry_strcpy( m_excModule,"<Unknown>" );

	if (m_symbols)
	{
		DWORD64 dwAddr = SymGetModuleBase64( process,(DWORD64)pex->ExceptionRecord->ExceptionAddress );
		if (dwAddr) 
		{
			char szBuff[MAX_PATH_LENGTH];
			if (GetModuleFileNameA( (HMODULE)dwAddr,szBuff,MAX_PATH_LENGTH )) 
			{
				cry_strcpy( m_excModule,szBuff );
				string path,fname,ext;
				
				char fdir[_MAX_PATH];
				char fdrive[_MAX_PATH];
				char file[_MAX_PATH];
				char fext[_MAX_PATH];
				_splitpath( m_excModule,fdrive,fdir,file,fext );
				_makepath( fdir,NULL,NULL,file,fext );

				cry_strcpy(m_excModule,fdir);
			}
		}
	}

	// Fill stack trace info.
	m_context = *pex->ContextRecord;
	FillStackTrace();

	return EXCEPTION_CONTINUE_EXECUTION;
}



//////////////////////////////////////////////////////////////////////////
void DebugCallStack::FillStackTrace(int maxStackEntries, int skipNumFunctions)
{
	HANDLE hThread = GetCurrentThread();
	HANDLE hProcess = GetCurrentProcess();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	int count;
	STACKFRAME64 stack_frame;
	BOOL b_ret=TRUE; //Setup stack frame 
	memset(&stack_frame, 0, sizeof(stack_frame));
	stack_frame.AddrPC.Mode = AddrModeFlat;
	stack_frame.AddrFrame.Mode = AddrModeFlat;
	stack_frame.AddrStack.Mode = AddrModeFlat;
	stack_frame.AddrReturn.Mode = AddrModeFlat;
	stack_frame.AddrBStore.Mode = AddrModeFlat;

	DWORD MachineType;

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

	//While there are still functions on the stack.. 
	for(count=0; count < maxStackEntries && b_ret==TRUE; count++)
	{
		b_ret = StackWalk64( MachineType,	hProcess, hThread, &stack_frame, &m_context,NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);

		if (count < skipNumFunctions)
			continue;

		if (m_symbols)
		{
			string funcName = LookupFunctionName( (void*)stack_frame.AddrPC.Offset,true );
			if (!funcName.empty())
			{
				m_functions.push_back( funcName );
			}
			else
			{
				void* p = (void*)stack_frame.AddrPC.Offset;
				char str[80];
				sprintf( str,"function=0x%p",p );
				m_functions.push_back( str );
			}
		} else {
			void* p = (void*)stack_frame.AddrPC.Offset;
			char str[80];
			sprintf( str,"function=0x%p",p );
			m_functions.push_back( str );
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------
string DebugCallStack::LookupFunctionName( void *pointer,bool fileInfo )
{
	string symName = "";

	HANDLE process = GetCurrentProcess();
	char symbolBuf[sizeof(SYMBOL_INFO)+MAX_SYMBOL_LENGTH+1];
	memset( symbolBuf, 0, sizeof(symbolBuf));
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;
 
	DWORD displacement = 0;
	DWORD64 displacement64 = 0;
	pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
	if (SymFromAddr( process,(DWORD64)pointer,&displacement64,pSymbol ))
	{
		symName = string(pSymbol->Name) + "()";
	}
		
	if (fileInfo)
	{
		// Lookup Line in source file.
		IMAGEHLP_LINE64 lineImg;
		memset( &lineImg,0,sizeof(lineImg) );
		lineImg.SizeOfStruct = sizeof(lineImg);

		if (SymGetLineFromAddr64( process,(DWORD_PTR)pointer, &displacement, &lineImg ))
		{
			char lineNum[1024];
			itoa( lineImg.LineNumber,lineNum,10 );
			string path;

			char file[1024];
			char fname[1024];
			char fext[1024];
			_splitpath( lineImg.FileName,NULL,NULL,fname,fext );
			_makepath( file,NULL,NULL,fname,fext );
			string fileName = file;

			symName += string("  [" ) + fileName + ":" + lineNum + "]";
		}
	}

	return symName;
}

void DebugCallStack::installErrorHandler()
{
	m_pVectoredExceptionHandlerHandle = AddVectoredExceptionHandler(1, VectoredHandler);
}

void DebugCallStack::uninstallErrorHandler()
{
	if (m_pVectoredExceptionHandlerHandle)
	{
		 RemoveVectoredExceptionHandler(m_pVectoredExceptionHandlerHandle);
		 m_pVectoredExceptionHandlerHandle = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
int	DebugCallStack::handleException( void *exception_pointer )
{
	EXCEPTION_POINTERS *pex = (EXCEPTION_POINTERS*)exception_pointer;

	uninstallErrorHandler();

	// Print exception info:
	{
		char excCode[80];
		char excAddr[80];
		m_pLog->Log(ILogger::eSeverity_Error, "<CRITICAL EXCEPTION>");
		sprintf( excAddr,"0x%04X:0x%p",pex->ContextRecord->SegCs,pex->ExceptionRecord->ExceptionAddress );
		sprintf( excCode,"0x%08X",pex->ExceptionRecord->ExceptionCode );
		m_pLog->Log(ILogger::eSeverity_Error, "Exception: %s, at Address: %s", excCode, excAddr);

		{
			HMODULE hPSAPI = LoadLibraryA("psapi.dll");
			if (hPSAPI)
			{
				GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
				if (pGetProcessMemoryInfo)
				{
					PROCESS_MEMORY_COUNTERS pc;
					HANDLE hProcess = GetCurrentProcess();
					pc.cb = sizeof(pc);
					pGetProcessMemoryInfo( hProcess, &pc, sizeof(pc) );
					uint32 nMemUsage = (uint32)pc.PagefileUsage/(1024*1024);
					m_pLog->Log(ILogger::eSeverity_Error, "Virtual memory usage: %dMb", nMemUsage);
				}
			}
		}
	}

	assert(!hwndException);

	if (initSymbols())
	{
		// Rise exception to call updateCallStack method.
		updateCallStack( exception_pointer );

		//! Print exception.
		PrintException(pex);

		doneSymbols();
		//exit(0);
	}

	if (pex->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)
	{
		// This is non continuable exception. abort application now.
		exit(1);
	}

	// Continue;
	return EXCEPTION_CONTINUE_EXECUTION;
}

INT_PTR CALLBACK ExceptionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	return FALSE; 
}

static void PutVersion(char* str, size_t sizeInBytes)
{
	char exe[_MAX_PATH];
	DWORD dwHandle;
	unsigned int len;

	GetModuleFileNameA( NULL, exe, _MAX_PATH );
	int fv[4],pv[4];
	
	int verSize = GetFileVersionInfoSizeA( exe,&dwHandle );
	if (verSize > 0)
	{
		char ver[1024*8];
		GetFileVersionInfoA( exe,dwHandle,sizeof(ver),ver );
		VS_FIXEDFILEINFO *vinfo;
		VerQueryValueA( ver,"\\",(void**)&vinfo,&len );
		
		fv[0] = vinfo->dwFileVersionLS & 0xFFFF;
		fv[1] = vinfo->dwFileVersionLS >> 16;
		fv[2] = vinfo->dwFileVersionMS & 0xFFFF;
		fv[3] = vinfo->dwFileVersionMS >> 16;
		
		pv[0] = vinfo->dwProductVersionLS & 0xFFFF;
		pv[1] = vinfo->dwProductVersionLS >> 16;
		pv[2] = vinfo->dwProductVersionMS & 0xFFFF;
		pv[3] = vinfo->dwProductVersionMS >> 16;
	}

	//! Get time.
	time_t ltime;
	time( &ltime );
	tm *today = localtime( &ltime );

	char s[1024];
	//! Use strftime to build a customized time string.
	strftime(s, sizeof(s), "Logged at %#c\n", today);
	cry_strcat(str, sizeInBytes, s);
	sprintf(s, "FileVersion: %d.%d.%d.%d\n", fv[3],fv[2],fv[1],fv[0]);
	cry_strcat(str, sizeInBytes, s);
	sprintf(s, "ProductVersion: %d.%d.%d.%d\n", pv[3],pv[2],pv[1],pv[0]);
	cry_strcat(str, sizeInBytes, s);
}

DebugCallStack::ErrorHandlerScope::ErrorHandlerScope(ILogger* log)
{
	DebugCallStack::instance()->SetLog(log);
	DebugCallStack::instance()->installErrorHandler();
}

DebugCallStack::ErrorHandlerScope::~ErrorHandlerScope()
{
	DebugCallStack::instance()->uninstallErrorHandler();
	DebugCallStack::instance()->SetLog(0);
}
