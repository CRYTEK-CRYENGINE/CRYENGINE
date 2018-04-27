// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   visareas.h
//  Version:     v1.00
//  Created:     18/12/2002 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: visibility areas header
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef VisArea_H
#define VisArea_H

#include "BasicArea.h"
#include "PolygonClipContext.h"

// Unique identifier for each VisArea instance
typedef unsigned int VisAreaId;

typedef uint64       VisAreaGUID;

#define ReservedVisAreaBytes 384

enum EVisAreaColdDataType
{
	eCDT_Generic = 0,
	eCDT_Portal
};

struct SGenericColdData
{
	SGenericColdData()
	{
		m_dataType = eCDT_Generic;
		m_sName[0] = 0;
	}

	ILINE void ResetGenericData()
	{
		m_dataType = eCDT_Generic;
		m_sName[0] = 0;
	}

	EVisAreaColdDataType m_dataType;
	char                 m_sName[64];
};

struct SPortalColdData : public SGenericColdData
{
	SPortalColdData()
	{
		m_dataType = eCDT_Portal;
	}

	ILINE void ResetPortalData()
	{
		m_dataType = eCDT_Portal;
	}

	OcclusionTestClient m_occlusionTestClient;
};

struct CVisArea : public IVisArea, public CBasicArea
{
	static void StaticReset();

	// editor interface
	virtual void Update(const Vec3* pPoints, int nCount, const char* szName, const SVisAreaInfo& info);

	CVisArea();
	CVisArea(VisAreaGUID visGUID);
	virtual ~CVisArea();

	bool                    operator==(const CVisArea& v1) const { return &v1 == this; }

	void                    Init();
	ILINE SGenericColdData* GetColdData()                               { return m_pVisAreaColdData; }
	ILINE void              SetColdDataPtr(SGenericColdData* pColdData) { m_pVisAreaColdData = pColdData; }
	bool                    IsSphereInsideVisArea(const Vec3& vPos, const f32 fRadius);
	bool                    IsPointInsideVisArea(const Vec3& vPos) const;
	bool                    IsBoxOverlapVisArea(const AABB& objBox);
	bool                    ClipToVisArea(bool bInside, Sphere& sphere, Vec3 const& vNormal);
	bool                    FindVisArea(IVisArea* pAnotherArea, int nMaxRecursion, bool bSkipDisabledPortals);
	bool                    FindVisAreaReqursive(IVisArea* pAnotherArea, int nMaxReqursion, bool bSkipDisabledPortals, StaticDynArray<CVisArea*, 1024>& arrVisitedParents);
	bool                    GetDistanceThruVisAreas(AABB vCurBoxIn, IVisArea* pTargetArea, const AABB& targetBox, int nMaxReqursion, float& fResDist);
	bool                    GetDistanceThruVisAreasReq(AABB vCurBoxIn, float fCurDistIn, IVisArea* pTargetArea, const AABB& targetBox, int nMaxReqursion, float& fResDist, CVisArea* pPrevArea, int nCallID);
	void                    FindSurroundingVisArea(int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*>* pVisitedAreas = NULL, int nMaxVisitedAreas = 0, int nDeepness = 0);
	void                    FindSurroundingVisAreaReqursive(int nMaxReqursion, bool bSkipDisabledPortals, PodArray<IVisArea*>* pVisitedAreas, int nMaxVisitedAreas, int nDeepness, PodArray<CVisArea*>* pUnavailableAreas);
	int                     GetVisFrameId();
	Vec3                    GetConnectionNormal(CVisArea* pPortal);
	void                    PreRender(int nReqursionLevel, CCamera CurCamera, CVisArea* pParent, CVisArea* pCurPortal, bool* pbOutdoorVisible, PodArray<CCamera>* plstOutPortCameras, bool* pbSkyVisible, bool* pbOceanVisible, PodArray<CVisArea*>& lstVisibleAreas, const SRenderingPassInfo& passInfo);
	void                    UpdatePortalCameraPlanes(CCamera& cam, Vec3* pVerts, bool bMergeFrustums, const SRenderingPassInfo& passInfo);
	int                     GetVisAreaConnections(IVisArea** pAreas, int nMaxConnNum, bool bSkipDisabledPortals = false);
	int                     GetRealConnections(IVisArea** pAreas, int nMaxConnNum, bool bSkipDisabledPortals = false);
	bool                    IsPortalValid();
	bool                    IsPortalIntersectAreaInValidWay(CVisArea* pPortal);
	bool                    IsPortal() const;
	bool                    IsShapeClockwise();
	bool                    IsAffectedByOutLights() const { return m_bAffectedByOutLights; }
	bool                    IsActive()                    { return m_bActive || (GetCVars()->e_Portals == 4); }
	void                    UpdateGeometryBBox();
	void                    UpdateClipVolume();
	void                    UpdatePortalBlendInfo(const SRenderingPassInfo& passInfo);
	void                    DrawAreaBoundsIntoCBuffer(class CCullBuffer* pCBuffer);
	void                    ClipPortalVerticesByCameraFrustum(PodArray<Vec3>* pPolygon, const CCamera& cam);
	void                    GetMemoryUsage(ICrySizer* pSizer);
	bool                    IsConnectedToOutdoor() const;
	bool                    IsIgnoringGI() const        { return m_bIgnoreGI; }
	bool                    IsIgnoringOutdoorAO() const { return m_bIgnoreOutdoorAO; }

	const char*             GetName()                   { return m_pVisAreaColdData->m_sName; }
#if ENGINE_ENABLE_COMPILATION
	int                     SaveHeader(byte*& pData, int& nDataSize);
	int                     SaveObjetsTree(byte*& pData, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo, byte* pHead);
	int                     GetData(byte*& pData, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	int                     GetSegmentData(byte*& pData, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
#endif
	template<class T>
	int                 LoadHeader_T(T*& f, int& nDataSizeLeft, EEndian eEndian, int& objBlockSize);
	template<class T>
	int                 LoadObjectsTree_T(T*& f, int& nDataSizeLeft, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, SHotUpdateInfo* pExportInfo, const int objBlockSize);
	template<class T>
	int                 Load_T(T*& f, int& nDataSize, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	int                 Load(byte*& f, int& nDataSizeLeft, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	int                 Load(FILE*& f, int& nDataSizeLeft, std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	const AABB*         GetAABBox() const;
	const AABB*         GetStaticObjectAABBox() const;
	void                UpdateOcclusionFlagInTerrain();
	void                AddConnectedAreas(PodArray<CVisArea*>& lstAreas, int nMaxRecursion);
	void                GetShapePoints(const Vec3*& pPoints, size_t& nPoints);
	float               GetHeight();
	float               CalcSignedArea();

	bool                CalcPortalBlendPlanes(Vec3 camPos);
	virtual void        GetClipVolumeMesh(_smart_ptr<IRenderMesh>& renderMesh, Matrix34& worldTM) const;
	virtual const AABB& GetClipVolumeBBox() const                       { return *GetStaticObjectAABBox(); }
	virtual uint8       GetStencilRef() const                           { return m_nStencilRef; }
	virtual uint        GetClipVolumeFlags() const;
	virtual bool        IsPointInsideClipVolume(const Vec3& vPos) const { return IsPointInsideVisArea(vPos); }

	void                OffsetPosition(const Vec3& delta);
	static VisAreaGUID  GetGUIDFromFile(byte* f, EEndian eEndian);
	VisAreaGUID         GetGUID() const { return m_nVisGUID; }

	const Vec3          GetFinalAmbientColor();

	static PodArray<CVisArea*>     m_lUnavailableAreas;
	static PodArray<Vec3>          s_tmpLstPortVertsClipped;
	static PodArray<Vec3>          s_tmpLstPortVertsSS;
	static PodArray<Vec3>          s_tmpPolygonA;
	static PodArray<IRenderNode*>  s_tmpLstLights;
	static PodArray<CTerrainNode*> s_tmpLstTerrainNodeResult;
	static CPolygonClipContext     s_tmpClipContext;
	static PodArray<CCamera>       s_tmpCameras;
	static int                     s_nGetDistanceThruVisAreasCallCounter;

	VisAreaGUID                    m_nVisGUID;
	PodArray<CVisArea*>            m_lstConnections;
	Vec3                           m_vConnNormals[2];
	int                            m_nRndFrameId;
	float                          m_fGetDistanceThruVisAreasMinDistance;
	int                            m_nGetDistanceThruVisAreasLastCallID;
	float                          m_fPortalBlending;

	PodArray<Vec3>                 m_lstShapePoints;
	float                          m_fHeight;

	_smart_ptr<IRenderMesh>        m_pClipVolumeMesh;

	Vec3                           m_vAmbientColor;
	float                          m_fDistance;
	float                          m_fViewDistRatio;
	CCamera*                       m_arrOcclCamera[MAX_RECURSION_LEVELS];
	int                            m_lstCurCamerasLen;
	int                            m_lstCurCamerasCap;
	int                            m_lstCurCamerasIdx;
	uint8                          m_nStencilRef;
	SGenericColdData*              m_pVisAreaColdData;
	bool                           m_bAffectedByOutLights;
	bool                           m_bSkyOnly;
	bool                           m_bOceanVisible;
	bool                           m_bDoubleSide;
	bool                           m_bUseDeepness;
	bool                           m_bUseInIndoors;
	bool                           m_bThisIsPortal;
	bool                           m_bIgnoreSky;
	bool                           m_bActive;
	bool                           m_bIgnoreGI;
	bool                           m_bIgnoreOutdoorAO;
};

struct SAABBTreeNode
{
	SAABBTreeNode(PodArray<CVisArea*>& lstAreas, AABB box, int nMaxRecursion = 0);
	~SAABBTreeNode();
	CVisArea*      FindVisarea(const Vec3& vPos);
	SAABBTreeNode* GetTopNode(const AABB& box, void** pNodeCache);
	bool           IntersectsVisAreas(const AABB& box);
	int            ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal);
	void           OffsetPosition(const Vec3& delta);

	AABB                nodeBox;
	PodArray<CVisArea*> nodeAreas;
	SAABBTreeNode*      arrChilds[2];
};

struct TTopologicalSorter;

struct CVisAreaSegmentData
{
	// active vis areas in current segment
	std::vector<int> m_visAreaIndices;
};

struct CVisAreaManager : public IVisAreaManager, Cry3DEngineBase
{
	CVisArea*                   m_pCurArea, * m_pCurPortal;
	PodArray<CVisArea*>         m_lstActiveEntransePortals;

	PodArray<CVisArea*>         m_lstVisAreas;
	PodArray<CVisArea*>         m_lstPortals;
	PodArray<CVisArea*>         m_lstOcclAreas;
	PodArray<CVisArea*>         m_segVisAreas;
	PodArray<CVisArea*>         m_segPortals;
	PodArray<CVisArea*>         m_segOcclAreas;
	PodArray<CVisArea*>         m_lstActiveOcclVolumes;
	PodArray<CVisArea*>         m_lstIndoorActiveOcclVolumes;
	PodArray<CVisArea*>         m_lstVisibleAreas;
	PodArray<CVisArea*>         m_tmpLstUnavailableAreas;
	PodArray<CVisArea*>         m_tmpLstLightBoxAreas;
	bool                        m_bOutdoorVisible;
	bool                        m_bSkyVisible;
	bool                        m_bOceanVisible;
	bool                        m_bSunIsNeeded;
	PodArray<CCamera>           m_lstOutdoorPortalCameras;
	PodArray<IVisAreaCallback*> m_lstCallbacks;
	SAABBTreeNode*              m_pAABBTree;

	CVisAreaManager();
	~CVisAreaManager();
	void                 UpdateAABBTree();
	void                 SetCurAreas(const SRenderingPassInfo& passInfo);
	PodArray<CVisArea*>* GetActiveEntransePortals() { return &m_lstActiveEntransePortals; }
	void                 PortalsDrawDebug();
	bool                 IsEntityVisible(IRenderNode* pEnt);
	bool                 IsOutdoorAreasVisible();
	bool                 IsSkyVisible();
	bool                 IsOceanVisible();
	CVisArea*            CreateVisArea(VisAreaGUID visGUID);
	bool                 DeleteVisArea(CVisArea* pVisArea);
	bool                 SetEntityArea(IRenderNode* pEnt, const AABB& objBox, const float fObjRadiusSqr);
	void                 CheckVis(const SRenderingPassInfo& passInfo);
	void                 DrawVisibleSectors(const SRenderingPassInfo& passInfo, uint32 passCullMask);
	void                 ActivatePortal(const Vec3& vPos, bool bActivate, const char* szEntityName);
	void                 ActivateOcclusionAreas(IVisAreaTestCallback* pTest, bool bActivate);
	void                 UpdateVisArea(CVisArea* pArea, const Vec3* pPoints, int nCount, const char* szName, const SVisAreaInfo& info);
	virtual void         UpdateConnections();
	void                 MoveObjectsIntoList(PodArray<SRNInfo>* plstVisAreasEntities, const AABB* boxArea, bool bRemoveObjects = false);
	IVisArea*            GetVisAreaFromPos(const Vec3& vPos);
	bool                 IntersectsVisAreas(const AABB& box, void** pNodeCache = 0);
	bool                 ClipOutsideVisAreas(Sphere& sphere, Vec3 const& vNormal, void* pNodeCache = 0);
	bool                 IsEntityVisAreaVisible(IRenderNode* pEnt, int nMaxReqursion, const SRenderLight* pLight, const SRenderingPassInfo& passInfo);
	void                 MakeActiveEntransePortalsList(const CCamera* pCamera, PodArray<CVisArea*>& lstActiveEntransePortals, CVisArea* pThisPortal, const SRenderingPassInfo& passInfo);
	void                 MergeCameras(CCamera& cam, const CCamera& camPlus, const SRenderingPassInfo& passInfo);
	void                 DrawOcclusionAreasIntoCBuffer(const SRenderingPassInfo& passInfo);
	bool                 IsValidVisAreaPointer(CVisArea* pVisArea);
	void                 GetStreamingStatus(int& nLoadedSectors, int& nTotalSectors);
	void                 GetMemoryUsage(ICrySizer* pSizer) const;
	bool                 IsOccludedByOcclVolumes(const AABB& objBox, const SRenderingPassInfo& passInfo, bool bCheckOnlyIndoorVolumes = false);
	void                 GetObjectsAround(Vec3 vExploPos, float fExploRadius, PodArray<SRNInfo>* pEntList, bool bSkip_ERF_NO_DECALNODE_DECALS = false, bool bSkipDynamicObjects = false);
	void                 IntersectWithBox(const AABB& aabbBox, PodArray<CVisArea*>* plstResult, bool bOnlyIfVisible);
	template<class T>
	bool                 Load_T(T*& f, int& nDataSize, struct SVisAreaManChunkHeader* pVisAreaManagerChunkHeader, std::vector<struct IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo);
	virtual bool         Load(FILE*& f, int& nDataSize, struct SVisAreaManChunkHeader* pVisAreaManagerChunkHeader, std::vector<struct IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable);
	virtual bool         SetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, bool bHotUpdate, SHotUpdateInfo* pExportInfo);
	virtual bool         GetCompiledData(byte* pData, int nDataSize, std::vector<struct IStatObj*>** ppStatObjTable, std::vector<IMaterial*>** ppMatTable, std::vector<struct IStatInstGroup*>** ppStatInstGroupTable, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	virtual int          GetCompiledDataSize(SHotUpdateInfo* pExportInfo);
	void                 UnregisterEngineObjectsInArea(const SHotUpdateInfo* pExportInfo, PodArray<IRenderNode*>& arrUnregisteredObjects, bool bOnlyEngineObjects);
	void                 PrecacheLevel(bool bPrecacheAllVisAreas, Vec3* pPrecachePoints, int nPrecachePointsNum);
	void                 AddLightSource(SRenderLight* pLight, const SRenderingPassInfo& passInfo);
	void                 AddLightSourceReqursive(SRenderLight* pLight, CVisArea* pArea, const int32 nDeepness, const SRenderingPassInfo& passInfo);
	bool                 IsEntityVisAreaVisibleReqursive(CVisArea* pVisArea, int nMaxReqursion, PodArray<CVisArea*>* pUnavailableAreas, const SRenderLight* pLight, const SRenderingPassInfo& passInfo);
	bool                 IsAABBVisibleFromPoint(AABB& aabb, Vec3 vPos);
	bool                 FindShortestPathToVisArea(CVisArea* pThisArea, CVisArea* pTargetArea, PodArray<CVisArea*>& arrVisitedAreas, int& nRecursion, const struct Shadowvolume& sv);

	int                  GetNumberOfVisArea() const;                            // the function give back the accumlated number of visareas and portals
	IVisArea*            GetVisAreaById(int nID) const;                         // give back the visarea interface based on the id (0..GetNumberOfVisArea()) it can be a visarea or a portal

	virtual void         AddListener(IVisAreaCallback* pListener);
	virtual void         RemoveListener(IVisAreaCallback* pListener);

	virtual void         CloneRegion(const AABB& region, const Vec3& offset, float zRotation);
	virtual void         ClearRegion(const AABB& region);

	void                 MarkAllSectorsAsUncompiled();
	void                 InitAABBTree();

	// -------------------------------------

	void         GetObjectsByType(PodArray<IRenderNode*>& lstObjects, EERType objType, const AABB* pBBox, bool* pInstStreamReady = NULL, uint64 dwFlags = ~0);
	void         GetObjectsByFlags(uint dwFlags, PodArray<IRenderNode*>& lstObjects);
	int          GetObjectsCount(EOcTeeNodeListType eListType);
	void         GetStreamedInNodesNum(int& nAllStreamable, int& nReady);

	void         GetNearestCubeProbe(float& fMinDistance, int& nMaxPriority, CLightEntity*& pNearestLight, const AABB* pBBox);

	void         GetObjects(PodArray<IRenderNode*>& lstObjects, const AABB* pBBox);
	CVisArea*    GetCurVisArea() { return m_pCurArea ? m_pCurArea : m_pCurPortal; }
	void         GenerateStatObjAndMatTables(std::vector<IStatObj*>* pStatObjTable, std::vector<IMaterial*>* pMatTable, std::vector<IStatInstGroup*>* pStatInstGroupTable, SHotUpdateInfo* pExportInfo);
	void         OnVisAreaDeleted(IVisArea* pArea);
	void         ActivateObjectsLayer(uint16 nLayerId, bool bActivate, bool bPhys, IGeneralMemoryHeap* pHeap, const AABB& layerBox);
	void         PhysicalizeInBox(const AABB&);
	void         DephysicalizeInBox(const AABB&);
	virtual void OffsetPosition(const Vec3& delta);
	void         CleanUpTrees();

private:
	void      DeleteAllVisAreas();

	CVisArea* CreateTypeVisArea();
	CVisArea* CreateTypePortal();
	CVisArea* CreateTypeOcclArea();

	CVisArea* FindVisAreaByGuid(VisAreaGUID guid, PodArray<CVisArea*>& lstVisAreas);
	template<class T>
	void      ResetVisAreaList(PodArray<CVisArea*>& lstVisAreas, PodArray<CVisArea*, ReservedVisAreaBytes>& visAreas, PodArray<T>& visAreaColdData);

	PodArray<CVisArea*, ReservedVisAreaBytes> m_portals;
	PodArray<CVisArea*, ReservedVisAreaBytes> m_visAreas;
	PodArray<CVisArea*, ReservedVisAreaBytes> m_occlAreas;

	PodArray<SGenericColdData>                m_visAreaColdData;
	PodArray<SPortalColdData>                 m_portalColdData;
	PodArray<SGenericColdData>                m_occlAreaColdData;

	PodArray<int>                             m_arrDeletedVisArea;
	PodArray<int>                             m_arrDeletedPortal;
	PodArray<int>                             m_arrDeletedOcclArea;

	struct SActiveVerts
	{
		Vec3 arrvActiveVerts[4];
	};

#if defined(OCCLUSIONCULLER_W)
	std::vector<SActiveVerts> m_allActiveVerts;
#endif
};

#endif // VisArea_H
