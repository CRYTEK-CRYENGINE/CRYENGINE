// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ToneMapping.h"

#include "SunShafts.h"
#include "ColorGrading.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include <Common/RenderDisplayContext.h>

void CToneMappingStage::Execute()
{
	// 0 is used for disable debugging and 1 is used to just show the average and estimated luminance, and exposure values.
	if (CRenderer::CV_r_HDRDebug > 1)
	{
		ExecuteDebug();
		return;
	}

	PROFILE_LABEL_SCOPE("TONEMAPPING");

	CSunShaftsStage*    pSunShaftsStage    = (CSunShaftsStage   *)GetStdGraphicsPipeline().GetStage(eStage_Sunshafts);
	CColorGradingStage* pColorGradingStage = (CColorGradingStage*)GetStdGraphicsPipeline().GetStage(eStage_ColorGrading);

	bool bSunShafts = false;
	bool bHighQualitySunshafts = false;
	bool bColorGrading = false;
	bool bBloomEnabled = CRenderer::CV_r_HDRBloom && CRenderer::CV_r_PostProcess;
	bool bApplyDithering = CRenderer::CV_r_HDRDithering && CRenderer::CV_r_PostProcess;
	bool bVignettingEnabled = CRenderer::CV_r_HDRVignetting && CRenderer::CV_r_PostProcess;

	CShader* pShader = CShaderMan::s_shHDRPostProcess;
	CTexture* pSunShaftsTex = CRendererResources::s_ptexBlack;
	CTexture* pColorChartTex = CRendererResources::s_ptexBlack;

	bSunShafts = pSunShaftsStage->IsActive();

	if (bSunShafts)
		pSunShaftsTex = pSunShaftsStage->GetFinalOutputRT();

	if (CTexture* pColorChartTexTentative = pColorGradingStage->GetColorChart())
	{
		bColorGrading = true;
		pColorChartTex = pColorChartTexTentative;
	}

	int featureMask = ((int)bSunShafts << 1) | ((int)bColorGrading << 2) | ((int)bBloomEnabled << 3) |
	                  ((CRenderer::CV_r_HDREyeAdaptationMode & 0xF) << 5) | ((CRenderer::CV_r_HDRDebug & 0xF) << 9);

	if (m_passToneMapping.IsDirty(featureMask, pSunShaftsTex->GetTextureID(), pColorChartTex->GetTextureID()))
	{
		uint64 rtMask = 0;
		if (CRenderer::CV_r_HDREyeAdaptationMode == 2)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE4];
		if (bColorGrading)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE1];
		if (bVignettingEnabled)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE2];
		if (bBloomEnabled)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE3];
		if (bSunShafts)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE5];
		if (bApplyDithering)
			rtMask |= g_HWSR_MaskBit[HWSR_SAMPLE6];

		static CCryNameTSCRC techToneMapping("HDRFinalPass");
		m_passToneMapping.SetTechnique(pShader, techToneMapping, rtMask);
		m_passToneMapping.SetRenderTarget(0, CRendererResources::s_ptexSceneDiffuse);
		m_passToneMapping.SetState(GS_NODEPTHTEST);
		m_passToneMapping.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);
		m_passToneMapping.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants);	// Enables reflection constants addition in the shader

		CTexture* pBloomTex = bBloomEnabled ? CRendererResources::s_ptexHDRFinalBloom : CRendererResources::s_ptexBlack;

		m_passToneMapping.SetSampler(0, EDefaultSamplerStates::LinearClamp);
		m_passToneMapping.SetTexture(0, CRendererResources::s_ptexHDRTarget);
		m_passToneMapping.SetTexture(1, CRendererResources::s_ptexCurLumTexture);
		m_passToneMapping.SetTexture(2, pBloomTex);
		m_passToneMapping.SetTexture(7, CRendererResources::s_ptexVignettingMap);
		m_passToneMapping.SetTexture(8, pColorChartTex);
		m_passToneMapping.SetTexture(9, pSunShaftsTex);
		m_passToneMapping.SetRequireWorldPos(true);
		m_passToneMapping.SetRequirePerViewConstantBuffer(true);
	}

	static CCryNameR eyeAdaptationName("HDREyeAdaptation");
	static CCryNameR filmCurveName("HDRFilmCurve");
	static CCryNameR colorBalanceName("HDRColorBalance");
	static CCryNameR bloomColorName("HDRBloomColor");
	static CCryNameR shaftsSunColName("SunShafts_SunCol");

	m_passToneMapping.BeginConstantUpdate();

	Vec4 hdrSetupParams[5];
	gEnv->p3DEngine->GetHDRSetupParams(hdrSetupParams);

	m_passToneMapping.SetConstant(eyeAdaptationName, CRenderer::CV_r_HDREyeAdaptationMode == 2 ? hdrSetupParams[4] : hdrSetupParams[3], eHWSC_Pixel);
	m_passToneMapping.SetConstant(filmCurveName, hdrSetupParams[0], eHWSC_Pixel);
	m_passToneMapping.SetConstant(colorBalanceName, hdrSetupParams[2], eHWSC_Pixel);
	m_passToneMapping.SetConstant(bloomColorName, hdrSetupParams[1] * Vec4(Vec3(1.0f / 8.0f), 1.0f), eHWSC_Pixel); // Division by 8.0f was done in shader before, remove this at some point

	Vec4 shaftsSunCol(0, 0, 0, 0);
	if (bSunShafts)
	{
		Vec4 sunShaftParams[2];
		pSunShaftsStage->GetCompositionParams(sunShaftParams[0], sunShaftParams[1]);
		Vec3 sunColor = gEnv->p3DEngine->GetSunColor();
		sunColor.Normalize();
		sunColor.SetLerp(Vec3(sunShaftParams[0].x, sunShaftParams[0].y, sunShaftParams[0].z), sunColor, sunShaftParams[1].w);
		shaftsSunCol = Vec4(sunColor * sunShaftParams[1].z, 1);
	}
	m_passToneMapping.SetConstant(shaftsSunColName, shaftsSunCol, eHWSC_Pixel);

	m_passToneMapping.Execute();

	m_pColorChartTex = pColorChartTex; // Keep a ref count on color chart to make sure it is destroyed after m_passToneMapping
}

void CToneMappingStage::ExecuteDebug()
{
	PROFILE_LABEL_SCOPE("TONEMAPPING-DEBUG");

	CShader* pShader = CShaderMan::s_shHDRPostProcess;

	int featureMask = ((CRenderer::CV_r_HDRDebug & 0xF) << 9);

	if (m_passToneMapping.IsDirty(featureMask))
	{
		uint64 rtMask = 0;

		// Supported HDR Debugging options
		if (CRenderer::CV_r_HDRDebug == 2)
			rtMask |= g_HWSR_MaskBit[HWSR_DEBUG0];
		if (CRenderer::CV_r_HDRDebug == 3)
			rtMask |= g_HWSR_MaskBit[HWSR_DEBUG1];

		static CCryNameTSCRC techToneMapping("HDRFinalDebugPass");
		const auto primFlags = CRenderer::CV_r_HDRDebug == 2 ? CRenderPrimitive::eFlags_ReflectShaderConstants_PS : CRenderPrimitive::eFlags_None;

		m_passToneMapping.SetTechnique(pShader, techToneMapping, rtMask);
		m_passToneMapping.SetRenderTarget(0, CRendererResources::s_ptexSceneDiffuse);
		m_passToneMapping.SetState(GS_NODEPTHTEST);
		m_passToneMapping.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);	
		m_passToneMapping.SetPrimitiveFlags(primFlags);
		m_passToneMapping.SetSampler(0, EDefaultSamplerStates::LinearClamp);
		m_passToneMapping.SetTexture(0, CRendererResources::s_ptexHDRTarget);
		m_passToneMapping.SetTexture(1, CRendererResources::s_ptexCurLumTexture);
		m_passToneMapping.SetRequireWorldPos(true);
		m_passToneMapping.SetRequirePerViewConstantBuffer(true);
	}


	if (CRenderer::CV_r_HDRDebug == 2)
	{
		m_passToneMapping.BeginConstantUpdate();

		Vec4 hdrSetupParams[5];
		gEnv->p3DEngine->GetHDRSetupParams(hdrSetupParams);

		static CCryNameR eyeAdaptationName("HDREyeAdaptation");
		m_passToneMapping.SetConstant(eyeAdaptationName, CRenderer::CV_r_HDREyeAdaptationMode == 2 ? hdrSetupParams[4] : hdrSetupParams[3], eHWSC_Pixel);
	}

	m_passToneMapping.Execute();
}

void CToneMappingStage::ExecuteFixedExposure(CTexture* pColorTex, CTexture* pDepthTex)
{
	PROFILE_LABEL_SCOPE("TONEMAPPING_FIXED_EXPOSURE");

	CRenderView* pRenderView = RenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();

//	ASSERT_LEGACY_PIPELINE
	return;
	// TODO: tonemap in-place (sadly)

	CTexture* pSrcTex = CRendererResources::s_ptexHDRTarget;
	CTexture* pDstTex = CRendererResources::s_ptexSceneDiffuse;

	auto& pass = m_passFixedExposureToneMapping;

	{
		uint64 rtMask = 0;
		static CCryNameTSCRC techToneMapping("HDRFinalPassFixedExposure");
		pass.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
		pass.SetTechnique(CShaderMan::s_shHDRPostProcess, techToneMapping, rtMask);
		pass.SetRenderTarget(0, pDstTex);
		pass.SetState(GS_NODEPTHTEST);
		pass.SetFlags(CPrimitiveRenderPass::ePassFlags_RequireVrProjectionConstants);

		pass.SetTextureSamplerPair(0, pSrcTex, EDefaultSamplerStates::LinearClamp);
		pass.SetRequireWorldPos(true);
		pass.SetRequirePerViewConstantBuffer(true);
	}

	pass.BeginConstantUpdate();

#if 0
	// parameter for filmic response curve.
	static CCryNameR filmCurveName("HDRFilmCurve");
	Vec4 hdrSetupParams[5];
	gEnv->p3DEngine->GetHDRSetupParams(hdrSetupParams);
	pass.SetConstant(filmCurveName, hdrSetupParams[0], eHWSC_Pixel);
#endif

	pass.Execute();
}

void CToneMappingStage::DisplayDebugInfo()
{
	if (CRenderer::CV_r_HDRDebug != 1)
		return;

	float fLuminance = -1.f;
	float fIlluminance = -1.f;

	CDeviceTexture* pSrcDevTex = CRendererResources::s_ptexHDRToneMaps[0]->GetDevTexture();
	CRY_ASSERT(pSrcDevTex);

	// Read back data
	const auto readbackData = [&fLuminance, &fIlluminance](void* pData, uint32 rowPitch, uint32 slicePitch) -> bool
	{
		CryHalf* pDataHalf = (CryHalf*)pData;
		fLuminance = CryConvertHalfToFloat(pDataHalf[0]);
		fIlluminance = CryConvertHalfToFloat(pDataHalf[1]);
		return true;
	};

	pSrcDevTex->DownloadToStagingResource(0, readbackData);

	// Display data
	char str[256];
	sprintf(str, "Average Luminance (cd/m2): %.2f", fLuminance * RENDERER_LIGHT_UNIT_SCALE);
	IRenderAuxText::DrawText(Vec3(5, 35, 1), IRenderAuxText::ASize(1.0f), ColorF(1, 0, 1, 1), eDrawText_800x600 | eDrawText_2D, str);
	uint32 illum = (uint32)(fIlluminance * RENDERER_LIGHT_UNIT_SCALE);
	sprintf(str, "Estimated Illuminance (lux): %03d,%03d", illum / 1000, illum % 1000);
	IRenderAuxText::DrawText(Vec3(5, 55, 1), IRenderAuxText::ASize(1.0f), ColorF(1, 0, 1, 1), eDrawText_800x600 | eDrawText_2D, str);

	Vec4 vHDRSetupParams[5];
	gEnv->p3DEngine->GetHDRSetupParams(vHDRSetupParams);

	if (CRenderer::CV_r_HDREyeAdaptationMode == 2)
	{
		// Compute scene key and exposure in the same way as in the tone mapping shader
		float sceneKey = 1.03f - 2.0f / (2.0f + log(fLuminance + 1.0f) / log(2.0f));
		float exposure = clamp_tpl<float>(sceneKey / fLuminance, vHDRSetupParams[4].y, vHDRSetupParams[4].z);

		sprintf(str, "Exposure: %.2f  SceneKey: %.2f", exposure, sceneKey);
		IRenderAuxText::DrawText(Vec3(5, 75, 1), IRenderAuxText::ASize(1.0f), ColorF(1, 0, 1, 1), eDrawText_800x600 | eDrawText_2D, str);
	}
	else
	{
		float exposure = log(fIlluminance * RENDERER_LIGHT_UNIT_SCALE * 100.0f / 330.0f) / log(2.0f);
		float sceneKey = log(fIlluminance * RENDERER_LIGHT_UNIT_SCALE + 1.0f) / log(10.0f);
		float autoCompensation = (clamp_tpl<float>(sceneKey, 0.1f, 5.2f) - 3.0f) / 2.0f * vHDRSetupParams[3].z;
		float finalExposure = clamp_tpl<float>(exposure - autoCompensation, vHDRSetupParams[3].x, vHDRSetupParams[3].y);

		sprintf(str, "Measured EV: %.1f  Auto-EC: %.1f  Final EV: %.1f", exposure, autoCompensation, finalExposure);
		IRenderAuxText::DrawText(Vec3(5, 75, 1), IRenderAuxText::ASize(1.0f), ColorF(1, 0, 1, 1), eDrawText_800x600 | eDrawText_2D, str);
	}
}
