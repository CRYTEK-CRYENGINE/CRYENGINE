// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

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

#include "DepthReadback.h"
#include "Common/TypedConstantBuffer.h"
#include "Common/Textures/TextureHelpers.h"
#include "Common/Include_HLSL_CPP_Shared.h"
#include "../Common/RenderView.h"

#include "DriverD3D.h" //for gcpRendD3D

#include "../D3DPostProcess.h"
#include "Common/ReverseDepth.h"
#include "D3D_SVO.h"

//////////////////////////////////////////////////////////////////////////
CStandardGraphicsPipeline::SViewInfo::SViewInfo()
	: pRenderCamera(nullptr)
	, pCamera(nullptr)
	, pFrustumPlanes(nullptr)
	, cameraProjZeroMatrix(IDENTITY)
	, cameraProjMatrix(IDENTITY)
	, cameraProjNearestMatrix(IDENTITY)
	, projMatrix(IDENTITY)
	, viewMatrix(IDENTITY)
	, invCameraProjMatrix(IDENTITY)
	, invViewMatrix(IDENTITY)
	, prevCameraMatrix(IDENTITY)
	, prevCameraProjMatrix(IDENTITY)
	, prevCameraProjNearestMatrix(IDENTITY)
	, downscaleFactor(1)
	, flags(eFlags_None)
{
}

void CStandardGraphicsPipeline::SViewInfo::SetCamera(const CRenderCamera& renderCam, const CCamera& cam, const CCamera& previousCam, Vec2 subpixelShift, float drawNearestFov, float drawNearestFarPlane, Plane obliqueClippingPlane)
{
	pCamera = &cam;
	pRenderCamera = &renderCam;

	Matrix44A proj, nearestProj;
	{
		const CRenderCamera& cam = renderCam;

		if (flags & eFlags_ReverseDepth) mathMatrixPerspectiveOffCenterReverseDepth((Matrix44A*)&proj, cam.fWL, cam.fWR, cam.fWB, cam.fWT, cam.fNear, cam.fFar);
		else                             mathMatrixPerspectiveOffCenter((Matrix44A*)&proj, cam.fWL, cam.fWR, cam.fWB, cam.fWT, cam.fNear, cam.fFar);

		if (flags & eFlags_ObliqueFrustumClipping)
		{
			Matrix44A mObliqueProjMatrix(IDENTITY);
			mObliqueProjMatrix.m02 = obliqueClippingPlane.n[0];
			mObliqueProjMatrix.m12 = obliqueClippingPlane.n[1];
			mObliqueProjMatrix.m22 = obliqueClippingPlane.n[2];
			mObliqueProjMatrix.m32 = obliqueClippingPlane.d;

			proj = proj * mObliqueProjMatrix;
		}

		if (flags & eFlags_SubpixelShift)
		{
			proj.m20 += subpixelShift.x;
			proj.m21 += subpixelShift.y;
		}

		nearestProj = GetNearestProjection(drawNearestFov, drawNearestFarPlane, subpixelShift);
	}

	Matrix44 view, viewZero, invView;
	ExtractViewMatrices(cam, view, viewZero, invView);

	Matrix44 prevView, prevViewZero, prevInvView;
	ExtractViewMatrices(previousCam, prevView, prevViewZero, prevInvView);

	cameraProjMatrix = view * proj;
	cameraProjZeroMatrix = viewZero * proj;
	projMatrix = proj;
	viewMatrix = view;
	invViewMatrix = invView;
	cameraProjNearestMatrix = viewZero * nearestProj;

	prevCameraMatrix = prevView;
	prevCameraProjMatrix = prevView * proj;
	prevCameraProjNearestMatrix = prevView;
	prevCameraProjNearestMatrix.SetRow(3, Vec3(ZERO));
	prevCameraProjNearestMatrix = prevCameraProjNearestMatrix * nearestProj;

	Matrix44_tpl<f64> mProjInv;
	if (mathMatrixPerspectiveFovInverse(&mProjInv, &proj))
	{
		Matrix44_tpl<f64> mViewInv;
		mathMatrixLookAtInverse(&mViewInv, &view);
		invCameraProjMatrix = mProjInv * mViewInv;
	}
	else
	{
		invCameraProjMatrix = cameraProjMatrix.GetInverted();
	}
}

void CStandardGraphicsPipeline::SViewInfo::SetLegacyCamera(CD3D9Renderer* pRenderer, const Matrix44& previousFrameCameraMatrix)
{
	SRenderPipeline& RESTRICT_REFERENCE rp = pRenderer->m_RP;

	pCamera = &pRenderer->GetCamera();
	pRenderCamera = &pRenderer->GetRCamera();

	Matrix44 nearestProj = GetNearestProjection(pRenderer->GetDrawNearestFOV(), pRenderer->CV_r_DrawNearFarPlane, pRenderer->m_vProjMatrixSubPixoffset);

	cameraProjZeroMatrix = pRenderer->m_CameraProjZeroMatrix;
	cameraProjMatrix = pRenderer->m_CameraProjMatrix;
	projMatrix = pRenderer->m_ProjMatrix;
	viewMatrix = pRenderer->m_ViewMatrix;
	Matrix44_tpl<f64> invView64;
	mathMatrixLookAtInverse(&invView64, &viewMatrix);
	invViewMatrix = invView64;
	invCameraProjMatrix = pRenderer->m_InvCameraProjMatrix;
	cameraProjNearestMatrix = pRenderer->m_CameraZeroMatrix * nearestProj;

	prevCameraMatrix = previousFrameCameraMatrix;
	prevCameraProjMatrix = previousFrameCameraMatrix * pRenderer->m_ProjMatrix;
	prevCameraProjNearestMatrix = previousFrameCameraMatrix;
	prevCameraProjNearestMatrix.SetRow(3, Vec3(ZERO));
	prevCameraProjNearestMatrix = prevCameraProjNearestMatrix * nearestProj;
}

//////////////////////////////////////////////////////////////////////////

void CStandardGraphicsPipeline::SViewInfo::ExtractViewMatrices(const CCamera& cam, Matrix44& view, Matrix44& viewZero, Matrix44& invView) const
{
	Matrix34_tpl<f64> mCam34 = cam.GetMatrix();
	mCam34.OrthonormalizeFast();

	Matrix44_tpl<f64> mCam44T = mCam34.GetTransposed();

	// Rotate around x-axis by -PI/2
	Vec4_tpl<f64> row1 = mCam44T.GetRow4(1);
	mCam44T.SetRow4(1, mCam44T.GetRow4(2));
	mCam44T.SetRow4(2, Vec4_tpl<f64>(-row1.x, -row1.y, -row1.z, -row1.w));

	if (flags & eFlags_MirrorCamera)
	{
		Vec4_tpl<f64> _row1 = mCam44T.GetRow4(1);
		mCam44T.SetRow4(1, Vec4_tpl<f64>(-_row1.x, -_row1.y, -_row1.z, -_row1.w));
	}
	invView = (Matrix44_tpl<f32> )mCam44T;

	Matrix44_tpl<f64> mView64;
	mathMatrixLookAtInverse(&mView64, &mCam44T);
	view = (Matrix44_tpl<f32> )mView64;

	viewZero = view;
	viewZero.SetRow(3, Vec3(ZERO));
}

//////////////////////////////////////////////////////////////////////////

Matrix44 CStandardGraphicsPipeline::SViewInfo::GetNearestProjection(float nearestFOV, float farPlane, Vec2 subpixelShift)
{
	CRY_ASSERT(pCamera);

	Matrix44A result;

	float fFov = pCamera->GetFov();
	if (nearestFOV > 1.0f && nearestFOV < 179.0f)
		fFov = DEG2RAD(nearestFOV);

	float fNearRatio = DRAW_NEAREST_MIN / pCamera->GetNearPlane();
	float wT = tanf(fFov * 0.5f) * DRAW_NEAREST_MIN, wB = -wT;
	float wR = wT * pCamera->GetProjRatio(), wL = -wR;

	wL += pCamera->GetAsymL() * fNearRatio;
	wR += pCamera->GetAsymR() * fNearRatio;
	wB += pCamera->GetAsymB() * fNearRatio, wT += pCamera->GetAsymT() * fNearRatio;

	if (flags & eFlags_ReverseDepth) mathMatrixPerspectiveOffCenterReverseDepth(&result, wL, wR, wB, wT, DRAW_NEAREST_MIN, farPlane);
	else                             mathMatrixPerspectiveOffCenter(&result, wL, wR, wB, wT, DRAW_NEAREST_MIN, farPlane);

	if (flags & eFlags_SubpixelShift)
	{
		result.m20 += subpixelShift.x;
		result.m21 += subpixelShift.y;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////
CRenderView* CGraphicsPipelineStage::RenderView()
{
	return gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView();
}

CStandardGraphicsPipeline::CStandardGraphicsPipeline()
	: m_changedCVars(gEnv->pConsole)
	, m_defaultMaterialBindPoints()
	, m_defaultInstanceExtraResources()
{}

void CStandardGraphicsPipeline::Init()
{
	// Initialize all subsequently created passes with the correct stereo-resources
	if (gcpRendD3D->GetS3DRend().IsStereoEnabled())
	{
		gcpRendD3D->m_RP.m_nRendFlags |= (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE);
	}

	// default material bind points
	{
		m_defaultMaterialBindPoints.SetConstantBuffer(eConstantBufferShaderSlot_PerMaterial, CDeviceBufferManager::GetNullConstantBuffer(), EShaderStage_AllWithoutCompute);

		for (EEfResTextures texType = EFTT_DIFFUSE; texType < EFTT_MAX; texType = EEfResTextures(texType + 1))
		{
			m_defaultMaterialBindPoints.SetTexture(texType, CTexture::s_pTexNULL, EDefaultResourceViews::Default, EShaderStage_AllWithoutCompute);
		}
	}

	// default extra per instance
	{
		EShaderStage shaderStages = EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Hull | EShaderStage_Domain;

		m_defaultInstanceExtraResources.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat, CDeviceBufferManager::GetNullConstantBuffer(), shaderStages);
		m_defaultInstanceExtraResources.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, CDeviceBufferManager::GetNullConstantBuffer(), shaderStages);
		m_defaultInstanceExtraResources.SetBuffer(EReservedTextureSlot_SkinExtraWeights, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, shaderStages);
		m_defaultInstanceExtraResources.SetBuffer(EReservedTextureSlot_AdjacencyInfo, CDeviceBufferManager::GetNullBufferTyped(), EDefaultResourceViews::Default, shaderStages);    // shares shader slot with EReservedTextureSlot_PatchID
		m_defaultInstanceExtraResources.SetBuffer(EReservedTextureSlot_ComputeSkinVerts, CDeviceBufferManager::GetNullBufferStructured(), EDefaultResourceViews::Default, shaderStages); // shares shader slot with EReservedTextureSlot_PatchID

		m_pDefaultInstanceExtraResourceSet = GetDeviceObjectFactory().CreateResourceSet();
		m_pDefaultInstanceExtraResourceSet->Update(m_defaultInstanceExtraResources);
	}

	// per view constant buffer
	m_mainViewConstantBuffer.CreateDeviceBuffer();

	// Register scene stages that make use of the global PSO cache
	RegisterSceneStage<CShadowMapStage, eStage_ShadowMap>(m_pShadowMapStage);
	RegisterSceneStage<CSceneGBufferStage, eStage_SceneGBuffer>(m_pSceneGBufferStage);
	RegisterSceneStage<CSceneForwardStage, eStage_SceneForward>(m_pSceneForwardStage);
	RegisterSceneStage<CSceneCustomStage, eStage_SceneCustom>(m_pSceneCustomStage);

	// Register all other stages that don't need the global PSO cache
	RegisterStage<CHeightMapAOStage>(m_pHeightMapAOStage, eStage_HeightMapAO);
	RegisterStage<CScreenSpaceObscuranceStage>(m_pScreenSpaceObscuranceStage, eStage_ScreenSpaceObscurance);
	RegisterStage<CScreenSpaceReflectionsStage>(m_pScreenSpaceReflectionsStage, eStage_ScreenSpaceReflections);
	RegisterStage<CScreenSpaceSSSStage>(m_pScreenSpaceSSSStage, eStage_ScreenSpaceSSS);
	RegisterStage<CVolumetricFogStage>(m_pVolumetricFogStage, eStage_VolumetricFog);
	RegisterStage<CFogStage>(m_pFogStage, eStage_Fog);
	RegisterStage<CVolumetricCloudsStage>(m_pVolumetricCloudsStage, eStage_VolumetricClouds);
	RegisterStage<CWaterRipplesStage>(m_pWaterRipplesStage, eStage_WaterRipples);
	RegisterStage<CWaterStage>(m_pWaterStage, eStage_Water);
	RegisterStage<CMotionBlurStage>(m_pMotionBlurStage, eStage_MotionBlur);
	RegisterStage<CDepthOfFieldStage>(m_pDepthOfFieldStage, eStage_DepthOfField);
	RegisterStage<CAutoExposureStage>(m_pAutoExposureStage, eStage_AutoExposure);
	RegisterStage<CBloomStage>(m_pBloomStage, eStage_Bloom);
	RegisterStage<CColorGradingStage>(m_pColorGradingStage, eStage_ColorGrading);
	RegisterStage<CToneMappingStage>(m_pToneMappingStage, eStage_ToneMapping);
	RegisterStage<CSunShaftsStage>(m_pSunShaftsStage, eStage_Sunshafts);
	RegisterStage<CPostAAStage>(m_pPostAAStage, eStage_PostAA);
	RegisterStage<CComputeSkinningStage>(m_pComputeSkinningStage, eStage_ComputeSkinning);
	RegisterStage<CGpuParticlesStage>(m_pGpuParticlesStage, eStage_GpuParticles);
	RegisterStage<CDeferredDecalsStage>(m_pDeferredDecalsStage, eStage_DeferredDecals);
	RegisterStage<CClipVolumesStage>(m_pClipVolumesStage, eStage_ClipVolumes);
	RegisterStage<CShadowMaskStage>(m_pShadowMaskStage, eStage_ShadowMask);
	RegisterStage<CTiledShadingStage>(m_pTiledShadingStage, eStage_TiledShading);
	RegisterStage<CLensOpticsStage>(m_pLensOpticsStage, eStage_LensOptics);
	RegisterStage<CPostEffectStage>(m_pPostEffectStage, eStage_PostEffet);
	RegisterStage<CRainStage>(m_pRainStage, eStage_Rain);
	RegisterStage<CSnowStage>(m_pSnowStage, eStage_Snow);
	RegisterStage<CDepthReadbackStage>(m_pDepthReadbackStage, eStage_DepthReadback);
	RegisterStage<CMobileCompositionStage>(m_pMobileCompositionStage, eStage_MobileComposition);

	// Now init stages
	InitStages();

	// Out-of-pipeline passes for display
	m_DownscalePass.reset(new CDownsamplePass);
	m_UpscalePass  .reset(new CSharpeningUpsamplePass);

	if (gcpRendD3D->GetS3DRend().IsStereoEnabled())
	{
		gcpRendD3D->m_RP.m_nRendFlags &= ~(SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE);
	}
}

void CStandardGraphicsPipeline::Prepare(CRenderView* pRenderView, EShaderRenderingFlags renderingFlags)
{
	m_pCurrentRenderView = pRenderView;
	m_renderingFlags = renderingFlags;
	m_numInvalidDrawcalls = 0;

	if (!m_changedCVars.GetCVars().empty())
	{
		for (int i = 0; i < eStage_Count; ++i)
			m_pipelineStages[i]->OnCVarsChanged(m_changedCVars);

		m_changedCVars.Reset();
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

int CStandardGraphicsPipeline::GetViewInfoCount() const
{
	int viewInfoCount = 0;

	if (m_pCurrentRenderView) // called from within CStandardGraphicsPipeline::Execute() scope => use renderview cameras
	{
		for (CCamera::EEye eye = CCamera::eEye_Left; eye != CCamera::eEye_eCount; eye = CCamera::EEye(eye + 1))
		{
			uint32 renderingFlags = m_renderingFlags;

			if ((renderingFlags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE)) == 0) // non-stereo case
				renderingFlags |= SHDF_STEREO_LEFT_EYE;

			uint32 currentEyeFlag = eye == CCamera::eEye_Left ? SHDF_STEREO_LEFT_EYE : SHDF_STEREO_RIGHT_EYE;

			if (renderingFlags & currentEyeFlag)
			{
				++viewInfoCount;
			}
		}
	}
	else
	{
		++viewInfoCount;
	}

	return viewInfoCount;
}

int CStandardGraphicsPipeline::GetViewInfo(CStandardGraphicsPipeline::SViewInfo viewInfo[2], const D3DViewPort* pCustomViewport)
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rp = gRenDev->m_RP;

	SViewInfo::eFlags viewFlags = SViewInfo::eFlags_None;
	if (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) viewFlags |= SViewInfo::eFlags_ReverseDepth;
	if (rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_MIRRORCULL)    viewFlags |= SViewInfo::eFlags_MirrorCull;
	if (!(rp.m_TI[rp.m_nProcessThreadID].m_PersFlags & RBPF_DRAWTOTEXTURE) && !(rp.m_PersFlags2 & RBPF2_NOPOSTAA))
		viewFlags |= SViewInfo::eFlags_SubpixelShift;

	SViewport viewport = pRenderer->GetViewport();

	if (pCustomViewport)
	{
		viewport.nX = int(pCustomViewport->TopLeftX);
		viewport.nY = int(pCustomViewport->TopLeftY);
		viewport.nWidth = int(pCustomViewport->Width);
		viewport.nHeight = int(pCustomViewport->Height);
	}

	int viewInfoCount = 0;

	if (m_pCurrentRenderView) // called from within CStandardGraphicsPipeline::Execute() scope => use renderview cameras
	{
		for (CCamera::EEye eye = CCamera::eEye_Left; eye != CCamera::eEye_eCount; eye = CCamera::EEye(eye + 1))
		{
			uint32 renderingFlags = m_renderingFlags;

			if ((renderingFlags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE)) == 0) // non-stereo case
				renderingFlags |= SHDF_STEREO_LEFT_EYE;

			uint32 currentEyeFlag = eye == CCamera::eEye_Left ? SHDF_STEREO_LEFT_EYE : SHDF_STEREO_RIGHT_EYE;

			if (renderingFlags & currentEyeFlag)
			{
				auto& cam = m_pCurrentRenderView->GetCamera(eye);
				auto& previousCam = m_pCurrentRenderView->GetPreviousCamera(eye);
				auto& rcam = m_pCurrentRenderView->GetRenderCamera(eye);

				viewInfo[viewInfoCount].flags = viewFlags;
				viewInfo[viewInfoCount].SetCamera(rcam, cam, previousCam, pRenderer->m_vProjMatrixSubPixoffset,
				                                  pRenderer->GetDrawNearestFOV(), pRenderer->CV_r_DrawNearFarPlane, rp.m_TI[rp.m_nProcessThreadID].m_pObliqueClipPlane);
				viewInfo[viewInfoCount].viewport = viewport;
				viewInfo[viewInfoCount].pFrustumPlanes = cam.GetFrustumPlane(0);
				viewInfo[viewInfoCount].downscaleFactor = Vec4(rp.m_CurDownscaleFactor.x, rp.m_CurDownscaleFactor.y, pRenderer->m_PrevViewportScale.x, pRenderer->m_PrevViewportScale.y);

				++viewInfoCount;
			}
		}
	}
	else
	{
		viewInfo[viewInfoCount].flags = viewFlags;
		viewInfo[viewInfoCount].SetLegacyCamera(pRenderer, pRenderer->GetPreviousFrameCameraMatrix());
		viewInfo[viewInfoCount].viewport = viewport;
		viewInfo[viewInfoCount].downscaleFactor = Vec4(rp.m_CurDownscaleFactor.x, rp.m_CurDownscaleFactor.y, pRenderer->m_PrevViewportScale.x, pRenderer->m_PrevViewportScale.y);
		viewInfo[viewInfoCount].pFrustumPlanes = pRenderer->GetCamera().GetFrustumPlane(0);

		++viewInfoCount;
	}

	return viewInfoCount;
}

void CStandardGraphicsPipeline::UpdateMainViewConstantBuffer()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rp = gRenDev->m_RP;

	SViewInfo viewInfo[2];
	int viewInfoCount = GetViewInfo(viewInfo, nullptr);
	UpdatePerViewConstantBuffer(viewInfo, viewInfoCount, m_mainViewConstantBuffer.GetDeviceConstantBuffer());
}

void CStandardGraphicsPipeline::UpdatePerViewConstantBuffer(const SViewInfo* pViewInfo, int viewInfoCount, CConstantBufferPtr pPerViewBuffer)
{
	if (!gEnv->p3DEngine || !pPerViewBuffer)
		return;

	CD3D9Renderer* pRenderer = gcpRendD3D;
	SRenderPipeline& RESTRICT_REFERENCE rp = gRenDev->m_RP;
	const SCGParamsPF& perFrameConstants = pRenderer->m_cEF.m_PF[rp.m_nProcessThreadID];
	CryStackAllocWithSizeVectorCleared(HLSL_PerViewGlobalConstantBuffer, viewInfoCount, bufferData, CDeviceBufferManager::AlignBufferSizeForStreaming);

	for (int i = 0; i < viewInfoCount; ++i)
	{
		CRY_ASSERT(pViewInfo[i].pCamera && pViewInfo[i].pRenderCamera);

		const SViewInfo& viewInfo = pViewInfo[i];
		const auto& camera = *viewInfo.pRenderCamera;
		HLSL_PerViewGlobalConstantBuffer& cb = bufferData[i];

		const float time = rp.m_TI[rp.m_nProcessThreadID].m_RealTime;
		const bool bReverseDepth = (viewInfo.flags & CStandardGraphicsPipeline::SViewInfo::eFlags_ReverseDepth) != 0;

		cb.CV_HPosScale = viewInfo.downscaleFactor;
		cb.CV_ScreenSize = Vec4(float(viewInfo.viewport.nWidth),
		                        float(viewInfo.viewport.nHeight),
		                        0.5f / (viewInfo.viewport.nWidth / viewInfo.downscaleFactor.x),
		                        0.5f / (viewInfo.viewport.nHeight / viewInfo.downscaleFactor.y));

		cb.CV_ViewProjZeroMatr = viewInfo.cameraProjZeroMatrix.GetTransposed();
		cb.CV_ViewProjMatr = viewInfo.cameraProjMatrix.GetTransposed();
		cb.CV_ViewProjNearestMatr = viewInfo.cameraProjNearestMatrix.GetTransposed();
		cb.CV_InvViewProj = viewInfo.invCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjMatr = viewInfo.prevCameraProjMatrix.GetTransposed();
		cb.CV_PrevViewProjNearestMatr = viewInfo.prevCameraProjNearestMatrix.GetTransposed();
		cb.CV_ViewMatr = viewInfo.viewMatrix.GetTransposed();
		cb.CV_InvViewMatr = viewInfo.invViewMatrix.GetTransposed();

		Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
		CShadowUtils::ProjectScreenToWorldExpansionBasis(Matrix44(IDENTITY), *viewInfo.pCamera, pRenderer->m_vProjMatrixSubPixoffset,
		                                                 float(viewInfo.viewport.nWidth), float(viewInfo.viewport.nHeight), vWBasisX, vWBasisY, vWBasisZ, vCamPos, true, NULL);

		cb.CV_ScreenToWorldBasis.SetColumn(0, Vec3r(vWBasisX));
		cb.CV_ScreenToWorldBasis.SetColumn(1, Vec3r(vWBasisY));
		cb.CV_ScreenToWorldBasis.SetColumn(2, Vec3r(vWBasisZ));
		cb.CV_ScreenToWorldBasis.SetColumn(3, camera.vOrigin);

		cb.CV_SunLightDir = Vec4(perFrameConstants.pSunDirection, 1.0f);
		cb.CV_SunColor = Vec4(perFrameConstants.pSunColor, perFrameConstants.sunSpecularMultiplier);
		cb.CV_SkyColor = Vec4(perFrameConstants.pSkyColor, 1.0f);
		cb.CV_FogColor = Vec4(rp.m_TI[rp.m_nProcessThreadID].m_FS.m_CurColor.toVec3(), perFrameConstants.pVolumetricFogParams.z);
		cb.CV_TerrainInfo = Vec4(gEnv->p3DEngine->GetTerrainTextureMultiplier(), 0, 0, 0);

		cb.CV_AnimGenParams = Vec4(time * 2.0f, time * 0.25f, time * 1.0f, time * 0.125f);

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

		cb.CV_CamRightVector = Vec4(camera.vX.GetNormalized(), 0);
		cb.CV_CamFrontVector = Vec4(camera.vZ.GetNormalized(), 0);
		cb.CV_CamUpVector = Vec4(camera.vY.GetNormalized(), 0);
		cb.CV_WorldViewPosition = Vec4(camera.vOrigin, 0);

		// CV_NearFarClipDist
		{
			// Note: CV_NearFarClipDist.z is used to put the weapon's depth range into correct relation to the whole scene
			// when generating the depth texture in the z pass (_RT_NEAREST)
			cb.CV_NearFarClipDist = Vec4(
				camera.fNear,
				camera.fFar,
				camera.fFar / gEnv->p3DEngine->GetMaxViewDistance(),
				1.0f / camera.fFar);
		}

		// CV_ProjRatio
		{
			float zn = camera.fNear;
			float zf = camera.fFar;
			float hfov = viewInfo.pCamera->GetHorizontalFov();
			cb.CV_ProjRatio.x = bReverseDepth ? zn / (zn - zf) : zf / (zf - zn);
			cb.CV_ProjRatio.y = bReverseDepth ? zn / (zf - zn) : zn / (zn - zf);
			cb.CV_ProjRatio.z = 1.0f / hfov;
			cb.CV_ProjRatio.w = 1.0f;
		}

		// CV_NearestScaled
		{
			float zn = camera.fNear;
			float zf = camera.fFar;
			float nearZRange = pRenderer->CV_r_DrawNearZRange;
			float camScale = pRenderer->CV_r_DrawNearFarPlane / gEnv->p3DEngine->GetMaxViewDistance();
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
			cb.CV_TessInfo.x = sqrtf(float(viewInfo.viewport.nWidth * viewInfo.viewport.nHeight)) / (hfov * pRenderer->CV_r_tessellationtrianglesize);
			cb.CV_TessInfo.y = pRenderer->CV_r_displacementfactor;
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
	CD3D9Renderer* pRenderer = gcpRendD3D;

	CShader* pShader = static_cast<CShader*>(inputDesc.shaderItem.m_pShader);
	SShaderTechnique* pTechnique = pShader->GetTechnique(inputDesc.shaderItem.m_nTechnique, inputDesc.technique, true);
	if (!pTechnique)
		return false;

	CShaderResources* pRes = static_cast<CShaderResources*>(inputDesc.shaderItem.m_pShaderResources);
	const uint64 objectFlags = inputDesc.objectFlags;
	const uint8 renderState = inputDesc.renderState;
	SShaderPass* pShaderPass = &pTechnique->m_Passes[0];

	// Handle quality flags
	CStandardGraphicsPipeline::ApplyShaderQuality(psoDesc, pRenderer->GetShaderProfile(pShader->m_eShaderType));

	SThreadInfo* const pShaderThreadInfo = &(pRenderer->m_RP.m_TI[pRenderer->m_RP.m_nProcessThreadID]);
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

	if (psoDesc.m_RenderState & GS_ALPHATEST)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ALPHATEST];

	if (renderState & OS_ENVIRONMENT_CUBEMAP)
		psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];

#ifdef TESSELLATION_RENDERER
	const bool bHasTesselationShaders = pShaderPass && pShaderPass->m_HShader && pShaderPass->m_DShader;
	if (bHasTesselationShaders && (!(objectFlags & FOB_NEAREST) && (objectFlags & FOB_ALLOW_TESSELLATION)))
	{
		psoDesc.m_ShaderFlags_RT &= ~g_HWSR_MaskBit[HWSR_NO_TESSELLATION];
		psoDesc.m_bAllowTesselation = true;
	}
#endif

	psoDesc.m_ShaderFlags_RT |= g_HWSR_MaskBit[HWSR_PER_INSTANCE_CB_TEMP];

	psoDesc.m_CullMode = bTwoSided ? eCULL_None : (pShaderPass->m_eCull != -1 ? (ECull)pShaderPass->m_eCull : eCULL_Back);
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
	layoutDesc.SetConstantBuffer(EResourceLayoutSlot_PerInstanceCB, eConstantBufferShaderSlot_PerInstance, EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Domain);
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerMaterialRS, GetDefaultMaterialBindPoints());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerInstanceExtraRS, GetDefaultInstanceExtraResources());
	layoutDesc.SetResourceSet(EResourceLayoutSlot_PerPassRS, perPassResources);

	CDeviceResourceLayoutPtr pResourceLayout = GetDeviceObjectFactory().CreateResourceLayout(layoutDesc);
	assert(pResourceLayout != nullptr);
	return pResourceLayout;
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
	CHWShader_D3D::s_nActivationFailMask = 0;

	GetDeviceObjectFactory().GetCoreCommandList().Reset();

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
	CHWShader_D3D::s_nActivationFailMask = 0;

	GetDeviceObjectFactory().GetCoreCommandList().Reset();
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

void CStandardGraphicsPipeline::RenderTiledShading()
{
	SwitchFromLegacyPipeline();
	m_pTiledShadingStage->Execute();
	SwitchToLegacyPipeline();
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

void CStandardGraphicsPipeline::ExecuteAnisotropicVerticalBlur(CTexture* pTex, int nAmount, float fScale, float fDistribution, bool bAlphaOnly)
{
	auto* pPassVerticalBlur = GetOrCreateUtilityPass<CAnisotropicVerticalBlurPass>();
	pPassVerticalBlur->Execute(pTex, nAmount, fScale, fDistribution, bAlphaOnly);
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
	if ((nAAMode & (eAT_SMAA_MASK | eAT_TSAA_MASK)) && gRenDev->CV_r_PostProcess && pDstRT)
	{
		assert(pDstRT);
		// Render to intermediate target, to avoid redundant imagecopy/stretchrect for PostAA
		pRenderer->FX_PushRenderTarget(0, pDstRT, &pRenderer->m_DepthBufferOrig);
		pRenderer->RT_SetViewport(0, 0, pDstRT->GetWidth(), pDstRT->GetHeight());
	}

	m_pRainStage->Execute();

	// Note: MB uses s_ptexHDRTargetPrev to avoid doing another copy, so this should be right before the MB pass
	{
		GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetPrev);
	}

	m_pDepthOfFieldStage->Execute();

	m_pMotionBlurStage->Execute();

	m_pSnowStage->Execute();

	// Half resolution downsampling
	{
		PROFILE_LABEL_SCOPE("HALFRES_DOWNSAMPLE_HDRTARGET");

		if (CRenderer::CV_r_HDRBloomQuality > 1)
			GetOrCreateUtilityPass<CStableDownsamplePass>()->Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetScaled[0], true);
		else
			GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetScaled[0]);
	}

	// Quarter resolution downsampling
	{
		PROFILE_LABEL_SCOPE("QUARTER_RES_DOWNSAMPLE_HDRTARGET");

		if (CRenderer::CV_r_HDRBloomQuality > 0)
			GetOrCreateUtilityPass<CStableDownsamplePass>()->Execute(CTexture::s_ptexHDRTargetScaled[0], CTexture::s_ptexHDRTargetScaled[1], CRenderer::CV_r_HDRBloomQuality >= 1);
		else
			GetOrCreateUtilityPass<CStretchRectPass>()->Execute(CTexture::s_ptexHDRTargetScaled[0], CTexture::s_ptexHDRTargetScaled[1]);
	}

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pAutoExposureStage->Execute();
	}

	m_pBloomStage->Execute();

	// Lens optics
	if (CRenderer::CV_r_flares && !CRenderer::CV_r_durango_async_dips)
	{
		PROFILE_LABEL_SCOPE("LENS_OPTICS");
		m_pLensOpticsStage->Execute(m_pCurrentRenderView);
	}

	m_pSunShaftsStage->Execute();
	m_pColorGradingStage->Execute();
	m_pToneMappingStage->Execute();
}

void CStandardGraphicsPipeline::ExecuteBillboards()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	pRenderer->FX_ClearTarget(CTexture::s_ptexSceneNormalsMap, Clr_Transparent);
	pRenderer->FX_ClearTarget(CTexture::s_ptexSceneDiffuse, Clr_Transparent);

	GetGBufferStage()->Execute();

	m_pCurrentRenderView->SwitchUsageMode(CRenderView::eUsageModeReadingDone);
	m_pCurrentRenderView->Clear();
}

// TODO: This will be used only for recursive render pass after all render views get rendered with full graphics pipeline including tiled forward shading.
void CStandardGraphicsPipeline::ExecuteMinimumForwardShading()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	void (*pRenderFunc)() = &pRenderer->FX_FlushShader_General;
	pRenderer->m_RP.m_pRenderFunc = pRenderFunc;

	CRenderView* pRenderView = GetCurrentRenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

	const bool bRecursive = pRenderView->IsRecursive();
	const bool bSecondaryViewport = (pRenderer->m_RP.m_nRendFlags & SHDF_SECONDARY_VIEWPORT) != 0;

	m_renderPassScheduler.SetEnabled(true);

	pRenderer->RT_SetCameraInfo();

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pGpuParticlesStage->Execute(m_pCurrentRenderView);
		m_pGpuParticlesStage->PreDraw(m_pCurrentRenderView);
		m_pComputeSkinningStage->Execute(m_pCurrentRenderView);
	}

	UpdateMainViewConstantBuffer();

	{
		PROFILE_FRAME(WaitForParticleRendItems);
		pRenderer->SyncComputeVerticesJobs();
		pRenderer->UnLockParticleVideoMemory();
	}

	// recursive pass doesn't use deferred fog, instead uses forward shader fog.
	pRenderer->m_RP.m_PersFlags2 &= ~(RBPF2_NOSHADERFOG);

	if (!(pRenderer->m_RP.m_nRendFlags & SHDF_CUBEMAPGEN))
	{
		if (!bRecursive && pOutput && bSecondaryViewport)
		{
			gRenDev->GetIRenderAuxGeom(IRenderer::eViewportType_Secondary)->Flush();
		}
	}

	// forward opaque and transparent passes for recursive rendering
	m_pSceneForwardStage->Execute_Minimum();

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence();

	// unbind render target
	{
		pRenderer->FX_PushRenderTarget(0, (CTexture*)nullptr, nullptr);
		pRenderer->FX_SetActiveRenderTargets();
		pRenderer->RT_UnbindTMUs();
		pRenderer->FX_PopRenderTarget(0);
	}

	if (pRenderer->m_CurRenderEye == RIGHT_EYE || !pRenderer->GetS3DRend().IsStereoEnabled() || !pRenderer->GetS3DRend().RequiresSequentialSubmission())
	{
		// Recursive pass doesn't need calling PostDraw().
		// Because general and recursive passes are executed in the same frame between BeginFrame() and EndFrame().
		if (!(pRenderView->IsRecursive()))
		{
			m_pGpuParticlesStage->PostDraw(m_pCurrentRenderView);
		}
	}

	if (!(pRenderer->m_RP.m_nRendFlags & SHDF_CUBEMAPGEN))
	{
		// Resolve HDR render target to back buffer if needed.
		if (!bRecursive && pOutput && bSecondaryViewport)
		{
			CTexture* pHDRTargetTex = pOutput->GetHDRTargetTexture();
			CTexture* pBackBufferTex = gcpRendD3D->GetCurrentTargetOutput();
			CRY_ASSERT(pHDRTargetTex->GetWidth() == pBackBufferTex->GetWidth());
			CRY_ASSERT(pHDRTargetTex->GetHeight() == pBackBufferTex->GetHeight());

			m_pToneMappingStage->ExecuteFixedExposure();
		}

		m_pSceneCustomStage->ExecuteHelperPass();
	}

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE_MINIMUM_FORWARD_SHADING");

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute();

	ResetUtilityPassCache();
	m_pCurrentRenderView = nullptr;
}

void CStandardGraphicsPipeline::ExecuteMobilePipeline()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	
	pRenderer->RT_SetCameraInfo();
	UpdateMainViewConstantBuffer();

	m_pTiledShadingStage->PrepareResources();

	if (CRenderer::CV_r_GraphicsPipelineMobile == 2)
		m_pSceneGBufferStage->Execute();
	else
		m_pSceneGBufferStage->ExecuteMicroGBuffer();

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pShadowMapStage->Prepare(m_pCurrentRenderView);
	}

	GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();

	m_pMobileCompositionStage->Execute(m_pCurrentRenderView);

	pRenderer->m_pPostProcessMgr->End();

	m_pCurrentRenderView = nullptr;
}

void CStandardGraphicsPipeline::Execute()
{
	if (CRenderer::CV_r_GraphicsPipelineMobile)
	{
		ExecuteMobilePipeline();
		return;
	}
	
	CD3D9Renderer* pRenderer = gcpRendD3D;

	PROFILE_LABEL_PUSH("GRAPHICS_PIPELINE");
	
	void (* pRenderFunc)() = &pRenderer->FX_FlushShader_General;
	pRenderer->m_RP.m_pRenderFunc = pRenderFunc;

	m_renderPassScheduler.SetEnabled(true);
	
	pRenderer->RT_SetCameraInfo();

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pGpuParticlesStage->Execute(m_pCurrentRenderView);
		m_pGpuParticlesStage->PreDraw(m_pCurrentRenderView);
		m_pComputeSkinningStage->Execute(m_pCurrentRenderView);

		m_pRainStage->ExecuteRainPreprocess();
		m_pSnowStage->ExecuteSnowPreprocess();
	}

	UpdateMainViewConstantBuffer();

	gcpRendD3D->GetS3DRend().TryInjectHmdCameraAsync(m_pCurrentRenderView);

	// new graphics pipeline doesn't need clearing stereo render targets.
	if (pRenderer->m_nGraphicsPipeline > 0)
	{
		gcpRendD3D->GetS3DRend().SkipEyeTargetClears();
	}

	// Prepare tiled shading resources early to give DMA operations enough time to finish
	m_pTiledShadingStage->PrepareResources();

	if (!m_pCurrentRenderView->IsRecursive() && pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		// compile shadow renderitems. needs to happen before gbuffer pass accesses renderitems
		m_pCurrentRenderView->PrepareShadowViews();
	}

	// GBuffer
	m_pSceneGBufferStage->Execute();

	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pShadowMapStage->Prepare(m_pCurrentRenderView);
	}

	if (pRenderer->m_nGraphicsPipeline >= 2)
	{
		// Wait for GBuffer draw jobs to finish
		GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();
	}

	// Issue split barriers for GBuffer
	CTexture* pTextures[] = {
		CTexture::s_ptexSceneNormalsMap,
		CTexture::s_ptexSceneDiffuse,
		CTexture::s_ptexSceneSpecular,
		pRenderer->m_DepthBufferOrig.pTexture
	};
	CDeviceGraphicsCommandInterface* pCmdList = GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface();
	pCmdList->BeginResourceTransitions(CRY_ARRAY_COUNT(pTextures), pTextures, eResTransition_TextureRead);

	// Shadow maps
	if (pRenderer->m_CurRenderEye != RIGHT_EYE)
	{
		m_pShadowMapStage->Execute();
	}

	if (CVrProjectionManager::IsMultiResEnabledStatic())
		CVrProjectionManager::Instance()->ExecuteFlattenDepth(CTexture::s_ptexZTarget, CVrProjectionManager::Instance()->GetZTargetFlattened());

	// Depth downsampling
	{
		CTexture* pZTexture = gcpRendD3D->m_DepthBufferOrig.pTexture;

		CTexture* pSourceDepth = CTexture::s_ptexZTarget;
#if CRY_PLATFORM_DURANGO
		pSourceDepth = pZTexture;  // On Durango reading device depth is faster since it is in ESRAM
#endif

		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(pSourceDepth, CTexture::s_ptexZTargetScaled, (pSourceDepth == pZTexture), true);
		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(CTexture::s_ptexZTargetScaled, CTexture::s_ptexZTargetScaled2, false, false);
		GetOrCreateUtilityPass<CDepthDownsamplePass>()->Execute(CTexture::s_ptexZTargetScaled2, CTexture::s_ptexZTargetScaled3, false, false);
	}

	// Depth readback (for occlusion culling)
	m_pDepthReadbackStage->Execute();

#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	// Deferred decals
	if (CRenderer::CV_r_deferredDecals == 2)
	{
		SwitchToLegacyPipeline();

		pRenderer->FX_PushRenderTarget(0, CTexture::s_ptexSceneNormalsMap, &pRenderer->m_DepthBufferOrig);
		pRenderer->FX_PushRenderTarget(1, CTexture::s_ptexSceneDiffuse, NULL);
		pRenderer->FX_PushRenderTarget(2, CTexture::s_ptexSceneSpecular, NULL);

		pRenderer->FX_DeferredDecals();

		pRenderer->FX_PopRenderTarget(2);
		pRenderer->FX_PopRenderTarget(1);
		pRenderer->FX_PopRenderTarget(0);

		SwitchFromLegacyPipeline();
	}
	else
#endif
	{
			pRenderer->GetGraphicsPipeline().GetDeferredDecalsStage()->Execute();
	}

	// GBuffer modifiers
	{
		m_pRainStage->ExecuteDeferredRainGBuffer();
		m_pSnowStage->ExecuteDeferredSnowGBuffer();
	}

	// Generate cloud volume textures for shadow mapping.
	m_pVolumetricCloudsStage->ExecuteShadowGen();

	if (pRenderer->m_nGraphicsPipeline >= 2)
	{
		// Wait for Shadow Map draw jobs to finish (also required for HeightMap AO and SVOGI)
		GetCurrentRenderView()->GetDrawer().WaitForDrawSubmission();
	}

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

	// Height Map AO
	ShadowMapFrustum* pHeightMapFrustum = nullptr;
	CTexture* pHeightMapAOScreenDepthTex = nullptr;
	CTexture* pHeightMapAOTex = nullptr;
	m_pHeightMapAOStage->Execute(pHeightMapFrustum, pHeightMapAOScreenDepthTex, pHeightMapAOTex);

	// Screen Space Obscurance
	if (!CRenderer::CV_r_DeferredShadingDebugGBuffer)
	{
		m_pScreenSpaceObscuranceStage->Execute(pHeightMapFrustum, pHeightMapAOScreenDepthTex, pHeightMapAOTex);
	}

	// Water volume caustics
	m_pWaterStage->ExecuteWaterVolumeCaustics();

	pRenderer->m_RP.m_PersFlags2 |= RBPF2_NOSHADERFOG;

	// Deferred shading
	{
		PROFILE_LABEL_SCOPE("DEFERRED_LIGHTING");
		m_pClipVolumesStage->Prepare(m_pCurrentRenderView);
		m_pClipVolumesStage->Execute();

		if (CRenderer::CV_r_DeferredShadingTiled > 1)
		{
			// TODO: To be refactored later
			pRenderer->m_RP.m_pSunLight = NULL;
			for (uint32 i = 0; i < m_pCurrentRenderView->GetDynamicLightsCount(); i++)
			{
				SRenderLight* pLight = &m_pCurrentRenderView->GetDynamicLight(i);
				if (pLight->m_Flags & DLF_SUN)
				{
					pRenderer->m_RP.m_pSunLight = pLight;
					break;
				}
			}
			
			m_pShadowMaskStage->Prepare(m_pCurrentRenderView);
			m_pShadowMaskStage->Execute();
			
			uint32 numVolumes;
			const Vec4* pVolumeParams;
			pRenderer->GetGraphicsPipeline().GetClipVolumesStage()->GetClipVolumeShaderParams(pVolumeParams, numVolumes);
			pRenderer->GetTiledShading().Render(m_pCurrentRenderView, (Vec4*)pVolumeParams);

			if (CRenderer::CV_r_DeferredShadingSSS)
			{
				m_pScreenSpaceSSSStage->Execute(CTexture::s_ptexSceneTargetR11G11B10F[0]);
			}
		}
		else
		{
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
			SwitchToLegacyPipeline();
			pRenderer->FX_ProcessRenderList(EFSLIST_DEFERRED_PREPROCESS, pRenderFunc, false);
			SwitchFromLegacyPipeline();
#endif
		}
	}

	{
		PROFILE_FRAME(WaitForParticleRendItems);
		pRenderer->SyncComputeVerticesJobs();
		pRenderer->UnLockParticleVideoMemory();
	}

	// Opaque forward passes
	m_pSceneForwardStage->Execute_Opaque();

	// Deferred ocean caustics
	m_pWaterStage->ExecuteDeferredOceanCaustics();

	// Fog
	{
		m_pVolumetricFogStage->Execute();

		m_pFogStage->Execute();
	}

	pRenderer->m_RP.m_PersFlags2 &= ~(RBPF2_NOSHADERFOG);

	// Clouds
	{
		m_pVolumetricCloudsStage->Execute();
	}

	// Water fog volumes
	{
		m_pWaterStage->ExecuteWaterFogVolumeBeforeTransparent();
	}

	pRenderer->UpdatePrevMatrix(true);

	// Transparent (below water)
	m_pSceneForwardStage->Execute_TransparentBelowWater();

	// Ocean and water volumes
	{
		m_pWaterStage->Execute();
	}

	// Transparent (above water)
	m_pSceneForwardStage->Execute_TransparentAboveWater();

#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	if (CRenderer::CV_r_TranspDepthFixup)
	{
		SwitchToLegacyPipeline();
		pRenderer->FX_DepthFixupMerge();
		SwitchFromLegacyPipeline();
	}

	// Half-res particles
	{
		SwitchToLegacyPipeline();
		pRenderer->FX_ProcessHalfResParticlesRenderList(m_pCurrentRenderView, EFSLIST_HALFRES_PARTICLES, pRenderFunc, true);
		SwitchFromLegacyPipeline();
	}

	pRenderer->m_CameraProjMatrixPrev = pRenderer->m_CameraProjMatrix;
#endif

	// Insert fence which is used on consoles to prevent overwriting video memory
	pRenderer->InsertParticleVideoDataFence();

	m_pSnowStage->ExecuteDeferredSnowDisplacement();

#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
	// Pop HDR target from RT stack
	{
		assert(pRenderer->m_RTStack[0][pRenderer->m_nRTStackLevel[0]].m_pTex == CTexture::s_ptexHDRTarget);
		pRenderer->FX_SetActiveRenderTargets();
		pRenderer->RT_UnbindTMUs();
		pRenderer->FX_PopRenderTarget(0);
		//pRenderer->EF_ClearTargetsLater(0);
	}
#endif

	if (pRenderer->m_CurRenderEye == RIGHT_EYE || !pRenderer->GetS3DRend().IsStereoEnabled() || !pRenderer->GetS3DRend().RequiresSequentialSubmission())
	{
		m_pGpuParticlesStage->PostDraw(m_pCurrentRenderView);
	}

	if (!(pRenderer->m_RP.m_nRendFlags & SHDF_CUBEMAPGEN))
	{
		ExecuteHDRPostProcessing();

		// HDR and LDR post-processing
		{
			pRenderer->m_RP.m_PersFlags1 &= ~RBPF1_SKIP_AFTER_POST_PROCESS;

#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
			if (pRenderer->m_nGraphicsPipeline < 3)
			{
				SwitchToLegacyPipeline();
				pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_HDRPOSTPROCESS, pRenderFunc, false);
				SwitchFromLegacyPipeline();
			}
#endif

			m_pPostEffectStage->Execute();

			pRenderer->RT_SetViewport(0, 0, pRenderer->GetWidth(), pRenderer->GetHeight());
		}

		// NOTE: this flag can be enabled in CThermalVision::Render().
		bool bDrawAfterPostProcess = !(pRenderer->m_RP.m_PersFlags1 & RBPF1_SKIP_AFTER_POST_PROCESS);
		if (bDrawAfterPostProcess)
		{
			if (pRenderer->m_nGraphicsPipeline >= 3)
			{
				m_pSceneForwardStage->Execute_AfterPostProcess();
			}
			else
			{
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
				SwitchToLegacyPipeline();
				pRenderer->FX_ProcessRenderList(EFSLIST_AFTER_POSTPROCESS, pRenderFunc, false);
				SwitchFromLegacyPipeline();
#endif
			}
		}

		m_pSceneCustomStage->Execute();

		// Display tone mapping debugging information on the screen
		if (CRenderer::CV_r_HDRDebug == 1 && (pRenderer->m_RP.m_nRendFlags & SHDF_ALLOWPOSTPROCESS))
		{
			m_pToneMappingStage->DisplayDebugInfo();
		}

		if (CRenderer::CV_r_DeferredShadingDebug)
		{
#if defined(RENDERER_ENABLE_LEGACY_PIPELINE)
			SwitchToLegacyPipeline();
			pRenderer->FX_DeferredRendering(m_pCurrentRenderView, true);
			SwitchFromLegacyPipeline();
#endif
		}
	}

	PROFILE_LABEL_POP("GRAPHICS_PIPELINE");

	m_pVolumetricFogStage->ResetFrame();

	m_renderPassScheduler.SetEnabled(false);
	m_renderPassScheduler.Execute();

	ResetUtilityPassCache();
	m_pCurrentRenderView = nullptr;
}
