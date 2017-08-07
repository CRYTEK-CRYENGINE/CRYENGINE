// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "D3DOculus.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#ifdef ENABLE_BENCHMARK_SENSOR
	#include <IBenchmarkFramework.h>
	#include <IBenchmarkRendererSensorManager.h>
#endif
#if defined(INCLUDE_VR_RENDERING)

	#if (CRY_RENDERER_DIRECT3D >= 120)
		#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"
		#if defined(DX12_LINKEDADAPTER)
			#include "DX12/API/Redirections/D3D12Device.inl"
			#include "DX12/API/Workarounds/OculusMultiGPU.inl"
		#endif
	#endif

CD3DOculusRenderer::SLayersManager::SLayersManager()
{
	// Init swap chains data
	for (uint32 eye = eSwapChainArray_Scene3D_LeftEye; eye <= eSwapChainArray_Scene3D_RightEye; ++eye)
	{
		m_swapChainInfoArray[eye].pDeviceTextureSwapChain = nullptr;
		m_swapChainInfoArray[eye].viewportPosition = Vec2i(ZERO);
		m_swapChainInfoArray[eye].viewportSize = Vec2i(1, 1);
		m_swapChainInfoArray[eye].layerType = RenderLayer::eLayer_Scene3D;
		m_swapChainInfoArray[eye].layerId = RenderLayer::eSceneLayers_0;
		m_swapChainInfoArray[eye].bActive = false;
		m_swapChainInfoArray[eye].eye = eye;
	}

	for (uint32 i = eSwapChainArray_FirstQuad; i <= eSwapChainArray_LastQuad; ++i)
	{
		m_swapChainInfoArray[i].pDeviceTextureSwapChain = nullptr;
		m_swapChainInfoArray[i].viewportPosition = Vec2i(ZERO);
		m_swapChainInfoArray[i].viewportSize = Vec2i(1, 1);
		m_swapChainInfoArray[i].layerType = RenderLayer::eLayer_Quad;
		m_swapChainInfoArray[i].layerId = CalculateQuadLayerId(static_cast<ESwapChainArray>(i));
		m_swapChainInfoArray[i].bActive = false;
		m_swapChainInfoArray[i].eye = 0;
	}

	// Init layers properties
	for (uint32 bufIdx = 0; bufIdx < BUFFER_SIZE_LAYER_PROPERTIES; ++bufIdx)
	{
		for (uint32 i = RenderLayer::eSceneLayers_0; i < RenderLayer::eSceneLayers_Total; ++i)
		{
			m_scene3DLayerProperties[i][bufIdx].SetType(RenderLayer::eLayer_Scene3D);
			m_scene3DLayerProperties[i][bufIdx].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -0.6f), 1.f));
			m_scene3DLayerProperties[i][bufIdx].SetId(i);
		}
	}

	for (uint32 bufIdx = 0; bufIdx < BUFFER_SIZE_LAYER_PROPERTIES; ++bufIdx)
	{
		for (uint32 i = RenderLayer::eQuadLayers_0; i < RenderLayer::eQuadLayers_Total; ++i)
		{
			m_quadLayerProperties[i][bufIdx].SetType(RenderLayer::eLayer_Quad);
			m_quadLayerProperties[i][bufIdx].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -0.6f), 1.f));
			m_quadLayerProperties[i][bufIdx].SetId(i);
		}
	}
}

uint32 CD3DOculusRenderer::SLayersManager::GetBufferIndex()
{
	const int frameID = gEnv->pRenderer->GetFrameID(false);
	return (frameID % BUFFER_SIZE_LAYER_PROPERTIES);
}

void CD3DOculusRenderer::SLayersManager::UpdateSwapChainData(CD3DStereoRenderer* pStereoRenderer, const STextureSwapChainRenderData* scene3DRenderData, const STextureSwapChainRenderData* quadRenderData)
{
	const uint32 frameIdx = SLayersManager::GetBufferIndex();
	const RenderLayer::CProperties& scene3DLayerPropertiesRT = m_scene3DLayerProperties[RenderLayer::eSceneLayers_0][frameIdx];

	for (uint32 eye = eSwapChainArray_Scene3D_LeftEye; eye <= eSwapChainArray_Scene3D_RightEye; ++eye)
	{
		m_swapChainInfoArray[eye].pDeviceTextureSwapChain = scene3DRenderData[eye].vrTextureSet.pDeviceTextureSwapChain;
		m_swapChainInfoArray[eye].viewportPosition = scene3DRenderData[eye].viewportPosition;
		m_swapChainInfoArray[eye].viewportSize = scene3DRenderData[eye].viewportSize;
		m_swapChainInfoArray[eye].pose = scene3DLayerPropertiesRT.GetPose();     // dario: TODO FIX this for multiple scene layers
		m_swapChainInfoArray[eye].bActive = scene3DLayerPropertiesRT.IsActive(); // dario: TODO FIX this for multiple scene layers
	}

	// DARIO TODO: account for more than one scene3d layers
	if (ITexture* pTexture = m_scene3DLayerProperties[RenderLayer::eSceneLayers_0][frameIdx].GetTexture())
	{
		CTexture* pLeftEye = pStereoRenderer->GetEyeTarget(LEFT_EYE);
		CTexture* pRightEye = pStereoRenderer->GetEyeTarget(RIGHT_EYE);
		GetUtils().StretchRect(static_cast<CTexture*>(pTexture), pLeftEye);
		GetUtils().StretchRect(static_cast<CTexture*>(pTexture), pRightEye);
	}

	for (uint32 i = eSwapChainArray_FirstQuad; i <= eSwapChainArray_LastQuad; ++i)
	{
		const RenderLayer::EQuadLayers layerId = CalculateQuadLayerId(static_cast<ESwapChainArray>(i));
		const RenderLayer::CProperties& quadLayerPropertiesRT = m_quadLayerProperties[layerId][frameIdx];

		const STextureSwapChainRenderData& quadLayer = quadRenderData[layerId];
		m_swapChainInfoArray[i].pDeviceTextureSwapChain = quadLayer.vrTextureSet.pDeviceTextureSwapChain;
		m_swapChainInfoArray[i].viewportPosition = quadLayer.viewportPosition;
		m_swapChainInfoArray[i].viewportSize = quadLayer.viewportSize;
		m_swapChainInfoArray[i].layerType = quadLayerPropertiesRT.GetType();
		m_swapChainInfoArray[i].pose = quadLayerPropertiesRT.GetPose();
		m_swapChainInfoArray[i].bActive = quadLayerPropertiesRT.IsActive();

		if (ITexture* pTexture = quadLayerPropertiesRT.GetTexture())
		{
			CTexture* pQuadTex = pStereoRenderer->GetVrQuadLayerTex(layerId);
			GetUtils().StretchRect(static_cast<CTexture*>(pTexture), pQuadTex);
		}
	}
}

CryVR::Oculus::SHmdSubmitFrameData CD3DOculusRenderer::SLayersManager::ConstructFrameData()
{
	CryVR::Oculus::SHmdSubmitFrameData data;
	data.pLeftEyeScene3D = &m_swapChainInfoArray[eSwapChainArray_Scene3D_LeftEye];
	data.pRightEyeScene3D = &m_swapChainInfoArray[eSwapChainArray_Scene3D_RightEye];
	data.pQuadLayersSwapChains = &m_swapChainInfoArray[eSwapChainArray_Quad_0];
	data.numQuadLayersSwapChains = RenderLayer::eQuadLayers_Total;

	return data;
}

// -------------------------------------------------------------------------

CD3DOculusRenderer::CD3DOculusRenderer(CryVR::Oculus::IOculusDevice* oculusDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer)
	: m_pOculusDevice(oculusDevice)
	, m_pRenderer(renderer)
	, m_pStereoRenderer(stereoRenderer)
	, m_eyeWidth(~0L)
	, m_eyeHeight(~0L)
	, m_pStrechRectPass(nullptr)
{
}

CD3DOculusRenderer::~CD3DOculusRenderer()
{
}

bool CD3DOculusRenderer::Initialize()
{
	ID3D11Device* pD3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();

	m_eyeWidth = m_pRenderer->GetWidth();
	m_eyeHeight = m_pRenderer->GetHeight();

	CryVR::Oculus::TextureDesc eyeTextureDesc;
	eyeTextureDesc.width = m_eyeWidth;
	eyeTextureDesc.height = m_eyeHeight;
	eyeTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	CryVR::Oculus::TextureDesc quadTextureDesc;
	quadTextureDesc.width = m_eyeWidth;
	quadTextureDesc.height = m_eyeHeight;
	quadTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	CryVR::Oculus::TextureDesc mirrorTextureDesc;
	mirrorTextureDesc.width = m_eyeWidth * 2;
	mirrorTextureDesc.height = m_eyeHeight;
	mirrorTextureDesc.format = (uint32)DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	if (!InitializeTextureSwapSet(pD3d11Device, EEyeType::eEyeType_LeftEye, eyeTextureDesc, "$LeftEye_%d") ||
	    !InitializeTextureSwapSet(pD3d11Device, EEyeType::eEyeType_RightEye, eyeTextureDesc, "$RightEye_%d") ||
	    !InitializeQuadTextureSwapSet(pD3d11Device, RenderLayer::eQuadLayers_0, quadTextureDesc, "$QuadLayer0_%d") ||
	    !InitializeQuadTextureSwapSet(pD3d11Device, RenderLayer::eQuadLayers_1, quadTextureDesc, "$QuadLayer1_%d") ||
	    !InitializeMirrorTexture(pD3d11Device, mirrorTextureDesc, "$StereoMirror"))
	{
		Shutdown();
		return false;
	}

	return true;
}

bool CD3DOculusRenderer::InitializeTextureSwapSet(ID3D11Device* pD3d11Device, EEyeType eye, STextureSwapChainRenderData& eyeRenderData, CryVR::Oculus::TextureDesc desc, const char* szNameFormat)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	ID3D12CommandQueue* pD3d12Queue = GetDeviceObjectFactory().GetNativeCoreCommandQueue();

	if (!m_pOculusDevice->CreateSwapTextureSetD3D12(pD3d12Queue, desc, &eyeRenderData.vrTextureSet))
		return false;
	#elif (CRY_RENDERER_DIRECT3D >= 110)
	if (!m_pOculusDevice->CreateSwapTextureSetD3D11(pD3d11Device, desc, &eyeRenderData.vrTextureSet))
		return false;
	#else
	return false;
	#endif

	uint32 numTextures = eyeRenderData.vrTextureSet.numTextures;
	if (numTextures == 0)
	{
		gEnv->pLog->Log("[HMD][Oculus] Texture swap set is empty!");
		return false;
	}

	eyeRenderData.textures.reserve(numTextures);
	for (uint32 t = 0; t < numTextures; ++t)
	{
		eyeRenderData.texturesNative.push_back(eyeRenderData.vrTextureSet.pTextures[t]);
		eyeRenderData.viewportPosition = Vec2i(0, 0);
		eyeRenderData.viewportSize = Vec2i(desc.width, desc.height);

		// NOTE: Workaround for missing MultiGPU-support in the Oculus library
		if (m_pRenderer->GetDevice().GetNodeCount() == 1)
		{
			char textureName[16];
			cry_sprintf(textureName, szNameFormat, t);

			#if (CRY_RENDERER_DIRECT3D >= 120)
			D3DTexture* pTex = CCryDX12Texture2D::Create((CCryDX12Device*)pD3d11Device, nullptr, static_cast<ID3D12Resource*>(eyeRenderData.vrTextureSet.pTextures[t]));
			#else
			D3DTexture* pTex = (D3DTexture*)(eyeRenderData.vrTextureSet.pTextures[t]);
			#endif

			CTexture* eyeTexture = m_pStereoRenderer->WrapD3DRenderTarget(pTex, desc.width, desc.height, (D3DFormat)desc.format, textureName, true);

			if (eyeTexture == nullptr)
				return false;

			eyeRenderData.textures.push_back(eyeTexture);
		}

		if (eyeRenderData.vrTextureSet.pTextures[t] == nullptr)
			return false;
	}

	return true;
}

bool CD3DOculusRenderer::InitializeTextureSwapSet(ID3D11Device* pD3d11Device, EEyeType eye, CryVR::Oculus::TextureDesc desc, const char* szNameFormat)
{
	return InitializeTextureSwapSet(pD3d11Device, eye, m_scene3DRenderData[eye], desc, szNameFormat);
}

bool CD3DOculusRenderer::InitializeQuadTextureSwapSet(ID3D11Device* d3dDevice, RenderLayer::EQuadLayers id, CryVR::Oculus::TextureDesc desc, const char* szNameFormat)
{
	return InitializeTextureSwapSet(d3dDevice, static_cast<EEyeType>(-1), m_quadLayerRenderData[id], desc, szNameFormat);
}

bool CD3DOculusRenderer::InitializeMirrorTexture(ID3D11Device* pD3d11Device, CryVR::Oculus::TextureDesc desc, const char* szName)
{
	#if (CRY_RENDERER_DIRECT3D >= 120)
	ID3D12CommandQueue* pD3d12Queue = GetDeviceObjectFactory().GetNativeCoreCommandQueue();

	if (!m_pOculusDevice->CreateMirrorTextureD3D12(pD3d12Queue, desc, &m_mirrorData.vrMirrorTexture))
		return false;
	#elif (CRY_RENDERER_DIRECT3D >= 110)
	if (!m_pOculusDevice->CreateMirrorTextureD3D11(pD3d11Device, desc, &m_mirrorData.vrMirrorTexture))
		return false;
	#endif

	m_mirrorData.pMirrorTextureNative = m_mirrorData.vrMirrorTexture.pTexture;

	// NOTE: Workaround for missing MultiGPU-support in the Oculus library
	if (m_pRenderer->GetDevice().GetNodeCount() == 1)
	{
		#if (CRY_RENDERER_DIRECT3D >= 120)
		D3DTexture* pTex = CCryDX12Texture2D::Create((CCryDX12Device*)pD3d11Device, nullptr, static_cast<ID3D12Resource*>(m_mirrorData.vrMirrorTexture.pTexture));
		#else
		D3DTexture* pTex = (D3DTexture*)(m_mirrorData.vrMirrorTexture.pTexture);
		#endif

		m_mirrorData.pMirrorTexture = m_pStereoRenderer->WrapD3DRenderTarget(pTex, desc.width, desc.height, (D3DFormat)desc.format, szName, true);

		return m_mirrorData.pMirrorTexture != nullptr;
	}

	return m_mirrorData.pMirrorTextureNative != nullptr;
}

void CD3DOculusRenderer::Shutdown()
{
	m_pStereoRenderer->SetEyeTextures(nullptr, nullptr);

	// Scene3D layers
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		for (uint32 t = 0; t < m_scene3DRenderData[eye].textures.size(); ++t)
		{
			m_scene3DRenderData[eye].textures[t]->ReleaseForce();
		}
		m_scene3DRenderData[eye].textures.clear();

		if (m_scene3DRenderData[eye].vrTextureSet.pDeviceTextureSwapChain != nullptr)
			m_pOculusDevice->DestroySwapTextureSet(&m_scene3DRenderData[eye].vrTextureSet);
	}

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		for (uint32 t = 0, numTextures = m_quadLayerRenderData[i].textures.size(); t < numTextures; ++t)
		{
			m_quadLayerRenderData[i].textures[t]->ReleaseForce();
		}

		m_quadLayerRenderData[i].textures.clear();
		m_pOculusDevice->DestroySwapTextureSet(&m_quadLayerRenderData[i].vrTextureSet);
	}

	// Mirror texture
	if (m_mirrorData.pMirrorTexture != nullptr)
		m_mirrorData.pMirrorTexture->ReleaseForce();
	if (m_mirrorData.vrMirrorTexture.pDeviceTexture != nullptr)
		m_pOculusDevice->DestroyMirrorTexture(&m_mirrorData.vrMirrorTexture);

	ReleaseBuffers();
}

void CD3DOculusRenderer::OnResolutionChanged()
{
	if (m_eyeWidth != m_pRenderer->GetWidth() ||
		m_eyeHeight != m_pRenderer->GetHeight())
	{
		Shutdown();
		Initialize();
	}
}

void CD3DOculusRenderer::ReleaseBuffers()
{
	SAFE_DELETE(m_pStrechRectPass);
}

void CD3DOculusRenderer::PrepareFrame()
{
	int index[eEyeType_NumEyes] = { 0 }; // texture index in various swap chains

	// Scene3D layer
	for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
	{
		index[eye] = m_pOculusDevice->GetCurrentSwapChainIndex(m_scene3DRenderData[eye].vrTextureSet.pDeviceTextureSwapChain);
	}

	// Assign L/R instead of backbuffers for MultiGPU as Oculus doesn't support it
	if (m_pRenderer->GetDevice().GetNodeCount() > 1)
	{
		m_pStereoRenderer->SetEyeTextures
		(
			CTexture::s_ptexStereoL,
			CTexture::s_ptexStereoR
		);

		// Quad layers
		for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
		{
			m_pStereoRenderer->SetVrQuadLayerTexture(static_cast<RenderLayer::EQuadLayers>(i), CTexture::s_ptexQuadLayers[i]);
		}
	}
	else
	{
		m_pStereoRenderer->SetEyeTextures
		(
			m_scene3DRenderData[0].textures[index[0]],
			m_scene3DRenderData[1].textures[index[1]]
		);

		// Quad layers
		for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
		{
			const int idx = m_pOculusDevice->GetCurrentSwapChainIndex(m_quadLayerRenderData[i].vrTextureSet.pDeviceTextureSwapChain);
			m_pStereoRenderer->SetVrQuadLayerTexture(static_cast<RenderLayer::EQuadLayers>(i), m_quadLayerRenderData[i].textures[idx]);
		}
	}

	// Check if we have to create a new RectPass
	if (m_pStrechRectPass == nullptr)
	{
		m_pStrechRectPass = new CStretchRectPass();
	}

	// DARIO (TODO): could we disable scene3d layer ? (Quad layers are activated on-demand)
	GetSceneLayerProperties(RenderLayer::eSceneLayers_0)->SetActive(true);
}

void CD3DOculusRenderer::SubmitFrame()
{
	FUNCTION_PROFILER_RENDERER

	// Update swap chain info with the latest render and layer properties
	m_layerManager.UpdateSwapChainData(m_pStereoRenderer, m_scene3DRenderData, m_quadLayerRenderData);
	CryVR::Oculus::SHmdSubmitFrameData data = m_layerManager.ConstructFrameData();

	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(m_pStereoRenderer->GetEyeTarget(LEFT_EYE), m_pStereoRenderer->GetEyeTarget(RIGHT_EYE));
	#endif

	#if (CRY_RENDERER_DIRECT3D >= 120)
	ID3D11Device* pD3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();
	NCryDX12::CCommandList* pCL = ((CCryDX12Device*)pD3d11Device)->GetDeviceContext()->GetCoreGraphicsCommandList();

	#if DX12_LINKEDADAPTER
	// NOTE: Workaround for missing MultiGPU-support in the Oculus library
	if (m_pRenderer->GetDevice().GetNodeCount() > 1)
	{
		CopyMultiGPUFrameData();
	}
	else
	#endif
	{
		// Scene3D layer
		CCryDX12RenderTargetView* lRV = (CCryDX12RenderTargetView*)m_pStereoRenderer->GetEyeTarget(LEFT_EYE)->GetSurface(-1, 0);
		CCryDX12RenderTargetView* rRV = (CCryDX12RenderTargetView*)m_pStereoRenderer->GetEyeTarget(RIGHT_EYE)->GetSurface(-1, 0);

		NCryDX12::CView& lV = lRV->GetDX12View();
		NCryDX12::CView& rV = rRV->GetDX12View();

		lV.GetDX12Resource().TransitionBarrier(pCL, lV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		rV.GetDX12Resource().TransitionBarrier(pCL, rV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// Quad layers
		for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
		{
			const int idx = m_pOculusDevice->GetCurrentSwapChainIndex(m_quadLayerRenderData[i].vrTextureSet.pDeviceTextureSwapChain);
			if (m_quadLayerRenderData[i].textures[idx])
			{
				CCryDX12RenderTargetView* qRV = (CCryDX12RenderTargetView*)m_quadLayerRenderData[i].textures[idx]->GetSurface(-1, 0);
				if (qRV)
				{
					NCryDX12::CView& qV = qRV->GetDX12View();

					qV.GetDX12Resource().TransitionBarrier(pCL, qV, D3D12_RESOURCE_STATE_RENDER_TARGET);
				}
			}
		}

		((CCryDX12Device*)pD3d11Device)->GetDeviceContext()->Finish();
	}
	#endif

	// Pass the final images and layer configuration to the Oculus device
	m_pOculusDevice->SubmitFrame(data);

	#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
	#endif

	// Deactivate layers
	GetSceneLayerProperties(RenderLayer::eSceneLayers_0)->SetActive(false);
	GetQuadLayerProperties(RenderLayer::eQuadLayers_0)->SetActive(false);
	GetQuadLayerProperties(RenderLayer::eQuadLayers_1)->SetActive(false);
}

void CD3DOculusRenderer::RenderSocialScreen()
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
				GetUtils().ClearScreen(1.0f, 1.0f, 1.0f, 1.f);
				break;
			// intentional fall through
			case EHmdSocialScreen::UndistortedLeftEye:
			case EHmdSocialScreen::UndistortedRightEye:
			case EHmdSocialScreen::UndistortedDualImage:
				{
					static CCryNameTSCRC pTechTexToTex("TextureToTexture");
					const auto frameData = m_layerManager.ConstructFrameData();
					const bool bRenderBothEyes = socialScreen == EHmdSocialScreen::UndistortedDualImage;

					int eyesToRender[2] = { LEFT_EYE, -1 };
					if (socialScreen == EHmdSocialScreen::UndistortedRightEye)  eyesToRender[0] = RIGHT_EYE;
					else if (socialScreen == EHmdSocialScreen::UndistortedDualImage)
						eyesToRender[1] = RIGHT_EYE;

					uint64 nSaveFlagsShader_RT = gRenDev->m_RP.m_FlagsShader_RT;
					gRenDev->m_RP.m_FlagsShader_RT &= ~(g_HWSR_MaskBit[HWSR_SAMPLE0] | g_HWSR_MaskBit[HWSR_SAMPLE1] | g_HWSR_MaskBit[HWSR_SAMPLE2] | g_HWSR_MaskBit[HWSR_SAMPLE4] | g_HWSR_MaskBit[HWSR_SAMPLE5] | g_HWSR_MaskBit[HWSR_REVERSE_DEPTH]);

					int iTempX, iTempY, iWidth, iHeight;
					gRenDev->GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

					if (bKeepAspect)
					{
						gcpRendD3D->FX_ClearTarget(pBackbufferTexture, Clr_Empty);
					}

					gcpRendD3D->FX_PushRenderTarget(0, pBackbufferTexture, nullptr);
					gcpRendD3D->FX_SetActiveRenderTargets();

					for (int i = 0; i < CRY_ARRAY_COUNT(eyesToRender); ++i)
					{
						if (eyesToRender[i] < 0)
							continue;

						const auto pSawpChainInfo = eyesToRender[i] == LEFT_EYE ? frameData.pLeftEyeScene3D : frameData.pRightEyeScene3D;

						Vec2 targetSize;
						targetSize.x = pBackbufferTexture->GetWidth() * (bRenderBothEyes ? 0.5f : 1.0f);
						targetSize.y = float(pBackbufferTexture->GetHeight());

						Vec2 srcToTargetScale;
						srcToTargetScale.x = targetSize.x / pSawpChainInfo->viewportSize.x;
						srcToTargetScale.y = targetSize.y / pSawpChainInfo->viewportSize.y;

						if (bKeepAspect)
						{
							float minScale = min(srcToTargetScale.x, srcToTargetScale.y);

							srcToTargetScale.x = minScale;
							srcToTargetScale.y = minScale;
						}

						auto rescaleViewport = [&](const Vec2i& viewportPosition, const Vec2i& viewportSize)
						{
							Vec4 result;
							result.x = viewportPosition.x * srcToTargetScale.x;
							result.y = viewportPosition.y * srcToTargetScale.y;
							result.z = viewportSize.x * srcToTargetScale.x;
							result.w = viewportSize.y * srcToTargetScale.y;

							// shift to center
							Vec2 emptySpace;
							emptySpace.x = targetSize.x - result.z;
							emptySpace.y = targetSize.y - result.w;

							if (bRenderBothEyes)
							{
								result.x += (i == 0) ? emptySpace.x : targetSize.x;
								result.y += (i == 0) ? emptySpace.y : 0;
							}
							else if (!bRenderBothEyes)
							{
								result.x += 0.5f * emptySpace.x;
								result.y += 0.5f * emptySpace.y;
							}

							return result;
						};

						// draw eye texture first
						if (CTexture* pSrcTex = m_pStereoRenderer->GetEyeTarget(StereoEye(eyesToRender[i])))
						{
							auto vp = rescaleViewport(pSawpChainInfo->viewportPosition, pSawpChainInfo->viewportSize);
							gcpRendD3D->RT_SetViewport(int(vp.x), int(vp.y), int(vp.z), int(vp.w));

							GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechTexToTex, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

							gRenDev->FX_SetState(GS_NODEPTHTEST);
							pSrcTex->Apply(0, EDefaultSamplerStates::LinearClamp);

							GetUtils().DrawFullScreenTri(0, 0);
							GetUtils().ShEndPass();
						}

						// now draw quad layers
						for (int i = 0; i < frameData.numQuadLayersSwapChains; ++i)
						{
							const auto& quadLayer = frameData.pQuadLayersSwapChains[i];
							if (quadLayer.bActive)
							{
								if (auto pQuadTex = m_pStereoRenderer->GetVrQuadLayerTex(RenderLayer::EQuadLayers(i)))
								{
									auto vp = rescaleViewport(quadLayer.viewportPosition, quadLayer.viewportSize);
									gcpRendD3D->RT_SetViewport(int(vp.x), int(vp.y), int(vp.z), int(vp.w));

									GetUtils().ShBeginPass(CShaderMan::s_shPostEffects, pTechTexToTex, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);

									gRenDev->FX_SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
									pQuadTex->Apply(0, EDefaultSamplerStates::LinearClamp);

									GetUtils().DrawFullScreenTri(0, 0);
									GetUtils().ShEndPass();
								}
							}
						}
					}

					// Restore previous viewport
					gcpRendD3D->FX_PopRenderTarget(0);
					gcpRendD3D->RT_SetViewport(iTempX, iTempY, iWidth, iHeight);

					gRenDev->m_RP.m_FlagsShader_RT = nSaveFlagsShader_RT;
				}
				break;

			case EHmdSocialScreen::DistortedDualImage:
			default:
				{
					if (pBackbufferTexture->GetDevTexture()->Get2DTexture() != nullptr)
					{
						#if (CRY_RENDERER_DIRECT3D >= 120) && DX12_LINKEDADAPTER
						// NOTE: Workaround for missing MultiGPU-support in the Oculus library
						if (m_pRenderer->GetDevice().GetNodeCount() > 1)
						{
							CopyMultiGPUMirrorData(pBackbufferTexture);
						}
						else
						#endif
						{
							if (m_pStrechRectPass != nullptr)
							{
								m_pStrechRectPass->Execute(m_mirrorData.pMirrorTexture, pBackbufferTexture);
							}
						
						}
					}
				}
				break;
			}
		}
	}
}

RenderLayer::CProperties* CD3DOculusRenderer::GetQuadLayerProperties(RenderLayer::EQuadLayers id)
{
	if (id < RenderLayer::eQuadLayers_Total)
	{
		const uint32 frameIdx = SLayersManager::GetBufferIndex();
		return &(m_layerManager.m_quadLayerProperties[id][frameIdx]);
	}
	return nullptr;
}

RenderLayer::CProperties* CD3DOculusRenderer::GetSceneLayerProperties(RenderLayer::ESceneLayers id)
{
	if (id < RenderLayer::eQuadLayers_Total)
	{
		const uint32 frameIdx = SLayersManager::GetBufferIndex();
		return &(m_layerManager.m_scene3DLayerProperties[id][frameIdx]);
	}
	return nullptr;
}

bool CD3DOculusRenderer::PushTextureSet(RenderLayer::ELayerType type, RenderLayer::TLayerId id, CTexture* pTexture)
{
	if (pTexture != nullptr)
	{
		if (type == RenderLayer::eLayer_Scene3D)
		{
			if (id >= RenderLayer::eSceneLayers_0 && id < RenderLayer::eSceneLayers_Total)
			{
				// DARIO TODO: account for more than one scene3d layers
				for (uint32 eye = 0; eye < 2; ++eye)
				{
					TArray<CTexture*>& ctextureArray = m_scene3DRenderData[eye].textures;
					for (uint32 i = 0, cnt = ctextureArray.size(); i < cnt; ++i)
					{
						GetUtils().StretchRect(pTexture, ctextureArray[i]);
					}
				}
				return true;
			}
		}
		else if (type == RenderLayer::eLayer_Quad || type == RenderLayer::eLayer_Quad_HeadLoked)
		{
			if (id >= RenderLayer::eQuadLayers_0 && id < RenderLayer::eQuadLayers_Total)
			{
				TArray<CTexture*>& ctextureArray = m_quadLayerRenderData[id].textures;
				for (uint32 i = 0, cnt = ctextureArray.size(); i < cnt; ++i)
				{
					GetUtils().StretchRect(pTexture, ctextureArray[i]);
				}
				return true;
			}
		}
	}

	return false;
}

RenderLayer::EQuadLayers CD3DOculusRenderer::CalculateQuadLayerId(ESwapChainArray swapChainIndex)
{
	if (swapChainIndex >= eSwapChainArray_FirstQuad && swapChainIndex <= eSwapChainArray_LastQuad)
	{
		return static_cast<RenderLayer::EQuadLayers>(swapChainIndex - eSwapChainArray_FirstQuad);
	}

	CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "[Oculus] CalculateQuadLayerId(%u) got passed a swapchain that did not belong to a Quad layer", swapChainIndex);
	return RenderLayer::eQuadLayers_0;
}

#endif // defined(INCLUDE_VR_RENDERING)
