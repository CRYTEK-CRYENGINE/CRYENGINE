// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Cache for front end models.

// Includes
#include "StdAfx.h"
#include "FrontEndModelCache.h"
#include "GameCVars.h"
#include "ItemSharedParams.h"
#include "GameXmlParamReader.h"
#include "MenuRender3DModelMgr.h"
#include "UI/UIManager.h"

// Defines
#define	FEMC_CACHE_LIST_FILENAME			"scripts/gamerules/frontendmodelcache.xml"
#define FEMC_FILE_ACCESS_LOG					0

#ifndef _RELEASE
	#define FEMC_LOG_CACHE_TIME					1
#else
	#define FEMC_LOG_CACHE_TIME					0
#endif

// Statics
CFrontEndModelCache*		CFrontEndModelCache::s_pSingletonInstance = NULL;
bool										CFrontEndModelCache::s_bAllowedToLoad3dFrontEndAssets = true;
bool										CFrontEndModelCache::s_bNeed3dFrontEndAssets = false;

//--------------------------------------------------------------------------------------------------
// Name: CFrontEndModelCache
// Desc: Constructor
//--------------------------------------------------------------------------------------------------
CFrontEndModelCache::CFrontEndModelCache()
{
	FE_LOG("Front End model cache creation");
	INDENT_LOG_DURING_SCOPE();

	IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
	if(pGameFramework)
	{
		pGameFramework->StartNetworkStallTicker(true);
	}

	m_bIsMultiplayerCache = gEnv->bMultiplayer;

#if FEMC_FILE_ACCESS_LOG
	gEnv->pConsole->GetCVar("sys_FileAccessLog")->Set(1);
#endif // #if FEMC_FILE_ACCESS_LOG

#ifdef FEMC_LOG_CACHE_TIME
	const float startTime = gEnv->pTimer->GetAsyncCurTime();
#endif // #ifdef FEMC_LOG_CACHE_TIME

#if FEMC_CACHE_FILE_ACCESSES
	if (g_pGameCVars->g_FEMenuCacheSaveList)
	{
		gEnv->pCryPak->RegisterFileAccessSink(this);
		m_recordedFiles.clear();
	}
	int oldSaveLevelResourceList = 0;
	ICVar* pPakSaveLevelResourceListCvar = gEnv->pConsole->GetCVar("sys_PakSaveLevelResourceList");
	if(pPakSaveLevelResourceListCvar)
	{
		oldSaveLevelResourceList = pPakSaveLevelResourceListCvar->GetIVal();
		pPakSaveLevelResourceListCvar->Set(0);
	}
	m_pReasonForReportingFileOpen = "CFrontEndModelCache constructor";
#endif // #if FEMC_CACHE_FILE_ACCESSES

	CRY_ASSERT(s_pSingletonInstance == NULL);
	s_pSingletonInstance = this;

	// Load model list from xml, and cache each entry
	XmlNodeRef pRootNode = gEnv->pSystem->LoadXmlFromFile(FEMC_CACHE_LIST_FILENAME);
	if(pRootNode)
	{
		CGameXmlParamReader xmlParamReader(pRootNode);
		const char* pPakName = NULL;

		// Pak
		const char* const pGameFolder = gEnv->pCryPak->GetGameFolder();
		const XmlNodeRef pPakData = xmlParamReader.FindFilteredChild("PakData");
		if(pPakData && pPakData->getChildCount())
		{
			const XmlNodeRef pPak = pPakData->getChild(0);
			if(pPak)
			{
				pPakName = pPak->getAttr("name");
				bool bSucceeded = gEnv->pCryPak->OpenPack(pGameFolder,pPakName,ICryPak::FLAGS_FILENAMES_AS_CRC32);
				bSucceeded |= gEnv->pCryPak->LoadPakToMemory(pPakName,ICryPak::eInMemoryPakLocale_GPU);
				FE_LOG ("%s to open pack file '%s' bound to %s", bSucceeded ? "Managed" : "Failed", pPakName, pGameFolder);
			}

			// There is a pak, so reserve space for some materials in cache
			const uint materialCacheReserve = 64;
			m_materialCache.reserve(materialCacheReserve);
		}

		// Cache character models
		const XmlNodeRef pCharacterModelList = xmlParamReader.FindFilteredChild("CharacterModels");
		if(pCharacterModelList)
		{
			const int characterModelCount = pCharacterModelList->getChildCount();
			if(characterModelCount)
			{
				CreateSupportForFrontEnd3dModels();
			}

			for(int i=0; i<characterModelCount; i++)
			{
				const XmlNodeRef pCharacterModel = pCharacterModelList->getChild(i);
				if(pCharacterModel)
				{
					const char* pCharacterModelName = pCharacterModel->getAttr("name");
					CacheCharacterModel(pCharacterModelName);
				}
			}
		}

		// Cache item models
		const XmlNodeRef pItemModelsList = xmlParamReader.FindFilteredChild("ItemModels");
		if(pItemModelsList)
		{
			const int itemModelCount = pItemModelsList->getChildCount();
			if(itemModelCount)
			{
				CreateSupportForFrontEnd3dModels();
			}

			for(int i=0; i<itemModelCount; i++)
			{
				const XmlNodeRef pItemModel = pItemModelsList->getChild(i);
				if(pItemModel)
				{
					const char* pItemModelName = pItemModel->getAttr("name");
					if (strcmp(pItemModel->getTag(), "GeometryModels") == 0)
					{
						m_myGeometryCache.CacheGeometry(pItemModelName, false, IStatObj::ELoadingFlagsIgnoreLoDs);
					}
					else if (strcmp(pItemModel->getTag(), "ItemModel") == 0)
					{
						CacheItemModel(pItemModelName);
					}
				}
			}
		}

		// Unload pak
		if(pPakName)
		{
			gEnv->pCryPak->LoadPakToMemory( pPakName,ICryPak::eInMemoryPakLocale_Unload );
			bool bSucceeded = gEnv->pCryPak->ClosePack( pPakName,0 );
			FE_LOG ("%s to close pack file '%s'", bSucceeded ? "Managed" : "Failed", pPakName);
		}
	}

#if FEMC_FILE_ACCESS_LOG
	gEnv->pConsole->GetCVar("sys_FileAccessLog")->Set(0);
#endif // #if FEMC_FILE_ACCESS_LOG
#ifdef FEMC_LOG_CACHE_TIME
	const float endTime = gEnv->pTimer->GetAsyncCurTime();
	const float deltaTime = endTime - startTime;
	FE_LOG("FrontEndModelCache loading took %3.1f seconds", deltaTime);
#endif // #FEMC_LOG_CACHE_TIME FE_MODEL_CACHE_LOG_CACHE_TIME
#if FEMC_CACHE_FILE_ACCESSES
	m_pReasonForReportingFileOpen = NULL;

	if (g_pGameCVars->g_FEMenuCacheSaveList)
	{
		// To stop any other threads from messing
		gEnv->pCryPak->UnregisterFileAccessSink(this);

		std::set<string> fileset;
		// eliminate duplicate values
		std::vector<string>::iterator endLocation = std::unique( m_recordedFiles.begin(),m_recordedFiles.end() );
		m_recordedFiles.erase( endLocation,m_recordedFiles.end() );

		fileset.insert( m_recordedFiles.begin(),m_recordedFiles.end() );

		string sResourceSetFilename = PathUtil::AddSlash("Levels/Multiplayer") + "mpmenu_list.txt";
		{
			FILE* pFile = fxopen(sResourceSetFilename,"wb",true);
			if(pFile)
			{
				for(std::set<string>::iterator it = fileset.begin(); it != fileset.end(); ++it)
				{
					const char *pStr = it->c_str();
					fprintf( pFile,"%s\n",pStr );
					// Automatically add cgf->cgfm, cga->cgam, dds->dds.0
					const char* const pExt = PathUtil::GetExt(pStr);
					if (strcmp(pExt, "cgf") == 0)
					{
						fprintf( pFile,"%sm\n",pStr );
					}
					else if (strcmp(pExt, "cga") == 0)
					{
						fprintf( pFile,"%sm\n",pStr );
					}
					else if (strcmp(pExt, "dds") == 0)
					{
						fprintf( pFile,"%s.0\n",pStr );
					}
				}
				fclose(pFile);
			}
		}
	}
	if(pPakSaveLevelResourceListCvar)
	{
		pPakSaveLevelResourceListCvar->Set(oldSaveLevelResourceList);
	}
#endif // #if FEMC_CACHE_FILE_ACCESSES

	if(pGameFramework)
	{
		pGameFramework->StopNetworkStallTicker();
	}

	FE_LOG("Done caching items for front end");
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ~CFrontEndModelCache
// Desc: Destructor
//--------------------------------------------------------------------------------------------------
CFrontEndModelCache::~CFrontEndModelCache()
{
	FE_LOG("Front End model cache deletion");
	INDENT_LOG_DURING_SCOPE();

	CRY_ASSERT(s_pSingletonInstance == this);
	s_pSingletonInstance = NULL;

	m_myGeometryCache.FlushCaches();
	m_materialCache.clear();

	ReleaseSupportForFrontEnd3dModels();
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CacheCharacterModel
// Desc: Cache character model
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::CacheCharacterModel(const char* pModelFileName)
{
	FE_LOG("Caching \"%s\"", pModelFileName);
	INDENT_LOG_DURING_SCOPE();
	m_myGeometryCache.CacheGeometry(pModelFileName, false, IStatObj::ELoadingFlagsIgnoreLoDs);
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CacheItemModel
// Desc: Cache item model
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::CacheItemModel(const char* pItemName)
{
	CItemSharedParams* pItemSharedParams = g_pGame->GetGameSharedParametersStorage()->GetItemSharedParameters(pItemName, false);
	if(pItemSharedParams)
	{
		const SGeometryDef* pGeomDef = pItemSharedParams->GetGeometryForSlot(eIGS_ThirdPerson);

		if(pGeomDef)
		{
			const char* pModelFileName = pGeomDef->modelPath.c_str();
			FE_LOG ("Caching \"%s\" (for item '%s')", pModelFileName, pItemName);
			INDENT_LOG_DURING_SCOPE();
			m_myGeometryCache.CacheGeometry(pModelFileName, false, IStatObj::ELoadingFlagsIgnoreLoDs);

			const char *pMaterialName = pGeomDef->material.c_str();
			FE_LOG("  caching material '%s'", pMaterialName);
			IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(pMaterialName);
			if(pMaterial)
			{
				m_materialCache.push_back(pMaterial);
				pMaterial->RequestTexturesLoading(0.0f);
			}
		}
	}
	else
	{
		GameWarning("Failed to find shared parameters for item '%s' - can't cache model", pItemName);
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReportFileOpen
// Desc: Reports file open
//--------------------------------------------------------------------------------------------------
#if FEMC_CACHE_FILE_ACCESSES
void CFrontEndModelCache::ReportFileOpen( FILE *pFile, const char *pFullPath )
{
	if(m_pReasonForReportingFileOpen)
	{
		FE_LOG ("[CFrontEndModelCache] <%s> Opening file '%s'", m_pReasonForReportingFileOpen, pFullPath);
	}

	if(g_pGameCVars->g_FEMenuCacheSaveList)
	{
		string file = PathUtil::MakeGamePath( string(pFullPath) );
		file.replace('\\','/');
		file.MakeLower();
		{
			m_recordedFiles.push_back(file);
		}
	}
}//-------------------------------------------------------------------------------------------------
#endif // #if FEMC_CACHE_FILE_ACCESSES

//--------------------------------------------------------------------------------------------------
// Name: Allow3dFrontEndAssets
// Desc: Allows 3d front end assets
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::Allow3dFrontEndAssets(bool bAllowed, bool bUpdateImmediate)
{
	if(bAllowed != s_bAllowedToLoad3dFrontEndAssets)
	{
		FE_LOG("Setting 'allowed to load 3d assets' value from %s", bAllowed ? "FALSE to TRUE" : "TRUE to FALSE");
		s_bAllowedToLoad3dFrontEndAssets = bAllowed;

		if(bUpdateImmediate)
		{
			UpdateNeed3dFrontEndAssets();
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: IsAllowed3dFrontEndAssets
// Desc: Returns true if 3d front end assets are allowed
//--------------------------------------------------------------------------------------------------
bool CFrontEndModelCache::IsAllowed3dFrontEndAssets()
{
	if(!g_pGame->IsGameActive())
	{
		return s_bAllowedToLoad3dFrontEndAssets;
	}
	else
	{
		return true;
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: UpdateNeed3dFrontEndAssets
// Desc: Updates need 3d front end assets
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::UpdateNeed3dFrontEndAssets()
{
	if(s_bAllowedToLoad3dFrontEndAssets != s_bNeed3dFrontEndAssets)
	{
		s_bNeed3dFrontEndAssets = s_bAllowedToLoad3dFrontEndAssets;

		if(s_bAllowedToLoad3dFrontEndAssets)
		{
			CFrontEndModelCache::Create();
		}
		else
		{
			CFrontEndModelCache::Release();
		}
	}
	else
	{
		// Swap between mp and sp paks if needed
		if(CFrontEndModelCache::s_pSingletonInstance)
		{
			if(CFrontEndModelCache::s_pSingletonInstance->m_bIsMultiplayerCache != gEnv->bMultiplayer)
			{
				CFrontEndModelCache::Release();
				CFrontEndModelCache::Create();
			}
		}
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: CreateSupportForFrontEnd3dModels
// Desc: Creates support for front end 3d models
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::CreateSupportForFrontEnd3dModels()
{
	if(CMenuRender3DModelMgr::IsMenu3dModelEngineSupportActive() == false)
	{
		FE_LOG ("Enabling support for rendering 3D models in the front end");
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_3D_POST_RENDERING_START,0,0 );
	}
}//-------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Name: ReleaseSupportForFrontEnd3dModels
// Desc: Releases support for front end 3d models
//--------------------------------------------------------------------------------------------------
void CFrontEndModelCache::ReleaseSupportForFrontEnd3dModels()
{
	FE_LOG ("Disabling support for rendering 3D models in the front end");
	CMenuRender3DModelMgr::Release(true);

	if(CMenuRender3DModelMgr::IsMenu3dModelEngineSupportActive())
	{
		gEnv->pCharacterManager->ClearResources(false);
		gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent( ESYSTEM_EVENT_3D_POST_RENDERING_END,0,0 );
	}
}//-------------------------------------------------------------------------------------------------
