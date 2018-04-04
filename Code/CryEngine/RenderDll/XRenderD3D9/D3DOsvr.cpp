// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#ifdef INCLUDE_VR_RENDERING

	#include "D3DOsvr.h"
	#include <DriverD3D.h>
	#include <CrySystem/VR/IHMDManager.h>
	#ifdef ENABLE_BENCHMARK_SENSOR
		#include <IBenchmarkFramework.h>
		#include <IBenchmarkRendererSensorManager.h>
	#endif
	#include "D3DPostProcess.h"

namespace CryVR
{
namespace Osvr
{
CD3DOsvrRenderer::CD3DOsvrRenderer(IOsvrDevice* pDevice, CD3D9Renderer* pRenderer, CD3DStereoRenderer* pStereoRenderer)
	: m_pOsvrDevice(pDevice)
	, m_pRenderer(pRenderer)
	, m_pStereoRenderer(pStereoRenderer)
	, m_eyeWidth(0)
	, m_eyeHeight(0)
	, m_swapSetCount(0)
	, m_currentFrame(0)
{

}
CD3DOsvrRenderer::~CD3DOsvrRenderer()
{

}

bool CD3DOsvrRenderer::Initialize()
{
	D3DDevice* d3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();
	D3DDeviceContext* d3d11DeviceContext = m_pRenderer->GetDeviceContext_Unsynchronized().GetRealDeviceContext();

	m_eyeWidth = m_pRenderer->GetWidth();
	m_eyeHeight = m_pRenderer->GetHeight();

	if (!m_pOsvrDevice->InitializeRenderer(d3d11Device, d3d11DeviceContext))
	{
		m_pOsvrDevice->ShutdownRenderer();
		return false;
	}

	CreateTextureSwapSets(m_eyeWidth, m_eyeHeight, 2);

	m_currentFrame = 0;

	return true;
}

void CD3DOsvrRenderer::CreateTextureSwapSets(uint32 width, uint32 height, uint32 swapSetCount)
{
	ReleaseTextureSwapSets();

	m_swapSetCount = swapSetCount;

	char textureName[16];
	const char* textureNameTemplate = "$OsvrEyeTex_%d_%d";

	for (uint32 i = 0; i < swapSetCount; ++i)
	{
		for (uint32 eye = 0; eye < EyeCount; ++eye)
		{
			sprintf_s(textureName, textureNameTemplate, eye, i);

			CTexture* tex = CTexture::GetOrCreateRenderTarget(textureName, width, height, Clr_Transparent, eTT_2D, FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_R8G8B8A8);
			m_scene3DRenderData[eye].textures.Add(tex);
		}

	}

	//register created textures to osvr device
	TextureSwapSet swapSets;
	TArray<TextureSet> texSets;
	TArray<Texture> textures;

	TextureSet* texSetsptr = texSets.Grow(swapSetCount);
	Texture* texptr = textures.Grow(swapSetCount * EyeCount);

	swapSets.numTextureSets = swapSetCount;
	swapSets.pTextureSets = texSetsptr;

	for (uint32 i = 0; i < swapSetCount; ++i)
	{
		for (uint32 eye = 0; eye < EyeCount; ++eye)
		{
			uint32 index = i * EyeCount + eye;
			Texture& tex = textures[index];
			CTexture* ctex = m_scene3DRenderData[eye].textures[i];
			tex.pRtView = ctex->GetSurface(0, 0);
			tex.pTexture = ctex->GetDevTexture()->Get2DTexture();

		}

		TextureSet& set = texSets[i];
		set.numTextures = EyeCount;
		set.pTextures = &textures[i * EyeCount];
	}

	m_pOsvrDevice->RegisterTextureSwapSet(&swapSets);

}
void CD3DOsvrRenderer::ReleaseTextureSwapSets()
{
	for (uint32 eye = 0; eye < EyeCount; ++eye)
	{
		for (uint32 j = 0; j < m_scene3DRenderData[eye].textures.Num(); ++j)
		{
			SAFE_RELEASE(m_scene3DRenderData[eye].textures[j]);
		}
		m_scene3DRenderData[eye].textures.SetUse(0);
	}

	m_swapSetCount = 0;
}

void CD3DOsvrRenderer::Shutdown()
{
	m_pStereoRenderer->SetEyeTextures(nullptr, nullptr);

	ReleaseTextureSwapSets();
	m_pOsvrDevice->ShutdownRenderer();
}

void CD3DOsvrRenderer::OnResolutionChanged()
{
	if (m_eyeWidth != m_pRenderer->GetWidth() ||
	    m_eyeHeight != m_pRenderer->GetHeight())
	{
		Shutdown();
		Initialize();
	}
}

void CD3DOsvrRenderer::ReleaseBuffers()
{

}

void CD3DOsvrRenderer::PrepareFrame()
{
	m_pStereoRenderer->SetEyeTextures(m_scene3DRenderData[0].textures[m_currentFrame], m_scene3DRenderData[1].textures[m_currentFrame]);
}

void CD3DOsvrRenderer::RestoreDeviceStateAfterFrameSubmit()
{

	//refresh viewport. Have to actually change the viewport so that it gets changed to the device itself
	int ox, oy, ow, oh;
	m_pRenderer->GetViewport(&ox, &oy, &ow, &oh);
	m_pRenderer->RT_SetViewport(ox, oy, ow, oh + 1);
	m_pRenderer->FX_SetViewport();
	m_pRenderer->RT_SetViewport(ox, oy, ow, oh);
	m_pRenderer->FX_SetViewport();

	m_pRenderer->FX_RestoreRenderTarget(0);

	m_pRenderer->FX_ResetVertexDeclaration();
}

void CD3DOsvrRenderer::SubmitFrame()
{
	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(m_scene3DRenderData[0].textures[m_currentFrame], m_scene3DRenderData[1].textures[m_currentFrame]);
	#endif

	// Scene3D layer
	if (!m_pOsvrDevice->PresentTextureSet(m_currentFrame))
	{
		CryLogAlways("[CD3DEOsvrRenderer] failed to present textureset %d!", m_currentFrame);
	}

	//OSVR RenderManager changes the DX device state and does not restore the state previously set. Thus, need to force Cryengine to reset it's state.
	RestoreDeviceStateAfterFrameSubmit();

	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
	#endif

	m_currentFrame = (m_currentFrame + 1) % m_swapSetCount;
}

void CD3DOsvrRenderer::RenderSocialScreen()
{
	if (const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager())
	{
		if (const IHmdDevice* pDev = pHmdManager->GetHmdDevice())
		{

			//with async timewarp, we are not allowed to touch the textures that have been submitted last, thus take the one before that
			int frame = (m_currentFrame - 1) < 0 ? m_swapSetCount - 1 : m_currentFrame - 1;

			CTexture* left = m_scene3DRenderData[0].textures[frame];
			CTexture* right = m_scene3DRenderData[1].textures[frame];

			const EHmdSocialScreen socialScreen = pDev->GetSocialScreenType();
			switch (socialScreen)
			{
			case EHmdSocialScreen::DistortedDualImage:
			case EHmdSocialScreen::UndistortedDualImage:
			{
				if (!CShaderMan::s_shPostEffects) return;

				//Assume that the current viewport is the viewport that the social screen is supposed to be rendered.
				int ox, oy, ow, oh;
				m_pRenderer->GetViewport(&ox, &oy, &ow, &oh);

				int halfW = ow >> 1;

				m_pRenderer->RT_SetViewport(0, 0, halfW, oh);
				PostProcessUtils().CopyTextureToScreen(left);

				m_pRenderer->RT_SetViewport(halfW, 0, halfW, oh);
				PostProcessUtils().CopyTextureToScreen(right);

				m_pRenderer->RT_SetViewport(ox, oy, ow, oh);

			}
			break;
			case EHmdSocialScreen::Off:
				if (CShaderMan::s_shPostEffects)
				{
					PostProcessUtils().ClearScreen(0.1f, 0.1f, 0.1f, 1.f);
				}

				break;

			case EHmdSocialScreen::UndistortedLeftEye:
			{
				PostProcessUtils().CopyTextureToScreen(left);
				break;
			}
			case EHmdSocialScreen::UndistortedRightEye:
			{
				PostProcessUtils().CopyTextureToScreen(right);
				break;
			}
			default:
				break;
			}
		}
	}

}
/*

   void CD3DOsvrRenderer::copyEyeTexturesToMirrorTexture(CTexture* left, CTexture* right)
   {
   if (!CShaderMan::s_shPostEffects || (!left && !right)) return;

   int ox, oy, ow, oh;
   m_renderer->GetViewport(&ox, &oy, &ow, &oh);

   m_renderer->FX_PushRenderTarget(0, m_mirrorTexture, NULL);
   m_renderer->FX_SetActiveRenderTargets();
   if (left)
   {
    m_renderer->RT_SetViewport(0, 0, m_eyeWidth, m_eyeHeight);
    PostProcessUtils().CopyTextureToScreen(left);
   }

   if (right)
   {
    m_renderer->RT_SetViewport(m_eyeWidth, 0, m_eyeWidth, m_eyeHeight);
    PostProcessUtils().CopyTextureToScreen(right);
   }


   m_renderer->RT_SetViewport(ox, oy, ow, oh);
   m_renderer->FX_PopRenderTarget(0);
   }*/

}
}
#endif
