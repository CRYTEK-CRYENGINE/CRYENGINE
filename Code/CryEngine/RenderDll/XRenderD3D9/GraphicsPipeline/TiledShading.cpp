// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TiledShading.h"

#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3D_SVO.h"

#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/OmniCamera.h"
#include "GraphicsPipeline/ClipVolumes.h"

CTiledShadingStage::CTiledShadingStage()
	: m_passCullingShading(CComputeRenderPass::eFlags_ReflectConstantBuffersFromShader)
{
}

CTiledShadingStage::~CTiledShadingStage()
{
}


void CTiledShadingStage::Init()
{
	CD3D9Renderer* pRenderer = gcpRendD3D;
	
	m_pPerViewConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerViewGlobalConstantBuffer));
}


void CTiledShadingStage::Execute()
{
	PROFILE_LABEL_SCOPE("TILED_SHADING");

	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	auto* clipVolumes = GetStdGraphicsPipeline().GetClipVolumesStage();
	auto* tiledLights = GetStdGraphicsPipeline().GetTiledLightVolumesStage();

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	// on dx11, some input resources might still be bound as render targets.
	// dx11 will then just ignore the bind call
	ID3D11RenderTargetView* nullViews[] = { nullptr, nullptr, nullptr, nullptr };
	rd->GetDeviceContext().OMSetRenderTargets(4, nullViews, nullptr);
#endif

	int screenWidth = GetViewport().width;
	int screenHeight = GetViewport().height;
	
	bool bSeparateCullingPass = tiledLights->IsSeparateVolumeListGen();
	
	if (CRenderer::CV_r_DeferredShadingTiled == 4 || CRenderer::CV_r_GraphicsPipelineMobile)
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
	if (CRenderer::CV_r_DeferredShadingAreaLights > 0)
		rtFlags |= g_HWSR_MaskBit[HWSR_SAMPLE5];

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

	rtFlags |= CVrProjectionManager::Instance()->GetRTFlags();

	CTexture* texClipVolumeIndex = CRendererResources::s_ptexVelocity;
	CTexture* pTexCaustics = tiledLights->IsCausticsVisible() ? CRendererResources::s_ptexSceneTargetR11G11B10F[1] : CRendererResources::s_ptexBlack;
	CTexture* pTexAOColorBleed = CRenderer::CV_r_ssdoColorBleeding ? CRendererResources::s_ptexAOColorBleed : CRendererResources::s_ptexBlack;

	CTexture* pTexGiDiff = CRendererResources::s_ptexBlack;
	CTexture* pTexGiSpec = CRendererResources::s_ptexBlack;

#if defined(FEATURE_SVO_GI)
	if (CSvoRenderer::GetInstance()->IsActive() && CSvoRenderer::GetInstance()->GetSpecularFinRT())
	{
		pTexGiDiff = (CTexture*)CSvoRenderer::GetInstance()->GetDiffuseFinRT();
		pTexGiSpec = (CTexture*)CSvoRenderer::GetInstance()->GetSpecularFinRT();
	}
#endif

	if (m_passCullingShading.IsDirty(rtFlags, pTexCaustics->GetID(), pTexGiDiff->GetID(), pTexGiSpec->GetID(), pTexAOColorBleed->GetID()))
	{
		static CCryNameTSCRC techTiledShading("TiledDeferredShading");
		m_passCullingShading.SetTechnique(CShaderMan::s_shDeferredShading, techTiledShading, rtFlags);

		m_passCullingShading.SetOutputUAV(0, tiledLights->GetTiledTranspLightMaskBuffer());
		m_passCullingShading.SetOutputUAV(1, CRendererResources::s_ptexHDRTarget);
		m_passCullingShading.SetOutputUAV(2, CRendererResources::s_ptexSceneTargetR11G11B10F[0]);

		m_passCullingShading.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);

		m_passCullingShading.SetTexture(0, CRendererResources::s_ptexLinearDepth);
		m_passCullingShading.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
		m_passCullingShading.SetTexture(2, CRendererResources::s_ptexSceneSpecular);
		m_passCullingShading.SetTexture(3, CRendererResources::s_ptexSceneDiffuse);
		m_passCullingShading.SetTexture(4, CRendererResources::s_ptexShadowMask);
		m_passCullingShading.SetTexture(5, CRendererResources::s_ptexSceneNormalsBent);
		m_passCullingShading.SetTexture(6, CRendererResources::s_ptexHDRTargetScaledTmp[0]);
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

		m_pTexGiDiff = pTexGiDiff;
		m_pTexGiSpec = pTexGiSpec;
	}

	SRenderViewport viewport( 0, 0, screenWidth, screenHeight );
	SRenderViewInfo viewInfo[2];
	size_t viewInfoCount = GetGraphicsPipeline().GenerateViewInfo(viewInfo);
	GetStdGraphicsPipeline().GeneratePerViewConstantBuffer(viewInfo, viewInfoCount, m_pPerViewConstantBuffer,&viewport);
	m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_PerView, m_pPerViewConstantBuffer);

	if (CVrProjectionManager::IsMultiResEnabledStatic())
	{
		auto constantBuffer = CVrProjectionManager::Instance()->GetProjectionConstantBuffer(screenWidth, screenHeight);
		m_passCullingShading.SetInlineConstantBuffer(eConstantBufferShaderSlot_VrProjection, constantBuffer);
	}
	
	m_passCullingShading.BeginConstantUpdate();
	{
		const auto &projMatrix = viewInfo[0].projMatrix;
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

		SD3DPostEffectsUtils::UpdateFrustumCorners();
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
		Vec4 sunDirParam(gEnv->p3DEngine->GetSunDirNormalized(), TiledShading_SunDistance);
		m_passCullingShading.SetConstant(sunDirName, sunDirParam);

		static CCryNameR sunColorName("SunColor");
		Vec3 sunColor;
		gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_COLOR, sunColor);
		sunColor *= rd->m_fAdaptedSceneScaleLBuffer;  // Apply LBuffers range rescale
		Vec4 sunColorParam(sunColor.x, sunColor.y, sunColor.z, gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_SUN_SPECULAR_MULTIPLIER));
		m_passCullingShading.SetConstant(sunColorName, sunColorParam);

		static CCryNameR ssdoParamsName("SSDOParams");
		Vec4 ssdoParams(CRenderer::CV_r_ssdoAmountDirect, CRenderer::CV_r_ssdoAmountAmbient, CRenderer::CV_r_ssdoAmountReflection, 0);
#if defined(FEATURE_SVO_GI)
		if (CSvoRenderer::GetInstance()->IsActive())
			ssdoParams *= CSvoRenderer::GetInstance()->GetSsaoAmount();
#endif
		Vec4 ssdoNullParams(0, 0, 0, 0);
		m_passCullingShading.SetConstant(ssdoParamsName, CRenderer::CV_r_ssdo ? ssdoParams : ssdoNullParams);
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