// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ScreenSpaceReflections.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "../../Common/ReverseDepth.h"

void CScreenSpaceReflectionsStage::Init()
{
	for (uint i = 0; i < MAX_GPU_NUM; i++)
		m_prevViewProj[i] = IDENTITY;
}

void CScreenSpaceReflectionsStage::Execute()
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	if (!CRenderer::CV_r_SSReflections)
		return;

	PROFILE_LABEL_SCOPE("SS_REFLECTIONS");

	// Store current state
	const uint32 prevPersFlags = rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags;

	Matrix44 mViewProj = rd->m_ViewMatrix * rd->m_ProjMatrix;

	const int frameID = SPostEffectsUtils::m_iFrameCounter;
	Matrix44 mViewport(0.5f, 0, 0, 0,
	                   0, -0.5f, 0, 0,
	                   0, 0, 1.0f, 0,
	                   0.5f, 0.5f, 0, 1.0f);
	uint32 numGPUs = rd->GetActiveGPUCount();
	Matrix44 mViewProjPrev = m_prevViewProj[max((frameID - (int)numGPUs) % MAX_GPU_NUM, 0)] * mViewport;

	CShader* pShader = CShaderMan::s_shDeferredShading;

	{
		PROFILE_LABEL_SCOPE("SSR_RAYTRACE");

		CTexture* destRT = CRenderer::CV_r_SSReflHalfRes ? CTexture::s_ptexHDRTargetScaled[0] : CTexture::s_ptexHDRTarget;

		if (m_passRaytracing.InputChanged(CRenderer::CV_r_SSReflHalfRes, rd->RT_GetCurrGpuID()))
		{
			static CCryNameTSCRC techRaytrace("SSR_Raytrace");
			m_passRaytracing.SetTechnique(pShader, techRaytrace, 0);
			m_passRaytracing.SetRenderTarget(0, destRT);
			m_passRaytracing.SetState(GS_NODEPTHTEST);
			m_passRaytracing.SetTextureSamplerPair(0, CTexture::s_ptexZTarget, EDefaultSamplerStates::PointClamp);
			m_passRaytracing.SetTextureSamplerPair(1, CTexture::s_ptexSceneNormalsMap, EDefaultSamplerStates::LinearClamp);
			m_passRaytracing.SetTextureSamplerPair(2, CTexture::s_ptexSceneSpecular, EDefaultSamplerStates::LinearClamp);
			m_passRaytracing.SetTextureSamplerPair(3, CTexture::s_ptexZTargetScaled, EDefaultSamplerStates::PointClamp);
			m_passRaytracing.SetTextureSamplerPair(4, CTexture::s_ptexHDRTargetPrev, EDefaultSamplerStates::LinearBorder_Black);
			m_passRaytracing.SetTextureSamplerPair(5, CTexture::s_ptexHDRMeasuredLuminance[rd->RT_GetCurrGpuID()], EDefaultSamplerStates::PointClamp);
			m_passRaytracing.SetRequireWorldPos(true);
			m_passRaytracing.SetRequirePerViewConstantBuffer(true);
			m_passRaytracing.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		static CCryNameR viewProjprevName("g_mViewProjPrev");

		m_passRaytracing.BeginConstantUpdate();
		m_passRaytracing.SetConstantArray(viewProjprevName, (Vec4*)mViewProjPrev.GetData(), 4, eHWSC_Pixel);
		m_passRaytracing.Execute();
	}

	if (!CRenderer::CV_r_SSReflHalfRes)
	{
		m_passCopy.Execute(CTexture::s_ptexHDRTarget, CTexture::s_ptexHDRTargetScaled[0]);
	}

	// Convolve sharp reflections
	m_passDownsample0.Execute(CTexture::s_ptexHDRTargetScaled[0], CTexture::s_ptexHDRTargetScaled[1]);
	m_passBlur0.Execute(CTexture::s_ptexHDRTargetScaled[1], CTexture::s_ptexHDRTargetScaledTempRT[1], 1.0f, 3.0f);

	m_passDownsample1.Execute(CTexture::s_ptexHDRTargetScaled[1], CTexture::s_ptexHDRTargetScaled[2]);
	m_passBlur1.Execute(CTexture::s_ptexHDRTargetScaled[2], CTexture::s_ptexHDRTargetScaledTempRT[2], 1.0f, 3.0f);

	m_passDownsample2.Execute(CTexture::s_ptexHDRTargetScaled[2], CTexture::s_ptexHDRTargetScaled[3]);
	m_passBlur2.Execute(CTexture::s_ptexHDRTargetScaled[3], CTexture::s_ptexHDRTargetScaledTempRT[3], 1.0f, 3.0f);

	{
		PROFILE_LABEL_SCOPE("SSR_COMPOSE");

		CTexture* destTex = CTexture::s_ptexHDRTargetScaledTmp[0];
		destTex->Unbind();

		if (m_passComposition.InputChanged())
		{
			static CCryNameTSCRC techComposition("SSReflection_Comp");
			m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
			m_passComposition.SetTechnique(pShader, techComposition, 0);
			m_passComposition.SetRenderTarget(0, destTex);
			m_passComposition.SetState(GS_NODEPTHTEST);
			m_passComposition.SetTextureSamplerPair(0, CTexture::s_ptexSceneSpecular, EDefaultSamplerStates::LinearClamp);
			m_passComposition.SetTextureSamplerPair(1, CTexture::s_ptexHDRTargetScaled[0], EDefaultSamplerStates::LinearClamp);
			m_passComposition.SetTextureSamplerPair(2, CTexture::s_ptexHDRTargetScaled[1], EDefaultSamplerStates::LinearClamp);
			m_passComposition.SetTextureSamplerPair(3, CTexture::s_ptexHDRTargetScaled[2], EDefaultSamplerStates::LinearClamp);
			m_passComposition.SetTextureSamplerPair(4, CTexture::s_ptexHDRTargetScaled[3], EDefaultSamplerStates::LinearClamp);

			m_passComposition.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		m_passComposition.BeginConstantUpdate();
		m_passComposition.Execute();
	}

	// Update array used for MGPU support
	m_prevViewProj[frameID % MAX_GPU_NUM] = mViewProj;

	// Restore original state
	rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags = prevPersFlags;
	if (rd->m_RP.m_TI[rd->m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
	{
		uint32 depthState = ReverseDepthHelper::ConvertDepthFunc(rd->m_RP.m_CurState);
		rd->FX_SetState(rd->m_RP.m_CurState, rd->m_RP.m_CurAlphaRef, depthState);
	}
}
