// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OmniCamera.h"

#include "SunShafts.h"
#include "D3DPostProcess.h"

void COmniCameraStage::Execute()
{
	PROFILE_LABEL_SCOPE("OMNICAMERA");

	CRY_ASSERT_MESSAGE(false, "I doubt this function actually works");

	CTexture* pTargetTexture = RenderView()->GetColorTarget();
	uint32 totalPixel = CRendererResources::s_renderArea;
	int cubeSize = 1;
	while (cubeSize * cubeSize * 6 < totalPixel)
		cubeSize *= 2;

	if (m_pOmniCameraTexture == nullptr || m_pOmniCameraTexture->GetWidth() != cubeSize)
	{
		const uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
		m_pOmniCameraTexture = CTexture::GetOrCreateTextureArray("$OmniCameraCube", cubeSize, cubeSize, 6, 1, eTT_CubeArray, nFlags, pTargetTexture->GetDstFormat());
	}

	if (m_pOmniCameraCubeFaceStagingTexture == nullptr || m_pOmniCameraCubeFaceStagingTexture->GetWidth() != cubeSize)
	{
		const uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
		m_pOmniCameraCubeFaceStagingTexture = CTexture::GetOrCreateRenderTarget("$OmniCameraCubeFaceStaging", cubeSize, cubeSize, ColorF(0, 0, 0), eTT_2D, nFlags, pTargetTexture->GetDstFormat());
	}

	if (m_pOmniCameraTexture && m_pOmniCameraCubeFaceStagingTexture)
	{
		m_downsamplePass.Execute(
			pTargetTexture,
			m_pOmniCameraCubeFaceStagingTexture,
			pTargetTexture->GetWidth(), pTargetTexture->GetHeight(),
			m_pOmniCameraCubeFaceStagingTexture->GetWidth(), m_pOmniCameraCubeFaceStagingTexture->GetHeight(),
			CDownsamplePass::EFilterType::FilterType_Box);

		int cubeFaceIdx = RenderView()->GetCamera(CCamera::eEye_Left).m_curCubeFace;

		const SResourceRegionMapping mapping =
		{
			{ 0, 0, 0, 0 }, // src position
			{ 0, 0, 0, D3D11CalcSubresource(0, cubeFaceIdx, m_pOmniCameraTexture->GetNumMips()) }, // dst position
			{
				static_cast<UINT>(m_pOmniCameraCubeFaceStagingTexture->GetWidth()),
				static_cast<UINT>(m_pOmniCameraCubeFaceStagingTexture->GetHeight()),
				static_cast<UINT>(m_pOmniCameraCubeFaceStagingTexture->GetDepth()),
				static_cast<UINT>(m_pOmniCameraTexture->GetNumMips())
			}, // size
			D3D11_COPY_NO_OVERWRITE_REVERT
		};

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(m_pOmniCameraCubeFaceStagingTexture->GetDevTexture(), m_pOmniCameraTexture->GetDevTexture(), mapping);

		if (cubeFaceIdx == 5)
		{
			static CCryNameTSCRC techName("CubeTextureToTexture");

			m_cubemapToScreenPass.SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
			m_cubemapToScreenPass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
			m_cubemapToScreenPass.SetRenderTarget(0, pTargetTexture);
			m_cubemapToScreenPass.SetTechnique(CShaderMan::s_shPostEffects, techName, 0);
			m_cubemapToScreenPass.SetState(GS_NODEPTHTEST);
			m_cubemapToScreenPass.SetTextureSamplerPair(0, m_pOmniCameraTexture, EDefaultSamplerStates::BilinearClamp);
			m_cubemapToScreenPass.Execute();
		}
	}
}
