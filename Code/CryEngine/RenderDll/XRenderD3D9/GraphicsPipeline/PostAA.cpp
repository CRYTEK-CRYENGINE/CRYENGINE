// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "PostAA.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/LensOptics.h"
#include "ColorGrading.h"

#include <Common/RenderDisplayContext.h>

struct PostAAConstants
{
	Matrix44 matReprojection;
	Vec4     params;
	Vec4     screenSize;
};

// 4x4 N-Queens pattern
static const Vec2 vNQAA4x[4] =
{
	Vec2(+3.0f / 8.0f, +1.0f / 8.0f),
	Vec2(+1.0f / 8.0f, -3.0f / 8.0f),
	Vec2(-1.0f / 8.0f, +3.0f / 8.0f),
	Vec2(-3.0f / 8.0f, -1.0f / 8.0f)
};

// 5x5 N-Queens pattern
static const Vec2 vNQAA5x[5] =
{
	Vec2(+4.0f / 10.0f, -4.0f / 10.0f),
	Vec2(+2.0f / 10.0f, +2.0f / 10.0f),
	Vec2(+0.0f / 10.0f, -2.0f / 10.0f),
	Vec2(-2.0f / 10.0f, +4.0f / 10.0f),
	Vec2(-4.0f / 10.0f, +0.0f / 10.0f)
};

// 8x8 N-Queens pattern
static const Vec2 vNQAA8x[8] =
{
	Vec2(+7.0f / 16.0f, -7.0f / 16.0f),
	Vec2(+5.0f / 16.0f, +5.0f / 16.0f),
	Vec2(+3.0f / 16.0f, +1.0f / 16.0f),
	Vec2(+1.0f / 16.0f, +7.0f / 16.0f),
	Vec2(-1.0f / 16.0f, -5.0f / 16.0f),
	Vec2(-3.0f / 16.0f, -1.0f / 16.0f),
	Vec2(-5.0f / 16.0f, +3.0f / 16.0f),
	Vec2(-7.0f / 16.0f, -3.0f / 16.0f)
};

static const Vec2 vSSAA2x[2] =
{
	Vec2(-0.25f, +0.25f),
	Vec2(+0.25f, -0.25f)
};

static const Vec2 vSSAA3x[3] =
{
	Vec2(-1.0f / 3.0f, -1.0f / 3.0f),
	Vec2(+1.0f / 3.0f, +0.0f / 3.0f),
	Vec2(+0.0f / 3.0f, +1.0f / 3.0f)
};

static const Vec2 vSSAA4x_regular[4] =
{
	Vec2(-0.25f, -0.25f), Vec2(-0.25f, +0.25f),
	Vec2(+0.25f, -0.25f), Vec2(+0.25f, +0.25f)
};

static const Vec2 vSSAA4x_rotated[4] =
{
	Vec2(-0.125f, -0.375f), Vec2(+0.375f, -0.125f),
	Vec2(-0.375f, +0.125f), Vec2(+0.125f, +0.375f)
};

static const Vec2 vSMAA4x[2] =
{
	Vec2(-0.125f, -0.125f),
	Vec2(+0.125f, +0.125f)
};

static const Vec2 vSSAA8x[8] =
{
	Vec2(0.0625,  -0.1875),  Vec2(-0.0625,   0.1875),
	Vec2(0.3125,  0.0625),   Vec2(-0.1875,   -0.3125),
	Vec2(-0.3125, 0.3125),   Vec2(-0.4375,   -0.0625),
	Vec2(0.1875,  0.4375),   Vec2(0.4375,    -0.4375)
};

static const Vec2 vSGSSAA8x8[8] =
{
	Vec2(6.0f / 7.0f, 0.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(2.0f / 7.0f, 1.0f / 7.0f) - Vec2(0.5f, 0.5f),
	Vec2(4.0f / 7.0f, 2.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(0.0f / 7.0f, 3.0f / 7.0f) - Vec2(0.5f, 0.5f),
	Vec2(7.0f / 7.0f, 4.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(3.0f / 7.0f, 5.0f / 7.0f) - Vec2(0.5f, 0.5f),
	Vec2(5.0f / 7.0f, 6.0f / 7.0f) - Vec2(0.5f, 0.5f), Vec2(1.0f / 7.0f, 7.0f / 7.0f) - Vec2(0.5f, 0.5f)
};

void CPostAAStage::CalculateJitterOffsets(CRenderView* pRenderView)
{
	pRenderView->m_vProjMatrixSubPixoffset = Vec2(0.0f, 0.0f);

	// TODO: Support temporal AA in the editor
	uint32 aaMode = CRenderer::FX_GetAntialiasingType();

	if (aaMode && gcpRendD3D->IsEditorMode())
		aaMode = 1U << (eAT_SMAA_1X * CRenderer::CV_r_AntialiasingModeEditor);

	if (aaMode & eAT_REQUIRES_SUBPIXELSHIFT_MASK)
	{
		int jitterPattern = CRenderer::CV_r_AntialiasingTAAPattern;
		if (jitterPattern == 1)
		{
			if (aaMode & eAT_SMAA_2TX_MASK)  jitterPattern = 2;
			else if (aaMode & eAT_TSAA_MASK) jitterPattern = 5;
			else                             jitterPattern = 0;
		}

		const int nSampleID = SPostEffectsUtils::m_iFrameCounter;
		Vec2 vCurrSubSample = Vec2(0, 0);
		switch (jitterPattern)
		{
		case 2:
			vCurrSubSample = vSSAA2x[nSampleID % 2];
			break;
		case 3:
			vCurrSubSample = vSSAA3x[nSampleID % 3];
			break;
		case 4:
			vCurrSubSample = vSSAA4x_regular[nSampleID % 4];
			break;
		case 5:
			vCurrSubSample = vSSAA4x_rotated[nSampleID % 4];
			break;
		case 6:
			vCurrSubSample = vSSAA8x[nSampleID % 8];
			break;
		case 7:
			vCurrSubSample = vSGSSAA8x8[nSampleID % 8];
			break;
		case 8:
			vCurrSubSample = Vec2(SPostEffectsUtils::srandf(), SPostEffectsUtils::srandf()) * 0.5f;
			break;
		case 9:
			vCurrSubSample = Vec2(SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 8, 2) - 0.5f,
				SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 8, 3) - 0.5f);
			break;
		case 10:
			vCurrSubSample = Vec2(SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 16, 2) - 0.5f,
				SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 16, 3) - 0.5f);
			break;
		case 11:
			vCurrSubSample = Vec2(SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 1024, 2) - 0.5f,
				SPostEffectsUtils::HaltonSequence(SPostEffectsUtils::m_iFrameCounter % 1024, 3) - 0.5f);
			break;
		}

		const auto& downscaleFactor = gRenDev->GetRenderQuality().downscaleFactor;

		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];

		pRenderView->m_vProjMatrixSubPixoffset.x = (vCurrSubSample.x * 2.0f / (float)renderWidth ) / downscaleFactor.x;
		pRenderView->m_vProjMatrixSubPixoffset.y = (vCurrSubSample.y * 2.0f / (float)renderHeight) / downscaleFactor.y;
	}
}

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
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CTexture* pEdgesRT = CRendererResources::s_ptexSceneNormalsMap;   // Reusing ESRAM resident target
	CTexture* pBlendWeightsRT = CRendererResources::s_ptexHDRTarget;  // Reusing ESRAM resident target (FP16 RT accessed using point filtering which gives full rate on GCN)
	CTexture* pDestRT = CRendererResources::s_ptexSceneNormalsMap;

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
			CClearSurfacePass::Execute(CRendererResources::s_ptexSceneDepth, CLEAR_STENCIL, 0.0f, 1);

			gcpRendD3D->m_nStencilMaskRef = 1;
		}

		stencilRef = gcpRendD3D->m_nStencilMaskRef;
	}

	CClearSurfacePass::Execute(pEdgesRT, Clr_Transparent);
	CClearSurfacePass::Execute(pBlendWeightsRT, Clr_Transparent);

	// Pass 1: Edge Detection
	{
		if (m_passSMAAEdgeDetection.InputChanged(pCurrRT->GetTextureID(), CRenderer::CV_r_AntialiasingModeSCull))
		{
			static CCryNameTSCRC techEdgeDetection("LumaEdgeDetectionSMAA");
			m_passSMAAEdgeDetection.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAAEdgeDetection.SetTechnique(CShaderMan::s_shPostAA, techEdgeDetection, 0);
			m_passSMAAEdgeDetection.SetTargetClearMask(CPrimitiveRenderPass::eClear_Color0);
			m_passSMAAEdgeDetection.SetRenderTarget(0, pEdgesRT);
			m_passSMAAEdgeDetection.SetDepthTarget(CRendererResources::s_ptexSceneDepth);
			m_passSMAAEdgeDetection.SetState(GS_NODEPTHTEST);
			m_passSMAAEdgeDetection.SetRequirePerViewConstantBuffer(true);
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
			m_passSMAABlendWeights.SetDepthTarget(CRendererResources::s_ptexSceneDepth);
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
	CTexture* pDestRT = GetUtils().GetTaaRT(RenderView(), true);
	CTexture* pPrevRT = ((SPostEffectsUtils::m_iFrameCounter - m_lastFrameID) < 10) ? GetUtils().GetTaaRT(RenderView(),false) : pCurrRT;

	assert((pCurrRT->GetFlags() & FT_USAGE_ALLOWREADSRGB));

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
		m_passTemporalAA.SetTextureSamplerPair(2, CRendererResources::s_ptexLinearDepth, EDefaultSamplerStates::PointClamp);
		m_passTemporalAA.SetTextureSamplerPair(3, GetUtils().GetVelocityObjectRT(RenderView()), EDefaultSamplerStates::PointClamp);
		m_passTemporalAA.SetTextureSamplerPair(4, pCurrRT, EDefaultSamplerStates::LinearClamp, EDefaultResourceViews::sRGB);
		m_passTemporalAA.SetTextureSamplerPair(5, pPrevRT, EDefaultSamplerStates::LinearClamp);
		m_passTemporalAA.SetTexture(16, CRendererResources::s_ptexSceneDepth);
	}

	(pMgpuRT = pDestRT)->MgpuResourceUpdate(true);
	m_passTemporalAA.BeginConstantUpdate();

	{
		size_t viewInfoCount = RenderView()->GetViewInfoCount();
	
		auto constants = m_passTemporalAA.BeginTypedConstantUpdate<PostAAConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);
		
		auto screenResolution = Vec2i(CRendererResources::s_renderWidth, CRendererResources::s_renderHeight);
		const float rcpWidth = 1.0f / (float)screenResolution.x;
		const float rcpHeight = 1.0f / (float)screenResolution.y;
		constants->screenSize = Vec4((float)screenResolution.x, (float)screenResolution.y, rcpWidth, rcpHeight);
		
		constants->params = Vec4(max(CRenderer::CV_r_AntialiasingTAASharpening + 1.0f, 1.0f), 0.0f, CRenderer::CV_r_AntialiasingTAAFalloffLowFreq + 1e-6f, CRenderer::CV_r_AntialiasingTAAFalloffHiFreq + 1e-6f);
		if (aaMode & eAT_TSAA_MASK)
			constants->params = Vec4(CRenderer::CV_r_AntialiasingTSAASubpixelDetection, CRenderer::CV_r_AntialiasingTSAASmoothness, 0, 0);

		// Compute reprojection matrix with highest possible precision to minimize numeric diffusion
		// TODO: Make sure NEAREST projection is handled correctly
		Matrix44_tpl<f64> matReprojection64[2];
		for (size_t i = 0; i < viewInfoCount; ++i)
		{
			const auto& viewInfo = RenderView()->GetViewInfo( static_cast<CCamera::EEye>(i) );
			
			Matrix44A matProj = viewInfo.unjitteredProjMatrix; // Use Projection matrix without the pixel shift

			// Changing stereo mode from dual-rendering to post-stereo causes 1 frame mismatch between projection matrix and stereo mode from GetStereoMode().
			const bool exceptionalCase = (pRenderer->GetS3DRend().GetStereoMode() == STEREO_MODE_POST_STEREO && CRenderer::CV_r_StereoMode == STEREO_MODE_DUAL_RENDERING);

			assert(pRenderer->GetS3DRend().GetStereoMode() == STEREO_MODE_DUAL_RENDERING
			       || exceptionalCase
			       || (matProj.m20 == 0 && matProj.m21 == 0)); // Ensure jittering is removed from projection matrix

			Matrix44_tpl<f64> matViewInv, matProjInv;
			mathMatrixLookAtInverse(&matViewInv, &viewInfo.viewMatrix);
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

			Matrix44 mPrevView = viewInfo.prevCameraMatrix;
			matReprojection64[i] = matProjInv * matViewInv * Matrix44_tpl<f64>(mPrevView) * Matrix44_tpl<f64>(matProj);
			matReprojection64[i] = matScaleBias2 * matReprojection64[i] * matScaleBias1;
		}

		constants->matReprojection = (Matrix44)matReprojection64[0];

		if (viewInfoCount > 1)
		{
			constants.BeginStereoOverride(true);
			constants->matReprojection = (Matrix44)matReprojection64[1];
		}

		m_passTemporalAA.EndTypedConstantUpdate(constants);
	}

	m_passTemporalAA.Execute();

	pCurrRT = pDestRT;
	m_lastFrameID = SPostEffectsUtils::m_iFrameCounter;
}

void CPostAAStage::DoFinalComposition(CTexture*& pCurrRT, CTexture* pDestRT, uint32 aaMode)
{
	PROFILE_LABEL_SCOPE("FLARES, GRAIN");

	CTexture* pTexLensOptics = CRendererResources::s_ptexSceneTargetR11G11B10F[0];
	CRY_ASSERT(pCurrRT != pDestRT);

	static uint64 prevRTMask = 0;
	uint64 rtMask = 0;
	if (aaMode & (eAT_SMAA_2TX_MASK | eAT_TSAA_MASK))
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (CRenderer::CV_r_colorRangeCompression)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE4];

	if (GetStdGraphicsPipeline().GetLensOpticsStage()->HasContent())
	{
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		if (CRenderer::CV_r_FlaresChromaShift > 0.5f / (float)CRendererResources::s_renderWidth)  // Only relevant if bigger than half pixel
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	}

	CTexture* pColorChartTex = CRendererResources::s_ptexBlack;
	if (CRenderer::CV_r_FlaresEnableColorGrading)
	{
		CColorGradingStage* pColorGradingStage = (CColorGradingStage*)GetStdGraphicsPipeline().GetStage(eStage_ColorGrading);
		if (CTexture* pColorChartTexTentative = pColorGradingStage->GetColorChart())
		{
			pColorChartTex = pColorChartTexTentative;
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE5];
		}
	}

	if (m_passComposition.InputChanged(pCurrRT->GetID(), pDestRT->GetID(), CRendererResources::s_ptexCurLumTexture->GetID(), pColorChartTex->GetID()) || rtMask != prevRTMask)
	{
		static CCryNameTSCRC techComposition("PostAAComposites");

		m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_passComposition.SetTechnique(CShaderMan::s_shPostAA, techComposition, rtMask);
		m_passComposition.SetRenderTarget(0, pDestRT);
		m_passComposition.SetState(GS_NODEPTHTEST);
		m_passComposition.SetTextureSamplerPair(0, pCurrRT, EDefaultSamplerStates::LinearClamp);
		m_passComposition.SetTextureSamplerPair(5, pTexLensOptics, EDefaultSamplerStates::PointClamp);
		m_passComposition.SetTextureSamplerPair(6, CRendererResources::s_ptexFilmGrainMap, EDefaultSamplerStates::PointWrap);
		m_passComposition.SetTextureSamplerPair(7, CRendererResources::s_ptexCurLumTexture, EDefaultSamplerStates::PointClamp);
		m_passComposition.SetTextureSamplerPair(8, pColorChartTex, EDefaultSamplerStates::LinearClamp);

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

	CTexture* pCurrRT = CRendererResources::s_ptexSceneDiffuse;
	CTexture* pCurrDS = CRendererResources::s_ptexSceneDepth;
	CTexture* pMgpuRT = NULL;

	// TODO: Support temporal AA in the editor
	uint32 aaMode = CRenderer::FX_GetAntialiasingType();

	if (aaMode && gcpRendD3D->IsEditorMode())
		aaMode = 1U << (eAT_SMAA_1X * CRenderer::CV_r_AntialiasingModeEditor);

	if (aaMode & eAT_SMAA_MASK)
		ApplySMAA(pCurrRT);
	if (aaMode & eAT_REQUIRES_PREVIOUSFRAME_MASK)
		ApplyTemporalAA(pCurrRT, pMgpuRT, aaMode);

	// TODO: Un-jitter depth buffer for AuxGeom depth tests (alternative: jitter aux)
	// TODO: Don't do anything and throw away depth when no depth-test/aux is used
	if (const auto* pRO = RenderView()->GetRenderOutput())
	{
		DoFinalComposition(pCurrRT, pRO->GetColorTarget(), aaMode);
		CStretchRectPass::GetPass().Execute(pCurrDS, pRO->GetDepthTarget());
	}

	if (pMgpuRT)
	{
		pMgpuRT->MgpuResourceUpdate(false);
	}
}
