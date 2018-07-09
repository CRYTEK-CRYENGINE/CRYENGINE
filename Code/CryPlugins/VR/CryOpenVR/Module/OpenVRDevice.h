// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>

#include "../Interface/IHmdOpenVRDevice.h"
#include "OpenVRController.h"

#include <Cry3DEngine/IIndexedMesh.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>

struct IConsoleCmdArgs;
struct IRenderer;

namespace CryVR
{
namespace OpenVR
{
class Controller;
class Device final : public IOpenVRDevice, public IHmdEventListener, public ISystemEventListener
{
public:
	// IHmdDevice
	virtual void                    AddRef() override;
	virtual void                    Release() override;

	virtual EHmdClass               GetClass() const override                         { return eHmdClass_OpenVR; }
	virtual void                    GetDeviceInfo(HmdDeviceInfo& info) const override { info = m_devInfo; }

	virtual void                    GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const override;
	virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const override;
	virtual void                    UpdateInternal(EInternalUpdate type) override;
	virtual void                    RecenterPose() override;
	virtual void                    UpdateTrackingState(EVRComponent type, uint64_t frameId) override;
	virtual const HmdTrackingState& GetNativeTrackingState() const override;
	virtual const HmdTrackingState& GetLocalTrackingState() const override;
	virtual Quad                    GetPlayArea() const override;
	virtual Vec2                    GetPlayAreaSize() const override;
	virtual const IHmdController*   GetController() const override      { return &m_controller; }
	virtual int                     GetControllerCount() const override { __debugbreak(); return 2; /* OPENVR_TODO */ }
	virtual void                    GetPreferredRenderResolution(unsigned int& width, unsigned int& height) override;
	virtual void                    DisableHMDTracking(bool disable) override;

	virtual stl::optional<Matrix34> RequestAsyncCameraUpdate(uint64_t frameId, const Quat& q, const Vec3 &p) override;
	// ~IHmdDevice

	// IOpenVRDevice
	virtual void SubmitOverlay(int id, const RenderLayer::CProperties* pOverlayProperties) override;
	virtual void SubmitFrame() override;
	virtual void OnSetupEyeTargets(ERenderAPI api, ERenderColorSpace colorSpace, void* leftEyeHandle, void* rightEyeHandle) override;
	// Setup an overlay. Only one overlay can be "highest quality", should be set to the top most overlay which should be used for video/hud.
	virtual void OnSetupOverlay(int id, ERenderAPI api, ERenderColorSpace colorSpace, void* overlayTextureHandle, const QuatTS &pose, bool absolute, bool highestQuality = false) override;
	virtual void OnDeleteOverlay(int id) override;
	virtual void OnPrepare() override;
	virtual void OnPostPresent() override;
	virtual bool IsActiveOverlay(int id) const override;
	virtual void GetRenderTargetSize(uint& w, uint& h) override;
	virtual void GetMirrorImageView(EEyeType eye, void* resource, void** mirrorTextureView) override;

	void* GetD3D12EyeTextureData(EEyeType eye, ID3D12Resource *res, ID3D12CommandQueue *queue) override;
	void* GetD3D12QuadTextureData(int quad, ID3D12Resource *res, ID3D12CommandQueue *queue) override;
	// ~IOpenVRDevice

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	// IHmdEventListener
	virtual void OnRecentered() override;
	// ~IHmdEventListener

public:
	int GetRefCount() const { return m_refCount; }

public:
	static Device* CreateInstance(vr::IVRSystem* pSystem);
	void           SetupRenderModels();

private:
	Device(vr::IVRSystem* pSystem);
	virtual ~Device();

	void                   CreateDevice();
	void                   PrintHmdInfo();
	void                   DebugDraw(float& xPosLabel, float& yPosLabel) const;

	string                 GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::ETrackedDeviceProperty prop, vr::ETrackedPropertyError* peError = NULL);
	const char*            GetTrackedDeviceCharPointer(vr::TrackedDeviceIndex_t unDevice, vr::ETrackedDeviceProperty prop, vr::ETrackedPropertyError* peError = NULL);
	static inline Matrix34 BuildMatrix(const vr::HmdMatrix34_t& in);
	static inline Matrix44 BuildMatrix(const vr::HmdMatrix44_t& in);
	static inline Quat     HmdQuatToWorldQuat(const Quat& quat);
	static inline Vec3     HmdVec3ToWorldVec3(const Vec3& vec);
	inline void            CopyPoseState(HmdPoseState& world, HmdPoseState& hmd, const vr::TrackedDevicePose_t& source);
	void                   LoadDeviceRenderModel(int deviceIndex);
	void                   DumpDeviceRenderModel(int deviceIndex);

private:
	enum ERenderModelState
	{
		eRMS_Loading,
		eRMS_Loaded,
		eRMS_Failed
	};
	struct RenderModel
	{
		RenderModel(vr::IVRRenderModels* renderModels, string name);
		~RenderModel();
		void Update();
		bool IsValid()   { return m_modelState != eRMS_Failed && m_textureState != eRMS_Failed; }
		bool IsLoading() { return m_modelState == eRMS_Loading || m_textureState == eRMS_Loading; }

	private:
		string                        m_name;
		vr::IVRRenderModels*          m_renderModels;
		vr::RenderModel_t*            m_model;
		ERenderModelState             m_modelState;
		vr::RenderModel_TextureMap_t* m_texture;
		ERenderModelState             m_textureState;
	};
	struct SOverlay
	{
		bool                  visible;
		bool                  submitted;
		bool                  absolute;
		vr::VROverlayHandle_t handle;
		vr::Texture_t         vrTexture;
		vr::HmdMatrix34_t     pos;
	};
	// OpenVR Pointers
	vr::IVRSystem*          m_system;
	vr::IVRCompositor*      m_compositor;
	vr::IVRRenderModels*    m_renderModels;
	vr::IVROverlay*         m_overlay;

	vr::Texture_t           m_eyeTargets[EEyeType::eEyeType_NumEyes] = {};
	vr::D3D12TextureData_t  m_d3d12EyeTextureData[EEyeType::eEyeType_NumEyes] = {};
	SOverlay                m_overlays[RenderLayer::eQuadLayers_Total] = {};
	vr::D3D12TextureData_t  m_d3d12QuadTextureData[RenderLayer::eQuadLayers_Total] = {};

	// General device fields:
	bool                    m_bLoadingScreenActive;
	volatile int            m_refCount;
	uint64_t                m_lastFrameID_UpdateTrackingState; // we could remove this. at some point we may want to sample more than once the tracking state per frame.
	HmdDeviceInfo           m_devInfo;
	EHmdSocialScreen        m_defaultSocialScreenBehavior;
	// Tracking related:
	HmdTrackingState        m_nativeStates[vr::k_unMaxTrackedDeviceCount];
	HmdTrackingState        m_localStates[vr::k_unMaxTrackedDeviceCount];
	HmdTrackingState        m_disabledTrackingState;
	// Controller related:
	Controller              m_controller;
	RenderModel*            m_deviceModels[vr::k_unMaxTrackedDeviceCount];
	bool                    m_hmdTrackingDisabled;

	ICVar*                  m_pHmdInfoCVar;
	ICVar*                  m_pHmdSocialScreenKeepAspectCVar;
	ICVar*                  m_pHmdSocialScreenCVar;
	ICVar*                  m_pTrackingOriginCVar;

	vr::TrackedDevicePose_t m_predictedRenderPose[vr::k_unMaxTrackedDeviceCount];

	double m_submitTimeStamp;
	double m_poseTimestamp;
	double m_predictedRenderPoseTimestamp;

	// Parameters for symmetric camera, used for frustum culling
	float m_symLeftTan{ 0.0f };
};
} // namespace OpenVR
} // namespace CryVR
