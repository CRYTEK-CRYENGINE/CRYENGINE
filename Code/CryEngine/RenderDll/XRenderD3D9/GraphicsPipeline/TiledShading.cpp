// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TiledShading.h"

#include "D3DPostProcess.h"
#include "D3D_SVO.h"

#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/ScreenSpaceObscurance.h"
#include "GraphicsPipeline/OmniCamera.h"
#include "GraphicsPipeline/ClipVolumes.h"

CTiledShadingStage::CTiledShadingStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_passCullingShading(&graphicsPipeline, CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
	, m_passCopyDepth(&graphicsPipeline)
{
}

CTiledShadingStage::~CTiledShadingStage()
{
}

void CTiledShadingStage::Init()
{
	m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
	if (m_pPerViewConstantBuffer) m_pPerViewConstantBuffer->SetDebugName("TiledShadingStage Per-View CB");
}

void CTiledShadingStage::Execute()
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE("TILED_SHADING");

	auto* clipVolumes = m_graphicsPipeline.GetStage<CClipVolumesStage>();
	auto* tiledLights = m_graphicsPipeline.GetStage<CTiledLightVolumesStage>();
	auto* ssObscurance = m_graphicsPipeline.GetStage<CScreenSpaceObscuranceStage>();

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	// on dx11, some input resources might still be bound as render targets.
	// dx11 will then just ignore the bind call
	ID3D11RenderTargetView* nullViews[] = { nullptr, nullptr, nullptr, nullptr };
	rd->GetDeviceContext().OMSetRenderTargets(4, nullViews, nullptr);
#endif

	int screenWidth = GetViewport().width;
	int screenHeight = GetViewport().height;
	int gridWidth = screenWidth;
	int gridHeight = screenHeight;

	if (m_graphicsPipeline.GetVrProjectionManager()->IsMultiResEnabledStatic())
		m_graphicsPipeline.GetVrProjectionManager()->GetProjectionSize(screenWidth, screenHeight, gridWidth, gridHeight);

	bool bSeparateCullingPass = tiledLights->IsSeparateVolumeListGen();

	if (CRenderer::CV_r_DeferredShadingTiled == eDeferredMode_Disabled || CRenderer::CV_r_GraphicsPipelineMobile)
		return;

	uint64 rtFlags = 0;
	if (CRenderer::CV_r_DeferredShadingTiled > 1)   // Tiled deferred
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE0];
	if (CRenderer::CV_r_DeferredShadingTiledDebug == 2)  // Light coverage visualization
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE2];
	if (bSeparateCullingPass)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE1];
	if (CRenderer::CV_r_SSReflections)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE3];
	if (CRenderer::CV_r_DeferredShadingSSS)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE4];

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive())
	{
		bool bSpecularTargetIsReady = false;
		int nModeGI = CSvoRenderer::GetInstance()->GetIntegratioMode(bSpecularTargetIsReady);

		if (nModeGI == 0 && CSvoRenderer::GetInstance()->GetUseLightProbes())
		{
			// AO modulates diffuse and specular
			rtFlags |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		}
		else
		{
			// GI replaces diffuse
			rtFlags |= g_HWSR_MaskBit[HWSR_DECAL_TEXGEN_2D];

			// GI replaces specular
			if (bSpecularTargetIsReady)
				rtFlags |= g_HWSR_MaskBit[HWSR_CUBEMAP0];
		}
	}
#endif

	rtFlags |= m_graphicsPipeline.GetVrProjectionManager()->GetRTFlags();

	CTexture* texClipVolumeIndex = m_graphicsPipelineResources.m_pTexClipVolumes;
	CTexture* pTexCaustics = tiledLights->IsCausticsVisible() ? m_graphicsPipelineResources.m_pTexSceneTargetR11G11B10F[1] : CRendererResources::s_ptexBlack;
	CTexture* pTexAOColorBleed = ssObscurance->IsColorBleeding() ? m_graphicsPipelineResources.m_pTexAOColorBleed : CRendererResources::s_ptexBlack;
	CTexture* pTexBentNormals = ssObscurance->IsObscuring() ? m_graphicsPipelineResources.m_pTexSceneNormalsBent : CRendererResources::s_ptexMedian;

	CTexture* pTexGiDiff = CRendererResources::s_ptexBlack;
	CTexture* pTexGiSpec = CRendererResources::s_ptexBlack;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive())
	{
		if (CSvoRenderer::GetInstance()->GetDiffuseFinRT())
			pTexGiDiff = (CTexture*)CSvoRenderer::GetInstance()->GetDiffuseFinRT();
		if (CSvoRenderer::GetInstance()->GetSpecularFinRT())
			pTexGiSpec = (CTexture*)CSvoRenderer::GetInstance()->GetSpecularFinRT();
	}
#endif

	if (m_passCullingShading.IsDirty(rtFlags, pTexCaustics->GetID(), pTexGiDiff->GetID(), pTexGiSpec->GetID(), pTexAOColorBleed->GetID()))
	{
		static CCryNameTSCRC techTiledShading("TiledDeferredShading");
		m_passCullingShading.SetTechnique(CShaderMan::s_shDeferredShading, techTiledShading, rtFlags);

		m_passCullingShading.SetOutputUAV(0, tiledLights->GetTiledTranspLightMaskBuffer());
		m_passCullingShading.SetOutputUAV(1, m_graphicsPipelineResources.m_pTexHDRTarget);
		m_passCullingShading.SetOutputUAV(2, m_graphicsPipelineResources.m_pTexSceneTargetR11G11B10F[0]);

		m_passCullingShading.SetSampler(15, EDefaultSamplerStates::TrilinearClamp);

		m_passCullingShading.SetTexture(0, m_graphicsPipelineResources.m_pTexLinearDepth);
		m_passCullingShading.SetTexture(1, m_graphicsPipelineResources.m_pTexSceneNormalsMap);
		m_passCullingShading.SetTexture(2, m_graphicsPipelineResources.m_pTexSceneSpecular);
		m_passCullingShading.SetTexture(3, m_graphicsPipelineResources.m_pTexSceneDiffuse);
		m_passCullingShading.SetTexture(4, m_graphicsPipelineResources.m_pTexShadowMask);
		m_passCullingShading.SetTexture(5, pTexBentNormals);
		m_passCullingShading.SetTexture(6, m_graphicsPipelineResources.m_pTexHDRTargetMaskedScaled[0][0]);
		m_passCullingShading.SetTexture(7, CRendererResources::s_ptexEnvironmentBRDF);
		m_passCullingShading.SetTexture(8, texClipVolumeIndex);
		m_passCullingShading.SetTexture(9, pTexAOColorBleed);
		m_passCullingShading.SetTexture(10, pTexGiDiff);
		m_passCullingShading.SetTexture(11, pTexGiSpec);
		m_passCullingShading.SetTexture(12, pTexCaustics);
		
		m_passCullingShading.SetBuffer(16, bSeparateCullingPass ? tiledLights->GetTiledOpaqueLightMaskBuffer() : tiledLights->GetLightCullInfoBuffer());
		m_passCullingShading.SetBuffer(17, tiledLights->GetLightShadeInfoBuffer());
		m_passCullingShading.SetBuffer(18, clipVolumes->GetClipVolumeInfoBuffer());
		m_passCullingShading.SetTexture(19, tiledLights->GetSpecularProbeAtlas());
		m_passCullingShading.SetTexture(20, tiledLights->GetDiffuseProbeAtlas());
		m_passCullingShading.SetTexture(21, tiledLights->GetProjectedLightAtlas());

		m_passCullingShading.SetTexture(48, CRendererResources::s_ptexLTC1);
		m_passCullingShading.SetTexture(49, CRendererResources::s_ptexLTC2);

		m_pTexGiDiff = pTexGiDiff;
		m_pTexGiSpec = pTexGiSpec;
	}

	SRenderViewport viewport(0, 0, screenWidth, screenHeight);
	SRenderViewInfo viewInfo[2];
	size_t viewInfoCount = m_graphicsPipeline.GenerateViewInfo(viewInfo);
	m_graphicsPipeline.GeneratePerViewConstantBuffer(viewInfo, viewInfoCount, m_pPerViewConstantBuffer, &viewport);
	m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer);

	m_passCullingShading.BeginConstantUpdate();
	{
		const auto& projMatrix = viewInfo[0].projMatrix;
		static CCryNameR projParamsName("ProjParams");
		Vec4 projParams(projMatrix.m00, projMatrix.m11, projMatrix.m20, projMatrix.m21);
		m_passCullingShading.SetConstant(projParamsName, projParams);

		static CCryNameR screenSizeName("ScreenSize");
		float fScreenWidth = (float)screenWidth;
		float fScreenHeight = (float)screenHeight;
		Vec4 screenSize(fScreenWidth, fScreenHeight, 1.0f / fScreenWidth, 1.0f / fScreenHeight);
		m_passCullingShading.SetConstant(screenSizeName, screenSize);

		static CCryNameR worldViewPosName("WorldViewPos");
		Vec4 worldViewPosParam(viewInfo[0].cameraOrigin, 0);
		m_passCullingShading.SetConstant(worldViewPosName, worldViewPosParam);

		SD3DPostEffectsUtils::UpdateFrustumCorners(m_graphicsPipeline);
		static CCryNameR frustumTLName("FrustumTL");
		Vec4 frustumTL(SD3DPostEffectsUtils::m_vLT.x, SD3DPostEffectsUtils::m_vLT.y, SD3DPostEffectsUtils::m_vLT.z, 0);
		m_passCullingShading.SetConstant(frustumTLName, frustumTL);
		static CCryNameR frustumTRName("FrustumTR");
		Vec4 frustumTR(SD3DPostEffectsUtils::m_vRT.x, SD3DPostEffectsUtils::m_vRT.y, SD3DPostEffectsUtils::m_vRT.z, 0);
		m_passCullingShading.SetConstant(frustumTRName, frustumTR);
		static CCryNameR frustumBLName("FrustumBL");
		Vec4 frustumBL(SD3DPostEffectsUtils::m_vLB.x, SD3DPostEffectsUtils::m_vLB.y, SD3DPostEffectsUtils::m_vLB.z, 0);
		m_passCullingShading.SetConstant(frustumBLName, frustumBL);

		static CCryNameR sunDirName("SunDir");
		static CCryNameR sunColorName("SunColor");

		m_passCullingShading.SetConstant(sunDirName, Vec4(m_pRenderView->GetSunLightDirection(), TiledShading_SunDistance));
		m_passCullingShading.SetConstant(sunColorName, m_pRenderView->GetSunLightColor());

		static CCryNameR ssdoParamsName("SSDOParams");
		Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance()->IsActive())
			ssdoParams *= CSvoRenderer::GetInstance()->GetSsaoAmount();
#endif
		Vec4 ssdoNullParams(0, 0, 0, 0);
		m_passCullingShading.SetConstant(ssdoParamsName, CRenderer::CV_r_ssdo ? ssdoParams : ssdoNullParams);
	}

	if (m_graphicsPipeline.GetVrProjectionManager()->IsMultiResEnabledStatic())
	{
		auto constantBuffer = m_graphicsPipeline.GetVrProjectionManager()->GetProjectionConstantBuffer(screenWidth, screenHeight);
		m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_VrProjection, constantBuffer);
	}

	m_passCullingShading.SetDispatchSize(
		tiledLights->GetDispatchSizeX(),
		tiledLights->GetDispatchSizeY(),
		1);

	{
		PROFILE_LABEL_SCOPE("SHADING");
		// Prepare buffers and textures which have been used in the z-pass and post-processes for use in the compute queue
		// Reduce resource state switching by requesting the most inclusive resource state
		m_passCullingShading.PrepareResourcesForUse(GetDeviceObjectFactory().GetCoreCommandList());

		{
			const bool bAsynchronousCompute = CRenderer::CV_r_D3D12AsynchronousCompute & BIT((eStage_TiledShading - eStage_FIRST_ASYNC_COMPUTE)) ? true : false;
			SScopedComputeCommandList computeCommandList(bAsynchronousCompute);

			m_passCullingShading.Execute(computeCommandList, EShaderStage_All);
		}
	}

	tiledLights->NotifyCausticsInvisible();  // Reset flag
}
