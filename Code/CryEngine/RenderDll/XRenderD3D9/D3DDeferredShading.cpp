// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "../Common/Textures/TextureManager.h"
#include "../Common/Textures/TextureHelpers.h"
#include "../Common/RendElements/FlareSoftOcclusionQuery.h"
#include "../Common/ReverseDepth.h"
#include "D3DTiledShading.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/DeferredDecals.h"
#include "GraphicsPipeline/ShadowMask.h"
#include "GraphicsPipeline/Water.h"
#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#include "Common/RenderView.h"

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

RenderLightIndex CDeferredShading::AddLight(const CDLight& pDL, float fMult, const SRenderingPassInfo& passInfo)
{
	const uint32 nThreadID = gcpRendD3D->m_RP.m_nFillThreadID;

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
		CDLight* pLight = const_cast<CDLight*>(&pDL);

		const uint8 numVisibilityFaders = 2; // For each flare type
		pLight->m_pSoftOccQuery = new CFlareSoftOcclusionQuery(numVisibilityFaders);
	}

	SRenderLight* pAddedLight = nullptr;

	const RenderLightIndex lightsNum = passInfo.GetRenderView()->GetLightsCount(LightType);
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

			const SRenderLight& compareLight = passInfo.GetRenderView()->GetLight(LightType, midPoint);
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
			pAddedLight = passInfo.GetRenderView()->AddLightAtIndex(LightType, pDL, startPoint);
			nLightId = startPoint /* => pAddedLight->m_Id */;
			bAppend = false;
		}
	}

	if (bAppend)
	{
		pAddedLight = passInfo.GetRenderView()->AddLightAtIndex(LightType, pDL);
		nLightId = pAddedLight->m_Id;
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

	gcpRendD3D->EF_CheckLightMaterial(const_cast<CDLight*>(&pDL), nLightId, passInfo);

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
	uint32 nThreadID = gcpRendD3D->m_RP.m_nFillThreadID;

	m_vecGIClipVolumes[nThreadID][0].resize(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::ReleaseData()
{
	gcpRendD3D->GetTiledShading().Clear();

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
	uint32 nThreadID = gRenDev->m_RP.m_nFillThreadID;

	TArray<SClipShape>& vecGIClipVolumes = m_vecGIClipVolumes[nThreadID][0];
	vecGIClipVolumes.push_back(SClipShape(pClipVolume, mxTransform));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SpecularAccEnableMRT(bool bEnable)
{
	assert(m_pLBufferSpecularRT);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (bEnable && !m_bSpecularState)
	{
		rd->FX_PushRenderTarget(1, m_pLBufferSpecularRT, NULL, -1, false, 1);
		m_bSpecularState = true;
	}
	else if (!bEnable && m_bSpecularState)
	{
		m_pLBufferSpecularRT->SetResolved(true);
		rd->FX_PopRenderTarget(1);
		m_bSpecularState = false;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupPasses()
{
	CreateDeferredMaps();

	m_nThreadID = gcpRendD3D->m_RP.m_nProcessThreadID;

	gcpRendD3D->m_RP.m_FlagsShader_RT &= ~(RT_LIGHTSMASK | RT_DEBUGMASK);

	gcpRendD3D->FX_SetMSAAFlagsRT();

	m_pLBufferDiffuseRT = CTexture::s_ptexCurrentSceneDiffuseAccMap;
	m_pLBufferSpecularRT = CTexture::s_ptexSceneSpecularAccMap;
	m_pNormalsRT = CTexture::s_ptexSceneNormalsMap;
	m_pDepthRT = CTexture::s_ptexZTarget;
	m_pResolvedStencilRT = CTexture::s_ptexVelocity;
	m_pDiffuseRT = CTexture::s_ptexSceneDiffuse;
	m_pSpecularRT = CTexture::s_ptexSceneSpecular;
	m_pMSAAMaskRT = CTexture::s_ptexBackBuffer;

	CRenderCamera* pCam = &gcpRendD3D->m_RP.m_TI[m_nThreadID].m_rcam;
	m_pCamFront = pCam->vZ;
	m_pCamFront.Normalize();
	m_pCamPos = pCam->vOrigin;

	m_fCamFar = pCam->fFar;
	m_fCamNear = pCam->fNear;

	m_fRatioWidth = (float)m_pLBufferDiffuseRT->GetWidth() / (float)CTexture::s_ptexSceneTarget->GetWidth();
	m_fRatioHeight = (float)m_pLBufferDiffuseRT->GetHeight() / (float)CTexture::s_ptexSceneTarget->GetHeight();

	m_pView = gcpRendD3D->m_CameraMatrix;
	m_pView.Transpose();

	m_mViewProj = gcpRendD3D->m_CameraProjMatrix;
	m_mViewProj.Transpose();

	m_pViewProjI = gcpRendD3D->m_CameraProjMatrix.GetInverted();

	gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
	m_pShader = CShaderMan::s_shDeferredShading;

	gcpRendD3D->SetCullMode(R_CULL_BACK);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest <= 1)
		m_nRenderState |= GS_NODEPTHTEST;
	else
		m_nRenderState &= ~GS_NODEPTHTEST;

	if (CRenderer::CV_r_DeferredShadingDebug == 2)
	{
		gcpRendD3D->m_RP.m_FlagsShader_RT |= RT_OVERDRAW_DEBUG;
	}

	m_nCurTargetWidth = m_pLBufferDiffuseRT->GetWidth();
	m_nCurTargetHeight = m_pLBufferDiffuseRT->GetHeight();

	// Verify if sun present in non-deferred light list (usually fairly small list)
	gcpRendD3D->m_RP.m_pSunLight = NULL;

	if (CRenderView* pView = RenderView())
	{
		for (uint32 i = 0; i < pView->GetDynamicLightsCount(); i++)
		{
			SRenderLight* dl = &pView->GetDynamicLight(i);
			if (dl->m_Flags & DLF_SUN)
			{
				gcpRendD3D->m_RP.m_pSunLight = dl;
				break;
			}
		}
	}

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
	CShadowUtils::ProjectScreenToWorldExpansionBasis(rd->m_IdentityMatrix, rd->GetCamera(), rd->m_vProjMatrixSubPixoffset, maskRTWidth, maskRTHeight, vWBasisX, vWBasisY, vWBasisZ, vCamPos, true, NULL);

	vWorldBasisX = vWBasisX / rd->m_RP.m_CurDownscaleFactor.x;
	vWorldBasisY = vWBasisY / rd->m_RP.m_CurDownscaleFactor.y;
	vWorldBasisZ = vWBasisZ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::FilterGBuffer()
{
	if (!CRenderer::CV_r_DeferredShadingFilterGBuffer)
	{
#if defined(DURANGO_USE_ESRAM)
		if (gcpRendD3D->m_nGraphicsPipeline == 0 || CRenderer::CV_r_ssdo == 0)
		{
			PostProcessUtils().StretchRect(CTexture::s_ptexSceneSpecularESRAM, CTexture::s_ptexSceneSpecular);
		}
#endif
		return;
	}

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	PROFILE_LABEL_SCOPE("GBUFFER_FILTER");

	static CCryNameTSCRC tech("FilterGBuffer");

#if defined(DURANGO_USE_ESRAM)
	CTexture* pSceneSpecular = CTexture::s_ptexSceneSpecularESRAM;
#else
	PostProcessUtils().StretchRect(CTexture::s_ptexSceneSpecular, CTexture::s_ptexStereoR);
	CTexture* pSceneSpecular = CTexture::s_ptexStereoR;
#endif

	rd->FX_PushRenderTarget(0, CTexture::s_ptexSceneSpecular, NULL);
	SD3DPostEffectsUtils::ShBeginPass(m_pShader, tech, FEF_DONTSETSTATES);
	rd->FX_SetState(GS_NODEPTHTEST);
	SPostEffectsUtils::SetTexture(CTexture::s_ptexSceneNormalsMap, 0, FILTER_POINT, eSamplerAddressMode_Wrap);
	SPostEffectsUtils::SetTexture(pSceneSpecular, 1, FILTER_POINT, eSamplerAddressMode_Wrap);
	SPostEffectsUtils::SetTexture(CTexture::s_ptexZTarget, 2, FILTER_POINT, eSamplerAddressMode_Wrap);

	// Bind sampler directly so that it works with DX11 style texture objects
	ID3D11SamplerState* pSamplers[1] = { (ID3D11SamplerState*)CDeviceObjectFactory::LookupSamplerState(EDefaultSamplerStates::PointClamp).second };
	rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, pSamplers, 15, 1);

	SD3DPostEffectsUtils::DrawFullScreenTri(CTexture::s_ptexSceneSpecular->GetWidth(), CTexture::s_ptexSceneSpecular->GetHeight());
	SD3DPostEffectsUtils::ShEndPass();

	ID3D11SamplerState* pSampNull[1] = { NULL };
	rd->m_DevMan.BindSampler(CSubmissionQueue_DX11::TYPE_PS, pSampNull, 15, 1);
	rd->FX_Commit();

	rd->FX_PopRenderTarget(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeferredShading::DrawLightVolume(EShapeMeshType meshType, const Matrix44& mUnitVolumeToWorld, const Vec4& vSphereAdjust)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	float maskRTWidth = (float) m_nCurTargetWidth;
	float maskRTHeight = (float) m_nCurTargetHeight;

	Vec4 vScreenScale(1.0f / maskRTWidth, 1.0f / maskRTHeight,
	                  0.0f, 0.0f);

	{
		static CCryNameR paramName("g_ScreenScale");
		m_pShader->FXSetPSFloat(paramName, &vScreenScale, 1);
	}

	{
		static CCryNameR paramName("vWBasisX");
		m_pShader->FXSetPSFloat(paramName, &vWorldBasisX, 1);
	}

	{
		static CCryNameR paramName("vWBasisY");
		m_pShader->FXSetPSFloat(paramName, &vWorldBasisY, 1);
	}

	{
		static CCryNameR paramName("vWBasisZ");
		m_pShader->FXSetPSFloat(paramName, &vWorldBasisZ, 1);
	}

	{
		static CCryNameR paramNameVolumeToWorld("g_mUnitLightVolumeToWorld");
		m_pShader->FXSetVSFloat(paramNameVolumeToWorld, (Vec4*) mUnitVolumeToWorld.GetData(), 4);
	}

	{
		static CCryNameR paramNameSphereAdjust("g_vLightVolumeSphereAdjust");
		m_pShader->FXSetVSFloat(paramNameSphereAdjust, &vSphereAdjust, 1);
	}

	{
		static CCryNameR paramName("g_mViewProj");
		Matrix44 mViewProjMatrix = rd->m_ViewMatrix * rd->m_ProjMatrix;
		m_pShader->FXSetVSFloat(paramName, (Vec4*) mViewProjMatrix.GetData(), 4);
	}

	// Vertex/index buffer
	rd->FX_SetVStream(0, rd->m_pUnitFrustumVB[meshType], 0, sizeof(SVF_P3F_C4B_T2F));
	rd->FX_SetIStream(rd->m_pUnitFrustumIB[meshType], 0, (rd->kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

	rd->D3DSetCull(eCULL_Back);
	if (!FAILED(rd->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		rd->FX_Commit();
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, rd->m_UnitFrustVBSize[meshType], 0, rd->m_UnitFrustIBSize[meshType]);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DrawDecalVolume(const SDeferredDecal& rDecal, Matrix44A& mDecalLightProj, ECull volumeCull)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	float maskRTWidth = float(m_nCurTargetWidth);
	float maskRTHeight = float(m_nCurTargetHeight);

	Vec4 vScreenScale(1.0f / maskRTWidth, 1.0f / maskRTHeight,
	                  0.0f, 0.0f);

	{
		static CCryNameR paramName("g_ScreenScale");
		m_pShader->FXSetPSFloat(paramName, &vScreenScale, 1);
	}

	//////////////// light sphere processing /////////////////////////////////
	{
		Matrix44 mInvDecalLightProj = mDecalLightProj.GetInverted();
		static CCryNameR paramName("g_mInvLightProj");
		m_pShader->FXSetVSFloat(paramName, alias_cast<Vec4*>(&mInvDecalLightProj), 4);
	}

	rd->FX_SetVStream(0, rd->m_pUnitFrustumVB[SHAPE_BOX], 0, sizeof(SVF_P3F_C4B_T2F));
	rd->FX_SetIStream(rd->m_pUnitFrustumIB[SHAPE_BOX], 0, (rd->kUnitObjectIndexSizeof == 2 ? Index16 : Index32));

	rd->D3DSetCull(volumeCull);
	if (!FAILED(rd->FX_SetVertexDeclaration(0, EDefaultInputLayouts::P3F_C4B_T2F)))
	{
		rd->FX_Commit();
		rd->FX_DrawIndexedPrimitive(eptTriangleList, 0, 0, rd->m_UnitFrustVBSize[SHAPE_BOX], 0, rd->m_UnitFrustIBSize[SHAPE_BOX]);
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::DeferredDecalPass(const SDeferredDecal& rDecal, uint32 indDecal)
{
	PROFILE_FRAME(CDeferredShading_DecalPass);

	gcpRendD3D->m_RP.m_FlagsShader_RT &= ~(RT_LIGHTSMASK | g_HWSR_MaskBit[HWSR_SAMPLE4]);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	int nThreadID = rd->m_RP.m_nProcessThreadID;

	rd->m_RP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;

	bool bProj2D = true;
	bool bStencilMask = false;
	bool bUseLightVolumes = true;

	rd->EF_Scissor(false, 0, 0, 1, 1);
	rd->SetDepthBoundTest(0.0f, 1.0f, false); // stencil pre-passes are rop bound, using depth bounds increases even more rop cost

	IMaterial* pDecalMaterial = rDecal.pMaterial;
	if (pDecalMaterial == NULL)
	{
		assert(0);
		return false;
	}

	SShaderItem& sItem = pDecalMaterial->GetShaderItem(0);
	if (sItem.m_pShaderResources == NULL)
	{
		assert(0);
		return false;
	}

	CTexture* pCurTarget = CTexture::s_ptexSceneNormalsMap;

	int nStates = m_nRenderState;

	nStates &= ~GS_BLEND_MASK;
	nStates &= ~GS_NOCOLMASK_RGBA;
	nStates |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

	ITexture* pNormalMap = TextureHelpers::LookupTexDefault(EFTT_NORMALS);
	if (SEfResTexture* pNormalRes = sItem.m_pShaderResources->GetTexture(EFTT_NORMALS))
	{
		if (pNormalRes->m_Sampler.m_pITex)
		{
			pNormalMap = pNormalRes->m_Sampler.m_pITex;
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		}
	}

	ITexture* pSmoothnessMap = TextureHelpers::LookupTexDefault(EFTT_SMOOTHNESS);
	if (SEfResTexture* pSmoothnessRes = sItem.m_pShaderResources->GetTexture(EFTT_SMOOTHNESS))
	{
		if (pSmoothnessRes->m_Sampler.m_pITex)
		{
			pSmoothnessMap = pSmoothnessRes->m_Sampler.m_pITex;
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		}
	}

	ITexture* pOpacityMap = TextureHelpers::LookupTexDefault(EFTT_OPACITY);
	if (SEfResTexture* pOpacityRes = sItem.m_pShaderResources->GetTexture(EFTT_OPACITY))
	{
		if (pOpacityRes->m_Sampler.m_pITex)
		{
			pOpacityMap = pOpacityRes->m_Sampler.m_pITex;
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5];
		}
	}

	if (rDecal.fGrowAlphaRef > 0.0f)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	nStates &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);
	nStates |= GS_DEPTHFUNC_LEQUAL | GS_NOCOLMASK_A;

	rd->m_RP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;

	//////////////////////////////////////////////////////////////////////////
	//apply stencil dynamic masking
	rd->FX_SetStencilState(
	  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
	  STENCOP_FAIL(FSS_STENCOP_KEEP) |
	  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
	  STENCOP_PASS(FSS_STENCOP_KEEP),
	  BIT_STENCIL_RESERVED, BIT_STENCIL_RESERVED, 0xFFFFFFFF
	  );

	rd->m_RP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;

	nStates |= GS_STENCIL;
	//////////////////////////////////////////////////////////////////////////

	if (bStencilMask)
	{
		rd->FX_StencilTestCurRef(true, false);
	}

	if (CRenderer::CV_r_deferredDecalsDebug == 2)
	{
		nStates &= ~GS_NOCOLMASK_RGBA;
		nStates &= ~GS_NODEPTHTEST;
		//newState |= GS_NODEPTHTEST;
		nStates |= GS_DEPTHWRITE;
		nStates |= GS_WIREFRAME;
	}
	else if (CRenderer::CV_r_deferredDecalsDebug == 1)
	{
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
		rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE1];
	}

	//////////////////////////////////////////////////////////////////////////
	if (bUseLightVolumes)
	{
		//enable light volumes rendering
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		static CCryNameTSCRC techName("DeferredDecalVolume");
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, techName, FEF_DONTSETSTATES);
	}
	else
	{
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pDeferredDecalTechName, FEF_DONTSETSTATES);
	}
	//////////////////////////////////////////////////////////////////////////

	m_nCurTargetWidth = pCurTarget->GetWidth();
	m_nCurTargetHeight = pCurTarget->GetHeight();

	ITexture* pDiffuseTex = NULL;
	Matrix44 texMatrix;
	SSamplerState texState;

	texMatrix.SetIdentity();

	texState.SetFilterMode(FILTER_TRILINEAR);

	if (SEfResTexture* pDiffuseRes = sItem.m_pShaderResources->GetTexture(EFTT_DIFFUSE))
	{
		if (pDiffuseRes->m_Sampler.m_pITex)
		{
			pDiffuseTex = pDiffuseRes->m_Sampler.m_pITex;

			if (pDiffuseRes->IsHasModificators())
			{
				pDiffuseRes->UpdateWithModifier(EFTT_MAX);
				SEfTexModificator* mod = pDiffuseRes->m_Ext.m_pTexModifier;
				texMatrix = pDiffuseRes->m_Ext.m_pTexModifier->m_TexMatrix;
			}

			ESamplerAddressMode Umode = pDiffuseRes->m_bUTile ? eSamplerAddressMode_Wrap : eSamplerAddressMode_Clamp;
			ESamplerAddressMode Vmode = pDiffuseRes->m_bVTile ? eSamplerAddressMode_Wrap : eSamplerAddressMode_Clamp;

			texState.SetClampMode(Umode, Vmode, Umode);
		}
	}

	{
		static CCryNameR paramTextureRect("g_TextureRect");
		assert(rDecal.rectTexture.w * rDecal.rectTexture.h > 0.f);
		Vec4 data[2];
		data[0] = Vec4(rDecal.rectTexture.w * texMatrix.m00, rDecal.rectTexture.h * texMatrix.m10, rDecal.rectTexture.x * texMatrix.m00 + rDecal.rectTexture.y * texMatrix.m10 + texMatrix.m30, 0);
		data[1] = Vec4(rDecal.rectTexture.w * texMatrix.m01, rDecal.rectTexture.h * texMatrix.m11, rDecal.rectTexture.x * texMatrix.m01 + rDecal.rectTexture.y * texMatrix.m11 + texMatrix.m31, 0);
		m_pShader->FXSetPSFloat(paramTextureRect, data, 2);
	}

	assert(pDiffuseTex != NULL);
	if (pDiffuseTex)
	{
		texState.m_bSRGBLookup = true;
		((CTexture*)pDiffuseTex)->Apply(2, CDeviceObjectFactory::GetOrCreateSamplerStateHandle(texState));
	}

	Vec4 vDiff = sItem.m_pShaderResources->GetColorValue(EFTT_DIFFUSE).toVec4();
	vDiff.w = sItem.m_pShaderResources->GetStrengthValue(EFTT_OPACITY) * rDecal.fAlpha;
	m_pShader->FXSetPSFloat(m_pParamDecalDiffuse, &vDiff, 1);

	Vec4 vSpec = sItem.m_pShaderResources->GetColorValue(EFTT_SPECULAR).toVec4();
	vSpec.w = sItem.m_pShaderResources->GetStrengthValue(EFTT_SMOOTHNESS);
	m_pShader->FXSetPSFloat(m_pParamDecalSpecular, &vSpec, 1);

	float decalAlphaMult = 1;
	float decalFalloff = 1;
	float decalDiffuseOpacity = 1;

	DynArrayRef<SShaderParam>& shaderParams = sItem.m_pShaderResources->GetParameters();
	for (uint32 i = 0, si = shaderParams.size(); i < si; ++i)
	{
		const char* name = shaderParams[i].m_Name;
		if (strcmp(name, "DecalAlphaMult") == 0)
			decalAlphaMult = shaderParams[i].m_Value.m_Float;
		else if (strcmp(name, "DecalFalloff") == 0)
			decalFalloff = shaderParams[i].m_Value.m_Float;
		else if (strcmp(name, "DecalDiffuseOpacity") == 0)
			decalDiffuseOpacity = shaderParams[i].m_Value.m_Float;
	}

	Vec4 decalParams(decalAlphaMult, decalFalloff, decalDiffuseOpacity, rDecal.fGrowAlphaRef);
	m_pShader->FXSetPSFloat(m_pGeneralParams, &decalParams, 1);

	if (pDiffuseTex && CRenderer::CV_r_deferredDecalsDebug == 1)
	{
		Vec4 decalParams;
		indDecal = pDiffuseTex->GetTextureID() % 3;
		//indDecal%=3;
		decalParams.x = indDecal == 0 ? 1.0f : 0.0;
		decalParams.y = indDecal == 1 ? 1.0f : 0.0;
		decalParams.z = indDecal == 2 ? 1.0f : 0.0;
		decalParams.w = 0.94f;
		m_pShader->FXSetPSFloat(m_pGeneralParams, &decalParams, 1);
	}

	SD3DPostEffectsUtils::SetTexture((CTexture*)pNormalMap, 3, FILTER_TRILINEAR, eSamplerAddressMode_Wrap);
	SD3DPostEffectsUtils::SetTexture((CTexture*)pSmoothnessMap, 4, FILTER_TRILINEAR, eSamplerAddressMode_Wrap);
	SD3DPostEffectsUtils::SetTexture((CTexture*)pOpacityMap, 5, FILTER_TRILINEAR, eSamplerAddressMode_Wrap);
	SD3DPostEffectsUtils::SetTexture((CTexture*)CTexture::s_ptexBackBuffer, 6, FILTER_POINT, eSamplerAddressMode_Wrap);   //contains copy of normals buffer

	//////////////////////////////////////////////////////////////////////////
	// texture and depth transformation matrix
	// fading distance is longer under the decal and shorter in front of the decal

	static const float fZNear = -0.3f;
	static const float fZFar = 0.5f;

	static const Matrix44A mTextureAndDepth(
	  0.5f, 0.0f, 0.0f, 0.5f,
	  0.0f, -0.5f, 0.0f, 0.5f,
	  0.0f, 0.0f, 1.0f / (fZNear - fZFar), fZNear / (fZNear - fZFar),
	  0.0f, 0.0f, 0.0f, 1.0f);

	// transform world coords to decal texture coords
	Matrix44A mDecalLightProj = mTextureAndDepth * rDecal.projMatrix.GetInverted();

	// coord systems conversion (from orientation to shader matrix)
	const Vec3 vBasisX = rDecal.projMatrix.GetColumn0();
	const Vec3 vBasisY = rDecal.projMatrix.GetColumn1();
	const Vec3 vBasisZ = rDecal.projMatrix.GetColumn2();

	const Vec3 vNormX = vBasisX.GetNormalized();
	const Vec3 vNormY = vBasisY.GetNormalized();
	const Vec3 vNormZ = vBasisZ.GetNormalized();

	// decal normal map to world transform
	Matrix44A mDecalTS(
	  vNormX.x, vNormX.y, vNormX.z, 0,
	  -vNormY.x, -vNormY.y, -vNormY.z, 0,
	  vNormZ.x, vNormZ.y, vNormZ.z, 0,
	  0, 0, 0, 1);

	// Manual mip level computation
	//
	//                 tan(fov) * (textureSize * tiling / decalSize) * distance
	// MipLevel = log2 --------------------------------------------------------
	//                 screenResolution * dot(viewVector, decalNormal)

	const float screenRes = (float)rd->GetWidth() * 0.5f + (float)rd->GetHeight() * 0.5f;
	const float decalSize = max(vBasisX.GetLength() * 2.0f, vBasisY.GetLength() * 2.0f);
	const float texScale = max(
	  texMatrix.GetColumn(0).GetLength() * rDecal.rectTexture.w,
	  texMatrix.GetColumn(1).GetLength() * rDecal.rectTexture.h);
	const float mipLevelFactor = (tan(rd->GetCamera().GetFov()) * texScale / decalSize) / screenRes;

	Vec4 vMipLevels;
	vMipLevels.x = pDiffuseTex ? mipLevelFactor * (float)max(pDiffuseTex->GetWidth(), pDiffuseTex->GetHeight()) : 0.f;
	vMipLevels.y = pNormalMap ? mipLevelFactor * (float)max(pNormalMap->GetWidth(), pNormalMap->GetHeight()) : 0.f;
	vMipLevels.z = pSmoothnessMap ? mipLevelFactor * (float)max(pSmoothnessMap->GetWidth(), pSmoothnessMap->GetHeight()) : 0.f;
	vMipLevels.w = pOpacityMap ? mipLevelFactor * (float)max(pOpacityMap->GetWidth(), pOpacityMap->GetHeight()) : 0.f;
	m_pShader->FXSetPSFloat(m_pParamDecalMipLevels, &vMipLevels, 1);

	ECull volumeCull = eCULL_Back;
	nStates &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);
	nStates |= GS_DEPTHFUNC_LEQUAL;

	rd->EF_Scissor(false, 0, 0, 1, 1);

	const float r = fabs(vBasisX.dot(m_pCamFront)) + fabs(vBasisY.dot(m_pCamFront)) + fabs(vBasisZ.dot(m_pCamFront));
	const float s = m_pCamFront.dot(rDecal.projMatrix.GetTranslation() - m_pCamPos);
	if (fabs(s) - m_fCamNear <= r) // OBB-Plane via separating axis test, to check if camera near plane intersects decal volume
	{
		nStates &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);
		nStates |= GS_DEPTHFUNC_GREAT;
		volumeCull = eCULL_Front;
	}

	if (CRenderer::CV_r_deferredDecalsDebug == 2)
	{
		volumeCull = eCULL_Back;
	}

	rd->FX_SetState(nStates);
	m_pDepthRT->Apply(0, EDefaultSamplerStates::PointClamp);

	m_pShader->FXSetPSFloat(m_pParamDecalTS, (Vec4*) mDecalTS.GetData(), 4);
	m_pShader->FXSetPSFloat(m_pParamLightProjMatrix, (Vec4*) mDecalLightProj.GetData(), 4);

	if (bUseLightVolumes)
	{
		DrawDecalVolume(rDecal, mDecalLightProj, volumeCull);
	}
	else
	{
		SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_nCurTargetWidth, m_nCurTargetHeight);
	}

	SD3DPostEffectsUtils::ShEndPass();

	if (bStencilMask)
		rd->FX_StencilTestCurRef(false);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::GetLightRenderSettings(const SRenderLight* const __restrict pDL, bool& bStencilMask, bool& bUseLightVolumes, EShapeMeshType& meshType)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;

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
			const Plane* pNearPlane(rd->GetCamera().GetFrustumPlane(FR_PLANE_NEAR));
			Vec3 pntOnNearPlane(rd->GetCamera().GetPosition() - pNearPlane->DistFromPlane(rd->GetCamera().GetPosition()) * pNearPlane->n);
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
	Vec4 scaledLightRect = pLightRect * Vec4(
	  rRP.m_CurDownscaleFactor.x, rRP.m_CurDownscaleFactor.y,
	  rRP.m_CurDownscaleFactor.x, rRP.m_CurDownscaleFactor.y
	  );

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

void CDeferredShading::LightPass(const SRenderLight* const __restrict pDL, bool bForceStencilDisable)
{
	PROFILE_FRAME(CDeferredShading_LightPass);

	PROFILE_LABEL(pDL->m_sName);

	PrefetchLine(&pDL->m_Color, 0);
	PrefetchLine(&pDL->m_sWidth, 0);

	// Skip non-ambient area light if support is disabled
	if ((pDL->m_Flags & DLF_AREA_LIGHT) && !(pDL->m_Flags & DLF_AMBIENT) && !CRenderer::CV_r_DeferredShadingAreaLights)
		return;

	gcpRendD3D->m_RP.m_FlagsShader_RT &= ~(RT_LIGHTPASS_RESETMASK | RT_CLIPVOLUME_ID);

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	const bool bMSAA = rd->m_RP.m_MSAAData.Type ? true : false;

#ifdef SUPPORTS_MSAA
	const int32 nNumPassesMSAA = (rd->FX_GetMSAAMode() == 1) ? 2 : 1;
	rd->FX_MSAASampleFreqStencilSetup();
#endif

	const ITexture* pLightTex = pDL->m_pLightImage ? pDL->m_pLightImage : pDL->m_pLightDynTexSource ? pDL->m_pLightDynTexSource->GetTexture() : NULL;
	const bool bProj2D = (pDL->m_Flags & DLF_PROJECT) && pLightTex && !(pLightTex->GetFlags() & FT_REPLICATE_TO_ALL_SIDES);
	const bool bAreaLight = (pDL->m_Flags & DLF_AREA_LIGHT) && pDL->m_fAreaWidth && pDL->m_fAreaHeight && pDL->m_fLightFrustumAngle;

	// Store light properties (color/radius, position relative to camera, rect, zbounds)
	Vec4 pLightDiffuse = Vec4(pDL->m_Color.r, pDL->m_Color.g, pDL->m_Color.b, pDL->m_SpecMult);

	float fInvRadius = (pDL->m_fRadius <= 0) ? 1.0f : 1.0f / pDL->m_fRadius;
	Vec4 pLightPos = Vec4(pDL->m_Origin, fInvRadius);
	Vec4 pDepthBounds = GetLightDepthBounds(pDL, (rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0);

	Vec4 scaledLightRect = Vec4(
	  pDL->m_sX * rRP.m_CurDownscaleFactor.x,
	  pDL->m_sY * rRP.m_CurDownscaleFactor.y,
	  pDL->m_sWidth * rRP.m_CurDownscaleFactor.x,
	  pDL->m_sHeight * rRP.m_CurDownscaleFactor.y
	  );

	bool bUseLightVolumes = false;
	bool bStencilMask = (CRenderer::CV_r_DeferredShadingStencilPrepass && (bProj2D || bAreaLight)) || CRenderer::CV_r_DebugLightVolumes || (pDL->m_fProjectorNearPlane < 0);
	rRP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;

	GetLightRenderSettings(pDL, bStencilMask, bUseLightVolumes, rRP.m_nDeferredPrimitiveID);

	//reset stencil mask
	if (bForceStencilDisable)
	{
		bStencilMask = false;
	}

	if (pDL->m_Flags & DLF_AMBIENT)
		rRP.m_FlagsShader_RT |= RT_AMBIENT_LIGHT;

	if (CRenderer::CV_r_DeferredShadingAreaLights)
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE3];

	// Apply LBuffers range rescale
	pLightDiffuse.x *= rd->m_fAdaptedSceneScaleLBuffer;
	pLightDiffuse.y *= rd->m_fAdaptedSceneScaleLBuffer;
	pLightDiffuse.z *= rd->m_fAdaptedSceneScaleLBuffer;

	float fAttenuationBulbSize = pDL->m_fAttenuationBulbSize;

	if (bAreaLight)
		fAttenuationBulbSize = (pDL->m_fAreaWidth + pDL->m_fAreaHeight) * 0.25;

	// Adjust light intensity so that the intended brightness is reached 1 meter from the light's surface
	if (!(pDL->m_Flags & DLF_AMBIENT))
	{
		fAttenuationBulbSize = max(fAttenuationBulbSize, 0.001f);

		// Solve I * 1 / (1 + d/lightsize)^2 = 1
		float intensityMul = 1.0f + 1.0f / fAttenuationBulbSize;
		intensityMul *= intensityMul;
		pLightDiffuse.x *= intensityMul;
		pLightDiffuse.y *= intensityMul;
		pLightDiffuse.z *= intensityMul;
	}

	// Enable light pass flags
	if ((pDL->m_Flags & DLF_PROJECT))
	{
		assert(!(pDL->GetDiffuseCubemap() && pDL->GetSpecularCubemap()));
		rRP.m_FlagsShader_RT |= RT_TEX_PROJECT;
		if (bProj2D && !bAreaLight)
		{
			rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_LIGHT_TEX_PROJ];
		}
	}

	if (bAreaLight)
	{
		rRP.m_FlagsShader_RT |= RT_AREALIGHT;
	}

	uint16 scaledX = (uint16)(scaledLightRect.x);
	uint16 scaledY = (uint16)(scaledLightRect.y);
	uint16 scaledWidth = (uint16)(scaledLightRect.z) + 1;
	uint16 scaledHeight = (uint16)(scaledLightRect.w) + 1;

	if (CRenderer::CV_r_DeferredShadingScissor)
	{
		SetupScissors(true, scaledX, scaledY, scaledWidth, scaledHeight);
	}

	if (bStencilMask)
	{
		PROFILE_LABEL_SCOPE("STENCIL_VOLUME");

		SpecularAccEnableMRT(false);
		auto prevPersFlags = rd->m_RP.m_TI[m_nThreadID].m_PersFlags;
		rd->m_RP.m_TI[m_nThreadID].m_PersFlags |= RBPF_MIRRORCULL;

		rd->SetDepthBoundTest(0.0f, 1.0f, false); // stencil pre-passes are rop bound, using depth bounds increases even more rop cost
		rd->FX_StencilFrustumCull((pDL->m_Flags & DLF_CASTSHADOW_MAPS) ? -4 : -1, pDL, NULL, 0);

		rd->m_RP.m_TI[m_nThreadID].m_PersFlags = prevPersFlags;
	}
	else if (rd->m_bDeviceSupports_NVDBT && CRenderer::CV_r_DeferredShadingDepthBoundsTest == 1)
		rd->SetDepthBoundTest(pDepthBounds.x, pDepthBounds.z, true);

	// todo: try out on consoles if DBT helps on light pass (on light stencil prepass is actually slower)
	if (rd->m_bDeviceSupports_NVDBT && bStencilMask && CRenderer::CV_r_DeferredShadingDepthBoundsTest && CRenderer::CV_r_deferredshadingDBTstencil)
		rd->SetDepthBoundTest(pDepthBounds.x, pDepthBounds.z, true);

	if (bStencilMask)
	{
		SpecularAccEnableMRT(true);
	}

#ifdef SUPPORTS_MSAA
	rd->FX_MSAARestoreSampleMask(); // If stencil clear occurred, restore sample mask

	for (int32 nPass = 0; nPass < nNumPassesMSAA; ++nPass)
	{
		if (nPass == 1)
		{
			PROFILE_LABEL_PUSH("LIGHT_SAMPLEFREQPASS");
			rd->FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_PASS);
		}
#endif
	uint nNumClipVolumes = RenderView()->GetClipVolumes().size();
	if (nNumClipVolumes > 0)
		rRP.m_FlagsShader_RT |= RT_CLIPVOLUME_ID;

	if (bUseLightVolumes)
	{
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pLightVolumeTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	}
	else
	{
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	}

	int nStates = m_nRenderState;
	nStates &= ~(GS_BLEND_MASK);

	nStates &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);     // Ensure zcull used.
	nStates |= GS_DEPTHFUNC_LEQUAL;

	if (!(pDL->m_Flags & DLF_AMBIENT))
		nStates |= (GS_BLSRC_ONE | GS_BLDST_ONE);
	else
		nStates |= (GS_BLSRC_DSTCOL | GS_BLDST_ZERO);

	if (CRenderer::CV_r_DeferredShadingDebug == 2)
	{
		nStates &= ~GS_BLEND_MASK;
		nStates |= (GS_BLSRC_ONE | GS_BLDST_ONE);
	}

	rd->FX_SetState(nStates);

#ifdef SUPPORTS_MSAA
	if (bMSAA)
	{
		bool bUsesStencil = bForceStencilDisable || bStencilMask;   // shadows/clip volumes, or using light stencil
		rRP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
		const int32 nMSAAStState = STENC_FUNC(FSS_STENCFUNC_EQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nMSAAStState, (bUsesStencil ? rd->m_nStencilMaskRef : 0) | ((rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) ? BIT_STENCIL_RESERVED : 0), bUsesStencil ? 0xFF : BIT_STENCIL_RESERVED, 0xFF);
		rd->FX_SetState(rRP.m_CurState | GS_STENCIL);
	}
	else
#endif
	if (bStencilMask)
		rd->FX_StencilTestCurRef(true, false);

	if ((pDL->m_Flags & DLF_PROJECT))
	{
		Matrix44A ProjMatrixT;

		if (bProj2D)
		{
			CShadowUtils::GetProjectiveTexGen(pDL, 0, &ProjMatrixT);
		}
		else
		{
			ProjMatrixT = pDL->m_ProjMatrix;
		}

		// translate into camera space
		ProjMatrixT.Transpose();
		m_pShader->FXSetPSFloat(m_pParamLightProjMatrix, (Vec4*) ProjMatrixT.GetData(), 4);
	}

	{
		Vec2 vLightSize = Vec2(pDL->m_fAreaWidth * 0.5f, pDL->m_fAreaHeight * 0.5f);

		float fAreaFov = pDL->m_fLightFrustumAngle * 2.0f;
		if ((pDL->m_Flags & DLF_CASTSHADOW_MAPS) && bAreaLight)
			fAreaFov = min(fAreaFov, 135.0f);   // Shadow can only cover ~135 degree FOV without looking bad, so we clamp the FOV to hide shadow clipping.

		const float fCosAngle = cosf(fAreaFov * (gf_PI / 360.0f));   // pre-compute on CPU.

		static CCryNameR arealightMatrixName("g_AreaLightMatrix");
		Matrix44 mAreaLightMatrix;
		mAreaLightMatrix.SetRow4(0, Vec4(pDL->m_ObjMatrix.GetColumn0().GetNormalized(), 1.0f));
		mAreaLightMatrix.SetRow4(1, Vec4(pDL->m_ObjMatrix.GetColumn1().GetNormalized(), 1.0f));
		mAreaLightMatrix.SetRow4(2, Vec4(pDL->m_ObjMatrix.GetColumn2().GetNormalized(), 1.0f));
		mAreaLightMatrix.SetRow4(3, Vec4(vLightSize.x, vLightSize.y, 0, fCosAngle));
		m_pShader->FXSetPSFloat(arealightMatrixName, alias_cast<Vec4*>(&mAreaLightMatrix), 4);
	}

	m_pShader->FXSetPSFloat(m_pParamLightPos, &pLightPos, 1);
	m_pShader->FXSetPSFloat(m_pParamLightDiffuse, &pLightDiffuse, 1);

	uint32 stencilID = (pDL->m_nStencilRef[1] + 1) << 16 | pDL->m_nStencilRef[0] + 1;
	Vec4 vAttenParams(fAttenuationBulbSize, *alias_cast<float*>(&stencilID), 0, 0);
	m_pShader->FXSetPSFloat(m_pAttenParams, &vAttenParams, 1);

	// Directional occlusion
	Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
	Vec4 ssdoParamsNull(0, 0, 0, 0);
	static CCryNameR ssdoParamsName("SSDOParams");
	m_pShader->FXSetPSFloat(ssdoParamsName, CRenderer::CV_r_ssdo ? &ssdoParams : &ssdoParamsNull, 1);

	if (pDL->m_Flags & DLF_CASTSHADOW_MAPS)
	{
		static ICVar* pVar = iConsole->GetCVar("e_ShadowsPoolSize");
		int nShadowAtlasRes = pVar->GetIVal();

		const ShadowMapFrustum& firstFrustum = CShadowUtils::GetFirstFrustum(m_pCurrentRenderView, m_nCurLighID);
		//LRad
		float kernelSize = firstFrustum.bOmniDirectionalShadow ? 2.5f : 1.5f;
		const Vec4 vShadowParams(kernelSize * (float(firstFrustum.nTexSize) / nShadowAtlasRes), 0.0f, 0.0f, firstFrustum.fDepthConstBias);
		m_pShader->FXSetPSFloat(m_pGeneralParams, &vShadowParams, 1);

		// set up shadow matrix
		static CCryNameR paramName("g_mLightShadowProj");
		Matrix44A shadowMat = gRenDev->m_TempMatrices[0][0];

		// pre-multiply by 1/frustrum_far_plane
		(Vec4&)shadowMat.m20 *= gRenDev->m_cEF.m_TempVecs[2].x;

		//camera matrix
		m_pShader->FXSetPSFloat(paramName, alias_cast<Vec4*>(&shadowMat), 4);
	}

	if (pDL->m_pLightDynTexSource)
	{
		CFlashTextureSourceBase::AddToLightRenderList(pDL->m_pLightDynTexSource);
	}

	CTexture* pTexLightImage = (CTexture*)pLightTex;

	// Note: Shadows use slot 3 and slot 7 for shadow map and jitter map

	m_pDepthRT->Apply(0, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pNormalsRT->Apply(1, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pDiffuseRT->Apply(2, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pSpecularRT->Apply(4, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));

	if ((pDL->m_Flags & DLF_PROJECT) && pTexLightImage)
		SD3DPostEffectsUtils::SetTexture(pTexLightImage, 5, FILTER_TRILINEAR, bProj2D ? eSamplerAddressMode_Clamp : eSamplerAddressMode_Wrap);

	CTexture::s_ptexSceneNormalsBent->Apply(8, EDefaultSamplerStates::PointClamp);
	m_pMSAAMaskRT->Apply(9, EDefaultSamplerStates::PointClamp);

	if (nNumClipVolumes > 0)
	{
		m_pResolvedStencilRT->Apply(11, EDefaultSamplerStates::PointClamp);
		m_pShader->FXSetPSFloat(m_pClipVolumeParams, m_vClipVolumeParams, min(static_cast<uint32>(CClipVolumesStage::MaxDeferredClipVolumes), static_cast<uint32>(nNumClipVolumes + CClipVolumesStage::VisAreasOutdoorStencilOffset)));
	}

	if (bUseLightVolumes)
	{
		gcpRendD3D->D3DSetCull(eCULL_Back);

		const Vec3 scale(pDL->m_fRadius * 1.08f);
		Matrix34 mUnitVolumeToWorld = bAreaLight ? CShadowUtils::GetAreaLightMatrix(pDL, scale) : Matrix34::CreateScale(scale, pDL->m_Origin);

		DrawLightVolume(bAreaLight ? SHAPE_BOX : SHAPE_SPHERE, mUnitVolumeToWorld.GetTransposed());
	}
	else
	{
		gcpRendD3D->D3DSetCull(eCULL_Back, true);   //fs quads should not revert test..
		SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_pLBufferDiffuseRT->GetWidth(), m_pLBufferDiffuseRT->GetHeight(), pDepthBounds.x);
	}

	SD3DPostEffectsUtils::ShEndPass();
#ifdef SUPPORTS_MSAA
	if (nPass == 1)
	{
		rd->FX_MSAASampleFreqStencilSetup();
		PROFILE_LABEL_POP("LIGHT_SAMPLEFREQPASS");
	}
}
#endif

	rd->SetDepthBoundTest(0.0f, 1.0f, false);

#ifdef SUPPORTS_MSAA
	if (bMSAA)
		rRP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;
#endif

	if (bStencilMask)
		rd->FX_StencilTestCurRef(false);

	if (CRenderer::CV_r_DeferredShadingScissor)
		rd->EF_Scissor(false, 0, 0, 0, 0);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeferredShading::PrepareClipVolumeData(bool& bOutdoorVisible)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bMSAA = rd->m_RP.m_MSAAData.Type ? true : false;
	const int nClipVolumeReservedStencilBit = BIT_STENCIL_INSIDE_CLIPVOLUME;

	// fetch clip volume shader data from clip volume stage
	const Vec4* pVolumeParams; uint32 numVolumes;
	rd->GetGraphicsPipeline().GetClipVolumesStage()->GetClipVolumeShaderParams(pVolumeParams, numVolumes);
	memcpy(m_vClipVolumeParams, pVolumeParams, numVolumes * sizeof(Vec4));

	bOutdoorVisible = rd->GetGraphicsPipeline().GetClipVolumesStage()->IsOutdoorVisible();

	rd->m_nStencilMaskRef = nClipVolumeReservedStencilBit + RenderView()->GetClipVolumes().size() + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::AmbientPass(const SRenderLight* pGlobalCubemap, bool& bOutdoorVisible)
{
	PROFILE_FRAME(CDeferredShading_AmbientPass);
	PROFILE_LABEL_SCOPE("AMBIENT_PASS");

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	const bool bMSAA = rd->m_RP.m_MSAAData.Type ? true : false;

	rd->m_RP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;
	rd->D3DSetCull(eCULL_Back, true); //fs quads should not revert test..

#ifdef SUPPORTS_MSAA
	const int32 nNumPassesMSAA = (rd->FX_GetMSAAMode() == 1) ? 2 : 1;
	rd->FX_MSAASampleFreqStencilSetup();
#endif

	const uint64 nFlagsShaderRT = rRP.m_FlagsShader_RT;
	rRP.m_FlagsShader_RT &= ~(RT_LIGHTSMASK);

	SpecularAccEnableMRT(false);
	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, DBT_SKY_CULL_DEPTH, false); // Disable depth bounds for ambient lookup.

	const uint32 nNumClipVolumes = RenderView()->GetClipVolumes().size();
	if (nNumClipVolumes)
		rRP.m_FlagsShader_RT |= RT_CLIPVOLUME_ID;
	else
		bOutdoorVisible = true;

	// Store global cubemap color
	Vec4 pLightDiffuse;
	if (pGlobalCubemap)
	{
		pLightDiffuse = Vec4(Vec3(pGlobalCubemap->m_Color.r, pGlobalCubemap->m_Color.g, pGlobalCubemap->m_Color.b) * rd->m_fAdaptedSceneScaleLBuffer, pGlobalCubemap->m_SpecMult);

		const float fLuminance = pGlobalCubemap->m_Color.Luminance();
		if (fLuminance > 0.001f)  // too dull => skip
		{
			rRP.m_FlagsShader_RT |= RT_GLOBAL_CUBEMAP;
			// ignore specular if it's too dull
			if (fLuminance * pLightDiffuse.w >= 0.005f)
				rRP.m_FlagsShader_RT |= RT_SPECULAR_CUBEMAP;

			rRP.m_FlagsShader_RT |= (pGlobalCubemap->m_Flags & DLF_IGNORES_VISAREAS) ? RT_GLOBAL_CUBEMAP_IGNORE_VISAREAS : 0;
		}
		else
			pGlobalCubemap = NULL;
	}

	// Patch z-target for all platforms, we need stencil access.
	CTexture* pDepthBufferRT = m_pDepthRT;
	SDepthTexture sBackup = rd->FX_ReplaceMainDepthBuffer(pDepthBufferRT, bMSAA);

	rd->FX_PushRenderTarget(0, m_pLBufferDiffuseRT, &rd->m_DepthBufferOrig, -1, false, 1);

	SpecularAccEnableMRT(true);
	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, DBT_SKY_CULL_DEPTH, true); // Enable depth bounds - discard sky

	Vec3 vE3DParam;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_AMBIENT_GROUND_COLOR, vE3DParam);

	const Vec4 cOutdoorAmbient = Vec4(gEnv->p3DEngine->GetSkyColor() * rd->m_fAdaptedSceneScaleLBuffer, 0);
	const Vec4 cAmbGroundColor = Vec4(vE3DParam * rd->m_fAdaptedSceneScaleLBuffer, 0);

	Vec4 cAmbHeightParams = Vec4(gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_AMBIENT_MIN_HEIGHT), gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_AMBIENT_MAX_HEIGHT), 0, 0);
	cAmbHeightParams.z = 1.0 / max(0.0001f, cAmbHeightParams.y);

	rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_MSAA_SAMPLEFREQ_PASS];

#ifdef SUPPORTS_MSAA
	rd->FX_MSAARestoreSampleMask(); // If stencil clear occurred, restore sample mask
	for (int32 nPass = 0; nPass < nNumPassesMSAA; ++nPass)
	{
		if (nPass == 1)
		{
			PROFILE_LABEL_PUSH("AMBIENT_SAMPLEFREQPASS");
			rd->FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_PASS);
		}
#endif

	SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pAmbientOutdoorTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	rd->FX_SetState(GS_NODEPTHTEST);

#ifdef SUPPORTS_MSAA
	if (bMSAA)
	{
		rRP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
		const int32 nMSAAStState = STENC_FUNC((rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) ? FSS_STENCFUNC_EQUAL : FSS_STENCFUNC_NOTEQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nMSAAStState, BIT_STENCIL_RESERVED, BIT_STENCIL_RESERVED, 0xFF);
		rd->FX_SetState(rRP.m_CurState | GS_STENCIL);
	}
#endif

	SD3DPostEffectsUtils::ShSetParamPS(m_pParamAmbient, cOutdoorAmbient);
	SD3DPostEffectsUtils::ShSetParamPS(m_pParamAmbientGround, cAmbGroundColor);
	SD3DPostEffectsUtils::ShSetParamPS(m_pParamAmbientHeight, cAmbHeightParams);

	if (nNumClipVolumes)
		m_pShader->FXSetPSFloat(m_pClipVolumeParams, m_vClipVolumeParams, min(static_cast<uint32>(CClipVolumesStage::MaxDeferredClipVolumes), static_cast<uint32>(nNumClipVolumes + CClipVolumesStage::VisAreasOutdoorStencilOffset)));

	m_pNormalsRT->Apply(0, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA);

	if (pGlobalCubemap)
	{
		CTexture* const texDiffuse = (CTexture*)pGlobalCubemap->GetDiffuseCubemap();
		CTexture* const texSpecular = (CTexture*)pGlobalCubemap->GetSpecularCubemap();

		SD3DPostEffectsUtils::SetTexture(texDiffuse->GetTextureType() < eTT_Cube ? CTexture::s_ptexNoTextureCM : texDiffuse, 1, FILTER_BILINEAR, eSamplerAddressMode_Clamp, texDiffuse->IsSRGB());
		SD3DPostEffectsUtils::SetTexture(texSpecular->GetTextureType() < eTT_Cube ? CTexture::s_ptexNoTextureCM : texSpecular, 2, FILTER_TRILINEAR, eSamplerAddressMode_Clamp, texSpecular->IsSRGB());

		SD3DPostEffectsUtils::ShSetParamPS(m_pParamLightDiffuse, pLightDiffuse);
		const Vec4 vCubemapParams(IntegerLog2((uint32)texSpecular->GetWidthNonVirtual()) - 2, 0, 0, 0);   // Use 4x4 mip for lowest gloss values
		m_pShader->FXSetPSFloat(m_pGeneralParams, &vCubemapParams, 1);

		// Directional occlusion
		Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
		Vec4 ssdoParamsNull(0, 0, 0, 0);
		static CCryNameR ssdoParamsName("SSDOParams");
		m_pShader->FXSetPSFloat(ssdoParamsName, CRenderer::CV_r_ssdo ? &ssdoParams : &ssdoParamsNull, 1);
		CTexture::s_ptexSceneNormalsBent->Apply(8, EDefaultSamplerStates::PointClamp);
	}

	// DX11 requires explicitly bind depth then stencil to have access to both depth and stencil read from shader. Formats also must match
	pDepthBufferRT->Apply(3, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::DepthOnly, bMSAA);
	pDepthBufferRT->Apply(4, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::StencilOnly, bMSAA);
	m_pMSAAMaskRT->Apply(5, EDefaultSamplerStates::PointClamp);
	m_pSpecularRT->Apply(7, EDefaultSamplerStates::PointClamp);
	m_pDiffuseRT->Apply(11, EDefaultSamplerStates::PointClamp);

	CTexture::s_ptexEnvironmentBRDF->Apply(10, EDefaultSamplerStates::LinearClamp);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_pLBufferDiffuseRT->GetWidth(), m_pLBufferDiffuseRT->GetHeight(), 0, &gcpRendD3D->m_FullResRect);
	SD3DPostEffectsUtils::ShEndPass();

#ifdef SUPPORTS_MSAA
	if (nPass == 1)
	{
		PROFILE_LABEL_POP("AMBIENT_SAMPLEFREQPASS");
	}
}
#endif

#ifdef SUPPORTS_MSAA
	if (bMSAA)
		rRP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;
#endif

	// Restore DSV/SRV
	rd->FX_RestoreMSAADepthBuffer(sBackup, pDepthBufferRT, bMSAA);
	rd->FX_PopRenderTarget(0);

	CTexture::s_ptexBlack->Apply(3, EDefaultSamplerStates::PointClamp);
	CTexture::s_ptexBlack->Apply(4, EDefaultSamplerStates::PointClamp);
	rd->FX_SetActiveRenderTargets();

	// Follow up with NVidia. Seems driver/pixel quads synchronization? Wrong behavior when reading from native stencil/depth.
	// Luckily can clear stencil since vis areas/decals tag not needed from here on
	if (CRenderer::CV_r_DeferredShadingAmbientSClear)
	{
		rd->EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL, Clr_Unused.r, 1);
#ifdef SUPPORTS_MSAA
		rd->FX_MSAARestoreSampleMask(); // If stencil clear occurred, restore sample mask
#endif
	}

	rRP.m_FlagsShader_RT = nFlagsShaderRT;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DeferredCubemaps(const RenderLightsList& rCubemaps, uint32 nStartIndex /* = 0 */)
{
	if (nStartIndex < rCubemaps.size() && CRenderer::CV_r_DeferredShadingEnvProbes)
	{
		// apply deferred cubemaps first
		PROFILE_LABEL_SCOPE("DEFERRED_CUBEMAPS");

		auto stp = rCubemaps.end();
		auto itr = rCubemaps.begin();
		while (--nStartIndex >= 0)
			++itr;
		while (itr != stp)
		{
			const SRenderLight& pDL = *itr;
			++itr;
			if (pDL.m_Flags & (DLF_FAKE | DLF_VOLUMETRIC_FOG_ONLY))
				continue;
			DeferredCubemapPass(&pDL);
			m_nLightsProcessedCount++;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DeferredCubemapPass(const SRenderLight* const __restrict pDL)
{
	PROFILE_FRAME(CDeferredShading_CubemapPass);
	PROFILE_LABEL(pDL->m_sName);

	float scissorInt2Float[] = { (float)(pDL->m_sX), (float)(pDL->m_sY), (float)(pDL->m_sWidth), (float)(pDL->m_sHeight) };

	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rRP = rd->m_RP;
	const bool bMSAA = rd->m_RP.m_MSAAData.Type ? true : false;

	rRP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;

	bool bStencilMask = CRenderer::CV_r_DeferredShadingStencilPrepass || CRenderer::CV_r_DebugLightVolumes;
	bool bUseLightVolumes = false;
	bool bHasSpecular = false;

#ifdef SUPPORTS_MSAA
	const int32 nNumPassesMSAA = (rd->FX_GetMSAAMode() == 1) ? 2 : 1;
	rd->FX_MSAASampleFreqStencilSetup();
#endif

	const uint64 nOldFlags = rRP.m_FlagsShader_RT;

	rRP.m_FlagsShader_RT &= ~(RT_CLIPVOLUME_ID | RT_LIGHTSMASK | RT_GLOBAL_CUBEMAP | RT_SPECULAR_CUBEMAP | RT_BOX_PROJECTION);

	uint nNumClipVolumes = RenderView()->GetClipVolumes().size();
	rd->m_RP.m_FlagsShader_RT |= (pDL->m_Flags & DLF_BOX_PROJECTED_CM) ? RT_BOX_PROJECTION : 0;
	rd->m_RP.m_FlagsShader_RT |= nNumClipVolumes > 0 ? RT_CLIPVOLUME_ID : 0;

	// Store light properties (color/radius, position relative to camera, rect, z bounds)
	Vec4 pLightDiffuse = Vec4(pDL->m_Color.r, pDL->m_Color.g, pDL->m_Color.b, pDL->m_SpecMult);

	const float fInvRadius = (pDL->m_fRadius <= 0) ? 1.0f : 1.0f / pDL->m_fRadius;
	const Vec4 pLightPos = Vec4(pDL->m_Origin, fInvRadius);
	const float fAttenFalloffMax = max(pDL->GetFalloffMax(), 1e-3f);

	const bool bReverseDepth = (rRP.m_TI[rRP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	Vec4 pDepthBounds = GetLightDepthBounds(pDL, bReverseDepth);

	// avoiding LHS, comment out if we ever use different resolution for light accumulation target
	Vec4 pLightRect = Vec4(scissorInt2Float[0], scissorInt2Float[1], scissorInt2Float[2], scissorInt2Float[3]);
	Vec4 scaledLightRect = Vec4(pLightRect.x * rRP.m_CurDownscaleFactor.x, pLightRect.y * rRP.m_CurDownscaleFactor.y,
	                            pLightRect.z * rRP.m_CurDownscaleFactor.x, pLightRect.w * rRP.m_CurDownscaleFactor.y);

	assert(!(pDL->m_Flags & DLF_PROJECT));

	if (CRenderer::CV_r_DeferredShadingLightLodRatio)
	{
		float fLightArea = pLightRect.z * pLightRect.w;
		float fScreenArea = (float) m_nCurTargetWidth * m_nCurTargetHeight;
		float fLightRatio = fLightArea / fScreenArea;

		const float fMinScreenAreaRatioThreshold = 0.01f;   // 1% of screen by default

		const float fDrawVolumeThres = 0.01f;
		if ((fLightRatio* CRenderer::CV_r_DeferredShadingLightLodRatio) < fDrawVolumeThres)
		{
			//scissor + depthbound test only
			bStencilMask = false;
		}
	}

	uint16 scaledX = (uint16)(scaledLightRect.x);
	uint16 scaledY = (uint16)(scaledLightRect.y);
	uint16 scaledWidth = (uint16)(scaledLightRect.z) + 1;
	uint16 scaledHeight = (uint16)(scaledLightRect.w) + 1;

	if (CRenderer::CV_r_DeferredShadingScissor)
	{
		SetupScissors(true, scaledX, scaledY, scaledWidth, scaledHeight);
	}

	if (bStencilMask)
	{
		SpecularAccEnableMRT(false);
		rd->SetDepthBoundTest(0.0f, 1.0f, false);
		rd->FX_StencilFrustumCull(-1, pDL, NULL, 0);
	}
	else if (rd->m_bDeviceSupports_NVDBT && CRenderer::CV_r_DeferredShadingDepthBoundsTest == 1)
		rd->SetDepthBoundTest(pDepthBounds.x, pDepthBounds.z, true);

	// todo: try out on consoles if DBT helps on light pass (on light stencil prepass is actually slower)
	if (rd->m_bDeviceSupports_NVDBT && bStencilMask && CRenderer::CV_r_DeferredShadingDepthBoundsTest && CRenderer::CV_r_deferredshadingDBTstencil)
		rd->SetDepthBoundTest(pDepthBounds.x, pDepthBounds.z, true);

	const float fFadeout = pDL->m_Color.a;  // fade-out intensity stored in alpha
	const float fLuminance = pDL->m_Color.Luminance() * fFadeout;

	if (fLuminance * pLightDiffuse.w >= 0.03f)  // if specular intensity is too low, skip it
	{
		rRP.m_FlagsShader_RT |= RT_SPECULAR_CUBEMAP;
		bHasSpecular = true;
	}

	SpecularAccEnableMRT(bHasSpecular);

	// Apply LBuffers range rescale
	pLightDiffuse.x *= rd->m_fAdaptedSceneScaleLBuffer;
	pLightDiffuse.y *= rd->m_fAdaptedSceneScaleLBuffer;
	pLightDiffuse.z *= rd->m_fAdaptedSceneScaleLBuffer;

	if (CRenderer::CV_r_deferredshadingLightVolumes)
	{
		const Plane* pNearPlane(rd->GetCamera().GetFrustumPlane(FR_PLANE_NEAR));
		Vec3 n = pNearPlane->n;
		Vec3 e = pDL->m_ProbeExtents;
		Vec3 u0 = pDL->m_ObjMatrix.GetColumn0().GetNormalized();
		Vec3 u1 = pDL->m_ObjMatrix.GetColumn1().GetNormalized();
		Vec3 u2 = pDL->m_ObjMatrix.GetColumn2().GetNormalized();

		// Check if OBB intersects near plane
		float r = e.x * fabs(n.dot(u0)) + e.y * fabs(n.dot(u1)) + e.z * fabs(n.dot(u2));
		float s = pNearPlane->DistFromPlane(pDL->m_Origin);
		bUseLightVolumes = fabs(s) > r;
	}

	int MultplyState = m_nRenderState;
	MultplyState &= ~GS_BLEND_MASK;
	MultplyState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

	if (CRenderer::CV_r_DeferredShadingDebug == 2)
	{
		// Debug mode
		MultplyState &= ~GS_BLEND_MASK;
		MultplyState |= (GS_BLSRC_ONE | GS_BLDST_ONE);
	}

	if (bStencilMask)
		MultplyState |= GS_STENCIL;

	MultplyState &= ~(GS_NODEPTHTEST | GS_DEPTHFUNC_MASK);      // Ensure zcull used.
	MultplyState |= GS_DEPTHFUNC_LEQUAL;

#ifdef SUPPORTS_MSAA
	rd->FX_MSAARestoreSampleMask(); // If stencil clear occurred, restore sample mask

	for (int32 nPass = 0; nPass < nNumPassesMSAA; ++nPass)
	{
		if (nPass == 1)
		{
			PROFILE_LABEL_PUSH("DEFERRED_CUBEMAP_SAMPLEFREQPASS");
			rd->FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_PASS);
		}
#endif

	// Render..
	if (bUseLightVolumes)
	{
		rRP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pCubemapsVolumeTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	}
	else
	{
		rRP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_CUBEMAP0];
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pCubemapsTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	}

	rd->FX_SetState(MultplyState);

#ifdef SUPPORTS_MSAA
	if (bMSAA)
	{
		rRP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
		const int32 nMSAAStState = STENC_FUNC(FSS_STENCFUNC_EQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nMSAAStState, (bStencilMask ? rd->m_nStencilMaskRef : 0) | ((rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) ? BIT_STENCIL_RESERVED : 0), bStencilMask ? 0xFF : BIT_STENCIL_RESERVED, 0xFF);
		rd->FX_SetState(rRP.m_CurState | GS_STENCIL);
	}
	else
#endif
	if (bStencilMask)
		rd->FX_StencilTestCurRef(true, false);

	m_pShader->FXSetPSFloat(m_pParamLightPos, &pLightPos, 1);
	m_pShader->FXSetPSFloat(m_pParamLightDiffuse, &pLightDiffuse, 1);

	m_pDepthRT->Apply(0, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pNormalsRT->Apply(1, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pDiffuseRT->Apply(2, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pSpecularRT->Apply(3, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rRP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));

	static CCryNameR pszProbeOBBParams("g_mProbeOBBParams");
	Vec4 vProbeOBBParams[3];
	vProbeOBBParams[0] = Vec4(pDL->m_ObjMatrix.GetColumn0().GetNormalized(), 1 / pDL->m_ProbeExtents.x);
	vProbeOBBParams[1] = Vec4(pDL->m_ObjMatrix.GetColumn1().GetNormalized(), 1 / pDL->m_ProbeExtents.y);
	vProbeOBBParams[2] = Vec4(pDL->m_ObjMatrix.GetColumn2().GetNormalized(), 1 / pDL->m_ProbeExtents.z);
	m_pShader->FXSetPSFloat(pszProbeOBBParams, vProbeOBBParams, 3);

	if (pDL->m_Flags & DLF_BOX_PROJECTED_CM)
	{
		static CCryNameR pszBoxProjectionMin("g_vBoxProjectionMin");
		static CCryNameR pszBoxProjectionMax("g_vBoxProjectionMax");
		Vec4 vBoxProjectionMin(-pDL->m_fBoxLength * 0.5f, -pDL->m_fBoxWidth * 0.5f, -pDL->m_fBoxHeight * 0.5f, 0);
		Vec4 vBoxProjectionMax(pDL->m_fBoxLength * 0.5f, pDL->m_fBoxWidth * 0.5f, pDL->m_fBoxHeight * 0.5f, 0);

		m_pShader->FXSetPSFloat(pszBoxProjectionMin, &vBoxProjectionMin, 1);
		m_pShader->FXSetPSFloat(pszBoxProjectionMax, &vBoxProjectionMax, 1);
	}

	CTexture* texDiffuse = (CTexture*)pDL->GetDiffuseCubemap();
	CTexture* texSpecular = (CTexture*)pDL->GetSpecularCubemap();

	// Use 4x4 mip for lowest gloss values
	Vec4 vCubemapParams(IntegerLog2((uint32)texSpecular->GetWidthNonVirtual()) - 2, 0, 0, 0);
	m_pShader->FXSetPSFloat(m_pGeneralParams, &vCubemapParams, 1);
	SD3DPostEffectsUtils::SetTexture(texDiffuse, 5, FILTER_BILINEAR, eSamplerAddressMode_Clamp, texDiffuse->IsSRGB());
	SD3DPostEffectsUtils::SetTexture(texSpecular, 6, FILTER_TRILINEAR, eSamplerAddressMode_Clamp, texSpecular->IsSRGB());

	uint32 stencilID = (pDL->m_nStencilRef[1] + 1) << 16 | pDL->m_nStencilRef[0] + 1;
	const Vec4 vAttenParams(fFadeout, *alias_cast<float*>(&stencilID), 0, fAttenFalloffMax);
	m_pShader->FXSetPSFloat(m_pAttenParams, &vAttenParams, 1);

	m_pMSAAMaskRT->Apply(7, EDefaultSamplerStates::PointClamp);

	// Directional occlusion
	Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
	Vec4 ssdoParamsNull(0, 0, 0, 0);
	static CCryNameR ssdoParamsName("SSDOParams");
	m_pShader->FXSetPSFloat(ssdoParamsName, CRenderer::CV_r_ssdo ? &ssdoParams : &ssdoParamsNull, 1);
	CTexture::s_ptexSceneNormalsBent->Apply(8, EDefaultSamplerStates::PointClamp);

	if (nNumClipVolumes > 0)
	{
		m_pResolvedStencilRT->Apply(9, EDefaultSamplerStates::PointClamp, -1, -1, -1);
		m_pShader->FXSetPSFloat(m_pClipVolumeParams, m_vClipVolumeParams, min(static_cast<uint32>(CClipVolumesStage::MaxDeferredClipVolumes), static_cast<uint32>(nNumClipVolumes + CClipVolumesStage::VisAreasOutdoorStencilOffset)));
	}

	CTexture::s_ptexEnvironmentBRDF->Apply(10, EDefaultSamplerStates::LinearClamp);

	if (bUseLightVolumes)
	{
		gcpRendD3D->D3DSetCull(eCULL_Back);

		Matrix33 rotMat(pDL->m_ObjMatrix.GetColumn0().GetNormalized() * pDL->m_ProbeExtents.x,
		                pDL->m_ObjMatrix.GetColumn1().GetNormalized() * pDL->m_ProbeExtents.y,
		                pDL->m_ObjMatrix.GetColumn2().GetNormalized() * pDL->m_ProbeExtents.z);
		Matrix34 mUnitVolumeToWorld = Matrix34::CreateTranslationMat(pDL->m_Origin) * rotMat * Matrix34::CreateScale(Vec3(2, 2, 2), Vec3(-1, -1, -1));

		DrawLightVolume(SHAPE_BOX, mUnitVolumeToWorld.GetTransposed());
	}
	else
	{
		rd->D3DSetCull(eCULL_Back, true);
		SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_pLBufferDiffuseRT->GetWidth(), m_pLBufferDiffuseRT->GetHeight(), pDepthBounds.x);
	}

	SD3DPostEffectsUtils::ShEndPass();

#ifdef SUPPORTS_MSAA
	if (nPass == 1)
	{
		rd->FX_MSAASampleFreqStencilSetup();
		PROFILE_LABEL_POP("DEFERRED_CUBEMAP_SAMPLEFREQPASS");
	}
}
#endif

#ifdef SUPPORTS_MSAA
	if (bMSAA)
		rRP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;
#endif

	if (bStencilMask)
	{
		rd->FX_StencilTestCurRef(false);
	}
	else if (rd->m_bDeviceSupports_NVDBT && CRenderer::CV_r_DeferredShadingDepthBoundsTest == 1)
		rd->SetDepthBoundTest(0.f, 1.f, false);

	if (rd->m_bDeviceSupports_NVDBT && bStencilMask && CRenderer::CV_r_DeferredShadingDepthBoundsTest && CRenderer::CV_r_deferredshadingDBTstencil)
		rd->SetDepthBoundTest(0.f, 1.f, false);

	SpecularAccEnableMRT(true);

	if (CRenderer::CV_r_DeferredShadingScissor)
		rd->EF_Scissor(false, 0, 0, 0, 0);

	rRP.m_FlagsShader_RT = nOldFlags;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DeferredShadingPass()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	const bool bMSAA = rd->m_RP.m_MSAAData.Type ? true : false;

	rd->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	rd->GetGraphicsPipeline().GetShadowMaskStage()->Prepare(m_pCurrentRenderView);
	rd->GetGraphicsPipeline().GetShadowMaskStage()->Execute();
	rd->GetGraphicsPipeline().SwitchToLegacyPipeline();

	rd->D3DSetCull(eCULL_Back, true); //fs quads should not revert test..

	PROFILE_LABEL_PUSH("DEFERRED_SHADING");

	CTexture* tmpTexSSS = CTexture::s_ptexSceneTargetR11G11B10F[0];

	const uint64 nFlagsShaderRT = rd->m_RP.m_FlagsShader_RT;
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | RT_CLIPVOLUME_ID);

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, DBT_SKY_CULL_DEPTH, true);

	// Deferred subsurface scattering
	bool deferredSSS = CRenderer::CV_r_DeferredShadingSSS && rd->FX_GetMSAAMode() == 0;  // Explicitly disabling deferred SSS has its incompatible with msaa in current stage

	if (deferredSSS)
		rd->FX_PushRenderTarget(1, tmpTexSSS, NULL);

	PROFILE_LABEL_PUSH("COMPOSITION");
	static CCryNameTSCRC techComposition("DeferredShadingPass");

#ifdef SUPPORTS_MSAA
	m_pNormalsRT->SetResolved(true); // normals was updated with shadows on alpha (would require another custom resolve to bind pre-resolved)
	rd->FX_MSAARestoreSampleMask();  // If stencil clear occurred, restore sample mask

	const int32 nNumPassesMSAA = (rd->FX_GetMSAAMode() == 1) ? 2 : 1;
	int32 nPrevStencilMaskRef = rd->m_nStencilMaskRef;
	rd->m_nStencilMaskRef = 0;

	for (int32 nPass = 0; nPass < nNumPassesMSAA; ++nPass)
	{
		if (nPass == 1)
		{
			PROFILE_LABEL_PUSH("SAMPLEFREQPASS");
			rd->FX_MSAASampleFreqStencilSetup(MSAA_SAMPLEFREQ_PASS);
		}
#endif

	if (deferredSSS)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];

	if (CRenderer::CV_r_DeferredShadingAreaLights)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

	const uint32 nNumClipVolumes = RenderView()->GetClipVolumes().size();
	if (nNumClipVolumes)
		rd->m_RP.m_FlagsShader_RT |= RT_CLIPVOLUME_ID;

	// Enable sun permutation (eg: when fully inside vis areas and sun not visible/used, skip sun computation)
	if (rd->m_RP.m_pSunLight)
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];

	SD3DPostEffectsUtils::ShBeginPass(m_pShader, techComposition, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	rd->FX_SetState(GS_NODEPTHTEST);
#ifdef SUPPORTS_MSAA
	if (bMSAA)
	{
		rd->m_RP.m_PersFlags2 |= RBPF2_READMASK_RESERVED_STENCIL_BIT;
		const int32 nMSAAStState = STENC_FUNC((rd->m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS) ? FSS_STENCFUNC_EQUAL : FSS_STENCFUNC_NOTEQUAL) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
		rd->FX_SetStencilState(nMSAAStState, BIT_STENCIL_RESERVED, BIT_STENCIL_RESERVED, 0xFF);
		rd->FX_SetState(rd->m_RP.m_CurState | GS_STENCIL);
	}
#endif

	m_pLBufferDiffuseRT->Apply(0, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA);
	m_pLBufferSpecularRT->Apply(1, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA);
	m_pDiffuseRT->Apply(2, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rd->m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pSpecularRT->Apply(3, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rd->m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pNormalsRT->Apply(4, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA);
	m_pDepthRT->Apply(5, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, EDefaultResourceViews::Default, bMSAA && (rd->m_RP.m_PersFlags2 & RBPF2_MSAA_SAMPLEFREQ_PASS));
	m_pResolvedStencilRT->Apply(6, EDefaultSamplerStates::PointClamp, EFTT_UNKNOWN, -1, -1);

	// Directional occlusion
	Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
	Vec4 ssdoParamsNull(0, 0, 0, 0);
	static CCryNameR ssdoParamsName("SSDOParams");
	m_pShader->FXSetPSFloat(ssdoParamsName, CRenderer::CV_r_ssdo ? &ssdoParams : &ssdoParamsNull, 1);
	CTexture::s_ptexSceneNormalsBent->Apply(7, EDefaultSamplerStates::PointClamp);   // todo: should this be msaaed or upscaled ?

	CTexture::s_ptexShadowMask->Apply(8, EDefaultSamplerStates::PointClamp);
	m_pDepthRT->Apply(9, EDefaultSamplerStates::PointClamp);

	Vec3 sunColor;
	gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);

	// Apply LBuffers range rescale
	sunColor.x *= rd->m_fAdaptedSceneScaleLBuffer;
	sunColor.y *= rd->m_fAdaptedSceneScaleLBuffer;
	sunColor.z *= rd->m_fAdaptedSceneScaleLBuffer;

	static CCryNameR paramNameSunColor("SunColor");
	const Vec4 paramSunColor(sunColor, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));
	m_pShader->FXSetPSFloat(paramNameSunColor, &paramSunColor, 1);

	static CCryNameR paramNameSunLightDir("SunLightDir");
	const Vec4 paramSunLightDir(rd->m_cEF.m_PF[rd->m_RP.m_nProcessThreadID].pSunDirection, 1.0f);
	m_pShader->FXSetPSFloat(paramNameSunLightDir, &paramSunLightDir, 1);

	const Vec4 vParamsHDRRange(0.0f, 0.0f, 0.0f, rd->m_fAdaptedSceneScale / (rd->m_fAdaptedSceneScaleLBuffer + FLT_MIN));  // packed rescale
	m_pShader->FXSetPSFloat(m_pGeneralParams, &vParamsHDRRange, 1);
	if (nNumClipVolumes)
		m_pShader->FXSetPSFloat(m_pClipVolumeParams, m_vClipVolumeParams, min(static_cast<uint32>(CClipVolumesStage::MaxDeferredClipVolumes), static_cast<uint32>(nNumClipVolumes + CClipVolumesStage::VisAreasOutdoorStencilOffset)));

	const float SunSourceDiameter = 94.0f;    // atan(AngDiameterSun) * 2 * SunDistance, where AngDiameterSun=0.54deg and SunDistance=10000
	static CCryNameR arealightMatrixName("g_AreaLightMatrix");
	Matrix44 mAreaLightMatrix;
	mAreaLightMatrix.SetIdentity();
	mAreaLightMatrix.SetRow4(3, Vec4(SunSourceDiameter, SunSourceDiameter, 0, 1.0f));
	m_pShader->FXSetPSFloat(arealightMatrixName, alias_cast<Vec4*>(&mAreaLightMatrix), 4);

	SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_pLBufferDiffuseRT->GetWidth(), m_pLBufferDiffuseRT->GetHeight(), 0, &gcpRendD3D->m_FullResRect);
	SD3DPostEffectsUtils::ShEndPass();

#ifdef SUPPORTS_MSAA
		if (nPass == 1)
		{
			rd->FX_MSAASampleFreqStencilSetup();
			PROFILE_LABEL_POP("SAMPLEFREQPASS");
		}
	}

	if (bMSAA)
		rd->m_RP.m_PersFlags2 &= ~RBPF2_READMASK_RESERVED_STENCIL_BIT;

	rd->FX_StencilTestCurRef(false);
	rd->m_nStencilMaskRef = nPrevStencilMaskRef;
#endif

	PROFILE_LABEL_POP("COMPOSITION");

	if (CRenderer::CV_r_DeferredShadingDepthBoundsTest)
		rd->SetDepthBoundTest(0.0f, DBT_SKY_CULL_DEPTH, false);

	if (deferredSSS)
	{
		rd->FX_PopRenderTarget(1);
		rd->FX_SetActiveRenderTargets();
	}

	rd->m_RP.m_FlagsShader_RT = nFlagsShaderRT;

	PROFILE_LABEL_POP("DEFERRED_SHADING");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DeferredLights(RenderLightsList& rLights, bool bCastShadows)
{
	if (rLights.size())
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTS");

		if (bCastShadows)
		{
			PackAllShadowFrustums(false);
		}

		auto stp = rLights.end();
		auto itr = rLights.begin();
		for (; itr != stp; ++itr)
		{
			const SRenderLight& pDL = *itr;
			if (pDL.m_Flags & (DLF_FAKE | DLF_VOLUMETRIC_FOG_ONLY))
				continue;

			assert(pDL.GetSpecularCubemap() == NULL);
			if (!(pDL.m_Flags & DLF_CASTSHADOW_MAPS))
			{
				LightPass(&pDL);
			}

			m_nLightsProcessedCount++;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::PackAllShadowFrustums(bool bPreLoop)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRY_ASSERT(rd->m_pRT->IsRenderThread());

	const uint64 nPrevFlagsShaderRT = rd->m_RP.m_FlagsShader_RT;

	static ICVar* p_e_ShadowsPoolSize = iConsole->GetCVar("e_ShadowsPoolSize");

	RenderLightsList& arrLights = m_pCurrentRenderView->GetLightsArray(eDLT_DeferredLight);

#ifndef _RELEASE
	rd->m_RP.m_PS[m_nThreadID].m_NumShadowPoolFrustums = 0;
	rd->m_RP.m_PS[m_nThreadID].m_NumShadowPoolAllocsThisFrame = 0;
	rd->m_RP.m_PS[m_nThreadID].m_NumShadowMaskChannels = 0;
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

	if (bPreLoop)
	{
		m_bClearPool = (CRenderer::CV_r_ShadowPoolMaxFrames > 0) ? false : true;

		m_nCurrentShadowPoolLight = 0;
		m_nFirstCandidateShadowPoolLight = 0;
	}

	bool bShadowRendered = false;

	auto nLightId = 0;
	auto itrStop = arrLights.end();
	auto itrCurr = arrLights.begin();

	// light pass here
	if (!bPreLoop)
	{
		nLightId = m_nFirstCandidateShadowPoolLight;
		while (--nLightId >= 0)
			++itrCurr;

		for (uint nLightPacked = m_nFirstCandidateShadowPoolLight; nLightPacked < m_nCurrentShadowPoolLight; ++itrCurr, ++nLightPacked)
		{
			const SRenderLight& rLight = *itrCurr;
			if (rLight.m_Flags & DLF_FAKE)
				continue;
			ShadowLightPasses(rLight, nLightPacked);
		}
	}

	// TODO: Sort lights so allocated ones are rendered first to reduce impact of a thrash

	for (; itrCurr != itrStop; ++itrCurr) //pre-loop to avoid 0.5 ms restore/resolve
	{
		const SRenderLight& rLight = *itrCurr;
		if (!(rLight.m_Flags & DLF_DIRECTIONAL) && (rLight.m_Flags & DLF_CASTSHADOW_MAPS))
			break;

		++m_nCurrentShadowPoolLight;
	}

	if (bPreLoop && (itrCurr != itrStop)) // Shadow allocation tick, free old shadows.
	{
		uint32 nAllocs = m_shadowPoolAlloc.Num();
		for (uint32 i = 0; i < nAllocs; i++)
		{
			SShadowAllocData* pAlloc = &m_shadowPoolAlloc[i];
			uint32 currFrame = gcpRendD3D->GetFrameID(false) & 0xFF;
			if (!pAlloc->isFree() && ((currFrame - pAlloc->m_frameID) > (uint)CRenderer::CV_r_ShadowPoolMaxFrames))
			{
				m_blockPack.RemoveBlock(pAlloc->m_blockID);
				pAlloc->Clear();
				break;  //Max one delete per frame, this should spread updates across more frames
			}
		}
	}

	if (!bPreLoop || !bShadowRendered)
	{
		while (itrCurr != itrStop)
		{
			auto itrNext = itrCurr;
			m_nFirstCandidateShadowPoolLight = m_nCurrentShadowPoolLight;

			// init before shadowgen
			SetupPasses();
			rd->FX_ResetPipe();
			rd->EF_Scissor(false, 0, 0, 0, 0);
			rd->SetDepthBoundTest(0.0f, 1.0f, false);

			{

				if (!bPreLoop)
					ResolveCurrentBuffers();

				while (itrNext != itrStop)
				{
					SRenderLight& rLight = *itrNext;
					if (!(rLight.m_Flags & (DLF_DIRECTIONAL | DLF_FAKE)) && (rLight.m_Flags & DLF_CASTSHADOW_MAPS))
					{

						bool bPacked = PackToPool(&m_blockPack, rLight, m_nCurrentShadowPoolLight, m_nFirstCandidateShadowPoolLight, m_bClearPool);
						m_bClearPool = !bPacked;
						if (!bPacked)
						{
							break;
						}
					}
					bShadowRendered = true;

					++m_nCurrentShadowPoolLight;
					++itrNext;
				}

	#ifndef _RELEASE
				{
					uint32 nAllocs = m_shadowPoolAlloc.Num();
					for (uint32 i = 0; i < nAllocs; i++)
					{
						if (!m_shadowPoolAlloc[i].isFree())
						{
							rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumShadowPoolFrustums++;
						}
					}
				}
	#endif
			}

			if (!bPreLoop && bShadowRendered)
			{
				RestoreCurrentBuffers();

				//insert light pass here
				for (uint nLightPacked = m_nFirstCandidateShadowPoolLight; itrCurr != itrNext; ++itrCurr, ++nLightPacked)
				{
					const SRenderLight& rLight = *itrCurr;
					if (rLight.m_Flags & DLF_FAKE)
						continue;

					ShadowLightPasses(rLight, nLightPacked);
				}
			}

			itrCurr = itrNext;
		}
	}

	rd->m_RP.m_FlagsShader_RT = nPrevFlagsShaderRT;

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
		int nFrameID = gcpRendD3D->GetFrameID(false);
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

	uint32 currFrame = gcpRendD3D->GetFrameID(false) & 0xFF;

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

#ifndef _RELEASE
		rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumShadowPoolAllocsThisFrame++;
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
			pAlloc->m_frameID = (nMGPUUpdate == -1) ? gcpRendD3D->GetFrameID(false) & 0xFF : nMGPUUpdate;
#else
			pAlloc->m_frameID = gcpRendD3D->GetFrameID(false) & 0xFF;
#endif
			bClearPool = true;
		}
		else
		{

#ifndef _RELEASE // failed alloc, will thrash!
			if (CRenderer::CV_r_ShadowPoolMaxFrames != 0 || CRenderer::CV_r_DeferredShadingTiled > 1)
				rd->m_RP.m_PS[rd->m_RP.m_nProcessThreadID].m_NumShadowPoolAllocsThisFrame |= 0x80000000;
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
	rd->FX_PopRenderTarget(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::RestoreCurrentBuffers()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	rd->FX_PushRenderTarget(1, m_pLBufferSpecularRT, NULL, -1, false, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeferredShading::ShadowLightPasses(const SRenderLight& light, const int nLightID)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CRenderView* pRenderView = RenderView();

	float scissorInt2Float[] = { (float)(light.m_sX), (float)(light.m_sY), (float)(light.m_sWidth), (float)(light.m_sHeight) };

	rd->m_RP.m_nDeferredPrimitiveID = SHAPE_PROJECTOR;
	int nDLights = RenderView()->GetDynamicLightsCount();

	//////////////////////////////////////////////////////////////////////////
	int nFrustumIdx = nLightID + nDLights;
	auto& SMFrustums = pRenderView->GetShadowFrustumsForLight(nFrustumIdx);

	assert((unsigned int) nFrustumIdx < (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS));
	if ((unsigned int) nFrustumIdx >= (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS))
	{
		//Warning("CDeferredShading::ShadowLightPasses: Too many light sources used ..."); // redundant
		return false;
	}

	//no single frustum was allocated for this light
	if (SMFrustums.empty())
		return false;

	if (pRenderView->IsRecursive())
		return false;

	// Area lights are a non-uniform box, not a cone in 1 of 6 directions, so we skip clipping/stencil testing and let the light pass take care of it.
	bool bAreaLight = (light.m_Flags & DLF_AREA_LIGHT) && light.m_fAreaWidth && light.m_fAreaHeight && light.m_fLightFrustumAngle && CRenderer::CV_r_DeferredShadingAreaLights;

	ShadowMapFrustum& firstFrustum = *SMFrustums.front()->pFrustum;

	int nSides = 1;
	if (firstFrustum.bOmniDirectionalShadow && !bAreaLight)
		nSides = 6;

	//enable shadow mapping
	rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];

	//enable hw-pcf per frustum
	if (firstFrustum.bHWPCFCompare)
	{
		rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];
	}

	Vec4 pLightRect = Vec4(scissorInt2Float[0], scissorInt2Float[1], scissorInt2Float[2], scissorInt2Float[3]);
	Vec4 scaledLightRect = Vec4(pLightRect.x * rd->m_RP.m_CurDownscaleFactor.x, pLightRect.y * rd->m_RP.m_CurDownscaleFactor.y,
	                            pLightRect.z * rd->m_RP.m_CurDownscaleFactor.x, pLightRect.w * rd->m_RP.m_CurDownscaleFactor.y);

	if (!bAreaLight)
	{
		rd->m_nStencilMaskRef += (nSides + 2);
		if (rd->m_nStencilMaskRef > STENC_MAX_REF)
		{
			rd->EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL);
			rd->m_nStencilMaskRef = nSides + 1;
		}
	}

	uint16 scaledX = 0, scaledY = 0, scaledWidth = 0, scaledHeight = 0;

	for (int nS = 0; nS < nSides; nS++)
	{
		uint32 nPersFlagsPrev = rd->m_RP.m_TI[m_nThreadID].m_PersFlags;
		rd->m_RP.m_TI[m_nThreadID].m_PersFlags |= RBPF_MIRRORCULL;

		SpecularAccEnableMRT(false);

		if (CRenderer::CV_r_DeferredShadingDepthBoundsTest == 1 && !bAreaLight)
		{
			Vec4 pDepthBounds = GetLightDepthBounds(&light, (rd->m_RP.m_TI[m_nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0);
			rd->SetDepthBoundTest(pDepthBounds.x, pDepthBounds.z, true);
		}

		if (nS == 0)
		{
			scaledX = (uint16)(scaledLightRect.x);
			scaledY = (uint16)(scaledLightRect.y);
			scaledWidth = (uint16)(scaledLightRect.z) + 1;
			scaledHeight = (uint16)(scaledLightRect.w) + 1;
		}

		if (!bAreaLight)
		{
			//use current WorldProj matrix
			rd->FX_StencilFrustumCull(-2, &light, &firstFrustum, nS);

			rd->SetDepthBoundTest(0, 1, false);
		}

		rd->m_RP.m_TI[m_nThreadID].m_PersFlags = nPersFlagsPrev;
		if (!bAreaLight)
		{
			rd->FX_StencilTestCurRef(true, true);
		}

		SetupPasses();

		if (!bAreaLight)
			m_nRenderState |= GS_STENCIL;

		SpecularAccEnableMRT(true);

		if (firstFrustum.nShadowGenMask & (1 << nS))
		{
			rd->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
			rd->ConfigShadowTexgen(0, &firstFrustum, nS);

			if (firstFrustum.bUseShadowsPool)
				CTexture::s_ptexRT_ShadowPool->Apply(3, firstFrustum.bHWPCFCompare ? EDefaultSamplerStates::LinearCompare : EDefaultSamplerStates::PointCompare);
			else
				SD3DPostEffectsUtils::SetTexture(firstFrustum.pDepthTex, 3, FILTER_POINT, eSamplerAddressMode_Wrap);

			SD3DPostEffectsUtils::SetTexture(CTexture::s_ptexShadowJitterMap, 7, FILTER_POINT, eSamplerAddressMode_Wrap);
		}
		else
		{
			rd->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE4];
		}

		//////////////////////////////////////////////////////////////////////////

		int nShadowQuality = rd->FX_ApplyShadowQuality();

		m_nCurLighID = nLightID;

		LightPass(&light, true);

		if (!bAreaLight)
			rd->FX_StencilTestCurRef(false);
	}

	//assign range
	if (!bAreaLight)
		rd->m_nStencilMaskRef += nSides;
	//////////////////////////////////////////////////////////////////////////
	rd->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE]);

	if (!bAreaLight)
		m_nRenderState &= ~GS_STENCIL;
	rd->FX_SetState(m_nRenderState);

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::CreateDeferredMaps()
{
	static uint32 nPrevLBuffersFmt = CRenderer::CV_r_DeferredShadingLBuffersFmt;
	if (!CTexture::s_ptexSceneNormalsMap ||
		!CTexture::s_ptexSceneNormalsMap->GetDevTexture() ||
		 CTexture::s_ptexSceneNormalsMap->GetDevTexture()->IsMSAAChanged()
	    || CTexture::s_ptexSceneNormalsMap->GetWidth() != gcpRendD3D->m_MainViewport.nWidth
	    || CTexture::s_ptexSceneNormalsMap->GetHeight() != gcpRendD3D->m_MainViewport.nHeight
	    || nPrevLBuffersFmt != CRenderer::CV_r_DeferredShadingLBuffersFmt
	    || (gRenDev->IsStereoEnabled() && !CTexture::s_ptexVelocityObjects[1]->GetDevTexture()))
	{
		nPrevLBuffersFmt = CRenderer::CV_r_DeferredShadingLBuffersFmt;

		const int width  = gRenDev->GetWidth(),  width_r2  = (width  + 1) / 2, width_r4  = (width_r2  + 1) / 2, width_r8  = (width_r4  + 1) / 2;
		const int height = gRenDev->GetHeight(), height_r2 = (height + 1) / 2, height_r4 = (height_r2 + 1) / 2, height_r8 = (height_r4 + 1) / 2;
		int32 nMSAAUsageFlag = CRenderer::CV_r_msaa ? FT_USAGE_MSAA : 0;

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneNormalsMap", CTexture::s_ptexSceneNormalsMap, width, height, Clr_Unknown, true, false, eTF_R8G8B8A8, TO_SCENE_NORMALMAP, nMSAAUsageFlag | FT_USAGE_ALLOWREADSRGB);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneNormalsBent", CTexture::s_ptexSceneNormalsBent, width, height, Clr_Median, true, false, eTF_R8G8B8A8);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$AOColorBleed", CTexture::s_ptexAOColorBleed, width_r8, height_r8, Clr_Unknown, true, false, eTF_R8G8B8A8);

		ETEX_Format fmtZScaled = eTF_R16G16B16A16F;
		ETEX_Format nTexFormat = eTF_R16G16B16A16F;
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		if (CRenderer::CV_r_DeferredShadingLBuffersFmt == 1)
			nTexFormat = eTF_R11G11B10F;
#endif

		int nFTFlags = nMSAAUsageFlag;

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneDiffuseAcc", CTexture::s_ptexSceneDiffuseAccMap, width, height, Clr_Transparent, true, false, nTexFormat, TO_SCENE_DIFFUSE_ACC, nFTFlags);
		CTexture::s_ptexCurrentSceneDiffuseAccMap = CTexture::s_ptexSceneDiffuseAccMap;
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecularAcc", CTexture::s_ptexSceneSpecularAccMap, width, height, Clr_Transparent, true, false, nTexFormat, TO_SCENE_SPECULAR_ACC, nFTFlags);

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneDiffuse", CTexture::s_ptexSceneDiffuse, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, nFTFlags | FT_USAGE_ALLOWREADSRGB);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecular", CTexture::s_ptexSceneSpecular, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, nFTFlags | FT_USAGE_ALLOWREADSRGB);
#if defined(DURANGO_USE_ESRAM)
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$SceneSpecularESRAM", CTexture::s_ptexSceneSpecularESRAM, width, height, Clr_Empty, true, false, eTF_R8G8B8A8, -1, nFTFlags);
#endif

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$VelocityObjects", CTexture::s_ptexVelocityObjects[0], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, nFTFlags);
		if (gRenDev->IsStereoEnabled())
		{
			SD3DPostEffectsUtils::GetOrCreateRenderTarget("$VelocityObject_R", CTexture::s_ptexVelocityObjects[1], width, height, Clr_Transparent, true, false, eTF_R16G16F, -1, nFTFlags);
		}

		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled" , CTexture::s_ptexZTargetScaled[0], width_r2, height_r2, Clr_FarPlane, 1, 0, fmtZScaled, TO_DOWNSCALED_ZTARGET_FOR_AO);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled2", CTexture::s_ptexZTargetScaled[1], width_r4, height_r4, Clr_FarPlane, 1, 0, fmtZScaled, TO_QUARTER_ZTARGET_FOR_AO);
		SD3DPostEffectsUtils::GetOrCreateRenderTarget("$ZTargetScaled3", CTexture::s_ptexZTargetScaled[2], width_r8, height_r8, Clr_FarPlane, 1, 0, fmtZScaled);

		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$DepthBufferQuarter", CTexture::s_ptexDepthBufferQuarter, width_r4, height_r4, Clr_FarPlane, false, false, eTF_D32F, -1, FT_USAGE_DEPTHSTENCIL);
		SD3DPostEffectsUtils::GetOrCreateDepthStencil("$DepthBufferHalfQuarter", CTexture::s_ptexDepthBufferHalfQuarter, width_r8, height_r8, Clr_FarPlane, false, false, eTF_D32F, -1, FT_USAGE_DEPTHSTENCIL);
	}

	// Pre-create shadow pool
	IF (gcpRendD3D->m_pRT->IsRenderThread() && gEnv->p3DEngine, 1)
	{
		//init shadow pool size
		static ICVar* p_e_ShadowsPoolSize = iConsole->GetCVar("e_ShadowsPoolSize");
		gcpRendD3D->m_nShadowPoolHeight = p_e_ShadowsPoolSize->GetIVal();
		gcpRendD3D->m_nShadowPoolWidth = gcpRendD3D->m_nShadowPoolHeight; //square atlas

		ETEX_Format eShadTF = gcpRendD3D->CV_r_shadowtexformat == 1 ? eTF_D16 : eTF_D32F;
		CTexture::s_ptexRT_ShadowPool->Invalidate(gcpRendD3D->m_nShadowPoolWidth, gcpRendD3D->m_nShadowPoolHeight, eShadTF);
		if (!CTexture::IsTextureExist(CTexture::s_ptexRT_ShadowPool))
		{
			CTexture::s_ptexRT_ShadowPool->CreateDepthStencil(eTF_Unknown, Clr_FarPlane);
		}
	}

	gcpRendD3D->GetTiledShading().CreateResources();

	// shadow mask
	{
		if (CTexture::s_ptexShadowMask)
			CTexture::s_ptexShadowMask->Invalidate(gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight(), eTF_R8);

		if (!CTexture::IsTextureExist(CTexture::s_ptexShadowMask))
		{
			const int nArraySize = gcpRendD3D->CV_r_ShadowCastingLightsMaxCount;
			CTexture::s_ptexShadowMask = CTexture::GetOrCreateTextureArray("$ShadowMask", gcpRendD3D->GetWidth(), gcpRendD3D->GetHeight(), nArraySize, 1, eTT_2DArray, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8, TO_SHADOWMASK);
		}
	}

	// height map AO mask
	if (CRenderer::CV_r_HeightMapAO > 0)
	{
		const int shift = clamp_tpl(3 - CRenderer::CV_r_HeightMapAO, 0, 2);
		
		int hmaoWidth = gcpRendD3D->GetWidth();
		int hmaoHeight = gcpRendD3D->GetHeight();
		for (int i = 0; i < shift; i++)
		{
			hmaoWidth = (hmaoWidth + 1) / 2;
			hmaoHeight = (hmaoHeight + 1) / 2;
		}

		for (int i = 0; i < 2; ++i)
		{
			if (CTexture::s_ptexHeightMapAO[i])
				CTexture::s_ptexHeightMapAO[i]->Invalidate(hmaoWidth, hmaoHeight, eTF_R8G8);

			if (!CTexture::IsTextureExist(CTexture::s_ptexHeightMapAO[i]))
			{
				char buf[128];
				cry_sprintf(buf, "$HeightMapAO_%d", i);

				SD3DPostEffectsUtils::GetOrCreateRenderTarget(buf, CTexture::s_ptexHeightMapAO[i], hmaoWidth, hmaoHeight, Clr_Neutral, true, false, eTF_R8G8);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::DestroyDeferredMaps()
{
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
	m_pCurrentRenderView = pRenderView;

	m_nLightsProcessedCount = 0;

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	uint64 nFlagsShaderRT = rd->m_RP.m_FlagsShader_RT;

	rd->FX_ResetPipe();

	SetupPasses();

	auto& rDeferredLights = pRenderView->GetLightsArray(eDLT_DeferredLight);
	auto& rDeferredCubemaps = pRenderView->GetLightsArray(eDLT_DeferredCubemap);
	auto& rDeferredAmbientLights = pRenderView->GetLightsArray(eDLT_DeferredAmbientLight);

#ifdef SUPPORTS_MSAA
	if (rd->m_RP.m_MSAAData.Type)
	{
		m_pNormalsRT->SetResolved(true);
		m_pDepthRT->SetResolved(true);
	}
#endif

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
		gcpRendD3D->m_RP.m_FlagsShader_RT |= RT_OVERDRAW_DEBUG;
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

		DeferredCubemaps(rDeferredCubemaps, nCurrentDeferredCubemap);

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

#ifdef SUPPORTS_MSAA
	if (rd->m_RP.m_MSAAData.Type)
	{
		// Commit any potential render target changes - required for when shadows disabled
		rd->FX_SetActiveRenderTargets();

		rd->FX_MSAASampleFreqStencilSetup();
		// Ensure no redundant resolves happen for MSAA
		m_pLBufferDiffuseRT->SetResolved(true);
		m_pLBufferSpecularRT->SetResolved(true);
		m_pNormalsRT->SetResolved(true);
	}
#endif

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

	rd->m_RP.m_FlagsShader_RT = nFlagsShaderRT;
	rd->m_RP.m_PersFlags2 &= ~RBPF2_WRITEMASK_RESERVED_STENCIL_BIT;

	rd->FX_ResetPipe();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::Release()
{
	DestroyDeferredMaps();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::GetScissors(const Vec3& vCenter, float fRadius, short& sX, short& sY, short& sWidth, short& sHeight) const
{
	Vec3 vViewVec = vCenter - gcpRendD3D->GetCamera().GetPosition();
	float fDistToLS = vViewVec.GetLength();

	bool bInsideLightVolume = fDistToLS <= fRadius;
	if (bInsideLightVolume)
	{
		sX = sY = 0;
		sWidth = gcpRendD3D->GetWidth();
		sHeight = gcpRendD3D->GetHeight();
		return;
	}

	Matrix44 mProj, mView;
	gcpRendD3D->GetProjectionMatrix(mProj.GetData());
	gcpRendD3D->GetModelViewMatrix(mView.GetData());

	Vec3 pBRectVertices[4];
	Vec4 vCenterVS = Vec4(vCenter, 1) * mView;

	{
		// Compute tangent planes
		float r = fRadius;
		float sq_r = r * r;

		Vec3 vLPosVS = Vec3(vCenterVS.x, vCenterVS.y, vCenterVS.z);
		float lx = vLPosVS.x;
		float ly = vLPosVS.y;
		float lz = vLPosVS.z;
		float sq_lx = lx * lx;
		float sq_ly = ly * ly;
		float sq_lz = lz * lz;

		// Compute left and right tangent planes to light sphere
		float sqrt_d = sqrt_tpl(max(sq_r * sq_lx - (sq_lx + sq_lz) * (sq_r - sq_lz), 0.0f));
		float nx = (r * lx + sqrt_d) / (sq_lx + sq_lz);
		float nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;

		Vec3 vTanLeft = Vec3(nx, 0, nz).normalized();

		nx = (r * lx - sqrt_d) / (sq_lx + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - nx * lx) / lz;
		Vec3 vTanRight = Vec3(nx, 0, nz).normalized();

		pBRectVertices[0] = vLPosVS - r * vTanLeft;
		pBRectVertices[1] = vLPosVS - r * vTanRight;

		// Compute top and bottom tangent planes to light sphere
		sqrt_d = sqrt_tpl(max(sq_r * sq_ly - (sq_ly + sq_lz) * (sq_r - sq_lz), 0.0f));
		float ny = (r * ly - sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanBottom = Vec3(0, ny, nz).normalized();

		ny = (r * ly + sqrt_d) / (sq_ly + sq_lz);
		nz = iszero(lz) ? 1.0f : (r - ny * ly) / lz;
		Vec3 vTanTop = Vec3(0, ny, nz).normalized();

		pBRectVertices[2] = vLPosVS - r * vTanTop;
		pBRectVertices[3] = vLPosVS - r * vTanBottom;
	}

	Vec2 vPMin = Vec2(1, 1);
	Vec2 vPMax = Vec2(0, 0);
	Vec2 vMin = Vec2(1, 1);
	Vec2 vMax = Vec2(0, 0);

	// Project all vertices
	for (int i = 0; i < 4; i++)
	{
		Vec4 vScreenPoint = Vec4(pBRectVertices[i], 1.0) * mProj;

		//projection space clamping
		vScreenPoint.w = max(vScreenPoint.w, 0.00000000000001f);
		vScreenPoint.x = max(vScreenPoint.x, -(vScreenPoint.w));
		vScreenPoint.x = min(vScreenPoint.x, vScreenPoint.w);
		vScreenPoint.y = max(vScreenPoint.y, -(vScreenPoint.w));
		vScreenPoint.y = min(vScreenPoint.y, vScreenPoint.w);

		//NDC
		vScreenPoint /= vScreenPoint.w;

		//output coords
		//generate viewport (x=0,y=0,height=1,width=1)
		Vec2 vWin;
		vWin.x = (1.0f + vScreenPoint.x) * 0.5f;
		vWin.y = (1.0f + vScreenPoint.y) * 0.5f;   //flip coords for y axis

		assert(vWin.x >= 0.0f && vWin.x <= 1.0f);
		assert(vWin.y >= 0.0f && vWin.y <= 1.0f);

		// Get light sphere screen bounds
		vMin.x = min(vMin.x, vWin.x);
		vMin.y = min(vMin.y, vWin.y);
		vMax.x = max(vMax.x, vWin.x);
		vMax.y = max(vMax.y, vWin.y);
	}

	float fWidth = (float)gcpRendD3D->GetWidth();
	float fHeight = (float)gcpRendD3D->GetHeight();

	sX = (short)(vMin.x * fWidth);
	sY = (short)((1.0f - vMax.y) * fHeight);
	sWidth = (short)ceilf((vMax.x - vMin.x) * fWidth);
	sHeight = (short)ceilf((vMax.y - vMin.y) * fHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::SetupScissors(bool bEnable, uint16 x, uint16 y, uint16 w, uint16 h) const
{
	gcpRendD3D->EF_Scissor(bEnable, x, y, w, h);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CDeferredShading::Debug()
{
	uint64 nFlagsShaderRT = gcpRendD3D->m_RP.m_FlagsShader_RT;

	if (CRenderer::CV_r_DeferredShadingDebug == 2)
	{
#ifdef SUPPORTS_MSAA
		if (gcpRendD3D->m_RP.m_MSAAData.Type)
		{
			// Explicit msaa resolve, so that non-msaa texture bind is valid for debug
			m_pLBufferDiffuseRT->SetResolved(false);
			m_pLBufferDiffuseRT->Resolve();
		}
#endif
		SD3DPostEffectsUtils::ShBeginPass(m_pShader, m_pDebugTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
		m_pLBufferDiffuseRT->Apply(0, EDefaultSamplerStates::PointClamp);
		SD3DPostEffectsUtils::SetTexture(CTexture::s_ptexPaletteDebug, 1, FILTER_LINEAR, eSamplerAddressMode_Clamp);
		SD3DPostEffectsUtils::DrawFullScreenTriWPOS(m_pLBufferDiffuseRT->GetWidth(), m_pLBufferDiffuseRT->GetHeight());
		SD3DPostEffectsUtils::ShEndPass();
	}

	gcpRendD3D->m_RP.m_FlagsShader_RT = nFlagsShaderRT;
}

void CDeferredShading::DebugGBuffer()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	static CCryNameTSCRC techShadingDebug("DebugGBuffer");

	CTexture* dstTex = CTexture::s_ptexStereoR;

	m_pDepthRT->Apply(0, EDefaultSamplerStates::PointClamp);
	m_pNormalsRT->Apply(1, EDefaultSamplerStates::PointClamp);
	m_pDiffuseRT->Apply(2, EDefaultSamplerStates::PointClamp);
	m_pSpecularRT->Apply(3, EDefaultSamplerStates::PointClamp);

	rd->FX_SetState(GS_NODEPTHTEST);

	rd->FX_PushRenderTarget(0, dstTex, NULL);
	SD3DPostEffectsUtils::ShBeginPass(m_pShader, techShadingDebug, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	static CCryNameR paramName("DebugViewMode");
	Vec4 param(CRenderer::CV_r_DeferredShadingDebugGBuffer, 0, 0, 0);
	m_pShader->FXSetPSFloat(paramName, &param, 1);
	SD3DPostEffectsUtils::DrawFullScreenTri(dstTex->GetWidth(), dstTex->GetHeight());
	SD3DPostEffectsUtils::ShEndPass();
	rd->FX_PopRenderTarget(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int CRenderer::EF_AddDeferredLight(const CDLight& pLight, float fMult, const SRenderingPassInfo& passInfo)
{
	CDeferredShading& pDS = CDeferredShading::Instance();
	RenderLightIndex nLightID = pDS.AddLight(pLight, fMult, passInfo);

	int nThreadID = m_RP.m_nFillThreadID;
	float mipFactor = (m_RP.m_TI[nThreadID].m_cam.GetPosition() - pLight.m_Origin).GetLengthSquared() / max(0.001f, pLight.m_fRadius * pLight.m_fRadius);
	EF_PrecacheResource(const_cast<CDLight*>(&pLight), mipFactor, 0.1f, FPR_STARTLOADING, gRenDev->m_RP.m_TI[nThreadID].m_arrZonesRoundId[1]);
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

bool CD3D9Renderer::FX_DeferredRendering(CRenderView* pRenderView, bool bDebugPass, bool bUpdateRTOnly)
{
	CDeferredShading& pDS = CDeferredShading::Instance();

	if (!CTexture::s_ptexSceneTarget)
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

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CD3D9Renderer::FX_DeferredDecals()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (CV_r_deferredDecals)
	{
		auto& deferredDecals = m_RP.RenderView()->GetDeferredDecals();
#if !CRY_PLATFORM_ORBIS
		// Want the buffer cleared or we'll just get black out
		if (deferredDecals.empty())
			return false;
#endif

		PROFILE_LABEL_SCOPE("DEFERRED_DECALS");

		CDeferredShading& rDS = CDeferredShading::Instance();
		rDS.SetupPasses();

		D3DTexture* pBBRes = CTexture::s_ptexBackBuffer->GetDevTexture()->Get2DTexture();
		D3DTexture* pNMRes = NULL;

		assert(CTexture::s_ptexBackBuffer->m_pPixelFormat->DeviceFormat == CTexture::s_ptexSceneNormalsMap->m_pPixelFormat->DeviceFormat);

		if (rd->m_RP.m_MSAAData.Type > 1) //always copy when deferredDecals is not empty
		{
			pNMRes = CTexture::s_ptexSceneNormalsMap->GetDevTexture()->GetMSAATexture()->Get2DTexture();
#ifdef RENDERER_ENABLE_LEGACY_PIPELINE
			rd->GetDeviceContext().ResolveSubresource(pBBRes, 0, pNMRes, 0, (DXGI_FORMAT)CTexture::s_ptexBackBuffer->m_pPixelFormat->DeviceFormat);
#endif
		}
		else
		{
			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(CTexture::s_ptexSceneNormalsMap->GetDevTexture(), CTexture::s_ptexBackBuffer->GetDevTexture());
		}

		std::stable_sort(deferredDecals.begin(), deferredDecals.end(), DeffDecalSort());

		//if (CV_r_deferredDecalsDebug == 0)
		//	rd->FX_PushRenderTarget(1, CTexture::s_ptexSceneNormalsMap, NULL);

		const size_t nNumDecals = deferredDecals.size();
		for (size_t d = 0; d < nNumDecals; ++d)
		{
			rDS.DeferredDecalPass(deferredDecals[d], d);
		}

		//if (CV_r_deferredDecalsDebug == 0)
		//	rd->FX_PopRenderTarget(1);

		rd->SetCullMode(R_CULL_BACK);

		// Commit any potential render target changes - required for when shadows disabled
		rd->FX_SetActiveRenderTargets();
		rd->FX_ResetPipe();

		return true;
	}
	return false;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CREDeferredShading::mfDraw(CShader* ef, SShaderPass* sfm)
{
	if (gcpRendD3D->m_bDeviceLost)
		return 0;

	gcpRendD3D->FX_DeferredRendering(gRenDev->m_RP.m_pCurrentRenderView);
	return true;
}

#pragma warning(pop)
