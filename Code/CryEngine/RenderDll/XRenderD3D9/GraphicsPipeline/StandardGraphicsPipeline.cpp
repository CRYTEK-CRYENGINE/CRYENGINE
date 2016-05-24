// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StandardGraphicsPipeline.h"

#include "ShadowMap.h"
#include "SceneGBuffer.h"
#include "SceneForward.h"
#include "AutoExposure.h"
#include "Bloom.h"
#include "HeightMapAO.h"
#include "ScreenSpaceObscurance.h"
#include "ScreenSpaceReflections.h"
#include "ScreenSpaceSSS.h"
#include "MotionBlur.h"
#include "DepthOfField.h"
#include "SunShafts.h"
#include "ToneMapping.h"
#include "PostAA.h"
#include "ComputeSkinning.h"
#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"
#include "D3DVolumetricClouds.h"

#include "DriverD3D.h" //for gcpRendD3D

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

//////////////////////////////////////////////////////////////////////////
CRenderView* CGraphicsPipelineStage::RenderView()
{
	return gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
}

void CStandardGraphicsPipeline::Init()
{
	// default material resources
	{
		m_pDefaultMaterialResources = CCryDeviceWrapper::GetObjectFactory().CreateResourceSet();
		m_pDefaultMaterialResources->SetConstantBuffer(eConstantBufferShaderSlot_PerMaterial, NULL, EShaderStage_AllWithoutCompute);

		for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
		{
			CTexture* pDefaultTexture = TextureHelpers::LookupTexDefault(texType);
			m_pDefaultMaterialResources->SetTexture(texType, pDefaultTexture, SResourceView::DefaultView, EShaderStage_AllWithoutCompute);
		}
	}

	// default extra per instance
	{
		EShaderStage shaderStages = EShaderStage_Vertex | EShaderStage_Hull | EShaderStage_Domain;
		CGpuBuffer nullBuffer;
		nullBuffer.Create(0, 0, DXGI_FORMAT_R32G32B32A32_FLOAT, DX11BUF_NULL_RESOURCE | DX11BUF_BIND_SRV, nullptr);
		m_pDefaultInstanceExtraResources = CCryDeviceWrapper::GetObjectFactory().CreateResourceSet();
		m_pDefaultInstanceExtraResources->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat, CDeviceBufferManager::CreateNullConstantBuffer(), shaderStages);
		m_pDefaultInstanceExtraResources->SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, CDeviceBufferManager::CreateNullConstantBuffer(), shaderStages);
		m_pDefaultInstanceExtraResources->SetBuffer(EReservedTextureSlot_SkinExtraWeights, nullBuffer, shaderStages);
		m_pDefaultInstanceExtraResources->SetBuffer(EReservedTextureSlot_AdjacencyInfo, nullBuffer, shaderStages);             // shares shader slot with EReservedTextureSlot_PatchID
		m_pDefaultInstanceExtraResources->SetBuffer(EReservedTextureSlot_ComputeSkinVerts, nullBuffer, shaderStages);          // shares shader slot with EReservedTextureSlot_PatchID
		m_pDefaultInstanceExtraResources->Build();                                                                             // This needs to be a valid resource-set since it's shared by all CompiledRenderObject that don't need a unique instance
	}

	// Register scene stages that make use of the global PSO cache
	RegisterSceneStage<CShadowMapStage, eStage_ShadowMap>(m_pShadowMapStage);
	RegisterSceneStage<CSceneGBufferStage, eStage_SceneGBuffer>(m_pSceneGBufferStage);
	RegisterSceneStage<CSceneForwardStage, eStage_SceneForward>(m_pSceneForwardStage);

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CHeightMapAOStage>(m_pHeightMapAOStage, eStage_HeightMapAO);
	RegisterStage<CScreenSpaceObscuranceStage>(m_pScreenSpaceObscuranceStage, eStage_ScreenSpaceObscurance);
	RegisterStage<CScreenSpaceReflectionsStage>(m_pScreenSpaceReflectionsStage, eStage_ScreenSpaceReflections);
	RegisterStage<CScreenSpaceSSSStage>(m_pScreenSpaceSSSStage, eStage_ScreenSpaceSSS);
	RegisterStage<CMotionBlurStage>(m_pMotionBlurStage, eStage_MotionBlur);
	RegisterStage<CDepthOfFieldStage>(m_pDepthOfFieldStage, eStage_DepthOfField);
	RegisterStage<CAutoExposureStage>(m_pAutoExposureStage, eStage_AutoExposure);
	RegisterStage<CBloomStage>(m_pBloomStage, eStage_Bloom);
	RegisterStage<CToneMappingStage>(m_pToneMappingStage, eStage_ToneMapping);
	RegisterStage<CSunShaftsStage>(m_pSunShaftsStage, eStage_Sunshafts);
	RegisterStage<CPostAAStage>(m_pPostAAStage, eStage_PostAA);
	RegisterStage<CComputeSkinningStage>(m_pComputeSkinningStage, eStage_ComputeSkinning);

	m_bInitialized = true;
}

void CStandardGraphicsPipeline::ReleaseBuffers()
{
	if (m_bInitialized && m_pSceneGBufferStage)
		m_pSceneGBufferStage->ReleaseBuffers();
}

void CStandardGraphicsPipeline::Prepare(CRenderView* pRenderView)
{
	m_pCurrentRenderView = pRenderView;

	for (auto& pStage : m_pipelineStages)
	{
		if (pStage)
		{
			pStage->Prepare(pRenderView);
		}
	}
}

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

void CStandardGraphicsPipeline::UpdatePerViewConstantBuffer(const RECT* pCustomViewport)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rp = gRenDev->m_RP;

	SViewInfo viewInfo(pRenderer->GetRCamera(), pRenderer->GetCamera());
	viewInfo.bReverseDepth = (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;
	viewInfo.bMirrorCull = (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL) != 0;

	int vpX, vpY, vpWidth, vpHeight;
	if (!pCustomViewport)
	{
		pRenderer->GetViewport(&vpX, &vpY, &vpWidth, &vpHeight);
	}
	else
	{
		vpX = pCustomViewport->left;
		vpY = pCustomViewport->top;
		vpWidth = pCustomViewport->right - pCustomViewport->left;
		vpHeight = pCustomViewport->bottom - pCustomViewport->top;
	}

	viewInfo.viewport.TopLeftX = float(vpX);
	viewInfo.viewport.TopLeftY = float(vpY);
	viewInfo.viewport.Width = float(vpWidth);
	viewInfo.viewport.Height = float(vpHeight);
	viewInfo.downscaleFactor = Vec4(rp.m_CurDownscaleFactor.x, rp.m_CurDownscaleFactor.y, pRenderer->m_PrevViewportScale.x, pRenderer->m_PrevViewportScale.y);

	// calculate nearest projection matrix
	Matrix44A nearestProj;

	{
		const CCamera& cam = pRenderer->GetCamera();
		const float fNearestFov = pRenderer->GetDrawNearestFOV();

		float fFov = cam.GetFov();
		if (fNearestFov > 1.0f && fNearestFov < 179.0f)
			fFov = DEG2RAD(fNearestFov);

		float fNearRatio = DRAW_NEAREST_MIN / cam.GetNearPlane();
		float wT = tanf(fFov * 0.5f) * DRAW_NEAREST_MIN, wB = -wT;
		float wR = wT * cam.GetProjRatio(), wL = -wR;

		wL += cam.GetAsymL() * fNearRatio;
		wR += cam.GetAsymR() * fNearRatio;
		wB += cam.GetAsymB() * fNearRatio, wT += cam.GetAsymT() * fNearRatio;

		if (viewInfo.bReverseDepth) mathMatrixPerspectiveOffCenterReverseDepth(&nearestProj, wL, wR, wB, wT, DRAW_NEAREST_MIN, pRenderer->CV_r_DrawNearFarPlane);
		else                        mathMatrixPerspectiveOffCenter(&nearestProj, wL, wR, wB, wT, DRAW_NEAREST_MIN, pRenderer->CV_r_DrawNearFarPlane);

		bool bApplySubpixelShift = !(rp.m_PersFlags2 & RBPF2_NOPOSTAA);
		bApplySubpixelShift &= !(rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & (RBPF_DRAWTOTEXTURE | RBPF_SHADOWGEN));

		if (bApplySubpixelShift)
		{
			nearestProj.m20 += pRenderer->m_vProjMatrixSubPixoffset.x;
			nearestProj.m21 += pRenderer->m_vProjMatrixSubPixoffset.y;
		}
	}

	viewInfo.m_CameraProjZeroMatrix = pRenderer->m_CameraProjZeroMatrix;
	viewInfo.m_CameraProjMatrix = pRenderer->m_CameraProjMatrix;
	viewInfo.m_ProjMatrix = pRenderer->m_ProjMatrix;
	viewInfo.m_InvCameraProjMatrix = pRenderer->m_InvCameraProjMatrix;
	viewInfo.m_CameraProjNearestMatrix = pRenderer->m_CameraZeroMatrix[rp.m_nProcessThreadID] * nearestProj;

	viewInfo.m_PrevCameraProjMatrix = pRenderer->GetPreviousFrameCameraMatrix() * pRenderer->m_ProjMatrix;
	viewInfo.m_PrevCameraProjNearestMatrix = pRenderer->GetPreviousFrameCameraMatrix();
	viewInfo.m_PrevCameraProjNearestMatrix.m30 = 0.0f;
	viewInfo.m_PrevCameraProjNearestMatrix.m31 = 0.0f;
	viewInfo.m_PrevCameraProjNearestMatrix.m32 = 0.0f;
	viewInfo.m_PrevCameraProjNearestMatrix = viewInfo.m_PrevCameraProjNearestMatrix * nearestProj;

	if (rp.m_ShadowInfo.m_pCurShadowFrustum && (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_SHADOWGEN))
	{
		const SRenderPipeline::ShadowInfo& shadowInfo = rp.m_ShadowInfo;
		assert(shadowInfo.m_nOmniLightSide >= 0 && shadowInfo.m_nOmniLightSide < OMNI_SIDES_NUM);

		CCamera& cam = shadowInfo.m_pCurShadowFrustum->FrustumPlanes[shadowInfo.m_nOmniLightSide];
		viewInfo.pFrustumPlanes = cam.GetFrustumPlane(0);
	}
	else
	{
		viewInfo.pFrustumPlanes = pRenderer->GetCamera().GetFrustumPlane(0);
	}

	UpdatePerViewConstantBuffer(viewInfo, m_pPerViewConstantBuffer);
}

void CStandardGraphicsPipeline::UpdatePerViewConstantBuffer(const SViewInfo& viewInfo, CConstantBufferPtr& pPerViewBuffer)
{
	if (!gEnv->p3DEngine)
		return;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rp = gRenDev->m_RP;
	const SCGParamsPF& perFrameConstants = pRenderer->m_cEF.m_PF[rp.m_nProcessThreadID];

	CTypedConstantBuffer<HLSL_PerViewGlobalConstantBuffer> cb(pPerViewBuffer);

	const float time = rp.m_TI[rp.m_nProcessThreadID].m_RealTime;

	cb->CV_WorldViewPos = Vec4(viewInfo.renderCamera.vOrigin, viewInfo.bMirrorCull ? -1.0f : 1.0f);
	cb->CV_HPosScale = viewInfo.downscaleFactor;
	cb->CV_ScreenSize = Vec4(viewInfo.viewport.Width,
	                         viewInfo.viewport.Height,
	                         0.5f / (viewInfo.viewport.Width / viewInfo.downscaleFactor.x),
	                         0.5f / (viewInfo.viewport.Height / viewInfo.downscaleFactor.y));

	cb->CV_ViewProjZeroMatr = viewInfo.m_CameraProjZeroMatrix.GetTransposed();
	cb->CV_ViewProjMatr = viewInfo.m_CameraProjMatrix.GetTransposed();
	cb->CV_ViewProjNearestMatr = viewInfo.m_CameraProjNearestMatrix.GetTransposed();
	cb->CV_InvViewProj = viewInfo.m_InvCameraProjMatrix.GetTransposed();
	cb->CV_PrevViewProjMatr = viewInfo.m_PrevCameraProjMatrix.GetTransposed();
	cb->CV_PrevViewProjNearestMatr = viewInfo.m_PrevCameraProjNearestMatrix.GetTransposed();

	cb->CV_SunLightDir = Vec4(rp.m_pSunLight ? rp.m_pSunLight->GetPosition().normalized() : Vec3(0, 0, 0), 1.0f);
	cb->CV_SunColor = Vec4(gEnv->p3DEngine->GetSunColor(), 0.0f);
	cb->CV_SkyColor = Vec4(gEnv->p3DEngine->GetSkyColor(), 1.0f);
	cb->CV_FogColor = Vec4(rp.m_TI[rp.m_nProcessThreadID].m_FS.m_CurColor.toVec3(), perFrameConstants.pVolumetricFogParams.z);
	cb->CV_TerrainInfo = Vec4(gEnv->p3DEngine->GetTerrainTextureMultiplier(), 0, 0, 0);

	cb->CV_AnimGenParams = Vec4(time * 2.0f, time * 0.5f, time * 1.0f, time * 0.125f);

	cb->CV_DecalZFightingRemedy = Vec4(perFrameConstants.pDecalZFightingRemedy, 0);

	cb->CV_CamFrontVector = Vec4(viewInfo.renderCamera.vZ.GetNormalized(), 0);

	// CV_NearFarClipDist
	{
		// Note: CV_NearFarClipDist.z is used to put the weapon's depth range into correct relation to the whole scene
		// when generating the depth texture in the z pass (_RT_NEAREST)
		cb->CV_NearFarClipDist = Vec4(viewInfo.renderCamera.fNear,
		                              viewInfo.renderCamera.fFar,
		                              viewInfo.renderCamera.fFar / gEnv->p3DEngine->GetMaxViewDistance(),
		                              1.0f / viewInfo.renderCamera.fFar);
	}

	// CV_ProjRatio
	{
		float zn = viewInfo.renderCamera.fNear;
		float zf = viewInfo.renderCamera.fFar;
		float hfov = viewInfo.camera.GetHorizontalFov();
		cb->CV_ProjRatio.x = viewInfo.bReverseDepth ? zn / (zn - zf) : zf / (zf - zn);
		cb->CV_ProjRatio.y = viewInfo.bReverseDepth ? zn / (zf - zn) : zn / (zn - zf);
		cb->CV_ProjRatio.z = 1.0f / hfov;
		cb->CV_ProjRatio.w = 1.0f;
	}

	// CV_NearestScaled
	{
		float zn = viewInfo.renderCamera.fNear;
		float zf = viewInfo.renderCamera.fFar;
		float nearZRange = pRenderer->CV_r_DrawNearZRange;
		float camScale = pRenderer->CV_r_DrawNearFarPlane / gEnv->p3DEngine->GetMaxViewDistance();
		cb->CV_NearestScaled.x = viewInfo.bReverseDepth ? 1.0f - zf / (zf - zn) * nearZRange : zf / (zf - zn) * nearZRange;
		cb->CV_NearestScaled.y = viewInfo.bReverseDepth ? zn / (zf - zn) * nearZRange * nearZRange : zn / (zn - zf) * nearZRange * nearZRange;
		cb->CV_NearestScaled.z = viewInfo.bReverseDepth ? 1.0f - (nearZRange - 0.001f) : nearZRange - 0.001f;
		cb->CV_NearestScaled.w = 1.0f;
	}

	// CV_TessInfo
	{
		// We want to obtain the edge length in pixels specified by CV_r_tessellationtrianglesize
		// Therefore the tess factor would depend on the viewport size and CV_r_tessellationtrianglesize
		static const ICVar* pCV_e_TessellationMaxDistance(gEnv->pConsole->GetCVar("e_TessellationMaxDistance"));
		assert(pCV_e_TessellationMaxDistance);

		const float hfov = viewInfo.camera.GetHorizontalFov();
		cb->CV_TessInfo.x = sqrtf(float(viewInfo.viewport.Width * viewInfo.viewport.Height)) / (hfov * pRenderer->CV_r_tessellationtrianglesize);
		cb->CV_TessInfo.y = pRenderer->CV_r_displacementfactor;
		cb->CV_TessInfo.z = pCV_e_TessellationMaxDistance->GetFVal();
		cb->CV_TessInfo.w = (float)CRenderer::CV_r_ParticlesTessellationTriSize;
	}

	cb->CV_FrustumPlaneEquation.SetRow4(0, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_RIGHT]);
	cb->CV_FrustumPlaneEquation.SetRow4(1, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_LEFT]);
	cb->CV_FrustumPlaneEquation.SetRow4(2, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_TOP]);
	cb->CV_FrustumPlaneEquation.SetRow4(3, (Vec4&)viewInfo.pFrustumPlanes[FR_PLANE_BOTTOM]);

	// Shadow specific
	{
		const ShadowMapFrustum* pShadowFrustum = rp.m_ShadowInfo.m_pCurShadowFrustum;
		if (pShadowFrustum && (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_SHADOWGEN))
		{
			cb->CV_ShadowLightPos = Vec4(pShadowFrustum->vLightSrcRelPos + pShadowFrustum->vProjTranslation, 0);
			cb->CV_ShadowViewPos = Vec4(rp.m_ShadowInfo.vViewerPos, 0);
		}
		else
		{
			cb->CV_ShadowLightPos = Vec4(0, 0, 0, 0);
			cb->CV_ShadowViewPos = Vec4(0, 0, 0, 0);
		}
	}
	if (gRenDev->m_pCurWindGrid)
	{
		float fSizeWH = (float)gRenDev->m_pCurWindGrid->m_nWidth * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
		float fSizeHH = (float)gRenDev->m_pCurWindGrid->m_nHeight * gRenDev->m_pCurWindGrid->m_fCellSize * 0.5f;
		cb->CV_WindGridOffset = Vec4(gRenDev->m_pCurWindGrid->m_vCentr.x - fSizeWH, gRenDev->m_pCurWindGrid->m_vCentr.y - fSizeHH, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nWidth, 1.0f / (float)gRenDev->m_pCurWindGrid->m_nHeight);
	}

	pPerViewBuffer = cb.GetDeviceConstantBuffer();
	cb.CopyToDevice();
}

void CStandardGraphicsPipeline::SwitchToLegacyPipeline()
{
	CD3D9Renderer* rd = gcpRendD3D;

	rd->m_nCurStateRS = (uint32) - 1;
	rd->m_nCurStateBL = (uint32) - 1;
	rd->m_nCurStateDP = (uint32) - 1;
	rd->ResetToDefault();
	rd->RT_UnbindResources();
	rd->FX_SetState(0, 0, 0xFFFFFFFF);
	rd->D3DSetCull(eCULL_Back);

	rd->m_CurTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	rd->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	rd->GetDeviceContext().ResetCachedState();

	CTexture::ResetTMUs();
	CHWShader::s_pCurPS = nullptr;
	CHWShader::s_pCurVS = nullptr;
	CHWShader::s_pCurGS = nullptr;
	CHWShader::s_pCurDS = nullptr;
	CHWShader::s_pCurHS = nullptr;
	CHWShader::s_pCurCS = nullptr;

	CCryDeviceWrapper::GetObjectFactory().GetCoreGraphicsCommandList()->Reset();

	// backup viewport we were about to set
	SViewport newViewport = rd->m_NewViewport;

	// Restore engine's render target stack by pushing dummy texture
	rd->FX_PushRenderTarget(0, CTexture::s_ptexHDRTargetScaled[0], NULL);
	rd->FX_PopRenderTarget(0);
	rd->FX_SetActiveRenderTargets();

	// now its safe to finally set viewport
	rd->m_bViewportDirty = true;
	rd->m_CurViewport = SViewport();
	rd->m_NewViewport = newViewport;
	rd->FX_SetViewport();
}

void CStandardGraphicsPipeline::SwitchFromLegacyPipeline()
{
	CCryDeviceWrapper::GetObjectFactory().GetCoreGraphicsCommandList()->Reset();
}

std::array<int, EFSS_MAX> CStandardGraphicsPipeline::GetDefaultMaterialSamplers() const
{
	std::array<int, EFSS_MAX> result =
	{
		{
			gcpRendD3D->m_nMaterialAnisoHighSampler,                                                             // EFSS_ANISO_HIGH
			gcpRendD3D->m_nMaterialAnisoLowSampler,                                                              // EFSS_ANISO_LOW
			CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_WRAP, TADDR_WRAP, TADDR_WRAP, 0x0)),         // EFSS_TRILINEAR
			CTexture::GetTexState(STexState(FILTER_BILINEAR, TADDR_WRAP, TADDR_WRAP, TADDR_WRAP, 0x0)),          // EFSS_BILINEAR
			CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0)),      // EFSS_TRILINEAR_CLAMP
			CTexture::GetTexState(STexState(FILTER_BILINEAR, TADDR_CLAMP, TADDR_CLAMP, TADDR_CLAMP, 0x0)),       // EFSS_BILINEAR_CLAMP
			gcpRendD3D->m_nMaterialAnisoSamplerBorder,                                                           // EFSS_ANISO_HIGH_BORDER
			CTexture::GetTexState(STexState(FILTER_TRILINEAR, TADDR_BORDER, TADDR_BORDER, TADDR_BORDER, 0x0)),   // EFSS_TRILINEAR_BORDER
		}
	};

	return result;
}

void CStandardGraphicsPipeline::RenderScreenSpaceSSS(CTexture* pIrradianceTex)
{
	SwitchFromLegacyPipeline();
	m_pScreenSpaceSSSStage->Execute(pIrradianceTex);
	SwitchToLegacyPipeline();
}

void CStandardGraphicsPipeline::RenderPostAA()
{
	SwitchFromLegacyPipeline();
	m_pPostAAStage->Execute();
	SwitchToLegacyPipeline();
}

void CStandardGraphicsPipeline::ExecuteHDRPostProcessing()
{
	PROFILE_LABEL_SCOPE("HDR_POSTPROCESS");

	CD3D9Renderer* pRenderer = gcpRendD3D;

	pRenderer->GetModelViewMatrix(PostProcessUtils().m_pView.GetData());
	pRenderer->GetProjectionMatrix(PostProcessUtils().m_pProj.GetData());

	PostProcessUtils().m_pViewProj = PostProcessUtils().m_pView * PostProcessUtils().m_pProj;
	if (pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		PostProcessUtils().m_pViewProj = ReverseDepthHelper::Convert(PostProcessUtils().m_pViewProj);
	PostProcessUtils().m_pViewProj.Transpose();

	// Use specular RT as temporary backbuffer when native resolution rendering is enabled; the RT gets popped before upscaling
	if (pRenderer->IsNativeScalingEnabled())
		pRenderer->FX_PushRenderTarget(0, CTexture::s_ptexSceneSpecular, NULL);

	const uint32 nAAMode = pRenderer->FX_GetAntialiasingType();
	CTexture* pDstRT = CTexture::s_ptexSceneDiffuse;
	if ((nAAMode & (eAT_SMAA_MASK | eAT_FXAA_MASK | eAT_MSAA_MASK)) && gRenDev->CV_r_PostProcess && pDstRT)
	{
		assert(pDstRT);
		// Render to intermediate target, to avoid redundant imagecopy/stretchrect for PostAA
		pRenderer->FX_PushRenderTarget(0, pDstRT, &pRenderer->m_DepthBufferOrigMSAA);
		pRenderer->RT_SetViewport(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());
	}

	// Rain
	{
		CSceneRain* pSceneRain = (CSceneRain*)PostEffectMgr()->GetEffect(ePFX_SceneRain);
		const SRainParams& rainInfo = gcpRendD3D->m_p3DEngineCommon.m_RainInfo;
		if (pSceneRain && pSceneRain->IsActive() && rainInfo.fRainDropsAmount > 0.01f)
		{
			SwitchToLegacyPipeline();
			pSceneRain->Render();
		}
	}

	SwitchFromLegacyPipeline();

	// Note: MB uses s_ptexHDRTargetPrev to avoid doing another copy, so this should be right before the MB pass
	{
		static CStretchRectPass* s_passCopyRT = nullptr;
		if (!m_bUtilityPassesInitialized) s_passCopyRT = CreateStaticUtilityPass<CStretchRectPass>();

		s_passCopyRT->Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetPrev);
	}

	m_pDepthOfFieldStage->Execute();

	m_pMotionBlurStage->Execute();

	// Snow
	{
		CSceneSnow* pSceneSnow = (CSceneSnow*)PostEffectMgr()->GetEffect(ePFX_SceneSnow);
		if (pSceneSnow->IsActiveSnow())
		{
			SwitchToLegacyPipeline();
			pSceneSnow->Render();
			SwitchFromLegacyPipeline();
		}
	}

	// Half resolution downsampling
	{
		PROFILE_LABEL_SCOPE("HALFRES_DOWNSAMPLE_HDRTARGET");
		static CStableDownsamplePass* s_passStableDownsample = nullptr;
		if (!m_bUtilityPassesInitialized) s_passStableDownsample = CreateStaticUtilityPass<CStableDownsamplePass>();

		s_passStableDownsample->Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetScaled[0], true);
	}

	// Quarter resolution downsampling
	{
		PROFILE_LABEL_SCOPE("QUARTER_RES_DOWNSAMPLE_HDRTARGET");
		static CStableDownsamplePass* s_passStableDownsample = nullptr;
		if (!m_bUtilityPassesInitialized) s_passStableDownsample = CreateStaticUtilityPass<CStableDownsamplePass>();

		bool bKillFireFlies = CRenderer::CV_r_HDRBloomQuality >= 1;
		s_passStableDownsample->Execute(CTexture::s_ptexHDRTargetScaled[0], CTexture::s_ptexHDRTargetScaled[1], bKillFireFlies);
	}

	m_pAutoExposureStage->Execute();

	m_pBloomStage->Execute();

	// Lens optics
	{
		pRenderer->m_RP.m_PersFlags2 &= ~RBPF2_LENS_OPTICS_COMPOSITE;
		if (CRenderer::CV_r_flares)
		{
			const uint32 nBatchMask = SRendItem::BatchFlags(EFSLIST_LENSOPTICS);
			if (nBatchMask & (FB_GENERAL | FB_TRANSPARENT))
			{
				PROFILE_LABEL_SCOPE("LENS_OPTICS");

				SwitchToLegacyPipeline();

				CTexture* pLensOpticsComposite = CTexture::s_ptexSceneTargetR11G11B10F[0];
				pRenderer->FX_ClearTarget(pLensOpticsComposite, Clr_Transparent);
				pRenderer->FX_PushRenderTarget(0, pLensOpticsComposite, 0);

				pRenderer->m_RP.m_PersFlags2 |= RBPF2_NOPOSTAA | RBPF2_LENS_OPTICS_COMPOSITE;

				GetUtils().Log(" +++ Begin lens-optics scene +++ \n");
				pRenderer->FX_ProcessRenderList(EFSLIST_LENSOPTICS, FB_GENERAL);
				pRenderer->FX_ProcessRenderList(EFSLIST_LENSOPTICS, FB_TRANSPARENT);
				pRenderer->FX_ResetPipe();
				GetUtils().Log(" +++ End lens-optics scene +++ \n");

				pRenderer->FX_SetActiveRenderTargets();
				pRenderer->FX_PopRenderTarget(0);
				pRenderer->FX_SetActiveRenderTargets();

				SwitchFromLegacyPipeline();
			}
		}
	}

	m_pSunShaftsStage->Execute();

	m_pToneMappingStage->Execute();
}

void CStandardGraphicsPipeline::Execute()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CRenderView* pMainRenderView = pRenderer->m_RP.m_pCurrentRenderView;
	m_pCurrentRenderView = pMainRenderView;
	m_numInvalidDrawcalls = 0;

	void (* pRenderFunc)() = &pRenderer->FX_FlushShader_General;
	pRenderer->m_RP.m_pRenderFunc = pRenderFunc;

	pRenderer->RT_SetCameraInfo();

	SwitchToLegacyPipeline();
	pRenderer->FX_DeferredRainPreprocess();
	m_pComputeSkinningStage->Execute(m_pCurrentRenderView);
	SwitchFromLegacyPipeline();

	gcpRendD3D->GetS3DRend().TryInjectHmdCameraAsync(pMainRenderView);

	// GBuffer
	m_pSceneGBufferStage->Execute();

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		SwitchToLegacyPipeline();
		m_pShadowMapStage->Prepare(pMainRenderView);
		SwitchFromLegacyPipeline();
	}

	if (pRenderer->m_nGraphicsPipeline >= 2)
	{
		// Wait for GBuffer draw jobs to finish
		GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();
	}

	// Shadow maps
	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pShadowMapStage->Execute();
	}

	m_pSceneGBufferStage->ExecuteLinearizeDepth();

	// Depth downsampling
	{
		CTexture* pZTexture = gcpRendD3D->m_DepthBufferOrigMSAA.pTexture;

		static CDepthDownsamplePass* s_passDepthDownsample2 = nullptr;
		if (!m_bUtilityPassesInitialized) s_passDepthDownsample2 = CreateStaticUtilityPass<CDepthDownsamplePass>();

		static CDepthDownsamplePass* s_passDepthDownsample4 = nullptr;
		if (!m_bUtilityPassesInitialized) s_passDepthDownsample4 = CreateStaticUtilityPass<CDepthDownsamplePass>();

		CTexture* pSourceDepth = CTexture::s_ptexZTarget;
#if CRY_PLATFORM_DURANGO
		pSourceDepth = pZTexture;  // On Durango reading device depth is faster since it is in ESRAM
#endif

		s_passDepthDownsample2->Execute(pSourceDepth, CTexture::s_ptexZTargetScaled, (pSourceDepth == pZTexture), true);
		s_passDepthDownsample4->Execute(CTexture::s_ptexZTargetScaled, CTexture::s_ptexZTargetScaled2, false, false);
	}

	SwitchToLegacyPipeline();

	// Deferred decals
	{
		pRenderer->FX_PushRenderTarget(0, CTexture::s_ptexSceneNormalsMap, &pRenderer->m_DepthBufferOrigMSAA);
		pRenderer->FX_PushRenderTarget(1, CTexture::s_ptexSceneDiffuse, NULL);
		pRenderer->FX_PushRenderTarget(2, CTexture::s_ptexSceneSpecular, NULL);

		pRenderer->FX_DeferredDecals();

		pRenderer->FX_PopRenderTarget(2);
		pRenderer->FX_PopRenderTarget(1);
		pRenderer->FX_PopRenderTarget(0);
	}

	// Depth readback (for occlusion culling)
	{
		pRenderer->FX_ZTargetReadBack();
	}

	// GBuffer modifiers
	{
		pRenderer->FX_DeferredRainGBuffer();
		pRenderer->FX_DeferredSnowLayer();
	}

	// Generate cloud volume textures for shadow mapping.
	pRenderer->GetVolumetricCloud().GenerateVolumeTextures();

	// SVOGI
	{
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance())
		{
			PROFILE_LABEL_SCOPE("SVOGI");
			SwitchToLegacyPipeline();
			CSvoRenderer::GetInstance()->UpdateCompute();
			CSvoRenderer::GetInstance()->UpdateRender();
			SwitchFromLegacyPipeline();
		}
#endif
	}

	// Screen Space Reflections
	m_pScreenSpaceReflectionsStage->Execute();

	if (pRenderer->m_nGraphicsPipeline >= 2)
	{
		// Wait for Shadow Map draw jobs to finish (also required for HeightMap AO)
		GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();
	}

	// Height Map AO
	ShadowMapFrustum* pHeightMapFrustum = nullptr;
	CTexture* pHeightMapAOScreenDepthTex = nullptr;
	CTexture* pHeightMapAOTex = nullptr;
	m_pHeightMapAOStage->Execute(pHeightMapFrustum, pHeightMapAOScreenDepthTex, pHeightMapAOTex);

	// Screen Space Obscurance
	m_pScreenSpaceObscuranceStage->Execute(pHeightMapFrustum, pHeightMapAOScreenDepthTex, pHeightMapAOTex);

	pRenderer->m_RP.m_PersFlags2 |= RBPF2_NOSHADERFOG;
	SwitchToLegacyPipeline();

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");
		pRenderer->FX_ProcessRenderList(EFSLIST_DEFERRED_PREPROCESS, pRenderFunc, false);
	}

	SwitchToLegacyPipeline();

	{
		PROFILE_FRAME(WaitForParticleRendItems);
		gEnv->pJobManager->WaitForJob(pRenderer->m_ComputeVerticesJobState);
		pRenderer->UnLockParticleVideoMemory();
	}

	// Opaque forward passes
	m_pSceneForwardStage->Execute_Opaque();

	// Deferred water caustics
	{
		pRenderer->FX_ResetPipe();
		pRenderer->FX_DeferredCaustics();
	}

	// Fog
	{
		{
			PROFILE_LABEL_SCOPE("VOLUMETRIC FOG");
			pRenderer->GetVolumetricFog().RenderVolumetricsToVolume(pRenderFunc);
			pRenderer->GetVolumetricFog().RenderVolumetricFog(pMainRenderView);
		}

		pRenderer->FX_RenderFog();
	}

	// Clouds
	{
		pRenderer->GetVolumetricCloud().RenderClouds();
	}

	// Water volumes
	{
		pRenderer->FX_ProcessRenderList(EFSLIST_WATER_VOLUMES, pRenderFunc, false);
	}

	pRenderer->UpdatePrevMatrix(true);

	// Transparent (below water)
	m_pSceneForwardStage->Execute_TransparentBelowWater();

	// Ocean and water volumes
	pRenderer->FX_RenderWater(pRenderFunc);

	// Transparent (above water)
	m_pSceneForwardStage->Execute_TransparentAboveWater();

	if (CRenderer::CV_r_TranspDepthFixup)
		pRenderer->FX_DepthFixupMerge();

	// Half-res particles
	{
		pRenderer->FX_ProcessHalfResParticlesRenderList(pMainRenderView, EFSLIST_HALFRES_PARTICLES, pRenderFunc, true);
	}

	pRenderer->m_CameraProjMatrixPrev = pRenderer->m_CameraProjMatrix;

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence();

	pRenderer->FX_DeferredSnowDisplacement();

	// Pop HDR target from RT stack
	{
		assert(pRenderer->m_RTStack[0][pRenderer->m_nRTStackLevel[0]].m_pTex == CTexture::s_ptexHDRTarget);
		pRenderer->FX_SetActiveRenderTargets();
		pRenderer->RT_UnbindTMUs();
		pRenderer->FX_PopRenderTarget(0);
		//pRenderer->EF_ClearTargetsLater(0);
	}

	if (!(pRenderer->m_RP.m_nRendFlags & SHDF_CUBEMAPGEN))
	{
		ExecuteHDRPostProcessing();

		// HDR and LDR post-processing
		{
			pRenderer->m_RP.m_PersFlags1 &= ~RBPF1_SKIP_AFTER_POST_PROCESS;
			SwitchToLegacyPipeline();

			pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, pRenderFunc, false);
			pRenderer->FX_ProcessRenderList(EFSLIST_POSTPROCESS, pRenderFunc, false);

			pRenderer->RT_SetViewport(0, 0, pRenderer->GetWidth(), pRenderer->GetHeight());

			pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, pRenderFunc, false);
		}

		if (CRenderer::CV_r_DeferredShadingDebug)
			pRenderer->FX_DeferredRendering(pMainRenderView, true);
	}

	m_bUtilityPassesInitialized = true;
}
