// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#define TERRAIN_BOTTOM_LEVEL     0
#define TERRAIN_NODE_TREE_DEPTH  16
#define TERRAIN_TEX_OFFSETS_SIZE 16
#define OCEAN_IS_VERY_FAR_AWAY   1000000.f

enum { nHMCacheSize = 64 };

class CTerrainUpdateDispatcher;

typedef std::pair<struct CTerrainNode*, uint32> STerrainVisItem;

// Heightmap data
class CHeightMap : public Cry3DEngineBase
{
public:
	// Access to heightmap data
	float                GetZSafe(float x, float y);
	float                GetZ(float x, float y, bool bUpdatePos = false) const;
	void                 SetZ(const float x, const float y, float fHeight);
	float                GetZfromUnits(uint32 nX_units, uint32 nY_units) const;
	void                 SetZfromUnits(uint32 nX_units, uint32 nY_units, float fHeight);
	float                GetZMaxFromUnits(uint32 nX0_units, uint32 nY0_units, uint32 nX1_units, uint32 nY1_units) const;
	uint8                GetSurfTypeFromUnits(uint32 nX_units, uint32 nY_units) const;
	SSurfaceTypeItem     GetSurfTypeItemfromUnits(uint32 nX_units, uint32 nY_units) const;
	static float         GetHeightFromUnits_Callback(int ix, int iy);
	static unsigned char GetSurfaceTypeFromUnits_Callback(int ix, int iy);

	uint8                GetSurfaceTypeID(float x, float y) const;
	SSurfaceTypeItem     GetSurfaceTypeItem(float x, float y) const;
	float                GetZApr(float x1, float y1) const;
	float                GetZMax(float x0, float y0, float x1, float y1) const;
	bool                 GetHole(float x, float y) const;
	bool                 IntersectWithHeightMap(Vec3 vStartPoint, Vec3 vStopPoint, float fDist, int nMaxTestsToScip);
	bool                 IsMeshQuadFlipped(const float x, const float y, const float unitSize) const;

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
	bool RayTrace(Vec3 const& vStart, Vec3 const& vEnd, SRayTrace* prt, bool bClampAbove = true);

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
	                        float fTestZ);

	inline bool IsPointUnderGround(uint32 nX_units, uint32 nY_units, float fTestZ)
	{
		CTerrain* const __restrict pTerrain = Cry3DEngineBase::GetTerrain();
		return IsPointUnderGround(pTerrain, m_nUnitsToSectorBitShift, nX_units, nY_units, fTestZ);
	}

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

// The Terrain Class
class CTerrain : public ITerrain, public CHeightMap
{
	friend struct CTerrainNode;

public:

	CTerrain(const STerrainInfo& TerrainInfo);
	~CTerrain();

	void                      InitHeightfieldPhysics();
	void                      SetMaterialMapping();
	inline static const int   GetTerrainSize()               { return m_nTerrainSize; }
	inline static const int   GetSectorSize()                { return m_nSectorSize; }
	inline static const float GetHeightMapUnitSize()         { return m_fUnitSize; }
	inline static const float GetHeightMapUnitSizeInverted() { return m_fInvUnitSize; }
	inline static const int   GetSectorsTableSize()
	{
		return m_nSectorsTableSize;
	}
	inline static const float GetInvUnitSize()  { return m_fInvUnitSize; }

	ILINE const int           GetTerrainUnits() { return m_nTerrainSizeDiv; }
	ILINE void                ClampUnits(uint32& xu, uint32& yu)
	{
		xu = (int)xu < 0 ? 0 : (int)xu < GetTerrainUnits() ? xu : GetTerrainUnits();//min(max((int)xu, 0), GetTerrainUnits());
		yu = (int)yu < 0 ? 0 : (int)yu < GetTerrainUnits() ? yu : GetTerrainUnits();//min(max((int)xu, 0), GetTerrainUnits());
	}
	ILINE CTerrainNode* GetSecInfoUnits(int xu, int yu)
	{
		if (m_arrSecInfoPyramid.IsEmpty())
			return nullptr;

		return m_arrSecInfoPyramid[0][xu >> m_nUnitsToSectorBitShift][yu >> m_nUnitsToSectorBitShift];
	}
	ILINE CTerrainNode* GetSecInfoUnits(int xu, int yu, int nUnitsToSectorBitShift)
	{
		return m_arrSecInfoPyramid[0][xu >> nUnitsToSectorBitShift][yu >> nUnitsToSectorBitShift];
	}
	CTerrainNode* GetSecInfo(float x, float y)
	{
		if (x < 0 || y < 0 || x >= m_nTerrainSize || y >= m_nTerrainSize || !m_pParentNode)
			return 0;

		return GetSecInfoUnits(int(x / GetTerrain()->GetHeightMapUnitSize()), int(y / GetTerrain()->GetHeightMapUnitSize()));
	}
	CTerrainNode* GetSecInfo(int x, int y)
	{
		Vec3 vPos((float)x, (float)y, 0);
		return GetSecInfo(vPos);
	}
	ILINE CTerrainNode* GetSecInfo(const Vec3& pos)
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
	virtual void         OnTerrainPaintActionComplete() {};
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
	int           GetActiveProcObjNodesCount() { return m_lstActiveProcObjNodes.Count(); }
	int           GetNotReadyTextureNodesCount();
	void          GetTextureCachesStatus(int& nCount0, int& nCount1)
	{ nCount0 = m_texCache[0].GetPoolSize(); nCount1 = m_texCache[1].GetPoolSize(); }

	void CheckVis(const SRenderingPassInfo& passInfo, uint32 passCullMask);
	int  UpdateOcean(const SRenderingPassInfo& passInfo);
	int  RenderOcean(const SRenderingPassInfo& passInfo);
	void UpdateNodesIncrementaly(const SRenderingPassInfo& passInfo);
	void CheckNodesGeomUnload(const SRenderingPassInfo& passInfo);
	void GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors);
	void InitTerrainWater(IMaterial* pTerrainWaterMat, int nWaterBottomTexId);
	void ResetTerrainVertBuffers(const AABB* pBox);
	void SetTerrainSectorTexture(int nTexSectorX, int nTexSectorY, unsigned int textureId, bool bMergeNotAllowed);
	void SetDetailLayerProperties(int nId, float fScaleX, float fScaleY, uint8 ucProjAxis, const char* szSurfName, const PodArray<int>& lstnVegetationGroups, IMaterial* pMat);
	bool IsOceanVisible() { return m_bOceanIsVisible != 0; }
	void SetTerrainElevation(int x1, int y1, int nSizeX, int nSizeY, float* pTerrainBlock, SSurfaceTypeItem* pSurfaceData, int nSurfOrgX, int nSurfOrgY, int nSurfSizeX, int nSurfSizeY, uint32* pResolMap, int nResolMapSizeX, int nResolMapSizeY);
	void HighlightTerrain(int x1, int y1, int x2, int y2);
	bool CanPaintSurfaceType(int x, int y, int r, uint16 usGlobalSurfaceType);
	void LoadSurfaceTypesFromXML(XmlNodeRef pDoc);
	void UpdateSurfaceTypes();
	bool RenderArea(Vec3 vPos, float fRadius, _smart_ptr<IRenderMesh>& arrLightRenderMeshs, CRenderObject* pObj, IMaterial* pMaterial, const char* szComment, float* pCustomData, Plane* planes, const SRenderingPassInfo& passInfo);
	void IntersectWithShadowFrustum(PodArray<IShadowCaster*>* plstResult, ShadowMapFrustum* pFrustum, const SRenderingPassInfo& passInfo);
	void IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult);
	void MarkAllSectorsAsUncompiled();
	void GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& crstAABB);
	void UpdateSectorMeshes(const SRenderingPassInfo& passInfo);
	void AddVisSector(CTerrainNode* pNode, uint32 passCullMask);

	void GetVegetationMaterials(std::vector<IMaterial*>*& pMatTable);
	void LoadVegetationData(PodArray<struct StatInstGroup>& rTable, PodArray<StatInstGroupChunk>& lstFileChunks, int i);

protected:

	CTerrainNode* m_pParentNode = nullptr;
	int           m_terrainPaintingFrameId = 0;
	void       BuildSectorsTree(bool bBuildErrorsTable);
	int        GetTerrainNodesAmount();
	bool       OpenTerrainTextureFile(SCommonFileHeader& hdrDiffTexHdr, STerrainTextureFileHeader& hdrDiffTexInfo, const char* szFileName, uint8*& ucpDiffTexTmpBuffer, int& nDiffTexIndexTableSize);
	ILINE bool IsRenderNodeIncluded(IRenderNode* pNode, const AABB& region, const uint16* pIncludeLayers, int numIncludeLayers);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Variables
	////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	CTerrainNode* GetParentNode() { return m_pParentNode; }

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

	int ReloadModifiedHMData(FILE* f);

protected: // ------------------------------------------------------------------------
	friend class CTerrainUpdateDispatcher;

	CTerrainModifications m_StoredModifications;                // to serialize (load/save) terrain heighmap changes and limit the modification
	int                   m_nLoadedSectors;                     //
	int                   m_bOceanIsVisible;                    //

	float                 m_fDistanceToSectorWithWater;         //
	bool                  m_bProcVegetationInUse;

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
	int                            m_nTerrainSizeDiv;
	static int                     m_nSectorSize;       // in meters
	static int                     m_nSectorsTableSize; // sector width/height of the finest LOD level (sector count is the square of this value)

	class COcean*                  m_pOcean;

	PodArray<CTerrainNode*>        m_lstActiveTextureNodes;
	PodArray<CTerrainNode*>        m_lstActiveProcObjNodes;

	CTextureCache                  m_texCache[3]; // RGB, Normal and Height

	EEndian                        m_eEndianOfTexture;

	static bool                    m_bOpenTerrainTextureFileNoLog;

	CTerrainUpdateDispatcher*      m_pTerrainUpdateDispatcher;

#if defined(FEATURE_SVO_GI)
	PodArray<ColorB>* m_pTerrainRgbLowResSystemCopy;
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
	const PodArray<ColorB>* GetTerrainRgbLowResSystemCopy() { return m_pTerrainRgbLowResSystemCopy; }
#endif
};

#endif // TERRAIN_H
