// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"

#if defined(INCLUDE_VR_RENDERING)

	#include "D3DOpenVR.h"
	#include "DriverD3D.h"
	#include "D3DPostProcess.h"
	#include "DeviceInfo.h"

	#include <Common/RenderDisplayContext.h>

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

bool CD3DOpenVRRenderer::Initialize(int initialWidth, int initialeight)
{
	D3DDevice* d3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();

	m_eyeWidth  = initialWidth;
	m_eyeHeight = initialeight;

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

void CD3DOpenVRRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_eyeWidth  != newWidth ||
	    m_eyeHeight != newHeight)
	{
		Shutdown();
		Initialize(newWidth, newHeight);
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

	m_pOpenVRDevice->OnPrepare();
}

void CD3DOpenVRRenderer::SubmitFrame()
{
	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(m_scene3DRenderData[0].texture, m_scene3DRenderData[1].texture);
	#endif

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		const RenderLayer::CProperties* pQuadProperties = GetQuadLayerProperties(static_cast<RenderLayer::EQuadLayers>(i));

		if (pQuadProperties->IsActive())
		{
			m_pOpenVRDevice->SubmitOverlay(i, pQuadProperties);
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

void CD3DOpenVRRenderer::OnPostPresent()
{
	m_pOpenVRDevice->OnPostPresent();
}

struct CD3DOpenVRRenderer::SSocialScreenRenderAutoRestore
{
	SSocialScreenRenderAutoRestore(CTexture* pRenderTarget)
	{
		ASSERT_LEGACY_PIPELINE

		#if 0
		shaderFlags = gRenDev->m_RP.m_FlagsShader_RT;
		gRenDev->GetViewport(&x, &y, &w, &h);

		gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE0];
		gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE2];
		gRenDev->m_RP.m_FlagsShader_RT &= ~g_HWSR_MaskBit[HWSR_SAMPLE5];

		gcpRendD3D->FX_PushRenderTarget(0, pRenderTarget, nullptr);
		gcpRendD3D->FX_SetActiveRenderTargets();

		gcpRendD3D->FX_SetState(GS_NODEPTHTEST);
		#endif
	}
	~SSocialScreenRenderAutoRestore()
	{
		ASSERT_LEGACY_PIPELINE

		#if 0
		gRenDev->m_RP.m_FlagsShader_RT = shaderFlags;

		gcpRendD3D->FX_PopRenderTarget(0);
		gcpRendD3D->SetViewport(x, y, w, h);
		#endif
	}

	int x;
	int y;
	int w;
	int h;
	uint64 shaderFlags;
};

void CD3DOpenVRRenderer::RenderQuadLayers()
{
	// Since Quad layers do not get rendered (yet) with orientation in the social screen, we will render only the first quad layer which is the one that contains the Flash textures
	const uint32 numQuadLayersToRender = 2;

	for (uint32 layerIdx = 0; layerIdx < numQuadLayersToRender; ++layerIdx)
	{
		if (m_pOpenVRDevice->IsActiveOverlay(layerIdx))
		{
			if (CTexture* pQuadTex = m_quadLayerRenderData[layerIdx].texture)
			{
				ASSERT_LEGACY_PIPELINE

				#if 0
				GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, m_textureToTexture, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

				gRenDev->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
				pQuadTex->Apply(0, EDefaultSamplerStates::LinearClamp);

				GetUtils().DrawFullScreenTri(0, 0);
				GetUtils().ShEndPass();
				#endif
			}
		}
	}
}

void CD3DOpenVRRenderer::RenderSocialScreen()
{
	CTexture* pBackbufferTexture = gcpRendD3D->GetActiveDisplayContext()->GetCurrentBackBuffer();
	
	if (const IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager())
	{
		if (const IHmdDevice* pDev = pHmdManager->GetHmdDevice())
		{
			const int backBufferWidth = pBackbufferTexture->GetWidth();
			const int backBufferHeight = pBackbufferTexture->GetHeight();

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
				ASSERT_LEGACY_PIPELINE

				#if 0
				if (CShaderMan::s_shPostEffects)
				{
					CTexture* pTex = m_pStereoRenderer->GetEyeTarget(socialScreen == EHmdSocialScreen::UndistortedLeftEye ? LEFT_EYE : RIGHT_EYE);
					if (bKeepAspect)
					{
						const SSocialScreenRenderAutoRestore renderStateAutoRestore(pBackbufferTexture);

						gcpRendD3D->RT_SetViewport(backBufferWidth >> 2, 0, backBufferWidth >> 1, backBufferHeight);
						gcpRendD3D->FX_ClearTarget(pBackbufferTexture, Clr_Empty);

						uint nPasses;
						CShaderMan::s_shPostEffects->FXSetTechnique(m_textureToTexture); // TextureToTexture technique
						CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
						CShaderMan::s_shPostEffects->FXBeginPass(0);
						pTex->Apply(0, EDefaultSamplerStates::LinearClamp); // Bind left-eye texture for rendering
						GetUtils().DrawFullScreenTri(pTex->GetWidth(), pTex->GetHeight(), 0);
						CShaderMan::s_shPostEffects->FXEndPass();
						CShaderMan::s_shPostEffects->FXEnd();

						RenderQuadLayers();
					}
					else
					{
						GetUtils().StretchRect(pTex, pBackbufferTexture);
					}
				}
				#endif

				}
				break;

			case EHmdSocialScreen::UndistortedDualImage: // intentional fall through - default to undistorted dual image
			case EHmdSocialScreen::DistortedDualImage:   // intentional fall through - OpenVR does not return distorted eye targets, therefore the only display the undistorted eye targets
			default:
				if (CShaderMan::s_shPostEffects)
				{
					ASSERT_LEGACY_PIPELINE

				#if 0
					const bool bUseMirrorTexture = socialScreen == EHmdSocialScreen::DistortedDualImage;

					// Get eye textures
					CTexture* pTexLeft = bUseMirrorTexture ? m_mirrorTextures[LEFT_EYE] : m_pStereoRenderer->GetEyeTarget(LEFT_EYE);
					CTexture* pTexRight = bUseMirrorTexture ? m_mirrorTextures[RIGHT_EYE] : m_pStereoRenderer->GetEyeTarget(RIGHT_EYE);

					// Store previous viewport and set new render target. Use RAII to restore previous state.
					const SSocialScreenRenderAutoRestore renderStateAutoRestore(pBackbufferTexture);

					// Left-Eye Pass
					gcpRendD3D->RT_SetViewport(0, 0, backBufferWidth >> 1, backBufferHeight); // Set viewport (left half of backbuffer)

					uint nPasses;
					CShaderMan::s_shPostEffects->FXSetTechnique(m_textureToTexture); // TextureToTexture technique
					CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
					CShaderMan::s_shPostEffects->FXBeginPass(0);
					pTexLeft->Apply(0, EDefaultSamplerStates::LinearClamp); // Bind left-eye texture for rendering
					GetUtils().DrawFullScreenTri(pTexLeft->GetWidth(), pTexLeft->GetHeight(), 0);
					CShaderMan::s_shPostEffects->FXEndPass();
					CShaderMan::s_shPostEffects->FXEnd();

					if (!bUseMirrorTexture)
					{
						RenderQuadLayers();
					}

					// Right-Eye Pass
					gcpRendD3D->RT_SetViewport(backBufferWidth >> 1, 0, backBufferWidth >> 1, backBufferHeight); // set viewport (right half of backbuffer)

					CShaderMan::s_shPostEffects->FXSetTechnique(m_textureToTexture); // TextureToTexture technique
					CShaderMan::s_shPostEffects->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
					CShaderMan::s_shPostEffects->FXBeginPass(0);
					pTexRight->Apply(0, EDefaultSamplerStates::LinearClamp); // Bind right-eye texture for rendering
					GetUtils().DrawFullScreenTri(pTexRight->GetWidth(), pTexRight->GetHeight(), 1);
					CShaderMan::s_shPostEffects->FXEndPass();
					CShaderMan::s_shPostEffects->FXEnd();

					if (!bUseMirrorTexture)
					{
						RenderQuadLayers();
					}
				#endif
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
