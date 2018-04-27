// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjman.h
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef CObjManager_H
#define CObjManager_H

#include "StatObj.h"
#include "../RenderDll/Common/Shadow_Renderer.h"
#include "terrain_sector.h"
#include <CryCore/StlUtils.h>
#include <CryMemory/PoolAllocator.h>
#include "CCullThread.h"

#include <map>
#include <vector>

#include "ObjManCullQueue.h"

#define ENTITY_MAX_DIST_FACTOR  100
#define MAX_VALID_OBJECT_VOLUME (10000000000.f)
#define DEFAULT_CGF_NAME        ("%ENGINE%\\EngineAssets\\Objects\\primitive_sphere.cgf")

struct CStatObj;
struct IIndoorBase;
struct IRenderNode;
struct ISystem;
struct IDecalRenderNode;
struct SCheckOcclusionJobData;
struct SCheckOcclusionOutput;

class CVegetation;

class C3DEngine;
struct IMaterial;

#define SMC_EXTEND_FRUSTUM              8
#define SMC_SHADOW_FRUSTUM_TEST         16

#define OCCL_TEST_HEIGHT_MAP            1
#define OCCL_TEST_CBUFFER               2
#define OCCL_TEST_INDOOR_OCCLUDERS_ONLY 4
#define POOL_STATOBJ_ALLOCS

//! contains stat obj instance group properties (vegetation object properties)
struct StatInstGroup : public IStatInstGroup
{
	StatInstGroup()
	{
		pStatObj = 0;
		ZeroStruct(m_arrSSpriteLightInfo);
		m_fSpriteSwitchDist = 0;
	}

	CStatObj* GetStatObj()
	{
		IStatObj* p = pStatObj;
		return (CStatObj*)p;
	}
	const CStatObj* GetStatObj() const
	{
		const IStatObj* p = pStatObj;
		return (const CStatObj*)p;
	}

	void  Update(struct CVars* pCVars, int nGeomDetailScreenRes);
	void  GetMemoryUsage(ICrySizer* pSizer) const {}
	float GetAlignToTerrainAmount() const;

	SVegetationSpriteLightInfo m_arrSSpriteLightInfo[FAR_TEX_COUNT];

	float                      m_fSpriteSwitchDist;
};

struct SExportedBrushMaterial
{
	int  size;
	char material[64];
};

struct SRenderMeshInfoOutput
{
	SRenderMeshInfoOutput() { memset(this, 0, sizeof(*this)); }
	_smart_ptr<IRenderMesh> pMesh;
	IMaterial*              pMat;
};

// Inplace object for IStreamable* to cache StreamableMemoryContentSize
struct SStreamAbleObject
{
	explicit SStreamAbleObject(IStreamable* pObj, bool bUpdateMemUsage = true) : m_pObj(pObj), fCurImportance(-1000.f)
	{
		if (pObj && bUpdateMemUsage)
			m_nStreamableContentMemoryUsage = pObj->GetStreamableContentMemoryUsage();
		else
			m_nStreamableContentMemoryUsage = 0;
	}

	bool operator==(const SStreamAbleObject& rOther) const
	{
		return m_pObj == rOther.m_pObj;
	}

	int          GetStreamableContentMemoryUsage() const { return m_nStreamableContentMemoryUsage; }
	IStreamable* GetStreamAbleObject() const             { return m_pObj; }
	uint32       GetLastDrawMainFrameId() const
	{
		return m_pObj->GetLastDrawMainFrameId();
	}
	float        fCurImportance;
private:
	IStreamable* m_pObj;
	int          m_nStreamableContentMemoryUsage;

};

struct SObjManPrecacheCamera
{
	SObjManPrecacheCamera()
		: vPosition(ZERO)
		, vDirection(ZERO)
		, bbox(AABB::RESET)
		, fImportanceFactor(1.0f)
	{
	}

	Vec3  vPosition;
	Vec3  vDirection;
	AABB  bbox;
	float fImportanceFactor;
};

struct SObjManPrecachePoint
{
	SObjManPrecachePoint()
		: nId(0)
	{
	}

	int        nId;
	CTimeValue expireTime;
};

struct SObjManRenderDebugInfo
{
	SObjManRenderDebugInfo()
		: pEnt(nullptr)
		, fEntDistance(0.0f)
	{}

	SObjManRenderDebugInfo(IRenderNode* _pEnt, float _fEntDistance)
		: pEnt(_pEnt)
		, fEntDistance(_fEntDistance)
	{}

	IRenderNode* pEnt;
	float        fEntDistance;
};

//////////////////////////////////////////////////////////////////////////
class CObjManager : public Cry3DEngineBase
{
public:
	enum
	{
		MaxPrecachePoints = 4,
	};

public:
	CObjManager();
	~CObjManager();

	void      PreloadLevelObjects();
	void      UnloadObjects(bool bDeleteAll);
	void      UnloadVegetationModels(bool bDeleteAll);
	void      UnloadFarObjects();

	void      DrawFarObjects(float fMaxViewDist, const SRenderingPassInfo& passInfo);
	void      GenerateFarObjects(float fMaxViewDist, const SRenderingPassInfo& passInfo);
	void      RenderFarObjects(const SRenderingPassInfo& passInfo);
	void      CheckTextureReadyFlag();

	CStatObj* AllocateStatObj();
	void      FreeStatObj(CStatObj* pObj);

	template<class T>
	static int GetItemId(std::vector<T*>* pArray, T* pItem, bool bAssertIfNotFound = true)
	{
		for (uint32 i = 0, end = pArray->size(); i < end; ++i)
			if ((*pArray)[i] == pItem)
				return i;

		//    if(bAssertIfNotFound)
		//    assert(!"Item not found");

		return -1;
	}

	template<class T>
	static T* GetItemPtr(std::vector<T*>* pArray, int nId)
	{
		if (nId < 0)
			return NULL;

		assert(nId < (int)pArray->size());

		if (nId < (int)pArray->size())
			return (*pArray)[nId];
		else
			return NULL;
	}

	CStatObj* LoadStatObj(const char* szFileName, const char* szGeomName = NULL, IStatObj::SSubObject** ppSubObject = NULL, bool bUseStreaming = true, unsigned long nLoadingFlags = 0,
	                      const void* m_pData = 0, int m_nDataSize = 0, const char* szBlockName = NULL);
	void      GetLoadedStatObjArray(IStatObj** pObjectsArray, int& nCount);

	// Deletes object.
	// Only should be called by Release function of CStatObj.
	bool InternalDeleteObject(CStatObj* pObject);

	PodArray<StatInstGroup> m_lstStaticTypes;

	CThreadSafeRendererContainer<SVegetationSpriteInfo> m_arrVegetationSprites[MAX_RECURSION_LEVELS][nThreadsNum];

	void MakeShadowCastersList(CVisArea* pReceiverArea, const AABB& aabbReceiver,
	                           int dwAllowedTypes, int32 nRenderNodeFlags, Vec3 vLightPos, SRenderLight* pLight, ShadowMapFrustum* pFr, PodArray<struct SPlaneObject>* pShadowHull, const SRenderingPassInfo& passInfo);

	int MakeStaticShadowCastersList(IRenderNode* pIgnoreNode, ShadowMapFrustum* pFrustum, const PodArray<struct SPlaneObject>* pShadowHull, int renderNodeExcludeFlags, int nMaxNodes, const SRenderingPassInfo& passInfo);

	// decal pre-caching
	typedef std::vector<IDecalRenderNode*> DecalsToPrecreate;
	DecalsToPrecreate m_decalsToPrecreate;

	void                    PrecacheStatObjMaterial(IMaterial* pMaterial, const float fEntDistance, IStatObj* pStatObj, bool bFullUpdate, bool bDrawNear);

	void                    PrecacheStatObj(CStatObj* pStatObj, int nLod, const Matrix34A& statObjMatrix, IMaterial* pMaterial, float fImportance, float fEntDistance, bool bFullUpdate, bool bHighPriority);

	NCullQueue::SCullQueue& CullQueue() { return m_cullQueue; }

	//////////////////////////////////////////////////////////////////////////

	typedef std::map<string, CStatObj*, stl::less_stricmp<string>> ObjectsMap;
	ObjectsMap m_nameToObjectMap;

	typedef std::set<CStatObj*> LoadedObjects;
	LoadedObjects m_lstLoadedObjects;

protected:
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( push )               //AMD Port
	#pragma warning( disable : 4267 )
#endif

public:
	int GetLoadedObjectCount() { return m_lstLoadedObjects.size(); }

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#pragma warning( pop )                //AMD Port
#endif

	uint16 CheckCachedNearestCubeProbe(IRenderNode* pEnt, Vec4* pEnvProbMults = nullptr)
	{
		if (const auto tempDataPtr = pEnt->m_pTempData.load())
		{
			SRenderNodeTempData::SUserData& pUserDataRN = tempDataPtr->userData;

			const uint16 nCacheClearThreshold = 32;
			++pUserDataRN.nCubeMapIdCacheClearCounter;
			pUserDataRN.nCubeMapIdCacheClearCounter &= (nCacheClearThreshold - 1);

			if (pUserDataRN.nCubeMapId && pUserDataRN.nCubeMapIdCacheClearCounter)
			{
				if (pEnvProbMults)
					*pEnvProbMults = pUserDataRN.vEnvironmentProbeMults;

				return pUserDataRN.nCubeMapId;
			}
		}

		// cache miss
		return 0;
	}

	int16 GetNearestCubeProbe(PodArray<SRenderLight*>* pAffectingLights, IVisArea* pVisArea, const AABB& objBox, bool bSpecular = true, Vec4* pEnvProbeMults = nullptr);

	void  RenderObject(IRenderNode* o,
	                   PodArray<SRenderLight*>* pAffectingLights,
	                   const Vec3& vAmbColor,
	                   const AABB& objBox,
	                   float fEntDistance,
	                   EERType eERType,
	                   const SRenderingPassInfo& passInfo,
	                   uint32 passCullMask);

	void RenderVegetation(class CVegetation* pEnt, PodArray<SRenderLight*>* pAffectingLights,
													const AABB &objBox, float fEntDistance,
													SSectorTextureSet * pTerrainTexInfo, bool nCheckOcclusion, const SRenderingPassInfo &passInfo, uint32 passCullMask);
	void RenderBrush(class CBrush* pEnt, PodArray<SRenderLight*>* pAffectingLights,
										 SSectorTextureSet * pTerrainTexInfo,
										 const AABB &objBox, float fEntDistance,
										 bool nCheckOcclusion, const SRenderingPassInfo &passInfo, uint32 passCullMask);

	int  ComputeDissolve(const CLodValue& lodValueIn, SRenderNodeTempData* pTempData, IRenderNode* pEnt, float fEntDistance, CLodValue arrlodValuesOut[2]);

	void RenderDecalAndRoad(IRenderNode* pEnt, PodArray<SRenderLight*>* pAffectingLights,
	                        const Vec3& vAmbColor, const AABB& objBox, float fEntDistance,
	                        bool nCheckOcclusion, const SRenderingPassInfo& passInfo);

	void      RenderObjectDebugInfo(IRenderNode* pEnt, float fEntDistance, const SRenderingPassInfo& passInfo);
	void      RenderAllObjectDebugInfo();
	void      RenderObjectDebugInfo_Impl(IRenderNode* pEnt, float fEntDistance);
	void      RemoveFromRenderAllObjectDebugInfo(IRenderNode* pEnt);

	float     GetXYRadius(int nType);
	bool      GetStaticObjectBBox(int nType, Vec3& vBoxMin, Vec3& vBoxMax);

	IStatObj* GetStaticObjectByTypeID(int nTypeID);
	IStatObj* FindStaticObjectByFilename(const char* filename);

	float     GetBendingRandomFactor();

	bool      IsBoxOccluded(const AABB& objBox,
	                        float fDistance,
	                        OcclusionTestClient* const __restrict pOcclTestVars,
	                        bool bIndoorOccludersOnly,
	                        EOcclusionObjectType eOcclusionObjectType,
	                        const SRenderingPassInfo& passInfo);

	// tmp containers (replacement for local static vars)

	//  void DrawObjSpritesSorted(PodArray<CVegetation*> *pList, float fMaxViewDist, int useBending);
	//	void ProcessActiveShadowReceiving(IRenderNode * pEnt, float fEntDistance, SRenderLight * pLight, bool bFocusOnHead);

	//	void SetupEntityShadowMapping( IRenderNode * pEnt, SRendParams * pDrawParams, float fEntDistance, SRenderLight * pLight );
	//////////////////////////////////////////////////////////////////////////

	void RegisterForStreaming(IStreamable* pObj);
	void UnregisterForStreaming(IStreamable* pObj);
	void UpdateRenderNodeStreamingPriority(IRenderNode* pObj, float fEntDistance, float fImportanceFactor, bool bFullUpdate, const SRenderingPassInfo& passInfo, bool bHighPriority = false);

	void GetMemoryUsage(class ICrySizer* pSizer) const;
	void GetBandwidthStats(float* fBandwidthRequested);

	//  PodArray<class CBrush*> m_lstBrushContainer;
	//  PodArray<class CVegetation*> m_lstVegetContainer;
	void       LoadBrushes();
	//  void MergeBrushes();
	void       ReregisterEntitiesInArea(AABB* pBox, bool bCleanUpTree = false);
	//	void ProcessEntityParticles(IRenderNode * pEnt, float fEntDistance);
	void       UpdateObjectsStreamingPriority(bool bSyncLoad, const SRenderingPassInfo& passInfo);
	ILINE void SetCurrentTime(float fCurrentTime) { m_fCurrTime = fCurrentTime; }
	void       ProcessObjectsStreaming(const SRenderingPassInfo& passInfo);

	// implementation parts of ProcessObjectsStreaming
	void ProcessObjectsStreaming_Impl(bool bSyncLoad, const SRenderingPassInfo& passInfo);
	void ProcessObjectsStreaming_Sort(bool bSyncLoad, const SRenderingPassInfo& passInfo);
	void ProcessObjectsStreaming_Release();
	void ProcessObjectsStreaming_InitLoad(bool bSyncLoad);
	void ProcessObjectsStreaming_Finish();

#ifdef OBJMAN_STREAM_STATS
	void ProcessObjectsStreaming_Stats(const SRenderingPassInfo& passInfo);
#endif

	// time counters

	static bool IsAfterWater(const Vec3& vPos, const Vec3& vCamPos, const SRenderingPassInfo& passInfo, float fUserWaterLevel = WATER_LEVEL_UNKNOWN);

	void        GetObjectsStreamingStatus(I3DEngine::SObjectsStreamingStatus& outStatus);
	//	bool ProcessShadowMapCasting(IRenderNode * pEnt, SRenderLight * pLight);

	//	bool IsSphereAffectedByShadow(IRenderNode * pCaster, IRenderNode * pReceiver, SRenderLight * pLight);
	//	void MakeShadowCastersListInArea(CBasicArea * pArea, const AABB & boxReceiver,
	//		int dwAllowedTypes, Vec3 vLightPos, SRenderLight * pLight, ShadowMapFrustum * pFr, PodArray<struct SPlaneObject> * pShadowHull );
	//	void DrawEntityShadowFrustums(IRenderNode * pEnt);

	void FreeNotUsedCGFs();

	//	void RenderObjectVegetationNonCastersNoFogVolume( IRenderNode * pEnt,uint32 nDLightMask,
	//	const CCamera & EntViewCamera,
	//bool bAllInside, float fMaxViewDist, IRenderNodeInfo * pEntInfo);
	//	void InitEntityShadowMapInfoStructure(IRenderNode * pEnt);
	//	float CalculateEntityShadowVolumeExtent(IRenderNode * pEntity, SRenderLight * pLight);
	//	void MakeShadowBBox(Vec3 & vBoxMin, Vec3 & vBoxMax, const Vec3 & vLightPos, float fLightRadius, float fShadowVolumeExtent);
	void MakeUnitCube();

	void BoxCastingShadow_HWOcclQuery(const AABB& objBox, const Vec3& rSunDir, OcclusionTestClient* const pOcclTestVars)
	{
#ifdef USE_CULL_QUEUE
		if (GetCVars()->e_CoverageBuffer)
		{
			const uint32 mainFrameID = passInfo.GetMainFrameID();
			CullQueue().AddItem(objBox, rSunDir, pOcclTestVars, mainFrameID);
		}
#endif
	}
	bool IsBoxOccluded_HeightMap(const AABB& objBox, float fDistance, EOcclusionObjectType eOcclusionObjectType, OcclusionTestClient* pOcclTestVars, const SRenderingPassInfo& passInfo);

	//////////////////////////////////////////////////////////////////////////
	// CheckOcclusion functionality
	bool CheckOcclusion_TestAABB(const AABB& rAABB, float fEntDistance);
	bool CheckOcclusion_TestQuad(const Vec3& vCenter, const Vec3& vAxisX, const Vec3& vAxisY);

	void PushIntoCullQueue(const SCheckOcclusionJobData& rCheckOcclusionData);
	void PopFromCullQueue(SCheckOcclusionJobData* pCheckOcclusionData);

	void PushIntoCullOutputQueue(const SCheckOcclusionOutput& rCheckOcclusionOutput);
	bool PopFromCullOutputQueue(SCheckOcclusionOutput* pCheckOcclusionOutput);

	void BeginCulling();
	void RemoveCullJobProducer();
	void AddCullJobProducer();

#ifndef _RELEASE
	void CoverageBufferDebugDraw();
#endif

	bool LoadOcclusionMesh(const char* pFileName);

	//////////////////////////////////////////////////////////////////////////
	// Garbage collection for parent stat objects.
	// Returns number of deleted objects
	void        ClearStatObjGarbage();
	void        CheckForGarbage(CStatObj* pObject);
	void        UnregisterForGarbage(CStatObj* pObject);

	static int  GetObjectLOD(const IRenderNode* pObj, float fDistance);
	static bool RayStatObjIntersection(IStatObj* pStatObj, const Matrix34& objMat, IMaterial* pMat,
	                                   Vec3 vStart, Vec3 vEnd, Vec3& vClosestHitPoint, float& fClosestHitDistance, bool bFastTest);
	static bool RayRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const Vec3& vInDir, Vec3& vOutPos, Vec3& vOutNormal, bool bFastTest, IMaterial* pMat);
	static bool SphereRenderMeshIntersection(IRenderMesh* pRenderMesh, const Vec3& vInPos, const float fRadius, IMaterial* pMat);
	static void FillTerrainTexInfo(IOctreeNode* pOcNode, float fEntDistance, struct SSectorTextureSet*& pTerrainTexInfo, const AABB& objBox);
	PodArray<CVisArea*> m_tmpAreas0, m_tmpAreas1;

	void         CleanStreamingData();
	IRenderMesh* GetRenderMeshBox();

	void         PrepareCullbufferAsync(const CCamera& rCamera);
	void         BeginOcclusionCulling(const SRenderingPassInfo& passInfo);
	void         EndOcclusionCulling();
	void         RenderNonJobObjects(const SRenderingPassInfo& passInfo);
	uint32       GetResourcesModificationChecksum(IRenderNode* pOwnerNode) const;
	bool         AddOrCreatePersistentRenderObject(SRenderNodeTempData* pTempData, CRenderObject*& pRenderObject, const CLodValue* pLodValue, const Matrix34& transformationMatrix, const SRenderingPassInfo& passInfo) const;
	IRenderMesh* GetBillboardRenderMesh(IMaterial* pMaterial);

public:
	//////////////////////////////////////////////////////////////////////////
	// Public Member variables.
	//////////////////////////////////////////////////////////////////////////

	static int                      m_nUpdateStreamingPrioriryRoundId;
	static int                      m_nUpdateStreamingPrioriryRoundIdFast;
	static int                      s_nLastStreamingMemoryUsage; //For streaming tools in editor

	Vec3                            m_vSkyColor;
	Vec3                            m_vSunColor;
	float                           m_fSunSkyRel; //relation factor of sun sky, 1->sun has full part of brightness, 0->sky has full part
	float                           m_fILMul;
	float                           m_fSkyBrightMul;
	float                           m_fSSAOAmount;
	float                           m_fSSAOContrast;
	float                           m_fGIAmount;
	SRainParams                     m_rainParams;
	SSnowParams                     m_snowParams;

	int                             m_bLockCGFResources;

	float                           m_fMaxViewDistanceScale;
	float                           m_fGSMMaxDistance;

	_smart_ptr<CStatObj>            m_pDefaultCGF;
	PodArray<SStreamAbleObject>     m_arrStreamableObjects;
	PodArray<COctreeNode*>          m_arrStreamingNodeStack;
	PodArray<SObjManPrecachePoint>  m_vStreamPreCachePointDefs;
	PodArray<SObjManPrecacheCamera> m_vStreamPreCacheCameras;
	int                             m_nNextPrecachePointId;
	bool                            m_bCameraPrecacheOverridden;
	NAsyncCull::CCullThread         m_CullThread;

private:
	//////////////////////////////////////////////////////////////////////////
	// Private Member variables.
	//////////////////////////////////////////////////////////////////////////

	PodArray<IStreamable*>  m_arrStreamableToRelease;
	PodArray<IStreamable*>  m_arrStreamableToLoad;
	PodArray<IStreamable*>  m_arrStreamableToDelete;
	bool                    m_bNeedProcessObjectsStreaming_Finish;

	float                   m_fCurrTime;

	_smart_ptr<IRenderMesh> m_pRMBox;
	_smart_ptr<IRenderMesh> m_pBillboardMesh;

	//////////////////////////////////////////////////////////////////////////
	std::vector<_smart_ptr<IStatObj>> m_lockedObjects;

	//////////////////////////////////////////////////////////////////////////
	CryMT::vector<CStatObj*> m_checkForGarbage;
	bool                     m_bGarbageCollectionEnabled;

	NCullQueue::SCullQueue   m_cullQueue;

	PodArray<CTerrainNode*>  m_lstTmpCastingNodes;

#ifdef POOL_STATOBJ_ALLOCS
	stl::PoolAllocator<sizeof(CStatObj), stl::PSyncMultiThread, alignof(CStatObj)>* m_statObjPool;
#endif

	CThreadSafeRendererContainer<SObjManRenderDebugInfo>             m_arrRenderDebugInfo;

	CryMT::SingleProducerSingleConsumerQueue<SCheckOcclusionJobData> m_CheckOcclusionQueue;
	CryMT::N_ProducerSingleConsumerQueue<SCheckOcclusionOutput>      m_CheckOcclusionOutputQueue;
};

#endif // CObjManager_H
