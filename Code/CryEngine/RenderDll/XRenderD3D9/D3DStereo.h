// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STEREORENDERER_H
#define STEREORENDERER_H

#pragma once

#include <CryRenderer/IStereoRenderer.h>
#include <Common/Textures/Texture.h>
#include <Common/RenderDisplayContext.h>

#include <CryCore/optional.h>

class CD3D9Renderer;

//////////////////////////////////////////////////////////////////////////
// LIQUID VR
enum GpuMask
{
	GPUMASK_LEFT = 0x1,
	GPUMASK_RIGHT = 0x2,
	GPUMASK_BOTH = (GPUMASK_LEFT | GPUMASK_RIGHT)
};

#if defined(AMD_LIQUID_VR) && !CRY_RENDERER_OPENGL
#include <LiquidVR/public_mgpu/inc/AmdExtMgpuAppControl.h>
#include <LiquidVR/public_mgpu/inc/AmdDxExtMgpuAppControlApi.h>

extern IAmdDxExtMgpuAppControl* g_pMgpu;
#define SELECT_GPU(a)    { if (g_pMgpu) g_pMgpu->SetRenderGpuMask(a); }
#define SELECT_ALL_GPU() { if (g_pMgpu) g_pMgpu->SetRenderGpuMask(GPUMASK_BOTH); }
#elif defined(USE_NV_API)
extern void* g_pMgpu;
#define SELECT_GPU(a)
#define SELECT_ALL_GPU()
#else
extern void* g_pMgpu;
#define SELECT_GPU(a)
#define SELECT_ALL_GPU()
#endif

struct SStereoRenderContext
{
	int                     flags{ 0 };
	int                     frameId{ 0 };
	_smart_ptr<CRenderView> pRenderView;

	stl::optional<std::pair<Quat, Vec3>> orientationForLateCameraInjection;
};

class CStereoRenderingScope
{
	friend class CD3DStereoRenderer;

private:
	std::string          profileMarkerTag;
	SDisplayContextKey   displayContextKey;
	SDisplayContextKey   previousDisplayContextKey;

	CStereoRenderingScope(std::string &&profileMarkerTag, SDisplayContextKey displayContextKey);

public:
	~CStereoRenderingScope() noexcept;

	CStereoRenderingScope(CStereoRenderingScope&& o) = default;
	CStereoRenderingScope(const CStereoRenderingScope& o) = delete;
	CStereoRenderingScope &operator=(CStereoRenderingScope&& o) = default;
	CStereoRenderingScope &operator=(const CStereoRenderingScope& o) = delete;

	SDisplayContextKey GetDisplayContextKey() const { return displayContextKey; }

	explicit operator bool() const { return displayContextKey != SDisplayContextKey{}; }
};

class CD3DStereoRenderer : public IStereoRenderer
{
	friend class CD3DOpenVRRenderer;
	friend class CD3DOculusRenderer;
	friend class CD3DOsvrRenderer;

public:
	CD3DStereoRenderer();
	virtual ~CD3DStereoRenderer()
	{
		Shutdown();
	}

	void              InitDeviceBeforeD3D();
	void              InitDeviceAfterD3D();
	void              Shutdown();

	bool              IsQuadLayerEnabled() const { return m_pHmdRenderer != nullptr && m_device != EStereoDevice::STEREO_DEVICE_NONE && (m_mode == EStereoMode::STEREO_MODE_DUAL_RENDERING || m_mode == EStereoMode::STEREO_MODE_MENU) && m_output == EStereoOutput::STEREO_OUTPUT_HMD; }
	bool              IsStereoEnabled() const { return m_pHmdRenderer != nullptr && m_device != EStereoDevice::STEREO_DEVICE_NONE && m_mode != EStereoMode::STEREO_MODE_NO_STEREO; }
	bool              IsMenuModeEnabled() const override { return m_pHmdRenderer != nullptr && m_device != EStereoDevice::STEREO_DEVICE_NONE && m_mode == EStereoMode::STEREO_MODE_MENU; }
	bool              IsPostStereoEnabled() const { return m_pHmdRenderer != nullptr && m_device != EStereoDevice::STEREO_DEVICE_NONE && m_mode == EStereoMode::STEREO_MODE_POST_STEREO; }
	bool              RequiresSequentialSubmission() const { return m_submission == EStereoSubmission::STEREO_SUBMISSION_SEQUENTIAL; }

	void              PrepareStereo();
	EStereoMode       GetStereoMode() const { return m_mode; }
	EStereoSubmission GetStereoSubmissionMode() const { return m_submission; }

	EStereoOutput          GetStereoOutput() const { return m_output; }
	std::array<CCamera, 2> GetStereoCameras() const;
	const CCamera&         GetHeadLockedQuadCamera() const { return m_headlockedQuadCamera; }

	std::pair<CCustomRenderDisplayContext*, SDisplayContextKey> GetEyeDisplayContext(CCamera::EEye eEye) const
	{
		if (m_pEyeDisplayContexts[eEye].first)
			return { m_pEyeDisplayContexts[eEye].first.get(), m_pEyeDisplayContexts[eEye].second };
		return { nullptr,{} };
	}
	std::pair<CCustomRenderDisplayContext*, SDisplayContextKey> GetVrQuadLayerDisplayContext(RenderLayer::EQuadLayers id) const
	{
		if (m_pVrQuadLayerDisplayContexts[id].first)
			return { m_pVrQuadLayerDisplayContexts[id].first.get(), m_pVrQuadLayerDisplayContexts[id].second };
		return { nullptr,{} };
	}

	void Clear(ColorF clearColor) override
	{
		ClearEyes(clearColor);
		ClearVrQuads(clearColor);
	}
	void              ClearEyes(ColorF clearColor);
	void              ClearVrQuads(ColorF clearColor);

	void              ReleaseBuffers();
	void              OnResolutionChanged(int newWidth, int newHeight);
	void              CalculateResolution(int requestedWidth, int requestedHeight, int* pRenderWidth, int *pRenderHeight);
	void              OnStereoModeChanged();

	SStereoRenderContext PrepareStereoRenderingContext(int nFlags, const SRenderingPassInfo& passInfo) const;

	void              ProcessScene(int sceneFlags, const SRenderingPassInfo& passInfo);
	void              RenderScene(const SStereoRenderContext &context);

	void              NotifyFrameFinished();

	CStereoRenderingScope PrepareRenderingToEye(CCamera::EEye eye);
	CStereoRenderingScope PrepareRenderingToVrQuadLayer(RenderLayer::EQuadLayers id);

	bool              TakeScreenshot(const char path[]);

	float             GetNearGeoShift() const { return m_zeroParallaxPlaneDist; }
	float             GetNearGeoScale() const { return m_nearGeoScale; }
	float             GetGammaAdjustment() const { return m_gammaAdjustment; }

	_smart_ptr<CTexture> WrapD3DRenderTarget(D3DTexture* d3dTexture, uint32 width, uint32 height, DXGI_FORMAT format, const char* name, bool shaderResourceView);

public:
	// IStereoRenderer Interface
	EStereoDevice      GetDevice() const override final { return m_device; }
	EStereoDeviceState GetDeviceState() const override final { return m_deviceState; }
	void               GetInfo(EStereoDevice* device, EStereoMode* mode, EStereoOutput* output, EStereoDeviceState* state) const override final;

	void               ReleaseDevice() override final;

	bool               GetStereoEnabled() const override final { return IsStereoEnabled(); }
	float              GetStereoStrength() const override final { return m_stereoStrength; }
	float              GetMaxSeparationScene(bool half = true) const override final { return m_maxSeparationScene * (half ? 0.5f : 1.0f); }
	float              GetZeroParallaxPlaneDist() const override final { return m_zeroParallaxPlaneDist; }
	void               GetNVControlValues(bool& stereoEnabled, float& stereoStrength) const override final;

	void               PrepareFrame() override final;
	void               SubmitFrameToHMD() override final;
	void               DisplaySocialScreen() override final;

	void               ReleaseRenderResources() override final;

	// Hmd specific
	void               OnHmdDeviceChanged(IHmdDevice* pHmdDevice) override final;
	IHmdRenderer*      GetIHmdRenderer() const override final { return m_pHmdRenderer; }

	// This returns the size of the headlocked quad layers
	Vec2_tpl<int>      GetOverlayResolution() const override final { return { m_headlockedQuadCamera.GetViewSurfaceX(), m_headlockedQuadCamera.GetViewSurfaceZ() }; }
	// ~IStereoRenderer Interface

private:
	enum DriverType
	{
		DRIVER_UNKNOWN = 0,
		DRIVER_NV = 1,
		DRIVER_AMD = 2
	};

	EStereoDevice      m_device;
	EStereoDeviceState m_deviceState;
	EStereoMode        m_mode;
	EStereoOutput      m_output;
	EStereoSubmission  m_submission;

	DriverType         m_driver;

	// Stereo and VR render target display contexts
	std::array<std::pair<std::shared_ptr<CCustomRenderDisplayContext>, SDisplayContextKey>, eEyeType_NumEyes>               m_pEyeDisplayContexts;
	std::array<std::pair<std::shared_ptr<CCustomRenderDisplayContext>, SDisplayContextKey>, RenderLayer::eQuadLayers_Total> m_pVrQuadLayerDisplayContexts;

	std::array<CCamera, 2> m_previousCameras;
	CCamera                m_headlockedQuadCamera{};
	bool                   m_bPreviousCameraValid;

	void*         m_nvStereoHandle;
	float         m_nvStereoStrength;
	uint8         m_nvStereoActivated;

	float         m_stereoStrength;
	float         m_zeroParallaxPlaneDist;
	float         m_maxSeparationScene;
	float         m_nearGeoScale;
	float         m_gammaAdjustment;

	bool          m_frameRendered{ false };
	bool          m_resourcesPatched;
	bool          m_needClearVrQuadLayer;

	IHmdRenderer* m_pHmdRenderer;

	// Render social screen passes
	std::unique_ptr<CFullscreenPass> m_eyeToScreenPass;
	std::unique_ptr<CFullscreenPass> m_quadLayerPass;

private:
	void          SelectDefaultDevice();

	bool          EnableStereo();
	void          DisableStereo();
	void          ChangeOutputFormat();
	void          HandleNVControl();
	void          ShutdownHmdRenderer();

	CCamera       PrepareCamera(int nEye, const CCamera& currentCamera);

	bool          IsDriver(DriverType driver) { return m_device == EStereoDevice::STEREO_DEVICE_DRIVER && m_driver == driver; }

	IHmdRenderer* CreateHmdRenderer(struct IHmdDevice& device);

	// If HMD device present this will read latest tracking data.
	// This is called from the render thread
	stl::optional<Matrix34> TryGetHmdCameraAsync(const SStereoRenderContext &context);

	void          RecreateDisplayContext(std::pair<std::shared_ptr<CCustomRenderDisplayContext>, SDisplayContextKey> &target, const ColorF &clearColor, std::vector<_smart_ptr<CTexture>> &&swapChain);
	void          CreateEyeDisplayContext(CCamera::EEye eEye, std::vector<_smart_ptr<CTexture>> &&swapChain);
	void          CreateVrQuadLayerDisplayContext(RenderLayer::EQuadLayers id, std::vector<_smart_ptr<CTexture>> &&swapChain);
	void          SetCurrentEyeSwapChainIndices(const std::array<uint32_t, eEyeType_NumEyes> &indices);
	void          SetCurrentQuadLayerSwapChainIndices(const std::array<uint32_t, RenderLayer::eQuadLayers_Total> &indices);

	std::pair<EHmdSocialScreen, EHmdSocialScreenAspectMode> GetSocialScreenType() const;
	void                                                    RenderSocialScreen(CTexture* pColorTarget, const std::pair<EHmdSocialScreen, EHmdSocialScreenAspectMode> &socialScreenType, const Vec2i &displayResolutions);
};

#endif
