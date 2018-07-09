// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ResourceManager.h
//  Version:     v1.00
//  Created:     8/02/2010 by Timur.
//  Description: Interface to the Resource Manager
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ResourceManager.h"
#include "System.h"
#include "CryPak.h"

#define LEVEL_PAK_FILENAME                      "level.pak"
#define LEVEL_PAK_INMEMORY_MAXSIZE              10 * 1024 * 1024

#define ENGINE_PAK_FILENAME                     "engine.pak"
#define LEVEL_CACHE_PAK_FILENAME                "xml.pak"

#define GAME_DATA_PAK_FILENAME                  "gamedata.pak"
#define FAST_LOADING_PAKS_SRC_FOLDER            "_fastload/"
#define FRONTEND_COMMON_PAK_FILENAME_SP         "modes/menucommon_sp.pak"
#define FRONTEND_COMMON_PAK_FILENAME_MP         "modes/menucommon_mp.pak"
#define FRONTEND_COMMON_LIST_FILENAME           "menucommon"
#define LEVEL_CACHE_SRC_FOLDER                  "_levelcache/"
#define LEVEL_CACHE_BIND_ROOT                   "LevelCache"
#define LEVEL_RESOURCE_LIST                     "resourcelist.txt"
#define AUTO_LEVEL_RESOURCE_LIST                "auto_resourcelist.txt"
#define AUTO_LEVEL_SEQUENCE_RESOURCE_LIST       "auto_resources_sequence.txt"
#define AUTO_LEVEL_TOTAL_RESOURCE_LIST          "auto_resourcelist_total.txt"
#define AUTO_LEVEL_TOTAL_SEQUENCE_RESOURCE_LIST "auto_resources_total_sequence.txt"

//////////////////////////////////////////////////////////////////////////
// IResourceList implementation class.
//////////////////////////////////////////////////////////////////////////
class CLevelResourceList : public IResourceList
{
public:
	CLevelResourceList()
	{
		m_pFileBuffer = 0;
		m_nBufferSize = 0;
		m_nCurrentLine = 0;
	};
	~CLevelResourceList()
	{
		Clear();
	};

	void UnifyFilename(const char* __restrict filename, char* __restrict buffer, int bufferSize)
	{
		const char* src = filename;
		char* trg = buffer;
		int len = 0;
		while (*src && len < bufferSize)
		{
			if (*src != '\\')
				*trg = tolower(*src);
			else
				*trg = '/';
			src++;
			trg++;
			len++;
		}
		*trg = 0;
	}
	void UnifyFilenameInPlace(char* filename, int bufferSize)
	{
		char* src = filename;
		int len = 0;
		while (*src && len < bufferSize)
		{
			if (*src != '\\')
				*src = tolower(*src);
			else
				*src = '/';
			src++;
			len++;
		}
	}
	uint32 GetFilenameHash(const char* sResourceFile)
	{
		char filename[512];
		UnifyFilename(sResourceFile, filename, sizeof(filename) - 1);

		uint32 code = CCrc32::ComputeLowercase(filename);
		return code;
	}

	virtual void Add(const char* sResourceFile)
	{
		assert(0); // Not implemented.
	}
	virtual void Clear()
	{
		delete[] m_pFileBuffer;
		m_pFileBuffer = 0;
		m_nBufferSize = 0;
		stl::free_container(m_lines);
		stl::free_container(m_resources_crc32);
		m_nCurrentLine = 0;
	}

	struct ComparePredicate
	{
		bool operator()(const char* s1, const char* s2) { return strcmp(s1, s2) < 0; }
	};

	virtual bool IsExist(const char* sResourceFile)
	{
		uint32 nHash = GetFilenameHash(sResourceFile);
		if (stl::binary_find(m_resources_crc32.begin(), m_resources_crc32.end(), nHash) != m_resources_crc32.end())
			return true;
		return false;
	}
	virtual bool Load(const char* sResourceListFilename)
	{
		Clear();
		CCryFile file;
		if (file.Open(sResourceListFilename, "rb", ICryPak::FOPEN_ONDISK)) // File access can happen from disk as well.
		{
			m_nBufferSize = file.GetLength();
			if (m_nBufferSize > 0)
			{
				m_pFileBuffer = new char[m_nBufferSize];
				file.ReadRaw(m_pFileBuffer, file.GetLength());
				m_pFileBuffer[m_nBufferSize - 1] = 0;

				char seps[] = "\r\n";

				m_lines.reserve(5000);

				// Parse file, every line in a file represents a resource filename.
				char* token = strtok(m_pFileBuffer, seps);
				while (token != NULL)
				{
					m_lines.push_back(token);
					token = strtok(NULL, seps);
				}

				m_resources_crc32.resize(m_lines.size());
				for (int i = 0, numlines = m_lines.size(); i < numlines; i++)
				{
					UnifyFilenameInPlace(const_cast<char*>(m_lines[i]), 512);
					m_resources_crc32[i] = CCrc32::ComputeLowercase(m_lines[i]);
				}
				std::sort(m_resources_crc32.begin(), m_resources_crc32.end());
			}
			return true;
		}
		return false;
	}
	virtual const char* GetFirst()
	{
		m_nCurrentLine = 0;
		if (!m_lines.empty())
			return m_lines[0];
		return NULL;
	}
	virtual const char* GetNext()
	{
		m_nCurrentLine++;
		if (m_nCurrentLine < (int)m_lines.size())
		{
			return m_lines[m_nCurrentLine];
		}
		return NULL;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		pSizer->Add(this, sizeof(*this));
		pSizer->Add(m_pFileBuffer, m_nBufferSize);
		pSizer->AddContainer(m_lines);
		pSizer->AddContainer(m_resources_crc32);
	}

public:
	char*               m_pFileBuffer;
	int                 m_nBufferSize;
	typedef std::vector<const char*> Lines;
	Lines               m_lines;
	int                 m_nCurrentLine;
	std::vector<uint32> m_resources_crc32;
};

//////////////////////////////////////////////////////////////////////////
CResourceManager::CResourceManager()
{
	m_bRegisteredFileOpenSink = false;
	m_bOwnResourceList = false;
	m_bLevelTransitioning = false;

	m_fastLoadPakPaths.reserve(8);
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::PrepareLevel(const char* sLevelFolder, const char* sLevelName)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Preload Level pak files");
	LOADING_TIME_PROFILE_SECTION;

	m_sLevelFolder = sLevelFolder;
	m_sLevelName = sLevelName;
	m_bLevelTransitioning = false;
	m_currentLevelCacheFolder = CryPathString(LEVEL_CACHE_SRC_FOLDER) + sLevelName;

	if (g_cvars.pakVars.nLoadCache)
	{
		CryPathString levelpak = PathUtil::Make(sLevelFolder, LEVEL_PAK_FILENAME);
		size_t nPakFileSize = gEnv->pCryPak->FGetSize(levelpak.c_str());
		if (nPakFileSize < LEVEL_PAK_INMEMORY_MAXSIZE) // 10 megs.
		{
			// Force level.pak from this level in memory.
			gEnv->pCryPak->LoadPakToMemory(LEVEL_PAK_FILENAME, ICryPak::eInMemoryPakLocale_GPU);
		}

		gEnv->pCryPak->LoadPakToMemory(ENGINE_PAK_FILENAME, ICryPak::eInMemoryPakLocale_GPU);

		//
		// Load _levelCache paks in the order they are stored on the disk - reduce seek time
		//

		if (gEnv->pConsole->GetCVar("e_StreamCgf") && gEnv->pConsole->GetCVar("e_StreamCgf")->GetIVal() != 0)
		{
			LoadLevelCachePak("cga.pak", "", true);
			LoadLevelCachePak("cgf.pak", "", true);

			if (g_cvars.pakVars.nStreamCache)
				LoadLevelCachePak("cgf_cache.pak", "", false);
		}

		LoadLevelCachePak("chr.pak", "", true);

		if (g_cvars.pakVars.nStreamCache)
		{
			LoadLevelCachePak("chr_cache.pak", "", false);
		}

		LoadLevelCachePak("dds0.pak", "", true);

		if (g_cvars.pakVars.nStreamCache)
		{
			LoadLevelCachePak("dds_cache.pak", "", false);
		}

		LoadLevelCachePak("skin.pak", "", true);

		if (g_cvars.pakVars.nStreamCache)
		{
			LoadLevelCachePak("skin_cache.pak", "", false);
		}

		LoadLevelCachePak(LEVEL_CACHE_PAK_FILENAME, "", true);
	}

	_smart_ptr<CLevelResourceList> pResList = new CLevelResourceList;
	gEnv->pCryPak->SetResourceList(ICryPak::RFOM_Level, pResList);
	m_bOwnResourceList = true;

	// Load resourcelist.txt, TODO: make sure there are no duplicates
	if (g_cvars.pakVars.nSaveLevelResourceList == 0)
	{
		string filename = PathUtil::Make(sLevelFolder, AUTO_LEVEL_RESOURCE_LIST);
		if (!pResList->Load(filename.c_str()))   // If we saving resource list do not use auto_resourcelist.txt
		{
			// Try resource list created by the editor.
			filename = PathUtil::Make(sLevelFolder, LEVEL_RESOURCE_LIST);
			pResList->Load(filename.c_str());
		}
	}
	//LoadFastLoadPaks();

	if (g_cvars.pakVars.nStreamCache)
		m_AsyncPakManager.ParseLayerPaks(GetCurrentLevelCacheFolder());
}

//////////////////////////////////////////////////////////////////////////
bool CResourceManager::LoadFastLoadPaks(bool bToMemory)
{
	if (g_cvars.pakVars.nSaveFastloadResourceList != 0)
	{
		// Record a file list for _FastLoad/startup.pak
		m_recordedFiles.clear();
		gEnv->pCryPak->RegisterFileAccessSink(this);
		m_bRegisteredFileOpenSink = true;
		return false;
	}
	else
	//if (g_cvars.pakVars.nLoadCache)
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Paks Fast Load Cache");
		LOADING_TIME_PROFILE_SECTION;

		// Load a special _fastload paks
		int nPakPreloadFlags = ICryPak::FLAGS_FILENAMES_AS_CRC32 | ICryArchive::FLAGS_OVERRIDE_PAK;
		if (bToMemory && g_cvars.pakVars.nLoadCache)
			nPakPreloadFlags |= ICryPak::FLAGS_PAK_IN_MEMORY;

		gEnv->pCryPak->OpenPacks(gEnv->pCryPak->GetGameFolder(), CryPathString(FAST_LOADING_PAKS_SRC_FOLDER) + "*.pak", nPakPreloadFlags, &m_fastLoadPakPaths);
		gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder() + "/", "Engine/ShaderCacheStartup.pak", ICryPak::FLAGS_PAK_IN_MEMORY | ICryPak::FLAGS_PATH_REAL | ICryArchive::FLAGS_OVERRIDE_PAK);
		gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder(), "Engine/Engine.pak", ICryPak::FLAGS_PAK_IN_MEMORY);

		return !m_fastLoadPakPaths.empty();
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadFastLoadPaks()
{
	for (uint32 i = 0; i < m_fastLoadPakPaths.size(); i++)
	{
		// Unload a special _fastload paks
		gEnv->pCryPak->ClosePack(m_fastLoadPakPaths[i].c_str(), ICryPak::FLAGS_PATH_REAL);
	}
	m_fastLoadPakPaths.clear();

	if (gEnv->pRenderer)
		gEnv->pRenderer->UnloadShaderStartupCache();
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadLevel()
{
	gEnv->pCryPak->SetResourceList(ICryPak::RFOM_Level, NULL);

	if (m_bRegisteredFileOpenSink)
	{
		if (g_cvars.pakVars.nSaveTotalResourceList)
		{
			SaveRecordedResources(true);
			m_recordedFiles.clear();
		}
	}

	stl::free_container(m_sLevelFolder);
	stl::free_container(m_sLevelName);
	stl::free_container(m_currentLevelCacheFolder);

	// should always be empty, since it is freed at the end of
	// the level loading process, if it is not
	// something went wrong and we have a levelheap leak
	assert(m_openedPaks.capacity() == 0);

	m_pSequenceResourceList = NULL;
}

//////////////////////////////////////////////////////////////////////////
IResourceList* CResourceManager::GetLevelResourceList()
{
	IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);
	return pResList;
}

//////////////////////////////////////////////////////////////////////////
bool CResourceManager::LoadLevelCachePak(const char* sPakName, const char* sBindRoot, bool bOnlyDuringLevelLoading)
{
	LOADING_TIME_PROFILE_SECTION;
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "LoadLevelCachePak %s", sPakName);

	CryPathString pakPath = GetCurrentLevelCacheFolder() + "/" + sPakName;

	pakPath.MakeLower();
	pakPath.replace(CCryPak::g_cNonNativeSlash, CCryPak::g_cNativeSlash);

	// Check if pak is already loaded
	for (int i = 0; i < (int)m_openedPaks.size(); i++)
	{
		if (strstr(m_openedPaks[i].filename.c_str(), pakPath.c_str()))
		{
			return true;
		}
	}

	// check pak file size.
	size_t nFileSize = gEnv->pCryPak->FGetSize(pakPath.c_str(), true);

	if (nFileSize <= 0)
	{
		// Cached file does not exist
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Level cache pak file %s does not exist", pakPath.c_str());
		return false;
	}

	//set these flags as DLC LevelCache Paks are found via the mod paths,
	//and the paks can never be inside other paks so we optimise the search
	uint32 nOpenPakFlags = ICryPak::FLAGS_FILENAMES_AS_CRC32 | ICryPak::FLAGS_CHECK_MOD_PATHS | ICryPak::FLAGS_NEVER_IN_PAK;

	if (nFileSize < LEVEL_PAK_INMEMORY_MAXSIZE) // 10 megs.
	{
		if (!(nOpenPakFlags & ICryPak::FLAGS_PAK_IN_MEMORY_CPU))
			nOpenPakFlags |= ICryPak::FLAGS_PAK_IN_MEMORY;
	}

	CryPathString pakBindRoot = PathUtil::AddSlash(CryPathString(gEnv->pCryPak->GetGameFolder()));
	pakBindRoot += sBindRoot;

	SOpenedPak op;

	if (gEnv->pCryPak->OpenPack(pakBindRoot.c_str(), pakPath.c_str(), nOpenPakFlags | ICryPak::FOPEN_HINT_QUIET, NULL, &op.filename))
	{
		op.bOnlyDuringLevelLoading = bOnlyDuringLevelLoading;
		m_openedPaks.push_back(op);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CResourceManager::LoadModeSwitchPak(const char* sPakName, const bool multiplayer)
{
	if (g_cvars.pakVars.nSaveLevelResourceList)
	{
		//Don't load the pak if we're trying to save a resourcelist in order to build it.
		m_recordedFiles.clear();
		gEnv->pCryPak->RegisterFileAccessSink(this);
		m_bRegisteredFileOpenSink = true;
		return true;
	}
	else
	{
		if (g_cvars.pakVars.nLoadModePaks)
		{
			// Unload SP common pak if switching to multiplayer
			if (multiplayer)
			{
				UnloadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_SP, FRONTEND_COMMON_LIST_FILENAME "_sp");
			}
			else
			{
				UnloadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_MP, FRONTEND_COMMON_LIST_FILENAME "_mp");
			}
			//Load the mode switching pak. If this is available and up to date it speeds up this process considerably
			bool bOpened = gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder(), sPakName, 0);
			bool bLoaded = gEnv->pCryPak->LoadPakToMemory(sPakName, ICryPak::eInMemoryPakLocale_GPU);
			return (bOpened && bLoaded);
		}
		else
		{
			return true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadModeSwitchPak(const char* sPakName, const char* sResourceListName, const bool multiplayer)
{
	if (g_cvars.pakVars.nSaveLevelResourceList && m_bRegisteredFileOpenSink)
	{
		m_sLevelFolder = sResourceListName;
		SaveRecordedResources();
		gEnv->pCryPak->UnregisterFileAccessSink(this);
		m_bRegisteredFileOpenSink = false;
	}
	else
	{
		if (g_cvars.pakVars.nLoadModePaks)
		{
			//Unload the mode switching pak.
			gEnv->pCryPak->LoadPakToMemory(sPakName, ICryPak::eInMemoryPakLocale_Unload);
			gEnv->pCryPak->ClosePack(sPakName, 0);
			//Load the frontend common mode switch pak, this can considerably reduce the time spent switching especially from disc, currently SP only
			if (!multiplayer && LoadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_SP) == false)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load %s during init. This file can significantly reduce frontend loading times.\n", FRONTEND_COMMON_PAK_FILENAME_SP);
			}
			else if (multiplayer && LoadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_MP) == false)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load %s during init. This file can significantly reduce frontend loading times.\n", FRONTEND_COMMON_PAK_FILENAME_MP);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CResourceManager::LoadMenuCommonPak(const char* sPakName)
{
	if (g_cvars.pakVars.nSaveMenuCommonResourceList)
	{
		//Don't load the pak if we're trying to save a resourcelist in order to build it.
		m_recordedFiles.clear();
		gEnv->pCryPak->RegisterFileAccessSink(this);
		m_bRegisteredFileOpenSink = true;
		return true;
	}
	else
	{
		//Load the mode switching pak. If this is available and up to date it speeds up this process considerably
		bool bOpened = gEnv->pCryPak->OpenPack(PathUtil::GetGameFolder(), sPakName, 0);
		bool bLoaded = gEnv->pCryPak->LoadPakToMemory(sPakName, ICryPak::eInMemoryPakLocale_GPU);
		return (bOpened && bLoaded);
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadMenuCommonPak(const char* sPakName, const char* sResourceListName)
{
	if (g_cvars.pakVars.nSaveMenuCommonResourceList)
	{
		m_sLevelFolder = sResourceListName;
		SaveRecordedResources();
		gEnv->pCryPak->UnregisterFileAccessSink(this);
		m_bRegisteredFileOpenSink = false;
	}
	else
	{
		//Unload the mode switching pak.
		gEnv->pCryPak->LoadPakToMemory(sPakName, ICryPak::eInMemoryPakLocale_Unload);
		gEnv->pCryPak->ClosePack(sPakName, 0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadLevelCachePak(const char* sPakName)
{
	LOADING_TIME_PROFILE_SECTION;
	CryPathString pakPath = GetCurrentLevelCacheFolder() + "/" + sPakName;
	pakPath.MakeLower();
	pakPath.replace(CCryPak::g_cNonNativeSlash, CCryPak::g_cNativeSlash);

	for (int i = 0; i < (int)m_openedPaks.size(); i++)
	{
		if (strstr(m_openedPaks[i].filename.c_str(), pakPath.c_str()))
		{
			gEnv->pCryPak->ClosePack(m_openedPaks[i].filename.c_str(), ICryPak::FLAGS_PATH_REAL);
			m_openedPaks.erase(m_openedPaks.begin() + i);
			break;
		}
	}

	if (m_openedPaks.empty())
		stl::free_container(m_openedPaks);
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::UnloadAllLevelCachePaks(bool bLevelLoadEnd)
{
	LOADING_TIME_PROFILE_SECTION;

	if (!bLevelLoadEnd)
	{
		m_AsyncPakManager.Clear();
		UnloadFastLoadPaks();
	}
	else
	{
		m_AsyncPakManager.UnloadLevelLoadPaks();
	}

	uint32 nClosePakFlags = ICryPak::FLAGS_PATH_REAL; //ICryPak::FLAGS_CHECK_MOD_PATHS | ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL;

	for (int i = 0; i < (int)m_openedPaks.size(); i++)
	{
		if ((m_openedPaks[i].bOnlyDuringLevelLoading && bLevelLoadEnd) ||
		    !bLevelLoadEnd)
		{
			gEnv->pCryPak->ClosePack(m_openedPaks[i].filename.c_str(), nClosePakFlags);
		}
	}

	if (g_cvars.pakVars.nLoadCache)
	{
		gEnv->pCryPak->LoadPakToMemory(ENGINE_PAK_FILENAME, ICryPak::eInMemoryPakLocale_Unload);

		// Force level.pak out of memory.
		gEnv->pCryPak->LoadPakToMemory(LEVEL_PAK_FILENAME, ICryPak::eInMemoryPakLocale_Unload);
	}
	if (!bLevelLoadEnd)
		stl::free_container(m_openedPaks);
}

//////////////////////////////////////////////////////////////////////////

bool CResourceManager::LoadPakToMemAsync(const char* pPath, bool bLevelLoadOnly)
{
	return m_AsyncPakManager.LoadPakToMemAsync(pPath, bLevelLoadOnly);
}

bool CResourceManager::LoadLayerPak(const char* sLayerName)
{
	return m_AsyncPakManager.LoadLayerPak(sLayerName);
}

void CResourceManager::UnloadLayerPak(const char* sLayerName)
{
	m_AsyncPakManager.UnloadLayerPak(sLayerName);
}

void CResourceManager::GetLayerPakStats(SLayerPakStats& stats, bool bCollectAllStats) const
{
	m_AsyncPakManager.GetLayerPakStats(stats, bCollectAllStats);
}

void CResourceManager::UnloadAllAsyncPaks()
{
	m_AsyncPakManager.Clear();
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::Update()
{
	m_AsyncPakManager.Update();
}
//////////////////////////////////////////////////////////////////////////
void CResourceManager::Init()
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this,"CResourceManager");
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::Shutdown()
{
	UnloadAllLevelCachePaks(false);
	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

//////////////////////////////////////////////////////////////////////////
bool CResourceManager::IsStreamingCachePak(const char* filename) const
{
	const char* cachePaks[] = {
		"dds_cache.pak",
		"cgf_cache.pak",
		"skin_cache.pak",
		"chr_cache.pak"
	};

	for (int i = 0; i < CRY_ARRAY_COUNT(cachePaks); ++i)
	{
		if (strstr(filename, cachePaks[i]))
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_FRONTEND_INITIALISED:
		{
			GetISystem()->GetStreamEngine()->PauseStreaming(false, -1);
		}
		break;

	case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
		{
			if (g_cvars.pakVars.nSaveFastloadResourceList != 0)
			{
				SaveRecordedResources();

				if (g_cvars.pakVars.nSaveLevelResourceList == 0 && g_cvars.pakVars.nSaveTotalResourceList == 0)
					m_recordedFiles.clear();
			}
			// Unload all paks from memory, after game init.
			UnloadAllLevelCachePaks(false);
			gEnv->pCryPak->LoadPaksToMemory(0, false);

			if (g_cvars.pakVars.nLoadCache)
			{
				//Load the frontend common mode switch pak, this can considerably reduce the time spent switching especially from disc
				if (!gEnv->bMultiplayer && LoadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_SP) == false)
				{
					CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not load %s during init. This file can significantly reduce frontend loading times.\n", FRONTEND_COMMON_PAK_FILENAME_SP);
				}
			}

			//Load and precache frontend shader cache
			if (g_cvars.pakVars.nLoadFrontendShaderCache)
			{
				gEnv->pRenderer->LoadShaderLevelCache();
				gEnv->pRenderer->EF_Query(EFQ_SetShaderCombinations);
			}

			break;
		}

	case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
		{
			if (g_cvars.pakVars.nLoadFrontendShaderCache)
			{
				gEnv->pRenderer->UnloadShaderLevelCache();
			}

			if (!gEnv->bMultiplayer)
			{
				UnloadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_SP, FRONTEND_COMMON_LIST_FILENAME "_sp");
			}
			else
			{
				UnloadMenuCommonPak(FRONTEND_COMMON_PAK_FILENAME_MP, FRONTEND_COMMON_LIST_FILENAME "_mp");
			}

			m_bLevelTransitioning = !m_sLevelName.empty();

			m_lastLevelLoadTime.SetValue(0);
			m_beginLevelLoadTime = gEnv->pTimer->GetAsyncTime();
			if (g_cvars.pakVars.nSaveLevelResourceList || g_cvars.pakVars.nSaveTotalResourceList)
			{
				if (!g_cvars.pakVars.nSaveTotalResourceList)
					m_recordedFiles.clear();

				if (!m_bRegisteredFileOpenSink)
				{
					gEnv->pCryPak->RegisterFileAccessSink(this);
					m_bRegisteredFileOpenSink = true;
				}
			}

			// Cancel any async pak loading, it will fight with the impending sync IO
			m_AsyncPakManager.CancelPendingJobs();

			// Pause streaming engine for anything but audio, video and flash.
			uint32 nMask = (1 << eStreamTaskTypeFlash) | (1 << eStreamTaskTypeVideo) | STREAM_TASK_TYPE_AUDIO_ALL; // Unblock specified streams
			nMask = ~nMask;                                                                                        // Invert mask, bit set means blocking type.
			GetISystem()->GetStreamEngine()->PauseStreaming(true, nMask);
		}
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		{
			if (m_bOwnResourceList)
			{
				m_bOwnResourceList = false;
				// Clear resource list, after level loading.
				IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);
				if (pResList)
				{
					pResList->Clear();
				}
			}
		}

		break;

	case ESYSTEM_EVENT_LEVEL_UNLOAD:
		UnloadAllLevelCachePaks(false);

		break;

	case ESYSTEM_EVENT_LEVEL_PRECACHE_START:
		{
			// Unpause all streams in streaming engine.
			GetISystem()->GetStreamEngine()->PauseStreaming(false, -1);
		}
		break;

	case ESYSTEM_EVENT_LEVEL_PRECACHE_FIRST_FRAME:
		{
			UnloadAllLevelCachePaks(true);
		}
		break;

	case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
		{
			CTimeValue t = gEnv->pTimer->GetAsyncTime();
			m_lastLevelLoadTime = t - m_beginLevelLoadTime;

			if (g_cvars.pakVars.nSaveLevelResourceList && m_bRegisteredFileOpenSink)
			{
				SaveRecordedResources();

				if (!g_cvars.pakVars.nSaveTotalResourceList)
				{
					gEnv->pCryPak->UnregisterFileAccessSink(this);
					m_bRegisteredFileOpenSink = false;
				}
			}

			UnloadAllLevelCachePaks(true);
		}

#if CAPTURE_REPLAY_LOG
		static int s_loadCount = 0;
		CryGetIMemReplay()->AddLabelFmt("precacheEnd%d_%s", s_loadCount++, m_sLevelName.c_str());
#endif

		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::GetMemoryStatistics(ICrySizer* pSizer)
{
	IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);

	pSizer->AddContainer(m_openedPaks);
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::ReportFileOpen(FILE* in, const char* szFullPath)
{
	if (!g_cvars.pakVars.nSaveLevelResourceList && !g_cvars.pakVars.nSaveFastloadResourceList && !g_cvars.pakVars.nSaveMenuCommonResourceList && !g_cvars.pakVars.nSaveTotalResourceList)
		return;

	string file = PathUtil::MakeGamePath(string(szFullPath));
	file.replace('\\', '/');
	file.MakeLower();
	{
		CryAutoCriticalSection lock(recordedFilesLock);
		m_recordedFiles.push_back(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CResourceManager::SaveRecordedResources(bool bTotalList)
{
	CryAutoCriticalSection lock(recordedFilesLock);

	std::set<string> fileset;

	// eliminate duplicate values
	std::vector<string>::iterator endLocation = std::unique(m_recordedFiles.begin(), m_recordedFiles.end());
	m_recordedFiles.erase(endLocation, m_recordedFiles.end());

	fileset.insert(m_recordedFiles.begin(), m_recordedFiles.end());

	string sSequenceFilename = PathUtil::AddSlash(m_sLevelFolder) + (bTotalList ? AUTO_LEVEL_TOTAL_SEQUENCE_RESOURCE_LIST : AUTO_LEVEL_SEQUENCE_RESOURCE_LIST);
	{
		FILE* file = fxopen(sSequenceFilename, "wb", true);
		if (file)
		{
			for (std::vector<string>::iterator it = m_recordedFiles.begin(); it != m_recordedFiles.end(); ++it)
			{
				const char* str = it->c_str();
				fprintf(file, "%s\n", str);
			}
			fclose(file);
		}
	}

	string sResourceSetFilename = PathUtil::AddSlash(m_sLevelFolder) + (bTotalList ? AUTO_LEVEL_TOTAL_RESOURCE_LIST : AUTO_LEVEL_RESOURCE_LIST);
	{
		FILE* file = fxopen(sResourceSetFilename, "wb", true);
		if (file)
		{
			for (std::set<string>::iterator it = fileset.begin(); it != fileset.end(); ++it)
			{
				const char* str = it->c_str();
				fprintf(file, "%s\n", str);
			}
			fclose(file);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CTimeValue CResourceManager::GetLastLevelLoadTime() const
{
	return m_lastLevelLoadTime;
}
