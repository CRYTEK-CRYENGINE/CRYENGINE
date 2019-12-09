// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if defined(INCLUDE_VR_RENDERING)

#include <CrySystem/VR/IHMDDevice.h>
#include <../CryPlugins/VR/CryVRgineers/Interface/IVRgineersDevice.h>
#include <CryRenderer/IStereoRenderer.h>

class CD3D9Renderer;

class CD3DVRgineersRenderer : public IHmdRenderer
{
public:
	CD3DVRgineersRenderer(CryVR::VRgineers::IVRgineersDevice* vrgDevice, CD3D9Renderer* renderer, CD3DStereoRenderer* stereoRenderer);
	virtual ~CD3DVRgineersRenderer();

public:
	// IHDMRenderer
	virtual bool Initialize(int initialWidth, int initialHeight) final;
	virtual void Shutdown() final;
	virtual void OnResolutionChanged(int newWidth, int newHeight) final;
	virtual void ReleaseBuffers() final;
	virtual void PrepareFrame(uint64_t frameId) final;
	virtual void SubmitFrame() final;
	virtual void OnPostPresent() final;
	virtual RenderLayer::CProperties*  GetQuadLayerProperties(RenderLayer::EQuadLayers id) final;
	virtual RenderLayer::CProperties*  GetSceneLayerProperties(RenderLayer::ESceneLayers id) final;
	virtual std::pair<CTexture*, Vec4> GetMirrorTexture(EEyeType eye) const final;
	// ~IHDMRenderer

protected:
	struct Eye
	{
		_smart_ptr<CTexture> texture;
	};

	bool InitializeEyeTarget(D3DDevice* d3dDevice, const EEyeType eye, const CryVR::VRgineers::TextureDesc& desc, const char* name, _smart_ptr<CTexture>& outTexture);

protected:
	Eye m_scene3DRenderData[EEyeType::eEyeType_NumEyes];
	
	uint32 m_eyeWidth;
	uint32 m_eyeHeight;

	CryVR::VRgineers::IVRgineersDevice* m_pVRgineersDevice;

	CD3D9Renderer* m_pRenderer;

	CD3DStereoRenderer* m_pStereoRenderer;
};

#endif //defined(INCLUDE_VR_RENDERING)
