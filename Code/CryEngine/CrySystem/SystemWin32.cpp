// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include <time.h>

#include <CryNetwork/INetwork.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/File/ICryPak.h>
#include <CryMovie/IMovieSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryInput/IInput.h>
#include <CrySystem/ILog.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryCore/Platform/CryLibrary.h>
#include <CryGame/IGameFramework.h>
#include <CryCore/Platform/IPlatformOS.h>
#include <CryString/StringUtils.h>
#include <CrySystem/Scaleform/IScaleformHelper.h>

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <unistd.h>
#endif

#if CRY_PLATFORM_WINDOWS
	#include <float.h>
	#include <CryCore/Platform/CryWindows.h>
	#include <shellapi.h> // Needed for ShellExecute.
	#include <Psapi.h>
	#include <Aclapi.h>
	#include <tlhelp32.h>

// HAX to resolve IEntity redefinition
	#define __IEntity_INTERFACE_DEFINED__
	#include <shlobj.h>

	#include "DebugCallStack.h"
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <pwd.h>
#endif

#include "IDebugCallStack.h"

#include "XConsole.h"
#include "IDebugCallStack.h"

#include "Statistics.h"

#include "CrySizerStats.h"
#include "CrySizerImpl.h"
#include "StreamEngine/StreamEngine.h"
#include "LocalizedStringManager.h"
#include "XML/XmlUtils.h"
#include "AutoDetectSpec.h"
#include "CryPak.h"

#if CRY_PLATFORM_WINDOWS
LINK_SYSTEM_LIBRARY("wininet.lib")
LINK_SYSTEM_LIBRARY("Winmm.lib")
#endif

#if CRY_PLATFORM_APPLE
	#include "SystemUtilsApple.h"
#endif

extern CMTSafeHeap* g_pPakHeap;

// this is the list of modules that can be loaded into the game process
// Each array element contains 2 strings: the name of the module (case-insensitive)
// and the name of the group the module belongs to
//////////////////////////////////////////////////////////////////////////
const char g_szGroupCore[] = "CryEngine";
const char* g_szModuleGroups[][2] = {
	{ "Sandbox.exe",         g_szGroupCore },
	{ "CrySystem.dll",       g_szGroupCore },
	{ "CryScriptSystem.dll", g_szGroupCore },
	{ "CryNetwork.dll",      g_szGroupCore },
	{ "CryPhysics.dll",      g_szGroupCore },
	{ "CryMovie.dll",        g_szGroupCore },
	{ "CryInput.dll",        g_szGroupCore },
	{ "CryAudioSystem.dll",  g_szGroupCore },
	{ "CryFont.dll",         g_szGroupCore },
	{ "CryAISystem.dll",     g_szGroupCore },
	{ "CryEntitySystem.dll", g_szGroupCore },
	{ "Cry3DEngine.dll",     g_szGroupCore },
	{ "Game.dll",            g_szGroupCore },
	{ "CryAction.dll",       g_szGroupCore },
	{ "CryAnimation.dll",    g_szGroupCore },
	{ "CryRenderD3D9.dll",   g_szGroupCore },
	{ "CryRenderD3D10.dll",  g_szGroupCore },
	{ "CryRenderD3D11.dll",  g_szGroupCore },
	{ "CryRenderD3D12.dll",  g_szGroupCore },
	{ "CryRenderOpenGL.dll", g_szGroupCore },
	{ "CryRenderVulkan.dll", g_szGroupCore },
	{ "CryRenderNULL.dll",   g_szGroupCore },
	{ "CryFlowGraph.dll",    g_szGroupCore }
};

//! dumps the memory usage statistics to the log
//////////////////////////////////////////////////////////////////////////
void CSystem::DumpMemoryUsageStatistics(bool bUseKB)
{
	//	CResourceCollector ResourceCollector;

	//	TickMemStats(nMSP_ForDump,&ResourceCollector);
	TickMemStats(nMSP_ForDump);

	CrySizerStatsRenderer StatsRenderer(this, m_pMemStats, 10, 0);
	StatsRenderer.dump(bUseKB);

	int iSizeInM = m_env.p3DEngine->GetTerrainSize();

	//	ResourceCollector.ComputeDependencyCnt();

	//	int iTerrainSectorSize = m_env.p3DEngine->GetTerrainSectorSize();

	//	if(iTerrainSectorSize)
	//		ResourceCollector.LogData(*GetILog(),AABB(Vec3(0,0,0),Vec3(iSizeInM,iSizeInM,0)),iSizeInM/iTerrainSectorSize);

	// since we've recalculated this mem stats for dumping, we'll want to calculate it anew the next time it's rendered
	SAFE_DELETE(m_pMemStats);
}

// collects the whole memory statistics into the given sizer object
//////////////////////////////////////////////////////////////////////////

#if CRY_PLATFORM_WINDOWS
	#pragma pack(push,1)
const struct PEHeader_DLL
{
	DWORD                 signature;
	IMAGE_FILE_HEADER     _head;
	IMAGE_OPTIONAL_HEADER opt_head;
	IMAGE_SECTION_HEADER* section_header;  // actual number in NumberOfSections
};
	#pragma pack(pop)
#endif

const SmallModuleInfo* FindModuleInfo(std::vector<SmallModuleInfo>& vec, const char* name)
{
	for (size_t i = 0; i < vec.size(); ++i)
	{
		if (!vec[i].name.compareNoCase(name))
			return &vec[i];
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::CollectMemInfo(SCryEngineStatsGlobalMemInfo& m_stats)
{
	m_stats.totalUsedInModules = 0;
	m_stats.totalCodeAndStatic = 0;
	m_stats.countedMemoryModules = 0;
	m_stats.totalAllocatedInModules = 0;
	m_stats.totalNumAllocsInModules = 0;

	const std::vector<string>& szModules = GetModuleNames();
	const int numModules = szModules.size();

	//////////////////////////////////////////////////////////////////////////
	// Hardcoded value for the OS memory allocation.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < numModules; i++)
	{
		const char* szModule = szModules[i];

		SCryEngineStatsModuleInfo moduleInfo;
		ZeroStruct(moduleInfo.memInfo);
		moduleInfo.moduleStaticSize = moduleInfo.SizeOfCode = moduleInfo.SizeOfInitializedData = moduleInfo.SizeOfUninitializedData = moduleInfo.usedInModule = 0;
		moduleInfo.name = szModule;

		if (!QueryModuleMemoryInfo(moduleInfo, i))
			continue;

		m_stats.totalNumAllocsInModules += moduleInfo.memInfo.num_allocations;
		m_stats.totalAllocatedInModules += moduleInfo.memInfo.allocated;
		m_stats.totalUsedInModules += moduleInfo.usedInModule;
		m_stats.countedMemoryModules++;
		m_stats.totalCodeAndStatic += moduleInfo.moduleStaticSize;

		m_stats.modules.push_back(moduleInfo);
	}
}

void CSystem::CollectMemStats(ICrySizer* pSizer, MemStatsPurposeEnum nPurpose, std::vector<SmallModuleInfo>* pStats)
{
	std::vector<SmallModuleInfo> stats;
#if CRY_PLATFORM_WINDOWS
	//////////////////////////////////////////////////////////////////////////
	const std::vector<string>& szModules = GetModuleNames();
	const int numModules = szModules.size();

	for (int i = 0; i < numModules; i++)
	{
		const char* szModule = szModules[i];
		HMODULE hModule = GetModuleHandle(szModule);
		if (!hModule)
			continue;

		//totalStatic += me.modBaseSize;
		typedef void (* PFN_MODULEMEMORY)(CryModuleMemoryInfo*);
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress(hModule, "CryModuleGetMemoryInfo");
		if (!fpCryModuleGetAllocatedMemory)
			continue;

		PEHeader_DLL pe_header;
		PEHeader_DLL* header = &pe_header;

		const IMAGE_DOS_HEADER* dos_head = (IMAGE_DOS_HEADER*)hModule;
		if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
		{
			// Wrong pointer, not to PE header.
			continue;
		}
		header = (PEHeader_DLL*)(const void*)((char*)dos_head + dos_head->e_lfanew);
		//#endif

		SmallModuleInfo moduleInfo;
		moduleInfo.name = szModule;
		fpCryModuleGetAllocatedMemory(&moduleInfo.memInfo);

		if (nMSP_ForCrashLog == nPurpose)
		{
			int64 usedInModule = static_cast<int64>(moduleInfo.memInfo.allocated - moduleInfo.memInfo.freed);
			int numAllocations = moduleInfo.memInfo.num_allocations;
			CryLogAlways("%s Used in module:%" PRId64 " Allocations:%d", szModule, usedInModule, numAllocations);
		}
		else
		{
			stats.push_back(moduleInfo);
		}

	}
#endif

	if (pStats)
		pStats->assign(stats.begin(), stats.end());

	if (nMSP_ForCrashLog == nPurpose || nMSP_ForBudget == nPurpose)
	{
		return;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "CrySystem");
		{
			pSizer->AddObject(this, sizeof(*this));
			{
				//SIZER_COMPONENT_NAME (pSizer, "$Allocations waste");
				//const SmallModuleInfo* info = FindModuleInfo(stats, "CrySystem.dll");
				//if (info)
				//pSizer->AddObject(info, info->memInfo.allocated - info->memInfo.requested );
			}

#ifndef _LIB // Only when compiling as dynamic library
			{
				//SIZER_COMPONENT_NAME(pSizer,"Strings");
				//pSizer->AddObject( (this+1),string::_usedMemory(0) );
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "STL Allocator Waste");
				CryModuleMemoryInfo meminfo;
				ZeroStruct(meminfo);
				CryGetMemoryInfoForModule(&meminfo);
				pSizer->AddObject((this + 2), static_cast<size_t>(meminfo.STL_wasted));
			}
#endif

			{
				SIZER_COMPONENT_NAME(pSizer, "VFS");
				if (m_pStreamEngine)
				{
					SIZER_COMPONENT_NAME(pSizer, "Stream Engine");
					m_pStreamEngine->GetMemoryStatistics(pSizer);
				}
				if (m_env.pCryPak)
				{
					SIZER_COMPONENT_NAME(pSizer, "CryPak");
					((CCryPak*)m_env.pCryPak)->GetMemoryStatistics(pSizer);
					g_pPakHeap->GetMemoryUsage(pSizer);
				}
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "Localization Data");
				m_pLocalizationManager->GetMemoryUsage(pSizer);
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "XML");
				m_pXMLUtils->GetMemoryUsage(pSizer);
			}

			if (m_env.pScaleformHelper)
			{
				SIZER_COMPONENT_NAME(pSizer, "Scaleform");
				m_env.pScaleformHelper->GetFlashMemoryUsage(pSizer);
			}

			if (m_env.pConsole)
			{
				SIZER_COMPONENT_NAME(pSizer, "Console");
				m_env.pConsole->GetMemoryUsage(pSizer);
			}

			if (m_env.pLog)
			{
				SIZER_COMPONENT_NAME(pSizer, "Log");
				m_env.pLog->GetMemoryUsage(pSizer);
			}
		}
		if (m_pPlatformOS.get())
		{
			SIZER_COMPONENT_NAME(pSizer, "PlatformOS");
			m_pPlatformOS->GetMemoryUsage(pSizer);
		}
	}

	if (m_env.pCharacterManager)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryAnimation");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAnimation.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}

			m_env.pCharacterManager->GetMemoryUsage(pSizer);
		}

	}

	if (m_env.pPhysicalWorld)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryPhysics");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryPhysics.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}
			m_env.pPhysicalWorld->GetMemoryStatistics(pSizer);
		}

	}

	if (m_env.p3DEngine)
	{

		SIZER_COMPONENT_NAME(pSizer, "Cry3DEngine");
		{
			m_env.p3DEngine->GetMemoryUsage(pSizer);
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "Cry3DEngine.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}

		}

	}

	if (m_env.pRenderer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryRenderer");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste D3D9");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryRenderD3D9.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste D3D10");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryRenderD3D10.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}

			m_env.pRenderer->GetMemoryUsage(pSizer);
		}

	}

	if (m_env.pCryFont && m_pIFont)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryFont");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryFont.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}

			m_env.pCryFont->GetMemoryUsage(pSizer);
			m_pIFont->GetMemoryUsage(pSizer);
		}

	}

	if (m_env.pAudioSystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryAudioSystem");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAudioSystem.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}
		}
	}

	if (m_env.pScriptSystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryScriptSystem");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryScriptSystem.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}
			m_env.pScriptSystem->GetMemoryStatistics(pSizer);
		}

	}

	if (m_env.pAISystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryAISystem");
		{
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAISystem.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}
			m_env.pAISystem->GetMemoryStatistics(pSizer);
		}

	}

	if (m_env.pGameFramework)
	{
		{
			SIZER_COMPONENT_NAME(pSizer, "GameFramework");
			{
				SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
				const SmallModuleInfo* info = FindModuleInfo(stats, "CryAction.dll");
				if (info)
					pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
			}

			m_env.pGameFramework->GetMemoryUsage(pSizer);
		}
	}

	if (m_env.pNetwork)
	{
		SIZER_COMPONENT_NAME(pSizer, "Network");
		{
			SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
			const SmallModuleInfo* info = FindModuleInfo(stats, "CryNetwork.dll");
			if (info)
				pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
		}

		m_env.pNetwork->GetMemoryStatistics(pSizer);
	}

	if (m_env.pEntitySystem)
	{
		SIZER_COMPONENT_NAME(pSizer, "CryEntitySystem");
		{
			SIZER_COMPONENT_NAME(pSizer, "$Allocations waste");
			const SmallModuleInfo* info = FindModuleInfo(stats, "CryEntitySystem.dll");
			if (info)
				pSizer->AddObject(info, static_cast<size_t>(info->memInfo.allocated - info->memInfo.requested));
		}

		m_env.pEntitySystem->GetMemoryStatistics(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "UserData");
		if (m_pUserCallback)
		{
			m_pUserCallback->GetMemoryUsage(pSizer);
		}
	}

#if CRY_PLATFORM_WINDOWS
	{
		SIZER_COMPONENT_NAME(pSizer, "Code");
		GetExeSizes(pSizer, nPurpose);
	}
#endif

	pSizer->End();
}

//////////////////////////////////////////////////////////////////////////
const char* CSystem::GetUserName()
{
#if CRY_PLATFORM_WINDOWS
	static const int iNameBufferSize = 1024;
	static char szNameBuffer[iNameBufferSize];
	memset(szNameBuffer, 0, iNameBufferSize);

	DWORD dwSize = iNameBufferSize;
	wchar_t nameW[iNameBufferSize];
	::GetUserNameW(nameW, &dwSize);
	cry_strcpy(szNameBuffer, CryStringUtils::WStrToUTF8(nameW));
	return szNameBuffer;
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	static uid_t uid = geteuid();
	static struct passwd* pw = getpwuid(uid);
	if (pw)
	{
		return  (pw->pw_name);
	}
	else
	{
		return NULL;
	}
#elif CRY_PLATFORM_APPLE
	static const int iNameBufferSize = 1024;
	static char szNameBuffer[iNameBufferSize];
	if (AppleGetUserName(szNameBuffer, iNameBufferSize))
		return szNameBuffer;
	else
		return "";
#else
	return "";
#endif
}

//////////////////////////////////////////////////////////////////////////
int CSystem::GetApplicationInstance()
{
#if CRY_PLATFORM_WINDOWS
	if (m_iApplicationInstance == -1)
	{
		string suffix;
		for (int instance = 0;; ++instance)
		{
			suffix.Format("(%d)", instance);

			HANDLE instanceMutex = CreateMutex(NULL, TRUE, "CrytekApplication" + suffix);
			// search for duplicates
			if (GetLastError() != ERROR_ALREADY_EXISTS)
			{
				m_iApplicationInstance = instance;
				break;
			}
		}
	}

	return m_iApplicationInstance;
#else
	return 0;
#endif
}

// refreshes the m_pMemStats if necessary; creates it if it's not created
//////////////////////////////////////////////////////////////////////////
void CSystem::TickMemStats(MemStatsPurposeEnum nPurpose, IResourceCollector* pResourceCollector)
{
	// gather the statistics, if required
	// if there's  no object, or if it's time to recalculate, or if it's for dump, then recalculate it
	if (!m_pMemStats || (m_env.pRenderer->GetFrameID(false) % m_cvMemStats->GetIVal()) == 0 || nPurpose == nMSP_ForDump)
	{
		if (!m_pMemStats)
		{
			if (m_cvMemStats->GetIVal() < 4 && m_cvMemStats->GetIVal())
				GetILog()->LogToConsole("memstats is too small (%d). Performnce impact can be significant. Please set to a greater value.", m_cvMemStats->GetIVal());
			m_pMemStats = new CrySizerStats();
		}

		if (!m_pSizer)
			m_pSizer = new CrySizerImpl();

		m_pSizer->SetResourceCollector(pResourceCollector);

		m_pMemStats->startTimer(0, GetITimer());
		CollectMemStats(m_pSizer, nPurpose);
		m_pMemStats->stopTimer(0, GetITimer());

		m_pMemStats->startTimer(1, GetITimer());
		CrySizerStatsBuilder builder(m_pSizer);
		builder.build(m_pMemStats);
		m_pMemStats->stopTimer(1, GetITimer());

		m_pMemStats->startTimer(2, GetITimer());
		m_pSizer->clear();
		m_pMemStats->stopTimer(2, GetITimer());
	}
	else
		m_pMemStats->incAgeFrames();
}

//#define __HASXP

// these 2 functions are duplicated in System.cpp in editor
//////////////////////////////////////////////////////////////////////////
#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
extern int CryStats(char* buf);
#endif
int        CSystem::DumpMMStats(bool log)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	return 0;
#else
	if (log)
	{
		char buf[1024];
		int n = CryStats(buf);
		GetILog()->Log("%s", buf);
		return n;
	}
	else
	{
		return CryStats(NULL);
	};
#endif
};

//////////////////////////////////////////////////////////////////////////
struct CryDbgModule
{
	HANDLE      heap;
	WIN_HMODULE handle;
	string      name;
	DWORD       dwSize;
};

//////////////////////////////////////////////////////////////////////////
void CSystem::DebugStats(bool checkpoint, bool leaks)
{
#if CRY_PLATFORM_WINDOWS
	std::vector<CryDbgModule> dbgmodules;

	//////////////////////////////////////////////////////////////////////////
	// Use windows Performance Monitoring API to enumerate all modules of current process.
	//////////////////////////////////////////////////////////////////////////
	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 me;
		memset(&me, 0, sizeof(me));
		me.dwSize = sizeof(me);

		if (Module32First(hSnapshot, &me))
		{
			// the sizes of each module group
			do
			{
				CryDbgModule module;
				module.handle = me.hModule;
				module.name = me.szModule;
				module.dwSize = me.modBaseSize;
				dbgmodules.push_back(module);
			}
			while (Module32Next(hSnapshot, &me));
		}
		CloseHandle(hSnapshot);
	}
	//////////////////////////////////////////////////////////////////////////

	ILog* log = GetILog();
	int totalal = 0, totalbl = 0, nolib = 0;

	#ifdef _DEBUG
	int extrastats[10];
	#endif

	int totalUsedInModules = 0;
	int countedMemoryModules = 0;
	for (int i = 0; i < (int)(dbgmodules.size()); i++)
	{
		if (!dbgmodules[i].handle)
		{
			CryLogAlways("WARNING: <CrySystem> CSystem::DebugStats: NULL handle for %s", dbgmodules[i].name.c_str());
			nolib++;
			continue;
		}
		;

		typedef int (* PFN_MODULEMEMORY)();
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress((HMODULE)dbgmodules[i].handle, "CryModuleGetAllocatedMemory");
		if (fpCryModuleGetAllocatedMemory)
		{
			int allocatedMemory = fpCryModuleGetAllocatedMemory();
			totalUsedInModules += allocatedMemory;
			countedMemoryModules++;
			CryLogAlways("%8d K used in Module %s: ", allocatedMemory / 1024, dbgmodules[i].name.c_str());
		}

	#ifdef _DEBUG
		typedef void (* PFNUSAGESUMMARY)(ILog* log, const char*, int*);
		typedef void (* PFNCHECKPOINT)();
		PFNUSAGESUMMARY fpu = (PFNUSAGESUMMARY)::GetProcAddress((HMODULE)dbgmodules[i].handle, "UsageSummary");
		PFNCHECKPOINT fpc = (PFNCHECKPOINT)::GetProcAddress((HMODULE)dbgmodules[i].handle, "CheckPoint");
		if (fpu && fpc)
		{
			if (checkpoint) fpc();
			else
			{
				extrastats[2] = (int)leaks;
				fpu(log, dbgmodules[i].name.c_str(), extrastats);
				totalal += extrastats[0];
				totalbl += extrastats[1];
			};

		}
		else
		{
			CryLogAlways("WARNING: <CrySystem> CSystem::DebugStats: could not retrieve function from DLL %s", dbgmodules[i].name.c_str());
			nolib++;
		};
	#endif

		typedef HANDLE (* PFNGETDLLHEAP)();
		PFNGETDLLHEAP fpg = (PFNGETDLLHEAP)::GetProcAddress((HMODULE)dbgmodules[i].handle, "GetDLLHeap");
		if (fpg)
		{
			dbgmodules[i].heap = fpg();
		}
		;
	}
	;

	CryLogAlways("-------------------------------------------------------");
	CryLogAlways("%8d K Total Memory Allocated in %d Modules", totalUsedInModules / 1024, countedMemoryModules);
	#ifdef _DEBUG
	CryLogAlways("$8GRAND TOTAL: %d k, %d blocks (%d dlls not included)", totalal / 1024, totalbl, nolib);
	CryLogAlways("estimated debugalloc overhead: between %d k and %d k", totalbl * 36 / 1024, totalbl * 72 / 1024);
	#endif

	//////////////////////////////////////////////////////////////////////////
	// Get HeapQueryInformation pointer if on windows XP.
	//////////////////////////////////////////////////////////////////////////
	typedef BOOL (WINAPI * FUNC_HeapQueryInformation)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);
	FUNC_HeapQueryInformation pFnHeapQueryInformation = NULL;
	HMODULE hKernelInstance = CryLoadLibrary("Kernel32.dll");
	if (hKernelInstance)
	{
		pFnHeapQueryInformation = (FUNC_HeapQueryInformation)(::GetProcAddress(hKernelInstance, "HeapQueryInformation"));
	}
	//////////////////////////////////////////////////////////////////////////

	const int MAXHANDLES = 100;
	HANDLE handles[MAXHANDLES];
	int realnumh = GetProcessHeaps(MAXHANDLES, handles);
	char hinfo[1024];
	PROCESS_HEAP_ENTRY phe;
	CryLogAlways("$6--------------------- dump of windows heaps ---------------------");
	int nTotalC = 0, nTotalCP = 0, nTotalUC = 0, nTotalUCP = 0, totalo = 0;
	for (int i = 0; i < realnumh; i++)
	{
		HANDLE hHeap = handles[i];
		HeapCompact(hHeap, 0);
		hinfo[0] = 0;
		if (pFnHeapQueryInformation)
		{
			pFnHeapQueryInformation(hHeap, HeapCompatibilityInformation, hinfo, 1024, NULL);
		}
		else
		{
			for (int m = 0; m < (int)(dbgmodules.size()); m++)
			{
				if (dbgmodules[m].heap == handles[i])
				{
					cry_strcpy(hinfo, dbgmodules[m].name.c_str());
				}
			}
		}
		phe.lpData = NULL;
		int nCommitted = 0, nUncommitted = 0, nOverhead = 0;
		int nCommittedPieces = 0, nUncommittedPieces = 0;
		int nPrevRegionIndex = -1;
		while (HeapWalk(hHeap, &phe))
		{
			if (phe.wFlags & PROCESS_HEAP_REGION)
			{
				assert(++nPrevRegionIndex == phe.iRegionIndex);
				nCommitted += phe.Region.dwCommittedSize;
				nUncommitted += phe.Region.dwUnCommittedSize;
				assert(phe.cbData == 0 || (phe.wFlags & PROCESS_HEAP_ENTRY_BUSY));
			}
			else if (phe.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE)
				nUncommittedPieces += phe.cbData;
			else
				//if (phe.wFlags & PROCESS_HEAP_ENTRY_BUSY)
				nCommittedPieces += phe.cbData;

			{
				/*
				   MEMORY_BASIC_INFORMATION mbi;
				   if (VirtualQuery(phe.lpData, &mbi,sizeof(mbi)) == sizeof(mbi))
				   {
				   if (mbi.State == MEM_COMMIT)
				   nCommittedPieces += phe.cbData;//mbi.RegionSize;
				   //else
				   //	nUncommitted += mbi.RegionSize;
				   }
				   else
				   nCommittedPieces += phe.cbData;
				 */
			}

			nOverhead += phe.cbOverhead;
		}
		;
		int nCommittedMin = min(nCommitted, nCommittedPieces);
		int nCommittedMax = max(nCommitted, nCommittedPieces);
		CryLogAlways("* heap %8x: %6d (or ~%6d) K in use, %6d..%6d K uncommitted, %6d K overhead (%s)\n",
		             handles[i], nCommittedPieces / 1024, nCommitted / 1024, nUncommittedPieces / 1024, nUncommitted / 1024, nOverhead / 1024, hinfo);

		nTotalC += nCommitted;
		nTotalCP += nCommittedPieces;
		nTotalUC += nUncommitted;
		nTotalUCP += nUncommittedPieces;
		totalo += nOverhead;
	}
	;
	CryLogAlways("$6----------------- total in heaps: %d megs committed (win stats shows ~%d) (%d..%d uncommitted, %d k overhead) ---------------------", nTotalCP / 1024 / 1024, nTotalC / 1024 / 1024, nTotalUCP / 1024 / 1024, nTotalUC / 1024 / 1024, totalo / 1024);

#endif // CRY_PLATFORM_WINDOWS
};

#if CRY_PLATFORM_WINDOWS
struct DumpHeap32Stats
{
	DumpHeap32Stats() : dwFree(0), dwMoveable(0), dwFixed(0), dwUnknown(0)
	{}
	void operator+=(const DumpHeap32Stats& right)
	{
		dwFree += right.dwFree;
		dwMoveable += right.dwMoveable;
		dwFixed += right.dwFixed;
		dwUnknown += right.dwUnknown;
	}
	DWORD dwFree;
	DWORD dwMoveable;
	DWORD dwFixed;
	DWORD dwUnknown;
};

#pragma warning(push)
#pragma warning(disable: 4244) //conversion' conversion from 'type1' to 'type2', possible loss of data
static void DumpHeap32(const HEAPLIST32& hl, DumpHeap32Stats& stats)
{
	HEAPENTRY32 he;
	memset(&he, 0, sizeof(he));
	he.dwSize = sizeof(he);

	if (Heap32First(&he, hl.th32ProcessID, hl.th32HeapID))
	{
		DumpHeap32Stats heap;
		do
		{
			if (he.dwFlags & LF32_FREE)
				heap.dwFree += he.dwBlockSize;
			else if (he.dwFlags & LF32_MOVEABLE)
				heap.dwMoveable += he.dwBlockSize;
			else if (he.dwFlags & LF32_FIXED)
			{
				heap.dwFixed += he.dwBlockSize;
			}
			else
				heap.dwUnknown += he.dwBlockSize;
		}
		while (Heap32Next(&he));

		CryLogAlways("%08X  %6d %6d %6d (%d)", hl.th32HeapID, heap.dwFixed / 0x400, heap.dwFree / 0x400, heap.dwMoveable / 0x400, heap.dwUnknown / 0x400);
		stats += heap;
	}
	else
		CryLogAlways("%08X  empty or invalid");
}
#pragma warning(pop)

//////////////////////////////////////////////////////////////////////////
class CStringOrder
{
public:
	bool operator()(const char* szLeft, const char* szRight) const { return stricmp(szLeft, szRight) < 0; }
};
typedef std::map<const char*, unsigned, CStringOrder> StringToSizeMap;
void AddSize(StringToSizeMap& mapSS, const char* szString, unsigned nSize)
{
	StringToSizeMap::iterator it = mapSS.find(szString);
	if (it == mapSS.end())
		mapSS.insert(StringToSizeMap::value_type(szString, nSize));
	else
		it->second += nSize;
}

//////////////////////////////////////////////////////////////////////////
const char* GetModuleGroup(const char* szString)
{
	for (unsigned i = 0; i < CRY_ARRAY_COUNT(g_szModuleGroups); ++i)
		if (stricmp(szString, g_szModuleGroups[i][0]) == 0)
			return g_szModuleGroups[i][1];
	return "Other";
}

//////////////////////////////////////////////////////////////////////////
void CSystem::GetExeSizes(ICrySizer* pSizer, MemStatsPurposeEnum nPurpose)
{
	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways("Cannot get the module snapshot, error code %d", GetLastError());
		return;
	}

	DWORD dwProcessID = GetCurrentProcessId();

	MODULEENTRY32 me;
	memset(&me, 0, sizeof(me));
	me.dwSize = sizeof(me);

	if (Module32First(hSnapshot, &me))
	{
		// the sizes of each module group
		StringToSizeMap mapGroupSize;
		DWORD dwTotalModuleSize = 0;
		do
		{
			dwProcessID = me.th32ProcessID;
			const char* szGroup = GetModuleGroup(me.szModule);
			SIZER_COMPONENT_NAME(pSizer, szGroup);
			if (nPurpose == nMSP_ForDump)
			{
				SIZER_COMPONENT_NAME(pSizer, me.szModule);
				pSizer->AddObject(me.modBaseAddr, static_cast<size_t>(me.modBaseSize));
			}
			else
				pSizer->AddObject(me.modBaseAddr, static_cast<size_t>(me.modBaseSize));
		}
		while (Module32Next(hSnapshot, &me));
	}
	else
		CryLogAlways("No modules to dump");

	CloseHandle(hSnapshot);
}

#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::DumpWinHeaps()
{
#if CRY_PLATFORM_WINDOWS
	//
	// Retrieve modules and log them; remember the process id

	HANDLE hSnapshot;
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways("Cannot get the module snapshot, error code %d", GetLastError());
		return;
	}

	DWORD dwProcessID = GetCurrentProcessId();

	MODULEENTRY32 me;
	memset(&me, 0, sizeof(me));
	me.dwSize = sizeof(me);

	if (Module32First(hSnapshot, &me))
	{
		// the sizes of each module group
		StringToSizeMap mapGroupSize;
		DWORD dwTotalModuleSize = 0;
		CryLogAlways("base        size  module");
		do
		{
			dwProcessID = me.th32ProcessID;
			const char* szGroup = GetModuleGroup(me.szModule);
			CryLogAlways("%08X %8X  %25s   - %s", me.modBaseAddr, me.modBaseSize, me.szModule, stricmp(szGroup, "Other") ? szGroup : "");
			dwTotalModuleSize += me.modBaseSize;
			AddSize(mapGroupSize, szGroup, me.modBaseSize);
		}
		while (Module32Next(hSnapshot, &me));

		CryLogAlways("------------------------------------");
		for (StringToSizeMap::iterator it = mapGroupSize.begin(); it != mapGroupSize.end(); ++it)
			CryLogAlways("         %6.3f Mbytes  - %s", double(it->second) / 0x100000, it->first);
		CryLogAlways("------------------------------------");
		CryLogAlways("         %6.3f Mbytes  - TOTAL", double(dwTotalModuleSize) / 0x100000);
		CryLogAlways("------------------------------------");
	}
	else
		CryLogAlways("No modules to dump");

	CloseHandle(hSnapshot);

	//
	// Retrieve the heaps and dump each of them with a special function

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{
		CryLogAlways("Cannot get the heap LIST snapshot, error code %d", GetLastError());
		return;
	}

	HEAPLIST32 hl;
	memset(&hl, 0, sizeof(hl));
	hl.dwSize = sizeof(hl);

	CryLogAlways("__Heap__   fixed   free   move (unknown)");
	if (Heap32ListFirst(hSnapshot, &hl))
	{
		DumpHeap32Stats stats;
		do
		{
			DumpHeap32(hl, stats);
		}
		while (Heap32ListNext(hSnapshot, &hl));

		CryLogAlways("-------------------------------------------------");
		CryLogAlways("$6          %6.3f %6.3f %6.3f (%.3f) Mbytes", double(stats.dwFixed) / 0x100000, double(stats.dwFree) / 0x100000, double(stats.dwMoveable) / 0x100000, double(stats.dwUnknown) / 0x100000);
		CryLogAlways("-------------------------------------------------");
	}
	else
		CryLogAlways("No heaps to dump");

	CloseHandle(hSnapshot);
#endif
}

// Make system error message string
//////////////////////////////////////////////////////////////////////////
//! \return pointer to the null terminated error string or 0
static const char* GetLastSystemErrorMessage()
{
#if CRY_PLATFORM_WINDOWS
	DWORD dwError = GetLastError();

	static char szBuffer[512]; // function will return pointer to this buffer

	if (dwError)
	{
		LPVOID lpMsgBuf = 0;

		if (FormatMessage(
		      FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		      (LPTSTR) &lpMsgBuf,
		      0,
		      NULL))
		{
			cry_strcpy(szBuffer, (char*)lpMsgBuf);
			LocalFree(lpMsgBuf);
		}
		else return 0;

		//#else

		//cry_sprintf(szBuffer, "Win32 ERROR: %i", dwError);
		//OutputDebugString(szBuffer);

		//#endif

		return szBuffer;
	}
#else
	return 0;

#endif // CRY_PLATFORM_WINDOWS

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSystem::FatalError(const char* format, ...)
{
	// format message
	va_list ArgList;
	char szBuffer[MAX_WARNING_LENGTH];
	const char* sPrefix = "";
	cry_strcpy(szBuffer, sPrefix);
	va_start(ArgList, format);
	cry_vsprintf(szBuffer + strlen(szBuffer), sizeof(szBuffer) - strlen(szBuffer), format, ArgList);
	va_end(ArgList);

	// get system error message before any attempt to write into log
	const char* szSysErrorMessage = GetLastSystemErrorMessage();

	CryLogAlways("=============================================================================");
	CryLogAlways("*ERROR");
	CryLogAlways("=============================================================================");
	// write both messages into log
	CryLogAlways("%s", szBuffer);

	if (szSysErrorMessage)
		CryLogAlways("<CrySystem> Last System Error: %s", szSysErrorMessage);

	if (const char* pLoadingProfilerCallstack = GetLoadingProfilerCallstack())
		if (pLoadingProfilerCallstack[0])
			CryLogAlways("<CrySystem> LoadingProfilerCallstack: %s", pLoadingProfilerCallstack);

	assert(szBuffer[0] >= ' ');
	//	strcpy(szBuffer,szBuffer+1);	// remove verbosity tag since it is not supported by ::MessageBox

	LogSystemInfo();

	CollectMemStats(0, nMSP_ForCrashLog);

	OutputDebugString(szBuffer);
#if CRY_PLATFORM_WINDOWS
	OnFatalError(szBuffer);

	if (!g_cvars.sys_no_crash_dialog)
	{
		CryMessageBox(szBuffer,"CRYENGINE FATAL ERROR", eMB_Error);
	}

	//Triggers a fatal error, so the DebugCallstack can create the error.log and terminate the application
	IDebugCallStack::instance()->FatalError(szBuffer);
#endif

	CryDebugBreak();

	// app can not continue
#ifdef _DEBUG

	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
	__debugbreak();
	#endif

#else

	#if CRY_PLATFORM_WINDOWS
	_flushall();
	#endif

	#if CRY_PLATFORM_ORBIS
	_Exit(1);
	#else
	_exit(1);
	#endif
#endif
}

void CSystem::ReportBug(const char* format, ...)
{
#if CRY_PLATFORM_WINDOWS
	va_list ArgList;
	char szBuffer[MAX_WARNING_LENGTH];
	const char* sPrefix = "";
	cry_strcpy(szBuffer, sPrefix);
	va_start(ArgList, format);
	cry_vsprintf(szBuffer + strlen(szBuffer), sizeof(szBuffer) - strlen(szBuffer), format, ArgList);
	va_end(ArgList);

	IDebugCallStack::instance()->ReportBug(szBuffer);
#endif
}

// tries to log the call stack . for DEBUG purposes only
//////////////////////////////////////////////////////////////////////////
void CSystem::LogCallStack()
{
#if CRY_PLATFORM_WINDOWS
	DebugCallStack::instance()->LogCallstack();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::debug_GetCallStack(const char** pFunctions, int& nCount)
{
#if CRY_PLATFORM_WINDOWS
	int nMaxCount = nCount;
	nCount = 0;
	DebugCallStack::instance()->CollectCurrentCallStack();
	static std::vector<string> funcs;
	funcs.clear();
	DebugCallStack::instance()->getCallStack(funcs);

	nCount = ((int)funcs.size()) - 1; // Remove this function from the call stack.
	if (nCount < 0)
		nCount = 0;
	else if (nCount > nMaxCount)
		nCount = nMaxCount;

	for (int i = 0; i < nMaxCount && i < nCount; i++)
	{
		pFunctions[i] = funcs[i + 1].c_str();
	}
#elif CRY_PLATFORM_ORBIS
	void* callstack[MAX_DEBUG_STACK_ENTRIES];
	uint callstackLength = min((uint)nCount, (uint)MAX_DEBUG_STACK_ENTRIES);
	debug_GetCallStackRaw(callstack, callstackLength);
	static std::vector<string> funcs;
	funcs.clear();
	int length = (int)callstackLength - 2;    // Don't include debug_GetCallStackRaw & this function in the call stack
	for (int i = 0; i < length; i++)
	{
		funcs.push_back(string().Format("\t%08X\r\n", callstack[i])); // Mimics DebugCallStack::FormatFunctions used on Xbox
		pFunctions[callstackLength - 1 - i] = funcs.back().c_str();   // Un-reverse the callstack to be consistent with other platforms
	}
	nCount = funcs.size();
#else
	nCount = 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
void CSystem::debug_LogCallStack(int nMaxFuncs, int nFlags)
{
	if (nMaxFuncs > 32)
		nMaxFuncs = 32;
	// Print call stack for each find.
	const char* funcs[32];
	int nCount = nMaxFuncs;
	int nCurFrame = 0;
	if (m_env.pRenderer)
		nCurFrame = (int)m_env.pRenderer->GetFrameID(false);
	CryLogAlways("    ----- CallStack (Frame: %d) -----", nCurFrame);
	GetISystem()->debug_GetCallStack(funcs, nCount);
	for (int i = 1; i < nCount; i++) // start from 1 to skip this function.
	{
		CryLogAlways("    %02d) %s", i, funcs[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
// Support relaunching for windows media center edition.
//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
bool CSystem::ReLaunchMediaCenter()
{
	// Skip if not running on a Media Center
	if (GetSystemMetrics(SM_MEDIACENTER) == 0)
		return false;

	// Get the path to Media Center
	char szExpandedPath[MAX_PATH];
	if (!ExpandEnvironmentStrings("%SystemRoot%\\ehome\\ehshell.exe", szExpandedPath, MAX_PATH))
		return false;

	// Skip if ehshell.exe doesn't exist
	if (GetFileAttributes(szExpandedPath) == 0xFFFFFFFF)
		return false;

	// Launch ehshell.exe
	INT_PTR result = (INT_PTR)ShellExecute(NULL, TEXT("open"), szExpandedPath, NULL, NULL, SW_SHOWNORMAL);
	return (result > 32);
}
#else
bool CSystem::ReLaunchMediaCenter()
{
	return false;
}
#endif // CRY_PLATFORM_WINDOWS

//////////////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_WINDOWS
void CSystem::LogSystemInfo()
{
	//////////////////////////////////////////////////////////////////////
	// Write the system informations to the log
	//////////////////////////////////////////////////////////////////////

	char szBuffer[1024];
	char szProfileBuffer[128];
	char szLanguageBuffer[64];
	//char szCPUModel[64];
	char* pChar = 0;

	MEMORYSTATUSEX MemoryStatus;
	MemoryStatus.dwLength = sizeof(MemoryStatus);

	DEVMODE DisplayConfig;
	OSVERSIONINFO OSVerInfo;
	OSVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	// log Windows type
	Win32SysInspect::GetOS(m_env.pi.winVer, m_env.pi.win64Bit, szBuffer, sizeof(szBuffer));
	CryLogAlways(szBuffer);

	// log user name
	CryLog("User name: \"%s\"", GetUserName());

	// log system language
	GetLocaleInfo(LOCALE_SYSTEM_DEFAULT, LOCALE_SENGLANGUAGE, szLanguageBuffer, sizeof(szLanguageBuffer));
	cry_sprintf(szBuffer, "System language: %s", szLanguageBuffer);
	CryLogAlways(szBuffer);

	// log Windows directory
	GetWindowsDirectory(szBuffer, sizeof(szBuffer));
	string str = "Windows Directory: \"";
	str += szBuffer;
	str += "\"";
	CryLogAlways(str);

	//////////////////////////////////////////////////////////////////////
	// Send system time & date
	//////////////////////////////////////////////////////////////////////

	str = "Local time is ";
	_strtime(szBuffer);
	str += szBuffer;
	str += " ";
	_strdate(szBuffer);
	str += szBuffer;
	cry_sprintf(szBuffer, ", system running for %d minutes", GetTickCount() / 60000);
	str += szBuffer;
	CryLogAlways(str);

	//////////////////////////////////////////////////////////////////////
	// Send system memory status
	//////////////////////////////////////////////////////////////////////

	GlobalMemoryStatusEx(&MemoryStatus);
	cry_sprintf(szBuffer, "%I64dMB physical memory installed, %I64dMB available, %I64dMB virtual memory installed, %ld percent of memory in use",
	            MemoryStatus.ullTotalPhys / 1048576 + 1,
	            MemoryStatus.ullAvailPhys / 1048576,
	            MemoryStatus.ullTotalVirtual / 1048576,
	            MemoryStatus.dwMemoryLoad);
	CryLogAlways(szBuffer);

	if (GetISystem()->GetIMemoryManager())
	{
		IMemoryManager::SProcessMemInfo memCounters;
		GetISystem()->GetIMemoryManager()->GetProcessMemInfo(memCounters);

		uint64 PagefileUsage = memCounters.PagefileUsage;
		uint64 PeakPagefileUsage = memCounters.PeakPagefileUsage;
		uint64 WorkingSetSize = memCounters.WorkingSetSize;
		cry_sprintf(szBuffer, "PageFile usage: %I64dMB, Working Set: %I64dMB, Peak PageFile usage: %I64dMB,",
		            (uint64)PagefileUsage / (1024 * 1024),
		            (uint64)WorkingSetSize / (1024 * 1024),
		            (uint64)PeakPagefileUsage / (1024 * 1024));
		CryLogAlways(szBuffer);
	}

	//////////////////////////////////////////////////////////////////////
	// Send display settings
	//////////////////////////////////////////////////////////////////////

	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &DisplayConfig);
	GetPrivateProfileString("boot.description", "display.drv",
	                        "(Unknown graphics card)", szProfileBuffer, sizeof(szProfileBuffer),
	                        "system.ini");
	cry_sprintf(szBuffer, "Current display mode is %dx%dx%d, %s",
	            DisplayConfig.dmPelsWidth, DisplayConfig.dmPelsHeight,
	            DisplayConfig.dmBitsPerPel, szProfileBuffer);
	CryLogAlways(szBuffer);

	//////////////////////////////////////////////////////////////////////
	// Send input device configuration
	//////////////////////////////////////////////////////////////////////

	str = "";
	// Detect the keyboard type
	switch (GetKeyboardType(0))
	{
	case 1:
		str = "IBM PC/XT (83-key)";
		break;
	case 2:
		str = "ICO (102-key)";
		break;
	case 3:
		str = "IBM PC/AT (84-key)";
		break;
	case 4:
		str = "IBM enhanced (101/102-key)";
		break;
	case 5:
		str = "Nokia 1050";
		break;
	case 6:
		str = "Nokia 9140";
		break;
	case 7:
		str = "Japanese";
		break;
	default:
		str = "Unknown";
		break;
	}

	// Any mouse attached ?
	if (!GetSystemMetrics(SM_MOUSEPRESENT))
		CryLogAlways(str + " keyboard and no mouse installed");
	else
	{
		cry_sprintf(szBuffer, " keyboard and %i+ button mouse installed",
		            GetSystemMetrics(SM_CMOUSEBUTTONS));
		CryLogAlways(str + szBuffer);
	}

	CryLogAlways("--------------------------------------------------------------------------------");
}
#else
void CSystem::LogSystemInfo()
{
}
#endif

#if CRY_PLATFORM_WINDOWS
//////////////////////////////////////////////////////////////////////////
bool CSystem::GetWinGameFolder(char* szMyDocumentsPath, int maxPathSize)
{
	bool bSucceeded = false;
	// check Vista and later OS first

	HMODULE shell32 = LoadLibraryA("Shell32.dll");
	if (shell32)
	{
		typedef long (__stdcall * T_SHGetKnownFolderPath)(REFKNOWNFOLDERID rfid, unsigned long dwFlags, void* hToken, wchar_t** ppszPath);
		T_SHGetKnownFolderPath _SHGetKnownFolderPath = (T_SHGetKnownFolderPath)GetProcAddress(shell32, "SHGetKnownFolderPath");
		if (_SHGetKnownFolderPath)
		{
			// We must be running Vista or newer
			wchar_t* wMyDocumentsPath;
			HRESULT hr = _SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE | KF_FLAG_DONT_UNEXPAND, NULL, &wMyDocumentsPath);
			bSucceeded = SUCCEEDED(hr);
			if (bSucceeded)
			{
				// Convert from UNICODE to UTF-8
				cry_strcpy(szMyDocumentsPath, maxPathSize, CryStringUtils::WStrToUTF8(wMyDocumentsPath));
				CoTaskMemFree(wMyDocumentsPath);
			}
		}
		FreeLibrary(shell32);
	}

	if (!bSucceeded)
	{
		// check pre-vista OS if not succeeded before
		wchar_t wMyDocumentsPath[MAX_PATH];
		bSucceeded = SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, wMyDocumentsPath));
		if (bSucceeded)
		{
			cry_strcpy(szMyDocumentsPath, maxPathSize, CryStringUtils::WStrToUTF8(wMyDocumentsPath));
		}
	}

	return bSucceeded;
}
#endif

//////////////////////////////////////////////////////////////////////////
void CSystem::ChangeUserPath(const char* sUserPath)
{
	string userFolder = sUserPath;

#if CRY_PLATFORM_WINDOWS
	bool folderCreated = false;

	#if defined(DEDICATED_SERVER)
	userFolder = "";              // Enforce userfolder as empty which will cause USER%d creation if root is not specified
	if (!(m_root.empty()))
	{
		m_sys_user_folder->Set(m_root.c_str());
		userFolder = m_root.c_str();
		folderCreated = true;
	}
	else
	#endif // defined(DEDICATED_SERVER)
	{
		if (userFolder.empty())
		{
			userFolder = "USER";
			m_env.pCryPak->MakeDir(userFolder.c_str());
		}
		else
		{
			char szMyDocumentsPath[MAX_PATH];
			if (GetWinGameFolder(szMyDocumentsPath, MAX_PATH))
			{
				string mydocs = string(szMyDocumentsPath) + "\\" + userFolder;
				mydocs = PathUtil::RemoveSlash(mydocs);
				mydocs = PathUtil::ToDosPath(mydocs);
				userFolder = mydocs;
				m_env.pCryPak->MakeDir(mydocs);
				folderCreated = true;
			}
		}
	}

	if (!folderCreated)
	{
		// pick a unique dir name for this instance
		int instance = GetApplicationInstance();
		if (instance != 0)
		{
			userFolder.Format("USER(%d)", instance);
			m_sys_user_folder->Set(userFolder.c_str());
		}

		// Make the userFolder path absolute
		char cwdBuffer[MAX_PATH];
		CryGetCurrentDirectory(MAX_PATH, cwdBuffer);
		string tempBuffer;
		tempBuffer.Format("%s\\%s", cwdBuffer, userFolder.c_str());
		tempBuffer = PathUtil::RemoveSlash(tempBuffer);
		tempBuffer = PathUtil::ToDosPath(tempBuffer);
		userFolder = tempBuffer;

		gEnv->pCryPak->MakeDir(userFolder.c_str());
	}

#elif CRY_PLATFORM_DURANGO
	userFolder = "USER";
	m_env.pCryPak->MakeDir(userFolder.c_str());
	m_sys_user_folder->Set(userFolder.c_str());
#elif CRY_PLATFORM_ANDROID
	userFolder = CryGetUserStoragePath();
	m_sys_user_folder->Set(userFolder.c_str());
	m_env.pCryPak->MakeDir(userFolder.c_str());
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	if (userFolder.empty())
	{
		userFolder = "USER";
		m_env.pCryPak->MakeDir(userFolder.c_str());
	}
	else
	{
		//TODO: Use Cocoa Shared Application folder to store info on Mac OS X
		const char* home = getenv("HOME");
		if (home != NULL)
		{
			string mydocs = string(home) + "/" + userFolder;
			mydocs = PathUtil::RemoveSlash(mydocs);
			userFolder = mydocs;
			m_env.pCryPak->MakeDir(userFolder.c_str());
		}
	}
#elif CRY_PLATFORM_ORBIS
	userFolder = PathUtil::Make(m_env.pSystem->GetRootFolder(), "user");
	m_env.pCryPak->MakeDir(userFolder);
#endif

	m_env.pCryPak->SetAlias("%USER%", userFolder.c_str(), true);
}

//////////////////////////////////////////////////////////////////////////
void CSystem::DetectGameFolderAccessRights()
{
	// This code is trying to figure out if the current folder we are now running under have write access.
	// By default assume folder is not writable.
	// If folder is writable game.log is saved there, otherwise it is saved in user documents folder.

#if CRY_PLATFORM_WINDOWS

	DWORD DesiredAccess = FILE_GENERIC_WRITE;
	DWORD GrantedAccess = 0;
	DWORD dwRes = 0;
	PACL pDACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	HANDLE hClientToken = 0;
	PRIVILEGE_SET PrivilegeSet;
	DWORD PrivilegeSetLength = sizeof(PrivilegeSet);
	BOOL bAccessStatus = FALSE;

	// Get a pointer to the existing DACL.
	dwRes = GetNamedSecurityInfo(".", SE_FILE_OBJECT,
	                             DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
	                             NULL, NULL, &pDACL, NULL, &pSD);

	if (ERROR_SUCCESS != dwRes)
	{
		//
		assert(0);
	}

	if (!ImpersonateSelf(SecurityIdentification))
		return;
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hClientToken) && hClientToken != 0)
		return;

	GENERIC_MAPPING GenMap;
	GenMap.GenericRead = FILE_GENERIC_READ;
	GenMap.GenericWrite = FILE_GENERIC_WRITE;
	GenMap.GenericExecute = FILE_GENERIC_EXECUTE;
	GenMap.GenericAll = FILE_ALL_ACCESS;

	MapGenericMask(&DesiredAccess, &GenMap);
	if (!AccessCheck(pSD, hClientToken, DesiredAccess, &GenMap, &PrivilegeSet, &PrivilegeSetLength, &GrantedAccess, &bAccessStatus))
	{
		RevertToSelf();
		CloseHandle(hClientToken);
		return;
	}
	CloseHandle(hClientToken);
	RevertToSelf();

	if (bAccessStatus)
	{
		m_bGameFolderWritable = true;
	}
#elif CRY_PLATFORM_MOBILE
	char cwd[PATH_MAX];

	if (getcwd(cwd, PATH_MAX) != NULL)
	{
		if (0 == access(cwd, W_OK))
		{
			m_bGameFolderWritable = true;
		}
	}
#endif
}