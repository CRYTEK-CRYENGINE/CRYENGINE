// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "../Common/Textures/TextureManager.h"
#include "../Common/Textures/TextureHelpers.h"
#include "../Common/RendElements/FlareSoftOcclusionQuery.h"
#include "../Common/ReverseDepth.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/DeferredDecals.h"
#include "GraphicsPipeline/ShadowMask.h"
#include "GraphicsPipeline/Water.h"
#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#include "Common/RenderView.h"
#include "Common/RendererResources.h"

#define MAX_VIS_AREAS 32

// MSAA potential optimizations todo:
//  - long term: port all functionality to compute, including all extra effects passes.

// About MSAA:
// - Please be careful when accessing or rendering into msaa'ed targets. When adding new techniques please make sure to test
// - For post process technique to be MSAA friendly, do either:
//    - Use compute. Single pass and as efficient as gets. Context switches might be problematic, until all lighting pipeline done like this.
//    - For non compute, require 2 passes. One at pixel frequency, other at sub sample frequency.
//				 - Reuse existing sample frequency regions on stencil via stencilread/write mask:
//						- If not possible, tag pixel frequency regions using stencil + m_pMSAAMaskRT
//						- Alternative poor man version, do clip in shader.

#pragma warning(push)
#pragma warning(disable: 4244)

CDeferredShading* CDeferredShading::m_pInstance = NULL;

#define RT_LIGHTSMASK                     g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ] | g_HWSR_MaskBit[HWSR_CUBEMAP0]
#define RT_LIGHTPASS_RESETMASK            g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ] | g_HWSR_MaskBit[HWSR_CUBEMAP0]
#define RT_DEBUGMASK                      g_HWSR_MaskBit[HWSR_DEBUG0] | g_HWSR_MaskBit[HWSR_DEBUG1] | g_HWSR_MaskBit[HWSR_DEBUG2] | g_HWSR_MaskBit[HWSR_DEBUG3]
#define RT_TEX_PROJECT                    g_HWSR_MaskBit[HWSR_SAMPLE0]
#define RT_GLOBAL_CUBEMAP                 g_HWSR_MaskBit[HWSR_SAMPLE0]
#define RT_SPECULAR_CUBEMAP               g_HWSR_MaskBit[HWSR_SAMPLE1]
#define RT_AMBIENT_LIGHT                  g_HWSR_MaskBit[HWSR_SAMPLE5]
#define RT_GLOBAL_CUBEMAP_IGNORE_VISAREAS g_HWSR_MaskBit[HWSR_SAMPLE4]
#define RT_AREALIGHT                      g_HWSR_MaskBit[HWSR_SAMPLE1]
#define RT_OVERDRAW_DEBUG                 g_HWSR_MaskBit[HWSR_DEBUG0]
#define RT_BOX_PROJECTION                 g_HWSR_MaskBit[HWSR_SAMPLE3]
#define RT_CLIPVOLUME_ID                  g_HWSR_MaskBit[HWSR_LIGHTVOLUME0]

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CTexPoolAtlas::Init(int nSize)
{
	m_nSize = nSize;
	Clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CTexPoolAtlas::Clear()
{
	memset(&m_arrAllocatedBlocks[0], 0, MAX_BLOCKS * sizeof(m_arrAllocatedBlocks[0]));
#ifdef _DEBUG
	m_nTotalWaste = 0;
	m_arrDebugBlocks.clear();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CTexPoolAtlas::FreeMemory()
{
#ifdef _DEBUG
	stl::free_container(m_arrDebugBlocks);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CTexPoolAtlas::AllocateGroup(int32* pOffsetX, int32* pOffsetY, int nSizeX, int nSizeY)
{
	int nBorder = 2;
	nSizeX += nBorder << 1;
	nSizeY += nBorder << 1;

	if (nSizeX > m_nSize || nSizeY > m_nSize)
		return false;

	uint16 nBestX = 0;
	uint16 nBestY = 0;
	uint16 nBestID = 0;
	uint32 nBestWaste = ~0;

	// outer loop over all relevant allocated blocks (Y dimension)
	int nCurrY = 0;
	for (int nCurrBlockID = 0;
	     m_arrAllocatedBlocks[max(nCurrBlockID - 1, 0)] > 0 && nCurrY <= m_nSize - nSizeY && nBestWaste > 0;
	     ++nCurrBlockID)
	{
		const uint32 nCurrBlock = m_arrAllocatedBlocks[nCurrBlockID];
		const uint16 nCurrBlockWidth = LOWORD(nCurrBlock);
		const uint16 nCurrBlockHeight = HIWORD(nCurrBlock);

		// get max X for intersected blocks
		int nCurrX = nCurrBlockWidth;
		int nNextY = nCurrBlockHeight;
		for (uint16 nNextBlockID = nCurrBlockID + 1;
		     m_arrAllocatedBlocks[nNextBlockID] > 0 && nNextY < nSizeY;
		     ++nNextBlockID)
		{
			const uint32 nNextBlock = m_arrAllocatedBlocks[nNextBlockID];
			const uint16 nNextBlockWidth = LOWORD(nNextBlock);
			const uint16 nNextBlockHeight = HIWORD(nNextBlock);
			nCurrX = max(nCurrX, int(nNextBlockWidth));
			nNextY += nNextBlockHeight;
		}

		// can fit next to them?
		if (nSizeX <= m_nSize - nCurrX)
		{
			// compute waste
			uint32 nWaste = 0;
			nNextY = nCurrY;
			for (uint16 nNextBlockID = nCurrBlockID;
			     m_arrAllocatedBlocks[nNextBlockID] > 0 && nNextY < nCurrY + nSizeY;
			     ++nNextBlockID)
			{
				const uint32 nNextBlock = m_arrAllocatedBlocks[nNextBlockID];
				const uint16 nNextBlockWidth = LOWORD(nNextBlock);
				const uint16 nNextBlockHeight = HIWORD(nNextBlock);
				nWaste += (nCurrX - nNextBlockWidth) * (min(nCurrY + nSizeY, nNextY + nNextBlockHeight) - max(nCurrY, nNextY));
				nNextY += nNextBlockHeight;
			}
			nWaste += max(nCurrY + nSizeY - nNextY, 0) * nCurrX;

			// right spot?
			if (nWaste < nBestWaste)
			{
				nBestX = nCurrX;
				nBestY = nCurrY;
				nBestID = nCurrBlockID;
				nBestWaste = nWaste;
			}
		}

		nCurrY += nCurrBlockHeight;
	}

	if ((nBestX | nBestY) || nCurrY <= m_nSize - nSizeY)
	{
		assert(nBestID < MAX_BLOCKS - 1);
		if (nBestID >= MAX_BLOCKS - 1)
			return false;

		*pOffsetX = nBestX + nBorder;
		*pOffsetY = nBestY + nBorder;

		// block to be added, update block info
		uint32 nBlockData = m_arrAllocatedBlocks[nBestID];
		const uint16 nReplacedHeight = HIWORD(nBlockData);
		if (nSizeY < nReplacedHeight)
		{
			nBlockData = MAKELONG(nBlockData, nReplacedHeight - nSizeY);
			// shift by 1
			for (uint16 nID = nBestID + 1; nBlockData > 0; ++nID)
				std::swap(m_arrAllocatedBlocks[nID], nBlockData);
		}
		else if (nSizeY > nReplacedHeight)
		{
			uint16 nCoveredHeight = nReplacedHeight;
			uint16 nBlocksToSkip = 0;
			nBlockData = m_arrAllocatedBlocks[nBestID + 1];
			for (uint16 nID = nBestID + 1; nBlockData > 0; nBlockData = m_arrAllocatedBlocks[++nID])
			{
				const uint16 nCurrHeight = HIWORD(nBlockData);
				nCoveredHeight += nCurrHeight;
				if (nSizeY >= nCoveredHeight)
					nBlocksToSkip++;
				else
				{
					m_arrAllocatedBlocks[nID] = MAKELONG(nBlockData, nCoveredHeight - nSizeY);
					break;
				}
			}
			// shift by nBlocksToSkip
			uint16 nID = nBestID + nBlocksToSkip + 1;
			nBlockData = m_arrAllocatedBlocks[nID];
			for (; nBlockData > 0; nBlockData = m_arrAllocatedBlocks[++nID])
				m_arrAllocatedBlocks[nID - nBlocksToSkip] = nBlockData;
		}
		m_arrAllocatedBlocks[nBestID] = MAKELONG(nBestX + nSizeX, nSizeY);

#ifdef _DEBUG
		if (m_arrDebugBlocks.size())
			m_nTotalWaste += nBestWaste;
		_AddDebugBlock(nBestX, nBestY, nSizeX, nSizeY);
#endif

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
void CTexPoolAtlas::_AddDebugBlock(int x, int y, int sizeX, int sizeY)
{
	SShadowMapBlock block;
	block.m_nX1 = x;
	block.m_nX2 = x + sizeX;
	block.m_nY1 = y;
	block.m_nY2 = y + sizeY;
	assert(block.m_nX2 <= m_nSize && block.m_nY2 <= m_nSize);
	for (std::vector<SShadowMapBlock>::const_iterator it = m_arrDebugBlocks.begin();
	     it != m_arrDebugBlocks.end(); it++)
	{
		assert(!block.Intersects(*it));
	}
	m_arrDebugBlocks.push_back(block);
}

float CTexPoolAtlas::_GetDebugUsage() const
{
	uint32 nUsed = 0;
	for (std::vector<SShadowMapBlock>::const_iterator it = m_arrDebugBlocks.begin();
	     it != m_arrDebugBlocks.end(); it++)
	{
		nUsed += (it->m_nX2 - it->m_nX1) * (it->m_nY2 - it->m_nY1);
	}

	return 100.f * nUsed / (m_nSize * m_nSize);
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

RenderLightIndex CDeferredShading::AddLight(const SRenderLight& pDL, float fMult, const SRenderingPassInfo& passInfo)
{
	const uint32 nThreadID = gRenDev->GetMainThreadID();
	CRenderView* pRenderView = passInfo.GetRenderView();
	CRY_ASSERT(pRenderView);

	bool bSort = true;
	bool bAppend = true;

	eDeferredLightType LightType = eDLT_DeferredLight;
	if (pDL.m_Flags & DLF_DEFERRED_CUBEMAPS)
	{
		LightType = eDLT_DeferredCubemap;
		bSort = false;
	}
	else if (pDL.m_Flags & DLF_AMBIENT)
	{
		LightType = eDLT_DeferredAmbientLight;
		bSort = false;
	}

	if (pDL.GetLensOpticsElement() && !pDL.m_pSoftOccQuery)
	{
		SRenderLight* pLight = const_cast<SRenderLight*>(&pDL);

		const uint8 numVisibilityFaders = 2; // For each flare type
		pLight->m_pSoftOccQuery = new CFlareSoftOcclusionQuery(numVisibilityFaders);
	}

	SRenderLight* pAddedLight = nullptr;

	const RenderLightIndex lightsNum = pRenderView->GetLightsCount(LightType);
	RenderLightIndex nLightId = -1;

	if (CRenderer::CV_r_DeferredShadingSortLights == 1 && bSort && lightsNum)
	{
		RenderLightIndex endPoint = lightsNum - 1;
		RenderLightIndex midPoint = 0;
		RenderLightIndex startPoint = 0;

		uint32 lightArea = pDL.m_sWidth * pDL.m_sHeight;

		while (startPoint <= endPoint)  //binary search for insertion point
		{
			midPoint = (endPoint + startPoint) / 2;

			const SRenderLight& compareLight = pRenderView->GetLight(LightType, midPoint);
			uint32 compareLightArea = compareLight.m_sWidth * compareLight.m_sHeight;

			if (lightArea < compareLightArea)
			{
				endPoint = midPoint - 1;
			}
			else
			{
				startPoint = midPoint + 1;
			}
		}

		if (startPoint < lightsNum)
		{
			pAddedLight = pRenderView->AddLightAtIndex(LightType, pDL, startPoint);
			nLightId = startPoint /* => pAddedLight->m_Id */;
			bAppend = false;
		}
	}

	if (bAppend)
	{
		pAddedLight = pRenderView->AddLightAtIndex(LightType, pDL);
		nLightId = pAddedLight->m_Id;
	}

	if (pDL.m_Flags & DLF_LINK_TO_SKY_COLOR)
	{
	#if defined(FEATURE_SVO_GI)
		CSvoRenderer* pSR = CSvoRenderer::GetInstance();
		if (pSR)
		{
			pAddedLight->m_Color *= pSR->GetSkyColor();
		}
		else
	#endif

		{
			pAddedLight->m_Color *= gEnv->p3DEngine->GetSkyColor();
		}
	}

	IF_LIKELY ((pDL.m_Flags & (DLF_DEFERRED_CUBEMAPS | DLF_AMBIENT)) == 0)
	{
		pAddedLight->m_Color *= fMult;
		pAddedLight->m_SpecMult *= fMult;
	}
	else if (pDL.m_Flags & DLF_AMBIENT)
	{
		ColorF origCol(pAddedLight->m_Color);
		pAddedLight->m_Color.lerpFloat(Col_White, origCol, fMult);
	}
	else
	{
		pAddedLight->m_Color.a = fMult; // store fade-out constant separately in alpha channel for deferred cubemaps
	}

	gcpRendD3D->EF_CheckLightMaterial(const_cast<SRenderLight*>(&pDL), nLightId, passInfo);

	return nLightId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeferredShading::GetClipVolumeParams(const Vec4*& pParams, uint32& nCount)
{
	pParams = m_vClipVolumeParams;
	nCount = RenderView()->GetClipVolumes().size();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::ResetLights()
{
	uint32 nThreadID = gRenDev->GetMainThreadID();

	m_vecGIClipVolumes[nThreadID][0].resize(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::ReleaseData()
{
	auto* tiledLights = gcpRendD3D.GetGraphicsPipeline().GetTiledLightVolumesStage();

	tiledLights->Clear();

	for (uint32 iThread = 0; iThread < 2; ++iThread)
	{
		for (uint32 nRecurseLevel = 0; nRecurseLevel < MAX_REND_RECURSION_LEVELS; ++nRecurseLevel)
		{
			m_vecGIClipVolumes[iThread][nRecurseLevel].clear();
		}
	}

	m_shadowPoolAlloc.SetUse(0);
	stl::free_container(m_shadowPoolAlloc);

	m_blockPack.FreeContainers();

	m_nShadowPoolSize = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::AddGIClipVolume(IRenderMesh* pClipVolume, const Matrix34& mxTransform)
{
	uint32 nThreadID = gRenDev->GetMainThreadID();

	TArray<SClipShape>& vecGIClipVolumes = m_vecGIClipVolumes[nThreadID][0];
	vecGIClipVolumes.push_back(SClipShape(pClipVolume, mxTransform));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupPasses(CRenderView* pRenderView)
{
	m_nThreadID = gRenDev->GetRenderThreadID();

	gRenDev->m_RP.m_FlagsShader_RT &= ~(RT_LIGHTSMASK | RT_DEBUGMASK);

	m_pNormalsRT = CRendererResources::s_ptexSceneNormalsMap;
	m_pDepthRT = CRendererResources::s_ptexLinearDepth;
	m_pResolvedStencilRT = CRendererResources::s_ptexVelocity;
	m_pDiffuseRT = CRendererResources::s_ptexSceneDiffuse;
	m_pSpecularRT = CRendererResources::s_ptexSceneSpecular;
	m_pMSAAMaskRT = CRendererResources::s_ptexBackBuffer;

	const auto& viewInfo = RenderView()->GetViewInfo(CCamera::eEye_Left);


	m_pCamFront = viewInfo.cameraVZ;
	m_pCamFront.Normalize();
	m_pCamPos = viewInfo.cameraOrigin;

	m_fCamFar = viewInfo.farClipPlane;
	m_fCamNear = viewInfo.nearClipPlane;

	m_pView = viewInfo.viewMatrix;
	m_pView.Transpose();

	m_mViewProj = viewInfo.cameraProjMatrix;
	m_pViewProjI = m_mViewProj.GetInverted();

	m_mViewProj.Transpose();

	gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
	m_pShader = CShaderMan::s_shDeferredShading;

	//gcpRendD3D->SetCullMode(R_CULL_BACK);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest <= 1)
		m_nRenderState |= GS_NODEPTHTEST;
	else
		m_nRenderState &= ~GS_NODEPTHTEST;

	m_nCurTargetWidth = m_pDiffuseRT->GetWidth();
	m_nCurTargetHeight = m_pDiffuseRT->GetHeight();

	SetupGlobalConsts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupGlobalConsts()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	//set world basis
	float maskRTWidth = float(m_nCurTargetWidth);
	float maskRTHeight = float(m_nCurTargetHeight);
	Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
	Vec4 vParamValue, vMag;

	CShadowUtils::ProjectScreenToWorldExpansionBasis(rd->m_IdentityMatrix, RenderView()->GetCamera(CCamera::eEye_Left), RenderView()->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

	const auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;
	vWorldBasisX = vWBasisX / downscaleFactor.x;
	vWorldBasisY = vWBasisY / downscaleFactor.y;
	vWorldBasisZ = vWBasisZ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::FilterGBuffer()
{
	/*
	if (!CRenderer::CV_r_DeferredShadingFilterGBuffer)
	{
#if defined(DURANGO_USE_ESRAM)
		if (gcpRendD3D->m_nGraphicsPipeline == 0 || CRenderer::CV_r_ssdo == 0)
		{
			PostProcessUtils().StretchRect(CRendererResources::s_ptexSceneSpecularESRAM, CRendererResources::s_ptexSceneSpecular);
		}
#endif
		return;
	}

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	PROFILE_LABEL_SCOPE("GBUFFER_FILTER");

	static CCryNameTSCRC tech("FilterGBuffer");

#if defined(DURANGO_USE_ESRAM)
	CTexture* pSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#else
	PostProcessUtils().StretchRect(CRendererResources::s_ptexSceneSpecular, CRendererResources::s_ptexStereoR);
	CTexture* pSceneSpecular = CRendererResources::s_ptexStereoR;
#endif

	rd->FX_PushRenderTarget(0, CRendererResources::s_ptexSceneSpecular, NULL);
	SD3DPostEffectsUtils::ShBeginPass(m_pShader, tech, FEF_DONTSETSTATES);
	rd->FX_SetState(GS_NODEPTHTEST);
	SPostEffectsUtils::SetTexture(CRendererResources::s_ptexSceneNormalsMap, 0, FILTER_POINT, eSamplerAddressMode_Wrap);
	SPostEffectsUtils::SetTexture(pSceneSpecular, 1, FILTER_POINT, eSamplerAddressMode_Wrap);
	SPostEffectsUtils::SetTexture(CRendererResources::s_ptexZTarget, 2, FILTER_POINT, eSamplerAddressMode_Wrap);

	// Bind sampler directly so that it works with DX11 style texture objects
	ID3D11SamplerState* pSamplers[1] = { (ID3D11SamplerState*)CDeviceObjectFactory::LookupSamplerState(EDefaultSamplerStates::PointClamp).second };
	rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, pSamplers, 15, 1);

	SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexSceneSpecular->GetWidth(), CRendererResources::s_ptexSceneSpecular->GetHeight());
	SD3DPostEffectsUtils::ShEndPass();

	ID3D11SamplerState* pSampNull[1] = { NULL };
	rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, pSampNull, 15, 1);

	rd->FX_PopRenderTarget(0);
	*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeferredShading::DrawLightVolume(EShapeMeshType meshType, const Matrix44& mUnitVolumeToWorld, const Vec4& vSphereAdjust)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DrawDecalVolume(const SDeferredDecal& rDecal, Matrix44A& mDecalLightProj, ECull volumeCull)
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::DeferredDecalPass(const SDeferredDecal& rDecal, uint32 indDecal)
{
	ASSERT_LEGACY_PIPELINE
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::GetLightRenderSettings(const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = gRenDev->m_RP;

	const bool bAreaLight = (pDL->m_Flags & DLF_AREA_LIGHT) && pDL->m_fAreaWidth && pDL->m_fAreaHeight && pDL->m_fLightFrustumAngle;

	if (CRenderer::CV_r_deferredshadingLightVolumes)
	{
		if (bAreaLight)
		{
			// area lights use non-uniform box volume
			// need to do more complex box intersection test
			float fExpensionRadius = pDL->m_fRadius * 1.08f;
			Vec3 vScale(fExpensionRadius, fExpensionRadius, fExpensionRadius);

			Matrix34 mObjInv = CShadowUtils::GetAreaLightMatrix(pDL, vScale);
			mObjInv.Invert();

			// check if volumes bounding box intersects the near clipping plane
			const CCamera& camera = RenderView()->GetCamera(CCamera::eEye_Left);
			const Plane* pNearPlane(camera.GetFrustumPlane(FR_PLANE_NEAR));
			Vec3 pntOnNearPlane(camera.GetPosition() - pNearPlane->DistFromPlane(camera.GetPosition()) * pNearPlane->n);
			Vec3 pntOnNearPlaneOS(mObjInv.TransformPoint(pntOnNearPlane));

			Vec3 nearPlaneOS_n(mObjInv.TransformVector(pNearPlane->n));
			f32 nearPlaneOS_d(-nearPlaneOS_n.Dot(pntOnNearPlaneOS));

			// get extreme lengths
			float t(fabsf(nearPlaneOS_n.x) + fabsf(nearPlaneOS_n.y) + fabsf(nearPlaneOS_n.z));

			float t0 = t + nearPlaneOS_d;
			float t1 = -t + nearPlaneOS_d;

			if (t0 * t1 > 0.0f)
				bUseLightVolumes = true;
			else
				bStencilMask = true;
		}
		else
		{
			const float kDLRadius = pDL->m_fRadius;
			const float fSmallLightBias = 0.5f;
			// the light mesh tessellation and near clipping plane require some bias when testing if inside sphere
			// higher bias for low radius lights
			float fSqLightRadius = kDLRadius * max(-0.1f * kDLRadius + 1.5f, 1.22f);
			fSqLightRadius = max(kDLRadius + fSmallLightBias, fSqLightRadius); //always add on a minimum bias, for very small light's sake
			fSqLightRadius *= fSqLightRadius;
			if (fSqLightRadius < pDL->m_Origin.GetSquaredDistance(m_pCamPos))
				bUseLightVolumes = true;
			else
				bStencilMask = true;
		}
	}

	Vec4 pLightRect = Vec4(pDL->m_sX, pDL->m_sY, pDL->m_sWidth, pDL->m_sHeight);

	float fCurTargetWidth = (float)(m_nCurTargetWidth);
	float fCurTargetHeight = (float)(m_nCurTargetHeight);

	if (!iszero(CRenderer::CV_r_DeferredShadingLightLodRatio))
	{
		if (CRenderer::CV_r_DeferredShadingLightStencilRatio > 0.01f)
		{
			const float fLightLodRatioScale = CRenderer::CV_r_DeferredShadingLightLodRatio;
			float fLightArea = pLightRect.z * pLightRect.w;
			float fScreenArea = fCurTargetHeight * fCurTargetWidth;
			float fLightRatio = fLightLodRatioScale * (fLightArea / fScreenArea);

			const float fDrawVolumeThres = 0.005f;
			if (fLightRatio < fDrawVolumeThres)
			{
				bUseLightVolumes = false;
			}

			if (fLightRatio > 4 * CRenderer::CV_r_DeferredShadingLightStencilRatio)
				meshType = SHAPE_PROJECTOR2;
			else if (fLightRatio > 2 * CRenderer::CV_r_DeferredShadingLightStencilRatio)
				meshType = SHAPE_PROJECTOR1;
		}
		else
		{
			const float fLightLodRatioScale = CRenderer::CV_r_DeferredShadingLightLodRatio;
			float fLightArea = pLightRect.z * pLightRect.w;
			float fScreenArea = fCurTargetHeight * fCurTargetWidth;
			float fLightRatio = fLightLodRatioScale * (fLightArea / fScreenArea);

			const float fDrawVolumeThres = 0.005f;
			if (fLightRatio < fDrawVolumeThres)
			{
				bUseLightVolumes = false;
			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::AmbientPass(const SRenderLight* pGlobalCubemap, bool& bOutdoorVisible)
{
	ASSERT_LEGACY_PIPELINE


	return true;
}

void CDeferredShading::DeferredShadingPass()
{
	ASSERT_LEGACY_PIPELINE
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CDeferredShading::DeferredLights(CRenderView *pRenderView,RenderLightsList& rLights, bool bCastShadows)
{
	if (rLights.size())
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTS");

		if (bCastShadows)
		{
			PackAllShadowFrustums(false);
		}
	}
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::PackAllShadowFrustums(CRenderView *pRenderView)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRY_ASSERT(rd->m_pRT->IsRenderThread());

	const uint64 nPrevFlagsShaderRT = gRenDev->m_RP.m_FlagsShader_RT;

	static ICVar* p_e_ShadowsPoolSize = iConsole->GetCVar("e_ShadowsPoolSize");

	RenderLightsList& arrLights = m_pCurrentRenderView->GetLightsArray(eDLT_DeferredLight);

#if defined(ENABLE_PROFILING_CODE)
	SRenderStatistics::Write().m_NumShadowPoolFrustums = 0;
	SRenderStatistics::Write().m_NumShadowPoolAllocsThisFrame = 0;
	SRenderStatistics::Write().m_NumShadowMaskChannels = 0;
#endif

	int nRequestedPoolSize = p_e_ShadowsPoolSize->GetIVal();
	if (m_nShadowPoolSize != nRequestedPoolSize)
	{
		m_blockPack.UpdateSize(nRequestedPoolSize >> TEX_POOL_BLOCKLOGSIZE, nRequestedPoolSize >> TEX_POOL_BLOCKLOGSIZE);
		m_nShadowPoolSize = nRequestedPoolSize;

		// clear pool and reset allocations
		m_blockPack.Clear();
		m_shadowPoolAlloc.SetUse(0);
	}

	
	m_bClearPool = (CRenderer::CV_r_ShadowPoolMaxFrames > 0) ? false : true;

	m_nFirstShadowPoolLight = 0;
	m_nLastShadowPoolLight = 0;

	auto nLightId = 0;
	auto itrStop = arrLights.end();
	auto itrCurr = arrLights.begin();


	// TODO: Sort lights so allocated ones are rendered first to reduce impact of a thrash

	for (; itrCurr != itrStop; ++itrCurr) //pre-loop to avoid 0.5 ms restore/resolve
	{
		const SRenderLight& rLight = *itrCurr;
		if (!(rLight.m_Flags & DLF_DIRECTIONAL) && (rLight.m_Flags & DLF_CASTSHADOW_MAPS))
			break;

		++m_nFirstShadowPoolLight;
	}

	if (itrCurr != itrStop) // Shadow allocation tick, free old shadows.
	{
		uint32 nAllocs = m_shadowPoolAlloc.Num();
		for (uint32 i = 0; i < nAllocs; i++)
		{
			SShadowAllocData* pAlloc = &m_shadowPoolAlloc[i];
			uint32 currFrame = RenderView()->GetFrameId() & 0xFF;
			if (!pAlloc->isFree() && ((currFrame - pAlloc->m_frameID) > (uint)CRenderer::CV_r_ShadowPoolMaxFrames))
			{
				m_blockPack.RemoveBlock(pAlloc->m_blockID);
				pAlloc->Clear();
				break;  //Max one delete per frame, this should spread updates across more frames
			}
		}
	}

	 m_nLastShadowPoolLight = m_nFirstShadowPoolLight;

	// init before shadowgen
	SetupPasses(pRenderView);

	{
		while (itrCurr != itrStop)
		{
			SRenderLight& rLight = *itrCurr;
			if (!(rLight.m_Flags & (DLF_DIRECTIONAL | DLF_FAKE)) && (rLight.m_Flags & DLF_CASTSHADOW_MAPS))
			{

				bool bPacked = PackToPool(&m_blockPack, rLight, m_nLastShadowPoolLight, m_nFirstShadowPoolLight, m_bClearPool);
				m_bClearPool = !bPacked;
				if (!bPacked)
				{
					break;
				}
			}

			++m_nLastShadowPoolLight;
			++itrCurr;
		}
	}

#if defined(ENABLE_PROFILING_CODE)
	SRenderStatistics::Write().m_NumShadowPoolFrustums += m_shadowPoolAlloc.Num();
#endif
	

	gRenDev->m_RP.m_FlagsShader_RT = nPrevFlagsShaderRT;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::PackToPool(CPowerOf2BlockPacker* pBlockPack, SRenderLight& light, const int nLightID, const int nFirstCandidateLight, bool bClearPool)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRenderView* pRenderView = RenderView();

	//////////////////////////////////////////////////////////////////////////
	int nDLights = RenderView()->GetDynamicLightsCount();

	//////////////////////////////////////////////////////////////////////////
	int nFrustumIdx = nLightID + nDLights;
	auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nFrustumIdx);

	int nUpdatedThisFrame = 0;

	assert((unsigned int) nFrustumIdx < (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS));
	if ((unsigned int) nFrustumIdx >= (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS))
	{
		int nFrameID = pRenderView->GetFrameId();
		if (m_nWarningFrame != nFrameID)
		{
			Warning("CDeferredShading::ShadowLightPasses: Too many light sources used ...");
			m_nWarningFrame = nFrameID;
		}
		//reset castshadow flag for further processing
		light.m_Flags &= ~DLF_CASTSHADOW_MAPS;
		return true;
	}
	//////////////////////////////////////////////////////////////////////////

	//no single frustum was allocated for this light
	if (SMFrustums.empty())
	{
		//reset castshadow flag for further processing
		light.m_Flags &= ~DLF_CASTSHADOW_MAPS;
		return true;
	}

	if (pRenderView->IsRecursive())
	{
		//reset castshadow flag for further processing
		light.m_Flags &= ~DLF_CASTSHADOW_MAPS;
		return true;
	}

	ShadowMapFrustum& firstFrustum = *SMFrustums.front()->pFrustum;

	//////////////////////////////////////////////////////////////////////////
	int nBlockW = firstFrustum.nTexSize >> TEX_POOL_BLOCKLOGSIZE;
	int nBlockH = firstFrustum.nTexSize >> TEX_POOL_BLOCKLOGSIZE;
	int nLogBlockW = IntegerLog2((uint32)nBlockW);
	int nLogBlockH = nLogBlockW;

	bool bNeedsUpdate = false;

	if (bClearPool)
	{
		pBlockPack->Clear();
		m_shadowPoolAlloc.SetUse(0);
	}

	uint32 currFrame = pRenderView->GetFrameId() & 0xFF;

	uint32 lightID = light.m_nEntityId;

	assert(lightID != (uint32) - 1);

	uint SidesNum = firstFrustum.bUnwrapedOmniDirectional ? OMNI_SIDES_NUM : 1;
	uint nUpdateMask = firstFrustum.bUnwrapedOmniDirectional ? 0x3F : 0x1;

	for (uint nSide = 0; nSide < SidesNum; nSide++)
	{
		bNeedsUpdate = false;
		uint nX1, nX2, nY1, nY2;

		//Find block allocation info (alternative: store in frustum data, but this does not persist)
		bool bFoundAlloc = false;
#if CRY_PLATFORM_WINDOWS
		int nMGPUUpdate = -1;
#endif
		uint32 nAllocs = m_shadowPoolAlloc.Num();
		SShadowAllocData* pAlloc = NULL;
		for (uint32 i = 0; i < nAllocs; i++)
		{
			pAlloc = &m_shadowPoolAlloc[i];

			if (pAlloc->m_lightID == lightID && pAlloc->m_side == nSide)
			{
				bFoundAlloc = true;
				break;
			}
		}

		if (bFoundAlloc)
		{
			uint32 nID = pBlockPack->GetBlockInfo(pAlloc->m_blockID, nX1, nY1, nX2, nY2);

			int nFrameCompare = (currFrame - pAlloc->m_frameID) % 256;

			if (nID == 0xFFFFFFFF)
			{
				bNeedsUpdate = true;
			}
			else
			{
				if (firstFrustum.nShadowPoolUpdateRate == 0)  // forced update, always do this
				{
					bNeedsUpdate = true;
				}
				else if (firstFrustum.nShadowPoolUpdateRate < (uint8) nFrameCompare)
				{
					if (nUpdatedThisFrame < CRenderer::CV_r_ShadowPoolMaxTimeslicedUpdatesPerFrame)
					{
						bNeedsUpdate = true;
						nUpdatedThisFrame++;
					}
				}
#if CRY_PLATFORM_WINDOWS // AFR support
				else if (rd->GetActiveGPUCount() > 1)
				{
					if (gRenDev->GetActiveGPUCount() > nFrameCompare)
					{
						bNeedsUpdate = true;
						nMGPUUpdate = pAlloc->m_frameID;
					}
				}
#endif
			}

			if (!bNeedsUpdate)
			{
				if (nX1 != 0xFFFFFFFF && nBlockW == (nX2 - nX1))   // ignore Y, is square
				{
					pBlockPack->GetBlockInfo(nID, nX1, nY1, nX2, nY2);
					firstFrustum.packX[nSide] = nX1 << TEX_POOL_BLOCKLOGSIZE;
					firstFrustum.packY[nSide] = nY1 << TEX_POOL_BLOCKLOGSIZE;
					firstFrustum.packWidth[nSide] = (nX2 - nX1) << TEX_POOL_BLOCKLOGSIZE;
					firstFrustum.packHeight[nSide] = (nY2 - nY1) << TEX_POOL_BLOCKLOGSIZE;
					firstFrustum.nShadowGenID[m_nThreadID][nSide] = 0xFFFFFFFF; // turn off shadow gen for this side

					nUpdateMask &= ~(1 << nSide);
					continue; // All currently valid, skip
				}
			}

			if (nID != 0xFFFFFFFF && nX1 != 0xFFFFFFFF) // Valid block, realloc
			{
				pBlockPack->RemoveBlock(nID);
				pAlloc->Clear();
			}
		}

		uint32 nID = pBlockPack->AddBlock(nLogBlockW, nLogBlockH);
		bool isAllocated = (nID != 0xFFFFFFFF) ? true : false;

#if defined(ENABLE_PROFILING_CODE)
		SRenderStatistics::Write().m_NumShadowPoolAllocsThisFrame++;
#endif

		if (isAllocated)
		{
			bNeedsUpdate = true;

			if (!bFoundAlloc)
			{
				pAlloc = NULL;
				nAllocs = m_shadowPoolAlloc.Num();
				for (uint32 i = 0; i < nAllocs; i++)
				{
					if (m_shadowPoolAlloc[i].isFree())
					{
						pAlloc = &m_shadowPoolAlloc[i];
						break;
					}
				}

				if (!pAlloc)
					pAlloc = m_shadowPoolAlloc.AddIndex(1);
			}

			pAlloc->m_blockID = nID;
			pAlloc->m_lightID = lightID;
			pAlloc->m_side = nSide;
#if CRY_PLATFORM_WINDOWS
			pAlloc->m_frameID = (nMGPUUpdate == -1) ? pRenderView->GetFrameId() & 0xFF : nMGPUUpdate;
#else
			pAlloc->m_frameID = pRenderView->GetFrameId() & 0xFF;
#endif
			bClearPool = true;
		}
		else
		{

#if defined(ENABLE_PROFILING_CODE)
			if (CRenderer::CV_r_ShadowPoolMaxFrames != 0 || CRenderer::CV_r_DeferredShadingTiled > 1)
				SRenderStatistics::Write().m_NumShadowPoolAllocsThisFrame |= 0x80000000;
#endif

			return false;
		}

		pBlockPack->GetBlockInfo(nID, nX1, nY1, nX2, nY2);
		firstFrustum.packX[nSide] = nX1 << TEX_POOL_BLOCKLOGSIZE;
		firstFrustum.packY[nSide] = nY1 << TEX_POOL_BLOCKLOGSIZE;
		firstFrustum.packWidth[nSide] = (nX2 - nX1) << TEX_POOL_BLOCKLOGSIZE;
		firstFrustum.packHeight[nSide] = (nY2 - nY1) << TEX_POOL_BLOCKLOGSIZE;

	}

	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//  next step is to use these values in shadowgen

	//////////////////////////////////////////////////////////////////////////

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::ResolveCurrentBuffers()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	PROFILE_LABEL_SCOPE("FLUSH_RESOLVE");
	//rd->FX_PopRenderTarget(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::RestoreCurrentBuffers()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	//rd->FX_PushRenderTarget(1, m_pLBufferSpecularRT, NULL, -1, false, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct CubemapsCompare
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		// Cubes sort by: Sort priority first, light radius, lastly by entity id (insertion order every frame is not guaranteed)
		if (l0.m_nSortPriority != l1.m_nSortPriority)
			return l0.m_nSortPriority < l1.m_nSortPriority;

		if (l0.m_fRadius == l1.m_fRadius)
			return l0.m_nEntityId < l1.m_nEntityId;

		return l0.m_fRadius > l1.m_fRadius;
	}
};

struct CubemapsCompareInv
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		// Cubes sort by: Sort priority first, light radius, lastly by entity id (insertion order every frame is not guaranteed)
		if (l0.m_nSortPriority != l1.m_nSortPriority)
			return l0.m_nSortPriority > l1.m_nSortPriority;

		if (l0.m_fRadius == l1.m_fRadius)
			return l0.m_nEntityId > l1.m_nEntityId;

		return l0.m_fRadius < l1.m_fRadius;
	}
};

struct LightsCompare
{
	bool operator()(const SRenderLight& l0, const SRenderLight& l1) const
	{
		if (!(l0.m_Flags & DLF_CASTSHADOW_MAPS) && (l1.m_Flags & DLF_CASTSHADOW_MAPS))
		{
			return true;
		}
		else
			return false;
	}
};

struct DeffDecalSort
{
	bool operator()(const SDeferredDecal& decal0, const SDeferredDecal& decal1) const
	{
		uint bBump0 = (decal0.nFlags & DECAL_HAS_NORMAL_MAP);
		uint bBump1 = (decal1.nFlags & DECAL_HAS_NORMAL_MAP);
		//bump-mapped decals first
		if (bBump0 != bBump1)
			return (bBump0 < bBump1);
		return
		  (decal0.nSortOrder < decal1.nSortOrder);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::Render(CRenderView* pRenderView)
{
	/*
	m_pCurrentRenderView = pRenderView;

	m_nLightsProcessedCount = 0;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	uint64 nFlagsShaderRT = gRenDev->m_RP.m_FlagsShader_RT;

	rd->FX_ResetPipe();

	SetupPasses();

	auto& rDeferredLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	auto& rDeferredCubemaps = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	auto& rDeferredAmbientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

	if (CRenderer::CV_r_DeferredShadingScissor)
		rd->EF_Scissor(false, 0, 0, 0, 0);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.f, DBT_SKY_CULL_DEPTH, true); // skip sky for ambient and deferred cubemaps

	int iTempX, iTempY, iWidth, iHeight;
	rd->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	bool bOutdoorVisible = false;
	PrepareClipVolumeData(bOutdoorVisible);

	FilterGBuffer();

	if (rd->m_nGraphicsPipeline == 0)
	{
		PackAllShadowFrustums(true);
	}

	rd->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ZERO);

	// sort lights
	if (rDeferredCubemaps.size())
	{
		// does not invalidate references or iterators, but invalidates all m_Id in the lights
		if (CRenderer::CV_r_DeferredShadingTiled <= 1)
			rDeferredCubemaps.sort(CubemapsCompare());
		else
			rDeferredCubemaps.sort(CubemapsCompareInv());
	}

	// Process deferred direct lighting accumulation
	rd->FX_PushRenderTarget(0, m_pLBufferDiffuseRT, &gcpRendD3D->m_DepthBufferOrig, -1, false, 1);
	rd->FX_PushRenderTarget(1, m_pLBufferSpecularRT, NULL, -1, false, 1);
	m_bSpecularState = true;

	if (gRenDev->GetWireframeMode())
	{
		rd->FX_ClearTarget(m_pLBufferDiffuseRT);
		rd->FX_ClearTarget(m_pLBufferSpecularRT);
		rd->FX_ClearTarget(&gcpRendD3D->m_DepthBufferOrig, CLEAR_STENCIL, Clr_Unused.r, 1);
		rd->m_nStencilMaskRef = 1;// Stencil initialized to 1 - 0 is reserved for MSAAed samples
	}

	rd->RT_SetViewport(0, 0, gRenDev->GetWidth(), gRenDev->GetHeight());

	uint32 nCurrentDeferredCubemap = 0;

	bool bTiledDeferredShading = CRenderer::CV_r_DeferredShadingTiled >= 2;

	if (CRenderer::CV_r_DeferredShadingDebug == 2)
	{
		gRenDev->m_RP.m_FlagsShader_RT |= RT_OVERDRAW_DEBUG;
		bTiledDeferredShading = false;
	}
	if (CRenderer::CV_r_Unlit)
	{
		bTiledDeferredShading = false;
	}

	// determine if we have a global cubemap in the scene
	SRenderLight* pGlobalCubemap = NULL;
	if (rDeferredCubemaps.size() && CRenderer::CV_r_DeferredShadingEnvProbes)
	{
		SRenderLight& pFirstLight = *rDeferredCubemaps.begin();
		ITexture* pDiffuseCubeCheck = pFirstLight.GetDiffuseCubemap();
		float fRadius = pFirstLight.m_fRadius;

		if (pDiffuseCubeCheck && fRadius >= 100000.f)
		{
			pGlobalCubemap = &pFirstLight;
			++nCurrentDeferredCubemap;
		}
	}

	if (!bTiledDeferredShading)
	{
		if (CRenderer::CV_r_DeferredShadingAmbient && AmbientPass(pGlobalCubemap, bOutdoorVisible))
			m_nLightsProcessedCount++;



		if (CRenderer::CV_r_DeferredShadingAmbientLights)
			DeferredLights(rDeferredAmbientLights, false);
	}

	SpecularAccEnableMRT(true);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, 1.0f, false);

	if (CRenderer::CV_r_DeferredShadingLights && !bTiledDeferredShading)
		DeferredLights(rDeferredLights, true);

	if (CRenderer::CV_r_DeferredShadingScissor)
		rd->EF_Scissor(false, 0, 0, 0, 0);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, 1.0f, false);

	if (CRenderer::CV_r_Unlit)
	{
		rd->FX_ClearTarget(m_pLBufferDiffuseRT, Clr_MedianHalf);
		rd->FX_ClearTarget(m_pLBufferSpecularRT, Clr_Transparent);
		rd->FX_ClearTarget(&gcpRendD3D->m_DepthBufferOrig, CLEAR_STENCIL, Clr_Unused.r, 0);
	}

	// Commit any potential render target changes - required for x360 resolves, do not remove this plz.
	rd->FX_SetActiveRenderTargets();

	rd->FX_PopRenderTarget(0);
	rd->FX_PopRenderTarget(1);
	m_bSpecularState = false;


	if (rd->m_nGraphicsPipeline > 0)
	{
		auto* pWaterStage = rd->GetGraphicsPipeline().GetWaterStage();
		if (pWaterStage && !bTiledDeferredShading)
		{
			// NOTE: when tiled deferred shading is enabled, this function is called in CWaterStage::ExecuteWaterVolumeCaustics().
			pWaterStage->ExecuteDeferredWaterVolumeCaustics(bTiledDeferredShading);
		}
	}

	if (bTiledDeferredShading)
	{
		gcpRendD3D->GetTiledShading().Render(pRenderView, m_vClipVolumeParams);
	}
	else
	{
		// GPU light culling
		if (CRenderer::CV_r_DeferredShadingTiled == 1)
		{
			// Sort cubemaps in inverse order for tiled forward shading
			// does not invalidate references or iterators, but invalidates all m_Id in the lights
			rDeferredCubemaps.sort(CubemapsCompareInv());

			gcpRendD3D->GetTiledShading().Render(pRenderView, m_vClipVolumeParams);
		}

		DeferredShadingPass();
	}

	rd->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

#ifndef _RELEASE
	if (CRenderer::CV_r_DeferredShadingDebugGBuffer)
		DebugGBuffer();
#endif

	// Commit any potential render target changes - required for when shadows disabled
	rd->FX_SetActiveRenderTargets();

	gRenDev->m_RP.m_FlagsShader_RT = nFlagsShaderRT;
	gRenDev->m_RP.m_PersFlags2 &= ~RBPF2_WRITEMASK_RESERVED_STENCIL_BIT;

	rd->FX_ResetPipe();
	*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::Release()
{
	CRendererResources::DestroyDeferredMaps();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DebugGBuffer()
{
	ASSERT_LEGACY_PIPELINE
}
////////////////////////////////////////////////////////////////////////////////////////////////////

int CRenderer::EF_AddDeferredLight(const SRenderLight& pLight, float fMult, const SRenderingPassInfo& passInfo)
{
	CDeferredShading& pDS = CDeferredShading::Instance();
	RenderLightIndex nLightID = pDS.AddLight(pLight, fMult, passInfo);

	int nThreadID = gRenDev->GetMainFrameID();
	float mipFactor = (passInfo.GetCamera().GetPosition() - pLight.m_Origin).GetLengthSquared() / max(0.001f, pLight.m_fRadius * pLight.m_fRadius);
	EF_PrecacheResource(const_cast<SRenderLight*>(&pLight), mipFactor, 0.1f, FPR_STARTLOADING, gRenDev->GetStreamZoneRoundId(1));
	return nLightID;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRenderer::EF_ReleaseDeferredData()
{
	if (CDeferredShading::IsValid())
		CDeferredShading::Instance().ReleaseData();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CRenderer::Ef_AddDeferredGIClipVolume(const IRenderMesh* pClipVolume, const Matrix34& mxTransform)
{
	if (pClipVolume)
		CDeferredShading::Instance().AddGIClipVolume(const_cast<IRenderMesh*>(pClipVolume), mxTransform);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/*
bool CD3D9Renderer::FX_DeferredRendering(CRenderView* pRenderView, bool bDebugPass, bool bUpdateRTOnly)
{
	CDeferredShading& pDS = CDeferredShading::Instance();

	if (!CRendererResources::s_ptexSceneTarget)
	{
		pDS.Release();
		return false;
	}

	if (bUpdateRTOnly)
	{
		pDS.CreateDeferredMaps();
		return true;
	}

	if (!bDebugPass)
		pDS.Render(pRenderView);
	else
		pDS.Debug();

	return true;
}
*/