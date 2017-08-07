// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   DeferredShading.h : Deferred shading pipeline

   =============================================================================*/

#ifndef _DEFERREDSHADING_H_
#define _DEFERREDSHADING_H_

#include "Common/RenderPipeline.h" // EShapeMeshType
#include "Common/Textures/Texture.h" // CTexture
#include "Common/Textures/PowerOf2BlockPacker.h" // CPowerOf2BlockPacker

struct IVisArea;

enum EDecalType
{
	DTYP_DARKEN,
	DTYP_BRIGHTEN,
	DTYP_ALPHABLEND,
	DTYP_ALPHABLEND_AND_BUMP,
	DTYP_ALPHABLEND_SPECULAR,
	DTYP_DARKEN_LIGHTBUF,
	DTYP_NUM,
};

#define MAX_DEFERRED_LIGHT_SOURCES 32
#define MAX_DEFERRED_CLIP_VOLUMES 64

class CTexPoolAtlas;

struct SShadowAllocData
{
	int    m_lightID;
	uint16 m_blockID;
	uint8  m_side;
	uint8  m_frameID;

	void Clear()  { m_blockID = 0xFFFF; m_lightID = -1; m_frameID = 0; }
	bool isFree() { return (m_blockID == 0xFFFF) ? true : false; }

	SShadowAllocData(){ Clear(); }
	~SShadowAllocData(){ Clear(); }

};

class CDeferredShading
{
public:

	static inline bool IsValid()
	{
		return m_pInstance != NULL;
	}

	static CDeferredShading& Instance()
	{
		return (*m_pInstance);
	}

	void Render(CRenderView* pRenderView);
	void SetupPasses();
	void SetupGlobalConsts();

	//shadows
	void        ResolveCurrentBuffers();
	void        RestoreCurrentBuffers();
	bool        PackAllShadowFrustums(bool bPreLoop);
	bool        PackToPool(CPowerOf2BlockPacker* pBlockPack, SRenderLight& light, const int nLightID, const int nFirstCandidateLight, bool bClearPool);

	void        FilterGBuffer();
	void        PrepareClipVolumeData(bool& bOutdoorVisible);
	bool        AmbientPass(const SRenderLight* pGlobalCubemap, bool& bOutdoorVisible);

	bool        DeferredDecalPass(const SDeferredDecal& rDecal, uint32 indDecal);
	bool        ShadowLightPasses(const SRenderLight& light, const int nLightID);
	void        DrawDecalVolume(const SDeferredDecal& rDecal, Matrix44A& mDecalLightProj, ECull volumeCull);
	void        DrawLightVolume(EShapeMeshType meshType, const Matrix44& mVolumeToWorld, const Vec4& vSphereAdjust = Vec4(ZERO));
	void        LightPass(const SRenderLight* const __restrict pDL, bool bForceStencilDisable = false);
	void        DeferredCubemaps(const RenderLightsList& rCubemaps, uint32 nStartIndex = 0);
	void        DeferredCubemapPass(const SRenderLight* const __restrict pDL);
	void        DeferredLights(RenderLightsList& rLights, bool bCastShadows);
	void        DeferredShadingPass();

	void        CreateDeferredMaps();
	void        DestroyDeferredMaps();
	void        Release();
	void        Debug();
	void        DebugGBuffer();

	RenderLightIndex AddLight(const CDLight& pDL, float fMult, const SRenderingPassInfo& passInfo);

	void        AddGIClipVolume(IRenderMesh* pClipVolume, const Matrix34& mxTransform);
	inline void ResetLights();

	// called in between levels to free up memory
	void          ReleaseData();

	void          GetClipVolumeParams(const Vec4*& pParams, uint32& nCount);
	CTexture*     GetResolvedStencilRT() { return m_pResolvedStencilRT; }
	void          GetLightRenderSettings(const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType);

	inline uint32 GetLightsCount() const
	{
		return m_nLightsProcessedCount;
	}

	inline Vec4 GetLightDepthBounds(const SRenderLight* pDL, bool bReverseDepth) const
	{
		float fRadius = pDL->m_fRadius;
		if (pDL->m_Flags & DLF_AREA_LIGHT) // Use max for area lights.
			fRadius += max(pDL->m_fAreaWidth, pDL->m_fAreaHeight);
		else if (pDL->m_Flags & DLF_DEFERRED_CUBEMAPS)
			fRadius = pDL->m_ProbeExtents.len(); // This is not optimal for a box

		return GetLightDepthBounds(pDL->m_Origin, fRadius, bReverseDepth);
	}

	inline Vec4 GetLightDepthBounds(const Vec3& vCenter, float fRadius, bool bReverseDepth) const
	{
		if (!CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		{
			return Vec4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		Vec3 pBounds = m_pCamFront * fRadius;
		Vec3 pMax = vCenter - pBounds;
		Vec3 pMin = vCenter + pBounds;

		float fMinZ = m_mViewProj.m20 * pMin.x + m_mViewProj.m21 * pMin.y + m_mViewProj.m22 * pMin.z + m_mViewProj.m23;
		float fMaxZ = 1.0f;
		float fMinW = m_mViewProj.m30 * pMin.x + m_mViewProj.m31 * pMin.y + m_mViewProj.m32 * pMin.z + m_mViewProj.m33;
		float fMaxW = 1.0f;

		float fMinDivisor = (float)__fsel(-fabsf(fMinW), 1.0f, fMinW);
		fMinZ = (float)__fsel(-fabsf(fMinW), 1.0f, fMinZ / fMinDivisor);
		fMinZ = (float)__fsel(fMinW, fMinZ, bReverseDepth ? 1.0f : 0.0f);

		fMinZ = clamp_tpl(fMinZ, 0.01f, 1.f);

		fMaxZ = m_mViewProj.m20 * pMax.x + m_mViewProj.m21 * pMax.y + m_mViewProj.m22 * pMax.z + m_mViewProj.m23;
		fMaxW = m_mViewProj.m30 * pMax.x + m_mViewProj.m31 * pMax.y + m_mViewProj.m32 * pMax.z + m_mViewProj.m33;
		float fMaxDivisor = (float)__fsel(-fabsf(fMaxW), 1.0f, fMaxW);
		fMaxZ = (float)__fsel(-fabsf(fMaxW), 1.0f, fMaxZ / fMaxDivisor);
		fMaxZ = (float)__fsel(fMaxW, fMaxZ, bReverseDepth ? 1.0f : 0.0f);

		if (bReverseDepth)
		{
			std::swap(fMinZ, fMaxZ);
		}

		fMaxZ = clamp_tpl(fMaxZ, fMinZ, 1.f);

		return Vec4(fMinZ, max(fMinW, 0.000001f), fMaxZ, max(fMaxW, 0.000001f));
	}

	void             GetScissors(const Vec3& vCenter, float fRadius, short& sX, short& sY, short& sWidth, short& sHeight) const;
	void             SetupScissors(bool bEnable, uint16 x, uint16 y, uint16 w, uint16 h) const;

	const Matrix44A& GetCameraProjMatrix() const { return m_mViewProj; }

private:

	CDeferredShading()
		: m_pCurrentRenderView(0)
		, m_pShader(0)
		, m_pTechName("DeferredLightPass")
		, m_pAmbientOutdoorTechName("AmbientPass")
		, m_pCubemapsTechName("DeferredCubemapPass")
		, m_pCubemapsVolumeTechName("DeferredCubemapVolumePass")
		, m_pReflectionTechName("SSR_Raytrace")
		, m_pDebugTechName("Debug")
		, m_pDeferredDecalTechName("DeferredDecal")
		, m_pLightVolumeTechName("DeferredLightVolume")
		, m_pParamLightPos("g_LightPos")
		, m_pParamLightProjMatrix("g_mLightProj")
		, m_pGeneralParams("g_GeneralParams")
		, m_pParamLightDiffuse("g_LightDiffuse")
		, m_pParamAmbient("g_cDeferredAmbient")
		, m_pParamAmbientGround("g_cAmbGround")
		, m_pParamAmbientHeight("g_vAmbHeightParams")
		, m_pAttenParams("g_vAttenParams")
		, m_pParamDecalTS("g_mDecalTS")
		, m_pParamDecalDiffuse("g_DecalDiffuse")
		, m_pParamDecalSpecular("g_DecalSpecular")
		, m_pParamDecalMipLevels("g_DecalMipLevels")
		, m_pClipVolumeParams("g_vVisAreasParams")
		, m_pLBufferDiffuseRT(CTexture::s_ptexCurrentSceneDiffuseAccMap)
		, m_pLBufferSpecularRT(CTexture::s_ptexSceneSpecularAccMap)
		, m_pNormalsRT(CTexture::s_ptexSceneNormalsMap)
		, m_pDepthRT(CTexture::s_ptexZTarget)
		, m_pMSAAMaskRT(CTexture::s_ptexBackBuffer)
		, m_pResolvedStencilRT(CTexture::s_ptexStereoR)
		, m_pDiffuseRT(CTexture::s_ptexSceneDiffuse)
		, m_pSpecularRT(CTexture::s_ptexSceneSpecular)
		, m_nLightsProcessedCount(0)
		, m_nCurLighID(-1)
		, m_nWarningFrame(0)
		, m_bSpecularState(false)
		, m_nShadowPoolSize(0)
		, m_nRenderState(GS_BLSRC_ONE | GS_BLDST_ONE)
		, m_nThreadID(0)
		, m_nRecurseLevel(0)
		, m_blockPack(0, 0)
	{

		memset(m_vClipVolumeParams, 0, sizeof(Vec4) * (MAX_DEFERRED_CLIP_VOLUMES));

		for (int i = 0; i < MAX_GPU_NUM; ++i)
		{
			m_prevViewProj[i].SetIdentity();
		}

		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			for (int j = 0; j < MAX_REND_RECURSION_LEVELS; ++j)
			{
				m_nVisAreasGIRef[i][j] = 0;
			}
		}
	}

	~CDeferredShading()
	{
		Release();
	}

	// Allow disable mrt usage: for double zspeed and on other passes less fillrate hit
	void SpecularAccEnableMRT(bool bEnable);

private:
	uint32 m_nVisAreasGIRef[RT_COMMAND_BUF_COUNT][MAX_REND_RECURSION_LEVELS];

	struct SClipShape
	{
		IRenderMesh* pShape;
		Matrix34     mxTransform;
		SClipShape() : pShape(NULL), mxTransform(Matrix34::CreateIdentity()) {}
		SClipShape(IRenderMesh* _pShape, const Matrix34& _mxTransform) : pShape(_pShape), mxTransform(_mxTransform) {}
	};

	// Clip volumes for GI for current view
	TArray<SClipShape> m_vecGIClipVolumes[RT_COMMAND_BUF_COUNT][MAX_REND_RECURSION_LEVELS];

	CRenderView*       m_pCurrentRenderView;

	Vec3               m_pCamPos;
	Vec3               m_pCamFront;
	float              m_fCamFar;
	float              m_fCamNear;

	float              m_fRatioWidth;
	float              m_fRatioHeight;

	CShader*           m_pShader;
	CCryNameTSCRC      m_pDeferredDecalTechName;
	CCryNameTSCRC      m_pLightVolumeTechName;
	CCryNameTSCRC      m_pTechName;
	CCryNameTSCRC      m_pAmbientOutdoorTechName;
	CCryNameTSCRC      m_pCubemapsTechName;
	CCryNameTSCRC      m_pCubemapsVolumeTechName;
	CCryNameTSCRC      m_pReflectionTechName;
	CCryNameTSCRC      m_pDebugTechName;
	CCryNameR          m_pParamLightPos;
	CCryNameR          m_pParamLightDiffuse;
	CCryNameR          m_pParamLightProjMatrix;
	CCryNameR          m_pGeneralParams;
	CCryNameR          m_pParamAmbient;
	CCryNameR          m_pParamAmbientGround;
	CCryNameR          m_pParamAmbientHeight;
	CCryNameR          m_pAttenParams;

	CCryNameR          m_pParamDecalTS;
	CCryNameR          m_pParamDecalDiffuse;
	CCryNameR          m_pParamDecalSpecular;
	CCryNameR          m_pParamDecalMipLevels;
	CCryNameR          m_pClipVolumeParams;

	Matrix44A          m_mViewProj;
	Matrix44A          m_pViewProjI;
	Matrix44A          m_pView;

	Matrix44A          m_prevViewProj[MAX_GPU_NUM];

	Vec4               vWorldBasisX, vWorldBasisY, vWorldBasisZ;

	CTexture*          m_pLBufferDiffuseRT;
	CTexture*          m_pLBufferSpecularRT;

	CRY_ALIGN(16) Vec4 m_vClipVolumeParams[MAX_DEFERRED_CLIP_VOLUMES];

	CTexture*                m_pDiffuseRT;
	CTexture*                m_pSpecularRT;
	CTexture*                m_pNormalsRT;
	CTexture*                m_pDepthRT;

	CTexture*                m_pMSAAMaskRT;
	CTexture*                m_pResolvedStencilRT;

	int                      m_nWarningFrame;

	int                      m_nRenderState;
	uint32                   m_nLightsProcessedCount;

	uint32                   m_nThreadID;
	int32                    m_nRecurseLevel;

	uint                     m_nCurrentShadowPoolLight;
	uint                     m_nFirstCandidateShadowPoolLight;
	uint                     m_nShadowPoolSize;
	bool                     m_bClearPool;

	bool                     m_bSpecularState;
	int                      m_nCurLighID;
	short                    m_nCurTargetWidth;
	short                    m_nCurTargetHeight;
	static CDeferredShading* m_pInstance;

	friend class CTiledShading;
	friend class CShadowMapStage;
	friend class CVolumetricFogStage; // for access to m_nCurrentShadowPoolLight and m_nFirstCandidateShadowPoolLight.

	CPowerOf2BlockPacker     m_blockPack;
	TArray<SShadowAllocData> m_shadowPoolAlloc;

public:
	CRenderView*             RenderView() { return m_pCurrentRenderView; }

	static CDeferredShading* CreateDeferredShading()
	{
		m_pInstance = new CDeferredShading();

		return m_pInstance;
	}

	static void DestroyDeferredShading()
	{
		SAFE_DELETE(m_pInstance);
	}
};

class CTexPoolAtlas
{
public:
	CTexPoolAtlas()
	{
		m_nSize = 0;
#ifdef _DEBUG
		m_nTotalWaste = 0;
#endif
	}
	~CTexPoolAtlas()    {}
	void Init(int nSize);
	void Clear();
	void FreeMemory();
	bool AllocateGroup(int32* pOffsetX, int32* pOffsetY, int nSizeX, int nSizeY);

	int              m_nSize;
	static const int MAX_BLOCKS = 128;
	uint32           m_arrAllocatedBlocks[MAX_BLOCKS];

#ifdef _DEBUG
	uint32 m_nTotalWaste;
	struct SShadowMapBlock
	{
		uint16 m_nX1, m_nX2, m_nY1, m_nY2;
		bool   Intersects(const SShadowMapBlock& b) const
		{
			return max(m_nX1, b.m_nX1) < min(m_nX2, b.m_nX2)
			       && max(m_nY1, b.m_nY1) < min(m_nY2, b.m_nY2);
		}
	};
	std::vector<SShadowMapBlock> m_arrDebugBlocks;

	void  _AddDebugBlock(int x, int y, int sizeX, int sizeY);
	float _GetDebugUsage() const;
#endif
};

#endif
