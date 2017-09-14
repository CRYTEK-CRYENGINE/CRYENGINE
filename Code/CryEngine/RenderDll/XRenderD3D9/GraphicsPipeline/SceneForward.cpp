// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneForward.h"

#include "DriverD3D.h"
#include "Common/ReverseDepth.h"
#include "Common/RendElements/Stars.h"
#include "Fog.h"
#include "VolumetricFog.h"

#include "GraphicsPipeline/Fog.h"
#include "GraphicsPipeline/VolumetricFog.h"

struct SPerPassConstantBuffer
{
	CFogStage::SForwardParams                 cbFog;
	CVolumetricFogStage::SForwardParams       cbVoxelFog;
	CShadowUtils::SShadowCascadesSamplingInfo cbShadowSampling;
	struct  
	{
		Vec4 CloudShadingColorSun;
		Vec4 CloudShadingColorSky;
	}                                         cbMisc;
};

CSceneForwardStage::CSceneForwardStage()
	: m_opaquePassResources()
	, m_transparentPassResources()
	, m_eyeOverlayPassResources()
{}


void CSceneForwardStage::Init()
{
	// Create per-pass resources
	m_pOpaquePassResourceSet      = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pTransparentPassResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pEyeOverlayPassResourceSet  = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_pPerPassCB                  = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(SPerPassConstantBuffer));
	
	bool bSuccess = PreparePerPassResources(nullptr, true);
	assert(bSuccess);

	// Create resource layout
	m_pOpaqueResourceLayout      = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_opaquePassResources);
	m_pTransparentResourceLayout = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_transparentPassResources);
	m_pEyeOverlayResourceLayout  = gcpRendD3D->GetGraphicsPipeline().CreateScenePassLayout(m_eyeOverlayPassResources);

	// Freeze resource-set layout (assert  will fire when violating the constraint)
	m_opaquePassResources     .AcceptChangedBindPoints();
	m_transparentPassResources.AcceptChangedBindPoints();
	m_eyeOverlayPassResources .AcceptChangedBindPoints();

	// Opaque forward scene pass
	m_forwardOpaquePass.SetLabel("FORWARD_OPAQUE");
	m_forwardOpaquePass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_TILED_FORWARD | FB_GENERAL);
	m_forwardOpaquePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);
	m_forwardOpaquePass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexHDRTarget
	);

	// Overlay forward scene pass
	m_forwardOverlayPass.SetLabel("FORWARD_OVERLAY");
	m_forwardOverlayPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_TILED_FORWARD | FB_GENERAL);
	m_forwardOverlayPass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);
	m_forwardOverlayPass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexHDRTarget
	);

	// Transparent forward scene passes
	m_forwardTransparentBWPass.SetLabel("FORWARD_TRANSPARENT_BW");
	m_forwardTransparentBWPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_BELOW_WATER, EFSLIST_TRANSP, 0);
	m_forwardTransparentBWPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardTransparentBWPass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexHDRTarget
	);
	
	m_forwardTransparentAWPass.SetLabel("FORWARD_TRANSPARENT_AW");
	m_forwardTransparentAWPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL, EFSLIST_TRANSP, FB_BELOW_WATER);
	m_forwardTransparentAWPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardTransparentAWPass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexHDRTarget
	);

	m_forwardTransparentLoResPass.SetLabel("FORWARD_TRANSPARENT_LORES");
	m_forwardTransparentLoResPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL, EFSLIST_HALFRES_PARTICLES, 0);
	m_forwardTransparentLoResPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardTransparentLoResPass.SetRenderTargets(
		// Depth
		nullptr,
		// Color 0
		CTexture::s_ptexHDRTargetScaled[0]//[1]//[2]
	);

	// Opaque forward recursive scene pass
	m_forwardOpaqueRecursivePass.SetLabel("FORWARD_OPAQUE_RECURSIVE");
	m_forwardOpaqueRecursivePass.SetupPassContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL, EFSLIST_FORWARD_OPAQUE);
	m_forwardOpaqueRecursivePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);
	m_forwardOpaqueRecursivePass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture, // this texture is used only for initialization.
		// Color 0
		CTexture::s_ptexHDRTarget // this texture is used only for initialization.
	);

	// Overlay forward recursive scene pass
	m_forwardOverlayRecursivePass.SetLabel("FORWARD_OVERLAY_RECURSIVE");
	m_forwardOverlayRecursivePass.SetupPassContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL, EFSLIST_DECAL);
	m_forwardOverlayRecursivePass.SetPassResources(m_pOpaqueResourceLayout, m_pOpaquePassResourceSet);
	m_forwardOverlayRecursivePass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture, // this texture is used only for initialization.
		// Color 0
		CTexture::s_ptexHDRTarget // this texture is used only for initialization.
	);

	m_forwardTransparentRecursivePass.SetLabel("FORWARD_TRANSPARENT_AW_RECURSIVE");
	m_forwardTransparentRecursivePass.SetupPassContext(m_stageID, ePass_ForwardRecursive, TTYPE_GENERAL, FB_GENERAL, EFSLIST_TRANSP, FB_BELOW_WATER);
	m_forwardTransparentRecursivePass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardTransparentRecursivePass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture, // this texture is used only for initialization.
		// Color 0
		CTexture::s_ptexHDRTarget // this texture is used only for initialization.
	);

	m_forwardEyeOverlayPass.SetLabel("FORWARD_EYE_AO_OVERLAY");
	m_forwardEyeOverlayPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL, EFSLIST_EYE_OVERLAY, 0);
	m_forwardEyeOverlayPass.SetPassResources(m_pEyeOverlayResourceLayout, m_pEyeOverlayPassResourceSet);
	m_forwardEyeOverlayPass.SetRenderTargets(
		// Depth
		gcpRendD3D->m_pZTexture,
		// Color 0
		CTexture::s_ptexSceneDiffuse
	);

	m_forwardLDRPass.SetLabel("FORWARD_AFTER_POSTFX_HDR");
	m_forwardLDRPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL, EFSLIST_AFTER_HDRPOSTPROCESS, 0);
	m_forwardLDRPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardLDRPass.SetRenderTargets(
		// Depth
		gcpRendD3D->GetCurrentDepthOutput(),
		// Color 0
		CTexture::s_ptexSceneDiffuse
	);

	m_forwardLDRPass.SetLabel("FORWARD_AFTER_POSTFX_LDR");
	m_forwardLDRPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_GENERAL, EFSLIST_AFTER_POSTPROCESS, 0);
	m_forwardLDRPass.SetPassResources(m_pTransparentResourceLayout, m_pTransparentPassResourceSet);
	m_forwardLDRPass.SetRenderTargets(
		// Depth
		gcpRendD3D->GetCurrentDepthOutput(),
		// Color 0
		gcpRendD3D->GetCurrentTargetOutput()
	);

	// Information flow:
	//  CTexture::s_ptexHDRTarget ->
	//  CTexture::s_ptexSceneDiffuse ->
	//  gcpRendD3D->GetCurrentTargetOutput()
}

bool CSceneForwardStage::CreatePipelineState(const SGraphicsPipelineStateDescription& desc,
                                             CDeviceGraphicsPSOPtr& outPSO,
                                             EPass passId,
                                             std::function<void(CDeviceGraphicsPSODesc& psoDesc, const SGraphicsPipelineStateDescription& desc)> customState)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	outPSO = nullptr;

	const bool bRecursive = (passId == ePass_ForwardRecursive);
	CSceneRenderPass* pSceneRenderPass = bRecursive ? &m_forwardTransparentRecursivePass : &m_forwardTransparentBWPass;

	if (bRecursive && desc.objectFlags & FOB_REQUIRES_RESOLVE)
	{
		// recursive pass doesn't support refractive render object.
		return true;
	}

	CDeviceGraphicsPSODesc psoDesc(nullptr, desc);
	
	if (passId == ePass_ForwardRecursive)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_SECONDARY_VIEW];

	if (customState)
	{
		customState(psoDesc, desc);

		if (bRecursive)
		{
			// NOTE: Only CREParticle and CREFogVolume use custom-state.
			//       CREParticle is rendered for recursive pass without shadow and depth buffer access.
			//       CREFogVolume isn't rendered for recursive pass.
			psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_PARTICLE_SHADOW] | g_HWSR_MaskBit[HWSR_SOFT_PARTICLE]);
		}
	}
	else
	{
		const int32 shaderFlags = ((CShader*)desc.shaderItem.m_pShader)->GetFlags();
		const int32 shaderFlags2 = ((CShader*)desc.shaderItem.m_pShader)->GetFlags2();
		const bool bSupportsDeferred = (shaderFlags & EF_SUPPORTSDEFERREDSHADING_FULL) != 0;
		const bool bAlphaBlended = desc.shaderItem.m_pShaderResources && desc.shaderItem.m_pShaderResources->IsTransparent();
		const bool bOpaquePass = !bAlphaBlended && (bSupportsDeferred || (desc.shaderItem.m_nPreprocessFlags & FB_TILED_FORWARD));
		const bool bOverlay = (desc.objectFlags & (FOB_TERRAIN_LAYER | FOB_DECAL)) || (shaderFlags & EF_DECAL);
		const bool bHair = (shaderFlags2 & EF2_HAIR) != 0;
		const bool bEyeOverlay = (shaderFlags2 & EF2_EYE_OVERLAY) != 0;
		const bool bAfterPostProcess = (shaderFlags2 & EF2_AFTERHDRPOSTPROCESS) != 0;
		const bool bEmissive = (desc.shaderItem.m_pShaderResources && desc.shaderItem.m_pShaderResources->IsEmissive());

		if (!pRenderer->GetGraphicsPipeline().FillCommonScenePassStates(desc, psoDesc))
			return true;

		if (bRecursive)
		{
			if (!(desc.objectFlags & FOB_NO_FOG))
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_FOG];

			if (bOpaquePass)
				pSceneRenderPass = &m_forwardOpaqueRecursivePass;
		}
		else if (bOpaquePass)
		{
			psoDesc.m_RenderState &= ~(GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_ALPHATEST);
			psoDesc.m_RenderState |= GS_DEPTHFUNC_EQUAL;
			psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_ALPHATEST];
			if (CRenderer::CV_r_DeferredShadingTiled)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

			pSceneRenderPass = &m_forwardOpaquePass;
		}
		else
		{
			if (!bOverlay && !(desc.objectFlags & FOB_NO_FOG))
			{
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_FOG];

				// volumetric fog supports only general pass currently.
				if (pRenderer->m_bVolumetricFogEnabled)
				{
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_VOLUMETRIC_FOG];
				}
			}
			if (CRenderer::CV_r_DeferredShadingTiled)
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

			pSceneRenderPass = pSceneRenderPass;
		}

		if (bRecursive && (bHair || bAfterPostProcess || bEyeOverlay))
		{
			// recursive pass doesn't support EF2_HAIR and EF2_AFTERHDRPOSTPROCESS.
			return true;
		}
		else if (bHair)
		{
			psoDesc.m_ShaderFlags_RT &= ~(g_HWSR_MaskBit[HWSR_TILED_SHADING] |g_HWSR_MaskBit[HWSR_QUALITY1]);
			if (CRenderer::CV_r_DeferredShadingTiled && CRenderer::CV_r_DeferredShadingTiledHairQuality > 0)
			{
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_TILED_SHADING];

				if (CRenderer::CV_r_DeferredShadingTiledHairQuality > 1)
					psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_QUALITY1];
			}
			
			psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
			if (shaderFlags2 & EF2_DEPTH_FIXUP)  // Thin hair feature enabled
			{
				psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL;
				psoDesc.m_RenderState |= GS_BLSRC_SRC1ALPHA | GS_BLDST_ONEMINUSSRC1ALPHA | GS_BLALPHA_MIN;
			}
			else
			{
				psoDesc.m_RenderState |= GS_DEPTHFUNC_EQUAL;
			}
			
			pSceneRenderPass = &m_forwardTransparentBWPass;
		}
		else if (bAlphaBlended || bOverlay || bEmissive)
		{
			if (!bAlphaBlended && bEmissive)
			{
				// Handle emissive materials
				psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
				psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_ONE | GS_BLDST_ONE | GS_NOCOLMASK_A;
			}
			else
			{
				psoDesc.m_RenderState = (psoDesc.m_RenderState & ~(GS_BLEND_MASK | GS_DEPTHWRITE | GS_DEPTHFUNC_MASK | GS_STENCIL));
				psoDesc.m_RenderState |= GS_DEPTHFUNC_LEQUAL | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NOCOLMASK_A;
				psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHABLEND];
			}

			if (bOverlay)
				pSceneRenderPass = bRecursive ? &m_forwardOverlayPass : &m_forwardOverlayPass;
		}
		else if (bEyeOverlay)
		{
			pSceneRenderPass = &m_forwardEyeOverlayPass;
		}
		else if (bAfterPostProcess)
		{
			psoDesc.m_CullMode = eCULL_None;
			pSceneRenderPass = &m_forwardLDRPass;
		}
	}

	if (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
	{
		psoDesc.m_RenderState |= ReverseDepthHelper::ConvertDepthFunc(psoDesc.m_RenderState);
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_REVERSE_DEPTH];
	}

	psoDesc.m_pResourceLayout = pSceneRenderPass->GetResourceLayout();
	psoDesc.m_pRenderPass     = pSceneRenderPass->GetRenderPass();

	outPSO = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
	return outPSO != nullptr;
}

bool CSceneForwardStage::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, const SGraphicsPipelineStateDescription& stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	DevicePipelineStatesArray& stageStates = pStateArray[m_stageID];

	if (pStateCache->Find(stateDesc, stageStates))
		return true;

	bool bFullyCompiled = true;

	SGraphicsPipelineStateDescription _stateDesc = stateDesc;

	_stateDesc.technique = TTYPE_GENERAL;
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_Forward], ePass_Forward);

	_stateDesc.technique = TTYPE_GENERAL;
	bFullyCompiled &= CreatePipelineState(_stateDesc, stageStates[ePass_ForwardRecursive], ePass_ForwardRecursive);

	if (bFullyCompiled)
	{
		pStateCache->Put(stateDesc, stageStates);
	}

	return bFullyCompiled;
}

bool CSceneForwardStage::PreparePerPassResources(CRenderView* pRenderView, bool bOnInit, bool bShadowMask, bool bFog)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CTexture* pShadowMask = bShadowMask ? CTexture::s_ptexShadowMask : CTexture::s_ptexBlack;

	CDeviceResourceSetDesc* pResourceDescs[] = { &m_opaquePassResources, &m_transparentPassResources, &m_eyeOverlayPassResources };
	CDeviceResourceSet*     pResourceSets[]  = { m_pOpaquePassResourceSet.get(), m_pTransparentPassResourceSet.get(), m_pEyeOverlayPassResourceSet.get() };

	for (uint32 i = 0; i < CRY_ARRAY_COUNT(pResourceDescs); i++)
	{
		const bool bOpaquePass = (i == 0);
		const bool bTransparentPass = (i == 1);
		const bool bEyeOverlayPass = (i == 2);

		CDeviceResourceSetDesc* pResources = pResourceDescs[i];
		CDeviceResourceSet*     pResourceSet = pResourceSets[i];

		// Samplers
		{
			auto materialSamplers = gcpRendD3D->GetGraphicsPipeline().GetDefaultMaterialSamplers();
			for (int i = 0; i < materialSamplers.size(); ++i)
			{
				pResources->SetSampler(EEfResSamplers(i), materialSamplers[i], EShaderStage_AllWithoutCompute);
			}
			
			pResources->SetSampler(8, EDefaultSamplerStates::PointWrap, EShaderStage_AllWithoutCompute);
			pResources->SetSampler(9, EDefaultSamplerStates::PointClamp, EShaderStage_AllWithoutCompute);
			
			// Custom for pass
			pResources->SetSampler(10, EDefaultSamplerStates::BilinearWrap, EShaderStage_AllWithoutCompute);
			pResources->SetSampler(11, EDefaultSamplerStates::LinearCompare, EShaderStage_AllWithoutCompute);
		}

		// Textures
		{
			int nTerrainTex0 = 0, nTerrainTex1 = 0, nTerrainTex2 = 0;
			if (gEnv->p3DEngine && gEnv->p3DEngine->GetITerrain())
				gEnv->p3DEngine->GetITerrain()->GetAtlasTexId(nTerrainTex0, nTerrainTex1, nTerrainTex2);

			pResources->SetTexture(ePerPassTexture_PerlinNoiseMap, CTexture::s_ptexPerlinNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_WindGrid, CTexture::s_ptexWindGrid, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_TerrainElevMap, CTexture::GetByID(nTerrainTex2), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_TerrainNormMap, CTexture::GetByID(nTerrainTex1), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_TerrainBaseMap, CTexture::GetByID(nTerrainTex0), EDefaultResourceViews::sRGB, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_NormalsFitting, CTexture::s_ptexNormalsFitting, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_DissolveNoise, CTexture::s_ptexDissolveNoiseMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(ePerPassTexture_SceneLinearDepth, CTexture::s_ptexZTarget, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

			pResources->SetTexture(38, pShadowMask, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(39, CTexture::s_ptexNoise3D, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(40, CTexture::s_ptexEnvironmentBRDF, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(45, (pRenderView && pRenderView->IsRecursive()) ? CTexture::s_ptexBlackCM : CTexture::s_ptexDefaultProbeCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}

		// Particle resources
		{
			const CParticleBufferSet& particleBuffer = pRenderer->m_RP.m_particleBuffer;
			const CLightVolumeBuffer& lightVolumes = pRenderer->m_RP.m_lightVolumeBuffer;
			pResources->SetBuffer(
				EReservedTextureSlot_LightvolumeInfos,
				const_cast<CGpuBuffer*>(&lightVolumes.GetLightInfosBuffer()),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetBuffer(
				EReservedTextureSlot_LightVolumeRanges,
				const_cast<CGpuBuffer*>(&lightVolumes.GetLightRangesBuffer()),
				EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			if (bOnInit)
			{
				pResources->SetBuffer(
					EReservedTextureSlot_ParticlePositionStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetBuffer(
					EReservedTextureSlot_ParticleAxesStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetBuffer(
					EReservedTextureSlot_ParticleColorSTStream,
					CDeviceBufferManager::GetNullBufferStructured(),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
			else
			{
				pResources->SetBuffer(
					EReservedTextureSlot_ParticlePositionStream,
					const_cast<CGpuBuffer*>(&particleBuffer.GetPositionStream()),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetBuffer(
					EReservedTextureSlot_ParticleAxesStream,
					const_cast<CGpuBuffer*>(&particleBuffer.GetAxesStream()),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetBuffer(
					EReservedTextureSlot_ParticleColorSTStream,
					const_cast<CGpuBuffer*>(&particleBuffer.GetColorSTsStream()),
					EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
		}

		// Tiled shading resources
		{
			CTiledShading& tiledShading = pRenderer->GetTiledShading();
			pResources->SetBuffer(17,  &tiledShading.m_tileOpaqueLightMaskBuf, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetBuffer(18,  &tiledShading.m_lightShadeInfoBuf, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetBuffer(19,  &tiledShading.m_clipVolumeInfoBuf, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(20, tiledShading.m_specularProbeAtlas.texArray, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(21, tiledShading.m_diffuseProbeAtlas.texArray, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(22, tiledShading.m_spotTexAtlas.texArray, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(23, CTexture::s_ptexRT_ShadowPool, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(24, CTexture::s_ptexSceneNormalsBent, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			pResources->SetTexture(41, CTexture::s_ptexSceneDiffuse, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);  //  Eye AO overlay

			if (CRenderer::CV_r_DeferredShadingTiled < 3)
			{
				pResources->SetBuffer(17, &tiledShading.m_tileTranspLightMaskBuf, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}

			// Overwrite resources for transparent pass (need to be careful that the layout is still the same)
			if (bTransparentPass)
			{
				pResources->SetBuffer(17, &tiledShading.m_tileTranspLightMaskBuf, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(24, CTexture::s_ptexShadowJitterMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				CShadowUtils::SShadowCascades cascades;
				if (bOnInit)
				{
					std::fill(std::begin(cascades.pShadowMap), std::end(cascades.pShadowMap), CTexture::s_ptexFarPlane);
				}
				else
				{
					CShadowUtils::GetShadowCascades(cascades, RenderView());
				}

				pResources->SetTexture(25, CTexture::s_ptexCurrSceneTarget, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(26, cascades.pShadowMap[0], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(27, cascades.pShadowMap[1], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(28, cascades.pShadowMap[2], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(29, cascades.pShadowMap[3], EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(30, CTexture::s_ptexWhite, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				pResources->SetTexture(31, CTexture::s_ptexShadowJitterMap, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);

				// volumetric fog supports only general pass currently so only transparent pass needs those textures.
				auto* pVolFogStage = pRenderer->GetGraphicsPipeline().GetVolumetricFogStage();
				if (bOnInit || !pVolFogStage || !bFog)
				{
					pResources->SetTexture(42, CTexture::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					pResources->SetTexture(43, CTexture::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					pResources->SetTexture(44, CTexture::s_ptexBlackCM, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
				else
				{
					pResources->SetTexture(42, pVolFogStage->GetVolumetricFogTex(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					pResources->SetTexture(43, pVolFogStage->GetGlobalEnvProbeTex0(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
					pResources->SetTexture(44, pVolFogStage->GetGlobalEnvProbeTex1(), EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
				}
			}
			else if (bEyeOverlayPass)
			{
				// Eye AO overlay pass resource must not contain eye AO overlay texture.
				pResources->SetTexture(41, CTexture::s_ptexBlack, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
			}
		}

		// Constant buffers
		{
			if (pRenderView)
			{
				CryStackAllocWithSize(SPerPassConstantBuffer, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

				CShadowUtils::GetShadowCascadesSamplingInfo(cb->cbShadowSampling, pRenderView);
				pRenderer->GetGraphicsPipeline().GetFogStage()->FillForwardParams(cb->cbFog, bFog);
				pRenderer->GetGraphicsPipeline().GetVolumetricFogStage()->FillForwardParams(cb->cbVoxelFog, bFog);

				SCGParamsPF& paramsPF = pRenderer->m_cEF.m_PF[pRenderer->m_RP.m_nProcessThreadID];
				cb->cbMisc.CloudShadingColorSun = Vec4(paramsPF.pCloudShadingColorSun, 0);
				cb->cbMisc.CloudShadingColorSky = Vec4(paramsPF.pCloudShadingColorSky, 0);

				CRY_ASSERT(m_pPerPassCB);
				m_pPerPassCB->UpdateBuffer(cb, cbSize);
			}

			pResources->SetConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassCB, EShaderStage_AllWithoutCompute);

			CConstantBufferPtr pPerViewCB;
			if (bOnInit)  // Handle case when no view is available in the initialization of the stage
				pPerViewCB = CDeviceBufferManager::GetNullConstantBuffer();
			else
				pPerViewCB = pRenderer->GetGraphicsPipeline().GetMainViewConstantBuffer();

			pResources->SetConstantBuffer(eConstantBufferShaderSlot_PerView, pPerViewCB, EShaderStage_AllWithoutCompute);
		}

		if (bOnInit)
			continue;

		CRY_ASSERT(!pResources->HasChangedBindPoints()); // Cannot change resource layout after init. It is baked into the shaders
		pResourceSet->Update(*pResources);
	}

	return bOnInit || (m_pOpaquePassResourceSet->IsValid() && m_pTransparentPassResourceSet->IsValid());
}

void CSceneForwardStage::Execute_Opaque()
{
	PROFILE_LABEL_SCOPE("FORWARD_OPAQUE");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	SThreadInfo* const pThreadInfo = &(pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID]);

	if (pRenderer->m_nGraphicsPipeline >= 3)
	{
		CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

		D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
		pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

		CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
		if (pThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
			passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;

		PreparePerPassResources(pRenderView, false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

		m_forwardEyeOverlayPass.PrepareRenderPassForUse(commandList);
		m_forwardEyeOverlayPass.SetFlags(passFlags);
		m_forwardEyeOverlayPass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardEyeOverlayPass.BeginExecution();
		m_forwardEyeOverlayPass.DrawRenderItems(pRenderView, EFSLIST_EYE_OVERLAY);
		m_forwardEyeOverlayPass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();

		m_forwardOpaquePass.PrepareRenderPassForUse(commandList);
		m_forwardOpaquePass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_VrProjectionPass | CSceneRenderPass::ePassFlags_RenderNearest);
		m_forwardOpaquePass.SetViewport(viewport);

		m_forwardOverlayPass.SetupPassContext(m_stageID, ePass_Forward, TTYPE_GENERAL, FB_TILED_FORWARD | FB_GENERAL, EFSLIST_DECAL, CRenderer::CV_r_DeferredShadingTiled != 4 ? FB_Z : 0);
		m_forwardOverlayPass.PrepareRenderPassForUse(commandList);
		m_forwardOverlayPass.SetFlags(passFlags);
		m_forwardOverlayPass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardOpaquePass.BeginExecution();

		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST);
		if (CRenderer::CV_r_DeferredShadingTiled == 4)
		{
			m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_GENERAL);
		}
		m_forwardOpaquePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE);
		m_forwardOpaquePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();

		Execute_SkyPass();

		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardOverlayPass.BeginExecution();
		if (CRenderer::CV_r_DeferredShadingTiled == 4)
		{
			m_forwardOverlayPass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		}
		m_forwardOverlayPass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_forwardOverlayPass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	else // Legacy pipeline
	{
		pRenderer->GetGraphicsPipeline().SwitchToLegacyPipeline();

		void (* pRenderFunc)() = &pRenderer->FX_FlushShader_General;
		pRenderer->m_RP.m_PersFlags2 |= RBPF2_FORWARD_SHADING_PASS;

		// Note: Eye overlay writes to diffuse color buffer for eye shader reading
		pRenderer->FX_ProcessEyeOverlayRenderLists(EFSLIST_EYE_OVERLAY, pRenderFunc, true);

		{
			PROFILE_LABEL_SCOPE("FORWARD_OPAQUE");
			pRenderer->GetTiledShading().BindForwardShadingResources(NULL);
			pRenderer->FX_ProcessRenderList(EFSLIST_FORWARD_OPAQUE_NEAREST, pRenderFunc, true, FB_GENERAL, FB_TILED_FORWARD);
			pRenderer->FX_ProcessRenderList(EFSLIST_FORWARD_OPAQUE, pRenderFunc, true, FB_GENERAL, FB_TILED_FORWARD);
			pRenderer->GetTiledShading().UnbindForwardShadingResources();
		}

		{
			PROFILE_LABEL_SCOPE("TERRAINLAYERS");
			pRenderer->FX_ProcessRenderList(EFSLIST_TERRAINLAYER, pRenderFunc, true);
		}

		{
			PROFILE_LABEL_SCOPE("FORWARD_DECALS");
			pRenderer->FX_ProcessRenderList(EFSLIST_DECAL, pRenderFunc, true);
		}

		pRenderer->FX_ProcessSkinRenderLists(EFSLIST_SKIN, pRenderFunc, true);

		pRenderer->m_RP.m_PersFlags2 &= ~RBPF2_FORWARD_SHADING_PASS;

		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	}
#endif
}

void CSceneForwardStage::Execute_Transparent(bool bBelowWater)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	SThreadInfo* const pThreadInfo = &(pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID]);

	const bool bItemsBelowWater = !SRendItem::IsListEmpty(EFSLIST_TRANSP) && (SRendItem::BatchFlags(EFSLIST_TRANSP) & FB_BELOW_WATER);
	const bool bItemsBelowWaterNearest = !SRendItem::IsListEmpty(EFSLIST_TRANSP_NEAREST) && (SRendItem::BatchFlags(EFSLIST_TRANSP_NEAREST) & FB_BELOW_WATER);
	if(bItemsBelowWater || bItemsBelowWaterNearest || !bBelowWater)
	{
		CStretchRectPass& copyPass = bBelowWater ? m_copySceneTargetBWPass : m_copySceneTargetAWPass;
		copyPass.Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexCurrSceneTarget);
	}

	if (pRenderer->m_nGraphicsPipeline >= 3)
	{
		CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

		D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
		pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

		CSceneRenderPass::EPassFlags passFlags = CSceneRenderPass::ePassFlags_None;
		if (pThreadInfo->m_PersFlags & RBPF_REVERSE_DEPTH)
			passFlags |= CSceneRenderPass::ePassFlags_ReverseDepth;

		PreparePerPassResources(pRenderView, false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		CSceneRenderPass& scenePass = bBelowWater ? m_forwardTransparentBWPass : m_forwardTransparentAWPass;

		scenePass.PrepareRenderPassForUse(commandList);
		scenePass.SetFlags(passFlags | CSceneRenderPass::ePassFlags_RenderNearest);
		scenePass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		scenePass.BeginExecution();

		scenePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP);
		scenePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP_NEAREST);

		scenePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	else
	{
		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();

		void (* pRenderFunc)() = &pRenderer->FX_FlushShader_General;

		pRenderer->GetTiledShading().BindForwardShadingResources(NULL);

		if (bBelowWater)
			pRenderer->FX_ProcessRenderList(EFSLIST_TRANSP, pRenderFunc, true, FB_BELOW_WATER, 0);
		else
			pRenderer->FX_ProcessRenderList(EFSLIST_TRANSP, pRenderFunc, true, FB_GENERAL, FB_BELOW_WATER);

		pRenderer->GetTiledShading().UnbindForwardShadingResources();

		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	}
#endif
}

void CSceneForwardStage::Execute_TransparentBelowWater()
{
	PROFILE_LABEL_SCOPE("TRANSPARENT_BW");

	Execute_Transparent(true);
}

void CSceneForwardStage::Execute_TransparentAboveWater()
{
	PROFILE_LABEL_SCOPE("TRANSPARENT_AW");

	Execute_Transparent(false);
}

void CSceneForwardStage::Execute_TransparentDepthFixup()
{
	PROFILE_LABEL_SCOPE("MERGE_DEPTH");

	CTexture* pSrcRT = CTexture::s_ptexHDRTarget;
	CTexture* pDestRT =  CTexture::s_ptexZTarget;

	if (!m_depthFixupPass.InputChanged(pSrcRT->GetTextureID(), pDestRT->GetTextureID()))
	{
		m_depthFixupPass.Execute();
		return;
	}

	static CCryNameTSCRC techName("TranspDepthFixupMerge");

	uint64 rtMask = 0;

	m_depthFixupPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
	m_depthFixupPass.SetRenderTarget(0, pDestRT);
	m_depthFixupPass.SetTechnique(CShaderMan::s_shPostEffects, techName, rtMask);
	m_depthFixupPass.SetState(GS_NODEPTHTEST | GS_BLSRC_ONE | GS_BLDST_ONE | GS_BLEND_OP_MIN);
	m_depthFixupPass.SetRequirePerViewConstantBuffer(true);
	m_depthFixupPass.SetTextureSamplerPair(0, pSrcRT, EDefaultSamplerStates::PointClamp);
	m_depthFixupPass.BeginConstantUpdate();
	m_depthFixupPass.Execute();
}

void CSceneForwardStage::Execute_TransparentLoRes(int subRes)
{
	PROFILE_LABEL_SCOPE("TRANSPARENT_SUBRES");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	const bool bAlphaBased = pRenderer->CV_r_ParticlesHalfResBlendMode == 0;

	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

	if (pRenderView->GetRenderItems(EFSLIST_HALFRES_PARTICLES).empty())
		return;

	if (pRenderer->m_nGraphicsPipeline >= 3)
	{
		CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

		D3DViewPort viewport = { 0.f, 0.f, float(CTexture::s_ptexHDRTargetScaled[subRes]->GetWidth()), float(CTexture::s_ptexHDRTargetScaled[subRes]->GetHeight()), 0.0f, 1.0f };
		pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

		PreparePerPassResources(pRenderView, false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
		CSceneRenderPass& scenePass = m_forwardTransparentLoResPass;//[subRes];

		scenePass.ExchangeRenderTarget(0, CTexture::s_ptexHDRTargetScaled[subRes]);

		scenePass.PrepareRenderPassForUse(commandList);
		scenePass.SetFlags(CSceneRenderPass::ePassFlags_None);
		scenePass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		scenePass.BeginExecution();
		scenePass.DrawRenderItems(pRenderView, EFSLIST_HALFRES_PARTICLES);
		scenePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();

		m_depthUpscalePass.Execute(
			CTexture::s_ptexZTarget, // TODO: Scaled[subRes - 1]
			CTexture::s_ptexHDRTargetScaled[subRes],
			CTexture::s_ptexZTargetScaled[subRes],
			gcpRendD3D->GetCurrentTargetOutput(), // TODO: Scaled[subRes - 1]
			bAlphaBased
		);
	}
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	else
	{
		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();

		void(*pRenderFunc)() = &pRenderer->FX_FlushShader_General;

		pRenderer->GetTiledShading().BindForwardShadingResources(NULL);

		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
		pRenderer->FX_ProcessRenderList(EFSLIST_HALFRES_PARTICLES, pRenderFunc, true);
		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();

		pRenderer->GetTiledShading().UnbindForwardShadingResources();

		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	}
#endif
}

void CSceneForwardStage::Execute_AfterPostProcessHDR()
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

	if (pRenderView->GetRenderItems(EFSLIST_AFTER_HDRPOSTPROCESS).empty())
		return;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	if (pRenderer->m_nGraphicsPipeline >= 3)
	{
		m_forwardHDRPass.ExchangeRenderTarget(0, CTexture::s_ptexSceneDiffuse);

		D3DViewPort viewport = { 0.f, 0.f, float(gcpRendD3D->m_MainViewport.nWidth), float(gcpRendD3D->m_MainViewport.nHeight), 0.0f, 1.0f };
		PreparePerPassResources(pRenderView, false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

		m_forwardHDRPass.PrepareRenderPassForUse(commandList);
		m_forwardHDRPass.SetFlags(CSceneRenderPass::ePassFlags_None);
		m_forwardHDRPass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardHDRPass.BeginExecution();
		m_forwardHDRPass.DrawRenderItems(pRenderView, EFSLIST_AFTER_HDRPOSTPROCESS);
		m_forwardHDRPass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	else
	{
		void(*pRenderFunc)() = &pRenderer->FX_FlushShader_General;

		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
		pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, pRenderFunc, false);
		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	}
#endif
}

void CSceneForwardStage::Execute_AfterPostProcessLDR()
{
	CRenderView* pRenderView = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();

	if (pRenderView->GetRenderItems(EFSLIST_AFTER_POSTPROCESS).empty())
		return;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	if (pRenderer->m_nGraphicsPipeline >= 3)
	{
		m_forwardLDRPass.ExchangeRenderTarget(0, gcpRendD3D->GetCurrentTargetOutput());
	
		D3DViewPort viewport = { 0.f, 0.f, float(gcpRendD3D->m_MainViewport.nWidth), float(gcpRendD3D->m_MainViewport.nHeight), 0.0f, 1.0f };
		PreparePerPassResources(pRenderView, false);

		auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();
	
		m_forwardLDRPass.PrepareRenderPassForUse(commandList);
		m_forwardLDRPass.SetFlags(CSceneRenderPass::ePassFlags_None);
		m_forwardLDRPass.SetViewport(viewport);

		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardLDRPass.BeginExecution();
		m_forwardLDRPass.DrawRenderItems(pRenderView, EFSLIST_AFTER_POSTPROCESS);
		m_forwardLDRPass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	else
	{
		void(*pRenderFunc)() = &pRenderer->FX_FlushShader_General;

		gcpRendD3D->GetGraphicsPipeline().SwitchToLegacyPipeline();
		pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, pRenderFunc, false);
		gcpRendD3D->GetGraphicsPipeline().SwitchFromLegacyPipeline();
	}
#endif
}

void CSceneForwardStage::Execute_Minimum()
{
	PROFILE_LABEL_SCOPE("FORWARD_MINIMUM");

	CD3D9Renderer* pRenderer = gcpRendD3D;
	const SRenderPipeline& rp(gcpRendD3D->m_RP);
	const SThreadInfo* const pShaderThreadInfo = &(rp.m_TI[rp.m_nProcessThreadID]);

	CRenderView* pRenderView = RenderView();
	CRY_ASSERT(pRenderer->m_nGraphicsPipeline == 3);

	CTexture* pTargetTex = CTexture::s_ptexHDRTarget;
	CTexture* pDepthTex = gcpRendD3D->m_pZTexture;
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();
	if (pOutput)
	{
		pTargetTex = pOutput->GetHDRTargetTexture();
		pDepthTex = pOutput->GetDepthTexture();
	}
	else
	{
		CRY_ASSERT(!(pRenderView->IsRecursive()));
		CRY_ASSERT((pRenderer->m_RP.m_nRendFlags & SHDF_SECONDARY_VIEWPORT) == 0);
	}

	D3DViewPort viewport = { 0.f, 0.f, float(pRenderer->m_MainViewport.nWidth), float(pRenderer->m_MainViewport.nHeight), 0.0f, 1.0f };
	if (pRenderView->IsRecursive())
	{
		viewport = { 0.f, 0.f, float(pTargetTex->GetWidth()), float(pTargetTex->GetHeight()), 0.0f, 1.0f };
	}
	pRenderer->RT_SetViewport(0, 0, int(viewport.Width), int(viewport.Height));

	CRY_ASSERT(pTargetTex && CTexture::s_ptexHDRTarget->GetTextureDstFormat() == pTargetTex->GetTextureDstFormat());
	CRY_ASSERT(pDepthTex && gcpRendD3D->m_pZTexture->GetTextureDstFormat() == pDepthTex->GetTextureDstFormat());

	m_forwardOpaqueRecursivePass.ExchangeRenderTarget(0, pTargetTex);
	m_forwardOpaqueRecursivePass.ExchangeDepthTarget(pDepthTex);

	m_forwardOverlayRecursivePass.ExchangeRenderTarget(0, pTargetTex);
	m_forwardOverlayRecursivePass.ExchangeDepthTarget(pDepthTex);

	m_forwardTransparentRecursivePass.ExchangeRenderTarget(0, pTargetTex);
	m_forwardTransparentRecursivePass.ExchangeDepthTarget(pDepthTex);

	const bool bShadowMask = false;
	const bool bFog = pShaderThreadInfo->m_FS.m_bEnable;
	PreparePerPassResources(pRenderView, false, bShadowMask, bFog);

	auto& RESTRICT_REFERENCE commandList = GetDeviceObjectFactory().GetCoreCommandList();

	m_forwardOpaqueRecursivePass.PrepareRenderPassForUse(commandList);
	m_forwardOpaqueRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_forwardOpaqueRecursivePass.SetViewport(viewport);

	m_forwardOverlayRecursivePass.PrepareRenderPassForUse(commandList);
	m_forwardOverlayRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_forwardOverlayRecursivePass.SetViewport(viewport);

	m_forwardTransparentRecursivePass.PrepareRenderPassForUse(commandList);
	m_forwardTransparentRecursivePass.SetFlags(CSceneRenderPass::ePassFlags_None);
	m_forwardTransparentRecursivePass.SetViewport(viewport);

	{
		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardOpaqueRecursivePass.BeginExecution();
		m_forwardOpaqueRecursivePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE_NEAREST);
		m_forwardOpaqueRecursivePass.DrawRenderItems(pRenderView, EFSLIST_FORWARD_OPAQUE);
		m_forwardOpaqueRecursivePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}

	Execute_SkyPass();

	{
		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardOverlayRecursivePass.BeginExecution();
		m_forwardOverlayRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TERRAINLAYER);
		m_forwardOverlayRecursivePass.DrawRenderItems(pRenderView, EFSLIST_DECAL);
		m_forwardOverlayRecursivePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}

	{
		RenderView()->GetDrawer().InitDrawSubmission();

		m_forwardTransparentRecursivePass.BeginExecution();
		m_forwardTransparentRecursivePass.DrawRenderItems(pRenderView, EFSLIST_TRANSP);
		m_forwardTransparentRecursivePass.EndExecution();

		RenderView()->GetDrawer().JobifyDrawSubmission();
		RenderView()->GetDrawer().WaitForDrawSubmission();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sky
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSceneForwardStage::SetSkyRE(CRESky* pSkyRE, CREHDRSky* pHDRSkyRE)
{
	m_pSkyRE = pSkyRE;
	m_pHDRSkyRE = pHDRSkyRE;
}

void CSceneForwardStage::SetupHDRSkyParameters()
{
	if (!m_pHDRSkyRE)
		return;

	I3DEngine* p3DEngine = gEnv->p3DEngine;

	// Day sky
	{
		static CCryNameR mieName("SkyDome_PartialMieInScatteringConst");
		static CCryNameR rayleighName("SkyDome_PartialRayleighInScatteringConst");
		static CCryNameR sunDirectionName("SkyDome_SunDirection");
		static CCryNameR phaseName("SkyDome_PhaseFunctionConstants");

		auto renderParams = m_pHDRSkyRE->m_pRenderParams;
		m_skyPass.SetConstant(mieName, renderParams->m_partialMieInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(rayleighName, renderParams->m_partialRayleighInScatteringConst, eHWSC_Pixel);
		m_skyPass.SetConstant(sunDirectionName, renderParams->m_sunDirection, eHWSC_Pixel);
		m_skyPass.SetConstant(phaseName, renderParams->m_phaseFunctionConsts, eHWSC_Pixel);
	}

	// Night sky
	{
		static CCryNameR nightColBaseName("SkyDome_NightSkyColBase");
		static CCryNameR nightColDeltaName("SkyDome_NightSkyColDelta");
		static CCryNameR nightZenithColShiftName("SkyDome_NightSkyZenithColShift");

		Vec3 nightSkyHorizonCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_HORIZON_COLOR, nightSkyHorizonCol);
		Vec3 nightSkyZenithCol;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_COLOR, nightSkyZenithCol);
		float nightSkyZenithColShift(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_ZENITH_SHIFT));
		const float minNightSkyZenithGradient(-0.1f);

		Vec4 colBase(nightSkyHorizonCol, 0);
		Vec4 colDelta(nightSkyZenithCol - nightSkyHorizonCol, 0);
		Vec4 zenithColShift(1.0f / (nightSkyZenithColShift - minNightSkyZenithGradient), -minNightSkyZenithGradient / (nightSkyZenithColShift - minNightSkyZenithGradient), 0, 0);

		m_skyPass.SetConstant(nightColBaseName, colBase, eHWSC_Pixel);
		m_skyPass.SetConstant(nightColDeltaName, colDelta, eHWSC_Pixel);
		m_skyPass.SetConstant(nightZenithColShiftName, zenithColShift, eHWSC_Pixel);
	}

	// Moon
	{
		static CCryNameR moonTexGenRightName("SkyDome_NightMoonTexGenRight");
		static CCryNameR moonTexGenUpName("SkyDome_NightMoonTexGenUp");
		static CCryNameR moonDirSizeName("SkyDome_NightMoonDirSize");
		static CCryNameR moonColorName("SkyDome_NightMoonColor");
		static CCryNameR moonInnerScaleName("SkyDome_NightMoonInnerCoronaColorScale");
		static CCryNameR moonOuterScaleName("SkyDome_NightMoonOuterCoronaColorScale");

		Vec3 nightMoonColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_COLOR, nightMoonColor);
		Vec4 moonColor(nightMoonColor, 0);

		Vec3 nightMoonInnerCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_COLOR, nightMoonInnerCoronaColor);
		float nightMoonInnerCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_INNERCORONA_SCALE));
		Vec4 moonInnerCoronaColorScale(nightMoonInnerCoronaColor, nightMoonInnerCoronaScale);

		Vec3 nightMoonOuterCoronaColor;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_COLOR, nightMoonOuterCoronaColor);
		float nightMoonOuterCoronaScale(1.0f + 1000.0f * p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_OUTERCORONA_SCALE));
		Vec4 moonOuterCoronaColorScale(nightMoonOuterCoronaColor, nightMoonOuterCoronaScale);

		m_skyPass.SetConstant(moonColorName, moonColor, eHWSC_Pixel);
		m_skyPass.SetConstant(moonInnerScaleName, moonInnerCoronaColorScale, eHWSC_Pixel);
		m_skyPass.SetConstant(moonOuterScaleName, moonOuterCoronaColorScale, eHWSC_Pixel);

		Vec3 mr;
		p3DEngine->GetGlobalParameter(E3DPARAM_SKY_MOONROTATION, mr);
		float moonLati = -gf_PI + gf_PI * mr.x / 180.0f;
		float moonLong = 0.5f * gf_PI - gf_PI * mr.y / 180.0f;

		float sinLonR = sinf(-0.5f * gf_PI);
		float cosLonR = cosf(-0.5f * gf_PI);
		float sinLatR = sinf(moonLati + 0.5f * gf_PI);
		float cosLatR = cosf(moonLati + 0.5f * gf_PI);
		Vec3 moonTexGenRight(sinLonR * cosLatR, sinLonR * sinLatR, cosLonR);
		m_paramMoonTexGenRight = Vec4(moonTexGenRight, 0);
		m_skyPass.SetConstant(moonTexGenRightName, m_paramMoonTexGenRight, eHWSC_Pixel);

		float sinLonU = sinf(moonLong + 0.5f * gf_PI);
		float cosLonU = cosf(moonLong + 0.5f * gf_PI);
		float sinLatU = sinf(moonLati);
		float cosLatU = cosf(moonLati);
		Vec3 moonTexGenUp(sinLonU * cosLatU, sinLonU * sinLatU, cosLonU);
		m_paramMoonTexGenUp = Vec4(moonTexGenUp, 0);
		m_skyPass.SetConstant(moonTexGenUpName, m_paramMoonTexGenUp, eHWSC_Pixel);

		Vec3 nightMoonDirection;
		p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_DIRECTION, nightMoonDirection);
		float nightMoonSize(25.0f - 24.0f * clamp_tpl(p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_MOON_SIZE), 0.0f, 1.0f));
		m_paramMoonDirSize = Vec4(nightMoonDirection, true ? nightMoonSize : 9999.0f);
		m_skyPass.SetConstant(moonDirSizeName, m_paramMoonDirSize, eHWSC_Pixel);
	}
}

static void FillSkyTextureData(CTexture* pTexture, const void* pData, const uint32 width, const uint32 height, const uint32 pitch)
{
	assert(pTexture && pTexture->GetWidth() == width && pTexture->GetHeight() == height && pTexture->GetDevTexture());
	const SResourceMemoryAlignment layout = SResourceMemoryAlignment::Linear<CryHalf4>(width, height);
	CRY_ASSERT(pitch == layout.rowStride);

	GPUPIN_DEVICE_TEXTURE(gcpRendD3D->GetPerformanceDeviceContext(), pTexture->GetDevTexture());
	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pData, pTexture->GetDevTexture(), layout);
}

void CSceneForwardStage::Execute_SkyPass()
{
	if (!m_pHDRSkyRE && !m_pSkyRE)
		return;

	PROFILE_LABEL_SCOPE("SKY_PASS");

	CTexture* pSkyDomeTex = CTexture::s_ptexBlack;

	// Update sky dome texture if new data is available
	if (m_pHDRSkyRE)
	{
		if (m_pHDRSkyRE->m_skyDomeTextureLastTimeStamp != m_pHDRSkyRE->m_pRenderParams->m_skyDomeTextureTimeStamp)
		{
			FillSkyTextureData(m_pHDRSkyRE->m_pSkyDomeTextureMie, m_pHDRSkyRE->m_pRenderParams->m_pSkyDomeTextureDataMie,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pHDRSkyRE->m_pRenderParams->m_skyDomeTexturePitch);
			FillSkyTextureData(m_pHDRSkyRE->m_pSkyDomeTextureRayleigh, m_pHDRSkyRE->m_pRenderParams->m_pSkyDomeTextureDataRayleigh,
			                   SSkyLightRenderParams::skyDomeTextureWidth, SSkyLightRenderParams::skyDomeTextureHeight, m_pHDRSkyRE->m_pRenderParams->m_skyDomeTexturePitch);

			// Update time stamp of last update
			m_pHDRSkyRE->m_skyDomeTextureLastTimeStamp = m_pHDRSkyRE->m_pRenderParams->m_skyDomeTextureTimeStamp;
		}
	}
	else if (m_pSkyRE)
	{
		IMaterial* skyMat = gEnv->p3DEngine->GetSkyMaterial();
		if (skyMat && skyMat->GetShaderItem().m_pShaderResources)
		{
			auto pSkyTexInfo = ((CShaderResources*)skyMat->GetShaderItem().m_pShaderResources)->m_Textures[EFTT_DIFFUSE];
			if (pSkyTexInfo)
				pSkyDomeTex = CTexture::ForName(pSkyTexInfo->m_Name, 0, eTF_Unknown);
		}
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
		pTargetTex = CTexture::s_ptexHDRTarget;
		pDepthTex = gcpRendD3D->m_pZTexture;
	}

	D3DViewPort viewport = { 0.f, 0.f, float(gcpRendD3D->m_MainViewport.nWidth), float(gcpRendD3D->m_MainViewport.nHeight), 0.0f, 1.0f };
	if (pRenderView->IsRecursive())
	{
		viewport = { 0.f, 0.f, float(pTargetTex->GetWidth()), float(pTargetTex->GetHeight()), 0.0f, 1.0f };
	}

	const SThreadInfo& ti = gcpRendD3D->m_RP.m_TI[gcpRendD3D->m_RP.m_nProcessThreadID];
	const bool bFog = (ti.m_FS.m_bEnable && !(gcpRendD3D->m_RP.m_PersFlags2 & RBPF2_NOSHADERFOG));

	//if (m_skyPass.InputChanged())
	{
		SSamplerState      samplerDescLinearWrapU(FILTER_LINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0);
		SamplerStateHandle samplerStateLinearWrapU = GetDeviceObjectFactory().GetOrCreateSamplerStateHandle(samplerDescLinearWrapU);

		uint64 rtMask = 0;
		rtMask |= m_pSkyRE ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
		rtMask |= bFog ? g_HWSR_MaskBit[HWSR_FOG] : 0;

		static CCryNameTSCRC techSkyPass("SkyPass");
		m_skyPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_skyPass.SetTechnique(CShaderMan::s_ShaderStars, techSkyPass, rtMask);
		m_skyPass.SetRequirePerViewConstantBuffer(true);
		m_skyPass.SetRenderTarget(0, pTargetTex);
		m_skyPass.SetDepthTarget(pDepthTex);
		m_skyPass.SetViewport(viewport);
		m_skyPass.SetState(GS_DEPTHFUNC_EQUAL);
		m_skyPass.SetTextureSamplerPair(0, pSkyDomeTex, EDefaultSamplerStates::LinearClamp);

		CTexture *pSkyDomeTextureMie = CTexture::s_ptexBlack;
		CTexture *pSkyDomeTextureRayleigh = CTexture::s_ptexBlack;
		CTexture *pSkyMoonTex = CTexture::s_ptexBlack;
		if (m_pHDRSkyRE)
		{
			pSkyDomeTextureMie = m_pHDRSkyRE->m_pSkyDomeTextureMie;
			pSkyDomeTextureRayleigh = m_pHDRSkyRE->m_pSkyDomeTextureRayleigh;
			if (m_pHDRSkyRE->m_moonTexId > 0)
			{
				pSkyMoonTex = CTexture::GetByID(m_pHDRSkyRE->m_moonTexId);
			}
		}

		m_skyPass.SetTextureSamplerPair(1, pSkyDomeTextureMie, samplerStateLinearWrapU);
		m_skyPass.SetTextureSamplerPair(2, pSkyDomeTextureRayleigh, samplerStateLinearWrapU);
		m_skyPass.SetTextureSamplerPair(3, pSkyMoonTex, EDefaultSamplerStates::LinearClamp);

		m_pSkyDomeTextureMie = pSkyDomeTextureMie;
		m_pSkyDomeTextureRayleigh = pSkyDomeTextureRayleigh;
		m_pSkyMoonTex = pSkyMoonTex;
	}

	m_skyPass.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerPass, m_pPerPassCB, EShaderStage_AllWithoutCompute);
	m_skyPass.BeginConstantUpdate();
	SetupHDRSkyParameters();
	m_skyPass.Execute();

	// Stars
	float starIntensity = gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_NIGHSKY_STAR_INTENSITY);
	if (m_pHDRSkyRE && m_pHDRSkyRE->m_pStars && m_pHDRSkyRE->m_pStars->m_pStarMesh && starIntensity > 1e-3f)
	{
		m_starsPass.SetRenderTarget(0, pTargetTex);
		m_starsPass.SetDepthTarget(pDepthTex);
		m_starsPass.SetViewport(viewport);
		m_starsPass.BeginAddingPrimitives();

		const bool bReverseDepth = (ti.m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
		const uint64 rtMask = bReverseDepth ? g_HWSR_MaskBit[HWSR_REVERSE_DEPTH] : 0;
		const int32 depthState = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;

		static CCryNameTSCRC techStars("Stars");
		m_starsPrimitive.SetFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);
		m_starsPrimitive.SetTechnique(CShaderMan::s_ShaderStars, techStars, rtMask);
		m_starsPrimitive.SetRenderState(depthState | GS_BLSRC_ONE | GS_BLDST_ONE);
		m_starsPrimitive.SetCullMode(eCULL_None);

		CRenderMesh* pStarMesh = static_cast<CRenderMesh*>(m_pHDRSkyRE->m_pStars->m_pStarMesh.get());
		buffer_handle_t hVertexStream = pStarMesh->_GetVBStream(VSF_GENERAL);

		m_starsPrimitive.SetCustomVertexStream(hVertexStream, pStarMesh->_GetVertexFormat(), pStarMesh->GetStreamStride(VSF_GENERAL));
		m_starsPrimitive.SetDrawInfo(eptTriangleList, 0, 0, 6 * m_pHDRSkyRE->m_pStars->m_numStars);
		m_starsPrimitive.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, gcpRendD3D->GetGraphicsPipeline().GetMainViewConstantBuffer(), EShaderStage_Vertex);
		m_starsPrimitive.Compile(m_starsPass);

		{
			static CCryNameR nameMoonTexGenRight("SkyDome_NightMoonTexGenRight");
			static CCryNameR nameMoonTexGenUp("SkyDome_NightMoonTexGenUp");
			static CCryNameR nameMoonDirSize("SkyDome_NightMoonDirSize");
			static CCryNameR nameStarSize("StarSize");
			static CCryNameR nameStarIntensity("StarIntensity");

			m_starsPrimitive.GetConstantManager().BeginNamedConstantUpdate();

			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenRight, m_paramMoonTexGenRight, eHWSC_Vertex);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonTexGenUp, m_paramMoonTexGenUp, eHWSC_Vertex);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameMoonDirSize, m_paramMoonDirSize, eHWSC_Vertex);

			const float size = 5.0f * min(1.f, min(viewport.Width / 1280.f, viewport.Height / 720.f));
			float flickerTime(gEnv->pTimer->GetCurrTime());
			Vec4 paramStarSize(size / (float)viewport.Width, size / (float)viewport.Height, 0, flickerTime * 0.5f);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarSize, paramStarSize, eHWSC_Vertex);
			Vec4 paramStarIntensity(starIntensity * min(1.0f, size), 0, 0, 0);
			m_starsPrimitive.GetConstantManager().SetNamedConstant(nameStarIntensity, paramStarIntensity, eHWSC_Pixel);

			m_starsPrimitive.GetConstantManager().EndNamedConstantUpdate();

			m_starsPass.AddPrimitive(&m_starsPrimitive);
			m_starsPass.Execute();
		}
	}

	m_pSkyRE = nullptr;
	m_pHDRSkyRE = nullptr;
}
