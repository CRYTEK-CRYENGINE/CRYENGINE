// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StandardGraphicsPipeline.h"

#include "ShadowMap.h"
#include "SceneGBuffer.h"
#include "SceneDepth.h"
#include "SceneForward.h"
#include "Sky.h"
#include "SceneCustom.h"
#include "AutoExposure.h"
#include "Bloom.h"
#include "HeightMapAO.h"
#include "ScreenSpaceObscurance.h"
#include "ScreenSpaceReflections.h"
#include "ScreenSpaceSSS.h"
#include "VolumetricFog.h"
#include "Fog.h"
#include "VolumetricClouds.h"
#include "Water.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "SunShafts.h"
#include "ToneMapping.h"
#include "PostAA.h"
#include "ComputeSkinning.h"
#include "GpuParticles.h"
#include "ClipVolumes.h"
#include "DeferredDecals.h"
#include "ShadowMask.h"
#include "TiledShading.h"
#include "ColorGrading.h"
#include "WaterRipples.h"
#include "LensOptics.h"
#include "PostEffects.h"
#include "Rain.h"
#include "Snow.h"
#include "MobileComposition.h"
#include "OmniCamera.h"
#include "TiledLightVolumes.h"
#include "DebugRenderTargets.h"

#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "Common/RenderDisplayContext.h"

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

CStandardGraphicsPipeline::CStandardGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key)
	: CGraphicsPipeline(desc, uniqueIdentifier, key)
{
}

void CStandardGraphicsPipeline::Init()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CStandardGraphicsPipeline::Init");

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CSceneCustomStage>();
	RegisterStage<CHeightMapAOStage>();
	RegisterStage<CScreenSpaceObscuranceStage>();
	RegisterStage<CScreenSpaceReflectionsStage>();
	RegisterStage<CScreenSpaceSSSStage>();
	RegisterStage<CVolumetricCloudsStage>();
	RegisterStage<CWaterRipplesStage>();
	RegisterStage<CMotionBlurStage>();
	RegisterStage<CDepthOfFieldStage>();
	RegisterStage<CAutoExposureStage>();
	RegisterStage<CBloomStage>();
	RegisterStage<CToneMappingStage>();
	RegisterStage<CSunShaftsStage>();
	RegisterStage<CPostAAStage>();
	RegisterStage<CComputeSkinningStage>();
	RegisterStage<CComputeParticlesStage>();
	RegisterStage<CDeferredDecalsStage>();
	RegisterStage<CShadowMaskStage>();
	RegisterStage<CTiledShadingStage>();
	RegisterStage<CWaterStage>(); // Has a custom PSO cache like Forward
	RegisterStage<CLensOpticsStage>();
	RegisterStage<CColorGradingStage>();
	RegisterStage<CPostEffectStage>();
	RegisterStage<CRainStage>();
	RegisterStage<CSnowStage>();
	RegisterStage<CSceneDepthStage>();
	RegisterStage<COmniCameraStage>();
	RegisterStage<CVolumetricFogStage>();

	// Register and initialize all common stages
	CGraphicsPipeline::Init();

	// Out-of-pipeline passes for display
	m_HDRToFramePass.reset(new CStretchRectPass(this));
	m_PostToFramePass.reset(new CStretchRectPass(this));
	m_FrameToFramePass.reset(new CStretchRectPass(this));

	m_HQSubResPass[0].reset(new CStableDownsamplePass(this));
	m_HQSubResPass[1].reset(new CStableDownsamplePass(this));
	m_LQSubResPass[0].reset(new CStretchRectPass(this));
	m_LQSubResPass[1].reset(new CStretchRectPass(this));

	m_ResolvePass.reset(new CStretchRectPass(this));
	m_DownscalePass.reset(new CDownsamplePass(this));
	m_UpscalePass.reset(new CSharpeningUpsamplePass(this));
	m_AnisoVBlurPass.reset(new CAnisotropicVerticalBlurPass(this));

	m_bInitialized = true;
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	CGraphicsPipeline::Resize(renderWidth, renderHeight);
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::ShutDown()
{
	m_bInitialized = false;

	CGraphicsPipeline::ShutDown();
	CGraphicsPipeline::SetCurrentRenderView(nullptr);

	m_HDRToFramePass.reset();
	m_PostToFramePass.reset();
	m_FrameToFramePass.reset();

	m_HQSubResPass[0].reset();
	m_HQSubResPass[1].reset();
	m_LQSubResPass[0].reset();
	m_LQSubResPass[1].reset();

	m_ResolvePass.reset();
	m_DownscalePass.reset();
	m_UpscalePass.reset();
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::Update(EShaderRenderingFlags renderingFlags)
{
	FUNCTION_PROFILER_RENDERER();

	m_numInvalidDrawcalls = 0;
	GenerateMainViewConstantBuffer();

	if (!m_changedCVars.GetCVars().empty())
	{
		CGraphicsPipeline::OnCVarsChanged(m_changedCVars);
		m_changedCVars.Reset();
	}

	m_renderingFlags = renderingFlags;
	CGraphicsPipeline::Update(renderingFlags);
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::ExecuteHDRPostProcessing()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("POST_EFFECTS_HDR");

	const auto& viewInfo = GetCurrentViewInfo(CCamera::eEye_Left);
	PostProcessUtils().m_pView = viewInfo.viewMatrix;
	PostProcessUtils().m_pProj = viewInfo.projMatrix;

	PostProcessUtils().m_pViewProj = PostProcessUtils().m_pView * PostProcessUtils().m_pProj;
	PostProcessUtils().m_pViewProj = ReverseDepthHelper::Convert(PostProcessUtils().m_pViewProj);
	PostProcessUtils().m_pViewProj.Transpose();

	if (GetStage<CRainStage>()->IsStageActive(m_renderingFlags))
		GetStage<CRainStage>()->Execute();

	// Note: MB uses m_pTexHDRTargetPrev to avoid doing another copy, so this should be right before the MB pass
	{
		m_FrameToFramePass->Execute(m_pipelineResources.m_pTexHDRTarget, m_pipelineResources.m_pTexHDRTargetPrev);
	}

	if (GetStage<CDepthOfFieldStage>()->IsStageActive(m_renderingFlags))
		GetStage<CDepthOfFieldStage>()->Execute();

	if (GetStage<CMotionBlurStage>()->IsStageActive(m_renderingFlags))
		GetStage<CMotionBlurStage>()->Execute();

	if (GetStage<CSnowStage>()->IsStageActive(m_renderingFlags))
		GetStage<CSnowStage>()->Execute();

	// Half resolution downsampling
	if (GetStage<CAutoExposureStage>()->IsStageActive(m_renderingFlags) ||
	    GetStage<CBloomStage>()->IsStageActive(m_renderingFlags) ||
	    GetStage<CSunShaftsStage>()->IsStageActive(m_renderingFlags))
	{
		PROFILE_LABEL_SCOPE("HALFRES_DOWNSAMPLE_HDRTARGET");

		if (CRendererCVars::CV_r_HDRBloomQuality > 1)
			m_HQSubResPass[0]->Execute(m_pipelineResources.m_pTexHDRTarget, m_pipelineResources.m_pTexHDRTargetScaled[0][0], true);
		else
			m_LQSubResPass[0]->Execute(m_pipelineResources.m_pTexHDRTarget, m_pipelineResources.m_pTexHDRTargetScaled[0][0]);
	}

	// Quarter resolution downsampling
	if (GetStage<CAutoExposureStage>()->IsStageActive(m_renderingFlags) ||
	    GetStage<CBloomStage>()->IsStageActive(m_renderingFlags))
	{
		PROFILE_LABEL_SCOPE("QUARTER_RES_DOWNSAMPLE_HDRTARGET");

		if (CRendererCVars::CV_r_HDRBloomQuality > 0)
			m_HQSubResPass[1]->Execute(m_pipelineResources.m_pTexHDRTargetScaled[0][0], m_pipelineResources.m_pTexHDRTargetScaled[1][0], CRendererCVars::CV_r_HDRBloomQuality >= 1);
		else
			m_LQSubResPass[1]->Execute(m_pipelineResources.m_pTexHDRTargetScaled[0][0], m_pipelineResources.m_pTexHDRTargetScaled[1][0]);
	}

	// reads CRendererResources::s_ptexHDRTargetScaled[1][0]
	if (GetStage<CAutoExposureStage>()->IsStageActive(m_renderingFlags))
		GetStage<CAutoExposureStage>()->Execute();

	// reads CRendererResources::s_ptexHDRTargetScaled[1][0] and then kills it
	if (GetStage<CBloomStage>()->IsStageActive(m_renderingFlags))
		GetStage<CBloomStage>()->Execute();

	// writes m_graphicsPipelineResources.m_pTexSceneTargetR11G11B10F[0]
	if (GetStage<CLensOpticsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CLensOpticsStage>()->Execute();

	// reads CRendererResources::s_ptexHDRTargetScaled[0][0]
	if (GetStage<CSunShaftsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CSunShaftsStage>()->Execute();

	if (GetStage<CColorGradingStage>()->IsStageActive(m_renderingFlags))
		GetStage<CColorGradingStage>()->Execute();

	// 0 is used for disable debugging and 1 is used to just show the average and estimated luminance, and exposure values.
	if (GetStage<CToneMappingStage>()->IsDebugDrawEnabled())
		GetStage<CToneMappingStage>()->ExecuteDebug();
	else
		GetStage<CToneMappingStage>()->Execute();
}

void CStandardGraphicsPipeline::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	// TODO: Make this into it's own stage or pipeline
	if (GetStage<CSceneCustomStage>() && GetStage<CSceneCustomStage>()->IsDebuggerEnabled() &&
		m_renderingFlags & EShaderRenderingFlags::SHDF_ALLOW_RENDER_DEBUG)
	{
		GetStage<CSceneCustomStage>()->ExecuteDebugger();

		if (GetStage<CSceneCustomStage>()->IsDebugOverlayEnabled())
			GetStage<CSceneCustomStage>()->ExecuteDebugOverlay();

		return;
	}

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	m_renderPassScheduler.SetEnabled(true);

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE");

	// Generate cloud volume textures for shadow mapping. Only needs view, and needs to run before ShadowMaskgen.
	GetStage<CVolumetricCloudsStage>()->ExecuteShadowGen();

	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		// Compute algorithms
		GetStage<CComputeSkinningStage>()->Execute();

		// Revert resource states to graphics pipeline
		GetStage<CComputeParticlesStage>()->PreDraw();
		GetStage<CComputeSkinningStage>()->PreDraw();

		if (GetStage<CRainStage>()->IsRainOcclusionEnabled())
			GetStage<CRainStage>()->ExecuteRainOcclusion();
	}

	// GBuffer
	GetStage<CSceneGBufferStage>()->Execute();

	// Wait for GBuffer draw jobs to finish
	renderItemDrawer.WaitForDrawSubmission();

	// Issue split barriers for GBuffer
	CTexture* pTextures[] = {
		m_pipelineResources.m_pTexSceneNormalsMap,
		m_pipelineResources.m_pTexSceneDiffuse,
		m_pipelineResources.m_pTexSceneSpecular,
		pZTexture
	};

	CDeviceGraphicsCommandInterface* pCmdList = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();
	pCmdList->BeginResourceTransitions(CRY_ARRAY_COUNT(pTextures), pTextures, eResTransition_TextureRead);

	// Shadow maps
	if (GetStage<CShadowMapStage>()->IsStageActive(m_renderingFlags))
		GetStage<CShadowMapStage>()->Execute();

	// Wait for Shadow Map draw jobs to finish (also required for HeightMap AO and SVOGI)
	renderItemDrawer.WaitForDrawSubmission();

	if (GetStage<CDeferredDecalsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CDeferredDecalsStage>()->Execute();

	if (GetStage<CSceneGBufferStage>()->IsGBufferVisualizationEnabled())
		GetStage<CSceneGBufferStage>()->ExecuteGBufferVisualization();

	// GBuffer modifiers
	if (GetStage<CRainStage>()->IsDeferredRainEnabled())
		GetStage<CRainStage>()->ExecuteDeferredRainGBuffer();

	if (GetStage<CSnowStage>()->IsDeferredSnowEnabled())
		GetStage<CSnowStage>()->ExecuteDeferredSnowGBuffer();

	// Generate cloud volume textures for shadow mapping.
	if (GetStage<CVolumetricCloudsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CVolumetricCloudsStage>()->ExecuteShadowGen();

	// SVOGI
	{
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance())
		{
			PROFILE_LABEL_SCOPE("SVOGI");

			CSvoRenderer::GetInstance()->UpdateCompute(pRenderView);
			CSvoRenderer::GetInstance()->UpdateRender(pRenderView);
		}
#endif
	}

	// Screen Space Reflections
	if (GetStage<CScreenSpaceReflectionsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CScreenSpaceReflectionsStage>()->Execute();

	// Height Map AO
	if (GetStage<CHeightMapAOStage>()->IsStageActive(m_renderingFlags))
		GetStage<CHeightMapAOStage>()->Execute();

	// Screen Space Obscurance
	if (GetStage<CScreenSpaceObscuranceStage>()->IsStageActive(m_renderingFlags))
		GetStage<CScreenSpaceObscuranceStage>()->Execute();

	if (GetStage<CTiledShadingStage>()->IsStageActive(m_renderingFlags))
		GetStage<CTiledLightVolumesStage>()->Execute();

	// Water volume caustics (before m_pTiledShadingStage->Execute())
	GetStage<CWaterStage>()->ExecuteWaterVolumeCaustics();

	SetPipelineFlags(GetPipelineFlags() | EPipelineFlags::NO_SHADER_FOG);

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

		GetStage<CClipVolumesStage>()->GenerateClipVolumeInfo();
		GetStage<CClipVolumesStage>()->Prepare();
		GetStage<CClipVolumesStage>()->Execute();

		if (GetStage<CTiledShadingStage>()->IsStageActive(m_renderingFlags))
		{
			GetStage<CShadowMaskStage>()->Prepare();
			GetStage<CShadowMaskStage>()->Execute();

			GetStage<CTiledShadingStage>()->Execute();

			if (GetStage<CScreenSpaceSSSStage>()->IsStageActive(m_renderingFlags))
				GetStage<CScreenSpaceSSSStage>()->Execute(m_pipelineResources.m_pTexSceneTargetR11G11B10F[0]);
		}
	}

	{
		PROFILE_LABEL_SCOPE("FORWARD Z");

		GetStage<CSceneForwardStage>()->ExecuteOpaque();

		// Opaque forward passes
		if (GetStage<CSkyStage>()->IsStageActive(m_renderingFlags))
			GetStage<CSkyStage>()->Execute(m_pipelineResources.m_pTexHDRTarget, pZTexture);
	}

	// Deferred ocean caustics
	if (GetStage<CWaterStage>()->IsDeferredOceanCausticsEnabled())
		GetStage<CWaterStage>()->ExecuteDeferredOceanCaustics();
	{
		// Fog
		if (GetStage<CVolumetricFogStage>()->IsStageActive(m_renderingFlags))
			GetStage<CVolumetricFogStage>()->Execute();

		if (GetStage<CFogStage>()->IsStageActive(m_renderingFlags))
			GetStage<CFogStage>()->Execute();

		SetPipelineFlags(GetPipelineFlags() & ~EPipelineFlags::NO_SHADER_FOG);

		// Clouds
		if (GetStage<CVolumetricCloudsStage>()->IsStageActive(m_renderingFlags))
			GetStage<CVolumetricCloudsStage>()->Execute();

		// Water fog volumes
		GetStage<CWaterStage>()->ExecuteWaterFogVolumeBeforeTransparent();
	}

	{
		PROFILE_LABEL_SCOPE("FORWARD T");

		// Transparent (below water)
		GetStage<CSceneForwardStage>()->ExecuteTransparentBelowWater();
		// Ocean and water volumes
		GetStage<CWaterStage>()->Execute();
		// Transparent (above water)
		GetStage<CSceneForwardStage>()->ExecuteTransparentAboveWater();

		if (GetStage<CSceneForwardStage>()->IsTransparentDepthFixupEnabled())
			GetStage<CSceneForwardStage>()->ExecuteTransparentDepthFixup();

		// Half-res particles
		if (GetStage<CSceneForwardStage>()->IsTransparentLoResEnabled())
			GetStage<CSceneForwardStage>()->ExecuteTransparentLoRes(1 + crymath::clamp<int>(CRendererCVars::CV_r_ParticlesHalfResAmount, 0, 1));
	}

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence(pRenderer->GetRenderFrameID());

	if (GetStage<CSnowStage>()->IsDeferredSnowDisplacementEnabled())
		GetStage<CSnowStage>()->ExecuteDeferredSnowDisplacement();

	if (pRenderView->GetCurrentEye() == CCamera::eEye_Right ||
	    !pRenderer->GetS3DRend().IsStereoEnabled() ||
	    !pRenderer->GetS3DRend().RequiresSequentialSubmission())
	{
		GetStage<CComputeParticlesStage>()->PostDraw();
	}

	if (!(m_renderingFlags & SHDF_CUBEMAPGEN))
	{
		// HDR and LDR post-processing
		{
			// CRendererResources::s_ptexHDRTarget -> CRendererResources::s_ptexDisplayTarget (Tonemapping)
			ExecuteHDRPostProcessing();

			// CRendererResources::s_ptexDisplayTarget
			GetStage<CSceneForwardStage>()->ExecuteAfterPostProcessHDR();

			// CRendererResources::s_ptexDisplayTarget -> CRenderOutput->m_pColorTarget (PostAA)
			// Post effects disabled, copy diffuse to color target
			if (!GetStage<CPostEffectStage>()->Execute())
				m_PostToFramePass->Execute(m_pipelineResources.m_pTexDisplayTargetDst, pRenderView->GetRenderOutput()->GetColorTarget());

			// CRenderOutput->m_pColorTarget
			GetStage<CSceneForwardStage>()->ExecuteAfterPostProcessLDR();
		}

		if (GetStage<CSceneCustomStage>()->IsSelectionHighlightEnabled())
		{
			GetStage<CSceneCustomStage>()->ExecuteHelpers();
			GetStage<CSceneCustomStage>()->ExecuteSelectionHighlight();
		}

		if (GetStage<CSceneCustomStage>()->IsDebugOverlayEnabled())
			GetStage<CSceneCustomStage>()->ExecuteDebugOverlay();

		// Display tone mapping debugging information on the screen
		if (GetStage<CToneMappingStage>()->IsDebugInfoEnabled())
			GetStage<CToneMappingStage>()->DisplayDebugInfo();
	}
	else
	{
		// Raw HDR copy
		m_HDRToFramePass->Execute(m_pipelineResources.m_pTexHDRTarget, pRenderView->GetRenderOutput()->GetColorTarget());
	}

	if (GetStage<COmniCameraStage>()->IsStageActive(m_renderingFlags))
		GetStage<COmniCameraStage>()->Execute();

	if (GetStage<CVolumetricFogStage>()->IsStageActive(m_renderingFlags))
		GetStage<CVolumetricFogStage>()->ResetFrame();

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE");

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute(this);

	if (CRendererCVars::CV_r_PipelineResourceDiscardAfterFrame > 1)
	{
		m_pipelineResources.Discard();
	}
}
