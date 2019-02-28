// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OsvrDevice.h"

#include "OsvrResources.h"

#include <Osvr/ClientKit/InterfaceC.h>
#include <Osvr/ClientKit/InterfaceCallbackC.h>
#include <Osvr/ClientKit/ParametersC.h>
#if CRY_PLATFORM_WINDOWS
#include <osvr/RenderKit/RenderManagerD3D11C.h>
#else
#include <osvr/RenderKit/RenderManagerOpenGLC.h>
#endif

#include <CrySystem/VR/IHMDManager.h>

#include <CryMath/Cry_Camera.h>
#include <CryRenderer/IRenderer.h>

namespace CryVR
{
namespace Osvr
{
static const char* s_szClientName = "com.crytek.cryengine";

void HMDQuatToWorldQuat(const Quat& hmdQuat, Quat& worldQuat)
{
	Matrix33 m33(hmdQuat);
	Vec3 column1 = -hmdQuat.GetColumn2();
	m33.SetColumn2(hmdQuat.GetColumn1());
	m33.SetColumn1(column1);
	worldQuat = Quat::CreateRotationX(gf_PI * 0.5f) * Quat(m33);
}

void HMDVec3ToWorldVec3(const Vec3& hmdVec, Vec3& worldVec)
{
	worldVec = Vec3(hmdVec.x, -hmdVec.z, hmdVec.y);
}

void OsvrQuatToCryQuat(const OSVR_Quaternion& quat, Quat& out)
{
	out.v = Vec3(static_cast<float>(osvrQuatGetX(&quat)), static_cast<float>(osvrQuatGetY(&quat)), static_cast<float>(osvrQuatGetZ(&quat)));
	out.w = static_cast<float>(osvrQuatGetW(&quat));
}

//rendermanager returns orientations in a different space than the clientkit. This should be fixed at some point but for now, do the fix here
void FixRenderManagerOrientation(Quat& orientation)
{
	orientation.v = -orientation.v;
}

void OsvrVec3ToCryVec3(const OSVR_Vec3& vec, Vec3& out)
{
	OSVR_Vec3& noConstVec3 = const_cast<OSVR_Vec3&>(vec);     //for some reason the OSVR_Vec3 getters are not const but the caller requires the first parameter to be const. Thus, const cast.
	out = Vec3(static_cast<float>(osvrVec3GetX(&noConstVec3)), static_cast<float>(osvrVec3GetY(&noConstVec3)), static_cast<float>(osvrVec3GetZ(&noConstVec3)));
}

float CalculateIPD(const OSVR_Pose3& left, const OSVR_Pose3& right)
{
	Vec3 l, r;
	OsvrVec3ToCryVec3(left.translation, l);
	OsvrVec3ToCryVec3(right.translation, r);

	return l.GetDistance(r);
}

void ExtractProjectionParamsFromMatrix(float* matrix, float nearPlane, float farPlane, float& lout, float& rout, float& bout, float& tout)
{
	float m00 = matrix[0];
	float m11 = matrix[5];
	float m20 = matrix[8];
	float m21 = matrix[9];

	lout = (m20 - 1.f) * (nearPlane / m00);
	rout = lout + (2.f * nearPlane) / m00;

	bout = (m21 - 1.f) * (nearPlane / m11);
	tout = bout + (2.f * nearPlane) / m11;
}

void ExtractEyeParamsFromMatrix(OSVR_ProjectionMatrix matrix, float& fov, float& aspect, float& asymV, float& asymH)
{
	aspect = static_cast<float>((matrix.right - matrix.left) / (matrix.top - matrix.bottom));
	fov = atanf(static_cast<float>(matrix.top / matrix.nearClip)) + atanf(static_cast<float>(abs(matrix.bottom) / matrix.nearClip));
	//AFAIK, asymmetry values get applied for both l and r (t and b) so divide by two
	asymH = static_cast<float>(matrix.right + matrix.left) / 2.f;
	asymV = static_cast<float>(matrix.top + matrix.bottom) / 2.f;
}

Device::Device()
	: m_localTrackingState(),
	m_nativeTrackingState(),
	m_refCount(1),
	m_context(0),
	m_headInterface(0),
	m_displayConfig(0),
	m_disableTracking(false),
	m_renderManager(0),
	m_renderManagerD3D11(0),
	m_lastFrameTrackingStateUpdated(0),
	m_loadingScreenActive(false)
{
	m_maxFovY = 1.853f;
	m_maxAspect = 2.f;

	m_recenterQuat = Quat::CreateIdentity();

	if (GetISystem()->GetISystemEventDispatcher())
	{
		GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CryVR::Osvr::Device");
	}

	gEnv->pSystem->GetHmdManager()->AddEventListener(this);

	m_pHmdSocialScreenKeepAspectCVar = gEnv->pConsole->GetCVar("hmd_social_screen_aspect_mode");
	m_pHmdSocialScreenCVar = gEnv->pConsole->GetCVar("hmd_social_screen");
}

Device::~Device()
{
	gEnv->pSystem->GetHmdManager()->RemoveEventListener(this);

	if (GetISystem()->GetISystemEventDispatcher())
	{
		GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
	}
	Shutdown();
}

// IHmdDevice interface
void Device::AddRef()
{
	CryInterlockedIncrement(&m_refCount);
}

void Device::Release()
{
	long refCount = CryInterlockedDecrement(&m_refCount);
	#if !defined(_RELEASE)
	if (refCount < 0)
		__debugbreak();
	#endif
	if (refCount == 0)
	{
		delete this;
	}
}

void trim(std::string& str, const char* c)
{
	std::size_t p = str.find(c);
	while (p != std::string::npos)
	{
		str.erase(p, 1);
		p = str.find('\"');
	}
}

//find a json value "find" from jsonStr (lowercase) and return the string from the original str (no lower case)
void findValue(std::string& jsonLowerCase, std::string& originalJson, const char* find, std::string& result)
{
	std::size_t start = jsonLowerCase.find(find);
	if (start == std::string::npos) return;

	start = jsonLowerCase.find(':', start);
	if (start == std::string::npos) return;
	start += 1;

	std::size_t end = jsonLowerCase.find("\n", start);
	if (end == std::string::npos) return;

	std::string substr = originalJson.substr(start, end - start);

	//trim
	trim(substr, "\"");
	trim(substr, " ");
	trim(substr, ",");
	result = substr;
}

bool Device::InitializeDevice()
{
	m_context = osvrClientInit(s_szClientName, 0);
	if (!m_context) return false;

	osvrClientUpdate(m_context);
	OSVR_ReturnCode hr = osvrClientCheckStatus(m_context);

	if (hr != OSVR_RETURN_SUCCESS) return false;

	hr |= osvrClientGetInterface(m_context, "/me/head", &m_headInterface);
	hr |= osvrClientGetDisplay(m_context, &m_displayConfig);

	if (hr != OSVR_RETURN_SUCCESS) return false;

	OSVR_DisplayInputCount displayCount;
	osvrClientGetNumDisplayInputs(m_displayConfig, &displayCount);

	if (displayCount < 1) return false;

	OSVR_DisplayDimension displayWidth, displayHeight;
	osvrClientGetDisplayDimensions(m_displayConfig, 0, &displayWidth, &displayHeight);

	m_deviceInfo.screenWidth = displayWidth;
	m_deviceInfo.screenHeight = displayHeight;

	CalculateSymmetricalFovsFromDisplayConfig(m_deviceInfo.fovH, m_deviceInfo.fovV);     //should this be calculated from the rendermanagers projection matrix rather than the display config?

	//just setup some sensible values at this point. Will be overridden once we get the correct information from the rendermanager.
	m_maxFovY = m_deviceInfo.fovV;
	m_maxAspect = static_cast<float>(displayWidth) / displayHeight;

	//get the HDK info
	size_t strLen = 1;
	char* charBuf;

	if (osvrClientGetStringParameterLength(m_context, "/display", &strLen) == OSVR_RETURN_SUCCESS)
	{
		charBuf = new char[strLen];
		if (osvrClientGetStringParameter(m_context, "/display", charBuf, strLen) == OSVR_RETURN_SUCCESS)
		{
			std::string model;
			std::string version;
			std::string str(charBuf);
			std::string str2(str);
			delete[] charBuf;
			charBuf = 0;

			std::transform(str.begin(), str.end(), str.begin(), ::tolower);

			findValue(str, str2, "vendor", m_vendor);
			findValue(str, str2, "model", model);
			findValue(str, str2, "version", version);

			m_model = model + version;

			if (m_model.size() > 0)
			{
				m_deviceInfo.productName = m_model.c_str();
			}

			if (m_vendor.size() > 0)
			{
				m_deviceInfo.manufacturer = m_vendor.c_str();
			}
		}
	}

	return true;
}

void Device::Shutdown()
{
	if (m_context)
	{
		if (m_headInterface)
		{
			osvrClientFreeInterface(m_context, m_headInterface);
			m_headInterface = 0;
		}

		if (m_displayConfig)
		{
			osvrClientFreeDisplay(m_displayConfig);
			m_displayConfig = 0;
		}

		osvrClientShutdown(m_context);
		m_context = 0;
	}
}

void Device::GetDeviceInfo(HmdDeviceInfo& info) const
{
	info = m_deviceInfo;
}

void Device::GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const
{
	fov = m_maxFovY;
	aspectRatioFactor = m_maxAspect;     //(not used);
}

HMDCameraSetup Device::GetHMDCameraSetup(int nEye, float ratio, float fnear) const
{
	const EyeSetup& setup = m_eyes[nEye];
	// TODO: Generate projection matrix
	assert(false);
	return HMDCameraSetup{};
}

void Device::DisableHMDTracking(bool disable)
{
	m_disableTracking = disable;
}

void Device::RecenterPose()
{
	m_recenterQuat = m_nativeTrackingState.pose.orientation.GetInverted();
}

void Device::UpdateTrackingState(EVRComponent e, uint64_t frameId)
{
	if ((e & eVRComponent_Hmd) != 0)
	{
	#if !defined(_RELEASE)
		const int frameID = gEnv->pRenderer->GetFrameID(false);
		if (!gEnv->IsEditor())
		{
			if (((e & eVRComponent_Hmd) != 0) && (CryGetCurrentThreadId() != gEnv->mMainThreadId) && (m_loadingScreenActive == false))
			{
				gEnv->pLog->LogError("[HMD][Osvr] Device::UpdateTrackingState() should be called from main thread only!");
			}

			if (frameID == m_lastFrameTrackingStateUpdated)
			{
				gEnv->pLog->LogError("[HMD][Osvr] Device::UpdateTrackingState() should be called only once per frame!");
			}

		}
		m_lastFrameTrackingStateUpdated = frameID;
	#endif

		//update client
		if (osvrClientUpdate(m_context) != OSVR_RETURN_SUCCESS)
		{
			gEnv->pLog->LogError("[HMD][Osvr] Device::UpdateTrackingState() Failed to update tracking state!");
			return;
		}

		if (m_renderManager)
		{
			UpdateRenderInfo();
			UpdatePoseFromRenderInfo();
			UpdateEyeSetups();
		}

	}
}

void Device::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START_LOADINGSCREEN:
		m_loadingScreenActive = true;
		break;

	case ESYSTEM_EVENT_LEVEL_LOAD_END:
	case ESYSTEM_EVENT_LEVEL_LOAD_ERROR:
		m_loadingScreenActive = false;
		break;

	default:
		break;
	}
}

const HmdTrackingState& Device::GetNativeTrackingState() const
{
	if (m_disableTracking)
	{
		return m_disabledTrackingState;
	}

	return m_nativeTrackingState;
}

const HmdTrackingState& Device::GetLocalTrackingState() const
{
	if (m_disableTracking)
	{
		return m_disabledTrackingState;
	}

	return m_localTrackingState;
}

const IHmdController* Device::GetController() const
{
	return nullptr;
}

Device* Device::CreateAndInitializeDevice()
{
	Device* pDevice = new Device();
	if (!pDevice->InitializeDevice())
	{
		pDevice->Release();
		return nullptr;
	}
	return pDevice;
}

void Device::OnRecentered()
{
	RecenterPose();
}

void Device::UpdatePose(const Quat& orientation, const Vec3& translation)
{
	//update local pose
	m_nativeTrackingState.pose.orientation = orientation;
	m_nativeTrackingState.pose.position = translation;

	//apply recenter
	Quat recenteredQuat = m_recenterQuat * m_nativeTrackingState.pose.orientation;

	//update world pose
	HMDQuatToWorldQuat(recenteredQuat, m_localTrackingState.pose.orientation);
	HMDVec3ToWorldVec3(m_nativeTrackingState.pose.position, m_localTrackingState.pose.position);

	const ICVar* pStereoScaleCoefficient = gEnv->pConsole ? gEnv->pConsole->GetCVar("r_stereoScaleCoefficient") : 0;
	const float stereoScaleCoefficient = pStereoScaleCoefficient ? pStereoScaleCoefficient->GetFVal() : 1.f;

	m_nativeTrackingState.pose.position *= stereoScaleCoefficient;
	m_localTrackingState.pose.position *= stereoScaleCoefficient;

	//setup flags (everything enabled for now)
	m_localTrackingState.statusFlags = eHmdStatus_OrientationTracked | eHmdStatus_HmdConnected;
	m_nativeTrackingState.statusFlags = m_localTrackingState.statusFlags;
}

void Device::UpdatePoseFromRenderInfo()
{
	if (m_latestRenderInfo.size() > 0)
	{
		Quat orientation;
		Vec3 translation;
		//monosetup
		if (m_latestRenderInfo.size() == 1)
		{

			OsvrQuatToCryQuat(m_latestRenderInfo[0].pose.rotation, orientation);
			OsvrVec3ToCryVec3(m_latestRenderInfo[0].pose.translation, translation);
		}
		else
		{
			Vec3 l, r;
			OsvrQuatToCryQuat(m_latestRenderInfo[0].pose.rotation, orientation);
			OsvrVec3ToCryVec3(m_latestRenderInfo[0].pose.translation, l);
			OsvrVec3ToCryVec3(m_latestRenderInfo[1].pose.translation, r);
			translation = Vec3::CreateLerp(l, r, 0.5f);
		}

		FixRenderManagerOrientation(orientation);
		UpdatePose(orientation, translation);
	}
}

void Device::UpdatePoseFromOsvrPose(const OSVR_PoseState& newPose)
{
	Quat o;
	Vec3 t;
	OsvrQuatToCryQuat(newPose.rotation, o);
	OsvrVec3ToCryVec3(newPose.translation, t);
	UpdatePose(o, t);
}

void Device::CalculateSymmetricalFovsFromDisplayConfig(float& fovH, float& fovV)
{
	//calculate symmetrical fov
	float leftMatrix[OSVR_MATRIX_SIZE];
	float rightMatrix[OSVR_MATRIX_SIZE];
	float nearPlane = GetISystem()->GetViewCamera().GetNearPlane();
	float farPlane = GetISystem()->GetViewCamera().GetFarPlane();
	osvrClientGetViewerEyeSurfaceProjectionMatrixf(m_displayConfig, 0, Left, 0, nearPlane, farPlane, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS, leftMatrix);
	osvrClientGetViewerEyeSurfaceProjectionMatrixf(m_displayConfig, 0, Right, 0, nearPlane, farPlane, OSVR_MATRIX_COLMAJOR | OSVR_MATRIX_COLVECTORS, rightMatrix);

	float l, r, b, t;
	float l2, r2, b2, t2;

	ExtractProjectionParamsFromMatrix(leftMatrix, nearPlane, farPlane, l, r, b, t);
	ExtractProjectionParamsFromMatrix(rightMatrix, nearPlane, farPlane, l2, r2, b2, t2);

	r = max(r, r2);
	t = max(t, t2);

	fovH = 2.f * atanf(r / nearPlane);
	fovV = 2.f * atanf(t / nearPlane);
}

bool Device::UpdateEyeSetups()
{

	if (m_latestRenderInfo.Num() < 2) return false;

	OSVR_Pose3 leftEye = m_latestRenderInfo[0].pose;
	OSVR_Pose3 rightEye = m_latestRenderInfo[1].pose;
	OSVR_ProjectionMatrix leftMatrix = m_latestRenderInfo[0].projection;
	OSVR_ProjectionMatrix rightMatrix = m_latestRenderInfo[1].projection;

	float eyeDist = CalculateIPD(leftEye, rightEye);
	m_eyes[Left].eyeDist = eyeDist;
	m_eyes[Right].eyeDist = eyeDist;

	EyeSetup& leftSetup = m_eyes[Left];
	ExtractEyeParamsFromMatrix(leftMatrix, leftSetup.fov, leftSetup.aspectRatio, leftSetup.asymV, leftSetup.asymH);

	EyeSetup& rightSetup = m_eyes[Right];
	ExtractEyeParamsFromMatrix(rightMatrix, rightSetup.fov, rightSetup.aspectRatio, rightSetup.asymV, rightSetup.asymH);

	m_maxFovY = max(leftSetup.fov, rightSetup.fov);
	m_maxAspect = max(leftSetup.aspectRatio, rightSetup.aspectRatio);

	return true;
}

void Device::UpdateRenderInfo()
{
	if (m_renderManager && m_renderManagerD3D11)
	{
		float nearPlane = GetISystem()->GetViewCamera().GetNearPlane();
		float farPlane = GetISystem()->GetViewCamera().GetFarPlane();
		m_renderParams.nearClipDistanceMeters = nearPlane;
		m_renderParams.farClipDistanceMeters = farPlane;

		OSVR_RenderInfoCount count;
		osvrRenderManagerGetNumRenderInfo(m_renderManager, m_renderParams, &count);

		if (m_latestRenderInfo.Num() != count)
		{
			m_latestRenderInfo.Reserve(count);
		}

		for (OSVR_RenderInfoCount i = 0; i < count; ++i)
		{
			OSVR_RenderInfoD3D11 info;
			osvrRenderManagerGetRenderInfoD3D11(m_renderManager, i, m_renderParams, &info);
			m_latestRenderInfo[i] = info;
		}
	}
}

bool Device::InitializeRenderer(void* d3dDevice, void* d3dContext)
{

	OSVR_GraphicsLibraryD3D11 library;
	library.device = static_cast<ID3D11Device*>(d3dDevice);
	library.context = static_cast<ID3D11DeviceContext*>(d3dContext);

	bool bSuccess = osvrCreateRenderManagerD3D11(m_context, "Direct3D11", library, &m_renderManager, &m_renderManagerD3D11) == OSVR_RETURN_SUCCESS;
	if (bSuccess && m_renderManager && m_renderManagerD3D11)
	{
		OSVR_OpenResultsD3D11 res;
		bSuccess = osvrRenderManagerOpenDisplayD3D11(m_renderManagerD3D11, &res) == OSVR_RETURN_SUCCESS;
	}
	if (bSuccess)
	{
		bSuccess = m_renderManager && osvrRenderManagerGetDoingOkay(m_renderManager) == OSVR_RETURN_SUCCESS;
	}

	if (bSuccess)
	{
		osvrRenderManagerGetDefaultRenderParams(&m_renderParams);

		float nearPlane = GetISystem()->GetViewCamera().GetNearPlane();
		float farPlane = GetISystem()->GetViewCamera().GetFarPlane();
		m_renderParams.nearClipDistanceMeters = nearPlane;
		m_renderParams.farClipDistanceMeters = farPlane;
		UpdateRenderInfo();
	}

	if (!bSuccess)
	{
		CryLogAlways("[OsvrDevice] Error: Failed to create rendermanager! Are you using a whitelisted GPU?");
	}

	return bSuccess;
}

bool Device::PresentTextureSet(int textureSetIndex)
{
	this->OnEndFrame();

	if (m_renderBufferSets.size() <= textureSetIndex) return false;

	TArray<OSVR_RenderBufferD3D11>& renderBuffers = m_renderBufferSets[textureSetIndex];

	if (renderBuffers.Num() > m_latestRenderInfo.Num())
	{
		CryLogAlways("[OsvrDevice] Error: render info not populated properly before present called!");
		return false;
	}

	OSVR_RenderManagerPresentState presentBufState;
	osvrRenderManagerStartPresentRenderBuffers(&presentBufState);

	OSVR_ViewportDescription viewPort;
	viewPort.left = 0.0;
	viewPort.lower = 0.0;
	viewPort.width = 1.0;
	viewPort.height = 1.0;

	for (unsigned int i = 0; i < renderBuffers.Num(); ++i)
	{
		osvrRenderManagerPresentRenderBufferD3D11(presentBufState, renderBuffers[i], m_latestRenderInfo[i], viewPort);
	}

	return (OSVR_RETURN_SUCCESS == osvrRenderManagerFinishPresentRenderBuffers(m_renderManager, presentBufState, m_renderParams, OSVR_FALSE));
}

bool Device::RegisterTextureSwapSet(TextureSwapSet* swapSet)
{
	OSVR_RenderManagerRegisterBufferState regBufState;
	osvrRenderManagerStartRegisterRenderBuffers(&regBufState);

	m_renderBufferSets.Reserve(swapSet->numTextureSets);

	bool bSuccess = true;
	for (int i = 0; i < swapSet->numTextureSets; ++i)
	{
		TArray<OSVR_RenderBufferD3D11>& renderBuffers = m_renderBufferSets[i];
		renderBuffers.SetUse(0);

		TextureSet& texSet = swapSet->pTextureSets[i];
		int texCount = texSet.numTextures;

		for (int texIndex = 0; texIndex < texCount; ++texIndex)
		{
			OSVR_RenderBufferD3D11 renderBuffer;
			renderBuffer.colorBuffer = static_cast<ID3D11Texture2D*>(texSet.pTextures[texIndex].pTexture);
			renderBuffer.colorBufferView = static_cast<ID3D11RenderTargetView*>(texSet.pTextures[texIndex].pRtView);
			renderBuffer.depthStencilBuffer = NULL;
			renderBuffer.depthStencilView = NULL;
			renderBuffers.Add(renderBuffer);

			bSuccess = (bSuccess && osvrRenderManagerRegisterRenderBufferD3D11(regBufState, renderBuffer) == OSVR_RETURN_SUCCESS);
		}
	}

	// The final parameter specifies whether we won't overwrite the texture before presenting again
	// We cannot currently guarantee this, so we indicate that a copy needs to be done on the OSVR side to enable Asynchronous Time Warp.
	return (bSuccess && osvrRenderManagerFinishRegisterRenderBuffers(m_renderManagerD3D11, regBufState, OSVR_FALSE) == OSVR_RETURN_SUCCESS);
}

void Device::GetPreferredRenderResolution(unsigned int& width, unsigned int& height)
{
	if (m_latestRenderInfo.Num() == 0)
	{
		width = m_deviceInfo.screenWidth;
		height = m_deviceInfo.screenHeight;
	}
	else
	{
		width = static_cast<unsigned int>(m_latestRenderInfo[0].viewport.width);
		height = static_cast<unsigned int>(m_latestRenderInfo[0].viewport.height);
	}
}

void Device::ShutdownRenderer()
{
	osvrDestroyRenderManager(m_renderManager);
	m_renderManager = 0;
	m_renderManagerD3D11 = 0;
}
}
}