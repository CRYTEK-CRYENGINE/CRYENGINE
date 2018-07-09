// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MobileComposition.h"
#include "GraphicsPipeline/ClipVolumes.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "DriverD3D.h"

#include <Common/RenderDisplayContext.h>

void CMobileCompositionStage::Init()
{
}

void CMobileCompositionStage::Execute()
{
	CRenderView* pRenderView = RenderView();
	const CRenderOutput* pOutput = pRenderView->GetRenderOutput();
	auto* tiledLights = GetStdGraphicsPipeline().GetTiledLightVolumesStage();

	CTexture* pBestProbe = CRendererResources::s_ptexDefaultProbeCM;
	CTexture* pFinalOutput = pOutput->GetColorTarget();
	CTexture* pZTexture = RenderView()->GetDepthTarget();
		
	// Find largest probe
	{
		auto& envProbes = gcpRendD3D->GetGraphicsPipeline().GetCurrentRenderView()->GetLightsArray(eDLT_DeferredCubemap);
		float bestRadius = 0;

		auto stp = envProbes.end();
		auto itr = envProbes.begin();
	
		while (itr != stp)
		{
			float currRadius = itr->m_fRadius;
			if (bestRadius < currRadius)
			{
				bestRadius = currRadius;
				pBestProbe = reinterpret_cast<CTexture*>(itr->m_pSpecularCubemap);
			}

			itr++;
		}
	}

	m_passDepthDownsample2.Execute(pZTexture, CRendererResources::s_ptexLinearDepthScaled[0], true , true);
	m_passDepthDownsample4.Execute(CRendererResources::s_ptexLinearDepthScaled[0], CRendererResources::s_ptexLinearDepthScaled[1], false, false);
	m_passDepthDownsample8.Execute(CRendererResources::s_ptexLinearDepthScaled[1], CRendererResources::s_ptexLinearDepthScaled[2], false, false);

	{
		PROFILE_LABEL_SCOPE("MOBILE_LIGHTING");
	
		static CCryNameTSCRC tech("LightingMobile");

		uint64 rtMask = (CRenderer::CV_r_GraphicsPipelineMobile == 1) ? g_HWSR_MaskBit[HWSR_SAMPLE0] : 0;

		m_passLighting.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passLighting.SetTechnique(CShaderMan::s_ShaderMobileComposition, tech, rtMask);
		m_passLighting.SetRequireWorldPos(true);
		m_passLighting.SetRenderTarget(0, CRendererResources::s_ptexHDRTarget);
		m_passLighting.SetState(GS_NODEPTHTEST);
		m_passLighting.SetRequirePerViewConstantBuffer(true);

		m_passLighting.SetSampler(0, EDefaultSamplerStates::TrilinearClamp);
		
		m_passLighting.SetTexture(0, pZTexture);
		m_passLighting.SetTexture(1, CRendererResources::s_ptexSceneNormalsMap);
		m_passLighting.SetTexture(2, CRendererResources::s_ptexSceneDiffuse);
		m_passLighting.SetTexture(3, CRendererResources::s_ptexSceneSpecular);
		m_passLighting.SetTextureSamplerPair(7, pBestProbe, EDefaultSamplerStates::TrilinearClamp);

		m_passLighting.SetBuffer(16, tiledLights->GetTiledOpaqueLightMaskBuffer());
		m_passLighting.SetBuffer(17, tiledLights->GetLightShadeInfoBuffer());
		m_passLighting.SetTexture(18, tiledLights->GetProjectedLightAtlas());
	
		m_passLighting.Execute();
	}

	{
		PROFILE_LABEL_SCOPE("MOBILE_TONEMAP");

		static CCryNameTSCRC tech("TonemappingMobile");

		m_passTonemappingTAA.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
		m_passTonemappingTAA.SetTechnique(CShaderMan::s_ShaderMobileComposition, tech, 0);
		m_passTonemappingTAA.SetRenderTarget(0, pFinalOutput);
		m_passTonemappingTAA.SetState(GS_NODEPTHTEST);

		m_passTonemappingTAA.SetTextureSamplerPair(0, CRendererResources::s_ptexHDRTarget, EDefaultSamplerStates::PointClamp);
		m_passTonemappingTAA.SetTextureSamplerPair(1, CRendererResources::s_ptexVignettingMap, EDefaultSamplerStates::LinearClamp);
	
		m_passTonemappingTAA.Execute();
	}
}
