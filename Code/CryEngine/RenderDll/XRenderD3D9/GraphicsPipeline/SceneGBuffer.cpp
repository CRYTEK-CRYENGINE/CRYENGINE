// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneGBuffer.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/TypedConstantBuffer.h"
#include "Common/ReverseDepth.h"

#include "Common/RenderView.h"
#include "CompiledRenderObject.h"
#include "Common/SceneRenderPass.h"

CSceneGBufferStage::CSceneGBufferStage()
	: m_perPassResources(nullptr, nullptr)
{}

void CSceneGBufferStage::Init()
{
	m_pPerPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

	bool bSuccess = SetAndBuildPerPassResources(true);
	assert(bSuccess);

	// Create resource layout
	m_pResourceLayout = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_perPassResources);

	// Depth Pre-pass
	m_depthPrepass.SetLabel("ZPREPASS");
	m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
	m_depthPrepass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_depthPrepass.SetRenderTargets(
	  // Depth
	  gcpRendD3D->m_pZTexture,
	  // Color 0
	  NULL
	  );

	CTexture* pSceneSpecular = CTexture::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	pSceneSpecular = CTexture::s_ptexSceneSpecularESRAM;
#endif

	// Opaque Pass
	m_opaquePass.SetLabel("OPAQUE");
	m_opaquePass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_opaquePass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_opaquePass.SetRenderTargets(
	  // Depth
	  gcpRendD3D->m_pZTexture,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap,
	  // Color 1
	  CTexture::s_ptexSceneDiffuse,
	  // Color 2
	  pSceneSpecular
	  );

	// Opaque with Velocity Pass
	m_opaqueVelocityPass.SetLabel("OPAQUE_VELOCITY");
	m_opaqueVelocityPass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_opaqueVelocityPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_opaqueVelocityPass.SetRenderTargets(
	  // Depth
	  gcpRendD3D->m_pZTexture,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap,
	  // Color 1
	  CTexture::s_ptexSceneDiffuse,
	  // Color 2
	  pSceneSpecular,
	  // Color 3
	  CTexture::s_ptexVelocityObjects[0]
	  );

	// Overlay Pass
	m_overlayPass.SetLabel("OVERLAYS");
	m_overlayPass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_overlayPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_overlayPass.SetRenderTargets(
	  // Depth
	  gcpRendD3D->m_pZTexture,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap,
	  // Color 1
	  CTexture::s_ptexSceneDiffuse,
	  // Color 2
	  pSceneSpecular
	  );

	// Micro GBuffer Pass
	m_microGBufferPass.SetLabel("MicroGBuffer");
	m_microGBufferPass.SetupPassContext(m_stageID, ePass_MicroGBufferFill, TTYPE_Z, FB_Z);
	m_microGBufferPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
	m_microGBufferPass.SetRenderTargets(
	  // Depth
	  gcpRendD3D->m_pZTexture,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap
	  );
}

void CSceneGBufferStage::OnResolutionChanged()
{
	CD3D9Renderer* rd = gcpRendD3D;

	int width = rd->m_MainViewport.nWidth;
	int height = rd->m_MainViewport.nHeight;

	if (!CTexture::s_ptexZTarget
	    || CTexture::s_ptexZTarget->GetDevTexture()->IsMSAAChanged()
	    || CTexture::s_ptexZTarget->GetDstFormat() != CTexture::s_eTFZ
	    || CTexture::s_ptexZTarget->GetWidth() != width
	    || CTexture::s_ptexZTarget->GetHeight() != height)
	{
		CTexture::GenerateZMaps();
		CDeferredShading::Instance().CreateDeferredMaps();
	}
}

bool CSceneGBufferStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	outPSO = NULL;

	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (!pRenderer->GetGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
		return true;

	CShader* pShader = static_cast<CShader*>(desc.shaderItem.m_pShader);
	CShaderResources* pRes = static_cast<CShaderResources*>(desc.shaderItem.m_pShaderResources);
	if (pRes->IsTransparent() && !(pShader->m_Flags2 & EF2_HAIR))
		return true;
	
	const uint64 objectFlags = desc.objectFlags;
	CSceneRenderPass* pSceneRenderPass = (passID == ePass_DepthPrepass) ? &m_depthPrepass : &m_opaquePass;

	if (passID != ePass_DepthPrepass)
	{
		if (objectFlags & FOB_HAS_PREVMATRIX)
		{
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_MOTION_BLUR];
			if ((objectFlags & FOB_MOTION_BLUR) && !(objectFlags & FOB_SKINNED))
			{
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_VERTEX_VELOCITY];
			}

			pSceneRenderPass = &m_opaqueVelocityPass;
		}

		/*// Disable alpha testing/depth writes if object renders using a z-prepass and requested technique is not z pre pass
		if (objectFlags & FOB_ZPREPASS)
		{
			psoDesc.m_RenderState &= ~(GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_ALPHATEST_MASK);
			psoDesc.m_RenderState |= GS_DEPTHFUNC_EQUAL;
			psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_ALPHATEST];
		}*/

		// Enable stencil for vis area and dynamic object markup
		psoDesc.m_RenderState |= GS_STENCIL;
		psoDesc.m_StencilState =
		  STENC_FUNC(FSS_STENCFUNC_ALWAYS)
		  | STENCOP_FAIL(FSS_STENCOP_KEEP)
		  | STENCOP_ZFAIL(FSS_STENCOP_KEEP)
		  | STENCOP_PASS(FSS_STENCOP_REPLACE);
		psoDesc.m_StencilReadMask = 0xFF & (~BIT_STENCIL_RESERVED);
		psoDesc.m_StencilWriteMask = 0xFF;

		if ((objectFlags & FOB_DECAL) || (objectFlags & FOB_TERRAIN_LAYER) || (((CShader*)desc.shaderItem.m_pShader)->GetFlags() & EF_DECAL))
		{
			psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
			psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_COLMASK_RGB;

			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];

			if (objectFlags & FOB_DECAL_TEXGEN_2D)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

			pSceneRenderPass = &m_overlayPass;
		}
	}

	if (passID == ePass_MicroGBufferFill)
	{
		pSceneRenderPass = &m_microGBufferPass;
		psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]);
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_MOTION_BLUR];
	}

	if (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}
	psoDesc.m_pRenderPass = pSceneRenderPass->GetRenderPass();

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
	return outPSO != nullptr;
}

bool CSceneGBufferStage::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	DevicePipelineStatesArray& stageStates = pStateArray[m_stageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_Z;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_GBufferFill, stageStates[ePass_GBufferFill]);
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_MicroGBufferFill, stageStates[ePass_MicroGBufferFill]);

	_stateDesc.technique = TTYPE_ZPREPASS;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DepthPrepass, stageStates[ePass_DepthPrepass]);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return bFullyCompiled;
}

bool CSceneGBufferStage::SetAndBuildPerPassResources(bool bOnInit)
{
	CD3D9Renderer* rd = gcpRendD3D;

	CDeviceResourceSetDesc::EDirtyFlags dirtyFlags = CDeviceResourceSetDesc::EDirtyFlags::eNone;

	// samplers
	{
		auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
		for (int i = 0; i < materialSamplers.size(); ++i)
		{
			dirtyFlags |= m_perPassResources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}
		// hardcoded point samplers
		dirtyFlags |= m_perPassResources.SetSampler(8, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetSampler(9, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);
	}

	// textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
			gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_PerlinNoiseMap, CTexture::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_WindGrid, CTexture::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_NormalsFitting, CTexture::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_DissolveNoise, CTexture::s_ptexDissolveNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		dirtyFlags |= m_perPassResources.SetTexture(ePerPassTexture_SceneLinearDepth, CTexture::s_ptexZTarget, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
	}

	// constant buffers
	{
		CConstantBufferPtr pPerViewCB;

		// Handle case when no view is available in the initialization of the gbuffer-stage
		if (bOnInit)
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
		{
			pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();
		}

		dirtyFlags |= m_perPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
		if (bOnInit)
			return true;
	}

	CRY_ASSERT(bOnInit || uint8(dirtyFlags & CDeviceResourceSetDesc::EDirtyFlags::eDirtyBindPoint) == 0);

	m_pPerPassResourceSet->Update(m_perPassResources, dirtyFlags); // Cannot change resource layout after init. It is baked into the shaders
	return m_pPerPassResourceSet->IsValid();
}

void CSceneGBufferStage::Prepare(CRenderView* pRenderView)
{
	OnResolutionChanged();
	SetAndBuildPerPassResources(false);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_depthPrepass.PrepareRenderPassForUse(commandList);
	m_opaquePass.PrepareRenderPassForUse(commandList);
	m_opaqueVelocityPass.PrepareRenderPassForUse(commandList);
	m_microGBufferPass.PrepareRenderPassForUse(commandList);
}

void CSceneGBufferStage::RenderDepthPrepass()
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	
	if (CRenderer::CV_r_DeferredShadingTiled == 4 && !CRenderer::CV_r_GraphicsPipelineMobile)
	{
		PROFILE_LABEL_SCOPE("ZPREPASS");
		m_depthPrepass.BeginExecution();

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS);

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);

		m_depthPrepass.EndExecution();
	}
	else if (CRenderer::CV_r_usezpass == 2)
	{
		PROFILE_LABEL_SCOPE("ZPREPASS");

		m_depthPrepass.BeginExecution();

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS);

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_ZPREPASS);

		m_depthPrepass.EndExecution();
	}
}

void CSceneGBufferStage::RenderSceneOpaque()
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

	int numItems_Nearest = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_NEAREST_OBJECTS).size();
	int velocityEnd_Nearest = pRenderView->FindRenderListSplit(EFSLIST_NEAREST_OBJECTS, FOB_HAS_PREVMATRIX);

	int numItems_General = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_GENERAL).size();
	int velocityEnd_General = pRenderView->FindRenderListSplit(EFSLIST_GENERAL, FOB_HAS_PREVMATRIX);
	int numItems_Skin = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_SKIN).size();
	int velocityEnd_Skin = pRenderView->FindRenderListSplit(EFSLIST_SKIN, FOB_HAS_PREVMATRIX);

	// Opaque
	m_opaquePass.BeginExecution();
	if (CRenderer::CV_r_nodrawnear == 0)
	{
		m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, velocityEnd_Nearest, numItems_Nearest);
	}
	m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, velocityEnd_General, numItems_General);
	m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_SKIN, velocityEnd_Skin, numItems_Skin);
	m_opaquePass.EndExecution();

	// OpaqueVelocity
	m_opaqueVelocityPass.BeginExecution();
	if (CRenderer::CV_r_nodrawnear == 0)
	{
		m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, 0, velocityEnd_Nearest);
	}
	m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, 0, velocityEnd_General);
	m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_SKIN, 0, velocityEnd_Skin);
	m_opaqueVelocityPass.EndExecution();
}

void CSceneGBufferStage::RenderSceneOverlays()
{
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_overlayPass.PrepareRenderPassForUse(commandList);
	m_overlayPass.BeginExecution();
	m_overlayPass.DrawRenderItems(RenderView(), EFSLIST_TERRAINLAYER);
	m_overlayPass.DrawRenderItems(RenderView(), EFSLIST_DECAL);
	m_overlayPass.EndExecution();
}

void CSceneGBufferStage::ExecuteLinearizeDepth()
{
	PROFILE_LABEL_SCOPE("LINEARIZE_DEPTH");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CShader* pShader = CShaderMan::s_shPostEffects;

	if (m_passDepthLinearization.InputChanged())
	{
		static CCryNameTSCRC techLinearizeDepth("LinearizeDepth");

		m_passDepthLinearization.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_passDepthLinearization.SetTechnique(pShader, techLinearizeDepth, 0);
		m_passDepthLinearization.SetRenderTarget(0, CTexture::s_ptexZTarget);
		m_passDepthLinearization.SetState(GS_NODEPTHTEST);
		m_passDepthLinearization.SetRequirePerViewConstantBuffer(true);
		m_passDepthLinearization.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);

		m_passDepthLinearization.SetTexture(0, gcpRendD3D->m_DepthBufferOrig.pTexture);
	}

	static CCryNameR paramName("NearProjection");

	m_passDepthLinearization.BeginConstantUpdate();

	float zn = DRAW_NEAREST_MIN;
	float zf = CRenderer::CV_r_DrawNearFarPlane;
	float nearZRange = CRenderer::CV_r_DrawNearZRange;
	float camScale = (zf / gEnv->p3DEngine->GetMaxViewDistance());

	const bool bReverseDepth = (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	Vec4 nearProjParams;
	nearProjParams.x = bReverseDepth ? 1.0f - zf / (zf - zn) * nearZRange : zf / (zf - zn) * nearZRange;
	nearProjParams.y = bReverseDepth ? zn / (zf - zn) * nearZRange * camScale : zn / (zn - zf) * nearZRange * camScale;
	nearProjParams.z = bReverseDepth ? 1.0f - (nearZRange - 0.001f) : nearZRange - 0.001f;
	nearProjParams.w = 1.0f - nearZRange;
	m_passDepthLinearization.SetConstant(paramName, nearProjParams, eHWSC_Pixel);

	m_passDepthLinearization.Execute();
}

void CSceneGBufferStage::ExecuteGBufferVisualization()
{
	// GBuffer Debug Visualization
	if (CRenderer::CV_r_DeferredShadingDebugGBuffer)
	{
		PROFILE_LABEL_SCOPE("BUFFER_VISUALIZATION");
		static CCryNameTSCRC tech("DebugGBuffer");
		m_passBufferVisualization.SetTechnique(CShaderMan::s_shDeferredShading, tech, 0);
		m_passBufferVisualization.SetRenderTarget(0, CTexture::s_ptexStereoR);
		m_passBufferVisualization.SetState(GS_NODEPTHTEST);

		m_passBufferVisualization.SetTextureSamplerPair(0, CTexture::s_ptexZTarget, EDefaultSamplerStates::PointClamp);
		m_passBufferVisualization.SetTextureSamplerPair(1, CTexture::s_ptexSceneNormalsMap, EDefaultSamplerStates::PointClamp);
		m_passBufferVisualization.SetTextureSamplerPair(2, CTexture::s_ptexSceneDiffuse, EDefaultSamplerStates::PointClamp);
		m_passBufferVisualization.SetTextureSamplerPair(3, CTexture::s_ptexSceneSpecular, EDefaultSamplerStates::PointClamp);

		m_passBufferVisualization.BeginConstantUpdate();
		static CCryNameR paramName("DebugViewMode");
		m_passBufferVisualization.SetConstant(paramName, Vec4((float)CRenderer::CV_r_DeferredShadingDebugGBuffer, 0, 0, 0), eHWSC_Pixel);
			
		m_passBufferVisualization.Execute();
	}
}

void CSceneGBufferStage::ExecuteMicroGBuffer()
{
	PROFILE_LABEL_SCOPE("GBUFFER");

	D3DViewPort viewport = { 0.f, 0.f, float(gcpRendD3D->m_MainViewport.nWidth), float(gcpRendD3D->m_MainViewport.nHeight), 0.0f, 1.0f };
	m_microGBufferPass.SetViewport(viewport);

	const bool bReverseDepth = (gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	commandList.GetGraphicsInterface()->ClearSurface(gcpRendD3D->m_DepthBufferOrig.pSurface, CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 0.0f : 1.0f, 1);

	m_microGBufferPass.SetFlags(bReverseDepth ? CSceneRenderPass::ePassFlags_ReverseDepth : CSceneRenderPass::ePassFlags_None);
	
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	Prepare(pRenderView);
	
	pRenderView->GetDrawer().InitDrawSubmission();

	m_microGBufferPass.BeginExecution();
	m_microGBufferPass.DrawRenderItems(RenderView(), EFSLIST_GENERAL);
	m_microGBufferPass.DrawRenderItems(RenderView(), EFSLIST_TERRAINLAYER);
	m_microGBufferPass.DrawRenderItems(RenderView(), EFSLIST_DECAL);
	m_microGBufferPass.EndExecution();

	pRenderView->GetDrawer().JobifyDrawSubmission();
}

void CSceneGBufferStage::Execute()
{
	PROFILE_LABEL_SCOPE("GBUFFER");

	CD3D9Renderer* rd = gcpRendD3D;
	SThreadInfo* const pThreadInfo = &(rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID]);

	D3DViewPort viewport = { 0.f, 0.f, float(rd->m_MainViewport.nWidth), float(rd->m_MainViewport.nHeight), 0.0f, 1.0f };
	rd->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));
	m_depthPrepass.SetViewport(viewport);
	m_opaquePass.SetViewport(viewport);
	m_opaqueVelocityPass.SetViewport(viewport);
	m_overlayPass.SetViewport(viewport);

	{
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	// Clear depth (stencil initialized to 1 - 0 is reserved for MSAAed samples)
	const bool bReverseDepth = (rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	if (CVrProjectionManager::Instance()->GetProjectionType() == CVrProjectionManager::eVrProjection_LensMatched)
	{ 
		// use inverse depth here
			rd->FX_ClearTarget(&rd->m_DepthBufferOrig, CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 1.0f : 0.0f, 1);
			CVrProjectionManager::Instance()->ExecuteLensMatchedOctagon(gcpRendD3D->m_pZTexture);
	}
	else
	{
			commandList.GetGraphicsInterface()->ClearSurface(rd->m_DepthBufferOrig.pSurface, CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 0.0f : 1.0f, 1);
	}

	// Clear velocity target
	commandList.GetGraphicsInterface()->ClearSurface(GetUtils().GetVelocityObjectRT()->GetSurface(0, 0), Clr_Transparent);

	bool bClearAll = CRenderer::CV_r_wireframe != 0;

	if (CVrProjectionManager::IsMultiResEnabledStatic())
		bClearAll = true;

	if (bClearAll)
	{
		RECT rect = { 0, 0, int(viewport.Width), int(viewport.Height) };
		commandList.GetGraphicsInterface()->ClearSurface(CTexture::s_ptexSceneNormalsMap->GetSurface(0, 0), Clr_Transparent);
		commandList.GetGraphicsInterface()->ClearSurface(CTexture::s_ptexSceneDiffuse->GetSurface(0, 0), Clr_Empty);
		commandList.GetGraphicsInterface()->ClearSurface(CTexture::s_ptexSceneSpecular->GetSurface(0, 0), Clr_Empty);
	}
	}

	// Update pass viewport and flags
	CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_VrProjectionPass;

	if (pThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
		passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;

	m_depthPrepass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaquePass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaqueVelocityPass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_overlayPass.SetFlags(passFlags);

	// Stereo has separate velocity targets for left and right eye
	m_opaqueVelocityPass.ExchangeRenderTarget(3, GetUtils().GetVelocityObjectRT());

	// NOTE: no more external state changes in here, everything should have been setup
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	{
		Prepare(pRenderView);

		pRenderView->GetDrawer().InitDrawSubmission();

		RenderDepthPrepass();

		if (CRenderer::CV_r_DeferredShadingTiled < 4)
		{
			RenderSceneOpaque();
		}

		if (rd->m_nGraphicsPipeline >= 2)
		{
			RenderView()->GetDrawer().JobifyDrawSubmission();

			RenderView()->GetDrawer().WaitForDrawSubmission(); // wait until we can do linearization
		}

		// linearize depth here so that it can be used in overlay passes for e.g for soft depth test
		ExecuteLinearizeDepth();

		RenderView()->GetDrawer().InitDrawSubmission();

		if (CRenderer::CV_r_DeferredShadingTiled < 4)
		{
			RenderSceneOverlays();
		}

		if (rd->m_nGraphicsPipeline >= 2)
		{
			pRenderView->GetDrawer().JobifyDrawSubmission();
		}
	}
}
