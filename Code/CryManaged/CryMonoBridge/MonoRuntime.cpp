// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "MonoLibraryIt.h"

#include <CryThreading/IThreadManager.h>
#include <CrySystem/ILog.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/utils/mono-logger.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

#if CRY_PLATFORM_WINDOWS
	#pragma comment(lib, "mono-2.0.lib")
#endif

static const char* s_monoLogLevels[] =
{
	NULL,
	"error",
	"critical",
	"warning",
	"message",
	"info",
	"debug"
};
static IMiniLog::ELogType s_monoToEngineLevels[] = 
{
	IMiniLog::eAlways,
	IMiniLog::eErrorAlways,
	IMiniLog::eError,
	IMiniLog::eWarning,
	IMiniLog::eMessage,
	IMiniLog::eMessage,
	IMiniLog::eComment
};

void CMonoRuntime::MonoLogCallback(const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data)
{
	IMiniLog::ELogType engineLvl = IMiniLog::eComment;
	if(log_level)
		for(int i = 1; i < 7; i++)
			if(_stricmp(s_monoLogLevels[i], log_level) == 0)
			{
				engineLvl = s_monoToEngineLevels[i];
				break;
			}

	gEnv->pLog->LogWithType(engineLvl, "[Mono][%s][%s] %s", log_domain, log_level, message);
}

void CMonoRuntime::MonoPrintCallback(const char *string, mono_bool is_stdout)
{
	gEnv->pLog->Log("[Mono] %s", string);
}

void CMonoRuntime::MonoPrintErrorCallback(const char *string, mono_bool is_stdout)
{
	gEnv->pLog->LogError("[Mono] %s", string);
}

void CMonoRuntime::MonoLoadHook(MonoAssembly *assembly, void* user_data)
{
	CMonoRuntime* pRuntime = (CMonoRuntime*)user_data;
	if(!pRuntime)
		return;

	IMonoLibrary* pLib = pRuntime->GetLibrary(assembly);
	if(!pLib)
	{
		pLib = new CMonoLibrary(assembly);
		pRuntime->m_loadedLibraries.push_back(pLib);
	}
}

template<class T> 
int CMonoRuntime::ReadAll(string fileName, T** data)
{
	_finddata_t fd;
	intptr_t handle = gEnv->pCryPak->FindFirst(fileName.c_str(), &fd, ICryPak::FLAGS_PATH_REAL, true);
	if(handle < 0)
		return 0;

	FILE *f = gEnv->pCryPak->FOpen(fileName, "rb", ICryPak::FLAGS_PATH_REAL);
	int size = gEnv->pCryPak->FGetSize(f);
	*data = new T[size];
	gEnv->pCryPak->FRead(*data, size, f);
	gEnv->pCryPak->FClose(f);
	return size;
}

MonoAssembly* CMonoRuntime::MonoSearchHook(MonoAssemblyName *aname, void* user_data)
{
	CMonoRuntime* pRuntime = (CMonoRuntime*)user_data;
	if(!pRuntime)
		return NULL;

	if(pRuntime->m_bSearchOpen)
		return NULL;
	
	string asmName = mono_assembly_name_get_name(aname);
	int cryPos = asmName.find("CryEngine");
	if(cryPos == 0) // CryEngine-specific assembly; Load to memory (via CryPak) and open from there.
	{
		MonoImageOpenStatus status = MONO_IMAGE_ERROR_ERRNO;
		// load assembly image
		string fName = PathUtil::Make(pRuntime->m_sBasePath, asmName + ".dll");

		//Prefer project dll if available
		string fProjectName = PathUtil::Make(pRuntime->m_sProjectDllDir, asmName + ".dll");
		if (gEnv->pCryPak->IsFileExist(fProjectName.c_str(), ICryPak::eFileLocation_OnDisk))
			fName = fProjectName;

		char* dataImg;
		int nImg = ReadAll(fName, &dataImg);
		if(nImg <= 0)
			return NULL;

		pRuntime->m_bSearchOpen = true;
		MonoImage* pImg = mono_image_open_from_data_full(dataImg, nImg, false, &status, 0);

		// load debug information
#ifndef _RELEASE
		string fNameDbg = fName + ".mdb";
		mono_byte* dataDbg;
		int nDbg = ReadAll(fNameDbg, &dataDbg);
		if(nDbg > 0)
			mono_debug_open_image_from_memory(pImg, dataDbg, nDbg);
#endif
		// load assembly and set data
		MonoAssembly* pAsm = mono_assembly_load_from_full(pImg, asmName.c_str(), &status, 0);
		// store the memory image of the assembly, so that we can delete it, when unloading the assembly
		IMonoLibrary* pLib = pRuntime->GetLibrary(pAsm);
		if(!pLib)
		{
			pLib = new CMonoLibrary(pAsm, dataImg);
			pRuntime->m_loadedLibraries.push_back(pLib);
		}
		else
		{
			((CMonoLibrary*)pLib)->SetData(dataImg);
		}
#ifndef _RELEASE
		if(nDbg > 0)
			((CMonoLibrary*)pLib)->SetDebugData(dataDbg);
#endif
		pRuntime->m_bSearchOpen = false;

		return pAsm;
	}
	return NULL;
}

MonoAssembly* CMonoRuntime::MonoPreLoadHook(MonoAssemblyName *aname, char **assemblies_path, void* user_data)
{
	return NULL;
}

CMonoRuntime::CMonoRuntime (const char* szProjectDllDir) 
	: m_logLevel(eMLL_NULL)
	, m_domain(NULL)
	, m_gameDomain(NULL)
	, m_bSearchOpen(false)
	, m_listeners(5)
{
	cry_strcpy(m_sProjectDllDir, szProjectDllDir);
}

CMonoRuntime::~CMonoRuntime()
{
	UnloadGame();
	mono_jit_cleanup(m_domain);
}
	
void CMonoRuntime::Initialize(EMonoLogLevel logLevel)
{
	CryLog("[Mono] Initialize Mono Runtime . . . ");

	REGISTER_COMMAND("mono_reload", CMD_ReloadGame, VF_CHEAT, "Reload C# Game Assembly");

	CryGetExecutableFolder(CRY_ARRAY_COUNT(m_sBasePath), m_sBasePath);

#ifndef _RELEASE
	mono_debug_init(MONO_DEBUG_FORMAT_MONO);
	char* options[] = {
		"--soft-breakpoints",
		"--debugger-agent=transport=dt_socket,address=127.0.0.1:17615,embedding=1,server=y,suspend=n"
	};
	mono_jit_parse_options(sizeof(options) / sizeof(char*), options);
#endif
	
	char sMonoLib[_MAX_PATH];
	sprintf_s(sMonoLib, "%smono\\lib", m_sBasePath);
	char sMonoEtc[_MAX_PATH];
	sprintf_s(sMonoEtc, "%smono\\etc", m_sBasePath);

	mono_set_dirs(sMonoLib, sMonoEtc);
	m_logLevel = logLevel;
	mono_trace_set_level_string(s_monoLogLevels[m_logLevel]);
			
	mono_trace_set_log_handler(MonoLogCallback, this);
	mono_trace_set_print_handler(MonoPrintCallback);
	mono_trace_set_printerr_handler(MonoPrintErrorCallback);
	mono_install_assembly_load_hook(MonoLoadHook, this);
	mono_install_assembly_search_hook(MonoSearchHook, this);
	mono_install_assembly_refonly_search_hook(MonoSearchHook, this);
	mono_install_assembly_preload_hook(MonoPreLoadHook, this);
	mono_install_assembly_refonly_preload_hook(MonoPreLoadHook, this);

	m_domain = mono_jit_init_version("CryEngine",  "v4.0.30319");
	if(!m_domain)
	{
		gEnv->pLog->LogError("[Mono] Could not create CryEngine domain");
		CryLog("[Mono] Initialization failed!");
	}
	else
	{
		CryLog("[Mono] Initialization done.");
	}
}

IMonoLibrary* CMonoRuntime::LoadLibrary(const char* name)
{
	gEnv->pLog->LogWithType(IMiniLog::eMessage, "[Mono] Load Library: %s", name);

	MonoImageOpenStatus status = MonoImageOpenStatus::MONO_IMAGE_ERROR_ERRNO;
	MonoAssemblyName* pAsmName = mono_assembly_name_new(name);
	MonoAssembly* pAsm = mono_assembly_load(pAsmName, m_sProjectDllDir, &status);
	mono_assembly_name_free(pAsmName);
	if(!pAsm)
		return NULL;

	IMonoLibrary* pLib = GetLibrary(pAsm);
	if(!pLib)
	{
		pLib = new CMonoLibrary(pAsm);
		m_loadedLibraries.push_back(pLib);
	}
	return pLib;
}

void CMonoRuntime::LoadGame()
{
	if(!m_domain) // Invalid app domain
		return;

	char * sGameDLL = "Launcher";

	m_gameDomain = mono_domain_create_appdomain(sGameDLL, NULL);
	mono_domain_set(m_gameDomain, 1);
#ifndef _RELEASE
	mono_debug_domain_create(m_gameDomain);
#endif

	string assemblyDll = "CryEngine." + string(sGameDLL);
	m_gameLibrary = LoadLibrary(assemblyDll.c_str());
	if(!m_gameLibrary)
	{
		gEnv->pLog->LogError("[Mono] could not load game library!");
		mono_domain_set(m_domain, 1);
		mono_domain_unload(m_gameDomain);
		m_gameDomain = NULL;
		return;
	}
	
	// Make sure debugger is connected.
	CrySleep(100);

	m_gameLibrary->RunMethod("CryEngine.Launcher.Launcher:Initialize()");

	// DBG
	gEnv->pLog->Log("[Mono] Loaded Libraries:");
	for(IMonoLibraryItPtr it = GetLibraryIterator(); it->This(); it->Next())
	{
		if(it->This()->IsInMemory())
			gEnv->pLog->Log("[Mono]   %s (In Memory)", it->This()->GetName());
		else
			gEnv->pLog->Log("[Mono]   %s", it->This()->GetName());
	}
	//// ~DBG
}

void CMonoRuntime::UnloadGame()
{
	if(m_gameDomain)
	{
		m_gameLibrary->RunMethod("CryEngine.MonoLauncher.App:Shutdown()");
		for(IMonoLibraryItPtr it = GetLibraryIterator(); it->This(); it->Next())
			if(it->This()->IsInMemory())
				((CMonoLibrary*)it->This())->CloseDebug();
		
#ifndef _RELEASE
		//mono_debug_domain_unload(m_pGameDomain);
#endif
		mono_domain_set(m_domain, 1);
		mono_domain_unload(m_gameDomain);
		m_gameDomain = NULL;

		for(IMonoLibraryItPtr it = GetLibraryIterator(); it->This(); it->Next())
			if(it->This()->IsInMemory())
				((CMonoLibrary*)it->This())->Close();
		m_loadedLibraries.clear();
	}
}

IMonoLibrary* CMonoRuntime::GetLibrary(MonoAssembly* assembly)
{
	for(Libraries::iterator it = m_loadedLibraries.begin(); it != m_loadedLibraries.end(); ++it)
		if(((CMonoLibrary*)*it)->GetAssembly() == assembly)
			return *it;
	return NULL;
}

IMonoLibraryIt* CMonoRuntime::GetLibraryIterator()
{
	return (IMonoLibraryIt*)new CMonoLibraryIt(this);
}
	
void CMonoRuntime::CMD_ReloadGame(IConsoleCmdArgs* args)
{
	if(gEnv->pMonoRuntime)
		gEnv->pMonoRuntime->ReloadGame();
}