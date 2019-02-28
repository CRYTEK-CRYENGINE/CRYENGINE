// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <../Interface/IHmdOculusRiftDevice.h>
#include <CrySystem/ISystem.h>
#include "OculusTouchController.h"

#include <CrySystem/vr/IHMDManager.h>

#include <atomic>

struct IConsoleCmdArgs;
struct IRenderer;

namespace CryVR {
namespace Oculus {

class Device : public IOculusDevice, public IHmdEventListener, public ISystemEventListener
{
public:
	// IHmdDevice interface
	virtual void                    AddRef() override;
	virtual void                    Release() override;

	virtual EHmdClass               GetClass() const override                         { return eHmdClass_Oculus; }
	virtual void                    GetDeviceInfo(HmdDeviceInfo& info) const override { info = m_devInfo; }

	virtual void                    GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const override;
	virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const;
	virtual void                    UpdateInternal(EInternalUpdate type) override;
	virtual void                    RecenterPose() override;
	virtual void                    UpdateTrackingState(EVRComponent type, uint64_t frameId) override;
	virtual const HmdTrackingState& GetNativeTrackingState() const override;
	virtual const HmdTrackingState& GetLocalTrackingState() const override;
	virtual Quad                    GetPlayArea() const override { return Quad(ZERO); }
	virtual Vec2                    GetPlayAreaSize() const override { return Vec2(ZERO); }
	virtual const IHmdController*   GetController() const override { return &m_controller; }
	virtual void                    DisableHMDTracking(bool disable) override;
	virtual void                    GetPreferredRenderResolution(unsigned int& width, unsigned int& height) override;
	virtual int                     GetControllerCount() const override;

	virtual stl::optional<Matrix34> RequestAsyncCameraUpdate(uint64_t frameId, const Quat& q, const Vec3 &p) override;
	// ~IHMDDevice interface

	// IOculusDevice interface
	// TODO: move into CRenderer, each implementation does it's own apropriate creation
	virtual bool CreateSwapTextureSetD3D12(IUnknown* d3d12CommandQueue, TextureDesc desc, STextureSwapChain* set) final;
	virtual bool CreateMirrorTextureD3D12(IUnknown* d3d12CommandQueue, TextureDesc desc, STexture* texture) final;
	virtual bool CreateSwapTextureSetD3D11(IUnknown* d3d11Device, TextureDesc desc, STextureSwapChain* set) final;
	virtual bool CreateMirrorTextureD3D11(IUnknown* d3d11Device, TextureDesc desc, STexture* texture) final;

	virtual void DestroySwapTextureSet(STextureSwapChain* set) override;
	virtual void DestroyMirrorTexture(STexture* texture) override;

	/* 
	 *	Rendering order: 
	 * 1. UpdateTrackingState() should be called from main thread with a unique frameid.
	 * 2. BeginFrame() should be called from render thread before submitting any rendering commands.
	 * 3. SubmitFrame() should be called from render thread to finiliaze a frame.
	 *
	 *	If the return is DeviceLost. Device needs to be destroyed and recreated along with all textures and swapchains.
	*/	
	virtual OculusStatus BeginFrame(uint64_t frameId) override;
	virtual OculusStatus SubmitFrame(const SHmdSubmitFrameData& data) override;

	virtual int  GetCurrentSwapChainIndex(void* pSwapChain) const override;

	virtual void CreateDevice() override;
	// ~IOculusDevice interface

	// IVRCmdListener
	virtual void OnRecentered() override;
	// ~IVRCmdListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

public:
	int            GetRefCount() const { return m_refCount; }
	bool           HasSession() const  { return m_pSession != 0; }

	ovrHmdDesc     GetDesc() const { return m_hmdDesc; }

	static Device* CreateInstance();

private:
	Device();
	virtual ~Device();

	void PrintHmdInfo();

private:
	struct SRenderParameters
	{
		ovrPosef         eyePoses[ovrEye_Count];
		ovrFovPort       eyeFovs[ovrEye_Count];
		ovrViewScaleDesc viewScaleDesc;
		double           sensorSampleTime;
		uint64_t         frameId;
	};

	struct SDeviceLayers
	{
		ovrLayerEyeFov eyeFov;
		ovrLayerQuad   quad[RenderLayer::eQuadLayers_Total];
	};

private:
	SRenderParameters& GetFrameRenderParameters();
	float              GetCurrentIPD() const;
	void               DebugDraw(float& xPosLabel, float& yPosLabel) const;
	bool               UpdateEyeFovLayer(ovrLayerEyeFov& layer, const SHmdSwapChainInfo* pEyeTarget, const SRenderParameters& frameParams);
	bool               UpdateQuadLayer(ovrLayerQuad& layer, const SHmdSwapChainInfo* pEyeTarget);
	void               InitLayers();
	void               CommitTextureSwapChain(const SHmdSwapChainInfo* pSwapChain);

private:
	volatile int     m_refCount;
	uint64_t         m_lastFrameID_UpdateTrackingState; // we could remove this. at some point we may want to sample more than once the tracking state per frame.
	int              m_perf_hud_info;
	bool             m_queueAhead;
	bool             m_bLoadingScreenActive;

	ovrSession       m_pSession; // opaque pointer to an OVR session
	ovrHmdDesc       m_hmdDesc;  // complete descriptor of the HMD
	HmdDeviceInfo    m_devInfo;

	HmdTrackingState m_nativeTrackingState;
	HmdTrackingState m_localTrackingState;
	HmdTrackingState m_disabledTrackingState;

	enum { BUFFER_SIZE_RENDER_PARAMS = 2 };
	SRenderParameters     m_frameRenderParams[BUFFER_SIZE_RENDER_PARAMS]; // double buffer params since write happens in main thread and read in render thread
	uint64_t              m_currentFrameId;

	ovrFovPort            m_eyeFovSym;

	ovrPosef              m_eyeRenderHmdToEyeOffset[ovrEye_Count];

	SDeviceLayers         m_layers;

	COculusController     m_controller;

	ovrSizei              m_preferredSize; //query & cache preferred texture size to map the texture/screen resolution 1:1

	bool                  m_disableHeadTracking;

	ICVar*                m_pHmdInfoCVar;
	ICVar*                m_pHmdSocialScreenKeepAspectCVar;
	ICVar*                m_pHmdSocialScreenCVar;
	ICVar*                m_pTrackingOriginCVar;
};

} // namespace Oculus
} // namespace CryVR