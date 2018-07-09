// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "processinfo.h"

#include "Psapi.h"

typedef BOOL (WINAPI * GetProcessMemoryInfoProc)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

static HMODULE g_hPSAPI = 0;
static GetProcessMemoryInfoProc g_pGetProcessMemoryInfo = 0;

CProcessInfo::CProcessInfo(void)
{
}

CProcessInfo::~CProcessInfo(void)
{
	UnloadPSApi();
}

void CProcessInfo::LoadPSApi()
{
	if (!g_hPSAPI)
	{
		g_hPSAPI = LoadLibrary("psapi.dll");

		if (g_hPSAPI)
		{
			g_pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(g_hPSAPI, "GetProcessMemoryInfo");
		}
	}
}

void CProcessInfo::UnloadPSApi()
{
	if (g_hPSAPI)
	{
		FreeLibrary(g_hPSAPI);
		g_hPSAPI = 0;
		g_pGetProcessMemoryInfo = 0;
	}
}

void CProcessInfo::QueryMemInfo(ProcessMemInfo& meminfo)
{
	if (!g_pGetProcessMemoryInfo)
	{
		memset(&meminfo, 0, sizeof(ProcessMemInfo));

		return;
	}

	PROCESS_MEMORY_COUNTERS pc;
	HANDLE hProcess = GetCurrentProcess();
	pc.cb = sizeof(pc);
	g_pGetProcessMemoryInfo(hProcess, &pc, sizeof(pc));

	meminfo.WorkingSet = pc.WorkingSetSize;
	meminfo.PeakWorkingSet = pc.PeakWorkingSetSize;
	meminfo.PagefileUsage = pc.PagefileUsage;
	meminfo.PeakPagefileUsage = pc.PeakPagefileUsage;
	meminfo.PageFaultCount = pc.PageFaultCount;
}

