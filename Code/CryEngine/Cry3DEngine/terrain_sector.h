// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   terrain_sector.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef SECINFO_H
#define SECINFO_H

#define MML_NOT_SET          ((uint8) - 1)

#define ARR_TEX_OFFSETS_SIZE 4

#include "BasicArea.h"
#include "Array2d.h"

enum eTexureType
{
	ett_Diffuse,
	ett_LM
};

class CUpdateTerrainTempData;

struct SSurfaceTypeInfo
{
	SSurfaceTypeInfo() { memset(this, 0, sizeof(SSurfaceTypeInfo)); }
	struct SSurfaceType*    pSurfaceType;
	_smart_ptr<IRenderMesh> arrpRM[3];

	bool HasRM() { return arrpRM[0] || arrpRM[1] || arrpRM[2]; }
	int  GetIndexCount();
	void DeleteRenderMeshes(IRenderer* pRend);

	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

struct SRangeInfo
{
	SRangeInfo()
		: fOffset(0.0f)
		, fRange(0.0f)
		, pHMData(nullptr)
		, nSize(0)
		, nUnitBitShift(0)
		, nModified(0)
		, pSTPalette(nullptr)
		, iOffset(0)
		, iRange(0)
		, iStep(0)
	{
	}

	void UpdateBitShift(int nUnitToSectorBS)
	{
		const int nSecSize = 1 << nUnitToSectorBS;
		int n = nSize - 1;
		nUnitBitShift = 0;
		while (n > 0 && nSecSize > n)
		{
			nUnitBitShift++;
			n *= 2;
		}
	}

	inline uint32 GetRawDataByIndex(unsigned i) const
	{
		//		assert(i < nSize*nSize);
		assert(pHMData);

		return pHMData[i];
	}

	inline uint32 GetRawData(unsigned nX, unsigned nY) const
	{
		assert(nX < nSize);
		assert(nY < nSize);
		assert(pHMData);

		return GetRawDataByIndex(nX * nSize + nY);
	}

	inline float RawDataToHeight(uint32 data) const
	{
		return 0.05f * iOffset + (data >> 4) * iStep * 0.05f;
	}

	inline float GetHeightByIndex(unsigned i) const
	{
		//		assert(i < nSize*nSize);
		assert(pHMData);

		return RawDataToHeight(pHMData[i]);
	}

	inline float GetHeight(unsigned nX, unsigned nY) const
	{
		assert(nX < nSize);
		assert(nY < nSize);
		assert(pHMData);

		return GetHeightByIndex(nX * nSize + nY);
	}

	inline uint32 GetSurfaceTypeByIndex(unsigned i) const
	{
		//		assert(i < nSize*nSize);
		assert(pHMData);

		return pHMData[i] & e_index_hole;
	}

	inline uint32 GetSurfaceType(unsigned nX, unsigned nY) const
	{
		assert(nX < nSize);
		assert(nY < nSize);
		assert(pHMData);

		return GetSurfaceTypeByIndex(nX * nSize + nY);
	}

	inline void SetDataLocal(int nX, int nY, uint16 usValue)
	{
		assert(nX >= 0 && nX < (int)nSize);
		assert(nY >= 0 && nY < (int)nSize);
		assert(pHMData);
		pHMData[nX * nSize + nY] = usValue;
	}

	inline uint16 GetDataUnits(int nX_units, int nY_units) const
	{
		int nMask = nSize - 2;
		int nX = nX_units >> nUnitBitShift;
		int nY = nY_units >> nUnitBitShift;
		return GetRawData(nX & nMask, nY & nMask);
	}

	// Finds or selects a 4-bit index in this sector (0-14) to represent the given global surface type index (0-126).
	uint16 GetLocalSurfaceTypeID(uint16 usGlobalSurfaceTypeID)
	{
		if (pSTPalette)
		{
			if (usGlobalSurfaceTypeID == e_undefined) return e_index_undefined;
			if (usGlobalSurfaceTypeID == e_hole) return e_index_hole;

			// Check if a local entry has already been assigned for this global entry.
			for (uint16 i = 0; i < e_index_undefined; i++)
				if (pSTPalette[i] == usGlobalSurfaceTypeID)
					return i;
			// No local entry has been assigned; look for an entry that is marked as currently unused.
			for (uint16 i = 0; i < e_index_undefined; i++)
				if (pSTPalette[i] == e_undefined)
				{
					pSTPalette[i] = (uchar)usGlobalSurfaceTypeID;
					return i;
				}
			// No local entry is marked as unused; look for one whose local ID does not actually occur in the data.
			int nUsageCounters[e_palette_size];
			memset(nUsageCounters, 0, sizeof(nUsageCounters));
			int nCount = nSize * nSize;

			for (uint16 i = 0; i < nCount; i++) nUsageCounters[pHMData[i] & e_index_hole]++;
			for (uint16 i = 0; i < e_index_undefined; i++)
				if (!nUsageCounters[i])
				{
					pSTPalette[i] = (uchar)usGlobalSurfaceTypeID;
					return i;
				}
			// Could not assign a local ID; mark the problem area with holes. (Should not happen, we have integrity checks.)
			return e_index_undefined;
		}
		else
		{
			// If the sector still has no palette, create one and assign local ID 0.
			pSTPalette = new uchar[e_palette_size];

			for (int i = 0; i < e_index_hole; pSTPalette[i++] = e_undefined);

			pSTPalette[0] = (uchar)usGlobalSurfaceTypeID;
			pSTPalette[e_index_hole] = e_hole;
			return 0;
		}
	}

	float   fOffset;
	float   fRange;
	uint16* pHMData;

	uint16  nSize;
	uint8   nUnitBitShift;
	uint8   nModified;
	uchar*  pSTPalette; // Maps the local surface type indices from the HM to the global ones in CTerrain

	int     iOffset;
	int     iRange;
	int     iStep;

	enum
	{
		e_index_undefined = 14,
		e_index_hole,
		e_palette_size,

		e_undefined = 127,
		e_hole,
		e_max_surface_types
	};
};

template<class T> class TPool
{
public:

	TPool(int nPoolSize)
	{
		m_nPoolSize = nPoolSize;
		m_pPool = new T[nPoolSize];
		m_lstFree.PreAllocate(nPoolSize, 0);
		m_lstUsed.PreAllocate(nPoolSize, 0);
		for (int i = 0; i < nPoolSize; i++)
			m_lstFree.Add(&m_pPool[i]);
	}

	~TPool()
	{
		delete[] m_pPool;
	}

	void ReleaseObject(T* pInst)
	{
		if (m_lstUsed.Delete(pInst))
			m_lstFree.Add(pInst);
	}

	int GetUsedInstancesCount(int& nAll)
	{
		nAll = m_nPoolSize;
		return m_lstUsed.Count();
	}

	T* GetObject()
	{
		T* pInst = NULL;
		if (m_lstFree.Count())
		{
			pInst = m_lstFree.Last();
			m_lstFree.DeleteLast();
			m_lstUsed.Add(pInst);
		}
		else
			assert(!"TPool::GetObject: Out of free elements error");

		return pInst;
	}

	void GetMemoryUsage(class ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_lstFree);
		pSizer->AddObject(m_lstUsed);

		if (m_pPool)
			for (int i = 0; i < m_nPoolSize; i++)
				m_pPool[i].GetMemoryUsage(pSizer);
	}

	PodArray<T*> m_lstFree;
	PodArray<T*> m_lstUsed;
	T*           m_pPool;
	int          m_nPoolSize;
};

#define MAX_PROC_OBJ_CHUNKS_NUM (GetCVars()->e_ProcVegetationMaxChunksInCache * GetCVars()->e_ProcVegetationMaxCacheLevels)
#define MAX_PROC_SECTORS_NUM    (GetCVars()->e_ProcVegetationMaxSectorsInCache * GetCVars()->e_ProcVegetationMaxCacheLevels)

struct SProcObjChunk : public Cry3DEngineBase
{
	CVegetation* m_pInstances;
	int nAllocatedItems;
	SProcObjChunk();
	~SProcObjChunk();
	void GetMemoryUsage(class ICrySizer* pSizer) const;
};

typedef TPool<SProcObjChunk> SProcObjChunkPool;

class CProcObjSector : public Cry3DEngineBase
{
public:
	CProcObjSector() { m_nProcVegetNum = 0; m_ProcVegetChunks.PreAllocate(32); }
	~CProcObjSector();
	CVegetation* AllocateProcObject(int nSID);
	void         ReleaseAllObjects();
	int          GetUsedInstancesCount(int& nAll) { nAll = m_ProcVegetChunks.Count(); return m_nProcVegetNum; }
	void         GetMemoryUsage(ICrySizer* pSizer) const;

protected:
	PodArray<SProcObjChunk*> m_ProcVegetChunks;
	int                      m_nProcVegetNum;
};

typedef TPool<CProcObjSector> CProcVegetPoolMan;

struct STerrainNodeLeafData
{
	STerrainNodeLeafData() { memset(this, 0, sizeof(*this)); }
	~STerrainNodeLeafData();
	float                   m_arrTexGen[MAX_RECURSION_LEVELS][ARR_TEX_OFFSETS_SIZE];
	int                     m_arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4];
	PodArray<CTerrainNode*> m_lstNeighbourSectors;
	PodArray<uint8>         m_lstNeighbourLods;
	_smart_ptr<IRenderMesh> m_pRenderMesh;
};

enum ETextureEditingState : unsigned int
{
	eTES_SectorIsUnmodified = 0,
	eTES_SectorIsModified_AtlasIsUpToDate,
	eTES_SectorIsModified_AtlasIsDirty
};

struct CTerrainNode : public Cry3DEngineBase, public IRenderNode, public IStreamCallback
{
public:

	// IRenderNode implementation
	virtual const char*             GetName() const                         { return "TerrainNode"; }
	virtual const char*             GetEntityClassName() const              { return "TerrainNodeClass"; }
	virtual Vec3                    GetPos(bool bWorldOnly = true) const    { return Vec3(m_nOriginX, m_nOriginY, 0); }
	virtual void                    SetBBox(const AABB& WSBBox)             {}
	virtual struct IPhysicalEntity* GetPhysics() const                      { return NULL; }
	virtual void                    SetPhysics(IPhysicalEntity* pPhys)      {}
	virtual void                    SetMaterial(IMaterial* pMat)            {}
	virtual IMaterial*              GetMaterial(Vec3* pHitPos = NULL) const { return NULL; }
	virtual IMaterial*              GetMaterialOverride()                   { return NULL; }
	virtual float                   GetMaxViewDist()                        { return 1000000.f; }
	virtual EERType                 GetRenderNodeType()                     { return eERType_TerrainSector;  }

	friend class CTerrain;
	friend class CTerrainUpdateDispatcher;

	virtual void                       Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo);
	const AABB                         GetBBox() const;
	virtual const AABB                 GetBBoxVirtual()                                                                                   { return GetBBox(); }
	virtual void                       FillBBox(AABB& aabb);
	virtual struct ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) { return NULL; };

	//////////////////////////////////////////////////////////////////////////
	// IStreamCallback
	//////////////////////////////////////////////////////////////////////////
	// streaming
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError);
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);
	//////////////////////////////////////////////////////////////////////////
	void         StartSectorTexturesStreaming(bool bFinishNow);

	void         Init(int x1, int y1, int nNodeSize, CTerrainNode* pParent, bool bBuildErrorsTable, int nSID);
	CTerrainNode() :
		m_nSID(0),
		m_nNodeTexSet(),
		m_nTexSet(),
		m_nNodeTextureOffset(-1),
		m_nNodeHMDataOffset(-1),
		m_pParent(),
		m_pLeafData(NULL),
		m_pUpdateTerrainTempData(NULL),
		m_pChilds(0),
		m_nLastTimeUsed(0),
		m_nSetLodFrameId(0),
		m_pGeomErrors(0),
		m_pProcObjPoolPtr(0),
		m_nGSMFrameId(0)
	{
		memset(&m_arrfDistance, 0, sizeof(m_arrfDistance));
		m_nNodesCounter++;
	}
	~CTerrainNode();
	static void   ResetStaticData();
	bool          CheckVis(bool bAllIN, bool bAllowRenderIntoCBuffer, const Vec3& vSegmentOrigin, const SRenderingPassInfo& passInfo);
	void          SetupTexturing(bool bMakeUncompressedForEditing, const SRenderingPassInfo& passInfo);
	void          RequestTextures(const SRenderingPassInfo& passInfo);
	void          EnableTextureEditingMode(unsigned int textureId);
	void          UpdateNodeTextureFromEditorData();
	void					UpdateNodeNormalMapFromEditorData();
	static void   SaveCompressedMipmapLevel(const void* data, size_t size, void* userData);
	void          CheckNodeGeomUnload(const SRenderingPassInfo& passInfo);
	void          SetChildsLod(int nNewGeomLOD, const SRenderingPassInfo& passInfo);
	int           GetAreaLOD(const SRenderingPassInfo& passInfo);
	bool          RenderNodeHeightmap(const SRenderingPassInfo& passInfo);
	bool          CheckUpdateProcObjects(const SRenderingPassInfo& passInfo);
	void          IntersectTerrainAABB(const AABB& aabbBox, PodArray<CTerrainNode*>& lstResult);
	void          UpdateDetailLayersInfo(bool bRecursive);
	void          RemoveProcObjects(bool bRecursive = false, bool bReleaseAllObjects = true);
	void          IntersectWithShadowFrustum(bool bAllIn, PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, const float fHalfGSMBoxSize, const SRenderingPassInfo& passInfo);
	void          IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult);
	CTerrainNode* FindMinNodeContainingBox(const AABB& aabbBox);
	bool          RenderSector(const SRenderingPassInfo& passInfo); // returns true only if the sector rendermesh is valid and does not need to be updated
	CTerrainNode* GetTexuringSourceNode(int nTexMML, eTexureType eTexType);
	CTerrainNode* GetReadyTexSourceNode(int nTexMML, eTexureType eTexType);
	int           GetData(byte*& pData, int& nDataSize, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	void          CalculateTexGen(const CTerrainNode* pTextureSourceNode, float& fTexOffsetX, float& fTexOffsetY, float& fTexScale);
	void          FillSectorHeightMapTextureData(Array2d<float> &arrHmData);
	void          RescaleToInt();

	template<class T>
	int   Load_T(T*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int   Load(uint8*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int   Load(FILE*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int   ReloadModifiedHMData(FILE* f);
	void  ReleaseHoleNodes();

	void  UnloadNodeTexture(bool bRecursive);
	float GetSurfaceTypeAmount(Vec3 vPos, int nSurfType);
	void  GetMemoryUsage(ICrySizer* pSizer) const;
	void  GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& cstAABB);

	void  SetLOD(const SRenderingPassInfo& passInfo);
	uint8 GetTextureLOD(float fDistance, const SRenderingPassInfo& passInfo);

	void                ReleaseHeightMapGeometry(bool bRecursive = false, const AABB* pBox = NULL);
	void                ResetHeightMapGeometry(bool bRecursive = false, const AABB* pBox = NULL);
	int                 GetSecIndex();

	void                DrawArray(const SRenderingPassInfo& passInfo);

	void                UpdateRenderMesh(struct CStripsInfo* pArrayInfo, bool bUpdateVertices);
	void                BuildVertices(int step, bool bSafetyBorder);
	void                SetVertexSurfaceType(int x, int y, int nStep, CTerrain* pTerrain, const int nSID, SVF_P2S_N4B_C4B_T1F &vert);
	void                SetVertexNormal(int x, int y, const int iLookupRadius, CTerrain* pTerrain, const int nTerrainSize, const int nSID, SVF_P2S_N4B_C4B_T1F &vert, Vec3 * pTerrainNorm = nullptr);
	void                AppendTrianglesFromObjects(const int nOriginX, const int nOriginY, CTerrain* pTerrain, const int nSID, const int nStep, const int nTerrainSize);

	int                 GetMML(int dist, int mmMin, int mmMax);

	uint32                       GetLastTimeUsed() { return m_nLastTimeUsed; }

	void                         AddIndexAliased(int _x, int _y, int _step, int nSectorSize, PodArray<CTerrainNode*>* plstNeighbourSectors, CStripsInfo* pArrayInfo, const SRenderingPassInfo& passInfo);
	static void                  GenerateIndicesForAllSurfaces(IRenderMesh* pRM, bool bOnlyBorder, int arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4], int nBorderStartIndex, SSurfaceTypeInfo* pSurfaceTypeInfos, int nSID, CUpdateTerrainTempData* pUpdateTerrainTempData = NULL);
	void                         BuildIndices(CStripsInfo& si, PodArray<CTerrainNode*>* pNeighbourSectors, bool bSafetyBorder, const SRenderingPassInfo& passInfo);

	void                         BuildIndices_Wrapper(SRenderingPassInfo passInfo);
	void                         BuildVertices_Wrapper();
	void                         RenderSectorUpdate_Finish(const SRenderingPassInfo& passInfo);

	static void                  UpdateSurfaceRenderMeshes(const _smart_ptr<IRenderMesh> pSrcRM, struct SSurfaceType* pSurface, _smart_ptr<IRenderMesh>& pMatRM, int nProjectionId, PodArray<vtx_idx>& lstIndices, const char* szComment, bool bUpdateOnlyBorders, int nNonBorderIndicesCount, const SRenderingPassInfo& passInfo);
	static void                  SetupTexGenParams(SSurfaceType* pLayer, float* pOutParams, uint8 ucProjAxis, bool bOutdoor, float fTexGenScale = 1.f);

	int                          CreateSectorTexturesFromBuffer(float * pSectorHeightMap);

	bool                         CheckUpdateDiffuseMap();
	bool                         AssignTextureFileOffset(int16*& pIndices, int16& nElementsNum);
	static CProcVegetPoolMan*    GetProcObjPoolMan()                                       { return m_pProcObjPoolMan; }
	static SProcObjChunkPool*    GetProcObjChunkPool()                                     { return m_pProcObjChunkPool; }
	static void                  SetProcObjPoolMan(CProcVegetPoolMan* pProcObjPoolMan)     { m_pProcObjPoolMan = pProcObjPoolMan; }
	static void                  SetProcObjChunkPool(SProcObjChunkPool* pProcObjChunkPool) { m_pProcObjChunkPool = pProcObjChunkPool; }
	void                         UpdateDistance(const SRenderingPassInfo& passInfo);
	const float                  GetDistance(const SRenderingPassInfo& passInfo);
	bool                         IsProcObjectsReady() { return m_bProcObjectsReady != 0; }
	void                         UpdateRangeInfoShift();
	int                          GetSectorSizeInHeightmapUnits() const;
	void                         CheckLeafData();
	inline STerrainNodeLeafData* GetLeafData() { return m_pLeafData; }
	void                         OffsetPosition(const Vec3& delta);
	_smart_ptr<IRenderMesh>			 GetSharedRenderMesh();
	uint32											 GetMaterialsModificationId();

	//////////////////////////////////////////////////////////////////////////
	// Member variables
	//////////////////////////////////////////////////////////////////////////
public:
	IReadStreamPtr       m_pReadStream;
	EFileStreamingStatus m_eTexStreamingStatus;

	CTerrainNode*        m_pChilds; // 4 childs or NULL

	// flags
	uint8                m_bProcObjectsReady  : 1;
	uint8                m_bMergeNotAllowed   : 1;
	uint8                m_bHasHoles          : 2; // sector has holes in the ground
	uint8                m_bNoOcclusion       : 1; // sector has visareas under terrain surface
	uint8                m_bUpdateOnlyBorders : 1; // remember if only the border were updated

#ifndef _RELEASE
	ETextureEditingState m_eTextureEditingState, m_eElevTexEditingState;
#endif // _RELEASE

	uint8 // LOD's
	  m_cNewGeomMML, m_cCurrGeomMML,
	  m_cNewGeomMML_Min, m_cNewGeomMML_Max,
	  m_cNodeNewTexMML, m_cNodeNewTexMML_Min;
	uint8  m_nTreeLevel;

	uint16 m_nOriginX, m_nOriginY; // sector origin
	int    m_nLastTimeUsed;        // basically last time rendered
	int    m_nSetLodFrameId;

	float* m_pGeomErrors;  // errors for each lod level

protected:

	// temp data for terrain generation
	CUpdateTerrainTempData* m_pUpdateTerrainTempData;

public:

	PodArray<SSurfaceTypeInfo> m_lstSurfaceTypeInfo;

	SRangeInfo                 m_rangeInfo;

	STerrainNodeLeafData*      m_pLeafData;

	CProcObjSector*            m_pProcObjPoolPtr;

	SSectorTextureSet          m_nNodeTexSet, m_nTexSet; // texture id's

	uint16                     m_nNodeTextureLastUsedSec4;
	uint16                     m_nSID;

	AABB                       m_boxHeigtmapLocal;
	float                      m_fBBoxExtentionByObjectsIntegration;
	struct CTerrainNode*       m_pParent;
	int                        m_nGSMFrameId;

	float                      m_arrfDistance[MAX_RECURSION_LEVELS];
	int                        m_nNodeTextureOffset;
	int                        m_nNodeHMDataOffset;
	int FTell(uint8*& f);
	int FTell(FILE*& f);

	static PodArray<vtx_idx>       m_arrIndices[SRangeInfo::e_max_surface_types][4];
	static PodArray<SSurfaceType*> m_lstReadyTypes;
	static CProcVegetPoolMan*      m_pProcObjPoolMan;
	static SProcObjChunkPool*      m_pProcObjChunkPool;
	static int                     m_nNodesCounter;

	OcclusionTestClient            m_occlusionTestClient;
};

// Container to manager temp memory as well as running update jobs
class CTerrainUpdateDispatcher : public Cry3DEngineBase
{
public:
	CTerrainUpdateDispatcher();
	~CTerrainUpdateDispatcher();

	void QueueJob(CTerrainNode*, const SRenderingPassInfo& passInfo);
	void SyncAllJobs(bool bForceAll, const SRenderingPassInfo& passInfo);
	bool Contains(CTerrainNode* pNode)
	{ return (m_queuedJobs.Find(pNode) != -1 || m_arrRunningJobs.Find(pNode) != -1); };

	void GetMemoryUsage(ICrySizer* pSizer) const;

	void RemoveJob(CTerrainNode* pNode);

private:
	bool AddJob(CTerrainNode*, bool executeAsJob, const SRenderingPassInfo& passInfo);

	static const size_t            TempPoolSize = (4U << 20);

	void*                          m_pHeapStorage;
	_smart_ptr<IGeneralMemoryHeap> m_pHeap;

	PodArray<CTerrainNode*>        m_queuedJobs;
	PodArray<CTerrainNode*>        m_arrRunningJobs;
};

#pragma pack(push,4)

struct STerrainNodeChunk
{
	int16 nChunkVersion;
	int16 bHasHoles;
	AABB  boxHeightmap;
	float fOffset;
	float fRange;
	int   nSize;
	int   nSurfaceTypesNum;

	AUTO_STRUCT_INFO;
};

#pragma pack(pop)

#include "terrain.h"

inline const AABB CTerrainNode::GetBBox() const
{
	const Vec3& vOrigin = GetTerrain()->m_arrSegmentOrigns[m_nSID];
	return AABB(m_boxHeigtmapLocal.min + vOrigin, m_boxHeigtmapLocal.max + vOrigin + Vec3(0, 0, m_fBBoxExtentionByObjectsIntegration));
}

#endif
