// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <../CryPlugins/VR/CryOculusVR/Interface/IHmdOculusRiftDevice.h>
#include <CryRenderer/IStereoRenderer.h>

class CD3D9Renderer;

#if defined(INCLUDE_VR_RENDERING)

class CD3DOculusRenderer : public IHmdRenderer
{
public:
	CD3DOculusRenderer(CryVR::Oculus::IOculusDevice* oculusDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer);
	~CD3DOculusRenderer();

	// IHDMRenderer
	virtual bool                      Initialize() override;
	virtual void                      Shutdown() override;
	virtual void                      OnResolutionChanged() override;
	virtual void                      ReleaseBuffers() override;
	virtual void                      PrepareFrame() override;
	virtual void                      SubmitFrame() override;
	virtual void                      RenderSocialScreen() override;
	virtual RenderLayer::CProperties* GetQuadLayerProperties(RenderLayer::EQuadLayers id) override;
	virtual RenderLayer::CProperties* GetSceneLayerProperties(RenderLayer::ESceneLayers id) override;
	//~ IHDMRenderer

	// Experimental: (TODO) this could be used when a more advance mechanism for controlling the layer update is in place
	bool PushTextureSet(RenderLayer::ELayerType type, RenderLayer::TLayerId id, CTexture* pTexture);

private:
	// Structure used for rendering the scene to the buffer of textures sent to the Hmd
	struct STextureSwapChainRenderData
	{
		CryVR::Oculus::STextureSwapChain vrTextureSet;
		Vec2i                            viewportPosition;
		Vec2i                            viewportSize;
		TArray<CTexture*>                textures;
		TArray<IUnknown*>                texturesNative;
	};

	// Structure used for rendering the social screen provided by Oculus (dual distorted) into a texture
	struct SMirrorTextureRenderData
	{
		SMirrorTextureRenderData() : pMirrorTexture(nullptr) {}
		CryVR::Oculus::STexture vrMirrorTexture; // device texture
		CTexture*               pMirrorTexture;  // CryEngine's texture used as render target for Oculus to write their mirror texture
		IUnknown*               pMirrorTextureNative;
	};

	bool InitializeTextureSwapSet(ID3D11Device* d3dDevice, EEyeType eye, STextureSwapChainRenderData& eyeRenderData, CryVR::Oculus::TextureDesc desc, const char* nameFormat);
	bool InitializeTextureSwapSet(ID3D11Device* d3dDevice, EEyeType eye, CryVR::Oculus::TextureDesc desc, const char* nameFormat);
	bool InitializeQuadTextureSwapSet(ID3D11Device* d3dDevice, RenderLayer::EQuadLayers id, CryVR::Oculus::TextureDesc desc, const char* nameFormat);
	bool InitializeMirrorTexture(ID3D11Device* d3dDevice, CryVR::Oculus::TextureDesc desc, const char* name);

private:

	enum ESwapChainArray
	{
		eSwapChainArray_Scene3D_LeftEye = 0,
		eSwapChainArray_Scene3D_RightEye,
		eSwapChainArray_Quad_0,
		eSwapChainArray_Quad_1,

		eSwapChainArray_Total,

		eSwapChainArray_FirstQuad = eSwapChainArray_Quad_0,
		eSwapChainArray_LastQuad  = eSwapChainArray_Total - eSwapChainArray_Quad_0 + 1,
	};

	static RenderLayer::EQuadLayers CalculateQuadLayerId(ESwapChainArray swapChainIndex);

	#if (CRY_RENDERER_DIRECT3D >= 120) && defined(DX12_LINKEDADAPTER)
	void                            CopyMultiGPUFrameData();
	void CopyMultiGPUMirrorData(CTexture* pBackbufferTexture);
	#endif

private:

	// Helper to manage the layers in the Renderer
	// TODO: Depending on the layer future support by other vendors, this could become a separate system with its own interface
	struct SLayersManager
	{
		SLayersManager();

		inline static uint32               GetBufferIndex();
		void                               UpdateSwapChainData(CD3DStereoRenderer* pStereoRenderer, const STextureSwapChainRenderData* scene3DRenderData, const STextureSwapChainRenderData* quadRenderData);
		CryVR::Oculus::SHmdSubmitFrameData ConstructFrameData();

		enum { BUFFER_SIZE_LAYER_PROPERTIES = 1 }; // TO DO
		RenderLayer::CProperties         m_scene3DLayerProperties[RenderLayer::eSceneLayers_Total][BUFFER_SIZE_LAYER_PROPERTIES];
		RenderLayer::CProperties         m_quadLayerProperties[RenderLayer::eQuadLayers_Total][BUFFER_SIZE_LAYER_PROPERTIES];
		CryVR::Oculus::SHmdSwapChainInfo m_swapChainInfoArray[eSwapChainArray_Total];
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

	CStretchRectPass*             m_pStrechRectPass;
};

#endif //defined(INCLUDE_VR_RENDERING)
