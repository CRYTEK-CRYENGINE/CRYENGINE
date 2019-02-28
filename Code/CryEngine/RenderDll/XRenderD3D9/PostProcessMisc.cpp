// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessMisc : misc post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"

#include "Common/RenderView.h"

#pragma warning(push)
#pragma warning(disable: 4244)

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CVolumetricScattering::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	// Get current viewport
	int iTempX, iTempY, iWidth, iHeight;
	gcpRendD3D->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Render god-rays into low-res render target for less fillrate hit

	gcpRendD3D->FX_ClearTarget(CRendererResources::s_ptexBackBufferScaled[1], Clr_Transparent);
	gcpRendD3D->FX_PushRenderTarget(0, CRendererResources::s_ptexBackBufferScaled[1], NULL);
	gcpRendD3D->RT_SetViewport(0, 0, CRendererResources::s_ptexBackBufferScaled[1]->GetWidth(), CRendererResources::s_ptexBackBufferScaled[1]->GetHeight());

	float fAmount = m_pAmount->GetParam();
	float fTilling = m_pTilling->GetParam();
	float fSpeed = m_pSpeed->GetParam();
	Vec4 pColor = m_pColor->GetParamVec4();

	static CCryNameTSCRC pTechName("VolumetricScattering");
	uint32 nPasses;
	CShaderMan::s_shPostEffects->FXSetTechnique(pTechName);
	CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETSTATES);

	gcpRendD3D->SetCullMode(R_CULL_NONE);
	gcpRendD3D->FX_SetState(GS_BLSRC_ONE | GS_BLDST_ONEMINUSSRCCOL | GS_NODEPTHTEST);

	int nSlicesCount = 10;

	Vec4 pParams;
	pParams = Vec4(fTilling, fSpeed, fTilling, fSpeed);

	static CCryNameR pParam0Name("VolumetricScattering");
	static CCryNameR pParam1Name("VolumetricScatteringColor");

	static CCryNameR pParam2Name("PI_volScatterParamsVS");
	static CCryNameR pParam3Name("PI_volScatterParamsPS");

	for (int r(0); r < nSlicesCount; ++r)
	{
		// !force updating constants per-pass! (dx10..)
		CShaderMan::s_shPostEffects->FXBeginPass(0);

		// Set PS default params
		Vec4 pParamsPI = Vec4(1.0f, fAmount, r, 1.0f / (float) nSlicesCount);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam0Name, &pParams, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam1Name, &pColor, 1);
		CShaderMan::s_shPostEffects->FXSetVSFloat(pParam2Name, &pParamsPI, 1);
		CShaderMan::s_shPostEffects->FXSetPSFloat(pParam3Name, &pParamsPI, 1);

		GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBufferScaled[1]->GetWidth(), CRendererResources::s_ptexBackBufferScaled[1]->GetHeight());

		CShaderMan::s_shPostEffects->FXEndPass();
	}

	CShaderMan::s_shPostEffects->FXEnd();

	// Restore previous viewport
	gcpRendD3D->FX_PopRenderTarget(0);
	gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Display volumetric scattering effect

	CCryNameTSCRC pTechName0("VolumetricScatteringFinal");

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName0, FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());
	GetUtils().ShEndPass();
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
void CPost3DRenderer::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("POST_3D_RENDERER");

	// Must update the RT pointers here, otherwise they can get out-of-date
	if (CRenderer::CV_r_UsePersistentRTForModelHUD > 0)
	{
		m_pFlashRT = CRendererResources::s_ptexModelHudBuffer;
	}
	else
	{
		m_pFlashRT = CRendererResources::s_ptexBackBuffer;
	}

	m_pTempRT = CRendererResources::s_ptexSceneDiffuse;
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	m_pTempRT = CRendererResources::s_ptexSceneNormalsBent; // non-msaaed target
#endif

	if (HasModelsToRender() && IsActive())
	{
		// Render model groups
		m_edgeFadeScale = (1.0f / clamp_tpl<float>(m_pEdgeFadeScale->GetParam(), 0.001f, 1.0f));
		m_post3DRendererflags |= eP3DR_DirtyFlashRT;
		m_groupCount = 1; // There must be at least 1 group, this will then get set the correct amount when processing the models
		for (uint8 groupId = 0; groupId < m_groupCount; groupId++)
		{
			RenderGroup(groupId);
		}
	}
	else
	{
		// Nothing to render, so clear Flash RT so that we don't render rubbish on the flash objects
		ClearFlashRT();
	}
*/
}

void CPost3DRenderer::ClearFlashRT()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("CLEAR_RT");

	const uint8 zbufferRenderTile = 2; // Use zbuffer render tile, because in stereo the 1st 2 are used for the eyes

	CClearSurfacePass::Execute(m_pFlashRT, Clr_Transparent);
*/
}

void CPost3DRenderer::RenderGroup(uint8 groupId)
{
	PROFILE_LABEL_SCOPE("RENDER_GROUP");

	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
}

void CPost3DRenderer::RenderMeshes(uint8 groupId, float screenRect[4], ERenderMeshMode renderMeshMode)
{
	// OLD PIPELINE
	ASSERT_LEGACY_PIPELINE
}

void CPost3DRenderer::AlphaCorrection()
{
	ASSERT_LEGACY_PIPELINE
}

void CPost3DRenderer::GammaCorrection(float screenRect[4])
{
	ASSERT_LEGACY_PIPELINE
}

void CPost3DRenderer::RenderSilhouettes(uint8 groupId, float screenRect[4])
{
	PROFILE_LABEL_SCOPE("SILHOUTTES");

	RenderMeshes(groupId, screenRect, eRMM_Custom);

	CTexture* pOutlineTex = CRendererResources::s_ptexBackBufferScaled[0];
	CTexture* pGlowTex = CRendererResources::s_ptexBackBufferScaled[1];

	uint64 nRTMaskQ = ApplyShaderQuality();
	SilhouetteOutlines(pOutlineTex, pGlowTex);
	SilhouetteGlow(pOutlineTex, pGlowTex);
	SilhouetteCombineBlurAndOutline(pOutlineTex, pGlowTex);
	GammaCorrection(screenRect);
}

void CPost3DRenderer::SilhouetteCombineBlurAndOutline(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	ASSERT_LEGACY_PIPELINE

}

void CPost3DRenderer::SilhouetteGlow(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	ASSERT_LEGACY_PIPELINE

}

void CPost3DRenderer::SilhouetteOutlines(CTexture* pOutlineTex, CTexture* pGlowTex)
{
	ASSERT_LEGACY_PIPELINE

}

uint64 CPost3DRenderer::ApplyShaderQuality(EShaderType shaderType)
{
	CD3D9Renderer* const __restrict pRD = gcpRendD3D;

	// Retrieve quality for shader type
	SShaderProfile* pSP = &pRD->m_cEF.m_ShaderProfiles[shaderType];
	int nQuality = (int)pSP->GetShaderQuality();

	// Apply correct flag set
	uint64 nRTMaskQ = 0;

	SRenderQuality rq = gRenDev->GetRenderQuality();
	rq.shaderQuality = pSP->GetShaderQuality();
	gRenDev->SetRenderQuality(rq);

	switch (nQuality)
	{
	case eSQ_Medium:
		nRTMaskQ |= g_HWSR_MaskBit[HWSR_QUALITY];
		break;
	case eSQ_High:
		nRTMaskQ |= g_HWSR_MaskBit[HWSR_QUALITY1];
		break;
	case eSQ_VeryHigh:
		nRTMaskQ |= g_HWSR_MaskBit[HWSR_QUALITY];
		nRTMaskQ |= g_HWSR_MaskBit[HWSR_QUALITY1];
		break;
	}
	return nRTMaskQ;
}

#pragma warning(pop)
