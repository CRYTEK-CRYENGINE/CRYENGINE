// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PostAA.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/LensOptics.h"
#include "GraphicsPipeline/ColorGrading.h"

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

void CPostAAStage::CalculateJitterOffsets(int renderWidth, int renderHeight, CRenderView* pRenderView)
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

		pRenderView->m_vProjMatrixSubPixoffset.x = (vCurrSubSample.x * 2.0f / (float)renderWidth)  / downscaleFactor.x;
		pRenderView->m_vProjMatrixSubPixoffset.y = (vCurrSubSample.y * 2.0f / (float)renderHeight) / downscaleFactor.y;
	}
}

void CPostAAStage::Init()
{
	m_pTexAreaSMAA.Assign_NoAddRef(CTexture::ForName("%ENGINE%/EngineAssets/ScreenSpace/AreaTex.dds", FT_DONT_STREAM, eTF_Unknown));
	m_pTexSearchSMAA.Assign_NoAddRef(CTexture::ForName("%ENGINE%/EngineAssets/ScreenSpace/SearchTex.dds", FT_DONT_STREAM, eTF_Unknown));
	m_lastFrameID = -1;

	m_passTemporalAA.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
	m_passTemporalAA.AllocateTypedConstantBuffer<PostAAConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel);
}

void CPostAAStage::ApplySMAA(CTexture*& pCurrRT)
{
	CTexture* pEdgesRT = m_graphicsPipelineResources.m_pTexSceneNormalsMap;   // Reusing ESRAM resident target
	CTexture* pBlendWeightsRT = m_graphicsPipelineResources.m_pTexHDRTargetMasked;  // Reusing ESRAM resident target (FP16 RT accessed using point filtering which gives full rate on GCN)
	CTexture* pDestRT = m_graphicsPipelineResources.m_pTexSceneNormalsMap;
	CTexture* pZTexture = RenderView()->GetDepthTarget();

	if (!pEdgesRT || !pBlendWeightsRT)
		return;

	// Prepare stencil prepass
	int stencilRef = -1;
	if (CRenderer::CV_r_AntialiasingModeSCull)
	{
		m_graphicsPipeline.m_nStencilMaskRef += 1;

		if (m_graphicsPipeline.m_nStencilMaskRef > STENC_MAX_REF)
		{
			CClearSurfacePass::Execute(pZTexture, CLEAR_STENCIL, 0.0f, 0);
			m_graphicsPipeline.m_nStencilMaskRef = 1;
		}

		stencilRef = m_graphicsPipeline.m_nStencilMaskRef;
	}

	CClearSurfacePass::Execute(pEdgesRT, Clr_Transparent);
	CClearSurfacePass::Execute(pBlendWeightsRT, Clr_Transparent);

	// Pass 1: Edge Detection
	{
		if (m_passSMAAEdgeDetection.IsDirty(pCurrRT->GetTextureID(), pZTexture->GetTextureID(), CRenderer::CV_r_AntialiasingModeSCull))
		{
			static CCryNameTSCRC techEdgeDetection("LumaEdgeDetectionSMAA");
			m_passSMAAEdgeDetection.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAAEdgeDetection.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passSMAAEdgeDetection.SetTechnique(CShaderMan::s_shPostAA, techEdgeDetection, 0);
			m_passSMAAEdgeDetection.SetTargetClearMask(CPrimitiveRenderPass::eClear_Color0);
			m_passSMAAEdgeDetection.SetRenderTarget(0, pEdgesRT);
			m_passSMAAEdgeDetection.SetDepthTarget(pZTexture);
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
		if (m_passSMAABlendWeights.IsDirty(pZTexture->GetTextureID(), CRenderer::CV_r_AntialiasingModeSCull))
		{
			static CCryNameTSCRC techBlendWeights("BlendWeightSMAA");
			m_passSMAABlendWeights.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAABlendWeights.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passSMAABlendWeights.SetTechnique(CShaderMan::s_shPostAA, techBlendWeights, 0);
			m_passSMAABlendWeights.SetTargetClearMask(CPrimitiveRenderPass::eClear_Color0);
			m_passSMAABlendWeights.SetRenderTarget(0, pBlendWeightsRT);
			m_passSMAABlendWeights.SetDepthTarget(pZTexture);
			m_passSMAABlendWeights.SetState(GS_NODEPTHTEST);
			m_passSMAABlendWeights.SetTexture(0, pEdgesRT);
			m_passSMAABlendWeights.SetTexture(1, m_pTexAreaSMAA);
			m_passSMAABlendWeights.SetTexture(2, m_pTexSearchSMAA);
			m_passSMAABlendWeights.SetSampler(0, EDefaultSamplerStates::PointClamp);
			m_passSMAABlendWeights.SetSampler(1, EDefaultSamplerStates::LinearClamp);
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
		if (m_passSMAANeighborhoodBlending.IsDirty(pCurrRT->GetTextureID()))
		{
			static CCryNameTSCRC techNeighborhoodBlending("NeighborhoodBlendingSMAA");
			m_passSMAANeighborhoodBlending.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passSMAANeighborhoodBlending.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passSMAANeighborhoodBlending.SetTechnique(CShaderMan::s_shPostAA, techNeighborhoodBlending, 0);
			m_passSMAANeighborhoodBlending.SetRenderTarget(0, pDestRT);
			m_passSMAANeighborhoodBlending.SetState(GS_NODEPTHTEST);
			m_passSMAANeighborhoodBlending.SetTexture(0, pBlendWeightsRT);
			m_passSMAANeighborhoodBlending.SetTexture(1, pCurrRT);
			m_passSMAANeighborhoodBlending.SetSampler(0, EDefaultSamplerStates::PointClamp);
			m_passSMAANeighborhoodBlending.SetSampler(1, EDefaultSamplerStates::LinearClamp);
		}
		m_passSMAANeighborhoodBlending.BeginConstantUpdate();
		m_passSMAANeighborhoodBlending.Execute();
	}

	pCurrRT = pDestRT;
}

void CPostAAStage::ApplySRGB(CTexture*& pCurrRT)
{
	CTexture* pDestRT = m_graphicsPipelineResources.m_pTexSceneNormalsMap;
	m_passCopySRGB.Execute(pCurrRT, pDestRT);
	pCurrRT = pDestRT;
}

void CPostAAStage::ApplyTemporalAA(CTexture*& pCurrRT, CTexture*& pMgpuRT, uint32 aaMode)
{
	CShader* pShader = CShaderMan::s_shPostAA;
	CTexture* pDestRT = GetAARenderTarget(RenderView(), true);
	CTexture* pPrevRT = ((SPostEffectsUtils::m_iFrameCounter - m_lastFrameID) < 10) ? GetAARenderTarget(RenderView(), false) : pCurrRT;

	CRY_ASSERT_MESSAGE(pDestRT && pPrevRT, "PostAA rendertargets do not exist!");
	CRY_ASSERT_MESSAGE(pCurrRT->GetFlags() & FT_USAGE_ALLOWREADSRGB, "PostAA: Expected sRGB target.");
	if (!pDestRT || !pPrevRT)
		return;

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

		m_passTemporalAA.SetTextureSamplerPair(4, pCurrRT, EDefaultSamplerStates::LinearClamp, EDefaultResourceViews::sRGB);

		m_passTemporalAA.SetTexture(0, pCurrRT);
		m_passTemporalAA.SetTexture(1, pPrevRT);
		m_passTemporalAA.SetTexture(2, m_graphicsPipelineResources.m_pTexLinearDepth);
		m_passTemporalAA.SetTexture(3, GetUtils().GetVelocityObjectRT(RenderView()));
		m_passTemporalAA.SetTexture(5, pPrevRT);
		m_passTemporalAA.SetSampler(0, EDefaultSamplerStates::LinearClamp);
		m_passTemporalAA.SetSampler(1, EDefaultSamplerStates::PointClamp);
		m_passTemporalAA.SetTexture(16, RenderView()->GetDepthTarget());
	}

	(pMgpuRT = pDestRT)->MgpuResourceUpdate(true);
	m_passTemporalAA.BeginConstantUpdate();

	{
		size_t viewInfoCount = RenderView()->GetViewInfoCount();

		auto constants = m_passTemporalAA.BeginTypedConstantUpdate<PostAAConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel);

		auto screenResolution = Vec2i(m_graphicsPipeline.GetRenderResolution().x, m_graphicsPipeline.GetRenderResolution().y);
		const float rcpWidth = 1.0f / (float)screenResolution.x;
		const float rcpHeight = 1.0f / (float)screenResolution.y;
		constants->screenSize = Vec4((float)screenResolution.x, (float)screenResolution.y, rcpWidth, rcpHeight);

		constants->params = Vec4(max(CRenderer::CV_r_AntialiasingTAASharpening + 1.0f, 1.0f), 0.0f, CRenderer::CV_r_AntialiasingTAAFalloffLowFreq + 1e-6f, CRenderer::CV_r_AntialiasingTAAFalloffHiFreq + 1e-6f);
		if (aaMode & eAT_TSAA_MASK)
			constants->params = Vec4(CRenderer::CV_r_AntialiasingTSAASubpixelDetection, CRenderer::CV_r_AntialiasingTSAASmoothness, 0, 0);

		constants->matReprojection = RenderView()->GetViewInfo(CCamera::eEye_Left).GetReprojection();

		if (viewInfoCount > 1)
		{
			constants.BeginStereoOverride(true);
			constants->matReprojection = RenderView()->GetViewInfo(CCamera::eEye_Right).GetReprojection();
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

	CTexture* pTexLensOptics = m_graphicsPipelineResources.m_pTexSceneTargetR11G11B10F[0];
	CRY_ASSERT(pCurrRT != pDestRT);

	Vec4 hdrSetupParams[5];
	gEnv->p3DEngine->GetHDRSetupParams(hdrSetupParams);

	// Calculate grain amount
	CEffectParam* pParamGrainAmount = PostEffectMgr()->GetByName("FilterGrain_Amount");
	CEffectParam* pParamArtifactsGrain = PostEffectMgr()->GetByName("FilterArtifacts_Grain");
	const float paramGrainAmount = max(pParamGrainAmount->GetParam(), pParamArtifactsGrain->GetParam());
	const float environmentGrainAmount = hdrSetupParams[1].w * CRenderer::CV_r_HDRGrainAmount;
	const float grainAmount = max(paramGrainAmount, environmentGrainAmount);

	uint64 rtMask = 0;
	if (aaMode & (eAT_SMAA_2TX_MASK | eAT_TSAA_MASK))
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (CRenderer::CV_r_colorRangeCompression)
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE4];
	if (grainAmount && CRenderer::CV_r_GrainEnableExposureThreshold) // enable legacy grain/exposure interaction
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE0];

	auto* pLensOpticStage = m_graphicsPipeline.GetStage<CLensOpticsStage>();
	if (pLensOpticStage && pLensOpticStage->HasContent())
	{
		rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		if (CRenderer::CV_r_FlaresChromaShift > 0.5f / (float)m_graphicsPipeline.GetRenderResolution().x)  // Only relevant if bigger than half pixel
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	}

	CTexture* pColorChartTex = CRendererResources::s_ptexBlack;
	auto* pColorGradingStage = m_graphicsPipeline.GetStage<CColorGradingStage>();
	if (CRenderer::CV_r_FlaresEnableColorGrading && pColorGradingStage)
	{
		if (CTexture* pColorChartTexTentative = pColorGradingStage->GetColorChart())
		{
			pColorChartTex = pColorChartTexTentative;
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE5];
		}
	}

	int lumID = CRendererResources::s_ptexCurLumTexture ? CRendererResources::s_ptexCurLumTexture->GetTextureID() : 0;
	if (m_passComposition.IsDirty(pCurrRT->GetID(), pDestRT->GetID(), pColorChartTex->GetID(), lumID, rtMask))
	{
		static CCryNameTSCRC techComposition("PostAAComposites");

		m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_passComposition.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_passComposition.SetTechnique(CShaderMan::s_shPostAA, techComposition, rtMask);
		m_passComposition.SetRenderTarget(0, pDestRT);
		m_passComposition.SetState(GS_NODEPTHTEST);

		m_passComposition.SetTexture(0, pCurrRT);
		m_passComposition.SetTexture(5, pTexLensOptics);
		m_passComposition.SetTexture(6, CRendererResources::s_ptexFilmGrainMap);
		m_passComposition.SetTexture(7, CRendererResources::s_ptexCurLumTexture ? CRendererResources::s_ptexCurLumTexture : CRendererResources::s_ptexBlack);
		m_passComposition.SetTexture(8, pColorChartTex);

		m_passComposition.SetSampler(0, EDefaultSamplerStates::LinearClamp);
		m_passComposition.SetSampler(1, EDefaultSamplerStates::PointClamp);
		m_passComposition.SetSampler(2, EDefaultSamplerStates::PointWrap);
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
		const Vec4 v = Vec4(0, 0, 0, grainAmount);

		m_passComposition.SetConstant(hdrParamsName, v, eHWSC_Pixel);
		// This sets exposure clamping max,min, causing weird interaction between between Exp. min/max params and grain (CE-13325)
		// it will be ignored if CV_r_GrainEnableExposureThreshold is 0
		m_passComposition.SetConstant(hdrEyeAdaptationName, hdrSetupParams[4], eHWSC_Pixel);
	}

	m_passComposition.Execute();
}

void CPostAAStage::Execute()
{
	// TODO: Handle rapid camera position changes in a better way

	PROFILE_LABEL_SCOPE("POST_AA");

	// TODO: CPostEffectContext::GetDstBackBufferTexture() pre-EnableAltBackBuffer()
	CTexture* pCurrRT = m_graphicsPipelineResources.m_pTexDisplayTargetDst;
	CTexture* pMgpuRT = NULL;

	// TODO: Support temporal AA in the editor
	uint32 aaMode = CRenderer::FX_GetAntialiasingType();

	if (aaMode && gcpRendD3D->IsEditorMode())
		aaMode = 1U << (eAT_SMAA_1X * CRenderer::CV_r_AntialiasingModeEditor);

	if (aaMode & eAT_SMAA_MASK)
		ApplySMAA(pCurrRT);
	else if (aaMode & eAT_REQUIRES_PREVIOUSFRAME_MASK)
		ApplySRGB(pCurrRT);

	if (aaMode & eAT_REQUIRES_PREVIOUSFRAME_MASK)
		ApplyTemporalAA(pCurrRT, pMgpuRT, aaMode);

	// TODO: Un-jitter depth buffer for AuxGeom depth tests (alternative: jitter aux)
	// TODO: Don't do anything and throw away depth when no depth-test/aux is used
	{
		// TODO: CPostEffectContext::GetDstBackBufferTexture() post-EnableAltBackBuffer()
		CTexture* pDestRT = RenderView()->GetColorTarget();
		DoFinalComposition(pCurrRT, pDestRT, aaMode);
	}

	if (pMgpuRT)
	{
		pMgpuRT->MgpuResourceUpdate(false);
	}
}

void CPostAAStage::Resize(int renderWidth, int renderHeight)
{
	if (CRenderer::CV_r_AntialiasingMode)
	{
		const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
		ETEX_Format accumulatorFormat = eTF_R16G16B16A16;
		if (CRenderer::CV_r_AntialiasingMode == eAT_SMAA_2TX && CRendererCVars::CV_r_HDRTexFormat == 0)
			accumulatorFormat = eTF_R10G10B10A2;

		if (m_pPrevBackBuffersLeftEye[0] && m_pPrevBackBuffersLeftEye[0]->GetDstFormat() != accumulatorFormat)
		{
			SAFE_RELEASE(m_pPrevBackBuffersLeftEye[0]);
			SAFE_RELEASE(m_pPrevBackBuffersLeftEye[1]);
			SAFE_RELEASE(m_pPrevBackBuffersRightEye[0]);
			SAFE_RELEASE(m_pPrevBackBuffersRightEye[1]);
		}

		std::string prevBackBuffer0texName = "$PrevBackBuffer0" + m_graphicsPipeline.GetUniqueIdentifierName();
		std::string prevBackBuffer1texName = "$PrevBackBuffer1" + m_graphicsPipeline.GetUniqueIdentifierName();
		m_pPrevBackBuffersLeftEye[0] = CTexture::GetOrCreateRenderTarget(prevBackBuffer0texName.c_str(), renderWidth, renderHeight, Clr_Unknown, eTT_2D, renderTargetFlags, accumulatorFormat);
		m_pPrevBackBuffersLeftEye[1] = CTexture::GetOrCreateRenderTarget(prevBackBuffer1texName.c_str(), renderWidth, renderHeight, Clr_Unknown, eTT_2D, renderTargetFlags, accumulatorFormat);

		if (gRenDev->IsStereoEnabled())
		{
			prevBackBuffer0texName = "$PrevBackBuffer0_R" + m_graphicsPipeline.GetUniqueIdentifierName();
			prevBackBuffer1texName = "$PrevBackBuffer1_R" + m_graphicsPipeline.GetUniqueIdentifierName();
			m_pPrevBackBuffersRightEye[0] = CTexture::GetOrCreateRenderTarget(prevBackBuffer0texName.c_str(), renderWidth, renderHeight, Clr_Unknown, eTT_2D, renderTargetFlags, accumulatorFormat);
			m_pPrevBackBuffersRightEye[1] = CTexture::GetOrCreateRenderTarget(prevBackBuffer1texName.c_str(), renderWidth, renderHeight, Clr_Unknown, eTT_2D, renderTargetFlags, accumulatorFormat);
		}
		else
		{
			SAFE_RELEASE(m_pPrevBackBuffersRightEye[0]);
			SAFE_RELEASE(m_pPrevBackBuffersRightEye[1]);
		}
	}
	else
	{
		SAFE_RELEASE(m_pPrevBackBuffersLeftEye[0]);
		SAFE_RELEASE(m_pPrevBackBuffersLeftEye[1]);
		SAFE_RELEASE(m_pPrevBackBuffersRightEye[0]);
		SAFE_RELEASE(m_pPrevBackBuffersRightEye[1]);
	}

	oldStereoEnabledState = gRenDev->IsStereoEnabled();
	oldAAState = CRenderer::CV_r_AntialiasingMode;
}

void CPostAAStage::Update()
{
	// Check if Stereo or AA settings have been updated, if so we might need to recreate prevBackBuffer rendertarget
	if (oldStereoEnabledState != gRenDev->IsStereoEnabled() ||
		oldAAState != CRenderer::CV_r_AntialiasingMode)
		Resize(m_graphicsPipeline.GetRenderResolution().y, m_graphicsPipeline.GetRenderResolution().y);
}

CTexture* CPostAAStage::GetAARenderTarget(const CRenderView* pRenderView, bool bCurrentFrame) const
{
	int eye = static_cast<int>(pRenderView->GetCurrentEye());
	int index = (bCurrentFrame ? SPostEffectsUtils::m_iFrameCounter : (SPostEffectsUtils::m_iFrameCounter + 1)) % 2;

	CRY_ASSERT(eye == CCamera::eEye_Left || eye == CCamera::eEye_Right);

	if (eye == CCamera::eEye_Left)
		return m_pPrevBackBuffersLeftEye[index];
	else
		return m_pPrevBackBuffersRightEye[index];
}
