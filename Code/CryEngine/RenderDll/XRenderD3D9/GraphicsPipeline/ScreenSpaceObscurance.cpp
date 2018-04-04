// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ScreenSpaceObscurance.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"


struct ObscuranceConstants
{
	Vec4 screenSize;
	Vec4 nearFarClipDist;
	Vec4 viewSpaceParams;
	Vec4 ssdoParams;
	Vec4 hmaoParams;
};

void CScreenSpaceObscuranceStage::Init()
{
	m_passObscurance.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
	m_passObscurance.AllocateTypedConstantBuffer<ObscuranceConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);
}

void CScreenSpaceObscuranceStage::Execute(ShadowMapFrustum* pHeightMapFrustum, CTexture* pHeightMapAOScreenDepthTex, CTexture* pHeightMapAOTex)
{
	CD3D9Renderer* const __restrict pRenderer = gcpRendD3D;

	if (!CRenderer::CV_r_ssdo)
	{
		pRenderer->FX_ClearTarget(CTexture::s_ptexSceneNormalsBent, Clr_Median);
		return;
	}

	PROFILE_LABEL_SCOPE("DIRECTIONAL_OCC");

#if defined(DURANGO_USE_ESRAM)
	m_passCopyFromESRAM.Execute(CTexture::s_ptexSceneSpecularESRAM, CTexture::s_ptexSceneSpecular);
#endif

	CTexture* pDestRT = CTexture::s_ptexStereoR;
#if defined(DURANGO_USE_ESRAM)
	pDestRT = CTexture::s_ptexSceneSpecularESRAM;
#endif

	const bool bLowResOutput = (CRenderer::CV_r_ssdoHalfRes == 3);
	if (bLowResOutput)
		pDestRT = CTexture::s_ptexBackBufferScaled[0];

	CShader* pShader = CShaderMan::s_shDeferredShading;

	// Obscurance generation
	{
		uint64 rtMask = 0;
		rtMask |= CRenderer::CV_r_ssdoHalfRes ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
		rtMask |= pHeightMapFrustum ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
		rtMask |= CRenderer::CV_r_DeferredShadingTiled == 4 ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;

		// Extreme magnification as happening with small FOVs will cause banding issues with half-res depth
		if (CRenderer::CV_r_ssdoHalfRes == 2 && RAD2DEG(pRenderer->GetCamera().GetFov()) < 30)
			rtMask &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];

		static CCryNameTSCRC techSampling("SSDO_Sampling");

		m_passObscurance.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passObscurance.SetTechnique(pShader, techSampling, rtMask);
		m_passObscurance.SetRenderTarget(0, pDestRT);
		m_passObscurance.SetState(GS_NODEPTHTEST);
		m_passObscurance.SetRequirePerViewConstantBuffer(true);
		m_passObscurance.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

		m_passObscurance.SetTextureSamplerPair(0, CTexture::s_ptexSceneNormalsMap, EDefaultSamplerStates::PointClamp);
		m_passObscurance.SetTextureSamplerPair(1, CTexture::s_ptexZTarget, EDefaultSamplerStates::PointClamp);
		m_passObscurance.SetTextureSamplerPair(3, CTexture::s_ptexAOVOJitter, EDefaultSamplerStates::PointWrap);
		m_passObscurance.SetTextureSamplerPair(5, bLowResOutput ? CTexture::s_ptexZTargetScaled2 : CTexture::s_ptexZTargetScaled, EDefaultSamplerStates::PointClamp);
		m_passObscurance.SetTextureSamplerPair(11, pHeightMapAOScreenDepthTex ? pHeightMapAOScreenDepthTex : CTexture::s_ptexWhite, EDefaultSamplerStates::PointClamp);
		m_passObscurance.SetTexture(12, pHeightMapAOTex ? pHeightMapAOTex : CTexture::s_ptexWhite);

		{
			CStandardGraphicsPipeline::SViewInfo viewInfo[CCamera::eEye_eCount];
			int viewInfoCount = pRenderer->GetGraphicsPipeline().GetViewInfo(viewInfo);

			Vec4 viewSpaceParams[CCamera::eEye_eCount];
			for (uint32 i = 0; i < viewInfoCount; i++)
			{
				Matrix44 projMat = viewInfo[i].projMatrix;
				float stereoShift = projMat.m20 * 2.0f;
				viewSpaceParams[i] = Vec4(2.0f / projMat.m00, 2.0f / projMat.m11, (-1.0f + stereoShift) / projMat.m00, -1.0f / projMat.m11);
			}
			
			auto constants = m_passObscurance.BeginTypedConstantUpdate<ObscuranceConstants>(eConstantBufferShaderSlot_PerBatch, EShaderStage_Pixel);

			constants->screenSize = Vec4((float)pDestRT->GetWidth(), (float)pDestRT->GetHeight(), 1.0f / (float)pDestRT->GetWidth(), 1.0f / (float)pDestRT->GetHeight());
			constants->nearFarClipDist = Vec4(viewInfo[0].pRenderCamera->fNear, viewInfo[0].pRenderCamera->fFar, 0, 0);
		
			float radius = CRenderer::CV_r_ssdoRadius / viewInfo[0].pRenderCamera->fFar;
	#if defined(FEATURE_SVO_GI)
			if (CSvoRenderer::GetInstance()->IsActive())
				radius *= CSvoRenderer::GetInstance()->GetSsaoAmount();
	#endif
			constants->ssdoParams = Vec4(radius * 0.5f * viewInfo[0].projMatrix.m00, radius * 0.5f * viewInfo[0].projMatrix.m11,
																	 CRenderer::CV_r_ssdoRadiusMin, CRenderer::CV_r_ssdoRadiusMax);

			constants->viewSpaceParams = viewSpaceParams[0];

			if (pHeightMapFrustum)
			{
				assert(pHeightMapAOTex && pHeightMapAOScreenDepthTex);
				assert(pHeightMapAOTex->GetWidth() == pHeightMapAOScreenDepthTex->GetWidth() && pHeightMapAOTex->GetHeight() == pHeightMapAOScreenDepthTex->GetHeight());
				const float resolutionFactor = (float)pHeightMapAOTex->GetWidth() / (float)pDestRT->GetWidth();
				constants->hmaoParams = Vec4(1.0f / (float)pHeightMapAOTex->GetWidth(), 1.0f / (float)pHeightMapAOTex->GetHeight(), resolutionFactor, 0);
			}
			else
			{
				constants->hmaoParams = Vec4(0, 0, 0, 0);
			}

			if (viewInfoCount > 1)
			{
				constants.BeginStereoOverride(true);
				constants->viewSpaceParams = viewSpaceParams[1];
			}
		
			m_passObscurance.EndTypedConstantUpdate(constants);
		}

		m_passObscurance.Execute();
	}

	// Filtering pass
	if (CRenderer::CV_r_ssdo != 99)
	{
		const int32 sizeX = CTexture::s_ptexZTarget->GetWidth();
		const int32 sizeY = CTexture::s_ptexZTarget->GetHeight();
		const int32 srcSizeX = pDestRT->GetWidth();
		const int32 srcSizeY = pDestRT->GetHeight();

		if (m_passFilter.InputChanged((int)bLowResOutput))
		{
			static CCryNameTSCRC techFilter("SSDO_Filter");
			m_passFilter.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passFilter.SetTechnique(pShader, techFilter, 0);
			m_passFilter.SetRenderTarget(0, CTexture::s_ptexSceneNormalsBent);
			m_passFilter.SetState(GS_NODEPTHTEST);
			m_passFilter.SetTextureSamplerPair(0, pDestRT, EDefaultSamplerStates::LinearClamp);
			m_passFilter.SetTextureSamplerPair(1, CTexture::s_ptexZTarget, EDefaultSamplerStates::PointClamp);
		}

		static CCryNameR sourceTexSizeName("SSDO_SourceTexSize");
		static CCryNameR blurOffsetName("SSDO_BlurOffset");
		static CCryNameR blurKernelName("SSDO_BlurKernel");

		m_passFilter.BeginConstantUpdate();

		Vec4 v = Vec4((float)srcSizeX, (float)srcSizeY, 0, 0);
		m_passFilter.SetConstant(sourceTexSizeName, v, eHWSC_Pixel);
		v = Vec4(0.5f / (float)sizeX, 0.5f / (float)sizeY, 1.0f / (float)srcSizeX, 1.0f / (float)srcSizeY);
		m_passFilter.SetConstant(blurOffsetName, v, eHWSC_Pixel);
		v = Vec4(2.0f / srcSizeX, 0, 2.0f / srcSizeY, 10.0f);  // w: weight coef
		m_passFilter.SetConstant(blurKernelName, v, eHWSC_Pixel);

		m_passFilter.Execute();
	}
	else  // For debugging
	{
		PostProcessUtils().StretchRect(pDestRT, CTexture::s_ptexSceneNormalsBent);
	}

	if (CRenderer::CV_r_ssdoColorBleeding)
	{
		// Generate low frequency scene albedo for color bleeding (convolution not gamma correct but acceptable)
		m_passAlbedoDownsample0.Execute(CTexture::s_ptexSceneDiffuse, CTexture::s_ptexBackBufferScaled[0]);
		m_passAlbedoDownsample1.Execute(CTexture::s_ptexBackBufferScaled[0], CTexture::s_ptexBackBufferScaled[1]);
		m_passAlbedoDownsample2.Execute(CTexture::s_ptexBackBufferScaled[1], CTexture::s_ptexAOColorBleed);
		m_passAlbedoBlur.Execute(CTexture::s_ptexAOColorBleed, CTexture::s_ptexBackBufferScaled[2], 1.0f, 4.0f);
	}
}
