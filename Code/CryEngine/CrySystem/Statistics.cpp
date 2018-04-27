// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/IConsole.h>
#include <CryAnimation/ICryAnimation.h>
#include <CrySystem/Profilers/IPerfHud.h>
#include <CryString/CryPath.h>
#include <CryMemory/CrySizer.h>
#include "CrySizerImpl.h"
#include "CrySizerStats.h"
#include "System.h"
#include <CryMemory/CryMemoryManager.h>
#include <CrySystem/ITestSystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>    // CResourceCompilerHelper
#include "LoadingProfiler.h"
#include "PhysRenderer.h"
#include <CrySystem/File/IResourceManager.h>
#include <CrySystem/Scaleform/IFlashPlayer.h>
#include <CrySystem/IStreamEngine.h>

// Access to some game info.
#include <CryGame/IGameFramework.h>    // IGameFramework
#include <../CryAction/ILevelSystem.h> // ILevelSystemListener

const std::vector<string>& GetModuleNames()
{
	static std::vector<string> moduleNames;

	if (moduleNames.empty())
	{
		static_cast<CSystem*>(gEnv->pSystem)->GetLoadedDynamicLibraries(moduleNames);
	}

	return moduleNames;
}

#if (!defined (_RELEASE) || defined(ENABLE_PROFILING_CODE))

	#if CRY_PLATFORM_WINDOWS
		#include "Psapi.h"
typedef BOOL (WINAPI * GetProcessMemoryInfoProc)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
	#endif

	#if CRY_PLATFORM_WINDOWS
		#pragma pack(push,1)
const struct PEHeader_DLL
{
	DWORD                 signature;
	IMAGE_FILE_HEADER     _head;
	IMAGE_OPTIONAL_HEADER opt_head;
	IMAGE_SECTION_HEADER* section_header; // actual number in NumberOfSections
};
		#pragma pack(pop)
	#endif

extern int  CryMemoryGetAllocatedSize();
static void SaveLevelStats(IConsoleCmdArgs* pArgs);

	#define g_szTestResults "%USER%/TestResults"

class CResourceCollector : public IResourceCollector
{
	struct SInstanceEntry
	{
		//		AABB			m_AABB;
		uint32 m_dwFileNameId;              // use with m_FilenameToId,  m_IdToFilename
	};

	struct SAssetEntry
	{
		SAssetEntry() : m_dwInstanceCnt(0), m_dwDependencyCnt(0), m_dwMemSize(0xffffffff), m_dwFileSize(0xffffffff)
		{
		}

		string m_sFileName;
		uint32 m_dwInstanceCnt;             // 1=this asset is used only once in the level, 2, 3, ...
		uint32 m_dwDependencyCnt;           // 1=this asset is only used by one asset, 2, 3, ...
		uint32 m_dwMemSize;                 // 0xffffffff if unknown (only needed to verify disk file size)
		uint32 m_dwFileSize;                // 0xffffffff if unknown
	};

public:                                 // -----------------------------------------------------------------------------

	CResourceCollector()
	{
		m_bEnabled = false;
	}
	void Enable(bool bEnabled) { m_bEnabled = bEnabled; }

	// compute m_dwDependencyCnt
	void ComputeDependencyCnt()
	{
		std::set<SDependencyPair>::const_iterator it, end = m_Dependencies.end();

		for (it = m_Dependencies.begin(); it != end; ++it)
		{
			const SDependencyPair& rRef = *it;

			++m_Assets[rRef.m_idDependsOnAsset].m_dwDependencyCnt;
		}
	}

	// watch out: this function modifies internal data
	void LogData(ILog& rLog)
	{
		if (!m_bEnabled)
			return;

		rLog.Log(" ");

		{
			rLog.Log("Assets:");

			std::vector<SAssetEntry>::const_iterator it, end = m_Assets.end();
			uint32 dwAssetID = 0;

			for (it = m_Assets.begin(); it != end; ++it, ++dwAssetID)
			{
				const SAssetEntry& rRef = *it;

				rLog.Log(" A%u: inst:%5u dep:%u mem:%9u file:%9u name:%s", dwAssetID, rRef.m_dwInstanceCnt, rRef.m_dwDependencyCnt, rRef.m_dwMemSize, rRef.m_dwFileSize, rRef.m_sFileName.c_str());
			}
		}

		rLog.Log(" ");

		{
			rLog.Log("Dependencies:");

			std::set<SDependencyPair>::const_iterator it, end = m_Dependencies.end();

			uint32 dwCurrentAssetID = 0xffffffff;
			uint32 dwSumFile = 0;

			for (it = m_Dependencies.begin(); it != end; ++it)
			{
				const SDependencyPair& rRef = *it;

				if (rRef.m_idAsset != dwCurrentAssetID)
				{
					if (dwSumFile != 0 && dwSumFile != 0xffffffff)
						rLog.Log("                                                ---> sum file: %u KB", (dwSumFile + 1023) / 1024);

					dwSumFile = 0;

					rLog.Log(" ");
					rLog.Log(" A%u '%s' depends on", rRef.m_idAsset, m_Assets[rRef.m_idAsset].m_sFileName.c_str());
				}

				uint32 dwFileSize = m_Assets[rRef.m_idDependsOnAsset].m_dwFileSize;

				rLog.Log("        A%u file:%9u dep:%u '%s'", rRef.m_idDependsOnAsset, dwFileSize, m_Assets[rRef.m_idDependsOnAsset].m_dwDependencyCnt, m_Assets[rRef.m_idDependsOnAsset].m_sFileName.c_str());

				if (dwFileSize != 0xffffffff)
					dwSumFile += dwFileSize;

				dwCurrentAssetID = rRef.m_idAsset;
			}

			if (dwSumFile != 0 && dwSumFile != 0xffffffff)
				rLog.Log("                                                ---> sum file: %u KB", (dwSumFile + 1023) / 1024);
		}

		rLog.Log(" ");

		{
			rLog.Log("SourceAtoms:");

			std::set<SDependencyPair>::const_iterator it;

			while (!m_Dependencies.empty())
			{
				for (it = m_Dependencies.begin(); it != m_Dependencies.end(); ++it)
				{
					const SDependencyPair& rRef1 = *it;

					rLog.Log(" ");
					std::set<uint32> localDependencies;

					localDependencies.insert(rRef1.m_idAsset);

					RecursiveMove(rRef1.m_idAsset, localDependencies);

					PrintDependencySet(rLog, localDependencies);
					break;
				}
			}
		}
	}

	// interface IResourceCollector -------------------------------------------------

	virtual bool AddResource(const char* szFileName, const uint32 dwSize = 0xffffffff)
	{
		if (!m_bEnabled)
			return true;

		uint32 dwNewAssetIdOrInvalid = _AddResource(szFileName, dwSize);

		return dwNewAssetIdOrInvalid != 0xffffffff;
	}

	virtual void AddInstance(const char* _szFileName, void* pInstance)
	{
		if (!m_bEnabled)
			return;

		assert(pInstance);

		{
			std::set<void*>::const_iterator itInstance = m_ReportedInstances.find(pInstance);

			if (itInstance != m_ReportedInstances.end())
				return;
		}

		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string, uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);

		if (it == m_FilenameToId.end())
		{
			OutputDebugString("ERROR: file wasn't registered with AddResource(): '");
			OutputDebugString(sOutputFileName.c_str());
			OutputDebugString("'\n");
			assert(0);                             // asset wasn't registered yet AddResource() missing - unpredictable result might happen
			return;
		}

		uint32 dwAssetId = it->second;
		/*
		    // debug
		    char str[256];
		    cry_sprintf(str,"AddInstance: %p '",pInstance);
		    OutputDebugString(str);
		    OutputDebugString(sOutputFileName.c_str());
		    OutputDebugString("'\n");
		 */
		++m_Assets[dwAssetId].m_dwInstanceCnt;
		m_ReportedInstances.insert(pInstance);
	}

	virtual void OpenDependencies(const char* _szFileName)
	{
		if (!m_bEnabled)
			return;

		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string, uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);

		if (it == m_FilenameToId.end())
		{
			m_OpenedAssetId.push_back(0xffffffff); // CloseDependencies() relies on that

			OutputDebugString("ERROR: file wasn't registered with AddResource(): '");
			OutputDebugString(sOutputFileName.c_str());
			OutputDebugString("'\n");
			//assert(0);		// asset wasn't registered yet AddResource() missing - unpredictable result might happen
			return;
		}

		uint32 dwAssetId = it->second;

		m_OpenedAssetId.push_back(dwAssetId);
	}

	virtual void Reset()
	{
		m_Assets.resize(0);
		m_Dependencies.clear();
		m_FilenameToId.clear();
		m_OpenedAssetId.resize(0);
		m_ReportedInstances.clear();
		m_ResourceEntries.resize(0);
	}

	virtual void CloseDependencies()
	{
		if (!m_bEnabled)
			return;

		assert(!m_OpenedAssetId.empty());              // internal error - OpenDependencies() should match CloseDependencies()

		m_OpenedAssetId.pop_back();
	}

private:                                           // -----------------------------------------------------------------------

	struct SDependencyPair
	{
		SDependencyPair(const uint32 idAsset, const uint32 idDependsOnAsset) : m_idAsset(idAsset), m_idDependsOnAsset(idDependsOnAsset)
		{
		}

		uint32 m_idAsset;                              // AssetID
		uint32 m_idDependsOnAsset;                     // AssetID

		bool operator<(const SDependencyPair& rhs) const
		{
			if (m_idAsset < rhs.m_idAsset) return true;
			if (m_idAsset > rhs.m_idAsset) return false;

			return m_idDependsOnAsset < rhs.m_idDependsOnAsset;
		}
	};

	std::vector<uint32>         m_OpenedAssetId;     // to track for dependencies
	std::map<string, uint32>    m_FilenameToId;      // could be done more efficiently
	std::vector<SAssetEntry>    m_Assets;            // could be done more efficiently
	std::vector<SInstanceEntry> m_ResourceEntries;   //
	std::set<SDependencyPair>   m_Dependencies;      //
	std::set<void*>             m_ReportedInstances; // to avoid counting them twice
	bool                        m_bEnabled;

	// ---------------------------------------------------------------------

	string UnifyFilename(const char* _szFileName) const
	{
		char* szFileName = (char*)_szFileName;

		// as bump and normal maps become combined during loading e.g.  blah.tif+blah_ddn.dds
		// the filename needs to be adjusted
		{
			char* pSearchForPlus = szFileName;

			while (*pSearchForPlus != 0 && *pSearchForPlus != '+')
				++pSearchForPlus;

			if (*pSearchForPlus == '+')
				szFileName = pSearchForPlus + 1;
		}

		string sOutputFileName;
		{
			char buffer[512];
			CResourceCompilerHelper::GetOutputFilename(szFileName, buffer, sizeof(buffer));
			sOutputFileName = buffer;
		}

		sOutputFileName = PathUtil::ToUnixPath(sOutputFileName);
		sOutputFileName.MakeLower();

		return sOutputFileName;
	}

	// Returns:
	//   0xffffffff if asset was already known (m_dwInstanceCnt will be increased), AssetId otherwise
	uint32 _AddResource(const char* _szFileName, const uint32 dwSize = 0xffffffff)
	{
		assert(_szFileName);

		if (_szFileName[0] == 0)
			return 0xffffffff;                // no name provided - ignore this case - this often means the feature is not used

		uint32 dwNewAssetIdOrInvalid = 0xffffffff;

		string sOutputFileName = UnifyFilename(_szFileName);

		std::map<string, uint32>::const_iterator it = m_FilenameToId.find(sOutputFileName);
		uint32 dwAssetId;

		if (it != m_FilenameToId.end())
			dwAssetId = it->second;
		else
		{
			dwAssetId = m_FilenameToId.size();
			m_FilenameToId[sOutputFileName] = dwAssetId;

			SAssetEntry NewAsset;

			NewAsset.m_sFileName = sOutputFileName;

			//			if(dwSize==0xffffffff)
			{
				CCryFile file;

				if (file.Open(sOutputFileName.c_str(), "rb"))
					NewAsset.m_dwFileSize = file.GetLength();
			}

			dwNewAssetIdOrInvalid = dwAssetId;
			m_Assets.push_back(NewAsset);
		}

		SAssetEntry& rAsset = m_Assets[dwAssetId];

		if (dwSize != 0xffffffff)                    // if size was specified
		{
			if (rAsset.m_dwMemSize == 0xffffffff)
			{
				rAsset.m_dwMemSize = dwSize;              // store size
			}
			else
				assert(rAsset.m_dwMemSize == dwSize);     // size should always be the same
		}

		//		rAsset.m_dwInstanceCnt+=dwInstanceCount;

		// debugging
		//		char str[1204];
		//		cry_sprintf(str,"_AddResource %s(size=%d cnt=%d)\n",_szFileName,rAsset.m_dwInstanceCnt,rAsset.m_dwMemSize);
		//		OutputDebugString(str);

		SInstanceEntry instance;

		instance.m_dwFileNameId = dwAssetId;

		AddDependencies(dwAssetId);

		m_ResourceEntries.push_back(instance);
		return dwNewAssetIdOrInvalid;
	}

	void AddDependencies(const uint32 dwPushedAssetId)
	{
		std::vector<uint32>::const_iterator it, end = m_OpenedAssetId.end();

		for (it = m_OpenedAssetId.begin(); it != end; ++it)
		{
			uint32 dwOpendedAssetId = *it;

			if (dwOpendedAssetId == 0xffffffff)
				continue;   //  asset wasn't registered yet AddResource() missing

			m_Dependencies.insert(SDependencyPair(dwOpendedAssetId, dwPushedAssetId));
		}
	}

	void PrintDependencySet(ILog& rLog, const std::set<uint32>& Dep)
	{
		rLog.Log("      {");
		std::set<uint32>::const_iterator it, end = Dep.end();
		uint32 dwSumFile = 0;

		// iteration could be optimized
		for (it = Dep.begin(); it != end; ++it)
		{
			uint32 idAsset = *it;

			uint32 dwFileSize = m_Assets[idAsset].m_dwFileSize;

			if (dwFileSize != 0xffffffff)
				dwSumFile += dwFileSize;

			rLog.Log("        A%u file:%9u dep:%u '%s'", idAsset, m_Assets[idAsset].m_dwFileSize, m_Assets[idAsset].m_dwDependencyCnt, m_Assets[idAsset].m_sFileName.c_str());
		}
		rLog.Log("      }                                           ---> sum file: %u KB", (dwSumFile + 1023) / 1024);
	}

	// find all dependencies to it and move to localDependencies
	void RecursiveMove(const uint32 dwCurrentAssetID, std::set<uint32>& localDependencies)
	{
		bool bProcess = true;

		// iteration could be optimized
		while (bProcess)
		{
			bProcess = false;

			std::set<SDependencyPair>::iterator it;

			for (it = m_Dependencies.begin(); it != m_Dependencies.end(); ++it)
			{
				SDependencyPair Pair = *it;

				if (Pair.m_idAsset == dwCurrentAssetID || Pair.m_idDependsOnAsset == dwCurrentAssetID)
				{
					uint32 idAsset = (Pair.m_idAsset == dwCurrentAssetID) ? Pair.m_idDependsOnAsset : Pair.m_idAsset;

					localDependencies.insert(idAsset);

					m_Dependencies.erase(it);

					RecursiveMove(idAsset, localDependencies);
					bProcess = true;
					break;
				}
			}
		}
	}

	friend class CStatsToExcelExporter;
};

	#define MAX_LODS 6

//////////////////////////////////////////////////////////////////////////
// Statistics about currently loaded level.
//////////////////////////////////////////////////////////////////////////
struct SCryEngineStats
{
	struct StatObjInfo
	{
		int       nVertices;
		int       nIndices;
		int       nIndicesPerLod[MAX_LODS];
		int       nMeshSize;
		int       nMeshSizeLoaded;
		int       nTextureSize;
		int       nPhysProxySize;
		int       nPhysProxySizeMax;
		int       nPhysPrimitives;
		int       nDrawCalls;
		int       nLods;
		int       nSubMeshCount;
		int       nNumRefs;
		bool      bSplitLods;
		IStatObj* pStatObj;
	};
	struct CharacterInfo
	{
		CharacterInfo()
			: nVertices(0)
			, nIndices(0)
			, nMeshSize(0)
			, nTextureSize(0)
			, nLods(0)
			, nInstances(0)
			, nPhysProxySize(0)
			, pIDefaultSkeleton(nullptr)
		{
			ZeroArray(nVerticesPerLod);
			ZeroArray(nIndicesPerLod);
		}

		int               nVertices;
		int               nIndices;
		int               nVerticesPerLod[MAX_LODS];
		int               nIndicesPerLod[MAX_LODS];
		int               nMeshSize;
		int               nTextureSize;
		int               nLods;
		int               nInstances;
		int               nPhysProxySize;
		IDefaultSkeleton* pIDefaultSkeleton;
	};
	struct MeshInfo
	{
		int         nVerticesSum;
		int         nIndicesSum;
		int         nCount;
		int         nMeshSizeDev;
		int         nMeshSizeSys;
		int         nTextureSize;
		const char* name;
	};
	struct MemInfo : public SCryEngineStatsGlobalMemInfo
	{
		MemInfo() : m_pSizer(0), m_pStats(0) {}
		~MemInfo() { SAFE_DELETE(m_pSizer); SAFE_DELETE(m_pStats); }

		//int totalUsedInModules;
		//int totalCodeAndStatic;
		//int countedMemoryModules;
		//uint64 totalAllocatedInModules;
		//int totalNumAllocsInModules;
		CrySizerImpl*  m_pSizer;
		CrySizerStats* m_pStats;

		//std::vector<SCryEngineStatsModuleInfo> modules;
	};

	struct SBrushMemInfo
	{
		string brushName;
		int    usedTextureMemory;
		int    lodNum;
	};

	struct ProfilerInfo
	{
		string m_name;
		string m_module;

		//! Total time spent in this counter including time of child profilers in current frame.
		float m_totalTime;
		//! Self frame time spent only in this counter (But includes recursive calls to same counter) in current frame.
		int64 m_selfTime;
		//! How many times this profiler counter was executed.
		int   m_count;
		//! Displayed quantity (interpolated or average).
		float m_displayedValue;
		//! How variant this value.
		float m_variance;
		// min value
		float m_min;
		// max value
		float m_max;

		int   m_mincount;

		int   m_maxcount;

	};

	struct SPeakProfilerInfo
	{
		ProfilerInfo profiler;
		float        peakValue;
		float        averageValue;
		float        variance;
		int          pageFaults; // Number of page faults at this frame.
		int          count;      // Number of times called for peak.
		float        when;       // when it added.
	};

	struct SModuleProfilerInfo
	{
		string name;
		float  overBugetRatio;
	};

	struct SEntityInfo
	{
		string name, models;
		bool   bInvisible, bHidden;
	};

	SCryEngineStats()
		: nSummary_CodeAndStaticSize(0)

		, nSummaryCharactersSize(0)

		//, nSummary_TextureSize(0)
		, nSummary_UserTextureSize(0)
		, nSummary_EngineTextureSize(0)
		, nSummary_TexturesStreamingThroughput(0.0f)
		, nSummaryEntityCount(0)

		, nStatObj_SummaryTextureSize(0)
		, nStatObj_SummaryMeshSize(0)
		, nStatObj_TotalCount(0)

		, nChar_SummaryMeshSize(0)
		, nChar_SummaryTextureSize(0)
		, nChar_NumInstances(0)

		, fLevelLoadTime(0.0f)
		, nSummary_TexturesPoolSize(0)
	{
		ISystem* pSystem = GetISystem();
		I3DEngine* p3DEngine = pSystem->GetI3DEngine();
		IRenderer* pRenderer = pSystem->GetIRenderer();

		nTotalAllocatedMemory = CryMemoryGetAllocatedSize();
		nSummaryMeshSize = 0;
		nSummaryMeshCount = 0;
		nAPI_MeshSize = 0;

		if (pRenderer)
		{
			pRenderer->EF_Query(EFQ_Alloc_Mesh_SysMem, nSummaryMeshSize);
			pRenderer->EF_Query(EFQ_Mesh_Count, nSummaryMeshCount);
			pRenderer->EF_Query(EFQ_Alloc_APIMesh, nAPI_MeshSize);
		}

		nSummaryScriptSize = pSystem->GetIScriptSystem()->GetScriptAllocSize();

		IMemoryManager::SProcessMemInfo procMeminfo;
		GetISystem()->GetIMemoryManager()->GetProcessMemInfo(procMeminfo);

		nWin32_WorkingSet = procMeminfo.WorkingSetSize;
		nWin32_PeakWorkingSet = procMeminfo.PeakWorkingSetSize;
		nWin32_PagefileUsage = procMeminfo.PagefileUsage;
		nWin32_PeakPagefileUsage = procMeminfo.PeakPagefileUsage;
		nWin32_PageFaultCount = procMeminfo.PageFaultCount;

		fLevelLoadTime = gEnv->pSystem->GetIResourceManager()->GetLastLevelLoadTime().GetSeconds();

		if (p3DEngine)
		{
			p3DEngine->FillDebugFPSInfo(infoFPS);
		}
	}

	uint64                            nWin32_WorkingSet;
	uint64                            nWin32_PeakWorkingSet;
	uint64                            nWin32_PagefileUsage;
	uint64                            nWin32_PeakPagefileUsage;
	uint64                            nWin32_PageFaultCount;

	uint32                            nTotalAllocatedMemory;
	uint32                            nSummary_CodeAndStaticSize; // Total size of all code plus static data

	uint32                            nSummaryScriptSize;
	uint32                            nSummaryCharactersSize;
	uint32                            nSummaryMeshCount;
	uint32                            nSummaryMeshSize;
	uint32                            nSummaryEntityCount;

	uint32                            nAPI_MeshSize; // Allocated by DirectX

	uint32                            nSummary_TextureSize;                 // Total size of all textures
	uint32                            nSummary_UserTextureSize;             // Size of eser textures, (from files...)
	uint32                            nSummary_EngineTextureSize;           // Dynamic Textures
	uint32                            nSummary_TexturesPoolSize;            // Dynamic Textures
	float                             nSummary_TexturesStreamingThroughput; // in KB/sec

	uint32                            nStatObj_SummaryTextureSize;
	uint32                            nStatObj_SummaryMeshSize;
	uint32                            nStatObj_TotalCount; // Including sub-objects.

	uint32                            nChar_SummaryMeshSize;
	uint32                            nChar_SummaryTextureSize;
	uint32                            nChar_NumInstances;
	SAnimMemoryTracker                m_AnimMemoryTracking;

	float                             fLevelLoadTime;
	SDebugFPSInfo                     infoFPS;

	std::vector<StatObjInfo>          objects;
	std::vector<CharacterInfo>        characters;
	std::vector<ITexture*>            textures;
	std::vector<MeshInfo>             meshes;
	std::vector<SBrushMemInfo>        brushes;
	std::vector<IMaterial*>           materials;
	std::vector<ProfilerInfo>         profilers;
	std::vector<SPeakProfilerInfo>    peaks;
	std::vector<SModuleProfilerInfo>  moduleprofilers;
	std::vector<SAnimationStatistics> animations;
	#if defined(ENABLE_LOADING_PROFILER)
	std::vector<SLoadingProfilerInfo> loading;
	#endif
	std::vector<SEntityInfo>          entities;

	MemInfo                           memInfo;
};

inline bool CompareFrameProfilersValueStats(const SCryEngineStats::ProfilerInfo& p1, const SCryEngineStats::ProfilerInfo& p2)
{
	return p1.m_displayedValue > p2.m_displayedValue;
}

class CEngineStats
{
	CEngineStats(bool bDepends)
	{
		m_ResourceCollector.Enable(bDepends);
		Collect();
	}

private: // ----------------------------------------------------------------------------

	// Collect all stats.
	void Collect();

	void CollectGeometry();
	void CollectCharacters();
	void CollectMaterialDependencies();
	void CollectTextures();
	void CollectMaterials();
	void CollectVoxels();
	void CollectRenderMeshes();
	void CollectBrushes();
	void CollectEntityDependencies();
	void CollectEntities();
	void CollectMemInfo();
	void CollectProfileStatistics();
	void CollectAnimations();
	void CollectLoadingData();

	// Arguments:
	//   pObj - 0 is ignored
	//   pMat - 0 if IStatObjet Material should be used
	void AddResource_StatObjWithLODs(IStatObj* pObj, CrySizerImpl& statObjTextureSizer, IMaterial* pMat = 0);
	// If the object was not previously registered, returns true. If the object was already registered, returns false.
	bool AddResource_SingleStatObj(IStatObj& rData);
	//
	void AddResource_CharInstance(ICharacterInstance& rData);
	//
	void AddResource_Material(IMaterial& rData, const bool bSubMaterial = false);

	CResourceCollector m_ResourceCollector; // dependencies between assets
	SCryEngineStats    m_stats;             //

	friend void SaveLevelStats(IConsoleCmdArgs* pArgs);
};

//////////////////////////////////////////////////////////////////////////
void CEngineStats::Collect()
{
	//////////////////////////////////////////////////////////////////////////
	// Collect CGFs
	//////////////////////////////////////////////////////////////////////////
	CollectMemInfo();                       // First of all collect memory info for modules (must be first).
	CollectGeometry();
	CollectCharacters();
	CollectTextures();
	CollectMaterials();
	CollectRenderMeshes();
	CollectBrushes();
	CollectMaterialDependencies();
	CollectEntityDependencies();
	CollectEntities();
	CollectProfileStatistics();
	CollectAnimations();
	//CollectLoadingData();
}

inline bool CompareMaterialsByName(IMaterial* pMat1, IMaterial* pMat2)
{
	return pMat1->GetName() > pMat2->GetName();
}

inline bool CompareTexturesBySizeFunc(ITexture* pTex1, ITexture* pTex2)
{
	return pTex1->GetDataSize() > pTex2->GetDataSize();
}
inline bool CompareStatObjBySizeFunc(const SCryEngineStats::StatObjInfo& s1, const SCryEngineStats::StatObjInfo& s2)
{
	return (s1.nMeshSize + s1.nTextureSize) > (s2.nMeshSize + s2.nTextureSize);
}
inline bool CompareCharactersBySizeFunc(const SCryEngineStats::CharacterInfo& s1, const SCryEngineStats::CharacterInfo& s2)
{
	return (s1.nMeshSize + s1.nTextureSize) > (s2.nMeshSize + s2.nTextureSize);
}
inline bool CompareRenderMeshByTypeName(IRenderMesh* pRM1, IRenderMesh* pRM2)
{
	return strcmp(pRM1->GetTypeName(), pRM2->GetTypeName()) < 0;
}

bool CEngineStats::AddResource_SingleStatObj(IStatObj& rData)
{
	if (!m_ResourceCollector.AddResource(rData.GetFilePath()))
		return false;   // was already registered

	// dependencies

	if (rData.GetMaterial())
	{
		m_ResourceCollector.OpenDependencies(rData.GetFilePath());

		AddResource_Material(*rData.GetMaterial());

		m_ResourceCollector.CloseDependencies();
	}
	return true;
}

void CEngineStats::AddResource_CharInstance(ICharacterInstance& rData)
{
	IMaterial* pMat = rData.GetIMaterial();

	if (!m_ResourceCollector.AddResource(rData.GetFilePath()))
		return;   // was already registered

	// dependencies

	if (rData.GetIMaterial())
	{
		m_ResourceCollector.OpenDependencies(rData.GetFilePath());

		AddResource_Material(*rData.GetIMaterial());

		m_ResourceCollector.CloseDependencies();
	}
}

void CEngineStats::AddResource_Material(IMaterial& rData, const bool bSubMaterial)
{
	if (!bSubMaterial)
	{
		string sName = string(rData.GetName()) + ".mtl";

		if (!m_ResourceCollector.AddResource(sName))
			return; // was already registered

		// dependencies

		m_ResourceCollector.OpenDependencies(sName);
	}

	{
		SShaderItem& rItem = rData.GetShaderItem();

		uint32 dwSubMatCount = rData.GetSubMtlCount();

		for (uint32 dwSubMat = 0; dwSubMat < dwSubMatCount; ++dwSubMat)
		{
			IMaterial* pSub = rData.GetSubMtl(dwSubMat);

			if (pSub)
				AddResource_Material(*pSub, true);
		}

		// this material
		if (rItem.m_pShaderResources)
			for (uint32 dwI = 0; dwI < EFTT_MAX; ++dwI)
			{
				SEfResTexture* pTex = rItem.m_pShaderResources->GetTexture(dwI);

				if (!pTex)
					continue;

				uint32 dwSize = 0xffffffff;
				/*
				   if(pTex->m_Sampler.m_pITex)
				   {
				   dwSize = pTex->m_Sampler.m_pITex->GetDataSize();

				   assert(pTex->m_Name);
				   assert(pTex->m_Sampler.m_pITex->GetName());

				   string sTex = PathUtil::ToUnixPath(PathUtil::ReplaceExtension(pTex->m_Name,""));
				   string sSampler = PathUtil::ToUnixPath(PathUtil::ReplaceExtension(pTex->m_Sampler.m_pITex->GetName(),""));

				   if(stricmp(sTex.c_str(),sSampler.c_str())!=0)
				   {
				   char str[1024];

				   cry_sprintf(str,"IGNORE  '%s' '%s'\n",sTex.c_str(),sSampler.c_str());
				   OutputDebugString(str);
				   dwSize=0;
				   //				IGNORE  'Textures/gradf' 'Editor/Objects/gradf'
				   //				IGNORE  'textures/cubemaps/auto_cubemap' '$RT_CM'
				   //				IGNORE  '' 'Textures/Defaults/White_ddn'
				   //				IGNORE  '' 'textures/defaults/oceanwaves_ddn'
				   //				IGNORE  '' 'textures/sprites/fire_blur1_ddn'
				   //				IGNORE  '' 'textures/sprites/fire_blur1_ddn'
				   //				IGNORE  '' 'Game/Objects/Library/Barriers/Sandbags/sandbags_ddn'
				   //				IGNORE  '' 'objects/characters/human/us/nanosuit/nanosuit_ddn'
				   //				IGNORE  '' 'objects/characters/human/us/nanosuit/nanosuit_ddndif'
				   }

				   m_ResourceCollector.AddResource(pTex->m_Sampler.m_pITex->GetName(),dwSize);		// used texture
				   }
				   else
				 */
				//		CryLog("AddResource ITex (%d): '%s' '%s'",dwI,pTex->m_Name.c_str(),pTex->m_Sampler.m_pITex->GetName());

				if (pTex->m_Sampler.m_pITex)
					m_ResourceCollector.AddResource(pTex->m_Sampler.m_pITex->GetName(), dwSize);
				//		m_ResourceCollector.AddResource(pTex->m_Name,dwSize);

				IDynTextureSource* pDynTextureSrc = pTex->m_Sampler.m_pDynTexSource;
				if (pDynTextureSrc)
				{
					const char* pStr = pDynTextureSrc->GetSourceFilePath();
					if (pStr)
						m_ResourceCollector.AddResource(pStr);
				}
			}
	}

	if (!bSubMaterial)
		m_ResourceCollector.CloseDependencies();
}

PREFAST_SUPPRESS_WARNING(6262)
void CEngineStats::AddResource_StatObjWithLODs(IStatObj* pObj, CrySizerImpl& statObjTextureSizer, IMaterial* pMat)
{
	if (!pObj)
		return;

	// Make sure we have not already registered this object
	if (!AddResource_SingleStatObj(*pObj))
		return;

	SCryEngineStats::StatObjInfo si;

	si.pStatObj = pObj;

	memset(si.nIndicesPerLod, 0, sizeof(si.nIndicesPerLod));

	CrySizerImpl localTextureSizer;

	si.nLods = 0;
	si.nDrawCalls = 0;
	si.nSubMeshCount = 0;
	si.nNumRefs = 0;
	si.bSplitLods = false;
	// Analyze geom object.

	bool bMultiSubObj = (si.pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND) != 0;

	si.nMeshSize = 0;
	si.nTextureSize = 0;
	si.nIndices = 0;
	si.nVertices = 0;
	si.nPhysProxySize = 0;
	si.nPhysProxySizeMax = 0;
	si.nPhysPrimitives = 0;

	m_stats.nStatObj_TotalCount++;

	IStatObj::SStatistics stats;
	stats.pTextureSizer = &statObjTextureSizer;
	stats.pTextureSizer2 = &localTextureSizer;

	si.pStatObj->GetStatistics(stats);

	si.nVertices = stats.nVertices;
	si.nIndices = stats.nIndices;
	for (int i = 0; i < MAX_STATOBJ_LODS_NUM; i++)
		si.nIndicesPerLod[i] = stats.nIndicesPerLod[i];
	si.nMeshSize = stats.nMeshSize;
	si.nMeshSizeLoaded = stats.nMeshSizeLoaded;
	si.nPhysProxySize = stats.nPhysProxySize;
	si.nPhysProxySizeMax = stats.nPhysProxySizeMax;
	si.nPhysPrimitives = stats.nPhysPrimitives;
	si.nLods = stats.nLods;
	si.nDrawCalls = stats.nDrawCalls;
	si.nSubMeshCount = stats.nSubMeshCount;
	si.nNumRefs = stats.nNumRefs;
	si.bSplitLods = stats.bSplitLods;

	si.nTextureSize = localTextureSizer.GetTotalSize();

	m_stats.nStatObj_SummaryMeshSize += si.nMeshSize;
	m_stats.objects.push_back(si);
}

inline bool CompareAnimations(const SAnimationStatistics& p1, const SAnimationStatistics& p2)
{
	return p1.count > p2.count;
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectLoadingData()
{
	#if defined(ENABLE_LOADING_PROFILER)
	CLoadingProfilerSystem::FillProfilersList(m_stats.loading);
	#endif
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectAnimations()
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	m_stats.animations.clear();
	/*
	   size_t count = pSystem->GetIAnimationSystem()->GetIAnimEvents()->GetGlobalAnimCount();

	   //m_stats.animations.reserve(count);
	   for (size_t i = 0; i < count; ++i) {
	    SAnimationStatistics stat;
	    pSystem->GetIAnimationSystem()->GetIAnimEvents()->GetGlobalAnimStatistics(i, stat);
	    if (stat.count)
	      m_stats.animations.push_back(stat);
	   }
	   std::sort( m_stats.animations.begin(),m_stats.animations.end(),CompareAnimations );
	 */

}

void GetObjectsByType(EERType objectType, std::vector<IRenderNode*>& lstInstances)
{
	I3DEngine* p3DEngine = GetISystem()->GetI3DEngine();

	const uint32 dwCount = p3DEngine->GetObjectsByType(objectType);
	if (dwCount)
	{
		const uint32 numObjects = lstInstances.size();
		lstInstances.resize(numObjects + dwCount);
		p3DEngine->GetObjectsByType(objectType, &lstInstances[numObjects]);
	}
}

//////////////////////////////////////////////////////////////////////////
PREFAST_SUPPRESS_WARNING(6262)
void CEngineStats::CollectGeometry()
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	m_stats.nStatObj_SummaryTextureSize = 0;
	m_stats.nStatObj_SummaryMeshSize = 0;
	m_stats.nStatObj_TotalCount = 0;

	CrySizerImpl statObjTextureSizer;

	/*
	   SCryEngineStats::StatObjInfo si;
	   si.nLods = 0;
	   si.nSubMeshCount = 0;
	   // Analyze geom object.
	   si.nMeshSize = 0;
	   si.nTextureSize = 0;
	   si.nIndices = 0;
	   si.nVertices = 0;
	   si.nPhysProxySize = 0;
	   si.nPhysPrimitives = 0;
	 */

	// iterate through all IStatObj
	{
		int nObjCount = 0;

		p3DEngine->GetLoadedStatObjArray(0, nObjCount);
		if (nObjCount > 0)
		{
			const int numStatObjs = nObjCount;
			m_stats.objects.reserve(nObjCount);
			IStatObj** pObjects = new IStatObj*[nObjCount];
			p3DEngine->GetLoadedStatObjArray(pObjects, nObjCount);

			PREFAST_ASSUME(numStatObjs == nObjCount);
			for (int nCurObj = 0; nCurObj < nObjCount; nCurObj++)
				AddResource_StatObjWithLODs(pObjects[nCurObj], statObjTextureSizer, 0);

			delete[]pObjects;
		}
	}

	// iterate through all instances
	{
		std::vector<IRenderNode*> lstInstances;

		GetObjectsByType(eERType_Brush, lstInstances);
		GetObjectsByType(eERType_Vegetation, lstInstances);
		GetObjectsByType(eERType_Light, lstInstances);
		GetObjectsByType(eERType_Decal, lstInstances);
		GetObjectsByType(eERType_Character, lstInstances);
		GetObjectsByType(eERType_MovableBrush, lstInstances);

		std::vector<IRenderNode*>::const_iterator itEnd = lstInstances.end();
		for (std::vector<IRenderNode*>::iterator it = lstInstances.begin(); it != itEnd; ++it)
		{
			IRenderNode* pRenderNode = *it;

			if (IStatObj* pEntObject = pRenderNode->GetEntityStatObj())
			{
				AddResource_StatObjWithLODs(pEntObject, statObjTextureSizer, 0); // Ensure object is registered
				m_ResourceCollector.AddInstance(pEntObject->GetFilePath(), pRenderNode);

				if (IMaterial* pMat = pRenderNode->GetMaterial())    // if this rendernode overwrites the IStatObj material
				{
					m_ResourceCollector.OpenDependencies(pEntObject->GetFilePath());
					AddResource_Material(*pMat);                    // to report the dependencies of this instance to the IStatObj
					m_ResourceCollector.CloseDependencies();
				}
			}

			if (ICharacterInstance* pCharInst = pRenderNode->GetEntityCharacter(0))
			{
				AddResource_CharInstance(*pCharInst);
				m_ResourceCollector.AddInstance(pCharInst->GetFilePath(), pRenderNode);
			}
		}
	}

	m_stats.nStatObj_SummaryTextureSize += statObjTextureSizer.GetTotalSize();
	std::sort(m_stats.objects.begin(), m_stats.objects.end(), CompareStatObjBySizeFunc);
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectCharacters() PREFAST_SUPPRESS_WARNING(6262)
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	m_stats.nChar_SummaryTextureSize = 0;
	m_stats.nChar_SummaryMeshSize = 0;
	m_stats.nChar_NumInstances = 0;

	CrySizerImpl totalCharactersTextureSizer;

	uint32 nObjCount = 0;
	ICharacterManager* pICharacterManager = pSystem->GetIAnimationSystem();

	if (!pICharacterManager)
		return;

	m_stats.m_AnimMemoryTracking = pICharacterManager->GetAnimMemoryTracker();

	pICharacterManager->GetLoadedModels(0, nObjCount);
	if (nObjCount > 0)
	{
		const int numLoadedModels = nObjCount;
		m_stats.characters.reserve(nObjCount);
		IDefaultSkeleton** pObjects = new IDefaultSkeleton*[nObjCount];
		pICharacterManager->GetLoadedModels(pObjects, nObjCount);
		PREFAST_ASSUME(numLoadedModels == nObjCount);
		for (uint32 nCurObj = 0; nCurObj < nObjCount; nCurObj++)
		{
			if (!pObjects[nCurObj])
				continue;

			// Do not consider cga files characters (they are already considered to be static geometries)
			//if (stricmp(PathUtil::GetExt(pObjects[nCurObj]->GetModelFilePath()),"cga") == 0)
			//	continue;

			SCryEngineStats::CharacterInfo si;
			si.pIDefaultSkeleton = pObjects[nCurObj];
			if (!si.pIDefaultSkeleton)
				continue;

			// dependencies
			//			AddResource(*si.pModel);

			CrySizerImpl textureSizer;

			si.nInstances = gEnv->pCharacterManager->GetNumInstancesPerModel(*si.pIDefaultSkeleton);
			si.nLods = 1;      //the base-model can have only 1 LOD
			si.nIndices = 0;
			si.nVertices = 0;
			memset(si.nVerticesPerLod, 0, sizeof(si.nVerticesPerLod));
			memset(si.nIndicesPerLod, 0, sizeof(si.nIndicesPerLod));

			CrySizerImpl meshSizer;

			si.nMeshSize = si.pIDefaultSkeleton->GetMeshMemoryUsage(&meshSizer);
			si.pIDefaultSkeleton->GetTextureMemoryUsage2(&textureSizer);
			si.pIDefaultSkeleton->GetTextureMemoryUsage2(&totalCharactersTextureSizer);
			si.nPhysProxySize = 0;
			bool bLod0_Found = false;
			IRenderMesh* pRenderMesh = si.pIDefaultSkeleton->GetIRenderMesh();
			if (pRenderMesh)
			{
				if (!bLod0_Found)
				{
					bLod0_Found = true;
					si.nVertices = pRenderMesh->GetVerticesCount();
					si.nIndices = pRenderMesh->GetIndicesCount();
				}
				si.nVerticesPerLod[0] = pRenderMesh->GetVerticesCount();
				si.nIndicesPerLod[0] = pRenderMesh->GetIndicesCount();
			}
			const phys_geometry* pgeom;
			{
				for (int i = si.pIDefaultSkeleton->GetJointCount() - 1; i >= 0; i--)
					if (pgeom = si.pIDefaultSkeleton->GetJointPhysGeom((uint32)i))
					{
						CrySizerImpl physMeshSizer;
						pgeom->pGeom->GetMemoryStatistics(&physMeshSizer);
						si.nPhysProxySize += physMeshSizer.GetTotalSize();
					}
			}
			si.nTextureSize = textureSizer.GetTotalSize();

			m_stats.nChar_SummaryMeshSize += si.nMeshSize;
			m_stats.characters.push_back(si);

			m_stats.nChar_NumInstances += si.nInstances;
		}
		delete[]pObjects;
	}
	m_stats.nChar_SummaryTextureSize = totalCharactersTextureSizer.GetTotalSize();
	std::sort(m_stats.characters.begin(), m_stats.characters.end(), CompareCharactersBySizeFunc);
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectEntityDependencies()
{
	ISystem* pSystem = GetISystem();

	IEntitySystem* pEntitySystem = pSystem->GetIEntitySystem();

	IEntityItPtr it = pEntitySystem->GetEntityIterator();
	while (!it->IsEnd())
	{
		IEntity* pEntity = it->Next();

		IMaterial* pMat = pEntity->GetMaterial();

		if (pMat)
			AddResource_Material(*pMat);

		uint32 dwSlotCount = pEntity->GetSlotCount();

		for (uint32 dwI = 0; dwI < dwSlotCount; ++dwI)
		{
			SEntitySlotInfo slotInfo;

			if (pEntity->GetSlotInfo(dwI, slotInfo))
			{
				if (slotInfo.pMaterial)
					AddResource_Material(*slotInfo.pMaterial);

				if (slotInfo.pCharacter)
					AddResource_CharInstance(*slotInfo.pCharacter);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectEntities()
{
	ISystem* pSystem = GetISystem();
	IEntitySystem* pEntitySystem = pSystem->GetIEntitySystem();
	IEntityItPtr it = pEntitySystem->GetEntityIterator();

	m_stats.entities.clear();

	while (!it->IsEnd())
	{
		IEntity* pEntity = it->Next();
		SCryEngineStats::SEntityInfo entityInfo;

		entityInfo.name = pEntity->GetName();
		entityInfo.bInvisible = !pEntity->IsInvisible();
		entityInfo.bHidden = pEntity->IsHidden();

		for (size_t j = 0, jCount = pEntity->GetSlotCount(); j < jCount; ++j)
		{
			if (pEntity->GetStatObj(j))
			{
				entityInfo.models += string(pEntity->GetStatObj(j)->GetFilePath()) + string(";");
			}
		}

		m_stats.entities.push_back(entityInfo);
	}

	m_stats.nSummaryEntityCount = m_stats.entities.size();
}
//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectBrushes()
{
	std::vector<IRenderNode*> lstInstances;
	GetObjectsByType(eERType_Brush, lstInstances);

	std::vector<IRenderNode*>::const_iterator itEnd = lstInstances.end();
	for (std::vector<IRenderNode*>::iterator it = lstInstances.begin(); it != itEnd; ++it)
	{
		IRenderNode* pRenderNode = *it;

		if (IStatObj* pEntObject = pRenderNode->GetEntityStatObj())
		{
			IMaterial* pMat = NULL;
			pMat = pRenderNode->GetMaterial();
			if (!pMat)
			{
				pMat = pEntObject->GetMaterial();
			}

			if (pMat)
			{
				for (int idx = 0; idx < 8; idx++)
				{
					if (IRenderMesh* pMesh = pRenderNode->GetRenderMesh(idx))
					{
						SCryEngineStats::SBrushMemInfo brushInfo;
						brushInfo.brushName = string(pRenderNode->GetName());
						brushInfo.usedTextureMemory = pMesh->GetTextureMemoryUsage(pMat);
						brushInfo.lodNum = idx;
						m_stats.brushes.push_back(brushInfo);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMaterialDependencies()
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	IMaterialManager* pManager = p3DEngine->GetMaterialManager();

	uint32 nObjCount = 0;
	pManager->GetLoadedMaterials(0, nObjCount);

	if (nObjCount > 0)
	{
		std::vector<IMaterial*> Materials;

		Materials.resize(nObjCount);

		pManager->GetLoadedMaterials(&Materials[0], nObjCount);

		std::vector<IMaterial*>::const_iterator it, end = Materials.end();

		for (it = Materials.begin(); it != end; ++it)
		{
			IMaterial* pMat = *it;

			AddResource_Material(*pMat);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectTextures()
{
	ISystem* pSystem = GetISystem();
	IRenderer* pRenderer = pSystem->GetIRenderer();
	if (!pRenderer)
	{
		return;
	}

	m_stats.nSummary_TextureSize = 0;
	m_stats.nSummary_UserTextureSize = 0;
	m_stats.nSummary_EngineTextureSize = 0;
	m_stats.nSummary_TexturesStreamingThroughput = 0;
	;
	pRenderer->EF_Query(EFQ_TexturesPoolSize, m_stats.nSummary_TexturesPoolSize);

	m_stats.textures.clear();
	SRendererQueryGetAllTexturesParam query;
	pRenderer->EF_Query(EFQ_GetAllTextures, query);
	if (query.pTextures)
	{
		m_stats.textures.reserve(query.numTextures);

		for (uint32 i = 0; i < query.numTextures; i++)
		{
			ITexture* pTexture = query.pTextures[i];
			int nTexSize = pTexture->GetDataSize();
			if (nTexSize > 0)
			{
				m_stats.textures.push_back(pTexture);
				m_stats.nSummary_TextureSize += nTexSize;

				if (pTexture->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
				{
					int numCopies = 1;
					numCopies += (pTexture->GetFlags() & FT_STAGE_UPLOAD) ? 1 : 0;
					numCopies += (pTexture->GetFlags() & FT_STAGE_READBACK) ? 1 : 0;

					m_stats.nSummary_EngineTextureSize += numCopies * nTexSize;
				}
				else
				{
					m_stats.nSummary_UserTextureSize += nTexSize;
				}
			}
		}

		std::sort(m_stats.textures.begin(), m_stats.textures.end(), CompareTexturesBySizeFunc);
	}

	pRenderer->EF_Query(EFQ_GetAllTexturesRelease, query);

	STextureStreamingStats stats(false);
	m_stats.nSummary_TexturesStreamingThroughput = 0;
	pRenderer->EF_Query(EFQ_GetTexStreamingInfo, stats);
	m_stats.nSummary_TexturesStreamingThroughput = (float)stats.nThroughput;
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMaterials()
{
	ISystem* pSystem = GetISystem();
	I3DEngine* p3DEngine = pSystem->GetI3DEngine();

	IMaterialManager* pManager = p3DEngine->GetMaterialManager();

	uint32 nObjCount = 0;
	pManager->GetLoadedMaterials(0, nObjCount);

	m_stats.materials.resize(nObjCount);

	if (nObjCount > 0)
	{
		pManager->GetLoadedMaterials(&m_stats.materials[0], nObjCount);

		std::sort(m_stats.materials.begin(), m_stats.materials.end(), CompareMaterialsByName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectRenderMeshes()
{
	ISystem* pSystem = GetISystem();
	IRenderer* pRenderer = pSystem->GetIRenderer();
	if (!pRenderer)
	{
		return;
	}

	m_stats.meshes.clear();
	m_stats.meshes.reserve(100);

	int nVertices = 0;
	int nIndices = 0;

	IRenderMesh** pMeshes = NULL;
	uint32 nCount = 0;
	pRenderer->EF_Query(EFQ_GetAllMeshes, pMeshes, nCount);
	if (nCount > 0 && pMeshes)
	{
		// Sort meshes by name.
		std::sort(pMeshes, pMeshes + nCount, CompareRenderMeshByTypeName);

		CrySizerImpl* pTextureSizer = new CrySizerImpl();

		int nInstances = 0;
		int nMeshSizeSys = 0;
		int nMeshSizeDev = 0;
		const char* sMeshName = 0;
		const char* sLastMeshName = "";
		for (size_t i = 0; i < nCount; i++)
		{
			IRenderMesh* pMesh = pMeshes[i];
			sMeshName = pMesh->GetTypeName();

			if ((strcmp(sMeshName, sLastMeshName) != 0 && i != 0))
			{
				SCryEngineStats::MeshInfo mi;
				mi.nCount = nInstances;
				mi.nVerticesSum = nVertices;
				mi.nIndicesSum = nIndices;
				mi.nMeshSizeDev = nMeshSizeDev;
				mi.nMeshSizeSys = nMeshSizeSys;
				mi.nTextureSize = (int)pTextureSizer->GetTotalSize();
				mi.name = sLastMeshName;
				m_stats.meshes.push_back(mi);

				delete pTextureSizer;
				pTextureSizer = new CrySizerImpl();
				nInstances = 0;
				nMeshSizeSys = 0;
				nMeshSizeDev = 0;
				nVertices = 0;
				nIndices = 0;
			}
			sLastMeshName = sMeshName;
			nMeshSizeSys += pMesh->GetMemoryUsage(0, IRenderMesh::MEM_USAGE_ONLY_SYSTEM);    // Collect System+Video memory usage.
			nMeshSizeDev += pMesh->GetMemoryUsage(0, IRenderMesh::MEM_USAGE_ONLY_VIDEO);     // Collect System+Video memory usage.

			// Vlad: checking default material in render mesh makes little sense, meshes can be used with any material
			//pMesh->GetTextureMemoryUsage(pMesh->GetMaterial(), pTextureSizer);

			nVertices += pMesh->GetVerticesCount();
			nIndices += pMesh->GetIndicesCount();

			nInstances++;
		}
		if (nCount > 0 && sMeshName)
		{
			SCryEngineStats::MeshInfo mi;
			mi.nCount = nInstances;
			mi.nVerticesSum = nVertices;
			mi.nIndicesSum = nIndices;
			mi.nMeshSizeSys = nMeshSizeSys;
			mi.nMeshSizeDev = nMeshSizeDev;
			mi.nTextureSize = (int)pTextureSizer->GetTotalSize();
			mi.name = sMeshName;
			m_stats.meshes.push_back(mi);
		}

		delete pTextureSizer;

		delete[]pMeshes;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectVoxels()
{
}

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectProfileStatistics()
{
	ISystem* pSystem = GetISystem();
	IFrameProfileSystem* pProfiler = pSystem->GetIProfileSystem();

	m_stats.profilers.clear();

	uint32 num = pProfiler->GetProfilerCount();                                             //min(20, pProfiler->GetProfilerCount());

	int need = 0;

	for (uint32 i = 0; i < num; ++i)
	{
		CFrameProfiler* pFrameInfo = pProfiler->GetProfiler(i);
		if (pFrameInfo && pFrameInfo->m_count.Average() > 0 && pFrameInfo->m_totalTime.Average() > 0.0f)
			++need;
	}

	m_stats.profilers.resize(need);
	for (uint32 j = 0, i = 0; j < num; ++j)
	{
		CFrameProfiler* pFrameInfo = pProfiler->GetProfiler(j);

		if (pFrameInfo && pFrameInfo->m_count.Average() > 0 && pFrameInfo->m_totalTime.Average() > 0.0f)
		{

			m_stats.profilers[i].m_count = pFrameInfo->m_count.Average();           //pFrameInfo->m_count;
			m_stats.profilers[i].m_displayedValue = pFrameInfo->m_selfTime.Average();
			m_stats.profilers[i].m_name = pFrameInfo->m_name;
			m_stats.profilers[i].m_module = ((CFrameProfileSystem*)pProfiler)->GetModuleName(pFrameInfo);
			m_stats.profilers[i].m_selfTime = pFrameInfo->m_selfTime;
			m_stats.profilers[i].m_totalTime = pFrameInfo->m_totalTime.Average();
			m_stats.profilers[i].m_variance = pFrameInfo->m_variance;
			m_stats.profilers[i].m_min = (float)pFrameInfo->m_selfTime.Min();
			m_stats.profilers[i].m_max = (float)pFrameInfo->m_selfTime.Max();
			m_stats.profilers[i].m_mincount = pFrameInfo->m_count.Min();
			m_stats.profilers[i].m_maxcount = pFrameInfo->m_count.Max();
			i++;
		}
	}

	std::sort(m_stats.profilers.begin(), m_stats.profilers.end(), CompareFrameProfilersValueStats);

	// fill peaks
	num = pProfiler->GetPeaksCount();

	m_stats.peaks.resize(num);
	for (uint32 i = 0; i < num; ++i)
	{

		const SPeakRecord* pPeak = pProfiler->GetPeak(i);
		CFrameProfiler* pFrameInfo = pPeak->pProfiler;

		m_stats.peaks[i].peakValue = pPeak->peakValue;
		m_stats.peaks[i].averageValue = pPeak->averageValue;
		m_stats.peaks[i].variance = pPeak->variance;
		m_stats.peaks[i].pageFaults = pPeak->pageFaults;                                       // Number of page faults at this frame.
		m_stats.peaks[i].count = pPeak->count;                                                 // Number of times called for peak.
		m_stats.peaks[i].when = pPeak->when;                                                   // when it added.

		m_stats.peaks[i].profiler.m_count = pFrameInfo->m_count.Average();           //pFrameInfo->m_count;
		m_stats.peaks[i].profiler.m_displayedValue = pFrameInfo->m_selfTime.Average();
		m_stats.peaks[i].profiler.m_name = pFrameInfo->m_name;
		m_stats.peaks[i].profiler.m_module = ((CFrameProfileSystem*)pProfiler)->GetModuleName(pFrameInfo);
		m_stats.peaks[i].profiler.m_selfTime = pFrameInfo->m_selfTime;
		m_stats.peaks[i].profiler.m_totalTime = pFrameInfo->m_totalTime.Average();
		m_stats.peaks[i].profiler.m_variance = pFrameInfo->m_variance;
		m_stats.peaks[i].profiler.m_min = (float)pFrameInfo->m_selfTime.Min();
		m_stats.peaks[i].profiler.m_max = (float)pFrameInfo->m_selfTime.Max();
		m_stats.peaks[i].profiler.m_mincount = pFrameInfo->m_count.Min();
		m_stats.peaks[i].profiler.m_maxcount = pFrameInfo->m_count.Max();
	}

	int modules = ((CFrameProfileSystem*)pProfiler)->GetModuleCount();
	m_stats.moduleprofilers.resize(modules);

	for (int i = 0; i < modules; i++)
	{
		float ratio = ((CFrameProfileSystem*)pProfiler)->GetOverBudgetRatio(i);
		m_stats.moduleprofilers[i].name = ((CFrameProfileSystem*)pProfiler)->GetModuleName(i);
		m_stats.moduleprofilers[i].overBugetRatio = ratio;
	}

}

//////////////////////////////////////////////////////////////////////////
	#if CRY_PLATFORM_WINDOWS

/*static*/ bool QueryModuleMemoryInfo(SCryEngineStatsModuleInfo& moduleInfo, int index)
{
	HMODULE hModule = GetModuleHandle(moduleInfo.name);
	if (!hModule)
		return false;

	typedef void (* PFN_MODULEMEMORY)(CryModuleMemoryInfo*);
	PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress(hModule, "CryModuleGetMemoryInfo");
	if (!fpCryModuleGetAllocatedMemory)
		return false;

	PEHeader_DLL pe_header;
	PEHeader_DLL* header = &pe_header;

	const IMAGE_DOS_HEADER* dos_head = (IMAGE_DOS_HEADER*)hModule;
	if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
	{
		// Wrong pointer, not to PE header.
		return false;
	}
	header = (PEHeader_DLL*)(const void*)((char*)dos_head + dos_head->e_lfanew);
	moduleInfo.moduleStaticSize = header->opt_head.SizeOfInitializedData + header->opt_head.SizeOfUninitializedData + header->opt_head.SizeOfCode + header->opt_head.SizeOfHeaders;
	moduleInfo.SizeOfCode = header->opt_head.SizeOfCode;
	moduleInfo.SizeOfInitializedData = header->opt_head.SizeOfInitializedData;
	moduleInfo.SizeOfUninitializedData = header->opt_head.SizeOfUninitializedData;

	fpCryModuleGetAllocatedMemory(&moduleInfo.memInfo);

	moduleInfo.usedInModule = (int)(moduleInfo.memInfo.allocated - moduleInfo.memInfo.freed);
	return true;
}

	#else //Another platform

/*static */ bool QueryModuleMemoryInfo(SCryEngineStatsModuleInfo& moduleInfo, int index)
{
	//CryModuleGetMemoryInfo( &moduleInfo.memInfo, (ECryModule)index );
	CryModuleGetMemoryInfo(&moduleInfo.memInfo);
	moduleInfo.usedInModule = (int)(moduleInfo.memInfo.allocated - moduleInfo.memInfo.freed);
	return true;
}

	#endif

//////////////////////////////////////////////////////////////////////////
void CEngineStats::CollectMemInfo()
{
	m_stats.memInfo.totalUsedInModules = 0;
	m_stats.memInfo.totalCodeAndStatic = 0;
	m_stats.memInfo.countedMemoryModules = 0;
	m_stats.memInfo.totalAllocatedInModules = 0;
	m_stats.memInfo.totalNumAllocsInModules = 0;

	const std::vector<string>& moduleNames = GetModuleNames();
	const int numModules = moduleNames.size();

	//////////////////////////////////////////////////////////////////////////
	// Hardcoded value for the OS memory allocation.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < numModules; i++)
	{
		const char* szModule = moduleNames[i].c_str();

		SCryEngineStatsModuleInfo moduleInfo;
		ZeroStruct(moduleInfo.memInfo);
		moduleInfo.moduleStaticSize = moduleInfo.SizeOfCode = moduleInfo.SizeOfInitializedData = moduleInfo.SizeOfUninitializedData = moduleInfo.usedInModule = 0;
		moduleInfo.name = szModule;

		if (!QueryModuleMemoryInfo(moduleInfo, i))
			continue;

		m_stats.memInfo.totalNumAllocsInModules += moduleInfo.memInfo.num_allocations;
		m_stats.memInfo.totalAllocatedInModules += moduleInfo.memInfo.allocated;
		m_stats.memInfo.totalUsedInModules += moduleInfo.usedInModule;
		m_stats.memInfo.countedMemoryModules++;
		m_stats.memInfo.totalCodeAndStatic += moduleInfo.moduleStaticSize;

		m_stats.memInfo.modules.push_back(moduleInfo);
	}

	m_stats.memInfo.m_pSizer = new CrySizerImpl();
	((CSystem*)GetISystem())->CollectMemStats(m_stats.memInfo.m_pSizer, CSystem::nMSP_ForDump);
	m_stats.memInfo.m_pStats = new CrySizerStats(m_stats.memInfo.m_pSizer);

}

// Exports engine stats to the Excel.
class CStatsToExcelExporter
{
public:
	enum CellFlags
	{
		CELL_BOLD     = 0x0001,
		CELL_CENTERED = 0x0002,
	};
	void ExportToFile(SCryEngineStats& stats, const char* filename);
	void ExportDependenciesToFile(CResourceCollector& stats, const char* filename);
	void Export(XmlNodeRef Workbook, SCryEngineStats& stats);

private:
	void       ExportSummary(SCryEngineStats& stats);
	void       ExportStatObjects(SCryEngineStats& stats);
	void       ExportCharacters(SCryEngineStats& stats);
	void       ExportRenderMeshes(SCryEngineStats& stats);
	void       ExportBrushes(SCryEngineStats& stats);
	void       ExportTextures(SCryEngineStats& stats);
	void       ExportMaterials(SCryEngineStats& stats);
	void       ExportMemStats(SCryEngineStats& stats);
	void       ExportMemInfo(SCryEngineStats& stats);
	void       ExportTimeDemoInfo();
	void       ExportStreamingInfo(SStreamEngineStatistics& stats);
	void       ExportAnimationInfo(SCryEngineStats& stats);
	void       ExportDependencies(CResourceCollector& stats);
	void       ExportProfilerStatistics(SCryEngineStats& stats);
	void       ExportAnimationStatistics(SCryEngineStats& stats);
	void       ExportAllLoadingStatistics(SCryEngineStats& stats);
	void       ExportLoadingStatistics(SCryEngineStats& stats);
	void       ExportFPSBuckets();
	void       ExportPhysEntStatistics(SCryEngineStats& stats);
	void       ExportEntitiesStatistics(SCryEngineStats& stats);

	void       InitExcelWorkbook(XmlNodeRef Workbook);

	XmlNodeRef NewWorksheet(const char* name);
	void       FreezeFirstRow();
	void       AutoFilter(int nRow, int nNumColumns);
	void       AddCell(float number);
	void       AddCell(int number);
	void       AddCell(uint32 number);
	void       AddCell(uint64 number) { AddCell((uint32)number); };
	void       AddCell(int64 number)  { AddCell((int)number); };
	void       AddCell(const char* str, int flags = 0);
	void       AddCellAtIndex(int nIndex, const char* str, int flags = 0);
	void       SetCellFlags(XmlNodeRef cell, int flags);
	void       AddRow();
	void       AddCell_SumOfRows(int nRows);
	string     GetXmlHeader();

private:
	XmlNodeRef m_Workbook;
	XmlNodeRef m_CurrTable;
	XmlNodeRef m_CurrWorksheet;
	XmlNodeRef m_CurrRow;
	XmlNodeRef m_CurrCell;
};

//////////////////////////////////////////////////////////////////////////
string CStatsToExcelExporter::GetXmlHeader()
{
	return "<?xml version=\"1.0\"?>\n<?mso-application progid=\"Excel.Sheet\"?>\n";
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::InitExcelWorkbook(XmlNodeRef Workbook)
{
	m_Workbook = Workbook;
	m_Workbook->setTag("Workbook");
	m_Workbook->setAttr("xmlns", "urn:schemas-microsoft-com:office:spreadsheet");
	XmlNodeRef ExcelWorkbook = Workbook->newChild("ExcelWorkbook");
	ExcelWorkbook->setAttr("xmlns", "urn:schemas-microsoft-com:office:excel");

	XmlNodeRef Styles = m_Workbook->newChild("Styles");
	{
		// Style s25
		// Bold header, With Background Color.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s25");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color", "#00FF00");
		StyleInterior->setAttr("ss:Pattern", "Solid");
		XmlNodeRef NumberFormat = Style->newChild("NumberFormat");
		NumberFormat->setAttr("ss:Format", "#,##0");
	}
	{
		// Style s26
		// Bold/Centered header.
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s26");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
		XmlNodeRef StyleInterior = Style->newChild("Interior");
		StyleInterior->setAttr("ss:Color", "#FFFF99");
		StyleInterior->setAttr("ss:Pattern", "Solid");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
	}
	{
		// Style s20
		// Centered
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s20");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
	}
	{
		// Style s21
		// Bold
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s21");
		XmlNodeRef StyleFont = Style->newChild("Font");
		StyleFont->setAttr("x:CharSet", "204");
		StyleFont->setAttr("x:Family", "Swiss");
		StyleFont->setAttr("ss:Bold", "1");
	}
	{
		// Style s22
		// Centered, Integer Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s22");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
		XmlNodeRef NumberFormat = Style->newChild("NumberFormat");
		NumberFormat->setAttr("ss:Format", "#,##0");
	}
	{
		// Style s23
		// Centered, Float Number format
		XmlNodeRef Style = Styles->newChild("Style");
		Style->setAttr("ss:ID", "s23");
		XmlNodeRef Alignment = Style->newChild("Alignment");
		Alignment->setAttr("ss:Horizontal", "Center");
		Alignment->setAttr("ss:Vertical", "Bottom");
		//XmlNodeRef NumberFormat = Style->newChild( "NumberFormat" );
		//NumberFormat->setAttr( "ss:Format","#,##0" );
	}

	/*
	    <Style ss:ID="s25">
	    <Font x:CharSet="204" x:Family="Swiss" ss:Bold="1"/>
	    <Interior ss:Color="#FFFF99" ss:Pattern="Solid"/>
	    </Style>
	 */
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CStatsToExcelExporter::NewWorksheet(const char* name)
{
	m_CurrWorksheet = m_Workbook->newChild("Worksheet");
	m_CurrWorksheet->setAttr("ss:Name", name);
	m_CurrTable = m_CurrWorksheet->newChild("Table");
	return m_CurrWorksheet;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::FreezeFirstRow()
{
	XmlNodeRef options = m_CurrWorksheet->newChild("WorksheetOptions");
	options->setAttr("xmlns", "urn:schemas-microsoft-com:office:excel");
	options->newChild("FreezePanes");
	options->newChild("FrozenNoSplit");
	options->newChild("SplitHorizontal")->setContent("1");
	options->newChild("TopRowBottomPane")->setContent("1");
	options->newChild("ActivePane")->setContent("2");
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AutoFilter(int nRow, int nNumColumns)
{
	XmlNodeRef options = m_CurrWorksheet->newChild("AutoFilter");
	options->setAttr("xmlns", "urn:schemas-microsoft-com:office:excel");
	string range;
	range.Format("R%dC1:R%dC%d", nRow, nRow, nNumColumns);
	options->setAttr("x:Range", range);   // x:Range="R1C1:R1C8"
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddRow()
{
	m_CurrRow = m_CurrTable->newChild("Row");
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell_SumOfRows(int nRows)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	data->setContent("0");
	m_CurrCell = cell;

	if (nRows > 0)
	{
		char buf[128];
		cry_sprintf(buf, "=SUM(R[-%d]C:R[-1]C)", nRows);
		m_CurrCell->setAttr("ss:Formula", buf);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell(float number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s23");  // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%.3f", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell(int number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s22");  // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%d", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell(uint32 number)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:StyleID", "s22");  // Centered
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "Number");
	char str[128];
	cry_sprintf(str, "%u", number);
	data->setContent(str);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCell(const char* str, int nFlags)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "String");
	data->setContent(str);
	SetCellFlags(cell, nFlags);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::AddCellAtIndex(int nIndex, const char* str, int nFlags)
{
	XmlNodeRef cell = m_CurrRow->newChild("Cell");
	cell->setAttr("ss:Index", nIndex);
	XmlNodeRef data = cell->newChild("Data");
	data->setAttr("ss:Type", "String");
	data->setContent(str);
	SetCellFlags(cell, nFlags);
	m_CurrCell = cell;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::SetCellFlags(XmlNodeRef cell, int flags)
{
	if (flags & CELL_BOLD)
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID", "s26");
		else
			cell->setAttr("ss:StyleID", "s21");
	}
	else
	{
		if (flags & CELL_CENTERED)
			cell->setAttr("ss:StyleID", "s20");
	}
}

static FILE* HandleFileExport(const char* filename)
{
	FILE* file = NULL;

	string temp = g_szTestResults;
	char path[ICryPak::g_nMaxPath];
	path[sizeof(path) - 1] = 0;
	gEnv->pCryPak->AdjustFileName(temp, path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
	gEnv->pCryPak->MakeDir(path);
	gEnv->pCryPak->AdjustFileName(string(path) + "/" + filename, path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
	file = fopen(path, "wb");

	return file;
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportToFile(SCryEngineStats& stats, const char* filename)
{
	XmlNodeRef Workbook = GetISystem()->CreateXmlNode("Workbook");
	Export(Workbook, stats);
	string xml = GetXmlHeader();
	//	xml = xml + Workbook->getXML();
	FILE* file = HandleFileExport(filename);
	if (file)
	{
		fprintf(file, "%s", xml.c_str());
		Workbook->saveToFile(filename, 8 * 1024 /*chunksize*/, file);
		fclose(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportDependenciesToFile(CResourceCollector& stats, const char* filename)
{
	XmlNodeRef Workbook = GetISystem()->CreateXmlNode("Workbook");

	{
		InitExcelWorkbook(Workbook);
		ExportDependencies(stats);
	}

	string xml = GetXmlHeader();
	//	xml = xml + Workbook->getXML();

	FILE* file = HandleFileExport(filename);
	if (file)
	{
		fprintf(file, "%s", xml.c_str());
		Workbook->saveToFile(filename, 8 * 1024 /*chunksize*/, file);
		fclose(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::Export(XmlNodeRef Workbook, SCryEngineStats& stats)
{
	bool bPhysWasActive = gEnv->pSystem->SetThreadState(ESubsys_Physics, false) != 0;
	InitExcelWorkbook(Workbook);
	ExportSummary(stats);
	ExportMemInfo(stats);
	ExportMemStats(stats);
	ExportStatObjects(stats);
	ExportCharacters(stats);
	ExportRenderMeshes(stats);
	ExportBrushes(stats);
	ExportTextures(stats);
	ExportMaterials(stats);
	ExportTimeDemoInfo();
	ExportProfilerStatistics(stats);
	ExportAnimationStatistics(stats);
	//ExportAllLoadingStatistics(stats);
	ExportPhysEntStatistics(stats);
	ExportEntitiesStatistics(stats);
	ExportAnimationInfo(stats);

	SStreamEngineStatistics& streamingStats = gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics();
	ExportStreamingInfo(streamingStats);
	gEnv->pSystem->SetThreadState(ESubsys_Physics, bPhysWasActive);
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportSummary(SCryEngineStats& stats)
{
	// Make summary sheet.en
	NewWorksheet("Summary");

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 200);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);

	// get version
	const SFileVersion& ver = GetISystem()->GetFileVersion();
	char sVersion[128];
	ver.ToString(sVersion);
	string levelName = "no_level";
	ICVar* sv_map = gEnv->pConsole->GetCVar("sv_map");
	if (sv_map)
		levelName = sv_map->GetString();

	AddRow();
	AddCell(string("CryEngine Version ") + sVersion);
	AddRow();
	AddCell(string("Level ") + levelName);
	AddRow();
	#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	AddCell("Running in 64bit version");
	#else
	AddCell("Running in 32bit version");
	#endif
	AddRow();
	AddCell("Level Load Time (sec):");
	AddCell((int)stats.fLevelLoadTime);
	AddRow();
	AddRow();
	AddCell("Average\\Min\\Max (fps):");
	AddCell((int)stats.infoFPS.fAverageFPS);
	AddCell((int)stats.infoFPS.fMinFPS);
	AddCell((int)stats.infoFPS.fMaxFPS);
	AddRow();

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Resource Type (MB)");
	AddCell("Count");
	AddCell("Memory Size");
	AddCell("Only Mesh Size");
	AddCell("Only Texture Size");

	AddRow();
	AddCell("CGF Objects", CELL_BOLD);
	AddCell((uint32)stats.objects.size());
	AddCell((stats.nStatObj_SummaryTextureSize + stats.nStatObj_SummaryMeshSize) / (1024 * 1024));
	AddCell((stats.nStatObj_SummaryMeshSize) / (1024 * 1024));
	AddCell((stats.nStatObj_SummaryTextureSize) / (1024 * 1024));
	AddRow();
	AddCell("Character Models", CELL_BOLD);
	AddCell((uint32)stats.characters.size());
	AddCell((stats.nChar_SummaryTextureSize + stats.nChar_SummaryMeshSize) / (1024 * 1024));
	AddCell((stats.nChar_SummaryMeshSize) / (1024 * 1024));
	AddCell((stats.nChar_SummaryTextureSize) / (1024 * 1024));

	AddRow();
	AddCell("Character Instances", CELL_BOLD);
	AddCell(stats.nChar_NumInstances);

	AddRow();
	AddCell("Entities", CELL_BOLD);
	AddCell(stats.nSummaryEntityCount);

	//AddCell( stats.nMemCharactersTotalSize );
	AddRow();

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Textures");
	AddRow();
	AddCell("Count", CELL_BOLD);
	AddCell((uint32)stats.textures.size());
	AddCell("Total count of textures");
	AddRow();

	AddCell("Textures Overall Size", CELL_BOLD);
	AddCell(stats.nSummary_TextureSize / (1024 * 1024));
	AddCell("Total amount of textures memory usage");

	AddRow();
	AddCell("Pool Size", CELL_BOLD);
	AddCell(stats.nSummary_TexturesPoolSize / 1024 / 1024);
	AddCell("Size of textures pool");

	AddRow();
	AddCell("Textures Memory Usage", CELL_BOLD);
	AddCell((stats.nSummary_TexturesPoolSize + stats.nSummary_UserTextureSize + stats.nSummary_EngineTextureSize) / (1024 * 1024));
	AddCell("Total memory of textures in RAM");

	AddRow();
	AddCell("Textures Engine Only", CELL_BOLD);
	AddCell((stats.nSummary_EngineTextureSize) / (1024 * 1024));
	AddCell("Textures for internal Engine usage ");

	AddRow();
	AddCell("User Textures", CELL_BOLD);
	AddCell((stats.nSummary_UserTextureSize) / (1024 * 1024));
	AddCell("User textures not stored in the textures pool");

	AddRow();
	AddCell("Textures streaming throughput(KB/s)", CELL_BOLD);
	;
	if (stats.nSummary_TexturesStreamingThroughput > 0)
		AddCell((stats.nSummary_TexturesStreamingThroughput) / 1024);
	AddRow();

	//AddRow();
	//m_CurrRow->setAttr( "ss:StyleID","s25" );
	//AddCell( "Resource Type (MB)" );
	//AddCell( "Count" );
	//AddCell( "Total Size" );
	//AddCell( "System Memory" );
	//AddCell( "Video Memory" );
	//AddCell( "Engine Textures" );
	//AddCell( "User Textures" );
	//if(stats.nSummary_TexturesStreamingThroughput > 0)
	//	AddCell( "Textures streaming throughput(KB/s)" );

	//AddRow();
	//AddCell( "Textures",CELL_BOLD );
	//AddCell( stats.textures.size() );
	//AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	//AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	//AddCell( (stats.nSummary_TextureSize)/(1024*1024) );
	//AddCell( (stats.nSummary_EngineTextureSize)/(1024*1024) );
	//AddCell( (stats.nSummary_UserTextureSize)/(1024*1024) );
	//if(stats.nSummary_TexturesStreamingThroughput > 0)
	//	AddCell( (stats.nSummary_TexturesStreamingThroughput) / 1024 );

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Meshes");
	AddRow();
	AddCell("Count", CELL_BOLD);
	AddCell(stats.nSummaryMeshCount);
	AddRow();
	AddCell("Total Size", CELL_BOLD);
	AddCell((stats.nAPI_MeshSize + stats.nSummaryMeshSize) / (1024 * 1024));
	AddRow();
	AddCell("System Memory", CELL_BOLD);
	AddCell((stats.nSummaryMeshSize) / (1024 * 1024));
	AddRow();
	AddCell("Video Memory", CELL_BOLD);
	AddCell((stats.nAPI_MeshSize) / (1024 * 1024));
	AddRow();
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddRow();

	AddRow();
	AddCell("Lua Memory Usage (MB)");
	AddCell(stats.nSummaryScriptSize / (1024 * 1024));

	AddRow();
	AddCell("Game Memory Usage (MB)");
	AddCell(stats.nTotalAllocatedMemory / (1024 * 1024));
	uint64 totalAll = stats.memInfo.totalUsedInModules;
	totalAll += stats.nAPI_MeshSize;
	totalAll += stats.nSummary_UserTextureSize;
	totalAll += stats.nSummary_EngineTextureSize;
	totalAll += stats.memInfo.totalCodeAndStatic;
	AddRow();
	AddCell("Total Allocated (With Code/Textures/Mesh) (MB)");
	AddCell(totalAll / (1024 * 1024));
	AddRow();
	AddRow();
	AddCell("Virtual Memory Usage (MB)");
	AddCell(stats.nWin32_PagefileUsage / (1024 * 1024));
	AddRow();

	AddCell("Peak Virtual Memory Usage (MB)");
	AddCell(stats.nWin32_PeakPagefileUsage / (1024 * 1024));

	//FPS buckets
	ExportFPSBuckets();
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportStatObjects(SCryEngineStats& stats)
{
	NewWorksheet("Static Geometry");
	FreezeFirstRow();
	AutoFilter(1, 12);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 350);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 60);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 60);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 120);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Filename");
	AddCell("Refs");
	AddCell("Mesh Size (KB)");
	AddCell("Texture Size (KB)");
	AddCell("DrawCalls");
	AddCell("LODs");
	AddCell("Sub Meshes");
	AddCell("Vertices");
	AddCell("Tris");
	AddCell("Physics Tris");
	AddCell("Physics Size (KB)");
	AddCell("LODs Tris");
	AddCell("Mesh Size Loaded (KB)");
	AddCell("Split LODs");

	int nRows = (int)stats.objects.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::StatObjInfo& si = stats.objects[i];

		AddRow();
		AddCell((si.pStatObj) ? si.pStatObj->GetFilePath() : "");
		AddCell(si.nNumRefs);
		AddCell(si.nMeshSize / 1024);
		AddCell(si.nTextureSize / 1024);
		AddCell(si.nDrawCalls);
		AddCell(si.nLods);
		AddCell(si.nSubMeshCount);
		AddCell(si.nVertices);
		AddCell(si.nIndices / 3);
		AddCell(si.nPhysPrimitives);
		AddCell((si.nPhysProxySize + 512) / 1024);

		if (si.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < MAX_LODS; lod++)
			{
				if (si.nIndicesPerLod[lod] != 0)
				{
					cry_sprintf(numstr, "%d", (si.nIndicesPerLod[lod] / 3));
					if (numlods > 0)
						cry_strcat(tempstr, " / ");
					cry_strcat(tempstr, numstr);
					numlods++;
				}
			}
			if (numlods > 1)
				AddCell(tempstr);
		}
		else
		{
			AddCell("");
		}

		AddCell(si.nMeshSizeLoaded / 1024);
		if (si.bSplitLods)
		{
			AddCell("Yes");
		}
		else
		{
			AddCell("");
		}

	}

	/*
	   AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	   AddCell( "" );
	   AddCell_SumOfRows( nRows ); // Mesh size
	   AddCell_SumOfRows( nRows ); // Texture size
	   AddCell( "" ); // LODs
	   AddCell( "" ); // Sub Meshes
	   AddCell_SumOfRows( nRows ); // Vertices
	   AddCell_SumOfRows( nRows ); // Tris
	   AddCell_SumOfRows( nRows ); // Physics Tris
	   AddCell_SumOfRows( nRows ); // Physics Size
	   AddCell( "" ); // LODs Tris
	   AddCell_SumOfRows( nRows ); // Mesh Size Loaded
	 */
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportAllLoadingStatistics(SCryEngineStats& stats)
{
	ExportLoadingStatistics(stats);
}

struct CrySizerNaive : ICrySizer
{
	CrySizerNaive() : m_count(0), m_size(0) {}
	virtual void                Release()        {}
	virtual void                End()            {}
	virtual size_t              GetTotalSize()   { return m_size; }
	virtual size_t              GetObjectCount() { return m_count; }
	virtual IResourceCollector* GetResourceCollector()
	{
		assert(0);
		return (IResourceCollector*)0;
	}
	virtual void Push(const char*)                                     {}
	virtual void PushSubcomponent(const char*)                         {}
	virtual void Pop()                                                 {}
	virtual bool AddObject(const void* id, size_t size, int count = 1) { m_size += size * count; m_count++; return true; }
	virtual void Reset()                                               { m_size = 0; m_count = 0; }
	virtual void SetResourceCollector(IResourceCollector* pColl)       {};
	size_t m_count, m_size;
};

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportLoadingStatistics(SCryEngineStats& stats)
{
	#if defined(ENABLE_LOADING_PROFILER)
	NewWorksheet("Load Stats");

	FreezeFirstRow();

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Function");
	AddCell("Self (sec)");
	AddCell("Total (sec)");
	AddCell("Calls");
	AddCell("Memory (Mb)");
	AddCell("Read file size");
	AddCell("Bandwith self (Kb/s)");
	AddCell("Bandwith total (Kb/s)");
	AddCell("FOpens self");
	AddCell("FReads self");
	AddCell("FSeeks self");
	AddCell("FOpens total");
	AddCell("FReads total");
	AddCell("FSeeks total");

	int nRows = (int)stats.loading.size();
	for (int i = 0; i < nRows; i++)
	{
		SLoadingProfilerInfo& an = stats.loading[i];
		AddRow();
		AddCell(an.name);
		AddCell((float)an.selfTime);
		AddCell((float)an.totalTime);
		AddCell(an.callsTotal);
		AddCell((float)an.memorySize);
		AddCell((float)an.selfInfo.m_dOperationSize / 1024.0f);
		float bandwithSelf = an.selfTime > 0. ? (float)(an.selfInfo.m_dOperationSize / an.selfTime / 1024.0) : 0.0f;
		float bandwithTotal = an.totalTime > 0. ? (float)(an.totalInfo.m_dOperationSize / an.totalTime / 1024.0) : 0.0f;
		AddCell(bandwithSelf);
		AddCell(bandwithTotal);
		AddCell(an.selfInfo.m_nFileOpenCount);
		AddCell(an.selfInfo.m_nFileReadCount);
		AddCell(an.selfInfo.m_nSeeksCount);
		AddCell(an.totalInfo.m_nFileOpenCount);
		AddCell(an.totalInfo.m_nFileReadCount);
		AddCell(an.totalInfo.m_nSeeksCount);
	}
	#endif
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportPhysEntStatistics(SCryEngineStats& stats)
{
	NewWorksheet("Physical Entities");

	FreezeFirstRow();
	AutoFilter(1, 6);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 150);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 150);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 150);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 150);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Name");
	AddCell("Type");
	AddCell("SimClass");
	AddCell("Total Size");
	AddCell("Instanced Geom Size");
	AddCell("Grid Footprint");
	AddCell("RB_Intersect Unfriendly");
	AddCell("Cloth Unfriendly");
	AddCell("Rope Unfriendly");
	AddCell("Featherstone Unfriendly");

	IPhysicalEntityIt* iter = gEnv->pPhysicalWorld->GetEntitiesIterator();
	IPhysicalEntity* pent;
	PhysicsVars* pVars = gEnv->pPhysicalWorld->GetPhysVars();

	for (iter->MoveFirst(); pent = iter->Next(); )
	{
		AddRow();
		pe_params_foreign_data pfd;
		pent->GetParams(&pfd);
		AddCell(CPhysRenderer::GetPhysForeignName(pfd.pForeignData, pfd.iForeignData, pfd.iForeignFlags));

		pe_status_pos sp;
		pent->GetStatus(&sp);
		const char* entypes[] = { "none", "static", "rigidbody", "vehicle", "living", "particle", "character", "rope", "cloth", "area" };
		AddCell(entypes[pent->GetType()]);
		const char* simclass[] = { "hidden", "0-static", "1-sleeping", "2-active", "3-actor", "4-independent", "5-area", "6-trigger", "7-deleted" };
		AddCell(simclass[sp.iSimClass + 1]);

		CrySizerNaive sizer;
		pVars->iDrawHelpers |= 1 << 31;
		pent->GetMemoryStatistics(&sizer);
		int sizeTot = sizer.m_size;
		pVars->iDrawHelpers &= ~(1 << 31);
		sizer.Reset();
		pent->GetMemoryStatistics(&sizer);
		int sizeGrid = sizeTot - sizer.m_size;

		pe_params_part pp;
		sizer.Reset();

		char szFallbackReason[2048];
		size_t len = sizeof(szFallbackReason);
		memset(szFallbackReason, 0, sizeof(szFallbackReason));
		for (pp.ipart = 0; pent->GetParams(&pp); pp.ipart++)
		{
			size_t old_len = len;
			if (old_len != len)
			{
				char token[64];
				cry_sprintf(token, "[part %d]", pp.ipart);
				cry_strcat(szFallbackReason, token);
				len -= strlen(token);
			}

			if (pp.flagsAND & geom_can_modify)
			{
				pp.pPhysGeom->pGeom->GetMemoryStatistics(&sizer);
				if (pp.pPhysGeomProxy != pp.pPhysGeom)
				{
					pp.pPhysGeomProxy->pGeom->GetMemoryStatistics(&sizer);
				}
			}
			MARK_UNUSED pp.partid;
		}
		AddCell(sizeTot + (int)sizer.m_size);
		AddCell((uint32)sizer.m_size);
		AddCell(sizeGrid);

		szFallbackReason[sizeof(szFallbackReason) - 1] = 0;
		AddCell(strlen(szFallbackReason) ? szFallbackReason : " ");
		AddCell(pent->GetType() == PE_SOFT && sizeTot + sizer.m_size - sizeGrid > 90 << 10 ? "too big" : " ");
		AddCell(pent->GetType() == PE_ROPE && sizeTot + sizer.m_size - sizeGrid > 70 << 10 ? "too big" : " ");
		AddCell(pent->GetType() == PE_ARTICULATED && sizeTot + sizer.m_size - sizeGrid > 60 << 10 ? "too big" : " ");
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportEntitiesStatistics(SCryEngineStats& stats)
{
	NewWorksheet("Entities");

	FreezeFirstRow();
	AutoFilter(1, 3);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Name");
	AddCell("Models");
	AddCell("IsInvisible(but updated)");
	AddCell("IsHidden(and disabled)");

	for (size_t i = 0, iCount = stats.entities.size(); i < iCount; ++i)
	{
		AddRow();
		AddCell(stats.entities[i].name.c_str());
		AddCell(stats.entities[i].models.c_str());
		AddCell(stats.entities[i].bInvisible ? "Yes" : "No");
		AddCell(stats.entities[i].bHidden ? "Yes" : "No");
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportAnimationStatistics(SCryEngineStats& stats)
{
	if (stats.animations.empty())
		return;

	NewWorksheet("Animations");
	FreezeFirstRow();
	AutoFilter(1, 2);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Name");
	AddCell("Count");

	int nRows = (int)stats.animations.size();
	for (int i = 0; i < nRows; i++)
	{
		SAnimationStatistics& an = stats.animations[i];
		AddRow();
		AddCell(an.name);
		AddCell((uint32)an.count);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportProfilerStatistics(SCryEngineStats& stats)
{
	if (stats.profilers.empty())
		return;

	{

		NewWorksheet("Profiler");
		FreezeFirstRow();
		AutoFilter(1, 10);

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 100);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 300);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);

		//	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("Module");
		AddCell("Name");
		AddCell("Self time, ms");
		AddCell("Total time, ms");
		AddCell("Count");
		AddCell("");
		AddCell("Min time, ms");
		AddCell("Max time, ms");
		AddCell("Min count");
		AddCell("Max count");

		int nRows = (int)stats.profilers.size();
		for (int i = 0; i < nRows; i++)
		{
			SCryEngineStats::ProfilerInfo& pi = stats.profilers[i];
			AddRow();
			AddCell(pi.m_module);
			AddCell(pi.m_name);
			AddCell(pi.m_displayedValue);
			AddCell(pi.m_totalTime);
			AddCell(pi.m_count);
			AddCell("");
			AddCell(pi.m_min);
			AddCell(pi.m_max);
			AddCell(pi.m_mincount);
			AddCell(pi.m_maxcount);

		}

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("");
		AddCell("");
		AddCell_SumOfRows(nRows);
		AddCell_SumOfRows(nRows);
		AddCell_SumOfRows(nRows);
		//	AddCell("");
		AddCell("");
		AddCell("");
	}

	// Peaks

	NewWorksheet("Peaks");

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 50);
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );
	//Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",50 );

	//	Column = m_CurrTable->newChild("Column");	Column->setAttr( "ss:Width",40 );

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Module");
	AddCell("Name");
	AddCell("Peak, ms");
	//AddCell( "Self time, ms" );
	//AddCell( "Total time, ms" );
	AddCell("Count");
	//AddCell( "" );
	//AddCell( "Min time, ms" );
	//AddCell( "Max time, ms" );
	//AddCell( "Min count" );
	//AddCell( "Max count" );

	int nRows = (int)stats.peaks.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::SPeakProfilerInfo& peak = stats.peaks[i];
		SCryEngineStats::ProfilerInfo& pi = stats.peaks[i].profiler;
		AddRow();
		AddCell(pi.m_module);
		AddCell(pi.m_name);
		AddCell(peak.peakValue);
		//AddCell( pi.m_displayedValue );
		//AddCell( pi.m_totalTime );
		AddCell(peak.count);
		//AddCell( "");
		//AddCell( pi.m_min );
		//AddCell( pi.m_max );
		//AddCell( pi.m_mincount );
		//AddCell( pi.m_maxcount );
	}

	//AddRow(); m_CurrRow->setAttr( "ss:StyleID","s25" );
	//AddCell("");
	//AddCell("");
	//AddCell("");
	//AddCell_SumOfRows( nRows );
	//AddCell_SumOfRows( nRows );
	//AddCell_SumOfRows( nRows );
	////	AddCell("");
	//AddCell("");
	//AddCell("");

	NewWorksheet("Budget");
	{
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 100);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 300);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 50);

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("Module");
		AddCell("OverBudgetRatio%");

		nRows = (int)stats.moduleprofilers.size();
		for (int i = 0; i < nRows; i++)
		{
			SCryEngineStats::SModuleProfilerInfo& moduleProfile = stats.moduleprofilers[i];
			AddRow();
			AddCell(moduleProfile.name);
			AddCell(moduleProfile.overBugetRatio * 100);
		}
	}

}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportCharacters(SCryEngineStats& stats)
{
	NewWorksheet("Characters");
	FreezeFirstRow();
	AutoFilter(1, 10);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 150);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Filename");
	AddCell("Num Instances");
	AddCell("Mesh Size (KB)");
	AddCell("Texture Size (KB)");
	AddCell("PhysProxy Size (KB)");
	AddCell("LODs");
	AddCell("Vertices Lod0");
	AddCell("Tris Lod0");
	AddCell("All Vertices");
	AddCell("All Tris");
	AddCell("LOD Tris");

	int nRows = (int)stats.characters.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::CharacterInfo& si = stats.characters[i];
		AddRow();
		AddCell(si.pIDefaultSkeleton->GetModelFilePath());
		AddCell(si.nInstances);
		AddCell(si.nMeshSize / 1024);
		AddCell(si.nTextureSize / 1024);
		AddCell((si.nPhysProxySize + 512) >> 10);
		AddCell(si.nLods);
		AddCell(si.nVertices);
		AddCell(si.nIndices / 3);

		int nAllVerts = 0;
		for (int k = 0; k < MAX_LODS; k++)
			nAllVerts += si.nVerticesPerLod[k];
		AddCell(nAllVerts);

		int nAllIndices = 0;
		for (int k = 0; k < MAX_LODS; k++)
			nAllIndices += si.nIndicesPerLod[k];
		AddCell(nAllIndices / 3);

		if (si.nLods > 1)
		{
			// Print lod1/lod2/lod3 ...
			char tempstr[256];
			char numstr[32];
			tempstr[0] = 0;
			int numlods = 0;
			for (int lod = 0; lod < MAX_LODS; lod++)
			{
				if (si.nIndicesPerLod[lod] != 0)
				{
					cry_sprintf(numstr, "%d", (si.nIndicesPerLod[lod] / 3));
					if (numlods > 0)
						cry_strcat(tempstr, " / ");
					cry_strcat(tempstr, numstr);
					numlods++;
				}
			}
			if (numlods > 1)
				AddCell(tempstr);
		}
	}
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("");
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell("");
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportDependencies(CResourceCollector& stats)
{
	{
		NewWorksheet("Assets");
		FreezeFirstRow();

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 72);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 108);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 73);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 60);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 1000);

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("Instances");
		AddCell("Dependencies");
		AddCell("FileMem (KB)");
		AddCell("Extension");
		AddCell("FileName");

		std::vector<CResourceCollector::SAssetEntry>::const_iterator it, end = stats.m_Assets.end();
		uint32 dwAssetID = 0;

		for (it = stats.m_Assets.begin(); it != end; ++it, ++dwAssetID)
		{
			const CResourceCollector::SAssetEntry& rRef = *it;

			AddRow();

			char szAName1[1024];
			cry_sprintf(szAName1, "A%u %s", dwAssetID, rRef.m_sFileName.c_str());

			AddCell(rRef.m_dwInstanceCnt);
			AddCell(rRef.m_dwDependencyCnt);
			AddCell(rRef.m_dwFileSize != 0xffffffff ? rRef.m_dwFileSize / 1024 : 0);
			AddCell(PathUtil::GetExt(rRef.m_sFileName));
			AddCell(szAName1);
		}

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell_SumOfRows(dwAssetID);
		AddCell("");
		AddCell_SumOfRows(dwAssetID);
	}

	{
		NewWorksheet("Dependencies");
		FreezeFirstRow();

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 600);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 90);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 60);

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("Asset Filename");
		AddCell("Requires Sum (KB)");
		AddCell("Requires Count");

		std::set<CResourceCollector::SDependencyPair>::const_iterator it, end = stats.m_Dependencies.end();

		uint32 dwCurrentAssetID = 0xffffffff;
		uint32 dwSumFile = 0, dwSum = 0;

		for (it = stats.m_Dependencies.begin();; ++it)
		{
			uint32 dwAssetsSize = stats.m_Assets.size();

			if (it == end || (*it).m_idAsset != dwCurrentAssetID)
			{
				if (dwSum != 0 && dwSumFile != 0xffffffff)
				{
					assert(dwCurrentAssetID < dwAssetsSize);

					char szAName0[1024];
					cry_sprintf(szAName0, "A%u %s", dwCurrentAssetID, stats.m_Assets[dwCurrentAssetID].m_sFileName.c_str());

					AddRow();
					AddCell(szAName0);
					AddCell(dwSumFile / 1024);
					AddCell(dwSum);
				}

				dwSumFile = 0;
				dwSum = 0;

				if (it == end)
					break;
			}

			const CResourceCollector::SDependencyPair& rRef = *it;

			assert(rRef.m_idDependsOnAsset < dwAssetsSize);

			CResourceCollector::SAssetEntry& rDepAsset = stats.m_Assets[rRef.m_idDependsOnAsset];

			if (rDepAsset.m_dwFileSize != 0xffffffff)
				dwSumFile += rDepAsset.m_dwFileSize;

			++dwSum;

			dwCurrentAssetID = rRef.m_idAsset;
		}
	}

	{
		NewWorksheet("Detailed Dependencies");
		FreezeFirstRow();

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 600);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 1000);
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 70);

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");
		AddCell("Asset Filename");
		AddCell("Requires Filename");
		AddCell("Requires (KB)");

		std::set<CResourceCollector::SDependencyPair>::const_iterator it, end = stats.m_Dependencies.end();

		uint32 dwCurrentAssetID = 0xffffffff;
		uint32 dwSumFile = 0, dwSum = 0;

		for (it = stats.m_Dependencies.begin();; ++it)
		{
			if (it == end || (*it).m_idAsset != dwCurrentAssetID)
			{
				if (dwSum != 0 && dwSumFile != 0xffffffff)
				{
					//					AddRow(); m_CurrRow->setAttr( "ss:StyleID","s21" );
					//					AddCell("");
					//					AddCell_SumOfRows( dwSum );
					AddRow();
					AddCell("");
				}

				dwSumFile = 0;
				dwSum = 0;

				if (it == end)
					break;
				/*
				   const CResourceCollector::SDependencyPair &rRef = *it;

				   char szAName0[20];
				   cry_sprintf(szAName0,"A%d",rRef.m_idAsset);

				   AddRow(); m_CurrRow->setAttr( "ss:StyleID","s21" );
				   AddCell( szAName0 );
				   AddCell( stats.m_Assets[rRef.m_idAsset].m_sFileName.c_str() );
				 */
			}

			const CResourceCollector::SDependencyPair& rRef = *it;

			CResourceCollector::SAssetEntry& rDepAsset = stats.m_Assets[rRef.m_idDependsOnAsset];

			AddRow();

			char szAName0[1024];
			cry_sprintf(szAName0, "A%u %s", rRef.m_idAsset, stats.m_Assets[rRef.m_idAsset].m_sFileName.c_str());
			char szAName1[1024];
			cry_sprintf(szAName1, "A%u %s", rRef.m_idDependsOnAsset, rDepAsset.m_sFileName.c_str());

			AddCell(szAName0);
			AddCell(szAName1);
			AddCell(rDepAsset.m_dwFileSize != 0xffffffff ? rDepAsset.m_dwFileSize / 1024 : 0);

			if (rDepAsset.m_dwFileSize != 0xffffffff)
				dwSumFile += rDepAsset.m_dwFileSize;

			++dwSum;

			dwCurrentAssetID = rRef.m_idAsset;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportRenderMeshes(SCryEngineStats& stats)
{
	NewWorksheet("Meshes");
	FreezeFirstRow();
	AutoFilter(1, 8);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Mesh Type");
	AddCell("Num Instances");
	AddCell("Mesh Size Sys (KB)");
	AddCell("Mesh Size Dev (KB)");
	AddCell("Texture Size (KB)");
	AddCell("Total Vertices");
	AddCell("Total Tris");
	int nRows = (int)stats.meshes.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::MeshInfo& mi = stats.meshes[i];
		AddRow();
		AddCell(mi.name);
		AddCell(mi.nCount);
		AddCell(mi.nMeshSizeSys / 1024);
		AddCell(mi.nMeshSizeDev / 1024);
		AddCell(mi.nTextureSize / 1024);
		AddCell(mi.nVerticesSum);
		AddCell(mi.nIndicesSum / 3);
	}
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("");
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
}

//////////////////////////////////////////////////////////////////////////
bool SortBrushes(SCryEngineStats::SBrushMemInfo a, SCryEngineStats::SBrushMemInfo b)
{
	return a.usedTextureMemory > b.usedTextureMemory;
}

void CStatsToExcelExporter::ExportBrushes(SCryEngineStats& stats)
{
	NewWorksheet("Brushes");
	FreezeFirstRow();
	AutoFilter(1, 8);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 700);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 100);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Brush Name");
	AddCell("Lod Num");
	AddCell("Used Texture Mem(KB)");

	std::sort(stats.brushes.begin(), stats.brushes.end(), SortBrushes);

	int nRows = (int)stats.brushes.size();
	for (int i = 0; i < nRows; i++)
	{
		SCryEngineStats::SBrushMemInfo& mi = stats.brushes[i];
		AddRow();
		AddCell(mi.brushName);
		AddCell(mi.lodNum);
		AddCell(mi.usedTextureMemory / 1024);
	}
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("");
	AddCell("");
	AddCell_SumOfRows(nRows);
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMaterials(SCryEngineStats& stats)
{
	{
		NewWorksheet("Materials");
		FreezeFirstRow();
		AutoFilter(1, 5);

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 400);

		AddRow();
		m_CurrRow->setAttr("ss:StyleID", "s25");

		AddCell("Name");
		AddCell("IsSystem");
		AddCell("IsConsoleMtl");
		AddCell("RefCount");
		AddCell("NumSubMtls");

		int nRows = (int)stats.materials.size();
		for (int i = 0; i < nRows; i++)
		{
			IMaterial* pMat = stats.materials[i];
			AddRow();
			AddCell(pMat->GetName());

			const bool isSysMtl = pMat->GetShaderItem().m_pShader && (pMat->GetShaderItem().m_pShader->GetFlags() & EF_SYSTEM) != 0;
			AddCell(isSysMtl ? "Yes" : "No");

			const bool isConsoleMtl = (pMat->GetFlags() & MTL_FLAG_CONSOLE_MAT) != 0;
			AddCell(isConsoleMtl ? "Yes" : "No");

			AddCell(pMat->GetNumRefs());

			AddCell(pMat->GetSubMtlCount());
		}
	}

	{
		NewWorksheet("Materials Unused Textures");
		FreezeFirstRow();

		XmlNodeRef Column;
		Column = m_CurrTable->newChild("Column");
		Column->setAttr("ss:Width", 60);

		AddRow();
		AddCell("This is an estimate of which materials have slots filled with textures that are unused but will still stream.");
		AddRow();
		AddCell("Please use caution when removing these textures, and double check that it's valid.");
		AddRow();
		AddCell("");

		int nRows = (int)stats.materials.size();
		for (int i = 0; i < nRows; i++)
		{
			IMaterial* pMat = stats.materials[i];

			for (int sub = 0; pMat && sub < pMat->GetSubMtlCount(); sub++)
			{
				IMaterial* pSubMat = pMat->GetSubMtl(sub);

				if (!pSubMat)
					return;

				bool isMaterialRowAdded = false;

				IShader* pShader = pSubMat->GetShaderItem().m_pShader;
				int nTech = max(0, pSubMat->GetShaderItem().m_nTechnique);

				if (!pShader)
					continue;

				IRenderShaderResources* pShaderResources = pSubMat->GetShaderItem().m_pShaderResources;

				if (!pShaderResources)
					continue;

				SShaderTexSlots* pShaderSlots = pShader->GetUsedTextureSlots(nTech);

				if (!pShaderSlots)
					continue;

				for (int t = 0; t < EFTT_MAX; ++t)
				{
					SShaderTextureSlot* pSlot = pShaderSlots->m_UsedSlots[t];

					const string& sTexName = pShaderResources->GetTexture(t) ? pShaderResources->GetTexture(t)->m_Name : "";

					if (!pSlot && !sTexName.empty())
					{
						// found unused texture.

						if (!isMaterialRowAdded)
						{
							// first texture in this material, add material name row

							AddRow();
							m_CurrRow->setAttr("ss:StyleID", "s25");

							AddCell(string(pMat->GetName()) + "/" + pSubMat->GetName());

							isMaterialRowAdded = true;
						}

						AddRow();
						AddCell(pMat->GetMaterialHelpers().LookupTexName((EEfResTextures)t));
						AddCell(sTexName);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportTextures(SCryEngineStats& stats)
{
	NewWorksheet("Textures");
	FreezeFirstRow();
	AutoFilter(1, 10);

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 400);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 40);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");

	AddCell("Filename");
	AddCell("Refs");
	AddCell("Texture Size (KB)");
	AddCell("Resolution");
	AddCell("Mip Levels");
	AddCell("Type");
	AddCell("Format");
	AddCell("Usage");
	AddCell("Actual current size (KB)");
	AddCell("Last frame used");

	int nRows = (int)stats.textures.size();
	for (int i = 0; i < nRows; i++)
	{
		ITexture* pTexture = stats.textures[i];
		AddRow();
		// Filename
		AddCell(pTexture->GetName());
		// Refs
		AddCell(pTexture->AddRef() - 1);
		pTexture->Release();
		// Texture Size
		AddCell(pTexture->GetDataSize() / 1024);
		{
			char texres[128];
			cry_sprintf(texres, "%d x %d", pTexture->GetWidth(), pTexture->GetHeight());
			AddCell(texres, CELL_CENTERED);
		}
		AddCell(pTexture->GetNumMips());
		AddCell(pTexture->GetTypeName(), CELL_CENTERED);
		AddCell(pTexture->GetFormatName(), CELL_CENTERED);
		int numCopies = 1;
		if (pTexture->IsStreamedVirtual())
			AddCell("Streamed", CELL_CENTERED);
		else
		{
			const char* pTexDesc = "Static";
			uint32 texFlags = pTexture->GetFlags();
			if (texFlags & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
				pTexDesc = "Render Target";
			else if (texFlags & (FT_STAGE_UPLOAD | FT_STAGE_READBACK))
			{
				pTexDesc = "Dynamic";
				numCopies += (texFlags & FT_STAGE_UPLOAD) ? 1 : 0;
				numCopies += (texFlags & FT_STAGE_READBACK) ? 1 : 0;
			}
			else if (texFlags & FT_USAGE_ATLAS)
				pTexDesc = "Atlas";
			AddCell(pTexDesc, CELL_CENTERED);
		}
		AddCell(numCopies * pTexture->GetDeviceDataSize() / 1024);
		{
			char pTexDesc[16];
			const uint32 nFrameId = pTexture->GetAccessFrameId();
			if (nFrameId == -1)
			{
				cry_sprintf(pTexDesc, "Not used");
			}
			else
			{
				cry_sprintf(pTexDesc, "%u", nFrameId);
			}
			AddCell(pTexDesc, CELL_CENTERED);
		}
	}

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("");
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMemStats(SCryEngineStats& stats)
{
	if (!stats.memInfo.m_pStats)
		return;

	NewWorksheet("Memory Stats");
	FreezeFirstRow();

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);

	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");

	AddCell("Section");
	AddCell("Size (KB)");
	AddCell("Total Size (KB)");
	AddCell("Object Count");

	AddRow();

	for (unsigned i = 0; i < stats.memInfo.m_pStats->size(); ++i)
	{
		AddRow();
		const CrySizerStats::Component& rComp = (*stats.memInfo.m_pStats)[i];

		if (rComp.nDepth < 2)
		{
			AddRow(); // Skip one row if primary component.
			m_CurrRow->setAttr("ss:StyleID", "s25");
		}

		char szDepth[64] = "                                                              ";
		if (rComp.nDepth < sizeof(szDepth))
			szDepth[rComp.nDepth * 2] = '\0';

		string sCompDisplayName = szDepth;
		sCompDisplayName += rComp.strName;

		AddCell(sCompDisplayName.c_str());
		//char szSize[32];
		//cry_sprintf(szSize, "%s%7.3f", szDepth,rComp.getSizeMBytes() );
		//AddCell( szSize );
		//cry_sprintf(szSize, "%s%7.3f", szDepth,rComp.getTotalSizeMBytes() );
		//AddCell( szSize );

		if (rComp.sizeBytes > 0)
			AddCell((unsigned int)(rComp.sizeBytes / 1024));
		else
			AddCell("");
		AddCell((unsigned int)(rComp.sizeBytesTotal / 1024));

		if (rComp.numObjects > 0)
		{
			AddCell((unsigned int)(rComp.numObjects));
		}

		//if (rComp.sizeBytesTotal <= m_nMinSubcomponentBytes || rComp.nDepth > m_nMaxSubcomponentDepth)
		//continue;

		/*
		   char szDepth[32] = " ..............................";
		   if (rComp.nDepth < sizeof(szDepth))
		   szDepth[rComp.nDepth] = '\0';

		   char szSize[32];
		   if (rComp.sizeBytes > 0)
		   {
		   if (rComp.sizeBytesTotal > rComp.sizeBytes)
		    cry_sprintf (szSize, "%7.3f  %7.3f", rComp.getTotalSizeMBytes(), rComp.getSizeMBytes());
		   else
		    cry_sprintf (szSize, "         %7.3f", rComp.getSizeMBytes());
		   }
		   else
		   {
		   assert (rComp.sizeBytesTotal > 0);
		   cry_sprintf (szSize, "%7.3f         ", rComp.getTotalSizeMBytes());
		   }
		   char szCount[16];

		   if (rComp.numObjects)
		   cry_sprintf (szCount, "%8u", rComp.numObjects);
		   else
		   szCount[0] = '\0';

		   m_pLog->LogToFile ("%s%-*s:%s%s",szDepth, nNameWidth-rComp.nDepth,rComp.strName.c_str(), szSize, szCount);
		 */
	}

	//delete m_pStats;
	//delete m_pSizer;
}

//////////////////////////////////////////////////////////////////////////

void CStatsToExcelExporter::ExportStreamingInfo(
  SStreamEngineStatistics& stats)
{
	NewWorksheet("Streaming Info");

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 400);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);

	// overal stats
	AddRow();
	AddCell("Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.fAverageCompletionTime);
	AddRow();
	AddCell("Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.nTotalSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Average Decompression Bandwidth (MB):", CELL_BOLD);
	AddCell(stats.nDecompressBandwidthAverage / (1024.f * 1024.f));
	AddRow();
	AddCell("Average Decryption Bandwidth (MB):", CELL_BOLD);
	AddCell(stats.nDecryptBandwidthAverage / (1024.f * 1024.f));
	AddRow();
	AddCell("Average Verification Bandwidth (MB):", CELL_BOLD);
	AddCell(stats.nVerifyBandwidthAverage / (1024.f * 1024.f));
	AddRow();
	AddCell("Bytes Read (MB):", CELL_BOLD);
	AddCell(stats.nTotalBytesRead / (1024.f * 1024.f));
	AddRow();
	AddCell("Request Count:", CELL_BOLD);
	AddCell(stats.nTotalRequestCount);

	AddRow();

	// HDD stats
	AddRow();
	AddCell("HDD - Average Active Time (%):", CELL_BOLD);
	AddCell(stats.hddInfo.fAverageActiveTime);
	AddRow();
	AddCell("HDD - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.hddInfo.nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("HDD - Effective Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.hddInfo.nAverageActualReadBandwidth / (1024.f));
	AddRow();
	AddCell("HDD - Average Seek Offset (MB):", CELL_BOLD);
	AddCell(stats.hddInfo.nAverageSeekOffset / (1024.f * 1024.f));
	AddRow();
	AddCell("HDD - Bytes Read (MB):", CELL_BOLD);
	AddCell(stats.hddInfo.nTotalBytesRead / (1024.f * 1024.f));

	AddRow();

	// texture stats
	AddRow();
	AddCell("Texture - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTexture].fAverageCompletionTime);
	AddRow();
	AddCell("Texture - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTexture].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Texture - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTexture].nTotalRequestCount);
	AddRow();
	AddCell("Texture - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTexture].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Texture - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTexture].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Texture - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeTexture].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeTexture].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Texture - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeTexture].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeTexture].nTotalStreamingRequestCount) / 1024));

	AddRow();

	// geometry stats
	AddRow();
	AddCell("Geometry - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeGeometry].fAverageCompletionTime);
	AddRow();
	AddCell("Geometry - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeGeometry].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Geometry - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeGeometry].nTotalRequestCount);
	AddRow();
	AddCell("Geometry - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeGeometry].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Geometry - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeGeometry].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Geometry - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeGeometry].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeGeometry].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Geometry - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeGeometry].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeGeometry].nTotalStreamingRequestCount) / 1024));

	AddRow();

	// terrain stats
	AddRow();
	AddCell("Terrain - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTerrain].fAverageCompletionTime);
	AddRow();
	AddCell("Terrain - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTerrain].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Terrain - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTerrain].nTotalRequestCount);
	AddRow();
	AddCell("Terrain - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTerrain].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Terrain - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeTerrain].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Terrain - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeTerrain].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeTerrain].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Terrain - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeTerrain].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeTerrain].nTotalStreamingRequestCount) / 1024));

	AddRow();

	// Animation stats
	AddRow();
	AddCell("Animation - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeAnimation].fAverageCompletionTime);
	AddRow();
	AddCell("Animation - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeAnimation].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Animation - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeAnimation].nTotalRequestCount);
	AddRow();
	AddCell("Animation - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeAnimation].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Animation - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeAnimation].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Animation - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeAnimation].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeAnimation].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Animation - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeAnimation].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeAnimation].nTotalStreamingRequestCount) / 1024));

	AddRow();

	// Sound stats
	AddRow();
	AddCell("Sound - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeSound].fAverageCompletionTime);
	AddRow();
	AddCell("Sound - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeSound].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Sound - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeSound].nTotalRequestCount);
	AddRow();
	AddCell("Sound - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeSound].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Sound - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeSound].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Sound - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeSound].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeSound].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Sound - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeSound].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeSound].nTotalStreamingRequestCount) / 1024));

	AddRow();

	// Shader stats
	AddRow();
	AddCell("Shader - Average Completion Time (ms):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeShader].fAverageCompletionTime);
	AddRow();
	AddCell("Shader - Session Read Bandwidth (KB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeShader].nSessionReadBandwidth / (1024.f));
	AddRow();
	AddCell("Shader - Request Count:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeShader].nTotalRequestCount);
	AddRow();
	AddCell("Shader - Streaming Requests:", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeShader].nTotalStreamingRequestCount);
	AddRow();
	AddCell("Shader - Total Read Bytes (MB):", CELL_BOLD);
	AddCell(stats.typeInfo[eStreamTaskTypeShader].nTotalReadBytes / (1024.f * 1024.f));
	AddRow();
	AddCell("Shader - Average Read Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeShader].nTotalReadBytes /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeShader].nTotalStreamingRequestCount) / 1024));
	AddRow();
	AddCell("Shader - Average Request Size (KB):", CELL_BOLD);
	AddCell((uint32)(stats.typeInfo[eStreamTaskTypeShader].nTotalRequestDataSize /
	                 max((uint32)1, stats.typeInfo[eStreamTaskTypeShader].nTotalStreamingRequestCount) / 1024));
}

void CStatsToExcelExporter::ExportAnimationInfo(SCryEngineStats& stats)
{
	NewWorksheet("AnimationKeys Info");

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 400);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);

	// overal stats
	AddRow();
	AddCell("Animation Keys Current KB:", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nAnimsCurrent / 1024);
	AddRow();
	AddCell("Animation Keys Maximum KB::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nAnimsMax / 1024);

	uint64 average = 0;
	if (stats.m_AnimMemoryTracking.m_nAnimsCounter)
		average = stats.m_AnimMemoryTracking.m_nAnimsAdd / stats.m_AnimMemoryTracking.m_nAnimsCounter;
	AddRow();
	AddCell("Animation Keys Average KB::", CELL_BOLD);
	AddCell(average / 1024);
	AddRow();
	AddCell("Animation Global Counter::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nGlobalCAFs);
	AddRow();
	AddCell("Animation Used Global Headers::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nUsedGlobalCAFs);

	AddRow();
	AddCell("Animation Char Instances Counter::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_numTCharInstances);
	AddRow();
	AddCell("Animation Char Instances Memory::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nTotalCharMemory);
	AddRow();
	AddCell("Animation Skin Instances Counter::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_numTSkinInstances);
	AddRow();
	AddCell("Animation Skin Instances Memory::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nTotalSkinMemory);
	AddRow();
	AddCell("Animation Model Counter::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_numModels);
	AddRow();
	AddCell("Animation Model Memory::", CELL_BOLD);
	AddCell(stats.m_AnimMemoryTracking.m_nTotalMMemory);

}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportMemInfo(SCryEngineStats& stats)
{
	NewWorksheet("Modules Memory Info");
	FreezeFirstRow();

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 300);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 20);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 90);
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Module");
	AddCell("Dynamic(KB)");
	AddCell("Num Allocs");
	AddCell("Sum Of Allocs (KB)");
	AddCell("");
	AddCell("Static Total (KB)");
	AddCell("Static Code (KB)");
	AddCell("Static Init. Data (KB)");
	AddCell("Static Uninit. Data (KB)");
	AddCell("Strings (KB)");
	AddCell("STL (KB)");
	AddCell("STL Wasted (KB)");
	AddCell("Dynamic - Wasted (KB)");

	AddRow();

	//////////////////////////////////////////////////////////////////////////
	int nRows = 0;

	for (uint32 i = 0; i < stats.memInfo.modules.size(); i++)
	{
		SCryEngineStatsModuleInfo& moduleInfo = stats.memInfo.modules[i];
		const char* szModule = moduleInfo.name;

		AddRow();
		nRows++;
		AddCell(szModule, CELL_BOLD);
		AddCell(moduleInfo.usedInModule / 1024);
		AddCell(moduleInfo.memInfo.num_allocations);
		AddCell(moduleInfo.memInfo.allocated / 1024);
		AddCell("");
		AddCell(moduleInfo.moduleStaticSize / 1024);
		AddCell((uint32)moduleInfo.SizeOfCode / 1024);
		AddCell((uint32)moduleInfo.SizeOfInitializedData / 1024);
		AddCell((uint32)moduleInfo.SizeOfUninitializedData / 1024);
		AddCell((uint32)(moduleInfo.memInfo.CryString_allocated / 1024));
		AddCell((uint32)(moduleInfo.memInfo.STL_allocated / 1024));
		AddCell((uint32)(moduleInfo.memInfo.STL_wasted / 1024));
		AddCell((uint32)((moduleInfo.memInfo.allocated - moduleInfo.memInfo.requested) / 1024));
	}

	AddRow();
	AddCell("");
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell("");
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);
	AddCell_SumOfRows(nRows);

	AddRow();

	AddRow();
	AddCell("Lua Memory Usage (KB)", CELL_BOLD);
	AddCell(stats.nSummaryScriptSize / (1024));
	AddRow();
	AddCell("Total Num Allocs", CELL_BOLD);
	AddCell(stats.memInfo.totalNumAllocsInModules);
	AddRow();
	AddCell("Total Allocated (KB)", CELL_BOLD);
	AddCell(stats.memInfo.totalUsedInModules / 1024);
	AddRow();
	AddCell("Total Code and Static (KB)", CELL_BOLD);
	AddCell(stats.memInfo.totalCodeAndStatic / 1024);
	AddRow();
	AddRow();
	AddRow();
	AddCell("API Textures (KB)", CELL_BOLD);
	AddCell((stats.nSummary_TexturesPoolSize + stats.nSummary_UserTextureSize + stats.nSummary_EngineTextureSize) / (1024 * 1024));
	AddRow();
	AddCell("API Meshes (KB)", CELL_BOLD);
	AddCell(stats.nAPI_MeshSize / 1024);
}

//////////////////////////////////////////////////////////////////////////
void CStatsToExcelExporter::ExportTimeDemoInfo()
{
	if (!GetISystem()->GetITestSystem())
		return;
	STimeDemoInfo* pTD = GetISystem()->GetITestSystem()->GetTimeDemoInfo();
	if (!pTD)
		return;

	NewWorksheet("TimeDemo");
	FreezeFirstRow();

	XmlNodeRef Column;
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 400);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);

	AddRow();
	AddCell("Play Time:", CELL_BOLD);
	AddCell(pTD->lastPlayedTotalTime);
	AddRow();
	AddCell("Num Frames:", CELL_BOLD);
	AddCell((int)pTD->frames.size());
	AddRow();
	AddCell("Average FPS:", CELL_BOLD);
	AddCell(pTD->lastAveFrameRate);
	AddRow();
	AddCell("Min FPS:", CELL_BOLD);
	AddCell(pTD->minFPS);
	AddCell("At Frame:");
	AddCell(pTD->minFPS_Frame);
	AddRow();
	AddCell("Max FPS:", CELL_BOLD);
	AddCell(pTD->maxFPS);
	AddCell("At Frame:");
	AddCell(pTD->maxFPS_Frame);
	AddRow();
	AddCell("Average Tri/Sec:", CELL_BOLD);
	AddCell((uint32)((float)pTD->nTotalPolysPlayed / pTD->lastPlayedTotalTime));
	AddRow();
	AddCell("Average Tri/Frame:", CELL_BOLD);
	AddCell((uint32)((float)pTD->nTotalPolysPlayed / pTD->frames.size()));
	AddRow();
	AddCell("Played/Recorded Tris ratio:", CELL_BOLD);
	AddCell(pTD->nTotalPolysRecorded ? (float)pTD->nTotalPolysPlayed / pTD->nTotalPolysRecorded : 0.f);

	//////////////////////////////////////////////////////////////////////////
	NewWorksheet("TimeDemoFrames");
	FreezeFirstRow();

	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	Column = m_CurrTable->newChild("Column");
	Column->setAttr("ss:Width", 80);
	AddRow();
	m_CurrRow->setAttr("ss:StyleID", "s25");
	AddCell("Frame Number");
	AddCell("Frame Rate");
	AddCell("Rendered Polygons");
	AddCell("Draw Calls");

	AddRow();

	for (int i = 0; i < pTD->frames.size(); i++)
	{
		AddRow();
		AddCell(i);
		AddCell(pTD->frames[i].fFrameRate);
		AddCell(pTD->frames[i].nPolysRendered);
		AddCell(pTD->frames[i].nDrawCalls);
	}
}

void CStatsToExcelExporter::ExportFPSBuckets()
{
	//get perfHUD Export stats
	ICryPerfHUD* perfHUD = GetISystem()->GetPerfHUD();

	if (perfHUD)
	{
		float totalTime = 0;
		const std::vector<ICryPerfHUD::PerfBucket>* fpsBuckets = perfHUD->GetFpsBuckets(totalTime);

		if (fpsBuckets && totalTime > 0.f)
		{
			int numBuckets = fpsBuckets->size();

			AddRow();
			AddRow();
			m_CurrRow->setAttr("ss:StyleID", "s25");

			AddCell("Frame Rate Bucket");
			AddCell("Time Spent%");

			for (int i = 0; i < numBuckets; i++)
			{
				AddRow();

				char buf[32];
				cry_sprintf(buf, ">=%.1f FPS", fpsBuckets->at(i).target);
				AddCell(buf);

				float percentAtTarget = 100.f * (fpsBuckets->at(i).timeAtTarget / totalTime);
				AddCell(percentAtTarget);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
static void SaveLevelStats(IConsoleCmdArgs* pArgs)
{
	#if !defined(_RELEASE)

	auto levelName = gEnv->pGameFramework->GetLevelName();
	if (!levelName || !*levelName)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Cannot save level statistics because level is not loaded.");
		return;
	}

	CryLog("Execute SaveLevelStats");

	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	{
		string levelName = "no_level";

		ICVar* sv_map = gEnv->pConsole->GetCVar("sv_map");
		if (sv_map)
			levelName = sv_map->GetString();

		levelName = PathUtil::GetFileName(levelName);

		bool bDepends = true;
		if (pArgs->GetArgCount() > 1)
			bDepends = true;

		CEngineStats engineStats(bDepends);

		// level.xml
		{
			CStatsToExcelExporter excelExporter;

			string filename = PathUtil::ReplaceExtension(levelName, "xml");

			excelExporter.ExportToFile(engineStats.m_stats, filename);

			CryLog("SaveLevelStats exported '%s'", filename.c_str());
		}

		// level_dependencies.xml
		if (bDepends)
		{
			CStatsToExcelExporter excelExporter;

			engineStats.m_ResourceCollector.ComputeDependencyCnt();

			string filename = PathUtil::ReplaceExtension(string("depends_") + levelName, "xml");

			excelExporter.ExportDependenciesToFile(engineStats.m_ResourceCollector, filename);

			// log to log file - modifies engineStats.m_ResourceCollector data
			//		engineStats.m_ResourceCollector.LogData(*gEnv->pLog);
			CryLog("SaveLevelStats exported '%s'", filename.c_str());
		}

	}
	#endif
}

#else  // (!defined (_RELEASE) || defined(ENABLE_PROFILING_CODE))

bool QueryModuleMemoryInfo(SCryEngineStatsModuleInfo& moduleInfo, int index)
{
	return false;
}

#endif // (!defined (_RELEASE) || defined(ENABLE_PROFILING_CODE))

void RegisterEngineStatistics()
{
#if (!defined (_RELEASE) || defined(ENABLE_PROFILING_CODE))
	REGISTER_COMMAND("SaveLevelStats", SaveLevelStats, 0,
	                 "Calling this command creates multiple XML files with level statistics.\n"
	                 "The data includes file usage, dependencies, size in more/disk.\n"
	                 "The files can be loaded in Excel.");
#endif
}
