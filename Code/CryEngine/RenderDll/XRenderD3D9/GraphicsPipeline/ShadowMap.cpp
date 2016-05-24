// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryMath/Range.h>
#include "ShadowMap.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include "Common/Include_HLSL_CPP_Shared.h"
#include "Common/GraphicsPipelineStateSet.h"
#include "Common/TypedConstantBuffer.h"
#include "Common/RenderView.h"
#include "Common/ReverseDepth.h"
#include "CompiledRenderObject.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

void CShadowMapStage::Init()
{
	// init per pass resource set template
	{
		m_pPerPassResourceSetTemplate = CCryDeviceWrapper::GetObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

		const EShaderStage shaderStages = EShaderStage_Vertex | EShaderStage_Hull | EShaderStage_Domain | EShaderStage_Pixel;
		m_pPerPassResourceSetTemplate->SetTexture(EPerPassTexture_WindGrid, nullptr, SResourceView::DefaultView, shaderStages);
		m_pPerPassResourceSetTemplate->SetTexture(EPerPassTexture_TerrainBaseMap, nullptr, SResourceView::DefaultViewSRGB, shaderStages);
		m_pPerPassResourceSetTemplate->SetTexture(EPerPassTexture_TerrainElevMap, nullptr, SResourceView::DefaultView, shaderStages);
		m_pPerPassResourceSetTemplate->SetTexture(EPerPassTexture_DissolveNoise, nullptr, SResourceView::DefaultView, shaderStages);
		m_pPerPassResourceSetTemplate->SetConstantBuffer(eConstantBufferShaderSlot_PerPass, nullptr, shaderStages);
		m_pPerPassResourceSetTemplate->SetConstantBuffer(eConstantBufferShaderSlot_PerView, nullptr, shaderStages);

		auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
		for (size_t i = 0; i < materialSamplers.size(); ++i)
			m_pPerPassResourceSetTemplate->SetSampler(EEfResSamplers(i), materialSamplers[i], shaderStages);
		// hardcoded point samplers
		m_pPerPassResourceSetTemplate->SetSampler(8, gcpRendD3D->m_nPointWrapSampler, shaderStages);
		m_pPerPassResourceSetTemplate->SetSampler(9, gcpRendD3D->m_nPointClampSampler, shaderStages);
	}

	// init resource layout
	{
		SDeviceResourceLayoutDesc layoutDesc;
		layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerInstanceCB, eConstantBufferShaderSlot_PerInstance, EShaderStage_Vertex | EShaderStage_Pixel);
		layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS, gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialResources());
		layoutDesc.SetResourceSet(EResourceLayoutSlot_PerInstanceExtraRS, gcpRendD3D->GetGraphicsPipeline().GetDefaultInstanceExtraResources());
		layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS, m_pPerPassResourceSetTemplate);

		m_pResourceLayout = CCryDeviceWrapper::GetObjectFactory().CreateResourceLayout(layoutDesc);
		CRY_ASSERT(m_pResourceLayout);
	}

	// preallocate passes
	m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_SunCached].Init(this, MAX_GSM_LODS_NUM);
	m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_HeightmapAO].Init(this, 1);
	m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_SunDynamic].Init(this, MAX_GSM_LODS_NUM);
	m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_LocalLight].Init(this, MAX_SHADOWMAP_FRUSTUMS);
	m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_Custom].Init(this, MAX_SHADOWMAP_FRUSTUMS);
}

bool CShadowMapStage::CreatePipelineState(const SGraphicsPipelineStateDescription& description, EPass passID, CDeviceGraphicsPSOPtr& outPSO)
{
	outPSO = NULL;

	CShader* pShader = static_cast<CShader*>(description.shaderItem.m_pShader);
	SShaderTechnique* pTechnique = pShader->GetTechnique(description.shaderItem.m_nTechnique, description.technique, true);
	if (!pTechnique)
		return true;

	CShaderResources* pRes = static_cast<CShaderResources*>(description.shaderItem.m_pShaderResources);
	if (pRes->m_ResFlags & MTL_FLAG_NOSHADOW)
		return true;

	SShaderPass* pShaderPass = &pTechnique->m_Passes[0];
	uint64 objectFlags = description.objectFlags;

	CDeviceGraphicsPSODesc psoDesc(m_pResourceLayout.get(), description);

	// Handle quality flags
	CStandardGraphicsPipeline::ApplyShaderQuality(psoDesc, gcpRendD3D->GetShaderProfile(pShader->m_eShaderType));

	SThreadInfo* const pShaderThreadInfo = &(gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID]);

	///////////////////////////////////
	//SStateRaster curRS = rd->m_StatesRS[rd->m_nCurStateRS];
	//if (CV_r_ShadowGenDepthClip==0)
	//{
	//	if( rTI.m_vFrustumInfo.x > 100.0f )
	//	{
	//		SStateRaster customRS = curRS;
	//		customRS.Desc.DepthClipEnable = false;
	//		rd->SetRasterState(&customRS);
	//	}
	//}

	// Set resource states
	bool bTwoSided = false;
	{
		if (pRes->m_ResFlags & MTL_FLAG_2SIDED)
			bTwoSided = true;

		if (pRes->IsAlphaTested())
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

		if (passID == ePass_DirectionalLightRSM || passID == ePass_LocalLightRSM)
		{
			if (pRes->m_Textures[EFTT_DIFFUSE] && pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_pTexModifier)
				psoDesc.m_ShaderFlags_MD |= pRes->m_Textures[EFTT_DIFFUSE]->m_Ext.m_nUpdateFlags;
		}

		if (pRes->m_pDeformInfo)
			psoDesc.m_ShaderFlags_MDV |= pRes->m_pDeformInfo->m_eType;
	}

	//tessellation
	psoDesc.m_bAllowTesselation = false;
	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NO_TESSELLATION];

#ifdef TESSELLATION_RENDERER
	const bool bHasTesselationShaders = pShaderPass && pShaderPass->m_HShader && pShaderPass->m_DShader;
	if (bHasTesselationShaders && (!(objectFlags & FOB_NEAREST) && (objectFlags & FOB_ALLOW_TESSELLATION)))
	{
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
		psoDesc.m_bAllowTesselation = true;
	}
#endif

	psoDesc.m_CullMode = bTwoSided ? eCULL_None : (pShaderPass->m_eCull != -1 ? (ECull)pShaderPass->m_eCull : eCULL_Back);
	if (pShader->m_eSHDType == eSHDT_Terrain)
	{
		//Flipped matrix for point light sources
		if (passID == ePass_DirectionalLight || passID == ePass_DirectionalLightCached)
			psoDesc.m_CullMode = eCULL_None;
		else
			psoDesc.m_CullMode = eCULL_Front; //front faces culling by default for terrain
	}

	if (passID == ePass_DirectionalLight || passID == ePass_DirectionalLightCached || passID == ePass_DirectionalLightRSM)
	{
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];
	}
	else if (passID == ePass_LocalLight || passID == ePass_LocalLightRSM)
	{
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_HW_PCF_COMPARE];
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_CUBEMAP0];

		// RBPF_MIRRORCULL
		if (psoDesc.m_CullMode != eCULL_None)
		{
			psoDesc.m_CullMode = (psoDesc.m_CullMode == eCULL_Front) ? eCULL_Back : eCULL_Front;
		}
	}

	if (passID == ePass_DirectionalLightRSM || passID == ePass_LocalLightRSM)
	{
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE4];

		if (!bTwoSided)
			psoDesc.m_CullMode = eCULL_Back;

		if (objectFlags & FOB_DECAL_TEXGEN_2D)
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

		if ((objectFlags & FOB_BLEND_WITH_TERRAIN_COLOR)) // && rRP.m_pCurObject->m_nTextureID > 0
			psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_BLEND_WITH_TERRAIN_COLOR];
	}

	psoDesc.m_ShaderFlags_MDV |= pShader->m_nMDV;
	if (objectFlags & FOB_OWNER_GEOMETRY)
		psoDesc.m_ShaderFlags_MDV &= ~MDV_DEPTH_OFFSET;
	if (objectFlags & FOB_BENDED)
		psoDesc.m_ShaderFlags_MDV |= MDV_BENDING;

	if (!(objectFlags & FOB_TRANS_MASK))  //&& gcpRendD3D->m_RP.m_RIs[0].Num() <= 1
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_OBJ_IDENTITY];

	if (objectFlags & FOB_NEAREST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_NEAREST];
	if (objectFlags & FOB_DISSOLVE)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_DISSOLVE];
	if (psoDesc.m_RenderState & GS_ALPHATEST_MASK)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

	if (psoDesc.m_bAllowTesselation)
	{
		psoDesc.m_PrimitiveType = ept3ControlPointPatchList;
		psoDesc.m_ObjectStreamMask |= VSM_NORMALS;
	}

	// rendertarget and depth stencil format
	{
		ETEX_Format depthStencilFormat[ePass_Count][3] =
		{
			{ eTF_D32F, eTF_D16, eTF_D24S8 }, // ePass_DirectionalLight
			{ eTF_D32F, eTF_D16, eTF_D16   }, // ePass_DirectionalLightCached
			{ eTF_D32F, eTF_D16, eTF_D32F  }, // ePass_LocalLight
			{ eTF_D32F, eTF_D16, eTF_D24S8 }, // ePass_DirectionalLightRSM
			{ eTF_D32F, eTF_D16, eTF_D24S8 }  // ePass_LocalLightRSM
		};

		int dsFormatIndex = (passID == ePass_DirectionalLightCached) ? CRenderer::CV_r_ShadowsCacheFormat : CRenderer::CV_r_shadowtexformat;
		psoDesc.m_DepthStencilFormat = depthStencilFormat[passID][clamp_tpl(dsFormatIndex, 0, 2)];

		if (passID == ePass_DirectionalLightRSM || passID == ePass_LocalLightRSM)
		{
			psoDesc.m_RenderTargetFormats[0] = eTF_R8G8B8A8;
			psoDesc.m_RenderTargetFormats[1] = eTF_R8G8B8A8;
			psoDesc.m_RenderTargetFormats[2] = eTF_R8G8B8A8;
		}
	}

	// TODO: handle per frustum slope scaled bias for directional lights CD3D9Renderer::PrepareDepthMap
	if (passID == ePass_DirectionalLight || passID == ePass_DirectionalLightCached || passID == ePass_DirectionalLightRSM)
	{
		psoDesc.m_DepthBias = 0;
		psoDesc.m_DepthBiasClamp = gcpRendD3D->CV_r_ShadowsBias * 20.0f;
		psoDesc.m_SlopeScaledDepthBias = 0.0f;//(lof->m_eFrustumType == ShadowMapFrustum::e_Nearest) ? 7 * lof->fDepthSlopeBias : lof->fDepthSlopeBias;
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	}

	// Create PSO
	outPSO = CCryDeviceWrapper::GetObjectFactory().CreateGraphicsPSO(psoDesc);
	return outPSO != nullptr;
}

bool CShadowMapStage::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	DevicePipelineStatesArray& stageStates = pStateArray[m_stageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	for (EPass passID = ePass_First; passID < ePass_Count; passID = EPass(passID + 1))
	{
		assert(passID < stageStates.size());
		bFullyCompiled &= CreatePipelineState(stateDesc, passID, stageStates[passID]);
	}

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return true;
}

void CShadowMapStage::Prepare(CRenderView* pRenderView)
{
	if (pRenderView->IsRecursive() || gcpRendD3D->m_CurRenderEye == RIGHT_EYE)
		return; // TODO: how will we handle recursion?

	// Prepare all our frustums render views for reading.
	pRenderView->PrepareShadowViews();

	if (gcpRendD3D->m_nGraphicsPipeline == 0)
		return;

	{
		PROFILE_LABEL_SCOPE("SHADOWMAP_PREPARE");

		// prepare the shadow pool
		PrepareShadowPool(pRenderView);

		// now prepare passes for each frustum
		for (auto& passGroup : m_ShadowMapPasses)
			passGroup.Reset();

		for (auto frustumType = CRenderView::eShadowFrustumRenderType_First;
		     frustumType != CRenderView::eShadowFrustumRenderType_Count;
		     frustumType = CRenderView::eShadowFrustumRenderType(frustumType + 1))
		{
			for (auto pFrustumToRender : pRenderView->GetShadowFrustumsByType(frustumType))
			{
				CRY_ASSERT(pRenderView->GetFrameId() == pFrustumToRender->pShadowsView->GetFrameId());
				PrepareShadowPasses(*pFrustumToRender, frustumType);
			}
		}
	}
}

void CShadowMapStage::PrepareShadowPool(CRenderView* pMainView)
{
	CD3D9Renderer* rd = gcpRendD3D;
	const int nThreadID = rd->m_RP.m_nProcessThreadID;

	// resize shadow pool if required
	static ICVar* pShadowsPoolSizeCVar = iConsole->GetCVar("e_ShadowsPoolSize");

	const int shadowPoolSize = pShadowsPoolSizeCVar->GetIVal();
	ETEX_Format eShadowPoolFormat = rd->CV_r_shadowtexformat == 1 ? eTF_D16 : eTF_D32F;
	CTexture::s_ptexRT_ShadowPool->Invalidate(shadowPoolSize, shadowPoolSize, eShadowPoolFormat);
	CTexture::s_ptexFarPlane->Invalidate(8, 8, eShadowPoolFormat); // 1x HTILE/DepthTile

	if (!CTexture::IsTextureExist(CTexture::s_ptexRT_ShadowPool))
	{
		static int reallocationCount = 0;
#if !defined(_RELEASE) && !CRY_PLATFORM_WINDOWS
		assert(reallocationCount == 0); // don't want any realloc on consoles
#endif
		CTexture::s_ptexRT_ShadowPool->CreateRenderTarget(eTF_Unknown, Clr_FarPlane);
		++reallocationCount;
	}

	if (!CTexture::IsTextureExist(CTexture::s_ptexFarPlane))
	{
		CTexture::s_ptexFarPlane->CreateRenderTarget(eTF_Unknown, Clr_FarPlane);

		gcpRendD3D.FX_ClearTarget(CTexture::s_ptexFarPlane->GetDeviceDepthStencilView(), CLEAR_ZBUFFER, Clr_FarPlane.r, 0, 0, nullptr);
	}

	// update shadow pool allocations
	CDeferredShading::Instance().m_pCurrentRenderView = pMainView;
	CDeferredShading::Instance().PackAllShadowFrustums(true, false);
}

void CShadowMapStage::PrepareShadowPasses(SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType)
{
	const int nSides = frustumToRender.pFrustum->bOmniDirectionalShadow ? OMNI_SIDES_NUM : 1;
	CRenderView* pShadowView = reinterpret_cast<CRenderView*>(frustumToRender.pShadowsView.get());
	ShadowMapFrustum* pFrustum = frustumToRender.pFrustum;

	ProfileLabel profileLabel;
	EPass passID;
	PreparePassIDForFrustum(frustumToRender, frustumRenderType, passID, profileLabel);

	for (int nS = 0; nS < nSides; nS++)
	{
		// assign empty shadow map
		frustumToRender.pFrustum->pDepthTex = CTexture::s_ptexFarPlane;

		if (pFrustum->nShadowGenMask & BIT(nS))
		{
			CShadowMapPass& curPass = m_ShadowMapPasses[frustumRenderType].AddPass();

			if (PrepareOutputsForPass(frustumToRender, nS, curPass))
			{
				curPass.SetupPassContext(m_stageID, passID, TTYPE_SHADOWGEN, FB_MASK);
				curPass.m_pFrustumToRender = &frustumToRender;
				curPass.m_nShadowFrustumSide = nS;

				curPass.m_bRequiresRender =
				  !pShadowView->GetRenderItems(nS).empty() ||
				  pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance ||
				  (pFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmCached && !pFrustum->bIncrementalUpdate);

				cry_strcpy(curPass.m_ProfileLabel, profileLabel);

				PrepareShadowPassForFrustum(frustumToRender, nS, curPass);
				UpdateShadowFrustumFromPass(curPass, *pFrustum);

				curPass.SetLabel(curPass.m_ProfileLabel);
				curPass.PrepareResources();
			}
			else
			{
				m_ShadowMapPasses[frustumRenderType].UndoAddPass();
			}
		}
	}
}

void CShadowMapStage::PreparePassIDForFrustum(const SShadowFrustumToRender& frustumToRender, CRenderView::eShadowFrustumRenderType frustumRenderType, EPass& passID, ProfileLabel& profileLabel) const
{
	const ShadowMapFrustum& frustum = *frustumToRender.pFrustum;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetRsmColorMap(frustum, true) && CSvoRenderer::GetRsmNormlMap(frustum, true))
	{
		if (frustumRenderType == CRenderView::eShadowFrustumRenderType_SunDynamic)
		{
			passID = ePass_DirectionalLightRSM;
			cry_sprintf(profileLabel, "SUN FRUSTUM (RSM) %i", frustum.nShadowMapLod);
		}
		else
		{
			passID = ePass_LocalLightRSM;
			cry_sprintf(profileLabel, "LOCAL LIGHT (RSM) %s", frustumToRender.pLight->m_sName);
		}
	}
	else
#endif
	{
		switch (frustumRenderType)
		{
		case CRenderView::eShadowFrustumRenderType_SunDynamic:
			{
				passID = ePass_DirectionalLight;
				const char* szLabel = frustum.m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance ? "SUN DISTANCE FRUSTUM %i" : "SUN FRUSTUM %i";
				cry_sprintf(profileLabel, szLabel, frustum.nShadowMapLod);
			}
			break;
		case CRenderView::eShadowFrustumRenderType_SunCached:
			{
				passID = ePass_DirectionalLightCached;
				cry_sprintf(profileLabel, "SUN CACHED %i", frustum.nShadowMapLod);
			}
			break;
		case CRenderView::eShadowFrustumRenderType_HeightmapAO:
			{
				passID = ePass_DirectionalLightCached;
				cry_sprintf(profileLabel, "HEIGHTMAP AO");
			}
			break;
		case CRenderView::eShadowFrustumRenderType_Custom:
			{
				passID = ePass_DirectionalLight;

				if (frustum.m_eFrustumType == ShadowMapFrustum::e_Nearest)
				{
					cry_sprintf(profileLabel, "SUN NEAREST");
				}
				else
				{
					IRenderNode* pRenderNode = reinterpret_cast<IRenderNode*>((IRenderNode*)frustum.castersList.front());
					const char* szName = pRenderNode->GetName();
					cry_sprintf(profileLabel, "SUN PER OBJECT %s", szName ? szName : "UNKNOWN");
				}
			}
			break;
		case CRenderView::eShadowFrustumRenderType_LocalLight:
			{
				passID = ePass_LocalLight;
				cry_sprintf(profileLabel, "LOCAL LIGHT %s", frustumToRender.pLight->m_sName);
			}
			break;
		}
	}
}

bool CShadowMapStage::PrepareOutputsForPass(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const
{
	const ShadowMapFrustum& frustum = *frustumToRender.pFrustum;

	ZeroStruct(targetPass.m_currentDepthTarget);
	targetPass.m_currentColorTarget.fill(nullptr);

	int arrViewport[4];
	CTexture* pDepthTarget = nullptr;
	std::array<CTexture*, 2> colorTargets;
	colorTargets.fill(nullptr);
	const CShadowMapPass* pClearDepthMapProvider = nullptr;
	CShadowMapPass::eClearMode clearMode = CShadowMapPass::eClearMode_Fill;

	if (frustum.bUseShadowsPool)
	{
		pDepthTarget = CTexture::s_ptexRT_ShadowPool;
		clearMode = CShadowMapPass::eClearMode_FillRect;

		frustum.GetSideViewport(nSide, arrViewport);
	}
	else
	{
		if (frustum.m_eFrustumType == ShadowMapFrustum::e_GsmDynamic ||
		    frustum.m_eFrustumType == ShadowMapFrustum::e_PerObject)
		{
			SDynTexture_Shadow* pDynTX = SDynTexture_Shadow::GetForFrustum(&frustum);
			pDepthTarget = pDynTX->m_pTexture;
		}
		else if (frustum.m_eFrustumType == ShadowMapFrustum::e_Nearest)
		{
			pDepthTarget = CTexture::s_ptexNearestShadowMap;
			if (!CTexture::IsTextureExist(pDepthTarget))
				pDepthTarget->CreateRenderTarget(frustum.m_eReqTF, Clr_FarPlane);
		}
		else
		{
			PrepareOutputsForFrustumWithCaching(frustum, pDepthTarget, pClearDepthMapProvider, clearMode);
		}

		arrViewport[0] = arrViewport[1] = 0;
		arrViewport[2] = pDepthTarget ? pDepthTarget->GetWidth() : 0;
		arrViewport[3] = pDepthTarget ? pDepthTarget->GetHeight() : 0;
	}

#if defined(FEATURE_SVO_GI)
	colorTargets[0] = CSvoRenderer::GetInstance()->GetRsmColorMap(frustum);
	colorTargets[1] = CSvoRenderer::GetInstance()->GetRsmNormlMap(frustum);
#endif

	if (!pDepthTarget || !pDepthTarget->GetDevTexture())
		return false;

	// now apply to pass
	targetPass.m_currentDepthTarget.nWidth = pDepthTarget->GetWidth();
	targetPass.m_currentDepthTarget.nHeight = pDepthTarget->GetHeight();
	targetPass.m_currentDepthTarget.nFrameAccess = -1;
	targetPass.m_currentDepthTarget.bBusy = false;
	targetPass.m_currentDepthTarget.pTexture = pDepthTarget;
	targetPass.m_currentDepthTarget.pTarget = pDepthTarget->GetDevTexture()->Get2DTexture();
	targetPass.m_currentDepthTarget.pSurface = pDepthTarget->GetDeviceDepthStencilView();
	targetPass.m_currentColorTarget = colorTargets;
	targetPass.m_pClearDepthMapProvider = pClearDepthMapProvider;
	targetPass.m_clearMode = clearMode;

	D3DViewPort viewport = { float(arrViewport[0]), float(arrViewport[1]), float(arrViewport[2]), float(arrViewport[3]), 0, 1 };

	targetPass.SetRenderTargets(
	  // Depth
	  &targetPass.m_currentDepthTarget,
	  // Color 0
	  targetPass.m_currentColorTarget[0],
	  // Color 1
	  targetPass.m_currentColorTarget[1]
	  );

	targetPass.SetViewport(viewport);

	return true;
}

void CShadowMapStage::UpdateShadowFrustumFromPass(const CShadowMapPass& sourcePass, ShadowMapFrustum& targetFrustum) const
{
	if (targetFrustum.m_eFrustumType == ShadowMapFrustum::e_Nearest)
	{
		targetFrustum.fWidthS *= targetFrustum.nTexSize / (float)sourcePass.m_currentDepthTarget.nWidth;
		targetFrustum.fWidthT *= targetFrustum.nTexSize / (float)sourcePass.m_currentDepthTarget.nHeight;
	}

	targetFrustum.pDepthTex = sourcePass.m_currentDepthTarget.pTexture;
	targetFrustum.nTextureWidth = sourcePass.m_currentDepthTarget.nWidth;
	targetFrustum.nTextureHeight = sourcePass.m_currentDepthTarget.nHeight;

	targetFrustum.mLightViewMatrix = sourcePass.m_ViewProjMatrix;
	targetFrustum.mLightProjMatrix.SetIdentity();

	if (sourcePass.m_pClearDepthMapProvider)
	{
		const ShadowMapFrustum* pSrcFrustum = sourcePass.m_pClearDepthMapProvider->m_pFrustumToRender->pFrustum;

		targetFrustum.fNearDist = pSrcFrustum->fNearDist;
		targetFrustum.fFarDist = pSrcFrustum->fFarDist;
		targetFrustum.fRendNear = pSrcFrustum->fRendNear;
		targetFrustum.fDepthConstBias = pSrcFrustum->fDepthConstBias;
		targetFrustum.fDepthTestBias = pSrcFrustum->fDepthTestBias;
		targetFrustum.fDepthSlopeBias = pSrcFrustum->fDepthSlopeBias;
	}
}

void CShadowMapStage::PrepareOutputsForFrustumWithCaching(const ShadowMapFrustum& frustum, CTexture*& pDepthTarget, const CShadowMapPass*& pClearDepthMapProvider, CShadowMapPass::eClearMode& clearMode) const
{
	CRY_ASSERT(frustum.IsCached() || frustum.m_eFrustumType == ShadowMapFrustum::eFrustumType::e_GsmDynamicDistance);

	pDepthTarget = nullptr;
	pClearDepthMapProvider = nullptr;
	clearMode = CShadowMapPass::eClearMode_Fill;

	if (frustum.IsCached())
	{
		if (frustum.m_eFrustumType == ShadowMapFrustum::eFrustumType::e_GsmCached)
		{
			int nCachedMapIndex = frustum.nShadowMapLod - (gcpRendD3D->CV_r_ShadowsCache - 1);
			CRY_ASSERT(nCachedMapIndex >= 0 && nCachedMapIndex < CRY_ARRAY_COUNT(CTexture::s_ptexCachedShadowMap));

			pDepthTarget = CTexture::s_ptexCachedShadowMap[clamp_tpl(nCachedMapIndex, 0, int(CRY_ARRAY_COUNT(CTexture::s_ptexCachedShadowMap) - 1))];
		}
		else // e_HeightMapAO
		{
			pDepthTarget = CTexture::s_ptexHeightMapAODepth[0];
		}

		clearMode = frustum.bIncrementalUpdate ? CShadowMapPass::eClearMode_None : CShadowMapPass::eClearMode_Fill;
	}
	else if (frustum.m_eFrustumType == ShadowMapFrustum::eFrustumType::e_GsmDynamicDistance)
	{
		// find corresponding cached frustum
		for (const auto& cachedPass : m_ShadowMapPasses[CRenderView::eShadowFrustumRenderType_SunCached])
		{
			const ShadowMapFrustum* pCachedFrustum = cachedPass.m_pFrustumToRender->pFrustum;
			if (pCachedFrustum->nShadowMapLod == frustum.nShadowMapLod)
			{
				pClearDepthMapProvider = &cachedPass;
				clearMode = CShadowMapPass::eClearMode_CopyDepthMap;

				SDynTexture_Shadow* pDynTX = SDynTexture_Shadow::GetForFrustum(&frustum);
				pDepthTarget = pDynTX->m_pTexture;
				break;
			}
		}
	}
}

void CShadowMapStage::PrepareShadowPassForFrustum(const SShadowFrustumToRender& frustumToRender, int nSide, CShadowMapPass& targetPass) const
{
	const ShadowMapFrustum& frustum = *frustumToRender.pFrustum;

	Vec4 frustumInfo = Vec4(
	  (gcpRendD3D->CV_r_ShadowGenDepthClip == 0 && frustum.fRendNear > 0.0f) ? frustum.fRendNear : frustum.fNearDist,
	  (frustum.m_eFrustumType == ShadowMapFrustum::e_HeightMapAO) ? 1.0f : frustum.fFarDist,
	  (frustum.m_eFrustumType == ShadowMapFrustum::e_Nearest) ? 7 * frustum.fDepthSlopeBias : frustum.fDepthSlopeBias,
	  frustum.fDepthTestBias
	  );

	// get view projection matrix
	if (!frustum.bOmniDirectionalShadow)
	{
		Matrix44A viewProj = frustum.mLightViewMatrix;

		if (frustum.m_eFrustumType == ShadowMapFrustum::e_Nearest)
		{
			const Vec3 camPos = gcpRendD3D->GetCamera().GetPosition();
			AABB aabb = frustum.aabbCasters;
			aabb.Move(camPos);

			Matrix44A view, proj;
			CShadowUtils::GetShadowMatrixForObject(proj, view, frustumInfo, frustum.vLightSrcRelPos, aabb);

			viewProj = view * proj;
		}

		Matrix44A viewProjOrig = viewProj;
		if (targetPass.m_pClearDepthMapProvider)
		{
			if (!CShadowUtils::GetSubfrustumMatrix(viewProj, targetPass.m_pClearDepthMapProvider->m_pFrustumToRender->pFrustum, &frustum))
				targetPass.m_clearMode = CShadowMapPass::eClearMode_Fill;
		}

		targetPass.m_ViewProjMatrix = viewProj;
		targetPass.m_ViewProjMatrixOrig = viewProjOrig;
		targetPass.m_FrustumInfo = frustumInfo;
	}
	else
	{
		Matrix44 view, proj;
		CShadowUtils::GetCubemapFrustum(FTYP_SHADOWOMNIPROJECTION, &frustum, nSide, &proj, &view);

		targetPass.m_ViewProjMatrix = view * proj;
		targetPass.m_FrustumInfo = frustumInfo;
	}
}

void CShadowMapStage::CShadowMapPassGroup::Init(CShadowMapStage* pStage, int nSize)
{
	m_Passes.reserve(nSize);
	for (int i = 0; i < nSize; ++i)
		m_Passes.emplace_back(pStage);
}

CShadowMapStage::CShadowMapPass::CShadowMapPass(CShadowMapStage* pStage)
	: m_ViewProjMatrix(IDENTITY)
	, m_ViewProjMatrixOrig(IDENTITY)
{
	m_pFrustumToRender = nullptr;
	m_nShadowFrustumSide = 0;
	m_pShadowMapStage = pStage;

	m_pPerPassResources = CCryDeviceWrapper::GetObjectFactory().CloneResourceSet(pStage->m_pPerPassResourceSetTemplate);
	m_pPerPassConstantBuffer.Assign_NoAddRef(gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerPassConstantBuffer_ShadowGen)));
	m_pPerViewConstantBuffer.Assign_NoAddRef(gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer)));
}

void CShadowMapStage::CShadowMapPass::PrepareResources()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	assert(m_pFrustumToRender);
	assert(m_pPerPassResources);
	assert(m_pPerViewConstantBuffer);
	assert(m_pPerPassConstantBuffer);

	const EShaderStage shaderStages = m_pPerPassResources->GetShaderStages();
	ShadowMapFrustum& frustum = *m_pFrustumToRender->pFrustum;

	// update per pass textures
	{
		int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
		gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

		m_pPerPassResources->SetTexture(EPerPassTexture_WindGrid, CTexture::s_ptexWindGrid, SResourceView::DefaultView, shaderStages);
		m_pPerPassResources->SetTexture(EPerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), SResourceView::DefaultViewSRGB, shaderStages);
		m_pPerPassResources->SetTexture(EPerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), SResourceView::DefaultView, shaderStages);
		m_pPerPassResources->SetTexture(EPerPassTexture_DissolveNoise, CTexture::s_ptexDissolveNoiseMap, SResourceView::DefaultView, shaderStages);
	}

	// per pass CB
	{
		CTypedConstantBuffer<HLSL_PerPassConstantBuffer_ShadowGen> cb(m_pPerPassConstantBuffer);

		cb->CP_ShadowGen_LightPos = Vec4(frustum.vLightSrcRelPos + frustum.vProjTranslation, 0);
		cb->CP_ShadowGen_ViewPos = Vec4(gcpRendD3D->GetCamera().GetPosition(), 0);
		cb->CP_ShadowGen_DepthTestBias = Vec4(ZERO); // TODO

		cb->CP_ShadowGen_FrustrumInfo = m_FrustumInfo;

		cb->CP_ShadowGen_VegetationAlphaClamp = Vec4(ZERO);
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer* pSvoRenderer = CSvoRenderer::GetInstance())
		{
			cb->CP_ShadowGen_VegetationAlphaClamp.x = pSvoRenderer->GetVegetationMaxOpacity();
		}
#endif

		//// TODO: find way to handle per object const bias for non-directional lights (%_RT_CUBEMAP0)
		//{
		//	// FX_DrawShader_General
		//	if (rTI.m_PersFlags & RBPF_SHADOWGEN)
		//	{
		//		if (slw->m_eCull == eCULL_None)
		//			m_cEF.m_TempVecs[1][0] = rTI.m_vFrustumInfo.w;
		//	}

		//	// FX_FlushShader_ShadowGen
		//	if (rd->m_RP.m_pShaderResources)
		//	{
		//		if (rd->m_RP.m_pShaderResources->m_ResFlags & MTL_FLAG_2SIDED)
		//		{
		//			//handle terrain self-shadowing and two-sided geom
		//			rd->m_cEF.m_TempVecs[1][0] = rTI.m_vFrustumInfo.w;
		//		}
		//}

		cb.CopyToDevice();

		m_pPerPassResources->SetConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassConstantBuffer.get(), shaderStages);
	}

	// per view CB
	{
		CStandardGraphicsPipeline::SViewInfo viewInfo(gcpRendD3D->GetRCamera(), gcpRendD3D->GetCamera());

		viewInfo.m_CameraProjZeroMatrix = m_ViewProjMatrix;
		viewInfo.m_CameraProjMatrix = m_ViewProjMatrix;
		viewInfo.m_CameraProjNearestMatrix = m_ViewProjMatrix;
		viewInfo.m_ProjMatrix = m_ViewProjMatrix;
		viewInfo.m_PrevCameraProjMatrix = m_ViewProjMatrix;
		viewInfo.m_PrevCameraProjNearestMatrix = m_ViewProjMatrix;

		D3D11_VIEWPORT viewport = { 0, 0, float(m_currentDepthTarget.nWidth), float(m_currentDepthTarget.nHeight), 0, 1.0f };

		viewInfo.viewport = viewport;
		viewInfo.downscaleFactor = Vec4(1);
		viewInfo.bReverseDepth = false;
		viewInfo.bMirrorCull = false;
		viewInfo.pFrustumPlanes = frustum.FrustumPlanes[0].GetFrustumPlane(0);

		gcpRendD3D->GetGraphicsPipeline().UpdatePerViewConstantBuffer(viewInfo, m_pPerViewConstantBuffer);

		m_pPerPassResources->SetConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer.get(), shaderStages);
	}

	m_pPerPassResources->SetDirty(true);
	m_pPerPassResources->Build();
	CRY_ASSERT(m_pPerPassResources->IsValid());

	// clear targets
	if (m_clearMode == eClearMode_Fill || m_clearMode == eClearMode_FillRect)
	{
		const bool bUseClearRect = m_clearMode == eClearMode_FillRect;
		gcpRendD3D->FX_ClearTarget(&m_currentDepthTarget, CLEAR_ZBUFFER, 1.0f, 0, bUseClearRect ? 1 : 0, bUseClearRect ? &m_scissorRect : nullptr, false);

		for (auto pColorTarget : m_currentColorTarget)
		{
			if (pColorTarget)
			{
				gcpRendD3D->FX_ClearTarget(pColorTarget, Clr_Transparent, bUseClearRect ? 1 : 0, bUseClearRect ? &m_scissorRect : nullptr, false);
			}
		}
	}
}

void CShadowMapStage::CShadowMapPass::PreRender()
{
	if (m_clearMode == eClearMode_CopyDepthMap)
	{
		CRY_ASSERT(m_pClearDepthMapProvider);
		m_pShadowMapStage->CopyShadowMap(*m_pClearDepthMapProvider, *this);
	}
}

void CShadowMapStage::CopyShadowMap(const CShadowMapPass& sourcePass, CShadowMapPass& targetPass)
{
	ShadowMapFrustum* pDst = targetPass.m_pFrustumToRender->pFrustum;
	const ShadowMapFrustum* pSrc = sourcePass.m_pFrustumToRender->pFrustum;

	CRY_ASSERT(pSrc->m_eFrustumType == ShadowMapFrustum::e_GsmCached);
	CRY_ASSERT(pDst->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance);
	CRY_ASSERT(pSrc->nShadowMapLod == pDst->nShadowMapLod);

	const bool bEmptySrcFrustum = pSrc->nShadowGenMask == 0;
	const auto& renderItems = reinterpret_cast<CRenderView*>(targetPass.m_pFrustumToRender->pShadowsView.get())->GetRenderItems(0);

	// do we need to merge static shadows into the dynamic shadow map?
	if (bEmptySrcFrustum || !renderItems.empty())
	{
		if (bEmptySrcFrustum)
		{
			gcpRendD3D->FX_ClearTarget(&targetPass.m_currentDepthTarget, CLEAR_ZBUFFER | CLEAR_STENCIL, Clr_FarPlane.r, 0);
		}
		else
		{
			static CCryNameTSCRC tech("ReprojectShadowMap");
			CShader* pShader = CShaderMan::s_ShaderShadowMaskGen;
			int nTexStatePoint = CTexture::GetTexState(STexState(FILTER_POINT, true));

			m_CopyShadowMapPass.SetDepthTarget(&targetPass.m_currentDepthTarget);
			m_CopyShadowMapPass.SetTechnique(pShader, tech, 0);
			m_CopyShadowMapPass.SetState(GS_DEPTHWRITE | GS_DEPTHFUNC_NOTEQUAL);
			m_CopyShadowMapPass.SetTextureSamplerPair(0, pSrc->pDepthTex, nTexStatePoint);
			m_CopyShadowMapPass.BeginConstantUpdate();

			Matrix44 mReprojDstToSrc = pDst->mLightViewMatrix.GetInverted() * pSrc->mLightViewMatrix;
			;
			static CCryNameR paramReprojMatDstToSrc("g_mReprojDstToSrc");
			pShader->FXSetPSFloat(paramReprojMatDstToSrc, (Vec4*) mReprojDstToSrc.GetData(), 4);

			Matrix44 mReprojSrcToDst = pSrc->mLightViewMatrix.GetInverted() * pDst->mLightViewMatrix;
			static CCryNameR paramReprojMatSrcToDst("g_mReprojSrcToDst");
			pShader->FXSetPSFloat(paramReprojMatSrcToDst, (Vec4*) mReprojSrcToDst.GetData(), 4);

			m_CopyShadowMapPass.Execute();
		}

		pDst->packWidth[0] = pDst->nTextureWidth;
		pDst->packHeight[0] = pDst->nTextureHeight;

		pDst->packX[0] = pDst->packY[0] = 0;
	}
	else
	{
		// get crop rectangle for projection
		Matrix44r mReproj = Matrix44r(targetPass.m_ViewProjMatrixOrig).GetInverted() * Matrix44r(pSrc->mLightViewMatrix);
		Vec4r srcClipPosTL = Vec4r(-1, -1, 0, 1) * mReproj;
		srcClipPosTL /= srcClipPosTL.w;

		const float fSnap = 2.0f / pSrc->pDepthTex->GetWidth();
		Vec4 crop = Vec4(
		  crop.x = fSnap * int(srcClipPosTL.x / fSnap),
		  crop.y = fSnap * int(srcClipPosTL.y / fSnap),
		  crop.z = 2.0f * pDst->nTextureWidth / float(pSrc->nTextureWidth),
		  crop.w = 2.0f * pDst->nTextureHeight / float(pSrc->nTextureHeight)
		  );

		pDst->packX[0] = int((crop.x * 0.5f + 0.5f) * pSrc->pDepthTex->GetWidth() + 0.5f);
		pDst->packY[0] = int((-(crop.y + crop.w) * 0.5f + 0.5f) * pSrc->pDepthTex->GetHeight() + 0.5f);
		pDst->packWidth[0] = pDst->nTextureWidth;
		pDst->packHeight[0] = pDst->nTextureHeight;

		pDst->pDepthTex = pSrc->pDepthTex;
		pDst->nTexSize = pSrc->nTexSize;
		pDst->nTextureWidth = pSrc->nTextureWidth;
		pDst->nTextureHeight = pSrc->nTextureHeight;
	}

	pDst->bIncrementalUpdate = true;
	pDst->fNearDist = pSrc->fNearDist;
	pDst->fFarDist = pSrc->fFarDist;
	pDst->fRendNear = pSrc->fRendNear;
	pDst->fDepthConstBias = pSrc->fDepthConstBias;
	pDst->fDepthTestBias = pSrc->fDepthTestBias;
	pDst->fDepthSlopeBias = pSrc->fDepthSlopeBias;
}

void CShadowMapStage::Execute()
{
	PROFILE_LABEL_SCOPE("SHADOWMAPS");

	if (!m_pResourceLayout)
		return;

	CD3D9Renderer* rd = gcpRendD3D;
	const int nThreadID = rd->m_RP.m_nProcessThreadID;

	RenderView()->GetDrawer().InitDrawSubmission();

	const char* pLastProfileLabel = nullptr;

	for (auto& curPassGroup : m_ShadowMapPasses)
	{
		for (auto& curPass : curPassGroup)
		{
			if (curPass.m_bRequiresRender)
			{
				CRenderView* pShadowsView = reinterpret_cast<CRenderView*>(curPass.GetFrustum()->pShadowsView.get());

				if (curPass.m_ProfileLabel[0] && curPass.m_nShadowFrustumSide == 0)
				{
					if (pLastProfileLabel)
					{
						PROFILE_LABEL_POP(pLastProfileLabel);
					}

					PROFILE_LABEL_PUSH(curPass.m_ProfileLabel);
					pLastProfileLabel = curPass.m_ProfileLabel;
				}

				curPass.PreRender();
				curPass.SetPassResources(m_pResourceLayout, curPass.GetResources());
				curPass.DrawRenderItems(pShadowsView, (ERenderListID)curPass.m_nShadowFrustumSide, -1, -1, EFSLIST_SHADOW_GEN);
			}
		}
	}

	if (pLastProfileLabel)
	{
		PROFILE_LABEL_POP(pLastProfileLabel);
	}
	rd->m_RP.m_TI[nThreadID].m_PersFlags &= ~RBPF_SHADOWGEN;

	if (rd->m_nGraphicsPipeline >= 2)
	{
		RenderView()->GetDrawer().JobifyDrawSubmission();
	}
}
