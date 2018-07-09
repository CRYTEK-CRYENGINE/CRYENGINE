// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OmniCamera.h"

#include "SunShafts.h"
#include "ColorGrading.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

void COmniCameraStage::Execute()
{
	PROFILE_LABEL_SCOPE("OMNICAMERA");

	if (IsEnabled())
	{
		CRY_ASSERT(0 && "I doubt this function actually works");

		uint32 totalPixel = CRendererResources::s_renderArea;
		int cubeSize = 1;
		while (cubeSize * cubeSize * 6 < totalPixel)
			cubeSize *= 2;
		
		if (m_pOmniCameraTexture == nullptr || m_pOmniCameraTexture->GetWidth() != cubeSize)
		{
			const uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
			m_pOmniCameraTexture = CTexture::GetOrCreateTextureArray("$OmniCameraCube", cubeSize, cubeSize, 6, 1, eTT_CubeArray, nFlags, eTF_R8G8B8A8);
		}

		if (m_pOmniCameraCubeFaceStagingTexture == nullptr || m_pOmniCameraCubeFaceStagingTexture->GetWidth() != cubeSize)
		{
			const uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
			m_pOmniCameraCubeFaceStagingTexture = CTexture::GetOrCreateRenderTarget("$OmniCameraCubeFaceStaging", cubeSize, cubeSize, ColorF(0, 0, 0), eTT_2D, nFlags, eTF_R8G8B8A8);
		}

		if (m_pOmniCameraTexture && m_pOmniCameraCubeFaceStagingTexture)
		{
			CTexture* pTargetTexture = RenderView()->GetColorTarget();
			CTexture* pScreenTexture = CRendererResources::s_ptexBackBuffer;

			RECT defaultRect;
			defaultRect.left = 0;
			defaultRect.top  = 0;
			defaultRect.right  = min(pScreenTexture->GetWidth (), pTargetTexture->GetWidth ());
			defaultRect.bottom = min(pScreenTexture->GetHeight(), pTargetTexture->GetHeight());

			const SResourceRegionMapping region =
			{
				{ 0, 0, 0, 0 },
				{ defaultRect.left, defaultRect.top, 0, 0 },
				{ defaultRect.right - defaultRect.left, defaultRect.bottom - defaultRect.top, 1, 1 }
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pTargetTexture->GetDevTexture(), pScreenTexture->GetDevTexture(), region);

			m_downsamplePass.Execute(
				pScreenTexture,
				m_pOmniCameraCubeFaceStagingTexture,
				pScreenTexture->GetWidth(), pScreenTexture->GetHeight(),
				m_pOmniCameraCubeFaceStagingTexture->GetWidth(), m_pOmniCameraCubeFaceStagingTexture->GetHeight(),
				CDownsamplePass::EFilterType::FilterType_Box);

			int cubeFaceIdx = RenderView()->GetCamera(CCamera::eEye_Left).m_curCubeFace;

			const SResourceRegionMapping mapping =
			{
				{ 0, 0, 0, 0 }, // src position
				{ 0, 0, 0, D3D11CalcSubresource(0, cubeFaceIdx, m_pOmniCameraTexture->GetNumMips()) }, // dst position
				{ m_pOmniCameraCubeFaceStagingTexture->GetWidth(), m_pOmniCameraCubeFaceStagingTexture->GetHeight(), m_pOmniCameraCubeFaceStagingTexture->GetDepth(), m_pOmniCameraTexture->GetNumMips() }, // size
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
}

bool COmniCameraStage::IsEnabled() const
{
	return RenderView()->GetCamera(CCamera::eEye_Left).m_bOmniCamera;
}