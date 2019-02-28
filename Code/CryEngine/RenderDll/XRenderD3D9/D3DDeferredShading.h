// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   DeferredShading.h : Deferred shading pipeline

   =============================================================================*/

#ifndef _DEFERREDSHADING_H_
#define _DEFERREDSHADING_H_

#include "Common/RenderPipeline.h" // EShapeMeshType
#include "Common/Textures/Texture.h" // CTexture
#include "Common/Textures/PowerOf2BlockPacker.h" // CPowerOf2BlockPacker
#include "Common/Shadow_Renderer.h"

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

class CDeferredShading
{
public:

	static inline bool IsValid()
	{
		return m_pInstance != nullptr;
	}

	static CDeferredShading& Instance()
	{
		return (*m_pInstance);
	}

	void SetupPasses(CRenderView* pRenderView);
	void SetupGlobalConsts(CRenderView* pRenderView);

	void        Release();

	void        AddGIClipVolume(IRenderMesh* pClipVolume, const Matrix34& mxTransform);

	// called in between levels to free up memory
	void          ReleaseData();

	void          GetClipVolumeParams(const Vec4*& pParams);
	CTexture*     GetResolvedStencilRT() { return m_pResolvedStencilRT; }
	void          GetLightRenderSettings(const CRenderView* pRenderView, const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType);

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

	const Matrix44A& GetCameraProjMatrix() const { return m_mViewProj; }

	CPowerOf2BlockPacker&           GetBlockPacker() { return m_blockPack; }
	const CPowerOf2BlockPacker&     GetBlockPacker() const { return m_blockPack; }
	TArray<SShadowAllocData>&       GetShadowPoolAllocator() { return m_shadowPoolAlloc; }
	const TArray<SShadowAllocData>& GetShadowPoolAllocator() const { return m_shadowPoolAlloc; }

private:

	CDeferredShading()
		: m_pShader(0)
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
		, m_pNormalsRT(CRendererResources::s_ptexSceneNormalsMap)
		, m_pDepthRT(CRendererResources::s_ptexLinearDepth)
		, m_pMSAAMaskRT(CRendererResources::s_ptexBackBuffer)
		, m_pResolvedStencilRT(CRendererResources::s_ptexVelocity)
		, m_pDiffuseRT(CRendererResources::s_ptexSceneDiffuse)
		, m_pSpecularRT(CRendererResources::s_ptexSceneSpecular)
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

	Vec3               m_pCamPos;
	Vec3               m_pCamFront;
	float              m_fCamFar;
	float              m_fCamNear;

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

	CRY_ALIGN(16) Vec4 m_vClipVolumeParams[MAX_DEFERRED_CLIP_VOLUMES];

	CTexture*                m_pDiffuseRT;
	CTexture*                m_pSpecularRT;
	CTexture*                m_pNormalsRT;
	CTexture*                m_pDepthRT;

	CTexture*                m_pMSAAMaskRT;
	CTexture*                m_pResolvedStencilRT;

	int                      m_nRenderState;

	uint32                   m_nThreadID;
	int32                    m_nRecurseLevel;

	uint                     m_nShadowPoolSize = 0;

	short                    m_nCurTargetWidth;
	short                    m_nCurTargetHeight;
	static CDeferredShading* m_pInstance;

	friend class CD3D9Renderer;
	friend class CRenderer;
	friend class CTiledLightVolumesStage;
	friend class CShadowMapStage;
	friend class CVolumetricFogStage; // for access to m_nCurrentShadowPoolLight and m_nFirstCandidateShadowPoolLight.

	CPowerOf2BlockPacker     m_blockPack;
	TArray<SShadowAllocData> m_shadowPoolAlloc;

public:
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
