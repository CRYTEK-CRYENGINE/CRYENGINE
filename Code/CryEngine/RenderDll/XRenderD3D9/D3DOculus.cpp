// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "D3DOculus.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"

#include <Common/RenderDisplayContext.h>

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
	m_swapChainInfoEyeLeft.pDeviceTextureSwapChain = nullptr;
	m_swapChainInfoEyeLeft.viewportPosition = Vec2i(ZERO);
	m_swapChainInfoEyeLeft.viewportSize = Vec2i(1, 1);
	m_swapChainInfoEyeLeft.layerType = RenderLayer::eLayer_Scene3D;
	m_swapChainInfoEyeLeft.layerId = RenderLayer::eSceneLayers_0;
	m_swapChainInfoEyeLeft.bActive = false;
	m_swapChainInfoEyeLeft.eye = CryVR::Oculus::SHmdSwapChainInfo::eye_t::left;
	m_swapChainInfoEyeRight.pDeviceTextureSwapChain = nullptr;
	m_swapChainInfoEyeRight.viewportPosition = Vec2i(ZERO);
	m_swapChainInfoEyeRight.viewportSize = Vec2i(1, 1);
	m_swapChainInfoEyeRight.layerType = RenderLayer::eLayer_Scene3D;
	m_swapChainInfoEyeRight.layerId = RenderLayer::eSceneLayers_0;
	m_swapChainInfoEyeRight.bActive = false;
	m_swapChainInfoEyeRight.eye = CryVR::Oculus::SHmdSwapChainInfo::eye_t::right;

	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_swapChainInfoQuadLayers[i].pDeviceTextureSwapChain = nullptr;
		m_swapChainInfoQuadLayers[i].viewportPosition = Vec2i(ZERO);
		m_swapChainInfoQuadLayers[i].viewportSize = Vec2i(1, 1);
		m_swapChainInfoQuadLayers[i].layerType = RenderLayer::eLayer_Quad;
		m_swapChainInfoQuadLayers[i].layerId = i;
		m_swapChainInfoQuadLayers[i].bActive = false;
	}

	// Init layers properties
	for (uint32 i = RenderLayer::eSceneLayers_0; i < RenderLayer::eSceneLayers_Total; ++i)
	{
		m_scene3DLayerProperties[i].SetType(RenderLayer::eLayer_Scene3D);
		m_scene3DLayerProperties[i].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -.8f), 1.f));
		m_scene3DLayerProperties[i].SetId(i);
	}

	for (uint32 i = RenderLayer::eQuadLayers_0; i < RenderLayer::eQuadLayers_Headlocked_0; ++i)
	{
		m_quadLayerProperties[i].SetType(RenderLayer::eLayer_Quad);
		m_quadLayerProperties[i].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -.8f), 1.f));
		m_quadLayerProperties[i].SetId(i);
	}
	for (uint32 i = RenderLayer::eQuadLayers_Headlocked_0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_quadLayerProperties[i].SetType(RenderLayer::eLayer_Quad_HeadLocked);
		m_quadLayerProperties[i].SetPose(QuatTS(Quat(IDENTITY), Vec3(0.f, 0.f, -.8f), 1.f));
		m_quadLayerProperties[i].SetId(i);
	}
}

void CD3DOculusRenderer::SLayersManager::UpdateSwapChainData(CD3DStereoRenderer* pStereoRenderer, const STextureSwapChainRenderData* scene3DRenderData, const STextureSwapChainRenderData* quadRenderData)
{
	const RenderLayer::CProperties& scene3DLayerPropertiesRT = m_scene3DLayerProperties[RenderLayer::eSceneLayers_0];

	m_swapChainInfoEyeLeft.pDeviceTextureSwapChain = scene3DRenderData[0].vrTextureSet.pDeviceTextureSwapChain;
	m_swapChainInfoEyeLeft.viewportPosition = scene3DRenderData[0].viewportPosition;
	m_swapChainInfoEyeLeft.viewportSize = scene3DRenderData[0].viewportSize;
	m_swapChainInfoEyeLeft.pose = scene3DLayerPropertiesRT.GetPose();
	m_swapChainInfoEyeLeft.bActive = scene3DLayerPropertiesRT.IsActive();
	m_swapChainInfoEyeRight.pDeviceTextureSwapChain = scene3DRenderData[1].vrTextureSet.pDeviceTextureSwapChain;
	m_swapChainInfoEyeRight.viewportPosition = scene3DRenderData[1].viewportPosition;
	m_swapChainInfoEyeRight.viewportSize = scene3DRenderData[1].viewportSize;
	m_swapChainInfoEyeRight.pose = scene3DLayerPropertiesRT.GetPose();
	m_swapChainInfoEyeRight.bActive = scene3DLayerPropertiesRT.IsActive();

	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		const RenderLayer::EQuadLayers layerId = RenderLayer::EQuadLayers(i);
		const RenderLayer::CProperties& quadLayerPropertiesRT = m_quadLayerProperties[layerId];

		const STextureSwapChainRenderData& quadLayer = quadRenderData[layerId];
		m_swapChainInfoQuadLayers[i].pDeviceTextureSwapChain = quadLayer.vrTextureSet.pDeviceTextureSwapChain;
		m_swapChainInfoQuadLayers[i].viewportPosition = quadLayer.viewportPosition;
		m_swapChainInfoQuadLayers[i].viewportSize = quadLayer.viewportSize;
		m_swapChainInfoQuadLayers[i].layerType = quadLayerPropertiesRT.GetType();
		m_swapChainInfoQuadLayers[i].pose = quadLayerPropertiesRT.GetPose();
		m_swapChainInfoQuadLayers[i].bActive = quadLayerPropertiesRT.IsActive();

		if (ITexture* pTexture = quadLayerPropertiesRT.GetTexture())
		{
			const auto *quadDc = pStereoRenderer->GetVrQuadLayerDisplayContext(layerId).first;
			if (quadDc)
			{
				CTexture* pQuadTex = quadDc->GetCurrentBackBuffer();
				GetUtils().StretchRect(static_cast<CTexture*>(pTexture), pQuadTex);
			}
		}
	}
}

CryVR::Oculus::SHmdSubmitFrameData CD3DOculusRenderer::SLayersManager::ConstructFrameData()
{
	CryVR::Oculus::SHmdSubmitFrameData data;
	data.pLeftEyeScene3D = &m_swapChainInfoEyeLeft;
	data.pRightEyeScene3D = &m_swapChainInfoEyeRight;
	data.pQuadLayersSwapChains = &m_swapChainInfoQuadLayers[0];
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
{
}

bool CD3DOculusRenderer::Initialize(int initialWidth, int initialeight)
{
	ID3D11Device* pD3d11Device = m_pRenderer->GetDevice_Unsynchronized().GetRealDevice();

	m_eyeWidth  = initialWidth;
	m_eyeHeight = initialeight;

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

	if (!InitializeTextureSwapSet(pD3d11Device, EEyeType::eEyeType_LeftEye, eyeTextureDesc, "$LeftEye_") ||
	    !InitializeTextureSwapSet(pD3d11Device, EEyeType::eEyeType_RightEye, eyeTextureDesc, "$RightEye_") ||
	    !InitializeQuadTextureSwapSet(pD3d11Device, RenderLayer::eQuadLayers_0, quadTextureDesc, "$QuadLayer0_") ||
		!InitializeQuadTextureSwapSet(pD3d11Device, RenderLayer::eQuadLayers_Headlocked_0, quadTextureDesc, "$QuadLayerHeadLocked0_") ||
		!InitializeQuadTextureSwapSet(pD3d11Device, RenderLayer::eQuadLayers_Headlocked_1, quadTextureDesc, "$QuadLayerHeadLocked1_") ||
	    !InitializeMirrorTexture(pD3d11Device, mirrorTextureDesc, "$StereoMirror"))
	{
		Shutdown();
		return false;
	}

	// Create display contexts for eyes
	for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
	{
		std::vector<_smart_ptr<CTexture>> swapChain;
		for (const auto& tex : m_scene3DRenderData[eye].textures)
			swapChain.emplace_back(tex.get());
		m_pStereoRenderer->CreateEyeDisplayContext(CCamera::EEye(eye), std::move(swapChain));
	}
	// Create display contexts for quad layers
	for (uint32 quad = 0; quad < RenderLayer::eQuadLayers_Total; ++quad)
	{
		std::vector<_smart_ptr<CTexture>> swapChain;
		for (const auto& tex : m_quadLayerRenderData[quad].textures)
			swapChain.emplace_back(tex.get());
		m_pStereoRenderer->CreateVrQuadLayerDisplayContext(RenderLayer::EQuadLayers(quad), std::move(swapChain));
	}

	SetupRenderTargets();

	m_deviceLostFlag = false;

	return true;
}

bool CD3DOculusRenderer::InitializeTextureSwapSet(ID3D11Device* pD3d11Device, EEyeType eye, STextureSwapChainRenderData& eyeRenderData, CryVR::Oculus::TextureDesc desc, const std::string& name)
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
			auto textureName = name + std::to_string(t);

			#if (CRY_RENDERER_DIRECT3D >= 120)
			D3DTexture* pTex = CCryDX12Texture2D::Create((CCryDX12Device*)pD3d11Device, nullptr, static_cast<ID3D12Resource*>(eyeRenderData.vrTextureSet.pTextures[t]));
			#else
			D3DTexture* pTex = (D3DTexture*)(eyeRenderData.vrTextureSet.pTextures[t]);
			#endif

			_smart_ptr<CTexture> eyeTexture = m_pStereoRenderer->WrapD3DRenderTarget(pTex, desc.width, desc.height, (D3DFormat)desc.format, textureName.c_str(), true);

			if (eyeTexture == nullptr)
				return false;

			eyeRenderData.textures.emplace_back(std::move(eyeTexture));
		}

		if (eyeRenderData.vrTextureSet.pTextures[t] == nullptr)
			return false;
	}

	return true;
}

bool CD3DOculusRenderer::InitializeTextureSwapSet(ID3D11Device* pD3d11Device, EEyeType eye, CryVR::Oculus::TextureDesc desc, const std::string& name)
{
	return InitializeTextureSwapSet(pD3d11Device, eye, m_scene3DRenderData[eye], desc, name);
}

bool CD3DOculusRenderer::InitializeQuadTextureSwapSet(ID3D11Device* d3dDevice, RenderLayer::EQuadLayers id, CryVR::Oculus::TextureDesc desc, const std::string& name)
{
	return InitializeTextureSwapSet(d3dDevice, static_cast<EEyeType>(-1), m_quadLayerRenderData[id], desc, name);
}

bool CD3DOculusRenderer::InitializeMirrorTexture(ID3D11Device* pD3d11Device, CryVR::Oculus::TextureDesc desc, const std::string& name)
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

		m_mirrorData.pMirrorTexture = m_pStereoRenderer->WrapD3DRenderTarget(pTex, desc.width, desc.height, (D3DFormat)desc.format, name.c_str(), true);

		return m_mirrorData.pMirrorTexture != nullptr;
	}

	return m_mirrorData.pMirrorTextureNative != nullptr;
}

void CD3DOculusRenderer::Shutdown()
{
	for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
		m_pStereoRenderer->CreateEyeDisplayContext(CCamera::EEye(eye), {});
	for (uint32 quad = 0; quad < RenderLayer::eQuadLayers_Total; ++quad)
		m_pStereoRenderer->CreateVrQuadLayerDisplayContext(RenderLayer::EQuadLayers(quad), {});

	// Scene3D layers
	for (uint32 eye = 0; eye < 2; ++eye)
	{
		if (m_scene3DRenderData[eye].vrTextureSet.pDeviceTextureSwapChain != nullptr)
			m_pOculusDevice->DestroySwapTextureSet(&m_scene3DRenderData[eye].vrTextureSet);
		m_scene3DRenderData[eye].textures = {};
	}

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_pOculusDevice->DestroySwapTextureSet(&m_quadLayerRenderData[i].vrTextureSet);
		m_quadLayerRenderData[i].textures = {};
	}

	// Mirror texture
	m_pOculusDevice->DestroyMirrorTexture(&m_mirrorData.vrMirrorTexture);
	m_mirrorData = {};

	ReleaseBuffers();

	m_deviceLostFlag = true;
}

void CD3DOculusRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_eyeWidth  != newWidth ||
		m_eyeHeight != newHeight)
	{
		Shutdown();
		Initialize(newWidth, newHeight);
	}
}

void CD3DOculusRenderer::PrepareFrame(uint64_t frameId)
{
	SetupRenderTargets();

	GetSceneLayerProperties(RenderLayer::eSceneLayers_0)->SetActive(true);

	// Recreate device if devicelost error was raised
	if (m_deviceLostFlag.load())
	{
		Shutdown();
		m_pOculusDevice->CreateDevice();
		Initialize(m_eyeWidth, m_eyeHeight);
	}

	if (m_pOculusDevice->BeginFrame(frameId) == CryVR::Oculus::OculusStatus::DeviceLost)
		m_deviceLostFlag = true;
}

void CD3DOculusRenderer::SetupRenderTargets()
{
	{
		std::array<uint32_t, eEyeType_NumEyes> indices = {}; // texture index in various swap chains
		for (uint32 eye = 0; eye < eEyeType_NumEyes; ++eye)
			indices[eye] = m_pOculusDevice->GetCurrentSwapChainIndex(m_scene3DRenderData[eye].vrTextureSet.pDeviceTextureSwapChain);
		m_pStereoRenderer->SetCurrentEyeSwapChainIndices(indices);
	}

	{
		// Quad layers
		std::array<uint32_t, RenderLayer::eQuadLayers_Total> indices = {}; // texture index in various swap chains
		for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
			indices[i] = m_pOculusDevice->GetCurrentSwapChainIndex(m_quadLayerRenderData[i].vrTextureSet.pDeviceTextureSwapChain);
		m_pStereoRenderer->SetCurrentQuadLayerSwapChainIndices(indices);
	}
}

void CD3DOculusRenderer::SubmitFrame()
{
	if (m_deviceLostFlag.load())
		return;

	FUNCTION_PROFILER_RENDERER();

	// Update swap chain info with the latest render and layer properties
	m_layerManager.UpdateSwapChainData(m_pStereoRenderer, m_scene3DRenderData, m_quadLayerRenderData);
	CryVR::Oculus::SHmdSubmitFrameData data = m_layerManager.ConstructFrameData();

#ifdef ENABLE_BENCHMARK_SENSOR
	const auto *leftDc = m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Left).first;
	const auto *rightDc = m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Right).first;
	if (leftDc && rightDc)
		gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(leftDc->GetCurrentBackBuffer(), rightDc->GetCurrentBackBuffer());
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
		const auto *leftDc = m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Left).first;
		const auto *rightDc = m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Right).first;
		if (!leftDc || !rightDc)
			return;

		CCryDX12RenderTargetView* lRV = (CCryDX12RenderTargetView*)leftDc->GetCurrentBackBuffer()->GetSurface(-1, 0);
		CCryDX12RenderTargetView* rRV = (CCryDX12RenderTargetView*)rightDc->GetCurrentBackBuffer()->GetSurface(-1, 0);

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
	if (m_pOculusDevice->SubmitFrame(data) == CryVR::Oculus::OculusStatus::DeviceLost)
		m_deviceLostFlag = true;

#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
#endif

	// Deactivate scene layers
	GetSceneLayerProperties(RenderLayer::eSceneLayers_0)->SetActive(false);
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
		GetQuadLayerProperties(static_cast<RenderLayer::EQuadLayers>(i))->SetActive(false);
}

RenderLayer::CProperties* CD3DOculusRenderer::GetQuadLayerProperties(RenderLayer::EQuadLayers id)
{
	if (id < RenderLayer::eQuadLayers_Total)
	{
		return &(m_layerManager.m_quadLayerProperties[id]);
	}
	return nullptr;
}

RenderLayer::CProperties* CD3DOculusRenderer::GetSceneLayerProperties(RenderLayer::ESceneLayers id)
{
	if (id < RenderLayer::eQuadLayers_Total)
	{
		return &(m_layerManager.m_scene3DLayerProperties[id]);
	}
	return nullptr;
}

std::pair<CTexture*, Vec4> CD3DOculusRenderer::GetMirrorTexture(EEyeType eye) const 
{
	Vec4 tc = Vec4(0.0f, 0.0f, 0.5f, 1.0f);
	if (eye == eEyeType_RightEye)
		tc.x = 0.5f;

	return std::make_pair(m_mirrorData.pMirrorTexture, tc);
}

#endif // defined(INCLUDE_VR_RENDERING)
