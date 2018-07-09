// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HeightMapAO.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "GraphicsPipeline/ClipVolumes.h"

void CHeightMapAOStage::Update()
{
	m_bHeightMapAOExecuted = false;
	m_pHeightMapFrustum = nullptr;
	m_pHeightMapAOScreenDepthTex = CRendererResources::s_ptexWhite;
	m_pHeightMapAOTex = CRendererResources::s_ptexWhite;
}

void CHeightMapAOStage::Execute()
{
	CD3D9Renderer* const __restrict pRenderer = gcpRendD3D;

	CRY_ASSERT(!m_bHeightMapAOExecuted);
	m_bHeightMapAOExecuted = true;

	if (!CRenderer::CV_r_HeightMapAO)
		return;

	if (CDeferredShading::Instance().GetResolvedStencilRT() == nullptr)
	{
		CDeferredShading::Instance().SetupPasses(RenderView());
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

		const int resolutionIndex = clamp_tpl(CRenderer::CV_r_HeightMapAO - 1, 0, 2);
		CTexture* pDepthTextures[] = { CRendererResources::s_ptexLinearDepthScaled[1], CRendererResources::s_ptexLinearDepthScaled[0], CRendererResources::s_ptexLinearDepth };
		CTexture* pDestRT = CRendererResources::s_ptexHeightMapAO[0];

		m_pHeightMapAOScreenDepthTex = pDepthTextures[resolutionIndex];
		m_pHeightMapAOTex = CRendererResources::s_ptexHeightMapAO[1];

		if (pHeightmapRenderView->GetRenderItems(0).size() > 0)
		{
			PROFILE_LABEL_SCOPE("GENERATE_MIPS");

			const SResourceRegionMapping mapping =
			{
				{ 0, 0, 0, 0 }, // src position
				{ 0, 0, 0, 0 }, // dst position
				CRendererResources::s_ptexHeightMapAODepth[0]->GetDevTexture()->GetDimension()
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(
				CRendererResources::s_ptexHeightMapAODepth[0]->GetDevTexture(), // DSV
				CRendererResources::s_ptexHeightMapAODepth[1]->GetDevTexture(), // SRV
				mapping);

			m_passMipmapGen.Execute(CRendererResources::s_ptexHeightMapAODepth[1], 3);
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

				m_passSampling.SetTexture(0, CRendererResources::s_ptexSceneNormalsMap);
				m_passSampling.SetTexture(1, m_pHeightMapAOScreenDepthTex);
				m_passSampling.SetTexture(10, CRendererResources::s_ptexSceneNormalsBent);
				m_passSampling.SetTexture(11, CRendererResources::s_ptexHeightMapAODepth[1]);

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
			const bool enableMinMaxSampling = CRenderer::CV_r_HeightMapAO < 3;
			Vec4 paramHMAO(CRenderer::CV_r_HeightMapAOAmount, texelsPerMeter / CRendererResources::s_ptexHeightMapAODepth[1]->GetWidth(), enableMinMaxSampling ? 1.0f : 0.0f, 0.0f);
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
			uint32 clipVolumeCount = RenderView()->GetClipVolumes().size();
			CDeferredShading::Instance().GetClipVolumeParams(pClipVolumeParams);

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
				m_passSmoothing.SetTextureSamplerPair(2, CDeferredShading::Instance().GetResolvedStencilRT(), EDefaultSamplerStates::PointClamp);
			}

			static CCryNameR namePixelOffset("PixelOffset");
			static CCryNameR nameClipVolumeData("HMAO_ClipVolumeData");

			m_passSmoothing.BeginConstantUpdate();

			m_passSmoothing.SetConstant(namePixelOffset, Vec4 (0, 0, (float)pDestRT->GetWidth(), (float)pDestRT->GetHeight()), eHWSC_Vertex);
			m_passSmoothing.SetConstantArray(nameClipVolumeData, pClipVolumeParams, CClipVolumesStage::MaxDeferredClipVolumes);
			m_passSmoothing.Execute();
		}
	}
}
