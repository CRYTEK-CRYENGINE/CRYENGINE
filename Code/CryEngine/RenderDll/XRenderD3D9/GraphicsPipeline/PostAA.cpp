// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "PostAA.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/LensOptics.h"

struct PostAAConstants
{
	Matrix44 matReprojection;
	Vec4     params;
	Vec4     screenSize;
	Vec4     worldViewPos;
};

void CPostAAStage::Init()
{
	m_pTexAreaSMAA.Assign_NoAddRef(CTexture::ForName("%ENGINE%/EngineAssets/ScreenSpace/AreaTex.dds", FT_DONT_STREAM, eTF_Unknown));
	m_pTexSearchSMAA.Assign_NoAddRef(CTexture::ForName("%ENGINE%/EngineAssets/ScreenSpace/SearchTex.dds", FT_DONT_STREAM, eTF_Unknown));
	m_lastFrameID = -1;

	m_passTemporalAA.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
	m_passTemporalAA.AllocateTypedConstantBuffer<PostAAConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);
}

void CPostAAStage::ApplySMAA(CTexture*& pCurrRT)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CTexture* pEdgesRT = CTexture::s_ptexSceneNormalsMap;   // Reusing ESRAM resident target
	CTexture* pBlendWeightsRT = CTexture::s_ptexHDRTarget;  // Reusing ESRAM resident target (FP16 RT accessed using point filtering which gives full rate on GCN)
	CTexture* pDestRT = CTexture::s_ptexSceneNormalsMap;

	assert(pBlendWeightsRT->GetDstFormat() != eTF_R11G11B10F);  // Alpha channel required

	if (!pEdgesRT || !pBlendWeightsRT)
		return;

	// Prepare stencil prepass
	int stencilRef = -1;
	if (CRenderer::CV_r_AntialiasingModeSCull)
	{
		pRenderer->m_nStencilMaskRef += 1;

		if (gcpRendD3D->m_nStencilMaskRef > STENC_MAX_REF)
		{
			// Stencil initialized to 1 - 0 is reserved for MSAAed samples
			gcpRendD3D->EF_ClearTargetsImmediately(FRT_CLEAR_STENCIL);
			gcpRendD3D->m_nStencilMaskRef = 1;
		}
		stencilRef = gcpRendD3D->m_nStencilMaskRef;
	}

	pRenderer->FX_ClearTarget(pEdgesRT, Clr_Transparent);
	pRenderer->FX_ClearTarget(pBlendWeightsRT, Clr_Transparent);

	// Pass 1: Edge Detection
	{
		if (m_passSMAAEdgeDetection.InputChanged(pCurrRT->GetTextureID(), CRenderer::CV_r_AntialiasingModeSCull))
		{
			static CCryNameTSCRC techEdgeDetection("LumaEdgeDetectionSMAA");
			m_passSMAAEdgeDetection.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAAEdgeDetection.SetTechnique(CShaderMan::s_shPostAA, techEdgeDetection, 0);
			m_passSMAAEdgeDetection.SetTargetClearMask(CPrimitiveRenderPass::eClear_Color0);
			m_passSMAAEdgeDetection.SetRenderTarget(0, pEdgesRT);
			m_passSMAAEdgeDetection.SetDepthTarget(pRenderer->m_pZTexture);
			m_passSMAAEdgeDetection.SetState(GS_NODEPTHTEST);
			m_passSMAAEdgeDetection.SetTextureSamplerPair(0, pCurrRT, EDefaultSamplerStates::PointClamp);
		}
		if (CRenderer::CV_r_AntialiasingModeSCull)
		{
			m_passSMAAEdgeDetection.SetState(GS_NODEPTHTEST | GS_STENCIL);
			m_passSMAAEdgeDetection.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_ALWAYS) |
			  STENCOP_FAIL(FSS_STENCOP_REPLACE) |
			  STENCOP_ZFAIL(FSS_STENCOP_REPLACE) |
			  STENCOP_PASS(FSS_STENCOP_REPLACE),
			  (uint8)stencilRef);
		}
		m_passSMAAEdgeDetection.BeginConstantUpdate();
		m_passSMAAEdgeDetection.Execute();
	}

	// Pass 2: Generate blend weight map
	{
		if (m_passSMAABlendWeights.InputChanged(CRenderer::CV_r_AntialiasingModeSCull))
		{
			static CCryNameTSCRC techBlendWeights("BlendWeightSMAA");
			m_passSMAABlendWeights.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAABlendWeights.SetTechnique(CShaderMan::s_shPostAA, techBlendWeights, 0);
			m_passSMAABlendWeights.SetTargetClearMask(CPrimitiveRenderPass::eClear_Color0);
			m_passSMAABlendWeights.SetRenderTarget(0, pBlendWeightsRT);
			m_passSMAABlendWeights.SetDepthTarget(pRenderer->m_pZTexture);
			m_passSMAABlendWeights.SetState(GS_NODEPTHTEST);
			m_passSMAABlendWeights.SetTextureSamplerPair(0, pEdgesRT, EDefaultSamplerStates::LinearClamp);
			m_passSMAABlendWeights.SetTextureSamplerPair(1, m_pTexAreaSMAA, EDefaultSamplerStates::LinearClamp);
			m_passSMAABlendWeights.SetTextureSamplerPair(2, m_pTexSearchSMAA, EDefaultSamplerStates::PointClamp);
		}
		if (CRenderer::CV_r_AntialiasingModeSCull)
		{
			m_passSMAABlendWeights.SetState(GS_NODEPTHTEST | GS_STENCIL);
			m_passSMAABlendWeights.SetStencilState(
			  STENC_FUNC(FSS_STENCFUNC_EQUAL) |
			  STENCOP_FAIL(FSS_STENCOP_KEEP) |
			  STENCOP_ZFAIL(FSS_STENCOP_KEEP) |
			  STENCOP_PASS(FSS_STENCOP_KEEP),
			  (uint8)stencilRef);
		}
		m_passSMAABlendWeights.BeginConstantUpdate();
		m_passSMAABlendWeights.Execute();
	}

	// Final Pass: Blend neighborhood pixels
	{
		if (m_passSMAANeighborhoodBlending.InputChanged(pCurrRT->GetTextureID()))
		{
			static CCryNameTSCRC techNeighborhoodBlending("NeighborhoodBlendingSMAA");
			m_passSMAANeighborhoodBlending.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAANeighborhoodBlending.SetTechnique(CShaderMan::s_shPostAA, techNeighborhoodBlending, 0);
			m_passSMAANeighborhoodBlending.SetRenderTarget(0, pDestRT);
			m_passSMAANeighborhoodBlending.SetState(GS_NODEPTHTEST);
			m_passSMAANeighborhoodBlending.SetTextureSamplerPair(0, pBlendWeightsRT, EDefaultSamplerStates::PointClamp);
			m_passSMAANeighborhoodBlending.SetTextureSamplerPair(1, pCurrRT, EDefaultSamplerStates::LinearClamp);
		}
		m_passSMAANeighborhoodBlending.BeginConstantUpdate();
		m_passSMAANeighborhoodBlending.Execute();
	}

	pCurrRT = pDestRT;
}

void CPostAAStage::ApplyTemporalAA(CTexture*& pCurrRT, CTexture*& pMgpuRT, uint32 aaMode)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CShader* pShader = CShaderMan::s_shPostAA;
	CTexture* pDestRT = GetUtils().GetTaaRT(true);
	CTexture* pPrevRT = ((SPostEffectsUtils::m_iFrameCounter - m_lastFrameID) < 10) ? GetUtils().GetTaaRT(false) : pCurrRT;

	assert((pCurrRT->GetFlags() & FT_USAGE_ALLOWREADSRGB) && (pPrevRT->GetFlags() & FT_USAGE_ALLOWREADSRGB));

	uint64 rtMask = 0;
	if (aaMode & (eAT_SMAA_1TX_MASK))
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	else if (aaMode & eAT_TSAA_MASK)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];

	{
		static CCryNameTSCRC techTemporalAA("PostAA");
		m_passTemporalAA.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passTemporalAA.SetTechnique(pShader, techTemporalAA, rtMask);
		m_passTemporalAA.SetRenderTarget(0, pDestRT);
		m_passTemporalAA.SetState(GS_NODEPTHTEST);
		m_passTemporalAA.SetRequireWorldPos(true);
		m_passTemporalAA.SetRequirePerViewConstantBuffer(true);
		m_passTemporalAA.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);

		m_passTemporalAA.SetTextureSamplerPair(0, pCurrRT, EDefaultSamplerStates::LinearClamp);
		m_passTemporalAA.SetTextureSamplerPair(1, pPrevRT, EDefaultSamplerStates::LinearClamp);
		m_passTemporalAA.SetTextureSamplerPair(2, CTexture::s_ptexZTarget, EDefaultSamplerStates::PointClamp);
		m_passTemporalAA.SetTextureSamplerPair(3, GetUtils().GetVelocityObjectRT(), EDefaultSamplerStates::PointClamp);
		m_passTemporalAA.SetTextureSamplerPair(4, pCurrRT, EDefaultSamplerStates::LinearClamp, EDefaultResourceViews::sRGB);
		m_passTemporalAA.SetTextureSamplerPair(5, pPrevRT, EDefaultSamplerStates::LinearClamp, EDefaultResourceViews::sRGB);
		m_passTemporalAA.SetTexture(16, pRenderer->m_DepthBufferOrig.pTexture);
	}

	(pMgpuRT = pDestRT)->MgpuResourceUpdate(true);
	m_passTemporalAA.BeginConstantUpdate();

	{
		CStandardGraphicsPipeline::SViewInfo viewInfo[2];
		int viewInfoCount = pRenderer->GetGraphicsPipeline().GetViewInfo(viewInfo);

		auto constants = m_passTemporalAA.BeginTypedConstantUpdate<PostAAConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);

		const float rcpWidth = 1.0f / (float)pRenderer->GetWidth();
		const float rcpHeight = 1.0f / (float)pRenderer->GetHeight();
		constants->screenSize = Vec4((float)pRenderer->GetWidth(), (float)pRenderer->GetHeight(), rcpWidth, rcpHeight);
		constants->worldViewPos = Vec4(pRenderer->GetRCamera().vOrigin, 0);
		constants->params = Vec4(max(CRenderer::CV_r_AntialiasingTAASharpening + 1.0f, 1.0f), 0.0f, CRenderer::CV_r_AntialiasingTAAFalloffLowFreq + 1e-6f, CRenderer::CV_r_AntialiasingTAAFalloffHiFreq + 1e-6f);
		if (aaMode & eAT_TSAA_MASK)
			constants->params = Vec4(CRenderer::CV_r_AntialiasingTSAASubpixelDetection, CRenderer::CV_r_AntialiasingTSAASmoothness, 0, 0);

		// Compute reprojection matrix with highest possible precision to minimize numeric diffusion
		// TODO: Make sure NEAREST projection is handled correctly
		Matrix44_tpl<f64> matReprojection64[2];
		for (int i = 0; i < viewInfoCount; ++i)
		{
			Matrix44A matProj = viewInfo[i].projMatrix;

			// Changing stereo mode from dual-rendering to post-stereo causes 1 frame mismatch between projection matrix and stereo mode from GetStereoMode().
			const bool exceptionalCase = (pRenderer->GetS3DRend().GetStereoMode() == STEREO_MODE_POST_STEREO && CRenderer::CV_r_StereoMode == STEREO_MODE_DUAL_RENDERING);

			assert(pRenderer->GetS3DRend().GetStereoMode() == STEREO_MODE_DUAL_RENDERING
			       || exceptionalCase
			       || (matProj.m20 == 0 && matProj.m21 == 0)); // Ensure jittering is removed from projection matrix

			Matrix44_tpl<f64> matViewInv, matProjInv;
			mathMatrixLookAtInverse(&matViewInv, &viewInfo[i].viewMatrix);
			const bool bCanInvert = mathMatrixPerspectiveFovInverse(&matProjInv, &matProj);
			assert(bCanInvert);

			Matrix44_tpl<f64> matScaleBias1 = Matrix44_tpl<f64>(
			  0.5, 0, 0, 0,
			  0, -0.5, 0, 0,
			  0, 0, 1, 0,
			  0.5, 0.5, 0, 1);
			Matrix44_tpl<f64> matScaleBias2 = Matrix44_tpl<f64>(
			  2.0, 0, 0, 0,
			  0, -2.0, 0, 0,
			  0, 0, 1, 0,
			  -1.0, 1.0, 0, 1);

			Matrix44 mPrevView = viewInfo[i].prevCameraMatrix;
			matReprojection64[i] = matProjInv * matViewInv * Matrix44_tpl<f64>(mPrevView) * Matrix44_tpl<f64>(matProj);
			matReprojection64[i] = matScaleBias2 * matReprojection64[i] * matScaleBias1;
		}

		constants->matReprojection = (Matrix44)matReprojection64[0];

		if (viewInfoCount > 1)
		{
			constants.BeginStereoOverride(true);
			constants->matReprojection = (Matrix44)matReprojection64[1];
			constants->worldViewPos = Vec4(viewInfo[1].pRenderCamera->vOrigin, 0);
		}

		m_passTemporalAA.EndTypedConstantUpdate(constants);
	}

	m_passTemporalAA.Execute();

	pCurrRT = pDestRT;
	m_lastFrameID = SPostEffectsUtils::m_iFrameCounter;
}

void CPostAAStage::DoFinalComposition(CTexture*& pCurrRT, uint32 aaMode)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	PROFILE_LABEL_SCOPE("FLARES, GRAIN");

	CTexture* pTexLensOptics = CTexture::s_ptexSceneTargetR11G11B10F[0];
	CTexture* pOutputRT = nullptr;

	if (pRenderer->IsNativeScalingEnabled())
	{
		pOutputRT = CTexture::s_ptexSceneSpecular;
	}
	else
	{
		pOutputRT = pRenderer->GetCurrentTargetOutput();
	}

	static uint64 prevRTMask = 0;
	uint64 rtMask = 0;
	if (aaMode & (eAT_SMAA_2TX_MASK | eAT_TSAA_MASK))
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (CRenderer::CV_r_colorRangeCompression)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE4];

	if (pRenderer->GetGraphicsPipeline().GetLensOpticsStage()->HasContent())
	{
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		if (CRenderer::CV_r_FlaresChromaShift > 0.5f / (float)pRenderer->GetWidth())  // Only relevant if bigger than half pixel
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	}

	if (m_passComposition.InputChanged(pCurrRT->GetID(), pOutputRT->GetID(), CTexture::s_ptexCurLumTexture->GetID()) || rtMask != prevRTMask)
	{
		static CCryNameTSCRC techComposition("PostAAComposites");
		m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_passComposition.SetTechnique(CShaderMan::s_shPostAA, techComposition, rtMask);
		m_passComposition.SetRenderTarget(0, pOutputRT);
		m_passComposition.SetState(GS_NODEPTHTEST);
		m_passComposition.SetTextureSamplerPair(0, pCurrRT, EDefaultSamplerStates::LinearClamp);
		m_passComposition.SetTextureSamplerPair(5, pTexLensOptics, EDefaultSamplerStates::PointClamp);
		m_passComposition.SetTextureSamplerPair(6, CTexture::s_ptexFilmGrainMap, EDefaultSamplerStates::PointWrap);
		m_passComposition.SetTextureSamplerPair(7, CTexture::s_ptexCurLumTexture, EDefaultSamplerStates::PointClamp);
		prevRTMask = rtMask;
	}

	m_passComposition.BeginConstantUpdate();

	{
		static CCryNameR paramsName("vParams");
		static CCryNameR lensOpticsParamsName("vLensOpticsParams");
		static CCryNameR hdrParamsName("HDRParams");
		static CCryNameR hdrEyeAdaptationName("HDREyeAdaptation");

		float sharpening = CRenderer::CV_r_AntialiasingTAASharpening;
		const Vec4 params(max(1.0f + sharpening, 1.0f), 0, 0, 0);
		m_passComposition.SetConstant(paramsName, params, eHWSC_Pixel);

		const Vec4 lensOpticsParams(1.0f, 1.0f, 1.0f, CRenderer::CV_r_FlaresChromaShift);
		m_passComposition.SetConstant(lensOpticsParamsName, lensOpticsParams, eHWSC_Pixel);

		// Apply grain (final luminance texture doesn't get its final value baked, so we have to replicate the entire hdr eye adaption)
		Vec4 hdrSetupParams[5];
		gEnv->p3DEngine->GetHDRSetupParams(hdrSetupParams);

		CEffectParam* pParamGrainAmount = PostEffectMgr()->GetByName("FilterGrain_Amount");
		CEffectParam* pParamArtifactsGrain = PostEffectMgr()->GetByName("FilterArtifacts_Grain");
		const float grainAmount = max(pParamGrainAmount->GetParam(), pParamArtifactsGrain->GetParam());
		const Vec4 v = Vec4(0, 0, 0, max(grainAmount, max(hdrSetupParams[1].w, CRenderer::CV_r_HDRGrainAmount)));

		m_passComposition.SetConstant(hdrParamsName, v, eHWSC_Pixel);
		m_passComposition.SetConstant(hdrEyeAdaptationName, hdrSetupParams[4], eHWSC_Pixel);
	}

	m_passComposition.Execute();
}

void CPostAAStage::Execute()
{
	// TODO: Handle rapid camera position changes in a better way

	PROFILE_LABEL_SCOPE("POST_AA");

	CD3D9Renderer* rd = gcpRendD3D;
	CTexture* pCurrRT = CTexture::s_ptexSceneDiffuse;
	CTexture* pMgpuRT = NULL;

	uint32 aaMode = rd->FX_GetAntialiasingType();

	if (rd->IsHDRModeEnabled() && (aaMode & (eAT_SMAA_MASK |  eAT_TSAA_MASK)) && pCurrRT)
	{
		rd->FX_PopRenderTarget(0);
	}
	rd->FX_SetActiveRenderTargets();

	rd->m_RP.m_PersFlags2 |= RBPF2_NOPOSTAA;

	if (aaMode & eAT_SMAA_MASK)
	{
		ApplySMAA(pCurrRT);
	}

	if ((aaMode & eAT_REQUIRES_PREVIOUSFRAME_MASK))
	{
		ApplyTemporalAA(pCurrRT, pMgpuRT, aaMode);
	}

	DoFinalComposition(pCurrRT, aaMode);

	rd->m_RP.m_PersFlags2 |= RBPF2_NOPOSTAA;

	if (pMgpuRT)
	{
		pMgpuRT->MgpuResourceUpdate(false);
	}
}
