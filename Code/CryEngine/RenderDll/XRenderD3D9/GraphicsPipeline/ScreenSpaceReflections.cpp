// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScreenSpaceReflections.h"
#include "D3DPostProcess.h"
#include "../../Common/ReverseDepth.h"

void CScreenSpaceReflectionsStage::Init()
{
	for (uint i = 0; i < MAX_GPU_NUM; i++)
		m_prevViewProj[i] = IDENTITY;
}

void CScreenSpaceReflectionsStage::Update()
{
	CTexture* targetRT = CRenderer::CV_r_SSReflHalfRes
		? m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1]
		: m_graphicsPipelineResources.m_pTexHDRTargetMasked;

	CClearSurfacePass::Execute(targetRT, Clr_Transparent);
}

void CScreenSpaceReflectionsStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("SS_REFLECTIONS");

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	Matrix44 mViewProj = GetCurrentViewInfo().cameraProjMatrix;
	Matrix44 mReprojToPrev = GetCurrentViewInfo().GetReprojection();

	const int frameID = SPostEffectsUtils::m_iFrameCounter;
	Matrix44 mViewport(0.5f, 0, 0, 0,
	                   0, -0.5f, 0, 0,
	                   0, 0, 1.0f, 0,
	                   0.5f, 0.5f, 0, 1.0f);
	uint32 numGPUs = rd->GetActiveGPUCount();
	Matrix44 mViewProjPrev = m_prevViewProj[max((frameID - (int)numGPUs) % MAX_GPU_NUM, 0)] * mViewport;

	CShader* pShader = CShaderMan::s_shDeferredShading;

	CTexture* targetRT = CRenderer::CV_r_SSReflHalfRes
		? m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1]
		: m_graphicsPipelineResources.m_pTexHDRTargetMasked;

	bool isSecondaryViewport = (m_graphicsPipeline.GetPipelineDescription().shaderFlags & SHDF_SECONDARY_VIEWPORT) != 0;
	uint64 rtMask = isSecondaryViewport ? g_HWSR_MaskBit[HWSR_SECONDARY_VIEW] : 0;
	{
		PROFILE_LABEL_SCOPE("SSR_RAYTRACE");

		if (m_passRaytracing.IsDirty(CRenderer::CV_r_SSReflHalfRes, rd->RT_GetCurrGpuID()))
		{
			static CCryNameTSCRC techRaytrace("SSR_Raytrace");
			m_passRaytracing.SetTechnique(pShader, techRaytrace, rtMask);
			m_passRaytracing.SetRenderTarget(0, targetRT);
			m_passRaytracing.SetState(GS_NODEPTHTEST);
			m_passRaytracing.SetTexture(0, m_graphicsPipelineResources.m_pTexLinearDepth);
			m_passRaytracing.SetTexture(1, m_graphicsPipelineResources.m_pTexSceneNormalsMap);
			m_passRaytracing.SetTexture(2, m_graphicsPipelineResources.m_pTexSceneSpecular);
			m_passRaytracing.SetTexture(3, m_graphicsPipelineResources.m_pTexLinearDepthScaled[0]);
			m_passRaytracing.SetTexture(4, m_graphicsPipelineResources.m_pTexHDRTargetPrev);
			m_passRaytracing.SetTexture(5, m_graphicsPipelineResources.m_pTexHDRMeasuredLuminance[rd->RT_GetCurrGpuID()]);

			m_passRaytracing.SetSampler(0, EDefaultSamplerStates::PointClamp);
			m_passRaytracing.SetSampler(1, EDefaultSamplerStates::LinearClamp);
			m_passRaytracing.SetSampler(2, EDefaultSamplerStates::LinearBorder_Black);

			m_passRaytracing.SetRequireWorldPos(true);
			m_passRaytracing.SetRequirePerViewConstantBuffer(true);
			m_passRaytracing.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		static CCryNameR viewProjPrevName("g_mViewProjPrev");
		static CCryNameR reprojToPrevName("g_mReprojectToPrev");
		static CCryNameR ssrParamsName("g_mSSRParams"); // we need to tell the shader to read from a depth buffer with twice the size of the output in halfres mode
		Vec4 ssrParams(
			CRenderer::CV_r_SSReflHalfRes ? 2.0f : 1.0f,
			CRenderer::CV_r_SSReflHalfRes ? 2.0f : 1.0f,
			CRenderer::CV_r_SSReflDistance,
			CRenderer::CV_r_SSReflSamples * 1.0f);

		m_passRaytracing.BeginConstantUpdate();
		m_passRaytracing.SetConstantArray(viewProjPrevName, (Vec4*)mViewProjPrev.GetData(), 4, eHWSC_Pixel);
		m_passRaytracing.SetConstantArray(reprojToPrevName, (Vec4*)mReprojToPrev.GetData(), 4, eHWSC_Pixel);
		m_passRaytracing.SetConstantArray(ssrParamsName, (Vec4*)&ssrParams, 1, eHWSC_Pixel);
		m_passRaytracing.Execute();
	}

	// TODO: Compute shader! because the targets are not compressed / able anyway, and we can half resource allocation)
	if (targetRT != m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1])
	{
		m_passCopy.Execute(targetRT, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1]);
	}

	// Convolve sharp reflections
	m_passDownsample0.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[1][0]);
	m_passBlur0.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[1][0], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[1][1], 1.0f, 3.0f);

	m_passDownsample1.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[1][1], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[2][0]);
	m_passBlur1.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[2][0], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[2][1], 1.0f, 3.0f);

	m_passDownsample2.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[2][1], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[3][0]);
	m_passBlur2.Execute(m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[3][0], m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[3][1], 1.0f, 3.0f);

	{
		PROFILE_LABEL_SCOPE("SSR_COMPOSE");

		CTexture* destTex = m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][0];

		if (m_passComposition.IsDirty())
		{
			static CCryNameTSCRC techComposition("SSReflection_Comp");
			m_passComposition.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
			m_passComposition.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passComposition.SetTechnique(pShader, techComposition, 0);
			m_passComposition.SetRenderTarget(0, destTex);
			m_passComposition.SetState(GS_NODEPTHTEST);
			m_passComposition.SetTexture(0, m_graphicsPipelineResources.m_pTexSceneSpecular);
			m_passComposition.SetTexture(1, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][1]);
			m_passComposition.SetTexture(2, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[1][1]);
			m_passComposition.SetTexture(3, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[2][1]);
			m_passComposition.SetTexture(4, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[3][1]);

			m_passComposition.SetSampler(0, EDefaultSamplerStates::LinearClamp);

			m_passComposition.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);
		}

		m_passComposition.BeginConstantUpdate();
		m_passComposition.Execute();
	}

	// Update array used for MGPU support
	m_prevViewProj[frameID % MAX_GPU_NUM] = mViewProj;
}
