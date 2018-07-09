// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Bloom.h"
#include "DriverD3D.h"

void CBloomStage::Execute()
{
	if (!CRenderer::CV_r_HDRBloom || !CRenderer::CV_r_PostProcess)
		return;

	// Approximate function (1 - r)^4 by a sum of Gaussians: 0.0174*G(0.008,r) + 0.192*G(0.0576,r)
	const float sigma1 = sqrtf(0.008f);
	const float sigma2 = sqrtf(0.0576f - 0.008f);

	PROFILE_LABEL_SCOPE("BLOOM_GEN");

	static CCryNameTSCRC techBloomGaussian("HDRBloomGaussian");
	static CCryNameR hdrParams0Name("HDRParams0");

	int width = CRendererResources::s_ptexHDRFinalBloom->GetWidth();
	int height = CRendererResources::s_ptexHDRFinalBloom->GetHeight();

	// Note: Just scaling the sampling offsets depending on the resolution is not very accurate but works acceptably
	int widthHalfRes = (CRendererResources::s_ptexHDRTarget->GetWidth() + 1) / 2;
	int widthQuarterRes = (widthHalfRes + 1) / 2;
	assert(CRendererResources::s_ptexHDRFinalBloom->GetWidth() == widthQuarterRes);
	float scaleW = ((float)width / 400.0f) / (float)width;
	float scaleH = ((float)height / 225.0f) / (float)height;

	SamplerStateHandle samplerBloom = (CRendererResources::s_ptexHDRFinalBloom->GetWidth() == 400 && CRendererResources::s_ptexHDRFinalBloom->GetHeight() == 225) ? EDefaultSamplerStates::PointClamp : EDefaultSamplerStates::LinearClamp;

	// Pass 1 Horizontal
	if (m_pass1H.IsDirty())
	{
		m_pass1H.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
		m_pass1H.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_pass1H.SetTechnique(CShaderMan::s_shHDRPostProcess, techBloomGaussian, 0);
		m_pass1H.SetRenderTarget(0, CRendererResources::s_ptexHDRTempBloom[1]);
		m_pass1H.SetState(GS_NODEPTHTEST);
		m_pass1H.SetTexture(0, CRendererResources::s_ptexHDRTargetScaled[1]);
		m_pass1H.SetTexture(2, CRendererResources::s_ptexHDRToneMaps[0]);
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
		m_pass1V.SetRenderTarget(0, CRendererResources::s_ptexHDRTempBloom[0]);
		m_pass1V.SetState(GS_NODEPTHTEST);
		m_pass1V.SetTexture(0, CRendererResources::s_ptexHDRTempBloom[1]);
		m_pass1V.SetTexture(2, CRendererResources::s_ptexHDRToneMaps[0]);
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
		m_pass2H.SetRenderTarget(0, CRendererResources::s_ptexHDRTempBloom[1]);
		m_pass2H.SetState(GS_NODEPTHTEST);
		m_pass2H.SetTexture(0, CRendererResources::s_ptexHDRTempBloom[0]);
		m_pass2H.SetTexture(2, CRendererResources::s_ptexHDRToneMaps[0]);
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
		m_pass2V.SetRenderTarget(0, CRendererResources::s_ptexHDRFinalBloom);
		m_pass2V.SetState(GS_NODEPTHTEST);
		m_pass2V.SetTexture(0, CRendererResources::s_ptexHDRTempBloom[1]);
		m_pass2V.SetTexture(1, CRendererResources::s_ptexHDRTempBloom[0]);
		m_pass2V.SetTexture(2, CRendererResources::s_ptexHDRToneMaps[0]);
		m_pass2V.SetSampler(0, samplerBloom);
	}

	m_pass2V.BeginConstantUpdate();
	m_pass2V.SetConstant(hdrParams0Name, Vec4(0, (sigma2 / sigma1) * scaleH, 0, 0), eHWSC_Pixel);
	m_pass2V.Execute();
}
