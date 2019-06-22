// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MinimumGraphicsPipeline.h"

#include "ShadowMap.h"
#include "SceneGBuffer.h"
#include "SceneForward.h"
#include "SceneCustom.h"
#include "ComputeSkinning.h"
#include "GpuParticles.h"
#include "ClipVolumes.h"
#include "Sky.h"
#include "TiledShading.h"
#include "TiledLightVolumes.h"

#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "Common/RenderDisplayContext.h"

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

CMinimumGraphicsPipeline::CMinimumGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key)
	: CGraphicsPipeline(desc, uniqueIdentifier, key)
{
}

void CMinimumGraphicsPipeline::Init()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CMinimumGraphicsPipeline::Init");

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CSceneCustomStage>();
	RegisterStage<CComputeSkinningStage>();
	RegisterStage<CComputeParticlesStage>();

	// Register all common stages
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

	m_bInitialized = true;
}

//////////////////////////////////////////////////////////////////////////
void CMinimumGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	CGraphicsPipeline::Resize(renderWidth, renderHeight);
}

//////////////////////////////////////////////////////////////////////////
void CMinimumGraphicsPipeline::ShutDown()
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
void CMinimumGraphicsPipeline::Update(EShaderRenderingFlags renderingFlags)
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
void CMinimumGraphicsPipeline::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

	// Render into these targets
	CTexture* pColorTex = pRenderView->GetColorTarget();
	CTexture* pDepthTex = pRenderView->GetDepthTarget();

	const bool bRecursive = pRenderView->IsRecursive();

	m_renderPassScheduler.SetEnabled(true);

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		GetStage<CComputeParticlesStage>()->PreDraw();
		GetStage<CComputeSkinningStage>()->Execute();
	}

	// recursive pass doesn't use deferred fog, instead uses forward shader fog.
	SetPipelineFlags(GetPipelineFlags() & ~EPipelineFlags::NO_SHADER_FOG);

	if (!(m_renderingFlags & SHDF_CUBEMAPGEN))
	{
		if (!bRecursive && pOutput)
		{
			gRenDev->GetIRenderAuxGeom()->Submit();
		}
	}

	// GBuffer ZPass only
	GetStage<CSceneGBufferStage>()->ExecuteMinimumZpass();

	// Function call is guaranteed to be side-effect-free
	//GetStage<CSceneDepthStage>()->Execute();

	// forward opaque and transparent passes for recursive rendering
	GetStage<CSceneForwardStage>()->ExecuteMinimum(pColorTex, pDepthTex);

	if (GetStage<CSkyStage>()->IsStageActive(m_renderingFlags))
		GetStage<CSkyStage>()->Execute(pColorTex, pDepthTex);

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence(pRenderer->GetRenderFrameID());

	if (pRenderView->GetCurrentEye() == CCamera::eEye_Right ||
	    !pRenderer->GetS3DRend().IsStereoEnabled() ||
	    !pRenderer->GetS3DRend().RequiresSequentialSubmission())
	{
		// Recursive pass doesn't need calling PostDraw().
		// Because general and recursive passes are executed in the same frame between BeginFrame() and EndFrame().
		if (!(pRenderView->IsRecursive()))
		{
			GetStage<CComputeParticlesStage>()->PostDraw();
		}
	}

	if (!(m_renderingFlags & SHDF_CUBEMAPGEN))
	{
		GetStage<CSceneCustomStage>()->ExecuteHelpers();
	}

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute(this);

	if (CRendererCVars::CV_r_PipelineResourceDiscardAfterFrame > 1)
	{
		m_pipelineResources.Discard();
	}
}
