// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "MobileComposition.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "DriverD3D.h"

void CMobileCompositionStage::Init()
{
}

void CMobileCompositionStage::Execute(CRenderView* pCurrentRenderView)
{
	// Pop HDR target from RT stack
	{
		assert(gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex == CTexture::s_ptexHDRTarget);
		gcpRendD3D->FX_SetActiveRenderTargets();
		gcpRendD3D->RT_UnbindTMUs();
		gcpRendD3D->FX_PopRenderTarget(0);
	}
	
	SRenderLight* pBestProbe = nullptr;
		
	// Find largest probe
	{
		auto& envProbes = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetLightsArray(eDLT_DeferredCubemap);
		float bestRadius = 0;
	
		for (uint32 i = 0; i < envProbes.size(); i++)
		{
			if (envProbes[i].m_fRadius > bestRadius)
			{
				bestRadius = envProbes[i].m_fRadius;
				pBestProbe = &envProbes[i];
			}
		}
	}

	m_passDepthDownsample2.Execute(gcpRendD3D->m_DepthBufferOrig.pTexture, CTexture::s_ptexZTargetScaled, true, true);
	m_passDepthDownsample4.Execute(CTexture::s_ptexZTargetScaled, CTexture::s_ptexZTargetScaled2, false, false);
	m_passDepthDownsample8.Execute(CTexture::s_ptexZTargetScaled2, CTexture::s_ptexZTargetScaled3, false, false);

	uint32 numVolumes;
	const Vec4* pVolumeParams;
	gcpRendD3D->GetGraphicsPipeline().GetClipVolumesStage()->GetClipVolumeShaderParams(pVolumeParams, numVolumes);
	gcpRendD3D->GetTiledShading().Render(pCurrentRenderView, (Vec4*)pVolumeParams);
	
	{
		PROFILE_LABEL_SCOPE("MOBILE_LIGHTING");
	
		static CCryNameTSCRC tech("LightingMobile");
		uint64 rtMask = (CRenderer::CV_r_GraphicsPipelineMobile == 1) ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;
		m_passLighting.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passLighting.SetTechnique(CShaderMan::s_ShaderMobileComposition, tech, rtMask);
		m_passLighting.SetRequireWorldPos(true);
		m_passLighting.SetRenderTarget(0, CTexture::s_ptexHDRTarget);
		m_passLighting.SetState(GS_NODEPTHTEST);
		m_passLighting.SetRequirePerViewConstantBuffer(true);

		m_passLighting.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		
		m_passLighting.SetTexture(0, gcpRendD3D->m_DepthBufferOrig.pTexture);
		m_passLighting.SetTexture(1, CTexture::s_ptexSceneNormalsMap);
		m_passLighting.SetTexture(2, CTexture::s_ptexSceneDiffuse);
		m_passLighting.SetTexture(3, CTexture::s_ptexSceneSpecular);
		m_passLighting.SetTextureSamplerPair(7, pBestProbe ? (CTexture*)pBestProbe->m_pSpecularCubemap : CTexture::s_ptexDefaultProbeCM, EDefaultSamplerStates::TrilinearClamp);

		CTiledShading& tiledShading = gcpRendD3D->GetTiledShading();
		m_passLighting.SetBuffer(16, &tiledShading.m_tileOpaqueLightMaskBuf);
		m_passLighting.SetBuffer(17, &tiledShading.m_lightShadeInfoBuf);
		m_passLighting.SetTexture(18, tiledShading.m_spotTexAtlas.texArray);
	
		m_passLighting.Execute();
	}

	{
		PROFILE_LABEL_SCOPE("MOBILE_TONEMAP");

		static CCryNameTSCRC tech("TonemappingMobile");
		m_passTonemappingTAA.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passTonemappingTAA.SetTechnique(CShaderMan::s_ShaderMobileComposition, tech, 0);
		m_passTonemappingTAA.SetRenderTarget(0, gcpRendD3D->GetCurrentTargetOutput());
		m_passTonemappingTAA.SetState(GS_NODEPTHTEST);

		m_passTonemappingTAA.SetTextureSamplerPair(0, CTexture::s_ptexHDRTarget, EDefaultSamplerStates::PointClamp);
		m_passTonemappingTAA.SetTextureSamplerPair(1, CTexture::s_ptexVignettingMap, EDefaultSamplerStates::LinearClamp);
	
		m_passTonemappingTAA.Execute();
	}
}
