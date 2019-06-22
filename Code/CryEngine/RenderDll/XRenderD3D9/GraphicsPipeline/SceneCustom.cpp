// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneCustom.h"
#include "D3DPostProcess.h"
#include "CompiledRenderObject.h"
#include "Common/ReverseDepth.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/TypedConstantBuffer.h"

CSceneCustomStage::CSceneCustomStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_perPassResources()
	, m_highlightPass(&graphicsPipeline)
	, m_resolvePass(&graphicsPipeline)
{}

void CSceneCustomStage::Init()
{
	// Create per-pass resources
	m_pPerPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pPerPassConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerPassConstantBuffer_Custom));

	CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
	cb.UploadZeros();
	CRY_VERIFY(UpdatePerPassResources(true));

	// Create resource layout
	m_pResourceLayout = m_graphicsPipeline.CreateScenePassLayout(m_perPassResources);

	// Freeze resource-set layout (assert will fire when violating the constraint)
	m_perPassResources.AcceptChangedBindPoints();

	_smart_ptr<CTexture> pDummyDepthTexture;
	pDummyDepthTexture = CTexture::GetOrCreateTextureObject("SCENE_CUSTOM_Z_DUMMY", 0, 0, 1,
	                                                        eTT_2D, 0, CRendererResources::GetDepthFormat());

	_smart_ptr<CTexture> pDummyColorTexture;
	pDummyColorTexture = CTexture::GetOrCreateTextureObject("SCENE_CUSTOM_COLOR_DUMMY", 0, 0, 1,
	                                                        eTT_2D, 0, eTF_R8G8B8A8);

	_smart_ptr<CTexture> pDummyHDRTexture;
	pDummyHDRTexture = CTexture::GetOrCreateTextureObject("SCENE_CUSTOM_HDR_DUMMY", 0, 0, 1,
	                                                      eTT_2D, 0, eTF_R11G11B10F);

	_smart_ptr<CTexture> pDummySelectionIDTexture;
	pDummySelectionIDTexture = CTexture::GetOrCreateTextureObject("SCENE_CUSTOM_SELECTION_ID_DUMMY", 0, 0, 1,
	                                                              eTT_2D, 0, eTF_R32F);

	m_debugViewPass.SetLabel("CUSTOM_DEBUGVIEW");
	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);
	m_debugViewPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_debugViewPass.SetRenderTargets(pDummyDepthTexture, pDummyHDRTexture);

	m_silhouetteMaskPass.SetLabel("CUSTOM_SILHOUETTE");
	m_silhouetteMaskPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);
	m_silhouetteMaskPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_silhouetteMaskPass.SetRenderTargets(pDummyDepthTexture, pDummyColorTexture);

	m_selectionIDPass.SetLabel("CUSTOM_HIGHLIGHTED_PASS");
	m_selectionIDPass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_selectionIDPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_selectionIDPass.SetRenderTargets(pDummyDepthTexture, pDummySelectionIDTexture);
}

bool CSceneCustomStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	outPSO = NULL;

	const auto shaderType = desc.shaderItem.m_pShader->GetShaderType();
	const bool bIsCommonMesh = (shaderType != eST_Particle);
	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (bIsCommonMesh && !CSceneRenderPass::FillCommonScenePassStates(desc, psoDesc, m_graphicsPipeline.GetVrProjectionManager()))
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
	else if (passID == ePass_DebugViewOverdraw)
	{
		psoDesc.m_RenderState = GS_NODEPTHTEST;
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
	else if (passID == ePass_Silhouette)
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
	DevicePipelineStatesArray& stageStates = pStateArray[StageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_DEBUG;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewSolid, stageStates[ePass_DebugViewSolid]);
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewWireframe, stageStates[ePass_DebugViewWireframe]);
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DebugViewOverdraw , stageStates[ePass_DebugViewOverdraw ]);
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

bool CSceneCustomStage::UpdatePerPassResources(bool bOnInit)
{
	assert(m_pPerPassConstantBuffer);

	// Samplers
	{
		auto materialSamplers = m_graphicsPipeline.GetDefaultMaterialSamplers();
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
		m_perPassResources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_NormalsFitting, CRendererResources::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_SceneLinearDepth, m_graphicsPipelineResources.m_pTexLinearDepth, EDefaultResourceViews::Default, EShaderStage_Pixel);
		m_perPassResources.SetTexture(ePerPassTexture_PaletteTexelsPerMeter, CRendererResources::s_ptexPaletteTexelsPerMeter, EDefaultResourceViews::Default, EShaderStage_Pixel);
	}

	// UA Buffers
	if (bOnInit || (CRenderer::CV_r_OverdrawComplexity == 0) || !m_overdrawCount.IsAvailable())
	{
		m_perPassResources.SetBuffer(2, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(3, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(4, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(5, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
	}
	else
	{
		// A buffer the same dimension as the Depth-Target
		m_perPassResources.SetBuffer(2, &m_overdrawCount, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(3, &m_overdrawDepth, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(4, &m_overdrawStats, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		m_perPassResources.SetBuffer(5, &m_overdrawHisto, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
	}

	// particle resources
	m_graphicsPipeline.SetParticleBuffers(bOnInit, m_perPassResources, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

	// Constant buffers
	{
		CConstantBufferPtr pPerViewCB;
		if (bOnInit)  // Handle case when no view is available in the initialization of the stage
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
			pPerViewCB = m_graphicsPipeline.GetMainViewConstantBuffer();

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
	bool operator()(SRendItem& item)
	{
		return (item.pCompiledObject->m_pRO->m_editorSelectionID & 0x2) == 0;
	}
};

struct CSelectionPredicate
{
	bool operator()(SRendItem& item)
	{
		return (item.pCompiledObject->m_pRO->m_editorSelectionID & 0x1) == 0;
	}
};

void CSceneCustomStage::Update()
{
	const CRenderView* pRenderView = RenderView();
	const SRenderViewport& viewport = pRenderView->GetViewport();

	CTexture* pColorTexture = pRenderView->GetColorTarget();
	CTexture* pDepthTexture = pRenderView->GetDepthTarget();

	UpdatePerPassResources(false);

	// Debug View Pass
	m_debugViewPass.SetViewport(viewport);
	m_debugViewPass.SetRenderTargets(
		// Depth
		pDepthTexture,
		// Color 0
		pColorTexture
	);

	// Silhouette Pass
	m_silhouetteMaskPass.SetViewport(viewport);
	m_silhouetteMaskPass.SetRenderTargets(
		// Depth
		pDepthTexture,
		// Color 0
		m_graphicsPipelineResources.m_pTexSceneNormalsMap
	);

	if (gEnv->IsEditor())
	{
		// Highlighted ID Pass
		m_selectionIDPass.SetViewport(viewport);
		m_selectionIDPass.SetRenderTargets(
			// Depth
			pDepthTexture,
			// Color 0
			m_graphicsPipelineResources.m_pTexSceneSelectionIDs
		);
	}
}

void CSceneCustomStage::Resize(int renderWidth, int renderHeight)
{
	const uint32 numPixels = renderWidth * renderHeight;

	if (m_overdrawCount.IsAvailable() && ((CRenderer::CV_r_OverdrawComplexity == 0) || (m_overdrawCount.GetElementCount() != numPixels)))
		m_overdrawCount.Release();
	if (m_overdrawDepth.IsAvailable() && ((CRenderer::CV_r_OverdrawComplexity == 0) || (m_overdrawDepth.GetElementCount() != numPixels)))
		m_overdrawDepth.Release();

	if (!m_overdrawCount.IsAvailable() && (CRenderer::CV_r_OverdrawComplexity != 0))
		m_overdrawCount.Create(numPixels, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);
	if (!m_overdrawDepth.IsAvailable() && (CRenderer::CV_r_OverdrawComplexity != 0))
		m_overdrawDepth.Create(numPixels, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);
}

void CSceneCustomStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	if (cvarUpdater.GetCVar("r_OverdrawComplexity"))
	{
		if (!m_overdrawStats.IsAvailable())
			m_overdrawStats.Create(  4, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);
		if (!m_overdrawHisto.IsAvailable())
			m_overdrawHisto.Create(256, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);

		const CRenderView* pRenderView = RenderView();
		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];

		Resize(renderWidth, renderHeight);
	}
}

bool CSceneCustomStage::IsDebuggerEnabled() const
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe    = pRenderer->GetWireframeMode() != R_SOLID_MODE;
	bool bViewOverdraw     = CRenderer::CV_r_OverdrawComplexity != 0;
	bool bDebugDraw        = IsDebugOverlayEnabled();

	return (bViewTexelDensity || bViewWireframe || bViewOverdraw || bDebugDraw);
}

void CSceneCustomStage::ExecuteDebugger()
{
	CRenderView* pRenderView = RenderView();
	const SRenderViewport& rViewport = pRenderView->GetViewport();

	bool bViewTexelDensity = CRenderer::CV_r_TexelsPerMeter > 0;
	bool bViewWireframe    = gcpRendD3D->GetWireframeMode() != R_SOLID_MODE;
	bool bViewOverdraw     = CRenderer::CV_r_OverdrawComplexity != 0;
	bool bViewODZPrePass   = CRenderer::CV_r_OverdrawComplexity < 0;

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		if (bViewTexelDensity)
			cb->CP_Custom_ViewMode = Vec4(1,          CRenderer::CV_r_TexelsPerMeter     , float(rViewport.width), float(rViewport.height));
		else if (bViewOverdraw && !bViewODZPrePass)
			cb->CP_Custom_ViewMode = Vec4(2, std::abs(CRenderer::CV_r_OverdrawComplexity), float(rViewport.width), float(rViewport.height));
		else if (bViewOverdraw && bViewODZPrePass)
			cb->CP_Custom_ViewMode = Vec4(2,                   2 /* enable depth-write */, float(rViewport.width), float(rViewport.height));
		cb.CopyToDevice();
	}

	CTexture* pTargetRT = pRenderView->GetColorTarget();
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, 0.0f, 1);
	CClearSurfacePass::Execute(pTargetRT, ColorF(0.2f, 0.2f, 0.2f, 1.0f));

	m_debugViewPass.ExchangeRenderTarget(0, pTargetRT);
	m_debugViewPass.ExchangeDepthTarget(pTargetDS);

	m_debugViewPass.SetFlags(CSceneRenderPass::ePassFlags_VrProjectionPass);

	// Prepare ========================================================================
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_debugViewPass.PrepareRenderPassForUse(commandList);

	if (bViewOverdraw)
	{
		const int32 renderWidth  = pRenderView->GetRenderResolution()[0];
		const int32 renderHeight = pRenderView->GetRenderResolution()[1];
		const uint32 numPixels = renderWidth * renderHeight;

		if (m_overdrawCount.IsAvailable() && ((CRenderer::CV_r_OverdrawComplexity == 0) || (m_overdrawCount.GetElementCount() != numPixels)))
			m_overdrawCount.Release();
		if (m_overdrawDepth.IsAvailable() && ((CRenderer::CV_r_OverdrawComplexity == 0) || (m_overdrawDepth.GetElementCount() != numPixels)))
			m_overdrawDepth.Release();

		if (!m_overdrawCount.IsAvailable() && (CRenderer::CV_r_OverdrawComplexity != 0))
			m_overdrawCount.Create(numPixels, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);
		if (!m_overdrawDepth.IsAvailable() && (CRenderer::CV_r_OverdrawComplexity != 0))
			m_overdrawDepth.Create(numPixels, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);

		if (!m_overdrawStats.IsAvailable())
			m_overdrawStats.Create(  4, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);
		if (!m_overdrawHisto.IsAvailable())
			m_overdrawHisto.Create(256, 4, DXGI_FORMAT_R32_UINT, CDeviceObjectFactory::BIND_SHADER_RESOURCE | CDeviceObjectFactory::BIND_UNORDERED_ACCESS, nullptr);

		{
			// Full clear
			const ColorI nulls = { 0, 0, 0, 0 };

			commandList.GetComputeInterface()->ClearUAV(m_overdrawCount.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
			commandList.GetComputeInterface()->ClearUAV(m_overdrawDepth.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
			commandList.GetComputeInterface()->ClearUAV(m_overdrawStats.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
			commandList.GetComputeInterface()->ClearUAV(m_overdrawHisto.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
		}

		if (bViewODZPrePass)
		{
			// Execute ========================================================================
			auto& renderItemDrawer = pRenderView->GetDrawer();
			renderItemDrawer.InitDrawSubmission();

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, ePass_DebugViewOverdraw, TTYPE_DEBUG, FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_ZPREPASS_NEAREST);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_ZPREPASS);
			m_debugViewPass.EndExecution();

			renderItemDrawer.JobifyDrawSubmission();
			renderItemDrawer.WaitForDrawSubmission();

			{
				// Clear without dumping zprepass values
				const ColorI nulls = { 0, 0, 0, 0 };

				commandList.GetComputeInterface()->ClearUAV(m_overdrawCount.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
				commandList.GetComputeInterface()->ClearUAV(m_overdrawStats.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
				commandList.GetComputeInterface()->ClearUAV(m_overdrawHisto.GetDevBuffer()->LookupUAV(EDefaultResourceViews::UnorderedAccess), nulls, 0, nullptr);
			}
		}
	}

	// NOTE: no more external state changes in here, everything should have been setup
	{
		// Execute ========================================================================
		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();
		
		CMotionBlurPredicate mbPredicate;
		CZPrePassPredicate zpPredicate;

		EPass passID;
		if (bViewWireframe)
			passID = ePass_DebugViewWireframe;
		else if (bViewOverdraw)
			passID = ePass_DebugViewOverdraw;
		else
			passID = ePass_DebugViewSolid;
		
		{
			int nearestNum            = pRenderView->GetRenderItems(EFSLIST_NEAREST_OBJECTS).size();
 			int nearestVelocity       = pRenderView->FindRenderListSplit(mbPredicate, EFSLIST_NEAREST_OBJECTS, 0, nearestNum);
			int nearestNoZPre         = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_NEAREST_OBJECTS, 0, nearestVelocity);
			int nearestVelocityNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_NEAREST_OBJECTS, nearestVelocity, nearestNum);

			int generalNum            = pRenderView->GetRenderItems(EFSLIST_GENERAL).size();
			int generalVelocity       = pRenderView->FindRenderListSplit(mbPredicate, EFSLIST_GENERAL, 0, generalNum);
			int generalNoZPre         = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_GENERAL, 0, generalVelocity);
			int generalVelocityNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_GENERAL, generalVelocity, generalNum);

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_Z, FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, 0, nearestNoZPre);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, 0, generalNoZPre);
			m_debugViewPass.EndExecution();

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_Z | FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, nearestNoZPre, nearestVelocity);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, generalNoZPre, generalVelocity);
			m_debugViewPass.EndExecution();

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_Z, FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, nearestVelocity, nearestVelocityNoZPre);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, generalVelocity, generalVelocityNoZPre);
			m_debugViewPass.EndExecution();

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_Z | FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, nearestVelocityNoZPre, nearestNum);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, generalVelocityNoZPre, generalNum);
			m_debugViewPass.EndExecution();
		}
		
		{
			int nearestFNum    = pRenderView->GetRenderItems(EFSLIST_FORWARD_OPAQUE_NEAREST).size();
			int nearestFNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_FORWARD_OPAQUE_NEAREST, 0, nearestFNum);

			int generalFNum    = pRenderView->GetRenderItems(EFSLIST_FORWARD_OPAQUE).size();
			int generalFNoZPre = pRenderView->FindRenderListSplit(zpPredicate, EFSLIST_FORWARD_OPAQUE, 0, generalFNum);

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_GENERAL | FB_TILED_FORWARD, FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST, 0, nearestFNoZPre);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE, 0, generalFNoZPre);
			m_debugViewPass.EndExecution();

			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_GENERAL | FB_TILED_FORWARD | FB_ZPREPASS);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST, nearestFNoZPre, nearestFNum);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE, generalFNoZPre, generalFNum);
			m_debugViewPass.EndExecution();
		}

		if (0 /* TODO */)
		{
			m_debugViewPass.BeginExecution(m_graphicsPipeline);
			m_debugViewPass.SetupDrawContext(StageID, passID, TTYPE_DEBUG, FB_GENERAL);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_BW);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_AW);
			m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_NEAREST);
			m_debugViewPass.EndExecution();
		}

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	if (bViewOverdraw)
	{
		static CCryNameTSCRC techOverdraw("OverdrawComplexity");

		m_resolvePass.SetCustomViewport(rViewport);

		if (m_resolvePass.IsDirty(pTargetRT->GetID()))
		{
			m_resolvePass.SetTechnique(CShaderMan::s_shPostEffects, techOverdraw, 0);
			m_resolvePass.SetRenderTarget(0, pTargetRT);
			m_resolvePass.SetState(GS_NODEPTHTEST);

			// A buffer the same dimension as the Depth-Target
			m_resolvePass.SetBuffer(2, &m_overdrawCount, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
			m_resolvePass.SetBuffer(3, &m_overdrawDepth, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
			m_resolvePass.SetBuffer(4, &m_overdrawStats, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
			m_resolvePass.SetBuffer(5, &m_overdrawHisto, EDefaultResourceViews::UnorderedAccess, EShaderStage_Pixel);
		}

		static CCryNameR overdrawViewMode("overdrawViewMode");
		static CCryNameR overdrawViewParm("overdrawViewParm");

		m_resolvePass.BeginConstantUpdate();
		m_resolvePass.SetConstant(overdrawViewMode, Vec4(2, std::abs(CRenderer::CV_r_OverdrawComplexity), float(rViewport.width), float(rViewport.height)), eHWSC_Pixel);
		m_resolvePass.SetConstant(overdrawViewParm, Vec4(CRenderer::CV_r_OverdrawComplexityBluePoint, CRenderer::CV_r_OverdrawComplexitySmoothness, CRenderer::CV_r_OverdrawComplexityCompression, 0), eHWSC_Pixel);

		m_resolvePass.Execute();
	}

	SAFE_RELEASE(pTargetDS);
}

void CSceneCustomStage::ExecuteDebugOverlay()
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(0.f, 0.f, 0.f, 0.f);
		cb.CopyToDevice();
	}

	CTexture* pTargetRT = pRenderView->GetRenderOutput()->GetColorTarget();
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, 1);

	m_debugViewPass.ExchangeRenderTarget(0, pTargetRT);
	m_debugViewPass.ExchangeDepthTarget(pTargetDS);

	// NOTE: no more external state changes in here, everything should have been setup
	{
		// Prepare ========================================================================
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		m_debugViewPass.PrepareRenderPassForUse(commandList);

		// Execute ========================================================================
		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();

		m_debugViewPass.BeginExecution(m_graphicsPipeline);
		m_debugViewPass.SetupDrawContext(StageID, ePass_DebugViewDrawModes, TTYPE_DEBUG, FB_DEBUG);
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_BW);
		m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_AW);
		m_debugViewPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	SAFE_RELEASE(pTargetDS);
}

void CSceneCustomStage::ExecuteSelectionHighlight()
{
	FUNCTION_PROFILER_RENDERER();

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = RenderView();

	// first check if we actually have anything worth drawing
	uint32 numItems = pRenderView->GetRenderItems(EFSLIST_HIGHLIGHT).size();
	if (numItems == 0)
		return;

	// update our depth texture here
	CTexture* pTargetRT = m_graphicsPipelineResources.m_pTexSceneSelectionIDs;
	CTexture* pTargetDS = CRendererResources::CreateDepthTarget(pTargetRT->GetWidth(), pTargetRT->GetHeight(), ColorF(Clr_FarPlane_Rev.r, 1, 0, 0), eTF_Unknown);

	CClearSurfacePass::Execute(pTargetDS, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, 1);
	CClearSurfacePass::Execute(pTargetRT, ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	m_selectionIDPass.ExchangeRenderTarget(0, pTargetRT);
	m_selectionIDPass.ExchangeDepthTarget(pTargetDS);

	// NOTE: no more external state changes in here, everything should have been setup
	{
		// Prepare ========================================================================
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		m_selectionIDPass.PrepareRenderPassForUse(commandList);

		// Execute ========================================================================
		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();

		CHighlightPredicate highlightPredicate;
		CSelectionPredicate selectionPredicate;

		uint32 startHighlight = pRenderView->FindRenderListSplit(highlightPredicate, EFSLIST_HIGHLIGHT, 0, numItems);
		uint32 startSelected  = pRenderView->FindRenderListSplit(selectionPredicate, EFSLIST_HIGHLIGHT, 0, startHighlight);

		// First pass, draw selected object IDs
		m_selectionIDPass.BeginExecution(m_graphicsPipeline);
		m_selectionIDPass.SetupDrawContext(StageID, ePass_SelectionIDs, TTYPE_DEBUG, FB_GENERAL);
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
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();

	if (!pRenderView->HasRenderItems(EFSLIST_CUSTOM, FB_CUSTOM_RENDER))
		return;

	auto prevPipelineFlags = m_graphicsPipeline.GetPipelineFlags();

	{
		CClearSurfacePass::Execute(m_graphicsPipelineResources.m_pTexSceneNormalsMap, Clr_Transparent);

		// Prepare ========================================================================
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		m_silhouetteMaskPass.PrepareRenderPassForUse(commandList);

		// Execute ========================================================================
		auto& renderItemDrawer = pRenderView->GetDrawer();
		renderItemDrawer.InitDrawSubmission();

		m_silhouetteMaskPass.BeginExecution(m_graphicsPipeline);
		m_silhouetteMaskPass.SetupDrawContext(StageID, ePass_Silhouette, TTYPE_CUSTOMRENDERPASS, FB_CUSTOM_RENDER);
		m_silhouetteMaskPass.DrawRenderItems(pRenderView, EFSLIST_CUSTOM);
		m_silhouetteMaskPass.EndExecution();

		renderItemDrawer.JobifyDrawSubmission();
		renderItemDrawer.WaitForDrawSubmission();
	}

	m_graphicsPipeline.SetPipelineFlags(prevPipelineFlags);
}

void CSceneCustomStage::ExecuteHelpers()
{
	FUNCTION_PROFILER_RENDERER();

	CRenderView* pRenderView = RenderView();

	// first check if we actually have anything worth drawing
	if (!pRenderView->HasRenderItems(EFSLIST_DEBUG_HELPER, FB_GENERAL))
		return;

	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_Custom, 256> cb(m_pPerPassConstantBuffer);
		cb->CP_Custom_ViewMode = Vec4(0.f, 0.f, 0.f, 0.f);
		cb.CopyToDevice();
	}

	// Prepare ========================================================================
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_debugViewPass.PrepareRenderPassForUse(commandList);

	// Execute ========================================================================
	auto& renderItemDrawer = pRenderView->GetDrawer();
	renderItemDrawer.InitDrawSubmission();

	m_debugViewPass.BeginExecution(m_graphicsPipeline);
	m_debugViewPass.SetupDrawContext(StageID, ePass_DebugViewSolid, TTYPE_DEBUG, FB_GENERAL);
	m_debugViewPass.DrawRenderItems(pRenderView, EFSLIST_DEBUG_HELPER);
	m_debugViewPass.EndExecution();

	renderItemDrawer.JobifyDrawSubmission();
	renderItemDrawer.WaitForDrawSubmission();
}
