// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(SHADOWRENDERER_H)
#define SHADOWRENDERER_H

#include "ShadowUtils.h"

#include "Textures/PowerOf2BlockPacker.h" // CPowerOf2BlockPacker

#include <CryCore/Containers/VectorSet.h>

#include <array>
#include <bitset>

#define OMNI_SIDES_NUM 6

constexpr uint32 kMaxShadowPassesNum = sizeof(uint32) * CHAR_BIT - 1; // reserve first bit for main view

// data used to compute a custom shadow frustum for near shadows
struct CustomShadowMapFrustumData
{
	AABB aabb;
};

struct SShadowAllocData
{
	int    m_lightID = -1;
	uint16 m_blockID = 0xFFFF;
	uint8  m_side;

	void Clear()        { m_blockID = 0xFFFF; m_lightID = -1; }
	bool isFree() const { return m_blockID == 0xFFFF; }
};

struct ShadowMapFrustum : public CMultiThreadRefCount
{
	enum eFrustumType // NOTE: Be careful when modifying the enum as it is used for sorting frustums in SCompareByLightIds
	{
		e_GsmDynamic         = 0, // normal sun shadow cascade or shadow from local light
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

		ShadowCacheData() { Reset(1); }

		void Reset(uint32 cacheGeneration)
		{
			memset(mOctreePath, 0x0, sizeof(mOctreePath));
			memset(mOctreePathNodeProcessed, 0x0, sizeof(mOctreePathNodeProcessed));
			mGeneration = cacheGeneration;
			mObjectsRendered = 0;
		}
		uint32                           mObjectsRendered;
		uint8                            mGeneration;

		static const int                 MAX_TRAVERSAL_PATH_LENGTH = 32;
		uint8                            mOctreePath[MAX_TRAVERSAL_PATH_LENGTH];
		uint8                            mOctreePathNodeProcessed[MAX_TRAVERSAL_PATH_LENGTH];
	};

public:
	eFrustumType m_eFrustumType; // NOTE: adjust constructor if you add any variable before this
	uint32       m_cachedShadowFrameId = 0;
	Matrix44A    mLightProjMatrix;
	Matrix44A    mLightViewMatrix;
	Vec4         vFrustInfo;

	//packer params
	uint              nPackID[OMNI_SIDES_NUM];
	TRect_tpl<uint32> shadowPoolPack[OMNI_SIDES_NUM];

	uint8             nShadowPoolUpdateRate;                                   // For time-sliced updates: Update rate in frames count

	// Only one side in case of directional lights
	std::bitset<OMNI_SIDES_NUM> nOmniFrustumMask;                              // Mask of enabled sides
	uint32                      nSideSampleMask = 0;                           // Mask of sides that participate in shadow casting
	uint8                       nSideDrawnOnFrame[OMNI_SIDES_NUM] = { 0 };     // Congruence class of FrameID upon which side was last rendererd (modulo 255).
	std::bitset<OMNI_SIDES_NUM> nSideCacheMask;                                // In case of time-sliced updates: Bit-mask indicating whether or not a frusutm side is cached and valid and does not require updates.
	std::bitset<OMNI_SIDES_NUM> nOutdatedSideMask = 0xff;                      // Mask of out-of-date frustum sides
	std::bitset<OMNI_SIDES_NUM> nSideInvalidatedMask = 0xff;                   // Mask of invalidated frustum sides for each GPU

	std::atomic<uint32>&       GetSideSampleMask()       { return *reinterpret_cast<std::atomic<uint32>*>(&this->nSideSampleMask); }
	const std::atomic<uint32>& GetSideSampleMask() const { return *reinterpret_cast<const std::atomic<uint32>*>(&this->nSideSampleMask); }
	std::atomic<uint32>&       GetOnePassCastersCount() const  { return *reinterpret_cast<std::atomic<uint32>*>(&this->onePassCastersNum); }

	// flags
	bool bIncrementalUpdate;

	// if set to true - castersList contains all casters in light radius
	// and all other members related only to single frustum projection case are undefined
	bool  bOmniDirectionalShadow;
	bool  bBlendFrustum;
	float fBlendVal;

	bool  bIsMGPUCopy;

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
	int                             nTextureWidth;
	int                             nTextureHeight;
	ColorF                          clearValue;
	int                             nShadowMapSize;
	int                             nResetID;
	float                           fFrustrumSize;
	float                           fProjRatio;
	float                           fDepthTestBias;
	float                           fDepthConstBias;
	float                           fDepthSlopeBias;
	float                           fDepthBiasClamp;
	mutable int                     onePassCastersNum = 0;        // Contains number of casters if one-pass octree traversal is used for this frustum

	CCamera                         FrustumPlanes[OMNI_SIDES_NUM];
	AABB                            aabbCasters;      //casters bbox in world space
	Vec3                            vLightSrcRelPos;  // relative world space
	Vec3                            vProjTranslation; // dst position
	float                           fRadius;
	int                             nUpdateFrameId;
	IRenderNode*                    pLightOwner;
	IRenderViewPtr                  pOnePassShadowView;           // if one-pass octree traversal is used this view is allocated and filled by 3DEngine
	int                             nShadowMapLod;                // currently use as GSMLod, can be used as cubemap side, -1 means this variable is not used
	int                             nShadowCacheLod;
	uint32                          m_Flags;

	// Render view that is used to accumulate items for this frustum.
	std::shared_ptr<ShadowCacheData> pShadowCacheData;

public:
	ShadowMapFrustum()
		: m_eFrustumType(e_GsmDynamic)
		, mLightProjMatrix(IDENTITY)
		, mLightViewMatrix(IDENTITY)
		, vFrustInfo(ZERO)
		, bIncrementalUpdate(false)
		, bOmniDirectionalShadow(false)
		, bBlendFrustum(false)
		, fBlendVal(0)
		, bIsMGPUCopy(false)
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
		, clearValue(Clr_FarPlane)
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
		, nUpdateFrameId(-1000)
		, pLightOwner(nullptr)
		, nShadowMapLod(0)
		, m_Flags(0)
		, nShadowCacheLod(0)
	{
		ZeroArray(nPackID);
	}

	void GetSideViewport(int nSide, int* pViewport) const
	{
		if (bUseShadowsPool)
		{
			pViewport[0] = shadowPoolPack[nSide].Min.x;
			pViewport[1] = shadowPoolPack[nSide].Min.y;
			pViewport[2] = shadowPoolPack[nSide].GetDim().x;
			pViewport[3] = shadowPoolPack[nSide].GetDim().y;
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

	void GetTexOffset(int nSide, float* pOffset, float* pScale) const
	{
		if (bUseShadowsPool)
		{
			pScale[0] = float(nShadowMapSize) / nTextureWidth;               //SHADOWS_POOL_SZ 1024
			pScale[1] = float(nShadowMapSize) / nTextureHeight;
			pOffset[0] = float(shadowPoolPack[nSide].Min.x) / nTextureWidth;
			pOffset[1] = float(shadowPoolPack[nSide].Min.y) / nTextureHeight;
		}
		else
		{
			pScale[0] = 1.0f / 3.0f;
			pScale[1] = 1.0f / 2.0f;
			pOffset[0] = 1.0f / 3.0f * (nSide % 3);
			pOffset[1] = 1.0f / 2.0f * (nSide / 3);
		}
	}

	void RequestUpdate()
	{
		nOutdatedSideMask.set();
	}

	// Invalidates current content and requests update
	void Invalidate()
	{
		// Mark frustum as out-of-date
		RequestUpdate();
		// And invalidate all sides
		nSideInvalidatedMask.set();
	}

	void InvalidateSide(int side)
	{
		nOutdatedSideMask.set(side);
		nSideInvalidatedMask.set(side);
	}

	bool isUpdateRequested() const
	{
		return nSideInvalidatedMask.any() || nOutdatedSideMask.any();
	}

	bool isSideOutdated(int side) const
	{
		return nOutdatedSideMask[side];
	}

	bool isSideInvalidated(int side) const
	{
		return nSideInvalidatedMask[side];
	}

	bool ShouldCacheSideHint(int side) const
	{
		return nSideCacheMask[side];
	}

	bool ShouldSampleSide(int side) const
	{
		return !!(GetSideSampleMask().load() & BIT(side));
	}

	bool ShouldSample() const
	{
		return !!GetSideSampleMask().load();
	}

	bool ShouldUpdateSide(int side) const
	{
		return !nSideCacheMask[side] && ShouldSampleSide(side);
	}

	void MarkSideAsRendered(int side, uint8 frameID8)
	{
		nOutdatedSideMask.set(side, false);
		nSideInvalidatedMask.set(side, false);
		nSideDrawnOnFrame[side] = frameID8;
	}

	void MarkShadowGenMaskForSide(int side)
	{
		GetSideSampleMask() |= BIT(side);
	}

	bool IsCached() const
	{
		return m_eFrustumType == e_GsmCached || m_eFrustumType == e_HeightMapAO;
	}

	bool IsDynamicGsmCascade() const
	{
		return (m_Flags & DLF_SUN) && (m_eFrustumType == e_GsmDynamic || m_eFrustumType == e_GsmDynamicDistance);
	}

	ILINE bool IntersectAABB(const AABB& bbox, bool* pAllIn, int side = -1) const
	{
		if (bOmniDirectionalShadow)
		{
			return (side < 0)
				? bbox.IsOverlapSphereBounds(vLightSrcRelPos + vProjTranslation, fFarDist)
				: FrustumPlanes[side].IsAABBVisible_EH(bbox, pAllIn) > 0;
		}

		if (bBlendFrustum)
		{
			if (FrustumPlanes[1].IsAABBVisible_EH(bbox, pAllIn) > 0)
				return true;
		}

		bool bDummy = false;
		return FrustumPlanes[0].IsAABBVisible_EH(bbox, bBlendFrustum ? &bDummy : pAllIn) > 0;
	}

	Vec3 UnProject(float sx, float sy, float sz, IRenderer* pRend) const
	{
		const int shadowViewport[4] = { 0, 0, 1, 1 };

		Matrix44A mIden;
		mIden.SetIdentity();

		//FIX remove float arrays
		Vec3 vert;
		pRend->UnProject(sx, sy, sz,
		                 &vert.x, &vert.y, &vert.z,
		                 (float*)&mLightViewMatrix,
		                 (float*)&mIden,
		                 shadowViewport);

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
			const float fFOV = static_cast<float>(bOmniDirectionalShadow ? DEG2RAD_R(g_fOmniShadowFov) : DEG2RAD_R(90.0f));

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

		ColorB c0 = cCascadeColors[nShadowMapLod % nColorCount];
		{
			pRendAux->DrawLine(
			  UnProject(0.0f, 0.0f, 0.0f, pRend), c0,
			  UnProject(0.0f, 0.0f, 1.0f, pRend), c0);

			pRendAux->DrawLine(
			  UnProject(1.0f, 0.0f, 0.0f, pRend), c0,
			  UnProject(1.0f, 0.0f, 1.0f, pRend), c0);

			pRendAux->DrawLine(
			  UnProject(1.0f, 1.0f, 0.0f, pRend), c0,
			  UnProject(1.0f, 1.0f, 1.0f, pRend), c0);

			pRendAux->DrawLine(
			  UnProject(0.0f, 1.0f, 0.0f, pRend), c0,
			  UnProject(0.0f, 1.0f, 1.0f, pRend), c0);
		}

		for (int i = 0; i <= 1; i++)
		{
			pRendAux->DrawLine(
			  UnProject(0.0f, 0.0f, static_cast<float>(i), pRend), c0,
			  UnProject(1.0f, 0.0f, static_cast<float>(i), pRend), c0);

			pRendAux->DrawLine(
			  UnProject(1.0f, 0.0f, static_cast<float>(i), pRend), c0,
			  UnProject(1.0f, 1.0f, static_cast<float>(i), pRend), c0);

			pRendAux->DrawLine(
			  UnProject(1.0f, 1.0f, static_cast<float>(i), pRend), c0,
			  UnProject(0.0f, 1.0f, static_cast<float>(i), pRend), c0);

			pRendAux->DrawLine(
			  UnProject(0.0f, 1.0f, static_cast<float>(i), pRend), c0,
			  UnProject(0.0f, 0.0f, static_cast<float>(i), pRend), c0);
		}
	}

	void ResetCasterLists()
	{
		onePassCastersNum = 0;
	}

	int GetCasterNum() const
	{
		return onePassCastersNum;
	}

	int GetNumSides() const
	{
		return bOmniDirectionalShadow ? OMNI_SIDES_NUM : 1;
	}

	const CCamera& GetCamera(int side) const
	{
		return bOmniDirectionalShadow ? FrustumPlanes[side] : gEnv->p3DEngine->GetRenderingCamera();
	}

	void GetMemoryUsage(ICrySizer* pSizer) const;

	void SortRenderItemsForFrustumAsync(int side, struct SRendItem* pFirst, size_t nNumRendItems);

	// Reserves a shadowpool slot
	void                         PrepareForShadowPool(uint32 frameID, uint32& numShadowPoolAllocsThisFrame, CPowerOf2BlockPacker& blockPack, TArray<SShadowAllocData>& shadowPoolAlloc, const SRenderLight& light, uint32 timeSlicedShadowUpdatesLimit = ~0, uint32* timeSlicedShadowsUpdated = nullptr);
	// For time-sliced updates: Returns a mask of per-side flags that hint whether or not the side should be updated
	std::bitset<6>               GenerateTimeSlicedUpdateCacheMask(uint32 frameID) const;

	CRenderView*                 GetNextAvailableShadowsView(CRenderView* pMainRenderView);

	_smart_ptr<ShadowMapFrustum> Clone() const { return new ShadowMapFrustum(*this); }

	bool NodeRequiresShadowCacheUpdate(const IRenderNode* pNode) const
	{
		CRY_ASSERT(IsCached() && nShadowCacheLod>=0 && nShadowCacheLod < MAX_GSM_CACHED_LODS_NUM);
		return pNode->m_shadowCacheLastRendered[nShadowCacheLod] != pShadowCacheData->mGeneration;
	}

	void MarkNodeAsCached(IRenderNode* pNode, bool isCached = true) const
	{
		CRY_ASSERT(this->IsCached());
		if (pNode)
		{
			CRY_ASSERT(nShadowCacheLod >= 0 && nShadowCacheLod < MAX_GSM_CACHED_LODS_NUM);
			pNode->m_shadowCacheLastRendered[nShadowCacheLod] = isCached ? pShadowCacheData->mGeneration : 0;
			pShadowCacheData->mObjectsRendered++;
		}
	}

	static void ForceMarkNodeAsUncached(IRenderNode* pNode)
	{
		if (pNode)
		{
			ZeroArray(pNode->m_shadowCacheLastRendered);
		}
	}
};
typedef _smart_ptr<ShadowMapFrustum> ShadowMapFrustumPtr;

struct SShadowRenderer
{
	// Iterate FrustumsToRender array from the CRenderView, and draw render nodes in every frustum there.
	static void FinishRenderFrustumsToView(CRenderView* pRenderView);
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
	SShadowFrustumToRender(ShadowMapFrustum* _pFrustum, SRenderLight* _pLight, int _nLightID, IRenderViewPtr _pShadowsView)
		: pFrustum(_pFrustum)
		, pLight(_pLight)
		, nLightID(_nLightID)
		, pShadowsView(std::move(_pShadowsView))
	{
		pShadowsView->SetShadowFrustumOwner(pFrustum);
		CRY_ASSERT(pFrustum->pDepthTex == nullptr);
	}
};

#endif
