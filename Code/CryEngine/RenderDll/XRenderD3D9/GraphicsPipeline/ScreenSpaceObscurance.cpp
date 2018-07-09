// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScreenSpaceObscurance.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"
#include "HeightMapAO.h"

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
	m_passObscurance.AllocateTypedConstantBuffer<ObscuranceConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel);
}

void CScreenSpaceObscuranceStage::Execute()
{
	if (!CRenderer::CV_r_ssdo)
	{
		CClearSurfacePass::Execute(CRendererResources::s_ptexSceneNormalsBent, Clr_Median);
		return;
	}

	PROFILE_LABEL_SCOPE("DIRECTIONAL_OCC");

#if defined(DURANGO_USE_ESRAM)
	m_passCopyFromESRAM.Execute(CRendererResources::s_ptexSceneSpecularESRAM, CRendererResources::s_ptexSceneSpecular);
#endif

	CTexture* CRendererResources__s_ptexSceneSpecular = CRendererResources::CRendererResources::s_ptexSceneSpecularTmp;
#if defined(DURANGO_USE_ESRAM)
	CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexSceneSpecularESRAM;
#endif

	const bool bLowResOutput = (CRenderer::CV_r_ssdoHalfRes == 3);
	if (bLowResOutput)
		CRendererResources__s_ptexSceneSpecular = CRendererResources::s_ptexBackBufferScaled[0];

	CShader* pShader = CShaderMan::s_shDeferredShading;

	auto* heightMapAO = GetStdGraphicsPipeline().GetHeightMapAOStage();

	// Obscurance generation
	{
		uint64 rtMask = 0;
		rtMask |= CRenderer::CV_r_ssdoHalfRes ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
		rtMask |= heightMapAO->GetHeightMapFrustum() ? g_HWSR_MaskBit[HWSR_SAMPLE1] : 0;
		rtMask |= CRenderer::CV_r_DeferredShadingTiled == 4 ? g_HWSR_MaskBit[HWSR_SAMPLE2] : 0;

		// Extreme magnification as happening with small FOVs will cause banding issues with half-res depth
		if (CRenderer::CV_r_ssdoHalfRes == 2 && RAD2DEG(RenderView()->GetCamera(CCamera::eEye_Left).GetFov()) < 30)
			rtMask &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];

		static CCryNameTSCRC techSampling("SSDO_Sampling");

		m_passObscurance.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passObscurance.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
		m_passObscurance.SetTechnique(pShader, techSampling, rtMask);
		m_passObscurance.SetRenderTarget(0, CRendererResources__s_ptexSceneSpecular);
		m_passObscurance.SetState(GS_NODEPTHTEST);
		m_passObscurance.SetRequirePerViewConstantBuffer(true);
		m_passObscurance.SetFlags(CPrimitiveRenderPass::ePassFlags_VrProjectionPass);

		// These 'pairs' all used the same samplerstate!!
		m_passObscurance.SetTexture(0, CRendererResources::s_ptexSceneNormalsMap);
		m_passObscurance.SetTexture(1, CRendererResources::s_ptexLinearDepth);
		m_passObscurance.SetTexture(5, CRendererResources::s_ptexLinearDepthScaled[bLowResOutput]);
		m_passObscurance.SetTexture(11, heightMapAO->GetHeightMapAOScreenDepthTex());
		m_passObscurance.SetTexture(12, heightMapAO->GetHeightMapAOTex());
		m_passObscurance.SetSampler(0, EDefaultSamplerStates::PointClamp);

		m_passObscurance.SetTexture(3, CRendererResources::s_ptexAOVOJitter);
		m_passObscurance.SetSampler(1, EDefaultSamplerStates::PointWrap);

		{
			SRenderViewInfo viewInfo[CCamera::eEye_eCount];
			size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);

			Vec4 viewSpaceParams[CCamera::eEye_eCount];
			for (uint32 i = 0; i < viewInfoCount; i++)
			{
				Matrix44 projMat = viewInfo[i].projMatrix;
				float stereoShift = projMat.m20 * 2.0f;
				viewSpaceParams[i] = Vec4(2.0f / projMat.m00, 2.0f / projMat.m11, (-1.0f + stereoShift) / projMat.m00, -1.0f / projMat.m11);
			}
			
			auto constants = m_passObscurance.BeginTypedConstantUpdate<ObscuranceConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Pixel);

			constants->screenSize = Vec4((float)CRendererResources::s_ptexLinearDepth->GetWidth(), (float)CRendererResources::s_ptexLinearDepth->GetHeight(), 1.0f / (float)CRendererResources__s_ptexSceneSpecular->GetWidth(), 1.0f / (float)CRendererResources__s_ptexSceneSpecular->GetHeight());
			constants->nearFarClipDist = Vec4(viewInfo[0].nearClipPlane, viewInfo[0].farClipPlane, 0, 0);
		
			float radius = CRenderer::CV_r_ssdoRadius / viewInfo[0].farClipPlane;
	#if defined(FEATURE_SVO_GI)
			if (CSvoRenderer::GetInstance()->IsActive())
				radius *= CSvoRenderer::GetInstance()->GetSsaoAmount();
	#endif
			constants->ssdoParams = Vec4(radius * 0.5f * viewInfo[0].projMatrix.m00, radius * 0.5f * viewInfo[0].projMatrix.m11,
																	 CRenderer::CV_r_ssdoRadiusMin, CRenderer::CV_r_ssdoRadiusMax);

			constants->viewSpaceParams = viewSpaceParams[0];

			if (heightMapAO->GetHeightMapFrustum())
			{
				assert(heightMapAO->GetHeightMapAOTex() && heightMapAO->GetHeightMapAOScreenDepthTex());
				assert(heightMapAO->GetHeightMapAOTex()->GetWidth() == heightMapAO->GetHeightMapAOScreenDepthTex()->GetWidth() && heightMapAO->GetHeightMapAOTex()->GetHeight() == heightMapAO->GetHeightMapAOScreenDepthTex()->GetHeight());
				const float resolutionFactor = (float)heightMapAO->GetHeightMapAOTex()->GetWidth() / (float)CRendererResources__s_ptexSceneSpecular->GetWidth();
				constants->hmaoParams = Vec4(1.0f / (float)heightMapAO->GetHeightMapAOTex()->GetWidth(), 1.0f / (float)heightMapAO->GetHeightMapAOTex()->GetHeight(), resolutionFactor, 0);
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
		const int32 sizeX = CRendererResources::s_ptexLinearDepth->GetWidth();
		const int32 sizeY = CRendererResources::s_ptexLinearDepth->GetHeight();
		const int32 srcSizeX = CRendererResources__s_ptexSceneSpecular->GetWidth();
		const int32 srcSizeY = CRendererResources__s_ptexSceneSpecular->GetHeight();

		if (m_passFilter.IsDirty(bLowResOutput))
		{
			static CCryNameTSCRC techFilter("SSDO_Filter");
			m_passFilter.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
			m_passFilter.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_passFilter.SetTechnique(pShader, techFilter, 0);
			m_passFilter.SetRenderTarget(0, CRendererResources::s_ptexSceneNormalsBent);
			m_passFilter.SetState(GS_NODEPTHTEST);
			m_passFilter.SetTexture(0, CRendererResources__s_ptexSceneSpecular);
			m_passFilter.SetTexture(1, CRendererResources::s_ptexLinearDepth);
			m_passFilter.SetSampler(0, EDefaultSamplerStates::LinearClamp);
			m_passFilter.SetSampler(1, EDefaultSamplerStates::PointClamp);
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
		PostProcessUtils().StretchRect(CRendererResources__s_ptexSceneSpecular, CRendererResources::s_ptexSceneNormalsBent);
	}

	if (CRenderer::CV_r_ssdoColorBleeding)
	{
		// Generate low frequency scene albedo for color bleeding (convolution not gamma correct but acceptable)
		m_passAlbedoDownsample0.Execute(CRendererResources::s_ptexSceneDiffuse, CRendererResources::s_ptexBackBufferScaled[0]);
		m_passAlbedoDownsample1.Execute(CRendererResources::s_ptexBackBufferScaled[0], CRendererResources::s_ptexBackBufferScaled[1]);
		m_passAlbedoDownsample2.Execute(CRendererResources::s_ptexBackBufferScaled[1], CRendererResources::s_ptexAOColorBleed);
		m_passAlbedoBlur.Execute(CRendererResources::s_ptexAOColorBleed, CRendererResources::s_ptexBackBufferScaled[2], 1.0f, 4.0f);
	}
}
