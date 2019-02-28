// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   WindowsErrorReporting.cpp
//  Created:     16/11/2006 by Timur.
//  Description: Support for Windows Error Reporting (WER)
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if CRY_PLATFORM_WINDOWS

	#include "System.h"
	#include <tchar.h>
	#include "errorrep.h"
	#include <CrySystem/ISystem.h>
	#include "dbghelp.h"

static WCHAR szPath[MAX_PATH + 1];
static WCHAR szFR[] = L"\\System32\\FaultRep.dll";

WCHAR* GetFullPathToFaultrepDll(void)
{
	CHAR* lpRet = NULL;
	UINT rc;

	rc = GetSystemWindowsDirectoryW(szPath, ARRAYSIZE(szPath));
	if (rc == 0 || rc > ARRAYSIZE(szPath) - ARRAYSIZE(szFR) - 1)
		return NULL;

	wcscat(szPath, szFR);
	return szPath;
}

typedef BOOL (WINAPI * MINIDUMPWRITEDUMP)(HANDLE hProcess, DWORD dwPid, HANDLE hFile, MINIDUMP_TYPE DumpType,
                                          CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
                                          CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
                                          CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam
                                          );

//////////////////////////////////////////////////////////////////////////
LONG WINAPI CryEngineExceptionFilterMiniDump(struct _EXCEPTION_POINTERS* pExceptionPointers, const char* szDumpPath, MINIDUMP_TYPE mdumpValue)
{
	LONG lRet = EXCEPTION_CONTINUE_SEARCH;
	HWND hParent = NULL;
	HMODULE hDll = NULL;
	//	char szDbgHelpPath[_MAX_PATH];

	if (hDll == NULL)
	{
		// load any version we can
		hDll = ::LoadLibraryA("DBGHELP.DLL");
	}

	const TCHAR* szResult = NULL;
	char szLogMessage[_MAX_PATH + 1024] = { 0 };// extra data for prefix

	//TCHAR * m_szAppName = _T("CE2Dump");
	if (hDll)
	{
		MINIDUMPWRITEDUMP pDump = (MINIDUMPWRITEDUMP)::GetProcAddress(hDll, "MiniDumpWriteDump");
		if (pDump)
		{
			// ask the user if they want to save a dump file
			if (true /*::MessageBox( NULL, "Something bad happened in your program, would you like to save a diagnostic file?", m_szAppName, MB_YESNO )==IDYES*/)
			{
				// create the file
				HANDLE hFile = ::CreateFile(szDumpPath, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
				                            FILE_ATTRIBUTE_NORMAL, NULL);

				if (hFile != INVALID_HANDLE_VALUE)
				{
					_MINIDUMP_EXCEPTION_INFORMATION ExInfo;

					ExInfo.ThreadId = ::GetCurrentThreadId();
					ExInfo.ExceptionPointers = pExceptionPointers;
					ExInfo.ClientPointers = NULL;

					BOOL bOK = pDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdumpValue, &ExInfo, NULL, NULL);
					if (bOK)
					{
						cry_sprintf(szLogMessage, "Saved dump file to '%s'", szDumpPath);
						szResult = szLogMessage;
						lRet = EXCEPTION_EXECUTE_HANDLER;
					}
					else
					{
						cry_sprintf(szLogMessage, "Failed to save dump file to '%s' (error %d)", szDumpPath, GetLastError());
						szResult = szLogMessage;
					}
					::CloseHandle(hFile);
				}
				else
				{
					cry_sprintf(szLogMessage, "Failed to create dump file '%s' (error %d)", szDumpPath, GetLastError());
					szResult = szLogMessage;
				}
			}
		}
		else
		{
			szResult = "DBGHELP.DLL too old";
		}
	}
	else
	{
		szResult = "DBGHELP.DLL not found";
	}
	CryLogAlways("%s", szResult);

	return lRet;
}

/*
   struct AutoSetCryEngineExceptionFilter
   {
   AutoSetCryEngineExceptionFilter()
   {
    WCHAR * psz = GetFullPathToFaultrepDll();
    SetUnhandledExceptionFilter(CryEngineExceptionFilterWER);
   }
   };
   AutoSetCryEngineExceptionFilter g_AutoSetCryEngineExceptionFilter;
 */

//////////////////////////////////////////////////////////////////////////
LONG WINAPI CryEngineExceptionFilterWER(struct _EXCEPTION_POINTERS* pExceptionPointers)
{

	if (g_cvars.sys_WER > 1)
	{
		char szScratch[_MAX_PATH];
		const char* szDumpPath = gEnv->pCryPak->AdjustFileName("%USER%/CE2Dump.dmp", szScratch, 0);

		MINIDUMP_TYPE mdumpValue = (MINIDUMP_TYPE)(MiniDumpNormal);
		if (g_cvars.sys_WER > 1)
			mdumpValue = (MINIDUMP_TYPE)(g_cvars.sys_WER - 2);

		return CryEngineExceptionFilterMiniDump(pExceptionPointers, szDumpPath, mdumpValue);
	}

	LONG lRet = EXCEPTION_CONTINUE_SEARCH;
	WCHAR* psz = GetFullPathToFaultrepDll();
	if (psz)
	{
		HMODULE hFaultRepDll = LoadLibraryW(psz);
		if (hFaultRepDll)
		{
			pfn_REPORTFAULT pfn = (pfn_REPORTFAULT)GetProcAddress(hFaultRepDll, "ReportFault");
			if (pfn)
			{
				EFaultRepRetVal rc = pfn(pExceptionPointers, 0);
				lRet = EXCEPTION_EXECUTE_HANDLER;
			}
			FreeLibrary(hFaultRepDll);
		}
	}
	return lRet;
}

#endif // CRY_PLATFORM_WINDOWS
