// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharacterToolGraphicsPipeline.h"

#include "SceneGBuffer.h"
#include "SceneForward.h"
#include "SceneCustom.h"
#include "SceneDepth.h"
#include "ComputeSkinning.h"
#include "ClipVolumes.h"
#include "ScreenSpaceObscurance.h"
#include "ScreenSpaceSSS.h"
#include "TiledShading.h"
#include "Sky.h"
#include "PostAA.h"
#include "TiledLightVolumes.h"
#include "MobileComposition.h"
#include "ScreenSpaceReflections.h"

#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "Common/RenderDisplayContext.h"

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

CCharacterToolGraphicsPipeline::CCharacterToolGraphicsPipeline(const IRenderer::SGraphicsPipelineDescription& desc, const std::string& uniqueIdentifier, const SGraphicsPipelineKey key)
	: CGraphicsPipeline(desc, uniqueIdentifier, key)
{
}

void CCharacterToolGraphicsPipeline::Init()
{
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CCharacterToolGraphicsPipeline::Init");

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CSceneCustomStage>();
	RegisterStage<CComputeSkinningStage>();
	RegisterStage<CSceneDepthStage>();
	RegisterStage<CTiledShadingStage>();
	RegisterStage<CScreenSpaceObscuranceStage>();
	RegisterStage<CScreenSpaceSSSStage>();
	RegisterStage<CMobileCompositionStage>();
	RegisterStage<CPostAAStage>();
	RegisterStage<CScreenSpaceReflectionsStage>();

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
void CCharacterToolGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	CGraphicsPipeline::Resize(renderWidth, renderHeight);
}

//////////////////////////////////////////////////////////////////////////
void CCharacterToolGraphicsPipeline::ShutDown()
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
void CCharacterToolGraphicsPipeline::Update(EShaderRenderingFlags renderingFlags)
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
void CCharacterToolGraphicsPipeline::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = GetCurrentRenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

	// Render into these targets
	CTexture* pDepthTex = pRenderView->GetDepthTarget();
	const bool bRecursive = pRenderView->IsRecursive();

	m_renderPassScheduler.SetEnabled(true);

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE_CHARACTER_TOOL_SHADING");

	// Compute algorithms
	GetStage<CComputeSkinningStage>()->Execute();

	// Revert resource states to graphics pipeline
	GetStage<CComputeSkinningStage>()->PreDraw();

	if (!bRecursive && pOutput)
	{
		gRenDev->GetIRenderAuxGeom()->Submit();
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
		pDepthTex
	};

	CDeviceGraphicsCommandInterface* pCmdList = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();
	pCmdList->BeginResourceTransitions(CRY_ARRAY_COUNT(pTextures), pTextures, eResTransition_TextureRead);

	// Screen Space Reflections
	if (GetStage<CScreenSpaceReflectionsStage>()->IsStageActive(m_renderingFlags))
		GetStage<CScreenSpaceReflectionsStage>()->Execute();

	// Screen Space Obscurance
	if (GetStage<CScreenSpaceObscuranceStage>()->IsStageActive(m_renderingFlags))
		GetStage<CScreenSpaceObscuranceStage>()->Execute();

	if (GetStage<CTiledShadingStage>()->IsStageActive(m_renderingFlags))
		GetStage<CTiledLightVolumesStage>()->Execute();

	SetPipelineFlags(GetPipelineFlags() | EPipelineFlags::NO_SHADER_FOG);

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

		GetStage<CClipVolumesStage>()->GenerateClipVolumeInfo();

		if (GetStage<CTiledShadingStage>()->IsStageActive(m_renderingFlags))
		{
			GetStage<CTiledShadingStage>()->Execute();

			if (GetStage<CScreenSpaceSSSStage>()->IsStageActive(m_renderingFlags))
				GetStage<CScreenSpaceSSSStage>()->Execute(m_pipelineResources.m_pTexSceneTargetR11G11B10F[0]);
		}
	}

	{
		PROFILE_LABEL_SCOPE("FORWARD Z");

		GetStage<CSceneForwardStage>()->ExecuteOpaque();

		if (GetStage<CSkyStage>())
		{
			GetStage<CSkyStage>()->ExecuteMinimum(m_pipelineResources.m_pTexHDRTarget, pDepthTex);
		}
	}

	SetPipelineFlags(GetPipelineFlags() & ~EPipelineFlags::NO_SHADER_FOG);

	{
		PROFILE_LABEL_SCOPE("FORWARD T");

		// Transparent (below water)
		GetStage<CSceneForwardStage>()->ExecuteTransparentBelowWater();

		// Transparent (above water)
		GetStage<CSceneForwardStage>()->ExecuteTransparentAboveWater();

		if (GetStage<CSceneForwardStage>()->IsTransparentDepthFixupEnabled())
			GetStage<CSceneForwardStage>()->ExecuteTransparentDepthFixup();
	}

	// Note: MB uses m_pTexHDRTargetPrev to avoid doing another copy, so this should be right before the MB pass
	{
		m_FrameToFramePass->Execute(m_pipelineResources.m_pTexHDRTarget, m_pipelineResources.m_pTexHDRTargetPrev);
	}

	GetStage<CMobileCompositionStage>()->ExecutePostProcessing(m_pipelineResources.m_pTexDisplayTargetDst);

	GetStage<CPostAAStage>()->Execute();

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE_CHARACTER_TOOL_SHADING");

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute(this);

	if (CRendererCVars::CV_r_PipelineResourceDiscardAfterFrame > 0)
	{
		m_pipelineResources.Discard();
	}
}
