// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

	Matrix44 mViewProj = GetCurrentViewInfo().cameraProjMatrix;

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

		CTexture* destRT = CRenderer::CV_r_SSReflHalfRes ? CRendererResources::s_ptexHDRTargetScaled[0] : CRendererResources::s_ptexHDRTarget;

		if (m_passRaytracing.IsDirty(CRenderer::CV_r_SSReflHalfRes, rd->RT_GetCurrGpuID()))
		{
			static CCryNameTSCRC techRaytrace("SSR_Raytrace");
			m_passRaytracing.SetTechnique(pShader, techRaytrace, 0);
			m_passRaytracing.SetRenderTarget(0, destRT);
			m_passRaytracing.SetState(GS_NODEPTHTEST);
			m_passRaytracing.SetTexture(0, CRendererResources::s_ptexLinearDepth);
			m_passRaytracing.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
			m_passRaytracing.SetTexture(2, CRendererResources::s_ptexSceneSpecular);
			m_passRaytracing.SetTexture(3, CRendererResources::s_ptexLinearDepthScaled[0]);
			m_passRaytracing.SetTexture(4, CRendererResources::s_ptexHDRTargetPrev);
			m_passRaytracing.SetTexture(5, CRendererResources::s_ptexHDRMeasuredLuminance[rd->RT_GetCurrGpuID()]);

			m_passRaytracing.SetSampler(0, EDefaultSamplerStates::PointClamp);
			m_passRaytracing.SetSampler(1, EDefaultSamplerStates::LinearClamp);
			m_passRaytracing.SetSampler(2, EDefaultSamplerStates::LinearBorder_Black);

			m_passRaytracing.SetRequireWorldPos(true);
			m_passRaytracing.SetRequirePerViewConstantBuffer(true);
			m_passRaytracing.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		static CCryNameR viewProjprevName("g_mViewProjPrev");
		static CCryNameR ssrParamsName("g_mSSRParams"); // we need to tell the shader to read from a depth buffer with twice the size of the output in halfres mode
		Vec4 ssrParams(
			CRenderer::CV_r_SSReflHalfRes ? 2.0f : 1.0f,
			CRenderer::CV_r_SSReflHalfRes ? 2.0f : 1.0f,
			CRenderer::CV_r_SSReflDistance,
			CRenderer::CV_r_SSReflSamples * 1.0f);

		m_passRaytracing.BeginConstantUpdate();
		m_passRaytracing.SetConstantArray(viewProjprevName, (Vec4*)mViewProjPrev.GetData(), 4, eHWSC_Pixel);
		m_passRaytracing.SetConstantArray(ssrParamsName, (Vec4*)&ssrParams, 1, eHWSC_Pixel);
		m_passRaytracing.Execute();
	}

	if (!CRenderer::CV_r_SSReflHalfRes)
	{
		m_passCopy.Execute(CRendererResources::s_ptexHDRTarget, CRendererResources::s_ptexHDRTargetScaled[0]);
	}

	// Convolve sharp reflections
	m_passDownsample0.Execute(CRendererResources::s_ptexHDRTargetScaled[0], CRendererResources::s_ptexHDRTargetScaled[1]);
	m_passBlur0.Execute(CRendererResources::s_ptexHDRTargetScaled[1], CRendererResources::s_ptexHDRTargetScaledTempRT[1], 1.0f, 3.0f);

	m_passDownsample1.Execute(CRendererResources::s_ptexHDRTargetScaled[1], CRendererResources::s_ptexHDRTargetScaled[2]);
	m_passBlur1.Execute(CRendererResources::s_ptexHDRTargetScaled[2], CRendererResources::s_ptexHDRTargetScaledTempRT[2], 1.0f, 3.0f);

	m_passDownsample2.Execute(CRendererResources::s_ptexHDRTargetScaled[2], CRendererResources::s_ptexHDRTargetScaled[3]);
	m_passBlur2.Execute(CRendererResources::s_ptexHDRTargetScaled[3], CRendererResources::s_ptexHDRTargetScaledTempRT[3], 1.0f, 3.0f);

	{
		PROFILE_LABEL_SCOPE("SSR_COMPOSE");

		CTexture* destTex = CRendererResources::s_ptexHDRTargetScaledTmp[0];

		if (m_passComposition.IsDirty())
		{
			static CCryNameTSCRC techComposition("SSReflection_Comp");
			m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
			m_passComposition.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passComposition.SetTechnique(pShader, techComposition, 0);
			m_passComposition.SetRenderTarget(0, destTex);
			m_passComposition.SetState(GS_NODEPTHTEST);
			m_passComposition.SetTexture(0, CRendererResources::s_ptexSceneSpecular);
			m_passComposition.SetTexture(1, CRendererResources::s_ptexHDRTargetScaled[0]);
			m_passComposition.SetTexture(2, CRendererResources::s_ptexHDRTargetScaled[1]);
			m_passComposition.SetTexture(3, CRendererResources::s_ptexHDRTargetScaled[2]);
			m_passComposition.SetTexture(4, CRendererResources::s_ptexHDRTargetScaled[3]);

			m_passComposition.SetSampler(0, EDefaultSamplerStates::LinearClamp);

			m_passComposition.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		m_passComposition.BeginConstantUpdate();
		m_passComposition.Execute();
	}

	// Update array used for MGPU support
	m_prevViewProj[frameID % MAX_GPU_NUM] = mViewProj;
}
