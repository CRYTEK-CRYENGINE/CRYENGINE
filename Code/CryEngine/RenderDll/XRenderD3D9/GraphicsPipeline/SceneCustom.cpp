// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneCustom.h"
#include "D3DPostProcess.h"
#include "CompiledRenderObject.h"
#include "DriverD3D.h"
#include "Common/ReverseDepth.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/TypedConstantBuffer.h"

CSceneCustomStage::CSceneCustomStage()
	: m_perPassResources()
{}
 
void CSceneCustomStage::Init()
{
	// Create per-pass resources
	m_pPerPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);	
	m_pPerPassConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerPassConstantBuffer_Custom));
	
	CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
	cb.UploadZeros();
	
	bool bSuccess = SetAndBuildPerPassResources(true);
	assert(bSuccess);

	// Create resource layout
	m_pResourceLayout = GetStdGraphicsPipeline().CreateScenePassLayout(m_perPassResources);

	// Freeze resource-set layout (assert will fire when violating the constraint)
	m_perPassResources.AcceptChangedBindPoints();
}

bool CSceneCustomStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	outPSO = NULL;

	const auto shaderType = desc.shaderItem.m_pShader->GetShaderType();
	const bool bIsCommonMesh = (shaderType != eST_Particle);
	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (bIsCommonMesh && !GetStdGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
		return true;

	CSceneRenderPass* pSceneRenderPass = nullptr;

	if (passID == ePass_DebugViewSolid)
	{
		psoDesc.m_RenderState = GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE;
		pSceneRenderPass = &m_debugViewPass;
	}
	else if (passID == ePass_DebugViewWireframe)
	{
		psoDesc.m_RenderState = GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE | GS_WIREFRAME;
		psoDesc.m_CullMode = eCULL_None;
		pSceneRenderPass = &m_debugViewPass;
	}
	else if (passID == ePass_DebugViewDrawModes)
	{
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		psoDesc.m_RenderState = GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE;
		pSceneRenderPass = &m_debugViewPass;
	}
	else if (passID == ePass_SelectionIDs)
	{
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		psoDesc.m_RenderState = GS_DEPTHFUNC_LEQUAL | GS_DEPTHWRITE;
		pSceneRenderPass = &m_selectionIDPass;
	}
	else if(passID == ePass_Silhouette)
	{
		// support only the mode of CRenderer::CV_r_customvisions=3.
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE2];

		psoDesc.m_RenderState = (desc.objectFlags & FOB_HUD_REQUIRE_DEPTHTEST) ? GS_DEPTHFUNC_LEQUAL : GS_NODEPTHTEST;
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5]; //Ignore depth threshold in SilhoueteVisionOptimised

		pSceneRenderPass = &m_silhouetteMaskPass;
	}

	if (shaderType == eST_Particle)
	{
		psoDesc.m_CullMode = eCULL_None;
		psoDesc.m_bAllowTesselation = true;
	}

	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}

	if (!pSceneRenderPass)
	{
		return false;
	}

	psoDesc.m_pRenderPass = pSceneRenderPass->GetRenderPass();

	if (!psoDesc.m_pRenderPass)
		return false;

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
	return outPSO != nullptr;
}

bool CSceneCustomStage::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	DevicePipelineStatesArray& stageStates = pStateArray[m_stageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_DEBUG;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewSolid, stageStates[ePass_DebugViewSolid]);
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewWireframe, stageStates[ePass_DebugViewWireframe]);
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewDrawModes, stageStates[ePass_DebugViewDrawModes]);
	if (gcpRendD3D->IsEditorMode())
		bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_SelectionIDs, stageStates[ePass_SelectionIDs]);

	_stateDesc.technique = TTYPE_CUSTOMRENDERPASS;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_Silhouette, stageStates[ePass_Silhouette]);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return bFullyCompiled;
}

bool CSceneCustomStage::SetAndBuildPerPassResources(bool bOnInit)
{
	assert(m_pPerPassConstantBuffer);
	
	// Samplers
	{
		auto materialSamplers = GetStdGraphicsPipeline().GetDefaultMaterialSamplers();
		for (int i = 0; i < materialSamplers.size(); ++i)
		{
			m_perPassResources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}
		// Hardcoded point samplers
		m_perPassResources.SetSampler(8, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetSampler(9, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);

		// Required by particles
		m_perPassResources.SetSampler(10, EDefaultSamplerStates::BilinearWrap, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetSampler(11, EDefaultSamplerStates::LinearCompare, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetSampler(2, EDefaultSamplerStates::TrilinearWrap, EShaderStage_AllWithoutCompute);
	}

	// Textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
			gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		m_perPassResources.SetTexture(ePerPassTexture_PerlinNoiseMap, CRendererResources::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_WindGrid, CRendererResources::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_NormalsFitting, CRendererResources::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_DissolveNoise, CRendererResources::s_ptexDissolveNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_SceneLinearDepth, CRendererResources::s_ptexLinearDepth, EDefaultResourceViews::Default, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_PaletteTexelsPerMeter, CRendererResources::s_ptexPaletteTexelsPerMeter, EDefaultResourceViews::Default, EShaderStage_Pixel);
	}

	// particle resources
	{
		if (bOnInit)
		{
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticlePositionStream, CDeviceBufferManager::GetNullBufferStructured(),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleAxesStream, CDeviceBufferManager::GetNullBufferStructured(),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleColorSTStream, CDeviceBufferManager::GetNullBufferStructured(),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}
		else
		{
			const CParticleBufferSet& particleBuffer = GetStdGraphicsPipeline().GetParticleBufferSet();
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticlePositionStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetPositionStream(RenderView()->GetFrameId())),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleAxesStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetAxesStream(RenderView()->GetFrameId())),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleColorSTStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetColorSTsStream(RenderView()->GetFrameId())),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}
	}
	
	// Constant buffers
	{
		CConstantBufferPtr pPerViewCB;
		if (bOnInit)  // Handle case when no view is available in the initialization of the stage
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
			pPerViewCB = GetStdGraphicsPipeline().GetMainViewConstantBuffer();

		m_perPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassConstantBuffer.get(), EShaderStage_AllWithoutCompute);
	}

	if (bOnInit)
		return true;

	CRY_ASSERT(!m_perPassResources.HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
	return m_pPerPassResourceSet->Update(m_perPassResources);
}

struct CHighlightPredicate
{
	bool operator() (SRendItem& item)
	{
		return (item.pCompiledObject->m_pRO->m_editorSelectionID & 0x2) == 0;
	}
};

struct CSelectionPredicate
{
	bool operator() (SRendItem& item)
	{
		return (item.pCompiledObject->m_pRO->m_editorSelectionID & 0x1) == 0;
	}
};

void CSceneCustomStage::Update()
{
	CRenderView* pRenderView = RenderView();

//	CTexture* pColorTexture = pRenderView->GetColorTarget();
	CTexture* pDepthTexture = pRenderView->GetDepthTarget();

	// Debug View Pass
	m_debugViewPass.SetLabel("CUSTOM_DEBUGVIEW");
	m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);
	m_debugViewPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_debugViewPass.SetRenderTargets(
		// Depth
		pDepthTexture,
		// Color 0
		CRendererResources::s_ptexHDRTarget
	);

	// Silhouette Pass
	m_silhouetteMaskPass.SetLabel("CUSTOM_SILHOUETTE");
	m_silhouetteMaskPass.SetupPassContext(m_stageID, ePass_Silhouette, TTYPE_CUSTOMRENDERPASS, FB_CUSTOM_RENDER, EFSLIST_CUSTOM);
	m_silhouetteMaskPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_silhouetteMaskPass.SetRenderTargets(
		// Depth
		pDepthTexture,
		// Color 0
		CRendererResources::s_ptexSceneNormalsMap
	);

	if (gEnv->IsEditor())
	{
		// Highlighted ID Pass
		m_selectionIDPass.SetLabel("CUSTOM_HIGHLIGHTED_PASS");
		m_selectionIDPass.SetupPassContext(m_stageID, ePass_SelectionIDs, TTYPE_DEBUG, FB_GENERAL, EFSLIST_CUSTOM);
		m_selectionIDPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
		m_selectionIDPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			CRendererResources::s_ptexSceneSelectionIDs
		);
	}
}

bool CSceneCustomStage::DoDebugRendering()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;

	return (bViewTexelDensity || bViewWireframe);
}

bool CSceneCustomStage::DoDebugOverlay()
{
	bool bDebugDraw = CRenderer::CV_e_DebugDraw != 0;

	return (bDebugDraw);
}

void CSceneCustomStage::Prepare()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	bool bDebugDraw = CRenderer::CV_e_DebugDraw != 0;
	// should probably somehow allow some editor viewports to not use this pass
	bool bSelectionIDPass = pRenderer->IsEditorMode() && !gEnv->IsEditorGameMode();

	SetAndBuildPerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	if (bViewTexelDensity || bViewWireframe || bDebugDraw)
		m_debugViewPass.PrepareRenderPassForUse(commandList);
	else if (bSelectionIDPass)
		m_selectionIDPass.PrepareRenderPassForUse(commandList);
}

void CSceneCustomStage::ExecuteDebugger()
{
	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();

	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = gcpRendD3D->GetWireframeMode() != R_SOLID_MODE;
	const bool bReverseDepth = true;

	m_debugViewPass.SetLabel("DEBUG_VIEW");
	m_debugViewPass.SetViewport(pRenderView->GetViewport());

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(bViewTexelDensity ? 1.f : 0.f, CRenderer::CV_r_TexelsPerMeter, 0.f, 0.f);
		cb.CopyToDevice();
	}

	CTexture* pTargetRT = pRenderView->GetColorTarget();
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	m_debugViewPass.ExchangeRenderTarget(0, pTargetRT);
	m_debugViewPass.ExchangeDepthTarget(pTargetDS);

	SetAndBuildPerPassResources(false);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, 1);
	CClearSurfacePass::Execute(pTargetRT, ColorF(0.2, 0.2, 0.2, 1));

	if (!bViewWireframe)
		m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);
	else
		m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewWireframe, TTYPE_DEBUG, FB_GENERAL);

	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);

	// NOTE: no more external state changes in here, everything should have been setup
	{
		Prepare();

		renderItemDrawer.InitDrawSubmission();

		m_debugViewPass.BeginExecution();
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		m_debugViewPass.DrawTransparentRenderItems(pRenderView, EFSLIST_TRANSP_BW);
		m_debugViewPass.DrawTransparentRenderItems(pRenderView, EFSLIST_TRANSP_AW);
		m_debugViewPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	SAFE_RELEASE(pTargetDS);
}

void CSceneCustomStage::ExecuteDebugOverlay()
{
	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();
	const bool bReverseDepth = true;

	m_debugViewPass.SetLabel("DEBUG_OVERLAY");
	m_debugViewPass.SetViewport(pRenderView->GetViewport());

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(0.f, 0.f, 0.f, 0.f);
		cb.CopyToDevice();
	}

	CTexture* pTargetRT = pRenderView->GetRenderOutput()->GetColorTarget();
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	m_debugViewPass.ExchangeRenderTarget(0, pTargetRT);
	m_debugViewPass.ExchangeDepthTarget(pTargetDS);

	SetAndBuildPerPassResources(false);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, 1);

	m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewDrawModes, TTYPE_DEBUG, FB_DEBUG);
	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);

	// NOTE: no more external state changes in here, everything should have been setup
	{
		Prepare();

		renderItemDrawer.InitDrawSubmission();

		m_debugViewPass.BeginExecution();
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		m_debugViewPass.DrawTransparentRenderItems(pRenderView, EFSLIST_TRANSP_BW);
		m_debugViewPass.DrawTransparentRenderItems(pRenderView, EFSLIST_TRANSP_AW);
		m_debugViewPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	SAFE_RELEASE(pTargetDS);
}

void CSceneCustomStage::ExecuteSelectionHighlight()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();
	const bool bReverseDepth = true;

	// first check if we actually have anything worth drawing
	uint32 numItems = pRenderView->GetRenderItems(EFSLIST_HIGHLIGHT).size();
	if (numItems == 0)
		return;

	// update our depth texture here
	CTexture* pTargetRT = CRendererResources::s_ptexSceneSelectionIDs;
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	m_selectionIDPass.SetLabel("EDITOR_SELECTION_HIGHLIGHT");
	m_selectionIDPass.SetViewport(D3DViewPort{
		0.f,
		0.f,
		float(pTargetRT->GetWidth()),
		float(pTargetRT->GetHeight()),
		0.0f,
		1.0f
	});

	SetAndBuildPerPassResources(false);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, 1);
	CClearSurfacePass::Execute(pTargetRT, ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	m_selectionIDPass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_selectionIDPass.SetRenderTargets(
		// Depth
		pTargetDS,
		// Color 0
		pTargetRT
	);

	// NOTE: no more external state changes in here, everything should have been setup
	{
		Prepare();

		renderItemDrawer.InitDrawSubmission();

		CHighlightPredicate highlightPredicate;
		CSelectionPredicate selectionPredicate;

		uint32 startHighlight = pRenderView->FindRenderListSplit(highlightPredicate, EFSLIST_HIGHLIGHT, 0, numItems);
		uint32 startSelected  = pRenderView->FindRenderListSplit(selectionPredicate, EFSLIST_HIGHLIGHT, 0, startHighlight);

		// First pass, draw selected object IDs
		m_selectionIDPass.BeginExecution();
		m_selectionIDPass.DrawRenderItems(pRenderView, EFSLIST_HIGHLIGHT, startSelected, numItems);
		m_selectionIDPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	// Now, we use the selection ID texture to alpha composite highlights and outlines on our scene
	static CCryNameTSCRC techSilhouette("SelectionSilhouetteHighlight");

	CTexture* pTargetTex = pRenderView->GetColorTarget();
	if (m_highlightPass.IsDirty(pTargetTex->GetID()))
	{
		m_highlightPass.SetTechnique(CShaderMan::s_shPostEffects, techSilhouette, 0);
		m_highlightPass.SetRenderTarget(0, pTargetTex);
		m_highlightPass.SetState(GS_NODEPTHTEST | GS_BLDST_ONEMINUSSRCALPHA | GS_BLSRC_SRCALPHA);
		m_highlightPass.SetTextureSamplerPair(0, pTargetRT, EDefaultResourceViews::Default);
		m_highlightPass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	}

	static CCryNameR highlightColorName("highlightColor");
	static CCryNameR selectionColorName("selectionColor");
	static CCryNameR ghostAlphaName("ghostAlpha");
	static CCryNameR outlineName("outlineWidth");

	m_highlightPass.BeginConstantUpdate();
	m_highlightPass.SetConstant(highlightColorName, pRenderer->GetHighlightColor().toVec4(), eHWSC_Pixel);
	m_highlightPass.SetConstant(selectionColorName, pRenderer->GetSelectionColor().toVec4(), eHWSC_Pixel);
	m_highlightPass.SetConstant(ghostAlphaName, Vec4(pRenderer->GetHighlightParams().y), eHWSC_Pixel);
	m_highlightPass.SetConstant(outlineName, Vec4(pRenderer->GetHighlightParams().x), eHWSC_Vertex);

	m_highlightPass.Execute();

	SAFE_RELEASE(pTargetDS);
}

void CSceneCustomStage::ExecuteSilhouettePass()
{
	CRenderView* pRenderView = RenderView();

	if (!(pRenderView->GetBatchFlags(EFSLIST_CUSTOM) & FB_CUSTOM_RENDER))
		return;

	auto prevPipelineFlags = GetStdGraphicsPipeline().GetPipelineFlags();

	{
		auto& renderItemDrawer = pRenderView->GetDrawer();

		CClearSurfacePass::Execute(CRendererResources::s_ptexSceneNormalsMap, Clr_Transparent);

		SetAndBuildPerPassResources(false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		m_silhouetteMaskPass.PrepareRenderPassForUse(commandList);

		const bool bReverseDepth = true;
		CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
		passFlags |= bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None;

		m_silhouetteMaskPass.SetFlags(passFlags);
		m_silhouetteMaskPass.SetViewport(pRenderView->GetViewport());

		renderItemDrawer.InitDrawSubmission();

		m_silhouetteMaskPass.BeginExecution();
		m_silhouetteMaskPass.DrawRenderItems(pRenderView, EFSLIST_CUSTOM);
		m_silhouetteMaskPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	GetStdGraphicsPipeline().SetPipelineFlags(prevPipelineFlags);
}

void CSceneCustomStage::ExecuteHelpers()
{
	CRenderView* pRenderView = RenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();

	// first check if we actually have anything worth drawing
	if (!pRenderView->GetRenderItems(EFSLIST_DEBUG_HELPER).size())
		return;

	m_debugViewPass.SetLabel("DEBUG_HELPERS");

	{
		bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;

		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(0.f, 0.f, 0.f, 0.f);
		cb.CopyToDevice();
	}

	CTexture* pTargetTex = pRenderView->GetColorTarget();
	CTexture* pDepthTex  = pRenderView->GetDepthTarget();

	m_debugViewPass.ExchangeRenderTarget(0, pTargetTex);
	m_debugViewPass.ExchangeDepthTarget(pDepthTex);

	SetAndBuildPerPassResources(false);

	m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_debugViewPass.PrepareRenderPassForUse(commandList);

	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);
	m_debugViewPass.SetViewport(pRenderView->GetViewport());

	renderItemDrawer.InitDrawSubmission();

	m_debugViewPass.BeginExecution();
	m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_DEBUG_HELPER);
	m_debugViewPass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}
