// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#if !defined(SHADOWRENDERER_H)
#define SHADOWRENDERER_H

#include "ShadowUtils.h"
#include <CryCore/Containers/VectorSet.h>
#include <array>

#define OMNI_SIDES_NUM 6

// data used to compute a custom shadow frustum for near shadows
struct CustomShadowMapFrustumData
{
	AABB aabb;
};

struct ShadowMapFrustum : public CMultiThreadRefCount
{
	enum eFrustumType // NOTE: Be careful when modifying the enum as it is used for sorting frustums in SCompareByLightIds
	{
		e_GsmDynamic         = 0,
		e_GsmDynamicDistance = 1,
		e_GsmCached          = 2,
		e_HeightMapAO        = 3,
		e_Nearest            = 4,
		e_PerObject          = 5,

		e_NumTypes
	};

	struct ShadowCacheData
	{
		enum eUpdateStrategy
		{
			eFullUpdate,
			eFullUpdateTimesliced,
			eIncrementalUpdate
		};

		ShadowCacheData() { Reset(); }

		void Reset()
		{
			memset(mOctreePath, 0x0, sizeof(mOctreePath));
			memset(mOctreePathNodeProcessed, 0x0, sizeof(mOctreePathNodeProcessed));
			mProcessedCasters.clear();
			mProcessedTerrainCasters.clear();
		}

		static const int                 MAX_TRAVERSAL_PATH_LENGTH = 32;
		uint8                            mOctreePath[MAX_TRAVERSAL_PATH_LENGTH];
		uint8                            mOctreePathNodeProcessed[MAX_TRAVERSAL_PATH_LENGTH];

		VectorSet<struct IShadowCaster*> mProcessedCasters;
		VectorSet<uint64>                mProcessedTerrainCasters;
	};

public:
	eFrustumType m_eFrustumType; // NOTE: adjust constructor if you add any variable before this

	Matrix44A    mLightProjMatrix;
	Matrix44A    mLightViewMatrix;
	Vec4         vFrustInfo;

	// flags
	bool bUseAdditiveBlending;
	bool bIncrementalUpdate;

	// if set to true - castersList contains all casters in light radius
	// and all other members related only to single frustum projection case are undefined
	bool   bOmniDirectionalShadow;
	uint8  nOmniFrustumMask;
	uint8  nInvalidatedFrustMask[MAX_GPU_NUM]; //for each GPU
	bool   bBlendFrustum;
	float  fBlendVal;

	uint32 nShadowGenMask;
	bool   bIsMGPUCopy;

	bool   bHWPCFCompare;
	bool   bUseHWShadowMap;

	bool   bNormalizedDepth;

	uint8  nShadowPoolUpdateRate;

	//sampling parameters
	f32 fWidthS, fWidthT;
	f32 fBlurS, fBlurT;

	//fading distance per light source
	float       fShadowFadingDist;

	ETEX_Format m_eReqTF;
	ETEX_Type   m_eReqTT;

	//texture in pool
	bool                     bUseShadowsPool;
	struct ShadowMapFrustum* pPrevFrustum;
	struct ShadowMapFrustum* pFrustumOwner;
	class CTexture*          pDepthTex;

	//3d engine parameters
	float fFOV;
	float fNearDist;
	float fFarDist;
	float fRendNear;
	//float fRendFar;
	int   nTexSize;

	//shadow renderer parameters - should be in separate structure
	//atlas parameters
	int  nTextureWidth;
	int  nTextureHeight;
	bool bUnwrapedOmniDirectional;
	int  nShadowMapSize;

	//packer params
	uint                            nPackID[OMNI_SIDES_NUM];
	int                             packX[OMNI_SIDES_NUM];
	int                             packY[OMNI_SIDES_NUM];
	int                             packWidth[OMNI_SIDES_NUM];
	int                             packHeight[OMNI_SIDES_NUM];

	int                             nResetID;
	float                           fFrustrumSize;
	float                           fProjRatio;
	float                           fDepthTestBias;
	float                           fDepthConstBias;
	float                           fDepthSlopeBias;
	float                           fDepthBiasClamp;

	PodArray<struct IShadowCaster*> castersList;
	PodArray<struct IShadowCaster*> jobExecutedCastersList;

	CCamera                         FrustumPlanes[OMNI_SIDES_NUM];
	uint32                          nShadowGenID[RT_COMMAND_BUF_COUNT][OMNI_SIDES_NUM];
	AABB                            aabbCasters;      //casters bbox in world space
	Vec3                            vLightSrcRelPos;  // relative world space
	Vec3                            vProjTranslation; // dst position
	float                           fRadius;
	int                             nDLightId;
	int                             nUpdateFrameId;
	IRenderNode*                    pLightOwner;
	uint32                          uCastersListCheckSum;
	int                             nShadowMapLod; // currently use as GSMLod, can be used as cubemap side, -1 means this variable is not used

	uint32                          m_Flags;

	// Render view that is used to accumulate items for this frustum.
	std::shared_ptr<ShadowCacheData>                          pShadowCacheData;

public:
	ShadowMapFrustum()
		: m_eFrustumType(e_GsmDynamic)
		, mLightProjMatrix(IDENTITY)
		, mLightViewMatrix(IDENTITY)
		, vFrustInfo(ZERO)
		, bUseAdditiveBlending(false)
		, bIncrementalUpdate(false)
		, bOmniDirectionalShadow(false)
		, nOmniFrustumMask(0)
		, bBlendFrustum(false)
		, fBlendVal(0)
		, nShadowGenMask(0)
		, bIsMGPUCopy(false)
		, bHWPCFCompare(false)
		, bUseHWShadowMap(false)
		, bNormalizedDepth(false)
		, nShadowPoolUpdateRate(0)
		, fWidthS(0)
		, fWidthT(0)
		, fBlurS(0)
		, fBlurT(0)
		, fShadowFadingDist(0)
		, m_eReqTF(eTF_Unknown)
		, m_eReqTT(eTT_2D)
		, bUseShadowsPool(false)
		, pPrevFrustum(nullptr)
		, pFrustumOwner(nullptr)
		, pDepthTex(nullptr)
		, fFOV(0)
		, fNearDist(0)
		, fFarDist(0)
		, fRendNear(0)
		, nTexSize(0)
		, nTextureWidth(0)
		, nTextureHeight(0)
		, bUnwrapedOmniDirectional(false)
		, nShadowMapSize(0)
		, nResetID(0)
		, fFrustrumSize(0)
		, fProjRatio(1.0f)
		, fDepthTestBias(0)
		, fDepthConstBias(0)
		, fDepthSlopeBias(0)
		, fDepthBiasClamp(0.001f)
		, aabbCasters(AABB::RESET)
		, vLightSrcRelPos(ZERO)
		, vProjTranslation(-1000.0f, -1000.0f, -1000.0f)
		, fRadius(0)
		, nDLightId(-1)
		, nUpdateFrameId(-1000)
		, pLightOwner(nullptr)
		, uCastersListCheckSum(0)
		, nShadowMapLod(0)
		, m_Flags(0)
	{
		ZeroArray(nInvalidatedFrustMask);
		ZeroArray(nPackID);
		ZeroArray(packX);
		ZeroArray(packY);
		ZeroArray(packWidth);
		ZeroArray(packHeight);
		ZeroArray(nShadowGenID);
	}

	void GetSideViewport(int nSide, int* pViewport) const
	{
		if (bUseShadowsPool)
		{
			pViewport[0] = packX[nSide];
			pViewport[1] = packY[nSide];
			pViewport[2] = packWidth[nSide];
			pViewport[3] = packHeight[nSide];
		}
		else
		{
			//simplest cubemap 6 faces unwrap
			pViewport[0] = nShadowMapSize * (nSide % 3);
			pViewport[1] = nShadowMapSize * (nSide / 3);
			pViewport[2] = nShadowMapSize;
			pViewport[3] = nShadowMapSize;
		}
	}

	void GetTexOffset(int nSide, float* pOffset, float* pScale, int nShadowsPoolSizeX, int nShadowsPoolSizeY) const
	{
		if (bUseShadowsPool)
		{
			pScale[0] = float(nShadowMapSize) / nShadowsPoolSizeX; //SHADOWS_POOL_SZ 1024
			pScale[1] = float(nShadowMapSize) / nShadowsPoolSizeY;
			pOffset[0] = float(packX[nSide]) / nShadowsPoolSizeX;
			pOffset[1] = float(packY[nSide]) / nShadowsPoolSizeY;
		}
		else
		{
			pOffset[0] = 1.0f / 3.0f * (nSide % 3);
			pOffset[1] = 1.0f / 2.0f * (nSide / 3);
			pScale[0] = 1.0f / 3.0f;
			pScale[1] = 1.0f / 2.0f;
		}
	}

	void RequestUpdate()
	{
		for (int i = 0; i < MAX_GPU_NUM; i++)
			nInvalidatedFrustMask[i] = 0x3F;
	}

	bool isUpdateRequested(int nMaskNum)
	{
		assert(nMaskNum >= 0 && nMaskNum < MAX_GPU_NUM);
		/*if (nMaskNum==-1) //request from 3dengine
		   {
		   for (int i=0; i<MAX_GPU_NUM; i++)
		   {
		    if(nInvalidatedFrustMask[i]>0)
		      return true;
		   }
		   return false;
		   }*/

		return (nInvalidatedFrustMask[nMaskNum] > 0);
	}

	bool IsCached() const
	{
		return m_eFrustumType == e_GsmCached || m_eFrustumType == e_HeightMapAO;
	}

	ILINE bool IntersectAABB(const AABB& bbox, bool* pAllIn) const
	{
		if (bOmniDirectionalShadow)
		{
			return bbox.IsOverlapSphereBounds(vLightSrcRelPos + vProjTranslation, fFarDist);
		}
		else
		{
			bool bDummy = false;
			if (bBlendFrustum)
			{
				if (FrustumPlanes[1].IsAABBVisible_EH(bbox, pAllIn) > 0)
					return true;
			}

			return FrustumPlanes[0].IsAABBVisible_EH(bbox, bBlendFrustum ? &bDummy : pAllIn) > 0;
		}
	}

	ILINE bool IntersectSphere(const Sphere& sp, bool* pAllIn)
	{
		if (bOmniDirectionalShadow)
			return Distance::Point_PointSq(sp.center, vLightSrcRelPos + vProjTranslation) < sqr(fFarDist + sp.radius);
		else
		{
			uint8 res = 0;
			if (bBlendFrustum)
			{
				res = FrustumPlanes[1].IsSphereVisible_FH(sp);
				*pAllIn = (res == CULL_INCLUSION);
				if (res != CULL_EXCLUSION)
					return true;
			}
			res = FrustumPlanes[0].IsSphereVisible_FH(sp);
			*pAllIn = bBlendFrustum ? false : (res == CULL_INCLUSION);
			return res != CULL_EXCLUSION;
		}
	}

	void UnProject(float sx, float sy, float sz, float* px, float* py, float* pz, IRenderer* pRend)
	{
		const int shadowViewport[4] = { 0, 0, 1, 1 };

		Matrix44A mIden;
		mIden.SetIdentity();

		//FIX remove float arrays
		pRend->UnProject(sx, sy, sz,
		                 px, py, pz,
		                 (float*)&mLightViewMatrix,
		                 (float*)&mIden,
		                 shadowViewport);
	}

	Vec3& UnProjectVertex3d(int sx, int sy, int sz, Vec3& vert, IRenderer* pRend)
	{
		float px;
		float py;
		float pz;
		UnProject((float)sx, (float)sy, (float)sz, &px, &py, &pz, pRend);
		vert.x = (float)px;
		vert.y = (float)py;
		vert.z = (float)pz;

		//		pRend->DrawBall(vert,10);

		return vert;
	}

	void UpdateOmniFrustums()
	{
		const float sCubeVector[6][7] =
		{
			{ 1,  0,  0,  0, 0, 1,  -90 }, //posx
			{ -1, 0,  0,  0, 0, 1,  90  }, //negx
			{ 0,  1,  0,  0, 0, -1, 0   }, //posy
			{ 0,  -1, 0,  0, 0, 1,  0   }, //negy
			{ 0,  0,  1,  0, 1, 0,  0   }, //posz
			{ 0,  0,  -1, 0, 1, 0,  0   }, //negz
		};

		const Vec3 vPos = vLightSrcRelPos + vProjTranslation;
		for (int nS = 0; nS < OMNI_SIDES_NUM; ++nS)
		{
			const Vec3 vForward = Vec3(sCubeVector[nS][0], sCubeVector[nS][1], sCubeVector[nS][2]);
			const Vec3 vUp = Vec3(sCubeVector[nS][3], sCubeVector[nS][4], sCubeVector[nS][5]);
			const Matrix33 matRot = Matrix33::CreateOrientation(vForward, vUp, DEG2RAD(sCubeVector[nS][6]));
			const float fFOV = bUnwrapedOmniDirectional ? (float)DEG2RAD_R(g_fOmniShadowFov) : (float)DEG2RAD_R(90.0f);

			FrustumPlanes[nS].SetMatrix(Matrix34(matRot, vPos));
			FrustumPlanes[nS].SetFrustum(nTexSize, nTexSize, fFOV, fNearDist, fFarDist);
		}
	}

	void DrawFrustum(IRenderer* pRend, int nFrames = 1)
	{
		if (abs(nUpdateFrameId - pRend->GetFrameID()) > nFrames)
			return;

		//if(!arrLightViewMatrix[0] && !arrLightViewMatrix[5] && !arrLightViewMatrix[10])
		//return;

		IRenderAuxGeom* pRendAux = pRend->GetIRenderAuxGeom();
		const ColorF cCascadeColors[] = { Col_Red, Col_Green, Col_Blue, Col_Yellow, Col_Magenta, Col_Cyan, Col_Black, Col_White };
		const uint nColorCount = CRY_ARRAY_COUNT(cCascadeColors);

		Vec3 vert1, vert2;
		ColorB c0 = cCascadeColors[nShadowMapLod % nColorCount];
		{
			pRendAux->DrawLine(
			  UnProjectVertex3d(0, 0, 0, vert1, pRend), c0,
			  UnProjectVertex3d(0, 0, 1, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(1, 0, 0, vert1, pRend), c0,
			  UnProjectVertex3d(1, 0, 1, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(1, 1, 0, vert1, pRend), c0,
			  UnProjectVertex3d(1, 1, 1, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(0, 1, 0, vert1, pRend), c0,
			  UnProjectVertex3d(0, 1, 1, vert2, pRend), c0);
		}

		for (int i = 0; i <= 1; i++)
		{
			pRendAux->DrawLine(
			  UnProjectVertex3d(0, 0, i, vert1, pRend), c0,
			  UnProjectVertex3d(1, 0, i, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(1, 0, i, vert1, pRend), c0,
			  UnProjectVertex3d(1, 1, i, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(1, 1, i, vert1, pRend), c0,
			  UnProjectVertex3d(0, 1, i, vert2, pRend), c0);

			pRendAux->DrawLine(
			  UnProjectVertex3d(0, 1, i, vert1, pRend), c0,
			  UnProjectVertex3d(0, 0, i, vert2, pRend), c0);
		}
	}

	void ResetCasterLists()
	{
		castersList.Clear();
		jobExecutedCastersList.Clear();
	}

	void                         GetMemoryUsage(ICrySizer* pSizer) const;

	int                          GetNumSides() const;
	CCamera                      GetCamera(int side) const;

	void                         SortRenderItemsForFrustumAsync(int side, struct SRendItem* pFirst, size_t nNumRendItems);

	void                         RenderShadowFrustum(IRenderViewPtr pShadowsView, int side, bool bJobCasters);
	void                         Job_RenderShadowCastersToView(const SRenderingPassInfo& passInfo, bool bJobCasters);

	CRenderView*                 GetNextAvailableShadowsView(CRenderView* pMainRenderView, ShadowMapFrustum* pOwnerFrustum);

	_smart_ptr<ShadowMapFrustum> Clone();
};
typedef _smart_ptr<ShadowMapFrustum> ShadowMapFrustumPtr;

struct SShadowRenderer
{
	// Iterate FrustumsToRender array from the CRenderView, and draw render nodes in every frustum there.
	static void RenderFrustumsToView(CRenderView* pRenderView);
};

struct ShadowFrustumMGPUCache : public ISyncMainWithRenderListener
{
	StaticArray<ShadowMapFrustumPtr, MAX_GSM_LODS_NUM> m_staticShadowMapFrustums;
	ShadowMapFrustumPtr                                m_pHeightMapAOFrustum;

	uint32 nUpdateMaskMT;
	uint32 nUpdateMaskRT;

	ShadowFrustumMGPUCache()
		: nUpdateMaskMT(0), nUpdateMaskRT(0)
	{
		m_pHeightMapAOFrustum = NULL;
		m_staticShadowMapFrustums.fill(NULL);
	};

	void Init()
	{
		m_pHeightMapAOFrustum = new ShadowMapFrustum;
		m_pHeightMapAOFrustum->pShadowCacheData = std::make_shared<ShadowMapFrustum::ShadowCacheData>();

		for (int i = 0; i < m_staticShadowMapFrustums.size(); ++i)
		{
			m_staticShadowMapFrustums[i] = new ShadowMapFrustum;
			m_staticShadowMapFrustums[i]->pShadowCacheData = std::make_shared<ShadowMapFrustum::ShadowCacheData>();
		}

		nUpdateMaskMT = nUpdateMaskRT = 0;
	}

	void Release()
	{
		m_pHeightMapAOFrustum = 0;
		for (int i = 0; i < m_staticShadowMapFrustums.size(); ++i)
		{
			m_staticShadowMapFrustums[i] = 0;
		}
	}

	void DeleteFromCache(IShadowCaster* pCaster)
	{
		for (int i = 0; i < m_staticShadowMapFrustums.size(); ++i)
		{
			if (ShadowMapFrustum* pFr = m_staticShadowMapFrustums[i])
			{
				pFr->castersList.Delete(pCaster);
				pFr->jobExecutedCastersList.Delete(pCaster);
			}
		}

		if (ShadowMapFrustum* pFr = m_pHeightMapAOFrustum)
		{
			m_pHeightMapAOFrustum->castersList.Delete(pCaster);
			m_pHeightMapAOFrustum->jobExecutedCastersList.Delete(pCaster);
		}
	}

	virtual void SyncMainWithRender()
	{
		/** What we need here is the renderer telling the main thread to update the shadow frustum cache when all GPUs are done
		 * with the current frustum.
		 *
		 * So in case the main thread has done a full update (nUpdateMaskMT has bits for all GPUs set) we need to copy
		 * the update mask to the renderer. Note that we reset the main thread update mask in the same spot to avoid doing it in
		 * the next frame again.
		 *
		 * Otherwise just copy the renderer's progress back to the main thread. The main thread will automatically do a full update
		 * when nUpdateMaskMT reaches 0																																													*/
		const int nFullUpdateMask = (1 << gEnv->pRenderer->GetActiveGPUCount()) - 1;
		if (nUpdateMaskMT == nFullUpdateMask)
		{
			nUpdateMaskRT = nUpdateMaskMT;
			nUpdateMaskMT = 0xFFFFFFFF;
		}
		else
			nUpdateMaskMT = nUpdateMaskRT;
	}
};

struct SShadowFrustumToRender
{
	ShadowMapFrustumPtr pFrustum;
	SRenderLight*       pLight;
	int                 nLightID;
	IRenderViewPtr      pShadowsView;

	SShadowFrustumToRender() : pFrustum(0), pLight(0), nLightID(0) {}
	SShadowFrustumToRender(ShadowMapFrustum* pFrustum, SRenderLight* pLight, int nLightID, IRenderViewPtr pShadowsView)
		: pFrustum(pFrustum)
		, pLight(pLight)
		, nLightID(nLightID)
		, pShadowsView(pShadowsView)
	{
		CRY_ASSERT(pFrustum->pDepthTex == nullptr);
	}
};

#endif
