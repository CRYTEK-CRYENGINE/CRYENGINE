// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef STEREORENDERER_H
#define STEREORENDERER_H

#pragma once

#include <CryRenderer/IStereoRenderer.h>

class CD3D9Renderer;

//////////////////////////////////////////////////////////////////////////
// LIQUID VR
enum GpuMask
{
	GPUMASK_LEFT  = 0x1,
	GPUMASK_RIGHT = 0x2,
	GPUMASK_BOTH  = (GPUMASK_LEFT | GPUMASK_RIGHT)
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

enum StereoEye
{
	LEFT_EYE  = 0,
	RIGHT_EYE = 1,

	BOTH_EYES = LEFT_EYE
};

class CD3DStereoRenderer : public IStereoRenderer
{
public:
	CD3DStereoRenderer(CD3D9Renderer& renderer, EStereoDevice device);
	virtual ~CD3DStereoRenderer();

	void              InitDeviceBeforeD3D();
	void              InitDeviceAfterD3D();
	void              Shutdown();
	float             GetScreenDiagonalInInches() const     { return m_screenSize; }
	void              SetScreenDiagonalInInches(float size) { m_screenSize = size; }

	void              CreateResources();
	void              ReleaseResources();

	bool              IsQuadLayerEnabled() const           { return m_pHmdRenderer != nullptr && m_device != STEREO_DEVICE_NONE && m_mode == STEREO_MODE_DUAL_RENDERING && m_output == STEREO_OUTPUT_HMD && m_pVrQuadLayerTex[0]; }
	bool              IsStereoEnabled() const              { return m_device != STEREO_DEVICE_NONE && m_mode != STEREO_MODE_NO_STEREO; }
	bool              IsPostStereoEnabled() const          { return m_device != STEREO_DEVICE_NONE && m_mode == STEREO_MODE_POST_STEREO; }
	bool              RequiresSequentialSubmission() const { return m_submission == STEREO_SUBMISSION_SEQUENTIAL; }
	bool              DisplayStereoDone()                  { return m_bDisplayStereoDone; }

	void              PrepareStereo(EStereoMode mode, EStereoOutput output);

	EStereoDevice     GetStereoDevice() const                        { return m_device; }
	EStereoMode       GetStereoMode() const                          { return m_mode; }
	EStereoSubmission GetStereoSubmissionMode() const                { return m_submission; }

	EStereoOutput     GetStereoOutput() const                        { return m_output; }
	StereoEye         GetCurrentEye() const                          { return m_curEye; }

	CTexture*         GetEyeTarget(StereoEye eEye)                   { return m_pSideTexs[eEye]; }
	CTexture*         GetVrQuadLayerTex(RenderLayer::EQuadLayers id) { return m_pVrQuadLayerTex[id]; }

	void              SetEyeTextures(CTexture* leftTex, CTexture* rightTex);
	void              SetVrQuadLayerTexture(RenderLayer::EQuadLayers id, CTexture* pTexture);

	void              Update();
	void              ProcessScene(int sceneFlags, const SRenderingPassInfo& passInfo);
	void              ReleaseBuffers();
	void              OnResolutionChanged(int newWidth, int newHeight);
	void              CalculateResolution(int requestedWidth, int requestedHeight, int* pRenderWidth, int *pRenderHeight);

	void              SubmitFrameToHMD();
	void              DisplayStereo();

	void              BeginRenderingMRT(bool disableClear);
	void              EndRenderingMRT(bool bResolve = true);

	void              SkipEyeTargetClears() { m_needClearLeft = m_needClearRight = false; }
	void              NotifyFrameFinished();

	void              BeginRenderingTo(StereoEye eye);
	void              EndRenderingTo(StereoEye eye);
	void              BeginRenderingToVrQuadLayer(RenderLayer::EQuadLayers id);
	void              EndRenderingToVrQuadLayer(RenderLayer::EQuadLayers id);

	bool              TakeScreenshot(const char path[]);

	float             GetNearGeoShift()    { return m_zeroParallaxPlaneDist; }
	float             GetNearGeoScale()    { return m_nearGeoScale; }
	float             GetGammaAdjustment() { return m_gammaAdjustment; }

	// If HMD device present, will read latest tracking data and update current rendering camera.
	// This is called from the render thread
	void TryInjectHmdCameraAsync(CRenderView* pRenderView);

	CTexture* WrapD3DRenderTarget(D3DTexture* d3dTexture, uint32 width, uint32 height, DXGI_FORMAT format, const char* name, bool shaderResourceView);

public:
	// IStereoRenderer Interface
	virtual EStereoDevice      GetDevice();
	virtual EStereoDeviceState GetDeviceState();
	virtual void               GetInfo(EStereoDevice* device, EStereoMode* mode, EStereoOutput* output, EStereoDeviceState* state);

	virtual bool               GetStereoEnabled();
	virtual float              GetStereoStrength();
	virtual float              GetMaxSeparationScene(bool half = true);
	virtual float              GetZeroParallaxPlaneDist();
	virtual void               GetNVControlValues(bool& stereoEnabled, float& stereoStrength);

	// Hmd specific
	virtual void          OnHmdDeviceChanged(IHmdDevice* pHmdDevice);
	virtual IHmdRenderer* GetIHmdRenderer() { return m_pHmdRenderer; }
	// ~IStereoRenderer Interface

private:
	enum DriverType
	{
		DRIVER_UNKNOWN = 0,
		DRIVER_NV      = 1,
		DRIVER_AMD     = 2
	};

	CD3D9Renderer&     m_renderer;

	EStereoDevice      m_device;
	EStereoDeviceState m_deviceState;
	EStereoMode        m_mode;
	EStereoOutput      m_output;
	EStereoSubmission  m_submission;

	DriverType         m_driver;

	// Stereo and VR render target textures
	CTexture*     m_pVrQuadLayerTex[RenderLayer::eQuadLayers_Total];
	CTexture*     m_pSideTexs[2];

	std::array<CCamera, 2> m_previousCameras;
	bool                   m_bPreviousCameraValid;

	void*         m_nvStereoHandle;
	float         m_nvStereoStrength;
	uint8         m_nvStereoActivated;

	StereoEye     m_curEye;
	CCryNameR     m_SourceSizeParamName;

	float         m_stereoStrength;
	float         m_zeroParallaxPlaneDist;
	float         m_maxSeparationScene;
	float         m_nearGeoScale;
	float         m_gammaAdjustment;
	float         m_screenSize;

	bool          m_systemCursorNeedsUpdate;
	bool          m_resourcesPatched;
	bool          m_needClearLeft;
	bool          m_needClearRight;
	bool          m_needClearVrQuadLayer;
	bool          m_bDisplayStereoDone;

	IHmdRenderer* m_pHmdRenderer;
	IHmdDevice*   m_pHmdDevice; // "weak" pointer (no ref is added and this pointer should not be Release() or deleted)

	Matrix34      m_asyncCameraMatrix;
	bool          m_bAsyncCameraMatrixValid;

private:
	void          SelectDefaultDevice();

	void          CreateIntermediateBuffers(int nWidth, int nHeight);

	bool          EnableStereo();
	void          DisableStereo();
	void          ChangeOutputFormat();
	void          HandleNVControl();
	void          ShutdownHmdRenderer();

	void          RenderScene(int sceneFlags, const SRenderingPassInfo& passInfo);

	void          SelectShaderTechnique();

	bool          IsRenderThread() const;

	void          PushRenderTargets();
	void          PopRenderTargets(bool bResolve);

	CCamera       PrepareCamera(int nEye, const CCamera& currentCamera);

	bool          IsDriver(DriverType driver) { return m_device == STEREO_DEVICE_DRIVER && m_driver == driver; }

	IHmdRenderer* CreateHmdRenderer(struct IHmdDevice& device, CD3D9Renderer* pRenderer, CD3DStereoRenderer* pStereoRenderer);
};

#endif
