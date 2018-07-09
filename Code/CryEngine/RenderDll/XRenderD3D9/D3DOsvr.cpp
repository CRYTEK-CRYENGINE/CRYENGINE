// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

using namespace CryVR::Osvr;


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

bool CD3DOsvrRenderer::Initialize(int initialWidth, int initialHeight)
{
	D3DDevice* d3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();
	D3DDeviceContext* d3d11DeviceContext = m_pRenderer->GetDeviceContext_Unsynchronized().GetRealDeviceContext();

	m_eyeWidth  = initialWidth;
	m_eyeHeight = initialHeight;

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
	ReleaseTextureSwapSets();
	m_pOsvrDevice->ShutdownRenderer();
}

void CD3DOsvrRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_eyeWidth  != newWidth ||
	    m_eyeHeight != newHeight)
	{
		Shutdown();
		Initialize(newWidth, newHeight);
	}
}

void CD3DOsvrRenderer::PrepareFrame(uint64_t frameId){}

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

	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
	#endif

	m_currentFrame = (m_currentFrame + 1) % m_swapSetCount;
}

#endif
