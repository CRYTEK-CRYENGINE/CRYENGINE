#include "VRgineersDevice.h"

#include "VRgineersUtils.h"

namespace CryVR
{
	namespace VRgineers
	{
		const int Device::DEFAULT_VRG_DEVICE_ID = 0;

		const char* Device::MANUFACTURER = "VRgineers, Inc.";

		const char* Device::PRODUCT_NAME = "VRG_DEVICE";

		// IHmdDevice
		void Device::AddRef()
		{
			CryInterlockedIncrement(&m_refCount);
		}

		void Device::Release()
		{
			long refCount = CryInterlockedDecrement(&m_refCount);
#if !defined(_RELEASE)
			IF(refCount < 0, 0)
				__debugbreak();
#endif
			IF(refCount == 0, 0)
			{
				delete this;
			}
		}

		EHmdClass Device::GetClass() const
		{
			return eHmdClass_VRgineers;
		}

		void Device::GetDeviceInfo(HmdDeviceInfo& info) const
		{
			info = m_deviceInfo;
		}

		void Device::GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const
		{
			fov = m_deviceInfo.fovH;
			aspectRatioFactor = float(m_deviceInfo.screenWidth) / float(m_deviceInfo.screenHeight);
		}

		HMDCameraSetup Device::GetHMDCameraSetup(int nEye, float projRatio, float fnear) const
		{
			float *leftProjMatrixArray, *rightProjMatrixArray;
			getDXProjectionMatricesLH(DEFAULT_VRG_DEVICE_ID, leftProjMatrixArray, rightProjMatrixArray);

			Matrix44 projectionMatrix = VRgineersUtils::CreateCryMatrix44FromFloatArray(nEye == CCamera::EEye::eEye_Left ? leftProjMatrixArray : rightProjMatrixArray);			
			Fov fov = VRgineersUtils::GetFovFromProjectionMatrix(projectionMatrix);
			
			Matrix44 newProjectionMatrix = VRgineersUtils::GetProjectionMatrixFromFov(fov, fnear, fnear * 10.0f);
			Frustum newFrustum = VRgineersUtils::GetFrustumFromProjectionMatrix(newProjectionMatrix);

			HMDCameraSetup cameraSetup = HMDCameraSetup::fromProjectionMatrix(newProjectionMatrix, projRatio, fnear);

			const float fullHorizontalExtents = std::fabsf(newFrustum.nearLeft) + std::fabsf(newFrustum.nearRight);
			const float halfHorizontalExtents = fullHorizontalExtents / 2.0f;

			const float fullVerticalExtents = std::fabsf(newFrustum.nearTop) + std::fabsf(newFrustum.nearBottom);
			const float halfVerticalExtents = fullVerticalExtents / 2.0f;

			cameraSetup.l = -(newFrustum.nearLeft + halfHorizontalExtents);
			cameraSetup.r = -(newFrustum.nearRight - halfHorizontalExtents);
			cameraSetup.b = -(newFrustum.nearBottom + halfVerticalExtents);
			cameraSetup.t = -(newFrustum.nearTop - halfVerticalExtents);

			float pdL, pdR;
			getHMDIPDinM(DEFAULT_VRG_DEVICE_ID, pdL, pdR);

			cameraSetup.SetIPD(pdL, pdR);

			return cameraSetup;
		}

		void Device::UpdateInternal(EInternalUpdate type)
		{
			// nothing to do here..
		}

		void Device::RecenterPose()
		{
			resetHMDIMU(DEFAULT_VRG_DEVICE_ID);
			setHMDPositionCalibration(DEFAULT_VRG_DEVICE_ID);
		}

		void Device::UpdateTrackingState(EVRComponent type, uint64_t frameId)
		{
			float qx, qy, qz, qw, x, y, z;
			getHMDPositionOrientation(DEFAULT_VRG_DEVICE_ID, x, y, z, qx, qy, qz, qw);

			m_hmdNativeTrackingState.pose.orientation = Quat(qw, -qy, -qz, qx).Normalize();
			m_hmdNativeTrackingState.pose.position = Vec3(y, z, -x);

			m_cryTrackingState.pose.orientation = VRgineersUtils::VRGOrientationToCry(m_hmdNativeTrackingState.pose.orientation);
			m_cryTrackingState.pose.position = VRgineersUtils::VRGPositionToCry(m_hmdNativeTrackingState.pose.position);

			switch (type)
			{
				case IHmdDevice::eInternalUpdate_DebugInfo:
					PrintHmdInfo();
					break;
				default:
					break;
			}
		}

		const HmdTrackingState& Device::GetNativeTrackingState() const
		{
			return m_hmdNativeTrackingState;
		}

		const HmdTrackingState& Device::GetLocalTrackingState() const
		{
			return m_cryTrackingState;
		}

		Quad Device::GetPlayArea() const
		{
			return Quad(type_zero::ZERO);
		}

		Vec2 Device::GetPlayAreaSize() const
		{
			return Vec2(type_zero::ZERO);
		}

		const IHmdController* Device::GetController() const
		{
			return nullptr;
		}

		int Device::GetControllerCount() const
		{
			return 0;
		}

		void Device::GetPreferredRenderResolution(unsigned int& width, unsigned int& height)
		{
			GetRenderTargetSize(width, height);
			width *= 2;
		}

		void Device::DisableHMDTracking(bool disable)
		{
			setHMDIMU(DEFAULT_VRG_DEVICE_ID, !disable);
		}

		stl::optional<Matrix34> Device::RequestAsyncCameraUpdate(uint64_t frameId, const Quat& q, const Vec3 &p)
		{
			return stl::optional<Matrix34>();
		}
		// ~IHmdDevice

		// IVRgineersDevice
		void Device::GetRenderTargetSize(uint& w, uint& h)
		{
			w = m_deviceInfo.screenWidth;
			h = m_deviceInfo.screenHeight;
		}

		bool Device::InitRenderer(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext)
		{
			m_d3dDevice = d3dDevice;
			m_d3dDeviceContext = d3dContext;

			int resultCode = attachHMDGfxDevice(DEFAULT_VRG_DEVICE_ID, API_DX11, m_d3dDevice);
			return resultCode == 0;
		}

		void Device::OnSetupEyeTargets(ID3D11Texture2D* leftEyeHandle, ID3D11Texture2D* rightEyeHandle)
		{
			m_eyeTargets[eEyeType_LeftEye] = leftEyeHandle;
			m_eyeTargets[eEyeType_RightEye] = rightEyeHandle;
		}

		void Device::OnPrepare()
		{
		}

		void Device::SubmitFrame(uint64 frameId)
		{
			drawHMDImageLR(DEFAULT_VRG_DEVICE_ID, m_eyeTargets[eEyeType_LeftEye], m_eyeTargets[eEyeType_RightEye]);
		}

		void Device::OnPostPresent()
		{
		}
		// ~IVRgineersDevice

		// IHmdEventListener
		void Device::OnRecentered()
		{
			m_cryTrackingState.pose.position = m_hmdNativeTrackingState.pose.position = Vec3(type_zero::ZERO);
			m_cryTrackingState.pose.orientation = m_hmdNativeTrackingState.pose.orientation = Quat(type_identity::IDENTITY);
		}
		// ~IHmdEventListener

		int Device::GetRefCount() const
		{
			return m_refCount;
		}

		Device* Device::CreateInstance()
		{
			// init hmd
			int initResult = initHMDDevice(DEFAULT_VRG_DEVICE_ID);
			if (initResult != 0)
			{
				gEnv->pLog->Log("[HMD][VRgineers] - Could not init HMD device: %d", DEFAULT_VRG_DEVICE_ID);
				return nullptr;
			}

			Device* device = new Device();

			// init tracking
			setHMDMenu(true);
			setHMDIMU(DEFAULT_VRG_DEVICE_ID, true);
			setHMDPositionCalibration(DEFAULT_VRG_DEVICE_ID);

			// init device Info
			int frameRate;
			unsigned int eyeScereenWidth, eyeScreenWHeight;
			getHMDResolutionFramerate(DEFAULT_VRG_DEVICE_ID, eyeScereenWidth, eyeScreenWHeight, frameRate);

			float *leftProjMatrixArray, *rightProjMatrixArray;
			getDXProjectionMatricesLH(DEFAULT_VRG_DEVICE_ID, leftProjMatrixArray, rightProjMatrixArray);
			Matrix44A projectionMatrixLeft = VRgineersUtils::CreateCryMatrix44FromFloatArray(leftProjMatrixArray);
			Fov fov = VRgineersUtils::GetFovFromProjectionMatrix(projectionMatrixLeft);

			device->m_deviceInfo.manufacturer = MANUFACTURER;
			device->m_deviceInfo.productName = PRODUCT_NAME;
			device->m_deviceInfo.screenWidth = eyeScereenWidth * 2;
			device->m_deviceInfo.screenHeight = eyeScreenWHeight;
			device->m_deviceInfo.fovH = std::fabsf(fov.left) + std::fabsf(fov.right);
			device->m_deviceInfo.fovV = std::fabsf(fov.top) + std::fabsf(fov.bottom);

			gEnv->pLog->Log("[HMD][VRgineers] - HMD %d was initialized", DEFAULT_VRG_DEVICE_ID);
			device->PrintHmdInfo();

			return device;
		}

		Device::Device()
		{
			// nothing to do here !?
		}

		Device::~Device()
		{
			setHMDMenu(false);
			setHMDIMU(DEFAULT_VRG_DEVICE_ID, false);

			freeHMDDevice(DEFAULT_VRG_DEVICE_ID);
		}

		void Device::PrintHmdInfo()
		{
			gEnv->pLog->Log("[HMD][VRgineers] - Device: %s", m_deviceInfo.productName);
			gEnv->pLog->Log("[HMD][VRgineers] --- Manufacturer: %s", m_deviceInfo.manufacturer);
			gEnv->pLog->Log("[HMD][VRgineers] --- Resolution: %dx%d", m_deviceInfo.screenWidth, m_deviceInfo.screenHeight);
		}
	}
}