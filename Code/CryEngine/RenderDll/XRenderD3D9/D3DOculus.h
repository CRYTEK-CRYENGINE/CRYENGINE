// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <../CryPlugins/VR/CryOculusVR/Interface/IHmdOculusRiftDevice.h>
#include <CryRenderer/IStereoRenderer.h>

#include <string>
#include <atomic>
#include <mutex>

class CD3D9Renderer;

#if defined(INCLUDE_VR_RENDERING)

class CD3DOculusRenderer : public IHmdRenderer
{
public:
	CD3DOculusRenderer(CryVR::Oculus::IOculusDevice* oculusDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer);
	virtual ~CD3DOculusRenderer() = default;

	// IHDMRenderer
	virtual bool                      Initialize(int initialWidth, int initialeight) final;
	virtual void                      Shutdown() final;
	virtual void                      OnResolutionChanged(int newWidth, int newHeight) final;
	virtual void                      ReleaseBuffers() final {}
	virtual void                      PrepareFrame(uint64_t frameId) final;
	virtual void                      SubmitFrame() final;

	virtual RenderLayer::CProperties*  GetQuadLayerProperties(RenderLayer::EQuadLayers id) final;
	virtual RenderLayer::CProperties*  GetSceneLayerProperties(RenderLayer::ESceneLayers id) final;
	virtual std::pair<CTexture*, Vec4> GetMirrorTexture(EEyeType eye) const final;
	//~ IHDMRenderer

private:
	// Structure used for rendering the scene to the buffer of textures sent to the Hmd
	struct STextureSwapChainRenderData
	{
		CryVR::Oculus::STextureSwapChain  vrTextureSet;
		Vec2i                             viewportPosition;
		Vec2i                             viewportSize;
		std::vector<_smart_ptr<CTexture>> textures;
		std::vector<IUnknown*>            texturesNative;
	};

	// Structure used for rendering the social screen provided by Oculus (dual distorted) into a texture
	struct SMirrorTextureRenderData
	{
		SMirrorTextureRenderData() : pMirrorTexture(nullptr) {}
		CryVR::Oculus::STexture vrMirrorTexture; // device texture
		_smart_ptr<CTexture>    pMirrorTexture;  // CryEngine's texture used as render target for Oculus to write their mirror texture
		IUnknown*               pMirrorTextureNative;
	};

	bool InitializeTextureSwapSet(ID3D11Device* d3dDevice, EEyeType eye, STextureSwapChainRenderData& eyeRenderData, CryVR::Oculus::TextureDesc desc, const std::string& name);
	bool InitializeTextureSwapSet(ID3D11Device* d3dDevice, EEyeType eye, CryVR::Oculus::TextureDesc desc, const std::string& name);
	bool InitializeQuadTextureSwapSet(ID3D11Device* d3dDevice, RenderLayer::EQuadLayers id, CryVR::Oculus::TextureDesc desc, const std::string& name);
	bool InitializeMirrorTexture(ID3D11Device* d3dDevice, CryVR::Oculus::TextureDesc desc, const std::string& name);

	void SetupRenderTargets();

#if (CRY_RENDERER_DIRECT3D >= 120) && defined(DX12_LINKEDADAPTER)
	void CopyMultiGPUFrameData();
	void CopyMultiGPUMirrorData(CTexture* pBackbufferTexture);
#endif

private:
	// Helper to manage the layers in the Renderer
	// TODO: Depending on the layer future support by other vendors, this could become a separate system with its own interface
	struct SLayersManager
	{
		SLayersManager();

		void                               UpdateSwapChainData(CD3DStereoRenderer* pStereoRenderer, const STextureSwapChainRenderData* scene3DRenderData, const STextureSwapChainRenderData* quadRenderData);
		CryVR::Oculus::SHmdSubmitFrameData ConstructFrameData();

		RenderLayer::CProperties         m_scene3DLayerProperties[RenderLayer::eSceneLayers_Total];
		RenderLayer::CProperties         m_quadLayerProperties[RenderLayer::eQuadLayers_Total];
		CryVR::Oculus::SHmdSwapChainInfo m_swapChainInfoEyeLeft;
		CryVR::Oculus::SHmdSwapChainInfo m_swapChainInfoEyeRight;
		CryVR::Oculus::SHmdSwapChainInfo m_swapChainInfoQuadLayers[2 + RenderLayer::eQuadLayers_Total];
	};

private:
	SMirrorTextureRenderData      m_mirrorData;
	STextureSwapChainRenderData   m_scene3DRenderData[eEyeType_NumEyes];
	STextureSwapChainRenderData   m_quadLayerRenderData[RenderLayer::eQuadLayers_Total];

	SLayersManager                m_layerManager;

	uint32                        m_eyeWidth;
	uint32                        m_eyeHeight;

	CryVR::Oculus::IOculusDevice* m_pOculusDevice;
	CD3D9Renderer*                m_pRenderer;
	CD3DStereoRenderer*           m_pStereoRenderer;

	// Flag that is raised in case device was lost during frame preparation or submission
	std::atomic<bool>             m_deviceLostFlag{ true };
};

#endif //defined(INCLUDE_VR_RENDERING)