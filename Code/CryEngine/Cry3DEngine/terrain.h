// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   terrain.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef TERRAIN_H
#define TERRAIN_H

#include "TerrainModifications.h"           // CTerrainModifications

// lowest level of the outdoor world
#define TERRAIN_BOTTOM_LEVEL      0

#define TERRAIN_BASE_TEXTURES_NUM 2

// max view distance for objects shadow is size of object multiplied by this number
#define OBJ_MAX_SHADOW_VIEW_DISTANCE_RATIO 4

#define TERRAIN_NODE_TREE_DEPTH            16

#define OCEAN_IS_VERY_FAR_AWAY             1000000.f

#define ARR_TEX_OFFSETS_SIZE_DET_MAT       16

enum { nHMCacheSize = 64 };

class CTerrainUpdateDispatcher;

// Heightmap data
class CHeightMap : public Cry3DEngineBase
{
public:
	// Access to heightmap data
	float GetZSafe(float x, float y, int nSID);
	float GetZ(float x, float y, int nSID, bool bUpdatePos = false) const;
	void SetZ(const float x, const float y, float fHeight, int nSID);
	float                GetZfromUnits(uint32 nX_units, uint32 nY_units, int nSID) const;
	void                 SetZfromUnits(uint32 nX_units, uint32 nY_units, float fHeight, int nSID);
	float                GetZMaxFromUnits(uint32 nX0_units, uint32 nY0_units, uint32 nX1_units, uint32 nY1_units, int nSID) const;
	uint8                GetSurfTypeFromUnits(uint32 nX_units, uint32 nY_units, int nSID) const;
	SSurfaceTypeItem     GetSurfTypeItemfromUnits(uint32 nX_units, uint32 nY_units, int nSID) const;
	static float GetHeightFromUnits_Callback(int ix, int iy);
	static unsigned char GetSurfaceTypeFromUnits_Callback(int ix, int iy);

	uint8                GetSurfaceTypeID(float x, float y, int nSID) const;
	SSurfaceTypeItem     GetSurfaceTypeItem(float x, float y, int nSID) const;
	float                GetZApr(float x1, float y1, int nSID) const;
	float                GetZMax(float x0, float y0, float x1, float y1, int nSID) const;
	bool GetHole(float x, float y, int nSID) const;
	bool                 IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip, int nSID);
	bool IsMeshQuadFlipped(const float x, const float y, const float unitSize, const int nSID) const;

#ifdef SUPP_HMAP_OCCL
	bool Intersect(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip, Vec3& vLastVisPoint, int nSID);
	bool IsBoxOccluded
	(
	  const AABB& objBox,
	  float fDistance,
	  bool bTerrainNode,
	  OcclusionTestClient* const __restrict pOcclTestVars,
	  int nSID,
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
	bool RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt, int nSID);

	CHeightMap()
	{
		m_nUnitsToSectorBitShift = 0;
		m_fHeightmapZRatio = 0;
		m_bHeightMapModified = 0;
		ResetHeightMapCache();
	}

	void ResetHeightMapCache()
	{
		memset(m_arrCacheHeight, 0, sizeof(m_arrCacheHeight));
		assert(sizeof(m_arrCacheHeight[0]) == 8);
		memset(m_arrCacheSurfType, 0, sizeof(m_arrCacheSurfType));
		assert(sizeof(m_arrCacheSurfType[0]) == 8);
	}

	int   m_nUnitsToSectorBitShift;
	int   m_bHeightMapModified;
	float m_fHeightmapZRatio;

protected:

	//protected: passes some internal data to avoid repetitive member loading
	bool IsPointUnderGround(CTerrain* const __restrict pTerrain,
	                        int nUnitsToSectorBitShift,
	                        uint32 nX_units,
	                        uint32 nY_units,
	                        float fTestZ, int nSID);

	inline bool IsPointUnderGround(uint32 nX_units, uint32 nY_units, float fTestZ, int nSID)
	{
		CTerrain* const __restrict pTerrain = Cry3DEngineBase::GetTerrain();
		return IsPointUnderGround(pTerrain, m_nUnitsToSectorBitShift, nX_units, nY_units, fTestZ, nSID);
	}

	struct SCachedHeight
	{
		SCachedHeight() { x = y = 0; fHeight = 0; }
		uint16 x, y;
		float  fHeight;
	};

	struct SCachedSurfType
	{
		SCachedSurfType() { x = y = surfType = 0; }
		uint16 x, y;
		uint32 surfType;
	};

	static CRY_ALIGN(128) SCachedHeight m_arrCacheHeight[nHMCacheSize * nHMCacheSize];
	static CRY_ALIGN(128) SCachedSurfType m_arrCacheSurfType[nHMCacheSize * nHMCacheSize];
};

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

			//assert(!"SSurfaceType::GetMaterialOfProjection: Material not found");
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
	float                arrRECustomData[4][ARR_TEX_OFFSETS_SIZE_DET_MAT];
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
	int		 GetPoolItemsNum();
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
	int   m_dwRndFlags;

	float fStiffness;
	float fDamping;
	float fVariance;
	float fAirResistance;

	AUTO_STRUCT_INFO_LOCAL;
};

struct SNameChunk
{
	SNameChunk() { memset(this, 0, sizeof(SNameChunk)); }

	char szFileName[256];

	AUTO_STRUCT_INFO_LOCAL;
};

#pragma pack(pop)

enum ETerrainDataLoadStep
{
	eTDLS_NotStarted,
	eTDLS_Initialize,
	eTDLS_BuildSectorsTree,
	eTDLS_SetupEntityGrid,
	eTDLS_ReadTables,
	eTDLS_LoadVegetationStatObjs,
	eTDLS_LoadBrushStatObjs,
	eTDLS_LoadMaterials,
	eTDLS_LoadTerrainNodes,
	eTDLS_StartLoadObjectTree,
	eTDLS_LoadObjectTree,
	eTDLS_FinishUp,
	eTDLS_Complete,
	eTDLS_Error
};

struct STerrainDataLoadStatus
{
	STerrainDataLoadStatus()
	{
		Clear();
	}

	void Clear()
	{
		loadingStep = eTDLS_NotStarted;
		vegetationIndex = 0;
		brushesIndex = 0;
		materialsIndex = 0;
		offset = 0;

		lstVegetation.Clear();
		lstBrushes.Clear();
		lstMaterials.Clear();
		arrUnregisteredObjects.Clear();

		statObjTable.clear();
		matTable.clear();
	}

	ETerrainDataLoadStep         loadingStep;
	PodArray<StatInstGroupChunk> lstVegetation;
	int                          vegetationIndex;
	PodArray<SNameChunk>         lstBrushes;
	int                          brushesIndex;
	PodArray<SNameChunk>         lstMaterials;
	int                          materialsIndex;
	std::vector<IStatObj*>       statObjTable;
	std::vector<IMaterial*>      matTable;
	ptrdiff_t                    offset;
	PodArray<IRenderNode*>       arrUnregisteredObjects;
};

struct TSegmentRect
{
	Rectf localRect;
	int   nSID;
};

// The Terrain Class
class CTerrain : public ITerrain, public CHeightMap
{
	friend struct CTerrainNode;

public:

	CTerrain(const STerrainInfo& TerrainInfo);
	~CTerrain();

	void                    InitHeightfieldPhysics(int nSID);
	void                    SetMaterialMapping(int nSID);
	inline static const int GetTerrainSize()       { return m_nTerrainSize; }
	inline static const int GetSectorSize()        { return m_nSectorSize; }
	inline static const float GetHeightMapUnitSize() { return m_fUnitSize; }
	inline static const float GetHeightMapUnitSizeInverted() { return m_fInvUnitSize; }
	inline static const int GetSectorsTableSize(int nSID)
	{
#ifdef SEG_WORLD
		CTerrain* pThis = Get3DEngine()->GetTerrain();
		AABB aabb;
		if (!pThis->GetSegmentBounds(nSID, aabb))
		{
			assert(0);
			return 0;
		}
		int i = (int) (aabb.max.x - aabb.min.x) >> (pThis->m_nBitShift + pThis->m_nUnitsToSectorBitShift);
		return i;
#else
		return m_nSectorsTableSize;
#endif
	}
	inline static const float GetInvUnitSize()  { return m_fInvUnitSize; }

	ILINE const int           GetTerrainUnits() { return m_nTerrainSizeDiv; }
	ILINE void                ClampUnits(uint32& xu, uint32& yu)
	{
		xu = (int)xu < 0 ? 0 : (int)xu < GetTerrainUnits() ? xu : GetTerrainUnits();//min(max((int)xu, 0), GetTerrainUnits());
		yu = (int)yu < 0 ? 0 : (int)yu < GetTerrainUnits() ? yu : GetTerrainUnits();//min(max((int)xu, 0), GetTerrainUnits());
	}
#ifdef SEG_WORLD
	CTerrainNode* GetSecInfoUnits(int xu, int yu, int nSID)
#else
	ILINE CTerrainNode * GetSecInfoUnits(int xu, int yu, int nSID)
#endif
	{
#ifdef SEG_WORLD
		nSID = WorldToSegment(xu, yu, m_nBitShift, nSID);
		if (nSID < 0)
			return 0;
		if (!m_pParentNodes[nSID])
			return 0;
	#ifdef _DEBUG
		Array2d<struct CTerrainNode*>& arrTerrainNode = m_arrSecInfoPyramid[nSID][0];
		int x = xu >> m_nUnitsToSectorBitShift;
		int y = yu >> m_nUnitsToSectorBitShift;
		assert(x < arrTerrainNode.m_nSize && y < arrTerrainNode.m_nSize);
	#endif
#endif

		if (m_arrSecInfoPyramid.IsEmpty() || m_arrSecInfoPyramid[nSID].IsEmpty())
			return nullptr;

		return m_arrSecInfoPyramid[nSID][0][xu >> m_nUnitsToSectorBitShift][yu >> m_nUnitsToSectorBitShift];
	}
	ILINE CTerrainNode* GetSecInfoUnits(int xu, int yu, int nUnitsToSectorBitShift, int nSID)
	{
#ifdef SEG_WORLD
		assert(nSID >= 0);
#endif
		return m_arrSecInfoPyramid[nSID][0][xu >> nUnitsToSectorBitShift][yu >> nUnitsToSectorBitShift];
	}
	CTerrainNode* GetSecInfo(float x, float y, int nSID)
	{
#ifdef SEG_WORLD
		nSID = WorldToSegment(x, y, 0, nSID);
		if (nSID < 0)
			return 0;
#else
		if (x < 0 || y < 0 || x >= m_nTerrainSize || y >= m_nTerrainSize || !m_pParentNodes[nSID])
			return 0;
#endif
		return GetSecInfoUnits(int(x / GetTerrain()->GetHeightMapUnitSize()), int(y / GetTerrain()->GetHeightMapUnitSize()), nSID);
	}
	CTerrainNode* GetSecInfo(const Vec3& pos)
	{
		for (int nSID = 0; nSID < m_arrSecInfoPyramid.Count() && nSID < m_pParentNodes.Count(); nSID++)
		{
			if (m_pParentNodes[nSID])
				if (Overlap::Point_AABB2D(pos, m_pParentNodes[nSID]->GetBBoxVirtual()))
					return GetSecInfo(pos.x, pos.y, nSID);
		}
		assert(!"Requested segment is not loaded");
		return NULL;
	}
	CTerrainNode* GetSecInfo(int x, int y)
	{
		Vec3 vPos((float)x, (float)y, 0);
		return GetSecInfo(vPos);
	}
	ILINE CTerrainNode* GetSecInfo(const Vec3& pos, int nSID)
	{
		return GetSecInfo(pos.x, pos.y, nSID);
	}
	ILINE float GetWaterLevel()                       { return m_fOceanWaterLevel; /* ? m_fOceanWaterLevel : WATER_LEVEL_UNKNOWN;*/ }
	ILINE void  SetWaterLevel(float oceanWaterLevel) { m_fOceanWaterLevel = oceanWaterLevel; }

	//////////////////////////////////////////////////////////////////////////
	// ITerrain Implementation.
	//////////////////////////////////////////////////////////////////////////

	template<class T>
	bool Load_Tables_T(T*& f, int& nDataSize, std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, bool bHotUpdate, bool bSW, EEndian eEndian);

	template<class T>
	bool                 Load_T(T*& f, int& nDataSize, STerrainChunkHeader* pTerrainChunkHeader, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo, int nSID, Vec3 vSegmentOrigin);

	virtual bool         GetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, std::vector<struct IStatInstGroup*>** ppStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo, int nSID, const Vec3& segmentOffset);
	virtual int          GetCompiledDataSize(SHotUpdateInfo* pExportInfo, int nSID);
	virtual void         GetStatObjAndMatTables(DynArray<IStatObj*>* pStatObjTable, DynArray<IMaterial*>* pMatTable, DynArray<IStatInstGroup*>* pStatInstGroupTable, uint32 nObjTypeMask, int nSID);

	virtual int          GetTablesSize(SHotUpdateInfo* pExportInfo, int nSID);
	virtual void         SaveTables(byte*& pData, int& nDataSize, std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, std::vector<struct IStatInstGroup*>*& pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo, int nSID);
	virtual void         GetTables(std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, std::vector<struct IStatInstGroup*>*& pStatInstGroupTable, int nSID);
	virtual void         ReleaseTables(std::vector<struct IStatObj*>*& pStatObjTable, std::vector<IMaterial*>*& pMatTable, std::vector<struct IStatInstGroup*>*& pStatInstGroupTable);

	virtual IRenderNode* AddVegetationInstance(int nStaticGroupIndex, const Vec3& vPos, const float fScale, uint8 ucBright, uint8 angle, uint8 angleX, uint8 angleY, int nSID);
	virtual void         SetOceanWaterLevel(float oceanWaterLevel);

	virtual int          CreateSegment(Vec3 vSegmentSize, Vec3 vSegmentOrigin = Vec3(0, 0, 0), const char* pcPath = 0);
	virtual bool         DeleteSegment(int nSID, bool bDeleteNow);

	virtual void         ReleaseInactiveSegments();
	virtual bool         SetSegmentPath(int nSID, const char* pcPath);
	virtual const char*  GetSegmentPath(int nSID);
	virtual bool         SetSegmentOrigin(int nSID, Vec3 vSegmentOrigin, bool callOffsetPosition = true);
	virtual const Vec3& GetSegmentOrigin(int nSID) const;
	virtual bool        GetSegmentBounds(int nSID, AABB& bbox);
	virtual int         FindSegment(Vec3 vPt);
	virtual int         FindSegment(int x, int y);
	virtual int         GetMaxSegmentsCount() const;
	virtual int         WorldToSegment(Vec3& vPt, int nSID = DEFAULT_SID);
	virtual int         WorldToSegment(int& x, int& y, int nBitShift, int nSID = DEFAULT_SID);
	virtual void        SplitWorldRectToSegments(const Rectf& worldRect, PodArray<TSegmentRect>& segmentRects);
	virtual void        MarkAndOffsetCloneRegion(const AABB& region, const Vec3& offset);
	virtual void        CloneRegion(const AABB& region, const Vec3& offset, float zRotation, const uint16* pIncludeLayers, int numIncludeLayers);
	virtual void        ClearCloneSources();
	virtual void        ChangeOceanMaterial(IMaterial* pMat);
	virtual void        OnTerrainPaintActionComplete() { m_bTerrainPaintingInProgress = false; };
	bool                IsTerrainPaintingInProgress() { return m_bTerrainPaintingInProgress; };
	//////////////////////////////////////////////////////////////////////////

	void          RemoveAllStaticObjects(int nSID);
	void          ApplyForceToEnvironment(Vec3 vPos, float fRadius, float fAmountOfForce);
	//	void CheckUnload();
	void          CloseTerrainTextureFile(int nSID);
	void          DrawVisibleSectors(const SRenderingPassInfo& passInfo);
	virtual int   GetDetailTextureMaterials(IMaterial* materials[], int nSID);
	void          MakeCrater(Vec3 vExploPos, float fExploRadius);
	bool          RemoveObjectsInArea(Vec3 vExploPos, float fExploRadius);
	float         GetDistanceToSectorWithWater() { return m_fDistanceToSectorWithWater; }
	void GetMemoryUsage(class ICrySizer* pSizer) const;
	void          GetObjects(PodArray<struct SRNInfo>* pLstObjects);
	void          GetObjectsAround(Vec3 vPos, float fRadius, PodArray<struct SRNInfo>* pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS, bool bSkipDynamicObjects);
	class COcean* GetOcean() { return m_pOcean; }
	Vec3          GetTerrainSurfaceNormal(Vec3 vPos, float fRange, int nSID = GetDefSID());
	Vec3 GetTerrainSurfaceNormal_Int(float x, float y, int nSID);
	void          GetTerrainAlignmentMatrix(const Vec3& vPos, const float amount, Matrix33& matrix33);
	int           GetActiveTextureNodesCount() { return m_lstActiveTextureNodes.Count(); }
	int           GetActiveProcObjNodesCount() { return m_lstActiveProcObjNodes.Count(); }
	int           GetNotReadyTextureNodesCount();
	void          GetTextureCachesStatus(int& nCount0, int& nCount1)
	{ nCount0 = m_texCache[0].GetPoolSize(); nCount1 = m_texCache[1].GetPoolSize(); }

	void CheckVis(const SRenderingPassInfo& passInfo);
	int  UpdateOcean(const SRenderingPassInfo& passInfo);
	int  RenderOcean(const SRenderingPassInfo& passInfo);
	void UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo);
	void CheckNodesGeomUnload(int nSID, const SRenderingPassInfo& passInfo);
	void GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors);
	void InitTerrainWater(IMaterial* pTerrainWaterMat, int nWaterBottomTexId);
	void ResetTerrainVertBuffers(const AABB* pBox, int nSID = GetDefSID());
	void SetTerrainSectorTexture(int nTexSectorX, int nTexSectorY, unsigned int textureId, bool bMergeNotAllowed, int nSID = GetDefSID());
	void SetDetailLayerProperties(int nId, float fScaleX, float fScaleY, uint8 ucProjAxis, const char* szSurfName, const PodArray<int>& lstnVegetationGroups, IMaterial* pMat, int nSID);
	bool IsOceanVisible() { return m_bOceanIsVisible != 0; }
	void SetTerrainElevation(int x1, int y1, int nSizeX, int nSizeY, float* pTerrainBlock, SSurfaceTypeItem* pSurfaceData, int nSurfOrgX, int nSurfOrgY, int nSurfSizeX, int nSurfSizeY, uint32* pResolMap, int nResolMapSizeX, int nResolMapSizeY, int nSID);
	void HighlightTerrain(int x1, int y1, int x2, int y2, int nSID = GetDefSID());
	bool CanPaintSurfaceType(int x, int y, int r, uint16 usGlobalSurfaceType);
	void GetVisibleSectorsInAABB(PodArray<struct CTerrainNode*>& lstBoxSectors, const AABB& boxBox);
	void LoadSurfaceTypesFromXML(XmlNodeRef pDoc, int nSID);
	void UpdateSurfaceTypes(int nSID);
	bool RenderArea(Vec3 vPos, float fRadius, _smart_ptr<IRenderMesh>& arrLightRenderMeshs, CRenderObject* pObj, IMaterial* pMaterial, const char* szComment, float* pCustomData, Plane* planes, const SRenderingPassInfo& passInfo);
	void IntersectWithShadowFrustum(PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, int nSID, const SRenderingPassInfo& passInfo);
	void IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult, int nSID);
	void MarkAllSectorsAsUncompiled(int nSID);
	void BuildErrorsTableForArea(float* pLodErrors, int nMaxLods, int X1, int Y1, int X2, int Y2, float* pTerrainBlock,
		SSurfaceTypeItem* pSurfaceData, int nSurfOffsetX, int nSurfOffsetY, int nSurfSizeX, int nSurfSizeY, bool& bHasHoleEdges);

	void GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB, int nSID);
	void UpdateSectorMeshes(const SRenderingPassInfo& passInfo);
	void AddVisSector(CTerrainNode* newsec);

	void ReleaseHeightmapGeometryAroundSegment(int nSID);
	void ResetHeightmapGeometryAroundSegment(int nSID);

	Vec3 GetSegmentOrigin(int nSID);
	void GetVegetationMaterials(std::vector<IMaterial*>*& pMatTable);
	void LoadVegetationData(PodArray<struct StatInstGroup>& rTable, PodArray<StatInstGroupChunk>& lstFileChunks, int i);

protected:

	PodArray<CTerrainNode*>          m_pParentNodes;
	PodArray<CTerrainNode*>          m_lstSectors;

	PodArray<Vec3>                   m_arrSegmentOrigns;

	PodArray<Vec2i>                  m_arrSegmentSizeUnits;
	PodArray<string>                 m_arrSegmentPaths;
	PodArray<STerrainDataLoadStatus> m_arrLoadStatuses;
	PodArray<int>                    m_arrDeletedSegments;

	bool m_bTerrainPaintingInProgress = false;

	void       BuildSectorsTree(bool bBuildErrorsTable, int nSID);
	int        GetTerrainNodesAmount(int nSID);
	bool       OpenTerrainTextureFile(SCommonFileHeader& hdrDiffTexHdr, STerrainTextureFileHeader& hdrDiffTexInfo, const char* szFileName, uint8*& ucpDiffTexTmpBuffer, int& nDiffTexIndexTableSize, int nSID);
	ILINE bool IsRenderNodeIncluded(IRenderNode* pNode, const AABB& region, const uint16* pIncludeLayers, int numIncludeLayers);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Variables
	////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	CTerrainNode* GetParentNode(int nId) { return (nId >= 0 && nId < m_pParentNodes.Count()) ? m_pParentNodes.GetAt(nId) : NULL; }

	bool          Recompile_Modified_Incrementaly_RoadRenderNodes();

	virtual void  SerializeTerrainState(TSerialize ser);

	void          SetHeightMapMaxHeight(float fMaxHeight);

	SSurfaceType* GetSurfaceTypePtr(float x, float y, int nSID)
	{
#ifdef SEG_WORLD
		nSID = WorldToSegment(x, y, 0, nSID);
		if (nSID < 0)
			return NULL;
#endif
		int nType = GetSurfaceTypeID(x, y, nSID);
		assert(nType >= 0 && nType < SRangeInfo::e_max_surface_types);
		return (nType < SRangeInfo::e_undefined) ? &m_SSurfaceType[nSID][nType] : NULL;
	}

	SSurfaceType* GetSurfaceTypes(int nSID)           { return &m_SSurfaceType[nSID][0]; }
	int           GetSurfaceTypeCount(int nSID) const { return (int) m_SSurfaceType[nSID].size(); }

	int           GetTerrainTextureNodeSizeMeters()   { return GetSectorSize(); }

	int           GetTerrainTextureNodeSizePixels(int nLayer, int nSID)
	{
		assert(nLayer >= 0 && nLayer < 2);
		return m_arrBaseTexInfos[nSID].m_TerrainTextureLayer[nLayer].nSectorSizePixels;
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

	PodArray<PodArray<Array2d<struct CTerrainNode*>>> m_arrSecInfoPyramid;         //[TERRAIN_NODE_TREE_DEPTH];

	float                         GetTerrainTextureMultiplier(int nSID) const { return m_arrBaseTexInfos[nSID].m_hdrDiffTexInfo.fBrMultiplier; }

	void                          ActivateNodeTexture(CTerrainNode* pNode, const SRenderingPassInfo& passInfo);
	void                          ActivateNodeProcObj(CTerrainNode* pNode);
	CTerrainNode*                 FindMinNodeContainingBox(const AABB& someBox, int nSID);
	int                           GetTerrainLightmapTexId(Vec4& vTexGenInfo, int nSID);
	void GetAtlasTexId(int& nTex0, int& nTex1, int& nTex2, int nSID);

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
			int nRes = GetPak()->FSeek(f, 1, SEEK_CUR);
			assert(nRes == 0);
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

	int ReloadModifiedHMData(FILE* f, int nSID);

protected: // ------------------------------------------------------------------------
	friend class CTerrainUpdateDispatcher;

	CTerrainModifications m_StoredModifications;                // to serialize (load/save) terrain heighmap changes and limit the modification
	int                   m_nLoadedSectors;                     //
	int                   m_bOceanIsVisible;                    //

	float                 m_fDistanceToSectorWithWater;         //
	bool                  m_bProcVegetationInUse;

	struct SBaseTexInfo
	{
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

	PodArray<SBaseTexInfo>           m_arrBaseTexInfos;

	_smart_ptr<IMaterial>            m_pTerrainEf;

	float                            m_fOceanWaterLevel;

	PodArray<struct CTerrainNode*>   m_lstVisSectors;
	PodArray<struct CTerrainNode*>   m_lstUpdatedSectors;

	PodArray<PodArray<SSurfaceType>> m_SSurfaceType;

	static float                     m_fUnitSize;    // in meters
	static float                     m_fInvUnitSize; // in 1/meters
	static int                       m_nTerrainSize; // in meters
	int                              m_nTerrainSizeDiv;
	static int                       m_nSectorSize; // in meters
#ifndef SEG_WORLD
	static int                       m_nSectorsTableSize; // sector width/height of the finest LOD level (sector count is the square of this value)
#endif

	class COcean*             m_pOcean;

	PodArray<CTerrainNode*>   m_lstActiveTextureNodes;
	PodArray<CTerrainNode*>   m_lstActiveProcObjNodes;

	CTextureCache m_texCache[3]; // RGB, Normal and Height

	EEndian                   m_eEndianOfTexture;

	static bool               m_bOpenTerrainTextureFileNoLog;

	CTerrainUpdateDispatcher* m_pTerrainUpdateDispatcher;

#if defined(FEATURE_SVO_GI)
	PodArray<ColorB>* m_pTerrainRgbLowResSystemCopy;
#endif

	_smart_ptr<IRenderMesh> m_pSharedRenderMesh;

public:
	bool    SetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo, int nSID, Vec3 vSegmentOrigin);
	bool    Load(FILE* f, int nDataSize, struct STerrainChunkHeader* pTerrainChunkHeader, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, int nSID, Vec3 vSegmentOrigin);
	EEndian GetEndianOfTexture() { return m_eEndianOfTexture; }

	void    ClearVisSectors()
	{
		m_lstVisSectors.Clear();
	}

#if defined(FEATURE_SVO_GI)
	const PodArray<ColorB>* GetTerrainRgbLowResSystemCopy() { return m_pTerrainRgbLowResSystemCopy; }
#endif
	virtual bool            StreamCompiledData(byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	virtual void            CancelStreamCompiledData(int nSID);

private:
	void StreamStep_Initialize(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_BuildSectorsTree(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_SetupEntityGrid(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_ReadTables(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_LoadVegetationStatObjs(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_LoadBrushStatObjs(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_LoadMaterials(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_LoadTerrainNodes(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_StartLoadObjectTree(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_LoadObjectTree(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
	void StreamStep_FinishUp(STerrainDataLoadStatus& status, byte* pData, int nDataSize, int nSID, const Vec3& vSegmentOrigin);
};

#endif // TERRAIN_H
