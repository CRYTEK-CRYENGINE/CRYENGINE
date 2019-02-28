// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostProcessFilters : image filters post processing

   Revision history:
* 23/02/2005: Re-factored/Converted to CryEngine 2.0 by Tiago Sousa
* Created by Tiago Sousa

   =============================================================================*/

#include "StdAfx.h"
#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include "D3DPostProcess.h"
#include "D3DStereo.h"

#include "GraphicsPipeline/PostEffects.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CSharpening::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("SHARPENING");

	const f32 fSharpenAmount = max(m_pAmount->GetParam(), CRenderer::CV_r_Sharpening + 1.0f);
	if (fSharpenAmount > 1e-6f)
		GetUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[0]);

	static CCryNameTSCRC pTechName("CA_Sharpening");
	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	Vec4 pParams = Vec4(CRenderer::CV_r_ChromaticAberration, 0, 0, fSharpenAmount);
	static CCryNameR pParamName("psParams");
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

	GetUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_POINT);
	GetUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 1);
	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// todo: handle diferent blurring filters, add wavelength based blur

void CBlurring::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("BLURRING");

	float fType = m_pType->GetParam();
	float fAmount = m_pAmount->GetParam();
	fAmount = clamp_tpl<float>(fAmount, 0.0f, 1.0f);

	// maximum blur amount to have nice results
	const float fMaxBlurAmount = 5.0f;

	// this is uber expensive - and barely no need for this - adjusting gaussian distribution already looks quite good
	//GetUtils().TexBlurGaussian(CRendererResources::s_ptexBackBuffer, 1, 1.0f, LERP(0.0f, fMaxBlurAmount, sqrtf(fAmount) ), false);

	GetUtils().StretchRect(CRendererResources::s_ptexBackBuffer, CRendererResources::s_ptexBackBufferScaled[0]);
	GetUtils().TexBlurGaussian(CRendererResources::s_ptexBackBufferScaled[0], 1, 1.0f, LERP(0.0f, fMaxBlurAmount, fAmount), false, 0, false, CRendererResources::s_ptexBackBufferScaledTemp[0]);

	static CCryNameTSCRC pTechName("BlurInterpolation");
	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	// Set PS default params
	Vec4 pParams = Vec4(0, 0, 0, fAmount * fAmount);
	static CCryNameR pParamName("psParams");
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName, &pParams, 1);

	GetUtils().SetTexture(CRendererResources::s_ptexBackBufferScaled[0], 0);
	GetUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 1, FILTER_POINT);
	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();
*/
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

bool CColorGrading::UpdateParams(SColorGradingMergeParams& pMergeParams, bool bUpdateChart)
{
	float fSharpenAmount = max(m_pSharpenAmount->GetParam(), 0.0f);

	// add cvar color_grading/color_grading_levels/color_grading_selectivecolor/color_grading_filters

	// Clamp to same Photoshop min/max values
	float fMinInput = clamp_tpl<float>(m_pMinInput->GetParam(), 0.0f, 255.0f);
	float fGammaInput = clamp_tpl<float>(m_pGammaInput->GetParam(), 0.0f, 10.0f);
	;
	float fMaxInput = clamp_tpl<float>(m_pMaxInput->GetParam(), 0.0f, 255.0f);
	float fMinOutput = clamp_tpl<float>(m_pMinOutput->GetParam(), 0.0f, 255.0f);

	float fMaxOutput = clamp_tpl<float>(m_pMaxOutput->GetParam(), 0.0f, 255.0f);

	float fBrightness = m_pBrightness->GetParam();
	float fContrast = m_pContrast->GetParam();
	float fSaturation = m_pSaturation->GetParam() + m_pSaturationOffset->GetParam();
	Vec4 pFilterColor = m_pPhotoFilterColor->GetParamVec4() + m_pPhotoFilterColorOffset->GetParamVec4();
	float fFilterColorDensity = clamp_tpl<float>(m_pPhotoFilterColorDensity->GetParam() + m_pPhotoFilterColorDensityOffset->GetParam(), 0.0f, 1.0f);
	float fGrain = min(m_pGrainAmount->GetParam() + m_pGrainAmountOffset->GetParam(), 1.0f);

	Vec4 pSelectiveColor = m_pSelectiveColor->GetParamVec4();
	float fSelectiveColorCyans = clamp_tpl<float>(m_pSelectiveColorCyans->GetParam() * 0.01f, -1.0f, 1.0f);
	float fSelectiveColorMagentas = clamp_tpl<float>(m_pSelectiveColorMagentas->GetParam() * 0.01f, -1.0f, 1.0f);
	float fSelectiveColorYellows = clamp_tpl<float>(m_pSelectiveColorYellows->GetParam() * 0.01f, -1.0f, 1.0f);
	float fSelectiveColorBlacks = clamp_tpl<float>(m_pSelectiveColorBlacks->GetParam() * 0.01f, -1.0f, 1.0f);

	// Saturation matrix
	Matrix44 pSaturationMat;
	{
		float y = 0.3086f, u = 0.6094f, v = 0.0820f, s = clamp_tpl<float>(fSaturation, -1.0f, 100.0f);

		float a = (1.0f - s) * y + s;
		float b = (1.0f - s) * y;
		float c = (1.0f - s) * y;
		float d = (1.0f - s) * u;
		float e = (1.0f - s) * u + s;
		float f = (1.0f - s) * u;
		float g = (1.0f - s) * v;
		float h = (1.0f - s) * v;
		float i = (1.0f - s) * v + s;

		pSaturationMat.SetIdentity();
		pSaturationMat.SetRow(0, Vec3(a, d, g));
		pSaturationMat.SetRow(1, Vec3(b, e, h));
		pSaturationMat.SetRow(2, Vec3(c, f, i));
	}

	//  Brightness matrix
	Matrix44 pBrightMat;
	fBrightness = clamp_tpl<float>(fBrightness, 0.0f, 100.0f);
	pBrightMat.SetIdentity();
	pBrightMat.SetRow(0, Vec3(fBrightness, 0, 0));
	pBrightMat.SetRow(1, Vec3(0, fBrightness, 0));
	pBrightMat.SetRow(2, Vec3(0, 0, fBrightness));

	// Create Contrast matrix
	Matrix44 pContrastMat;
	{
		float c = clamp_tpl<float>(fContrast, -1.0f, 100.0f);
		pContrastMat.SetIdentity();
		pContrastMat.SetRow(0, Vec3(c, 0, 0));
		pContrastMat.SetRow(1, Vec3(0, c, 0));
		pContrastMat.SetRow(2, Vec3(0, 0, c));
		pContrastMat.SetColumn(3, 0.5f * Vec3(1.0f - c, 1.0f - c, 1.0f - c));
	}

	// Compose final color matrix and set fragment program constants
	Matrix44 pColorMat = pSaturationMat * (pBrightMat * pContrastMat);

	Vec4 pParams0 = Vec4(fMinInput, fGammaInput, fMaxInput, fMinOutput);
	Vec4 pParams1 = Vec4(fMaxOutput, fGrain, cry_random(0.f, 1023.f), cry_random(0.f, 1023.f));
	Vec4 pParams2 = Vec4(pFilterColor.x, pFilterColor.y, pFilterColor.z, fFilterColorDensity);
	Vec4 pParams3 = Vec4(pSelectiveColor.x, pSelectiveColor.y, pSelectiveColor.z, fSharpenAmount + 1.0f);
	Vec4 pParams4 = Vec4(fSelectiveColorCyans, fSelectiveColorMagentas, fSelectiveColorYellows, fSelectiveColorBlacks);

	// Enable corresponding shader variation
	pMergeParams.nFlagsShaderRT = 0;
	pMergeParams.nFlagsShaderRT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE3] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5]);

	if (CRenderer::CV_r_colorgrading_levels && (fMinInput || fGammaInput || fMaxInput || fMinOutput || fMaxOutput))
		pMergeParams.nFlagsShaderRT |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	if (CRenderer::CV_r_colorgrading_filters && (fFilterColorDensity || fGrain || fSharpenAmount))
	{
		if (fFilterColorDensity)
			pMergeParams.nFlagsShaderRT |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		if (fGrain || fSharpenAmount)
			pMergeParams.nFlagsShaderRT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	}

	if (CRenderer::CV_r_colorgrading_selectivecolor && (fSelectiveColorCyans || fSelectiveColorMagentas || fSelectiveColorYellows || fSelectiveColorBlacks))
		pMergeParams.nFlagsShaderRT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

	Matrix44 pColorMatFromUserAndTOD = GetUtils().GetColorMatrix();
	pColorMat = pColorMat * pColorMatFromUserAndTOD;

	Vec4 pColorMatrix[3] =
	{
		Vec4(pColorMat.m00, pColorMat.m01, pColorMat.m02, pColorMat.m03),
		Vec4(pColorMat.m10, pColorMat.m11, pColorMat.m12, pColorMat.m13),
		Vec4(pColorMat.m20, pColorMat.m21, pColorMat.m22, pColorMat.m23),
	};

	pMergeParams.pColorMatrix[0] = pColorMatrix[0];
	pMergeParams.pColorMatrix[1] = pColorMatrix[1];
	pMergeParams.pColorMatrix[2] = pColorMatrix[2];
	pMergeParams.pLevels[0] = pParams0;
	pMergeParams.pLevels[1] = pParams1;
	pMergeParams.pFilterColor = pParams2;
	pMergeParams.pSelectiveColor[0] = pParams3;
	pMergeParams.pSelectiveColor[1] = pParams4;

	// Always using color charts
	if (bUpdateChart)
	{
		if (gcpRendD3D->m_pColorGradingControllerD3D && (gEnv->IsCutscenePlaying() || (gRenDev->GetRenderFrameID() % max(1, CRenderer::CV_r_ColorgradingChartsCache)) == 0))
		{
			if (!gcpRendD3D->m_pColorGradingControllerD3D->Update(&pMergeParams))
				return false;
		}

	}

	return true;
}

void CColorGrading::Render()
{
	// Depreceated: to be removed / replaced by UberPostProcess shader
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CPostAA::Render()
{
	gcpRendD3D->GetGraphicsPipeline().ExecutePostAA();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

bool CPostStereo::Preprocess(const SRenderViewInfo& viewInfo)
{
	return gcpRendD3D->GetS3DRend().IsPostStereoEnabled();
}

void CPostStereo::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	CD3DStereoRenderer* const __restrict rendS3D = &gcpRendD3D->GetS3DRend();

	if (!rendS3D->IsPostStereoEnabled())
		return;

	if (gcpRendD3D->m_nGraphicsPipeline == 0 &&
	    CRenderer::CV_r_PostProcessHUD3D &&
	    CRenderer::CV_r_PostProcessHUD3D != 2  &&
	    !PostEffectMgr()->GetEffect(EPostEffectID::NanoGlass)->IsActive() ) // NanoGlass also updates flash
	{
		// If HUD enabled, pre-process flash updates first (for performance reasons)
		C3DHud* pPostProcessHUD3D = (C3DHud*) PostEffectMgr()->GetEffect(EPostEffectID::HUD3D);
		pPostProcessHUD3D->FlashUpdateRT();
	}

	PROFILE_LABEL_SCOPE("POST_STEREO");

	// Mask near geometry (weapon)
	CTexture* pTmpMaskTex = CRendererResources::s_ptexSceneNormalsBent; // non-msaaed target
	assert(pTmpMaskTex);
	assert(pTmpMaskTex->GetWidth() == CRendererResources::s_ptexBackBuffer->GetWidth());
	assert(pTmpMaskTex->GetHeight() == CRendererResources::s_ptexBackBuffer->GetHeight());
	assert(pTmpMaskTex->GetDstFormat() == CRendererResources::s_ptexBackBuffer->GetDstFormat());

	{
		PROFILE_LABEL_SCOPE("NEAR_MASK");

		static CCryNameTSCRC TechNameMask("StereoNearMask");

		gcpRendD3D->FX_ClearTarget(pTmpMaskTex, Clr_Neutral);
		gcpRendD3D->FX_PushRenderTarget(0, pTmpMaskTex, &gcpRendD3D->m_DepthBufferOrig); // this will likely need MSAA fixing ? needs testing, maybe post resolve too troublesome

		GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, TechNameMask, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
		gcpRendD3D->FX_SetState(GS_DEPTHFUNC_LEQUAL);
		SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight(), CRenderer::CV_r_DrawNearZRange);
		GetUtils().ShEndPass();

		gcpRendD3D->FX_PopRenderTarget(0);
	}

	const CCamera& cam = gcpRendD3D->GetCamera();
	float maxParallax = rendS3D->GetMaxSeparationScene();
	float screenDist = rendS3D->GetZeroParallaxPlaneDist() / cam.GetFarPlane();
	float nearGeoShift = rendS3D->GetNearGeoShift() / cam.GetFarPlane();
	float nearGeoScale = rendS3D->GetNearGeoScale();

	static CCryNameTSCRC TechName("PostStereo");

	rendS3D->BeginRenderingMRT(true);

	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	gcpRendD3D->FX_SetState(GS_NODEPTHTEST);

	static CCryNameR pParamName0("StereoParams");
	Vec4 stereoParams(maxParallax, screenDist, nearGeoShift, nearGeoScale);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName0, &stereoParams, 1);

	static CCryNameR pParamName1("NearZRange");
	Vec4 nearZRange(CRenderer::CV_r_DrawNearZRange, CRenderer::CV_r_DrawNearZRange, CRenderer::CV_r_DrawNearZRange, CRenderer::CV_r_DrawNearZRange);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName1, &nearZRange, 1);

	static CCryNameR pParamName2("HPosScale");
	Vec4 hposScale(gcpRendD3D->m_CurViewportScale.x, gcpRendD3D->m_CurViewportScale.y, 0.0f, 0.0f);
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamName2, &hposScale, 1);

	GetUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_LINEAR, eSamplerAddressMode_Mirror);
	GetUtils().SetTexture(CRendererResources::s_ptexZTarget, 1, FILTER_POINT);
	GetUtils().SetTexture(pTmpMaskTex, 2, FILTER_POINT);

	SD3DPostEffectsUtils::DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();
	rendS3D->EndRenderingMRT(false);
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CImageGhosting::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	CTexture* pPrevFrame = CRendererResources::s_ptexPrevFrameScaled;
	CTexture* pPrevFrameRead = CRendererResources::s_ptexPrevFrameScaled;

	if (m_bInit)
	{
		m_bInit = false;

		gcpRendD3D->FX_ClearTarget(pPrevFrame, Clr_Transparent);
	}

	PROFILE_LABEL_SCOPE("IMAGE_GHOSTING");

	// 0.25ms / 0.4ms
	static CCryNameTSCRC TechName("ImageGhosting");
	GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	// Update ghosting
	gcpRendD3D->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	static CCryNameR pParamNamePS("ImageGhostingParamsPS");
	Vec4 vParamsPS = Vec4(1, 1, max(0.0f, 1 - m_pAmount->GetParam()), gEnv->pTimer->GetFrameTime());
	CShaderMan::s_shPostEffects->FXSetPSFloat(pParamNamePS, &vParamsPS, 1);

	GetUtils().SetTexture(pPrevFrameRead, 0, FILTER_LINEAR);

	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();

	// todo: on x360 use msaa (0.1ms overall)
	GetUtils().CopyScreenToTexture(CRendererResources::s_ptexBackBuffer);     // 0.25ms / 0.4 ms
	GetUtils().StretchRect(CRendererResources::s_ptexBackBuffer, pPrevFrame); // 0.25ms
*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void CUberGamePostProcess::Render()
{
	ASSERT_LEGACY_PIPELINE
/*
	PROFILE_LABEL_SCOPE("UBER_GAME_POSTPROCESS");

	uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
	gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2]);

	gRenDev->m_cEF.mfRefreshSystemShader("PostEffectsGame", CShaderMan::s_shPostEffectsGame);

	if (m_nCurrPostEffectsMask & ePE_ChromaShift)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	//if( m_nCurrPostEffectsMask & ePE_RadialBlur )
	//	gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (m_nCurrPostEffectsMask & ePE_SyncArtifacts)
		gRenDev->m_RP.m_FlagsShader_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

	static CCryNameTSCRC TechName("UberGamePostProcess");
	GetUtils().ShBeginPass(CShaderMan::s_shPostEffectsGame, TechName, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

	CTexture* pMaskTex = const_cast<CTexture*>(static_cast<CParamTexture*>(m_pMask)->GetParamTexture());

	int32 nRenderState = GS_NODEPTHTEST;

	// Blend with backbuffer when user sets a mask
	if (pMaskTex)
		nRenderState |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA;

	gcpRendD3D->FX_SetState(nRenderState);

	Vec4 cColor = m_pColorTint->GetParamVec4();
	static CCryNameR pParamNamePS0("UberPostParams0");
	static CCryNameR pParamNamePS1("UberPostParams1");
	static CCryNameR pParamNamePS2("UberPostParams2");
	static CCryNameR pParamNamePS3("UberPostParams3");
	static CCryNameR pParamNamePS4("UberPostParams4");
	static CCryNameR pParamNamePS5("UberPostParams5");

	Vec4 vParamsPS[6] =
	{
		Vec4(m_pVSyncAmount->GetParam(), m_pInterlationAmount->GetParam(),           m_pInterlationTilling->GetParam(), m_pInterlationRotation->GetParam()),
		//Vec4(m_pVSyncFreq->GetParam(), 1.0f + max(0.0f, m_pPixelationScale->GetParam()*0.25f), m_pNoise->GetParam()*0.25f, m_pChromaShiftAmount->GetParam() + m_pFilterChromaShiftAmount->GetParam()),
		Vec4(m_pVSyncFreq->GetParam(),   1.0f,                                       m_pNoise->GetParam() * 0.25f,      m_pChromaShiftAmount->GetParam() + m_pFilterChromaShiftAmount->GetParam()),
		Vec4(min(1.0f,                   m_pGrainAmount->GetParam() * 0.1f * 0.25f), m_pGrainTile->GetParam(),          m_pSyncWavePhase->GetParam(),                                              m_pSyncWaveFreq->GetParam()),
		Vec4(cColor.x,                   cColor.y,                                   cColor.z,                          min(1.0f,                                                                  m_pSyncWaveAmplitude->GetParam() * 0.01f)),
		Vec4(cry_random(0.0f,            1.0f),                                      cry_random(0.0f,                   1.0f),                                                                     cry_random(0.0f,                            1.0f),cry_random(0.0f, 1.0f)),
		Vec4(0,                          0,                                          0,                                 0)
	};

	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS0, &vParamsPS[0], 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS1, &vParamsPS[1], 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS2, &vParamsPS[2], 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS3, &vParamsPS[3], 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS4, &vParamsPS[4], 1);
	CShaderMan::s_shPostEffectsGame->FXSetPSFloat(pParamNamePS5, &vParamsPS[5], 1);

	GetUtils().SetTexture(CRendererResources::s_ptexBackBuffer, 0, FILTER_LINEAR);
	GetUtils().SetTexture(CRendererResources::s_ptexScreenNoiseMap, 1, FILTER_LINEAR, eSamplerAddressMode_Wrap);

	if (pMaskTex)
		GetUtils().SetTexture(pMaskTex, 2, FILTER_LINEAR);
	else
		GetUtils().SetTexture(CRendererResources::s_ptexWhite, 2, FILTER_LINEAR);

	GetUtils().DrawFullScreenTri(CRendererResources::s_ptexBackBuffer->GetWidth(), CRendererResources::s_ptexBackBuffer->GetHeight());

	GetUtils().ShEndPass();

	gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
*/
}
