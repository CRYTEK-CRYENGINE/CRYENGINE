// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "HMDDeviceEmulator.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>
#include "CryRenderer/IRenderAuxGeom.h"

namespace
{
	Quat HmdQuatToWorldQuat(const Quat& quat)
	{
		Matrix33 m33(quat);
		Vec3 column1 = -quat.GetColumn2();
		m33.SetColumn2(m33.GetColumn1());
		m33.SetColumn1(column1);
		return Quat::CreateRotationX(gf_PI * 0.5f) * Quat(m33);
	}

	Vec3 HmdVec3ToWorldVec3(const Vec3& vec)
	{
		return Vec3(vec.x, -vec.z, vec.y);
	}

} // anonymous namespace

// -------------------------------------------------------------------------
namespace CryVR {
	namespace Emulator {

		// -------------------------------------------------------------------------
		Device::Device()
			: m_refCount(1)     //OculusResources.cpp assumes refcount is 1 on allocation
			, m_lastFrameID_UpdateTrackingState(-1)
			, m_devInfo()
			, m_disableHeadTracking(false)
		{
			m_devInfo.manufacturer = "Crytek";
			m_devInfo.productName = s_deviceName;
			m_devInfo.fovH = 100;
			m_devInfo.fovV = 100;
			m_devInfo.screenWidth = 0;
			m_devInfo.screenHeight = 0;
		}

		// -------------------------------------------------------------------------
		Device::~Device()
		{
			gEnv->pSystem->GetHmdManager()->RemoveEventListener(this);

			if (GetISystem()->GetISystemEventDispatcher())
				GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
		}

		// -------------------------------------------------------------------------
		void Device::AddRef()
		{
			CryInterlockedIncrement(&m_refCount);
		}

		// -------------------------------------------------------------------------
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

		// -------------------------------------------------------------------------
		Device* Device::CreateInstance()
		{
			Device* pDevice = new Device();
			if (pDevice)
			{
				pDevice->AddRef(); //add ref here to avoid warnings about ref being 0 when releaes called.
				//SAFE_RELEASE(pDevice);

				if (GetISystem()->GetISystemEventDispatcher())
					GetISystem()->GetISystemEventDispatcher()->RegisterListener(pDevice, "CryVR::Emulator::Device");
			}
			return pDevice;
		}

		// -------------------------------------------------------------------------
		void Device::GetPreferredRenderResolution(unsigned int& width, unsigned int& height)
		{
			auto rect = gEnv->pRenderer->GetDefaultContextWindowCoordinates();
			width = rect.w;
			height = rect.h;

			const EStereoOutput output = EStereoOutput(gEnv->pConsole->GetCVar("hmd_resolution_scale")->GetIVal());

			switch (output)
			{
			case EStereoOutput::STEREO_OUTPUT_SIDE_BY_SIDE:
			case EStereoOutput::STEREO_OUTPUT_HMD:
			{
				width = width / 2;
			}
			break;
			case EStereoOutput::STEREO_OUTPUT_ABOVE_AND_BELOW:
			{
				height = height / 2;
			}
			break;
			case EStereoOutput::STEREO_OUTPUT_LINE_BY_LINE:
			case EStereoOutput::STEREO_OUTPUT_CHECKERBOARD:
			case EStereoOutput::STEREO_OUTPUT_ANAGLYPH:
			default:
			{
			}
			}

		}
		// -------------------------------------------------------------------------
		void Device::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			switch (event)
			{
			case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
				break;

				// Intentional fall through
			case ESYSTEM_EVENT_LEVEL_LOAD_END:
			case ESYSTEM_EVENT_LEVEL_LOAD_ERROR:
				break;

			case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
				break;

			default:
				break;
			}
		}

		// -------------------------------------------------------------------------
		void Device::GetDeviceInfo(HmdDeviceInfo& info) const 
		{
			info = m_devInfo;
			auto rect = gEnv->pRenderer->GetDefaultContextWindowCoordinates();
			info.screenWidth = rect.w;
			info.screenHeight = rect.h;
		}

		// -------------------------------------------------------------------------
		void Device::GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const
		{
			fov = 0.5f * gf_PI;
			aspectRatioFactor = 2.0f;
		}

		// -------------------------------------------------------------------------
		HMDCameraSetup Device::GetHMDCameraSetup(int nEye, float projRatio, float fnear) const
		{

			struct EyeFov
			{
				float UpTan, DownTan, LeftTan, RightTan;
			};

			EyeFov defaultEyeFov[2] = 
			{
				{
					0.889498115,
					1.11092532,
					0.964925885,
					0.715263784
				},

				{
					0.889498115,
					1.11092532,
					0.715263784,
					0.964925885
				}
			};

			HMDCameraSetup ret = HMDCameraSetup::fromAsymmetricFrustum(
				defaultEyeFov[nEye].RightTan,
				defaultEyeFov[nEye].UpTan,
				defaultEyeFov[nEye].LeftTan,
				defaultEyeFov[nEye].DownTan,
				projRatio,
				fnear);

			ret.SetIPD(GetCurrentIPD());

			return ret;
		}

		// -------------------------------------------------------------------------
		void Device::RecenterPose()
		{
		}

		// -------------------------------------------------------------------------
		void Device::DisableHMDTracking(bool disable)
		{
			m_disableHeadTracking = disable;
		}

		// -------------------------------------------------------------------------
		void Device::UpdateTrackingState(EVRComponent type, uint64_t frameId)
		{
		}

		// -------------------------------------------------------------------------
		void Device::UpdateInternal(EInternalUpdate type)
		{
		}

		// -------------------------------------------------------------------------
		void Device::OnRecentered()
		{
			RecenterPose();
		}

		// -------------------------------------------------------------------------
		const HmdTrackingState& Device::GetNativeTrackingState() const
		{
			return !m_disableHeadTracking ? m_nativeTrackingState : m_disabledTrackingState;
		}

		// -------------------------------------------------------------------------
		const HmdTrackingState& Device::GetLocalTrackingState() const
		{
			return !m_disableHeadTracking ? m_localTrackingState : m_disabledTrackingState;
		}

		// -------------------------------------------------------------------------
		float Device::GetCurrentIPD() const
		{
			return 0.064f; // Return a default good value
		}

		// -------------------------------------------------------------------------
		void Device::DebugDraw(float& xPosLabel, float& yPosLabel) const
		{
			// Basic info
			const float yPos = yPosLabel, xPosData = xPosLabel, yDelta = 20.f;
			float y = yPos;
			const ColorF fColorLabel(1.0f, 1.0f, 1.0f, 1.0f);
			const ColorF fColorIdInfo(1.0f, 1.0f, 0.0f, 1.0f);
			const ColorF fColorInfo(0.0f, 1.0f, 0.0f, 1.0f);
			const ColorF fColorDataPose(0.0f, 1.0f, 1.0f, 1.0f);

			IRenderAuxText::Draw2dLabel(xPosLabel, y, 1.3f, fColorLabel, false, "Oculus Hmd Info:");
			y += yDelta;

			const Vec3 hmdPos = m_localTrackingState.pose.position;
			const Ang3 hmdRotAng(m_localTrackingState.pose.orientation);
			const Vec3 hmdRot(RAD2DEG(hmdRotAng));
			IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "HmdPos(LS):[%.f,%f,%f]", hmdPos.x, hmdPos.y, hmdPos.z);
			y += yDelta;
			IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "HmdRot(LS):[%.f,%f,%f] (PRY) degrees", hmdRot.x, hmdRot.y, hmdRot.z);
			y += yDelta;

			yPosLabel = y;
		}

		// -------------------------------------------------------------------------
		stl::optional<Matrix34> Device::RequestAsyncCameraUpdate(std::uint64_t frameId, const Quat& oq, const Vec3 &op)
		{
			const ICVar* pStereoScaleCoefficient = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_stereoScaleCoefficient") : 0;
			const float stereoScaleCoefficient = pStereoScaleCoefficient ? pStereoScaleCoefficient->GetFVal() : 1.f;

			HmdTrackingState nativeTrackingState;
			HmdTrackingState localTrackingState;

			Vec3 p = oq * localTrackingState.pose.position;
			Quat q = oq * localTrackingState.pose.orientation;

			Matrix34 viewMtx(q);
			viewMtx.SetTranslation(op + p);

			return viewMtx;
		}

	} // namespace Emulator
} // namespace CryVR