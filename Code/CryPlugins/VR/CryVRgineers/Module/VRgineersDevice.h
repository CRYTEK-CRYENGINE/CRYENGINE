#pragma once

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>

#include "IVRgineersDevice.h"

#include <Cry3DEngine/IIndexedMesh.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>

namespace CryVR
{
	namespace VRgineers
	{
		class Device final : public IVRgineersDevice, public IHmdEventListener
		{
		public:
			// IHmdDevice
			virtual void AddRef() override;

			virtual void Release() override;

			virtual EHmdClass GetClass() const override;

			virtual void GetDeviceInfo(HmdDeviceInfo& info) const override;

			virtual void GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const override;

			virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const override;

			virtual void UpdateInternal(EInternalUpdate type) override;

			virtual void RecenterPose() override;

			virtual void UpdateTrackingState(EVRComponent type, uint64_t frameId) override;

			virtual const HmdTrackingState& GetNativeTrackingState() const override;

			virtual const HmdTrackingState& GetLocalTrackingState() const override;

			virtual Quad GetPlayArea() const override;

			virtual Vec2 GetPlayAreaSize() const override;

			virtual const IHmdController* GetController() const override;

			virtual int GetControllerCount() const override;

			virtual void GetPreferredRenderResolution(unsigned int& width, unsigned int& height) override;

			virtual void DisableHMDTracking(bool disable) override;

			virtual stl::optional<Matrix34> RequestAsyncCameraUpdate(uint64_t frameId, const Quat& q, const Vec3 &p) override;
			// ~IHmdDevice

			// IVRgineersDevice
			virtual void GetRenderTargetSize(uint& w, uint& h) override;

			virtual bool InitRenderer(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext) override;

			virtual void OnSetupEyeTargets(ID3D11Texture2D* leftEyeHandle, ID3D11Texture2D* rightEyeHandle) override;

			virtual void OnPrepare() override;

			virtual void SubmitFrame(uint64 frameId) override;

			virtual void OnPostPresent() override;
			// ~IVRgineersDevice

			// IHmdEventListener
			virtual void OnRecentered() override;
			// ~IHmdEventListener

		public:
			int GetRefCount() const;

		public:
			static Device* CreateInstance();

		private:
			Device();

		public:
			virtual ~Device();

		private:			
			void PrintHmdInfo();

		private:
			static const int DEFAULT_VRG_DEVICE_ID;

			static const char* MANUFACTURER;

			static const char* PRODUCT_NAME;

		private:
			ID3D11Device* m_d3dDevice;

			ID3D11DeviceContext* m_d3dDeviceContext;

			ID3D11Texture2D* m_eyeTargets[EEyeType::eEyeType_NumEyes] = {};

			volatile int m_refCount = 1;

			HmdTrackingState m_hmdNativeTrackingState;

			HmdTrackingState m_cryTrackingState;

			HmdDeviceInfo m_deviceInfo;
		};
	}
}