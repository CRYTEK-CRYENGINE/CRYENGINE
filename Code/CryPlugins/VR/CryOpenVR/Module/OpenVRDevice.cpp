// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OpenVRDevice.h"
#include "OpenVRResources.h"
#include "OpenVRController.h"

#include "PluginDll.h"

#include <CrySystem/VR/IHMDManager.h>

#include <CryRenderer/IStereoRenderer.h>
#include <Cry3DEngine/IIndexedMesh.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Camera.h>

#include <CryCore/Platform/CryWindows.h>

// -------------------------------------------------------------------------
namespace vr
{
// -------------------------------------------------------------------------
static void SetIdentity(HmdMatrix34_t& in)
{
	memset(in.m, 0, sizeof(HmdMatrix34_t));
	in.m[0][0] = 1;
	in.m[1][1] = 1;
	in.m[2][2] = 1;
}

// -------------------------------------------------------------------------
static void SetZero(HmdVector3_t& in)
{
	memset(in.v, 0, sizeof(HmdVector3_t));
}

static Matrix34 RawConvert(HmdMatrix34_t in)
{
	return Matrix34(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
	                in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
	                in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3]);
}

static Matrix44 RawConvert(HmdMatrix44_t in)
{
	return Matrix44(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
	                in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
	                in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3],
	                in.m[3][0], in.m[3][1], in.m[3][2], in.m[3][3]);
}

static HmdMatrix34_t RawConvert(Matrix34 in)
{
	HmdMatrix34_t out;
	out.m[0][0] = in.m00;
	out.m[0][1] = in.m01;
	out.m[0][2] = in.m02;
	out.m[0][3] = in.m03;
	out.m[1][0] = in.m10;
	out.m[1][1] = in.m11;
	out.m[1][2] = in.m12;
	out.m[1][3] = in.m13;
	out.m[2][0] = in.m20;
	out.m[2][1] = in.m21;
	out.m[2][2] = in.m22;
	out.m[2][3] = in.m23;
	return out;
}

static HmdMatrix44_t RawConvert(Matrix44 in)
{
	HmdMatrix44_t out;
	out.m[0][0] = in.m00;
	out.m[0][1] = in.m01;
	out.m[0][2] = in.m02;
	out.m[0][3] = in.m03;
	out.m[1][0] = in.m10;
	out.m[1][1] = in.m11;
	out.m[1][2] = in.m12;
	out.m[1][3] = in.m13;
	out.m[2][0] = in.m20;
	out.m[2][1] = in.m21;
	out.m[2][2] = in.m22;
	out.m[2][3] = in.m23;
	out.m[3][0] = in.m30;
	out.m[3][1] = in.m31;
	out.m[3][2] = in.m32;
	out.m[3][3] = in.m33;
	return out;
}
}

namespace CryVR {
namespace OpenVR {
// -------------------------------------------------------------------------
Device::RenderModel::RenderModel(vr::IVRRenderModels* renderModels, string name)
	: m_renderModels(renderModels)
	, m_name(name)
	, m_model(nullptr)
	, m_texture(nullptr)
	, m_modelState(eRMS_Loading)
	, m_textureState(eRMS_Loading)
{

}

Device::RenderModel::~RenderModel()
{
	// Note: we currently do not use the render models, therefore we also do not set them up for rendering!
	/*vertices.clear();
	   normals.clear();
	   uvs.clear();
	   indices.clear();*/

	m_model = nullptr; // Object is managed by OpenVR runtime. Since it is possible, that the rendermodel is being accessed by something else, we DO NOT want to delete it
	m_modelState = eRMS_Failed;
	m_texture = nullptr; // Object is managed by OpenVR runtime. Since it is possible, that the texture is being accessed by something else, we DO NOT want to delete it
	m_textureState = eRMS_Failed;

	m_renderModels = nullptr;
}

void Device::RenderModel::Update()
{
	if (m_modelState == eRMS_Loading) // check model loading progress
	{
		vr::EVRRenderModelError result = m_renderModels->LoadRenderModel_Async(m_name.c_str(), &m_model);
		if (result == vr::VRRenderModelError_Loading)
		{
			// still loading
		}
		else if (result == vr::VRRenderModelError_None)
		{
			m_modelState = eRMS_Loaded;

			// Note: we currently do not use the render models, therefore we also do not set them up for rendering!
			/*vertices.reserve(m_model->unVertexCount);
			   normals.reserve(m_model->unVertexCount);
			   uvs.reserve(m_model->unVertexCount);
			   indices.reserve(m_model->unVertexCount);

			   for (int i = 0; i < m_model->unVertexCount; i++)
			   {
			   const vr::RenderModel_Vertex_t vrVert = m_model->rVertexData[i];
			   vertices.Add(Vec3(vrVert.vPosition.v[0], -vrVert.vPosition.v[2], vrVert.vPosition.v[1]));
			   normals.Add(Vec3(vrVert.vNormal.v[0], -vrVert.vNormal.v[2], vrVert.vNormal.v[1]));
			   uvs.Add(Vec2(vrVert.rfTextureCoord[0], vrVert.rfTextureCoord[1]));
			   indices.Add(m_model->rIndexData[i]);
			   }*/
		}
		else
		{
			m_modelState = eRMS_Loaded;
		}
	}

	if (m_modelState == eRMS_Loaded && m_textureState == eRMS_Loading) // check texture loading progress
	{
		vr::EVRRenderModelError result = m_renderModels->LoadTexture_Async(m_model->diffuseTextureId, &m_texture);
		if (result == vr::VRRenderModelError_Loading)
		{
			// still loading
		}
		else if (result == vr::VRRenderModelError_None)
		{
			m_textureState = eRMS_Loaded;

			// TODO: Setup engine texture

			gEnv->pLog->Log("[HMD][OpenVR] Device render model loaded: %s", m_name.c_str());
		}
		else
		{
			m_textureState = eRMS_Failed;
		}
	}
}

static ICVar* pParallax = nullptr;
// -------------------------------------------------------------------------
Device::Device(vr::IVRSystem* pSystem)
	: m_refCount(1)     //OpenVRResources.cpp assumes refcount is 1 on allocation
	, m_system(pSystem)
	, m_controller(pSystem)
	, m_lastFrameID_UpdateTrackingState(-1)
	, m_devInfo()
	, m_bLoadingScreenActive(false)
	, m_hmdTrackingDisabled(false)
	, m_submitTimeStamp(0)
	, m_poseTimestamp(0)
	, m_predictedRenderPoseTimestamp(0)
{
	CreateDevice();

	gEnv->pSystem->GetHmdManager()->AddEventListener(this);

	pParallax = gEnv->pConsole->GetCVar("sys_flash_stereo_maxparallax");

	if (GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CryVR::OpenVR::Device");

	m_controller.Init();

	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
		if (m_system->GetTrackedDeviceClass(i) == vr::TrackedDeviceClass_Controller)
			m_controller.OnControllerConnect(i);

	m_pHmdInfoCVar = gEnv->pConsole->GetCVar("hmd_info");
	m_pHmdSocialScreenKeepAspectCVar = gEnv->pConsole->GetCVar("hmd_social_screen_aspect_mode");
	m_pHmdSocialScreenCVar = gEnv->pConsole->GetCVar("hmd_social_screen");
	m_pTrackingOriginCVar = gEnv->pConsole->GetCVar("hmd_tracking_origin");

	ZeroArray(m_deviceModels);
	ZeroArray(m_predictedRenderPose);
}

// -------------------------------------------------------------------------
Device::~Device()
{
	gEnv->pSystem->GetHmdManager()->RemoveEventListener(this);

	if (GetISystem()->GetISystemEventDispatcher())
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

// -------------------------------------------------------------------------
string Device::GetTrackedDeviceString(vr::TrackedDeviceIndex_t unDevice, vr::ETrackedDeviceProperty prop, vr::ETrackedPropertyError* peError)
{
	uint32_t unRequiredBufferLen = m_system->GetStringTrackedDeviceProperty(unDevice, prop, nullptr, 0, peError);
	if (unRequiredBufferLen == 0)
		return "";

	char* pchBuffer = new char[unRequiredBufferLen];
	unRequiredBufferLen = m_system->GetStringTrackedDeviceProperty(unDevice, prop, pchBuffer, unRequiredBufferLen, peError);
	string result = string(pchBuffer, unRequiredBufferLen);
	delete[] pchBuffer;
	return result;
}

// -------------------------------------------------------------------------
const char* Device::GetTrackedDeviceCharPointer(vr::TrackedDeviceIndex_t unDevice, vr::ETrackedDeviceProperty prop, vr::ETrackedPropertyError* peError)
{
	uint32_t unRequiredBufferLen = m_system->GetStringTrackedDeviceProperty(unDevice, prop, nullptr, 0, peError);
	if (unRequiredBufferLen == 0)
		return nullptr;

	char* pBuffer = new char[unRequiredBufferLen];
	m_system->GetStringTrackedDeviceProperty(unDevice, prop, pBuffer, unRequiredBufferLen, peError);
	return const_cast<char*>(pBuffer);
}

// -------------------------------------------------------------------------
Matrix34 Device::BuildMatrix(const vr::HmdMatrix34_t& in)
{
	return Matrix34(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
	                in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
	                in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3]);
}

// -------------------------------------------------------------------------
Matrix44 Device::BuildMatrix(const vr::HmdMatrix44_t& in)
{
	return Matrix44(in.m[0][0], in.m[0][1], in.m[0][2], in.m[0][3],
	                in.m[1][0], in.m[1][1], in.m[1][2], in.m[1][3],
	                in.m[2][0], in.m[2][1], in.m[2][2], in.m[2][3],
	                in.m[3][0], in.m[3][1], in.m[3][2], in.m[3][3]);
}

// -------------------------------------------------------------------------
Quat Device::HmdQuatToWorldQuat(const Quat& quat)
{
	Matrix33 m33(quat);
	Vec3 column1 = -quat.GetColumn2();
	m33.SetColumn2(m33.GetColumn1());
	m33.SetColumn1(column1);
	return Quat::CreateRotationX(gf_PI * 0.5f) * Quat(m33);
}

// -------------------------------------------------------------------------
Vec3 Device::HmdVec3ToWorldVec3(const Vec3& vec)
{
	return Vec3(vec.x, -vec.z, vec.y);
}

// -------------------------------------------------------------------------
void Device::CopyPoseState(HmdPoseState& world, HmdPoseState& hmd, const vr::TrackedDevicePose_t& src)
{
	Matrix34 m = BuildMatrix(src.mDeviceToAbsoluteTracking);

	hmd.position = m.GetTranslation();
	hmd.orientation = Quat(m);
	hmd.linearVelocity = Vec3(src.vVelocity.v[0], src.vVelocity.v[1], src.vVelocity.v[2]);
	hmd.angularVelocity = Vec3(src.vAngularVelocity.v[0], src.vAngularVelocity.v[1], src.vAngularVelocity.v[2]);

	world.position = HmdVec3ToWorldVec3(hmd.position);
	world.orientation = HmdQuatToWorldQuat(hmd.orientation);
	world.linearVelocity = HmdVec3ToWorldVec3(hmd.linearVelocity);
	world.angularVelocity = HmdVec3ToWorldVec3(hmd.angularVelocity);
}

// -------------------------------------------------------------------------
void Device::SetupRenderModels()
{
	gEnv->pLog->Log("[HMD][OpenVR] SetupRenderModels");
	for (int dev = vr::k_unTrackedDeviceIndex_Hmd + 1; dev < vr::k_unMaxTrackedDeviceCount; dev++)
	{
		if (!m_system->IsTrackedDeviceConnected(dev))
			continue;

		LoadDeviceRenderModel(dev);
	}
}

// -------------------------------------------------------------------------
void Device::LoadDeviceRenderModel(int deviceIndex)
{
	if (!m_renderModels)
		return;

	vr::ETrackedPropertyError eError = vr::ETrackedPropertyError::TrackedProp_Success;
	string strModel = GetTrackedDeviceString(deviceIndex, vr::ETrackedDeviceProperty::Prop_RenderModelName_String, &eError);
	if (eError != vr::ETrackedPropertyError::TrackedProp_Success)
	{
		gEnv->pLog->Log("[HMD][OpenVR] Could not load device render model: %s", m_system->GetPropErrorNameFromEnum(eError));
		return;
	}

	m_deviceModels[deviceIndex] = new RenderModel(m_renderModels, strModel.c_str());
	gEnv->pLog->Log("[HMD][OpenVR] Device render model loading: %s", strModel.c_str());

}

// -------------------------------------------------------------------------
void Device::DumpDeviceRenderModel(int deviceIndex)
{
	SAFE_DELETE(m_deviceModels[deviceIndex]);
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
void Device::GetPreferredRenderResolution(unsigned int& width, unsigned int& height)
{
	GetRenderTargetSize(width, height);
	// Convert to full screen size, render target is per eye
	width *= 2;
}

// -------------------------------------------------------------------------
void Device::DisableHMDTracking(bool disable)
{
	m_hmdTrackingDisabled = disable;
}

// -------------------------------------------------------------------------
Device* Device::CreateInstance(vr::IVRSystem* pSystem)
{
	return new Device(pSystem);
}

// -------------------------------------------------------------------------
// Camera setup for frustum culling
void Device::GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const
{
	fov = m_devInfo.fovV;
	aspectRatioFactor = 2.0f * m_symLeftTan;
}

// -------------------------------------------------------------------------
HMDCameraSetup Device::GetHMDCameraSetup(int nEye, float projRatio, float fnear) const
{
	const vr::HmdMatrix44_t vrproj = m_system->GetProjectionMatrix((vr::EVREye)nEye, fnear, fnear*2.f);	// We do not care about far
	Matrix44A proj;
	std::memcpy(proj.GetData(), vrproj.m, sizeof(vrproj.m));

	HMDCameraSetup ret = HMDCameraSetup::fromProjectionMatrix(proj, projRatio, fnear);
	ret.ipd = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_UserIpdMeters_Float, nullptr);

	return ret;


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
		m_controller.ClearState();
		break;

	case ESYSTEM_EVENT_LEVEL_GAMEPLAY_START:
		m_controller.ClearState();
		break;

	default:
		break;
	}
}

// -------------------------------------------------------------------------
void Device::UpdateTrackingState(EVRComponent type, uint64_t frameID)
{
	IRenderer* pRenderer = gEnv->pRenderer;

#if !defined(_RELEASE)
	if (!gEnv->IsEditor())// we currently assume one system update per frame rendered, which is not always the case in editor (i.e. no level)
	{
		if (((type & eVRComponent_Hmd) != 0) && (CryGetCurrentThreadId() != gEnv->mMainThreadId) && (m_bLoadingScreenActive == false))
		{
			gEnv->pLog->LogError("[HMD][OpenVR] Device::UpdateTrackingState() should be called from main thread only!");
		}

		if (frameID == m_lastFrameID_UpdateTrackingState)
		{
			gEnv->pLog->LogError("[HMD][OpenVR] Device::UpdateTrackingState() should be called only once per frame!");
		}
	}
	m_lastFrameID_UpdateTrackingState = frameID;
#endif

	vr::Compositor_FrameTiming ft;
	ft.m_nSize = sizeof(vr::Compositor_FrameTiming);
	m_compositor->GetFrameTiming(&ft, 0);

	float rFreq = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_DisplayFrequency_Float);
	if (rFreq < 1 || !NumberValid(rFreq)) // in case we can't get a proper refresh rate, we just assume 90 fps, to avoid crashes
		rFreq = 90.0f;
	double fFrameDuration = 1.0f / rFreq;
	double elapsedTime = (ft.m_flSystemTimeInSeconds - m_submitTimeStamp);
	static double maxFrame = -10000000000000.f, minFrame = 100000000000.f;
	if (elapsedTime < 1000 && elapsedTime * 1000 >= 1)
	{
		minFrame = min(elapsedTime, minFrame);
		maxFrame = max(elapsedTime, maxFrame);
	}
	uint64 numFrames = clamp_tpl(2llu, 10llu, (uint64)(0.5 + elapsedTime / fFrameDuration));
	double fSecondsUntilPhotons = (fFrameDuration*numFrames) - (ft.m_flPresentCallCpuMs / 1000.0f) + m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_SecondsFromVsyncToPhotons_Float);

	const auto trackingOrigin = m_pTrackingOriginCVar->GetIVal() == (int)EHmdTrackingOrigin::Standing ? vr::ETrackingUniverseOrigin::TrackingUniverseStanding : vr::ETrackingUniverseOrigin::TrackingUniverseSeated;

	vr::TrackedDevicePose_t trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
	m_system->GetDeviceToAbsoluteTrackingPose(trackingOrigin, (float)fSecondsUntilPhotons, trackedDevicePose, vr::k_unMaxTrackedDeviceCount);

	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
	{
		const auto trackedStatus = trackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid ? (eHmdStatus_OrientationTracked | eHmdStatus_PositionTracked | eHmdStatus_CameraPoseTracked) : 0;
		const auto connectedStatus = m_system->IsTrackedDeviceConnected(vr::k_unTrackedDeviceIndex_Hmd) ? (eHmdStatus_PositionConnected | eHmdStatus_HmdConnected) : 0;
		m_localStates[i].statusFlags = m_nativeStates[i].statusFlags = trackedStatus | connectedStatus;

		if (trackedDevicePose[i].bPoseIsValid && trackedDevicePose[i].bDeviceIsConnected)
		{
			CopyPoseState(m_localStates[i].pose, m_nativeStates[i].pose, trackedDevicePose[i]);

			vr::ETrackedDeviceClass devClass = m_system->GetTrackedDeviceClass(i);
			switch (devClass)
			{
			case vr::ETrackedDeviceClass::TrackedDeviceClass_Controller:
				{
					vr::VRControllerState_t state;
					if (m_system->GetControllerState(i, &state, sizeof(vr::VRControllerState_t)))
					{
						m_controller.Update(i, m_nativeStates[i], m_localStates[i], state);
					}
				}
				break;
			}
		}
	}

	if (m_pHmdInfoCVar->GetIVal() > 0)
	{
		float xPos = 10.f, yPos = 10.f;
		DebugDraw(xPos, yPos);
		yPos += 25.f;
		m_controller.DebugDraw(xPos, yPos);
	}
}

// -------------------------------------------------------------------------
void Device::UpdateInternal(EInternalUpdate type)
{
	// vr event handling
	vr::VREvent_t event;
	while (m_system->PollNextEvent(&event, sizeof(vr::VREvent_t)))
	{
		switch (event.eventType)
		{
		case vr::VREvent_TrackedDeviceActivated:
			{
				if (m_refCount)
					LoadDeviceRenderModel(event.trackedDeviceIndex);
				vr::ETrackedDeviceClass devClass = m_system->GetTrackedDeviceClass(event.trackedDeviceIndex);
				switch (devClass)
				{
				case vr::ETrackedDeviceClass::TrackedDeviceClass_Controller:
					{
						gEnv->pLog->Log("[HMD][OpenVR] - Controller connected:", event.trackedDeviceIndex);
						gEnv->pLog->Log("[HMD][OpenVR] --- Tracked Device Index: %i", event.trackedDeviceIndex);
						gEnv->pLog->Log("[HMD][OpenVR] --- Attached Device Id: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_AttachedDeviceId_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Tracking System Name: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_TrackingSystemName_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Model Number: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_ModelNumber_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Serial Number: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_SerialNumber_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Manufacteurer: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Tracking Firmware Version: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_TrackingFirmwareVersion_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Hardware Revision: %s", GetTrackedDeviceCharPointer(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_HardwareRevision_String));
						gEnv->pLog->Log("[HMD][OpenVR] --- Is Wireless: %s", (m_system->GetBoolTrackedDeviceProperty(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_DeviceIsWireless_Bool) ? "True" : "False"));
						gEnv->pLog->Log("[HMD][OpenVR] --- Is Charging: %s", (m_system->GetBoolTrackedDeviceProperty(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_DeviceIsCharging_Bool) ? "True" : "False"));
						//gEnv->pLog->Log("[HMD][OpenVR] --- Battery: %f%%", (m_system->GetFloatTrackedDeviceProperty(event.trackedDeviceIndex, vr::ETrackedDeviceProperty::Prop_DeviceBatteryPercentage_Float) * 100.0f)); <-- was test implementation in OpenVR | Values not available (anymore/yet?)!

						m_controller.OnControllerConnect(event.trackedDeviceIndex);
					}
					break;
				}
			}
			break;
		case vr::VREvent_TrackedDeviceDeactivated:
			{
				if (m_renderModels)
					DumpDeviceRenderModel(event.trackedDeviceIndex);

				vr::ETrackedDeviceClass devClass = m_system->GetTrackedDeviceClass(event.trackedDeviceIndex);
				switch (devClass)
				{
				case vr::ETrackedDeviceClass::TrackedDeviceClass_Controller:
					{
						gEnv->pLog->Log("[HMD][OpenVR] - Controller disconnected: %i", event.trackedDeviceIndex);

						m_controller.OnControllerDisconnect(event.trackedDeviceIndex);
					}
					break;
				}
			}
			break;
		}
	}

	switch (type)
	{
	case IHmdDevice::eInternalUpdate_DebugInfo:
		{
			if (m_pHmdInfoCVar->GetIVal() != 0)
				if (Device* pDevice = Resources::GetAssociatedDevice())
					pDevice->PrintHmdInfo();
		}
		break;
	default:
		break;
	}

	// Render model loading
	for (int i = 0; i < vr::k_unMaxTrackedDeviceCount; i++)
	{
		if (!m_deviceModels[i])
			continue;

		if (!m_deviceModels[i]->IsValid())
		{
			DumpDeviceRenderModel(i);
		}
		else if (m_deviceModels[i]->IsLoading())
		{
			m_deviceModels[i]->Update();
		}
	}
}

// -------------------------------------------------------------------------
void Device::CreateDevice()
{
	vr::EVRInitError eError = vr::EVRInitError::VRInitError_None;
	m_compositor = (vr::IVRCompositor*)vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &eError);
	if (eError != vr::EVRInitError::VRInitError_None)
	{
		m_compositor = nullptr;
		gEnv->pLog->Log("[HMD][OpenVR] Could not create compositor: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		return;
	}

	m_renderModels = (vr::IVRRenderModels*)vr::VR_GetGenericInterface(vr::IVRRenderModels_Version, &eError);
	if (eError != vr::EVRInitError::VRInitError_None)
	{
		m_renderModels = nullptr;
		gEnv->pLog->Log("[HMD][OpenVR] Could not create render models: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		// let's not exit here, because we can live without render models!
	}

	m_overlay = (vr::IVROverlay*)vr::VR_GetGenericInterface(vr::IVROverlay_Version, &eError);
	if (eError != vr::EVRInitError::VRInitError_None)
	{
		m_overlay = nullptr;
		gEnv->pLog->Log("[HMD][OpenVR] Could not create overlay: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
	}

	// The previous calls to get the compositor window resolution were (temporarily?) dropped from the OpenVR SDK, therefore we report the suggested render resolution as Hmd screen resolution - even though that is NOT correct in the case of the HTC Vive!!!
	GetPreferredRenderResolution(m_devInfo.screenWidth, m_devInfo.screenHeight);
	const float stereoDeviceAspectRatio = static_cast<float>(m_devInfo.screenWidth) * .5f / static_cast<float>(m_devInfo.screenHeight);

	// Setup asymmetric camera properties
	const float fNear = GetISystem()->GetViewCamera().GetNearPlane();
	const float fFar = GetISystem()->GetViewCamera().GetFarPlane();
	
	vr::HmdMatrix44_t projL = m_system->GetProjectionMatrix(vr::EVREye::Eye_Left, fNear, fFar);
	vr::HmdMatrix44_t projR = m_system->GetProjectionMatrix(vr::EVREye::Eye_Right, fNear, fFar);
	const float upTanL = 1.0f / projL.m[1][1];
	const float upTanR = 1.0f / projR.m[1][1];

	// Setup symmetric camera (used for frutum culling) properties
	const float symTanUp = std::max(upTanL, upTanR);

	m_symLeftTan = stereoDeviceAspectRatio * (symTanUp / 2.f);
	m_devInfo.fovH = 2.0f * atanf(m_symLeftTan);
	m_devInfo.fovV = 2.0f * atanf(symTanUp);

	vr::IVRSettings* vrSettings = vr::VRSettings();

	m_devInfo.manufacturer = GetTrackedDeviceCharPointer(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_ManufacturerName_String);
	m_devInfo.productName = GetTrackedDeviceCharPointer(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_TrackingSystemName_String);

	gEnv->pLog->Log("[HMD][OpenVR] - Device: %s", m_devInfo.productName);
	gEnv->pLog->Log("[HMD][OpenVR] --- Manufacturer: %s", m_devInfo.manufacturer);
	gEnv->pLog->Log("[HMD][OpenVR] --- SerialNumber: %s", GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_SerialNumber_String).c_str());
	gEnv->pLog->Log("[HMD][OpenVR] --- Firmware: %s", GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_TrackingFirmwareVersion_String).c_str());
	gEnv->pLog->Log("[HMD][OpenVR] --- Hardware Revision: %s", GetTrackedDeviceString(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_HardwareRevision_String).c_str());
	gEnv->pLog->Log("[HMD][OpenVR] --- Display Firmware: %d", m_system->GetUint64TrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_DisplayFirmwareVersion_Uint64));
	gEnv->pLog->Log("[HMD][OpenVR] --- Resolution: %dx%d", m_devInfo.screenWidth, m_devInfo.screenHeight);
	gEnv->pLog->Log("[HMD][OpenVR] --- Refresh Rate: %.2f", m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::ETrackedDeviceProperty::Prop_DisplayFrequency_Float));
	gEnv->pLog->Log("[HMD][OpenVR] --- FOVH: %.2f", RAD2DEG(m_devInfo.fovH));
	gEnv->pLog->Log("[HMD][OpenVR] --- FOVV: %.2f", RAD2DEG(m_devInfo.fovV));

#if CRY_PLATFORM_WINDOWS
	// the following is (hopefully just) a (temporary) hack to shift focus back to the CryEngine window, after (potentially) spawning the SteamVR Compositor
	if (!gEnv->IsEditor())
	{
		LockSetForegroundWindow(LSFW_UNLOCK);
		SetForegroundWindow((HWND)gEnv->pSystem->GetHWND());
	}
#endif
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

	IRenderAuxText::Draw2dLabel(xPosLabel, y, 1.3f, fColorLabel, false, "OpenVR HMD Info:");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorIdInfo, false, "Device:%ss", m_devInfo.productName ? m_devInfo.productName : "unknown");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorIdInfo, false, "Manufacturer:%s", m_devInfo.manufacturer ? m_devInfo.manufacturer : "unknown");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorInfo, false, "Resolution: %dx%d", m_devInfo.screenWidth, m_devInfo.screenHeight);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorInfo, false, "FOV (degrees): H:%.2f - V:%.2f", m_devInfo.fovH * 180.0f / gf_PI, m_devInfo.fovV * 180.0f / gf_PI);
	y += yDelta;

	const Vec3 hmdPos = m_localStates[vr::k_unTrackedDeviceIndex_Hmd].pose.position;
	const Ang3 hmdRotAng(m_localStates[vr::k_unTrackedDeviceIndex_Hmd].pose.orientation);
	const Vec3 hmdRot(RAD2DEG(hmdRotAng));
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "HmdPos(LS):[%.f,%f,%f]", hmdPos.x, hmdPos.y, hmdPos.z);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "HmdRot(LS):[%.f,%f,%f] (PRY) degrees", hmdRot.x, hmdRot.y, hmdRot.z);
	y += yDelta;

	yPosLabel = y;
}

// -------------------------------------------------------------------------
void Device::OnRecentered()
{
	RecenterPose();
}

// -------------------------------------------------------------------------
void Device::RecenterPose()
{
	m_system->ResetSeatedZeroPose();
}

// -------------------------------------------------------------------------
const HmdTrackingState& Device::GetLocalTrackingState() const
{
	return m_hmdTrackingDisabled ? m_disabledTrackingState : m_localStates[vr::k_unTrackedDeviceIndex_Hmd];
}

// -------------------------------------------------------------------------
Quad Device::GetPlayArea() const
{
	if (auto* pChaperone = vr::VRChaperone())
	{
		vr::HmdQuad_t hmdQuad;
		if (pChaperone->GetPlayAreaRect(&hmdQuad))
		{
			Quad result;
			result.vCorners[0] = HmdVec3ToWorldVec3(Vec3(hmdQuad.vCorners[0].v[0], hmdQuad.vCorners[0].v[1], hmdQuad.vCorners[0].v[2]));
			result.vCorners[1] = HmdVec3ToWorldVec3(Vec3(hmdQuad.vCorners[1].v[0], hmdQuad.vCorners[1].v[1], hmdQuad.vCorners[1].v[2]));
			result.vCorners[2] = HmdVec3ToWorldVec3(Vec3(hmdQuad.vCorners[2].v[0], hmdQuad.vCorners[2].v[1], hmdQuad.vCorners[2].v[2]));
			result.vCorners[3] = HmdVec3ToWorldVec3(Vec3(hmdQuad.vCorners[3].v[0], hmdQuad.vCorners[3].v[1], hmdQuad.vCorners[3].v[2]));
			return result;
		}
	}

	return Quad(ZERO);
}

// -------------------------------------------------------------------------
Vec2 Device::GetPlayAreaSize() const
{
	if (auto* pChaperone = vr::VRChaperone())
	{
		Vec2 result;
		if (pChaperone->GetPlayAreaSize(&result.x, &result.y))
		{
			return result;
		}
	}

	return Vec2(ZERO);
}

// -------------------------------------------------------------------------
const HmdTrackingState& Device::GetNativeTrackingState() const
{
	return m_hmdTrackingDisabled ? m_disabledTrackingState : m_nativeStates[vr::k_unTrackedDeviceIndex_Hmd];
}

// -------------------------------------------------------------------------
void Device::PrintHmdInfo()
{
	// nada
}

// -------------------------------------------------------------------------
void Device::SubmitOverlay(int id, const RenderLayer::CProperties* pOverlayProperties)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (!m_overlay)
		return;

	CRY_ASSERT(pOverlayProperties->IsActive());
	if (pOverlayProperties->IsActive())
	{
		m_overlay->SetOverlayTexture(m_overlays[id].handle, &m_overlays[id].vrTexture);
		m_overlays[id].submitted = true;
	}
}

// -------------------------------------------------------------------------
void Device::SubmitFrame()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (m_compositor)
	{
		vr::EVRCompositorError leftSubmitResult = m_compositor->Submit(vr::Eye_Left, &m_eyeTargets[EEyeType::eEyeType_LeftEye]);
		vr::EVRCompositorError rightSubmitResult = m_compositor->Submit(vr::Eye_Right, &m_eyeTargets[EEyeType::eEyeType_RightEye]);

		if (leftSubmitResult != vr::EVRCompositorError::VRCompositorError_None || 
			rightSubmitResult != vr::EVRCompositorError::VRCompositorError_None)
		{
			gEnv->pLog->Log("[HMD][OpenVR] Unable to submit frame to the OpenVR device!: Left [%d]. Right [%d].", leftSubmitResult, rightSubmitResult);
		}
	}
	else
	{
		gEnv->pLog->Log("[HMD][OpenVR] SubmitFrame() called but device is not properly set up.");
	}

	m_submitTimeStamp = m_poseTimestamp;

	if (!m_overlay)
		return;

	for (int id = 0; id < RenderLayer::eQuadLayers_Total; id++)
	{
		if (!m_overlays[id].submitted && m_overlays[id].visible)
		{
			m_overlay->HideOverlay(m_overlays[id].handle);
			m_overlays[id].visible = false;
		}
		else if (m_overlays[id].submitted && !m_overlays[id].visible)
		{
			m_overlay->ShowOverlay(m_overlays[id].handle);
			m_overlays[id].visible = true;
		}
		m_overlays[id].submitted = false;
	}

	this->OnEndFrame();
}

// -------------------------------------------------------------------------
void Device::GetRenderTargetSize(uint& w, uint& h)
{
	m_system->GetRecommendedRenderTargetSize(&w, &h);
}

// -------------------------------------------------------------------------
void Device::GetMirrorImageView(EEyeType eye, void* resource, void** mirrorTextureView)
{
	vr::VRCompositor()->GetMirrorTextureD3D11(static_cast<vr::EVREye>(eye), resource, mirrorTextureView);
}

// -------------------------------------------------------------------------
void Device::OnSetupEyeTargets(ERenderAPI api, ERenderColorSpace colorSpace, void* leftEyeHandle, void* rightEyeHandle)
{
	m_eyeTargets[EEyeType::eEyeType_LeftEye] = {};
	m_eyeTargets[EEyeType::eEyeType_RightEye] = {};

	switch (colorSpace)
	{
	default:
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Auto:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eColorSpace = m_eyeTargets[EEyeType::eEyeType_RightEye].eColorSpace = vr::EColorSpace::ColorSpace_Auto;
		break;
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Gamma:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eColorSpace = m_eyeTargets[EEyeType::eEyeType_RightEye].eColorSpace = vr::EColorSpace::ColorSpace_Gamma;
		break;
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Linear:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eColorSpace = m_eyeTargets[EEyeType::eEyeType_RightEye].eColorSpace = vr::EColorSpace::ColorSpace_Linear;
		break;
	}

	switch (api)
	{
	default:
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eType = m_eyeTargets[EEyeType::eEyeType_RightEye].eType = vr::TextureType_DirectX;
		break;
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX12:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eType = m_eyeTargets[EEyeType::eEyeType_RightEye].eType = vr::TextureType_DirectX12;
		break;
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_OpenGL:
		m_eyeTargets[EEyeType::eEyeType_LeftEye].eType = m_eyeTargets[EEyeType::eEyeType_RightEye].eType = vr::TextureType_OpenGL;
		break;
	}

	m_eyeTargets[EEyeType::eEyeType_LeftEye].handle = leftEyeHandle;
	m_eyeTargets[EEyeType::eEyeType_RightEye].handle = rightEyeHandle;
}

// -------------------------------------------------------------------------
bool Device::IsActiveOverlay(int id) const
{
	return m_overlays[id].visible;
}

// -------------------------------------------------------------------------
void Device::OnSetupOverlay(int id, ERenderAPI api, ERenderColorSpace colorSpace, void* overlayTextureHandle, const QuatTS &pose, bool absolute, bool highestQuality)
{
	if (!m_overlay)
		return;

	char keyName[64];
	cry_sprintf(keyName, 64, "cry.overlay_%d", id);

	char overlayName[64];
	cry_sprintf(overlayName, 64, "cryOverlay_%d", id);

	vr::VROverlayHandle_t handle;
	if (m_overlay->FindOverlay(keyName, &handle) == vr::EVROverlayError::VROverlayError_None)
		m_overlay->DestroyOverlay(handle);

	vr::EVROverlayError err = m_overlay->CreateOverlay(keyName, overlayName, &m_overlays[id].handle);
	if (err != vr::EVROverlayError::VROverlayError_None)
	{
		gEnv->pLog->Log("[HMD][OpenVR] Error creating overlay %i: %s", id, m_overlay->GetOverlayErrorNameFromEnum(err));
		return;
	}

	m_overlays[id].vrTexture = {};
	m_overlays[id].vrTexture.handle = overlayTextureHandle;
	switch (api)
	{
	default:
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX:
		m_overlays[id].vrTexture.eType = vr::TextureType_DirectX;
		break;
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_DirectX12:
		m_overlays[id].vrTexture.eType = vr::TextureType_DirectX12;
		break;
	case CryVR::OpenVR::ERenderAPI::eRenderAPI_OpenGL:
		m_overlays[id].vrTexture.eType = vr::TextureType_OpenGL;
		break;
	}
	switch (colorSpace)
	{
	default:
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Auto:
		m_overlays[id].vrTexture.eColorSpace = vr::EColorSpace::ColorSpace_Auto;
		break;
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Gamma:
		m_overlays[id].vrTexture.eColorSpace = vr::EColorSpace::ColorSpace_Gamma;
		break;
	case CryVR::OpenVR::ERenderColorSpace::eRenderColorSpace_Linear:
		m_overlays[id].vrTexture.eColorSpace = vr::EColorSpace::ColorSpace_Linear;
		break;
	}

	m_overlays[id].pos = vr::RawConvert(Matrix34(pose));
	m_overlays[id].absolute = absolute;

	vr::VRTextureBounds_t overlayTextureBounds = { 0, 0, 1.f, 1.f };
	m_overlay->SetOverlayTextureBounds(m_overlays[id].handle, &overlayTextureBounds);
	m_overlay->SetOverlayTexture(m_overlays[id].handle, &m_overlays[id].vrTexture);
	if (m_overlays[id].absolute)
		m_overlay->SetOverlayTransformAbsolute(m_overlays[id].handle, m_pTrackingOriginCVar->GetIVal() == (int)EHmdTrackingOrigin::Standing ? vr::ETrackingUniverseOrigin::TrackingUniverseStanding : vr::ETrackingUniverseOrigin::TrackingUniverseSeated, &(m_overlays[id].pos));
	else
		m_overlay->SetOverlayTransformTrackedDeviceRelative(m_overlays[id].handle, vr::k_unTrackedDeviceIndex_Hmd, &(m_overlays[id].pos));
	m_overlay->SetOverlayWidthInMeters(m_overlays[id].handle, 1.f);
	if (highestQuality)
		m_overlay->SetHighQualityOverlay(m_overlays[id].handle);
	m_overlay->HideOverlay(m_overlays[id].handle);
	m_overlays[id].visible = false;
	m_overlays[id].submitted = false;
}

// -------------------------------------------------------------------------
void Device::OnDeleteOverlay(int id)
{
	if (!m_overlay)
		return;

	char keyName[64];
	cry_sprintf(keyName, 64, "cry.overlay_%d", id);

	char overlayName[64];
	cry_sprintf(overlayName, 64, "cryOverlay_%d", id);

	vr::VROverlayHandle_t handle;
	if (m_overlay->FindOverlay(keyName, &handle) == vr::EVROverlayError::VROverlayError_None)
	{
		m_overlay->ClearOverlayTexture(handle);
		m_overlay->HideOverlay(handle);
		m_overlay->DestroyOverlay(handle);

	}

	m_overlays[id].visible = false;
	m_overlays[id].submitted = false;
}

// -------------------------------------------------------------------------
void Device::OnPrepare()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_compositor->SetTrackingSpace(m_pTrackingOriginCVar->GetIVal() == (int)EHmdTrackingOrigin::Standing == 1 ? vr::ETrackingUniverseOrigin::TrackingUniverseStanding : vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
	vr::EVRCompositorError ret = m_compositor->WaitGetPoses(m_predictedRenderPose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

	if (ret != vr::EVRCompositorError::VRCompositorError_None)
	{
		gEnv->pLog->Log("[HMD][OpenVR] WaitGetPoses() failed!: [%d]", ret);
	}

	vr::Compositor_FrameTiming ft;
	ft.m_nSize = sizeof(vr::Compositor_FrameTiming);
	m_compositor->GetFrameTiming(&ft, 0);
	
	m_predictedRenderPoseTimestamp = ft.m_flSystemTimeInSeconds;
}

// -------------------------------------------------------------------------
void Device::OnPostPresent()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_compositor->PostPresentHandoff();
}

// -------------------------------------------------------------------------
stl::optional<Matrix34> Device::RequestAsyncCameraUpdate(uint64_t frameId, const Quat& oq, const Vec3 &op)
{
	// Predict time till photons in seconds
	float secondsSinceLastVsync;
	m_system->GetTimeSinceLastVsync(&secondsSinceLastVsync, nullptr);
	float displayFrequency = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);
	float frameDuration = 1.f / displayFrequency;
	float vsyncToPhotons = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SecondsFromVsyncToPhotons_Float);
	float timeTillPhotons = frameDuration - secondsSinceLastVsync + vsyncToPhotons;

	const auto trackingOrigin = m_pTrackingOriginCVar->GetIVal() == static_cast<int>(EHmdTrackingOrigin::Standing) ? vr::ETrackingUniverseOrigin::TrackingUniverseStanding : vr::ETrackingUniverseOrigin::TrackingUniverseSeated;
	vr::TrackedDevicePose_t trackedDevicePoses[vr::k_unMaxTrackedDeviceCount];
	m_system->GetDeviceToAbsoluteTrackingPose(trackingOrigin, timeTillPhotons, trackedDevicePoses, vr::k_unMaxTrackedDeviceCount);

	for (int i=0;i<vr::k_unMaxTrackedDeviceCount;++i)
	{
		const auto& trackedDevicePose = trackedDevicePoses[i];
		if (trackedDevicePose.bPoseIsValid && trackedDevicePose.bDeviceIsConnected)
		{
			vr::ETrackedDeviceClass devClass = m_system->GetTrackedDeviceClass(i);
			if (devClass == vr::ETrackedDeviceClass::TrackedDeviceClass_HMD)
			{
				// Found an updated pose for a connected HMD
				HmdPoseState worldPose, hmdPose;
				CopyPoseState(worldPose, hmdPose, trackedDevicePose);

				Vec3 p = oq * worldPose.position;
				Quat q = oq * worldPose.orientation;

				Matrix34 viewMtx(q);
				viewMtx.SetTranslation(op + p);

				m_poseTimestamp = m_predictedRenderPoseTimestamp;

				return viewMtx;
			}
		}
	}

	return stl::nullopt;
}

void* Device::GetD3D12EyeTextureData(EEyeType eye, ID3D12Resource *res, ID3D12CommandQueue *queue)
{
	auto *t = &m_d3d12EyeTextureData[eye];
	t->m_nNodeMask = 0;
	t->m_pCommandQueue = queue;
	t->m_pResource = res;

	return t;
}

void* Device::GetD3D12QuadTextureData(int quad, ID3D12Resource *res, ID3D12CommandQueue *queue)
{
	auto *t = &m_d3d12QuadTextureData[quad];
	t->m_nNodeMask = 0;
	t->m_pCommandQueue = queue;
	t->m_pResource = res;

	return t;
}

} // namespace OpenVR
} // namespace CryVR
