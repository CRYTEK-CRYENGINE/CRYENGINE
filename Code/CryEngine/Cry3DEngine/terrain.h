// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "TerrainModifications.h"

#define TERRAIN_BOTTOM_LEVEL     0
#define TERRAIN_NODE_TREE_DEPTH  16
#define TERRAIN_TEX_OFFSETS_SIZE 16
#define OCEAN_IS_VERY_FAR_AWAY   1000000.f

enum { nHMCacheSize = 64 };

class CTerrainUpdateDispatcher;

typedef std::pair<struct CTerrainNode*, uint32> STerrainVisItem;

struct SSurfaceType
{
	SSurfaceType()
	{ memset(this, 0, sizeof(SSurfaceType)); ucThisSurfaceTypeId = 255; }

	bool       IsMaterial3D() { return pLayerMat && pLayerMat->GetSubMtlCount() == 3; }

	bool       HasMaterial()  { return pLayerMat != NULL; }

	IMaterial* GetMaterialOfProjection(uint8 ucProjAxis)
	{
		if (pLayerMat)
		{
			if (pLayerMat->GetSubMtlCount() == 3)
			{
				return pLayerMat->GetSubMtl(ucProjAxis - 'X');
			}
			else if (ucProjAxis == ucDefProjAxis)
				return pLayerMat;
		}

		return NULL;
	}

	float GetMaxMaterialDistanceOfProjection(uint8 ucProjAxis)
	{
		if (ucProjAxis == 'X' || ucProjAxis == 'Y')
			return fMaxMatDistanceXY;

		return fMaxMatDistanceZ;
	}

	char                 szName[128];
	_smart_ptr<CMatInfo> pLayerMat;
	float                fScale;
	PodArray<int>        lstnVegetationGroups;
	float                fMaxMatDistanceXY;
	float                fMaxMatDistanceZ;
	float                arrRECustomData[4][TERRAIN_TEX_OFFSETS_SIZE];
	uint8                ucDefProjAxis;
	uint8                ucThisSurfaceTypeId;
	float                fCustomMaxDistance;
};

struct CTerrainNode;

struct CTextureCache : public Cry3DEngineBase
{
	PodArray<uint16> m_FreeTextures;
	PodArray<uint16> m_UsedTextures;
	PodArray<uint16> m_Quarantine;
	ETEX_Format      m_eTexFormat;
	int              m_nDim;
	int              m_nPoolTexId;
	int              m_nPoolDim;

	CTextureCache();
	~CTextureCache();
	uint16 GetTexture(byte* pData, uint16& nSlotId);
	void   UpdateTexture(byte* pData, const uint16& nSlotId);
	void   ReleaseTexture(uint16& nTexId, uint16& nSlotId);
	bool   Update();
	void   InitPool(byte* pData, int nDim, ETEX_Format eTexFormat);
	void   GetSlotRegion(RectI& region, int i);
	int    GetPoolTexDim() { return m_nPoolDim * m_nDim; }
	void   ResetTexturePool();
	int    GetPoolSize();
	int    GetPoolItemsNum();
};

#pragma pack(push,4)

//! structure to vegetation group properties loading/saving
//! do not change the size of this struct
struct StatInstGroupChunk
{
	StatInstGroupChunk()
	{
		ZeroStruct(*this);
	}
	char   szFileName[256];
	float  fBending;
	float  fSpriteDistRatio;
	float  fShadowDistRatio;
	float  fMaxViewDistRatio;
	float  fBrightness;
	int32  nRotationRangeToTerrainNormal; // applied to a vegetation object that has been realigned in the terrain's Y/X direction
	float  fAlignToTerrainCoefficient;
	uint32 nMaterialLayers;

	float  fDensity;
	float  fElevationMax;
	float  fElevationMin;
	float  fSize;
	float  fSizeVar;
	float  fSlopeMax;
	float  fSlopeMin;

	float  fStatObjRadius_NotUsed;
	int    nIDPlusOne; // For backward compatibility, we need to save ID + 1

	float  fLodDistRatio;
	uint32 nReserved;

	int    nFlags;
	int    nMaterialId;

	//! flags similar to entity render flags
	uint64 m_dwRndFlags;

	float  fStiffness;
	float  fDamping;
	float  fVariance;
	float  fAirResistance;

	AUTO_STRUCT_INFO_LOCAL;
};

struct SNameChunk
{
	SNameChunk() { memset(this, 0, sizeof(SNameChunk)); }

	char szFileName[256];

	AUTO_STRUCT_INFO_LOCAL;
};

#pragma pack(pop)

// The Terrain Class
class CTerrain : public ITerrain, public Cry3DEngineBase
{
	friend struct CTerrainNode;

public:

	CTerrain(const STerrainInfo& TerrainInfo);
	~CTerrain();

	// Access to heightmap data
	float            GetZSafe(float x, float y);
	float            GetZ(float x, float y, bool bUpdatePos = false) const;
	void             SetZ(const float x, const float y, float fHeight);
	uint8            GetSurfaceTypeID(float x, float y) const;
	SSurfaceTypeItem GetSurfaceTypeItem(float x, float y) const;
	float            GetZApr(float x, float y) const;
	Vec4             GetNormalAndZ(float x, float y, float size = 0) const;
	float            GetZMax(float x0, float y0, float x1, float y1) const;
	bool             GetHole(float x, float y) const;
	bool             IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip);
	bool             IsMeshQuadFlipped(const float x, const float y, const float unitSize) const;

#ifdef SUPP_HMAP_OCCL
	bool Intersect(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip, Vec3& vLastVisPoint);
	bool IsBoxOccluded
	(
		const AABB& objBox,
		float fDistance,
		bool bTerrainNode,
		OcclusionTestClient* const __restrict pOcclTestVars,
		const SRenderingPassInfo& passInfo
	);
#endif

	// Exact test.
	struct SRayTrace
	{
		float      fInterp;
		Vec3       vHit;
		Vec3       vNorm;
		IMaterial* pMaterial;

		SRayTrace() : fInterp(0), vHit(0, 0, 0), vNorm(0, 0, 1), pMaterial(0) {}
		SRayTrace(float fT, Vec3 const& vH, Vec3 const& vN, IMaterial* pM)
			: fInterp(fT), vHit(vH), vNorm(vN), pMaterial(pM)
		{}
	};
	bool              RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt, bool bClampAbove = true);

	void              InitHeightfieldPhysics();
	void              SetMaterialMapping();
	static int        GetTerrainSize()               { return m_nTerrainSize; }
	static int        GetSectorSize()                { return m_nSectorSize; }
	static float      GetHeightMapUnitSize()         { return m_fUnitSize; }
	static float      GetHeightMapUnitSizeInverted() { return m_fInvUnitSize; }
	static int        GetSectorsTableSize()          { return m_nSectorsTableSize; }
	static float      GetInvUnitSize()               { return m_fInvUnitSize; }
	int               GetHeightmapUnits() const      { return m_nTerrainUnits; }

	ILINE bool InsideTerrainUnits(int xu, int yu) const
	{
		return min(xu, yu) >= 0 && max(xu, yu) < m_nTerrainUnits;
	}
	ILINE void ClampUnits(int& xu, int& yu) const
	{
		xu = crymath::clamp(xu, 0, GetHeightmapUnits() - 1);
		yu = crymath::clamp(yu, 0, GetHeightmapUnits() - 1);
	}
	CTerrainNode* GetSecInfo(float x, float y) const
	{
		if (x < 0 || y < 0 || x >= m_nTerrainSize || y >= m_nTerrainSize || !m_pParentNode)
			return nullptr;
		return GetSecInfoUnits(int(x * GetInvUnitSize()), int(y * GetInvUnitSize()));
	}
	ILINE CTerrainNode* GetSecInfo(const Vec3& pos) const
	{
		return GetSecInfo(pos.x, pos.y);
	}
	ILINE float GetWaterLevel()                      { return m_fOceanWaterLevel; /* ? m_fOceanWaterLevel : WATER_LEVEL_UNKNOWN;*/ }
	ILINE void  SetWaterLevel(float oceanWaterLevel) { m_fOceanWaterLevel = oceanWaterLevel; }

	//////////////////////////////////////////////////////////////////////////
	// ITerrain Implementation.
	//////////////////////////////////////////////////////////////////////////

	template<class T>
	bool Load_Tables_T(T*& f, int& nDataSize, std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, bool bHotUpdate, bool bSW, EEndian eEndian);

	template<class T>
	bool                 Load_T(T*& f, int& nDataSize, STerrainChunkHeader* pTerrainChunkHeader, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo);

	virtual bool         GetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, std::vector<struct IStatInstGroup*>** ppStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	virtual int          GetCompiledDataSize(SHotUpdateInfo* pExportInfo);
	virtual void         GetStatObjAndMatTables(DynArray<IStatObj*>* pStatObjTable, DynArray<IMaterial*>* pMatTable, DynArray<IStatInstGroup*>* pStatInstGroupTable, uint32 nObjTypeMask);

	virtual int          GetTablesSize(SHotUpdateInfo* pExportInfo);
	virtual void         SaveTables(byte*& pData, int& nDataSize, std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, std::vector<struct IStatInstGroup*>*& pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);

	virtual IRenderNode* AddVegetationInstance(int nStaticGroupIndex, const Vec3& vPos, const float fScale, uint8 ucBright, uint8 angle, uint8 angleX, uint8 angleY);
	virtual void         SetOceanWaterLevel(float oceanWaterLevel);

	virtual void         MarkAndOffsetCloneRegion(const AABB& region, const Vec3& offset);
	virtual void         CloneRegion(const AABB& region, const Vec3& offset, float zRotation, const uint16* pIncludeLayers, int numIncludeLayers);
	virtual void         ClearCloneSources();
	virtual void         ChangeOceanMaterial(IMaterial* pMat);
	virtual void         OnTerrainPaintActionComplete() {}
	//////////////////////////////////////////////////////////////////////////

	void          RemoveAllStaticObjects();
	void          ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce);
	//	void CheckUnload();
	void          CloseTerrainTextureFile();
	void          DrawVisibleSectors(const SRenderingPassInfo& passInfo);
	virtual int   GetDetailTextureMaterials(IMaterial* materials[]);
	void          MakeCrater(Vec3 vExploPos, float fExploRadius);
	bool          RemoveObjectsInArea(Vec3 vExploPos, float fExploRadius);
	float         GetDistanceToSectorWithWater() { return m_fDistanceToSectorWithWater; }
	void GetMemoryUsage(class ICrySizer* pSizer) const;
	void          GetObjects(PodArray<struct SRNInfo>* pLstObjects);
	void          GetObjectsAround(Vec3 vPos, float fRadius, PodArray<struct SRNInfo>* pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects);
	class COcean* GetOcean() { return m_pOcean; }
	Vec3          GetTerrainSurfaceNormal(Vec3 vPos, float fRange);
	Vec3          GetTerrainSurfaceNormal_Int(float x, float y);
	void          GetTerrainAlignmentMatrix(const Vec3& vPos, const float amount, Matrix33& matrix33);
	int           GetActiveTextureNodesCount() { return m_lstActiveTextureNodes.Count(); }
	int           GetActiveProcObjNodesCount(int& objectsNum, int& maxSectorsNum);
	int           GetNotReadyTextureNodesCount();
	void          GetTextureCachesStatus(int& nCount0, int& nCount1) { nCount0 = m_texCache[0].GetPoolSize(); nCount1 = m_texCache[1].GetPoolSize(); }

	void          CheckVis(const SRenderingPassInfo& passInfo, uint32 passCullMask);
	int           UpdateOcean(const SRenderingPassInfo& passInfo);
	int           RenderOcean(const SRenderingPassInfo& passInfo);
	void          UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo);
	void          ProcessActiveProcObjNodes(bool bSyncUpdate = false);
	bool          CheckUpdateProcObjectsInArea(const AABB& areaBox, bool bForceSyncUpdate);
	void          CheckNodesGeomUnload(const SRenderingPassInfo& passInfo);
	void          GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors);
	void          InitTerrainWater(IMaterial* pTerrainWaterMat, int nWaterBottomTexId);
	void          ResetTerrainVertBuffers(const AABB* pBox);
	void          SetTerrainSectorTexture(int nTexSectorX, int nTexSectorY, unsigned int textureId, bool bMergeNotAllowed);
	void          SetDetailLayerProperties(int nId, float fScaleX, float fScaleY, uint8 ucProjAxis, const char* szSurfName, const PodArray<int>& lstnVegetationGroups, IMaterial* pMat);
	bool          IsOceanVisible() { return m_bOceanIsVisible != 0; }
	void          SetTerrainElevation(int x1, int y1, int nSizeX, int nSizeY, float* pTerrainBlock, SSurfaceTypeItem* pSurfaceData, int nSurfOrgX, int nSurfOrgY, int nSurfSizeX, int nSurfSizeY, uint32* pResolMap, int nResolMapSizeX, int nResolMapSizeY);
	void          HighlightTerrain(int x1, int y1, int x2, int y2);
	bool          CanPaintSurfaceType(int x, int y, int r, uint16 usGlobalSurfaceType);
	void          LoadSurfaceTypesFromXML(XmlNodeRef pDoc);
	void          UpdateSurfaceTypes();
	bool          RenderArea(Vec3 vPos, float fRadius, _smart_ptr<IRenderMesh>& arrLightRenderMeshs, CRenderObject* pObj, IMaterial* pMaterial, const char* szComment, float* pCustomData, Plane* planes, const SRenderingPassInfo& passInfo);
	void          IntersectWithShadowFrustum(PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, const SRenderingPassInfo& passInfo);
	void          IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult);
	void          MarkAllSectorsAsUncompiled();
	void          GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB);
	void          UpdateSectorMeshes(const SRenderingPassInfo& passInfo);
	void          AddVisSector(CTerrainNode* pNode, uint32 passCullMask);

	void          GetVegetationMaterials(std::vector<IMaterial*>*& pMatTable);
	void          LoadVegetationData(PodArray<struct StatInstGroup>& rTable, PodArray<StatInstGroupChunk>& lstFileChunks, int i);

	CTerrainNode* GetParentNode() const { return m_pParentNode; }

	bool          Recompile_Modified_Incrementaly_RoadRenderNodes();

	virtual void  SerializeTerrainState(TSerialize ser);

	void          SetHeightMapMaxHeight(float fMaxHeight);

	SSurfaceType* GetSurfaceTypePtr(float x, float y)
	{
		int nType = GetSurfaceTypeID(x, y);
		assert(nType >= 0 && nType < SRangeInfo::e_max_surface_types);
		return (nType < SRangeInfo::e_undefined) ? &m_SSurfaceType[nType] : NULL;
	}

	SSurfaceType* GetSurfaceTypes()                 { return m_SSurfaceType.GetElements(); }
	int           GetSurfaceTypeCount() const       { return (int) m_SSurfaceType.size(); }

	int           GetTerrainTextureNodeSizeMeters() { return GetSectorSize(); }

	int           GetTerrainTextureNodeSizePixels(int nLayer)
	{
		assert(nLayer >= 0 && nLayer < 2);
		return m_arrBaseTexInfos.m_TerrainTextureLayer[nLayer].nSectorSizePixels;
	}

	void GetMaterials(IMaterial*& pTerrainEf)
	{
		pTerrainEf = m_pTerrainEf;
	}

	IMaterial* GetMaterial()
	{
		return m_pTerrainEf;
	}

	int m_nWhiteTexId;
	int m_nBlackTexId;

	PodArray<Array2d<struct CTerrainNode*>> m_arrSecInfoPyramid;         //[TERRAIN_NODE_TREE_DEPTH];

	float                         GetTerrainTextureMultiplier() const { return m_arrBaseTexInfos.m_hdrDiffTexInfo.fBrMultiplier; }

	void                          ActivateNodeTexture(CTerrainNode* pNode, const SRenderingPassInfo& passInfo);
	void                          ActivateNodeProcObj(CTerrainNode* pNode);
	CTerrainNode*                 FindMinNodeContainingBox(const AABB& someBox);
	int                           GetTerrainLightmapTexId(Vec4& vTexGenInfo);
	void                          GetAtlasTexId(int& nTex0, int& nTex1, int& nTex2);

	_smart_ptr<IRenderMesh>       MakeAreaRenderMesh(const Vec3& vPos, float fRadius, IMaterial* pMat, const char* szLSourceName, Plane* planes);

	template<class T> static bool LoadDataFromFile(T* data, size_t elems, FILE*& f, int& nDataSize, EEndian eEndian, int* pSeek = 0)
	{
		if (pSeek)
			*pSeek = GetPak()->FTell(f);

		if (GetPak()->FRead(data, elems, f, eEndian) != elems)
		{
			assert(0);
			return false;
		}
		nDataSize -= sizeof(T) * elems;
		assert(nDataSize >= 0);
		return true;
	}

	static bool LoadDataFromFile_Seek(size_t elems, FILE*& f, int& nDataSize, EEndian eEndian)
	{
		GetPak()->FSeek(f, elems, SEEK_CUR);
		nDataSize -= elems;
		assert(nDataSize >= 0);
		return (nDataSize >= 0);
	}

	template<class T> static bool LoadDataFromFile(T* data, size_t elems, uint8*& f, int& nDataSize, EEndian eEndian, int* pSeek = 0)
	{
		StepDataCopy(data, f, elems, eEndian);
		nDataSize -= elems * sizeof(T);
		assert(nDataSize >= 0);
		return (nDataSize >= 0);
	}

	static bool LoadDataFromFile_Seek(size_t elems, uint8*& f, int& nDataSize, EEndian eEndian)
	{
		nDataSize -= elems;
		f += elems;
		assert(nDataSize >= 0);
		return true;
	}

	static void LoadDataFromFile_FixAllignemt(FILE*& f, int& nDataSize)
	{
		while (nDataSize & 3)
		{
#if defined(USE_CRY_ASSERT)
			int nRes = GetPak()->FSeek(f, 1, SEEK_CUR);
			assert(nRes == 0);
#else
			GetPak()->FSeek(f, 1, SEEK_CUR);
#endif
			assert(nDataSize);
			nDataSize--;
		}
		assert(nDataSize >= 0);
	}

	static void LoadDataFromFile_FixAllignemt(uint8*& f, int& nDataSize)
	{
		while (nDataSize & 3)
		{
			assert(*f == 222);
			f++;
			assert(nDataSize);
			nDataSize--;
		}
		assert(nDataSize >= 0);
	}

	int         ReloadModifiedHMData(FILE* f);

	bool        IsHeightMapModified() const { return m_bHeightMapModified; }
	void        SetHeightMapModified()      { m_bHeightMapModified = true; }
	static void ResetHeightMapCache();

protected:

	void                BuildSectorsTree(bool bBuildErrorsTable);
	int                 GetTerrainNodesAmount();
	bool                OpenTerrainTextureFile(SCommonFileHeader& hdrDiffTexHdr, STerrainTextureFileHeader& hdrDiffTexInfo, const char* szFileName, uint8*& ucpDiffTexTmpBuffer, int& nDiffTexIndexTableSize);
	bool                IsRenderNodeIncluded(IRenderNode* pNode, const AABB& region, const uint16* pIncludeLayers, int numIncludeLayers);

	float               GetZfromUnits(int nX_units, int nY_units) const;
	float               GetZMaxFromUnits(int nX0_units, int nY0_units, int nX1_units, int nY1_units) const;
	Vec4                Get4ZUnits(int nX_units, int nY_units) const;
	uint8               GetSurfTypeFromUnits(int nX_units, int nY_units) const;
	SSurfaceTypeItem    GetSurfTypeItemfromUnits(int nX_units, int nY_units) const;
	bool                IsPointUnderGround(int nX_units, int nY_units, float fTestZ);

	ILINE CTerrainNode* GetSecInfoUnits(int xu, int yu) const
	{
		return m_arrSecInfoPyramid[0][xu >> m_nUnitsToSectorBitShift][yu >> m_nUnitsToSectorBitShift];
	}
	ILINE CTerrainNode* GetSecInfoUnitsSafe(int xu, int yu) const
	{
		if (m_arrSecInfoPyramid.IsEmpty())
			return nullptr;
		return GetSecInfoUnits(xu, yu);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Variables
	////////////////////////////////////////////////////////////////////////////////////////////////////

	friend class CTerrainUpdateDispatcher;

	CTerrainNode*         m_pParentNode = nullptr;
	int                   m_terrainPaintingFrameId = 0;

	CTerrainModifications m_StoredModifications;                // to serialize (load/save) terrain heighmap changes and limit the modification
	int                   m_nLoadedSectors;                     //
	bool                  m_bOceanIsVisible;                    //
	bool                  m_bProcVegetationInUse;
	bool                  m_bHeightMapModified;
	int                   m_nUnitsToSectorBitShift;
	float                 m_fHeightmapZRatio;
	float                 m_fDistanceToSectorWithWater;         //

	struct SBaseTexInfo
	{
		SBaseTexInfo() { ZeroStruct(*this); }
		int                            m_nDiffTexIndexTableSize;
		SCommonFileHeader              m_hdrDiffTexHdr;
		STerrainTextureFileHeader      m_hdrDiffTexInfo;
		STerrainTextureLayerFileHeader m_TerrainTextureLayer[2];
		uint8*                         m_ucpDiffTexTmpBuffer;
		void                           GetMemoryUsage(ICrySizer* pSizer) const
		{
			pSizer->AddObject(m_ucpDiffTexTmpBuffer, m_TerrainTextureLayer[0].nSectorSizeBytes + m_TerrainTextureLayer[1].nSectorSizeBytes);
		}
	};

	SBaseTexInfo                   m_arrBaseTexInfos;

	_smart_ptr<IMaterial>          m_pTerrainEf;

	float                          m_fOceanWaterLevel;

	PodArray<STerrainVisItem>      m_lstVisSectors;
	PodArray<struct CTerrainNode*> m_lstUpdatedSectors;
	int                            m_checkVisSectorsCount = 0;

	PodArray<SSurfaceType>         m_SSurfaceType;

	static float                   m_fUnitSize;      // in meters
	static float                   m_fInvUnitSize;   // in 1/meters
	static int                     m_nTerrainSize;   // in meters
	int                            m_nTerrainUnits;  // in units
	static int                     m_nSectorSize;       // in meters
	static int                     m_nSectorsTableSize; // sector width/height of the finest LOD level (sector count is the square of this value)

	class COcean*                  m_pOcean;

	PodArray<CTerrainNode*>        m_lstActiveTextureNodes;
	PodArray<CTerrainNode*>        m_lstActiveProcObjNodes;
	bool                           m_isOfflineProceduralVegetationReady = false;

	CTextureCache                  m_texCache[3]; // RGB, Normal and Height

	EEndian                        m_eEndianOfTexture;

	static bool                    m_bOpenTerrainTextureFileNoLog;

	CTerrainUpdateDispatcher*      m_pTerrainUpdateDispatcher;

	// Height and SurfaceType cache
	union SCachedHeight
	{
		SCachedHeight() : packedValue(0)
		{
			static_assert(sizeof(x) + sizeof(y) + sizeof(fHeight) == sizeof(packedValue), "SCachedHeight: Unexpected data size!");
		}
		SCachedHeight(const SCachedHeight &other)
			: packedValue(other.packedValue)
		{}

		struct
		{
			uint16 x, y;
			float  fHeight;
		};
		uint64 packedValue;
	};

	union SCachedSurfType
	{
		struct
		{
			uint16 x, y;
			uint32 surfType;
		};
		uint64 packedValue;

		SCachedSurfType() : packedValue(0)
		{
			static_assert(sizeof(x) + sizeof(y) + sizeof(surfType) == sizeof(packedValue), "SCachedSurfType: Unexpected data size!");
		}
		SCachedSurfType(const SCachedSurfType &other)
			: packedValue(other.packedValue)
		{}
	};

	static CRY_ALIGN(128) SCachedHeight m_arrCacheHeight[nHMCacheSize * nHMCacheSize];
	static CRY_ALIGN(128) SCachedSurfType m_arrCacheSurfType[nHMCacheSize * nHMCacheSize];

	static float GetHeightFromUnits_Callback(int ix, int iy);
	static uint8 GetSurfaceTypeFromUnits_Callback(int ix, int iy);

#if defined(FEATURE_SVO_GI)
	PodArray<ColorB>* m_pTerrainRgbLowResSystemCopy;
	int               m_terrainRgbLowResSystemCopyUserData = 0;
#endif

	_smart_ptr<IRenderMesh> m_pSharedRenderMesh;

public:
	bool    SetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo);
	bool    Load(FILE* f, int nDataSize, struct STerrainChunkHeader* pTerrainChunkHeader, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable);
	EEndian GetEndianOfTexture() { return m_eEndianOfTexture; }

	void    ClearVisSectors()
	{
		m_lstVisSectors.Clear();
		m_checkVisSectorsCount = 0;
	}

#if defined(FEATURE_SVO_GI)
	const PodArray<ColorB>* GetTerrainRgbLowResSystemCopy(int** ppUserData = nullptr) { if (ppUserData) *ppUserData = &m_terrainRgbLowResSystemCopyUserData; return m_pTerrainRgbLowResSystemCopy; }
#endif
};
