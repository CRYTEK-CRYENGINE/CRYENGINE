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

void CSceneGBufferStage::Init()
{
	m_pPerPassResources = CCryDeviceWrapper::GetObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

	bool bSuccess0 = PreparePerPassResources();
	bool bSuccess1 = PrepareResourceLayout();
	assert(bSuccess0 && bSuccess1);

	// Depth Pre-pass
	m_depthPrepass.SetLabel("ZPREPASS");
	m_depthPrepass.SetupPassContext(m_stageID, ePass_DepthPrepass, TTYPE_ZPREPASS, FB_ZPREPASS);
	m_depthPrepass.SetPassResources(m_pResourceLayout, m_pPerPassResources);
	m_depthPrepass.SetRenderTargets(
	  // Depth
	  &gcpRendD3D->m_DepthBufferOrigMSAA,
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
	m_opaquePass.SetPassResources(m_pResourceLayout, m_pPerPassResources);
	m_opaquePass.SetRenderTargets(
	  // Depth
	  &gcpRendD3D->m_DepthBufferOrigMSAA,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap,
	  // Color 1
	  CTexture::s_ptexSceneDiffuse,
	  // Color 2
	  pSceneSpecular
	  );

	// Opaque with Velocity Pass
	m_opaqueVelocityPass.SetLabel("OPAQUE");
	m_opaqueVelocityPass.SetupPassContext(m_stageID, ePass_GBufferFill, TTYPE_Z, FB_Z);
	m_opaqueVelocityPass.SetPassResources(m_pResourceLayout, m_pPerPassResources);
	m_opaqueVelocityPass.SetRenderTargets(
	  // Depth
	  &gcpRendD3D->m_DepthBufferOrigMSAA,
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
	m_overlayPass.SetPassResources(m_pResourceLayout, m_pPerPassResources);
	m_overlayPass.SetRenderTargets(
	  // Depth
	  &gcpRendD3D->m_DepthBufferOrigMSAA,
	  // Color 0
	  CTexture::s_ptexSceneNormalsMap,
	  // Color 1
	  CTexture::s_ptexSceneDiffuse,
	  // Color 2
	  pSceneSpecular
	  );
}

void CSceneGBufferStage::ReleaseBuffers()
{
	m_passDepthLinearization.SetTexture(16, nullptr);  // TODO: Change slot
}

void CSceneGBufferStage::OnResolutionChanged()
{
	CD3D9Renderer* rd = gcpRendD3D;

	int width = rd->m_MainViewport.nWidth;
	int height = rd->m_MainViewport.nHeight;

	if (!CTexture::s_ptexZTarget
	    || CTexture::s_ptexZTarget->IsMSAAChanged()
	    || CTexture::s_ptexZTarget->GetDstFormat() != CTexture::s_eTFZ
	    || CTexture::s_ptexZTarget->GetWidth() != width
	    || CTexture::s_ptexZTarget->GetHeight() != height)
	{
		rd->FX_Commit(); // Flush to unset the Z target before regenerating
		CTexture::GenerateZMaps();
		CDeferredShading::Instance().CreateDeferredMaps();
	}
}

bool CSceneGBufferStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	CD3D9Renderer* rd = gcpRendD3D;

	// TODO: Handle MSAA flags if required
	// TODO: Handle Hair flags!

	outPSO = NULL;

	CShader* pShader = static_cast<CShader*>(desc.shaderItem.m_pShader);
	SShaderTechnique* pTechnique = pShader->GetTechnique(desc.shaderItem.m_nTechnique, desc.technique, true);
	if (!pTechnique)
		return true;

	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout.get(), desc);
	CShaderResources* pRes = static_cast<CShaderResources*>(desc.shaderItem.m_pShaderResources);
	const uint64 objectFlags = desc.objectFlags;
	SShaderPass* pShaderPass = &pTechnique->m_Passes[0];

	// Handle quality flags
	CStandardGraphicsPipeline::ApplyShaderQuality(psoDesc, gcpRendD3D->GetShaderProfile(pShader->m_eShaderType));

	SThreadInfo* const pShaderThreadInfo = &(gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID]);
	if (pShaderThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];

	// Set resource states
	bool bTwoSided = false;
	{
		if (pRes->m_ResFlags & MTL_FLAG_2SIDED)
		{
			bTwoSided = true;
		}

		if (pRes->IsAlphaTested())
		{
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];
		}

		if (pRes->m_Textures[EFTT_DIFFUSE] && pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_pTexModifier)
		{
			psoDesc.m_ShaderFlags_MD |= pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_nUpdateFlags;
		}

		if (pRes->m_pDeformInfo)
			psoDesc.m_ShaderFlags_MDV |= pRes->m_pDeformInfo->m_eType;
	}

	psoDesc.m_ShaderFlags_MDV |= psoDesc.m_pShader->m_nMDV;

	if (objectFlags & FOB_OWNER_GEOMETRY)
		psoDesc.m_ShaderFlags_MDV &= ~MDV_DEPTH_OFFSET;

	if (objectFlags & FOB_BENDED)
		psoDesc.m_ShaderFlags_MDV |= MDV_BENDING;

	if (!(objectFlags & FOB_TRANS_MASK))
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];

	if (objectFlags & FOB_BLEND_WITH_TERRAIN_COLOR)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];

	psoDesc.m_bAllowTesselation = false;
	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];

	if (objectFlags & FOB_NEAREST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NEAREST];

	if (objectFlags & FOB_DISSOLVE)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_DISSOLVE];

	if (psoDesc.m_RenderState & GS_ALPHATEST_MASK)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

#ifdef TESSELLATION_RENDERER
	const bool bHasTesselationShaders = pShaderPass && pShaderPass->m_HShader && pShaderPass->m_DShader;
	if (bHasTesselationShaders && (!(objectFlags & FOB_NEAREST) && (objectFlags & FOB_ALLOW_TESSELLATION)))
	{
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
		psoDesc.m_bAllowTesselation = true;
	}
#endif

	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_PER_INSTANCE_CB_TEMP];

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

	if (gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
	{
		psoDesc.m_RenderState = ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
	}

	psoDesc.m_CullMode = bTwoSided ? eCULL_None : (pShaderPass->m_eCull != -1 ? (ECull)pShaderPass->m_eCull : eCULL_Back);
	psoDesc.m_PrimitiveType = ERenderPrimitiveType(desc.primitiveType);

	if (psoDesc.m_bAllowTesselation)
	{
		psoDesc.m_PrimitiveType = ept3ControlPointPatchList;
		psoDesc.m_ObjectStreamMask |= VSM_NORMALS;
	}

	pSceneRenderPass->ExtractRenderTargetFormats(psoDesc);

	outPSO = CCryDeviceWrapper::GetObjectFactory().CreateGraphicsPSO(psoDesc);
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

	_stateDesc.technique = TTYPE_ZPREPASS;
	bFullyCompiled &= CreatePipelineState(_stateDesc, ePass_DepthPrepass, stageStates[ePass_DepthPrepass]);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return bFullyCompiled;
}

bool CSceneGBufferStage::PreparePerPassResources()
{
	CD3D9Renderer* rd = gcpRendD3D;

	m_pPerPassResources->Clear();

	// samplers
	{
		auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
		for (int i = 0; i < materialSamplers.size(); ++i)
		{
			m_pPerPassResources->SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
		}
		// hardcoded point samplers
		m_pPerPassResources->SetSampler(8, gcpRendD3D->m_nPointWrapSampler, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetSampler(9, gcpRendD3D->m_nPointClampSampler, EShaderStage_AllWithoutCompute);
	}

	// textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
			gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		m_pPerPassResources->SetTexture(ePerPassTexture_WindGrid, CTexture::s_ptexWindGrid, SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), SResourceView::DefaultViewSRGB, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetTexture(ePerPassTexture_NormalsFitting, CTexture::s_ptexNormalsFitting, SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
		m_pPerPassResources->SetTexture(ePerPassTexture_DissolveNoise, CTexture::s_ptexDissolveNoiseMap, SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
	}

	// constant buffers
	{
		rd->GetGraphicsPipeline().UpdatePerViewConstantBuffer();
		CConstantBufferPtr pPerViewCB = rd->GetGraphicsPipeline().GetPerViewConstantBuffer();
		m_pPerPassResources->SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
		if (!pPerViewCB)
		{
			return true; // During startup, this may be null
		}
	}

	m_pPerPassResources->Build();
	return m_pPerPassResources->IsValid();
}

bool CSceneGBufferStage::PrepareResourceLayout() // threadsafe
{
	SDeviceResourceLayoutDesc layoutDesc;

	layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerInstanceCB, eConstantBufferShaderSlot_PerInstance, EShaderStage_Vertex | EShaderStage_Pixel);
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS, gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialResources());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerInstanceExtraRS, gcpRendD3D->GetGraphicsPipeline().GetDefaultInstanceExtraResources());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS, m_pPerPassResources);

	m_pResourceLayout = CCryDeviceWrapper::GetObjectFactory().CreateResourceLayout(layoutDesc);
	return m_pResourceLayout != nullptr;
}

void CSceneGBufferStage::Prepare(CRenderView* pRenderView)
{
	OnResolutionChanged();
	PreparePerPassResources();
}

void CSceneGBufferStage::RenderDepthPrepass()
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
	if (CRenderer::CV_r_usezpass == 2)
	{
		PROFILE_LABEL_SCOPE("ZPREPASS");

		m_depthPrepass.DrawRenderItems(pRenderView, EFSLIST_ZPREPASS);
	}
}

void CSceneGBufferStage::RenderSceneOpaque()
{
	PROFILE_LABEL_SCOPE("OPAQUE");

	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

	int numItems_Nearest = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_NEAREST_OBJECTS).size();
	int velocityEnd_Nearest = pRenderView->FindRenderListSplit(EFSLIST_NEAREST_OBJECTS, FOB_HAS_PREVMATRIX);

	int numItems_General = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_GENERAL).size();
	int velocityEnd_General = pRenderView->FindRenderListSplit(EFSLIST_GENERAL, FOB_HAS_PREVMATRIX);
	int numItems_Skin = gcpRendD3D->m_RP.m_pCurrentRenderView->GetRenderItems(EFSLIST_SKIN).size();
	int velocityEnd_Skin = pRenderView->FindRenderListSplit(EFSLIST_SKIN, FOB_HAS_PREVMATRIX);

	if (CRenderer::CV_r_nodrawnear == 0)
	{
		m_opaquePass.EnableNearestViewport(true);
		m_opaqueVelocityPass.EnableNearestViewport(true);

		m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, velocityEnd_Nearest, numItems_Nearest);
		m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_NEAREST_OBJECTS, 0, velocityEnd_Nearest);

		m_opaquePass.EnableNearestViewport(false);
		m_opaqueVelocityPass.EnableNearestViewport(false);
	}

	m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, velocityEnd_General, numItems_General);
	m_opaquePass.DrawRenderItems(pRenderView, EFSLIST_SKIN, velocityEnd_Skin, numItems_Skin);
	m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_GENERAL, 0, velocityEnd_General);
	m_opaqueVelocityPass.DrawRenderItems(pRenderView, EFSLIST_SKIN, 0, velocityEnd_Skin);
}

void CSceneGBufferStage::RenderSceneOverlays()
{
	{
		PROFILE_LABEL_SCOPE("TERRAIN_LAYERS");
		m_overlayPass.DrawRenderItems(RenderView(), EFSLIST_TERRAINLAYER);
	}

	{
		PROFILE_LABEL_SCOPE("DECALS");
		m_overlayPass.DrawRenderItems(RenderView(), EFSLIST_DECAL);
	}
}

void CSceneGBufferStage::ExecuteLinearizeDepth()
{
	PROFILE_LABEL_SCOPE("LINEARIZE_DEPTH");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	CShader* pShader = CShaderMan::s_shPostEffects;

	if (m_passDepthLinearization.InputChanged())
	{
		static CCryNameTSCRC techLinearizeDepth("LinearizeDepth");

		m_passDepthLinearization.SetTechnique(pShader, techLinearizeDepth, 0);
		m_passDepthLinearization.SetRenderTarget(0, CTexture::s_ptexZTarget);
		m_passDepthLinearization.SetState(GS_NODEPTHTEST);
		m_passDepthLinearization.SetRequirePerViewConstantBuffer(true);

		m_passDepthLinearization.SetTexture(16, gcpRendD3D->m_DepthBufferOrigMSAA.pTexture);  // TODO: Change slot
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
	nearProjParams.w = 1.0f;
	pShader->FXSetPSFloat(paramName, &nearProjParams, 1);

	m_passDepthLinearization.Execute();
}

void CSceneGBufferStage::Execute()
{
	PROFILE_LABEL_SCOPE("GBUFFER");

	Prepare(RenderView());

	CD3D9Renderer* rd = gcpRendD3D;
	SThreadInfo* const pThreadInfo = &(rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID]);

	D3DViewPort viewport = { 0.f, 0.f, float(rd->m_MainViewport.nWidth), float(rd->m_MainViewport.nHeight), 0.0f, 1.0f };
	rd->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

	// Clear depth (stencil initialized to 1 - 0 is reserved for MSAAed samples)
	const bool bReverseDepth = (rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	rd->FX_ClearTarget(&rd->m_DepthBufferOrigMSAA, CLEAR_ZBUFFER | CLEAR_STENCIL, bReverseDepth ? 0.0f : 1.0f, 1);

	// Clear velocity target
	rd->FX_ClearTarget(GetUtils().GetVelocityObjectRT(), Clr_Transparent);

	if (CRenderer::CV_r_wireframe != 0)
	{
		RECT rect = { 0, 0, int(viewport.Width), int(viewport.Height) };
		rd->FX_ClearTarget(CTexture::s_ptexSceneNormalsMap, Clr_Transparent, 1, &rect, true);
		rd->FX_ClearTarget(CTexture::s_ptexSceneDiffuse, Clr_Empty, 1, &rect, true);
		rd->FX_ClearTarget(CTexture::s_ptexSceneSpecular, Clr_Empty, 1, &rect, true);
	}

	// Stereo has separate velocity targets for left and right eye
	m_opaqueVelocityPass.ExchangeRenderTarget(3, GetUtils().GetVelocityObjectRT());

	// Update pass viewport and flags
	CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;

	if (pThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
		passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;

	RenderView()->GetDrawer().InitDrawSubmission();

	m_depthPrepass.SetFlags(passFlags);
	m_depthPrepass.SetViewport(viewport);
	m_opaquePass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaquePass.SetViewport(viewport);
	m_opaqueVelocityPass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
	m_opaqueVelocityPass.SetViewport(viewport);
	m_overlayPass.SetFlags(passFlags);
	m_overlayPass.SetViewport(viewport);

	RenderDepthPrepass();
	RenderSceneOpaque();
	RenderSceneOverlays();

	if (rd->m_nGraphicsPipeline >= 2)
	{
		RenderView()->GetDrawer().JobifyDrawSubmission();
	}
}
