// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HeightMapAO.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/ClipVolumes.h"

void CHeightMapAOStage::Init()
{
	const bool shouldApplyHMAO = CRendererCVars::CV_r_HeightMapAO > 0 ? true : false;
	const int resourceSize = (int)clamp_tpl(CRendererCVars::CV_r_HeightMapAOResolution, 0.f, 16384.f);
	const ETEX_Format texFormat = CRendererCVars::CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;

	ResizeDepthResources(shouldApplyHMAO, resourceSize, resourceSize, texFormat);

	m_nHeightMapAOConfig = CRendererCVars::CV_r_HeightMapAO;
	m_nHeightMapAOAmount = CRendererCVars::CV_r_HeightMapAOAmount;
}

void CHeightMapAOStage::ResizeScreenResources(bool shouldApplyHMAO, int resourceWidth, int resourceHeight)
{
	ETEX_Format texFormat = eTF_R8G8;

	std::string heightMapAOTex0Name = "$HeightMapAO_0" + m_graphicsPipeline.GetUniqueIdentifierName();
	std::string heightMapAOTex1Name = "$HeightMapAO_1" + m_graphicsPipeline.GetUniqueIdentifierName();
	if (!m_pHeightMapAOMask[0])
	{
		m_pHeightMapAOMask[0] = CTexture::GetOrCreateRenderTarget(heightMapAOTex0Name.c_str(), resourceWidth, resourceHeight, Clr_Neutral, eTT_2D, FT_DONT_STREAM, eTF_Unknown);
		m_pHeightMapAOMask[1] = CTexture::GetOrCreateRenderTarget(heightMapAOTex1Name.c_str(), resourceWidth, resourceHeight, Clr_Neutral, eTT_2D, FT_DONT_STREAM, eTF_Unknown);
	}

	if (!shouldApplyHMAO)
	{
		if (CTexture::IsTextureExist(m_pHeightMapAOMask[0]))
			m_pHeightMapAOMask[0]->ReleaseDeviceTexture(false);
		if (CTexture::IsTextureExist(m_pHeightMapAOMask[1]))
			m_pHeightMapAOMask[1]->ReleaseDeviceTexture(false);
	}
	else
	{
		if (m_pHeightMapAOMask[0]->Invalidate(resourceWidth, resourceHeight, texFormat) || !CTexture::IsTextureExist(m_pHeightMapAOMask[0]))
			m_pHeightMapAOMask[0]->CreateRenderTarget(texFormat, Clr_Neutral);
		if (m_pHeightMapAOMask[1]->Invalidate(resourceWidth, resourceHeight, texFormat) || !CTexture::IsTextureExist(m_pHeightMapAOMask[1]))
			m_pHeightMapAOMask[1]->CreateRenderTarget(texFormat, Clr_Neutral);
	}
}

void CHeightMapAOStage::ResizeDepthResources(bool shouldApplyHMAO, int resourceWidth, int resourceHeight, ETEX_Format texFormatDepth)
{
	ETEX_Format texFormatMips = CRendererResources::s_hwTexFormatSupport.GetLessPreciseFormatSupported(texFormatDepth == eTF_D32F ? eTF_R32F : eTF_R16);

	std::string heightMapAODepth0Name = "$HeightMapAO_Depth" + m_graphicsPipeline.GetUniqueIdentifierName();
	std::string heightMapAODepth1Name = "$HeightMapAO_Pyramid" + m_graphicsPipeline.GetUniqueIdentifierName();
	if (!m_pHeightMapAODepth[0])
	{
		m_pHeightMapAODepth[0] = CTexture::GetOrCreateDepthStencil(heightMapAODepth0Name.c_str(), resourceWidth, resourceHeight, Clr_FarPlane, eTT_2D, FT_DONT_STREAM, eTF_Unknown);
		m_pHeightMapAODepth[1] = CTexture::GetOrCreateRenderTarget(heightMapAODepth1Name.c_str(), resourceWidth, resourceHeight, Clr_FarPlane, eTT_2D, FT_DONT_STREAM | FT_FORCE_MIPS, eTF_Unknown);
	}

	if (!shouldApplyHMAO)
	{
		if (CTexture::IsTextureExist(m_pHeightMapAODepth[0]))
			m_pHeightMapAODepth[0]->ReleaseDeviceTexture(false);
		if (CTexture::IsTextureExist(m_pHeightMapAODepth[1]))
			m_pHeightMapAODepth[1]->ReleaseDeviceTexture(false);
	}
	else
	{
		if (m_pHeightMapAODepth[0]->Invalidate(resourceWidth, resourceHeight, texFormatDepth) || !CTexture::IsTextureExist(m_pHeightMapAODepth[0]))
			m_pHeightMapAODepth[0]->CreateDepthStencil(texFormatDepth, Clr_FarPlane);
		if (m_pHeightMapAODepth[1]->Invalidate(resourceWidth, resourceHeight, texFormatMips) || !CTexture::IsTextureExist(m_pHeightMapAODepth[1]))
			m_pHeightMapAODepth[1]->CreateRenderTarget(texFormatMips, Clr_FarPlane);
	}
}

void CHeightMapAOStage::Rescale(int resolutionScale)
{
	const bool shouldApplyHMAO = m_nHeightMapAOConfig > 0 ? true : false;
	const CRenderView* pRenderView = RenderView();

	int hmaoWidth  = pRenderView->GetRenderResolution()[0];
	int hmaoHeight = pRenderView->GetRenderResolution()[1];
	for (int i = 0; i < resolutionScale; i++)
	{
		hmaoWidth  = (hmaoWidth  + 1) / 2;
		hmaoHeight = (hmaoHeight + 1) / 2;
	}

	ResizeScreenResources(shouldApplyHMAO, hmaoWidth, hmaoHeight);
}

void CHeightMapAOStage::Resize(int renderWidth, int renderHeight)
{
	const bool shouldApplyHMAO = m_nHeightMapAOConfig > 0 ? true : false;
	const int scaleHMAO = clamp_tpl(3 - m_nHeightMapAOConfig, 0, 2);

	int hmaoWidth  = renderWidth;
	int hmaoHeight = renderHeight;
	for (int i = 0; i < scaleHMAO; i++)
	{
		hmaoWidth  = (hmaoWidth  + 1) / 2;
		hmaoHeight = (hmaoHeight + 1) / 2;
	}

	ResizeScreenResources(shouldApplyHMAO, hmaoWidth, hmaoHeight);
}

void CHeightMapAOStage::OnCVarsChanged(const CCVarUpdateRecorder& cvarUpdater)
{
	if (auto pVar = cvarUpdater.GetCVar("r_HeightMapAO"))
	{
		m_nHeightMapAOConfig = pVar->intValue;

		bool shouldApplyHMAO = m_nHeightMapAOConfig > 0 ? true : false;
		const int scaleHMAO = clamp_tpl(3 - m_nHeightMapAOConfig, 0, 2);

		if (shouldApplyHMAO)
			Rescale(scaleHMAO);
		else
			ResizeScreenResources(false, 0, 0);
	}

	if (auto pVar = cvarUpdater.GetCVar("r_HeightMapAOAmount"))
	{
		m_nHeightMapAOAmount = pVar->floatValue;
	}

	if (cvarUpdater.GetCVar("r_HeightMapAO") ||
		cvarUpdater.GetCVar("r_HeightMapAOResolution") ||
		cvarUpdater.GetCVar("r_ShadowsCacheFormat"))
	{
		bool shouldApplyHMAO = m_nHeightMapAOConfig > 0 ? true : false;
		if (auto pVar = cvarUpdater.GetCVar("r_HeightMapAO"))
			shouldApplyHMAO = pVar->intValue > 0 ? true : false;

		int resourceSize = (int)clamp_tpl(CRendererCVars::CV_r_HeightMapAOResolution, 0.f, 16384.f);
		if (auto pVar = cvarUpdater.GetCVar("r_HeightMapAOResolution"))
			resourceSize = (int)clamp_tpl(pVar->floatValue, 0.f, 16384.f);

		ETEX_Format texFormat = CRendererCVars::CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;
		if (auto pVar = cvarUpdater.GetCVar("r_ShadowsCacheFormat"))
			texFormat = pVar->intValue ? eTF_D32F : eTF_D16;

		ResizeDepthResources(shouldApplyHMAO, resourceSize, resourceSize, texFormat);
	}
}

void CHeightMapAOStage::Update()
{
	m_bHeightMapAOExecuted = false;
	m_pHeightMapFrustum = nullptr;
	m_pHeightMapAOScreenDepthTex = CRendererResources::s_ptexWhite;
	m_pHeightMapAOTex = CRendererResources::s_ptexWhite;
}

void CHeightMapAOStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();

	CD3D9Renderer* const __restrict pRenderer = gcpRendD3D;
	CDeferredShading* pDeferredShading = m_graphicsPipeline.GetDeferredShading();

	CRY_ASSERT(!m_bHeightMapAOExecuted);
	m_bHeightMapAOExecuted = true;

	if (m_graphicsPipelineResources.m_pTexClipVolumes == nullptr)
	{
		pDeferredShading->SetupPasses(RenderView());
	}

	// Prepare Height Map AO frustum
	CShadowUtils::SShadowsSetupInfo shadowsSetup;
	CRenderView* pHeightmapRenderView = nullptr;
	const auto& heightmapAOFrustums = RenderView()->GetShadowFrustumsByType(CRenderView::eShadowFrustumRenderType_HeightmapAO);
	if (!heightmapAOFrustums.empty())
	{
		const ShadowMapFrustum* pFrustum = heightmapAOFrustums.front()->pFrustum;
		if (pFrustum->pDepthTex)
		{
			shadowsSetup = pRenderer->ConfigShadowTexgen(RenderView(), pFrustum);

			m_pHeightMapFrustum = pFrustum;
			pHeightmapRenderView = reinterpret_cast<CRenderView*>(heightmapAOFrustums.front()->pShadowsView.get());
		}
	}

	if (m_pHeightMapFrustum)
	{
		PROFILE_LABEL_SCOPE("HEIGHTMAP_OCC");

		const int resolutionIndex = clamp_tpl(m_nHeightMapAOConfig - 1, 0, 2);
		CTexture* pDepthTextures[] = {
			m_graphicsPipelineResources.m_pTexLinearDepthScaled[1],
			m_graphicsPipelineResources.m_pTexLinearDepthScaled[0],
			m_graphicsPipelineResources.m_pTexLinearDepth };
		CTexture* pDestRT = m_pHeightMapAOMask[0];

		m_pHeightMapAOScreenDepthTex = pDepthTextures[resolutionIndex];
		m_pHeightMapAOTex = m_pHeightMapAOMask[1];

		// Q: What is the 0-list?
		if (pHeightmapRenderView->GetRenderItems(ERenderListID(0)).size() > 0)
		{
			PROFILE_LABEL_SCOPE("GENERATE_MIPS");

			const SResourceRegionMapping mapping =
			{
				{ 0, 0, 0, 0 }, // src position
				{ 0, 0, 0, 0 }, // dst position
				m_pHeightMapAODepth[0]->GetDevTexture()->GetDimension()
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				m_pHeightMapAODepth[0]->GetDevTexture(), // DSV
				m_pHeightMapAODepth[1]->GetDevTexture(), // SRV
				mapping);

			m_passMipmapGen.Execute(m_pHeightMapAODepth[1], 3);
		}

		// Generate occlusion
		{
			PROFILE_LABEL_SCOPE("GENERATE_OCCL");

			CShader* pShader = CShaderMan::s_shDeferredShading;

			if (m_passSampling.IsDirty(resolutionIndex))
			{
				static CCryNameTSCRC techSampling("HeightMapAOPass");
				m_passSampling.SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_PS);
				m_passSampling.SetTechnique(pShader, techSampling, 0);
				m_passSampling.SetRenderTarget(0, pDestRT);
				m_passSampling.SetState(GS_NODEPTHTEST);

				m_passSampling.SetTexture(0, m_pHeightMapAOScreenDepthTex);
				m_passSampling.SetTexture(1, m_pHeightMapAODepth[1]);

				m_passSampling.SetSampler(0, EDefaultSamplerStates::PointClamp);
				m_passSampling.SetSampler(1, EDefaultSamplerStates::TrilinearBorder_White);

				m_passSampling.SetRequireWorldPos(true);
				m_passSampling.SetRequirePerViewConstantBuffer(true);
			}

			static CCryNameR nameParams("HMAO_Params");
			static CCryNameR nameTexToWorldT("HMAO_TexToWorldTranslation");
			static CCryNameR nameTexToWorldS("HMAO_TexToWorldScale");
			static CCryNameR nameTransform("HMAO_Transform");

			m_passSampling.BeginConstantUpdate();

			Matrix44A matHMAOTransform = shadowsSetup.ShadowMat;
			Matrix44A texToWorld = matHMAOTransform.GetInverted();

			const float texelsPerMeter = CRenderer::CV_r_HeightMapAOResolution / CRenderer::CV_r_HeightMapAORange;
			const bool enableMinMaxSampling = m_nHeightMapAOConfig < 3;
			Vec4 paramHMAO(m_nHeightMapAOAmount, texelsPerMeter / m_pHeightMapAODepth[1]->GetWidth(), enableMinMaxSampling ? 1.0f : 0.0f, 0.0f);
			m_passSampling.SetConstant(nameParams, paramHMAO, eHWSC_Pixel);

			Vec4 translation = Vec4(texToWorld.m03, texToWorld.m13, texToWorld.m23, 0);
			m_passSampling.SetConstant(nameTexToWorldT, translation, eHWSC_Pixel);

			Vec4 scale = Vec4(texToWorld.m00, texToWorld.m11, texToWorld.m22, 1);
			m_passSampling.SetConstant(nameTexToWorldS, scale, eHWSC_Pixel);

			m_passSampling.SetConstantArray(nameTransform, (Vec4*)matHMAOTransform.GetData(), 4, eHWSC_Pixel);

			m_passSampling.Execute();
		}

		// Depth aware blur
		{
			PROFILE_LABEL_SCOPE("BLUR");

			CShader* pShader = pRenderer->m_cEF.s_ShaderShadowBlur;

			const Vec4* pClipVolumeParams = nullptr;
			uint32 clipVolumeCount = 0;

			if (auto pClipVolumesStage = m_graphicsPipeline.GetStage<CClipVolumesStage>())
				clipVolumeCount = pClipVolumesStage->GetClipVolumeShaderParams(pClipVolumeParams);

			if (m_passSmoothing.IsDirty(resolutionIndex, clipVolumeCount > 0 ? 1 : 0))
			{
				uint64 rtMask = clipVolumeCount > 0 ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;

				static CCryNameTSCRC techSmoothing("HMAO_Blur");
				m_passSmoothing.SetTechnique(pShader, techSmoothing, rtMask);
				m_passSmoothing.SetRenderTarget(0, m_pHeightMapAOTex);
				m_passSmoothing.SetState(GS_NODEPTHTEST);
				m_passSmoothing.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);

				m_passSmoothing.SetTextureSamplerPair(0, pDestRT, EDefaultSamplerStates::PointClamp);
				m_passSmoothing.SetTextureSamplerPair(1, m_pHeightMapAOScreenDepthTex, EDefaultSamplerStates::PointClamp);
				m_passSmoothing.SetTextureSamplerPair(2, m_graphicsPipelineResources.m_pTexClipVolumes, EDefaultSamplerStates::PointClamp);
			}

			static CCryNameR namePixelOffset("PixelOffset");
			static CCryNameR nameClipVolumeData("HMAO_ClipVolumeData");

			m_passSmoothing.BeginConstantUpdate();

			m_passSmoothing.SetConstant(namePixelOffset, Vec4(0, 0, (float)pDestRT->GetWidth(), (float)pDestRT->GetHeight()), eHWSC_Vertex);
			m_passSmoothing.SetConstantArray(nameClipVolumeData, pClipVolumeParams, CClipVolumesStage::MaxDeferredClipVolumes);
			m_passSmoothing.Execute();
		}
	}
}
