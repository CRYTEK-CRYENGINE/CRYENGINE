// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjman.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Loading trees, buildings, ragister/unregister entities for rendering
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "ObjMan.h"
#include "VisAreas.h"
#include "terrain_sector.h"
#include "PolygonClipContext.h"
#include "3dEngine.h"
#include "IndexedMesh.h"
#include "Brush.h"
#include "Vegetation.h"
#include "terrain.h"
#include "ObjectsTree.h"
#include <CrySystem/File/IResourceManager.h>
#include "DecalRenderNode.h"

#define BRUSH_LIST_FILE     "brushlist.txt"
#define CGF_LEVEL_CACHE_PAK "cgf.pak"

//Platform specific includes MemoryBarrier
#if defined(CRY_PLATFORM_WINDOWS) || defined(CRY_PLATFORM_DURANGO)
	#include <CryCore/Platform/CryWindows.h>
#endif

//////////////////////////////////////////////////////////////////////////
IStatObj* CObjManager::GetStaticObjectByTypeID(int nTypeID)
{
	if (nTypeID >= 0 && nTypeID < m_lstStaticTypes.Count())
		return m_lstStaticTypes[nTypeID].pStatObj;

	return 0;
}

IStatObj* CObjManager::FindStaticObjectByFilename(const char* filename)
{
	return stl::find_in_map(m_nameToObjectMap, CONST_TEMP_STRING(filename), NULL);
}

void CObjManager::UnloadVegetationModels(bool bDeleteAll)
{
	PodArray<StatInstGroup>& rGroupTable = m_lstStaticTypes;
	for (uint32 nGroupId = 0; nGroupId < rGroupTable.size(); nGroupId++)
	{
		StatInstGroup& rGroup = rGroupTable[nGroupId];

		rGroup.pStatObj = NULL;
		rGroup.pMaterial = NULL;

		for (int j = 0; j < FAR_TEX_COUNT; ++j)
		{
			SVegetationSpriteLightInfo& rLightInfo = rGroup.m_arrSSpriteLightInfo[j];
			SAFE_RELEASE(rLightInfo.m_pDynTexture);
		}
	}

	if (bDeleteAll)
		rGroupTable.Free();
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::UnloadObjects(bool bDeleteAll)
{
	UnloadVegetationModels(bDeleteAll);
	UnloadFarObjects();

	CleanStreamingData();

	m_pRMBox = 0;

	m_decalsToPrecreate.resize(0);

	// Clear all objects that are in the garbage collector.
	ClearStatObjGarbage();

	stl::free_container(m_checkForGarbage);
	m_bGarbageCollectionEnabled = false;

	if (bDeleteAll)
	{
		m_lockedObjects.clear(); // Lock/Unlock resources will not work with this.

		// Release default stat obj.
		m_pDefaultCGF = 0;

		m_nameToObjectMap.clear();
		m_lstLoadedObjects.clear();

#if !defined(_RELEASE)
		int nNumLeaks = 0;
#endif //_RELEASE

		std::vector<CStatObj*> garbage;
		for (CStatObj* pStatObj = CStatObj::get_intrusive_list_root(); pStatObj; pStatObj = pStatObj->get_next_intrusive())
		{
			garbage.push_back(pStatObj);

#if !defined(_RELEASE)
			if (!pStatObj->IsDefaultObject())
			{
				nNumLeaks++;
				Warning("StatObj not deleted: %s (%s)  RefCount: %d", pStatObj->m_szFileName.c_str(), pStatObj->m_szGeomName.c_str(), pStatObj->m_nUsers);
			}
#endif //_RELEASE
		}

#ifndef _RELEASE
		// deleting leaked objects
		if (nNumLeaks > 0)
		{
			Warning("CObjManager::CheckObjectLeaks: %d object(s) found in memory", nNumLeaks);
		}
#endif //_RELEASE

		for (int i = 0, num = (int)garbage.size(); i < num; i++)
		{
			CStatObj* pStatObj = garbage[i];
			pStatObj->ShutDown();
		}
		for (int i = 0, num = (int)garbage.size(); i < num; i++)
		{
			CStatObj* pStatObj = garbage[i];
			delete pStatObj;
		}

#ifdef POOL_STATOBJ_ALLOCS
		assert(m_statObjPool->GetTotalMemory().nUsed == 0);
#endif
	}
	m_bGarbageCollectionEnabled = true;

#ifdef POOL_STATOBJ_ALLOCS
	m_statObjPool->FreeMemoryIfEmpty();
#endif

	stl::free_container(m_lstTmpCastingNodes);
	stl::free_container(m_decalsToPrecreate);
	stl::free_container(m_tmpAreas0);
	stl::free_container(m_tmpAreas1);
	for (size_t rl = 0; rl < MAX_RECURSION_LEVELS; ++rl)
	{
		for (size_t ti = 0; ti < nThreadsNum; ++ti)
		{
			m_arrVegetationSprites[rl][ti].reset_container();
		}
	}

	m_lstStaticTypes.Free();
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::CleanStreamingData()
{
	stl::free_container(m_arrStreamingNodeStack);

	stl::free_container(m_arrStreamableToRelease);
	stl::free_container(m_arrStreamableToLoad);
	stl::free_container(m_arrStreamableToDelete);
}

//////////////////////////////////////////////////////////////////////////
// Preload in efficient way all CGF's used in level
//////////////////////////////////////////////////////////////////////////

class CObjManager::CPreloadTimeslicer : public IStatObjFoundCallback
{
public:
	enum class EStep
	{
		Init,
		BrushListLoad,
		ResourceListStream,
		Finish,

		// Results
		Done,
		Failed,
		Count
	};

	CPreloadTimeslicer(CObjManager& owner)
		: m_owner(owner)
	{}

	I3DEngine::ELevelLoadStatus DoStep(const float fTimeslicingLimitSec);

	void OnFound(CStatObj* pObject, IStatObj::SSubObject* pSubObject) override
	{
		if (pObject && pObject->m_bMeshStrippedCGF)
			m_inLevelCacheCount++;
	}

private:

	I3DEngine::ELevelLoadStatus SetInProgress(EStep nextStep)
	{
		m_currentStep = nextStep;
		return I3DEngine::ELevelLoadStatus::InProgress;
	}

	I3DEngine::ELevelLoadStatus SetDone()
	{
		m_currentStep = EStep::Done;
		return I3DEngine::ELevelLoadStatus::Done;
	}

	I3DEngine::ELevelLoadStatus SetFailed()
	{
		m_currentStep = EStep::Failed;
		return I3DEngine::ELevelLoadStatus::Failed;
	}

	// inputs
	CObjManager& m_owner;

	// state support
	EStep m_currentStep = EStep::Init;

	// intermediate
	float m_startTime = 0;
	bool m_isCgfCacheExist = false;
	int m_loadedCgfCounter = 0;
	int m_inLevelCacheCount = 0;
	bool m_isVerboseLogging = GetCVars()->e_StatObjPreload > 1;
};

I3DEngine::ELevelLoadStatus CObjManager::CPreloadTimeslicer::DoStep(const float timeSlicingLimitSec)
{
#define NEXT_STEP(step) return SetInProgress(step); case step: 

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	INDENT_LOG_DURING_SCOPE();

	switch (m_currentStep)
	{
		case EStep::Init:
		{
			// Starting a new level, so make sure the round ids are ahead of what they were in the last level
			m_owner.m_nUpdateStreamingPrioriryRoundId += 8;
			m_owner.m_nUpdateStreamingPrioriryRoundIdFast += 8;

			m_owner.PrintMessage("Starting loading level CGF's ...");


			m_startTime = GetCurAsyncTimeSec();

			m_isCgfCacheExist = false;
			if (GetCVars()->e_StreamCgf != 0)
			{
				// Only when streaming enable use no-mesh cgf pak.
				//m_bCgfCacheExist = GetISystem()->GetIResourceManager()->LoadLevelCachePak( CGF_LEVEL_CACHE_PAK,"" );
			}

			m_loadedCgfCounter = 0;
			m_inLevelCacheCount = 0;

			m_isVerboseLogging = GetCVars()->e_StatObjPreload > 1;
		}

		NEXT_STEP(EStep::BrushListLoad)
		{
			//////////////////////////////////////////////////////////////////////////
			// Start loading all .CGF inside level from the "brushlist.txt" file.
			{
				const char* brushListFilename = Get3DEngine()->GetLevelFilePath(BRUSH_LIST_FILE);
				CCryFile file;
				if (file.Open(brushListFilename, "rb") && file.GetLength() > 0)
				{
					const size_t fileLength = file.GetLength();

					std::unique_ptr<char[]> brushListBuffer(new char[fileLength + 1]);

					brushListBuffer[fileLength] = 0; // Null terminate
					file.ReadRaw(brushListBuffer.get(), fileLength);

					// Parse file, every line in a file represents a resource filename.
					const char* seps = "\r\n";
					char* token = strtok(brushListBuffer.get(), seps);
					while (token != NULL)
					{
						const char* szCgfFileName;
						const int nAliasLen = sizeof("%level%") - 1;
						if (strncmp(token, "%level%", nAliasLen) == 0)
							szCgfFileName = Get3DEngine()->GetLevelFilePath(token + nAliasLen);
						else
							szCgfFileName = token;

						if (m_isVerboseLogging)
						{
							CryLog("%s", szCgfFileName);
						}

						GetObjManager()->LoadStatObjAsync(szCgfFileName, NULL, false, 0);
						m_loadedCgfCounter++;
						token = strtok(NULL, seps);
					}
				}
			}
		}

		NEXT_STEP(EStep::ResourceListStream)
		{
			IResourceList* pResList = GetISystem()->GetIResourceManager()->GetLevelResourceList();

			// Request objects loading from Streaming System.
			for(const char* szResFileName = pResList->GetFirst(); szResFileName != nullptr; szResFileName = pResList->GetNext())
			{
				const char* szFileExt = PathUtil::GetExt(szResFileName);
				const bool isCgf = cry_stricmp(szFileExt, CRY_GEOMETRY_FILE_EXT) == 0;

				if (isCgf)
				{
					const char* sLodName = strstr(szResFileName, "_lod");
					if (sLodName && (sLodName[4] >= '0' && sLodName[4] <= '9'))
					{
						// Ignore Lod files.
						continue;
					}

					if (m_isVerboseLogging)
					{
						CryLog("%s", szResFileName);
					}

					GetObjManager()->LoadStatObjAsync(szResFileName, NULL, true, 0, this);
					m_loadedCgfCounter++;
				}
			}
		}

		NEXT_STEP(EStep::Finish)
		{
			// wait for the async loads to finish
			GetSystem()->GetStreamEngine()->UpdateAndWait();
			
			if (m_isCgfCacheExist)
			{
				//GetISystem()->GetIResourceManager()->UnloadLevelCachePak( CGF_LEVEL_CACHE_PAK );
			}

			float dt = GetCurAsyncTimeSec() - m_startTime;
			PrintMessage("Finished loading level CGF's: %d objects loaded (%d from LevelCache) in %.1f sec", m_loadedCgfCounter, m_inLevelCacheCount, dt);
		}

	case EStep::Done:
		return SetDone();

	case EStep::Failed:
	default:
		return SetFailed();
	}
#undef NEXT_STEP
}

void CObjManager::PreloadLevelObjects()
{
	CPreloadTimeslicer slicer(*this);
	I3DEngine::ELevelLoadStatus result = I3DEngine::ELevelLoadStatus::InProgress;
	const float infiniteTimeSlicingLimit = -1.0f;
	do
	{
		result = slicer.DoStep(infiniteTimeSlicingLimit);
	} while (result == I3DEngine::ELevelLoadStatus::InProgress);
}

void CObjManager::StartPreloadLevelObjects()
{
	if (m_pPreloadTimeSlicer)
	{
		return;
	}

	m_pPreloadTimeSlicer.reset(new CPreloadTimeslicer(*this));
}

I3DEngine::ELevelLoadStatus CObjManager::UpdatePreloadLevelObjects()
{
	if (!m_pPreloadTimeSlicer)
	{
		return I3DEngine::ELevelLoadStatus::Failed;
	}

	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	const float timeSlicingLimitSec = 1.0f;

	switch (m_pPreloadTimeSlicer->DoStep(timeSlicingLimitSec))
	{
	case I3DEngine::ELevelLoadStatus::InProgress:
		return I3DEngine::ELevelLoadStatus::InProgress;

	case I3DEngine::ELevelLoadStatus::Done:
		m_pPreloadTimeSlicer.reset();
		return I3DEngine::ELevelLoadStatus::Done;

	case I3DEngine::ELevelLoadStatus::Failed:
	default:
		m_pPreloadTimeSlicer.reset();
		return I3DEngine::ELevelLoadStatus::Failed;
	}
}

void CObjManager::CancelPreloadLevelObjects()
{
	if (m_pPreloadTimeSlicer)
	{
		m_pPreloadTimeSlicer.reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Create / delete object
//////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadPrepareState
{
	CStatObj* pObject;
	int  flagCloth;
	bool forceBreakable;
	char szAdjustedFilename[MAX_PATH];
};

SLoadPrepareState CObjManager::LoadStatObj_Prepare(const char* szFileName, const char* szGeomName, IStatObj::SSubObject** ppSubObject, uint32 loadingFlags)
{
	SLoadPrepareState prepState;
	prepState.forceBreakable = false;
	prepState.flagCloth = 0;
	prepState.pObject = nullptr;
	
	if (ppSubObject)
		*ppSubObject = NULL;

	if (!m_pDefaultCGF && strcmp(szFileName, DEFAULT_CGF_NAME) != 0)
	{
		// Load default object if not yet loaded.
		m_pDefaultCGF = LoadStatObj(DEFAULT_CGF_NAME, NULL, NULL, false, loadingFlags);
		if (!m_pDefaultCGF)
		{
			Error("CObjManager::LoadStatObj: Default object not found (%s)", DEFAULT_CGF_NAME);
			m_pDefaultCGF = new CStatObj();
		}
		m_pDefaultCGF->m_bDefaultObject = true;
	}

	if (CryStringUtils::stristr(szFileName, "_lod"))
	{
		Warning("Failed to load cgf: %s, '_lod' meshes can be loaded only internally as part of multi-lod CGF loading", szFileName);
		prepState.pObject = m_pDefaultCGF;
		return prepState;
	}

	if (!strcmp(szFileName, "NOFILE"))
	{
		// make empty object to be filled from outside
		prepState.pObject = new CStatObj();
		m_lstLoadedObjects.insert(prepState.pObject);
		return prepState;
	}

	//////////////////////////////////////////////////////////////////////////
	// Remap %level% alias if needed and unify filename
	{
		int nAliasNameLen = sizeof("%level%") - 1;
		if (strncmp(szFileName, "%level%", nAliasNameLen) == 0)
		{
			cry_strcpy(prepState.szAdjustedFilename, Get3DEngine()->GetLevelFilePath(szFileName + nAliasNameLen));
		}
		else
		{
			cry_strcpy(prepState.szAdjustedFilename, szFileName);
		}

		PREFAST_SUPPRESS_WARNING(6054) // sFilename is null terminated
		std::replace(prepState.szAdjustedFilename, prepState.szAdjustedFilename + strlen(prepState.szAdjustedFilename), '\\', '/'); // To Unix Path
	}
	//////////////////////////////////////////////////////////////////////////

	prepState.forceBreakable = strstr(prepState.szAdjustedFilename, "break") != 0;
	if (szGeomName && !strcmp(szGeomName, "#ForceBreakable"))
	{
		prepState.forceBreakable = true;
	}

	if (szGeomName && !strcmp(szGeomName, "cloth"))
	{
		prepState.flagCloth = STATIC_OBJECT_DYNAMIC | STATIC_OBJECT_CLONE;
	}
	else
	{
		prepState.pObject = stl::find_in_map(m_nameToObjectMap, CONST_TEMP_STRING(prepState.szAdjustedFilename), NULL);
	}

	return prepState;
}

CStatObj* TryFindSubobject(CStatObj* pObject, const char* szGeomName, IStatObj::SSubObject** ppSubObject)
{
	if (szGeomName)
	{
		// special tags that we do not want to look up
		if(strcmp(szGeomName, "#ForceBreakable") == 0 || strcmp(szGeomName, "cloth") == 0)
			szGeomName = nullptr;
	}
	
	if (ppSubObject)
		*ppSubObject = nullptr;

	if (szGeomName && szGeomName[0])
	{
		CStatObj::SSubObject* pSubObject = pObject->FindSubObject(szGeomName);
		if (!pSubObject || !pSubObject->pStatObj)
			return nullptr;

		if (ppSubObject)
			*ppSubObject = pSubObject;
		return (CStatObj*)pSubObject->pStatObj;
	}
	return pObject;
}

// actual callback implementation
class CStatObjAsyncLoader : IStatObjLoadedCallback
{
public:
	CStatObjAsyncLoader(CStatObj* pObject, uint32 loadingFlags, const char* szGeomName, IStatObjFoundCallback* pCallback) :
		m_pObject(pObject), m_pCallback(pCallback), m_geomName(szGeomName), m_loadingFlags(loadingFlags), m_lodLoadingStarted(false), m_useStreaming(false)
	{
		if (s_pStreamEngine == nullptr)
			s_pStreamEngine = gEnv->pSystem->GetStreamEngine();
	}

	void OnLoaded(bool succeeded, CStatObj* object) override
	{
		CObjManager* const pObjectManager = Cry3DEngineBase::GetObjManager();

		if (!succeeded && !m_lodLoadingStarted)
		{
			if (!(m_loadingFlags & IStatObj::ELoadingFlagsNoErrorIfFail))
				Cry3DEngineBase::Error("Failed to load cgf: %s", m_pObject->GetFilePath());
			SAFE_DELETE(m_pObject);

			if(m_pCallback)
			{
				if (!m_geomName.empty())
					m_pCallback->OnFound(nullptr, nullptr);
				else
					m_pCallback->OnFound(pObjectManager->m_pDefaultCGF, nullptr);
			}
		}
		else
		{
			if (!m_lodLoadingStarted)
			{
				StartLoad_LodsOnly(m_useStreaming);
				return;
			}

			if (!m_pObject->m_bCanUnload)
			{
				m_pObject->DisableStreaming();
			}
			m_pObject->TryMergeSubObjects(false);

			pObjectManager->m_lstLoadedObjects.insert(m_pObject);
			pObjectManager->m_nameToObjectMap[m_pObject->m_szFileName] = m_pObject;

			if (m_pCallback)
			{
				IStatObj::SSubObject* pSubObject;
				CStatObj* pFound = TryFindSubobject(m_pObject, m_geomName.c_str(), &pSubObject);
				m_pCallback->OnFound(pFound, pSubObject);
			}
		}
		
		delete this;
	}

	void StartLoad(const char* szFilename, bool useStreaming)
	{
		m_useStreaming = useStreaming;
		m_pObject->LoadCGFAsync(szFilename,m_loadingFlags, this);
		m_lodLoadingStarted = false;
	}

	void StartLoad_LodsOnly(bool useStreaming)
	{
		m_lodLoadingStarted = true;
		m_pObject->LoadLowLODsAsync(useStreaming, m_loadingFlags, this);
	}

private:
	static IStreamEngine* s_pStreamEngine;

	CStatObj* m_pObject;
	IStatObjFoundCallback* m_pCallback;
	string m_geomName;
	uint32 m_loadingFlags;
	bool m_lodLoadingStarted;
	bool m_useStreaming;
};

IStreamEngine* CStatObjAsyncLoader::s_pStreamEngine = nullptr;

void CObjManager::LoadStatObjAsync(const char* szFileName, const char* szGeomName, bool useStreaming, uint32 loadingFlags, IStatObjFoundCallback* pCallback)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, szFileName);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Static Geometry");

	SLoadPrepareState prepState = LoadStatObj_Prepare(szFileName, szGeomName, nullptr, loadingFlags);
	CStatObj* pObject = prepState.pObject;

	if (pObject)
	{
		if (!useStreaming && pObject->m_bCanUnload)
		{
			pObject->DisableStreaming();
		}

		if (!pObject->m_bLodsLoaded)
		{
			auto pLoader = new CStatObjAsyncLoader(pObject, loadingFlags, szGeomName, pCallback);
			pLoader->StartLoad_LodsOnly(useStreaming);
		}
		else if (pCallback)
		{
			IStatObj::SSubObject* pSubObject;
			CStatObj* pFound = TryFindSubobject(pObject, szGeomName, &pSubObject);
			pCallback->OnFound(pFound, pSubObject);
		}
		return;
	}
	
	// Load new CGF
	pObject = new CStatObj();
	pObject->m_nFlags |= prepState.flagCloth;

	useStreaming &= (GetCVars()->e_StreamCgf != 0);

	if (useStreaming)
		pObject->m_bCanUnload = true;
	if (prepState.forceBreakable)
		loadingFlags |= IStatObj::ELoadingFlagsForceBreakable;

	auto pLoader = new CStatObjAsyncLoader(pObject, loadingFlags, szGeomName, pCallback);
	pLoader->StartLoad(prepState.szAdjustedFilename, useStreaming);
}

CStatObj* CObjManager::LoadStatObj(const char* szFileName
                                   , const char* szGeomName, IStatObj::SSubObject** ppSubObject
                                   , bool useStreaming, uint32 loadingFlags)
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, szFileName);
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "Static Geometry");
	
	SLoadPrepareState prepState = LoadStatObj_Prepare(szFileName, szGeomName, ppSubObject, loadingFlags);
	CStatObj* pObject = prepState.pObject;

	if (pObject)
	{
		if (!useStreaming && pObject->m_bCanUnload)
		{
			pObject->DisableStreaming();
		}

		if (!pObject->m_bLodsLoaded)
		{
			pObject->LoadLowLODs(useStreaming, loadingFlags);
		}
	}
	else // Load new CGF
	{
		pObject = new CStatObj();
		pObject->m_nFlags |= prepState.flagCloth;

		useStreaming &= (GetCVars()->e_StreamCgf != 0);

		if (useStreaming)
			pObject->m_bCanUnload = true;
		if (prepState.forceBreakable)
			loadingFlags |= IStatObj::ELoadingFlagsForceBreakable;

		if (!pObject->LoadCGF(prepState.szAdjustedFilename, loadingFlags))
		{
			if (!(loadingFlags & IStatObj::ELoadingFlagsNoErrorIfFail))
				Error("Failed to load cgf: %s", szFileName);
			delete pObject;

			if (szGeomName && szGeomName[0])
				return nullptr;
			else
				return m_pDefaultCGF;
		}

		pObject->LoadLowLODs(useStreaming, loadingFlags);
		
		if (!pObject->m_bCanUnload)
		{
			// even if streaming is disabled we register object for potential streaming (streaming system will never unload it)
			pObject->DisableStreaming();
		}

		// sub meshes merging
		pObject->TryMergeSubObjects(false);

		m_lstLoadedObjects.insert(pObject);
		m_nameToObjectMap[pObject->m_szFileName] = pObject;
	}

	return TryFindSubobject(pObject, szGeomName, ppSubObject);
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::InternalDeleteObject(CStatObj* pObject)
{
	assert(pObject);

	if (!m_bLockCGFResources && !IsResourceLocked(pObject->m_szFileName))
	{
		LoadedObjects::iterator it = m_lstLoadedObjects.find(pObject);
		if (it != m_lstLoadedObjects.end())
		{
			m_lstLoadedObjects.erase(it);
			m_nameToObjectMap.erase(pObject->m_szFileName);
		}
		else
		{
			//Warning( "CObjManager::ReleaseObject called on object not loaded in ObjectManager %s",pObject->m_szFileName.c_str() );
			//return false;
		}

		delete pObject;
		return true;
	}
	else if (m_bLockCGFResources)
	{
		// Put them to locked stat obj list.
		stl::push_back_unique(m_lockedObjects, pObject);
	}

	return false;
}

CStatObj* CObjManager::AllocateStatObj()
{
#ifdef POOL_STATOBJ_ALLOCS
	return (CStatObj*)m_statObjPool->Allocate();
#else
	return (CStatObj*)malloc(sizeof(CStatObj));
#endif
}

void CObjManager::FreeStatObj(CStatObj* pObj)
{
#ifdef POOL_STATOBJ_ALLOCS
	m_statObjPool->Deallocate(pObj);
#else
	free(pObj);
#endif
}

CObjManager::CObjManager() :
	m_pDefaultCGF(NULL),
	m_decalsToPrecreate(),
	m_bNeedProcessObjectsStreaming_Finish(false),
	m_CullThread(),
	m_CheckOcclusionOutputQueue(GetCVars()->e_CheckOcclusionOutputQueueSize)

{
#ifdef POOL_STATOBJ_ALLOCS
	m_statObjPool = new stl::PoolAllocator<sizeof(CStatObj), stl::PSyncMultiThread, alignof(CStatObj)>(stl::FHeap().PageSize(64)); // 20Kb per page
#endif

	m_vStreamPreCachePointDefs.Add(SObjManPrecachePoint());
	m_vStreamPreCacheCameras.Add(SObjManPrecacheCamera());
	m_nNextPrecachePointId = 0;
	m_bCameraPrecacheOverridden = false;

	m_pObjManager = this;

	m_fCurrTime = 0.0f;

	m_vSkyColor.Set(0, 0, 0);
	m_fSunSkyRel = 0;
	m_vSunColor.Set(0, 0, 0);
	m_fILMul = 1.0f;
	m_fSkyBrightMul = 0.3f;
	m_fSSAOAmount = 1.f;
	m_fSSAOContrast = 1.f;
	m_fGIAmount = 1.f;
	m_rainParams.nUpdateFrameID = -1;
	m_rainParams.fAmount = 0.f;
	m_rainParams.fRadius = 1.f;
	m_rainParams.vWorldPos.Set(0, 0, 0);
	m_rainParams.vColor.Set(1, 1, 1);
	m_rainParams.fFakeGlossiness = 0.5f;
	m_rainParams.fFakeReflectionAmount = 1.5f;
	m_rainParams.fDiffuseDarkening = 0.5f;
	m_rainParams.fRainDropsAmount = 0.5f;
	m_rainParams.fRainDropsSpeed = 1.f;
	m_rainParams.fRainDropsLighting = 1.f;
	m_rainParams.fMistAmount = 3.f;
	m_rainParams.fMistHeight = 8.f;
	m_rainParams.fPuddlesAmount = 1.5f;
	m_rainParams.fPuddlesMaskAmount = 1.0f;
	m_rainParams.fPuddlesRippleAmount = 2.0f;
	m_rainParams.fSplashesAmount = 1.3f;
	m_rainParams.bIgnoreVisareas = false;
	m_rainParams.bDisableOcclusion = false;

	m_fMaxViewDistanceScale = 1.f;
	m_fGSMMaxDistance = 0;
	m_bLockCGFResources = false;

	m_pRMBox = NULL;
	m_bGarbageCollectionEnabled = true;

	m_decalsToPrecreate.reserve(128);
}

// make unit box for occlusion test
void CObjManager::MakeUnitCube()
{
	if (m_pRMBox)
		return;

	SVF_P3F_C4B_T2F arrVerts[8];
	arrVerts[0].xyz = Vec3(0, 0, 0);
	arrVerts[1].xyz = Vec3(1, 0, 0);
	arrVerts[2].xyz = Vec3(0, 0, 1);
	arrVerts[3].xyz = Vec3(1, 0, 1);
	arrVerts[4].xyz = Vec3(0, 1, 0);
	arrVerts[5].xyz = Vec3(1, 1, 0);
	arrVerts[6].xyz = Vec3(0, 1, 1);
	arrVerts[7].xyz = Vec3(1, 1, 1);

	//		6-------7
	//	 /		   /|
	//	2-------3	|
	//	|	      |	|
	//	|	4			| 5
	//	|				|/
	//	0-------1

	static const vtx_idx arrIndices[] =
	{
		// front + back
		1, 0, 2,
		2, 3, 1,
		5, 6, 4,
		5, 7, 6,
		// left + right
		0, 6, 2,
		0, 4, 6,
		1, 3, 7,
		1, 7, 5,
		// top + bottom
		3, 2, 6,
		6, 7, 3,
		1, 4, 0,
		1, 5, 4
	};

	m_pRMBox = GetRenderer()->CreateRenderMeshInitialized(
	  arrVerts,
	  CRY_ARRAY_COUNT(arrVerts),
	  EDefaultInputLayouts::P3F_C4B_T2F,
	  arrIndices,
	  CRY_ARRAY_COUNT(arrIndices),
	  prtTriangleList,
	  "OcclusionQueryCube", "OcclusionQueryCube",
	  eRMT_Static);

	m_pRMBox->SetChunk(NULL, 0, CRY_ARRAY_COUNT(arrVerts), 0, CRY_ARRAY_COUNT(arrIndices), 1.0f, 0);

	m_bGarbageCollectionEnabled = true;
}

CObjManager::~CObjManager()
{
	// free default object
	m_pDefaultCGF = 0;

	// free brushes
	/*  assert(!m_lstBrushContainer.Count());
	   for(int i=0; i<m_lstBrushContainer.Count(); i++)
	   {
	    if(m_lstBrushContainer[i]->GetEntityStatObj())
	      ReleaseObject((CStatObj*)m_lstBrushContainer[i]->GetEntityStatObj());
	    delete m_lstBrushContainer[i];
	   }
	   m_lstBrushContainer.Reset();
	 */
	UnloadObjects(true);
#ifdef POOL_STATOBJ_ALLOCS
	delete m_statObjPool;
#endif
}

int CObjManager::ComputeDissolve(const CLodValue &lodValueIn, SRenderNodeTempData* pTempData, IRenderNode* pEnt, float fEntDistance, CLodValue arrlodValuesOut[2])
{
	int nLodMain = CLAMP(0, lodValueIn.LodA(), MAX_STATOBJ_LODS_NUM - 1);
	int nLodMin = std::max(nLodMain - 1, 0);
	int nLodMax = std::min(nLodMain + 1, MAX_STATOBJ_LODS_NUM - 1);

	float prevLodLastTimeUsed = 0;
	float* arrLodLastTimeUsed = pTempData->userData.arrLodLastTimeUsed;

	// Find when previous lod was used as primary lod last time and update last time used for current primary lod
	arrLodLastTimeUsed[nLodMain] = GetCurTimeSec();
	for (int nLO = nLodMin; nLO <= nLodMax; nLO++)
	{
		if (nLO != nLodMain)
			prevLodLastTimeUsed = std::max(prevLodLastTimeUsed, arrLodLastTimeUsed[nLO]);
	}

	float fDissolveRef = 1.f - SATURATE((GetCurTimeSec() - prevLodLastTimeUsed) / GetCVars()->e_LodTransitionTime);
	prevLodLastTimeUsed = std::max(prevLodLastTimeUsed, GetCurTimeSec() - GetCVars()->e_LodTransitionTime);

	// Compute also max view distance fading
	const float fDistFadeInterval = 2.f;
	float fDistFadeRef = SATURATE(min(fEntDistance / pEnt->m_fWSMaxViewDist * 5.f - 4.f, ((fEntDistance - pEnt->m_fWSMaxViewDist) / fDistFadeInterval + 1.f)));

	CLodValue lodSubValue;
	int nLodsNum = 0;

	// Render current lod and (if needed) previous lod
	for (int nLO = nLodMin; nLO <= nLodMax && nLodsNum < 2; nLO++)
	{
		if (arrLodLastTimeUsed[nLO] < prevLodLastTimeUsed)
			continue;

		if (nLodMain == nLO)
		{
			// Incoming LOD
			float fDissolveMaxDistRef = std::max(fDissolveRef, fDistFadeRef);
			lodSubValue = CLodValue(nLO, int(fDissolveMaxDistRef * 255.f), -1);
		}
		else
		{
			// Outgoing LOD
			float fDissolveMaxDistRef = std::min(fDissolveRef, 1.f - fDistFadeRef);
			lodSubValue = CLodValue(-1, int(fDissolveMaxDistRef * 255.f), nLO);
		}

		arrlodValuesOut[nLodsNum] = lodSubValue;
		nLodsNum++;
	}

	return nLodsNum;
}

// mostly xy size
float CObjManager::GetXYRadius(int type)
{
	if ((m_lstStaticTypes.Count() <= type || !m_lstStaticTypes[type].pStatObj))
		return 0;

	Vec3 vSize = m_lstStaticTypes[type].pStatObj->GetBoxMax() - m_lstStaticTypes[type].pStatObj->GetBoxMin();
	vSize.z *= 0.5f;

	float fXYRadius = vSize.GetLength() * 0.5f;

	return fXYRadius;
}

bool CObjManager::GetStaticObjectBBox(int nType, Vec3& vBoxMin, Vec3& vBoxMax)
{
	if ((m_lstStaticTypes.Count() <= nType || !m_lstStaticTypes[nType].pStatObj))
		return 0;

	vBoxMin = m_lstStaticTypes[nType].pStatObj->GetBoxMin();
	vBoxMax = m_lstStaticTypes[nType].pStatObj->GetBoxMax();

	return true;
}

void CObjManager::GetMemoryUsage(class ICrySizer* pSizer) const
{
	{
		SIZER_COMPONENT_NAME(pSizer, "Self");
		pSizer->AddObject(this, sizeof(*this));
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "StaticTypes");
		pSizer->AddObject(m_lstStaticTypes);
	}

	for (int i = 0; i < MAX_RECURSION_LEVELS; i++)
	{
		SIZER_COMPONENT_NAME(pSizer, "VegetationSprites");
		for (int t = 0; t < nThreadsNum; t++)
		{
			pSizer->AddObject(m_arrVegetationSprites[i][t]);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "CMesh");
		CIndexedMesh* pMesh = CIndexedMesh::get_intrusive_list_root();
		while (pMesh)
		{
			pSizer->AddObject(pMesh);
			pMesh = pMesh->get_next_intrusive();
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "StatObj");
		for (CStatObj* pStatObj = CStatObj::get_intrusive_list_root(); pStatObj; pStatObj = pStatObj->get_next_intrusive())
		{
			pStatObj->GetMemoryUsage(pSizer);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "EmptyNodes");
		pSizer->AddObject(COctreeNode::m_arrEmptyNodes);
	}
}

// retrieves the bandwidth calculations for the audio streaming
void CObjManager::GetBandwidthStats(float* fBandwidthRequested)
{
#if !defined (_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	if (fBandwidthRequested && CStatObj::s_fStreamingTime != 0.0f)
	{
		*fBandwidthRequested = (CStatObj::s_nBandwidth / CStatObj::s_fStreamingTime) / 1024.0f;
	}
#endif
}

void CObjManager::ReregisterEntitiesInArea(AABB * pBox, bool bCleanUpTree)
{
	PodArray<SRNInfo> lstEntitiesInArea;

	Get3DEngine()->MoveObjectsIntoListGlobal(&lstEntitiesInArea, pBox, true);

	if (GetVisAreaManager())
		GetVisAreaManager()->MoveObjectsIntoList(&lstEntitiesInArea, pBox, true);

	for (int i = 0; i < lstEntitiesInArea.Count(); i++)
	{
		Get3DEngine()->UnRegisterEntityDirect(lstEntitiesInArea[i].pNode);

		if (lstEntitiesInArea[i].pNode->GetRenderNodeType() == eERType_Decal)
			((CDecalRenderNode*)lstEntitiesInArea[i].pNode)->RequestUpdate();
	}

	if (bCleanUpTree)
	{
		Get3DEngine()->GetObjectsTree()->CleanUpTree();
		if (GetVisAreaManager())
			GetVisAreaManager()->CleanUpTrees();
	}

	for (int i = 0; i < lstEntitiesInArea.Count(); i++)
	{
		Get3DEngine()->RegisterEntity(lstEntitiesInArea[i].pNode);
	}
}

void CObjManager::FreeNotUsedCGFs()
{
	//assert(!m_bLockCGFResources);
	m_lockedObjects.clear();

	if (!m_bLockCGFResources)
	{
		//Timur, You MUST use next here, or with erase you invalidating
		LoadedObjects::iterator next;
		for (LoadedObjects::iterator it = m_lstLoadedObjects.begin(); it != m_lstLoadedObjects.end(); it = next)
		{
			next = it;
			++next;
			CStatObj* p = (CStatObj*)(*it);
			if (p->m_nUsers <= 0)
			{
				CheckForGarbage(p);
			}
		}
	}

	ClearStatObjGarbage();
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::GetLoadedStatObjArray(IStatObj** pObjectsArray, int& nCount)
{
	if (!pObjectsArray)
	{
		nCount = m_lstLoadedObjects.size();
		return;
	}

	CObjManager::LoadedObjects::iterator it = m_lstLoadedObjects.begin();
	for (int i = 0; i < nCount && it != m_lstLoadedObjects.end(); ++i, ++it)
	{
		pObjectsArray[i] = *it;
	}
}

void StatInstGroup::Update(CVars* pCVars, int nGeomDetailScreenRes)
{
	m_dwRndFlags = 0;

	static ICVar* pObjShadowCastSpec = gEnv->pConsole->GetCVar("e_ObjShadowCastSpec");
	if (nCastShadowMinSpec <= pObjShadowCastSpec->GetIVal())
	{
		m_dwRndFlags |= ERF_CASTSHADOWMAPS | ERF_HAS_CASTSHADOWMAPS;
	}

	if (bDynamicDistanceShadows)
		m_dwRndFlags |= ERF_DYNAMIC_DISTANCESHADOWS;
	if (bHideability)
		m_dwRndFlags |= ERF_HIDABLE;
	if (bHideabilitySecondary)
		m_dwRndFlags |= ERF_HIDABLE_SECONDARY;
	if (bPickable)
		m_dwRndFlags |= ERF_PICKABLE;
	if (!bAllowIndoor)
		m_dwRndFlags |= ERF_OUTDOORONLY;
	if (bGIMode)
		m_dwRndFlags |= ERF_GI_MODE_BIT0; // corresponds to IRenderNode::eGM_StaticVoxelization

	uint32 nSpec = (uint32)minConfigSpec;
	if (nSpec != 0)
	{
		m_dwRndFlags &= ~ERF_SPEC_BITS_MASK;
		m_dwRndFlags |= (nSpec << ERF_SPEC_BITS_SHIFT) & ERF_SPEC_BITS_MASK;
	}

	if (GetStatObj())
	{
		fVegRadiusVert = GetStatObj()->GetRadiusVert();
		fVegRadiusHor = GetStatObj()->GetRadiusHors();
		fVegRadius = max(fVegRadiusVert, fVegRadiusHor);
	}
	else
	{
		fVegRadiusHor = fVegRadius = fVegRadiusVert = 0;
	}

	if (bUseSprites && fVegRadius > 0)
	{
		m_fSpriteSwitchDist = 18.f * fVegRadius * fSize *
		                      max(pCVars->e_VegetationSpritesDistanceCustomRatioMin, fSpriteDistRatio);
		m_fSpriteSwitchDist *= pCVars->e_VegetationSpritesDistanceRatio;
		m_fSpriteSwitchDist *= max(1.f, (float)nGeomDetailScreenRes / 1024.f);
		if (m_fSpriteSwitchDist < pCVars->e_VegetationSpritesMinDistance)
			m_fSpriteSwitchDist = pCVars->e_VegetationSpritesMinDistance;
	}
	else
		m_fSpriteSwitchDist = 1000000.f;

	for (int j = 0; j < FAR_TEX_COUNT; ++j)
	{
		SVegetationSpriteLightInfo& rLightInfo = m_arrSSpriteLightInfo[j];
		if (rLightInfo.m_pDynTexture)
			rLightInfo.m_pDynTexture->SetFlags(rLightInfo.m_pDynTexture->GetFlags() | IDynTexture::fNeedRegenerate);
	}

#if defined(FEATURE_SVO_GI)
	IMaterial* pMat = pMaterial ? pMaterial.get() : (pStatObj ? pStatObj->GetMaterial() : 0);
	if (pMat && (Cry3DEngineBase::GetCVars()->e_svoTI_Active >= 0) && (gEnv->IsEditor() || Cry3DEngineBase::GetCVars()->e_svoTI_Apply))
		pMat->SetKeepLowResSysCopyForDiffTex();
#endif
}

float StatInstGroup::GetAlignToTerrainAmount() const
{
	return fAlignToTerrainCoefficient;
}

bool CObjManager::SphereRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const float fRadius, IMaterial* pMat)
{
	FUNCTION_PROFILER_3DENGINE;

	// get position offset and stride
	int nPosStride = 0;
	byte* pPos = pRenderMesh->GetPosPtr(nPosStride, FSL_READ);

	// get indices
	vtx_idx* pInds = pRenderMesh->GetIndexPtr(FSL_READ);

#if defined(USE_CRY_ASSERT)
	int nInds = pRenderMesh->GetIndicesCount();
	assert(nInds % 3 == 0);
#endif

	// test tris
	TRenderChunkArray& Chunks = pRenderMesh->GetChunks();
	for (int nChunkId = 0; nChunkId < Chunks.size(); nChunkId++)
	{
		CRenderChunk* pChunk = &Chunks[nChunkId];
		if (pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		// skip transparent and alpha test
		if (pMat)
		{
			const SShaderItem& shaderItem = pMat->GetShaderItem(pChunk->m_nMatID);
			if (!shaderItem.m_pShader || shaderItem.m_pShader->GetFlags() & EF_NODRAW)
				continue;
		}

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
		for (int i = pChunk->nFirstIndexId; i < nLastIndexId; i += 3)
		{
			assert((int)pInds[i + 0] < pRenderMesh->GetVerticesCount());
			assert((int)pInds[i + 1] < pRenderMesh->GetVerticesCount());
			assert((int)pInds[i + 2] < pRenderMesh->GetVerticesCount());

			// get triangle vertices
			Vec3 v0 = (*(Vec3*)&pPos[nPosStride * pInds[i + 0]]);
			Vec3 v1 = (*(Vec3*)&pPos[nPosStride * pInds[i + 1]]);
			Vec3 v2 = (*(Vec3*)&pPos[nPosStride * pInds[i + 2]]);

			AABB triBox;
			triBox.min = v0;
			triBox.max = v0;
			triBox.Add(v1);
			triBox.Add(v2);

			if (Overlap::Sphere_AABB(Sphere(vInPos, fRadius), triBox))
				return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::ClearStatObjGarbage()
{
	FUNCTION_PROFILER_3DENGINE;

	std::vector<CStatObj*> garbage;

	CryMT::vector<CStatObj*>::AutoLock lock(m_checkForGarbage.get_lock());

	while (!m_checkForGarbage.empty())
	{
		garbage.resize(0);

		// Make sure all stat objects inside this array are unique.
		if (!m_checkForGarbage.empty())
		{
			// Only check explicitly added objects.
			// First ShutDown object clearing all pointers.
			CStatObj* pStatObj;
			while (m_checkForGarbage.try_pop_back(pStatObj))
			{
				if (pStatObj->m_bCheckGarbage)
				{
					// Check if it must be released.
					int nChildRefs = pStatObj->CountChildReferences();
					if (pStatObj->m_nUsers <= 0 && nChildRefs <= 0)
					{
						garbage.push_back(pStatObj);
					}
					else
					{
						pStatObj->m_bCheckGarbage = false;
					}
				}
			}
		}

		// First ShutDown object clearing all pointers.
		for (int i = 0, num = (int)garbage.size(); i < num; i++)
		{
			CStatObj* pStatObj = garbage[i];

			if (!m_bLockCGFResources && !IsResourceLocked(pStatObj->m_szFileName))
			{
				// only shutdown object if it can be deleted by InternalDeleteObject()
				pStatObj->ShutDown();
			}
		}

		// Then delete all garbage objects.
		for (int i = 0, num = (int)garbage.size(); i < num; i++)
		{
			CStatObj* pStatObj = garbage[i];
			InternalDeleteObject(pStatObj);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
IRenderMesh* CObjManager::GetRenderMeshBox()
{
	if (!m_pRMBox)
	{
		MakeUnitCube();
	}
	return m_pRMBox;
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::CheckForGarbage(CStatObj* pObject)
{
	if (m_bGarbageCollectionEnabled &&
	    !pObject->m_bCheckGarbage)
	{
		pObject->m_bCheckGarbage = true;
		m_checkForGarbage.push_back(pObject);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjManager::UnregisterForGarbage(CStatObj* pObject)
{
	CRY_ASSERT(pObject);

	if (m_bGarbageCollectionEnabled &&
	    pObject->m_bCheckGarbage)
	{
		m_checkForGarbage.try_remove(pObject);
		pObject->m_bCheckGarbage = false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjManager::AddOrCreatePersistentRenderObject(SRenderNodeTempData* pTempData, CRenderObject*& pRenderObject, const CLodValue* pLodValue, const Matrix34& transformationMatrix, const SRenderingPassInfo& passInfo) const
{
	CRY_ASSERT(pRenderObject == nullptr);
	const bool multipleLevelsOfDetail = pLodValue && pLodValue->DissolveRefA();
	const bool shouldGetOrCreatePermanentObject = (GetCVars()->e_PermanentRenderObjects && (pTempData || pRenderObject) && GetCVars()->e_DebugDraw == 0 && !multipleLevelsOfDetail) &&
		!(passInfo.IsRecursivePass() || (pTempData && (pTempData->userData.m_pFoliage || (pTempData->userData.pOwnerNode && (pTempData->userData.pOwnerNode->GetRndFlags() & ERF_SELECTED)))));

	if (shouldGetOrCreatePermanentObject)
	{
		if (pLodValue && pLodValue->LodA() == -1 && pLodValue->LodB() == -1)
			return true;

		if (pLodValue && pLodValue->LodA() == -1 && pLodValue->DissolveRefB() == 255)
			return true;

		if (pLodValue && pLodValue->LodB() == -1 && pLodValue->DissolveRefA() == 255)
			return true;

		int nLod = pLodValue ? CLAMP(0, pLodValue->LodA(), MAX_STATOBJ_LODS_NUM - 1) : 0;
		IPermanentRenderObject* pPermanentRenderObj = static_cast<IPermanentRenderObject*>(pTempData->GetRenderObject(nLod));

		const CRenderObject::ERenderPassType passType = passInfo.GetPassType();

		// Update instance only for dirty objects
		const bool instanceDataDirty = pPermanentRenderObj->IsInstanceDataDirty(passType);

		// When permanent renderobject has sub-object and its instance data dirty, its instances need to be updated as well.
		if (pPermanentRenderObj->IsPreparedForPass(passType) && instanceDataDirty && pPermanentRenderObj->HasSubObject())
		{
			pPermanentRenderObj = static_cast<IPermanentRenderObject*>(pTempData->RefreshRenderObject(nLod));
		}

		passInfo.GetIRenderView()->AddPermanentObject(pPermanentRenderObj, passInfo);

		// Has this object already been filled?
		if (pPermanentRenderObj->IsPreparedForPass(passType)) // Object drawn once => fast path.
		{
			if (instanceDataDirty)
			{
				// Update instance matrix
				pPermanentRenderObj->SetMatrix(transformationMatrix);
				pPermanentRenderObj->SetInstanceDataDirty(passType, false);
			}

			if (GetCVars()->e_BBoxes && pTempData && pTempData->userData.pOwnerNode)
				GetObjManager()->RenderObjectDebugInfo(pTempData->userData.pOwnerNode, pPermanentRenderObj->m_fDistance, passInfo);

			return true;
		}

		// Permanent object needs to be filled first time,
		if (pTempData && pTempData->userData.pOwnerNode)
			pTempData->userData.nStatObjLastModificationId = GetResourcesModificationChecksum(pTempData->userData.pOwnerNode);

		pPermanentRenderObj->SetPreparedForPass(passType);

		pRenderObject = pPermanentRenderObj;
	}
	else
	{
		// Release persistent ROs associated with this temp-data
		if (pTempData)
			pTempData->FreeRenderObjects();

		// Fallback to temporary render object
		pRenderObject = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	}

	// We do not have a persistant render object
	// Always update instance matrix
	pRenderObject->SetMatrix(transformationMatrix);

	return false;
}

//////////////////////////////////////////////////////////////////////////
uint32 CObjManager::GetResourcesModificationChecksum(IRenderNode* pOwnerNode) const
{
	uint32 nModificationId = 1;

	if (CStatObj* pStatObj = (CStatObj*)pOwnerNode->GetEntityStatObj())
		nModificationId += pStatObj->GetModificationId();

	if (CMatInfo* pMatInfo = (CMatInfo*)pOwnerNode->GetMaterial())
		nModificationId += pMatInfo->GetModificationId();

	if (pOwnerNode->GetRenderNodeType() == eERType_TerrainSector)
		nModificationId += ((CTerrainNode*)pOwnerNode)->GetMaterialsModificationId();

	return nModificationId;
}

//////////////////////////////////////////////////////////////////////////
IRenderMesh* CObjManager::GetBillboardRenderMesh(IMaterial* pMaterial)
{
	if(!m_pBillboardMesh)
	{
		PodArray<SVF_P3F_C4B_T2F> arrVertices;
		PodArray<SPipTangents> arrTangents;
		PodArray<vtx_idx> arrIndices;

		SVF_P3F_C4B_T2F vert;
		ZeroStruct(vert);
		vert.st = Vec2(0.0f, 0.0f);
		vert.color.dcolor = -1;

		// verts
		vert.xyz.Set(+.5, 0, -.5);
		vert.st = Vec2(1, 1);
		arrVertices.Add(vert);
		vert.xyz.Set(-.5, 0, -.5);
		vert.st = Vec2(0, 1);
		arrVertices.Add(vert);
		vert.xyz.Set(+.5, 0, +.5);
		vert.st = Vec2(1, 0);
		arrVertices.Add(vert);
		vert.xyz.Set(-.5, 0, +.5);
		vert.st = Vec2(0, 0);
		arrVertices.Add(vert);

		// tangents
		arrTangents.Add(SPipTangents(Vec3(1, 0, 0), Vec3(0, 0, 1), 1));
		arrTangents.Add(SPipTangents(Vec3(1, 0, 0), Vec3(0, 0, 1), 1));
		arrTangents.Add(SPipTangents(Vec3(1, 0, 0), Vec3(0, 0, 1), 1));
		arrTangents.Add(SPipTangents(Vec3(1, 0, 0), Vec3(0, 0, 1), 1));

		// indices
		arrIndices.Add(0);
		arrIndices.Add(1);
		arrIndices.Add(2);
		arrIndices.Add(1);
		arrIndices.Add(3);
		arrIndices.Add(2);

		m_pBillboardMesh = Cry3DEngineBase::GetRenderer()->CreateRenderMeshInitialized(
			arrVertices.GetElements(), arrVertices.Count(), EDefaultInputLayouts::P3F_C4B_T2F,
			arrIndices.GetElements(), arrIndices.Count(), prtTriangleList,
			"Billboard", "Billboard", eRMT_Static, 1, 0, NULL, NULL, false, true,
			arrTangents.GetElements());

		m_pBillboardMesh->SetChunk(pMaterial, 0, arrVertices.Count(), 0, arrIndices.Count(), 1);
	}

	return m_pBillboardMesh;
}
