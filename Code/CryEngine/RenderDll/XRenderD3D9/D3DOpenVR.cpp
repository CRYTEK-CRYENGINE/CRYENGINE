// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#if defined(INCLUDE_VR_RENDERING)

	#include "D3DOpenVR.h"
	#include "DriverD3D.h"
	#include "D3DPostProcess.h"
	#include "DeviceInfo.h"

	#include <CrySystem/VR/IHMDManager.h>
	#include <CrySystem/VR/IHMDDevice.h>
	#ifdef ENABLE_BENCHMARK_SENSOR
		#include <IBenchmarkFramework.h>
		#include <IBenchmarkRendererSensorManager.h>
	#endif

CD3DOpenVRRenderer::CD3DOpenVRRenderer(CryVR::OpenVR::IOpenVRDevice* openVRDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer)
	: m_pOpenVRDevice(openVRDevice)
	, m_pRenderer(renderer)
	, m_pStereoRenderer(stereoRenderer)
	, m_eyeWidth(~0L)
	, m_eyeHeight(~0L)
	, m_numFrames(0)
	, m_currentFrame(0)
{
	ZeroArray(m_scene3DRenderData);
	ZeroArray(m_quadLayerRenderData);
	ZeroArray(m_mirrorTextures);

	for (uint32 i = RenderLayer::eQuadLayers_0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_quadLayerProperties[i].SetType(RenderLayer::eLayer_Quad);
		m_quadLayerProperties[i].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -0.6f), 1.f));
		m_quadLayerProperties[i].SetId(i);
	}

	m_Param0Name = CCryNameR("texToTexParams0");
	m_Param1Name = CCryNameR("texToTexParams1");
	m_textureToTexture = CCryNameTSCRC("TextureToTexture");
}

CD3DOpenVRRenderer::~CD3DOpenVRRenderer()
{
}

bool CD3DOpenVRRenderer::Initialize()
{
	D3DDevice* d3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();

	m_eyeWidth = m_pRenderer->GetWidth();
	m_eyeHeight = m_pRenderer->GetHeight();

	CryVR::OpenVR::TextureDesc eyeTextureDesc;
	eyeTextureDesc.width = m_eyeWidth;
	eyeTextureDesc.height = m_eyeHeight;
	eyeTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM;

	CryVR::OpenVR::TextureDesc quadTextureDesc;
	quadTextureDesc.width = m_eyeWidth;
	quadTextureDesc.height = m_eyeHeight;
	quadTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM;

	CryVR::OpenVR::TextureDesc mirrorTextureDesc;
	mirrorTextureDesc.width = m_eyeWidth;
	mirrorTextureDesc.height = m_eyeHeight;
	mirrorTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM;

	if (!InitializeEyeTarget(d3d11Device, EEyeType::eEyeType_LeftEye, eyeTextureDesc, "$LeftEye") ||
	    !InitializeEyeTarget(d3d11Device, EEyeType::eEyeType_RightEye, eyeTextureDesc, "$RightEye") ||
	    !InitializeQuadLayer(d3d11Device, RenderLayer::eQuadLayers_0, quadTextureDesc, "$QuadLayer0") ||
	    !InitializeQuadLayer(d3d11Device, RenderLayer::eQuadLayers_1, quadTextureDesc, "$QuadLayer1") ||
	    !InitializeMirrorTexture(d3d11Device, EEyeType::eEyeType_LeftEye, mirrorTextureDesc, "$LeftMirror") ||
	    !InitializeMirrorTexture(d3d11Device, EEyeType::eEyeType_RightEye, mirrorTextureDesc, "$RightMirror"))
	{
		CryLogAlways("[HMD][OpenVR] Texture creation failed");
		Shutdown();
		return false;
	}
	
	// Scene3D layers
	m_pOpenVRDevice->OnSetupEyeTargets(
		CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX,
		CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Auto,
		m_scene3DRenderData[EEyeType::eEyeType_LeftEye].texture->GetDevTexture()->Get2DTexture(),
		m_scene3DRenderData[EEyeType::eEyeType_RightEye].texture->GetDevTexture()->Get2DTexture()
	);

	// Quad layers
	for (int i = 0; i < RenderLayer::eQuadLayers_Total; i++)
	{
		m_pOpenVRDevice->OnSetupOverlay(i,
	  CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX,
	  CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Auto,
			m_quadLayerRenderData[i].texture->GetDevTexture()->Get2DTexture()
	  );
	}

	// Mirror texture
	void* srv = nullptr;
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		// Request the resource-view from openVR
		m_pOpenVRDevice->GetMirrorImageView(static_cast<EEyeType>(eye), m_mirrorTextures[eye]->GetDevTexture()->Get2DTexture(), &srv);
		m_mirrorTextures[eye]->SetDefaultShaderResourceView(static_cast<D3DShaderResource*>(srv), false);
	}

	return true;
}

bool CD3DOpenVRRenderer::InitializeEyeTarget(D3DDevice* d3d11Device, EEyeType eye, CryVR::OpenVR::TextureDesc desc, const char* name)
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
	ID3D11Texture2D* texture;
	d3d11Device->CreateTexture2D(&textureDesc, nullptr, &texture);

	m_scene3DRenderData[eye].texture = m_pStereoRenderer->WrapD3DRenderTarget(static_cast<D3DTexture*>(texture), desc.width, desc.height, (DXGI_FORMAT)desc.format, name, true);

	return true;
}

bool CD3DOpenVRRenderer::InitializeQuadLayer(D3DDevice* d3d11Device, RenderLayer::EQuadLayers quadLayer, CryVR::OpenVR::TextureDesc desc, const char* name)
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
	textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
	ID3D11Texture2D* texture;
	d3d11Device->CreateTexture2D(&textureDesc, nullptr, &texture);

	char textureName[16];
	cry_sprintf(textureName, name, quadLayer);

	m_quadLayerRenderData[quadLayer].texture = m_pStereoRenderer->WrapD3DRenderTarget(static_cast<D3DTexture*>(texture), desc.width, desc.height, (DXGI_FORMAT)desc.format, textureName, true);

	return true;
}

bool CD3DOpenVRRenderer::InitializeMirrorTexture(D3DDevice* d3d11Device, EEyeType eye, CryVR::OpenVR::TextureDesc desc, const char* name)
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

	ID3D11Texture2D* texture;
	d3d11Device->CreateTexture2D(&textureDesc, nullptr, &texture);

	m_mirrorTextures[eye] = m_pStereoRenderer->WrapD3DRenderTarget(static_cast<D3DTexture*>(texture), desc.width, desc.height, (DXGI_FORMAT)desc.format, name, false);
	
	return true;
}

void CD3DOpenVRRenderer::Shutdown()
{
	m_pStereoRenderer->SetEyeTextures(nullptr, nullptr);

	// Scene3D layers
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		m_scene3DRenderData[eye].texture->ReleaseForce();
		m_scene3DRenderData[eye].texture = nullptr;
	}

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_pOpenVRDevice->OnDeleteOverlay(i);
		m_quadLayerRenderData[i].texture->ReleaseForce();
		m_quadLayerRenderData[i].texture = nullptr;
	}

	// Mirror texture
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		if (m_mirrorTextures[eye] != nullptr)
		{
			m_mirrorTextures[eye]->ReleaseForce();
			m_mirrorTextures[eye] = nullptr;
		}
	}
	
	ReleaseBuffers();
}

void CD3DOpenVRRenderer::OnResolutionChanged()
{
	if (m_eyeWidth != m_pRenderer->GetWidth() ||
	    m_eyeHeight != m_pRenderer->GetHeight())
	{
		Shutdown();
		Initialize();
	}
}

void CD3DOpenVRRenderer::ReleaseBuffers()
{
}

void CD3DOpenVRRenderer::PrepareFrame()
{
	// Scene3D layer
	m_pStereoRenderer->SetEyeTextures(m_scene3DRenderData[0].texture, m_scene3DRenderData[1].texture);

	// Quad layers
	for (int i = 0; i < RenderLayer::eQuadLayers_Total; i++)
		m_pStereoRenderer->SetVrQuadLayerTexture(static_cast<RenderLayer::EQuadLayers>(i), m_quadLayerRenderData[i].texture);
}

void CD3DOpenVRRenderer::SubmitFrame()
{
	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(m_scene3DRenderData[0].texture, m_scene3DRenderData[1].texture);
	#endif

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		if (GetQuadLayerProperties(static_cast<RenderLayer::EQuadLayers>(i))->IsActive())
		{
			m_pOpenVRDevice->SubmitOverlay(i);
		}
	}

	// Pass the final images and layer configuration to the OpenVR device
	m_pOpenVRDevice->SubmitFrame();

	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
	#endif

	// Deactivate layers
	GetQuadLayerProperties(RenderLayer::eQuadLayers_0)->SetActive(false);
	GetQuadLayerProperties(RenderLayer::eQuadLayers_1)->SetActive(false);
}

void CD3DOpenVRRenderer::RenderSocialScreen()
{
	CTexture* pBackbufferTexture = gcpRendD3D->GetCurrentBackBuffer(gcpRendD3D->GetActiveDisplayContext());
	
	if (const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager())
	{
		if (const IHmdDevice* pDev = pHmdManager->GetHmdDevice())
		{
			bool bKeepAspect = false;
			const EHmdSocialScreen socialScreen = pDev->GetSocialScreenType(&bKeepAspect);
			switch (socialScreen)
			{
			case EHmdSocialScreen::Off:
				// TODO: Clear backbuffer texture? Show a selected wallpaper?
					GetUtils().ClearScreen(0.1f, 0.1f, 0.1f, 1.0f); // we don't want true black, to distinguish between rendering error and no-social-screen. NOTE: THE CONSOLE WILL NOT BE DISPLAYED!!!
				break;
			// intentional fall through
			case EHmdSocialScreen::UndistortedLeftEye:
			case EHmdSocialScreen::UndistortedRightEye:
				{
					CTexture* pTex = socialScreen == EHmdSocialScreen::UndistortedLeftEye ? m_mirrorTextures[LEFT_EYE] : m_mirrorTextures[RIGHT_EYE];
					if (CShaderMan::s_shPostEffects)
						GetUtils().StretchRect(pTex, pBackbufferTexture);
				}
				break;

			case EHmdSocialScreen::UndistortedDualImage: // intentional fall through - default to undistorted dual image
			case EHmdSocialScreen::DistortedDualImage:   // intentional fall through - OpenVR does not return distorted eye targets, therefore the only display the undistorted eye targets
			default:
				if (CShaderMan::s_shPostEffects)
				{
					// Get eye textures
					uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
					gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_REVERSE_DEPTH]);

					// Store current viewport
					int iTempX, iTempY, iWidth, iHeight;
					gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

					if (bKeepAspect)
					{
						gcpRendD3D->FX_ClearTarget(pBackbufferTexture, Clr_Empty);
					}

					// Set backbuffer texture as renter target
					gcpRendD3D->FX_PushRenderTarget(0, pBackbufferTexture, nullptr);
					gcpRendD3D->FX_SetActiveRenderTargets();

					for (uint32 eye = 0; eye < 2; ++eye)
					{
						if (CTexture* pSrcTex = m_mirrorTextures[eye])
						{
					// Set rendering and shader flags
							if (eye == LEFT_EYE)
					gcpRendD3D->RT_SetViewport(0, 0, pBackbufferTexture->GetWidth() >> 1, pBackbufferTexture->GetHeight()); // Set viewport (left half of backbuffer)
							else if (eye == RIGHT_EYE)
								gcpRendD3D->RT_SetViewport(pBackbufferTexture->GetWidth() >> 1, 0, pBackbufferTexture->GetWidth() >> 1, pBackbufferTexture->GetHeight()); // set viewport (right half of backbuffer)

							GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_textureToTexture, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

							gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
							pSrcTex->Apply(0, EDefaultSamplerStates::LinearClamp); // Bind left-eye texture for rendering

							GetUtils().DrawFullScreenTri(0, 0);
							GetUtils().ShEndPass();
						}
					}

					// Restore previous viewport
					gcpRendD3D->FX_PopRenderTarget(0);
					gcpRendD3D->SetViewport(iTempX, iTempY, iWidth, iHeight);

					gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
				}
				break;
			}
		}
	}
}

RenderLayer::CProperties* CD3DOpenVRRenderer::GetQuadLayerProperties(RenderLayer::EQuadLayers id)
{
	if (id < RenderLayer::eQuadLayers_Total)
	{
		return &(m_quadLayerProperties[id]);
	}
	return nullptr;
}

#endif //defined(INCLUDE_VR_RENDERING)
