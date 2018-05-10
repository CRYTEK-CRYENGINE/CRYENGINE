// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "D3DStereo.h"
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

CD3DStereoRenderer::CD3DStereoRenderer(CD3D9Renderer& renderer, EStereoDevice device)
	: m_renderer(renderer)
	, m_device(device)
	, m_deviceState(STEREO_DEVSTATE_UNSUPPORTED_DEVICE)
	, m_mode(STEREO_MODE_NO_STEREO)
	, m_output(STEREO_OUTPUT_STANDARD)
	, m_submission(STEREO_SUBMISSION_SEQUENTIAL)
	, m_driver(DRIVER_UNKNOWN)
	, m_nvStereoHandle(nullptr)
	, m_nvStereoStrength(0.0f)
	, m_nvStereoActivated(0)
	, m_curEye(LEFT_EYE)
	, m_stereoStrength(0.0f)
	, m_zeroParallaxPlaneDist(0.25f)
	, m_maxSeparationScene(0.0f)
	, m_nearGeoScale(0.0f)
	, m_gammaAdjustment(0)
	, m_screenSize(0.0f)
	, m_systemCursorNeedsUpdate(false)
	, m_resourcesPatched(false)
	, m_needClearLeft(true)
	, m_needClearRight(true)
	, m_needClearVrQuadLayer(true)
	, m_pHmdRenderer(nullptr)
	, m_pHmdDevice(nullptr)
	, m_bAsyncCameraMatrixValid(false)
	, m_asyncCameraMatrix(IDENTITY)
	, m_bPreviousCameraValid(false)
{
	if (device == STEREO_DEVICE_DEFAULT)
		SelectDefaultDevice();

	memset(m_pVrQuadLayerTex, 0, sizeof(m_pVrQuadLayerTex));
	m_bDisplayStereoDone = false;

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

	m_pSideTexs[LEFT_EYE ] = nullptr;
	m_pSideTexs[RIGHT_EYE] = nullptr;
}

CD3DStereoRenderer::~CD3DStereoRenderer()
{
	Shutdown();
}

void CD3DStereoRenderer::SelectDefaultDevice()
{
	m_device = STEREO_DEVICE_FRAMECOMP;
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

	if (!availableDriverNV && m_device == STEREO_DEVICE_DRIVER && CRenderer::CV_r_StereoDevice == STEREO_DEVICE_DEFAULT)
	{
		// Fall back to frame-compatible device
		m_device = STEREO_DEVICE_FRAMECOMP;
	}
#endif

	if (m_device == STEREO_DEVICE_NONE)
		return;

	bool success = true;
	m_deviceState = STEREO_DEVSTATE_UNSUPPORTED_DEVICE;

#if defined(USE_NV_API)
	if (m_device == STEREO_DEVICE_DRIVER)
	{
		if (availableDriverNV)
		{
			m_driver = DRIVER_NV;
		}
		else
		{
			success = false;
			m_deviceState = STEREO_DEVSTATE_BAD_DRIVER;
		}
	}
#elif CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	if (m_device != STEREO_DEVICE_FRAMECOMP)
		success = false;
#endif

	if (success)
	{
		m_deviceState = STEREO_DEVSTATE_OK;
	}
	else
	{
		iLog->LogError("Failed to create stereo device");
		m_device = STEREO_DEVICE_NONE;
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
			device = m_renderer.GetDevice().GetRealDevice();
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
		D3DDevice* device = m_renderer.GetDevice().GetRealDevice();

		if (NvAPI_Stereo_CreateHandleFromIUnknown(device, &pStereoHandle) == NVAPI_OK)
			m_nvStereoHandle = pStereoHandle;
		else
			m_device = STEREO_DEVICE_NONE;

		DisableStereo();  // Always disabled at startup
	}

	g_pMgpu = NULL;
#endif

	if (m_renderer.m_DeviceWrapper.GetNodeCount() & 2)
	{
		m_submission = STEREO_SUBMISSION_PARALLEL;

		if (m_renderer.m_DeviceWrapper.GetNodeCount() & 4)
		{
			m_submission = STEREO_SUBMISSION_PARALLEL_MULTIRES;
		}
	}
}

void CD3DStereoRenderer::CreateResources()
{
	if (m_device == STEREO_DEVICE_NONE)
		return;

	m_SourceSizeParamName = CCryNameR("SourceSize");

	CreateIntermediateBuffers(
		CRendererResources::s_displayWidth,
		CRendererResources::s_displayHeight);
}

void CD3DStereoRenderer::CreateIntermediateBuffers(int nWidth, int nHeight)
{
	if (m_device == STEREO_DEVICE_NONE)
		return;

	if (m_submission > STEREO_SUBMISSION_SEQUENTIAL)
	{
		// Allocate the following buffers with visibility to all nodes (to be able to copy them around)
		m_renderer->GetDevice_Unsynchronized().SwitchNodeVisibility(~UINT(0));
	}

#if (CRY_RENDERER_DIRECT3D >= 120) && DX12_LINKEDADAPTER
	// NOTE: Workaround for missing MultiGPU-support in the HMD libraries
	if (m_renderer->GetDevice().GetNodeCount() > 1)
	{
		const uint32 nFlags = FT_DONT_STREAM | FT_USAGE_RENDERTARGET;

		CRendererResources::s_ptexStereoL = CTexture::GetOrCreateRenderTarget("$StereoL", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF_R8G8B8A8);
		CRendererResources::s_ptexStereoR = CTexture::GetOrCreateRenderTarget("$StereoR", nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF_R8G8B8A8);

		m_pSideTexs[LEFT_EYE ] = CRendererResources::s_ptexStereoL;
		m_pSideTexs[RIGHT_EYE] = CRendererResources::s_ptexStereoR;

		for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
		{
			char textureName[16];
			cry_sprintf(textureName, "$QuadLayer0_%d", i);

			CRendererResources::s_ptexQuadLayers[i] = CTexture::GetOrCreateRenderTarget(textureName, nWidth, nHeight, Clr_Empty, eTT_2D, nFlags, eTF_R8G8B8A8);
		}
	}
#endif

	if (m_submission > STEREO_SUBMISSION_SEQUENTIAL)
	{
		m_renderer->GetDevice_Unsynchronized().SwitchNodeVisibility(0UL);
	}
}

void CD3DStereoRenderer::Shutdown()
{
	if(m_pHmdRenderer)
		m_pHmdRenderer->ReleaseBuffers();

	SAFE_DELETE(m_pHmdRenderer);

	ReleaseResources();

	OnHmdDeviceChanged(nullptr);

#if defined(USE_NV_API)
	if (m_nvStereoHandle)
	{
		NvAPI_Stereo_DestroyHandle(m_nvStereoHandle);
		m_nvStereoHandle = nullptr;
	}
#endif
}

void CD3DStereoRenderer::ReleaseResources()
{
	if (m_device == STEREO_DEVICE_NONE)
		return;

	SAFE_RELEASE(CRendererResources::s_ptexStereoL);
	SAFE_RELEASE(CRendererResources::s_ptexStereoR);
}

bool CD3DStereoRenderer::EnableStereo()
{
#if defined(USE_NV_API)
	m_systemCursorNeedsUpdate = true;

	if (IsDriver(DRIVER_NV))
	{
		uint8 windowedSupport = 0;
		NvAPI_Stereo_IsWindowedModeSupported(&windowedSupport);

		if (m_renderer.IsFullscreen() || windowedSupport)
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
	m_systemCursorNeedsUpdate = true;

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
}

void CD3DStereoRenderer::PrepareStereo(EStereoMode mode, EStereoOutput output)
{
#if defined(USE_NV_API)
	if (m_systemCursorNeedsUpdate && gEnv->pHardwareMouse)
	{
		if (!gEnv->IsEditor() && !IsDriver(DRIVER_NV))
		{
			gEnv->pHardwareMouse->UseSystemCursor(IsStereoEnabled() == false);
		}
		m_systemCursorNeedsUpdate = false;
	}

	if (IsDriver(DRIVER_NV))
	{
		HandleNVControl();
	}
#endif

	if (m_mode != mode || m_output != output)
	{
		m_renderer.ForceFlushRTCommands();
		
		if (m_mode != mode)
		{
			m_mode = mode;
			m_output = output;

			if (mode == STEREO_MODE_NO_STEREO)
			{
				DisableStereo();
			}
			else
			{
				EnableStereo();
				ChangeOutputFormat();
			}
		}
		else if (m_output != output)
		{
			m_output = output;

			if (IsStereoEnabled())
				ChangeOutputFormat();
		}
	}

	if (IsStereoEnabled())
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

		float screenDiagonal  = GetScreenDiagonalInInches();
		if (screenDiagonal > 0.0f) // override CVar if we can determine the correct maximum separation
		{
			float aspect = 9.0f / 16.0f;
			float horizontal = screenDiagonal / sqrtf(1.0f + aspect * aspect);
			float typicalEyeSeperation = 2.5f;                                                   // In inches
			m_maxSeparationScene = min(typicalEyeSeperation / horizontal, m_maxSeparationScene); // Get bleeding at edges so limit to CVar
		}

		// Apply stereo strength
		m_maxSeparationScene *= m_stereoStrength;

		if (m_output == STEREO_OUTPUT_HMD && m_pHmdRenderer == nullptr && m_pHmdDevice != nullptr)
		{
			// Create the HMD renderer
			m_pHmdRenderer = CreateHmdRenderer(*m_pHmdDevice, &m_renderer, this);

			if (m_pHmdRenderer != nullptr)
			{
				m_pHmdRenderer->Initialize(
					CRendererResources::s_displayWidth,
					CRendererResources::s_displayHeight);
			}
		}

		if (m_pHmdRenderer != nullptr)
			m_pHmdRenderer->PrepareFrame();
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

void CD3DStereoRenderer::SetEyeTextures(CTexture* leftTex, CTexture* rightTex)
{
	m_pSideTexs[LEFT_EYE ] = leftTex  ? leftTex  : CRendererResources::s_ptexStereoL;
	m_pSideTexs[RIGHT_EYE] = rightTex ? rightTex : CRendererResources::s_ptexStereoR;
}

void CD3DStereoRenderer::SetVrQuadLayerTexture(RenderLayer::EQuadLayers id, CTexture* pTexture)
{
	m_pVrQuadLayerTex[id] = pTexture;
}

void CD3DStereoRenderer::Update()
{
	if (m_device != STEREO_DEVICE_NONE)
	{
		gRenDev->ExecuteRenderThreadCommand(
			[=]
			{
				if (gRenDev->m_pRT->m_eVideoThreadMode == SRenderThread::eVTM_Disabled)
					this->PrepareStereo((EStereoMode)CRenderer::CV_r_StereoMode, (EStereoOutput)CRenderer::CV_r_StereoOutput);
			},
			ERenderCommandFlags::None
		);
	}
	else
	{
		static int prevMode = 0;
		if (CRenderer::CV_r_StereoMode != prevMode)
		{
			LogWarning("No stereo device enabled, ignoring stereo mode");
			prevMode = CRenderer::CV_r_StereoMode;
		}
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
			IHmdManager::SAsymmetricCameraSetupInfo asymCamSetupInfo;
			if (pHmdManager->GetAsymmetricCameraSetupInfo(nEye, asymCamSetupInfo))
			{
				// Note:
				// The inter-pupilar distance (IPD) gets modulated by r_stereoScaleCoefficient in order to be able
				// to "scale" the feeling of the player's size.
				// It also modifies the movement of the player's pose tracking

				const float modulatedHalfIpd = asymCamSetupInfo.eyeDist * 0.5f * CRenderer::CV_r_stereoScaleCoefficient;
				const float transfAsymH = asymCamSetupInfo.asymH * cam.GetNearPlane();
				const float transfAsymV = asymCamSetupInfo.asymV * cam.GetNearPlane();

				Matrix34 stereoMat = Matrix34::CreateTranslationMat(Vec3(nEye == LEFT_EYE ? -modulatedHalfIpd : modulatedHalfIpd, 0, 0));
				cam.SetMatrix(cam.GetMatrix() * stereoMat);
				cam.SetFrustum(1, 1, asymCamSetupInfo.fov, cam.GetNearPlane(), cam.GetFarPlane(), 1.0f / asymCamSetupInfo.aspectRatio);
				cam.SetAsymmetry(transfAsymH, transfAsymH, transfAsymV, transfAsymV);
			}
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
		//frustumShift *= (nEye == LEFT_EYE ? 1 : -1);  // Support postive and negative parallax for non-VR stereo

		Matrix34 stereoMat = Matrix34::CreateTranslationMat(Vec3(nEye == LEFT_EYE ? -camOffset : camOffset, 0, 0));
		cam.SetMatrix(cam.GetMatrix() * stereoMat);
		cam.SetAsymmetry(frustumShift, frustumShift, 0, 0);
	}

	return cam;
}

void CD3DStereoRenderer::ProcessScene(int sceneFlags, const SRenderingPassInfo& passInfo)
{
	// for recursive rendering (e.g. rendering to ocean reflection texture), stereo is not needed
	if (passInfo.IsRecursivePass() || (CRenderer::CV_r_StereoMode != STEREO_MODE_DUAL_RENDERING))
	{
		RenderScene(sceneFlags, passInfo);
		return;
	}

	int nThreadID = passInfo.ThreadID();
	CRenderView* pRenderView = passInfo.GetRenderView();

	std::array<CCamera, 2> cameras =
	{
		{
			PrepareCamera(LEFT_EYE,  passInfo.GetCamera()),
			PrepareCamera(RIGHT_EYE, passInfo.GetCamera())
		}
	};

	if (!m_bPreviousCameraValid)
	{
		m_previousCameras = cameras;
		m_bPreviousCameraValid = true;
	}

	if (!RequiresSequentialSubmission())
	{
		int sceneFlagsDual = SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE;

		// both eyes
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//m_renderer->SelectGPU(GPUMASK_LEFT);
			}

			m_renderer.PushProfileMarker("DUAL_EYES");

			pRenderView->SetCameras(cameras.data(), 2);
			pRenderView->SetPreviousFrameCameras(m_previousCameras.data(), 2);

			RenderScene(sceneFlags | sceneFlagsDual, passInfo);

			m_renderer.PopProfileMarker("DUAL_EYES");
		}

		if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
		{
			//m_renderer->SelectGPU(GPUMASK_BOTH);
		}
	}
	else 
	{
		int sceneFlagsLeft = SHDF_STEREO_LEFT_EYE;
		int sceneFlagsRight = SHDF_STEREO_RIGHT_EYE | SHDF_NO_SHADOWGEN;

		if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
		{
			// Render shadows on both GPUs
			sceneFlagsLeft = 0;
			sceneFlagsRight = 0;
		}

		// Left eye
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//m_renderer->SelectGPU(GPUMASK_LEFT);
			}

			m_renderer.PushProfileMarker("LEFT_EYE");

			pRenderView->SetCameras(&cameras[LEFT_EYE], 1);
			pRenderView->SetPreviousFrameCameras(&m_previousCameras[LEFT_EYE], 1);

			RenderScene(sceneFlags | sceneFlagsLeft, passInfo);

			m_renderer.PopProfileMarker("LEFT_EYE");
		}

		// Right eye
		{
			if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
			{
				//m_renderer->SelectGPU(GPUMASK_RIGHT);
			}

			m_renderer.PushProfileMarker("RIGHT_EYE");

			pRenderView->SetCameras(&cameras[RIGHT_EYE], 1);
			pRenderView->SetPreviousFrameCameras(&m_previousCameras[RIGHT_EYE], 1);

			RenderScene(sceneFlags | sceneFlagsRight, passInfo);

			m_renderer.PopProfileMarker("RIGHT_EYE");
		}

		if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
		{
			//m_renderer->SelectGPU(GPUMASK_BOTH);
		}
	}

	m_previousCameras = cameras;
}

void CD3DStereoRenderer::SubmitFrameToHMD()
{
	assert(IsRenderThread());

	if (!IsStereoEnabled() || m_renderer.m_bDeviceLost)  // When unloading level, m_bDeviceLost is set to 2
		return;

	if (m_pHmdRenderer != nullptr)
	{
		m_pHmdRenderer->SubmitFrame();
		m_pHmdRenderer->RenderSocialScreen();

		m_needClearVrQuadLayer = true;
	}
}

void CD3DStereoRenderer::DisplayStereo()
{
	assert(IsRenderThread());

	m_bDisplayStereoDone = true;

	if (!IsStereoEnabled() || m_renderer.m_bDeviceLost)  // When unloading level, m_bDeviceLost is set to 2
		return;

	// NOTE: Needs to be executed unconditionally if missing MultiGPU-support in the HMD libraries
	// Otherwise it could possibly be skipped by "m_pHmdRenderer != nullptr"
	if (m_submission > STEREO_SUBMISSION_SEQUENTIAL)
	{
		// NOTE: Magic number to allow MultiGPU driver debugging
		if (CRenderer::CV_r_StereoEnableMgpu != 111)
		{
			// Left eye contains both eye views, copy the content crossed into right eye
			// Left eye contains now L+R and right eye contains R+L
			m_renderer->GetDeviceContext().CopyResourceOvercross(
			  GetEyeTarget(RIGHT_EYE)->GetDevTexture()->GetBaseTexture(),
			  GetEyeTarget(LEFT_EYE)->GetDevTexture()->GetBaseTexture());
		}
	}

	if (m_pHmdRenderer != nullptr)
	{
		return;
	}

#ifdef ENABLE_BENCHMARK_SENSOR
	gcpRendD3D->m_benchmarkRendererSensor->PreStereoFrameSubmit(GetEyeTarget(LEFT_EYE), GetEyeTarget(RIGHT_EYE));
	gcpRendD3D->m_benchmarkRendererSensor->AfterStereoFrameSubmit();
#endif

	m_needClearLeft = true;
	m_needClearRight = true;

	CRenderDisplayContext* pDC = m_renderer->GetActiveDisplayContext();
	CTexture* pBB = pDC->GetCurrentBackBuffer();

	const int widthBB  = pDC->GetDisplayResolution()[0];
	const int heightBB = pDC->GetDisplayResolution()[1];

	const int width  = CRendererResources::s_displayWidth;
	const int height = CRendererResources::s_displayHeight;

	if ((m_device == STEREO_DEVICE_FRAMECOMP) && (m_output == STEREO_OUTPUT_SIDE_BY_SIDE))
	{
		const SResourceRegionMapping regionL = { { 0, 0, 0, 0 }, { 0           , 0, 0, 0 }, { width, height, 1, 1 } };
		const SResourceRegionMapping regionR = { { 0, 0, 0, 0 }, { widthBB >> 1, 0, 0, 0 }, { width, height, 1, 1 } };

		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(GetEyeTarget(LEFT_EYE )->GetDevTexture(), pBB->GetDevTexture(), regionL);
		GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(GetEyeTarget(RIGHT_EYE)->GetDevTexture(), pBB->GetDevTexture(), regionR);

		return;
	}

	// The following functions need 3DEngine
	if (!gEnv->p3DEngine)
		return;

	m_renderer.m_cEF.mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);

	PROFILE_LABEL_SCOPE("DISPLAY_STEREO");

	//TD legacy old multi-gpu support
	//if (g_pMgpu && CRenderer::CV_r_StereoEnableMgpu)
	//{
	//	//D3D11_RECT fullRect = { m_renderer->GetWidth()*0.5, 0, m_renderer->GetWidth(), m_renderer->GetHeight() };
	//	D3D11_RECT fullRect = {0, 0, m_renderer->GetWidth() / 2, m_renderer->GetHeight() };

	//	g_pMgpu->TransferResource(
	//		GetEyeTarget(LEFT_EYE)->GetDevTexture()->GetBaseTexture(),
	//		GetEyeTarget(LEFT_EYE)->GetDevTexture()->GetBaseTexture(),
	//		1,
	//		0,
	//		0,
	//		0,
	//		&fullRect);
	//}

	// TODO: Fix this properly
	//if (!gEnv->IsEditor())
//		m_renderer.RT_SetViewport(0, 0, widthBB, heightBB);

	ASSERT_LEGACY_PIPELINE
		/*
	CShader* pSH = m_renderer.m_cEF.s_ShaderStereo;
	SelectShaderTechnique();

	uint32 nPasses = 0;
	pSH->FXBegin(&nPasses, FEF_DONTSETSTATES);
	pSH->FXBeginPass(0);

	m_renderer.FX_SetActiveRenderTargets();
	m_renderer.FX_SetState(GS_NODEPTHTEST);

	Vec4 pParams = Vec4((float) width, (float) height, 0, 0);
	CShaderMan::s_shPostEffects->FXSetPSFloat(m_SourceSizeParamName, &pParams, 1);

	GetUtils().SetTexture(!CRenderer::CV_r_StereoFlipEyes ? GetEyeTarget(LEFT_EYE) : GetEyeTarget(RIGHT_EYE), 0, FILTER_LINEAR);
	GetUtils().SetTexture(!CRenderer::CV_r_StereoFlipEyes ? GetEyeTarget(RIGHT_EYE) : GetEyeTarget(LEFT_EYE), 1, FILTER_LINEAR);

	GetUtils().DrawFullScreenTri(width, height);

#if defined(USE_NV_API)
	if (IsDriver(DRIVER_NV) && m_nvStereoActivated)
	{
		NvAPI_Stereo_SetActiveEye(m_nvStereoHandle, NVAPI_STEREO_EYE_RIGHT);
		GetUtils().SetTexture(!CRenderer::CV_r_StereoFlipEyes ? GetEyeTarget(RIGHT_EYE) : GetEyeTarget(LEFT_EYE), 0, FILTER_LINEAR);
		GetUtils().DrawFullScreenTri(width, height);
		NvAPI_Stereo_SetActiveEye(m_nvStereoHandle, NVAPI_STEREO_EYE_LEFT);
	}
#endif

	pSH->FXEndPass();
	pSH->FXEnd();
	*/
}

void CD3DStereoRenderer::BeginRenderingMRT(bool disableClear)
{
	if (!IsStereoEnabled())
		return;

	if (disableClear)
	{
		m_needClearLeft = false;
		m_needClearRight = false;
	}

	PushRenderTargets();
}

void CD3DStereoRenderer::EndRenderingMRT(bool bResolve)
{
	if (!IsStereoEnabled())
		return;

	PopRenderTargets(bResolve);
}

bool CD3DStereoRenderer::TakeScreenshot(const char path[])
{
	stack_string pathL(path);
	stack_string pathR(path);

	pathL.insert(pathL.find('.'), "_L");
	pathR.insert(pathR.find('.'), "_R");

	bool bResult = true;
	bResult &= ((CD3D9Renderer*) gRenDev)->RT_StoreTextureToFile(pathL.c_str(), GetEyeTarget(LEFT_EYE));
	bResult &= ((CD3D9Renderer*) gRenDev)->RT_StoreTextureToFile(pathR.c_str(), GetEyeTarget(RIGHT_EYE));

	return bResult;
}

void CD3DStereoRenderer::NotifyFrameFinished()
{
	m_bDisplayStereoDone = false;
}

void CD3DStereoRenderer::BeginRenderingTo(StereoEye eye)
{
	m_curEye = eye;

	if (eye == LEFT_EYE)
	{
		//gRenDev->SetProfileMarker("LEFT_EYE", CRenderer::ESPM_PUSH);
		if (m_needClearLeft)
		{
			m_needClearLeft = false;
			CClearSurfacePass::Execute(GetEyeTarget(LEFT_EYE), Clr_Transparent);
		}
		//m_renderer.FX_PushRenderTarget(0, GetEyeTarget(LEFT_EYE), &m_renderer.m_DepthBufferOrig);
	}
	else
	{
		//gRenDev->SetProfileMarker("RIGHT_EYE", CRenderer::ESPM_PUSH);
		if (m_needClearRight)
		{
			m_needClearRight = false;
			CClearSurfacePass::Execute(GetEyeTarget(RIGHT_EYE), Clr_Transparent);
		}
		//m_renderer.FX_PushRenderTarget(0, GetEyeTarget(RIGHT_EYE), &m_renderer.m_DepthBufferOrig);
	}

	if (eye == LEFT_EYE)
		GetEyeTarget(LEFT_EYE)->SetResolved(true);
	else
		GetEyeTarget(RIGHT_EYE)->SetResolved(true);
}

void CD3DStereoRenderer::EndRenderingTo(StereoEye eye)
{
	if (eye == LEFT_EYE)
	{
		//gRenDev->SetProfileMarker("LEFT_EYE", CRenderer::ESPM_POP);
		GetEyeTarget(LEFT_EYE)->SetResolved(true);
		//m_renderer.FX_PopRenderTarget(0);
	}
	else
	{
		//gRenDev->SetProfileMarker("RIGHT_EYE", CRenderer::ESPM_POP);
		GetEyeTarget(RIGHT_EYE)->SetResolved(true);
		//m_renderer.FX_PopRenderTarget(0);
	}
}

void CD3DStereoRenderer::BeginRenderingToVrQuadLayer(RenderLayer::EQuadLayers id)
{
	CTexture* pTexture = GetVrQuadLayerTex(id);
	if (pTexture && m_pHmdRenderer)
	{
		if (RenderLayer::CProperties* pLayer = m_pHmdRenderer->GetQuadLayerProperties(id))
		{
			pLayer->SetActive(true);
		}

		gRenDev->SetProfileMarker("STEREO_QUAD", CRenderer::ESPM_PUSH);
		if (m_needClearVrQuadLayer)
		{
			CClearSurfacePass::Execute(pTexture, Clr_Transparent);
			m_needClearVrQuadLayer = false;
		}
		//m_renderer.FX_PushRenderTarget(0, pTexture, &m_renderer.m_DepthBufferOrig);

		//m_renderer.FX_SetActiveRenderTargets();

		pTexture->SetResolved(true);
	}

}

void CD3DStereoRenderer::EndRenderingToVrQuadLayer(RenderLayer::EQuadLayers id)
{
	CTexture* pTexture = GetVrQuadLayerTex(id);
	if (pTexture)
	{
		gRenDev->SetProfileMarker("STEREO_QUAD", CRenderer::ESPM_POP);
		pTexture->SetResolved(true);
		//m_renderer.FX_PopRenderTarget(0);
	}
}

void CD3DStereoRenderer::SelectShaderTechnique()
{
	gRenDev->m_cEF.mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);

	ASSERT_LEGACY_PIPELINE
		/*

	CShader* pSH = m_renderer.m_cEF.s_ShaderStereo;

	if (m_device == STEREO_DEVICE_FRAMECOMP)
	{
		switch (GetStereoOutput())
		{
		case STEREO_OUTPUT_SIDE_BY_SIDE_SQUEEZED:
			pSH->FXSetTechnique("SideBySide");
			break;
		case STEREO_OUTPUT_CHECKERBOARD:
			pSH->FXSetTechnique("Checkerboard");
			break;
		case STEREO_OUTPUT_SIDE_BY_SIDE:
			pSH->FXSetTechnique("SideBySide");
			break;
		case STEREO_OUTPUT_LINE_BY_LINE:
			pSH->FXSetTechnique("LineByLine");
			break;
#ifndef _RELEASE
		case STEREO_OUTPUT_ANAGLYPH:
			pSH->FXSetTechnique("Anaglyph");
			break;
#endif
		default:
			pSH->FXSetTechnique("Emulation");
		}
	}
	else if (IsDriver(DRIVER_NV))
	{
		pSH->FXSetTechnique("NV3DVision");
	}
	else
	{
		pSH->FXSetTechnique("Emulation");
	}
	*/
}

void CD3DStereoRenderer::RenderScene(int sceneFlags, const SRenderingPassInfo& passInfo)
{
	m_renderer.RenderFrame(sceneFlags, passInfo);
}

bool CD3DStereoRenderer::IsRenderThread() const
{
	return m_renderer.m_pRT->IsRenderThread();
}

void CD3DStereoRenderer::PushRenderTargets()
{
	if (m_needClearLeft)
	{
		m_needClearLeft = false;
		CClearSurfacePass::Execute(GetEyeTarget(LEFT_EYE), Clr_Transparent);
	}

	if (m_needClearRight)
	{
		m_needClearRight = false;
		CClearSurfacePass::Execute(GetEyeTarget(RIGHT_EYE), Clr_Transparent);
	}

	//m_renderer.FX_PushRenderTarget(0, GetEyeTarget(LEFT_EYE), &m_renderer.m_DepthBufferOrig);
	//m_renderer.FX_PushRenderTarget(1, GetEyeTarget(RIGHT_EYE), NULL);

	//m_renderer.RT_SetViewport(0, 0, GetEyeTarget(LEFT_EYE)->GetWidth(), GetEyeTarget(LEFT_EYE)->GetHeight());
}

void CD3DStereoRenderer::PopRenderTargets(bool bResolve)
{
	//m_renderer.FX_PopRenderTarget(1);
	//m_renderer.FX_PopRenderTarget(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRenderer Interface
////////////////////////////////////////////////////////////////////////////////////////////////////

EStereoDevice CD3DStereoRenderer::GetDevice()
{
	return m_device;
}

EStereoDeviceState CD3DStereoRenderer::GetDeviceState()
{
	return m_deviceState;
}

void CD3DStereoRenderer::GetInfo(EStereoDevice* device, EStereoMode* mode, EStereoOutput* output, EStereoDeviceState* state)
{
	if (device) *device = m_device;
	if (mode) *mode = m_mode;
	if (output) *output = m_output;
	if (state) *state = m_deviceState;
}

bool CD3DStereoRenderer::GetStereoEnabled()
{
	return IsStereoEnabled();
}

float CD3DStereoRenderer::GetStereoStrength()
{
	return m_stereoStrength;
}

float CD3DStereoRenderer::GetMaxSeparationScene(bool half)
{
	return m_maxSeparationScene * (half ? 0.5f : 1.0f);
}

float CD3DStereoRenderer::GetZeroParallaxPlaneDist()
{
	return m_zeroParallaxPlaneDist;
}

void CD3DStereoRenderer::GetNVControlValues(bool& stereoActivated, float& stereoStrength)
{
	stereoActivated = m_nvStereoActivated != 0;
	stereoStrength = m_nvStereoStrength;
}

void CD3DStereoRenderer::ReleaseBuffers()
{
	if (m_pHmdRenderer != nullptr)
		m_pHmdRenderer->ReleaseBuffers();
}

void CD3DStereoRenderer::OnResolutionChanged(int newWidth, int newHeight)
{
	if (m_device == STEREO_DEVICE_NONE)
		return;

	CreateIntermediateBuffers(newWidth, newHeight);

	if (m_pHmdRenderer != nullptr)
	{
		m_pHmdRenderer->OnResolutionChanged(newWidth, newHeight);
	}
}

void CD3DStereoRenderer::CalculateResolution(int requestedWidth, int requestedHeight, int* pDisplayWidth, int *pDisplayHeight)
{
	switch (m_output)
	{
		case STEREO_OUTPUT_SIDE_BY_SIDE:
			*pDisplayWidth  = requestedWidth * 2;
			*pDisplayHeight = requestedHeight;
			break;
		case STEREO_OUTPUT_ABOVE_AND_BELOW:
			*pDisplayWidth  = requestedWidth;
			*pDisplayHeight = requestedHeight * 2;
			break;
		case STEREO_OUTPUT_HMD:
			if (m_pHmdDevice)
			{
				// Update renderer resolution
				HmdDeviceInfo deviceInfo;
				m_pHmdDevice->GetDeviceInfo(deviceInfo);

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
	m_pHmdDevice = pHmdDevice;

	if (m_pHmdDevice == nullptr)
	{
		m_device = STEREO_DEVICE_NONE;
		return;
	}

	// Make sure PrepareStereo is called in Update by specifying a device
	SelectDefaultDevice();
}

IHmdRenderer* CD3DStereoRenderer::CreateHmdRenderer(IHmdDevice& device, CD3D9Renderer* pRenderer, CD3DStereoRenderer* pStereoRenderer)
{
#if defined(INCLUDE_VR_RENDERING)
	switch (device.GetClass())
	{
	case eHmdClass_Oculus:
		return new CD3DOculusRenderer(static_cast<CryVR::Oculus::IOculusDevice*>(&device), pRenderer, pStereoRenderer);
	case eHmdClass_OpenVR:
		return new CD3DOpenVRRenderer(static_cast<CryVR::OpenVR::IOpenVRDevice*>(&device), pRenderer, pStereoRenderer);
	case eHmdClass_Osvr:
		return new CryVR::Osvr::CD3DOsvrRenderer(static_cast<CryVR::Osvr::IOsvrDevice*>(&device), pRenderer, pStereoRenderer);
	default:
		iLog->LogError("Tried to create HMD renderer for unknown headset!");
		break;
	}
#else
	iLog->LogError("Tried to create HMD renderer with VR rendering support disabled for renderer at compile-time!");
#endif

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CD3DStereoRenderer::TryInjectHmdCameraAsync(CRenderView* pRenderView)
{
	if (pRenderView->GetCurrentEye() == CCamera::eEye_Left)
	{
		IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
		if (!pHmdManager)
			return;

		IHmdDevice* pHmdDevice = pHmdManager->GetHmdDevice();
		if (!pHmdDevice)
			return;

		m_bAsyncCameraMatrixValid = false;
		// Only query async tracking state for the first left eye.
		IHmdDevice::AsyncCameraContext context;
		context.frameId = pRenderView->GetFrameId();
		if (!pHmdDevice->RequestAsyncCameraUpdate(context))
			return;
		m_bAsyncCameraMatrixValid = true;
		m_asyncCameraMatrix = context.outputCameraMatrix;
	}

	if (!m_bAsyncCameraMatrixValid)
		return;

	CCamera currentCamera = pRenderView->GetCamera(CCamera::eEye_Left);
	currentCamera.SetMatrix(m_asyncCameraMatrix);
	uint32 renderingFlags = pRenderView->GetShaderRenderingFlags();

	if ((renderingFlags & (SHDF_STEREO_LEFT_EYE | SHDF_STEREO_RIGHT_EYE)) == 0) // non-stereo case
		renderingFlags |= SHDF_STEREO_LEFT_EYE;

	for (CCamera::EEye eye = CCamera::eEye_Left; eye != CCamera::eEye_eCount; eye = CCamera::EEye(eye + 1))
	{
		uint32 currentEyeFlag = eye == CCamera::eEye_Left ? SHDF_STEREO_LEFT_EYE : SHDF_STEREO_RIGHT_EYE;

		if (renderingFlags & currentEyeFlag)
		{
			CCamera newCamera = PrepareCamera(eye, currentCamera);

			static CCamera previousCameras[2];
			pRenderView->SetPreviousFrameCameras(&previousCameras[eye], 1);
			previousCameras[eye] = newCamera;

			pRenderView->SetCameras(&newCamera, 1);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CTexture* CD3DStereoRenderer::WrapD3DRenderTarget(D3DTexture* d3dTexture, uint32 width, uint32 height, DXGI_FORMAT format, const char* name, bool shaderResourceView)
{
	ETEX_Format texFormat = DeviceFormats::ConvertToTexFormat(format);
	CTexture* texture = CTexture::GetOrCreateTextureObject(name, width, height, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, texFormat);
	if (texture == nullptr)
	{
		gEnv->pLog->Log("[HMD][Oculus] Unable to create texture object!");
		return nullptr;
	}

	// CTexture::CreateTextureObject does not set width and height if the texture already existed
	assert(texture->GetWidth() == width);
	assert(texture->GetHeight() == height);
	assert(texture->GetDepth() == 1);

	d3dTexture->AddRef();

	texture->SRGBRead(DeviceFormats::ConvertToSRGB(format) == format);
	texture->SetDevTexture(CDeviceTexture::Associate(texture->GetLayout(), d3dTexture));
	texture->SetClosestFormatSupported();

	return texture;
}
