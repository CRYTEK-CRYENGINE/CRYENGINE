// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MobileGraphicsPipeline.h"

#include "ShadowMap.h"
#include "SceneGBuffer.h"
#include "SceneForward.h"
#include "SceneCustom.h"
#include "SceneDepth.h"
#include "ComputeSkinning.h"
#include "Sky.h"
#include "GpuParticles.h"
#include "ClipVolumes.h"
#include "TiledShading.h"
#include "TiledLightVolumes.h"
#include "MobileComposition.h"

#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "Common/RenderDisplayContext.h"

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

CMobileGraphicsPipeline::CMobileGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key)
	: CGraphicsPipeline(desc, uniqueIdentifier, key)
{
}

void CMobileGraphicsPipeline::Init()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CMobileGraphicsPipeline::Init");

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CSceneCustomStage>();
	RegisterStage<CTiledShadingStage>();
	RegisterStage<CMobileCompositionStage>();
	RegisterStage<CSceneDepthStage>();

	// Register and initialize all common stages
	CGraphicsPipeline::Init();

	// Out-of-pipeline passes for display
	m_HDRToFramePass.reset(new CStretchRectPass(this));
	m_PostToFramePass.reset(new CStretchRectPass(this));
	m_FrameToFramePass.reset(new CStretchRectPass(this));

	m_LZSubResPass[0].reset(new CDepthDownsamplePass(this));
	m_LZSubResPass[1].reset(new CDepthDownsamplePass(this));
	m_LZSubResPass[2].reset(new CDepthDownsamplePass(this));
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
void CMobileGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	CGraphicsPipeline::Resize(renderWidth, renderHeight);
}

//////////////////////////////////////////////////////////////////////////
void CMobileGraphicsPipeline::ShutDown()
{
	m_bInitialized = false;

	CGraphicsPipeline::ShutDown();
	CGraphicsPipeline::SetCurrentRenderView(nullptr);

	m_HDRToFramePass.reset();
	m_PostToFramePass.reset();
	m_FrameToFramePass.reset();

	m_LZSubResPass[0].reset();
	m_LZSubResPass[1].reset();
	m_LZSubResPass[2].reset();
	m_HQSubResPass[0].reset();
	m_HQSubResPass[1].reset();
	m_LQSubResPass[0].reset();
	m_LQSubResPass[1].reset();

	m_ResolvePass.reset();
	m_DownscalePass.reset();
	m_UpscalePass.reset();
}

//////////////////////////////////////////////////////////////////////////
void CMobileGraphicsPipeline::Update(EShaderRenderingFlags renderingFlags)
{
	FUNCTION_PROFILER_RENDERER();

	m_numInvalidDrawcalls = 0;
	GenerateMainViewConstantBuffer();

	m_renderingFlags = renderingFlags;
	CGraphicsPipeline::Update(renderingFlags);

	if (!m_changedCVars.GetCVars().empty())
	{
		CGraphicsPipeline::OnCVarsChanged(m_changedCVars);

		m_changedCVars.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMobileGraphicsPipeline::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	if (CRendererCVars::CV_r_GraphicsPipelineMobile == 2)
		GetStage<CSceneGBufferStage>()->Execute();
	else
		GetStage<CSceneGBufferStage>()->ExecuteMicroGBuffer();

	pRenderView->GetDrawer().WaitForDrawSubmission();

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

		{
			GetStage<CClipVolumesStage>()->GenerateClipVolumeInfo();
			GetStage<CTiledLightVolumesStage>()->Execute();
			GetStage<CTiledShadingStage>()->Execute();
			GetStage<CMobileCompositionStage>()->ExecuteDeferredLighting();
		}
	}

	// Opaque and transparent forward passes
	if (GetStage<CSkyStage>())
	{
		GetStage<CSkyStage>()->Execute(m_pipelineResources.m_pTexHDRTarget, pZTexture);
	}
	GetStage<CSceneForwardStage>()->ExecuteMobile();

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence(pRenderer->GetRenderFrameID());

	GetStage<CMobileCompositionStage>()->ExecutePostProcessing(pRenderView->GetRenderOutput()->GetColorTarget());

	pRenderer->m_pPostProcessMgr->End(pRenderView);

	if (CRendererCVars::CV_r_PipelineResourceDiscardAfterFrame > 1)
	{
		m_pipelineResources.Discard();
	}
}
