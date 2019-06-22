// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/RenderPipeline.h"
#include "Common/Textures/Texture.h"
#include "Common/Textures/PowerOf2BlockPacker.h"
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

#define MAX_DEFERRED_CLIP_VOLUMES 64

class CTexPoolAtlas;

class CDeferredShading
{
public:
	CDeferredShading(CGraphicsPipeline* pGraphicsPipeline);
	~CDeferredShading()
	{
		Release();
	}

	void SetupPasses(CRenderView* pRenderView);
	void SetupGlobalConsts(CRenderView* pRenderView);

	void Release();

	// called in between levels to free up memory
	void        ReleaseData();

	void        GetLightRenderSettings(const CRenderView* pRenderView, const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType);

	const Matrix44A&                GetCameraProjMatrix()    const { return m_mViewProj; }

	CPowerOf2BlockPacker&           GetBlockPacker()               { return m_blockPack; }
	const CPowerOf2BlockPacker&     GetBlockPacker()         const { return m_blockPack; }
	TArray<SShadowAllocData>&       GetShadowPoolAllocator()       { return m_shadowPoolAlloc; }
	const TArray<SShadowAllocData>& GetShadowPoolAllocator() const { return m_shadowPoolAlloc; }

private:
	uint32 m_nVisAreasGIRef[RT_COMMAND_BUF_COUNT][MAX_REND_RECURSION_LEVELS];

	struct SClipShape
	{
		IRenderMesh* pShape;
		Matrix34     mxTransform;
		SClipShape() : pShape(NULL), mxTransform(Matrix34::CreateIdentity()) {}
		SClipShape(IRenderMesh* _pShape, const Matrix34& _mxTransform) : pShape(_pShape), mxTransform(_mxTransform) {}
	};

	Vec3               m_pCamPos;
	Vec3               m_pCamFront;
	float              m_fCamFar;
	float              m_fCamNear;

	Matrix44A          m_mViewProj;
	Matrix44A          m_pView;

	Matrix44A          m_prevViewProj[MAX_GPU_NUM];

	Vec4               vWorldBasisX, vWorldBasisY, vWorldBasisZ;

	CTexture*          m_pDiffuseRT;

	uint               m_nShadowPoolSize = 0;

	short              m_nCurTargetWidth;
	short              m_nCurTargetHeight;


	friend class CD3D9Renderer;
	friend class CRenderer;
	friend class CTiledLightVolumesStage;
	friend class CShadowMapStage;
	friend class CVolumetricFogStage; // for access to m_nCurrentShadowPoolLight and m_nFirstCandidateShadowPoolLight.

	CPowerOf2BlockPacker     m_blockPack;
	TArray<SShadowAllocData> m_shadowPoolAlloc;

	CGraphicsPipeline*       m_pGraphicsPipeline;
};
