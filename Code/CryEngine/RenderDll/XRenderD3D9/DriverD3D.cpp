// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include "DriverD3D.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryInput/IHardwareMouse.h>
#include <CrySystem/Profilers/IStatoscope.h>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ClassWeaver.h>

#include "../Common/Textures/TextureManager.h"
#include "../Common/Textures/TextureStreamPool.h"
#include "../Common/ReverseDepth.h"
#include "D3DStereo.h"
#include "D3DPostProcess.h"
#include "StatoscopeRenderStats.h"
#include "GraphicsPipeline/DebugRenderTargets.h"
#include "GraphicsPipeline/VolumetricFog.h"
#include "GraphicsPipeline/Common/UtilityPasses.h"

#include <CryMovie/AnimKey.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>
#include <CrySystem/VR/IHMDManager.h>

#ifdef ENABLE_BENCHMARK_SENSOR
	#include <IBenchmarkFramework.h>
	#include <IBenchmarkRendererSensorManager.h>
#endif
#include "Gpu/Particles/GpuParticleManager.h"

#include "Common/RenderDisplayContext.h"
#include "DeviceManager/D3D11/DeviceObjects_D3D11.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#pragma warning(push)
#pragma warning(disable: 4244)

#if CRY_PLATFORM_WINDOWS && !CRY_RENDERER_OPENGL && !CRY_RENDERER_VULKAN
	#include <D3Dcompiler.h>
#endif

void CD3D9Renderer::LimitFramerate(const int maxFPS, const bool bUseSleep)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "RT_FRAME_CAP");

	if (maxFPS > 0)
	{
		CTimeValue timeFrameMax;
		const float safeMarginFPS = 0.5f;//save margin to not drop below 30 fps
		static CTimeValue sTimeLast = gEnv->pTimer->GetAsyncTime();

		timeFrameMax.SetMilliSeconds((int64)(1000.f / ((float)maxFPS + safeMarginFPS)));
		const CTimeValue timeLast = timeFrameMax + sTimeLast;
		while (timeLast.GetValue() > gEnv->pTimer->GetAsyncTime().GetValue())
		{
			if (bUseSleep)
			{
				CrySleep(1);
			}
			else
			{
				volatile int i = 0;
				while (i++ < 1000)
					;
			}
		}

		sTimeLast = gEnv->pTimer->GetAsyncTime();
	}
}

CCryNameTSCRC CTexture::s_sClassName = CCryNameTSCRC("CTexture");
CCryNameTSCRC CShader::s_sClassName = CCryNameTSCRC("CShader");

CD3D9Renderer gcpRendD3D;

// Direct 3D console variables
CD3D9Renderer::CD3D9Renderer()
	: m_nAsyncDeviceState(0)
	, m_DeviceOwningthreadID(0)
#if CRY_PLATFORM_WINDOWS
	, m_bDisplayChanged(false)
	, m_nConnectedMonitors(1)
#endif // CRY_PLATFORM_WINDOWS
{
	CreateDeferredUnitBox(m_arrDeferredInds, m_arrDeferredVerts);
}

void CD3D9Renderer::InitRenderer()
{
	CRenderer::InitRenderer();

	m_uLastBlendFlagsPassGroup = 0xFFFFFFFF;
	m_fAdaptedSceneScaleLBuffer = 1.0f;
	m_bInitialized = false;
	gRenDev = this;

	m_pBaseDisplayContext = std::make_shared<CSwapChainBackedRenderDisplayContext>(IRenderer::SDisplayContextDescription{}, "Base-SwapShain", m_uniqueDisplayContextId++);
	{
		SDisplayContextKey baseContextKey;
		baseContextKey.key.emplace<HWND>(m_pBaseDisplayContext->GetWindowHandle());
		m_displayContexts.emplace(std::make_pair(std::move(baseContextKey), m_pBaseDisplayContext));
	}

	m_pStereoRenderer = new CD3DStereoRenderer();
	m_pGraphicsPipeline.reset(new CStandardGraphicsPipeline);

	m_pPipelineProfiler = NULL;

#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	m_pPipelineProfiler = new CRenderPipelineProfiler();
#endif

	m_LogFile = NULL;

#if defined(ENABLE_PROFILING_CODE)
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; i++)
		m_pCaptureCallBack[i] = NULL;
	m_frameCaptureRegisterNum = 0;
	m_captureFlipFlop = 0;
	m_pSaveTexture[0] = nullptr;
	m_pSaveTexture[1] = nullptr;

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		m_nScreenCaptureRequestFrame[i] = 0;
		m_screenCapTexHandle[i] = 0;
	}
#endif

	m_dwPresentStatus = 0;

	m_hWnd = NULL;
#if CRY_PLATFORM_WINDOWS
	m_hIconBig = NULL;
	m_hIconSmall = NULL;
#endif
	m_dwCreateFlags = 0L;

	m_lockCharCB = 0;
	m_CharCBFrameRequired[0] = m_CharCBFrameRequired[1] = m_CharCBFrameRequired[2] = 0;
	m_CharCBAllocated = 0;
	m_vSceneLuminanceInfo = Vec4(1.f, 1.f, 1.f, 1.f);
	m_fScotopicSceneScale = m_fAdaptedSceneScale = m_fAdaptedSceneScaleLBuffer = 1.0f;

	for (int i = 0; i < SHAPE_MAX; i++)
	{
		m_pUnitFrustumVB[i] = NULL;
		m_pUnitFrustumIB[i] = NULL;
	}

	m_pQuadVB = NULL;
	m_nQuadVBSize = 0;

	//memset(&m_LumGamma, 0, sizeof(m_LumGamma));
	//memset(&m_LumHistogram, 0, sizeof(m_LumHistogram));

	m_strDeviceStats[0] = 0;

	m_Features = RFT_SUPPORTZBIAS;

#if defined(ENABLE_RENDER_AUX_GEOM)
	m_pRenderAuxGeomD3D = 0;
	if (CV_r_enableauxgeom)
		m_pRenderAuxGeomD3D = CRenderAuxGeomD3D::Create(*this);
#endif
	m_pColorGradingControllerD3D = new CColorGradingController();

	m_wireframe_mode = R_SOLID_MODE;

#ifdef SHADER_ASYNC_COMPILATION
	uint32 nThreads = CV_r_shadersasyncmaxthreads; //clamp_tpl(CV_r_shadersasyncmaxthreads, 1, 4);
	uint32 nOldThreads = m_AsyncShaderTasks.size();
	for (uint32 a = nThreads; a < nOldThreads; a++)
		delete m_AsyncShaderTasks[a];
	m_AsyncShaderTasks.resize(nThreads);
	for (uint32 a = nOldThreads; a < nThreads; a++)
		m_AsyncShaderTasks[a] = new CAsyncShaderTask();
	for (int32 i = 0; i < m_AsyncShaderTasks.size(); i++)
	{
		m_AsyncShaderTasks[i]->SetThread(i);
	}
#endif

	m_pSFResD3D = 0;
	m_pPostProcessMgr = 0;
	m_pWaterSimMgr = 0;

#if ENABLE_STATOSCOPE
	m_pGPUTimesDG = new CGPUTimesDG(this);
	m_pDetailedRenderTimesDG = new CDetailedRenderTimesDG(this);
	m_pGraphicsDG = new CGraphicsDG(this);
	m_pPerformanceOverviewDG = new CPerformanceOverviewDG(this);
	gEnv->pStatoscope->RegisterDataGroup(m_pGPUTimesDG);
	gEnv->pStatoscope->RegisterDataGroup(m_pDetailedRenderTimesDG);
	gEnv->pStatoscope->RegisterDataGroup(m_pGraphicsDG);
	gEnv->pStatoscope->RegisterDataGroup(m_pPerformanceOverviewDG);
#endif

	m_MSAAData.Clear();
}

CD3D9Renderer::~CD3D9Renderer()
{
	//Code moved to Release
}

void CD3D9Renderer::Release()
{
	//FreeResources(FRR_ALL);
	ShutDown();

#if ENABLE_STATOSCOPE
	gEnv->pStatoscope->UnregisterDataGroup(m_pGPUTimesDG);
	gEnv->pStatoscope->UnregisterDataGroup(m_pGraphicsDG);
	gEnv->pStatoscope->UnregisterDataGroup(m_pPerformanceOverviewDG);
	SAFE_DELETE(m_pGPUTimesDG);
	SAFE_DELETE(m_pGraphicsDG);
	SAFE_DELETE(m_pPerformanceOverviewDG);
#endif

#if defined(ENABLE_PROFILING_CODE)
	SAFE_RELEASE(m_pSaveTexture[0]);
	SAFE_RELEASE(m_pSaveTexture[1]);
#endif

#ifdef SHADER_ASYNC_COMPILATION
	for (int32 a = 0; a < m_AsyncShaderTasks.size(); a++)
		delete m_AsyncShaderTasks[a];
#endif

	CRenderer::Release();

	DestroyWindow();
}

void CD3D9Renderer::Reset(void)
{
	m_pRT->RC_ResetDevice();
}
void CD3D9Renderer::RT_Reset(void)
{

	if (CheckDeviceLost())
		return;
	m_bDeviceLost = 1;
	//iLog->Log("...Reset");
	RestoreGamma();
	m_bDeviceLost = 0;

	if (IsFullscreen())
		SetGamma(CV_r_gamma + m_fDeltaGamma, CV_r_brightness, CV_r_contrast, false);
}

bool CD3D9Renderer::ChangeDisplay(CRenderDisplayContext* pDC, unsigned int width, unsigned int height, unsigned int cbpp)
{
	return false;
}

void CD3D9Renderer::ChangeViewport(CRenderDisplayContext* pDC, unsigned int viewPortOffsetX, unsigned int viewPortOffsetY, unsigned int viewportWidth, unsigned int viewportHeight)
{
	if (m_bDeviceLost)
		return;

	gRenDev->ExecuteRenderThreadCommand([=]{
			// This change will propagate to the other dimensions (output and render)
			// when HandleDisplayPropertyChanges() is called just before rendering
			pDC->ChangeDisplayResolution(viewPortOffsetX + viewportWidth, viewPortOffsetY + viewportHeight, SRenderViewport(viewPortOffsetX, viewPortOffsetY, viewportWidth, viewportHeight));

			if (pDC->IsMainViewport())
			{
				SetCurDownscaleFactor(Vec2(1, 1));
				if (auto pRenderOutput = pDC->GetRenderOutput().get())
				{
					CRendererResources::OnOutputResolutionChanged(pDC->GetDisplayResolution()[0], pDC->GetDisplayResolution()[1]);
					pRenderOutput->ReinspectDisplayContext();
				}
			}
		}, ERenderCommandFlags::None
	);
}

void CD3D9Renderer::SetCurDownscaleFactor(Vec2 sf)
{
	m_renderQuality.downscaleFactor = sf;
}

void CD3D9Renderer::ChangeLog()
{
#ifdef DO_RENDERLOG
	static bool singleFrame = false;

	if (CV_r_log && !m_LogFile)
	{
		if (CV_r_log < 0)
		{
			singleFrame = true;
			CV_r_log = abs(CV_r_log);
		}

		if (CV_r_log == 3)
			SetLogFuncs(true);

		m_LogFile = fxopen("Direct3DLog.txt", "w");

		if (m_LogFile)
		{
			iLog->Log("Direct3D log file '%s' opened\n", "Direct3DLog.txt");
			char time[128];
			char date[128];

			_strtime(time);
			_strdate(date);

			fprintf(m_LogFile, "\n==========================================\n");
			fprintf(m_LogFile, "Direct3D Log file opened: %s (%s)\n", date, time);
			fprintf(m_LogFile, "==========================================\n");
		}
	}
	else if (m_LogFile && singleFrame)
	{
		CV_r_log = 0;
		singleFrame = false;
	}

	if (!CV_r_log && m_LogFile)
	{
		SetLogFuncs(false);

		char time[128];
		char date[128];
		_strtime(time);
		_strdate(date);

		fprintf(m_LogFile, "\n==========================================\n");
		fprintf(m_LogFile, "Direct3D Log file closed: %s (%s)\n", date, time);
		fprintf(m_LogFile, "==========================================\n");

		fclose(m_LogFile);
		m_LogFile = NULL;
		iLog->Log("Direct3D log file '%s' closed\n", "Direct3DLog.txt");
	}

	if (CV_r_logTexStreaming && !m_LogFileStr)
	{
		m_LogFileStr = fxopen("Direct3DLogStreaming.txt", "w");
		if (m_LogFileStr)
		{
			iLog->Log("Direct3D texture streaming log file '%s' opened\n", "Direct3DLogStreaming.txt");
			char time[128];
			char date[128];

			_strtime(time);
			_strdate(date);

			fprintf(m_LogFileStr, "\n==========================================\n");
			fprintf(m_LogFileStr, "Direct3D Textures streaming Log file opened: %s (%s)\n", date, time);
			fprintf(m_LogFileStr, "==========================================\n");
		}
	}
	else if (!CV_r_logTexStreaming && m_LogFileStr)
	{
		char time[128];
		char date[128];
		_strtime(time);
		_strdate(date);

		fprintf(m_LogFileStr, "\n==========================================\n");
		fprintf(m_LogFileStr, "Direct3D Textures streaming Log file closed: %s (%s)\n", date, time);
		fprintf(m_LogFileStr, "==========================================\n");

		fclose(m_LogFileStr);
		m_LogFileStr = NULL;
		iLog->Log("Direct3D texture streaming log file '%s' closed\n", "Direct3DLogStreaming.txt");
	}

	if (CV_r_logShaders && !m_LogFileSh)
	{
		m_LogFileSh = fxopen("Direct3DLogShaders.txt", "w");
		if (m_LogFileSh)
		{
			iLog->Log("Direct3D shaders log file '%s' opened\n", "Direct3DLogShaders.txt");
			char time[128];
			char date[128];

			_strtime(time);
			_strdate(date);

			fprintf(m_LogFileSh, "\n==========================================\n");
			fprintf(m_LogFileSh, "Direct3D Shaders Log file opened: %s (%s)\n", date, time);
			fprintf(m_LogFileSh, "==========================================\n");
		}
	}
	else if (!CV_r_logShaders && m_LogFileSh)
	{
		char time[128];
		char date[128];
		_strtime(time);
		_strdate(date);

		fprintf(m_LogFileSh, "\n==========================================\n");
		fprintf(m_LogFileSh, "Direct3D Textures streaming Log file closed: %s (%s)\n", date, time);
		fprintf(m_LogFileSh, "==========================================\n");

		fclose(m_LogFileSh);
		m_LogFileSh = NULL;
		iLog->Log("Direct3D Shaders log file '%s' closed\n", "Direct3DLogShaders.txt");
	}
#endif
}

void CD3D9Renderer::DrawTexelsPerMeterInfo()
{
	/*
#ifndef _RELEASE
	if (CV_r_TexelsPerMeter > 0)
	{
		FX_SetState(GS_NODEPTHTEST);

		int x = 800 - 310 + 2;
		int y = 600 - 20 + 2;
		int w = 296;
		int h = 6;

		Draw2dImage(x - 2, y - 2, w + 4, h + 4, CRendererResources::s_ptexWhite->GetTextureID(), 0, 0, 1, 1, 0, 1, 1, 1, 1, 0);
		Draw2dImage(x, y, w, h, CRendererResources::s_ptexPaletteTexelsPerMeter->GetTextureID(), 0, 0, 1, 1, 0, 1, 1, 1, 1, 0);

		float color[4] = { 1, 1, 1, 1 };

		x = x * m_width / 800;
		y = y * m_height / 600;
		w = w * m_width / 800;

		IRenderAuxText::Draw2dLabel(x - 100, y - 20, 1.2f, color, false, "r_TexelsPerMeter:");
		IRenderAuxText::Draw2dLabel(x - 2, y - 20, 1.2f, color, false, "0");
		IRenderAuxText::Draw2dLabel(x + w / 2 - 5, y - 20, 1.2f, color, false, "%.0f", CV_r_TexelsPerMeter);
		IRenderAuxText::Draw2dLabel(x + w - 50, y - 20, 1.2f, color, false, ">= %.0f", CV_r_TexelsPerMeter * 2.0f);
	}
#endif
*/
}

#if CRY_PLATFORM_DURANGO
void CD3D9Renderer::RT_SuspendDevice()
{
	assert(!m_bDeviceSuspended);

#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	if (m_pPerformanceDeviceContext)
		m_pPerformanceDeviceContext->Suspend(0);   // must be 0 for now, reserved for future use.
#endif

	m_bDeviceSuspended = true;
}

void CD3D9Renderer::RT_ResumeDevice()
{
	assert(m_bDeviceSuspended);

#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	if (m_pPerformanceDeviceContext)
		m_pPerformanceDeviceContext->Resume();
#endif

	m_bDeviceSuspended = false;
}
#endif

void CD3D9Renderer::CalculateResolutions(int displayWidthRequested, int displayHeightRequested, bool bUseNativeRes, bool findClosestMatching, int* pRenderWidth, int* pRenderHeight, int* pOutputWidth, int* pOutputHeight, int* pDisplayWidth, int* pDisplayHeight)
{
	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	displayWidthRequested  = max(displayWidthRequested , 32);
	displayHeightRequested = max(displayHeightRequested, 32);

	// Figure out the displays native resolution (the monitor) //////////////////////////////////////////////////////
	*pDisplayWidth  = displayWidthRequested;
	*pDisplayHeight = displayHeightRequested;

	if (!IsEditorMode())
	{
	#if CRY_PLATFORM_CONSOLE
		*pDisplayWidth  = 1920;
		*pDisplayHeight = 1080;
	#elif CRY_PLATFORM_MOBILE
		SDL_DisplayMode desktopDispMode;
		if (SDL_GetDesktopDisplayMode(0, &desktopDispMode) == 0)
		{
			*pDisplayWidth  = desktopDispMode.w;
			*pDisplayHeight = desktopDispMode.h;
		}
		else
		{
			CryLogAlways("Failed to get desktop dimensions: %s", SDL_GetError());
			*pDisplayWidth = 1280;
			*pDisplayHeight = 720;
		}
	#elif CRY_PLATFORM_WINDOWS
		RectI monitorBounds = {};
		if (pDC->IsSwapChainBacked())
			monitorBounds = static_cast<const CSwapChainBackedRenderDisplayContext*>(pDC)->GetCurrentMonitorBounds();
		else
		{
			monitorBounds.w = pDC->GetDisplayResolution().x;
			monitorBounds.h = pDC->GetDisplayResolution().y;
		}
		
		if (findClosestMatching && pDC->IsSwapChainBacked())
		{
			const auto match = static_cast<const CSwapChainBackedRenderDisplayContext*>(pDC)->FindClosestMatchingScreenResolution(Vec2_tpl<uint32_t>{ static_cast<uint32_t>(displayWidthRequested), static_cast<uint32_t>(displayHeightRequested) });
			displayWidthRequested  = match.x;
			displayHeightRequested = match.y;
		}

		*pDisplayWidth  = bUseNativeRes ? monitorBounds.w : displayWidthRequested;
		*pDisplayHeight = bUseNativeRes ? monitorBounds.h : displayHeightRequested;
	#endif
	}

	// Calculate the rendering resolution based on inputs ///////////////////////////////////////////////////////////
	*pOutputWidth = pDC->IsScalable() && CV_r_CustomResWidth ? std::min(CV_r_CustomResMaxSize, CV_r_CustomResWidth) : *pDisplayWidth;
	*pOutputHeight = pDC->IsScalable() && CV_r_CustomResHeight ? std::min(CV_r_CustomResMaxSize, CV_r_CustomResHeight) : *pDisplayHeight;

	const int nMaxResolutionX = std::max(std::min(CV_r_CustomResMaxSize, m_MaxTextureSize ? m_MaxTextureSize : INT_MAX), *pOutputWidth);
	const int nMaxResolutionY = std::max(std::min(CV_r_CustomResMaxSize, m_MaxTextureSize ? m_MaxTextureSize : INT_MAX), *pOutputHeight);

	int nSSSamplesX = pDC->m_nSSSamplesX; do { *pRenderWidth  = *pOutputWidth  * nSSSamplesX; --nSSSamplesX; } while (*pRenderWidth  > nMaxResolutionX);
	int nSSSamplesY = pDC->m_nSSSamplesY; do { *pRenderHeight = *pOutputHeight * nSSSamplesY; --nSSSamplesX; } while (*pRenderHeight > nMaxResolutionY);
}

void CD3D9Renderer::HandleDisplayPropertyChanges()
{
	CRenderDisplayContext* pBC = GetBaseDisplayContext();
	CRenderDisplayContext* pDC = GetActiveDisplayContext();
	CRenderOutput* pRO = pDC->GetRenderOutput().get();

	bool bChangedRendering = false;
	bool bChangedOutputting = false;
	bool bResizeSwapchain = false;
	bool bRecreateSwapchain = false;
	bool bNativeRes = false;
	bool wasFullscreen = IsFullscreen();

	EWindowState previousWindowState = m_windowState;

	if (!IsEditorMode())
	{
		CRY_ASSERT(pDC == pBC);

		// Detect changes in refresh property ///////////////////////////////////////////////////////////////////////////
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
		bChangedRendering |= m_overrideRefreshRate != CV_r_overrideRefreshRate || m_overrideScanlineOrder != CV_r_overrideScanlineOrder;
#endif

		EWindowState windowState = CalculateWindowState();
		m_windowState = windowState;

		bChangedRendering |= windowState != previousWindowState;
		bResizeSwapchain |= windowState != previousWindowState;

#if CRY_PLATFORM_CONSOLE || CRY_PLATFORM_MOBILE
		bNativeRes = true;
#elif CRY_PLATFORM_WINDOWS
		bResizeSwapchain |= (m_bWindowRestored || m_bDisplayChanged && IsFullscreen());
		m_bDisplayChanged = false;

		const bool bFullScreenNativeRes = CV_r_FullscreenNativeRes && CV_r_FullscreenNativeRes->GetIVal() != 0;
		bNativeRes = bFullScreenNativeRes && (IsFullscreen() || m_windowState == EWindowState::BorderlessWindow);
#else
		bNativeRes = false;
#endif
	}

	{
		// Detect changes in rendering resolution ///////////////////////////////////////////////////////////////////////
		pDC->m_nSSSamplesX = CV_r_Supersampling;
		pDC->m_nSSSamplesY = CV_r_Supersampling;

		// Detect changes in back-buffer property ///////////////////////////////////////////////////////////////////////
		const int colorBits = m_CVColorBits ? m_CVColorBits->GetIVal() : m_cbpp;
//		const int depthBits = m_CVDepthBits ? m_CVDepthBits->GetIVal() : m_zbpp;
//		const int stnclBits = m_CVStnclBits ? m_CVStnclBits->GetIVal() : m_sbpp;
		const int vSync = !IsEditorMode() ? CV_r_vsync : 0;

		const int displayWidthBefore  = pDC->GetDisplayResolution()[0];
		const int displayHeightBefore = pDC->GetDisplayResolution()[1];
#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
		const int bufferCountBefore = pDC->IsSwapChainBacked() ? static_cast<const CSwapChainBackedRenderDisplayContext*>(pDC)->GetSwapChain().GetDesc                   ().BufferCount : CRendererCVars::CV_r_MaxFrameLatency + 1;
#else
		const int bufferCountBefore = pDC->IsSwapChainBacked() ? static_cast<const CSwapChainBackedRenderDisplayContext*>(pDC)->GetSwapChain().GetSwapChain()->GnmGetDesc().numBuffers  : CRendererCVars::CV_r_MaxFrameLatency + 1;
#endif

		// r_width and r_height are only honored when in game, otherwise
		// the resolution is entirely controlled by dynamic window size
#if CRY_PLATFORM_MOBILE
		int displayWidthRequested  = displayWidthBefore;
		int displayHeightRequested = displayHeightBefore;
#else
		int displayWidthRequested  = !IsEditorMode() && (pDC == pBC) && m_CVWidth  ? m_CVWidth ->GetIVal() : displayWidthBefore;
		int displayHeightRequested = !IsEditorMode() && (pDC == pBC) && m_CVHeight ? m_CVHeight->GetIVal() : displayHeightBefore;

#if CRY_PLATFORM_WINDOWS
		// Match monitor resolution in borderless full screen mode
		if (m_windowState == EWindowState::BorderlessFullscreen)
		{
			HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO monitorInfo;
			monitorInfo.cbSize = sizeof(monitorInfo);
			GetMonitorInfo(hMonitor, &monitorInfo);

			displayWidthRequested = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
			displayHeightRequested = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
		}
#endif
#endif

		// Tweak, adjust and fudge any of the requested changes
		int renderWidth, renderHeight, outputWidth, outputHeight, displayWidth, displayHeight;

		CalculateResolutions(displayWidthRequested, displayHeightRequested, bNativeRes, IsFullscreen(), &renderWidth, &renderHeight, &outputWidth, &outputHeight, &displayWidth, &displayHeight);

		if (!IsEditorMode() && m_pStereoRenderer && m_pStereoRenderer->IsStereoEnabled())
		{
			m_pStereoRenderer->CalculateResolution(renderWidth, renderHeight, &renderWidth, &renderHeight);
		}

		// Renderer resize
		if (CRendererResources::s_renderWidth  != renderWidth  ||
			CRendererResources::s_renderHeight != renderHeight)
		{
			// The global resources are only used for the main viewport currently, but never for
			// secondary ones, as they are forward shaded minimally without utilizing global resources
			bChangedRendering = pDC->IsDeferredShadeable();

			// Hack for editor (editor viewports will be resized earlier)
			if (IsEditorMode() && pDC->IsMainViewport())
				bResizeSwapchain = true;
		}

		// Output resize
		if (pRO->GetOutputResolution() != Vec2i(outputWidth, outputHeight))
		{
			bChangedOutputting = true;
		}

		// Swap-Chain resize
		if (pDC->GetDisplayResolution() != Vec2i(displayWidth, displayHeight))
		{
			bResizeSwapchain = true;
		}

		// Swap-Chain recreate
		if (m_cbpp != colorBits ||
			m_VSync != vSync ||
			wasFullscreen != IsFullscreen() ||
			bufferCountBefore != (CRendererCVars::CV_r_MaxFrameLatency + 1))
		{
			bRecreateSwapchain = true;
		}

		if (bResizeSwapchain | bRecreateSwapchain)
		{
			ChangeDisplayResolution(displayWidth, displayHeight, colorBits, 75, previousWindowState, bResizeSwapchain | bRecreateSwapchain, pDC);

			CRendererResources::OnDisplayResolutionChanged(displayWidth, displayHeight);
		}

		if (bChangedOutputting)
		{
			ChangeOutputResolution(outputWidth, outputHeight, pRO);

			CRendererResources::OnOutputResolutionChanged(outputWidth, outputHeight);
		}

		if (bChangedRendering)
		{
		//	ChangeRenderResolution(renderWidth, renderHeight, ???);
			GetGraphicsPipeline().Resize(renderWidth, renderHeight);

			CRendererResources::OnRenderResolutionChanged(renderWidth, renderHeight);
		}

		if (!IsEditorMode() && (pDC == pBC) && (bResizeSwapchain | bChangedOutputting | bChangedRendering))
		{
			iLog->Log("  Display resolution: %dx%dx%d (%s)", CRendererResources::s_displayWidth, CRendererResources::s_displayHeight, colorBits, GetWindowStateName());
			iLog->Log("  Post/Overlay resolution: %dx%d", CRendererResources::s_outputWidth, CRendererResources::s_outputHeight);
			iLog->Log("  Render resolution: %dx%d", CRendererResources::s_renderWidth, CRendererResources::s_renderHeight);
		}

		m_bWindowRestored = false;
	}
}

EWindowState CD3D9Renderer::CalculateWindowState() const
{
	if (gEnv->IsEditor())
	{
		return EWindowState::Windowed;
	}

	return static_cast<EWindowState>(m_CVWindowType->GetIVal());
}

const char* CD3D9Renderer::GetWindowStateName() const
{
	switch (m_windowState)
	{
		case EWindowState::Fullscreen:
			return "Fullscreen";
		case EWindowState::BorderlessWindow:
			return "Fullscreen Window";
			break;
		case EWindowState::Windowed:
		default:
			return "Windowed";
			break;
	}
}

void CD3D9Renderer::BeginFrame(const SDisplayContextKey& displayContextKey)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_renderThreadAuxGeom.SetCurrentDisplayContext(displayContextKey);
#endif
	//////////////////////////////////////////////////////////////////////
	// Set up everything so we can start rendering
	//////////////////////////////////////////////////////////////////////

	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	CheckDeviceLost();

	FlushRTCommands(false, false, false);

	m_pRT->ExecuteRenderThreadCommand([=]
	{
		GetS3DRend().PrepareStereo();
	}, ERenderCommandFlags::None);

	CaptureFrameBufferPrepare();

	// Switching of MT mode in run-time
	//CV_r_multithreaded = 0;

	m_cEF.mfBeginFrame();

	CRenderElement::Tick();
	CFlashTextureSourceSharedRT::Tick();

	gEnv->nMainFrameID++;

	CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
	CREOcclusionQuery::m_nReadResultNowCounter = 0;
	CREOcclusionQuery::m_nReadResultTryCounter = 0;

	if (gEnv->p3DEngine)
		gEnv->p3DEngine->ResetCoverageBufferSignalVariables();

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		Logv("******************************* BeginFrame %d ********************************\n", gRenDev->GetMainFrameID());
#endif
	if (CRenderer::CV_r_logTexStreaming)
		LogStrv(0, "******************************* BeginFrame %d ********************************\n", gRenDev->GetMainFrameID());

	PREFAST_SUPPRESS_WARNING(6326)
	bool bUseWaterTessHW(CV_r_WaterTessellationHW != 0 && m_bDeviceSupportsTessellation);
	if (bUseWaterTessHW != m_bUseWaterTessHW)
	{
		PREFAST_SUPPRESS_WARNING(6326)
		m_bUseWaterTessHW = bUseWaterTessHW;
		m_cEF.mfReloadAllShaders(1, SHGD_HW_WATER_TESSELLATION, gRenDev->GetMainFrameID());
	}

	PREFAST_SUPPRESS_WARNING(6326)
	if ((CV_r_SilhouettePOM != 0) != m_bUseSilhouettePOM)
	{
		PREFAST_SUPPRESS_WARNING(6326)
		m_bUseSilhouettePOM = CV_r_SilhouettePOM != 0;
		m_cEF.mfReloadAllShaders(1, SHGD_HW_SILHOUETTE_POM, gRenDev->GetMainFrameID());
	}

	if (CV_r_reloadshaders)
	{
		//exit(0);
		//ShutDown();
		//iConsole->Exit("Test");

		m_cEF.m_Bin.InvalidateCache();
		m_cEF.mfReloadAllShaders(CV_r_reloadshaders, 0, gRenDev->GetMainFrameID());

#ifndef CONSOLE_CONST_CVAR_MODE
		CV_r_reloadshaders = 0;
#endif

		// after reloading shaders, update all shader items
		// and flush the RenderThread to make the changes visible
		gEnv->p3DEngine->UpdateShaderItems();
		gRenDev->FlushRTCommands(true, true, true);
	}

	const CCamera& camera = !GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled() ?
		gEnv->pSystem->GetViewCamera() :
		GetS3DRend().GetHeadLockedQuadCamera();

#if defined(ENABLE_RENDER_AUX_GEOM)
	if (auto pCurrAuxGeomCBCollector = m_currentAuxGeomCBCollector)
	{
		// Setting all aux geometries command buffers of the collector to the new camera.
		// Technically this may not be correct but for now it fixes the issues.
		pCurrAuxGeomCBCollector->SetDefaultCamera(camera);

		m_pRT->ExecuteRenderThreadCommand([=]
		{
			// Initialize render thread's aux geometry command buffer's camera
			m_renderThreadAuxGeom.SetCamera(camera);

			const SDisplayContextKey auxDisplayContextKey = GetS3DRend().IsStereoEnabled() ? 
				GetS3DRend().GetEyeDisplayContext(CCamera::eEye_Left).second :
				displayContextKey;
			pCurrAuxGeomCBCollector->SetDisplayContextKey(auxDisplayContextKey);

			m_nTimeSlicedShadowsUpdatedThisFrame = 0;
		}, ERenderCommandFlags::None);
	}
#endif

	m_pRT->RC_BeginFrame(displayContextKey);
}

void CD3D9Renderer::FillFrame(ColorF clearColor)
{
	CRenderDisplayContext* pDisplayContext = GetActiveDisplayContext();

	m_pRT->ExecuteRenderThreadCommand([=]
	{
		PROFILE_FRAME(RT_FillFrame);

		pDisplayContext->GetCurrentBackBuffer()->Clear(clearColor);
		pDisplayContext->GetRenderOutput()->m_hasBeenCleared |= FRT_CLEAR_COLOR;

		if (GetS3DRend().IsStereoEnabled())
		{
			GetS3DRend().PrepareFrame();
			GetS3DRend().ClearEyes(clearColor);
			GetS3DRend().ClearVrQuads(Clr_Transparent);   // Force transparent clear for VR quads
		}

	}, ERenderCommandFlags::SkipDuringLoading);
}

void CD3D9Renderer::RT_BeginFrame(const SDisplayContextKey& displayContextKey)
{
	PROFILE_FRAME(RT_BeginFrame);

#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	m_devInfo.ProcessSystemEventQueue();
#endif

	{
		const auto pair = SetCurrentContext(displayContextKey);
		CRY_ASSERT_MESSAGE(pair.first, "RT_BeginFrame: SetCurrentContext() failed!");
	}

	HandleDisplayPropertyChanges();

	GetActiveDisplayContext()->GetRenderOutput()->m_hasBeenCleared = 0;

#ifdef ENABLE_BENCHMARK_SENSOR
	m_benchmarkRendererSensor->update();
#endif
	{
		static const ICVar* pCVarShadows = nullptr;
		static const ICVar* pCVarShadowsClouds = nullptr;

		SELECT_ALL_GPU();
		// assume these cvars will exist
		if (!pCVarShadows) pCVarShadows = iConsole->GetCVar("e_Shadows");
		if (!pCVarShadowsClouds) pCVarShadowsClouds = iConsole->GetCVar("e_ShadowsClouds");

		m_bShadowsEnabled = pCVarShadows && pCVarShadows->GetIVal() != 0;
		m_bCloudShadowsEnabled = pCVarShadowsClouds && pCVarShadowsClouds->GetIVal() != 0;

		static ICVar* pCVarVolumetricFog = nullptr;
		if (!pCVarVolumetricFog) pCVarVolumetricFog = iConsole->GetCVar("e_VolumetricFog");
		bool bVolumetricFog = pCVarVolumetricFog && (pCVarVolumetricFog->GetIVal() != 0);
		m_bVolumetricFogEnabled = bVolumetricFog
		                          && CVolumetricFogStage::IsEnabledInFrame();
		if (pCVarVolumetricFog && bVolumetricFog && (CRenderer::CV_r_DeferredShadingTiled == 0))
		{
#if !defined(_RELEASE)
			gEnv->pLog->LogWarning("e_VolumetricFog is set to 0 when r_DeferredShadingTiled is 0.");
#endif
			pCVarVolumetricFog->Set((const int)0);
		}

		static ICVar* pCVarClouds = nullptr;
		if (!pCVarClouds) pCVarClouds = iConsole->GetCVar("e_Clouds");
		bool renderClouds = (pCVarClouds && (pCVarClouds->GetIVal() != 0)) ? true : false;
		m_bVolumetricCloudsEnabled = renderClouds && (CRenderer::CV_r_VolumetricClouds > 0);

#if defined(VOLUMETRIC_FOG_SHADOWS)
		Vec3 volFogShadowEnable(0, 0, 0);
		if (gEnv->p3DEngine)
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_ENABLE, volFogShadowEnable);

		m_bVolFogShadowsEnabled = m_bShadowsEnabled && CV_r_FogShadows != 0 && volFogShadowEnable.x != 0 && !m_bVolumetricFogEnabled;
		m_bVolFogCloudShadowsEnabled = m_bVolFogShadowsEnabled && m_bCloudShadowsEnabled && m_cloudShadowTexId > 0 && volFogShadowEnable.y != 0 && !m_bVolumetricCloudsEnabled;
#endif

		SRainParams& rainVolParams = m_p3DEngineCommon.m_RainInfo;
		m_bDeferredRainEnabled = (rainVolParams.fAmount * CRenderer::CV_r_rainamount > 0.05f
		                          && rainVolParams.fCurrentAmount > 0.05f
		                          && rainVolParams.fRadius > 0.05f
		                          && CV_r_rain > 0);

		SSnowParams& snowVolParams = m_p3DEngineCommon.m_SnowInfo;
		m_bDeferredSnowEnabled = ((snowVolParams.m_fSnowAmount > 0.05f || snowVolParams.m_fFrostAmount > 0.05f)
		                          && snowVolParams.m_fRadius > 0.05f
		                          && CV_r_snow > 0);

		const auto& arrOccluders = m_p3DEngineCommon.m_RainOccluders.m_arrCurrOccluders[gRenDev->GetRenderThreadID()];
		m_bDeferredRainOcclusionEnabled = (rainVolParams.bApplyOcclusion
		                                   && ((CV_r_snow == 2 && m_bDeferredSnowEnabled) || (CV_r_rain == 2 && m_bDeferredRainEnabled))
		                                   && !arrOccluders.empty());

		m_nGraphicsPipeline = CV_r_GraphicsPipeline;
	}

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	if (m_bUpdateD3DDebug)
	{
		m_d3dDebug.Update(ESeverityCombination(CV_d3d11_debugMuteSeverity->GetIVal()), CV_d3d11_debugMuteMsgID->GetString(), CV_d3d11_debugBreakOnMsgID->GetString());
		if (stricmp(CV_d3d11_debugBreakOnMsgID->GetString(), "0") != 0 && CV_d3d11_debugBreakOnce)
		{
			CV_d3d11_debugBreakOnMsgID->Set("0");
		}
		else
		{
			m_bUpdateD3DDebug = false;
		}
	}
#endif

	//////////////////////////////////////////////////////////////////////
	// Profiling and statistics
	//////////////////////////////////////////////////////////////////////
	{
		SRenderStatistics::s_pCurrentOutput = &m_frameRenderStats[m_nProcessThreadID];
		SRenderStatistics::s_pCurrentOutput->Begin(&m_frameRenderStats[m_nFillThreadID]);

		m_renderTargetStats.resize(0);

#if !defined(_RELEASE)
		m_pGraphicsPipeline->GetDrawCallInfoPerMesh()->clear();
		m_pGraphicsPipeline->GetDrawCallInfoPerNode()->clear();
#endif

		if (m_pPipelineProfiler)
			m_pPipelineProfiler->BeginFrame(gRenDev->GetRenderFrameID());
	}

	//////////////////////////////////////////////////////////////////////
	// Pre-Frame management
	//////////////////////////////////////////////////////////////////////

	// Delete resources scheduled for deletion.
	RT_DelayedDeleteResources(false);

	// Update PSOs
	GetDeviceObjectFactory().UpdatePipelineStates();

	CResFile::Tick();
	m_DevBufMan.Update(gRenDev->GetRenderFrameID(), false);
	GetDeviceObjectFactory().OnBeginFrame(gRenDev->GetRenderFrameID());

	// Render updated dynamic flash textures
	CFlashTextureSourceSharedRT::TickRT();
	CFlashTextureSourceBase::RenderLights();

	//////////////////////////////////////////////////////////////////////
	// Build the matrices
	//////////////////////////////////////////////////////////////////////

#ifndef CONSOLE_CONST_CVAR_MODE
	ICVar* pCVDebugTexelDensity = gEnv->pConsole->GetCVar("e_texeldensity");
	ICVar* pCVDebugDraw = gEnv->pConsole->GetCVar("e_debugdraw");
	CRendererCVars::CV_e_DebugTexelDensity = pCVDebugTexelDensity ? pCVDebugTexelDensity->GetIVal() : 0;
	CRendererCVars::CV_e_DebugDraw = pCVDebugDraw ? pCVDebugDraw->GetIVal() : 0;
#endif

	CheckDeviceLost();

	if (!m_bDeviceLost && m_bIsWindowActive)
	{
		if (CV_r_gamma + m_fDeltaGamma + GetS3DRend().GetGammaAdjustment() != m_fLastGamma || CV_r_brightness != m_fLastBrightness || CV_r_contrast != m_fLastContrast)
			SetGamma(CV_r_gamma + m_fDeltaGamma, CV_r_brightness, CV_r_contrast, false);
	}

	if (!m_bDeviceSupportsInstancing)
	{
		if (CV_r_geominstancing)
		{
			iLog->Log("Device doesn't support HW geometry instancing (or it's disabled)");
			_SetVar("r_GeomInstancing", 0);
		}
	}

	if (CV_r_usehwskinning != (int)m_bUseHWSkinning)
	{
		m_bUseHWSkinning = CV_r_usehwskinning != 0;
		CRenderElement* pRE = CRenderElement::s_RootGlobal.m_NextGlobal;
		for (pRE = CRenderElement::s_RootGlobal.m_NextGlobal; pRE != &CRenderElement::s_RootGlobal; pRE = pRE->m_NextGlobal)
		{
			CRenderElement* pR = (CRenderElement*)pRE;
			if (pR->mfIsHWSkinned())
				pR->mfReset();
		}
	}
	
	float mipLodBias = CRenderer::FX_GetAntialiasingType() == eAT_TSAA_MASK ? CV_r_AntialiasingTSAAMipBias : 0.0f;
	if (CV_r_texminanisotropy != m_nCurMinAniso || CV_r_texmaxanisotropy != m_nCurMaxAniso || mipLodBias != m_fCurMipLodBias)
	{
		m_nCurMinAniso = CV_r_texminanisotropy;
		m_nCurMaxAniso = CV_r_texmaxanisotropy;
		m_fCurMipLodBias = mipLodBias;
		for (uint32 i = 0; i < CShader::s_ShaderResources_known.Num(); i++)
		{
			CShaderResources* pSR = CShader::s_ShaderResources_known[i];
			if (!pSR)
				continue;
			pSR->AdjustForSpec();
		}

		auto getAnisoFilter = [](int nLevel)
		{
			int8 nFilter =
			  nLevel >= 16 ? FILTER_ANISO16X :
			  (nLevel >= 8 ? FILTER_ANISO8X :
			   (nLevel >= 4 ? FILTER_ANISO4X :
			    (nLevel >= 2 ? FILTER_ANISO2X :
			     FILTER_TRILINEAR)));

			return nFilter;
		};

		int8 nAniso = min(CRenderer::CV_r_texminanisotropy, CRenderer::CV_r_texmaxanisotropy); // for backwards compatibility. should really be CV_r_texmaxanisotropy

		SSamplerState ssAnisoHigh(getAnisoFilter(nAniso), false);
		SSamplerState ssAnisoLow(getAnisoFilter(CRenderer::CV_r_texminanisotropy), false);
		SSamplerState ssAnisoBorder(getAnisoFilter(nAniso), eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, 0x0);

		ssAnisoHigh.SetMipLodBias(mipLodBias);
		ssAnisoLow.SetMipLodBias(mipLodBias);
		ssAnisoBorder.SetMipLodBias(mipLodBias);
		
		m_nMaterialAnisoHighSampler = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(ssAnisoHigh);
		m_nMaterialAnisoLowSampler = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(ssAnisoLow);
		m_nMaterialAnisoSamplerBorder = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(ssAnisoBorder);
	}

	// Verify if water caustics needed at all
	if (CV_r_watercaustics)
	{
		N3DEngineCommon::SOceanInfo& OceanInfo = gRenDev->m_p3DEngineCommon.m_OceanInfo;
		m_bWaterCaustics = (OceanInfo.m_nOceanRenderFlags & OCR_OCEANVOLUME_VISIBLE) != 0;
	}

	m_drawNearFov = CV_r_drawnearfov;

	if (CV_r_wireframe != m_wireframe_mode)
	{
		//assert(CV_r_wireframe == R_WIREFRAME_MODE || CV_r_wireframe == R_SOLID_MODE);
		m_wireframe_mode = CV_r_wireframe;
	}

	UpdateRenderingModesInfo();

	//////////////////////////////////////////////////////////////////////
	// Begin the scene
	//////////////////////////////////////////////////////////////////////
	ChangeLog();

	if (!m_SceneRecurseCount)
	{
		m_SceneRecurseCount++;
	}

	m_nStencilMaskRef = STENCIL_VALUE_OUTDOORS + 1;

	{
		static float fWaitForGPU;
		float fSmooth = 5.0f;
		fWaitForGPU = (m_fTimeWaitForGPU[gRenDev->GetMainThreadID()] + fWaitForGPU * fSmooth) / (fSmooth + 1.0f);
		if (fWaitForGPU >= 0.004f)
		{
			if (m_nGPULimited < 1000)
				m_nGPULimited++;
		}
		else
			m_nGPULimited = 0;

		m_bUseGPUFriendlyBatching[gRenDev->GetRenderThreadID()] = m_nGPULimited > 10; // On PC if we are GPU limited use z-pass distance sorting and disable instancing
		if (CV_r_batchtype == 0)
			m_bUseGPUFriendlyBatching[gRenDev->GetRenderThreadID()] = false;
		else if (CV_r_batchtype == 1)
			m_bUseGPUFriendlyBatching[gRenDev->GetRenderThreadID()] = true;
	}
}

bool CD3D9Renderer::CheckDeviceLost()
{
#if CRY_PLATFORM_WINDOWS
	if (m_pRT && CV_r_multithreaded == 1 && m_pRT->IsRenderThread())
		return false;

	// DX10/11 should still handle gamma changes on window focus loss
	if (!m_bStartLevelLoading)
	{
		const bool bWindowActive = (GetForegroundWindow() == m_hWnd);

		// Adjust gamma to match focused window
		if (bWindowActive != m_bIsWindowActive)
		{
			if (bWindowActive)
			{
				SetGamma(CV_r_gamma + m_fDeltaGamma, CV_r_brightness, CV_r_contrast, true);
			}
			else
			{
				RestoreGamma();
			}

			m_bIsWindowActive = bWindowActive;
		}
	}
#endif
	return false;
}

bool CD3D9Renderer::RT_StoreTextureToFile(const char* szFilePath, CTexture* pSrc)
{
	if (!szFilePath)
		return false;

	bool captureSuccess = false;

	const char* pReqFileFormatExt(PathUtil::GetExt(szFilePath));
	SCaptureFormatInfo::ECaptureFileFormat captureFormat = SCaptureFormatInfo::GetCaptureFormatByExtension(pReqFileFormatExt);

	bool formatBGRA = pSrc->GetDevTexture()->GetNativeFormat() == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	bool needRBSwap = (captureFormat == SCaptureFormatInfo::eCaptureFormat_TGA ? !formatBGRA : formatBGRA);	SResourceDimension srcDimensions = pSrc->GetDevTexture()->GetDimension();

	EReadTextureFormat dstFormat = (captureFormat == SCaptureFormatInfo::eCaptureFormat_TGA) ? EReadTextureFormat::BGR8 : EReadTextureFormat::RGB8;

	byte* pDest = new byte[srcDimensions.Width * srcDimensions.Height * 3]; // 3 eight bit channels

	if (RT_ReadTexture(pDest, srcDimensions.Width, srcDimensions.Height, dstFormat, pSrc))
	{
		switch (captureFormat)
		{
		case SCaptureFormatInfo::eCaptureFormat_TGA:
			captureSuccess = ::WriteTGA(pDest, srcDimensions.Width, srcDimensions.Height, szFilePath, 3 * 8, 3 * 8);
			break;
		case SCaptureFormatInfo::eCaptureFormat_JPEG:
			captureSuccess = ::WriteJPG(pDest, srcDimensions.Width, srcDimensions.Height, szFilePath, 3 * 8 , 90);
			break;
		case SCaptureFormatInfo::eCaptureFormat_PNG:
			captureSuccess = ::WritePNG(pDest, srcDimensions.Width, srcDimensions.Height, szFilePath);
			break;
		}
	}

	delete[] pDest;

	return captureSuccess;
}

#define DEPTH_BUFFER_SCALE 1024.0f

void CD3D9Renderer::CaptureFrameBuffer()
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	CacheCaptureCVars();
	if (!CV_capture_frames || !CV_capture_folder || !CV_capture_file_format || !CV_capture_frame_once ||
	    !CV_capture_file_name || !CV_capture_file_prefix)
		return;

	int frameNum(CV_capture_frames->GetIVal());
	if (frameNum > 0)
	{
		char path[ICryPak::g_nMaxPath];
		path[0] = '\0';

		const char* capture_file_name = CV_capture_file_name->GetString();
		if (capture_file_name && capture_file_name[0])
		{
			gEnv->pCryPak->AdjustFileName(capture_file_name, path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
		}

		if (path[0] == '\0')
		{
			gEnv->pCryPak->AdjustFileName(CV_capture_folder->GetString(), path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);
			gEnv->pCryPak->MakeDir(path);

			char prefix[64] = "Frame";
			const char* capture_file_prefix = CV_capture_file_prefix->GetString();
			if (capture_file_prefix != 0 && capture_file_prefix[0])
			{
				cry_strcpy(prefix, capture_file_prefix);
			}

			const size_t pathLen = strlen(path);
			cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "\\%s%06d.%s", prefix, frameNum - 1, CV_capture_file_format->GetString());
		}

		if (CV_capture_frame_once->GetIVal())
		{
			CV_capture_frames->Set(0);
			CV_capture_frame_once->Set(0);
		}
		else
			CV_capture_frames->Set(frameNum + 1);

		if (!RT_StoreTextureToFile(path, GetActiveDisplayContext()->GetPresentedBackBuffer()))
		{
			if (iLog)
				iLog->Log("Warning: Frame capture failed!\n");
			CV_capture_frames->Set(0); // disable capturing
		}
	}
}

void CD3D9Renderer::ResolveSupersampledRendering()
{
	// CRendererResources::s_ptexBackBuffer/CRendererResources::s_ptexSceneSpecular -> pContext->GetColorOutput()
	if (!m_pActiveContext->IsSuperSamplingEnabled())
		return;

	PROFILE_LABEL_SCOPE("RESOLVE_SUPERSAMPLED");

	const CRenderView* pRenderView = GetGraphicsPipeline().GetCurrentRenderView();
	const CRenderOutput* pOutput = GetGraphicsPipeline().GetCurrentRenderOutput();
	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	CDownsamplePass::EFilterType eFilter = CDownsamplePass::FilterType_Box;
	if (CV_r_SupersamplingFilter == 1)
		eFilter = CDownsamplePass::FilterType_Tent;
	else if (CV_r_SupersamplingFilter == 2)
		eFilter = CDownsamplePass::FilterType_Gauss;
	else if (CV_r_SupersamplingFilter == 3)
		eFilter = CDownsamplePass::FilterType_Lanczos;

	// TODO: respect CRenderDisplayContext::GetViewport() instead of using full resolution
	// NOTE: DownscalePass only supports whole factors of scaling, only SS resolve is possible here
	CRY_ASSERT(
		(CRendererResources::s_renderWidth  % pOutput->GetOutputResolution()[0]) == 0 &&
		(CRendererResources::s_renderHeight % pOutput->GetOutputResolution()[1]) == 0);

	GetGraphicsPipeline().m_DownscalePass->Execute(
		pRenderView->GetColorTarget(),
		pOutput->GetColorTarget(),
		CRendererResources::s_renderWidth, CRendererResources::s_renderHeight,
		pOutput->GetOutputResolution()[0], pOutput->GetOutputResolution()[1],
		eFilter);
}

void CD3D9Renderer::ResolveSubsampledOutput()
{
	// CRendererResources::s_ptexBackBuffer/CRendererResources::s_ptexSceneSpecular -> pContext->GetColorOutput()
	if (!m_pActiveContext->IsNativeScalingEnabled())
		return;

	PROFILE_LABEL_SCOPE("RESOLVE_SUBSAMPLED");

	auto* pColorTarget = GetGraphicsPipeline().GetCurrentRenderView()->GetColorTarget();
	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	CRY_ASSERT(pColorTarget != pDC->GetStorableColorOutput());
	CRY_ASSERT(pColorTarget != pDC->GetCurrentBackBuffer());

	// TODO: add HDR meta-data coding to upscaling
	GetGraphicsPipeline().m_UpscalePass->Execute(pColorTarget, pDC->GetCurrentBackBuffer());
}

void CD3D9Renderer::ResolveHighDynamicRangeDisplay()
{
	if (m_pActiveContext->IsNativeScalingEnabled() || !m_pActiveContext->IsHighDynamicRangeDisplay())
		return;

	PROFILE_LABEL_SCOPE("RESOLVE_HIGHDYNAMICRANGE");

	const CRenderOutput* pOutput = GetGraphicsPipeline().GetCurrentRenderOutput();
	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	CRY_ASSERT(pOutput->GetColorTarget() == pDC->GetStorableColorOutput());
	CRY_ASSERT(pOutput->GetColorTarget() != pDC->GetCurrentBackBuffer());

	// TODO: add HDR meta-data coding to back-buffer copy
	CStretchRectPass::GetPass().Execute(pOutput->GetColorTarget(), pDC->GetCurrentBackBuffer());
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::ClearPerFrameData(const SRenderingPassInfo& passInfo)
{
	CRenderer::ClearPerFrameData(passInfo);
}

static float LogMap(const float fA)
{
	return logf(fA) / logf(2.0f) + 5.5f;    // offset to bring values <0 in viewable range
}

void CD3D9Renderer::PrintResourcesLeaks()
{
#ifndef _RELEASE
	iLog->Log("\n \n");

	CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

	CCryNameTSCRC Name;
	SResourceContainer* pRL;
	uint32 n = 0;
	uint32 i;
	Name = CShader::mfGetClassName();
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CShader* sh = (CShader*)itor->second;
			if (!sh)
				continue;
			Warning("--- CShader '%s' leak after level unload", sh->GetName());
			n++;
		}
	}
	iLog->Log("\n \n");

	n = 0;
	for (i = 0; i < CShader::s_ShaderResources_known.Num(); i++)
	{
		CShaderResources* pSR = CShader::s_ShaderResources_known[i];
		if (!pSR)
			continue;
		n++;
		if (pSR->m_szMaterialName)
			Warning("--- Shader Resource '%s' leak after level unload", pSR->m_szMaterialName);
	}
	if (!n)
		CShader::s_ShaderResources_known.Free();
	iLog->Log("\n \n");

	int nVS = 0;
	Name = CHWShader::mfGetClassName(eHWSC_Vertex);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* vsh = (CHWShader*)itor->second;
			if (!vsh)
				continue;
			nVS++;
			Warning("--- Vertex Shader '%s' leak after level unload", vsh->GetName());
		}
	}
	iLog->Log("\n \n");

	int nPS = 0;
	Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* psh = (CHWShader*)itor->second;
			if (!psh)
				continue;
			nPS++;
			Warning("--- Pixel Shader '%s' leak after level unload", psh->GetName());
		}
	}
	iLog->Log("\n \n");

	n = 0;
	Name = CTexture::mfGetClassName();
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CTexture* pTX = (CTexture*)itor->second;
			if (!pTX)
				continue;
			if (!pTX->m_bCreatedInLevel)
				continue;
			Warning("--- CTexture '%s' leak after level unload", pTX->GetName());
			n++;
		}
	}
	iLog->Log("\n \n");

	CRenderElement* pRE;
	for (pRE = CRenderElement::s_RootGlobal.m_NextGlobal; pRE != &CRenderElement::s_RootGlobal; pRE = pRE->m_NextGlobal)
	{
		const char* name = pRE->mfGetType() == eDATA_Mesh && ((CREMeshImpl*)pRE)->m_pRenderMesh ? ((CREMeshImpl*)pRE)->m_pRenderMesh->m_sSource.c_str() : "-";

		Warning("--- CRenderElement \"%s\" leak after level unload: %s", pRE->mfTypeString(), name);
	}
	iLog->Log("\n \n");

	CRenderMesh::PrintMeshLeaks();
#endif
}

#define BYTES_TO_KB(bytes) ((bytes) / 1024.0)
#define BYTES_TO_MB(bytes) ((bytes) / 1024.0 / 1024.0)


static void AuxDrawQuad(float x0, float y0, float x1, float y1, const ColorF & color, float z = 1.0f)
{
	IRenderAuxGeom::GetAux()->DrawQuad(Vec3(x0, y0, z), ColorB(color), Vec3(x1, y0, z), ColorB(color), Vec3(x1, y1, z), ColorB(color), Vec3(x0, y1, z), ColorB(color));
}

static int DebugVertBufferSize(D3DVertexBuffer* pVB)
{
	if (!pVB)
		return 0;
#if CRY_RENDERER_GNM
	return pVB->GnmGetTotalSize();
#else
	D3D11_BUFFER_DESC Desc;
	pVB->GetDesc(&Desc);
	return Desc.ByteWidth;
#endif
}
static int DebugIndexBufferSize(D3DIndexBuffer* pIB)
{
	if (!pIB)
		return 0;
#if CRY_RENDERER_GNM
	return pIB->GnmGetTotalSize();
#else
	D3D11_BUFFER_DESC Desc;
	pIB->GetDesc(&Desc);
	return Desc.ByteWidth;
#endif
}


void CD3D9Renderer::DebugDrawStats1(const SRenderStatistics& RStats)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

	const int nYstep = 10;
	uint32 i;
	int nY = 30; // initial Y pos
	int nX = 50; // initial X pos

	ColorF col = Col_Yellow;
	IRenderAuxText::Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Per-frame stats:");

	col = Col_White;
	nX += 10;
	nY += 25;
	//DebugDrawRect(nX-2, nY, nX+180, nY+150, &col.r);
	IRenderAuxText::Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Draw-calls:");

	float fFSize = 1.2f;

	nX += 5;
	nY += 10;
	int nXBars = nX;

	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "General: %d (%d polys)", RStats.m_nDIPs[EFSLIST_GENERAL], RStats.m_nPolygons[EFSLIST_GENERAL]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Decals: %d (%d polys)", RStats.m_nDIPs[EFSLIST_DECAL], RStats.m_nPolygons[EFSLIST_DECAL]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Terrain layers: %d (%d polys)", RStats.m_nDIPs[EFSLIST_TERRAINLAYER], RStats.m_nPolygons[EFSLIST_TERRAINLAYER]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Transparent aw: %d (%d polys)", RStats.m_nDIPs[EFSLIST_TRANSP_AW], RStats.m_nPolygons[EFSLIST_TRANSP_AW]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Transparent bw: %d (%d polys)", RStats.m_nDIPs[EFSLIST_TRANSP_BW], RStats.m_nPolygons[EFSLIST_TRANSP_BW]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shadow-gen: %d (%d polys)", RStats.m_nDIPs[EFSLIST_SHADOW_GEN], RStats.m_nPolygons[EFSLIST_SHADOW_GEN]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shadow-pass: %d (%d polys)", RStats.m_nDIPs[EFSLIST_SHADOW_PASS], RStats.m_nPolygons[EFSLIST_SHADOW_PASS]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Water: %d (%d polys)", RStats.m_nDIPs[EFSLIST_WATER], RStats.m_nPolygons[EFSLIST_WATER]);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Imposters: %d (Updates: %d)", RStats.m_NumCloudImpostersDraw, RStats.m_NumCloudImpostersUpdates);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Sprites: %d (%d dips, %d updates, %d altases, %d cells, %d polys)", RStats.m_NumSprites, RStats.m_NumSpriteDIPS, RStats.m_NumSpriteUpdates, RStats.m_NumSpriteAltasesUsed, RStats.m_NumSpriteCellsUsed, RStats.m_NumSpritePolys);

	IRenderAuxText::Draw2dLabel(nX - 5, nY + 20, 1.4f, &col.r, false, "Total: %d (%d polys)", RStats.GetNumberOfDrawCalls(), RStats.GetNumberOfPolygons());

	col = Col_Yellow;
	nX -= 5;
	nY += 45;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	IRenderAuxText::Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Occlusions: Issued: %d, Occluded: %d, NotReady: %d", RStats.m_NumQIssued, RStats.m_NumQOccluded, RStats.m_NumQNotReady);

	col = Col_Cyan;
	nX -= 5;
	nY += 45;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	IRenderAuxText::Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource switches:");

	nX += 5;
	nY += 10;
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "VShaders: %d (%d unique)", RStats.m_NumVShadChanges, RStats.m_NumVShaders);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "PShaders: %d (%d unique)", RStats.m_NumPShadChanges, RStats.m_NumPShaders);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Textures: %d (%d unique)", RStats.m_NumTextChanges, RStats.m_NumTextures);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "RT's: %d (%d unique), cleared: %d times, copied: %d times", RStats.m_NumRTChanges, RStats.m_NumRTs, RStats.m_RTCleared, RStats.m_RTCopied);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "States: %d", RStats.m_NumStateChanges);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MatBatches: %d, GeomBatches: %d, Instances: %d", RStats.m_NumRendMaterialBatches, RStats.m_NumRendGeomBatches, RStats.m_NumRendInstances);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "HW Instances: DIP's: %d, Instances: %d (polys: %d/%d)", RStats.m_RendHWInstancesDIPs, RStats.m_NumRendHWInstances, RStats.m_RendHWInstancesPolysOne, RStats.m_RendHWInstancesPolysAll);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Skinned instances: %d", RStats.m_NumRendSkinnedObjects);

	CHWShader_D3D* ps = (CHWShader_D3D*)RStats.m_pMaxPShader;
	CHWShader_D3D::SHWSInstance* pInst = (CHWShader_D3D::SHWSInstance*)RStats.m_pMaxPSInstance;
	if (ps)
		IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MAX PShader: %s (instructions: %d, lights: %d)", ps->GetName(), pInst->m_nInstructions, pInst->m_Ident.m_LightMask & 0xf);
	CHWShader_D3D* vs = (CHWShader_D3D*)RStats.m_pMaxVShader;
	pInst = (CHWShader_D3D::SHWSInstance*)RStats.m_pMaxVSInstance;
	if (vs)
		IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MAX VShader: %s (instructions: %d, lights: %d)", vs->GetName(), pInst->m_nInstructions, pInst->m_Ident.m_LightMask & 0xf);

	col = Col_Green;
	nX -= 5;
	nY += 35;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	IRenderAuxText::Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource sizes:");

	nX += 5;
	nY += 10;
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Managed non-streamed textures: Sys=%.3f Mb, Vid:=%.3f", BYTES_TO_MB(RStats.m_ManagedTexturesSysMemSize), BYTES_TO_MB(RStats.m_ManagedTexturesVidMemSize));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Managed streamed textures: Sys=%.3f Mb, Vid:=%.3f", BYTES_TO_MB(RStats.m_ManagedTexturesStreamSysSize), BYTES_TO_MB(RStats.m_ManagedTexturesStreamVidSize));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "RT textures: Used: %.3f Mb, Updated: %.3f Mb, Cleared: %.3f Mb, Copied: %.3f Mb", BYTES_TO_MB(RStats.m_DynTexturesSize), BYTES_TO_MB(RStats.m_RTSize), BYTES_TO_MB(RStats.m_RTClearedSize), BYTES_TO_MB(RStats.m_RTCopiedSize));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Meshes updated: Static: %.3f Mb, Dynamic: %.3f Mb", BYTES_TO_MB(RStats.m_MeshUpdateBytes), BYTES_TO_MB(RStats.m_DynMeshUpdateBytes));

	int nYBars = nY;

	nY = 30;  // initial Y pos
	nX = 430; // initial X pos
	col = Col_Yellow;
	IRenderAuxText::Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Global stats:");

	col = Col_YellowGreen;
	nX += 10;
	nY += 55;
	IRenderAuxText::Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Mesh size:");

	size_t nMemApp = 0;
	size_t nMemDevVB = 0;
	size_t nMemDevIB = 0;
	size_t nMemDevVBPool = 0;
	size_t nMemDevIBPool = 0;
	size_t nMemDevVBPoolUsed = 0;
	size_t nMemDevIBPoolUsed = 0;
	{
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::s_MeshList.prev; iter != &CRenderMesh::s_MeshList; iter = iter->prev)
		{
			CRenderMesh* pRM = iter->item<& CRenderMesh::m_Chain>();
			nMemApp += pRM->Size(CRenderMesh::SIZE_ONLY_SYSTEM);
			nMemDevVB += pRM->Size(CRenderMesh::SIZE_VB);
			nMemDevIB += pRM->Size(CRenderMesh::SIZE_IB);
		}
	}
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Static: (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb)",
	                            nMemApp / 1024.0f / 1024.0f, nMemDevVB / 1024.0f / 1024.0f, nMemDevIB / 1024.0f / 1024.0f);

	for (i = 0; i < BBT_MAX; i++)
		for (int j = 0; j < BU_MAX; j++)
		{
			SDeviceBufferPoolStats stats;
			if (m_DevBufMan.GetStats((BUFFER_BIND_TYPE)i, (BUFFER_USAGE)j, stats) == false)
				continue;
			IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Pool '%10s': size %5.3f banks %4" PRISIZE_T " allocs %6d frag %3.3f pinned %4d moving %4d",
			                            stats.buffer_descr.c_str()
			                            , (stats.num_banks * stats.bank_size) / (1024.f * 1024.f)
			                            , stats.num_banks
			                            , stats.allocator_stats.nInUseBlocks
			                            , (stats.allocator_stats.nCapacity - stats.allocator_stats.nInUseSize - stats.allocator_stats.nLargestFreeBlockSize) / (float)max(stats.allocator_stats.nCapacity, (size_t)1)
			                            , stats.allocator_stats.nPinnedBlocks
			                            , stats.allocator_stats.nMovingBlocks);
		}

	nMemDevVB = 0;
	nMemDevIB = 0;
	nMemApp = 0;

	uint32 j;
	for (i = 0; i < SHAPE_MAX; i++)
	{
		nMemDevVB += DebugVertBufferSize(m_pUnitFrustumVB[i]);
		nMemDevIB += DebugIndexBufferSize(m_pUnitFrustumIB[i]);
	}

	#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		nMemDevVB += m_pRenderAuxGeomD3D->GetDeviceDataSize();
	#endif

	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Dynamic: %.3f (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb)", BYTES_TO_MB(nMemDevVB + nMemDevIB), BYTES_TO_MB(nMemApp), BYTES_TO_MB(nMemDevVB), BYTES_TO_MB(nMemDevIB) /*, BYTES_TO_MB(nMemDevVBPool)*/);

	CCryNameTSCRC Name;
	SResourceContainer* pRL;
	uint32 n = 0;
	size_t nSize = 0;
	Name = CShader::mfGetClassName();
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CShader* sh = (CShader*)itor->second;
			if (!sh)
				continue;
			nSize += sh->Size(0);
			n++;
		}
	}
	nY += nYstep;
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "FX Shaders: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

	n = 0;
	nSize = m_cEF.m_Bin.mfSizeFXParams(n);
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "FX Shader parameters for %d shaders (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

	nSize = 0;
	n = 0;
	for (i = 0; i < (int)CShader::s_ShaderResources_known.Num(); i++)
	{
		CShaderResources* pSR = CShader::s_ShaderResources_known[i];
		if (!pSR)
			continue;
		nSize += pSR->Size();
		n++;
	}
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shader resources: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shader manager (size: %.3f Mb)", BYTES_TO_MB(m_cEF.Size()));

	n = 0;
	for (i = 0; i < CGParamManager::s_Groups.size(); i++)
	{
		n += CGParamManager::s_Groups[i].nParams;

	}
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Groups: %d, Shader parameters: %d (size: %.3f Mb), in pool: %d (size: %.3f Mb)", i, n, BYTES_TO_MB(n * sizeof(SCGParam)), CGParamManager::s_Pools.size(), BYTES_TO_MB(CGParamManager::s_Pools.size() * PARAMS_POOL_SIZE * sizeof(SCGParam)));

	nY += nYstep;
	nY += nYstep;
	TArray<void*> ShadersVS;
	TArray<void*> ShadersPS;

	nSize = 0;
	n = 0;
	int nInsts = 0;
	Name = CHWShader::mfGetClassName(eHWSC_Vertex);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* vsh = (CHWShader*)itor->second;
			if (!vsh)
				continue;
			nSize += vsh->Size();
			n++;

			CHWShader_D3D* pD3D = (CHWShader_D3D*)vsh;
			for (i = 0; i < pD3D->m_Insts.size(); i++)
			{
				CHWShader_D3D::SHWSInstance* pShInst = pD3D->m_Insts[i];
				if (pShInst->m_bDeleted)
					continue;
				nInsts++;
				if (pShInst->m_Handle.m_pShader)
				{
					for (j = 0; j < (int)ShadersVS.Num(); j++)
					{
						if (ShadersVS[j] == pShInst->m_Handle.m_pShader->GetHandle())
							break;
					}
					if (j == ShadersVS.Num())
					{
						ShadersVS.AddElem(pShInst->m_Handle.m_pShader->GetHandle());
					}
				}
			}
		}
	}
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "VShaders: %d (size: %.3f Mb), Instances: %d, Device VShaders: %d (Size: %.3f Mb)", n, BYTES_TO_MB(nSize), nInsts, ShadersVS.Num(), BYTES_TO_MB(CHWShader_D3D::s_nDeviceVSDataSize));

	nInsts = 0;
	Name = CHWShader::mfGetClassName(eHWSC_Pixel);
	pRL = CBaseResource::GetResourcesForClass(Name);
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CHWShader* psh = (CHWShader*)itor->second;
			if (!psh)
				continue;
			nSize += psh->Size();
			n++;

			CHWShader_D3D* pD3D = (CHWShader_D3D*)psh;
			for (i = 0; i < pD3D->m_Insts.size(); i++)
			{
				CHWShader_D3D::SHWSInstance* pShInst = pD3D->m_Insts[i];
				if (pShInst->m_bDeleted)
					continue;
				nInsts++;
				if (pShInst->m_Handle.m_pShader)
				{
					for (j = 0; j < (int)ShadersPS.Num(); j++)
					{
						if (ShadersPS[j] == pShInst->m_Handle.m_pShader->GetHandle())
							break;
					}
					if (j == ShadersPS.Num())
					{
						ShadersPS.AddElem(pShInst->m_Handle.m_pShader->GetHandle());
					}
				}
			}
		}
	}
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "PShaders: %d (size: %.3f Mb), Instances: %d, Device PShaders: %d (Size: %.3f Mb)", n, BYTES_TO_MB(nSize), nInsts, ShadersPS.Num(), BYTES_TO_MB(CHWShader_D3D::s_nDevicePSDataSize));

	n = 0;
	nSize = 0;
	size_t nSizeD = 0;
	CRenderElement* pRE = CRenderElement::s_RootGlobal.m_NextGlobal;
	while (pRE != &CRenderElement::s_RootGlobal)
	{
		n++;
		nSize += pRE->Size();
		pRE = pRE->m_NextGlobal;
	}
	nY += nYstep;
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Render elements: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

	size_t nSAll = 0;
	size_t nSOneMip = 0;
	size_t nSNM = 0;
	size_t nSysAll = 0;
	size_t nSysOneMip = 0;
	size_t nSysNM = 0;
	size_t nSRT = 0;
	size_t nObjSize = 0;
	size_t nStreamed = 0;
	size_t nStreamedSys = 0;
	size_t nStreamedUnload = 0;
	n = 0;
	pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CTexture* tp = (CTexture*)itor->second;
			if (!tp || tp->IsNoTexture())
				continue;
			n++;
			nObjSize += tp->GetSize(true);
			int nS = tp->GetDeviceDataSize();
			int nSys = tp->GetDataSize();
			if (tp->IsStreamed())
			{
				if (tp->IsUnloaded())
				{
					assert(nS == 0);
					nStreamedUnload += nSys;
				}
				else if (tp->GetDevTexture())
					nStreamedSys += nSys;
				nStreamed += nS;
			}
			if (tp->GetDevTexture())
			{
				if (!(tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS)))
				{
					if (tp->GetName()[0] != '$' && tp->GetNumMips() <= 1)
						nSysOneMip += nSys;
					if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
						nSysNM += nSys;
					else
						nSysAll += nSys;
				}
			}
			if (!nS)
				continue;
			if (tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
				nSRT += nS;
			else
			{
				if (tp->GetName()[0] != '$' && tp->GetNumMips() <= 1)
					nSOneMip += nS;
				if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
					nSNM += nS;
				else
					nSAll += nS;
			}
			
			nSAll += (tp->GetFlags() & FT_STAGE_UPLOAD)   ? nS : 0;
			nSAll += (tp->GetFlags() & FT_STAGE_READBACK) ? nS : 0;
		}
	}

	nY += nYstep;
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "CryName: %d, Size: %.3f Mb...", CCryNameR::GetNumberOfEntries(), BYTES_TO_MB(CCryNameR::GetMemoryUsage()));
	nY += nYstep;

	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Textures: %d, ObjSize: %.3f Mb...", n, BYTES_TO_MB(nObjSize));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " All Managed Video Size: %.3f Mb (Normals: %.3f Mb + Other: %.3f Mb), One mip: %.3f", BYTES_TO_MB(nSNM + nSAll), BYTES_TO_MB(nSNM), BYTES_TO_MB(nSAll), BYTES_TO_MB(nSOneMip));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " All Managed System Size: %.3f Mb (Normals: %.3f Mb + Other: %.3f Mb), One mip: %.3f", BYTES_TO_MB(nSysNM + nSysAll), BYTES_TO_MB(nSysNM), BYTES_TO_MB(nSysAll), BYTES_TO_MB(nSysOneMip));
	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " Streamed Size: Video: %.3f, System: %.3f, Unloaded: %.3f", BYTES_TO_MB(nStreamed), BYTES_TO_MB(nStreamedSys), BYTES_TO_MB(nStreamedUnload));

	SDynTexture_Shadow* pTXSH = SDynTexture_Shadow::s_RootShadow.m_NextShadow;
	size_t nSizeSH = 0;
	while (pTXSH != &SDynTexture_Shadow::s_RootShadow)
	{
		if (pTXSH->m_pTexture)
			nSizeSH += pTXSH->m_pTexture->GetDeviceDataSize();
		pTXSH = pTXSH->m_NextShadow;
	}

	size_t nSizeAtlasClouds = SDynTexture2::s_nMemoryOccupied[eTP_Clouds];
	size_t nSizeAtlasSprites = SDynTexture2::s_nMemoryOccupied[eTP_Sprites];
	size_t nSizeAtlasVoxTerrain = SDynTexture2::s_nMemoryOccupied[eTP_VoxTerrain];
	size_t nSizeAtlasDynTexSources = SDynTexture2::s_nMemoryOccupied[eTP_DynTexSources];
	size_t nSizeAtlas = nSizeAtlasClouds + nSizeAtlasSprites + nSizeAtlasVoxTerrain + nSizeAtlasDynTexSources;
	size_t nSizeManagedDyn = SDynTexture::s_nMemoryOccupied;

	IRenderAuxText::Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " Dynamic DataSize: %.3f Mb (Atlases: %.3f Mb, Managed: %.3f Mb (Shadows: %.3f Mb), Other: %.3f Mb)", BYTES_TO_MB(nSRT), BYTES_TO_MB(nSizeAtlas), BYTES_TO_MB(nSizeManagedDyn), BYTES_TO_MB(nSizeSH), BYTES_TO_MB(nSRT - nSizeManagedDyn - nSizeAtlas));

	size_t nSizeZRT = 0;
	size_t nSizeCRT = 0;

	DebugPerfBars(RStats,nXBars, nYBars + 30);
#endif
}

void CD3D9Renderer::DebugVidResourcesBars(int nX, int nY)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS)
	CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

	int i;
	int nYst = 15;
	float fFSize = 1.4f;
	ColorF col = Col_Yellow;

	// Draw performance bars
	
	//FX_SetState(GS_NODEPTHTEST);

	float fMaxBar = 200;
	float fOffs = 190.0f;

	ColorF colT = Col_Gray;
	IRenderAuxText::Draw2dLabel(nX + 50, nY, 1.6f, &colT.r, false, "Video resources:");
	nY += 20;

	double fMaxTextureMemory = m_MaxTextureMemory * 1024.0 * 1024.0;

	ColorF colF = Col_Orange;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Total memory: %.1f Mb", BYTES_TO_MB(fMaxTextureMemory));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + fMaxBar, nY + 12, Col_Cyan, 1.0f);
	nY += nYst;

	SDynTexture_Shadow* pTXSH = SDynTexture_Shadow::s_RootShadow.m_NextShadow;
	size_t nSizeSH = 0;
	while (pTXSH != &SDynTexture_Shadow::s_RootShadow)
	{
		if (pTXSH->m_pTexture)
			nSizeSH += pTXSH->m_pTexture->GetDeviceDataSize();
		pTXSH = pTXSH->m_NextShadow;
	}
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Shadow textures: %.1f Mb", BYTES_TO_MB(nSizeSH));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeSH / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	SDynTexture* pTX = SDynTexture::s_Root.m_Next;
	size_t nSizeD = 0;
	while (pTX != &SDynTexture::s_Root)
	{
		if (pTX->m_pTexture)
			nSizeD += pTX->m_pTexture->GetDeviceDataSize();
		pTX = pTX->m_Next;
	}
	nSizeD -= nSizeSH;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. text.: %.1f Mb", BYTES_TO_MB(nSizeD));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeD / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeD2 = 0;
	for (i = 0; i < eTP_Max; i++)
	{
		CryAutoReadLock<CryRWLock> scopeLock(SDynTexture2::s_memoryOccupiedLock);
		nSizeD2 += SDynTexture2::s_nMemoryOccupied[i];
	}
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. atlas text.: %.1f Mb", BYTES_TO_MB(nSizeD2));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeD2 / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSAll = 0;
	size_t nSOneMip = 0;
	size_t nSNM = 0;
	size_t nSRT = 0;
	size_t nObjSize = 0;
	size_t nStreamed = 0;
	size_t nStreamedSys = 0;
	size_t nStreamedUnload = 0;
	i = 0;
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	if (pRL)
	{
		ResourcesMapItor itor;
		for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
		{
			CTexture* tp = (CTexture*)itor->second;
			if (!tp || tp->IsNoTexture())
				continue;
			i++;
			nObjSize += tp->GetSize(true);
			int nS = tp->GetDeviceDataSize();
			if (tp->IsStreamed())
			{
				int nSizeSys = tp->GetDataSize();
				if (tp->IsUnloaded())
				{
					assert(nS == 0);
					nStreamedUnload += nSizeSys;
				}
				else
					nStreamedSys += nSizeSys;
				nStreamed += nS;
			}
			if (!nS)
				continue;
			if (tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
				nSRT += nS;
			else
			{
				if (!tp->IsStreamed())
				{
					int nnn = 0;
				}
				if (tp->GetName()[0] != '$' && tp->GetNumMips() <= 1)
					nSOneMip += nS;
				if (tp->GetFlags() & FT_TEX_NORMAL_MAP)
					nSNM += nS;
				else
					nSAll += nS;
			}

			nSAll += (tp->GetFlags() & FT_STAGE_UPLOAD)   ? nS : 0;
			nSAll += (tp->GetFlags() & FT_STAGE_READBACK) ? nS : 0;
		}
	}
	nSRT -= (nSizeD + nSizeD2 + nSizeSH);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Render targets: %.1f Mb", BYTES_TO_MB(nSRT));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSRT / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Textures: %.1f Mb", BYTES_TO_MB(nSAll));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSAll / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeMeshes = 0;
	{
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::s_MeshList.next; iter != &CRenderMesh::s_MeshList; iter = iter->next)
		{
			nSizeMeshes += iter->item<& CRenderMesh::m_Chain>()->Size(CRenderMesh::SIZE_VB | CRenderMesh::SIZE_IB);
		}
	}
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Meshes: %.1f Mb", BYTES_TO_MB(nSizeMeshes));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeMeshes / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeDynM = 0;
	for (i = 0; i < SHAPE_MAX; i++)
	{
		nSizeDynM += DebugVertBufferSize(m_pUnitFrustumVB[i]);
		nSizeDynM += DebugIndexBufferSize(m_pUnitFrustumIB[i]);
	}

	#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		m_pRenderAuxGeomD3D->GetDeviceDataSize();
	#endif

	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. mesh: %.1f Mb", BYTES_TO_MB(nSizeDynM));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeDynM / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst + 4;

	size_t nSize = nSizeDynM + nSRT + nSizeSH + nSizeD + nSizeD2;
	ColorF colO = Col_Blue;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colO.r, false, "Overall (Pure): %.1f Mb", BYTES_TO_MB(nSize));

	AuxDrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSize / fMaxTextureMemory * fMaxBar, nY + 12, Col_White, 1.0f);
	nY += nYst;
#endif
}

void CD3D9Renderer::DebugPerfBars(const SRenderStatistics& RStats,int nX, int nY)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	int nYst = 15;
	float fFSize = 1.4f;
	ColorF col = Col_Yellow;
	ColorF colP = Col_Cyan;

	CRenderDisplayContext* pDC = GetActiveDisplayContext();
	SRenderViewport vp = pDC->GetViewport();

	// Draw performance bars
	const SAuxGeomRenderFlags oldRenderFlags = IRenderAuxGeom::GetAux()->GetRenderFlags();

	SAuxGeomRenderFlags newRenderFlags = oldRenderFlags;
	newRenderFlags.SetMode2D3DFlag(e_Mode2D);
	newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
	newRenderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	newRenderFlags.SetCullMode(e_CullModeNone);
	newRenderFlags.SetAlphaBlendMode(e_AlphaNone);
	IRenderAuxGeom::GetAux()->SetRenderFlags(newRenderFlags);

	static float fTimeDIP[EFSLIST_NUM];
	static float fTimeDIPAO;
	static float fTimeDIPZ;
	static float fTimeDIPRAIN;
	static float fTimeDIPLayers;
	static float fTimeDIPSprites;
	static float fWaitForGPU;
	static float fFrameTime;
	static float fRTTimeProcess;
	static float fRTTimeEndFrame;
	static float fRTTimeFlashRender;
	static float fRTTimeSceneRender;
	static float fRTTimeMiscRender;

	float fMaxBar = 200;
	float fOffs = 180.0f;

	IRenderAuxText::Draw2dLabel(nX + 30, nY, 1.6f, &colP.r, false, "Instances: %d, GeomBatches: %d, MatBatches: %d, DrawCalls: %d, Text: %d, Stat: %d, PShad: %d, VShad: %d", RStats.m_NumRendInstances, RStats.m_NumRendGeomBatches, RStats.m_NumRendMaterialBatches, RStats.GetNumberOfDrawCalls(), RStats.m_NumTextChanges, RStats.m_NumStateChanges, RStats.m_NumPShadChanges, RStats.m_NumVShadChanges);
	nY += 30;

	ColorF colT = Col_Gray;
	IRenderAuxText::Draw2dLabel(nX + 50, nY, 1.6f, &colT.r, false, "Performance:");
	nY += 20;

	float fSmooth = 5.0f;
	fFrameTime = (iTimer->GetFrameTime() + fFrameTime * fSmooth) / (fSmooth + 1.0f);

	ColorF colF = Col_Orange;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Frame: %.3fms", fFrameTime * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fMaxBar, nY + 12, Col_Cyan, 1.0f);
	nY += nYst + 5;

	float fTimeDIPSum = fTimeDIPZ + fTimeDIP[EFSLIST_GENERAL] + fTimeDIP[EFSLIST_TERRAINLAYER] + fTimeDIP[EFSLIST_SHADOW_GEN] + fTimeDIP[EFSLIST_DECAL] + fTimeDIPAO + fTimeDIPRAIN + fTimeDIPLayers + fTimeDIP[EFSLIST_WATER_VOLUMES] + fTimeDIP[EFSLIST_TRANSP_AW] + fTimeDIP[EFSLIST_TRANSP_BW] + fTimeDIPSprites;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Sum all passes: %.3fms", fTimeDIPSum * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fTimeDIPSum / fFrameTime * fMaxBar, nY + 12, Col_Yellow, 1.0f);
	nY += nYst + 5;

	fRTTimeProcess = (m_fTimeProcessedRT[gRenDev->GetMainThreadID()] + fRTTimeProcess * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT process time: %.3fms", fRTTimeProcess * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeProcess / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;
	nX += 5;

	fRTTimeEndFrame = (m_fRTTimeEndFrame + fRTTimeEndFrame * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT end frame: %.3fms", fRTTimeEndFrame * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeEndFrame / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeFlashRender = (m_fRTTimeFlashRender + fRTTimeFlashRender * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT flash render: %.3fms", fRTTimeFlashRender * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeFlashRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeMiscRender = (m_fRTTimeMiscRender + fRTTimeMiscRender * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT misc render: %.3fms", fRTTimeMiscRender * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeMiscRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeSceneRender = (m_fRTTimeSceneRender + fRTTimeSceneRender * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT scene render: %.3fms", fRTTimeSceneRender * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeSceneRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	float fRTTimeOverall = fRTTimeSceneRender + fRTTimeEndFrame + fRTTimeFlashRender + fRTTimeMiscRender;
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT CPU overall: %.3fms", fRTTimeOverall * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeOverall / fFrameTime * fMaxBar, nY + 12, Col_LightGray, 1.0f);
	nY += nYst + 5;
	nX -= 5;

	fWaitForGPU = (m_fTimeWaitForGPU[gRenDev->GetMainThreadID()] + fWaitForGPU * fSmooth) / (fSmooth + 1.0f);
	IRenderAuxText::Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Wait for GPU: %.3fms", fWaitForGPU * 1000.0f);

	AuxDrawQuad(nX + fOffs, nY + 4, nX + fOffs + fWaitForGPU / fFrameTime * fMaxBar, nY + 12, Col_Blue, 1.0f);
	nY += nYst;

	IRenderAuxGeom::GetAux()->SetRenderFlags(oldRenderFlags);

#endif
}

inline bool CompareTexturesSize(CTexture* a, CTexture* b)
{
	return (a->GetDeviceDataSize() > b->GetDeviceDataSize());
}

void CD3D9Renderer::VidMemLog()
{
#if !defined(_RELEASE) && !defined(CONSOLE_CONST_CVAR_MODE)
	if (!CV_r_logVidMem)
		return;

	CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

	SResourceContainer* pRL = 0;

	pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());

	DynArray<CTexture*> pRenderTargets;
	DynArray<CTexture*> pDynTextures;
	DynArray<CTexture*> pTextures;

	size_t nSizeTotalRT = 0;
	size_t nSizeTotalDynTex = 0;
	size_t nSizeTotalTex = 0;

	for (uint32 i = 0; i < pRL->m_RList.size(); ++i)
	{
		CTexture* pTexResource = (CTexture*) pRL->m_RList[i];
		if (!pTexResource)
			continue;

		if (!pTexResource->GetDeviceDataSize())
			continue;

		if (pTexResource->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS))
		{
			pRenderTargets.push_back(pTexResource);
			nSizeTotalRT += pTexResource->GetDeviceDataSize();
		}
		else
		{
			pTextures.push_back(pTexResource);
			nSizeTotalTex += pTexResource->GetDeviceDataSize();
		}

		if (pTexResource->GetFlags() & FT_STAGE_READBACK)
		{
			pDynTextures.push_back(pTexResource);
			nSizeTotalDynTex += pTexResource->GetDeviceDataSize();
		}

		if (pTexResource->GetFlags() & FT_STAGE_UPLOAD)
		{
			pDynTextures.push_back(pTexResource);
			nSizeTotalDynTex += pTexResource->GetDeviceDataSize();
		}
	}

	std::sort(pRenderTargets.begin(), pRenderTargets.end(), CompareTexturesSize);
	std::sort(pDynTextures.begin(), pDynTextures.end(), CompareTexturesSize);
	std::sort(pTextures.begin(), pTextures.end(), CompareTexturesSize);

	FILE* fp = fxopen("VidMemLogTest.txt", "w");
	if (fp)
	{
		char time[128];
		char date[128];

		_strtime(time);
		_strdate(date);

		fprintf(fp, "\n==========================================\n");
		fprintf(fp, "VidMem Log file opened: %s (%s)\n", date, time);
		fprintf(fp, "==========================================\n");

		fprintf(fp, "\nTotal Vid mem used for textures: %.1f MB", BYTES_TO_MB(nSizeTotalRT + nSizeTotalDynTex + nSizeTotalTex));
		fprintf(fp, "\nRender targets (%d): %.1f kb, CPU accessible textures (%d): %.1f kb, Textures (%d): %.1f kb", pRenderTargets.size(), BYTES_TO_KB(nSizeTotalRT), pDynTextures.size(), BYTES_TO_KB(nSizeTotalDynTex), pTextures.size(), BYTES_TO_KB(nSizeTotalTex));

		fprintf(fp, "\n\n*** Start render targets list *** \n");
		int i = 0;
		for (i = 0; i < pRenderTargets.size(); ++i)
		{
			fprintf(fp, "\nName: %s, Format: %s, Width: %d, Height: %d, Size: %.1f kb", pRenderTargets[i]->GetName(), pRenderTargets[i]->GetFormatName(), pRenderTargets[i]->GetWidth(),
			        pRenderTargets[i]->GetHeight(), BYTES_TO_KB(pRenderTargets[i]->GetDeviceDataSize()));
		}

		fprintf(fp, "\n\n*** Start dynamic textures list *** \n");
		for (i = 0; i < pDynTextures.size(); ++i)
		{
			fprintf(fp, "\nName: %s, Format: %s, Width: %d, Height: %d, Size: %.1f kb", pDynTextures[i]->GetName(), pDynTextures[i]->GetFormatName(), pDynTextures[i]->GetWidth(),
			        pDynTextures[i]->GetHeight(), BYTES_TO_KB(pDynTextures[i]->GetDeviceDataSize()));
		}

		fprintf(fp, "\n\n*** Start textures list *** \n");
		for (i = 0; i < pTextures.size(); ++i)
		{
			fprintf(fp, "\nName: %s, Format: %s, Width: %d, Height: %d, Size: %.1f kb", pTextures[i]->GetName(), pTextures[i]->GetFormatName(), pTextures[i]->GetWidth(),
			        pTextures[i]->GetHeight(), BYTES_TO_KB(pTextures[i]->GetDeviceDataSize()));
		}

		fprintf(fp, "\n\n==========================================\n");
		fprintf(fp, "VidMem Log file closed\n");
		fprintf(fp, "==========================================\n\n");

		fclose(fp);
		fp = NULL;
	}

	CV_r_logVidMem = 0;
#endif
}

void CD3D9Renderer::DebugPrintShader(CHWShader_D3D* pSH, void* pI, int nX, int nY, ColorF colSH)
{
#if CRY_PLATFORM_DURANGO
	// currently not supported yet (D3DDisassemble missing)
	return;
#else
	if (!pSH)
		return;
	CHWShader_D3D::SHWSInstance* pInst = (CHWShader_D3D::SHWSInstance*)pI;
	if (!pInst)
		return;

	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	char str[512];
	pSH->m_pCurInst = pInst;
	cry_strcpy(str, pSH->m_EntryFunc.c_str());
	uint32 nSize = strlen(str);
	pSH->mfGenName(pInst, &str[nSize], 512 - nSize, 1);

	ColorF col = Col_Green;
	IRenderAuxText::Draw2dLabel(nX, nY, 1.6f, &col.r, false, "Shader: %s (%d instructions)", str, pInst->m_nInstructions);
	nX += 10;
	nY += 25;

	SD3DShader* pHWS = pInst->m_Handle.m_pShader;
	if (!pHWS || !pHWS->GetHandle())
		return;
	if (!pInst->m_Shader.m_pShaderData)
		return;
	byte* pData = NULL;
	D3DBlob* pAsm = NULL;
	D3DDisassemble((UINT*)pInst->m_Shader.m_pShaderData, pInst->m_Shader.m_nDataSize, 0, NULL, &pAsm);
	if (!pAsm)
		return;
	char* szAsm = (char*)pAsm->GetBufferPointer();
	int nMaxY = pDC->GetViewport().height;
	int nM = 0;
	while (szAsm[0])
	{
		fxFillCR(&szAsm, str);
		IRenderAuxText::Draw2dLabel(nX, nY, 1.2f, &colSH.r, false, "%s", str);
		nY += 11;
		if (nY + 12 > nMaxY)
		{
			nX += 280;
			nY = 120;
			nM++;
		}
		if (nM == 2)
			break;
	}

	SAFE_RELEASE(pAsm);
	SAFE_DELETE_ARRAY(pData);
#endif
}

void CD3D9Renderer::DebugDrawStats8(const SRenderStatistics& RStats)
{
#if !defined(_RELEASE) && defined(ENABLE_PROFILING_CODE)
	ColorF col = Col_White;
	IRenderAuxText::Draw2dLabel(30, 50, 1.2f, &col.r, false, "%d total instanced DIPs in %d batches", RStats.GetNumGeomInstances(), RStats.GetNumGeomInstanceDrawCalls());
#endif
}

void CD3D9Renderer::DebugDrawStats2(const SRenderStatistics& RStats)
{
	// NOT IMPLEMENTED
	assert(0 && "CD3D9Renderer::DebugDrawStats2");
}

void CD3D9Renderer::DebugDrawStats20(const SRenderStatistics& RStats)
{
#if !defined(_RELEASE) && defined(ENABLE_PROFILING_CODE)
	ColorF col = Col_Yellow;
	IRenderAuxText::Draw2dLabel(30, 50, 1.5f, &col.r, false, "Compiled Render Objects");
	IRenderAuxText::Draw2dLabel(30, 80, 1.5f, &col.r, false, "Objects: Modified: %-5d  Temporary: %-5d  Incomplete: %-5d",
	                            RStats.m_nModifiedCompiledObjects,
	                            RStats.m_nTempCompiledObjects,
	                            RStats.m_nIncompleteCompiledObjects);
	IRenderAuxText::Draw2dLabel(30, 110, 1.5f, &col.r, false, "State Changes: PSO [%d] PT [%d] L [%d] I [%d] RS [%d]",
	                            RStats.m_nNumPSOSwitches,
	                            RStats.m_nNumTopologySets,
	                            RStats.m_nNumLayoutSwitches,
	                            RStats.m_nNumInlineSets,
	                            RStats.m_nNumResourceSetSwitches);
	IRenderAuxText::Draw2dLabel(30, 140, 1.5f, &col.r, false, "Resource bindings Mem[GPU,CPU]: VB [%d,%d] IB [%d,%d] CB [%d,%d] / [%d,%d] UB [%d,%d] TEX [%d,%d]",
	                            RStats.m_nNumBoundVertexBuffers[0],
	                            RStats.m_nNumBoundVertexBuffers[1],
	                            RStats.m_nNumBoundIndexBuffers[0],
	                            RStats.m_nNumBoundIndexBuffers[1],
	                            RStats.m_nNumBoundConstBuffers[0],
	                            RStats.m_nNumBoundConstBuffers[1],
	                            RStats.m_nNumBoundInlineBuffers[0],
	                            RStats.m_nNumBoundInlineBuffers[1],
	                            RStats.m_nNumBoundUniformBuffers[0],
	                            RStats.m_nNumBoundUniformBuffers[1],
	                            RStats.m_nNumBoundUniformTextures[0],
	                            RStats.m_nNumBoundUniformTextures[1]);
#endif
}

void CD3D9Renderer::DebugDrawStats(const SRenderStatistics& RStats)
{
#ifndef _RELEASE
	if (CV_r_stats)
	{
		CRenderer* crend = gRenDev;

		CCryNameTSCRC Name;
		switch (CV_r_stats)
		{
		case 1:
			DebugDrawStats1(RStats);
			break;
		case 2:
			DebugDrawStats2(RStats);
			break;
		case 3:
			DebugPerfBars(RStats,40, 50);
			DebugVidResourcesBars(450, 80);
			break;
		case 4:
			DebugPerfBars(RStats,40, 50);
			break;
		case 8:
			DebugDrawStats8(RStats);
			break;
		case 13:
			EF_PrintRTStats("Cleared Render Targets:");
			break;
		case 20:
			DebugDrawStats20(RStats);
			break;
		case 5:
			{
				const int nYstep = 30;
				int nYpos = 270; // initial Y pos
				IRenderAuxText::WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery stats:%d",
				                        CREOcclusionQuery::m_nQueriesPerFrameCounter);
				IRenderAuxText::WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nQueriesPerFrameCounter=%d",
				                        CREOcclusionQuery::m_nQueriesPerFrameCounter);
				IRenderAuxText::WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nReadResultNowCounter=%d",
				                        CREOcclusionQuery::m_nReadResultNowCounter);
				IRenderAuxText::WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nReadResultTryCounter=%d",
				                        CREOcclusionQuery::m_nReadResultTryCounter);
			}
			break;

		case 6:
			{
				ColorF clrDPBlue = ColorF(0, 1, 1, 1);
				ColorF clrDPRed = ColorF(1, 0, 0, 1);
				ColorF clrDPInterp = ColorF(1, 0, 0, 1);
				ColorF clrInfo = ColorF(1, 1, 0, 1);

				auto pEnd = m_pGraphicsPipeline->GetDrawCallInfoPerNode()->end();
				auto pItor = m_pGraphicsPipeline->GetDrawCallInfoPerNode()->begin();

				for (; pItor != pEnd; ++pItor)
				{
					IRenderNode* pRenderNode = pItor->first;
					SDrawCallCountInfo& pInfo = pItor->second;

					uint32 nDrawcalls = pInfo.nShadows + pInfo.nZpass + pInfo.nGeneral + pInfo.nTransparent + pInfo.nMisc;

					// Display drawcall count per render object
					const float* pfColor;
					int nMinDrawCalls = CV_r_statsMinDrawcalls;
					if (nDrawcalls < nMinDrawCalls)
						continue;
					else if (nDrawcalls <= 4)
						pfColor = &clrDPBlue.r;
					else if (nDrawcalls > 20)
						pfColor = &clrDPRed.r;
					else
					{
						// interpolation from orange to red
						clrDPInterp.g = 0.5f - 0.5f * (nDrawcalls - 4) / (20 - 4);
						pfColor = &clrDPInterp.r;
					}

					IRenderAuxText::DrawLabelExF(pInfo.pPos, 1.3f, pfColor, true, true, "DP: %d (%d/%d/%d/%d/%d)",
					                             nDrawcalls, pInfo.nZpass, pInfo.nGeneral, pInfo.nTransparent, pInfo.nShadows, pInfo.nMisc);
				}

				IRenderAuxText::Draw2dLabel(40.f, 40.f, 1.3f, &clrInfo.r, false, "Instance drawcall count (zpass/general/transparent/shadows/misc)");
			}
			break;
		}
	}

	if (m_pDebugRenderNode)
	{
	#ifndef _RELEASE
		static ICVar* debugDraw = gEnv->pConsole->GetCVar("e_DebugDraw");
		if (debugDraw && debugDraw->GetIVal() == 16)
		{
			float yellow[4] = { 1.f, 1.f, 0.f, 1.f };

			auto pEnd  = m_pGraphicsPipeline->GetDrawCallInfoPerNode()->end();
			auto pItor = m_pGraphicsPipeline->GetDrawCallInfoPerNode()->begin();
			for (; pItor != pEnd; ++pItor)
			{
				//display info for render node under debug gun
				if (pItor->first == m_pDebugRenderNode)
				{
					SDrawCallCountInfo& pInfo = pItor->second;

					uint32 nDrawcalls = pInfo.nShadows + pInfo.nZpass + pInfo.nGeneral + pInfo.nTransparent + pInfo.nMisc;

					IRenderAuxText::Draw2dLabel(970.f, 65.f, 1.5f, yellow, false, "Draw calls: %d \n  zpass: %d\n  general: %d\n  transparent: %d\n  shadows: %d\n  misc: %d\n",
					                            nDrawcalls, pInfo.nZpass, pInfo.nGeneral, pInfo.nTransparent, pInfo.nShadows, pInfo.nMisc);
				}
			}
		}
	#endif
	}
#endif
}

void CD3D9Renderer::RenderDebug(bool bRenderStats)
{
	ExecuteRenderThreadCommand( [=]{
		this->RT_RenderDebug(bRenderStats); },
		ERenderCommandFlags::None );
}

void CD3D9Renderer::RT_RenderDebug(bool bRenderStats)
{
	if (gEnv->IsEditor() && !IsCurrentContextMainVP())
		return;

	if (m_bDeviceLost)
		return;
#if !defined(_RELEASE)
	if (CV_r_showbufferusage)
	{
		const uint32 xStartCoord = 695;
		int YStep = 12;
	}

	const SRenderStatistics& RStats = SRenderStatistics::Write();

	CSwapChainBackedRenderDisplayContext* pDC = GetBaseDisplayContext();
	if (GetS3DRend().IsStereoEnabled())
	{
		SDisplayContextKey displayContextKey;
		displayContextKey.key.emplace<HWND>(pDC->GetWindowHandle());
		gEnv->pRenderer->GetIRenderAuxGeom(/*eType*/)->SetCurrentDisplayContext(displayContextKey);
	}

	if (CV_r_RefractionPartialResolvesDebug)
	{
		const float xPos = 0.0f;
		float yPos = 90.0f;
		const float textYSpacing = 18.0f;
		const float size = 1.5f;
		const ColorF titleColor = Col_SpringGreen;
		const ColorF textColor = Col_Yellow;
		const bool bCentre = false;

		float fInvScreenArea = 1.0f / ((float)CRendererResources::s_renderArea);

		IRenderAuxText::Draw2dLabel(xPos, yPos, size, &titleColor.r, bCentre, "Partial Resolves");
		yPos += textYSpacing;
		IRenderAuxText::Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Count: %d", RStats.m_refractionPartialResolveCount);
		yPos += textYSpacing;
		IRenderAuxText::Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Pixels: %d", RStats.m_refractionPartialResolvePixelCount);
		yPos += textYSpacing;
		IRenderAuxText::Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Percentage of Screen area: %d", (int) (RStats.m_refractionPartialResolvePixelCount * fInvScreenArea * 100.0f));
	}

	#ifndef EXCLUDE_DOCUMENTATION_PURPOSE
	if (CV_r_DebugFontRendering)
	{
		const float fPixelPerfectScale = 16.0f / UIDRAW_TEXTSIZEFACTOR * 2.0f;    // we have to compensate  vSize.x/16.0f; and pFont->SetCharWidthScale(0.5f);
		const int line = 10;

		float y = 0;
		ColorF color(1.0f);
		int flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;

		color[2] = 0.0f;
		IRenderAuxText::DrawText(Vec3(0, y += line, 0), 1, color, eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace, "Colors (black,white,blue,..): { $00$11$22$33$44$55$66$77$88$99$$$o } ()_!+*/# ?");
		color[2] = 1.0f;
		IRenderAuxText::DrawText(Vec3(0, y += line, 0), 1, color, eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace, "Colors (black,white,blue,..): { $00$11$22$33$44$55$66$77$88$99$$$o } ()_!+*/# ?");

		for (int iPass = 0; iPass < 3; ++iPass)      // left, center, right
		{
			float scale = fPixelPerfectScale * 0.5f;
			float x = 0;

			y = 3 * line;

			int passflags = eDrawText_2D;

			if (iPass == 1)
			{
				passflags |= eDrawText_Center;
				x = (float)pDC->GetViewport().width * 0.5f;
			}
			else if (iPass == 2)
			{
				x = (float)pDC->GetViewport().width;
				passflags |= eDrawText_Right;
			}

			color[3] = 0.5f;
			int flags = passflags | eDrawText_FixedSize | eDrawText_Monospace;
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "0");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !MMMMM!");

			color[0] = 0.0f;
			color[3] = 1.0f;
			flags = passflags | eDrawText_FixedSize;
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "1");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !MMMMM!");
			/*
			      flags = passflags | eDrawText_Monospace;
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"2");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !.....!");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !MMMMM!");

			      flags = passflags;
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"3");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !.....!");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !MMMMM!");
			 */
			color[1] = 0.0f;
			flags = passflags | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace;
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "4");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !MMMMM!");

			color[0] = 1.0f;
			color[1] = 1.0f;
			flags = passflags | eDrawText_800x600 | eDrawText_FixedSize;
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "5");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line, 0), scale, color, flags, "AbcW !MMMMM!");

			/*
			      flags = passflags | eDrawText_800x600 | eDrawText_Monospace;
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"6");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"Halfsize");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !MMMMM!");

			      flags = passflags | eDrawText_800x600;
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"7");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !.....!");
			      IRenderAuxText::DrawText(Vec3(x,y+=line,0),scale, color, flags,"AbcW !MMMMM!");
			 */
			// Pixel Perfect (1:1 mapping of the texels to the pixels on the screeen)

			flags = passflags | eDrawText_FixedSize | eDrawText_Monospace;

			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "8");
			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "AbcW !MMMMM!");

			flags = passflags | eDrawText_FixedSize;

			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "9");
			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "AbcW !.....!");
			IRenderAuxText::DrawText(Vec3(x, y += line * 2, 0), fPixelPerfectScale, color, flags, "AbcW !MMMMM!");
		}

	}
	#endif // EXCLUDE_DOCUMENTATION_PURPOSE

	#ifdef DO_RENDERSTATS
	static std::vector<SStreamEngineStatistics::SAsset> vecStreamingProblematicAssets;

	if (CV_r_showtimegraph)
	{
		static byte* fg;
		static float fPrevTime = iTimer->GetCurrTime();
		static int sPrevWidth = 0;
		static int sPrevHeight = 0;
		static int nC;

		float fCurTime = iTimer->GetCurrTime();
		float frametime = fCurTime - fPrevTime;
		fPrevTime = fCurTime;
		int wdt = pDC->GetViewport().width;
		int hgt = pDC->GetViewport().height;

		if (sPrevHeight != hgt || sPrevWidth != wdt)
		{
			if (fg)
			{
				delete[] fg;
				fg = NULL;
			}
			sPrevWidth = wdt;
			sPrevHeight = hgt;
		}

		if (!fg)
		{
			fg = new byte[wdt];
			memset(fg, -1, wdt);
			nC = 0;
		}

		int type = CV_r_showtimegraph;
		float f;
		float fScale;
		if (type > 1)
		{
			type = 1;
			fScale = (float)CV_r_showtimegraph / 1000.0f;
		}
		else
			fScale = 0.1f;
		f = frametime / fScale;
		f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
		if (fg)
		{
			fg[nC] = (byte)f;
			ColorF c = Col_Green;
			Graph(fg, 0, hgt - 280, wdt, 256, nC, type, "Frame Time", c, fScale);
		}
		nC++;
		if (nC >= wdt)
			nC = 0;
	}
	else if (CV_profileStreaming)
	{
		static byte* fgUpl;
		static byte* fgStreamSync;
		static byte* fgTimeUpl;
		static byte* fgDistFact;
		static byte* fgTotalMem;
		static byte* fgCurMem;
		static byte* fgStreamSystem;

		static float fScaleUpl = 10;        // in Mb
		static float fScaleStreamSync = 10; // in Mb
		static float fScaleTimeUpl = 75;    // in Ms
		static float fScaleDistFact = 4;    // Ratio
		static FLOAT fScaleTotalMem = 0;    // in Mb
		static float fScaleCurMem = 80;     // in Mb
		static float fScaleStreaming = 4;   // in Mb

		static ColorF ColUpl = Col_White;
		static ColorF ColStreamSync = Col_Cyan;
		static ColorF ColTimeUpl = Col_SeaGreen;
		static ColorF ColDistFact = Col_Orchid;
		static ColorF ColTotalMem = Col_Red;
		static ColorF ColCurMem = Col_Yellow;
		static ColorF ColCurStream = Col_BlueViolet;

		static int sMask = -1;

		fScaleTotalMem = (float)CRenderer::GetTexturesStreamPoolSize() - 1;

		static float fPrevTime = iTimer->GetCurrTime();
		static int sPrevWidth = 0;
		static int sPrevHeight = 0;
		static int nC;

		int wdt = pDC->GetViewport().width;
		int hgt = pDC->GetViewport().height;
		int type = 2;

		if (sPrevHeight != hgt || sPrevWidth != wdt)
		{
			SAFE_DELETE_ARRAY(fgUpl);
			SAFE_DELETE_ARRAY(fgStreamSync);
			SAFE_DELETE_ARRAY(fgTimeUpl);
			SAFE_DELETE_ARRAY(fgDistFact);
			SAFE_DELETE_ARRAY(fgTotalMem);
			SAFE_DELETE_ARRAY(fgCurMem);
			SAFE_DELETE_ARRAY(fgStreamSystem);
			sPrevWidth = wdt;
			sPrevHeight = hgt;
		}

		if (!fgUpl)
		{
			fgUpl = new byte[wdt];
			memset(fgUpl, -1, wdt);
			fgStreamSync = new byte[wdt];
			memset(fgStreamSync, -1, wdt);
			fgTimeUpl = new byte[wdt];
			memset(fgTimeUpl, -1, wdt);
			fgDistFact = new byte[wdt];
			memset(fgDistFact, -1, wdt);
			fgTotalMem = new byte[wdt];
			memset(fgTotalMem, -1, wdt);
			fgCurMem = new byte[wdt];
			memset(fgCurMem, -1, wdt);
			fgStreamSystem = new byte[wdt];
			memset(fgStreamSystem, -1, wdt);
		}

		const SAuxGeomRenderFlags oldRenderFlags = IRenderAuxGeom::GetAux()->GetRenderFlags();

		SAuxGeomRenderFlags newRenderFlags = oldRenderFlags;
		newRenderFlags.SetMode2D3DFlag(e_Mode2D);
		newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
		newRenderFlags.SetDepthWriteFlag(e_DepthWriteOff);
		newRenderFlags.SetCullMode(e_CullModeNone);
		newRenderFlags.SetAlphaBlendMode(e_AlphaNone);
		IRenderAuxGeom::GetAux()->SetRenderFlags(newRenderFlags);

		ColorF col = Col_White;
		int num = CRendererResources::s_ptexWhite->GetID();
		IRenderAuxImage::DrawImage((float)nC, (float)(hgt - 280), 1, 256, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
		IRenderAuxGeom::GetAux()->SetRenderFlags(oldRenderFlags);

		// disable some statistics
		sMask &= ~(1 | 2 | 4 | 8 | 64);

		float f;
		if (sMask & 1)
		{
			f = (BYTES_TO_MB(CTexture::s_nTexturesDataBytesUploaded)) / fScaleUpl;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgUpl[nC] = (byte)f;
			Graph(fgUpl, 0, hgt - 280, wdt, 256, nC, type, NULL, ColUpl, fScaleUpl);
			col = ColUpl;
			IRenderAuxText::WriteXY(4, hgt - 280, 1, 1, col.r, col.g, col.b, 1, "UploadMB (%d-%d)", (int)(BYTES_TO_MB(CTexture::s_nTexturesDataBytesUploaded)), (int)fScaleUpl);
		}

		if (sMask & 2)
		{
			f = RStats.m_fTexUploadTime / fScaleTimeUpl;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgTimeUpl[nC] = (byte)f;
			Graph(fgTimeUpl, 0, hgt - 280, wdt, 256, nC, type, NULL, ColTimeUpl, fScaleTimeUpl);
			col = ColTimeUpl;
			IRenderAuxText::WriteXY(4, hgt - 280 + 16, 1, 1, col.r, col.g, col.b, 1, "Upload Time (%.3fMs - %.3fMs)", RStats.m_fTexUploadTime, fScaleTimeUpl);
		}

		if (sMask & 4)
		{
			f = BYTES_TO_MB(CTexture::s_nTexturesDataBytesLoaded) / fScaleStreamSync;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgStreamSync[nC] = (byte)f;
			Graph(fgStreamSync, 0, hgt - 280, wdt, 256, nC, type, NULL, ColStreamSync, fScaleStreamSync);
			col = ColStreamSync;
			IRenderAuxText::WriteXY(4, hgt - 280 + 16 * 2, 1, 1, col.r, col.g, col.b, 1, "StreamMB (%d-%d)", (int)BYTES_TO_MB(CTexture::s_nTexturesDataBytesLoaded), (int)fScaleStreamSync);
		}

		if (sMask & 32)
		{
			size_t pool = CTexture::s_nStatsStreamPoolInUseMem;
			f = BYTES_TO_MB(pool) / fScaleTotalMem;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgTotalMem[nC] = (byte)f;
			Graph(fgTotalMem, 0, hgt - 280, wdt, 256, nC, type, NULL, ColTotalMem, fScaleTotalMem);
			col = ColTotalMem;
			IRenderAuxText::WriteXY(4, hgt - 280 + 16 * 5, 1, 1, col.r, col.g, col.b, 1, "Streaming textures pool used (Mb) (%d of %d)", (int)BYTES_TO_MB(pool), (int)fScaleTotalMem);
		}

		if (sMask & 64)
		{
			f = (BYTES_TO_MB(RStats.m_ManagedTexturesSysMemSize + RStats.m_ManagedTexturesStreamSysSize + RStats.m_DynTexturesSize)) / fScaleCurMem;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgCurMem[nC] = (byte)f;
			Graph(fgCurMem, 0, hgt - 280, wdt, 256, nC, type, NULL, ColCurMem, fScaleCurMem);
			col = ColCurMem;
			IRenderAuxText::WriteXY(4, hgt - 280 + 16 * 6, 1, 1, col.r, col.g, col.b, 1, "Cur Scene Size: Dyn. + Stat. (Mb) (%d-%d)", (int)(BYTES_TO_MB(RStats.m_ManagedTexturesSysMemSize + RStats.m_ManagedTexturesStreamSysSize + RStats.m_DynTexturesSize)), (int)fScaleCurMem);
		}

		if (sMask & 128)    // streaming stat
		{
			int nLineStep = 12;
			static float thp = 0.f;

			SStreamEngineStatistics& stats = gEnv->pSystem->GetStreamEngine()->GetStreamingStatistics();

			float dt = 1.0f;//max(stats.m_fDeltaTime / 1000, .00001f);

			float newThp = (float)stats.nTotalCurrentReadBandwidth / 1024 / dt;
			thp = (thp + min(1.f, dt / 5) * (newThp - thp));  // lerp

			f = thp / (fScaleStreaming * 1024);
			if (f > 1.f && !stats.vecHeavyAssets.empty())
			{
				for (int i = stats.vecHeavyAssets.size() - 1; i >= 0; --i)
				{
					SStreamEngineStatistics::SAsset asset = stats.vecHeavyAssets[i];

					bool bPart = false;
					// remove texture mips extensions
					if (asset.m_sName.size() > 2 && asset.m_sName[asset.m_sName.size() - 2] == '.' &&
					    asset.m_sName[asset.m_sName.size() - 1] >= '0' && asset.m_sName[asset.m_sName.size() - 1] <= '9')
					{
						asset.m_sName = asset.m_sName.substr(0, asset.m_sName.size() - 2);
						bPart = true;
					}

					// check for unique name
					uint32 j = 0;
					for (; j < vecStreamingProblematicAssets.size(); ++j)
						if (vecStreamingProblematicAssets[j].m_sName.compareNoCase(asset.m_sName) == 0)
							break;
					if (j == vecStreamingProblematicAssets.size())
						vecStreamingProblematicAssets.insert(vecStreamingProblematicAssets.begin(), asset);
					else if (bPart)
						vecStreamingProblematicAssets[j].m_nSize = max(vecStreamingProblematicAssets[j].m_nSize, asset.m_nSize);  // else adding size to existing asset
				}
				if (vecStreamingProblematicAssets.size() > 20)
					vecStreamingProblematicAssets.resize(20);
				std::sort(vecStreamingProblematicAssets.begin(), vecStreamingProblematicAssets.end());  // sort by descending
			}
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgStreamSystem[nC] = (byte)f;
			Graph(fgStreamSystem, 0, hgt - 280, wdt, 256, nC, type, NULL, ColCurStream, fScaleStreaming);
			col = ColCurStream;
			IRenderAuxText::WriteXY(4, hgt - 280 + 14 * 7, 1, 1, col.r, col.g, col.b, 1, "Streaming throughput (Kb/s) (%d of %d)", (int)thp, (int)fScaleStreaming * 1024);

			// output assets
			if (!vecStreamingProblematicAssets.empty())
			{
				int top = vecStreamingProblematicAssets.size() * nLineStep + 320;
				IRenderAuxText::WriteXY(4, hgt - top - nLineStep, 1, 1, col.r, col.g, col.b, 1, "Problematic assets:");
				for (int i = vecStreamingProblematicAssets.size() - 1; i >= 0; --i)
					IRenderAuxText::WriteXY(4, hgt - top + nLineStep * i, 1, 1, col.r, col.g, col.b, 1, "[%.1fKB] '%s'", BYTES_TO_KB(vecStreamingProblematicAssets[i].m_nSize), vecStreamingProblematicAssets[i].m_sName.c_str());
			}
		}
		nC++;
		if (nC == wdt)
			nC = 0;
	}
	else
		vecStreamingProblematicAssets.clear();

	DrawTexelsPerMeterInfo();

	if (m_pColorGradingControllerD3D)
		m_pColorGradingControllerD3D->DrawDebugInfo();

	double time = 0;
	ticks(time);

	if (bRenderStats)
	{
		DebugDrawStats(RStats);
	}

	VidMemLog();

	#endif

	if (m_pGraphicsPipeline)
	{
		m_pGraphicsPipeline->GetDebugRenderTargetsStage()->Execute();
	}

	static char r_showTexture_prevString[256] = "";  // a wrokaround to reset the "??" command
	// show custom texture
	if (CV_r_ShowTexture && CV_r_ShowTexture->GetString()[0] != 0)
	{
		const char* arg = CV_r_ShowTexture->GetString();

		//SetState(GS_NODEPTHTEST);
		// SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
		const SAuxGeomRenderFlags oldRenderFlags = IRenderAuxGeom::GetAux()->GetRenderFlags();

		SAuxGeomRenderFlags newRenderFlags = oldRenderFlags;
		newRenderFlags.SetMode2D3DFlag(e_Mode2D);
		newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
		newRenderFlags.SetDepthWriteFlag(e_DepthWriteOff);
		newRenderFlags.SetCullMode(e_CullModeNone);
		newRenderFlags.SetAlphaBlendMode(e_AlphaNone);
		IRenderAuxGeom::GetAux()->SetRenderFlags(newRenderFlags);

		char* endp;
		int texId = strtol(arg, &endp, 10);

		if (endp != arg)
		{
			CTexture* pTexToShow = CTexture::GetByID(texId);

			if (pTexToShow)
			{
				//RT_SetViewport(m_width - m_width / 3 - 10, m_height - m_height / 3 - 10, m_width / 3, m_height / 3);
				IRenderAuxImage::DrawImage(0, 0, 1, 1, pTexToShow->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
				IRenderAuxText::WriteXY(10, 10, 1, 1, 1, 1, 1, 1, "Name: %s", pTexToShow->GetSourceName());
				IRenderAuxText::WriteXY(10, 25, 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", pTexToShow->GetFormatName(), CTexture::NameForTextureType(pTexToShow->GetTextureType()));
				IRenderAuxText::WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight(), pTexToShow->GetDepth());
				IRenderAuxText::WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight());
				IRenderAuxText::WriteXY(10, 55, 1, 1, 1, 1, 1, 1, "Mips: %d", pTexToShow->GetNumMips());
			}
		}
		else if (strlen(arg) == 2)
		{
			// Special cmd
			if (strcmp(arg, "??") == 0)
			{
				// print all entries in the index book
				iLog->Log("All entries:\n");
				CTexture::LogTextures(iLog);

				// reset after done  (revert to previous argument set)
				CV_r_ShowTexture->ForceSet(r_showTexture_prevString);
			}
		}
		else if (strlen(arg) > 2)
		{
			cry_strcpy(r_showTexture_prevString, arg);    // save argument set (for "??" processing)

			CTexture* pTexToShow = CTexture::GetByName(arg);

			if (pTexToShow)
			{
				//RT_SetViewport(m_width - m_width / 3 - 10, m_height - m_height / 3 - 10, m_width / 3, m_height / 3);
				IRenderAuxImage::DrawImage(0, 0, 1, 1, pTexToShow->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
				IRenderAuxText::WriteXY(10, 10, 1, 1, 1, 1, 1, 1, "Name: %s", pTexToShow->GetSourceName());
				IRenderAuxText::WriteXY(10, 25, 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", pTexToShow->GetFormatName(), CTexture::NameForTextureType(pTexToShow->GetTextureType()));
				IRenderAuxText::WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight(), pTexToShow->GetDepth());
				IRenderAuxText::WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight());
				IRenderAuxText::WriteXY(10, 55, 1, 1, 1, 1, 1, 1, "Mips: %d", pTexToShow->GetNumMips());
			}
			else
			{
				// parse the arguments (use space as delimiter)
				string nameListStr = arg;
				char* nameListCh = (char*)nameListStr.c_str();

				std::vector<string> nameList;
				char* p = strtok(nameListCh, " ");
				while (p)
				{
					string s(p);
					s.Trim();
					if (s.length() > 0)
					{
						const char* curName = s.c_str();
						nameList.push_back(s);
					}
					p = strtok(NULL, " ");
				}

				// render them in a tiled fashion
				//RT_SetViewport(0, 0, m_width, m_height);
				const float tileW = 0.24f;
				const float tileH = 0.24f;
				const float tileGapW = 5.f / pDC->GetViewport().width;
				const float tileGapH = 25.f / pDC->GetViewport().height;

				const int maxTilesInRow = (int) ((1 - tileGapW) / (tileW + tileGapW));
				for (size_t i = 0; i < nameList.size(); i++)
				{
					CTexture* tex = CTexture::GetByName(nameList[i].c_str());
					if (!tex)  continue;

					int row = i / maxTilesInRow;
					int col = i - row * maxTilesInRow;
					float curX = tileGapW + col * (tileW + tileGapW);
					float curY = tileGapH + row * (tileH + tileGapH);
					//gcpRendD3D->FX_SetState(GS_NODEPTHTEST);  // fix the state change by using WriteXY
					IRenderAuxImage::DrawImage(curX, curY, tileW, tileH, tex->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
					IRenderAuxText::WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 - 15), 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", tex->GetFormatName(), CTexture::NameForTextureType(tex->GetTextureType()));
					IRenderAuxText::WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 + 1), 1, 1, 1, 1, 1, 1, "%s   %d x %d", nameList[i].c_str(), tex->GetWidth(), tex->GetHeight());
				}
			}
		}

		IRenderAuxGeom::GetAux()->SetRenderFlags(oldRenderFlags);
	}

	// print dyn. textures
	{
		static bool bWasOn = false;

		if (CV_r_showdyntextures)
		{
			DrawAllDynTextures(CV_r_ShowDynTexturesFilter->GetString(), !bWasOn, CV_r_showdyntextures == 2);
			bWasOn = true;
		}
		else
			bWasOn = false;
	}
#endif//_RELEASE
}

void CD3D9Renderer::TryFlush()
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	//////////////////////////////////////////////////////////////////////
	// End the scene and update
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	m_pRT->RC_TryFlush();
}

void CD3D9Renderer::RenderAux()
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	if (auto currentCollector = m_currentAuxGeomCBCollector)
	{
		auto auxData = currentCollector->Get(0);
		auto renderData = currentCollector->SubmitAuxGeomsAndPrepareForRendering();

		// Commit all Aux Geom buffers except ones from the Render Thread,
		// Render Thread will commit it's own buffer right before final rendering
		m_pRT->ExecuteRenderThreadCommand([=/*, renderData = std::move(renderData)*/]() mutable        // Renable the capture-by-move once we support C++14..............
		{
			CRY_PROFILE_REGION(PROFILE_RENDERER, "CD3D9Renderer::RenderAux lambda");
			
			// Renders the aux geometries collected with the collector assigned to the renderer between begin and end.
			if (!GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled())
			{
				// In menu mode we also render to screen in addition to quad layer
				m_pRenderAuxGeomD3D->RT_Render(renderData);
			}
			if (GetS3DRend().IsStereoEnabled())
			{
				const auto target = RenderLayer::eQuadLayers_Headlocked_0;

				// Clear VR Quad
				if (auto dc = GetS3DRend().GetVrQuadLayerDisplayContext(target).first)
				{
					if (auto* bb = dc->GetCurrentBackBuffer())
						CClearSurfacePass::Execute(bb, Clr_Transparent);
				}

				auto stereoRenderScope = GetS3DRend().PrepareRenderingToVrQuadLayer(target);
				auxData->SetCamera(GetS3DRend().GetHeadLockedQuadCamera());
				auxData->SetCurrentDisplayContext(stereoRenderScope.GetDisplayContextKey());
				m_pRenderAuxGeomD3D->RT_Render(renderData);
			}

			m_pRenderAuxGeomD3D->RT_Reset(renderData);
			m_pRenderAuxGeomD3D->FreeMemory();
			ReturnAuxGeomCollector(currentCollector);
		}, ERenderCommandFlags::SkipDuringLoading);

		// Prepares aux geometry command buffer collector for next frame in case someone was generating aux commands before beginning next frame.
		SetCurrentAuxGeomCollector(GetOrCreateAuxGeomCollector(auxData->GetCamera()));
	}
#endif //ENABLE_RENDER_AUX_GEOM
}

void CD3D9Renderer::RenderAux_RT()
{
	CRY_ASSERT(m_pRT->IsRenderThread());

	// Add debug icons to Aux
	if (m_CVDisplayInfo && m_CVDisplayInfo->GetIVal() && iSystem && iSystem->IsDevMode())
	{
		static const int nIconSize = 32;
		int nIconIndex = 0;

		//FX_SetState(GS_NODEPTHTEST);

		const Vec2 overscanOffset = Vec2(s_overscanBorders.x * VIRTUAL_SCREEN_WIDTH, s_overscanBorders.y * VIRTUAL_SCREEN_HEIGHT);

		if (SShaderAsyncInfo::s_nPendingAsyncShaders > 0 && CV_r_shadersasynccompiling > 0 && CRendererResources::s_ptexIconShaderCompiling)
			IRenderAuxImage::Draw2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CRendererResources::s_ptexIconShaderCompiling->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		if (CTexture::IsStreamingInProgress() && CRendererResources::s_ptexIconStreaming)
			IRenderAuxImage::Draw2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CRendererResources::s_ptexIconStreaming->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		// indicate terrain texture streaming activity
		if (gEnv->p3DEngine && CRendererResources::s_ptexIconStreamingTerrainTexture && gEnv->p3DEngine->IsTerrainTextureStreamingInProgress())
			IRenderAuxImage::Draw2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CRendererResources::s_ptexIconStreamingTerrainTexture->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		if (IAISystem* pAISystem = gEnv->pAISystem)
		{
			if (INavigationSystem* pAINavigation = pAISystem->GetNavigationSystem())
			{
				if (pAINavigation->GetState() == INavigationSystem::Working)
					IRenderAuxImage::Draw2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CRendererResources::s_ptexIconNavigationProcessing->GetID(), 0, 1, 1, 0);
			}
		}

		++nIconIndex;
	}

	m_nDisableTemporalEffects = max(0, m_nDisableTemporalEffects - 1);

#if defined(ENABLE_RENDER_AUX_GEOM)
	m_renderThreadAuxGeom.Submit();

	auto renderData = std::vector<CAuxGeomCB*>{ &m_renderThreadAuxGeom };

	// In menu mode we also render to screen in addition to quad layer
	if (!GetS3DRend().IsStereoEnabled() || GetS3DRend().IsMenuModeEnabled())
	{
		m_pRenderAuxGeomD3D->RT_Render(renderData);
	}
	if (GetS3DRend().IsStereoEnabled())
	{
		const auto target = RenderLayer::eQuadLayers_Headlocked_0;

		auto stereoRenderScope = GetS3DRend().PrepareRenderingToVrQuadLayer(target);
		m_renderThreadAuxGeom.SetCamera(GetS3DRend().GetHeadLockedQuadCamera());
		m_renderThreadAuxGeom.SetCurrentDisplayContext(stereoRenderScope.GetDisplayContextKey());
		m_pRenderAuxGeomD3D->RT_Render(renderData);
	}

	m_pRenderAuxGeomD3D->RT_Reset(renderData);
	m_renderThreadAuxGeom.FreeMemory();
#endif //ENABLE_RENDER_AUX_GEOM
}

void CD3D9Renderer::EndFrame()
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	//////////////////////////////////////////////////////////////////////
	// End the scene and update
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	// Aux
	RenderAux();

	m_pRT->RC_EndFrame(!m_bStartLevelLoading);
}

static uint32 ComputePresentInterval(bool vsync, uint32 refreshNumerator, uint32 refreshDenominator)
{
	uint32 presentInterval = vsync ? 1 : 0;
	if (vsync && refreshNumerator != 0 && refreshDenominator != 0)
	{
		static ICVar* pSysMaxFPS = gEnv && gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_MaxFPS") : 0;
		if (pSysMaxFPS)
		{
			const int32 maxFPS = pSysMaxFPS->GetIVal();
			if (maxFPS > 0)
			{
				const float refreshRate = refreshNumerator / refreshDenominator;
				const float lockedFPS = maxFPS;

				presentInterval = (uint32) clamp_tpl((int) floorf(refreshRate / lockedFPS + 0.1f), 1, 4);
			}
		}
	}
	return presentInterval;
};

void CD3D9Renderer::RT_EndFrame()
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	if (!m_SceneRecurseCount)
	{
		iLog->Log("EndScene without BeginScene\n");
		return;
	}

	if (m_bDeviceLost)
		return;

	float fTime = 0; //iTimer->GetAsyncCurTimePrec();
	HRESULT hReturn = E_FAIL;

	CTimeValue TimeEndF = iTimer->GetAsyncTime();

	const ESystemGlobalState systemState = iSystem->GetSystemGlobalState();
	if (systemState > ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_END)
	{
		CTimeValue time0 = iTimer->GetAsyncTime();

		CTexture::Update();

		SRenderStatistics::Write().m_fTexUploadTime += (iTimer->GetAsyncTime() - time0).GetSeconds();
	}

	// Rnder-thread Aux
	RenderAux_RT();

	// VR social screen
	if (!GetS3DRend().IsMenuModeEnabled())
		GetS3DRend().DisplaySocialScreen();

	// Hardware mouse
	if (gEnv->pHardwareMouse)
		gEnv->pHardwareMouse->Render();

	EF_RemoveParticlesFromScene();

	//////////////////////////////////////////////////////////////////////
	// Profiling and statistics
	//////////////////////////////////////////////////////////////////////

	bool bProfilerOnSocialScreen = false;
#if !CRY_PLATFORM_ORBIS  // PSVR currently does not use a custom social screen
	if (const ICVar* pSocialScreenCVar = gEnv->pConsole->GetCVar("hmd_social_screen"))
		bProfilerOnSocialScreen = pSocialScreenCVar->GetIVal() >= 0;
#endif

	if (m_pPipelineProfiler && !bProfilerOnSocialScreen)
		m_pPipelineProfiler->EndFrame();

	CTimeValue timePresentBegin = iTimer->GetAsyncTime();
	GetS3DRend().SubmitFrameToHMD();

	if (m_pPipelineProfiler && bProfilerOnSocialScreen)
		m_pPipelineProfiler->EndFrame();

	SRenderStatistics::Write().Finish();

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		Logv("******************************* EndFrame ********************************\n");
#endif

	m_SceneRecurseCount--;

#if !defined(RELEASE)
	int numInvalidDrawcalls =
	  m_DevMan.GetNumInvalidDrawcalls() +
	  GetGraphicsPipeline().GetNumInvalidDrawcalls();

	if (numInvalidDrawcalls > 0 && m_cEF.m_ShaderCacheStats.m_nNumShaderAsyncCompiles == 0)
	{
		// If drawcalls are skipped although there are no pending shaders, something is going wrong
		iLog->LogError("Renderer: Skipped %i drawcalls", numInvalidDrawcalls);
		assert(0);
	}
#endif

	gRenDev->m_DevMan.RT_Tick();
	GetDeviceObjectFactory().OnEndFrame(gRenDev->GetRenderFrameID());

	gRenDev->m_fRTTimeEndFrame = iTimer->GetAsyncTime().GetDifferenceInSeconds(TimeEndF);

	// Update downscaled viewport
	m_PrevViewportScale = m_CurViewportScale;
	m_CurViewportScale = m_ReqViewportScale;

	// debug modes that disable viewport downscaling for ease of use
	if (CV_r_wireframe || CV_r_shownormals || CV_r_showtangents || CV_r_measureoverdraw)
	{
		m_CurViewportScale = Vec2(1, 1);
	}

	// will be overridden in RT_RenderScene by m_CurViewportScale
	SetCurDownscaleFactor(Vec2(1, 1));

	//assert (m_nRTStackLevel[0] == 0 && m_nRTStackLevel[1] == 0 && m_nRTStackLevel[2] == 0 && m_nRTStackLevel[3] == 0);
	// OLD PIPELINE
	//if (!m_bDeviceLost)
		//FX_Commit();

	// Capture previously presented back-buffer to not cause syncing CPU with GPU, lag is irrelevant because capturing is continuously going on
	// Frame-capture works without a swap-chain, and without presenting, but rendering will serialize between CPU and GPU (TODO: rotate buffers even when not presenting)
	CaptureFrameBuffer();
	CaptureFrameBufferCallBack();

	// Flip the back buffer to the front
	bool bSetActive = false;
	if (m_bSwapBuffers)
	{
		CRenderDisplayContext* pDC = GetActiveDisplayContext();
		CRY_ASSERT(pDC->IsSwapChainBacked());
		auto swapDC = static_cast<CSwapChainBackedRenderDisplayContext*>(pDC);

		if (!IsEditorMode())
		{
			CRY_PROFILE_REGION(PROFILE_RENDERER, "Present");
			pDC->PrePresent();

#if !defined(SUPPORT_DEVICE_INFO)
			if (gEnv && gEnv->pConsole)
			{
				static ICVar* pSysMaxFPS = gEnv->pConsole->GetCVar("sys_MaxFPS");
				static ICVar* pVSync = gEnv->pConsole->GetCVar("r_Vsync");
				if (pSysMaxFPS && pVSync)
				{
					int32 maxFPS = pSysMaxFPS->GetIVal();
					uint32 vSync = pVSync->GetIVal();
					if (vSync != 0)
					{
						LimitFramerate(maxFPS, false);
					}
				}
			}
#endif

#if CRY_RENDERER_GNM
			auto* const pCommandList = GnmCommandList(GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterfaceImpl());
			const CGnmSwapChain::EFlipMode flipMode = m_VSync ? CGnmSwapChain::kFlipModeSequential : CGnmSwapChain::kFlipModeImmediate;
			swapDC->GetSwapChain().Present(pCommandList, flipMode);
#elif CRY_PLATFORM_ORBIS
			hReturn = swapDC->GetSwapChain().Present(m_VSync ? 1 : 0, 0);
#elif CRY_PLATFORM_DURANGO
	#if DURANGO_ENABLE_ASYNC_DIPS
			WaitForAsynchronousDevice();
	#endif
			hReturn = swapDC->GetSwapChain().Present(m_VSync ? 1 : 0, 0);
	#if DURANGO_ENABLE_ASYNC_DIPS
			WaitForAsynchronousDevice();
	#endif
#elif defined(SUPPORT_DEVICE_INFO)
	#if CRY_PLATFORM_WINDOWS && !CRY_RENDERER_VULKAN
			swapDC->EnforceFullscreenPreemption();
	#endif
			DWORD syncInterval = ComputePresentInterval(swapDC->GetVSyncState() != 0, swapDC->GetRefreshRateNumerator(), swapDC->GetRefreshRateDemoninator());
			hReturn = swapDC->GetSwapChain().Present(syncInterval, 0);

			if (IHmdRenderer* pHmdRenderer = GetS3DRend().GetIHmdRenderer())
			{
				pHmdRenderer->OnPostPresent();
			}

			if (DXGI_ERROR_DEVICE_RESET == hReturn)
			{
				CryFatalError("DXGI_ERROR_DEVICE_RESET");
			}
			else if (DXGI_ERROR_DEVICE_REMOVED == hReturn)
			{
				//CryFatalError("DXGI_ERROR_DEVICE_REMOVED");
				HRESULT result = m_DeviceWrapper.GetDeviceRemovedReason();
				switch (result)
				{
				case DXGI_ERROR_DEVICE_HUNG:
					CryFatalError("DXGI_ERROR_DEVICE_HUNG");
					break;
				case DXGI_ERROR_DEVICE_REMOVED:
					CryFatalError("DXGI_ERROR_DEVICE_REMOVED");
					break;
				case DXGI_ERROR_DEVICE_RESET:
					CryFatalError("DXGI_ERROR_DEVICE_RESET");
					break;
				case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
					CryFatalError("DXGI_ERROR_DRIVER_INTERNAL_ERROR");
					break;
				case DXGI_ERROR_INVALID_CALL:
					CryFatalError("DXGI_ERROR_INVALID_CALL");
					break;

				default:
					CryFatalError("DXGI_ERROR_DEVICE_REMOVED");
					break;
				}
			}
			else if (SUCCEEDED(hReturn))
			{
				m_dwPresentStatus = 0;
			}

			// TODO
			bool bIsVRActive = false;
			IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
			if (pHmdManager && pHmdManager->GetHmdDevice())
				bIsVRActive = true;
#endif
		}
		else
		{
#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
			CRY_ASSERT_MESSAGE(0, "Case in EndFrame() not implemented yet");
#elif defined(SUPPORT_DEVICE_INFO)
			DWORD dwFlags = 0;
			if (m_dwPresentStatus & (epsOccluded | epsNonExclusive))
				dwFlags = DXGI_PRESENT_TEST;
			else
				dwFlags = 0;
			//hReturn = GetSwapChain().Present(0, dwFlags);
			if (swapDC)
			{
				swapDC->PrePresent();
				hReturn = swapDC->GetSwapChain().Present(0, dwFlags);
				if (hReturn == DXGI_ERROR_INVALID_CALL)
				{
					assert(0);
				}
				else if (DXGI_STATUS_OCCLUDED == hReturn)
					m_dwPresentStatus |= epsOccluded;
				else if (DXGI_ERROR_DEVICE_RESET == hReturn)
				{
					CryFatalError("DXGI_ERROR_DEVICE_RESET");
				}
				else if (DXGI_ERROR_DEVICE_REMOVED == hReturn)
				{
					CryFatalError("DXGI_ERROR_DEVICE_REMOVED");
				}
				else if (SUCCEEDED(hReturn))
				{
					// Now that we're no longer occluded and the other app has gone non-exclusive
					// allow us to render again
					m_dwPresentStatus = 0;
				}
			}
#endif
		}

		m_nFrameSwapID++;
	}

	m_fTimeWaitForGPU[gRenDev->GetRenderThreadID()] += iTimer->GetAsyncTime().GetDifferenceInSeconds(timePresentBegin);

#ifdef ENABLE_BENCHMARK_SENSOR
	m_benchmarkRendererSensor->afterSwapBuffers(GetDevice(), GetDeviceContext());
#endif
	CheckDeviceLost();

	GetS3DRend().NotifyFrameFinished();

	// Disable screenshot code path for pure release builds on consoles
	// Capture currently presented back-buffer to be able to captures frames without capture-lag
#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS || defined(ENABLE_LW_PROFILERS)
	if (CV_r_GetScreenShot)
	{
		ScreenShot(nullptr, IsEditorMode() ? GetActiveDisplayContext() : GetBaseDisplayContext());
		CV_r_GetScreenShot = 0;
	}
#endif
#ifndef CONSOLE_CONST_CVAR_MODE
	if (m_wireframe_mode != m_wireframe_mode_prev)
	{
		//// disable zpass in wireframe mode?
		CV_r_usezpass = m_nUseZpass;
	}
#endif
	if (CRenderer::CV_r_logTexStreaming)
	{
		LogStrv(0, "******************************* EndFrame ********************************\n");
		LogStrv(0, "Loaded: %.3f Kb, UpLoaded: %.3f Kb, UploadTime: %.3fMs\n\n", BYTES_TO_KB(CTexture::s_nTexturesDataBytesLoaded), BYTES_TO_KB(CTexture::s_nTexturesDataBytesUploaded), SRenderStatistics::Write().m_fTexUploadTime);
	}

	/*if (CTexture::s_nTexturesDataBytesLoaded > 3.0f*1024.0f*1024.0f)
	   CTexture::m_fStreamDistFactor = min(2048.0f, CTexture::m_fStreamDistFactor*1.2f);
	   else
	   CTexture::m_fStreamDistFactor = max(1.0f, CTexture::m_fStreamDistFactor/1.2f);*/
	CTexture::s_nTexturesDataBytesUploaded = 0;
	CTexture::s_nTexturesDataBytesLoaded = 0;

	m_wireframe_mode_prev = m_wireframe_mode;

	m_SceneRecurseCount++;

	if (GetGraphicsPipeline().IsInitialized())
	{
		// we render directly to a video memory buffer
		// we need to unlock it here in case we renderered a frame without any particles
		// lock the VMEM buffer for the next frame here (to prevent a lock in the mainthread)
		// NOTE: main thread is already working on buffer+1 and we want to prepare the next one => hence buffer+2
		gRenDev->LockParticleVideoMemory(gRenDev->GetRenderFrameID() + 2);
	}

#if CRY_PLATFORM_ORBIS && !CRY_RENDERER_GNM
	m_fTimeGPUIdlePercent[gRenDev->GetRenderThreadID()] = DXOrbis::Device()->GetGPUIdlePercentage();
	m_fTimeWaitForGPU[gRenDev->GetRenderThreadID()] = DXOrbis::Device()->GetCPUWaitOnGPUTime();
	m_fTimeProcessedGPU[gRenDev->GetRenderThreadID()] = DXOrbis::Device()->GetCPUFrameTime();
#else
	m_fTimeGPUIdlePercent[gRenDev->GetRenderThreadID()] = 0;

	#if !defined(ENABLE_SIMPLE_GPU_TIMERS)
	// BK: We need a way of getting gpu frame time in release, without gpu timers
	// for now we just use overall frame time
	m_fTimeProcessedGPU[gRenDev->GetRenderThreadID()] = m_fTimeProcessedRT[gRenDev->GetRenderThreadID()];
	#else
	RPProfilerStats profileStats = m_pPipelineProfiler->GetBasicStats(eRPPSTATS_OverallFrame, gRenDev->GetRenderThreadID());
	m_fTimeProcessedGPU[gRenDev->GetRenderThreadID()] = profileStats.gpuTime / 1000.0f; // Store in "seconds"
#endif
#endif

#if defined(USE_GEOM_CACHES)
	if (m_SceneRecurseCount == 1)
	{
		CREGeomCache::UpdateModified();
	}
#endif

#if CRY_RENDERER_OPENGL
	DXGLIssueFrameFences(GetDevice().GetRealDevice());
#endif //CRY_RENDERER_OPENGL

	// Note: Please be aware that the below has to be called AFTER the xenon texture-manager performs
	// Note: Please be aware that the below has to be called AFTER the texture-manager performs
	// it's garbage collection because a scheduled gpu copy might be pending
	// touching the memory that will be reclaimed below.
	m_DevBufMan.ReleaseEmptyBanks(gRenDev->GetRenderFrameID());

	// Free render objects that could have been used for this frame
	FreePermanentRenderObjects(gRenDev->GetRenderThreadID());
	// Free Blend, Depth, and Rasterizer State(s)
#if (CRY_RENDERER_DIRECT3D < 120) &&  defined(CRY_RENDERER_DIRECT3D)
	CDeviceStatesManagerDX11::GetInstance()->ReleaseUnusedStates(gcpRendD3D->GetFrameID());
#endif
	if (m_bStopRendererAtFrameEnd)
	{
		m_mtxStopAtRenderFrameEnd.Lock();
		m_condStopAtRenderFrameEnd.Notify();
		m_condStopAtRenderFrameEnd.Wait(m_mtxStopAtRenderFrameEnd);
		m_mtxStopAtRenderFrameEnd.Unlock();
	}
}

void CD3D9Renderer::RT_PresentFast()
{
	CRenderDisplayContext* pDC = GetActiveDisplayContext();
	pDC->PrePresent();

	CRY_ASSERT(pDC->IsSwapChainBacked());
	auto swapDC = static_cast<CSwapChainBackedRenderDisplayContext*>(pDC);

	HRESULT hReturn = S_OK;
#if CRY_PLATFORM_DURANGO
	#if DURANGO_ENABLE_ASYNC_DIPS
	WaitForAsynchronousDevice();
	#endif
	hReturn = swapDC->GetSwapChain().Present(m_VSync ? 1 : 0, 0);
#elif CRY_RENDERER_GNM
	auto* const pCommandList = GnmCommandList(GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterfaceImpl());
	const CGnmSwapChain::EFlipMode flipMode = m_VSync ? CGnmSwapChain::kFlipModeSequential : CGnmSwapChain::kFlipModeImmediate;
	swapDC->GetSwapChain().Present(pCommandList, flipMode);
#elif CRY_PLATFORM_ORBIS
	hReturn = swapDC->GetSwapChain().Present(m_VSync ? 1 : 0, 0);
#elif defined(SUPPORT_DEVICE_INFO)

	GetS3DRend().NotifyFrameFinished();

	#if CRY_PLATFORM_WINDOWS
	swapDC->EnforceFullscreenPreemption();
	#endif
	hReturn = swapDC->GetSwapChain().Present(swapDC->GetVSyncState() ? 1 : 0, 0);
#endif
	assert(hReturn == S_OK);

	m_nRenderThreadFrameID++;
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::RT_ScreenShot(const char* filename, CRenderDisplayContext* pDisplayContext)
{
	// ignore invalid file access for screenshots
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	bool bRet = true;
#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS || defined(ENABLE_LW_PROFILERS)

	CTexture* pPresentedBackBuffer = nullptr;
	if (pDisplayContext)
		pPresentedBackBuffer = pDisplayContext->GetPresentedBackBuffer();

	if (!gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting() || gEnv->bIsOutOfMemory || !pPresentedBackBuffer)
	{
		return false;
	}

	char path[ICryPak::g_nMaxPath];

	path[sizeof(path) - 1] = 0;
	gEnv->pCryPak->AdjustFileName(filename != 0 ? filename : "%USER%/ScreenShots", path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);

	if (!filename)
	{
		size_t pathLen = strlen(path);
		const char* pSlash = (!pathLen || path[pathLen - 1] == '/' || path[pathLen - 1] == '\\') ? "" : "/";

		int i = 0;
		for (; i < 10000; i++)
		{
	#if !CRY_PLATFORM_ORBIS
			cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d.jpg", pSlash, i);
	#else
			cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d.tga", pSlash, i); // only TGA for now, ##FMT_CHK##
	#endif
			// HACK
			// CaptureFrameBufferToFile must be fixed for 64-bit stereo screenshots
			if (GetS3DRend().IsStereoEnabled())
			{
	#if !CRY_PLATFORM_ORBIS
				cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d_L.jpg", pSlash, i);
	#else
				cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d_L.tga", pSlash, i); // only TGA for now, ##FMT_CHK##
	#endif
			}

			FILE* fp = fxopen(path, "rb");
			if (!fp)
				break; // file doesn't exist

			fclose(fp);
		}

		// HACK: DONT REMOVE Stereo3D will add _L and _R suffix later
	#if !CRY_PLATFORM_ORBIS
		cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d.jpg", pSlash, i);
	#else
		cry_sprintf(&path[pathLen], sizeof(path) - pathLen, "%sScreenShot%04d.tga", pSlash, i); // only TGA for now, ##FMT_CHK##
	#endif

		if (i == 10000)
		{
			iLog->Log("Cannot save screen shot! Too many files.");
			return false;
		}
	}

	if (!gEnv->pCryPak->MakeDir(PathUtil::GetParentDirectory(path)))
	{
		iLog->Log("Cannot save screen shot! Failed to create directory \"%s\".", path);
		return false;
	}

	// Log some stats
	// -----------------------------------
	iLog->LogWithType(ILog::eInputResponse, " ");
	iLog->LogWithType(ILog::eInputResponse, "Screenshot: %s", path);
	gEnv->pConsole->ExecuteString("goto");  // output position and angle, can be used with "goto" command from console

#if defined(ENABLE_PROFILING_CODE)
	iLog->LogWithType(ILog::eInputResponse, " ");
	iLog->LogWithType(ILog::eInputResponse, "$5Drawcalls: %d", SRenderStatistics::Write().GetNumberOfDrawCalls());
	iLog->LogWithType(ILog::eInputResponse, "$5FPS: %.1f (%.1f ms)", gEnv->pTimer->GetFrameRate(), gEnv->pTimer->GetFrameTime() * 1000.0f);
#endif

	int nPolygons, nShadowVolPolys;
	GetPolyCount(nPolygons, nShadowVolPolys);
	iLog->LogWithType(ILog::eInputResponse, "Tris: %2d,%03d", nPolygons / 1000, nPolygons % 1000);

	int nStreamCgfPoolSize = -1;
	ICVar* pVar = gEnv->pConsole->GetCVar("e_StreamCgfPoolSize");
	if (pVar)
		nStreamCgfPoolSize = pVar->GetIVal();

	if (gEnv->p3DEngine)
	{
		I3DEngine::SObjectsStreamingStatus objectsStreamingStatus;
		gEnv->p3DEngine->GetObjectsStreamingStatus(objectsStreamingStatus);
		iLog->LogWithType(ILog::eInputResponse, "CGF streaming: Loaded:%d InProg:%d All:%d Act:%d MemUsed:%2.2f MemReq:%2.2f PoolSize:%d",
		                  objectsStreamingStatus.nReady, objectsStreamingStatus.nInProgress, objectsStreamingStatus.nTotal, objectsStreamingStatus.nActive, BYTES_TO_MB(objectsStreamingStatus.nAllocatedBytes), BYTES_TO_MB(objectsStreamingStatus.nMemRequired), nStreamCgfPoolSize);
	}

	STextureStreamingStats stats(false);
	EF_Query(EFQ_GetTexStreamingInfo, stats);
	const int iPercentage = int((float)stats.nCurrentPoolSize / stats.nMaxPoolSize * 100.f);
	iLog->LogWithType(ILog::eInputResponse, "TexStreaming: MemUsed:%.2fMB(%d%%%%) PoolSize:%2.2fMB Trghput:%2.2fKB/s", BYTES_TO_MB(stats.nCurrentPoolSize), iPercentage, BYTES_TO_MB(stats.nMaxPoolSize), BYTES_TO_KB(stats.nThroughput));

	gEnv->pConsole->ExecuteString("sys_RestoreSpec test*");   // to get useful debugging information about current spec settings to the log file
	iLog->LogWithType(ILog::eInputResponse, " ");

	if (GetS3DRend().IsStereoEnabled())
	{
		bRet = GetS3DRend().TakeScreenshot(path);
	}
	else
	{
		bRet = RT_StoreTextureToFile(path, pPresentedBackBuffer);
	}
#endif//_RELEASE

	return bRet;
}

bool CD3D9Renderer::ShouldTrackStats()
{
	bool bShouldTrackStats = (CV_r_stats == 6 || m_pDebugRenderNode || m_bCollectDrawCallsInfo || m_bCollectDrawCallsInfoPerNode);
	return bShouldTrackStats;
}

bool CD3D9Renderer::ScreenShot(const char* filename)
{
	return ScreenShot(filename, SDisplayContextKey{});
}

bool CD3D9Renderer::ScreenShot(const char* filename, CRenderDisplayContext *pDC)
{
	bool bResult = false;

	ExecuteRenderThreadCommand([=, &bResult] { bResult = RT_ScreenShot(filename, pDC); },
		ERenderCommandFlags::FlushAndWait);

	return bResult;
}

bool CD3D9Renderer::ScreenShot(const char* filename, const SDisplayContextKey& displayContextKey)
{
	return ScreenShot(filename, FindDisplayContext(displayContextKey));
}

bool CD3D9Renderer::ReadFrameBuffer(uint32* pDstRGBA8, int destinationWidth, int destinationHeight, bool readPresentedBackBuffer, EReadTextureFormat format)
{
	bool bResult = false;

	ExecuteRenderThreadCommand([=, &bResult]
		{
			if (CTexture* pSourceTexture = readPresentedBackBuffer ? GetActiveDisplayContext()->GetPresentedBackBuffer() : GetActiveDisplayContext()->GetCurrentBackBuffer())
			{
				bResult = gRenDev->RT_ReadTexture(pDstRGBA8, destinationWidth, destinationHeight, format, pSourceTexture);
			}
		},
		ERenderCommandFlags::FlushAndWait
	);

	return bResult;
}

bool CD3D9Renderer::RT_ReadTexture(void* pDst, int destinationWidth, int destinationHeight, EReadTextureFormat dstFormat, CTexture* pSrc)
{
	if (!pDst || !CTexture::IsTextureExist(pSrc))
		return false;

#if CRY_PLATFORM_WINDOWS
	CTexture* pTmpCopyTex = nullptr;
	ETEX_Format dstTexFormat = dstFormat == EReadTextureFormat::BGR8 ? eTF_B8G8R8A8 : eTF_R8G8B8A8;

	if (destinationWidth != pSrc->GetWidth() || destinationHeight != pSrc->GetHeight() || pSrc->GetDstFormat() != dstTexFormat)
	{
		pTmpCopyTex = CTexture::GetOrCreate2DTexture("$TempCopyTex", destinationWidth, destinationHeight, 1, FT_USAGE_RENDERTARGET, nullptr, dstTexFormat);
		CStretchRectPass().Execute(pSrc, pTmpCopyTex);
		pSrc = pTmpCopyTex;
	}

	// Copy the frame from our local surface to the requested buffer location
	pSrc->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		CRY_ASSERT(rowPitch);

		const int srcStride = 4;
		const int dstStride = dstFormat == EReadTextureFormat::RGBA8 ? 4 : 3;

		for (unsigned int i = 0; i < destinationHeight; ++i)
		{
			uint8* pSrc(static_cast<uint8*>(pData) + i * rowPitch);
			uint8* pDst(static_cast<uint8*>(pDst)  + i * destinationWidth * dstStride);

			for (unsigned int j = 0; j < destinationWidth; ++j, pSrc += srcStride, pDst += dstStride)
			{
				pDst[0] = pSrc[0];
				pDst[1] = pSrc[1];
				pDst[2] = pSrc[2];

				if (dstFormat == EReadTextureFormat::RGBA8)
					pDst[3] = 255;
			}
		}

		return true;
	});

	SAFE_RELEASE(pTmpCopyTex);
#endif

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine initializes 2 destination surfaces for use by the CaptureFrameBufferFast routine
// It also, captures the current backbuffer into one of the created surfaces
//
// Inputs :  none
//
// Outputs : returns true if surfaces were created otherwise returns false
//
bool CD3D9Renderer::InitCaptureFrameBufferFast(uint32 bufferWidth, uint32 bufferHeight)
{
#if defined(ENABLE_PROFILING_CODE)
	if (!GetDevice().IsValid()) return(false);

	// Free the existing textures if they exist
	SAFE_RELEASE(m_pSaveTexture[0]);
	SAFE_RELEASE(m_pSaveTexture[1]);

	if (bufferWidth == 0)
		bufferWidth = GetActiveDisplayContext()->GetDisplayResolution()[0];
	if (bufferHeight == 0)
		bufferHeight = GetActiveDisplayContext()->GetDisplayResolution()[1];

	m_pSaveTexture[0] = CTexture::GetOrCreate2DTexture("$SaveTexture0", bufferWidth, bufferHeight, 1, FT_STAGE_READBACK, nullptr, eTF_R8G8B8A8);
	m_pSaveTexture[1] = CTexture::GetOrCreate2DTexture("$SaveTexture1", bufferWidth, bufferHeight, 1, FT_STAGE_READBACK, nullptr, eTF_R8G8B8A8);

	// Initialize one of the buffers by capturing the current backbuffer
	// If we allow this call on the MT we cannot interact with the device
	// The first screen grab will perform the copy anyway
	/*D3D11_BOX srcBox;
	   srcBox.left = 0; srcBox.right = dstDesc.Width;
	   srcBox.top = 0; srcBox.bottom = dstDesc.Height;
	   srcBox.front = 0; srcBox.back = 1;
	   GetDeviceContext().CopySubresourceRegion(m_pSaveTexture[0], 0, 0, 0, 0, pSourceTexture, 0, &srcBox);*/
#endif
	return(true);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine releases the 2 surfaces used for frame capture by the CaptureFrameBufferFast routine
//
// Inputs : None
//
// Returns : None
//
void CD3D9Renderer::CloseCaptureFrameBufferFast(void)
{

#if defined(ENABLE_PROFILING_CODE)
	SAFE_RELEASE(m_pSaveTexture[0]);
	SAFE_RELEASE(m_pSaveTexture[1]);
#endif

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routines uses 2 destination surfaces.  It triggers a backbuffer copy to one of its surfaces,
// and then copies the other surface to system memory.  This hopefully will remove any
// CPU stalls due to the rect lock call since the buffer will already be in system
// memory when it is called
// Inputs :
//			pDstARGBA8			:	Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface)
//			destinationWidth	:	Width of the frame to copy
//			destinationHeight	:	Height of the frame to copy
//
//			Note :	If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions
//					of the surface are used for the copy
//
bool CD3D9Renderer::CaptureFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight)
{
	bool bStatus(false);

#if defined(ENABLE_PROFILING_CODE)
	//In case this routine is called without the init function being called
	if (m_pSaveTexture[0] == NULL || m_pSaveTexture[1] == NULL || !GetDevice().IsValid()) return bStatus;

	CTexture* pSourceTexture = GetActiveDisplayContext()->GetPresentedBackBuffer();
	if (pSourceTexture)
	{
		if (true /*sourceDesc.SampleDesc.Count == 1*/)
		{
			CTexture* pCopySourceTexture;
			CTexture* pTargetTexture, * pCopyTexture;
			unsigned int width  = std::min(destinationWidth , pSourceTexture->GetWidth ());
			unsigned int height = std::min(destinationHeight, pSourceTexture->GetHeight());

			pTargetTexture = m_captureFlipFlop ? m_pSaveTexture[1] : m_pSaveTexture[0];
			pCopyTexture   = m_captureFlipFlop ? m_pSaveTexture[0] : m_pSaveTexture[1];
			m_captureFlipFlop = (m_captureFlipFlop + 1) % 2;

			if (width  != pSourceTexture->GetWidth() ||
				height != pSourceTexture->GetHeight())
			{
				RECT srcRct;
				srcRct.left = srcRct.top = 0;
				srcRct.right = pSourceTexture->GetWidth();
				srcRct.bottom = pSourceTexture->GetHeight();
			
				RECT dstRct;
				dstRct.left = dstRct.top = 0;
				dstRct.right = width;
				dstRct.bottom = height;

				// reuse stereo left and right RTs to downscale
				CStretchRegionPass().Execute(
					pSourceTexture,
					CRendererResources::s_ptexBackBuffer,
					&srcRct, &dstRct,
					true, ColorF(1,1,1,1), 0);


				pCopySourceTexture = CRendererResources::s_ptexBackBuffer;
			}
			else
			{
				pCopySourceTexture = pSourceTexture;
			}

			// Setup the copy of the captured frame to our local surface in the background
			const SResourceRegionMapping region =
			{
				{ 0,     0,      0, 0 },
				{ 0,     0,      0, 0 },
				{ width, height, 1, 1 }
			};

			GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(pCopySourceTexture->GetDevTexture(), pTargetTexture->GetDevTexture(), region);

			// Copy the previous frame from our local surface to the requested buffer location
			pCopyTexture->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
			{
				for (unsigned int i = 0; i < height; ++i)
				{
					uint8* pSrc((uint8*)pData + i * rowPitch);
					uint8* pDst((uint8*)pDstRGBA8 + i * width * 4);
					for (unsigned int j = 0; j < width; ++j, pSrc += 4, pDst += 4)
					{
						pDst[0] = pSrc[2];
						pDst[1] = pSrc[1];
						pDst[2] = pSrc[0];
						pDst[3] = 255;
					}
				}

				return true;
			});

			bStatus = true;
		}
	}
#endif

	return bStatus;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Copy a captured surface to a buffer
//
// Inputs :
//			pDstARGBA8			:	Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface)
//			destinationWidth	:	Width of the frame to copy
//			destinationHeight	:	Height of the frame to copy
//
//			Note :	If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions
//					of the surface are used for the copy
//
bool CD3D9Renderer::CopyFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight)
{
	bool bStatus(false);

#if defined(ENABLE_PROFILING_CODE)
	//In case this routine is called without the init function being called
	if (m_pSaveTexture[0] == NULL || m_pSaveTexture[1] == NULL || !GetDevice().IsValid())
		return bStatus;

	CTexture* pCopyTexture = m_captureFlipFlop ? m_pSaveTexture[0] : m_pSaveTexture[1];
	unsigned int width  = std::min(destinationWidth , pCopyTexture->GetWidth ());
	unsigned int height = std::min(destinationHeight, pCopyTexture->GetHeight());

	// Copy the previous frame from our local surface to the requested buffer location
	pCopyTexture->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		for (unsigned int i = 0; i < height; ++i)
		{
			uint8* pSrc((uint8*)pData + i * rowPitch);
			uint8* pDst((uint8*)pDstRGBA8 + i * width * 4);
			for (unsigned int j = 0; j < width; ++j, pSrc += 4, pDst += 4)
			{
				pDst[0] = pSrc[2];
				pDst[1] = pSrc[1];
				pDst[2] = pSrc[0];
				pDst[3] = 255;
			}
		}

		return true;
	});

	bStatus = true;
#endif

	return bStatus;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine checks for any frame buffer callbacks that are needed and calls them
//
// Inputs : None
//
//	Outputs : None
//
void CD3D9Renderer::CaptureFrameBufferCallBack(void)
{
#if defined(ENABLE_PROFILING_CODE)
	bool firstCopy = true;
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; ++i)
	{
		if (m_pCaptureCallBack[i] != NULL)
		{
			{
				unsigned char* pDestImage = NULL;
				bool bRequiresScreenShot = m_pCaptureCallBack[i]->OnNeedFrameData(pDestImage);

				if (bRequiresScreenShot)
				{
					if (pDestImage != NULL)
					{
						const int widthNotAligned = m_pCaptureCallBack[i]->OnGetFrameWidth();
						const int width = widthNotAligned - widthNotAligned % 4;
						const int height = m_pCaptureCallBack[i]->OnGetFrameHeight();

						bool bCaptured;
						if (firstCopy)
						{
							bCaptured = CaptureFrameBufferFast(pDestImage, width, height);
						}
						else
						{
							bCaptured = CopyFrameBufferFast(pDestImage, width, height);
						}

						if (bCaptured == true)
						{
							m_pCaptureCallBack[i]->OnFrameCaptured();
						}

						firstCopy = false;
					}
					else
					{
						//Assuming this texture is just going to be read by GPU, no need to block on fence
						//if( !m_pd3dDevice->IsFencePending( m_FenceFrameCaptureReady ) )
						{
							m_pCaptureCallBack[i]->OnFrameCaptured();
						}
					}
				}
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine checks for any frame buffer callbacks that are needed and checks their flags, calling preparation functions if required
//
// Inputs : None
//
//	Outputs : None
//
void CD3D9Renderer::CaptureFrameBufferPrepare(void)
{
#if defined(ENABLE_PROFILING_CODE)
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; ++i)
	{
		if (m_pCaptureCallBack[i] != NULL)
		{
			//The consoles collect screenshots asynchronously
			int texHandle = 0;
			int flags = m_pCaptureCallBack[i]->OnCaptureFrameBegin(&texHandle);

			//screen shot requested
			if (flags & ICaptureFrameListener::eCFF_CaptureThisFrame)
			{
				int currentFrame = gRenDev->GetMainFrameID();

				//Currently we only support one screen capture request per frame
				if (m_nScreenCaptureRequestFrame[gRenDev->GetMainThreadID()] != currentFrame)
				{
					m_screenCapTexHandle[gRenDev->GetMainThreadID()] = texHandle;
					m_nScreenCaptureRequestFrame[gRenDev->GetMainThreadID()] = currentFrame;
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Multiple screen caps in a single frame not supported.");
				}
			}
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// This routine registers an ICaptureFrameListener object for callback when a frame is available
//
// Inputs :
//			pCapture	:	address of the ICaptureFrameListener object
//
//	Outputs : true if successful, false if not successful
//
bool CD3D9Renderer::RegisterCaptureFrame(ICaptureFrameListener* pCapture)
{
#if defined(ENABLE_PROFILING_CODE)
	// Check to see if the address is already registered
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; ++i)
	{
		if (m_pCaptureCallBack[i] == pCapture) return(true);
	}

	// Try to find a space for the caller
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; ++i)
	{
		if (m_pCaptureCallBack[i] == NULL)
		{

			// If no other callers are registered, init the capture routine
			if (m_frameCaptureRegisterNum == 0)
			{
				uint32 bufferWidth = 0, bufferHeight = 0;
				bool status = InitCaptureFrameBufferFast(bufferWidth, bufferHeight);
				if (status == false) return(false);
			}

			m_pCaptureCallBack[i] = pCapture;
			++m_frameCaptureRegisterNum;
			return(true);
		}
	}
#endif
	// Could not find space
	return(false);

}

/////////////////////////////////////////////////////////////////////////
// This routine unregisters an ICaptureFrameListener object for callback
//
// Inputs :
//			pCapture	:	address of the ICaptureFrameListener object
//
//	Outputs : 1 if successful, 0 if not successful
//
bool CD3D9Renderer::UnRegisterCaptureFrame(ICaptureFrameListener* pCapture)
{
#if defined(ENABLE_PROFILING_CODE)
	// Check to see if the address is registered, and unregister if it is
	for (unsigned int i = 0; i < MAXFRAMECAPTURECALLBACK; ++i)
	{
		if (m_pCaptureCallBack[i] == pCapture)
		{
			m_pCaptureCallBack[i] = NULL;
			--m_frameCaptureRegisterNum;
			if (m_frameCaptureRegisterNum == 0) CloseCaptureFrameBufferFast();
			return(true);
		}
	}
#endif
	return(false);
}

int CD3D9Renderer::GetDetailedRayHitInfo(IPhysicalEntity* pCollider, const Vec3& vOrigin, const Vec3& vDirection, const float maxRayDist, float* pUOut, float* pVOut)
{
	int type = pCollider->GetiForeignData();

	if (type == PHYS_FOREIGN_ID_ENTITY)
	{
		IEntity* pEntity = (IEntity*)pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		IEntityRender* pIEntityRender = pEntity ? pEntity->GetRenderInterface() : nullptr;

		if (pIEntityRender == nullptr)
		{
			return -1;
		}

		int dynTexGeomSlot = 0;

		IStatObj* pObj = nullptr;
		for (uint i = 0; i < pEntity->GetSlotCount(); ++i)
		{
			pObj = pEntity->GetStatObj(i);

			if (pObj != nullptr)
			{
				dynTexGeomSlot = i;
				break;
			}
		}

		if (pObj == nullptr)
		{
			return -1;
		}

		// result
		bool hasHit = false;
		Vec2 uv0, uv1, uv2;
		Vec3 p0, p1, p2;
		Vec3 hitpos;

		// calculate ray dir
		CCamera cam = GetISystem()->GetViewCamera();
		if (pEntity->GetSlotFlags(dynTexGeomSlot) & ENTITY_SLOT_RENDER_NEAREST)
		{
			static ICVar* r_drawnearfov = gEnv->pConsole->GetCVar("r_DrawNearFoV");
			assert(r_drawnearfov);
			cam.SetFrustum(cam.GetViewSurfaceX(), cam.GetViewSurfaceZ(), DEG2RAD(r_drawnearfov->GetFVal()), cam.GetNearPlane(), cam.GetFarPlane(), cam.GetPixelAspectRatio());
		}

		Vec3 vPos0 = vOrigin;
		Vec3 vPos1 = vOrigin + vDirection;

		// translate into object space
		const Matrix34 m = pEntity->GetWorldTM().GetInverted();
		vPos0 = m * vPos0;
		vPos1 = m * vPos1;

		// walk through all sub objects
		const int objCount = pObj->GetSubObjectCount();
		for (int obj = 0; obj <= objCount && !hasHit; ++obj)
		{
			Vec3 vP0, vP1;
			IStatObj* pSubObj = NULL;

			if (obj == objCount)
			{
				vP0 = vPos0;
				vP1 = vPos1;
				pSubObj = pObj;
			}
			else
			{
				IStatObj::SSubObject* pSub = pObj->GetSubObject(obj);
				const Matrix34 mm = pSub->tm.GetInverted();
				vP0 = mm * vPos0;
				vP1 = mm * vPos1;
				pSubObj = pSub->pStatObj;
			}

			IRenderMesh* pMesh = pSubObj ? pSubObj->GetRenderMesh() : NULL;
			if (pMesh)
			{
				const Ray ray(vP0, (vP1 - vP0).GetNormalized() * maxRayDist);
				hasHit = pMesh->RayIntersectMesh(ray, hitpos, p0, p1, p2, uv0, uv1, uv2);
			}
		}

		// skip if not hit
		if (!hasHit)
			return -1;

		// calculate vectors from hitpos to vertices p0, p1 and p2:
		const Vec3 v0 = p0 - hitpos;
		const Vec3 v1 = p1 - hitpos;
		const Vec3 v2 = p2 - hitpos;

		// calculate factors
		const float h = (p0 - p1).Cross(p0 - p2).GetLength();
		const float f0 = v1.Cross(v2).GetLength() / h;
		const float f1 = v2.Cross(v0).GetLength() / h;
		const float f2 = v0.Cross(v1).GetLength() / h;

		// find the uv corresponding to hitpos
		Vec3 uv = uv0 * f0 + uv1 * f1 + uv2 * f2;
		*pUOut = uv.x;
		*pVOut = uv.y;
		return (int)pEntity->GetId();
	}
	return -1;
}

Vec3 CD3D9Renderer::UnprojectFromScreen(int x, int y)
{
	Vec3 vDir, vCenter;
	UnProjectFromScreen((float)x, (float)y, 0, &vCenter.x, &vCenter.y, &vCenter.z);
	UnProjectFromScreen((float)x, (float)y, 1, &vDir.x, &vDir.y, &vDir.z);
	return (vDir - vCenter).normalized();
}

void CD3D9Renderer::Graph(byte* g, int x, int y, int width, int height, int nC, int type, const char* text, ColorF& color, float fScale)
{
	ColorF col;
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3 * vp = (Vec3*) alloca(width * sizeof(Vec3));
	int i;

	CRenderDisplayContext* pDC = GetActiveDisplayContext();

	const SAuxGeomRenderFlags oldRenderFlags = IRenderAuxGeom::GetAux()->GetRenderFlags();

	SAuxGeomRenderFlags newRenderFlags = oldRenderFlags;
	newRenderFlags.SetMode2D3DFlag(e_Mode2D);
	newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
	newRenderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	newRenderFlags.SetCullMode(e_CullModeNone);
	newRenderFlags.SetAlphaBlendMode(e_AlphaNone);
	IRenderAuxGeom::GetAux()->SetRenderFlags(newRenderFlags);

	col = Col_Blue;
	int num = CRendererResources::s_ptexWhite->GetTextureID();

	float fy = (float)y;
	float fx = (float)x;
	float fwdt = (float)width;
	float fhgt = (float)height;

	IRenderAuxImage::DrawImage(fx, fy, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	IRenderAuxImage::DrawImage(fx, fy + fhgt, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	IRenderAuxImage::DrawImage(fx, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	IRenderAuxImage::DrawImage(fx + fwdt - 2, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);

	float fGround = CV_r_graphstyle ? fy + fhgt : -1;

	for (i = 0; i < width; i++)
	{
		PREFAST_SUPPRESS_WARNING(6386);
		vp[i].x = (float)i + fx;
		vp[i].y = fy + (float)(g[i]) * fhgt / 255.0f;
		vp[i].z = 0;
	}
	ColorB colorb(col);
	if (type == 1)
	{
		colorb = color;
		IRenderAuxGeom::GetAux()->DrawLineStrip(&vp[0], nC, &colorb, 3);
		colorb = ColorF(1.0f) - col;
		colorb[3] = 255;
		IRenderAuxGeom::GetAux()->DrawLineStrip(&vp[nC], width - nC, &colorb, 3);
	}
	else if (type == 2)
	{
		colorb = color;
		IRenderAuxGeom::GetAux()->DrawLineStrip(&vp[0], width, &colorb, 3);
	}

	if (text)
	{
		IRenderAuxText::WriteXY(4, y - 18, 0.5f, 1, 1, 1, 1, 1, "%s", text);
		IRenderAuxText::WriteXY(width - 260, y - 18, 0.5f, 1, 1, 1, 1, 1, "%d ms", (int)(1000.0f * fScale));
	}

	IRenderAuxGeom::GetAux()->SetRenderFlags(oldRenderFlags);
}

void CD3D9Renderer::PushProfileMarker(const char* label)
{
	ExecuteRenderThreadCommand(
		[=]{ this->SetProfileMarker(label, CRenderer::ESPM_PUSH); },
		ERenderCommandFlags::SkipDuringLoading
	);
}

void CD3D9Renderer::PopProfileMarker(const char* label)
{
	ExecuteRenderThreadCommand(
		[=] { this->SetProfileMarker(label, CRenderer::ESPM_POP); },
		ERenderCommandFlags::SkipDuringLoading
	);
}

///////////////////////////////////////////
void CD3D9Renderer::EnableVSync(bool enable)
{
	// TODO: remove
}
///////////////////////////////////////////

void CD3D9Renderer::SetProfileMarker(const char* label, ESPM mode) const
{
	if (mode == ESPM_PUSH)
	{
		PROFILE_LABEL_PUSH(label);
	}
	else
	{
		PROFILE_LABEL_POP(label);
	}
};

///////////////////////////////////////////
bool CD3D9Renderer::ProjectToScreen(float ptx, float pty, float ptz, float* sx, float* sy, float* sz)
{
	const CCamera& camera = GetISystem()->GetViewCamera();
	Vec3 out(0, 0, 0);
	bool visible = camera.Project(Vec3(ptx, pty, ptz), out);

	// Returns sx,sy in the range 0 to 100
	*sx = out.x * 100.f / camera.GetViewSurfaceX();
	*sy = out.y * 100.f / camera.GetViewSurfaceZ();
	*sz = out.z;

	return visible;
}

static bool InvertMatrixPrecise(Matrix44& out, const float* m)
{
	// Inverts matrix using Gaussian Elimination which is slower but numerically more stable than Cramer's Rule

	float expmat[4][8] = {
		{ m[0], m[4], m[8],  m[12], 1.f, 0.f, 0.f, 0.f },
		{ m[1], m[5], m[9],  m[13], 0.f, 1.f, 0.f, 0.f },
		{ m[2], m[6], m[10], m[14], 0.f, 0.f, 1.f, 0.f },
		{ m[3], m[7], m[11], m[15], 0.f, 0.f, 0.f, 1.f }
	};

	float t0, t1, t2, t3, t;
	float* r0 = expmat[0], * r1 = expmat[1], * r2 = expmat[2], * r3 = expmat[3];

	// Choose pivots and eliminate variables
	if (fabs(r3[0]) > fabs(r2[0])) std::swap(r3, r2);
	if (fabs(r2[0]) > fabs(r1[0])) std::swap(r2, r1);
	if (fabs(r1[0]) > fabs(r0[0])) std::swap(r1, r0);
	if (r0[0] == 0) return false;
	t1 = r1[0] / r0[0];
	t2 = r2[0] / r0[0];
	t3 = r3[0] / r0[0];
	t = r0[1];
	r1[1] -= t1 * t;
	r2[1] -= t2 * t;
	r3[1] -= t3 * t;
	t = r0[2];
	r1[2] -= t1 * t;
	r2[2] -= t2 * t;
	r3[2] -= t3 * t;
	t = r0[3];
	r1[3] -= t1 * t;
	r2[3] -= t2 * t;
	r3[3] -= t3 * t;
	t = r0[4];
	if (t != 0.0) { r1[4] -= t1 * t; r2[4] -= t2 * t; r3[4] -= t3 * t; }
	t = r0[5];
	if (t != 0.0) { r1[5] -= t1 * t; r2[5] -= t2 * t; r3[5] -= t3 * t; }
	t = r0[6];
	if (t != 0.0) { r1[6] -= t1 * t; r2[6] -= t2 * t; r3[6] -= t3 * t; }
	t = r0[7];
	if (t != 0.0) { r1[7] -= t1 * t; r2[7] -= t2 * t; r3[7] -= t3 * t; }

	if (fabs(r3[1]) > fabs(r2[1])) std::swap(r3, r2);
	if (fabs(r2[1]) > fabs(r1[1])) std::swap(r2, r1);
	if (r1[1] == 0) return false;
	t2 = r2[1] / r1[1];
	t3 = r3[1] / r1[1];
	r2[2] -= t2 * r1[2];
	r3[2] -= t3 * r1[2];
	r2[3] -= t2 * r1[3];
	r3[3] -= t3 * r1[3];
	t = r1[4];
	if (0.0 != t) { r2[4] -= t2 * t; r3[4] -= t3 * t; }
	t = r1[5];
	if (0.0 != t) { r2[5] -= t2 * t; r3[5] -= t3 * t; }
	t = r1[6];
	if (0.0 != t) { r2[6] -= t2 * t; r3[6] -= t3 * t; }
	t = r1[7];
	if (0.0 != t) { r2[7] -= t2 * t; r3[7] -= t3 * t; }

	if (fabs(r3[2]) > fabs(r2[2])) std::swap(r3, r2);
	if (r2[2] == 0) return false;
	t3 = r3[2] / r2[2];
	r3[3] -= t3 * r2[3];
	r3[4] -= t3 * r2[4];
	r3[5] -= t3 * r2[5];
	r3[6] -= t3 * r2[6];
	r3[7] -= t3 * r2[7];

	if (r3[3] == 0) return false;

	// Substitute back
	t = 1.0f / r3[3];
	r3[4] *= t;
	r3[5] *= t;
	r3[6] *= t;
	r3[7] *= t;                                                        // Row 3

	t2 = r2[3];
	t = 1.0f / r2[2];            // Row 2
	r2[4] = t * (r2[4] - r3[4] * t2);
	r2[5] = t * (r2[5] - r3[5] * t2);
	r2[6] = t * (r2[6] - r3[6] * t2);
	r2[7] = t * (r2[7] - r3[7] * t2);
	t1 = r1[3];
	r1[4] -= r3[4] * t1;
	r1[5] -= r3[5] * t1;
	r1[6] -= r3[6] * t1;
	r1[7] -= r3[7] * t1;
	t0 = r0[3];
	r0[4] -= r3[4] * t0;
	r0[5] -= r3[5] * t0;
	r0[6] -= r3[6] * t0;
	r0[7] -= r3[7] * t0;

	t1 = r1[2];
	t = 1.0f / r1[1];            // Row 1
	r1[4] = t * (r1[4] - r2[4] * t1);
	r1[5] = t * (r1[5] - r2[5] * t1);
	r1[6] = t * (r1[6] - r2[6] * t1);
	r1[7] = t * (r1[7] - r2[7] * t1);
	t0 = r0[2];
	r0[4] -= r2[4] * t0;
	r0[5] -= r2[5] * t0;
	r0[6] -= r2[6] * t0, r0[7] -= r2[7] * t0;

	t0 = r0[1];
	t = 1.0f / r0[0];            // Row 0
	r0[4] = t * (r0[4] - r1[4] * t0);
	r0[5] = t * (r0[5] - r1[5] * t0);
	r0[6] = t * (r0[6] - r1[6] * t0);
	r0[7] = t * (r0[7] - r1[7] * t0);

	out.m00 = r0[4];
	out.m01 = r0[5];
	out.m02 = r0[6];
	out.m03 = r0[7];
	out.m10 = r1[4];
	out.m11 = r1[5];
	out.m12 = r1[6];
	out.m13 = r1[7];
	out.m20 = r2[4];
	out.m21 = r2[5];
	out.m22 = r2[6];
	out.m23 = r2[7];
	out.m30 = r3[4];
	out.m31 = r3[5];
	out.m32 = r3[6];
	out.m33 = r3[7];

	return true;
}

static int sUnProject(float winx, float winy, float winz, const float model[16], const float proj[16], const int viewport[4], float* objx, float* objy, float* objz)
{
	Vec4 vIn;
	vIn.x = (winx - viewport[0]) * 2 / viewport[2] - 1.0f;
	vIn.y = (winy - viewport[1]) * 2 / viewport[3] - 1.0f;
	vIn.z = winz;//2.0f * winz - 1.0f;
	vIn.w = 1.0;

	float m1[16];
	for (int i = 0; i < 4; i++)
	{
		float ai0 = proj[i], ai1 = proj[4 + i], ai2 = proj[8 + i], ai3 = proj[12 + i];
		m1[i] = ai0 * model[0] + ai1 * model[1] + ai2 * model[2] + ai3 * model[3];
		m1[4 + i] = ai0 * model[4] + ai1 * model[5] + ai2 * model[6] + ai3 * model[7];
		m1[8 + i] = ai0 * model[8] + ai1 * model[9] + ai2 * model[10] + ai3 * model[11];
		m1[12 + i] = ai0 * model[12] + ai1 * model[13] + ai2 * model[14] + ai3 * model[15];
	}

	Matrix44 m;
	InvertMatrixPrecise(m, m1);

	Vec4 vOut = m * vIn;
	if (vOut.w == 0.0)
		return false;
	*objx = vOut.x / vOut.w;
	*objy = vOut.y / vOut.w;
	*objz = vOut.z / vOut.w;
	return true;
}

int CD3D9Renderer::UnProject(float sx, float sy, float sz, float* px, float* py, float* pz, const float modelMatrix[16], const float projMatrix[16], const int viewport[4])
{
	return sUnProject(sx, sy, sz, modelMatrix, projMatrix, viewport, px, py, pz);
}

int CD3D9Renderer::UnProjectFromScreen(float sx, float sy, float sz, float* px, float* py, float* pz)
{
	const CCamera& camera = GetISystem()->GetViewCamera();

	Matrix44 viewMatrix = camera.GetRenderViewMatrix();
	Matrix44 projMatrix = camera.GetRenderProjectionMatrix();
	int viewport[4];

	const int nThreadID = m_pRT->GetThreadList();
	//const bool bReverseDepth = true;
	//if (bReverseDepth)
		//sz = 1.0f - sz;

	viewport[0] = 0;
	viewport[1] = 0;
	viewport[2] = camera.GetViewSurfaceX();
	viewport[3] = camera.GetViewSurfaceZ();

	return sUnProject(sx, sy, sz, &viewMatrix.m00, &projMatrix.m00, viewport, px, py, pz);
}

void CD3D9Renderer::Set2DMode(bool enable, int ortox, int ortoy, float znear, float zfar)
{
	//ASSERT_LEGACY_PIPELINE
}

void CD3D9Renderer::RemoveTexture(unsigned int TextureId)
{
	if (!TextureId)
		return;

	CTexture* pTexture = CTexture::GetByID(TextureId);
	if (pTexture)
	{
		SAFE_RELEASE(pTexture);
	}
}

void CD3D9Renderer::UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTFSrc, int posz, int sizez)
{
	if (m_bDeviceLost)
		return;

	CTexture* pTex = CTexture::GetByID(tnum);
	pTex->UpdateTextureRegion(newdata, posx, posy, posz, w, h, sizez, eTFSrc);
}

bool CD3D9Renderer::EF_PrecacheResource(SShaderItem* pSI, int iScreenTexels, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	CShader* pSH = (CShader*)pSI->m_pShader;

	if (pSH && !(pSH->m_Flags & EF_NODRAW))
	{
		if (CShaderResources* pSR = (CShaderResources*)pSI->m_pShaderResources)
		{
			// Optimisations: 1) Virtual calls removed. 2) Prefetch next iteration's SEfResTexture
			SEfResTexture* pNextResTex = NULL;
			EEfResTextures iSlot;
			for (iSlot = EEfResTextures(0); iSlot < EFTT_MAX; iSlot = EEfResTextures(iSlot + 1))
			{
				pNextResTex = pSR->m_Textures[iSlot];
				if (pNextResTex)
				{
					PrefetchLine(pNextResTex, offsetof(SEfResTexture, m_Sampler.m_pITex));
					iSlot = EEfResTextures(iSlot + 1);
					break;
				}
			}
			while (pNextResTex)
			{
				SEfResTexture* pResTex = pNextResTex;
				pNextResTex = NULL;
				for (; iSlot < EFTT_MAX; iSlot = EEfResTextures(iSlot + 1))
				{
					pNextResTex = pSR->m_Textures[iSlot];
					if (pNextResTex)
					{
						PrefetchLine(pNextResTex, offsetof(SEfResTexture, m_Sampler.m_pITex));
						iSlot = EEfResTextures(iSlot + 1);
						break;
					}
				}
				if (ITexture* pITex = pResTex->m_Sampler.m_pITex)
				{
					float minTiling = min(fabsf(pResTex->GetTiling(0)), fabsf(pResTex->GetTiling(1)));
					int maxResolution = max(abs(pITex->GetHeight()), abs(pITex->GetWidth()));

					float minMipMap = log(max(1.0f, iScreenTexels / minTiling)) / log(2.0f);
					float maxMipMap = log(maxResolution) / log(2.0f);

					float currentMipFactor = expf((maxMipMap - minMipMap) * 2.0f * 0.69314718055994530941723212145818f /*LN2*/);
					float fMipFactor = currentMipFactor / gRenDev->GetMipDistFactor(pITex->GetHeight(), pITex->GetWidth());

					CD3D9Renderer::EF_PrecacheResource(pITex, fMipFactor, 0.f, Flags, nUpdateId, nCounter);
				}
			}
		}
	}

	return true;
}

bool CD3D9Renderer::EF_PrecacheResource(SShaderItem* pSI, float fMipFactorSI, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	CShader* pSH = (CShader*)pSI->m_pShader;

	if (pSH && !(pSH->m_Flags & EF_NODRAW))
	{
		if (CShaderResources* pSR = (CShaderResources*)pSI->m_pShaderResources)
		{
			// Optimisations: 1) Virtual calls removed. 2) Prefetch next iteration's SEfResTexture
			SEfResTexture* pNextResTex = NULL;
			EEfResTextures iSlot;
			for (iSlot = EEfResTextures(0); iSlot < EFTT_MAX; iSlot = EEfResTextures(iSlot + 1))
			{
				pNextResTex = pSR->m_Textures[iSlot];
				if (pNextResTex)
				{
					PrefetchLine(pNextResTex, offsetof(SEfResTexture, m_Sampler.m_pITex));
					iSlot = EEfResTextures(iSlot + 1);
					break;
				}
			}
			while (pNextResTex)
			{
				SEfResTexture* pResTex = pNextResTex;
				pNextResTex = NULL;
				for (; iSlot < EFTT_MAX; iSlot = EEfResTextures(iSlot + 1))
				{
					pNextResTex = pSR->m_Textures[iSlot];
					if (pNextResTex)
					{
						PrefetchLine(pNextResTex, offsetof(SEfResTexture, m_Sampler.m_pITex));
						iSlot = EEfResTextures(iSlot + 1);
						break;
					}
				}
				if (ITexture* pITex = pResTex->m_Sampler.m_pITex)
				{
					float fMipFactor = fMipFactorSI * min(fabsf(pResTex->GetTiling(0)), fabsf(pResTex->GetTiling(1)));
					CD3D9Renderer::EF_PrecacheResource(pITex, fMipFactor, 0.f, Flags, nUpdateId, nCounter);
				}
			}
		}
	}

	return true;
}

bool CD3D9Renderer::EF_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	if (CRenderer::CV_r_TexturesStreamingDebug)
	{
		const char* const sTexFilter = CRenderer::CV_r_TexturesStreamingDebugfilter->GetString();
		if (sTexFilter != 0 && sTexFilter[0])
			if (strstr(pTP->GetName(), sTexFilter))
				CryLogAlways("CD3D9Renderer::EF_PrecacheResource: Mip=%5.2f nUpdateId=%4d (%s) Name=%s",
				             fMipFactor, nUpdateId, (Flags & FPR_SINGLE_FRAME_PRIORITY_UPDATE) ? "NEAR" : "FAR", pTP->GetName());
	}

	m_pRT->RC_PrecacheResource(pTP, fMipFactor, fTimeToReady, Flags, nUpdateId, nCounter);

	return true;
}

unsigned int CD3D9Renderer::UploadToVideoMemory(unsigned char* pSrcData, int w, int h, int d, ETEX_Format eSrcFormat, ETEX_Format eTFDst, int nummipmap, ETEX_Type eTT, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	char name[128];
	if (!szCacheName)
		cry_sprintf(name, "$AutoDownload_%d", m_TexGenID++);
	else
		cry_strcpy(name, szCacheName);

	if (!nummipmap)
	{
		if (filter != FILTER_BILINEAR && filter != FILTER_TRILINEAR)
			flags |= FT_NOMIPS;
		nummipmap = 1;
	}
	else if (nummipmap < 0)
		nummipmap = CTexture::CalcNumMips(w, h);

	flags |= FT_DONT_STREAM;
	if (!repeat)
		flags |= FT_STATE_CLAMP;

	CTexture* pTex = 0;
	if (Id > 0)
	{
		if (pTex = CTexture::GetByID(Id))
		{
			if (pRegion)
				pTex->UpdateTextureRegion(pSrcData, pRegion->x, pRegion->y, 0, pRegion->w, pRegion->h, 1, eSrcFormat);
			else
				pTex->UpdateTextureRegion(pSrcData, 0, 0, 0, w, h, 1, eSrcFormat);
		}
	}
	else
	{
		if (bAsyncDevTexCreation && m_pRT->IsMultithreaded())
		{
			// make texture without device texture and request it async creation
			int nImgSize = CTexture::TextureDataSize(w, h, d, nummipmap, 1, eSrcFormat);
			std::shared_ptr<std::vector<uint8>> pImgData = std::make_shared<std::vector<uint8>>((uint8*)pSrcData, (uint8*)pSrcData + nImgSize);

			if (eTT == eTT_3D)
				pTex = CTexture::GetOrCreateTextureObject(name, w, h, d, eTT_3D, flags, eTFDst);
			else if (eTT == eTT_2D)
				pTex = CTexture::GetOrCreateTextureObject(name, w, h, 1, eTT_2D, flags, eTFDst);
			else if (eTT == eTT_2DArray)
				assert(!"Not supported");
			else if (eTT == eTT_Cube)
				assert(!"Not supported");
			else if (eTT == eTT_CubeArray)
				assert(!"Not supported");   // tp = CTexture::CreateCubeTexture(name, w, h, nummipmap, flags, data, eTFSrc);

			pTex->m_bAsyncDevTexCreation = bAsyncDevTexCreation;
			pTex->m_eSrcFormat = eSrcFormat;
			pTex->m_nMips = nummipmap;

			auto asyncTextureCreateCallback = [=]
			{
				const void* pData[] = { pImgData->data(), nullptr, nullptr, nullptr };
				pTex->CreateDeviceTexture(pData);
			};

			ExecuteRenderThreadCommand(asyncTextureCreateCallback, ERenderCommandFlags::LevelLoadingThread_executeDirect);
		}
		else if (eTT == eTT_3D)
			pTex = CTexture::GetOrCreate3DTexture(name, w, h, d, nummipmap, flags, pSrcData, eSrcFormat);
		else if (eTT == eTT_2D)
			pTex = CTexture::GetOrCreate2DTexture(name, w, h, nummipmap, flags, pSrcData, eSrcFormat);
		else if (eTT == eTT_2DArray)
			assert(!"Not supported");
		else if (eTT == eTT_Cube)
			assert(!"Not supported");
		else if (eTT == eTT_CubeArray)
			assert(!"Not supported");   // tp = CTexture::CreateCubeTexture(name, w, h, nummipmap, flags, data, eTFSrc);
	}

	return (pTex) ? pTex->GetID() : 0;
}

unsigned int CD3D9Renderer::UploadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return UploadToVideoMemory(data, w, h, 1, eTFSrc, eTFDst, nummipmap, eTT_2D, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
}

unsigned int CD3D9Renderer::UploadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return UploadToVideoMemory(data, w, h, 1, eTFSrc, eTFDst, nummipmap, eTT_Cube, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
}

unsigned int CD3D9Renderer::UploadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return UploadToVideoMemory(data, w, h, d, eTFSrc, eTFDst, nummipmap, eTT_3D, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
}

float CD3D9Renderer::GetGPUFrameTime()
{
	return CRenderer::GetGPUFrameTime();
}

void CD3D9Renderer::GetRenderTimes(SRenderTimes& outTimes)
{
	CRenderer::GetRenderTimes(outTimes);
}

const char* sStreamNames[] = {
	"VSF_GENERAL",
	"VSF_TANGENTS",
	"VSF_QTANGENTS",
	"VSF_HWSKIN_INFO",
	"VSF_VERTEX_VELOCITY",
#if ENABLE_NORMALSTREAM_SUPPORT
	"VSF_NORMALS"
#endif
};

void CD3D9Renderer::GetLogVBuffers()
{
	CRenderMesh* pRM = NULL;
	int nNums = 0;
	AUTO_LOCK(CRenderMesh::m_sLinkLock);
	for (util::list<CRenderMesh>* iter = CRenderMesh::s_MeshList.next; iter != &CRenderMesh::s_MeshList; iter = iter->next)
	{
		int nTotal = 0;
		string final;
		char tmp[128];

		static_assert(CRY_ARRAY_COUNT(sStreamNames) == VSF_NUM, "Invalid array size!");
		for (int i = 0; i < VSF_NUM; i++)
		{
			int nSize = iter->item<& CRenderMesh::m_Chain>()->GetStreamStride(i);

			cry_sprintf(tmp, "| %s | %d ", sStreamNames[i], nSize);
			final += tmp;
			nTotal += nSize;
		}
		if (nTotal)
			CryLog("%s | Total | %d %s", pRM->m_sSource.c_str(), nTotal, final.c_str());
		nNums++;
	}
}

extern CryCriticalSection m_sREResLock;

void CD3D9Renderer::GetMemoryUsage(ICrySizer* Sizer)
{
	assert(Sizer);

	{
		SIZER_COMPONENT_NAME(Sizer, "CRenderer");

		CRenderer::GetMemoryUsage(Sizer);
	}

#ifndef _LIB // Only when compiling as dynamic library
	{
		//SIZER_COMPONENT_NAME(pSizer,"Strings");
		//pSizer->AddObject( (this+1),string::_usedMemory(0) );
	}
	{
		SIZER_COMPONENT_NAME(Sizer, "STL Allocator Waste");
		CryModuleMemoryInfo meminfo;
		ZeroStruct(meminfo);
		CryGetMemoryInfoForModule(&meminfo);
		Sizer->AddObject((this + 2), meminfo.STL_wasted);
	}
#endif

	{
		SIZER_COMPONENT_NAME(Sizer, "Renderer dynamic");

		Sizer->AddObject(this, sizeof(CD3D9Renderer));

		GetMemoryUsageParticleREs(Sizer);
	}
#if defined(ENABLE_RENDER_AUX_GEOM)
	{
		SIZER_COMPONENT_NAME(Sizer, "Renderer Aux Geometries");
		Sizer->AddObject(m_pRenderAuxGeomD3D);
	}
#endif
	{
		SIZER_COMPONENT_NAME(Sizer, "Renderer CryName");
		static CCryNameR sName;
		Sizer->AddObject(&sName, CCryNameR::GetMemoryUsage());
	}

	{
		SIZER_COMPONENT_NAME(Sizer, "Shaders");
		CCryNameTSCRC Name = CShader::mfGetClassName();
		SResourceContainer* pRL = NULL;
		{
			SIZER_COMPONENT_NAME(Sizer, "Shader manager");
			Sizer->AddObject(m_cEF);
		}
		{
			SIZER_COMPONENT_NAME(Sizer, "Shader resources");
			Sizer->AddObject(CShader::s_ShaderResources_known);
		}
		{
			CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

			SIZER_COMPONENT_NAME(Sizer, "HW Shaders");

			Name = CHWShader::mfGetClassName(eHWSC_Vertex);
			pRL = CBaseResource::GetResourcesForClass(Name);
			Sizer->AddObject(pRL);

			Name = CHWShader::mfGetClassName(eHWSC_Pixel);
			pRL = CBaseResource::GetResourcesForClass(Name);
			Sizer->AddObject(pRL);
		}

		{
			SIZER_COMPONENT_NAME(Sizer, "Shared Shader Parameters");
			Sizer->AddObject(CGParamManager::s_Groups);
			Sizer->AddObject(CGParamManager::s_Pools);
		}
		{
			SIZER_COMPONENT_NAME(Sizer, "Light styles");
			Sizer->AddObject(CLightStyle::s_LStyles);
		}
		{
			CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

			SIZER_COMPONENT_NAME(Sizer, "SResourceContainer");
			Name = CShader::mfGetClassName();
			pRL = CBaseResource::GetResourcesForClass(Name);
			Sizer->AddObject(pRL);
		}
	}
	{
		SIZER_COMPONENT_NAME(Sizer, "Mesh");
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::s_MeshList.next; iter != &CRenderMesh::s_MeshList; iter = iter->next)
		{
			CRenderMesh* pRM = iter->item<& CRenderMesh::m_Chain>();
			pRM->m_sResLock.Lock();
			pRM->GetMemoryUsage(Sizer);
			if (pRM->_GetVertexContainer() != pRM)
				pRM->_GetVertexContainer()->GetMemoryUsage(Sizer);
			pRM->m_sResLock.Unlock();
		}
	}
	{
		SIZER_COMPONENT_NAME(Sizer, "Render elements");

		AUTO_LOCK(CRenderElement::s_accessLock);
		CRenderElement* pRE = CRenderElement::s_RootGlobal.m_NextGlobal;
		while (pRE != &CRenderElement::s_RootGlobal)
		{
			Sizer->AddObject(pRE);
			pRE = pRE->m_NextGlobal;
		}
	}
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);

		SIZER_COMPONENT_NAME(Sizer, "Texture Objects");
		SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
		if (pRL)
		{
			ResourcesMapItor itor;
			for (itor = pRL->m_RMap.begin(); itor != pRL->m_RMap.end(); itor++)
			{
				CTexture* tp = (CTexture*)itor->second;
				if (!tp || tp->IsNoTexture())
					continue;
				tp->GetMemoryUsage(Sizer);
			}
		}
	}
	CTexture::s_pPoolMgr->GetMemoryUsage(Sizer);
}

void CD3D9Renderer::SetWindowIconCVar(ICVar* pVar)
{
	gRenDev->SetWindowIcon(pVar->GetString());
}

void CD3D9Renderer::SetMouseCursorIconCVar(ICVar* pVar)
{
	gEnv->pHardwareMouse->SetCursor(pVar->GetString());
}

IRenderAuxGeom* CD3D9Renderer::GetIRenderAuxGeom()
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	// Separate aux geometry primitive for render thread
	if (m_pRT->IsRenderThread())
		return &m_renderThreadAuxGeom;

	return m_currentAuxGeomCBCollector->Get(0);
#endif
	return &m_renderAuxGeomNull;
}

IRenderAuxGeom* CD3D9Renderer::GetOrCreateIRenderAuxGeom(const CCamera* pCustomCamera)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	auto auxGeom = m_auxGeomCBPool.GetOrCreateOneElement();

	bool usesDefaultCamera = true;
	if (pCustomCamera)
		usesDefaultCamera = false;

	auxGeom->SetUsingCustomCamera(usesDefaultCamera);
	auxGeom->SetCamera(!usesDefaultCamera ? *pCustomCamera : m_currentAuxGeomCBCollector->GetCamera());

	return auxGeom;
#else
	return nullptr;
#endif
}

void CD3D9Renderer::UpdateAuxDefaultCamera(const CCamera & systemCamera)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_currentAuxGeomCBCollector->SetDefaultCamera(systemCamera);
#endif
}

void CD3D9Renderer::DeleteAuxGeom(IRenderAuxGeom* pRenderAuxGeom)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_auxGeomCBPool.ReturnToPool(static_cast<CAuxGeomCB*>(pRenderAuxGeom));
#endif
}

void CD3D9Renderer::SubmitAuxGeom(IRenderAuxGeom* pIRenderAuxGeom, bool merge)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	if (merge)
	{
		m_currentAuxGeomCBCollector->Get(0)->Merge(static_cast<const CAuxGeomCB*>(pIRenderAuxGeom));
	}
	else
	{
		m_currentAuxGeomCBCollector->Add(static_cast<CAuxGeomCB*>(pIRenderAuxGeom));
	}
#endif
}

void CD3D9Renderer::DeleteAuxGeomCBs()
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_auxGeomCBPool.ShutDown();
#endif
}

void CD3D9Renderer::SetCurrentAuxGeomCollector(CAuxGeomCBCollector* auxGeomCollector)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_currentAuxGeomCBCollector = auxGeomCollector;
	gEnv->pAuxGeomRenderer = m_currentAuxGeomCBCollector->Get(0);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

CAuxGeomCBCollector* CD3D9Renderer::GetOrCreateAuxGeomCollector(const CCamera &defaultCamera)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	auto p = m_auxGeometryCollectorPool.GetOrCreateOneElement();
	p->SetDefaultCamera(defaultCamera);
	return p;
#else
	return nullptr;
#endif
}

void CD3D9Renderer::ReturnAuxGeomCollector(CAuxGeomCBCollector* auxGeomCollector)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_auxGeometryCollectorPool.ReturnToPool(auxGeomCollector);
#endif
}

void CD3D9Renderer::DeleteAuxGeomCollectors()
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	m_auxGeometryCollectorPool.ShutDown();
#endif
}

IColorGradingController* CD3D9Renderer::GetIColorGradingController()
{
	return m_pColorGradingControllerD3D;
}

IStereoRenderer* CD3D9Renderer::GetIStereoRenderer() const
{
	return m_pStereoRenderer;
}

bool CD3D9Renderer::IsStereoEnabled() const
{
	return m_pStereoRenderer != nullptr ? m_pStereoRenderer->IsStereoEnabled() : false;
}

const RPProfilerStats* CD3D9Renderer::GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread /*= true */)
{
	return m_pPipelineProfiler ? &m_pPipelineProfiler->GetBasicStats(eStat, bCalledFromMainThread ? gRenDev->GetMainThreadID() : gRenDev->GetRenderThreadID()) : nullptr;	
}

const RPProfilerStats* CD3D9Renderer::GetRPPStatsArray(bool bCalledFromMainThread /*= true */)
{
	return m_pPipelineProfiler ? m_pPipelineProfiler->GetBasicStatsArray(bCalledFromMainThread ? gRenDev->GetMainThreadID() : gRenDev->GetRenderThreadID()) : nullptr;
}

const DynArray<RPProfilerDetailedStats>* CD3D9Renderer::GetRPPDetailedStatsArray(bool bCalledFromMainThread /*= true */)
{
	return m_pPipelineProfiler ? m_pPipelineProfiler->GetDetailedStatsArray(bCalledFromMainThread ? gRenDev->GetMainThreadID() : gRenDev->GetRenderThreadID()) : nullptr;
}

int CD3D9Renderer::GetPolygonCountByType(uint32 EFSList, EVertexCostTypes vct, uint32 z, bool bCalledFromMainThread /*= true*/)
{
#if defined(ENABLE_PROFILING_CODE)
	return m_frameRenderStats[bCalledFromMainThread ? gRenDev->GetMainThreadID() : gRenDev->GetRenderThreadID()].m_nPolygonsByTypes[EFSList][vct][z];
#else
	return 0;
#endif
}

//====================================================================

void CD3D9Renderer::PostLevelUnload()
{
	if (m_pRT)
	{
		ExecuteRenderThreadCommand( []{
			CTexture::RT_FlushStreaming(false);

			// reset post effects
			if (gRenDev->m_pPostProcessMgr)
				gRenDev->m_pPostProcessMgr->Reset(false);
		}, ERenderCommandFlags::FlushAndWait );
	}

#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		m_pRenderAuxGeomD3D->FreeMemory();
#endif

	CPoissonDiskGen::FreeMemory();
	if (m_pColorGradingControllerD3D)
	{
		m_pColorGradingControllerD3D->FreeMemory();
	}

	CDynTextureSourceLayerActivator::ReleaseData();

	g_shaderBucketAllocator.cleanup();
	g_shaderGeneralHeap->Cleanup();
}

void CD3D9Renderer::DebugShowRenderTarget()
{
	if (m_pGraphicsPipeline)
	{
		m_pGraphicsPipeline->GetDebugRenderTargetsStage()->Execute();
	}
}

//====================================================================

ILog* iLog;
IConsole* iConsole;
ITimer* iTimer;
ISystem* iSystem;

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListener_Render : public ISystemEventListener
{
public:
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		static bool bInside = false;
		if (bInside)
			return;
		bInside = true;
		switch (event)
		{
		case ESYSTEM_EVENT_GAME_POST_INIT:
			{

			}
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_RESUME_GAME:
		case ESYSTEM_EVENT_LEVEL_LOAD_PREPARE:
			if (gRenDev && gRenDev->m_pRT)
			{
				gRenDev->m_pRT->FlushAndWait();
			}
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				if (gRenDev)
				{
					gRenDev->m_cEF.m_bActivated = false;
					gRenDev->m_bEndLevelLoading = false;
					if (!(iConsole->GetCVar("g_asynclevelload")->GetIVal()))
					{
						gRenDev->m_bStartLevelLoading = true;
					}
					gRenDev->m_bInLevel = true;
					if (gRenDev->m_pRT)
					{
						gRenDev->m_pRT->m_fTimeIdleDuringLoading = 0;
						gRenDev->m_pRT->m_fTimeBusyDuringLoading = 0;
					}
				}
				if (CRenderer::CV_r_texpostponeloading)
					CTexture::s_bPrecachePhase = true;

				CResFile::m_nMaxOpenResFiles = MAX_OPEN_RESFILES * 2;
				SShaderBin::s_nMaxFXBinCache = MAX_FXBIN_CACHE * 2;
			}
			break;
		case ESYSTEM_EVENT_LEVEL_LOAD_END:
			gRenDev->m_bStartLevelLoading = false;
			gRenDev->m_bEndLevelLoading = true;
			gRenDev->m_nFrameLoad++;
			{
				gRenDev->RefreshSystemShaders();
				// make sure all commands have been properly processes before leaving the level loading
				if (gRenDev->m_pRT)
					gRenDev->m_pRT->FlushAndWait();

				g_shaderBucketAllocator.cleanup();
				g_shaderGeneralHeap->Cleanup();
			}
			break;

		case ESYSTEM_EVENT_LEVEL_PRECACHE_START:
			{
				CTexture::s_bPrestreamPhase = true;
			}
			break;

		case ESYSTEM_EVENT_LEVEL_PRECACHE_END:
			{
				CTexture::s_bPrestreamPhase = false;
			}
			break;

		case ESYSTEM_EVENT_LEVEL_UNLOAD:
			{
				gRenDev->m_bInLevel = false;
			}
			break;

		case ESYSTEM_EVENT_LEVEL_POST_UNLOAD:
			{
				gRenDev->PostLevelUnload();
				break;
			}
		case ESYSTEM_EVENT_RESIZE:
			break;
		case ESYSTEM_EVENT_ACTIVATE:
#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
			gcpRendD3D->DevInfo().OnActivate(wparam, lparam);
#endif
			break;
		case ESYSTEM_EVENT_CHANGE_FOCUS:
			break;
		case ESYSTEM_EVENT_GAME_POST_INIT_DONE:
			if (gRenDev && !gRenDev->IsEditorMode())
				EnableCloseButton(gRenDev->GetHWND(), true);
			break;
		}
		bInside = false;
	}
};

static CSystemEventListener_Render g_system_event_listener_render;

extern "C" DLL_EXPORT IRenderer * CreateCryRenderInterface(ISystem * pSystem)
{
	iConsole = gEnv->pConsole;
	iLog = gEnv->pLog;
	iTimer = gEnv->pTimer;
	iSystem = gEnv->pSystem;

	gcpRendD3D->InitRenderer();

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	srand((uint32) li.QuadPart);

	iSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_render, "CSystemEventListener_Render");
	return gRenDev;
}

class CEngineModule_CryRenderer : public IRendererEngineModule
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IRendererEngineModule)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CEngineModule_CryRenderer, "EngineModule_CryRenderer", "540c91a7-338e-41d3-acee-ac9d55614450"_cry_guid)

	virtual ~CEngineModule_CryRenderer() 
	{
		SAFE_RELEASE(gEnv->pRenderer);
	}

	virtual const char* GetName() const override     { return "CryRenderer"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;
		env.pRenderer = CreateCryRenderInterface(pSystem);
		return env.pRenderer != 0;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryRenderer)

//=========================================================================================
void CD3D9Renderer::LockParticleVideoMemory(int frameId)
{
	CRY_PROFILE_REGION(PROFILE_RENDERER, "LockParticleVideoMemory");

	gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet().Lock(frameId);
}

void CD3D9Renderer::UnLockParticleVideoMemory(int frameId)
{
	gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet().Unlock(frameId);
}

void CD3D9Renderer::InsertParticleVideoDataFence(int frameId)
{
	gcpRendD3D.GetGraphicsPipeline().GetParticleBufferSet().SetFence(frameId);
}
//================================================================================================================================

void CD3D9Renderer::ActivateLayer(const char* pLayerName, bool activate)
{
	CDynTextureSourceLayerActivator::ActivateLayer(pLayerName, activate);
}


void CD3D9Renderer::RegisterDeviceWrapperHook(ICryDeviceWrapperHook* pDeviceWrapperHook)
{
	GetDevice().RegisterHook(pDeviceWrapperHook);
	GetDeviceContext().RegisterHook(pDeviceWrapperHook);
#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	GetPerformanceDevice().RegisterHook(pDeviceWrapperHook);
	GetPerformanceDeviceContext().RegisterHook(pDeviceWrapperHook);
#endif // DEVICE_SUPPORTS_PERFORMANCE_DEVICE
}

void CD3D9Renderer::UnregisterDeviceWrapperHook(const char* pDeviceHookName)
{
	GetDevice().UnregisterHook(pDeviceHookName);
	GetDeviceContext().UnregisterHook(pDeviceHookName);
#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	GetPerformanceDevice().UnregisterHook(pDeviceHookName);
	GetPerformanceDeviceContext().UnregisterHook(pDeviceHookName);
#endif
}

#ifdef SUPPORT_HW_MOUSE_CURSOR
IHWMouseCursor* CD3D9Renderer::GetIHWMouseCursor()
{
	#if CRY_PLATFORM_ORBIS
	return DXOrbis::m_pSwapChain ? DXOrbis::m_pSwapChain->GetIHWMouseCursor() : NULL;
	#else
		#error Need implementation of IHWMouseCursor
	#endif
}
#endif

IGraphicsDeviceConstantBufferPtr CD3D9Renderer::CreateGraphiceDeviceConstantBuffer()
{
	return new CGraphicsDeviceConstantBuffer;
}

void CD3D9Renderer::BeginRenderDocCapture()
{
#if defined (CRY_PLATFORM_WINDOWS)
	typedef void (__cdecl * pRENDERDOC_StartFrameCapture)(HWND wndHandle);

	HMODULE rdocDll = GetModuleHandleA("renderdoc.dll");
	if (rdocDll != NULL)
	{
		if (pRENDERDOC_StartFrameCapture fpStartFrameCap = (pRENDERDOC_StartFrameCapture)GetProcAddress(rdocDll, "RENDERDOC_StartFrameCapture"))
		{
			auto *pDC = gcpRendD3D->GetActiveDisplayContext();
			CRY_ASSERT(pDC->IsSwapChainBacked());
			auto swapDC = static_cast<CSwapChainBackedRenderDisplayContext*>(pDC);

			DXGI_SWAP_CHAIN_DESC desc = swapDC->GetSwapChain().GetDesc();
			fpStartFrameCap(desc.OutputWindow);
			CryLogAlways("Started RenderDoc capture");
		}
	}
#endif
}

void CD3D9Renderer::EndRenderDocCapture()
{
#if defined (CRY_PLATFORM_WINDOWS)
	typedef uint32 (__cdecl * pRENDERDOC_EndFrameCapture)(HWND wndHandle);

	HMODULE rdocDll = GetModuleHandleA("renderdoc.dll");
	if (rdocDll != NULL)
	{
		if (pRENDERDOC_EndFrameCapture fpEndFrameCap = (pRENDERDOC_EndFrameCapture)GetProcAddress(rdocDll, "RENDERDOC_EndFrameCapture"))
		{
			auto *pDC = gcpRendD3D->GetActiveDisplayContext();
			CRY_ASSERT(pDC->IsSwapChainBacked());
			auto swapDC = static_cast<CSwapChainBackedRenderDisplayContext*>(pDC);

			DXGI_SWAP_CHAIN_DESC desc = swapDC->GetSwapChain().GetDesc();
			fpEndFrameCap(desc.OutputWindow);
			CryLogAlways("Finished RenderDoc capture");
		}
	}
#endif
}

#include "GraphicsPipeline/ComputeSkinning.h"

compute_skinning::IComputeSkinningStorage* CD3D9Renderer::GetComputeSkinningStorage()
{
	return &GetGraphicsPipeline().GetComputeSkinningStage()->GetStorage();
}

#pragma warning(pop)
