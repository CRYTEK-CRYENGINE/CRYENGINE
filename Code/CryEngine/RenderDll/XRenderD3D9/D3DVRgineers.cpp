// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(INCLUDE_VR_RENDERING)

#include "D3DVRgineers.h"
#include "D3DPostProcess.h"
#include "DeviceInfo.h"

#include <Common/RenderDisplayContext.h>

#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#ifdef ENABLE_BENCHMARK_SENSOR
#include <IBenchmarkFramework.h>
#include <IBenchmarkRendererSensorManager.h>
#endif

CD3DVRgineersRenderer::CD3DVRgineersRenderer(CryVR::VRgineers::IVRgineersDevice* vrgDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer)
	: m_pVRgineersDevice(vrgDevice)
	, m_pRenderer(renderer)
	, m_pStereoRenderer(stereoRenderer)
	, m_eyeWidth(~0L)
	, m_eyeHeight(~0L)
{
}

CD3DVRgineersRenderer::~CD3DVRgineersRenderer()
{
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		m_scene3DRenderData[eye].texture = nullptr;
	}
}

// IHDMRenderer
bool CD3DVRgineersRenderer::Initialize(int initialWidth, int initialHeight)
{
	D3DDevice* d3d11Device = m_pRenderer->GetDevice();
	D3DDeviceContext* d3d11DeviceContext = m_pRenderer->GetDeviceContext();

	m_eyeWidth = initialWidth;
	m_eyeHeight = initialHeight;

	if (!m_pVRgineersDevice->InitRenderer(d3d11Device, d3d11DeviceContext))
	{
		return false;
	}

	ZeroArray(m_scene3DRenderData);

	CryVR::VRgineers::TextureDesc eyeTextureDesc;
	eyeTextureDesc.width = m_eyeWidth;
	eyeTextureDesc.height = m_eyeHeight;
	eyeTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM;

	if (!InitializeEyeTarget(d3d11Device, EEyeType::eEyeType_LeftEye, eyeTextureDesc, "$LeftEye", m_scene3DRenderData[EEyeType::eEyeType_LeftEye].texture)
		|| !InitializeEyeTarget(d3d11Device, EEyeType::eEyeType_RightEye, eyeTextureDesc, "$RightEye", m_scene3DRenderData[EEyeType::eEyeType_RightEye].texture))
	{
		gEnv->pLog->Log("[HMD][VRgineers] - Could not initalize eye textures.");
		return false;
	}

	for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
	{
		std::vector<_smart_ptr<CTexture>> swapChain = { m_scene3DRenderData[eye].texture };
		m_pStereoRenderer->CreateEyeDisplayContext(CCamera::EEye(eye), std::move(swapChain));
	}

	ID3D11Texture2D* leftTexture = m_scene3DRenderData[EEyeType::eEyeType_LeftEye].texture->GetDevTexture()->Get2DTexture();
	ID3D11Texture2D* rightTexture = m_scene3DRenderData[EEyeType::eEyeType_RightEye].texture->GetDevTexture()->Get2DTexture();

	m_pVRgineersDevice->OnSetupEyeTargets(leftTexture, rightTexture);

	return true;
}

void CD3DVRgineersRenderer::Shutdown()
{
	for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
	{
		m_scene3DRenderData[eye].texture = nullptr;
	}

	ReleaseBuffers();

	m_pVRgineersDevice->Release();
}

void CD3DVRgineersRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_eyeWidth != newWidth || m_eyeHeight != newHeight)
	{
		Shutdown();
		Initialize(newWidth, newHeight);
	}
}

void CD3DVRgineersRenderer::ReleaseBuffers()
{
}

void CD3DVRgineersRenderer::PrepareFrame(uint64_t frameId)
{
	m_pVRgineersDevice->OnPrepare();
}

void CD3DVRgineersRenderer::SubmitFrame()
{
	if (gEnv->IsEditor() && !gEnv->IsEditorGameMode()) // show black
	{
		m_scene3DRenderData[EEyeType::eEyeType_LeftEye].texture->Clear();
		m_scene3DRenderData[EEyeType::eEyeType_RightEye].texture->Clear();
	}
	
	m_pVRgineersDevice->SubmitFrame(gcpRendD3D->GetRenderFrameID());
}

void CD3DVRgineersRenderer::OnPostPresent()
{
	m_pVRgineersDevice->OnPostPresent();
}

RenderLayer::CProperties* CD3DVRgineersRenderer::GetQuadLayerProperties(RenderLayer::EQuadLayers id)
{
	return nullptr;
}

RenderLayer::CProperties* CD3DVRgineersRenderer::GetSceneLayerProperties(RenderLayer::ESceneLayers id)
{
	return nullptr;
}

std::pair<CTexture*, Vec4> CD3DVRgineersRenderer::GetMirrorTexture(EEyeType eye) const
{
	return std::make_pair(m_scene3DRenderData[eye].texture, Vec4(0, 0, 1, 1));
}
// ~IHDMRenderer

bool CD3DVRgineersRenderer::InitializeEyeTarget(D3DDevice* d3dDevice, const EEyeType eye, const CryVR::VRgineers::TextureDesc& desc, const char* name, _smart_ptr<CTexture>& outTexture)
{
	D3D11_TEXTURE2D_DESC textureDesc;
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = (DXGI_FORMAT)desc.format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	ID3D11Texture2D* d3dTexture;
	HRESULT hr = d3dDevice->CreateTexture2D(&textureDesc, nullptr, &d3dTexture);
	if (FAILED(hr))
	{
		return false;
	}

	outTexture = m_pStereoRenderer->WrapD3DRenderTarget(static_cast<D3DTexture*>(d3dTexture), desc.width, desc.height, (DXGI_FORMAT)desc.format, name, true);
	return true;
}

#endif