// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OculusDevice.h"
#include "OculusResources.h"

#include "PluginDll.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>
#include "CryRenderer/IRenderAuxGeom.h"

#include <d3d11.h>
#include <d3d12.h>

#include <Extras/OVR_StereoProjection.h>

namespace
{
inline CryVR::Oculus::OculusStatus CheckOvrResult(ovrResult result) 
{
	if (OVR_SUCCESS(result))
		return CryVR::Oculus::OculusStatus::Success;

	// Unsuccessful
	ovrErrorInfo errorInfo;
	ovr_GetLastErrorInfo(&errorInfo);
	gEnv->pLog->Log("[HMD][Oculus] Unable to submit frame to the Oculus device!: [%s]", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");

	if (result == ovrError_DisplayLost)
		return CryVR::Oculus::OculusStatus::DeviceLost;
	return CryVR::Oculus::OculusStatus::UnknownError;
}

// Note: This symmetric fov is still used for frustum culling
ovrFovPort ComputeSymmetricalFov(ovrHmdType hmdType, const ovrFovPort& fovLeftEye, const ovrFovPort& fovRightEye)
{
	// CB aspect ratio = 1.8 (2160x1200), DK2 aspect ratio = 1.7777 (1920x1080)
	const float stereoDeviceAR =
		hmdType == ovrHmd_CB ? 1.8f :
		(hmdType >= ovrHmd_ES06 || hmdType == ovrHmd_Other) ? 1.8f :
		hmdType == ovrHmd_DK2 ? 1.7777f :
		1.7777f;   // not clear that we should support anything under DK2

	CryLog("[OculusDevice] Aspect Ratio selected: %f", stereoDeviceAR);
	ovrFovPort fovMax;
	fovMax.UpTan = max(fovLeftEye.UpTan, fovRightEye.UpTan);
	fovMax.DownTan = max(fovLeftEye.DownTan, fovRightEye.DownTan);
	fovMax.LeftTan = max(fovLeftEye.LeftTan, fovRightEye.LeftTan);
	fovMax.RightTan = max(fovLeftEye.RightTan, fovRightEye.RightTan);

	const float combinedTanHalfFovHorizontal = max(fovMax.LeftTan, fovMax.RightTan);
	const float combinedTanHalfFovVertical = max(fovMax.UpTan, fovMax.DownTan);

	ovrFovPort fovSym;
	fovSym.UpTan = fovSym.DownTan = combinedTanHalfFovVertical * 1.f;
	// DARIO_NOTE: This adapts the Horiz. Fov to the proper AR of Oculus. The 2.f factor is because the width of the eye textures is half the width of the backbuffer one
	fovSym.LeftTan = fovSym.RightTan = fovSym.UpTan / (2.f / stereoDeviceAR);

	CryLog("[OculusDevice] Fov: Up/Down tans [%f] Left/Right tans [%f]", fovSym.UpTan, fovSym.LeftTan);
	return fovSym;
}

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

void CopyPoseState(HmdPoseState& dst, HmdPoseState& worldDst, const ovrPoseStatef& src)
{
	dst.orientation = Quat(src.ThePose.Orientation.w, src.ThePose.Orientation.x, src.ThePose.Orientation.y, src.ThePose.Orientation.z);
	dst.position = Vec3(src.ThePose.Position.x, src.ThePose.Position.y, src.ThePose.Position.z);
	dst.angularVelocity = Vec3(src.AngularVelocity.x, src.AngularVelocity.y, src.AngularVelocity.z);
	dst.linearVelocity = Vec3(src.LinearVelocity.x, src.LinearVelocity.y, src.LinearVelocity.z);
	dst.angularAcceleration = Vec3(src.AngularAcceleration.x, src.AngularAcceleration.y, src.AngularAcceleration.z);
	dst.linearAcceleration = Vec3(src.LinearAcceleration.x, src.LinearAcceleration.y, src.LinearAcceleration.z);

	worldDst.orientation = HmdQuatToWorldQuat(dst.orientation);
	worldDst.position = HmdVec3ToWorldVec3(dst.position);
	worldDst.angularVelocity = HmdVec3ToWorldVec3(dst.angularVelocity);
	worldDst.linearVelocity = HmdVec3ToWorldVec3(dst.linearVelocity);
	worldDst.angularAcceleration = HmdVec3ToWorldVec3(dst.angularAcceleration);
	worldDst.linearAcceleration = HmdVec3ToWorldVec3(dst.linearAcceleration);
}

void LogHmdCharacteristics(const ovrHmdDesc& hmdDesc, const HmdDeviceInfo& devInfo, const CryVR::Oculus::COculusController& controller)
{
	CryLogAlways("[HMD][Oculus] - Device: %s - Type:%u", hmdDesc.ProductName ? hmdDesc.ProductName : "unknown", hmdDesc.Type);
	CryLogAlways("[HMD][Oculus] --- Manufacturer: %s", hmdDesc.Manufacturer ? hmdDesc.Manufacturer : "unknown");
	CryLogAlways("[HMD][Oculus] --- VendorId: %d", hmdDesc.VendorId);
	CryLogAlways("[HMD][Oculus] --- ProductId: %d", hmdDesc.ProductId);
	CryLogAlways("[HMD][Oculus] --- Firmware: %d.%d", hmdDesc.FirmwareMajor, hmdDesc.FirmwareMinor);
	CryLogAlways("[HMD][Oculus] --- SerialNumber: %s", hmdDesc.SerialNumber);
	CryLogAlways("[HMD][Oculus] - Resolution: %dx%d", hmdDesc.Resolution.w, hmdDesc.Resolution.h);
	CryLogAlways("[HMD][Oculus] - Horizontal FOV: %.2f degrees", devInfo.fovH * 180.0f / gf_PI);
	CryLogAlways("[HMD][Oculus] - Vertical FOV: %.2f degrees", devInfo.fovV * 180.0f / gf_PI);

	CryLogAlways("-[HMD][Oculus] Low persistence mode: Enabled by default");
	CryLogAlways("-[HMD][Oculus] Dynamic prediction: Enabled by default (for DK2 and later)");

	CryLogAlways("-[HMD][Oculus] Sensor orientation tracking: %s", (hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) ? "supported" : "unsupported");
	CryLogAlways("-[HMD][Oculus] Sensor mag yaw correction: %s", (hmdDesc.AvailableTrackingCaps & ovrTrackingCap_MagYawCorrection) ? "supported" : "unsupported");
	CryLogAlways("-[HMD][Oculus] Sensor position tracking: %s", (hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Position) ? "supported" : "unsupported");

	CryLogAlways("-[HMD][Oculus] Controller - Left Hand : %s", controller.IsConnected(eHmdController_OculusLeftHand) ? "Connected" : "Disconnected");
	CryLogAlways("-[HMD][Oculus] Controller - Right Hand: %s", controller.IsConnected(eHmdController_OculusRightHand) ? "Connected" : "Disconnected");
}

} // anonymous namespace

// -------------------------------------------------------------------------
namespace CryVR {
namespace Oculus {

// -------------------------------------------------------------------------
Device::Device()
	: m_refCount(1)     //OculusResources.cpp assumes refcount is 1 on allocation
	, m_lastFrameID_UpdateTrackingState(-1)
	, m_queueAhead(CPlugin_OculusVR::s_hmd_queue_ahead > 0)
	, m_perf_hud_info(CPlugin_OculusVR::s_hmd_perf_hud)
	, m_bLoadingScreenActive(false)
	, m_pSession(0)
	, m_hmdDesc()
	, m_devInfo()
	, m_disableHeadTracking(false)
{
	CreateDevice();
	gEnv->pSystem->GetHmdManager()->AddEventListener(this);
}

// -------------------------------------------------------------------------
Device::~Device()
{
	gEnv->pSystem->GetHmdManager()->RemoveEventListener(this);

	if (m_pSession)
	{
		if (GetISystem()->GetISystemEventDispatcher())
			GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

		ovr_Destroy(m_pSession);

		m_pSession = 0;
	}
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
	IF (refCount < 0, 0)
		__debugbreak();
	#endif
	IF (refCount == 0, 0)
	{
		delete this;
	}
}

// -------------------------------------------------------------------------
Device* Device::CreateInstance()
{
	Device* pDevice = new Device();
	if (pDevice && !pDevice->HasSession())
	{
		pDevice->AddRef(); //add ref here to avoid warnings about ref being 0 when releaes called.
		SAFE_RELEASE(pDevice);
	}
	return pDevice;
}

// -------------------------------------------------------------------------
void Device::CreateDevice()
{
	memset(&m_eyeFovSym, 0, sizeof(m_eyeFovSym));
	memset(&m_eyeRenderHmdToEyeOffset, 0, sizeof(m_eyeRenderHmdToEyeOffset));
	memset(&m_frameRenderParams, 0, sizeof(m_frameRenderParams));

	m_pHmdInfoCVar = gEnv->pConsole->GetCVar("hmd_info");
	m_pHmdSocialScreenKeepAspectCVar = gEnv->pConsole->GetCVar("hmd_social_screen_aspect_mode");
	m_pHmdSocialScreenCVar = gEnv->pConsole->GetCVar("hmd_social_screen");
	m_pTrackingOriginCVar = gEnv->pConsole->GetCVar("hmd_tracking_origin");

	if (m_pSession)
	{
		if (GetISystem()->GetISystemEventDispatcher())
			GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);

		ovr_Destroy(m_pSession);

		m_pSession = 0;
	}

	ovrGraphicsLuid luid;
	ovrResult result = ovr_Create(&m_pSession, &luid);
	if (m_pSession && OVR_SUCCESS(result))
	{
		m_hmdDesc = ovr_GetHmdDesc(m_pSession);
		
		if (m_hmdDesc.Type >= ovrHmd_ES06 || m_hmdDesc.Type == ovrHmd_Other)
		{
			m_controller.Init(m_pSession);
			CryLogAlways("[HMD][Oculus] Touch Controllers initialized.");
		}
		else
		{
			CryLogAlways("[HMD][Oculus] Touch Controllers were not initialized because the detected HMD does not support them. Device %d", m_hmdDesc.Type);
		}

		m_eyeFovSym = ComputeSymmetricalFov(m_hmdDesc.Type, m_hmdDesc.DefaultEyeFov[ovrEye_Left], m_hmdDesc.DefaultEyeFov[ovrEye_Right]);

		m_devInfo.productName = m_hmdDesc.ProductName;
		m_devInfo.manufacturer = m_hmdDesc.Manufacturer;
		m_devInfo.screenWidth = m_hmdDesc.Resolution.w;
		m_devInfo.screenHeight = m_hmdDesc.Resolution.h;
		m_devInfo.fovH = 2.0f * atanf(m_eyeFovSym.LeftTan);
		m_devInfo.fovV = 2.0f * atanf(m_eyeFovSym.UpTan);

		InitLayers();

		// Store eye info for IPD
		m_eyeRenderHmdToEyeOffset[0] = ovr_GetRenderDesc(m_pSession, ovrEye_Left, m_hmdDesc.DefaultEyeFov[0]).HmdToEyePose;
		m_eyeRenderHmdToEyeOffset[1] = ovr_GetRenderDesc(m_pSession, ovrEye_Right, m_hmdDesc.DefaultEyeFov[1]).HmdToEyePose;

		LogHmdCharacteristics(m_hmdDesc, m_devInfo, m_controller);

		ovrSizei leftEyeSize = ovr_GetFovTextureSize(m_pSession, ovrEye_Left, m_hmdDesc.DefaultEyeFov[ovrEye_Left], 1.f);
		ovrSizei rightEyeSize = ovr_GetFovTextureSize(m_pSession, ovrEye_Right, m_hmdDesc.DefaultEyeFov[ovrEye_Right], 1.f);

		m_preferredSize.w = max(leftEyeSize.w, rightEyeSize.w);
		m_preferredSize.h = max(leftEyeSize.h, rightEyeSize.h);

		ovr_SetTrackingOriginType(m_pSession, m_pTrackingOriginCVar->GetIVal() == static_cast<int>(EHmdTrackingOrigin::Standing) ? ovrTrackingOrigin_FloorLevel : ovrTrackingOrigin_EyeLevel);
	}
	else
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		CryLogAlways("[HMD][Oculus] Failed to initialize HMD device! [%s]", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
	}

	if (m_pSession && GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CryVR::Oculus::Device");
}

// -------------------------------------------------------------------------
void Device::GetPreferredRenderResolution(unsigned int& width, unsigned int& height)
{
	width = m_preferredSize.w;
	height = m_preferredSize.h;
}
// -------------------------------------------------------------------------
void Device::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
		m_bLoadingScreenActive = true;
		break;

	// Intentional fall through
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
	case ESYSTEM_EVENT_LEVEL_LOAD_ERROR:
		m_bLoadingScreenActive = false;
		m_controller.ClearControllerState(COculusController::eCV_FullController);
		break;

	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
		m_controller.ClearControllerState(COculusController::eCV_FullController);
		break;

	default:
		break;
	}
}

// -------------------------------------------------------------------------
void Device::GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const
{
	// Calculate a single camera with a fov that encapsulates the two eyes asymmetries
	if (m_pSession)
	{
		fov = m_devInfo.fovV;
		aspectRatioFactor = 2.0f * m_eyeFovSym.LeftTan;
	}
	else
	{
		CryLog("[HMD][Oculus] GetCameraSetupInfo returns invalid information since the HMD was not created");
		// Just return something valid
		fov = 0.5f * gf_PI;
		aspectRatioFactor = 2.0f;
	}
}

// -------------------------------------------------------------------------
HMDCameraSetup Device::GetHMDCameraSetup(int nEye, float projRatio, float fnear) const
{
	HMDCameraSetup ret = HMDCameraSetup::fromAsymmetricFrustum(
		m_hmdDesc.DefaultEyeFov[nEye].RightTan,
		m_hmdDesc.DefaultEyeFov[nEye].UpTan,
		m_hmdDesc.DefaultEyeFov[nEye].LeftTan,
		m_hmdDesc.DefaultEyeFov[nEye].DownTan,
		projRatio,
		fnear);
	ret.ipd = GetCurrentIPD();

	return ret;
}

// -------------------------------------------------------------------------
void Device::RecenterPose()
{
	if (m_pSession)
	{
		ovr_RecenterTrackingOrigin(m_pSession);
	}
}

// -------------------------------------------------------------------------
void Device::DisableHMDTracking(bool disable)
{
	m_disableHeadTracking = disable;
}

// -------------------------------------------------------------------------
void Device::UpdateTrackingState(EVRComponent type, uint64_t frameId)
{
	if (m_pSession)
	{
		// Allow update of performance hud at run-time
		if (m_perf_hud_info != CPlugin_OculusVR::s_hmd_perf_hud)
		{
			if (CPlugin_OculusVR::s_hmd_perf_hud >= 0 && CPlugin_OculusVR::s_hmd_perf_hud < ovrPerfHud_Count)
			{
				m_perf_hud_info = CPlugin_OculusVR::s_hmd_perf_hud;
				if (OVR_SUCCESS(ovr_SetInt(m_pSession, OVR_PERF_HUD_MODE, m_perf_hud_info)))
				{
					CryLog("[HMD][Oculus] OVR_PERF_HUD_MODE changed to %d", m_perf_hud_info);
				}
				else
				{
					CryLog("[HMD][Oculus] OVR_PERF_HUD_MODE failed to change to %d", m_perf_hud_info);
				}
			}
			else
			{
				m_perf_hud_info = CPlugin_OculusVR::s_hmd_perf_hud = ovrPerfHud_Off;
				CryLog("[HMD][Oculus] OVR_PERF_HUD_MODE passed a wrong value (it should be be between 0 and %d, see help). Setting the value to %d (off)", ovrPerfHud_Count, ovrPerfHud_Off);
			}
		}

		// Allow update Queue-Ahead at run-time
		const bool cvarQueueAhead = (CPlugin_OculusVR::s_hmd_queue_ahead > 0);
		if (m_queueAhead != cvarQueueAhead)
		{
			m_queueAhead = cvarQueueAhead;
			if (OVR_SUCCESS(ovr_SetBool(m_pSession, "QueueAheadEnabled", m_queueAhead)))
			{
				CryLog("[HMD][Oculus] QueueAheadEnabled changed to %s", m_queueAhead ? "TRUE" : "FALSE");
			}
			else
			{
				CryLog("[HMD][Oculus] QueueAheadEnabled changed failed trying to set the value to %s", m_queueAhead ? "TRUE" : "FALSE");
			}
		}

	#if !defined(_RELEASE)
		// Check we update things in the right thread at not too often

		if (!gEnv->IsEditor())// we currently assume one system update per frame rendered, which is not always the case in editor (i.e. no level)
		{
			if (((type & eVRComponent_Hmd) != 0) && (CryGetCurrentThreadId() != gEnv->mMainThreadId) && (m_bLoadingScreenActive == false))
			{
				gEnv->pLog->LogError("[HMD][Oculus] Device::UpdateTrackingState() should be called from main thread only!");
			}

			if (frameId && frameId == m_lastFrameID_UpdateTrackingState)
			{
				gEnv->pLog->LogError("[HMD][Oculus] Device::UpdateTrackingState() should be called only once per frame!");
			}
		}
		m_lastFrameID_UpdateTrackingState = frameId;
	#endif
		const bool bZeroEyeOffset = false;
		ovrPosef hmdToEyeOffset[2] = {};

		// See if user changed the tracking origin, and update in Oculus Runtime if necessary
		auto desiredTrackingOrigin = m_pTrackingOriginCVar->GetIVal() == static_cast<int>(EHmdTrackingOrigin::Standing) ? ovrTrackingOrigin_FloorLevel : ovrTrackingOrigin_EyeLevel;
		if(ovr_GetTrackingOriginType(m_pSession) != desiredTrackingOrigin)
		{
			ovr_SetTrackingOriginType(m_pSession, desiredTrackingOrigin);
		}

		// get the current tracking state
		const bool kbLatencyMarker = false;
		double frameTime = ovr_GetPredictedDisplayTime(m_pSession, frameId);
		ovrTrackingState trackingState = ovr_GetTrackingState(m_pSession, frameTime, kbLatencyMarker);

		ovrPosef eyePose[2];
		double sensorSampleTime;
		ovr_GetEyePoses(m_pSession, frameId, kbLatencyMarker, bZeroEyeOffset ? hmdToEyeOffset : m_eyeRenderHmdToEyeOffset, eyePose, &sensorSampleTime);

		const ICVar* pStereoScaleCoefficient = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_stereoScaleCoefficient") : 0;
		const float stereoScaleCoefficient = pStereoScaleCoefficient ? pStereoScaleCoefficient->GetFVal() : 1.f;

		CopyPoseState(m_nativeTrackingState.pose, m_localTrackingState.pose, trackingState.HeadPose);

		const int statusFlags =
		  ((trackingState.StatusFlags & ovrStatus_OrientationTracked) ? eHmdStatus_OrientationTracked : 0) |
		  ((trackingState.StatusFlags & ovrStatus_PositionTracked) ? eHmdStatus_PositionTracked : 0);

		// Dario : integration
		// Added DisplayLost and ShouldRecenter to ovr_GetSessionStatus

		// HMD-specific tracking update
		if ((type & eVRComponent_Hmd) != 0)
		{
			// scale pose based on r_stereoScaleCoefficient (this cvar also affects the eye distance)
			m_nativeTrackingState.pose.position *= stereoScaleCoefficient;
			m_localTrackingState.pose.position *= stereoScaleCoefficient;
			m_localTrackingState.statusFlags = m_nativeTrackingState.statusFlags = statusFlags;

			// store the parameters required for distortion rendering
			SRenderParameters& frameParams = GetFrameRenderParameters();
			for (uint32 eye = 0; eye < 2; ++eye)
			{
				frameParams.eyeFovs[eye] = m_hmdDesc.DefaultEyeFov[eye];
				frameParams.eyePoses[eye] = eyePose[eye];
				frameParams.viewScaleDesc.HmdToEyePose[eye] = hmdToEyeOffset[eye];
			}
			frameParams.sensorSampleTime = sensorSampleTime;
			frameParams.frameId = frameId;
			frameParams.viewScaleDesc.HmdSpaceToWorldScaleInMeters = stereoScaleCoefficient;
		}

		// Controller-specific tracking update
		if ((type & eVRComponent_Controller) != 0)
		{
			CopyPoseState(m_controller.m_state.m_handNativeTrackingState[eHmdController_OculusLeftHand].pose, m_controller.m_state.m_handLocalTrackingState[eHmdController_OculusLeftHand].pose, trackingState.HandPoses[ovrHand_Left]);
			CopyPoseState(m_controller.m_state.m_handNativeTrackingState[eHmdController_OculusRightHand].pose, m_controller.m_state.m_handLocalTrackingState[eHmdController_OculusRightHand].pose, trackingState.HandPoses[ovrHand_Right]);

			for (size_t i = 0; i < eHmdController_MaxNumOculusControllers; ++i)
			{
				m_controller.m_state.m_handNativeTrackingState[i].pose.position *= stereoScaleCoefficient;
				m_controller.m_state.m_handLocalTrackingState[i].pose.position *= stereoScaleCoefficient;

				m_controller.m_state.m_handNativeTrackingState[i].statusFlags = statusFlags;
				m_controller.m_state.m_handLocalTrackingState[i].statusFlags = statusFlags;
			}

			m_controller.Update();
			if (m_pHmdInfoCVar->GetIVal() != 0)
			{
				float xPos = 10.f, yPos = 10.f;
				DebugDraw(xPos, yPos);
				yPos += 25.f;
				m_controller.DebugDraw(xPos, yPos);
			}
		}
	}
}

// -------------------------------------------------------------------------
void Device::UpdateInternal(EInternalUpdate type)
{
	switch (type)
	{
	case IHmdDevice::eInternalUpdate_DebugInfo:
		{
			if (m_pHmdInfoCVar->GetIVal() != 0)
			{
				if (Device* pDevice = Resources::GetAssociatedDevice())
					pDevice->PrintHmdInfo();
			}
		}
		break;
	default:
		break;
	}
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
Device::SRenderParameters& Device::GetFrameRenderParameters()
{
	IRenderer* pRenderer = gEnv->pRenderer;
	const int frameID = pRenderer->GetFrameID(false);
	return m_frameRenderParams[frameID % BUFFER_SIZE_RENDER_PARAMS];
}

// -------------------------------------------------------------------------
float Device::GetCurrentIPD() const
{
	if (CPlugin_OculusVR::s_hmd_ipd > 0)
		return CPlugin_OculusVR::s_hmd_ipd;
	if (m_pSession)
	{
		ovrFovPort fov; // only used to get the IPD
		fov.UpTan = m_devInfo.fovV * 0.5f;
		fov.DownTan = m_devInfo.fovV * 0.5f;
		fov.LeftTan = m_devInfo.fovH * 0.5f;
		fov.RightTan = m_devInfo.fovH * 0.5f;

		ovrEyeRenderDesc desc = ovr_GetRenderDesc(m_pSession, ovrEye_Left, fov);
		const float ipd = fabs(desc.HmdToEyePose.Position.x * 2.f);

		// let's make sure we read some reasonable values
		if (ipd >= 0.0000f && ipd < 0.15f)
			return ipd;
	}

	return 0.064f; // Return a default good value
}

// -------------------------------------------------------------------------
void Device::PrintHmdInfo()
{
	// TODO
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

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorIdInfo, false, "Device:%s (%u)-Manufacturer:%s", m_hmdDesc.ProductName ? m_hmdDesc.ProductName : "unknown", m_hmdDesc.Type, m_hmdDesc.Manufacturer ? m_hmdDesc.Manufacturer : "unknown");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorIdInfo, false, "VendorId:%d-ProductId:%d-Firmware: %d.%d-SerialNumber: %s", m_hmdDesc.VendorId, m_hmdDesc.ProductId, m_hmdDesc.FirmwareMajor, m_hmdDesc.FirmwareMinor, m_hmdDesc.SerialNumber);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorInfo, false, "Resolution: %dx%d", m_hmdDesc.Resolution.w, m_hmdDesc.Resolution.h);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorInfo, false, "FOV (degrees): H:%.2f - V:%.2f", RAD2DEG(m_devInfo.fovH), RAD2DEG(m_devInfo.fovV));
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorInfo, false, "Caps: Rot tracking [%s], Mag Yaw correction [%s], Pos tracking [%s]",
	                       (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Orientation) ? "supported" : "unsupported",
	                       (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_MagYawCorrection) ? "supported" : "unsupported",
	                       (m_hmdDesc.AvailableTrackingCaps & ovrTrackingCap_Position) ? "supported" : "unsupported"
	                       );
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
// From SDK 0.6.0 we can only use Oculus SDK Rendering
// Hence the code below
// -------------------------------------------------------------------------
bool Device::CreateSwapTextureSetD3D12(IUnknown* d3d12CommandQueue, TextureDesc desc, STextureSwapChain* set)
{
	ovrTextureSwapChainDesc textureDesc = {};
	textureDesc.Type = ovrTexture_2D;
	textureDesc.ArraySize = 1;
	textureDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.MipLevels = 1;
	textureDesc.SampleCount = 1;
	textureDesc.StaticImage = ovrFalse;
	textureDesc.MiscFlags = ovrTextureMisc_DX_Typeless;
	textureDesc.BindFlags = ovrTextureBind_DX_RenderTarget;

	ovrTextureSwapChain textureSwapChain = nullptr;
	ovrResult result = ovr_CreateTextureSwapChainDX(
		m_pSession,
		d3d12CommandQueue,
		&textureDesc,
		&textureSwapChain);

	if (!OVR_SUCCESS(result) || textureSwapChain == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Unable to create D3D12 texture swap chain!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}

	int textureCount = 0;
	ovr_GetTextureSwapChainLength(m_pSession, textureSwapChain, &textureCount);

	set->pDeviceTextureSwapChain = textureSwapChain;
	set->pTextures = new IUnknown*[textureCount];
	for (uint32 t = 0; t < textureCount; ++t)
	{
		ID3D12Resource* pTex2D = nullptr;
		result = ovr_GetTextureSwapChainBufferDX(m_pSession, textureSwapChain, t, IID_PPV_ARGS(&pTex2D));
		set->pTextures[t] = pTex2D;

		if (!OVR_SUCCESS(result) || pTex2D == nullptr)
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo(&errorInfo);
			gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "CreateSwapTextureSetD3D12 failed in ovr_GetTextureSwapChainBufferDX %s", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error", pTex2D == nullptr ? " - tex = nullptr" : "");
			return false;
		}
	}
	set->numTextures = textureCount;
	return true;
}

bool Device::CreateMirrorTextureD3D12(IUnknown* d3d12CommandQueue, TextureDesc desc, STexture* texture)
{
	ovrMirrorTextureDesc mirrorDesc = {};
	mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	mirrorDesc.Width = desc.width;
	mirrorDesc.Height = desc.height;
	mirrorDesc.MiscFlags = ovrTextureMisc_DX_Typeless;

	ovrMirrorTexture mirrorTexture = nullptr;
	ovrResult result = ovr_CreateMirrorTextureDX(
		m_pSession,
		d3d12CommandQueue,
		&mirrorDesc,
		&mirrorTexture);

	if (!OVR_SUCCESS(result) || mirrorTexture == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Unable to create D3D12 mirror texture!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}

	texture->pDeviceTexture = mirrorTexture;

	ID3D12Resource* pTex2D = nullptr;
	result = ovr_GetMirrorTextureBufferDX(m_pSession, mirrorTexture, IID_PPV_ARGS(&pTex2D));
	if (!OVR_SUCCESS(result) || mirrorTexture == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Error: ovr_GetMirrorTextureBufferDX", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}
	texture->pTexture = pTex2D;

	return true;
}

// -------------------------------------------------------------------------
bool Device::CreateSwapTextureSetD3D11(IUnknown* d3d11Device, TextureDesc desc, STextureSwapChain* set)
{
	ovrTextureSwapChainDesc textureDesc = {};
	textureDesc.Type = ovrTexture_2D;
	textureDesc.ArraySize = 1;
	textureDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.MipLevels = 1;
	textureDesc.SampleCount = 1;
	textureDesc.StaticImage = ovrFalse;
	textureDesc.MiscFlags = ovrTextureMisc_DX_Typeless;
	textureDesc.BindFlags = ovrTextureBind_DX_RenderTarget;

	ovrTextureSwapChain textureSwapChain = nullptr;
	ovrResult result = ovr_CreateTextureSwapChainDX(
	  m_pSession,
	  static_cast<ID3D11Device*>(d3d11Device),
	  &textureDesc,
	  &textureSwapChain);

	if (!OVR_SUCCESS(result) || textureSwapChain == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Unable to create D3D11 texture swap chain!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}

	int textureCount = 0;
	ovr_GetTextureSwapChainLength(m_pSession, textureSwapChain, &textureCount);

	set->pDeviceTextureSwapChain = textureSwapChain;
	set->pTextures = new IUnknown*[textureCount];
	for (uint32 t = 0; t < textureCount; ++t)
	{
		ID3D11Texture2D* pTex2D = nullptr;
		result = ovr_GetTextureSwapChainBufferDX(m_pSession, textureSwapChain, t, IID_PPV_ARGS(&pTex2D));
		set->pTextures[t] = pTex2D;

		if (!OVR_SUCCESS(result) || pTex2D == nullptr)
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo(&errorInfo);
			gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "CreateSwapTextureSetD3D11 failed in ovr_GetTextureSwapChainBufferDX %s", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error", pTex2D == nullptr ? " - tex = nullptr" : "");
			return false;
		}
	}
	set->numTextures = textureCount;
	return true;
}

bool Device::CreateMirrorTextureD3D11(IUnknown* d3d11Device, TextureDesc desc, STexture* texture)
{
	ovrMirrorTextureDesc mirrorDesc = {};
	mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	mirrorDesc.Width = desc.width;
	mirrorDesc.Height = desc.height;
	mirrorDesc.MiscFlags = ovrTextureMisc_DX_Typeless;

	ovrMirrorTexture mirrorTexture = nullptr;
	ovrResult result = ovr_CreateMirrorTextureDX(
	  m_pSession,
	  d3d11Device,
	  &mirrorDesc,
	  &mirrorTexture);

	if (!OVR_SUCCESS(result) || mirrorTexture == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Unable to create D3D11 mirror texture!", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}

	texture->pDeviceTexture = mirrorTexture;

	ID3D11Texture2D* pTex2D = nullptr;
	result = ovr_GetMirrorTextureBufferDX(m_pSession, mirrorTexture, IID_PPV_ARGS(&pTex2D));
	if (!OVR_SUCCESS(result) || mirrorTexture == nullptr)
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		gEnv->pLog->Log("[HMD][Oculus] %s [%s]", "Error: ovr_GetMirrorTextureBufferDX", *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error");
		return false;
	}
	texture->pTexture = pTex2D;

	return true;
}

// -------------------------------------------------------------------------
void Device::DestroySwapTextureSet(STextureSwapChain* set)
{
	ovr_DestroyTextureSwapChain(m_pSession, static_cast<ovrTextureSwapChain>(set->pDeviceTextureSwapChain));
	SAFE_DELETE_ARRAY(set->pTextures);
}

// -------------------------------------------------------------------------
void Device::DestroyMirrorTexture(STexture* texture)
{
	ovr_DestroyMirrorTexture(m_pSession, static_cast<ovrMirrorTexture>(texture->pDeviceTexture));
	texture->pDeviceTexture = nullptr;
}

// -------------------------------------------------------------------------
bool Device::UpdateEyeFovLayer(ovrLayerEyeFov& layer, const SHmdSwapChainInfo* pEyeTarget, const SRenderParameters& frameParams)
{
	bool bUpdated = false;
	if (CPlugin_OculusVR::s_hmd_projection == eHmdProjection_Stereo /*&& !m_bLoadingScreenActive*/)
	{
		if (pEyeTarget && pEyeTarget->bActive)
		{
			const uint32 eye = static_cast<uint32_t>(pEyeTarget->eye);

			layer.ColorTexture[eye] = pEyeTarget->pDeviceTextureSwapChain;
			layer.Viewport[eye].Pos.x = pEyeTarget->viewportPosition.x;
			layer.Viewport[eye].Pos.y = pEyeTarget->viewportPosition.y;
			layer.Viewport[eye].Size.w = pEyeTarget->viewportSize.x;
			layer.Viewport[eye].Size.h = pEyeTarget->viewportSize.y;
			layer.Fov[eye] = frameParams.eyeFovs[eye];
			layer.RenderPose[eye] = frameParams.eyePoses[eye];
			layer.SensorSampleTime = frameParams.sensorSampleTime;

			bUpdated = true;
		}
	}
	return bUpdated;
}

// -------------------------------------------------------------------------
bool Device::UpdateQuadLayer(ovrLayerQuad& layer, const SHmdSwapChainInfo* pEyeTarget)
{
	bool bUpdated = false;

	if (pEyeTarget->bActive)
	{
		const bool bHeadLock = CPlugin_OculusVR::s_hmd_projection == eHmdProjection_Mono_HeadLocked || pEyeTarget->layerType == RenderLayer::eLayer_Quad_HeadLocked;
		layer.Header.Flags = bHeadLock ? ovrLayerFlag_HeadLocked : 0;

		layer.ColorTexture = pEyeTarget->pDeviceTextureSwapChain;

		layer.Viewport.Pos.x = pEyeTarget->viewportPosition.x;
		layer.Viewport.Pos.y = pEyeTarget->viewportPosition.y;
		layer.Viewport.Size.w = pEyeTarget->viewportSize.x;
		layer.Viewport.Size.h = pEyeTarget->viewportSize.y;

		layer.QuadSize.y = pEyeTarget->pose.s * (pEyeTarget->viewportSize.y / (float)pEyeTarget->viewportSize.x);
		layer.QuadSize.x = pEyeTarget->pose.s;

		layer.QuadPoseCenter.Position.x = pEyeTarget->pose.t.x;
		layer.QuadPoseCenter.Position.y = pEyeTarget->pose.t.y;
		layer.QuadPoseCenter.Position.z = CPlugin_OculusVR::s_hmd_projection_screen_dist > 0.f ? -CPlugin_OculusVR::s_hmd_projection_screen_dist : pEyeTarget->pose.t.z;
		layer.QuadPoseCenter.Orientation.x = pEyeTarget->pose.q.v.x;
		layer.QuadPoseCenter.Orientation.y = pEyeTarget->pose.q.v.y;
		layer.QuadPoseCenter.Orientation.z = pEyeTarget->pose.q.v.z;
		layer.QuadPoseCenter.Orientation.w = pEyeTarget->pose.q.w;

		bUpdated = true;
	}

	return bUpdated;
}

// -------------------------------------------------------------------------
void Device::InitLayers()
{
	memset(&m_layers, 0, sizeof(m_layers));

	// Scene3D layer
	m_layers.eyeFov.Header.Type = ovrLayerType_EyeFov;

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		m_layers.quad[i].Header.Type = ovrLayerType_Quad;
		m_layers.quad[i].QuadPoseCenter.Position.z = -1.f;
		m_layers.quad[i].QuadPoseCenter.Orientation.w = 1.f;
	}
}

// Tell Oculus runtime to update the Swap Chain Index
void Device::CommitTextureSwapChain(const SHmdSwapChainInfo* pSwapChain)
{
	if (pSwapChain)
	{
		const ovrResult result = ovr_CommitTextureSwapChain(m_pSession, pSwapChain->pDeviceTextureSwapChain);
		if (!OVR_SUCCESS(result))
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo(&errorInfo);
			gEnv->pLog->Log("[HMD][Oculus] %s [%s] SubmitFrame failed to commit CommitTextureSwapChain for layer index %u, type: %u",
			                *errorInfo.ErrorString ? errorInfo.ErrorString : "unspecified error", pSwapChain->layerId, pSwapChain->layerType);
		}
	}
}

OculusStatus Device::BeginFrame(uint64_t frameId)
{
	CRY_PROFILE_REGION(PROFILE_SYSTEM, "OculusDevice::BeginFrame()");

	const auto waitResult = CheckOvrResult(ovr_WaitToBeginFrame(m_pSession, frameId));
	const auto beginResult = CheckOvrResult(ovr_BeginFrame(m_pSession, frameId));
	
	m_currentFrameId = frameId;

	// Return the unsuccessful one, if any.
	return waitResult == CryVR::Oculus::OculusStatus::Success ? beginResult : waitResult;
}

OculusStatus Device::SubmitFrame(const SHmdSubmitFrameData &data)
{
	// Get the render parameters (as stored by the main thread in UpdateTrackingState)
	const SRenderParameters& frameParams = GetFrameRenderParameters();

	// Prepare a list of layers to be submitted
	const ovrLayerHeader* activeLayers[RenderLayer::eQuadLayers_Total + RenderLayer::eSceneLayers_Total] = { nullptr };

	// Keep track of the active layers
	uint32 numActiveLayers = 0;

	// Update Scene3D layer
	const SHmdSwapChainInfo* eyeScene3D[2] = { data.pLeftEyeScene3D, data.pRightEyeScene3D };
	uint32 cntActiveEyes = 0;
	for (uint32 i = 0; i < 2; ++i)
	{
		const SHmdSwapChainInfo* pSwapChain = eyeScene3D[i];
		if (CPlugin_OculusVR::s_hmd_projection == eHmdProjection_Stereo)
		{
			if (UpdateEyeFovLayer(m_layers.eyeFov, pSwapChain, frameParams))
			{
				CommitTextureSwapChain(pSwapChain);
				++cntActiveEyes;
			}
		}
		else
		{
			if (UpdateQuadLayer(m_layers.quad[RenderLayer::eQuadLayers_0], pSwapChain))
			{
				CommitTextureSwapChain(pSwapChain);
				activeLayers[numActiveLayers++] = &m_layers.quad[RenderLayer::eQuadLayers_0].Header;
				break;
			}
		}
	}

	if (cntActiveEyes == 2)
		activeLayers[numActiveLayers++] = &m_layers.eyeFov.Header;

	// Update Quad layers
	for (uint32 i = 0; i < data.numQuadLayersSwapChains; ++i)
	{
		const SHmdSwapChainInfo* pSwapChain = &data.pQuadLayersSwapChains[i];
		const RenderLayer::TLayerId layerId = pSwapChain->layerId;
		if (UpdateQuadLayer(m_layers.quad[layerId], pSwapChain))
		{
			CommitTextureSwapChain(pSwapChain);
			activeLayers[numActiveLayers++] = &m_layers.quad[layerId].Header;
		}
	}

	this->OnEndFrame();

	{
		CRY_PROFILE_REGION(PROFILE_SYSTEM, "OculusDevice::ovr_EndFrame");
		// Submit all active layers to Oculus runtime
		// (Is expected to be called after PrepareFrame() on same thread)
		return CheckOvrResult(ovr_EndFrame(m_pSession, m_currentFrameId, &frameParams.viewScaleDesc, activeLayers, numActiveLayers));
	}
}

int Device::GetCurrentSwapChainIndex(void* pSwapChain) const
{
	int index = -1;
	ovr_GetTextureSwapChainCurrentIndex(m_pSession, (ovrTextureSwapChain)pSwapChain, &index);

	return index;
}

int Device::GetControllerCount() const
{
	const uint32 cnt = m_controller.IsConnected(eHmdController_OculusLeftHand) ? 1 : 0;
	return m_controller.IsConnected(eHmdController_OculusRightHand) ? cnt + 1 : cnt;
}

stl::optional<Matrix34> Device::RequestAsyncCameraUpdate(std::uint64_t frameId, const Quat& oq, const Vec3 &op)
{
	const bool bZeroEyeOffset = false;
	ovrPosef hmdToEyeOffset[2] = {};

	// get the current tracking state
	const bool kbLatencyMarker = true;
	double frameTime = ovr_GetPredictedDisplayTime(m_pSession, static_cast<int64_t>(frameId));
	ovrTrackingState trackingState = ovr_GetTrackingState(m_pSession, frameTime, kbLatencyMarker);

	ovrPosef eyePose[2];
	double sensorSampleTime;
	ovr_GetEyePoses(m_pSession, static_cast<int64_t>(frameId), kbLatencyMarker, bZeroEyeOffset ? hmdToEyeOffset : m_eyeRenderHmdToEyeOffset, eyePose, &sensorSampleTime);

	const ICVar* pStereoScaleCoefficient = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_stereoScaleCoefficient") : 0;
	const float stereoScaleCoefficient = pStereoScaleCoefficient ? pStereoScaleCoefficient->GetFVal() : 1.f;

	HmdTrackingState nativeTrackingState;
	HmdTrackingState localTrackingState;
	CopyPoseState(nativeTrackingState.pose, localTrackingState.pose, trackingState.HeadPose);

	const int statusFlags =
	  ((trackingState.StatusFlags & ovrStatus_OrientationTracked) ? eHmdStatus_OrientationTracked : 0) |
	  ((trackingState.StatusFlags & ovrStatus_PositionTracked) ? eHmdStatus_PositionTracked : 0);

	Vec3 p = oq * localTrackingState.pose.position;
	Quat q = oq * localTrackingState.pose.orientation;

	Matrix34 viewMtx(q);
	viewMtx.SetTranslation(op + p);

	// HMD-specific tracking update
	{
		// scale pose based on r_stereoScaleCoefficient (this cvar also affects the eye distance)
		nativeTrackingState.pose.position *= stereoScaleCoefficient;
		localTrackingState.pose.position *= stereoScaleCoefficient;
		localTrackingState.statusFlags = nativeTrackingState.statusFlags = statusFlags;

		// store the parameters required for distortion rendering
		SRenderParameters& frameParams = GetFrameRenderParameters();
		for (uint32 eye = 0; eye < 2; ++eye)
		{
			frameParams.eyeFovs[eye] = m_hmdDesc.DefaultEyeFov[eye];
			frameParams.eyePoses[eye] = eyePose[eye];
			frameParams.viewScaleDesc.HmdToEyePose[eye] = hmdToEyeOffset[eye];
			frameParams.sensorSampleTime = sensorSampleTime;
			frameParams.frameId = frameId;
		}
		frameParams.viewScaleDesc.HmdSpaceToWorldScaleInMeters = stereoScaleCoefficient;
	}

	return viewMtx;
}

} // namespace Oculus
} // namespace CryVR