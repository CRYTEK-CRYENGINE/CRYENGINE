// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once
#ifdef INCLUDE_VR_RENDERING

	#include <CrySystem/VR/IHMDDevice.h>
	#include <../CryPlugins/VR/CryOSVR/Interface/IHmdOSVRDevice.h>
	#include <CryRenderer/IStereoRenderer.h>

class CD3D9Renderer;
class CTexture;
namespace CryVR
{
namespace Osvr
{
class CD3DOsvrRenderer : public IHmdRenderer
{
public:

	CD3DOsvrRenderer(IOsvrDevice* pDevice, CD3D9Renderer* pRenderer, CD3DStereoRenderer* pStereoRenderer);
	virtual ~CD3DOsvrRenderer();

	// IHDMRenderer implementation
	virtual bool                      Initialize(int initialWidth, int initialHeight) final;
	virtual void                      Shutdown() final;
	virtual void                      OnResolutionChanged(int newWidth, int newHeight) final;
	virtual void                      ReleaseBuffers() final;
	virtual void                      PrepareFrame() final;
	virtual void                      SubmitFrame() final;
	virtual void                      RenderSocialScreen() final;
	virtual RenderLayer::CProperties* GetQuadLayerProperties(RenderLayer::EQuadLayers id) final   { return nullptr; }
	virtual RenderLayer::CProperties* GetSceneLayerProperties(RenderLayer::ESceneLayers id) final { return nullptr; }
private:

	static const uint32 EyeCount = 2;

	struct EyeTextures
	{
		TArray<CTexture*> textures;
	};

	CryVR::Osvr::IOsvrDevice* m_pOsvrDevice;
	CD3D9Renderer*            m_pRenderer;
	CD3DStereoRenderer*       m_pStereoRenderer;

	uint32                    m_eyeWidth;
	uint32                    m_eyeHeight;

	EyeTextures               m_scene3DRenderData[EyeCount];

	int                       m_swapSetCount;
	int                       m_currentFrame;

	void CreateTextureSwapSets(uint32 width, uint32 height, uint32 swapSetCount);
	void ReleaseTextureSwapSets();

	void RestoreDeviceStateAfterFrameSubmit();

};
}
}
#endif
