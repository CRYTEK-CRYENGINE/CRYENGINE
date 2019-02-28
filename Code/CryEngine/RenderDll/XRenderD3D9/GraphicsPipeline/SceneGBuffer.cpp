// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	: m_perPassResources()
{}

void CSceneGBufferStage::Init()
{
	m_pPerPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

	bool bSuccess = SetAndBuildPerPassResources(true);
	assert(bSuccess);

	// Create resource layout
	m_pResourceLayout = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_perPassResources);

	// Freeze resource-set layout (assert will fire when violating the constraint)
	m_perPassResources.AcceptChangedBindPoints();

	// Depth Pre-pass
	m_depthPrepass.SetLabel("ZPREPASS");
	m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
	m_depthPrepass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);

	CTexture* pSceneSpecular = CRendererResources::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
	pSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

	// Opaque Pass
	m_opaquePass.SetLabel("OPAQUE");
	m_opaquePass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_opaquePass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);

	// Opaque with Velocity Pass
	m_opaqueVelocityPass.SetLabel("OPAQUE_VELOCITY");
	m_opaqueVelocityPass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_opaqueVelocityPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);

	// Overlay Pass
	m_overlayPass.SetLabel("OVERLAYS");
	m_overlayPass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_overlayPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);

	// Micro GBuffer Pass
	m_microGBufferPass.SetLabel("MicroGBuffer");
	m_microGBufferPass.SetupPassContext(m_stageID, ePass_MicroGBufferFill, TTYPE_Z, FB_Z);
	m_microGBufferPass.SetPassResources(m_pResourceLayout, m_pPerPassResourceSet);
}

bool CSceneGBufferStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	outPSO = NULL;

	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout, desc);
	if (!GetStdGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
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
			psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NOCOLMASK_GBUFFER_OVERLAY;

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

	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}

	psoDesc.m_pRenderPass = pSceneRenderPass->GetRenderPass();

	if (!psoDesc.m_pRenderPass)
		return false;

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

	// samplers
	{
		auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
		for (int i = 0; i < materialSamplers.size(); ++i)
		{
			m_perPassResources.SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}
		// hard-coded point samplers
		m_perPassResources.SetSampler(8, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetSampler(9, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);
	}

	// textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
			gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		m_perPassResources.SetTexture(ePerPassTexture_PerlinNoiseMap, CRendererResources::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_WindGrid, CRendererResources::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_NormalsFitting, CRendererResources::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_DissolveNoise, CRendererResources::s_ptexDissolveNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		m_perPassResources.SetTexture(ePerPassTexture_SceneLinearDepth, CRendererResources::s_ptexLinearDepth, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
	}

	// constant buffers
	{
		CConstantBufferPtr pPerViewCB;

		// Handle case when no view is available in the initialization of the g-buffer-stage
		if (bOnInit)
			pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
		else
			pPerViewCB = rd->GetGraphicsPipeline().GetMainViewConstantBuffer();

		m_perPassResources.SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
	}

	if (bOnInit)
		return true;

	CRY_ASSERT(!m_perPassResources.HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
	return m_pPerPassResourceSet->Update(m_perPassResources);
}

void CSceneGBufferStage::Update()
{
	CRenderView* pRenderView = RenderView();

	CStandardGraphicsPipeline* p = static_cast<CStandardGraphicsPipeline*>(&GetGraphicsPipeline());
	EShaderRenderingFlags flags = (EShaderRenderingFlags)p->GetRenderFlags();
	const bool isForwardMinimal = (flags & SHDF_FORWARD_MINIMAL) != 0;

	SetAndBuildPerPassResources(false);

	{
		CTexture* pZTexture = pRenderView->GetDepthTarget();

		// Depth pre-pass
		m_depthPrepass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			NULL
		);
	}

	if (!isForwardMinimal)
	{
		CTexture* pZTexture = pRenderView->GetDepthTarget();
		CTexture* pSceneSpecular = CRendererResources::s_ptexSceneSpecular;
#if defined(DURANGO_USE_ESRAM)
		pSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

		// Opaque Pass
		m_opaquePass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			CRendererResources::s_ptexSceneNormalsMap,
			// Color 1
			CRendererResources::s_ptexSceneDiffuse,
			// Color 2
			pSceneSpecular
		);

		// Opaque with Velocity Pass
		m_opaqueVelocityPass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			CRendererResources::s_ptexSceneNormalsMap,
			// Color 1
			CRendererResources::s_ptexSceneDiffuse,
			// Color 2
			pSceneSpecular,
			// Color 3
			CRendererResources::s_ptexVelocityObjects[0]
		);

		// Overlay Pass
		m_overlayPass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			CRendererResources::s_ptexSceneNormalsMap,
			// Color 1
			CRendererResources::s_ptexSceneDiffuse,
			// Color 2
			pSceneSpecular
		);

		// Micro GBuffer Pass
		m_microGBufferPass.SetRenderTargets(
			// Depth
			pZTexture,
			// Color 0
			CRendererResources::s_ptexSceneNormalsMap
		);
	}
}

void CSceneGBufferStage::Prepare(bool bPostLinearize)
{
	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	if (CRenderer::CV_r_GraphicsPipelineMobile == 1)
	{
		m_microGBufferPass.PrepareRenderPassForUse(commandList);
	}
	else if (!bPostLinearize)
	{
		m_depthPrepass.PrepareRenderPassForUse(commandList);
		m_opaquePass.PrepareRenderPassForUse(commandList);
		m_opaqueVelocityPass.PrepareRenderPassForUse(commandList);
	}
	else
	{
		m_overlayPass.PrepareRenderPassForUse(commandList);
	}
}

void CSceneGBufferStage::ExecuteDepthPrepass()
{
	CRenderView* pRenderView = RenderView();
	
	if (CRenderer::CV_r_DeferredShadingTiled == 4 && !CRenderer::CV_r_GraphicsPipelineMobile)
	{
		m_depthPrepass.BeginExecution();

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS);

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);

		m_depthPrepass.EndExecution();
	}
	else if (CRenderer::CV_r_usezpass == 2)
	{
		m_depthPrepass.BeginExecution();

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS);

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_ZPREPASS);

		m_depthPrepass.EndExecution();
	}
}

void CSceneGBufferStage::ExecuteSceneOpaque()
{
	CRenderView* pRenderView = RenderView();

	int numItems_Nearest = pRenderView->GetRenderItems(EFSLIST_NEAREST_OBJECTS).size();
	int velocityEnd_Nearest = pRenderView->FindRenderListSplit(EFSLIST_NEAREST_OBJECTS, FOB_HAS_PREVMATRIX);

	int numItems_General = pRenderView->GetRenderItems(EFSLIST_GENERAL).size();
	int velocityEnd_General = pRenderView->FindRenderListSplit(EFSLIST_GENERAL, FOB_HAS_PREVMATRIX);
	int numItems_Skin = pRenderView->GetRenderItems(EFSLIST_SKIN).size();
	int velocityEnd_Skin = pRenderView->FindRenderListSplit(EFSLIST_SKIN, FOB_HAS_PREVMATRIX);

	{
		// Opaque
		m_opaquePass.BeginExecution();
		if (CRenderer::CV_r_nodrawnear == 0)
		{
			m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, velocityEnd_Nearest, numItems_Nearest);
		}
		m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, velocityEnd_General, numItems_General);
		m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_SKIN, velocityEnd_Skin, numItems_Skin);
		m_opaquePass.EndExecution();
	}

	{
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
}

void CSceneGBufferStage::ExecuteSceneOverlays()
{
	CRenderView* pRenderView = RenderView();

	{
		m_overlayPass.BeginExecution();
		m_overlayPass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		m_overlayPass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_overlayPass.EndExecution();
	}
}

void CSceneGBufferStage::ExecuteLinearizeDepth()
{
	PROFILE_LABEL_SCOPE("LINEARIZE_DEPTH");

	if (m_passDepthLinearization.IsDirty(RenderView()->GetDepthTarget()->GetTextureID()))
	{
		static CCryNameTSCRC techLinearizeDepth("LinearizeDepth");

		m_passDepthLinearization.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_passDepthLinearization.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_passDepthLinearization.SetTechnique(CShaderMan::s_shPostEffects, techLinearizeDepth, 0);
		m_passDepthLinearization.SetRenderTarget(0, CRendererResources::s_ptexLinearDepth);
		m_passDepthLinearization.SetState(GS_NODEPTHTEST);
		m_passDepthLinearization.SetRequirePerViewConstantBuffer(true);
		m_passDepthLinearization.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);

		m_passDepthLinearization.SetTexture(0, RenderView()->GetDepthTarget());
	}

	static CCryNameR paramName("NearProjection");

	m_passDepthLinearization.BeginConstantUpdate();

	float zn = DRAW_NEAREST_MIN;
	float zf = CRenderer::CV_r_DrawNearFarPlane;
	float nearZRange = CRenderer::CV_r_DrawNearZRange;
	float camScale = (zf / gEnv->p3DEngine->GetMaxViewDistance());

	const bool bReverseDepth = true;

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
		m_passBufferVisualization.SetRenderTarget(0, CRendererResources::s_ptexSceneDiffuseTmp);
		m_passBufferVisualization.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_passBufferVisualization.SetState(GS_NODEPTHTEST);

		m_passBufferVisualization.SetTexture(0, CRendererResources::s_ptexLinearDepth);
		m_passBufferVisualization.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
		m_passBufferVisualization.SetTexture(2, CRendererResources::s_ptexSceneDiffuse);
		m_passBufferVisualization.SetTexture(3, CRendererResources::s_ptexSceneSpecular);
		m_passBufferVisualization.SetSampler(0, EDefaultSamplerStates::PointClamp);

		m_passBufferVisualization.BeginConstantUpdate();
		static CCryNameR paramName("DebugViewMode");
		m_passBufferVisualization.SetConstant(paramName, Vec4((float)CRenderer::CV_r_DeferredShadingDebugGBuffer, 0, 0, 0), eHWSC_Pixel);
			
		m_passBufferVisualization.Execute();
	}
}

void CSceneGBufferStage::ExecuteMicroGBuffer()
{
	CRenderView* pRenderView = RenderView();
	auto& rendItemDrawer = pRenderView->GetDrawer();

	m_microGBufferPass.SetViewport(GetViewport());

	CClearSurfacePass::Execute(RenderView()->GetDepthTarget(), CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane_Rev.r, STENCIL_VALUE_OUTDOORS);

	m_microGBufferPass.SetFlags(CSceneRenderPass::ePassFlags_ReverseDepth);

	Prepare(false);
	
	rendItemDrawer.InitDrawSubmission();

	m_microGBufferPass.BeginExecution();
	m_microGBufferPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
	m_microGBufferPass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
	m_microGBufferPass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
	m_microGBufferPass.EndExecution();

	rendItemDrawer.JobifyDrawSubmission();
}

void CSceneGBufferStage::Execute()
{
	PROFILE_LABEL_SCOPE("GBUFFER");

	// NOTE: no more external state changes in here, everything should have been setup
	auto& rViewport = GetViewport();
	CRenderView* pRenderView = RenderView();
	auto& rendItemDrawer = pRenderView->GetDrawer();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	m_depthPrepass.SetViewport(rViewport);
	m_opaquePass.SetViewport(rViewport);
	m_opaqueVelocityPass.SetViewport(rViewport);
	m_overlayPass.SetViewport(rViewport);

	{
		// Clear depth (stencil initialized to STENCIL_VALUE_OUTDOORS)
		if (CVrProjectionManager::Instance()->GetProjectionType() == CVrProjectionManager::eVrProjection_LensMatched)
		{
			// use inverse depth here
			CClearSurfacePass::Execute(pZTexture, CLEAR_ZBUFFER | CLEAR_STENCIL, 1.0f - Clr_FarPlane_Rev.r, STENCIL_VALUE_OUTDOORS);
			CVrProjectionManager::Instance()->ExecuteLensMatchedOctagon(pRenderView->GetDepthTarget());
		}
		else
		{
			CClearSurfacePass::Execute(pZTexture, CLEAR_ZBUFFER | CLEAR_STENCIL, 0.0f + Clr_FarPlane_Rev.r, STENCIL_VALUE_OUTDOORS);
		}

		// Clear velocity target
		CClearSurfacePass::Execute(GetUtils().GetVelocityObjectRT(pRenderView), Clr_Transparent);

		bool bClearAll = CRenderer::CV_r_wireframe != 0;

		if (CVrProjectionManager::IsMultiResEnabledStatic())
			bClearAll = true;

		if (bClearAll)
		{
			CClearSurfacePass::Execute(CRendererResources::s_ptexSceneNormalsMap, Clr_Transparent);
			CClearSurfacePass::Execute(CRendererResources::s_ptexSceneDiffuse, Clr_Empty);
			CClearSurfacePass::Execute(CRendererResources::s_ptexSceneSpecular, Clr_Empty);
		}
	}

	// Update pass viewport and flags
	CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_VrProjectionPass;

	passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;

	m_depthPrepass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaquePass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaqueVelocityPass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_overlayPass.SetFlags(passFlags);

	// Stereo has separate velocity targets for left and right eye
	m_opaqueVelocityPass.ExchangeRenderTarget(3, GetUtils().GetVelocityObjectRT(pRenderView));

	Prepare(false);

	{
		rendItemDrawer.InitDrawSubmission();

		ExecuteDepthPrepass();
		if (CRenderer::CV_r_DeferredShadingTiled < 4)
			ExecuteSceneOpaque();

		rendItemDrawer.JobifyDrawSubmission();
		rendItemDrawer.WaitForDrawSubmission(); // wait until we can do linearization
	}

	// linearize depth here so that it can be used in overlay passes for e.g for soft depth test
	ExecuteLinearizeDepth();

	Prepare(true); // Necessary because of DepthRead and state-change to PIXELSHADER

	{
		rendItemDrawer.InitDrawSubmission();

		if (CRenderer::CV_r_DeferredShadingTiled < 4)
			ExecuteSceneOverlays();

		rendItemDrawer.JobifyDrawSubmission();
	}
}

void CSceneGBufferStage::ExecuteMinimumZpass()
{
	PROFILE_LABEL_SCOPE("MINIMUM_ZPASS");

	// NOTE: no more external state changes in here, everything should have been setup
	auto& rViewport = GetViewport();
	CRenderView* pRenderView = RenderView();
	auto& rendItemDrawer = pRenderView->GetDrawer();

	m_depthPrepass.SetViewport(rViewport);

	// Update pass viewport and flags
	CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_VrProjectionPass;
	passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;
	m_depthPrepass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	m_depthPrepass.PrepareRenderPassForUse(commandList);
	{
		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

		m_depthPrepass.PrepareRenderPassForUse(commandList);
	}

	{
		rendItemDrawer.InitDrawSubmission();

		m_depthPrepass.BeginExecution();

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST);

		m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS | FB_GENERAL);
		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE);

		m_depthPrepass.EndExecution();

		rendItemDrawer.JobifyDrawSubmission();
		rendItemDrawer.WaitForDrawSubmission();
	}
}
