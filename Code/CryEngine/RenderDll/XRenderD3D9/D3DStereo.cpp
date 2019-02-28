// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "D3DStereo.h"
#include "DriverD3D.h"
#include "D3DPostProcess.h"
#include "D3DOculus.h"
#include "D3DOsvr.h"
#include "D3DOpenVR.h"

#include <Common/RenderDisplayContext.h>

#define MGPU_STEREO

#if CRY_PLATFORM_WINDOWS
	#include <CryInput/IHardwareMouse.h>
#endif

#if defined(USE_NV_API)
	#include NV_API_HEADER
#endif

#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#ifdef ENABLE_BENCHMARK_SENSOR
	#include <IBenchmarkFramework.h>
	#include <IBenchmarkRendererSensorManager.h>
#endif

#if defined(AMD_LIQUID_VR) && !CRY_RENDERER_OPENGL
	#ifdef _M_AMD64
		#define DXX_DLL_NAME "atidxx64.dll"
		#define CFX_DLL_NAME "aticfx64.dll"
	#else
		#define DXX_DLL_NAME "atidxx32.dll"
		#define CFX_DLL_NAME "aticfx32.dll"
	#endif

IAmdDxExt* g_pDxxExt = NULL;
IAmdDxExtMgpuAppControl* g_pMgpu = NULL;
HANDLE g_drawDoneSync = NULL;
HANDLE g_xferDoneSync = NULL;
#elif defined(USE_NV_API)
void* g_pMgpu;
	#define SELECT_GPU(a)
	#define SELECT_ALL_GPU()
#else
void* g_pMgpu;
	#define SELECT_GPU(a)
	#define SELECT_ALL_GPU()
#endif

void OnStereoModeVarChanged(ICVar* pCvar)
{
	gcpRendD3D->GetS3DRend().OnStereoModeChanged();
}


CStereoRenderingScope::CStereoRenderingScope(std::string &&tag, SDisplayContextKey displayContextKey)
	: profileMarkerTag(std::move_if_noexcept(tag))
	, displayContextKey(displayContextKey)
{
	if (profileMarkerTag.length())
		gcpRendD3D->PushProfileMarker(profileMarkerTag.c_str());

	auto pair = gcpRendD3D->SetCurrentContext(displayContextKey);
	CRY_ASSERT(pair.first);
	previousDisplayContextKey = pair.second;
}

CStereoRenderingScope::~CStereoRenderingScope() noexcept
{
	gcpRendD3D->SetCurrentContext(previousDisplayContextKey);
	if (profileMarkerTag.length())
		gcpRendD3D->PopProfileMarker(profileMarkerTag.c_str());
}


CD3DStereoRenderer::CD3DStereoRenderer()
	: m_device(EStereoDevice::STEREO_DEVICE_DEFAULT)
	, m_deviceState(EStereoDeviceState::STEREO_DEVSTATE_UNSUPPORTED_DEVICE)
	, m_mode(EStereoMode::STEREO_MODE_NO_STEREO)
	, m_output(EStereoOutput::STEREO_OUTPUT_STANDARD)
	, m_submission(EStereoSubmission::STEREO_SUBMISSION_SEQUENTIAL)
	, m_driver(DRIVER_UNKNOWN)
	, m_nvStereoHandle(nullptr)
	, m_nvStereoStrength(0.0f)
	, m_nvStereoActivated(0)
	, m_stereoStrength(0.0f)
	, m_zeroParallaxPlaneDist(0.25f)
	, m_maxSeparationScene(0.0f)
	, m_nearGeoScale(0.0f)
	, m_gammaAdjustment(0)
	, m_resourcesPatched(false)
	, m_needClearVrQuadLayer(true)
	, m_pHmdRenderer(nullptr)
	, m_bPreviousCameraValid(false)
{
	REGISTER_CVAR3("r_StereoDevice", m_device, m_device, VF_REQUIRE_APP_RESTART | VF_DUMPTODISK,
		"Sets stereo device (only possible before app start)\n"
		"Usage: r_StereoDevice [0/1/2/3/4]\n"
		"0: No stereo support (default)\n"
		"1: Frame compatible formats (side-by-side, interlaced, anaglyph)\n"
		"2: Stereo driver (PC only, NVidia or AMD)\n"
		"100: Auto-detect device for platform");

	REGISTER_CVAR3_CB("r_StereoMode", m_mode, m_mode, VF_DUMPTODISK,
		"Sets stereo rendering mode.\n"
		"Usage: r_StereoMode [0=off/1/2]\n"
		"1: Dual rendering\n"
		"2: Post Stereo\n"
		"3: Menu mode: Flash rendered twice, once to social screen and once to HMD", OnStereoModeVarChanged);

	REGISTER_CVAR3_CB("r_StereoOutput", m_output, m_output, VF_DUMPTODISK,
		"Sets stereo output. Output depends on the stereo monitor\n"
		"Usage: r_StereoOutput [0=off/1/2/3/4/5/6/...]\n"
		"0: Standard\n"
		"1: Side by Side Squeezed\n"
		"2: Checkerboard\n"
		"3: Above and Below (not supported)\n"
		"4: Side by Side\n"
		"5: Line by Line (Interlaced)\n"
		"6: Anaglyph\n"
		"7: VR Device\n", OnStereoModeVarChanged);

	if (m_device == EStereoDevice::STEREO_DEVICE_DEFAULT)
		SelectDefaultDevice();

#if defined(AMD_LIQUID_VR) && !CRY_RENDERER_OPENGL
	//////////////////////////////////////////////////////////////////////////
	// INTIALIZE LIQUID VR (to be relocated where appropriate later)
	//////////////////////////////////////////////////////////////////////////
	static bool _hack_bUseMGPUAffinity = true;
	if (_hack_bUseMGPUAffinity)
	{
		HMODULE hCfx = LoadLibraryA(CFX_DLL_NAME);

		PFNAmdExtRequestMgpuAppControl pFnEnable =
		  (PFNAmdExtRequestMgpuAppControl)GetProcAddress(hCfx, "AmdExtRequestMgpuAppControl");

		if (pFnEnable != nullptr)
		{
			pFnEnable(TRUE);
			CryLogAlways("AMD LIQUID VR library loaded");
		}
		else
		{
			CryLogAlways("AMD LIQUID VR library failed to loaded");
		}
	}
#endif
}

void CD3DStereoRenderer::SelectDefaultDevice()
{
	m_device = EStereoDevice::STEREO_DEVICE_FRAMECOMP;
}

void CD3DStereoRenderer::OnStereoModeChanged()
{
	if (m_mode == EStereoMode::STEREO_MODE_NO_STEREO)
	{
		DisableStereo();
	}
	else
	{
		EnableStereo();
		ShutdownHmdRenderer();
	}
}

void CD3DStereoRenderer::InitDeviceBeforeD3D()
{
	LOADING_TIME_PROFILE_SECTION;
#if defined(USE_NV_API)
	bool availableDriverNV = false;
	uint8 enabled = 0;

	// Always set direct mode so that the NV driver never does its own stereo
	if (NvAPI_Initialize() == NVAPI_OK &&
	    NvAPI_Stereo_SetDriverMode(NVAPI_STEREO_DRIVER_MODE_DIRECT) == NVAPI_OK &&
	    NvAPI_Stereo_IsEnabled(&enabled) == NVAPI_OK)
	{
		if (!enabled)
			iLog->LogWarning("3DVision is disabled in control panel");
		else
			availableDriverNV = true;
	}

	if (!availableDriverNV && m_device == EStereoDevice::STEREO_DEVICE_DRIVER)
	{
		// Fall back to frame-compatible device
		m_device = EStereoDevice::STEREO_DEVICE_FRAMECOMP;
	}
#endif

	if (m_device == EStereoDevice::STEREO_DEVICE_NONE)
		return;

	bool success = true;
	m_deviceState = EStereoDeviceState::STEREO_DEVSTATE_UNSUPPORTED_DEVICE;

#if defined(USE_NV_API)
	if (m_device == EStereoDevice::STEREO_DEVICE_DRIVER)
	{
		if (availableDriverNV)
		{
			m_driver = DRIVER_NV;
		}
		else
		{
			success = false;
			m_deviceState = EStereoDeviceState::STEREO_DEVSTATE_BAD_DRIVER;
		}
	}
#elif CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	if (m_device != EStereoDevice::STEREO_DEVICE_FRAMECOMP)
		success = false;
#endif

	if (success)
	{
		m_deviceState = EStereoDeviceState::STEREO_DEVSTATE_OK;
	}
	else
	{
		iLog->LogError("Failed to create stereo device");
		m_device = EStereoDevice::STEREO_DEVICE_NONE;
	}
}

void CD3DStereoRenderer::InitDeviceAfterD3D()
{
	LOADING_TIME_PROFILE_SECTION;

	g_pMgpu = NULL;
#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(AMD_LIQUID_VR)
	//TD need to add AMD lib check
	// LIQUID
	HMODULE hDxx = LoadLibraryA(DXX_DLL_NAME);

	if (hDxx != NULL)
	{
		PFNAmdDxExtCreate11 pFnCreate = (PFNAmdDxExtCreate11)GetProcAddress(hDxx, "AmdDxExtCreate11");

		if (pFnCreate != nullptr)
		{
			D3DDevice* device = NULL;
			device = gcpRendD3D->GetDevice().GetRealDevice();
			assert(device);
			pFnCreate(device, &g_pDxxExt);

			if (g_pDxxExt != nullptr)
			{
				IAmdDxExtInterface* pIface = g_pDxxExt->GetExtInterface(AmdDxExtMgpuAppControlID);

				if (pIface != nullptr)
				{
					g_pMgpu = static_cast<IAmdDxExtMgpuAppControl*>(pIface);
					if (g_pMgpu)
					{
						// Set our default masks
						g_pMgpu->SetRenderGpuMask(GPUMASK_BOTH);

						g_drawDoneSync = g_pMgpu->SyncCreate();
						g_xferDoneSync = g_pMgpu->SyncCreate();
					}
				}
			}
		}
	}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && defined(USE_NV_API)
	if (IsDriver(DRIVER_NV))
	{
		if (m_nvStereoHandle)
		{
			NvAPI_Stereo_DestroyHandle(m_nvStereoHandle);
			m_nvStereoHandle = nullptr;
		}

		void* pStereoHandle;
		D3DDevice* device = gcpRendD3D->GetDevice().GetRealDevice();

		if (NvAPI_Stereo_CreateHandleFromIUnknown(device, &pStereoHandle) == NVAPI_OK)
			m_nvStereoHandle = pStereoHandle;
		else
			m_device = EStereoDevice::STEREO_DEVICE_NONE;

		DisableStereo();  // Always disabled at startup
	}

	g_pMgpu = NULL;
#endif

	if (gcpRendD3D->m_DeviceWrapper.GetNodeCount() & 2)
	{
		m_submission = EStereoSubmission::STEREO_SUBMISSION_PARALLEL;

		if (gcpRendD3D->m_DeviceWrapper.GetNodeCount() & 4)
			m_submission = EStereoSubmission::STEREO_SUBMISSION_PARALLEL_MULTIRES;
	}
}

void CD3DStereoRenderer::Shutdown()
{
	OnHmdDeviceChanged(nullptr);

#if defined(USE_NV_API)
	if (m_nvStereoHandle)
	{
		NvAPI_Stereo_DestroyHandle(m_nvStereoHandle);
		m_nvStereoHandle = nullptr;
	}
#endif

	for (auto &e : m_pEyeDisplayContexts)
		RecreateDisplayContext(e, "", Clr_Empty, {});
	for (auto &e : m_pVrQuadLayerDisplayContexts)
		RecreateDisplayContext(e, "", Clr_Transparent, {});
}

bool CD3DStereoRenderer::EnableStereo()
{
#if defined(USE_NV_API)
	if (IsDriver(DRIVER_NV))
	{
		uint8 windowedSupport = 0;
		NvAPI_Stereo_IsWindowedModeSupported(&windowedSupport);

		if (gcpRendD3D->IsFullscreen() || windowedSupport)
		{
			NvAPI_Stereo_Activate(m_nvStereoHandle);
			NvAPI_Stereo_SetNotificationMessage(m_nvStereoHandle, 0, 0);
			NvAPI_Stereo_SetActiveEye(m_nvStereoHandle, NVAPI_STEREO_EYE_LEFT);
			return true;
		}
		else
		{
			NvAPI_Stereo_Deactivate(m_nvStereoHandle);
			iLog->LogError("3DVision requires fullscreen mode");
			return false;
		}
	}
#endif

	return true;
}

void CD3DStereoRenderer::DisableStereo()
{
#if defined(USE_NV_API)
	if (IsDriver(DRIVER_NV))
	{
		NvAPI_Stereo_Deactivate(m_nvStereoHandle);
	}
#endif

	m_gammaAdjustment = 0;

	ShutdownHmdRenderer();
}

void CD3DStereoRenderer::ChangeOutputFormat()
{
	ShutdownHmdRenderer();
}

void CD3DStereoRenderer::ShutdownHmdRenderer()
{
	if (m_pHmdRenderer != nullptr)
	{
		m_pHmdRenderer->Shutdown();
		SAFE_DELETE(m_pHmdRenderer);
	}

	ReleaseRenderResources();
}

void CD3DStereoRenderer::ReleaseRenderResources()
{
	m_eyeToScreenPass = nullptr;
	m_quadLayerPass = nullptr;
}

void CD3DStereoRenderer::PrepareStereo()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

#if defined(USE_NV_API)
	if (IsDriver(DRIVER_NV))
	{
		HandleNVControl();
	}
#endif

	if (m_device != EStereoDevice::STEREO_DEVICE_NONE && m_mode != EStereoMode::STEREO_MODE_NO_STEREO)
	{
		// VR FIX ME
		// These code was written for 3D TV stereo, not for VR.
		// VR EyeDist concept is the half distance between the eyes, it can be different for 3D TV with reprojection

		// Update stereo parameters
		m_stereoStrength        = CRenderer::CV_r_StereoStrength;
		m_maxSeparationScene    = CRenderer::CV_r_StereoEyeDist;
		m_zeroParallaxPlaneDist = CRenderer::CV_r_StereoScreenDist;
		m_nearGeoScale          = CRenderer::CV_r_StereoNearGeoScale;
		m_gammaAdjustment       = CRenderer::CV_r_StereoGammaAdjustment;

		// Apply stereo strength
		m_maxSeparationScene *= m_stereoStrength;

		auto device = gEnv->pSystem->GetHmdManager()->GetHmdDevice();
		if (m_output == EStereoOutput::STEREO_OUTPUT_HMD && m_pHmdRenderer == nullptr && device)
		{
			// Create the HMD renderer
			m_pHmdRenderer = CreateHmdRenderer(*device);

			if (m_pHmdRenderer != nullptr)
			{
				int eyeWidth, eyeHeight;
				CalculateResolution(0, 0, &eyeWidth, &eyeHeight);

				if (!m_pHmdRenderer->Initialize(eyeWidth, eyeHeight))
				{
					CRY_ASSERT_MESSAGE(false, "m_pHmdRenderer->Initialize() failed!");
				}
			}
		}
	}
}

void CD3DStereoRenderer::HandleNVControl()
{
#if defined(USE_NV_API)
	if (m_stereoStrength != CRenderer::CV_r_StereoStrength)
	{
		NvAPI_Stereo_SetSeparation(m_nvStereoHandle, clamp_tpl((CRenderer::CV_r_StereoStrength - 0.5f) * 100.f, 0.f, 100.f));
	}

	NvAPI_Stereo_IsActivated(m_nvStereoHandle, &m_nvStereoActivated);
	NvAPI_Stereo_GetSeparation(m_nvStereoHandle, &m_nvStereoStrength);
#endif
}

void CD3DStereoRenderer::RecreateDisplayContext(std::pair<std::shared_ptr<CCustomRenderDisplayContext>, SDisplayContextKey> &target, std::string name, const ColorF &clearColor, std::vector<_smart_ptr<CTexture>> &&swapChain)
{
	IRenderer::SDisplayContextDescription displayContextDesc = {};
	displayContextDesc.clearColor = clearColor;

	if (target.first)
	{
		if (name.empty())
			name = target.first->GetName();

		gcpRendD3D->DeleteContext(target.second);
	}

	target.first = swapChain.size() ? std::make_shared<CCustomRenderDisplayContext>(displayContextDesc, name, gcpRendD3D->GenerateUniqueContextId(), std::move(swapChain)) : nullptr;
	target.second = {};
	if (target.first)
		target.second = gcpRendD3D->AddCustomContext(target.first);
}

void CD3DStereoRenderer::CreateEyeDisplayContext(CCamera::EEye eEye, std::vector<_smart_ptr<CTexture>> &&swapChain)
{
	static_assert(CCamera::eEye_eCount <= 2, "More eyes defined than covered in the following code, please adjust.");
	const std::string name = (eEye == CCamera::EEye::eEye_Left ? "LeftEye-SwapChain" : "RightEye-SwapChain");

	RecreateDisplayContext(m_pEyeDisplayContexts[eEye], name, Clr_Empty, std::move(swapChain));
}

void CD3DStereoRenderer::CreateVrQuadLayerDisplayContext(RenderLayer::EQuadLayers id, std::vector<_smart_ptr<CTexture>> &&swapChain)
{
	static_assert(RenderLayer::eQuadLayers_Total <= 9, "More quadlayers defined than covered in the following code, please adjust.");
	const char number = '0' + id;
	const std::string name = std::string("Quad") + number + std::string("-SwapChain");

	RecreateDisplayContext(m_pVrQuadLayerDisplayContexts[id], name, Clr_Transparent, std::move(swapChain));
}

void CD3DStereoRenderer::SetCurrentEyeSwapChainIndices(const std::array<uint32_t, eEyeType_NumEyes> &indices)
{
	for (auto it = m_pEyeDisplayContexts.begin(); it != m_pEyeDisplayContexts.end(); ++it)
	{
		if (it->first)
			it->first->SetSwapChainIndex(indices[std::distance(m_pEyeDisplayContexts.begin(), it)]);
	}
}

void CD3DStereoRenderer::SetCurrentQuadLayerSwapChainIndices(const std::array<uint32_t, RenderLayer::eQuadLayers_Total> &indices)
{
	for (auto it = m_pVrQuadLayerDisplayContexts.begin(); it != m_pVrQuadLayerDisplayContexts.end(); ++it)
	{
		if (it->first)
			it->first->SetSwapChainIndex(indices[std::distance(m_pVrQuadLayerDisplayContexts.begin(), it)]);
	}
}

CCamera CD3DStereoRenderer::PrepareCamera(int nEye, const CCamera& currentCamera)
{
	CCamera cam = currentCamera;
	cam.SetEye(CCamera::EEye(nEye));

	bool renderToHmd = false;

	if (gEnv->pConsole)
	{
		if (ICVar* pCvar = gEnv->pConsole->GetCVar("sys_vr_support"))
		{
			renderToHmd = pCvar->GetIVal() > 0;
		}
	}

	if (gEnv->IsEditor() && !gEnv->IsEditorGameMode())
		renderToHmd = false;

	if (renderToHmd)
	{
		if (IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager())
		{
			int width = 0, height = 0;
			if (auto *dc = GetEyeDisplayContext(CCamera::EEye(nEye)).first)
			{
				const CTexture *tex = dc->GetCurrentBackBuffer();
				width  = tex->GetWidth();
				height = tex->GetHeight();
			}
			CRY_ASSERT(width > 0 && height > 0);

			const float n = cam.GetNearPlane();
			const float f = cam.GetFarPlane();
			const float projRatio = static_cast<float>(width) / static_cast<float>(height);
			const HMDCameraSetup cameraSetup = pHmdManager->GetHMDCameraSetup(nEye, projRatio, n);

			// Note:
			// The inter-pupilar distance (IPD) gets modulated by r_stereoScaleCoefficient in order to be able
			// to "scale" the feeling of the player's size.
			// It also modifies the movement of the player's pose tracking

			const float modulatedHalfIpd = cameraSetup.ipd * 0.5f * CRenderer::CV_r_stereoScaleCoefficient;
			const float fov = cameraSetup.sfov;

			Matrix34 stereoMat = Matrix34::CreateTranslationMat(Vec3(nEye == CCamera::eEye_Left ? -modulatedHalfIpd : modulatedHalfIpd, 0, 0));
			cam.SetMatrix(cam.GetMatrix() * stereoMat);
			cam.SetFrustum(width, height, fov, n, f, 1.f);
			cam.SetAsymmetry(cameraSetup.l, cameraSetup.r, cameraSetup.b, cameraSetup.t);
		}
	}
	else
	{
		float fNear = cam.GetNearPlane();
		float screenDist = CRenderer::CV_r_StereoScreenDist;
		float z = 99999.0f;  // Point which is far away

		// standard 3D TV stereo projection parameters
		float wT = tanf(cam.GetFov() * 0.5f) * fNear;
		float wR = wT * cam.GetProjRatio(), wL = -wR;
		float p00 = 2 * fNear / (wR - wL);
		float p02 = (wL + wR) / (wL - wR);

		// Compute required camera shift, so that a distant point gets the desired maximum separation
		const float maxSeparation = CRenderer::CV_r_StereoEyeDist;
		const float camOffset = (maxSeparation - p02) / (p00 / z - p00 / screenDist);
		float frustumShift = camOffset * (fNear / screenDist);
		//frustumShift *= (nEye == CCamera::eEye_Left ? 1 : -1);  // Support postive and negative parallax for non-VR stereo

		Matrix34 stereoMat = Matrix34::CreateTranslationMat(Vec3(nEye == CCamera::eEye_Left ? -camOffset : camOffset, 0, 0));
		cam.SetMatrix(cam.GetMatrix() * stereoMat);
		cam.SetAsymmetry(frustumShift, frustumShift, 0, 0);
	}

	return cam;
}

void CD3DStereoRenderer::ProcessScene(int sceneFlags, const SRenderingPassInfo& passInfo)
{
	CRY_ASSERT(gRenDev->m_pRT->IsMainThread());

	// for recursive rendering (e.g. rendering to ocean reflection texture), stereo is not needed
	if (passInfo.IsRecursivePass() || (m_mode != EStereoMode::STEREO_MODE_DUAL_RENDERING))
	{
		gcpRendD3D->RenderFrame(sceneFlags, passInfo);
		return;
	}

	// Invoke RenderFrame with both eyes
	// We will prepare cameras in CD3DStereoRenderer::RenderScene when it is called by the render thread
	gcpRendD3D->RenderFrame(sceneFlags | SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE, passInfo);
}

void CD3DStereoRenderer::ClearEyes(ColorF clearColor)
{
	for (const auto &dc : m_pEyeDisplayContexts)
	{
		if (dc.first)
			dc.first->GetCurrentBackBuffer()->Clear(clearColor);
	}
}

void CD3DStereoRenderer::ClearVrQuads(ColorF clearColor)
{
	for (const auto &dc : m_pVrQuadLayerDisplayContexts)
	{
		if (dc.first)
			dc.first->GetCurrentBackBuffer()->Clear(clearColor);
	}
}

std::array<CCamera, 2> CD3DStereoRenderer::GetStereoCameras() const
{ 
	if (m_bPreviousCameraValid)
		return m_previousCameras;

	CCamera dummyCamera;
	if (auto *dc = GetEyeDisplayContext(CCamera::eEye_Left).first)
	{
		const CTexture *tex = dc->GetCurrentBackBuffer();

		Matrix44A mat;
		mathMatrixOrtho(&mat, static_cast<float>(tex->GetWidth()), static_cast<float>(tex->GetHeight()), 0, 1);

		dummyCamera.SetMatrix(Matrix34(mat));
		dummyCamera.SetFrustum(tex->GetWidth(), tex->GetHeight(), dummyCamera.GetFov(), 0, 1, 
			static_cast<float>(tex->GetHeight()) / static_cast<float>(tex->GetWidth()));
	}

	return { { dummyCamera, dummyCamera } };
}

SStereoRenderContext CD3DStereoRenderer::PrepareStereoRenderingContext(int nFlags, const SRenderingPassInfo& passInfo) const
{
	CRY_ASSERT(gRenDev->m_pRT->IsMainThread());

	SStereoRenderContext context = {};

	IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
	if (pHmdManager)
	{

		IHmdDevice* pHmdDevice = pHmdManager->GetHmdDevice();
		if (pHmdDevice)
			context.orientationForLateCameraInjection = pHmdDevice->GetOrientationForLateCameraInjection();
	}

	context.flags = nFlags;
	context.frameId = passInfo.GetFrameID();
	context.pRenderView = passInfo.GetRenderView();

	return context;
}

void CD3DStereoRenderer::PrepareFrame()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	if (!m_pHmdRenderer || m_framePrepared)
		return;

	CTimeValue timePresentBegin = gEnv->pTimer->GetAsyncTime();
	m_pHmdRenderer->PrepareFrame(static_cast<uint64_t>(gcpRendD3D->GetRenderFrameID()));
	gRenDev->m_fTimeWaitForGPU[gRenDev->GetRenderThreadID()] += gEnv->pTimer->GetAsyncTime().GetDifferenceInSeconds(timePresentBegin);

	m_framePrepared = true;
}

void CD3DStereoRenderer::RenderScene(const SStereoRenderContext &context)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());
	// ProcessScene provides both flags, we split this up into two render passes if we have to further below
	CRY_ASSERT(context.flags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE));

	CRenderView *pRenderView = context.pRenderView.get();
	CCamera camera = pRenderView->GetCamera(CCamera::eEye_Left);

	if (!m_pHmdRenderer)
		return;

	CRY_ASSERT_MESSAGE(!m_framePrepared, "Frame already prepared.");

	// Wait to begin frame, might block
	PrepareFrame();

	// Inject new tracking data, if possible, as late as possible, just before beginning the rendering pipeline
	const auto& trackingState = TryGetHmdCameraAsync(context);
	if (trackingState)
		camera.SetMatrix(trackingState.value());

	const std::array<CCamera, 2> cameras =
	{
		{
			PrepareCamera(CCamera::eEye_Left,  camera),
			PrepareCamera(CCamera::eEye_Right, camera)
		}
	};

	if (!m_bPreviousCameraValid)
	{
		m_previousCameras = cameras;
		m_bPreviousCameraValid = true;
	}

	pRenderView->SetCameras(cameras.data(), 2);
	pRenderView->SetPreviousFrameCameras(m_previousCameras.data(), 2);

	// Render
	if (!RequiresSequentialSubmission())
	{
		// both eyes
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//gcpRendD3D->SelectGPU(GPUMASK_LEFT);
			}

			gcpRendD3D->PushProfileMarker("DUAL_EYES");
			pRenderView->SetShaderRenderingFlags(context.flags);
			gcpRendD3D->RT_RenderScene(pRenderView);
			gcpRendD3D->PopProfileMarker("DUAL_EYES");
		}
	}
	else 
	{
		const int sceneFlagsLeft = context.flags & ~SHDF_STEREO_RIGHT_EYE;
		int sceneFlagsRight = (context.flags & ~SHDF_STEREO_LEFT_EYE) | SHDF_NO_SHADOWGEN;

		if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
		{
			// Render shadows on both GPUs
			sceneFlagsRight &= ~SHDF_NO_SHADOWGEN;

#if !defined(_RELEASE)
			// Divide the cascades across the GPUs
			//??AC CRenderer::CV_r_ShadowGenMode = 1;
#endif
		}

		// Left eye
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//gcpRendD3D->SelectGPU(GPUMASK_LEFT);
			}

			auto eyeRenderScope = PrepareRenderingToEye(CCamera::eEye_Left);

			// Manually call prepresent and postpresent on the eye's display contexts, as RT_EndFrame and friends are not being called.
			gcpRendD3D->GetActiveDisplayContext()->PrePresent();

			pRenderView->SetShaderRenderingFlags(sceneFlagsLeft);
			gcpRendD3D->RT_RenderScene(pRenderView);

			gcpRendD3D->GetActiveDisplayContext()->PostPresent();
		}

		// Right eye
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//gcpRendD3D->SelectGPU(GPUMASK_RIGHT);
			}

			auto eyeRenderScope = PrepareRenderingToEye(CCamera::eEye_Right);

			// Manually call prepresent and postpresent on the eye's display contexts, as RT_EndFrame and friends are not being called.
			gcpRendD3D->GetActiveDisplayContext()->PrePresent();

			pRenderView->SetShaderRenderingFlags(sceneFlagsRight);
			gcpRendD3D->RT_RenderScene(pRenderView);

			gcpRendD3D->GetActiveDisplayContext()->PostPresent();
		}
	}

	if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
	{
		//gcpRendD3D->SelectGPU(GPUMASK_BOTH);
	}

	m_previousCameras = cameras;
}

void CD3DStereoRenderer::SubmitFrameToHMD()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	// When unloading level, m_bDeviceLost is set to 2
	if (!m_pHmdRenderer || !m_framePrepared || gcpRendD3D->m_bDeviceLost) 
		return;

	m_pHmdRenderer->SubmitFrame();

	m_needClearVrQuadLayer = true;
	m_framePrepared = false;
}

std::pair<EHmdSocialScreen, EHmdSocialScreenAspectMode> CD3DStereoRenderer::GetSocialScreenType() const
{
	const auto& pHmdSocialScreenKeepAspectCVar = gEnv->pConsole->GetCVar("hmd_social_screen_aspect_mode");
	const auto& pHmdSocialScreenCVar = gEnv->pConsole->GetCVar("hmd_social_screen");

	EHmdSocialScreenAspectMode aspectMode = static_cast<EHmdSocialScreenAspectMode>(pHmdSocialScreenKeepAspectCVar->GetIVal());
	EHmdSocialScreen socialScreenType = EHmdSocialScreen::DistortedDualImage;
	if (pHmdSocialScreenCVar->GetIVal() >= -1 && pHmdSocialScreenCVar->GetIVal() < static_cast<int>(EHmdSocialScreen::FirstInvalidIndex))
		socialScreenType = static_cast<EHmdSocialScreen>(pHmdSocialScreenCVar->GetIVal());

	return std::make_pair(socialScreenType, aspectMode);
}

void CD3DStereoRenderer::DisplaySocialScreen()
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	if (!IsStereoEnabled() || gcpRendD3D->m_bDeviceLost)  // When unloading level, m_bDeviceLost is set to 2
		return;

	CRenderDisplayContext* pDisplayContext = gcpRendD3D->GetActiveDisplayContext();
	const Vec2i displayResolution = pDisplayContext->GetDisplayResolution();
	CTexture* pColorTarget = pDisplayContext->GetRenderOutput()->GetColorTarget();

	const auto &type = GetSocialScreenType();
	if (type.first == EHmdSocialScreen::Off || !m_pHmdRenderer)
	{
		CClearSurfacePass::Execute(pColorTarget, ColorF(0.1f, 0.1f, 0.1f, 1.0f));
		return;
	}

	RenderSocialScreen(pColorTarget, type, displayResolution);
}
void CD3DStereoRenderer::RenderSocialScreen(CTexture* pColorTarget, const std::pair<EHmdSocialScreen, EHmdSocialScreenAspectMode> &socialScreenType, const Vec2i &displayResolutions)
{
	CRY_ASSERT(m_pHmdRenderer);

	if (!m_eyeToScreenPass || !m_quadLayerPass)
	{
		m_eyeToScreenPass = stl::make_unique<CFullscreenPass>();
		m_quadLayerPass = stl::make_unique<CFullscreenPass>();
	}

	const auto socialScreen = socialScreenType.first;
	const auto aspectMode = socialScreenType.second;

	const bool useMirrorTexture = socialScreen == EHmdSocialScreen::DistortedDualImage;
	const bool shouldRenderBothEyes = socialScreen == EHmdSocialScreen::UndistortedDualImage || socialScreen == EHmdSocialScreen::DistortedDualImage;

	CCamera::EEye eyesToRender[2] = { CCamera::EEye::eEye_Left, CCamera::EEye::eEye_eCount };
	if (socialScreen == EHmdSocialScreen::UndistortedRightEye)
		eyesToRender[0] = CCamera::EEye::eEye_Right;
	else if (shouldRenderBothEyes)
		eyesToRender[1] = CCamera::EEye::eEye_Right;

	Vec2 targetSize;
	targetSize.x = static_cast<float>(pColorTarget->GetWidth()) * (shouldRenderBothEyes ? 0.5f : 1.0f);
	targetSize.y = static_cast<float>(pColorTarget->GetHeight());

	// Rescale utility function
	const auto rescaleViewport = [&](const Vec2_tpl<uint32_t> &size) -> D3D11_RECT
	{
		float w = static_cast<float>(size.x);
		float h = static_cast<float>(size.y);

		Vec2 srcToTargetScale;
		srcToTargetScale.x = targetSize.x / w;
		srcToTargetScale.y = targetSize.y / h;

		if (aspectMode != EHmdSocialScreenAspectMode::Stretch)
		{
			float s = aspectMode == EHmdSocialScreenAspectMode::Center ? std::min(srcToTargetScale.x, srcToTargetScale.y) : std::max(srcToTargetScale.x, srcToTargetScale.y);

			srcToTargetScale.x = s;
			srcToTargetScale.y = s;
		}

		D3D11_RECT result;
		result.left = static_cast<int32_t>(0);
		result.top = static_cast<int32_t>(0);
		result.right = static_cast<int32_t>(result.left + w * srcToTargetScale.x);
		result.bottom = static_cast<int32_t>(result.top + h * srcToTargetScale.y);

		// shift to center
		Vec2i emptySpace;
		emptySpace.x = static_cast<int32_t>(targetSize.x - (result.right - result.left));
		emptySpace.y = static_cast<int32_t>(targetSize.y - (result.bottom - result.top));
		emptySpace /= 2;

		result.right +=  emptySpace.x;
		result.left +=   emptySpace.x;
		result.top +=    emptySpace.y;
		result.bottom += emptySpace.y;

		return result;
	};

	// Clear
	CClearSurfacePass::Execute(pColorTarget, Clr_Empty);

	if (!CShaderMan::s_shPostEffects)
		return;

	// Prepare social screen passes
	static const CCryNameTSCRC techniqueNameSimple("TextureToTexture");
	static const CCryNameTSCRC techniqueNameRegion("TextureToTextureReg");
	static const CCryNameR vsRegionParamName("texToTexParamsTC");

	m_eyeToScreenPass->SetTechnique(CShaderMan::s_shPostEffects, techniqueNameRegion, 0);
	m_eyeToScreenPass->SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuadCentered);
	m_eyeToScreenPass->SetPrimitiveFlags(CRenderPrimitive::eFlags_ReflectShaderConstants_VS);
	m_eyeToScreenPass->SetState(GS_NODEPTHTEST);

	m_quadLayerPass->SetTechnique(CShaderMan::s_shPostEffects, techniqueNameSimple, 0);
	m_quadLayerPass->SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	m_quadLayerPass->SetPrimitiveFlags(CRenderPrimitive::eFlags_None);
	m_quadLayerPass->SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	// Draw eyes' outputs
	int32_t x = 0;
	for (const auto &eye : eyesToRender)
	{
		if (eye == CCamera::EEye::eEye_eCount)
			continue;
		auto* displayContext = GetEyeDisplayContext(eye).first;
		if (!displayContext)
			continue;

		// Calculate viewport rect and scissor area
		const auto rect = rescaleViewport(displayContext->GetDisplayResolution());
		D3DRectangle scissor = { x, 0, x + static_cast<uint32_t>(targetSize.x), static_cast<uint32_t>(targetSize.y) };

		// draw eye
		m_eyeToScreenPass->SetCustomViewport(SRenderViewport{ x + rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top });
		m_eyeToScreenPass->SetScissor(true, scissor);
		m_eyeToScreenPass->SetRenderTarget(0, pColorTarget);
		if (useMirrorTexture)
		{
			// Render mirror
			const auto& mirrorPair = m_pHmdRenderer->GetMirrorTexture(static_cast<EEyeType>(eye));
			if (!mirrorPair.first)
				continue;

			m_eyeToScreenPass->SetTextureSamplerPair(0, mirrorPair.first, EDefaultSamplerStates::LinearClamp);

			m_eyeToScreenPass->BeginConstantUpdate();
			m_eyeToScreenPass->SetConstant(vsRegionParamName, mirrorPair.second, eHWSC_Vertex);

			m_eyeToScreenPass->Execute();
		}
		else if (CTexture* pSourceTexture = displayContext->GetCurrentBackBuffer())
		{
			// Render eye target
			m_eyeToScreenPass->SetTextureSamplerPair(0, pSourceTexture, EDefaultSamplerStates::LinearClamp);

			m_eyeToScreenPass->BeginConstantUpdate();
			m_eyeToScreenPass->SetConstant(vsRegionParamName, Vec4(0, 0, 1, 1), eHWSC_Vertex);

			m_eyeToScreenPass->Execute();

			// now draw quad layers
			for (int i = RenderLayer::eQuadLayers_0; i < RenderLayer::eQuadLayers_Total; ++i)
			{
				if (!m_pHmdRenderer->GetQuadLayerProperties(RenderLayer::EQuadLayers(i))->IsActive())
					continue;

				if (auto* displayContext = GetVrQuadLayerDisplayContext(RenderLayer::EQuadLayers(i)).first)
				{
					CTexture* pQuadTex = displayContext->GetCurrentBackBuffer();
					if (!pQuadTex)
						continue;

					const auto rect = rescaleViewport(displayContext->GetDisplayResolution());

					m_quadLayerPass->SetCustomViewport(SRenderViewport{ x + rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top });
					m_quadLayerPass->SetScissor(true, scissor);
					m_quadLayerPass->SetRenderTarget(0, pColorTarget);
					m_quadLayerPass->SetTextureSamplerPair(0, pQuadTex, EDefaultSamplerStates::LinearClamp);

					m_quadLayerPass->Execute();
				}
			}
		}

		x += static_cast<int32_t>(targetSize.x);
	}
}

bool CD3DStereoRenderer::TakeScreenshot(const char path[])
{
	stack_string pathL(path);
	stack_string pathR(path);

	pathL.insert(pathL.find('.'), "_L");
	pathR.insert(pathR.find('.'), "_R");

	const auto *leftDc = GetEyeDisplayContext(CCamera::eEye_Left).first;
	const auto *rightDc = GetEyeDisplayContext(CCamera::eEye_Right).first;

	if (!leftDc || !rightDc)
		return false;

	auto leftSrc = leftDc->GetCurrentBackBuffer();
	auto rightSrc = rightDc->GetCurrentBackBuffer();
	CRY_ASSERT(leftSrc && rightSrc);

	bool bResult = true;
	bResult &= ((CD3D9Renderer*) gRenDev)->RT_StoreTextureToFile(pathL.c_str(), leftSrc);
	bResult &= ((CD3D9Renderer*) gRenDev)->RT_StoreTextureToFile(pathR.c_str(), rightSrc);

	return bResult;
}

void CD3DStereoRenderer::NotifyFrameFinished()
{}

CStereoRenderingScope CD3DStereoRenderer::PrepareRenderingToEye(CCamera::EEye eye)
{
	const auto profileTag = eye == CCamera::eEye_Left ? "LEFT_EYE" : "RIGHT_EYE";

	return CStereoRenderingScope{ profileTag, m_pEyeDisplayContexts[eye].second };
}

CStereoRenderingScope CD3DStereoRenderer::PrepareRenderingToVrQuadLayer(RenderLayer::EQuadLayers id)
{
	const auto profileTag = "STEREO_QUAD";

	const auto &dc = m_pVrQuadLayerDisplayContexts[id];
	if (dc.first)
	{
		CTexture* pTexture = dc.first->GetCurrentBackBuffer();;
		if (pTexture && m_pHmdRenderer)
		{
			if (RenderLayer::CProperties* pLayer = m_pHmdRenderer->GetQuadLayerProperties(id))
				pLayer->SetActive(true);

			if (m_needClearVrQuadLayer)
			{
				CClearSurfacePass::Execute(pTexture, Clr_Transparent);
				m_needClearVrQuadLayer = false;
			}
		}
	}

	return CStereoRenderingScope{ profileTag, dc.second };
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRenderer Interface

void CD3DStereoRenderer::GetInfo(EStereoDevice* device, EStereoMode* mode, EStereoOutput* output, EStereoDeviceState* state) const
{
	if (device) *device = m_device;
	if (mode)   *mode = m_mode;
	if (output) *output = m_output;
	if (state)  *state = m_deviceState;
}

void CD3DStereoRenderer::GetNVControlValues(bool& stereoActivated, float& stereoStrength) const
{
	stereoActivated = m_nvStereoActivated != 0;
	stereoStrength = m_nvStereoStrength;
}

void CD3DStereoRenderer::ReleaseBuffers()
{
	if (m_pHmdRenderer)
		m_pHmdRenderer->ReleaseBuffers();
}

void CD3DStereoRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_device != EStereoDevice::STEREO_DEVICE_NONE && m_pHmdRenderer)
		m_pHmdRenderer->OnResolutionChanged(newWidth, newHeight);

	m_headlockedQuadCamera = {};
	m_headlockedQuadCamera.SetFrustum(newWidth, newHeight);
}

void CD3DStereoRenderer::CalculateResolution(int requestedWidth, int requestedHeight, int* pDisplayWidth, int *pDisplayHeight)
{
	switch (m_output)
	{
		case EStereoOutput::STEREO_OUTPUT_SIDE_BY_SIDE:
			*pDisplayWidth  = requestedWidth * 2;
			*pDisplayHeight = requestedHeight;
			break;
		case EStereoOutput::STEREO_OUTPUT_ABOVE_AND_BELOW:
			*pDisplayWidth  = requestedWidth;
			*pDisplayHeight = requestedHeight * 2;
			break;
		case EStereoOutput::STEREO_OUTPUT_HMD:
			if (IHmdDevice* pHmdDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
			{
				// Update renderer resolution
				HmdDeviceInfo deviceInfo;
				pHmdDevice->GetDeviceInfo(deviceInfo);

				const float resolutionScale = gEnv->pConsole->GetCVar("hmd_resolution_scale")->GetFVal();
				const int screenWidth  = (int)floor(float(deviceInfo.screenWidth / 2) * resolutionScale);
				const int screenHeight = (int)floor(float(deviceInfo.screenHeight   ) * resolutionScale);

				*pDisplayWidth  = screenWidth;
				*pDisplayHeight = screenHeight;
			}
			else
			{
				*pDisplayWidth  = requestedWidth;
				*pDisplayHeight = requestedHeight;
			}
			break;
		default:
			*pDisplayWidth  = requestedWidth;
			*pDisplayHeight = requestedHeight;
			break;
	}
}

void CD3DStereoRenderer::OnHmdDeviceChanged(IHmdDevice* pHmdDevice)
{
	if (!pHmdDevice)
	{
		if (m_pHmdRenderer)
			m_pHmdRenderer->ReleaseBuffers();
		ShutdownHmdRenderer();
	}

	if (m_output == EStereoOutput::STEREO_OUTPUT_HMD && gEnv->pSystem->GetHmdManager()->GetHmdDevice() == nullptr)
	{
		m_output = EStereoOutput::STEREO_OUTPUT_STANDARD;
		m_device = EStereoDevice::STEREO_DEVICE_NONE;

		return;
	}

	// Make sure PrepareStereo is called in Update by specifying a device
	SelectDefaultDevice();
}

IHmdRenderer* CD3DStereoRenderer::CreateHmdRenderer(IHmdDevice& device)
{
#if defined(INCLUDE_VR_RENDERING)
	switch (device.GetClass())
	{
	case eHmdClass_Oculus:
		return new CD3DOculusRenderer(static_cast<CryVR::Oculus::IOculusDevice*>(&device), gcpRendD3D, this);
	case eHmdClass_OpenVR:
		return new CD3DOpenVRRenderer(static_cast<CryVR::OpenVR::IOpenVRDevice*>(&device), gcpRendD3D, this);
	case eHmdClass_Osvr:
		return new CD3DOsvrRenderer(static_cast<CryVR::Osvr::IOsvrDevice*>(&device), gcpRendD3D, this);
	default:
		iLog->LogError("Tried to create HMD renderer for unknown headset!");
		break;
	}
#else
	iLog->LogError("Tried to create HMD renderer with VR rendering support disabled for renderer at compile-time!");
#endif

	return nullptr;
}

void CD3DStereoRenderer::ReleaseDevice()
{
	// Make sure hmdrenderer isn't in use
	gcpRendD3D->FlushRTCommands(true, true, true);

	// Disable stereo
	DisableStereo();
	m_mode = EStereoMode::STEREO_MODE_NO_STEREO;
	m_output = EStereoOutput::STEREO_OUTPUT_STANDARD;
}

//////////////////////////////////////////////////////////////////////////
stl::optional<Matrix34> CD3DStereoRenderer::TryGetHmdCameraAsync(const SStereoRenderContext &context)
{
	auto hmd_post_inject_camera = gEnv->pConsole->GetCVar("hmd_post_inject_camera");
	if (!hmd_post_inject_camera || !hmd_post_inject_camera->GetIVal() || !context.orientationForLateCameraInjection)
		return stl::nullopt;

	IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
	if (!pHmdManager)
		return stl::nullopt;
	IHmdDevice* pHmdDevice = pHmdManager->GetHmdDevice();
	if (!pHmdDevice)
		return stl::nullopt;

	// Query async tracking state
	return pHmdDevice->RequestAsyncCameraUpdate(context.frameId, 
		context.orientationForLateCameraInjection.value().first, 
		context.orientationForLateCameraInjection.value().second);
}

//////////////////////////////////////////////////////////////////////////
_smart_ptr<CTexture> CD3DStereoRenderer::WrapD3DRenderTarget(D3DTexture* d3dTexture, uint32 width, uint32 height, DXGI_FORMAT format, const char* name, bool shaderResourceView)
{
	ETEX_Format texFormat = DeviceFormats::ConvertToTexFormat(format);
	_smart_ptr<CTexture> texture = CTexture::GetOrCreateTextureObjectPtr(name, width, height, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, texFormat);
	if (texture == nullptr)
	{
		gEnv->pLog->Log("[HMD][Oculus] Unable to create texture object!");
		return nullptr;
	}

	// CTexture::CreateTextureObject does not set width and height if the texture already existed
	if (texture->GetWidth() != width || texture->GetHeight() != height)
	{
		texture->SetWidth(width);
		texture->SetHeight(height);
	}
	assert(texture->GetDepth() == 1);

	d3dTexture->AddRef();

	texture->SRGBRead(DeviceFormats::ConvertToSRGB(format) == format);
	texture->SetDevTexture(CDeviceTexture::Associate(texture->GetLayout(), d3dTexture));
	texture->SetClosestFormatSupported();

	return texture;
}