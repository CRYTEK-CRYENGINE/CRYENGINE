// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StandardGraphicsPipeline.h"

#include "ShadowMap.h"
#include "SceneGBuffer.h"
#include "SceneForward.h"
#include "SceneCustom.h"
#include "AutoExposure.h"
#include "Bloom.h"
#include "HeightMapAO.h"
#include "ScreenSpaceObscurance.h"
#include "ScreenSpaceReflections.h"
#include "ScreenSpaceSSS.h"
#include "VolumetricFog.h"
#include "Fog.h"
#include "VolumetricClouds.h"
#include "Water.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "SunShafts.h"
#include "ToneMapping.h"
#include "PostAA.h"
#include "ComputeSkinning.h"
#include "GpuParticles.h"
#include "ClipVolumes.h"
#include "DeferredDecals.h"
#include "ShadowMask.h"
#include "TiledShading.h"
#include "ColorGrading.h"
#include "WaterRipples.h"
#include "LensOptics.h"
#include "PostEffects.h"
#include "Rain.h"
#include "Snow.h"
#include "MobileComposition.h"
#include "OmniCamera.h"
#include "TiledLightVolumes.h"
#include "DebugRenderTargets.h"

#include "DepthReadback.h"
#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "Common/RenderDisplayContext.h"

#include "DriverD3D.h" //for gcpRendD3D

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

CStandardGraphicsPipeline::CStandardGraphicsPipeline()
	: m_defaultMaterialBindPoints()
	, m_defaultDrawExtraRL()
	, m_changedCVars(gEnv->pConsole)
{}

void CStandardGraphicsPipeline::Init()
{
	// default material bind points
	{
		m_defaultMaterialBindPoints.SetConstantBuffer(eConstantBufferShaderSlot_PerMaterial, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_AllWithoutCompute);

		for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
		{
			m_defaultMaterialBindPoints.SetTexture(texType, CRendererResources::s_pTexNULL, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}
	}

	// default extra per instance
	{
		m_defaultDrawExtraRL.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat    , CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex);
		m_defaultDrawExtraRL.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex);
		m_defaultDrawExtraRL.SetConstantBuffer(eConstantBufferShaderSlot_PerGroup    , CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Hull);

		// Deliberately aliasing slots/use-cases here for visibility (e.g. EReservedTextureSlot_ComputeSkinVerts, EReservedTextureSlot_SkinExtraWeights and
		// EReservedTextureSlot_GpuParticleStream). The resource layout will just pick the first.
		m_defaultDrawExtraRL.SetBuffer(EReservedTextureSlot_SkinExtraWeights , CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_defaultDrawExtraRL.SetBuffer(EReservedTextureSlot_ComputeSkinVerts , CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_defaultDrawExtraRL.SetBuffer(EReservedTextureSlot_GpuParticleStream, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, EShaderStage_Vertex);
		m_defaultDrawExtraRL.SetBuffer(EReservedTextureSlot_AdjacencyInfo    , CDeviceBufferManager::GetNullBufferTyped()     , EDefaultResourceViews::Default, EShaderStage_Domain);

		m_pDefaultDrawExtraRS = GetDeviceObjectFactory().CreateResourceSet();
		m_pDefaultDrawExtraRS->Update(m_defaultDrawExtraRL);
	}

	// per view constant buffer
	m_mainViewConstantBuffer.CreateDeviceBuffer();

	// Register scene stages that make use of the global PSO cache
	RegisterSceneStage<CShadowMapStage   , eStage_ShadowMap   >(m_pShadowMapStage);
	RegisterSceneStage<CSceneGBufferStage, eStage_SceneGBuffer>(m_pSceneGBufferStage);
	RegisterSceneStage<CSceneForwardStage, eStage_SceneForward>(m_pSceneForwardStage);
	RegisterSceneStage<CSceneCustomStage , eStage_SceneCustom >(m_pSceneCustomStage);

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CTiledLightVolumesStage     >(m_pTiledLightVolumesStage     , eStage_TiledLightVolumes);
	RegisterStage<CHeightMapAOStage           >(m_pHeightMapAOStage           , eStage_HeightMapAO);
	RegisterStage<CScreenSpaceObscuranceStage >(m_pScreenSpaceObscuranceStage , eStage_ScreenSpaceObscurance);
	RegisterStage<CScreenSpaceReflectionsStage>(m_pScreenSpaceReflectionsStage, eStage_ScreenSpaceReflections);
	RegisterStage<CScreenSpaceSSSStage        >(m_pScreenSpaceSSSStage        , eStage_ScreenSpaceSSS);
	RegisterStage<CVolumetricFogStage         >(m_pVolumetricFogStage         , eStage_VolumetricFog);
	RegisterStage<CFogStage                   >(m_pFogStage                   , eStage_Fog);
	RegisterStage<CVolumetricCloudsStage      >(m_pVolumetricCloudsStage      , eStage_VolumetricClouds);
	RegisterStage<CWaterRipplesStage          >(m_pWaterRipplesStage          , eStage_WaterRipples);
	RegisterStage<CMotionBlurStage            >(m_pMotionBlurStage            , eStage_MotionBlur);
	RegisterStage<CDepthOfFieldStage          >(m_pDepthOfFieldStage          , eStage_DepthOfField);
	RegisterStage<CAutoExposureStage          >(m_pAutoExposureStage          , eStage_AutoExposure);
	RegisterStage<CBloomStage                 >(m_pBloomStage                 , eStage_Bloom);
	RegisterStage<CColorGradingStage          >(m_pColorGradingStage          , eStage_ColorGrading);
	RegisterStage<CToneMappingStage           >(m_pToneMappingStage           , eStage_ToneMapping);
	RegisterStage<CSunShaftsStage             >(m_pSunShaftsStage             , eStage_Sunshafts);
	RegisterStage<CPostAAStage                >(m_pPostAAStage                , eStage_PostAA);
	RegisterStage<CComputeSkinningStage       >(m_pComputeSkinningStage       , eStage_ComputeSkinning);
	RegisterStage<CComputeParticlesStage      >(m_pComputeParticlesStage      , eStage_ComputeParticles);
	RegisterStage<CDeferredDecalsStage        >(m_pDeferredDecalsStage        , eStage_DeferredDecals);
	RegisterStage<CClipVolumesStage           >(m_pClipVolumesStage           , eStage_ClipVolumes);
	RegisterStage<CShadowMaskStage            >(m_pShadowMaskStage            , eStage_ShadowMask);
	RegisterStage<CTiledShadingStage          >(m_pTiledShadingStage          , eStage_TiledShading);
	RegisterStage<CWaterStage                 >(m_pWaterStage                 , eStage_Water); // Has a custom PSO cache like Forward
	RegisterStage<CLensOpticsStage            >(m_pLensOpticsStage            , eStage_LensOptics);
	RegisterStage<CPostEffectStage            >(m_pPostEffectStage            , eStage_PostEffet);
	RegisterStage<CRainStage                  >(m_pRainStage                  , eStage_Rain);
	RegisterStage<CSnowStage                  >(m_pSnowStage                  , eStage_Snow);
	RegisterStage<CDepthReadbackStage         >(m_pDepthReadbackStage         , eStage_DepthReadback);
	RegisterStage<CMobileCompositionStage     >(m_pMobileCompositionStage     , eStage_MobileComposition);
	RegisterStage<COmniCameraStage            >(m_pOmniCameraStage            , eStage_OmniCamera);
	RegisterStage<CDebugRenderTargetsStage    >(m_pDebugRenderTargetsStage    , eStage_DebugRenderTargets);

	// Now init stages
	InitStages();

	// Out-of-pipeline passes for display
	m_DownscalePass.reset(new CDownsamplePass);
	m_UpscalePass  .reset(new CSharpeningUpsamplePass);

	// preallocate light volume buffer
	GetLightVolumeBuffer().Create();
	// preallocate video memory buffer for particles when using the job system
	GetParticleBufferSet().Create(CRenderer::CV_r_ParticleVerticePoolSize, CRenderer::CV_r_ParticleMaxVerticePoolSize);

	m_bInitialized = true;
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::Resize(int renderWidth, int renderHeight)
{
	if (CVrProjectionManager::IsMultiResEnabledStatic())
	{
		CVrProjectionManager::Instance()->Configure(SRenderViewport(0, 0, renderWidth, renderHeight), false);
	}
	
	CGraphicsPipeline::Resize(renderWidth, renderHeight);
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::ShutDown()
{
	m_bInitialized = false;

	CGraphicsPipeline::ShutDown();
	CGraphicsPipeline::SetCurrentRenderView(nullptr);

	m_mainViewConstantBuffer.Clear();
	m_defaultDrawExtraRL.ClearResources();
	m_pDefaultDrawExtraRS.reset();
	m_DownscalePass.reset();
	m_UpscalePass.reset();

	m_particleBuffer.Release();
	m_lightVolumeBuffer.Release();
}

//////////////////////////////////////////////////////////////////////////
void CStandardGraphicsPipeline::Update(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags)
{
	CGraphicsPipeline::SetCurrentRenderView(pRenderView);

	m_numInvalidDrawcalls = 0;
	GenerateMainViewConstantBuffer();

	// Compile shadow renderitems
	if (!pRenderView->IsRecursive() && pRenderView->GetCurrentEye() != CCamera::eEye_Right)
		pRenderView->PrepareShadowViews();

	m_renderingFlags = renderingFlags;
	CGraphicsPipeline::Update(pRenderView, renderingFlags);

	if (!m_changedCVars.GetCVars().empty())
	{
		CGraphicsPipeline::OnCVarsChanged(m_changedCVars);

		m_changedCVars.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStandardGraphicsPipeline::CreatePipelineStates(DevicePipelineStatesArray* pStateArray, SGraphicsPipelineStateDescription stateDesc, CGraphicsPipelineStateLocalCache* pStateCache)
{
	// NOTE: Please update SDeviceObjectHelpers::CheckTessellationSupport when adding new techniques types here.

	bool bFullyCompiled = true;

	// GBuffer
	{
		stateDesc.technique = TTYPE_Z;
		bFullyCompiled &= m_pSceneGBufferStage->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	// ShadowMap
	{
		stateDesc.technique = TTYPE_SHADOWGEN;
		bFullyCompiled &= m_pShadowMapStage->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	// Forward
	{
		stateDesc.technique = TTYPE_GENERAL;
		bFullyCompiled &= m_pSceneForwardStage->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	// Custom
	{
		stateDesc.technique = TTYPE_DEBUG;
		bFullyCompiled &= m_pSceneCustomStage->CreatePipelineStates(pStateArray, stateDesc, pStateCache);
	}

	return bFullyCompiled;
}

void CStandardGraphicsPipeline::ApplyShaderQuality(CDeviceGraphicsPSODesc& psoDesc, const SShaderProfile& shaderProfile)
{
	const uint64 quality = g_HWSR_MaskBit[HWSR_QUALITY];
	const uint64 quality1 = g_HWSR_MaskBit[HWSR_QUALITY1];

	psoDesc.m_ShaderFlags_RT &= ~(quality | quality1);
	switch (psoDesc.m_ShaderQuality = shaderProfile.GetShaderQuality())
	{
	case eSQ_Medium:
		psoDesc.m_ShaderFlags_RT |= quality;
		break;
	case eSQ_High:
		psoDesc.m_ShaderFlags_RT |= quality1;
		break;
	case eSQ_VeryHigh:
		psoDesc.m_ShaderFlags_RT |= (quality | quality1);
		break;
	}
}

size_t CStandardGraphicsPipeline::GetViewInfoCount() const
{
	const CRenderView* pRenderView = GetCurrentRenderView();
	return pRenderView->GetViewInfoCount();
}

size_t CStandardGraphicsPipeline::GenerateViewInfo(SRenderViewInfo viewInfo[2])
{
	const CRenderView* pRenderView = GetCurrentRenderView();
	for (int i = 0; i < pRenderView->GetViewInfoCount(); i++)
	{
		viewInfo[i] = pRenderView->GetViewInfo((CCamera::EEye)i);
	}
	return pRenderView->GetViewInfoCount();
}

const SRenderViewInfo& CStandardGraphicsPipeline::GetCurrentViewInfo(CCamera::EEye eye) const
{
	const CRenderView* pRenderView = GetCurrentRenderView();
	CRY_ASSERT(pRenderView);
	if (pRenderView)
	{
		return pRenderView->GetViewInfo(eye);
	}

	static SRenderViewInfo viewInfo;
	return viewInfo;
}

void CStandardGraphicsPipeline::GenerateMainViewConstantBuffer()
{
	const CRenderView* pRenderView = GetCurrentRenderView();

	GeneratePerViewConstantBuffer(&pRenderView->GetViewInfo(CCamera::eEye_Left), pRenderView->GetViewInfoCount(), m_mainViewConstantBuffer.GetDeviceConstantBuffer());
}

void CStandardGraphicsPipeline::GeneratePerViewConstantBuffer(const SRenderViewInfo* pViewInfo, int viewInfoCount, CConstantBufferPtr pPerViewBuffer,const SRenderViewport* pCustomViewport)
{
	const CRenderView* pRenderView = GetCurrentRenderView();
	CRY_ASSERT(pRenderView);
	if (!gEnv->p3DEngine || !pPerViewBuffer)
		return;

	const SRenderViewShaderConstants& perFrameConstants = pRenderView->GetShaderConstants();
	CryStackAllocWithSizeVectorCleared(HLSL_PerViewGlobalConstantBuffer, viewInfoCount, bufferData, CDeviceBufferManager::AlignBufferSizeForStreaming);

	const SRenderGlobalFogDescription& globalFog = pRenderView->GetGlobalFog();

	for (int i = 0; i < viewInfoCount; ++i)
	{
		CRY_ASSERT(pViewInfo[i].pCamera);

		const SRenderViewInfo& viewInfo = pViewInfo[i];

		HLSL_PerViewGlobalConstantBuffer& cb = bufferData[i];

		const float animTime = GetAnimationTime().GetSeconds();
		const bool bReverseDepth = (viewInfo.flags & SRenderViewInfo::eFlags_ReverseDepth) != 0;

		cb.CV_HPosScale = viewInfo.downscaleFactor;

		SRenderViewport viewport = pCustomViewport ? *pCustomViewport : viewInfo.viewport;
		cb.CV_ScreenSize = Vec4(float(viewport.width),
		                        float(viewport.height),
		                        0.5f / (viewport.width / viewInfo.downscaleFactor.x),
		                        0.5f / (viewport.height / viewInfo.downscaleFactor.y));

		cb.CV_ViewProjZeroMatr = viewInfo.cameraProjZeroMatrix.GetTransposed();
		cb.CV_ViewProjMatr = viewInfo.cameraProjMatrix.GetTransposed();
		cb.CV_ViewProjNearestMatr = viewInfo.cameraProjNearestMatrix.GetTransposed();
		cb.CV_InvViewProj = viewInfo.invCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjMatr = viewInfo.prevCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjNearestMatr = viewInfo.prevCameraProjNearestMatrix.GetTransposed();
		cb.CV_ViewMatr = viewInfo.viewMatrix.GetTransposed();
		cb.CV_InvViewMatr = viewInfo.invViewMatrix.GetTransposed();

		Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(Matrix44(IDENTITY), *viewInfo.pCamera, pRenderView->m_vProjMatrixSubPixoffset,
		                                                 float(viewport.width), float(viewport.height), vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

		cb.CV_ScreenToWorldBasis.SetColumn(0, Vec3r(vWBasisX));
		cb.CV_ScreenToWorldBasis.SetColumn(1, Vec3r(vWBasisY));
		cb.CV_ScreenToWorldBasis.SetColumn(2, Vec3r(vWBasisZ));
		cb.CV_ScreenToWorldBasis.SetColumn(3, viewInfo.cameraOrigin);

		cb.CV_SunLightDir = Vec4(perFrameConstants.pSunDirection, 1.0f);
		cb.CV_SunColor = Vec4(perFrameConstants.pSunColor, perFrameConstants.sunSpecularMultiplier);
		cb.CV_SkyColor = Vec4(perFrameConstants.pSkyColor, 1.0f);
		cb.CV_FogColor = Vec4( globalFog.bEnable ? globalFog.color.toVec3() : Vec3(0.f,0.f,0.f), perFrameConstants.pVolumetricFogParams.z);
		cb.CV_TerrainInfo = Vec4(gEnv->p3DEngine->GetTerrainTextureMultiplier(), 0, 0, 0);

		cb.CV_AnimGenParams = Vec4(animTime*2.0f, animTime*0.25f, animTime*1.0f, animTime*0.125f);

		Vec3 pDecalZFightingRemedy;
		{
			const float* mProj = viewInfo.projMatrix.GetData();
			const float s = clamp_tpl(CRenderer::CV_r_ZFightingDepthScale, 0.1f, 1.0f);

			pDecalZFightingRemedy.x = s;                                      // scaling factor to pull decal in front
			pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4 * 3 + 2]); // correction factor for homogeneous z after scaling is applied to xyzw { = ( 1 - v[0] ) * zMappingRageBias }
			pDecalZFightingRemedy.z = clamp_tpl(CRenderer::CV_r_ZFightingExtrude, 0.0f, 1.0f);

			// alternative way the might save a bit precision
			//PF.pDecalZFightingRemedy.x = s; // scaling factor to pull decal in front
			//PF.pDecalZFightingRemedy.y = (float)((1.0f - s) * mProj[4*2+2]);
			//PF.pDecalZFightingRemedy.z = clamp_tpl(CRenderer::CV_r_ZFightingExtrude, 0.0f, 1.0f);
		}
		cb.CV_DecalZFightingRemedy = Vec4(pDecalZFightingRemedy, 0);

		cb.CV_CamRightVector = Vec4(viewInfo.cameraVX.GetNormalized(), 0);
		cb.CV_CamFrontVector = Vec4(viewInfo.cameraVZ.GetNormalized(), 0);
		cb.CV_CamUpVector = Vec4(viewInfo.cameraVY.GetNormalized(), 0);
		cb.CV_WorldViewPosition = Vec4(viewInfo.cameraOrigin, 0);

		// CV_NearFarClipDist
		{
			// Note: CV_NearFarClipDist.z is used to put the weapon's depth range into correct relation to the whole scene
			// when generating the depth texture in the z pass (_RT_NEAREST)
			cb.CV_NearFarClipDist = Vec4(
				viewInfo.nearClipPlane,
				viewInfo.farClipPlane,
				viewInfo.farClipPlane / gEnv->p3DEngine->GetMaxViewDistance(),
				1.0f / viewInfo.farClipPlane);
		}

		// CV_ProjRatio
		{
			float zn = viewInfo.nearClipPlane;
			float zf = viewInfo.farClipPlane;
			float hfov = viewInfo.pCamera->GetHorizontalFov();
			cb.CV_ProjRatio.x = bReverseDepth ? zn / (zn - zf) : zf / (zf - zn);
			cb.CV_ProjRatio.y = bReverseDepth ? zn / (zf - zn) : zn / (zn - zf);
			cb.CV_ProjRatio.z = 1.0f / hfov;
			cb.CV_ProjRatio.w = 1.0f;
		}

		// CV_NearestScaled
		{
			float zn = viewInfo.nearClipPlane;
			float zf = viewInfo.farClipPlane;
			float nearZRange = CRendererCVars::CV_r_DrawNearZRange;
			float camScale = CRendererCVars::CV_r_DrawNearFarPlane / gEnv->p3DEngine->GetMaxViewDistance();
			cb.CV_NearestScaled.x = bReverseDepth ? 1.0f - zf / (zf - zn) * nearZRange : zf / (zf - zn) * nearZRange;
			cb.CV_NearestScaled.y = bReverseDepth ? zn / (zf - zn) * nearZRange * nearZRange : zn / (zn - zf) * nearZRange * nearZRange;
			cb.CV_NearestScaled.z = bReverseDepth ? 1.0f - (nearZRange - 0.001f) : nearZRange - 0.001f;
			cb.CV_NearestScaled.w = 1.0f;
		}

		// CV_TessInfo
		{
			// We want to obtain the edge length in pixels specified by CV_r_tessellationtrianglesize
			// Therefore the tess factor would depend on the viewport size and CV_r_tessellationtrianglesize
			static const ICVar* pCV_e_TessellationMaxDistance(gEnv->pConsole->GetCVar("e_TessellationMaxDistance"));
			assert(pCV_e_TessellationMaxDistance);

			const float hfov = viewInfo.pCamera->GetHorizontalFov();
			cb.CV_TessInfo.x = sqrtf(float(viewport.width * viewport.height)) / (hfov * CRendererCVars::CV_r_tessellationtrianglesize);
			cb.CV_TessInfo.y = CRendererCVars::CV_r_displacementfactor;
			cb.CV_TessInfo.z = pCV_e_TessellationMaxDistance->GetFVal();
			cb.CV_TessInfo.w = (float)CRenderer::CV_r_ParticlesTessellationTriSize;
		}

		cb.CV_FrustumPlaneEquation.SetRow4(0, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_RIGHT]);
		cb.CV_FrustumPlaneEquation.SetRow4(1, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_LEFT]);
		cb.CV_FrustumPlaneEquation.SetRow4(2, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_TOP]);
		cb.CV_FrustumPlaneEquation.SetRow4(3, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_BOTTOM]);

		if (gRenDev->m_pCurWindGrid)
		{
			float fSizeWH = (float)gRenDev->m_pCurWindGrid->m_nWidth * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
			float fSizeHH = (float)gRenDev->m_pCurWindGrid->m_nHeight * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
			cb.CV_WindGridOffset = Vec4(gRenDev->m_pCurWindGrid->m_vCentr.x - fSizeWH, gRenDev->m_pCurWindGrid->m_vCentr.y - fSizeHH, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nWidth, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nHeight);
		}
	}

	pPerViewBuffer->UpdateBuffer(&bufferData[0], sizeof(HLSL_PerViewGlobalConstantBuffer), 0, viewInfoCount);
}

bool CStandardGraphicsPipeline::FillCommonScenePassStates(const SGraphicsPipelineStateDescription& inputDesc, CDeviceGraphicsPSODesc& psoDesc)
{
	CShader* pShader = static_cast<CShader*>(inputDesc.shaderItem.m_pShader);
	SShaderTechnique* pTechnique = pShader->GetTechnique(inputDesc.shaderItem.m_nTechnique, inputDesc.technique, true);
	if (!pTechnique)
		return false;

	CShaderResources* pRes = static_cast<CShaderResources*>(inputDesc.shaderItem.m_pShaderResources);
	const uint64 objectFlags = inputDesc.objectFlags;
	const uint8 renderState = inputDesc.renderState;
	SShaderPass* pShaderPass = &pTechnique->m_Passes[0];

	// Handle quality flags
	CStandardGraphicsPipeline::ApplyShaderQuality(psoDesc, gcpRendD3D->GetShaderProfile(pShader->m_eShaderType));

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

	if (psoDesc.m_RenderState & GS_ALPHATEST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

#ifdef TESSELLATION_RENDERER
	const bool bHasTesselationShaders = pShaderPass && pShaderPass->m_HShader && pShaderPass->m_DShader;
	if (bHasTesselationShaders && (!(objectFlags & FOB_NEAREST) && (objectFlags & FOB_ALLOW_TESSELLATION)))
	{
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
		psoDesc.m_bAllowTesselation = true;
	}
#endif

	psoDesc.m_CullMode = bTwoSided ? eCULL_None : ((pShaderPass && pShaderPass->m_eCull != -1) ? (ECull)pShaderPass->m_eCull : eCULL_Back);
	psoDesc.m_PrimitiveType = inputDesc.primitiveType;

	if (psoDesc.m_bAllowTesselation)
	{
		psoDesc.m_PrimitiveType = ept3ControlPointPatchList;
		psoDesc.m_ObjectStreamMask |= VSM_NORMALS;
	}

	psoDesc.m_ShaderFlags_RT |= CVrProjectionManager::Instance()->GetRTFlags();

	return true;
}

CDeviceResourceLayoutPtr CStandardGraphicsPipeline::CreateScenePassLayout(const CDeviceResourceSetDesc& perPassResources)
{
	SDeviceResourceLayoutDesc layoutDesc;

	layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerDrawCB, eConstantBufferShaderSlot_PerDraw, EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);
	
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerDrawExtraRS, GetDefaultDrawExtraResourceLayout());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS , GetDefaultMaterialBindPoints());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS     , perPassResources);

	CDeviceResourceLayoutPtr pResourceLayout = GetDeviceObjectFactory().CreateResourceLayout(layoutDesc);
	assert(pResourceLayout != nullptr);
	return pResourceLayout;
}

std::array<SamplerStateHandle, EFSS_MAX> CStandardGraphicsPipeline::GetDefaultMaterialSamplers() const
{
	std::array<SamplerStateHandle, EFSS_MAX> result =
	{
		{
			gcpRendD3D->m_nMaterialAnisoHighSampler,                                                                                                                                         // EFSS_ANISO_HIGH
			gcpRendD3D->m_nMaterialAnisoLowSampler,                                                                                                                                          // EFSS_ANISO_LOW
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, 0x0)),         // EFSS_TRILINEAR
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, eSamplerAddressMode_Wrap, 0x0)),          // EFSS_BILINEAR
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0)),      // EFSS_TRILINEAR_CLAMP
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_BILINEAR, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, eSamplerAddressMode_Clamp, 0x0)),       // EFSS_BILINEAR_CLAMP
			gcpRendD3D->m_nMaterialAnisoSamplerBorder,                                                                                                                                       // EFSS_ANISO_HIGH_BORDER
			CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_TRILINEAR, eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, 0x0)),   // EFSS_TRILINEAR_BORDER
		}
	};

	return result;
}

void CStandardGraphicsPipeline::ExecutePostAA()
{
	m_pPostAAStage->Execute();
}

void CStandardGraphicsPipeline::ExecuteAnisotropicVerticalBlur(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly)
{
	GetOrCreateUtilityPass<CAnisotropicVerticalBlurPass>()->Execute(pTex, nAmount, fScale, fDistribution, bAlphaOnly);
}

void CStandardGraphicsPipeline::ExecuteHDRPostProcessing()
{
	PROFILE_LABEL_SCOPE("POST_EFFECTS_HDR");

	const auto& viewInfo = GetCurrentViewInfo(CCamera::eEye_Left);
	PostProcessUtils().m_pView = viewInfo.viewMatrix;
	PostProcessUtils().m_pProj = viewInfo.projMatrix;

	PostProcessUtils().m_pViewProj = PostProcessUtils().m_pView * PostProcessUtils().m_pProj;
	PostProcessUtils().m_pViewProj = ReverseDepthHelper::Convert(PostProcessUtils().m_pViewProj);
	PostProcessUtils().m_pViewProj.Transpose();

	m_pRainStage->Execute();

	// Note: MB uses s_ptexHDRTargetPrev to avoid doing another copy, so this should be right before the MB pass
	{
		GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexHDRTargetPrev);
	}

	m_pDepthOfFieldStage->Execute();
	m_pMotionBlurStage->Execute();
	m_pSnowStage->Execute();

	// Half resolution downsampling
	{
		PROFILE_LABEL_SCOPE("HALFRES_DOWNSAMPLE_HDRTARGET");

		if (CRenderer::CV_r_HDRBloomQuality > 1)
			GetOrCreateUtilityPass<CStableDownsamplePass>()->Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexHDRTargetScaled[0], true);
		else
			GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexHDRTargetScaled[0]);
	}

	// Quarter resolution downsampling
	{
		PROFILE_LABEL_SCOPE("QUARTER_RES_DOWNSAMPLE_HDRTARGET");

		if (CRenderer::CV_r_HDRBloomQuality > 0)
			GetOrCreateUtilityPass<CStableDownsamplePass>()->Execute(CRendererResources::s_ptexHDRTargetScaled[0], CRendererResources::s_ptexHDRTargetScaled[1], CRenderer::CV_r_HDRBloomQuality >= 1);
		else
			GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CRendererResources::s_ptexHDRTargetScaled[0], CRendererResources::s_ptexHDRTargetScaled[1]);
	}

	if (GetCurrentRenderView()->GetCurrentEye() != CCamera::eEye_Right)
	{
		m_pAutoExposureStage->Execute();
	}

	m_pBloomStage->Execute();

	// Lens optics
	if (CRenderer::CV_r_flares && !CRenderer::CV_r_durango_async_dips)
	{
		PROFILE_LABEL_SCOPE("LENS_OPTICS");
		m_pLensOpticsStage->Execute();
	}

	m_pSunShaftsStage->Execute();
	m_pColorGradingStage->Execute();
	m_pToneMappingStage->Execute();
}

void CStandardGraphicsPipeline::ExecuteDebugger()
{
	m_pSceneCustomStage->ExecuteDebugger();

	if (m_pSceneCustomStage->DoDebugOverlay())
	{
		m_pSceneCustomStage->ExecuteDebugOverlay();
	}
}

void CStandardGraphicsPipeline::ExecuteBillboards()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();

	CClearSurfacePass::Execute(CRendererResources::s_ptexSceneNormalsMap, Clr_Transparent);
	CClearSurfacePass::Execute(CRendererResources::s_ptexSceneDiffuse, Clr_Transparent);

	GetGBufferStage()->Execute();

	pRenderView->SwitchUsageMode(CRenderView::eUsageModeReadingDone);
}

// TODO: This will be used only for recursive render pass after all render views get rendered with full graphics pipeline including tiled forward shading.
void CStandardGraphicsPipeline::ExecuteMinimumForwardShading()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	// Render into these targets
	CTexture* pColorTex = pRenderView->GetColorTarget();
	CTexture* pDepthTex = pRenderView->GetDepthTarget();

	const bool bRecursive = pRenderView->IsRecursive();
	const bool bSecondaryViewport = (pRenderView->GetShaderRenderingFlags() & SHDF_SECONDARY_VIEWPORT) != 0;

	m_renderPassScheduler.SetEnabled(true);

	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		m_pComputeParticlesStage->PreDraw();
		m_pComputeSkinningStage->Execute();
	}

	// recursive pass doesn't use deferred fog, instead uses forward shader fog.
	SetPipelineFlags(GetPipelineFlags() & ~EPipelineFlags::NO_SHADER_FOG);

	if (!(pRenderView->GetShaderRenderingFlags() & SHDF_CUBEMAPGEN))
	{
		if (!bRecursive && pOutput && bSecondaryViewport)
		{
			gRenDev->GetIRenderAuxGeom()->Submit();
		}
	}

	// GBuffer ZPass only
	m_pSceneGBufferStage->ExecuteMinimumZpass();

	// forward opaque and transparent passes for recursive rendering
	m_pSceneForwardStage->ExecuteMinimum(pColorTex, pDepthTex);

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
			m_pComputeParticlesStage->PostDraw();
		}
	}

	if (!(pRenderView->GetShaderRenderingFlags() & SHDF_CUBEMAPGEN))
	{
		// Resolve HDR render target to back buffer if needed.
		if (!bRecursive && pOutput && bSecondaryViewport)
		{
			m_pToneMappingStage->ExecuteFixedExposure(pColorTex, pDepthTex);
		}

		m_pSceneCustomStage->ExecuteHelpers();
	}

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute();

	ResetUtilityPassCache();
}

void CStandardGraphicsPipeline::ExecuteMobilePipeline()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	
	if (CRenderer::CV_r_GraphicsPipelineMobile == 2)
		m_pSceneGBufferStage->Execute();
	else
		m_pSceneGBufferStage->ExecuteMicroGBuffer();

	pRenderView->GetDrawer().WaitForDrawSubmission();

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

		{
			m_pClipVolumesStage->GenerateClipVolumeInfo();
			m_pTiledLightVolumesStage->Execute();
			m_pTiledShadingStage->Execute();
		}
	}

	m_pMobileCompositionStage->Execute();

	pRenderer->m_pPostProcessMgr->End(pRenderView);
}

void CStandardGraphicsPipeline::Execute()
{
	if (CRenderer::CV_r_GraphicsPipelineMobile)
	{
		ExecuteMobilePipeline();
		return;
	}
	
	CD3D9Renderer* pRenderer = gcpRendD3D;
	CRenderView* pRenderView = GetCurrentRenderView();
	auto& renderItemDrawer = pRenderView->GetDrawer();
	CTexture* pZTexture = pRenderView->GetDepthTarget();

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE");
	
	m_renderPassScheduler.SetEnabled(true);

	// Generate cloud volume textures for shadow mapping. Only needs view, and needs to run before ShadowMaskgen.
	m_pVolumetricCloudsStage->ExecuteShadowGen();

	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		// Compute algorithms
		m_pComputeSkinningStage->Execute();

		// Revert resource states to graphics pipeline
		m_pComputeParticlesStage->PreDraw();
		m_pComputeSkinningStage->PreDraw();

		m_pRainStage->ExecuteRainOcclusion();
	}

	// GBuffer
	m_pSceneGBufferStage->Execute();

	// Wait for GBuffer draw jobs to finish
	renderItemDrawer.WaitForDrawSubmission();

	// Issue split barriers for GBuffer
	CTexture* pTextures[] = {
		CRendererResources::s_ptexSceneNormalsMap,
		CRendererResources::s_ptexSceneDiffuse,
		CRendererResources::s_ptexSceneSpecular,
		pZTexture
	};

	CDeviceGraphicsCommandInterface* pCmdList = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();
	pCmdList->BeginResourceTransitions(CRY_ARRAY_COUNT(pTextures), pTextures, eResTransition_TextureRead);

	// Shadow maps
	if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	{
		m_pShadowMapStage->Execute();
	}

	// Wait for Shadow Map draw jobs to finish (also required for HeightMap AO and SVOGI)
	renderItemDrawer.WaitForDrawSubmission();

	// Depth downsampling
	{
		CTexture* pSourceDepth = CRendererResources::s_ptexLinearDepth;

#if CRY_PLATFORM_DURANGO
		pSourceDepth = pZTexture;  // On Durango reading device depth is faster since it is in ESRAM
#endif

		if (CVrProjectionManager::IsMultiResEnabledStatic())
			CVrProjectionManager::Instance()->ExecuteFlattenDepth(pSourceDepth, CVrProjectionManager::Instance()->GetZTargetFlattened());

		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(pSourceDepth, CRendererResources::s_ptexLinearDepthScaled[0], (pSourceDepth == pZTexture), true);
		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(CRendererResources::s_ptexLinearDepthScaled[0], CRendererResources::s_ptexLinearDepthScaled[1], false, false);
		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(CRendererResources::s_ptexLinearDepthScaled[1], CRendererResources::s_ptexLinearDepthScaled[2], false, false);
	}

	// Depth readback (for occlusion culling)
	m_pDepthReadbackStage->Execute();
	m_pDeferredDecalsStage->Execute();

	m_pSceneGBufferStage->ExecuteGBufferVisualization();

	// GBuffer modifiers
	m_pRainStage->ExecuteDeferredRainGBuffer();
	m_pSnowStage->ExecuteDeferredSnowGBuffer();

	// Generate cloud volume textures for shadow mapping.
	m_pVolumetricCloudsStage->ExecuteShadowGen();

	// SVOGI
	{
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance())
		{
			PROFILE_LABEL_SCOPE("SVOGI");

			CSvoRenderer::GetInstance()->UpdateCompute(pRenderView);
			CSvoRenderer::GetInstance()->UpdateRender(pRenderView);
		}
#endif
	}

	// Screen Space Reflections
	m_pScreenSpaceReflectionsStage->Execute();
	// Height Map AO
	m_pHeightMapAOStage->Execute();

	// Screen Space Obscurance
	if (!CRenderer::CV_r_DeferredShadingDebugGBuffer)
	{
		m_pScreenSpaceObscuranceStage->Execute();
	}

	if (CRenderer::CV_r_DeferredShadingTiled > 1)
	{
		m_pTiledLightVolumesStage->Execute();
	}

	// Water volume caustics
	m_pWaterStage->ExecuteWaterVolumeCaustics();

	SetPipelineFlags(GetPipelineFlags() | EPipelineFlags::NO_SHADER_FOG);

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");

		m_pClipVolumesStage->GenerateClipVolumeInfo();
		m_pClipVolumesStage->Prepare();
		m_pClipVolumesStage->Execute();

		if (CRenderer::CV_r_DeferredShadingTiled > 1)
		{		
			m_pShadowMaskStage->Prepare();
			m_pShadowMaskStage->Execute();
			
			m_pTiledShadingStage->Execute();

			if (CRenderer::CV_r_DeferredShadingSSS)
			{
				m_pScreenSpaceSSSStage->Execute(CRendererResources::s_ptexSceneTargetR11G11B10F[0]);
			}
		}
	}

	// Opaque forward passes
	m_pSceneForwardStage->ExecuteOpaque();
	// Deferred ocean caustics
	m_pWaterStage->ExecuteDeferredOceanCaustics();
	// Fog
	m_pVolumetricFogStage->Execute();
	m_pFogStage->Execute();

	SetPipelineFlags(GetPipelineFlags() & ~EPipelineFlags::NO_SHADER_FOG);

	// Clouds
	m_pVolumetricCloudsStage->Execute();
	// Water fog volumes
	m_pWaterStage->ExecuteWaterFogVolumeBeforeTransparent();
	// Transparent (below water)
	m_pSceneForwardStage->ExecuteTransparentBelowWater();
	// Ocean and water volumes
	m_pWaterStage->Execute();
	// Transparent (above water)
	m_pSceneForwardStage->ExecuteTransparentAboveWater();

	if (CRenderer::CV_r_TranspDepthFixup)
	{
		m_pSceneForwardStage->ExecuteTransparentDepthFixup();
	}

	// Half-res particles
	if (CRenderer::CV_r_ParticlesHalfRes)
	{
		m_pSceneForwardStage->ExecuteTransparentLoRes(1 + crymath::clamp<int>(CRenderer::CV_r_ParticlesHalfResAmount, 0, 1));
	}

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence(pRenderer->GetRenderFrameID());

	m_pSnowStage->ExecuteDeferredSnowDisplacement();

	if (pRenderView->GetCurrentEye() == CCamera::eEye_Right ||
		!pRenderer->GetS3DRend().IsStereoEnabled() ||
		!pRenderer->GetS3DRend().RequiresSequentialSubmission())
	{
		m_pComputeParticlesStage->PostDraw();
	}

	if (!(pRenderView->GetShaderRenderingFlags() & SHDF_CUBEMAPGEN))
	{
		// HDR and LDR post-processing
		{
			// CRendererResources::s_ptexHDRTarget -> CRendererResources::s_ptexSceneDiffuse (Tonemapping)
			ExecuteHDRPostProcessing();

			// CRendererResources::s_ptexSceneDiffuse
			m_pSceneForwardStage->ExecuteAfterPostProcessHDR();

			// CRendererResources::s_ptexSceneDiffuse -> CRenderOutput->m_pColorTarget (PostAA)
			if (!m_pPostEffectStage->Execute())
			{
				// Post effects disabled, copy diffuse to color target
				CStretchRectPass::GetPass().Execute(CRendererResources::s_ptexSceneDiffuse, pRenderView->GetRenderOutput()->GetColorTarget());
			}

			// CRenderOutput->m_pColorTarget
			m_pSceneForwardStage->ExecuteAfterPostProcessLDR();
		}

		if (gEnv->IsEditor() && !gEnv->IsEditorGameMode())
		{
			m_pSceneCustomStage->ExecuteSelectionHighlight();
		}

		if (m_pSceneCustomStage->DoDebugOverlay())
		{
			m_pSceneCustomStage->ExecuteDebugOverlay();
		}

		if (gEnv->IsEditor())
		{
			m_pSceneCustomStage->ExecuteHelpers();
		}

		// Display tone mapping debugging information on the screen
		if (CRenderer::CV_r_HDRDebug == 1 && !pRenderView->IsRecursive())
		{
			m_pToneMappingStage->DisplayDebugInfo();
		}
	}
	else
	{
		GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CRendererResources::s_ptexHDRTarget, pRenderView->GetRenderOutput()->GetColorTarget());
	}

	m_pOmniCameraStage->Execute();

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE");

	m_pVolumetricFogStage->ResetFrame();

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute();

	ResetUtilityPassCache();
}
