// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "SceneCustom.h"
#include "D3DPostProcess.h"
#include "DriverD3D.h"
#include "Common/ReverseDepth.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/TypedConstantBuffer.h"

CSceneCustomStage::CSceneCustomStage()
	: m_perPassResources()
{}
 
void CSceneCustomStage::Init()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	// Create per-pass resources
	m_pPerPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);	
	m_pPerPassConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerPassConstantBuffer_Custom));
	
	CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom> cb(m_pPerPassConstantBuffer);
	cb.UploadZeros();
	
	bool bSuccess = SetAndBuildPerPassResources(true);
	assert(bSuccess);

	// Create resource layout
	m_pResourceLayout = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_perPassResources);

	// Freeze resource-set layout (assert will fire when violating the constraint)
	m_perPassResources.AcceptChangedBindPoints();

	// Debug View Pass
	m_debugViewPass.SetLabel("CUSTOM_DEBUGVIEW");
	m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);
	m_debugViewPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_debugViewPass.SetRenderTargets(
		// Depth
		gcpRendD3D->GetCurrentDepthOutput(),
		// Color 0
		gcpRendD3D->GetCurrentTargetOutput()
	);

	// Silhouette Pass
	m_silhouetteMaskPass.SetLabel("CUSTOM_SILHOUETTE");
	m_silhouetteMaskPass.SetupPassContext(m_stageID, ePass_Silhouette, TTYPE_CUSTOMRENDERPASS, FB_CUSTOM_RENDER, EFSLIST_CUSTOM);
	m_silhouetteMaskPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_silhouetteMaskPass.SetRenderTargets(
	  // Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexSceneNormalsMap
	);

	// Highlighted ID Pass
	m_selectionIDPass.SetLabel("CUSTOM_HIGHLIGHTED_PASS");
	m_selectionIDPass.SetupPassContext(m_stageID, ePass_SelectionIDs, TTYPE_DEBUG, FB_GENERAL, EFSLIST_CUSTOM);
	m_selectionIDPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_selectionIDPass.SetRenderTargets(
		// Depth. Initialize with null since depth texture will not be properly initialized at this point
		CTexture::s_ptexSceneHalfDepthStencil,
		// Color 0
		CTexture::s_ptexSceneSelectionIDs
	);
}

bool CSceneCustomStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	outPSO = NULL;

	const auto shaderType = desc.shaderItem.m_pShader->GetShaderType();
	const bool bIsCommonMesh = (shaderType != eST_Particle);
	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (bIsCommonMesh && !pRenderer->GetGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
		return true;

	CSceneRenderPass* pSceneRenderPass;

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

		// support only no-depth-test mode, not support COB_HUD_REQUIRE_DEPTHTEST of render object custom flag.
		psoDesc.m_RenderState = GS_NODEPTHTEST;
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE5]; //Ignore depth threshold in SilhoueteVisionOptimised

		pSceneRenderPass = &m_silhouetteMaskPass;
	}

	if (shaderType == eST_Particle)
	{
		psoDesc.m_CullMode = eCULL_None;
		psoDesc.m_bAllowTesselation = true;
	}

	if (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}
	psoDesc.m_pRenderPass = pSceneRenderPass->GetRenderPass();

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
	CD3D9Renderer* pRenderer = gcpRendD3D;

	assert(m_pPerPassConstantBuffer);
	
	// Samplers
	{
		auto materialSamplers = pRenderer->GetGraphicsPipeline().GetDefaultMaterialSamplers();
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
	}

	// Textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
			gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		m_perPassResources.SetTexture(ePerPassTexture_PerlinNoiseMap, CTexture::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_WindGrid, CTexture::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_NormalsFitting, CTexture::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_DissolveNoise, CTexture::s_ptexDissolveNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_SceneLinearDepth, CTexture::s_ptexZTarget, EDefaultResourceViews::Default, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_PaletteTexelsPerMeter, CTexture::s_ptexPaletteTexelsPerMeter, EDefaultResourceViews::Default, EShaderStage_Pixel);
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
			const CParticleBufferSet& particleBuffer = pRenderer->m_RP.m_particleBuffer;
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticlePositionStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetPositionStream()),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleAxesStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetAxesStream()),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			m_perPassResources.SetBuffer(
				EReservedTextureSlot_ParticleColorSTStream,
				const_cast<CGpuBuffer*>(&particleBuffer.GetColorSTsStream()),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}
	}
	
	// Constant buffers
	{
		CConstantBufferPtr pPerViewCB;
		if (bOnInit)  // Handle case when no view is available in the initialization of the stage
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
			pPerViewCB = pRenderer->GetGraphicsPipeline().GetMainViewConstantBuffer();

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
		return (item.pObj->m_editorSelectionID & 0x2) == 0;
	}
};

struct CSelectionPredicate
{
	bool operator() (SRendItem& item)
	{
		return (item.pObj->m_editorSelectionID & 0x1) == 0;
	}
};

void CSceneCustomStage::Prepare(CRenderView* pRenderView)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	bool bDebugDraw = CRenderer::CV_e_DebugDraw > 0;
	// should probably somehow allow some editor viewports to not use this pass
	bool bSelectionIDPass = pRenderer->IsEditorMode() && !gEnv->IsEditorGameMode();

	SetAndBuildPerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	if (bViewTexelDensity || bViewWireframe || bDebugDraw)
		m_debugViewPass.PrepareRenderPassForUse(commandList);
	else if (bSelectionIDPass)
		m_selectionIDPass.PrepareRenderPassForUse(commandList);
}

void CSceneCustomStage::Execute_DebugModes()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	bool bDebugDraw = CRenderer::CV_e_DebugDraw > 0;
	// should probably somehow allow some editor viewports to not use this pass
	bool bSelectionIDPass = pRenderer->IsEditorMode() && !gEnv->IsEditorGameMode();

	D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
	pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));
	m_debugViewPass.SetViewport(viewport);

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(bViewTexelDensity ? 1.f : 0.f, CRenderer::CV_r_TexelsPerMeter, 0.f, 0.f);
		cb.CopyToDevice();
	}

	SetAndBuildPerPassResources(false);

	const bool bReverseDepth = (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	pRenderer->FX_ClearTarget(gcpRendD3D->GetCurrentDepthOutput()->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil), CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 0.0f : 1.0f, 1);

	if (!bDebugDraw)
	{
		pRenderer->FX_ClearTarget(pRenderer->GetCurrentTargetOutput(), ColorF(0.2, 0.2, 0.2, 1));

		if (!bViewWireframe)
			m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);
		else
			m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewWireframe, TTYPE_DEBUG, FB_GENERAL);
	}
	else
	{
		m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewDrawModes, TTYPE_DEBUG, FB_DEBUG);
	}

	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);
	m_debugViewPass.SetRenderTargets(gcpRendD3D->GetCurrentDepthOutput(), gcpRendD3D->GetCurrentTargetOutput());

	// NOTE: no more external state changes in here, everything should have been setup
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	{
		Prepare(pRenderView);

		pRenderView->GetDrawer().InitDrawSubmission();

		m_debugViewPass.BeginExecution();
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP);
		m_debugViewPass.EndExecution();

		pRenderView->GetDrawer().JobifyDrawSubmission();
		pRenderView->GetDrawer().WaitForDrawSubmission();
	}
}

void CSceneCustomStage::Execute_SelectionID()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	// should probably somehow allow some editor viewports to not use this pass
	bool bSelectionIDPass = pRenderer->IsEditorMode() && !gEnv->IsEditorGameMode();

	// first check if we actually have anything worth drawing
	uint32 numItems = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_HIGHLIGHT).size();

	if (numItems == 0)
		return;

	// update our depth texture here
	CTexture* pDepthRT = CTexture::s_ptexSceneHalfDepthStencil;

	m_depthTarget.nWidth = pDepthRT->GetWidth();
	m_depthTarget.nHeight = pDepthRT->GetHeight();
	m_depthTarget.nFrameAccess = -1;
	m_depthTarget.bBusy = false;
	m_depthTarget.pTexture = pDepthRT;
	m_depthTarget.pTarget = pDepthRT->GetDevTexture()->Get2DTexture();
	m_depthTarget.pSurface = pDepthRT->GetDevTexture()->LookupDSV(EDefaultResourceViews::DepthStencil);

	D3DViewPort viewport = { 0.f, 0.f, float(CTexture::s_ptexSceneSelectionIDs->GetWidth()), float(CTexture::s_ptexSceneSelectionIDs->GetHeight()), 0.0f, 1.0f };
	pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));
	m_selectionIDPass.SetViewport(viewport);

	SetAndBuildPerPassResources(false);

	const bool bReverseDepth = (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	pRenderer->FX_ClearTarget(&m_depthTarget, CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 0.0f : 1.0f, 1);
	pRenderer->FX_ClearTarget(CTexture::s_ptexSceneSelectionIDs, ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	m_selectionIDPass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_selectionIDPass.SetRenderTargets(
		// Depth
		pDepthRT,
		// Color 0
		CTexture::s_ptexSceneSelectionIDs
	);

	// NOTE: no more external state changes in here, everything should have been setup
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	{
		Prepare(pRenderView);

		pRenderView->GetDrawer().InitDrawSubmission();

		CHighlightPredicate highlightPredicate;
		CSelectionPredicate selectionPredicate;

		uint32 startHighlight = pRenderView->FindRenderListSplit(highlightPredicate, EFSLIST_HIGHLIGHT, 0, numItems);
		uint32 startSelected = pRenderView->FindRenderListSplit(selectionPredicate, EFSLIST_HIGHLIGHT, 0, startHighlight);

		// First pass, draw selected object IDs
		m_selectionIDPass.BeginExecution();
		m_selectionIDPass.DrawRenderItems(pRenderView, EFSLIST_HIGHLIGHT, startSelected, numItems);
		m_selectionIDPass.EndExecution();

		pRenderView->GetDrawer().JobifyDrawSubmission();
		pRenderView->GetDrawer().WaitForDrawSubmission();
	}

	// Now, we use the selection ID texture to alpha composite highlights and outlines on our scene
	viewport.Width = float(pRenderer->m_MainViewport.nWidth);
	viewport.Height = float(pRenderer->m_MainViewport.nHeight);

	pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));
	m_highlightPass.SetViewport(viewport);

	static CCryNameTSCRC techSilhouette("SelectionSilhouetteHighlight");

	CTexture* pRT = pRenderer->GetCurrentTargetOutput();
	if (m_highlightPass.InputChanged(pRT->GetID()))
	{
		m_highlightPass.SetTechnique(CShaderMan::s_shPostEffects, techSilhouette, 0);
		m_highlightPass.SetRenderTarget(0, pRT);
		m_highlightPass.SetState(GS_NODEPTHTEST | GS_BLDST_ONEMINUSSRCALPHA | GS_BLSRC_SRCALPHA);
		m_highlightPass.SetTextureSamplerPair(0, CTexture::s_ptexSceneSelectionIDs, EDefaultResourceViews::Default);
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
}

void CSceneCustomStage::Execute()
{	
	CD3D9Renderer* pRenderer = gcpRendD3D;
	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	bool bDebugDraw = CRenderer::CV_e_DebugDraw > 0;
	// should probably somehow allow some editor viewports to not use this pass
	bool bSelectionIDPass = pRenderer->IsEditorMode() && !gEnv->IsEditorGameMode();

	if (!bViewTexelDensity && !bViewWireframe && !bSelectionIDPass && !bDebugDraw)
		return;
	
	PROFILE_LABEL_SCOPE("CUSTOM_SCENE_PASSES");
	
	if (bViewTexelDensity || bViewWireframe || bDebugDraw)
	{
		Execute_DebugModes();
	}
	else if (bSelectionIDPass)
	{
		Execute_SelectionID();
	}
}

void CSceneCustomStage::ExecuteSilhouettePass()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	if (!(SRendItem::BatchFlags(EFSLIST_CUSTOM) & FB_CUSTOM_RENDER))
	{
		return;
	}

	auto prevPersFlags2 = pRenderer->m_RP.m_PersFlags2;
	pRenderer->m_RP.m_PersFlags2 &= ~RBPF2_NOPOSTAA;

	{
		D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
		pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

		pRenderer->FX_ClearTarget(CTexture::s_ptexSceneNormalsMap, Clr_Transparent);

		SetAndBuildPerPassResources(false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		m_silhouetteMaskPass.PrepareRenderPassForUse(commandList);

		const SThreadInfo& threadInfo = pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID];
		const bool bReverseDepth = (threadInfo.m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
		CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
		passFlags |= bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None;

		m_silhouetteMaskPass.SetFlags(passFlags);
		m_silhouetteMaskPass.SetViewport(viewport);

		CRenderView* pRenderView = pRenderer->GetGraphicsPipeline().GetCurrentRenderView();

		RenderView()->GetDrawer().InitDrawSubmission();

		m_silhouetteMaskPass.BeginExecution();
		m_silhouetteMaskPass.DrawRenderItems(pRenderView, EFSLIST_CUSTOM);
		m_silhouetteMaskPass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}

	pRenderer->m_RP.m_PersFlags2 = prevPersFlags2;
}

void CSceneCustomStage::ExecuteHelperPass()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	PROFILE_LABEL_SCOPE("CUSTOM_SCENE_PASSE_DEBUG_HELPER");

	D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
	pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

	{
		bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;

		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(bViewTexelDensity ? 1.f : 0.f, CRenderer::CV_r_TexelsPerMeter, 0.f, 0.f);
		cb.CopyToDevice();
	}

	CRenderView* pRenderView = RenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

	CTexture* pTargetTex = nullptr;
	CTexture* pDepthTex = nullptr;
	if (pOutput)
	{
		pTargetTex = pOutput->GetHDRTargetTexture();
		pDepthTex = pOutput->GetDepthTexture();
	}
	else
	{
		pTargetTex = gcpRendD3D->GetCurrentTargetOutput();
		pDepthTex = gcpRendD3D->GetCurrentDepthOutput();
	}

	m_debugViewPass.ExchangeRenderTarget(0, pTargetTex);
	m_debugViewPass.ExchangeDepthTarget(pDepthTex);

	SetAndBuildPerPassResources(false);

	m_debugViewPass.SetupPassContext(m_stageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_debugViewPass.PrepareRenderPassForUse(commandList);

	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);
	m_debugViewPass.SetViewport(viewport);

	RenderView()->GetDrawer().InitDrawSubmission();

	m_debugViewPass.BeginExecution();
	m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_DEBUG_HELPER);
	m_debugViewPass.EndExecution();

	RenderView()->GetDrawer().JobifyDrawSubmission();
	RenderView()->GetDrawer().WaitForDrawSubmission();
}
