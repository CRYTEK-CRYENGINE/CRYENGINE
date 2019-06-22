// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bloom.h"

void CBloomStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	// Approximate function (1 - r)^4 by a sum of Gaussians: 0.0174*G(0.008,r) + 0.192*G(0.0576,r)
	const float sigma1 = sqrtf(0.008f);
	const float sigma2 = sqrtf(0.0576f - 0.008f);

	PROFILE_LABEL_SCOPE("BLOOM_GEN");

	static CCryNameTSCRC techBloomGaussian("HDRBloomGaussian");
	static CCryNameR hdrParams0Name("HDRParams0");

	int width = m_graphicsPipelineResources.m_pTexHDRFinalBloom->GetWidth();
	int height = m_graphicsPipelineResources.m_pTexHDRFinalBloom->GetHeight();

	// Note: Just scaling the sampling offsets depending on the resolution is not very accurate but works acceptably
#if defined(USE_CRY_ASSERT)
	int widthHalfRes = (m_graphicsPipelineResources.m_pTexHDRTarget->GetWidth() + 1) / 2;
	int widthQuarterRes = (widthHalfRes + 1) / 2;
	CRY_ASSERT(m_graphicsPipelineResources.m_pTexHDRFinalBloom->GetWidth() == widthQuarterRes);
#endif
	float scaleW = ((float)width / 400.0f) / (float)width;
	float scaleH = ((float)height / 225.0f) / (float)height;

	// TODO: Compute shader! because the targets are not compressed / able anyway, and we can half resource allocation)
	SamplerStateHandle samplerBloom = (m_graphicsPipelineResources.m_pTexHDRFinalBloom->GetWidth() == 400 && m_graphicsPipelineResources.m_pTexHDRFinalBloom->GetHeight() == 225) ? EDefaultSamplerStates::PointClamp : EDefaultSamplerStates::LinearClamp;

	// Pass 1 Horizontal
	if (m_pass1H.IsDirty())
	{
		m_pass1H.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_pass1H.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_pass1H.SetTechnique(CShaderMan::s_shHDRPostProcess, techBloomGaussian, 0);
		m_pass1H.SetRenderTarget(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][1]);
		m_pass1H.SetState(GS_NODEPTHTEST | GS_NOCOLMASK_A);
		m_pass1H.SetTexture(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][0]);
		m_pass1H.SetSampler(0, samplerBloom);
	}

	m_pass1H.BeginConstantUpdate();
	m_pass1H.SetConstant(hdrParams0Name, Vec4(scaleW, 0, 0, 0), eHWSC_Pixel);
	m_pass1H.Execute();

	// Pass 1 Vertical
	if (m_pass1V.IsDirty())
	{
		m_pass1V.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_pass1V.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_pass1V.SetTechnique(CShaderMan::s_shHDRPostProcess, techBloomGaussian, 0);
		m_pass1V.SetRenderTarget(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][0]);
		m_pass1V.SetState(GS_NODEPTHTEST | GS_NOCOLMASK_A);
		m_pass1V.SetTexture(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][1]);
		m_pass1V.SetSampler(0, samplerBloom);
	}

	m_pass1V.BeginConstantUpdate();
	m_pass1V.SetConstant(hdrParams0Name, Vec4(0, scaleH, 0, 0), eHWSC_Pixel);
	m_pass1V.Execute();

	// Pass 2 Horizontal
	if (m_pass2H.IsDirty())
	{
		m_pass2H.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_pass2H.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_pass2H.SetTechnique(CShaderMan::s_shHDRPostProcess, techBloomGaussian, 0);
		m_pass2H.SetRenderTarget(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][1]);
		m_pass2H.SetState(GS_NODEPTHTEST | GS_NOCOLMASK_A);
		m_pass2H.SetTexture(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][0]);
		m_pass2H.SetSampler(0, samplerBloom);
	}

	m_pass2H.BeginConstantUpdate();
	m_pass2H.SetConstant(hdrParams0Name, Vec4((sigma2 / sigma1) * scaleW, 0, 0, 0), eHWSC_Pixel);
	m_pass2H.Execute();

	// Pass 2 Vertical
	if (m_pass2V.IsDirty())
	{
		m_pass2V.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_pass2V.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_pass2V.SetTechnique(CShaderMan::s_shHDRPostProcess, techBloomGaussian, g_HWSR_MaskBit[HWSR_SAMPLE0]);
		m_pass2V.SetRenderTarget(0, m_graphicsPipelineResources.m_pTexHDRFinalBloom);
		m_pass2V.SetState(GS_NODEPTHTEST | GS_NOCOLMASK_A);
		m_pass2V.SetTexture(0, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][1]);
		m_pass2V.SetTexture(1, m_graphicsPipelineResources.m_pTexHDRTargetScaled[1][0]);
		m_pass2V.SetSampler(0, samplerBloom);
	}

	m_pass2V.BeginConstantUpdate();
	m_pass2V.SetConstant(hdrParams0Name, Vec4(0, (sigma2 / sigma1) * scaleH, 0, 0), eHWSC_Pixel);
	m_pass2V.Execute();
}
