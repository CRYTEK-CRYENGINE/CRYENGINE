// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
#include "D3DVolumetricClouds.h"

#include <CryMovie/AnimKey.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>
#include <CrySystem/VR/IHMDManager.h>

#ifdef ENABLE_BENCHMARK_SENSOR
	#include <IBenchmarkFramework.h>
	#include <IBenchmarkRendererSensorManager.h>
#endif
#include "Gpu/Particles/GpuParticleManager.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#pragma warning(disable: 4244)

#if CRY_PLATFORM_WINDOWS && !defined(OPENGL)
	#include <D3Dcompiler.h>
#endif

#if CRY_PLATFORM_MOBILE
	#include <SDL.h>
#endif

#if RENDERER_ENABLE_BREAK_ON_ERROR
namespace detail
{
const char* ToString(long const hr)
{
	#if !CRY_PLATFORM_DURANGO
	switch (hr)
	{
	case D3DOK_NOAUTOGEN:
		return "D3DOK_NOAUTOGEN This is a success code. However, the autogeneration of mipmaps is not supported for this format. This means that resource creation will succeed but the mipmap levels will not be automatically generated";
	case D3DERR_CONFLICTINGRENDERSTATE:
		return "D3DERR_CONFLICTINGRENDERSTATE The currently set render states cannot be used together";
	case D3DERR_CONFLICTINGTEXTUREFILTER:
		return "D3DERR_CONFLICTINGTEXTUREFILTER The current texture filters cannot be used together";
	case D3DERR_CONFLICTINGTEXTUREPALETTE:
		return "D3DERR_CONFLICTINGTEXTUREPALETTE The current textures cannot be used simultaneously.";
	case D3DERR_DEVICEHUNG:
		return "D3DERR_DEVICEHUNG The device that returned this code caused the hardware adapter to be reset by the OS. Most applications should destroy the device and quit. Applications that must continue should destroy all video memory objects (surfaces, textures, state blocks etc) and call Reset() to put the device in a default state. If the application then continues rendering in the same way, the device will return to this state";

	case D3DERR_DEVICELOST:
		return "D3DERR_DEVICELOST The device has been lost but cannot be reset at this time. Therefore, rendering is not possible.A Direct 3D device object other than the one that returned this code caused the hardware adapter to be reset by the OS. Delete all video memory objects (surfaces, textures, state blocks) and call Reset() to return the device to a default state. If the application continues rendering without a reset, the rendering calls will succeed.";
	case D3DERR_DEVICENOTRESET:
		return "D3DERR_DEVICENOTRESET The device has been lost but can be reset at this time.";
	case D3DERR_DEVICEREMOVED:
		return "D3DERR_DEVICEREMOVED The hardware adapter has been removed. Application must destroy the device, do enumeration of adapters and create another Direct3D device. If application continues rendering without calling Reset, the rendering calls will succeed";

	case D3DERR_DRIVERINTERNALERROR:
		return "D3DERR_DRIVERINTERNALERROR Internal driver error. Applications should destroy and recreate the device when receiving this error. For hints on debugging this error, see Driver Internal Errors (Direct3D 9).";
	case D3DERR_DRIVERINVALIDCALL:
		return "D3DERR_DRIVERINVALIDCALL Not used.";
	case D3DERR_INVALIDCALL:
		return "D3DERR_INVALIDCALL The method call is invalid. For example, a method's parameter may not be a valid pointer.";
	case D3DERR_INVALIDDEVICE:
		return "D3DERR_INVALIDDEVICE The requested device type is not valid.";
	case D3DERR_MOREDATA:
		return "D3DERR_MOREDATA There is more data available than the specified buffer size can hold.";
	case D3DERR_NOTAVAILABLE:
		return "D3DERR_NOTAVAILABLE This device does not support the queried technique.";
	case D3DERR_NOTFOUND:
		return "D3DERR_NOTFOUND The requested item was not found.";
	case D3D_OK:
		return "D3D_OK No error occurred.";
	case D3DERR_OUTOFVIDEOMEMORY:
		return "D3DERR_OUTOFVIDEOMEMORY Direct3D does not have enough display memory to perform the operation. The device is using more resources in a single scene than can fit simultaneously into video memory. Present, PresentEx, or CheckDeviceState can return this error. Recovery is similar to D3DERR_DEVICEHUNG, though the application may want to reduce its per-frame memory usage as well to avoid having the error recur.";
	case D3DERR_TOOMANYOPERATIONS:
		return "D3DERR_TOOMANYOPERATIONS The application is requesting more texture-filtering operations than the device supports.";
	case D3DERR_UNSUPPORTEDALPHAARG:
		return "D3DERR_UNSUPPORTEDALPHAARG The device does not support a specified texture-blending argument for the alpha channel.";
	case D3DERR_UNSUPPORTEDALPHAOPERATION:
		return "D3DERR_UNSUPPORTEDALPHAOPERATION The device does not support a specified texture-blending operation for the alpha channel.";
	case D3DERR_UNSUPPORTEDCOLORARG:
		return "D3DERR_UNSUPPORTEDCOLORARG The device does not support a specified texture-blending argument for color values.";
	case D3DERR_UNSUPPORTEDCOLOROPERATION:
		return "D3DERR_UNSUPPORTEDCOLOROPERATION The device does not support a specified texture-blending operation for color values.";
	case D3DERR_UNSUPPORTEDFACTORVALUE:
		return "D3DERR_UNSUPPORTEDFACTORVALUE The device does not support the specified texture factor value. Not used; provided only to support older drivers.";
	case D3DERR_UNSUPPORTEDTEXTUREFILTER:
		return "D3DERR_UNSUPPORTEDTEXTUREFILTER The device does not support the specified texture filter.";
	case D3DERR_WASSTILLDRAWING:
		return "D3DERR_WASSTILLDRAWING The previous blit operation that is transferring information to or from this surface is incomplete.";
	case D3DERR_WRONGTEXTUREFORMAT:
		return "D3DERR_WRONGTEXTUREFORMAT The pixel format of the texture surface is not valid.";
	case E_FAIL:
		return "E_FAIL An undetermined error occurred inside the Direct3D subsystem.";
	case E_INVALIDARG:
		return "E_INVALIDARG An invalid parameter was passed to the returning function.";
	case E_NOINTERFACE:
		return "E_NOINTERFACE No object interface is available.";
	case E_NOTIMPL:
		return "E_NOTIMPL Not implemented.";
	case E_OUTOFMEMORY:
		return "E_OUTOFMEMORY Direct3D could not allocate sufficient memory to complete the call.";

	case D3DERR_UNSUPPORTEDOVERLAY:
		return "D3DERR_UNSUPPORTEDOVERLAY The device does not support overlay for the specified size or display mode.";
	case D3DERR_UNSUPPORTEDOVERLAYFORMAT:
		return "D3DERR_UNSUPPORTEDOVERLAYFORMAT The device does not support overlay for the specified surface format.";
	case D3DERR_CANNOTPROTECTCONTENT:
		return "D3DERR_CANNOTPROTECTCONTENT The specified content cannot be protected";
	case D3DERR_UNSUPPORTEDCRYPTO:
		return "D3DERR_UNSUPPORTEDCRYPTO The specified cryptographic algorithm is not supported.";
	default:
		break;
	}
	#endif
	return "Unknown HRESULT CODE!";
}
bool CheckHResult(long const hr
                  , bool breakOnError
                  , const char* file
                  , const int line)
{
	IF (hr == S_OK, 1)
	{
		return true;
	}
	CryLogAlways("[%s:%d] d3d error: '%s'", file, line, ToString(hr));
	IF (breakOnError, 0)
	{
		__debugbreak();
	}
	return false;
}
}
#endif

void CD3D9Renderer::LimitFramerate(const int maxFPS, const bool bUseSleep)
{
	FRAME_PROFILER("RT_FRAME_CAP", gEnv->pSystem, PROFILE_RENDERER);

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

CCryNameTSCRC CHWShader::s_sClassNameVS = CCryNameTSCRC("CHWShader_VS");
CCryNameTSCRC CHWShader::s_sClassNamePS = CCryNameTSCRC("CHWShader_PS");

CCryNameTSCRC CShader::s_sClassName = CCryNameTSCRC("CShader");

CD3D9Renderer gcpRendD3D;

extern CTexture* gTexture2;

bool QueryIsFullscreen()
{
	return gcpRendD3D->IsFullscreen();
}

unsigned int CD3D9Renderer::GetNumBackBufferIndices(const DXGI_SWAP_CHAIN_DESC& scDesc)
{
	unsigned indices = 1;

#ifdef CRY_USE_DX12
	indices = scDesc.BufferCount;
#endif

	return indices;
}

unsigned int CD3D9Renderer::GetCurrentBackBufferIndex(IDXGISwapChain* pSwapChain)
{
	unsigned index = 0;

#ifdef CRY_USE_DX12
	IDXGISwapChain3* spSwapChain3;
	pSwapChain->QueryInterface(&spSwapChain3);
	if (spSwapChain3)
	{
		index = spSwapChain3->GetCurrentBackBufferIndex();
		spSwapChain3->Release();
	}
#endif

	return index;
}

void CD3D9Renderer::ReleaseBackBuffers()
{
	m_pBackBuffer = nullptr;
	std::fill(m_pBackBuffers.begin(), m_pBackBuffers.end(), nullptr);
}

uint32 CD3D9Renderer::GetOrCreateBlendState(const D3D11_BLEND_DESC& desc)
{
	uint32 i;
	HRESULT hr = S_OK;
	uint64 nHash = SStateBlend::GetHash(desc);
	for (i = 0; i < m_StatesBL.Num(); i++)
	{
		if (m_StatesBL[i].nHashVal == nHash)
			break;
	}
	if (i == m_StatesBL.Num())
	{
		SStateBlend* pState = m_StatesBL.AddIndex(1);
		pState->Desc = desc;
		pState->nHashVal = nHash;
		hr = GetDevice().CreateBlendState(&pState->Desc, &pState->pState);
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

bool CD3D9Renderer::SetBlendState(const SStateBlend* pNewState)
{
	uint32 bsIndex = GetOrCreateBlendState(pNewState->Desc);
	if (bsIndex == uint32(-1))
		return false;

	if (bsIndex != m_nCurStateBL)
	{
		m_nCurStateBL = bsIndex;
		m_DevMan.SetBlendState(m_StatesBL[bsIndex].pState, 0, 0xffffffff);
	}
	return true;
}

uint32 CD3D9Renderer::GetOrCreateRasterState(const D3D11_RASTERIZER_DESC& rasterizerDec, const bool bAllowMSAA /*= true*/)
{
	uint32 i;
	HRESULT hr = S_OK;

	D3D11_RASTERIZER_DESC desc = rasterizerDec;

	BOOL bMSAA = bAllowMSAA && m_RP.m_MSAAData.Type > 1;
	if (bMSAA != desc.MultisampleEnable)
		desc.MultisampleEnable = bMSAA;

	uint32 nHash = SStateRaster::GetHash(desc);
	uint64 nValuesHash = SStateRaster::GetValuesHash(desc);
	for (i = 0; i < m_StatesRS.Num(); i++)
	{
		if (m_StatesRS[i].nHashVal == nHash && m_StatesRS[i].nValuesHash == nValuesHash)
			break;
	}
	if (i == m_StatesRS.Num())
	{
		SStateRaster* pState = m_StatesRS.AddIndex(1);
		pState->Desc = desc;
		pState->nHashVal = nHash;
		pState->nValuesHash = nValuesHash;
		hr = GetDevice().CreateRasterizerState(&pState->Desc, &pState->pState);
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

bool CD3D9Renderer::SetRasterState(const SStateRaster* pNewState, const bool bAllowMSAA /*= true*/)
{
	uint32 rsIndex = GetOrCreateRasterState(pNewState->Desc, bAllowMSAA);
	if (rsIndex == uint32(-1))
		return false;

	if (rsIndex != m_nCurStateRS)
	{
		m_nCurStateRS = rsIndex;

		m_DevMan.SetRasterState(m_StatesRS[rsIndex].pState);
	}
	return true;
}

uint32 CD3D9Renderer::GetOrCreateDepthState(const D3D11_DEPTH_STENCIL_DESC& desc)
{
	uint32 i;
	HRESULT hr = S_OK;
	uint64 nHash = SStateDepth::GetHash(desc);
	const int kNumStates = m_StatesDP.Num();
	for (i = 0; i < kNumStates; i++)
	{
		if (m_StatesDP[i].nHashVal == nHash)
			break;
	}
	if (i == kNumStates)
	{
		SStateDepth* pState = m_StatesDP.AddIndex(1);
		pState->Desc = desc;
		pState->nHashVal = nHash;
		hr = GetDevice().CreateDepthStencilState(&pState->Desc, &pState->pState);
		assert(SUCCEEDED(hr));
	}
	return SUCCEEDED(hr) ? i : uint32(-1);
}

bool CD3D9Renderer::SetDepthState(const SStateDepth* pNewState, uint8 newStencRef)
{
	uint32 dsIndex = GetOrCreateDepthState(pNewState->Desc);
	if (dsIndex == uint32(-1))
		return false;

	if (dsIndex != m_nCurStateDP || m_nCurStencRef != newStencRef)
	{
		m_nCurStateDP = dsIndex;
		m_nCurStencRef = newStencRef;
		m_DevMan.SetDepthStencilState(m_StatesDP[dsIndex].pState, newStencRef);
	}

	return true;
}

// Direct 3D console variables
CD3D9Renderer::CD3D9Renderer()
	: m_nAsyncDeviceState(0)
	, m_DeviceOwningthreadID(0)
#if CRY_PLATFORM_WINDOWS
	, m_bDisplayChanged(false)
	, m_nConnectedMonitors(1)
#endif // CRY_PLATFORM_WINDOWS
{
	m_bDraw2dImageStretchMode = false;

	CreateDeferredUnitBox(m_arrDeferredInds, m_arrDeferredVerts);
#if CRY_PLATFORM_DURANGO
	m_occlusionGPUData[0] = m_occlusionGPUData[1] = NULL;
	m_occlusionReadBackTexture[0] = m_occlusionReadBackTexture[1] = NULL;
#endif
}

void CD3D9Renderer::InitRenderer()
{
	CRenderer::InitRenderer();
	m_windowParametersOverridden = false;
	memset(m_arrWireFrameStack, 0, sizeof(m_arrWireFrameStack));
	m_nWireFrameStack = 0;

	m_uLastBlendFlagsPassGroup = 0xFFFFFFFF;
	m_fAdaptedSceneScaleLBuffer = 1.0f;
	m_bInitialized = false;
	gRenDev = this;

	m_pStereoRenderer = new CD3DStereoRenderer(*this, (EStereoDevice)CRenderer::CV_r_StereoDevice);
	m_bDualStereoSupport = CRenderer::CV_r_StereoDevice > 0;
	m_pGraphicsPipeline = new CStandardGraphicsPipeline();
	m_pTiledShading = new CTiledShading();
	m_pVolumetricCloudMan = new CVolumetricCloudManager();

	m_pPipelineProfiler = NULL;
#ifdef USE_PIX_DURANGO
	m_pPixPerf = NULL;
#endif

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

	m_nmInstancingParams = CCryNameR("_InstancingParams");
	m_nmInstancingData = CCryNameR("_g_InstData");

	m_nCurStateBL = (uint32) - 1;
	m_nCurStateRS = (uint32) - 1;
	m_nCurStateDP = (uint32) - 1;
	m_CurTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_dwPresentStatus = 0;

	m_hWnd = NULL;
#if CRY_PLATFORM_WINDOWS
	m_hIconBig = NULL;
	m_hIconSmall = NULL;
#endif
	m_dwCreateFlags = 0L;

	m_frameFenceCounter = 0;
	m_completedFrameFenceCounter = 0;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		m_frameFences[i] = NULL;

	m_PrevFenceOcclusionReady = m_FenceOcclusionReady = 0L;
	m_numOcclusionDownsampleStages = 1;
	m_occlusionDownsampleSizeX = 0;
	m_occlusionDownsampleSizeY = 0;
	m_occlusionRequestedSizeX = 0;
	m_occlusionRequestedSizeY = 0;
	m_occlusionSourceSizeX = 0;
	m_occlusionSourceSizeY = 0;

	m_occlusionLastProj.SetIdentity();
	m_occlusionViewProj.SetIdentity();
	for (int i = 0; i < 4; i++)
		m_occlusionViewProjBuffer[i].SetIdentity();

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

	m_2dImages.reserve(32);

	//memset(&m_LumGamma, 0, sizeof(m_LumGamma));
	//memset(&m_LumHistogram, 0, sizeof(m_LumHistogram));

	m_strDeviceStats[0] = 0;

	m_Features = RFT_SUPPORTZBIAS;

#if defined(ENABLE_RENDER_AUX_GEOM)
	m_pRenderAuxGeomD3D = 0;
	if (CV_r_enableauxgeom)
		m_pRenderAuxGeomD3D = CRenderAuxGeomD3D::Create(*this);
#endif
	m_pColorGradingControllerD3D = new CColorGradingControllerD3D(this);

	m_NewViewport.fMinZ = 0;
	m_NewViewport.fMaxZ = 1;

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

#if defined(INCLUDE_SCALEFORM_SDK) || defined(CRY_FEATURE_SCALEFORM_HELPER)
	SF_CreateResources();
	assert(m_pSFResD3D);
#endif

	m_OcclQueriesUsed = 0;

	m_pPostProcessMgr = 0;
	m_pWaterSimMgr = 0;

#if ENABLE_STATOSCOPE
	m_pGPUTimersDG = new CGPUTimesDG(this);
	m_pGraphicsDG = new CGraphicsDG(this);
	m_pPerformanceOverviewDG = new CPerformanceOverviewDG(this);
	gEnv->pStatoscope->RegisterDataGroup(m_pGPUTimersDG);
	gEnv->pStatoscope->RegisterDataGroup(m_pGraphicsDG);
	gEnv->pStatoscope->RegisterDataGroup(m_pPerformanceOverviewDG);
#endif

	memset(&m_RP.m_MSAAData, 0, sizeof(m_RP.m_MSAAData));
}

CD3D9Renderer::~CD3D9Renderer()
{
	//Code moved to Release
}

void CD3D9Renderer::StaticCleanup()
{
	stl::free_container(s_tempRIs);
	stl::free_container(s_tempObjects[0]);
	stl::free_container(s_tempObjects[1]);
}

void CD3D9Renderer::Release()
{
	//FreeResources(FRR_ALL);
	ShutDown();

#if ENABLE_STATOSCOPE
	gEnv->pStatoscope->UnregisterDataGroup(m_pGPUTimersDG);
	gEnv->pStatoscope->UnregisterDataGroup(m_pGraphicsDG);
	gEnv->pStatoscope->UnregisterDataGroup(m_pPerformanceOverviewDG);
	SAFE_DELETE(m_pGPUTimersDG);
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
	m_MSAA = 0;
	if (m_bFullScreen)
		SetGamma(CV_r_gamma + m_fDeltaGamma, CV_r_brightness, CV_r_contrast, false);
}

void CD3D9Renderer::ChangeViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height, bool bMainViewport)
{
	if (m_bDeviceLost)
		return;
	assert(m_CurrContext);

	m_CurrContext->m_nViewportWidth = width;
	m_CurrContext->m_nViewportHeight = height;
	m_CurrContext->m_bMainViewport = bMainViewport;

	if (bMainViewport)
	{
		const int nMaxRes = clamp_tpl(CV_r_CustomResMaxSize, 32, m_MaxTextureSize);
		if (CV_r_CustomResWidth && CV_r_CustomResHeight)
		{
			width = clamp_tpl(CV_r_CustomResWidth, 32, nMaxRes);
			height = clamp_tpl(CV_r_CustomResHeight, 32, nMaxRes);
		}
		if (CV_r_Supersampling > 1)
		{
			const int nMaxSSX = nMaxRes / width;
			const int nMaxSSY = nMaxRes / height;
			m_CurrContext->m_nSSSamplesX = clamp_tpl(CV_r_Supersampling, 1, nMaxSSX);
			m_CurrContext->m_nSSSamplesY = clamp_tpl(CV_r_Supersampling, 1, nMaxSSY);
		}
		else
		{
			m_CurrContext->m_nSSSamplesX = 1;
			m_CurrContext->m_nSSSamplesY = 1;
		}
	}

	width *= m_CurrContext->m_nSSSamplesX;
	height *= m_CurrContext->m_nSSSamplesY;

	DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;

	if (!m_CurrContext->m_pSwapChain && !m_CurrContext->m_pBackBuffer)
	{
		DXGI_SWAP_CHAIN_DESC scDesc;
		scDesc.BufferDesc.Width = width;
		scDesc.BufferDesc.Height = height;
		scDesc.BufferDesc.RefreshRate.Numerator = 0;
		scDesc.BufferDesc.RefreshRate.Denominator = 1;
		scDesc.BufferDesc.Format = fmt;
		scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		scDesc.SampleDesc.Count = 1;
		scDesc.SampleDesc.Quality = 0;

		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		scDesc.BufferCount = 1;
#if CRY_PLATFORM_ORBIS
		scDesc.OutputWindow = (__typeof__(scDesc.OutputWindow))(TRUNCATE_PTR) m_CurrContext->m_hWnd;
#else
		scDesc.OutputWindow = m_CurrContext->m_hWnd;
#endif
		scDesc.Windowed = TRUE;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scDesc.Flags = 0;

#if defined(SUPPORT_DEVICE_INFO)
		HRESULT hr = m_devInfo.Factory()->CreateSwapChain(GetDevice().GetRealDevice(), &scDesc, &m_CurrContext->m_pSwapChain);
#elif CRY_PLATFORM_ORBIS
		HRESULT hr = CCryDXOrbisGIFactory().CreateSwapChain(GetDevice().GetRealDevice(), &scDesc, &m_CurrContext->m_pSwapChain);
#elif CRY_PLATFORM_DURANGO
		// create the D3D11 swapchain here
		HRESULT hr = NULL;
		__debugbreak();
#else
	#error UNKNOWN PLATFORM TRYING TO CREATE SWAP CHAIN
#endif
		assert(SUCCEEDED(hr) && m_CurrContext->m_pSwapChain != 0);

		assert(m_CurrContext->m_pSwapChain);
		PREFAST_ASSUME(m_CurrContext->m_pSwapChain);
		m_CurrContext->m_pSwapChain->GetDesc(&scDesc);

		unsigned int numIndices = GetNumBackBufferIndices(scDesc);
		m_CurrContext->m_pBackBuffers.resize(numIndices);
		for (unsigned int b = 0; b < numIndices; ++b)
		{
			void* pBackBuf(0);
			hr = m_CurrContext->m_pSwapChain->GetBuffer(b, __uuidof(ID3D11Texture2D), &pBackBuf);
			ID3D11Texture2D* pBackBuffer = (ID3D11Texture2D*)pBackBuf;
			assert(SUCCEEDED(hr) && pBackBuffer != 0);

			D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
			rtDesc.Format = fmt;
			rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rtDesc.Texture2D.MipSlice = 0;

			D3DSurface* pBackbufferView;
			hr = GetDevice().CreateRenderTargetView(pBackBuffer, &rtDesc, &pBackbufferView);
			assert(SUCCEEDED(hr) && pBackbufferView != nullptr);

			m_CurrContext->m_pBackBuffers[b] = pBackbufferView;
			SAFE_RELEASE(pBackbufferView);

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
			char str[38] = "";
			_snprintf(str, CRY_ARRAY_COUNT(str), "Contextual Swap-Chain back buffer %d", b);
			pBackBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(str), str);
#endif

			SAFE_RELEASE(pBackBuffer);
		}

		m_CurrContext->m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_CurrContext->m_pSwapChain);
		m_CurrContext->m_pBackBuffer = m_CurrContext->m_pBackBuffers[m_CurrContext->m_pCurrentBackBufferIndex];
	}
	else if (m_CurrContext->m_Width != width || m_CurrContext->m_Height != height)
	{
		DXGI_SWAP_CHAIN_DESC scDesc;

		// Release all back-buffers of the m_CurrContext
		m_CurrContext->m_pBackBuffer = nullptr;
		std::fill(m_CurrContext->m_pBackBuffers.begin(), m_CurrContext->m_pBackBuffers.end(), nullptr);

		// In case the m_CurrContext is the active context, release the buffers stored in the renderer
		ReleaseBackBuffers();

		// Force the release of the back-buffer currently bound as a render-target (otherwise resizing the swap-chain will fail, because of outstanding reference)
		FX_SetRenderTarget(0, (D3DSurface*)nullptr, NULL);
		FX_SetActiveRenderTargets();

		HRESULT hr = m_CurrContext->m_pSwapChain->ResizeBuffers(0, width, height, fmt, 0);
		assert(SUCCEEDED(hr));

		PREFAST_ASSUME(m_CurrContext->m_pSwapChain);
		m_CurrContext->m_pSwapChain->GetDesc(&scDesc);

		unsigned int numIndices = GetNumBackBufferIndices(scDesc);
		m_CurrContext->m_pBackBuffers.resize(numIndices);
		for (unsigned int b = 0; b < numIndices; ++b)
		{
			void* pBackBuf(0);
			hr = m_CurrContext->m_pSwapChain->GetBuffer(b, __uuidof(ID3D11Texture2D), &pBackBuf);
			ID3D11Texture2D* pBackBuffer = (ID3D11Texture2D*)pBackBuf;
			assert(SUCCEEDED(hr) && pBackBuffer != 0);

			D3D11_RENDER_TARGET_VIEW_DESC rtDesc;
			rtDesc.Format = fmt;
			rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rtDesc.Texture2D.MipSlice = 0;

			D3DSurface* pBackbufferView;
			hr = GetDevice().CreateRenderTargetView(pBackBuffer, &rtDesc, &pBackbufferView);
			assert(SUCCEEDED(hr) && pBackbufferView != nullptr);

			m_CurrContext->m_pBackBuffers[b] = pBackbufferView;
			SAFE_RELEASE(pBackbufferView);

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
			char str[64] = "";
			sprintf(str, "Contextual Swap-Chain back buffer %d", b);
			pBackBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(str), str);
#endif

			SAFE_RELEASE(pBackBuffer);
		}

		m_CurrContext->m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_CurrContext->m_pSwapChain);
		m_CurrContext->m_pBackBuffer = m_CurrContext->m_pBackBuffers[m_CurrContext->m_pCurrentBackBufferIndex];
	}

	if (m_CurrContext->m_pSwapChain && m_CurrContext->m_pBackBuffer)
	{
		assert(m_nRTStackLevel[0] == 0);

		m_CurrContext->m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_CurrContext->m_pSwapChain);
		m_CurrContext->m_pBackBuffer = m_CurrContext->m_pBackBuffers[m_CurrContext->m_pCurrentBackBufferIndex];

		m_pBackBuffer = m_CurrContext->m_pBackBuffer;
		for (int i = 0; i < m_CurrContext->m_pBackBuffers.size(); ++i)
			m_pBackBuffers[i] = m_CurrContext->m_pBackBuffers[i];

		m_pCurrentBackBufferIndex = m_CurrContext->m_pCurrentBackBufferIndex;

		// Force unbinding any render-target which is from another swap-chain, by setting the m_CurrContext's back-buffer
		FX_SetRenderTarget(0, m_CurrContext->m_pBackBuffer, &m_DepthBufferNative);
		FX_SetActiveRenderTargets();
	}

	m_CurrContext->m_X = x;
	m_CurrContext->m_Y = y;
	m_CurrContext->m_Width = width;
	m_CurrContext->m_Height = height;
	m_width = m_nativeWidth = m_backbufferWidth = width;
	m_height = m_nativeHeight = m_backbufferHeight = height;

	SetCurDownscaleFactor(Vec2(1, 1));
	RT_SetViewport(x, y, width, height);
}

void CD3D9Renderer::SetFullscreenViewport()
{
	m_NewViewport.nX = 0;
	m_NewViewport.nY = 0;
	m_NewViewport.nWidth = GetWidth();
	m_NewViewport.nHeight = GetHeight();
	m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
	m_RP.m_nCommitFlags |= FC_GLOBAL_PARAMS;
	m_bViewportDirty = true;
}

void CD3D9Renderer::SetCurDownscaleFactor(Vec2 sf)
{
	m_RP.m_CurDownscaleFactor = sf;

	m_FullResRect.left = m_FullResRect.top = 0;
	m_FullResRect.right = LONG(GetWidth() * m_RP.m_CurDownscaleFactor.x);
	m_FullResRect.bottom = LONG(GetHeight() * m_RP.m_CurDownscaleFactor.y);

	m_HalfResRect.left = m_HalfResRect.top = 0;
	m_HalfResRect.right = m_FullResRect.right >> 1;
	m_HalfResRect.bottom = m_FullResRect.bottom >> 1;

	// need to update shader parameters
	m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
	m_RP.m_nCommitFlags |= FC_GLOBAL_PARAMS;
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

void CD3D9Renderer::PostMeasureOverdraw()
{
#if !defined(_RELEASE)
	if (CV_r_measureoverdraw)
	{
		gRenDev->m_cEF.mfRefreshSystemShader("Debug", CShaderMan::s_ShaderDebug);

		int iTmpX, iTmpY, iTempWidth, iTempHeight;
		GetViewport(&iTmpX, &iTmpY, &iTempWidth, &iTempHeight);
		RT_SetViewport(0, 0, m_width, m_height);
		Set2DMode(true, 1, 1);

		{
			TempDynVB<SVF_P3F_C4B_T2F> vb;
			vb.Allocate(4);
			SVF_P3F_C4B_T2F* vQuad = vb.Lock();

			vQuad[0].xyz.x = 0;
			vQuad[0].xyz.y = 0;
			vQuad[0].xyz.z = 1;
			vQuad[0].color.dcolor = (uint32) - 1;
			vQuad[0].st = Vec2(0.0f, 0.0f);

			vQuad[1].xyz.x = 1;
			vQuad[1].xyz.y = 0;
			vQuad[1].xyz.z = 1;
			vQuad[1].color.dcolor = (uint32) - 1;
			vQuad[1].st = Vec2(1.0f, 0.0f);

			vQuad[2].xyz.x = 0;
			vQuad[2].xyz.y = 1;
			vQuad[2].xyz.z = 1;
			vQuad[2].color.dcolor = (uint32) - 1;
			vQuad[2].st = Vec2(0.0f, 1.0f);

			vQuad[3].xyz.x = 1;
			vQuad[3].xyz.y = 1;
			vQuad[3].xyz.z = 1;
			vQuad[3].color.dcolor = (uint32) - 1;
			vQuad[3].st = Vec2(1.0f, 1.0f);

			vb.Unlock();
			vb.Bind(0);
			vb.Release();

			SetCullMode(R_CULL_DISABLE);
			FX_SetState(GS_NODEPTHTEST);
			CTexture::s_ptexWhite->Apply(0);
		}

		{
			uint32 nPasses;
			CShader* pSH = m_cEF.s_ShaderDebug;
			pSH->FXSetTechnique("ShowInstructions");
			pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			pSH->FXBeginPass(0);
			STexState TexStateLinear = STexState(FILTER_LINEAR, true);

			if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
			{
				STexState TexStatePoint = STexState(FILTER_POINT, true);
				FX_Commit();
				SDynTexture* pRT = new SDynTexture(m_width, m_height, eTF_R8G8B8A8, eTT_2D, FT_STATE_CLAMP, "TempDebugRT");
				if (pRT)
				{
					pRT->Update(m_width, m_height);
					PostProcessUtils().CopyScreenToTexture(pRT->m_pTexture);

					pRT->Apply(0, CTexture::GetTexState(TexStatePoint));
					CTexture::s_ptexPaletteDebug->Apply(1, CTexture::GetTexState(TexStateLinear));

					FX_DrawPrimitive(eptTriangleStrip, 0, 4);

					delete pRT;
					pRT = 0;
				}
			}
			Set2DMode(false, 1, 1);

			int nX = 800 - 100 + 2;
			int nY = 600 - 100 + 2;
			int nW = 96;
			int nH = 96;

			Draw2dImage(nX - 2, nY - 2, nW + 4, nH + 4, CTexture::s_ptexWhite->GetTextureID());

			Set2DMode(true, 800, 600);

			TempDynVB<SVF_P3F_C4B_T2F> vb;
			vb.Allocate(4);
			SVF_P3F_C4B_T2F* vQuad = vb.Lock();

			vQuad[0].xyz.x = nX;
			vQuad[0].xyz.y = nY;
			vQuad[0].xyz.z = 1;
			vQuad[0].color.dcolor = (uint32) - 1;
			vQuad[0].st = Vec2(0.0f, 0.0f);

			vQuad[1].xyz.x = nX + nW;
			vQuad[1].xyz.y = nY;
			vQuad[1].xyz.z = 1;
			vQuad[1].color.dcolor = (uint32) - 1;
			vQuad[1].st = Vec2(1.0f, 0.0f);

			vQuad[2].xyz.x = nX;
			vQuad[2].xyz.y = nY + nH;
			vQuad[2].xyz.z = 1;
			vQuad[2].color.dcolor = (uint32) - 1;
			vQuad[2].st = Vec2(0.0f, 1.0f);

			vQuad[3].xyz.x = nX + nW;
			vQuad[3].xyz.y = nY + nH;
			vQuad[3].xyz.z = 1;
			vQuad[3].color.dcolor = (uint32) - 1;
			vQuad[3].st = Vec2(1.0f, 1.0f);

			vb.Unlock();
			vb.Bind(0);
			vb.Release();

			pSH->FXSetTechnique("InstructionsGrad");
			pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
			pSH->FXBeginPass(0);
			FX_Commit();

			CTexture::s_ptexPaletteDebug->Apply(0, CTexture::GetTexState(TexStateLinear));

			FX_DrawPrimitive(eptTriangleStrip, 0, 4);

			nX = nX * m_width / 800;
			nY = nY * m_height / 600;
			nW = 10 * m_width / 800;
			nH = 10 * m_height / 600;
			float color[4] = { 1, 1, 1, 1 };
			if (CV_r_measureoverdraw == 1 || CV_r_measureoverdraw == 3)
			{
				Draw2dLabel(nX + nW - 25, nY + nH - 30, 1.2f, color, false, CV_r_measureoverdraw == 1 ? "Pixel Shader:" : "Vertex Shader:");
				int n = FtoI(32.0f * CV_r_measureoverdrawscale);
				for (int i = 0; i < 8; i++)
				{
					char str[256];
					cry_sprintf(str, "-- >%d instructions --", n);

					Draw2dLabel(nX + nW, nY + nH * (i + 1), 1.2f, color, false, "%s", str);
					n += FtoI(32.0f * CV_r_measureoverdrawscale);
				}
			}
			else
			{
				Draw2dLabel(nX + nW - 25, nY + nH - 30, 1.2f, color, false, CV_r_measureoverdraw == 2 ? "Pass Count:" : "Overdraw Estimation (360 Hi-Z):");
				Draw2dLabel(nX + nW, nY + nH, 1.2f, color, false, "-- 1 pass --");
				Draw2dLabel(nX + nW, nY + nH * 2, 1.2f, color, false, "-- 2 passes --");
				Draw2dLabel(nX + nW, nY + nH * 3, 1.2f, color, false, "-- 3 passes --");
				Draw2dLabel(nX + nW, nY + nH * 4, 1.2f, color, false, "-- 4 passes --");
				Draw2dLabel(nX + nW, nY + nH * 5, 1.2f, color, false, "-- 5 passes --");
				Draw2dLabel(nX + nW, nY + nH * 6, 1.2f, color, false, "-- 6 passes --");
				Draw2dLabel(nX + nW, nY + nH * 7, 1.2f, color, false, "-- 7 passes --");
				Draw2dLabel(nX + nW, nY + nH * 8, 1.2f, color, false, "-- >8 passes --");
			}
			//WriteXY(nX+10, nY+10, 1, 1,  1,1,1,1, "-- 10 instructions --");
		}
		Set2DMode(false, 1, 1);
		RT_FlushTextMessages();
	}
#endif //_RELEASE
}

void CD3D9Renderer::DrawTexelsPerMeterInfo()
{
#ifndef _RELEASE
	if (CV_r_TexelsPerMeter > 0)
	{
		FX_SetState(GS_NODEPTHTEST);

		int x = 800 - 310 + 2;
		int y = 600 - 20 + 2;
		int w = 296;
		int h = 6;

		Draw2dImage(x - 2, y - 2, w + 4, h + 4, CTexture::s_ptexWhite->GetTextureID(), 0, 0, 1, 1, 0, 1, 1, 1, 1, 0);
		Draw2dImage(x, y, w, h, CTexture::s_ptexPaletteTexelsPerMeter->GetTextureID(), 0, 0, 1, 1, 0, 1, 1, 1, 1, 0);

		float color[4] = { 1, 1, 1, 1 };

		x = x * m_width / 800;
		y = y * m_height / 600;
		w = w * m_width / 800;

		Draw2dLabel(x - 100, y - 20, 1.2f, color, false, "r_TexelsPerMeter:");
		Draw2dLabel(x - 2, y - 20, 1.2f, color, false, "0");
		Draw2dLabel(x + w / 2 - 5, y - 20, 1.2f, color, false, "%.0f", CV_r_TexelsPerMeter);
		Draw2dLabel(x + w - 50, y - 20, 1.2f, color, false, ">= %.0f", CV_r_TexelsPerMeter * 2.0f);

		RT_FlushTextMessages();
	}
#endif
}

#if CRY_PLATFORM_DURANGO

void CD3D9Renderer::RT_SuspendDevice()
{
	assert(!m_bDeviceSuspended);

	if (m_pPerformanceDeviceContext)
		m_pPerformanceDeviceContext->Suspend(0);   // must be 0 for now, reserved for future use.
	m_bDeviceSuspended = true;
}

void CD3D9Renderer::RT_ResumeDevice()
{
	assert(m_bDeviceSuspended);

	if (m_pPerformanceDeviceContext)
		m_pPerformanceDeviceContext->Resume();
	m_bDeviceSuspended = false;
}
#endif

void CD3D9Renderer::RT_ForceSwapBuffers()
{
}

void CD3D9Renderer::SwitchToNativeResolutionBackbuffer()
{
#if CRY_PLATFORM_DURANGO
	if (IRenderAuxGeom* aux = GetIRenderAuxGeom()) aux->Flush();
#endif
	m_pRT->RC_SwitchToNativeResolutionBackbuffer();
}

void CD3D9Renderer::CalculateResolutions(int width, int height, bool bUseNativeRes, int* pRenderWidth, int* pRenderHeight, int* pNativeWidth, int* pNativeHeight, int* pBackbufferWidth, int* pBackbufferHeight)
{
	width = max(width, 512);
	height = max(height, 300);

	*pRenderWidth = width * m_numSSAASamples;
	*pRenderHeight = height * m_numSSAASamples;

#if CRY_PLATFORM_CONSOLE
	*pNativeWidth = 1920;
	*pNativeHeight = 1080;
#elif CRY_PLATFORM_MOBILE
	SDL_DisplayMode desktopDispMode;
	if (SDL_GetDesktopDisplayMode(0, &desktopDispMode) == 0)
	{
		*pNativeWidth = desktopDispMode.w;
		*pNativeHeight = desktopDispMode.h;
	}
	else
	{
		CryLogAlways("Failed to get desktop dimensions: %s", SDL_GetError());
		*pNativeWidth = 1280;
		*pNativeHeight = 720;
	}
#elif CRY_PLATFORM_WINDOWS
	*pNativeWidth = bUseNativeRes ? m_prefMonWidth : width;
	*pNativeHeight = bUseNativeRes ? m_prefMonHeight : height;
#else
	*pNativeWidth = width;
	*pNativeHeight = height;
#endif

	if (m_pStereoRenderer && m_pStereoRenderer->IsStereoEnabled())
	{
		m_pStereoRenderer->CalculateBackbufferResolution(*pNativeWidth, *pNativeHeight, pBackbufferWidth, pBackbufferHeight);
	}
	else
	{
		*pBackbufferWidth = *pNativeWidth;
		*pBackbufferHeight = *pNativeHeight;
	}

	/*if (m_windowParametersOverridden)
	   {
	 * pBackbufferWidth = m_overriddenWindowSize.x;
	 * pBackbufferHeight = m_overriddenWindowSize.y;
	   }*/
}

void CD3D9Renderer::RT_SwitchToNativeResolutionBackbuffer(bool resolveBackBuffer)
{
	if (!IsNativeScalingEnabled())
		return;

#if DURANGO_ENABLE_ASYNC_DIPS
	WaitForAsynchronousDevice();
#endif

	// Pop temporary backbuffer if it was bound
	if (gcpRendD3D->m_RTStack[0][gcpRendD3D->m_nRTStackLevel[0]].m_pTex == CTexture::s_ptexSceneSpecular)
		gcpRendD3D->FX_PopRenderTarget(0);

	RT_SetViewport(0, 0, m_nativeWidth, m_nativeHeight);

	if (IsSuperSamplingEnabled())
	{
		ResolveSupersampledBackbuffer();
	}
	else if (resolveBackBuffer)
	{
		// Custom upscaling from internal to native resolution
		CPostEffect* pPostAA = PostEffectMgr()->GetEffect(ePFX_PostAA);
		if (pPostAA)
		{
			((CPostAA*)pPostAA)->UpscaleImage();
		}
	}
}

void CD3D9Renderer::HandleDisplayPropertyChanges()
{
	const bool msaaChanged = CheckMSAAChange();
	const bool ssaaChanged = CheckSSAAChange();

	if (!IsEditorMode())
	{
		bool bChangeRes = ssaaChanged;

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
		bChangeRes |= m_overrideRefreshRate != CV_r_overrideRefreshRate || m_overrideScanlineOrder != CV_r_overrideScanlineOrder;
#endif

		bool bFullScreen;
#if CRY_PLATFORM_MOBILE || CRY_PLATFORM_CONSOLE
		bFullScreen = m_bFullScreen;
#else
		bFullScreen = m_CVFullScreen ? m_CVFullScreen->GetIVal() != 0 : m_bFullScreen;
#endif

		bool bForceReset = msaaChanged;
		bool bNativeRes;
#if CRY_PLATFORM_CONSOLE || CRY_PLATFORM_MOBILE
		bNativeRes = true;
#elif CRY_PLATFORM_WINDOWS
		bForceReset |= m_bDisplayChanged && bFullScreen;
		m_bDisplayChanged = false;

		const bool fullscreenWindow = CV_r_FullscreenWindow && CV_r_FullscreenWindow->GetIVal() != 0;
		bChangeRes |= m_fullscreenWindow != fullscreenWindow;
		m_fullscreenWindow = fullscreenWindow;

		const bool bFullScreenNativeRes = CV_r_FullscreenNativeRes && CV_r_FullscreenNativeRes->GetIVal() != 0;
		bNativeRes = bFullScreenNativeRes && (bFullScreen || m_fullscreenWindow);
#else
		bNativeRes = false;
#endif

		const int colorBits = m_CVColorBits ? m_CVColorBits->GetIVal() : m_cbpp;

		int width, height;
#if CRY_PLATFORM_MOBILE
		width = m_width;
		height = m_height;
#else
		width = m_CVWidth ? m_CVWidth->GetIVal() : m_width;
		height = m_CVHeight ? m_CVHeight->GetIVal() : m_height;
#endif

#if !CRY_PLATFORM_MOBILE && !CRY_PLATFORM_CONSOLE
		int renderWidth, renderHeight, nativeWidth, nativeHeight, backbufferWidth, backbufferHeight;
		CalculateResolutions(width, height, bNativeRes, &renderWidth, &renderHeight, &nativeWidth, &nativeHeight, &backbufferWidth, &backbufferHeight);

		if (m_width != renderWidth || m_height != renderHeight || m_nativeWidth != nativeWidth || m_nativeHeight != nativeHeight)
		{
			bChangeRes = true;
			width = renderWidth;
			height = renderHeight;
			m_nativeWidth = nativeWidth;
			m_nativeHeight = nativeHeight;
		}

		if (m_backbufferWidth != backbufferWidth || m_backbufferHeight != backbufferHeight)
		{
			bForceReset = true;
			m_backbufferWidth = backbufferWidth;
			m_backbufferHeight = backbufferHeight;
		}
#endif

		if (bForceReset || bChangeRes || bFullScreen != m_bFullScreen || colorBits != m_cbpp || CV_r_vsync != m_VSync)
			ChangeResolution(width, height, colorBits, 75, bFullScreen, bForceReset);
	}
	else if (m_CurrContext && m_CurrContext->m_bMainViewport)
	{
		static bool bCustomRes = false;
		static int nOrigWidth = m_d3dsdBackBuffer.Width;
		static int nOrigHeight = m_d3dsdBackBuffer.Height;

		if (!bCustomRes)
		{
			nOrigWidth = m_d3dsdBackBuffer.Width;
			nOrigHeight = m_d3dsdBackBuffer.Height;
		}

		int nNewBBWidth = nOrigWidth;
		int nNewBBHeight = nOrigHeight;

		bCustomRes = (CV_r_CustomResWidth && CV_r_CustomResHeight) || CV_r_Supersampling > 1;
		if (bCustomRes)
		{
			const int nMaxBBSize = max(min(CV_r_CustomResMaxSize, m_MaxTextureSize), max(nOrigWidth, nOrigHeight));
			nNewBBWidth = clamp_tpl(m_width, nOrigWidth, nMaxBBSize);
			nNewBBHeight = clamp_tpl(m_height, nOrigHeight, nMaxBBSize);
		}

		if (msaaChanged || m_d3dsdBackBuffer.Width != nNewBBWidth || m_d3dsdBackBuffer.Height != nNewBBHeight)
		{
			if (CV_r_CustomResWidth > CV_r_CustomResMaxSize || CV_r_CustomResHeight > CV_r_CustomResMaxSize)
				iLog->LogWarning("Custom resolution exceeds given limits, try increasing CV_r_CustomResMaxSize");
			iLog->Log("Resizing back buffer to match custom resolution rendering:");
			ChangeResolution(nNewBBWidth, nNewBBHeight, 32, 75, false, true);
		}
	}
}

void CD3D9Renderer::BeginFrame()
{
	//////////////////////////////////////////////////////////////////////
	// Set up everything so we can start rendering
	//////////////////////////////////////////////////////////////////////

	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	CheckDeviceLost();

	FlushRTCommands(false, false, false);

	CaptureFrameBufferPrepare();

	// Switching of MT mode in run-time
	//CV_r_multithreaded = 0;

	GetS3DRend().Update();

	m_cEF.mfBeginFrame();

	CRendElement::Tick();
	CFlashTextureSourceSharedRT::Tick();

	CREImposter::m_PrevMemPostponed = CREImposter::m_MemPostponed;
	CREImposter::m_PrevMemUpdated = CREImposter::m_MemUpdated;
	CREImposter::m_MemPostponed = 0;
	CREImposter::m_MemUpdated = 0;

	m_RP.m_TI[m_RP.m_nFillThreadID].m_nFrameID++;
	m_RP.m_TI[m_RP.m_nFillThreadID].m_nFrameUpdateID++;
	m_RP.m_TI[m_RP.m_nFillThreadID].m_RealTime = iTimer->GetCurrTime();
	m_RP.m_TI[m_RP.m_nFillThreadID].m_PersFlags &= ~RBPF_HDR;

	CREOcclusionQuery::m_nQueriesPerFrameCounter = 0;
	CREOcclusionQuery::m_nReadResultNowCounter = 0;
	CREOcclusionQuery::m_nReadResultTryCounter = 0;

	assert(m_RP.m_TI[m_RP.m_nFillThreadID].m_matView->GetDepth() == 0);
	assert(m_RP.m_TI[m_RP.m_nFillThreadID].m_matProj->GetDepth() == 0);
	g_SelectedTechs.resize(0);
	m_RP.m_SysVertexPool[m_RP.m_nFillThreadID].SetUse(0);
	m_RP.m_SysIndexPool[m_RP.m_nFillThreadID].SetUse(0);
	if (gEnv->p3DEngine)
		gEnv->p3DEngine->ResetCoverageBufferSignalVariables();

#ifdef DO_RENDERLOG
	if (CRenderer::CV_r_log)
		Logv("******************************* BeginFrame %d ********************************\n", m_RP.m_TI[m_RP.m_nFillThreadID].m_nFrameUpdateID);
#endif
	if (CRenderer::CV_r_logTexStreaming)
		LogStrv(0, "******************************* BeginFrame %d ********************************\n", m_RP.m_TI[m_RP.m_nFillThreadID].m_nFrameUpdateID);

	PREFAST_SUPPRESS_WARNING(6326)
	bool bUseWaterTessHW(CV_r_WaterTessellationHW != 0 && m_bDeviceSupportsTessellation);
	if (bUseWaterTessHW != m_bUseWaterTessHW)
	{
		PREFAST_SUPPRESS_WARNING(6326)
		m_bUseWaterTessHW = bUseWaterTessHW;
		m_cEF.mfReloadAllShaders(1, SHGD_HW_WATER_TESSELLATION);
	}

	PREFAST_SUPPRESS_WARNING(6326)
	if ((CV_r_SilhouettePOM != 0) != m_bUseSilhouettePOM)
	{
		PREFAST_SUPPRESS_WARNING(6326)
		m_bUseSilhouettePOM = CV_r_SilhouettePOM != 0;
		m_cEF.mfReloadAllShaders(1, SHGD_HW_SILHOUETTE_POM);
	}

	if (CV_r_reloadshaders)
	{
		//exit(0);
		//ShutDown();
		//iConsole->Exit("Test");

		m_cEF.m_Bin.InvalidateCache();
		m_cEF.mfReloadAllShaders(CV_r_reloadshaders, 0);

#ifndef CONSOLE_CONST_CVAR_MODE
		CV_r_reloadshaders = 0;
#endif

		// after reloading shaders, update all shader items
		// and flush the RenderThread to make the changes visible
		gEnv->p3DEngine->UpdateShaderItems();
		gRenDev->FlushRTCommands(true, true, true);
	}

	m_pRT->RC_BeginFrame();
}

void CD3D9Renderer::RT_BeginFrame()
{
	PROFILE_FRAME(RT_BeginFrame);

#ifdef ENABLE_BENCHMARK_SENSOR
	m_benchmarkRendererSensor->update();
#endif
	{
		static const ICVar* pCVarShadows = NULL;
		static const ICVar* pCVarShadowsClouds = NULL;

		SELECT_ALL_GPU();
		// assume these cvars will exist
		if (!pCVarShadows) pCVarShadows = iConsole->GetCVar("e_Shadows");
		if (!pCVarShadowsClouds) pCVarShadowsClouds = iConsole->GetCVar("e_ShadowsClouds");

		m_bShadowsEnabled = pCVarShadows && pCVarShadows->GetIVal() != 0;
		m_bCloudShadowsEnabled = pCVarShadowsClouds && pCVarShadowsClouds->GetIVal() != 0;

#if defined(VOLUMETRIC_FOG_SHADOWS)
		Vec3 volFogShadowEnable(0, 0, 0);
		if (gEnv->p3DEngine)
			gEnv->p3DEngine->GetGlobalParameter(E3DPARAM_VOLFOG_SHADOW_ENABLE, volFogShadowEnable);

		m_bVolFogShadowsEnabled = m_bShadowsEnabled && CV_r_FogShadows != 0 && volFogShadowEnable.x != 0;
		m_bVolFogCloudShadowsEnabled = m_bVolFogShadowsEnabled && m_bCloudShadowsEnabled && m_cloudShadowTexId > 0 && volFogShadowEnable.y != 0;
#endif

		static ICVar* pCVarVolumetricFog = NULL;
		if (!pCVarVolumetricFog) pCVarVolumetricFog = iConsole->GetCVar("e_VolumetricFog");
		bool bVolumetricFog = pCVarVolumetricFog && pCVarVolumetricFog->GetIVal();
		m_bVolumetricFogEnabled = bVolumetricFog && GetVolumetricFog().IsEnableInFrame();
		if (pCVarVolumetricFog && bVolumetricFog && (CRenderer::CV_r_DeferredShadingTiled == 0))
		{
#if !defined(_RELEASE)
			gEnv->pLog->LogWarning("e_VolumetricFog is set to 0 when r_DeferredShadingTiled is 0.");
#endif
			pCVarVolumetricFog->Set((const int)0);
		}

		m_nGraphicsPipeline = CV_r_GraphicsPipeline;
	}

#if defined(SUPPORT_D3D_DEBUG_RUNTIME)
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

	CResFile::Tick();
	m_DevBufMan.Update(m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID, false);

	if (m_pPipelineProfiler)
		m_pPipelineProfiler->BeginFrame();

	//////////////////////////////////////////////////////////////////////
	// Build the matrices
	//////////////////////////////////////////////////////////////////////

#ifndef CONSOLE_CONST_CVAR_MODE
	ICVar* pCVDebugTexelDensity = gEnv->pConsole->GetCVar("e_texeldensity");
	CV_e_DebugTexelDensity = pCVDebugTexelDensity ? pCVDebugTexelDensity->GetIVal() : 0;
#endif

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_matView->LoadIdentity();

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
		CRendElement* pRE = CRendElement::m_RootGlobal.m_NextGlobal;
		for (pRE = CRendElement::m_RootGlobal.m_NextGlobal; pRE != &CRendElement::m_RootGlobal; pRE = pRE->m_NextGlobal)
		{
			CRendElementBase* pR = (CRendElementBase*)pRE;
			if (pR->mfIsHWSkinned())
				pR->mfReset();
		}
	}
	if (CV_r_texminanisotropy != m_nCurMinAniso || CV_r_texmaxanisotropy != m_nCurMaxAniso)
	{
		m_nCurMinAniso = CV_r_texminanisotropy;
		m_nCurMaxAniso = CV_r_texmaxanisotropy;
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
		m_nMaterialAnisoHighSampler = CTexture::GetTexState(STexState(getAnisoFilter(nAniso), false));
		m_nMaterialAnisoLowSampler = CTexture::GetTexState(STexState(getAnisoFilter(CRenderer::CV_r_texminanisotropy), false));
		m_nMaterialAnisoSamplerBorder = CTexture::GetTexState(STexState(getAnisoFilter(nAniso), TADDR_BORDER, TADDR_BORDER, TADDR_BORDER, 0x0));
	}

	// Verify if water caustics needed at all
	if (CV_r_watercaustics)
	{
		N3DEngineCommon::SOceanInfo& OceanInfo = gRenDev->m_p3DEngineCommon.m_OceanInfo;
		m_bWaterCaustics = (OceanInfo.m_nOceanRenderFlags & OCR_OCEANVOLUME_VISIBLE) != 0;
	}

	m_drawNearFov = CV_r_drawnearfov;

#if defined(SUPPORT_DEVICE_INFO_MSG_PROCESSING)
	m_devInfo.ProcessSystemEventQueue();
#endif

	HandleDisplayPropertyChanges();

	if (CV_r_wireframe != m_wireframe_mode)
	{
		//assert(CV_r_wireframe == R_WIREFRAME_MODE || CV_r_wireframe == R_SOLID_MODE);
		FX_SetWireframeMode(CV_r_wireframe);
	}

	UpdateRenderingModesInfo();

#if !defined(_RELEASE)
	m_RP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID].clear();
	m_RP.m_pRNDrawCallsInfoPerMesh[m_RP.m_nProcessThreadID].clear();
#endif

	//////////////////////////////////////////////////////////////////////
	// Begin the scene
	//////////////////////////////////////////////////////////////////////

	SetMaterialColor(1, 1, 1, 1);

	ChangeLog();

	ResetToDefault();

	if (!m_SceneRecurseCount)
	{
		m_SceneRecurseCount++;
	}

	if (CV_r_wireframe != 0 || !CV_r_usezpass)
	{
		EF_ClearTargetsLater(FRT_CLEAR);
	}

	m_nStencilMaskRef = 1;

	if (!IsRecursiveRenderView())
	{
		memset(&m_RP.m_PS[m_RP.m_nProcessThreadID], 0, sizeof(SPipeStat));
		m_RP.m_RTStats.resize(0);
	}

	m_OcclQueriesUsed = 0;

	{
		static float fWaitForGPU;
		float fSmooth = 5.0f;
		fWaitForGPU = (m_fTimeWaitForGPU[m_RP.m_nFillThreadID] + fWaitForGPU * fSmooth) / (fSmooth + 1.0f);
		if (fWaitForGPU >= 0.004f)
		{
			if (m_nGPULimited < 1000)
				m_nGPULimited++;
		}
		else
			m_nGPULimited = 0;

		m_bUseGPUFriendlyBatching[m_RP.m_nProcessThreadID] = m_nGPULimited > 10; // On PC if we are GPU limited use z-pass distance sorting and disable instancing
		if (CV_r_batchtype == 0)
			m_bUseGPUFriendlyBatching[m_RP.m_nProcessThreadID] = false;
		else if (CV_r_batchtype == 1)
			m_bUseGPUFriendlyBatching[m_RP.m_nProcessThreadID] = true;
	}

#if defined(SUPPORT_DEVICE_INFO)
	if (m_bEditor)
	{
		const int width = GetWidth();
		const int height = GetHeight();

		if (m_DepthBufferOrigMSAA.nWidth < width || m_DepthBufferOrigMSAA.nHeight < height)
		{
			D3DDepthSurface* pDSV = 0;
			D3DSurface* pRTVs[8] = { 0 };
			GetDeviceContext().OMSetRenderTargets(8, pRTVs, pDSV);

			m_pGraphicsPipeline->ReleaseBuffers();
			GetS3DRend().ReleaseBuffers();
			ReleaseBackBuffers();

			m_devInfo.SwapChainDesc().BufferDesc.Width = max(m_DepthBufferOrigMSAA.nWidth, width);
			m_devInfo.SwapChainDesc().BufferDesc.Height = max(m_DepthBufferOrigMSAA.nHeight, height);
			m_devInfo.ResizeDXGIBuffers();

			OnD3D11PostCreateDevice(m_devInfo.Device());

			GetS3DRend().OnResolutionChanged();
		}
	}
#endif

#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
	// Refraction Partial Resolves debug views
	if (CRenderer::CV_r_RefractionPartialResolvesDebug == eRPR_DEBUG_VIEW_2D_AREA)
	{
		// Clear the screen for the partial resolve debug view 1
		IRenderAuxGeom* pAuxRenderer = gEnv->pRenderer->GetIRenderAuxGeom();
		if (pAuxRenderer)
		{
			SAuxGeomRenderFlags oldRenderFlags = pAuxRenderer->GetRenderFlags();

			SAuxGeomRenderFlags newRenderFlags;
			newRenderFlags.SetDepthTestFlag(e_DepthTestOff);
			newRenderFlags.SetAlphaBlendMode(e_AlphaNone);
			newRenderFlags.SetMode2D3DFlag(e_Mode2D);
			pAuxRenderer->SetRenderFlags(newRenderFlags);

			const bool bConsoleVisible = GetISystem()->GetIConsole()->GetStatus();
			const float screenTop = (bConsoleVisible) ? 0.5f : 0.0f;

			// Draw full screen black triangle
			const uint vertexCount = 3;
			Vec3 vert[vertexCount] = {
				Vec3(0.0f, screenTop, 0.0f),
				Vec3(0.0f, 2.0f,      0.0f),
				Vec3(2.0f, screenTop, 0.0f)
			};

			pAuxRenderer->DrawTriangles(vert, vertexCount, Col_Black);
			pAuxRenderer->SetRenderFlags(oldRenderFlags);
		}
	}
#endif

	//FX_SetActiveRenderTargets();
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

void CD3D9Renderer::EF_DirtyMatrix()
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_FP_MATRIXDIRTY | RBPF_FP_DIRTY;
}

void CD3D9Renderer::EF_PushMatrix()
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_matView->Push();
	m_MatDepth++;
}

void CD3D9Renderer::EF_PopMatrix()
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_matView->Pop();
	m_MatDepth--;
	EF_DirtyMatrix();
}

void CD3D9Renderer::EF_SetGlobalColor(float r, float g, float b, float a)
{
	assert(m_pRT->IsRenderThread());

	EF_SetColorOp(255, 255, eCA_Texture | (eCA_Constant << 3), eCA_Texture | (eCA_Constant << 3));
	if (m_RP.m_CurGlobalColor[0] != r || m_RP.m_CurGlobalColor[1] != g || m_RP.m_CurGlobalColor[2] != b || m_RP.m_CurGlobalColor[3] != a)
	{
		m_RP.m_CurGlobalColor[0] = r;
		m_RP.m_CurGlobalColor[1] = g;
		m_RP.m_CurGlobalColor[2] = b;
		m_RP.m_CurGlobalColor[3] = a;
		m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags |= RBPF_FP_DIRTY;
	}
}

void CD3D9Renderer::EF_SetVertColor()
{
	EF_SetColorOp(255, 255, eCA_Texture | (eCA_Diffuse << 3), eCA_Texture | (eCA_Diffuse << 3));
}

#if defined(FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE)
uint32 CD3D9Renderer::FX_GetInputLayoutCacheId(int StreamMask, EVertexFormat eVF)
{
	return (StreamMask & (0xfe | VSM_MORPHBUDDY)) ^ (eVF << 24);
}
#endif //FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE

bool CD3D9Renderer::CaptureFrameBufferToFile(const char* pFilePath, CTexture* pRenderTarget)
{
	assert(pFilePath);
	if (!pFilePath)
		return false;

	const char* pReqFileFormatExt(fpGetExtension(pFilePath));
	SCaptureFormatInfo::ECaptureFileFormat captureFormat = SCaptureFormatInfo::GetCaptureFormatByExtension(++pReqFileFormatExt);

	if (pRenderTarget && pRenderTarget->GetDstFormat() != eTF_R8G8B8A8)
	{
		if (iLog)
			iLog->Log("Warning: Captured RenderTarget has unsupported format.\n");
		return false;
	}

	assert(!IsEditorMode() || (m_CurrContext && m_CurrContext->m_pBackBuffers == m_pBackBuffers));

	// get last buffer used for Present()
	D3DSurface* pBackBuffer;
	pBackBuffer = m_pBackBuffers[(m_pCurrentBackBufferIndex + m_pBackBuffers.size() - 1) % m_pBackBuffers.size()];

	assert(pBackBuffer);

	bool frameCaptureSuccessful(false);
	D3DResource* pBackBufferRsrc(0);
	if (!pRenderTarget)
	{
		pBackBuffer->GetResource(&pBackBufferRsrc);
	}
	else
	{
		D3DSurface* pSurface = pRenderTarget->GetSurface(0, 0);
		assert(pSurface);
		pSurface->GetResource(&pBackBufferRsrc);
	}

	CTexture* pTempCopyTex = nullptr;
	D3DTexture* pBackBufferTex = (D3DTexture*)pBackBufferRsrc;
	if (pBackBufferTex)
	{
		D3D11_TEXTURE2D_DESC bbDesc;
		pBackBufferTex->GetDesc(&bbDesc);

		if (bbDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
		    || m_CurrContext->m_nSSSamplesX > 1
		    || m_CurrContext->m_nSSSamplesY > 1)
		{
			const int nResolvedWidth = m_width / m_CurrContext->m_nSSSamplesX;
			const int nResolvedHeight = m_height / m_CurrContext->m_nSSSamplesY;
			bbDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			bbDesc.Width = nResolvedWidth;
			bbDesc.Height = nResolvedHeight;

			pTempCopyTex = CTexture::Create2DTexture("$TempCopyTex", bbDesc.Width, bbDesc.Height, 1, 0, nullptr, eTF_R8G8B8A8, eTF_R8G8B8A8);

			D3D11_BOX srcRegion;
			srcRegion.left = 0;
			srcRegion.right = nResolvedWidth;
			srcRegion.top = 0;
			srcRegion.bottom = nResolvedHeight;
			srcRegion.front = 0;
			srcRegion.back = 1;

			// copy resolved part of back-buffer to temporary target
			GetDeviceContext().CopySubresourceRegion(pTempCopyTex->GetDevTexture()->Get2DTexture(), 0, 0, 0, 0, pBackBufferTex, 0, &srcRegion);

			pBackBufferTex = pTempCopyTex->GetDevTexture()->Get2DTexture();
		}

		D3D11_TEXTURE2D_DESC tmpDesc = bbDesc;
		tmpDesc.Usage = D3D11_USAGE_STAGING;
		tmpDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		tmpDesc.BindFlags = 0;

		CTexture* pTmpTexture = CTexture::Create2DTexture("$TmpTexture", bbDesc.Width, bbDesc.Height, 1, FT_STAGE_READBACK, nullptr, eTF_R8G8B8A8, eTF_R8G8B8A8);

		gcpRendD3D->GetDeviceContext().CopyResource(pTmpTexture->GetDevTexture()->Get2DTexture(), pBackBufferTex);

		const int INPUT_BYTES_PER_PIXEL = 4;
		const int OUTPUT_BYTES_PER_PIXEL = 3;
		const int texsize = bbDesc.Width * bbDesc.Height * OUTPUT_BYTES_PER_PIXEL;
		byte* pDest = new byte[texsize + sizeof(uint32)];  // Extra space required since we always copy 32 bits
		assert(pDest);

#if CRY_PLATFORM_DURANGO
		bool formatBGRA = !pRenderTarget ? true : false;
#else
		bool formatBGRA = false;
#endif
		bool needRBSwap = (captureFormat == SCaptureFormatInfo::eCaptureFormat_TGA ? !formatBGRA : formatBGRA);

		pTmpTexture->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
		{
			// copy, flip red with blue
			const byte* pSrc = reinterpret_cast<const byte*>(pData);
			byte* dst = pDest;
			const byte* src = pSrc;
			for (int i = 0; i < bbDesc.Height; i++)
			{
			  for (int j = 0; j < bbDesc.Width; j++)
			  {
			    *(uint32*)&dst[j * OUTPUT_BYTES_PER_PIXEL] = *(uint32*)&src[j * INPUT_BYTES_PER_PIXEL];
			  }
			  if (needRBSwap)
			  {
			    PREFAST_ASSUME(texsize == bbDesc.Width * 4);
			    for (int j = 0; j < bbDesc.Width; j++)
						Exchange(dst[j * OUTPUT_BYTES_PER_PIXEL + 0], dst[j * OUTPUT_BYTES_PER_PIXEL + 2]);
			  }
			  src += rowPitch;
			  dst += bbDesc.Width * OUTPUT_BYTES_PER_PIXEL;
			}

			return true;
		});

		SAFE_RELEASE(pTmpTexture);

		switch (captureFormat)
		{
		case SCaptureFormatInfo::eCaptureFormat_TGA:
			frameCaptureSuccessful = ::WriteTGA(pDest, bbDesc.Width, bbDesc.Height, pFilePath, 8 * OUTPUT_BYTES_PER_PIXEL, 8 * OUTPUT_BYTES_PER_PIXEL);
			break;
		case SCaptureFormatInfo::eCaptureFormat_JPEG:
			frameCaptureSuccessful = ::WriteJPG(pDest, bbDesc.Width, bbDesc.Height, pFilePath, 8 * OUTPUT_BYTES_PER_PIXEL, 90);
			break;
		default:
			frameCaptureSuccessful = false;
			break;
		}

		delete[] pDest;
	}

	SAFE_RELEASE(pBackBufferTex);
	SAFE_RELEASE(pTempCopyTex);

	return frameCaptureSuccessful;
}

#define DEPTH_BUFFER_SCALE 1024.0f

void CD3D9Renderer::CaptureFrameBuffer()
{
	CDebugAllowFileAccess ignoreInvalidFileAccess;

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

		if (!CaptureFrameBufferToFile(path))
		{
			if (iLog)
				iLog->Log("Warning: Frame capture failed!\n");
			CV_capture_frames->Set(0); // disable capturing
		}
	}
}

void CD3D9Renderer::ResolveSupersampledBackbuffer()
{
	if (IsEditorMode() && (m_CurrContext->m_nSSSamplesX <= 1 || m_CurrContext->m_nSSSamplesY <= 1))
		return;

	PROFILE_LABEL_SCOPE("RESOLVE_SUPERSAMPLED");

	SD3DPostEffectsUtils::EFilterType eFilter = SD3DPostEffectsUtils::FilterType_Box;
	if (CV_r_SupersamplingFilter == 1)
		eFilter = SD3DPostEffectsUtils::FilterType_Tent;
	else if (CV_r_SupersamplingFilter == 2)
		eFilter = SD3DPostEffectsUtils::FilterType_Gauss;
	else if (CV_r_SupersamplingFilter == 3)
		eFilter = SD3DPostEffectsUtils::FilterType_Lanczos;

	if (IsEditorMode())
	{
		RT_SetViewport(0, 0, m_width, m_height);
		PostProcessUtils().CopyScreenToTexture(CTexture::s_ptexBackBuffer);
		PostProcessUtils().Downsample(CTexture::s_ptexBackBuffer, NULL,
		                              m_width, m_height,
		                              m_width / m_CurrContext->m_nSSSamplesX, m_height / m_CurrContext->m_nSSSamplesY,
		                              eFilter, false);
	}
	else
	{
		PostProcessUtils().Downsample(CTexture::s_ptexSceneSpecular, NULL,
		                              m_width, m_height,
		                              m_backbufferWidth, m_backbufferHeight,
		                              eFilter, false);
	}
}

void CD3D9Renderer::ScaleBackbufferToViewport()
{
	if (m_CurrContext->m_nSSSamplesX > 1 || m_CurrContext->m_nSSSamplesY > 1)
	{
		PROFILE_LABEL_SCOPE("STRETCH_TO_VIEWPORT");

		const int nResolvedWidth = m_width / m_CurrContext->m_nSSSamplesX;
		const int nResolvedHeight = m_height / m_CurrContext->m_nSSSamplesY;

		RECT srcRegion;
		srcRegion.left = 0;
		srcRegion.right = nResolvedWidth;
		srcRegion.top = 0;
		srcRegion.bottom = nResolvedHeight;

		// copy resolved part of backbuffer to temporary target
		PostProcessUtils().CopyScreenToTexture(CTexture::s_ptexBackBufferScaled[0], &srcRegion);
		// stretch back to backbuffer
		PostProcessUtils().CopyTextureToScreen(CTexture::s_ptexBackBufferScaled[0], &srcRegion);
	}
}

gpu_pfx2::IManager* CD3D9Renderer::GetGpuParticleManager()
{
	return static_cast<gpu_pfx2::IManager*>(m_gpuParticleManager.get());
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::ClearPerFrameData(const SRenderingPassInfo& passInfo)
{
	CRenderer::ClearPerFrameData(passInfo);
	if (passInfo.IsGeneralPass())
	{
		GetVolumetricFog().Clear();
	}
}

static float LogMap(const float fA)
{
	return logf(fA) / logf(2.0f) + 5.5f;    // offset to bring values <0 in viewable range
}

void CD3D9Renderer::DebugDrawRect(float x1, float y1, float x2, float y2, float* fColor)
{
#ifndef _RELEASE
	SetMaterialColor(fColor[0], fColor[1], fColor[2], fColor[3]);
	int w = GetWidth();
	int h = GetHeight();
	float dx = 1.0f / w;
	float dy = 1.0f / h;
	x1 *= dx;
	x2 *= dx;
	y1 *= dy;
	y2 *= dy;

	ColorB col((uint8)(fColor[0] * 255.0f), (uint8)(fColor[1] * 255.0f), (uint8)(fColor[2] * 255.0f), (uint8)(fColor[3] * 255.0f));

	IRenderAuxGeom* pAux = GetIRenderAuxGeom();
	SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
	flags.SetMode2D3DFlag(e_Mode2D);
	pAux->SetRenderFlags(flags);
	pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x2, y1, 0), col);
	pAux->DrawLine(Vec3(x1, y2, 0), col, Vec3(x2, y2, 0), col);
	pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x1, y2, 0), col);
	pAux->DrawLine(Vec3(x2, y1, 0), col, Vec3(x2, y2, 0), col);
#endif
}

void CD3D9Renderer::PrintResourcesLeaks()
{
#ifndef _RELEASE
	iLog->Log("\n \n");

	AUTO_LOCK(CBaseResource::s_cResLock);

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
			Warning("--- Vertex Shader '%s' leak after level unload", psh->GetName());
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

	CRendElement* pRE;
	for (pRE = CRendElement::m_RootGlobal.m_NextGlobal; pRE != &CRendElement::m_RootGlobal; pRE = pRE->m_NextGlobal)
	{
		Warning("--- CRenderElement %s leak after level unload", pRE->mfTypeString());
	}
	iLog->Log("\n \n");

	CRenderMesh::PrintMeshLeaks();
#endif
}

#define BYTES_TO_KB(bytes) ((bytes) / 1024.0)
#define BYTES_TO_MB(bytes) ((bytes) / 1024.0 / 1024.0)

void CD3D9Renderer::DebugDrawStats1()
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	const int nYstep = 10;
	uint32 i;
	int nY = 30; // initial Y pos
	int nX = 50; // initial X pos

	ColorF col = Col_Yellow;
	Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Per-frame stats:");

	col = Col_White;
	nX += 10;
	nY += 25;
	//DebugDrawRect(nX-2, nY, nX+180, nY+150, &col.r);
	Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Draw-calls:");

	float fFSize = 1.2f;

	nX += 5;
	nY += 10;
	int nXBars = nX;

	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "General: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_GENERAL], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_GENERAL]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Decals: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_DECAL], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_DECAL]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Terrain layers: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_TERRAINLAYER], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_TERRAINLAYER]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Transparent: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_TRANSP], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_TRANSP]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shadow-gen: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_SHADOW_GEN], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_SHADOW_GEN]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shadow-pass: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_SHADOW_PASS], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_SHADOW_PASS]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Water: %d (%d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[EFSLIST_WATER], m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[EFSLIST_WATER]);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Imposters: %d (Updates: %d)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumCloudImpostersDraw, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumCloudImpostersUpdates);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Sprites: %d (%d dips, %d updates, %d altases, %d cells, %d polys)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSprites, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteDIPS, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteUpdates, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteAltasesUsed, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpriteCellsUsed, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumSpritePolys);

	Draw2dLabel(nX - 5, nY + 20, 1.4f, &col.r, false, "Total: %d (%d polys)", GetCurrentNumberOfDrawCalls(), RT_GetPolyCount());

	col = Col_Yellow;
	nX -= 5;
	nY += 45;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Occlusions: Issued: %d, Occluded: %d, NotReady: %d", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumQIssued, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumQOccluded, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumQNotReady);

	col = Col_Cyan;
	nX -= 5;
	nY += 45;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource switches:");

	nX += 5;
	nY += 10;
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "VShaders: %d (%d unique)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumVShadChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumVShaders);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "PShaders: %d (%d unique)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumPShadChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumPShaders);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Textures: %d (%d unique)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumTextChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumTextures);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "RT's: %d (%d unique), cleared: %d times, copied: %d times", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRTChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRTs, m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCleared, m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCopied);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "States: %d", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumStateChanges);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MatBatches: %d, GeomBatches: %d, Instances: %d", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendMaterialBatches, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendGeomBatches, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendInstances);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "HW Instances: DIP's: %d, Instances: %d (polys: %d/%d)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_RendHWInstancesDIPs, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendHWInstances, m_RP.m_PS[m_RP.m_nProcessThreadID].m_RendHWInstancesPolysOne, m_RP.m_PS[m_RP.m_nProcessThreadID].m_RendHWInstancesPolysAll);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Skinned instances: %d", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendSkinnedObjects);

	CHWShader_D3D* ps = (CHWShader_D3D*)m_RP.m_PS[m_RP.m_nProcessThreadID].m_pMaxPShader;
	CHWShader_D3D::SHWSInstance* pInst = (CHWShader_D3D::SHWSInstance*)m_RP.m_PS[m_RP.m_nProcessThreadID].m_pMaxPSInstance;
	if (ps)
		Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MAX PShader: %s (instructions: %d, lights: %d)", ps->GetName(), pInst->m_nInstructions, pInst->m_Ident.m_LightMask & 0xf);
	CHWShader_D3D* vs = (CHWShader_D3D*)m_RP.m_PS[m_RP.m_nProcessThreadID].m_pMaxVShader;
	pInst = (CHWShader_D3D::SHWSInstance*)m_RP.m_PS[m_RP.m_nProcessThreadID].m_pMaxVSInstance;
	if (vs)
		Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "MAX VShader: %s (instructions: %d, lights: %d)", vs->GetName(), pInst->m_nInstructions, pInst->m_Ident.m_LightMask & 0xf);

	col = Col_Green;
	nX -= 5;
	nY += 35;
	//DebugDrawRect(nX-2, nY, nX+180, nY+160, &col.r);
	Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Device resource sizes:");

	nX += 5;
	nY += 10;
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Managed non-streamed textures: Sys=%.3f Mb, Vid:=%.3f", BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesVidMemSize));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Managed streamed textures: Sys=%.3f Mb, Vid:=%.3f", BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesStreamVidSize));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "RT textures: Used: %.3f Mb, Updated: %.3f Mb, Cleared: %.3f Mb, Copied: %.3f Mb", BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_DynTexturesSize), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTSize), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTClearedSize), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_RTCopiedSize));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Meshes updated: Static: %.3f Mb, Dynamic: %.3f Mb", BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_MeshUpdateBytes), BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_DynMeshUpdateBytes));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Cloud textures updated: %.3f Mb", BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_CloudImpostersSizeUpdate));

	int nYBars = nY;

	nY = 30;  // initial Y pos
	nX = 430; // initial X pos
	col = Col_Yellow;
	Draw2dLabel(nX, nY, 2.0f, &col.r, false, "Global stats:");

	col = Col_YellowGreen;
	nX += 10;
	nY += 55;
	Draw2dLabel(nX, nY, 1.4f, &col.r, false, "Mesh size:");

	size_t nMemApp = 0;
	size_t nMemDevVB = 0;
	size_t nMemDevIB = 0;
	size_t nMemDevVBPool = 0;
	size_t nMemDevIBPool = 0;
	size_t nMemDevVBPoolUsed = 0;
	size_t nMemDevIBPoolUsed = 0;
	{
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.prev; iter != &CRenderMesh::m_MeshList; iter = iter->prev)
		{
			CRenderMesh* pRM = iter->item<& CRenderMesh::m_Chain>();
			nMemApp += pRM->Size(CRenderMesh::SIZE_ONLY_SYSTEM);
			nMemDevVB += pRM->Size(CRenderMesh::SIZE_VB);
			nMemDevIB += pRM->Size(CRenderMesh::SIZE_IB);
		}
	}
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Static: (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb)",
	            nMemApp / 1024.0f / 1024.0f, nMemDevVB / 1024.0f / 1024.0f, nMemDevIB / 1024.0f / 1024.0f);

	for (i = 0; i < BBT_MAX; i++)
		for (int j = 0; j < BU_MAX; j++)
		{
			SDeviceBufferPoolStats stats;
			if (m_DevBufMan.GetStats((BUFFER_BIND_TYPE)i, (BUFFER_USAGE)j, stats) == false)
				continue;
			Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Pool '%10s': size %5.3f banks %4" PRISIZE_T " allocs %6d frag %3.3f pinned %4d moving %4d",
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
	nMemApp = m_RP.m_SizeSysArray;

	uint32 j;
	for (i = 0; i < SHAPE_MAX; i++)
	{
		nMemDevVB += _VertBufferSize(m_pUnitFrustumVB[i]);
		nMemDevIB += _IndexBufferSize(m_pUnitFrustumIB[i]);
	}

	#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		nMemDevVB += m_pRenderAuxGeomD3D->GetDeviceDataSize();
	#endif

	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Dynamic: %.3f (app: %.3f Mb, dev VB: %.3f Mb, dev IB: %.3f Mb)", BYTES_TO_MB(nMemDevVB + nMemDevIB), BYTES_TO_MB(nMemApp), BYTES_TO_MB(nMemDevVB), BYTES_TO_MB(nMemDevIB) /*, BYTES_TO_MB(nMemDevVBPool)*/);

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
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "FX Shaders: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

	n = 0;
	nSize = m_cEF.m_Bin.mfSizeFXParams(n);
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "FX Shader parameters for %d shaders (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

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
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shader resources: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shader manager (size: %.3f Mb)", BYTES_TO_MB(m_cEF.Size()));

	n = 0;
	for (i = 0; i < CGParamManager::s_Groups.size(); i++)
	{
		n += CGParamManager::s_Groups[i].nParams;

	}
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Groups: %d, Shader parameters: %d (size: %.3f Mb), in pool: %d (size: %.3f Mb)", i, n, BYTES_TO_MB(n * sizeof(SCGParam)), CGParamManager::s_Pools.size(), BYTES_TO_MB(CGParamManager::s_Pools.size() * PARAMS_POOL_SIZE * sizeof(SCGParam)));

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
						if (ShadersVS[j] == pShInst->m_Handle.m_pShader->m_pHandle)
							break;
					}
					if (j == ShadersVS.Num())
					{
						ShadersVS.AddElem(pShInst->m_Handle.m_pShader->m_pHandle);
					}
				}
			}
		}
	}
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "VShaders: %d (size: %.3f Mb), Instances: %d, Device VShaders: %d (Size: %.3f Mb)", n, BYTES_TO_MB(nSize), nInsts, ShadersVS.Num(), BYTES_TO_MB(CHWShader_D3D::s_nDeviceVSDataSize));

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
						if (ShadersPS[j] == pShInst->m_Handle.m_pShader->m_pHandle)
							break;
					}
					if (j == ShadersPS.Num())
					{
						ShadersPS.AddElem(pShInst->m_Handle.m_pShader->m_pHandle);
					}
				}
			}
		}
	}
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "PShaders: %d (size: %.3f Mb), Instances: %d, Device PShaders: %d (Size: %.3f Mb)", n, BYTES_TO_MB(nSize), nInsts, ShadersPS.Num(), BYTES_TO_MB(CHWShader_D3D::s_nDevicePSDataSize));

	n = 0;
	nSize = 0;
	size_t nSizeD = 0;
	size_t nSizeAll = 0;

	FXShaderCacheItor FXitor;
	size_t nCache = 0;
	nSize = 0;
	for (FXitor = CHWShader::m_ShaderCache.begin(); FXitor != CHWShader::m_ShaderCache.end(); FXitor++)
	{
		SShaderCache* sc = FXitor->second;
		if (!sc)
			continue;
		nCache++;
		nSize += sc->Size();
	}
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Shader Cache: %" PRISIZE_T " (size: %.3f Mb)", nCache, BYTES_TO_MB(nSize));

	nSize = 0;
	n = 0;
	CRendElement* pRE = CRendElement::m_RootGlobal.m_NextGlobal;
	while (pRE != &CRendElement::m_RootGlobal)
	{
		n++;
		nSize += pRE->Size();
		pRE = pRE->m_NextGlobal;
	}
	nY += nYstep;
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Render elements: %d (size: %.3f Mb)", n, BYTES_TO_MB(nSize));

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
				if (!(tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET)))
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
			if (tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
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
		}
	}

	nY += nYstep;
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "CryName: %d, Size: %.3f Mb...", CCryNameR::GetNumberOfEntries(), BYTES_TO_MB(CCryNameR::GetMemoryUsage()));
	nY += nYstep;

	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, "Textures: %d, ObjSize: %.3f Mb...", n, BYTES_TO_MB(nObjSize));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " All Managed Video Size: %.3f Mb (Normals: %.3f Mb + Other: %.3f Mb), One mip: %.3f", BYTES_TO_MB(nSNM + nSAll), BYTES_TO_MB(nSNM), BYTES_TO_MB(nSAll), BYTES_TO_MB(nSOneMip));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " All Managed System Size: %.3f Mb (Normals: %.3f Mb + Other: %.3f Mb), One mip: %.3f", BYTES_TO_MB(nSysNM + nSysAll), BYTES_TO_MB(nSysNM), BYTES_TO_MB(nSysAll), BYTES_TO_MB(nSysOneMip));
	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " Streamed Size: Video: %.3f, System: %.3f, Unloaded: %.3f", BYTES_TO_MB(nStreamed), BYTES_TO_MB(nStreamedSys), BYTES_TO_MB(nStreamedUnload));

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

	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " Dynamic DataSize: %.3f Mb (Atlases: %.3f Mb, Managed: %.3f Mb (Shadows: %.3f Mb), Other: %.3f Mb)", BYTES_TO_MB(nSRT), BYTES_TO_MB(nSizeAtlas), BYTES_TO_MB(nSizeManagedDyn), BYTES_TO_MB(nSizeSH), BYTES_TO_MB(nSRT - nSizeManagedDyn - nSizeAtlas));

	size_t nSizeZRT = 0;
	size_t nSizeCRT = 0;

	if (m_DepthBufferOrig.pSurface)
		nSizeZRT += m_DepthBufferOrig.nWidth * m_DepthBufferOrig.nHeight * 4;
	if (m_DepthBufferOrigMSAA.pSurface && m_DepthBufferOrig.pSurface != m_DepthBufferOrigMSAA.pSurface)
		nSizeZRT += m_DepthBufferOrig.nWidth * m_DepthBufferOrig.nHeight * 2 * 4;

	for (i = 0; i < (int)m_TempDepths.Num(); i++)
	{
		SDepthTexture* pSrf = m_TempDepths[i];
		if (pSrf->pSurface)
			nSizeZRT += pSrf->nWidth * pSrf->nHeight * 4;
	}

	nSizeCRT += m_d3dsdBackBuffer.Width * m_d3dsdBackBuffer.Height * 4 * 2;

	Draw2dLabel(nX, nY += nYstep, fFSize, &col.r, false, " Targets DataSize: %.3f Mb (Color Buffer RT's: %.3f Mb, Z-Buffers: %.3f Mb", BYTES_TO_MB(nSizeCRT + nSizeZRT), BYTES_TO_MB(nSizeCRT), BYTES_TO_MB(nSizeZRT));

	DebugPerfBars(nXBars, nYBars + 30);
#endif
}

void CD3D9Renderer::DebugVidResourcesBars(int nX, int nY)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS)
	int i;
	int nYst = 15;
	float fFSize = 1.4f;
	ColorF col = Col_Yellow;

	// Draw performance bars
	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
	FX_SetState(GS_NODEPTHTEST);

	float fMaxBar = 200;
	float fOffs = 190.0f;

	ColorF colT = Col_Gray;
	Draw2dLabel(nX + 50, nY, 1.6f, &colT.r, false, "Video resources:");
	nY += 20;

	double fMaxTextureMemory = m_MaxTextureMemory * 1024.0 * 1024.0;

	ColorF colF = Col_Orange;
	Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Total memory: %.1f Mb", BYTES_TO_MB(fMaxTextureMemory));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + fMaxBar, nY + 12, Col_Cyan, 1.0f);
	nY += nYst;

	SDynTexture_Shadow* pTXSH = SDynTexture_Shadow::s_RootShadow.m_NextShadow;
	size_t nSizeSH = 0;
	while (pTXSH != &SDynTexture_Shadow::s_RootShadow)
	{
		if (pTXSH->m_pTexture)
			nSizeSH += pTXSH->m_pTexture->GetDeviceDataSize();
		pTXSH = pTXSH->m_NextShadow;
	}
	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Shadow textures: %.1f Mb", BYTES_TO_MB(nSizeSH));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeSH / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
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
	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. text.: %.1f Mb", BYTES_TO_MB(nSizeD));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeD / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeD2 = 0;
	for (i = 0; i < eTP_Max; i++)
	{
		nSizeD2 += SDynTexture2::s_nMemoryOccupied[i];
	}
	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. atlas text.: %.1f Mb", BYTES_TO_MB(nSizeD2));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeD2 / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeZRT = 0;
	size_t nSizeCRT = 0;

	if (m_DepthBufferOrig.pSurface)
		nSizeZRT += m_DepthBufferOrig.nWidth * m_DepthBufferOrig.nHeight * 4;
	if (m_DepthBufferOrigMSAA.pSurface && m_DepthBufferOrig.pSurface != m_DepthBufferOrigMSAA.pSurface)
		nSizeZRT += m_DepthBufferOrig.nWidth * m_DepthBufferOrig.nHeight * 2 * 4;

	for (i = 0; i < (int)m_TempDepths.Num(); i++)
	{
		SDepthTexture* pSrf = m_TempDepths[i];
		if (pSrf->pSurface)
			nSizeZRT += pSrf->nWidth * pSrf->nHeight * 4;
	}

	nSizeCRT += m_d3dsdBackBuffer.Width * m_d3dsdBackBuffer.Height * 4 * 2;
	nSizeCRT += nSizeZRT;

	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Frame targets: %.1f Mb", BYTES_TO_MB(nSizeCRT));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeCRT / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
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
			if (tp->GetFlags() & (FT_USAGE_DYNAMIC | FT_USAGE_RENDERTARGET))
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
		}
	}
	nSRT -= (nSizeD + nSizeD2 + nSizeSH);
	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Render targets: %.1f Mb", BYTES_TO_MB(nSRT));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSRT / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Textures: %.1f Mb", BYTES_TO_MB(nSAll));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSAll / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeMeshes = 0;
	{
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
		{
			nSizeMeshes += iter->item<& CRenderMesh::m_Chain>()->Size(CRenderMesh::SIZE_VB | CRenderMesh::SIZE_IB);
		}
	}
	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Meshes: %.1f Mb", BYTES_TO_MB(nSizeMeshes));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeMeshes / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst;

	size_t nSizeDynM = 0;
	for (i = 0; i < SHAPE_MAX; i++)
	{
		nSizeDynM += _VertBufferSize(m_pUnitFrustumVB[i]);
		nSizeDynM += _IndexBufferSize(m_pUnitFrustumIB[i]);
	}

	#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		m_pRenderAuxGeomD3D->GetDeviceDataSize();
	#endif

	Draw2dLabel(nX, nY, fFSize, &col.r, false, "Dyn. mesh: %.1f Mb", BYTES_TO_MB(nSizeDynM));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSizeDynM / fMaxTextureMemory * fMaxBar, nY + 12, Col_Green, 1.0f);
	nY += nYst + 4;

	size_t nSize = nSizeDynM + nSRT + nSizeCRT + nSizeSH + nSizeD + nSizeD2;
	ColorF colO = Col_Blue;
	Draw2dLabel(nX, nY, fFSize, &colO.r, false, "Overall (Pure): %.1f Mb", BYTES_TO_MB(nSize));
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 1, nX + fOffs + (float)nSize / fMaxTextureMemory * fMaxBar, nY + 12, Col_White, 1.0f);
	nY += nYst;
#endif
}

void CD3D9Renderer::DebugPerfBars(int nX, int nY)
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS) && defined(ENABLE_PROFILING_CODE)
	int nYst = 15;
	float fFSize = 1.4f;
	ColorF col = Col_Yellow;
	ColorF colP = Col_Cyan;

	// Draw performance bars
	Set2DMode(true, m_width, m_height);
	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
	FX_SetState(GS_NODEPTHTEST);
	D3DSetCull(eCULL_None);
	FX_SetFPMode();

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

	Draw2dLabel(nX + 30, nY, 1.6f, &colP.r, false, "Instances: %d, GeomBatches: %d, MatBatches: %d, DrawCalls: %d, Text: %d, Stat: %d, PShad: %d, VShad: %d", m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendInstances, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendGeomBatches, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumRendMaterialBatches, GetCurrentNumberOfDrawCalls(), m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumTextChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumStateChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumPShadChanges, m_RP.m_PS[m_RP.m_nProcessThreadID].m_NumVShadChanges);
	nY += 30;

	ColorF colT = Col_Gray;
	Draw2dLabel(nX + 50, nY, 1.6f, &colT.r, false, "Performance:");
	nY += 20;

	float fSmooth = 5.0f;
	fFrameTime = (iTimer->GetFrameTime() + fFrameTime * fSmooth) / (fSmooth + 1.0f);

	ColorF colF = Col_Orange;
	Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Frame: %.3fms", fFrameTime * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fMaxBar, nY + 12, Col_Cyan, 1.0f);
	nY += nYst + 5;

	float fTimeDIPSum = fTimeDIPZ + fTimeDIP[EFSLIST_DEFERRED_PREPROCESS] + fTimeDIP[EFSLIST_GENERAL] + fTimeDIP[EFSLIST_TERRAINLAYER] + fTimeDIP[EFSLIST_SHADOW_GEN] + fTimeDIP[EFSLIST_DECAL] + fTimeDIPAO + fTimeDIPRAIN + fTimeDIPLayers + fTimeDIP[EFSLIST_WATER_VOLUMES] + fTimeDIP[EFSLIST_TRANSP] + fTimeDIP[EFSLIST_POSTPROCESS] + fTimeDIPSprites;
	Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Sum all passes: %.3fms", fTimeDIPSum * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fTimeDIPSum / fFrameTime * fMaxBar, nY + 12, Col_Yellow, 1.0f);
	nY += nYst + 5;

	fRTTimeProcess = (m_fTimeProcessedRT[m_RP.m_nFillThreadID] + fRTTimeProcess * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT process time: %.3fms", fRTTimeProcess * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeProcess / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;
	nX += 5;

	fRTTimeEndFrame = (m_fRTTimeEndFrame + fRTTimeEndFrame * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT end frame: %.3fms", fRTTimeEndFrame * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeEndFrame / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeFlashRender = (m_fRTTimeFlashRender + fRTTimeFlashRender * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT flash render: %.3fms", fRTTimeFlashRender * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeFlashRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeMiscRender = (m_fRTTimeMiscRender + fRTTimeMiscRender * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT misc render: %.3fms", fRTTimeMiscRender * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeMiscRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	fRTTimeSceneRender = (m_fRTTimeSceneRender + fRTTimeSceneRender * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT scene render: %.3fms", fRTTimeSceneRender * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeSceneRender / fFrameTime * fMaxBar, nY + 12, Col_Gray, 1.0f);
	nY += nYst;

	float fRTTimeOverall = fRTTimeSceneRender + fRTTimeEndFrame + fRTTimeFlashRender + fRTTimeMiscRender;
	Draw2dLabel(nX, nY, fFSize, &colT.r, false, "RT CPU overall: %.3fms", fRTTimeOverall * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fRTTimeOverall / fFrameTime * fMaxBar, nY + 12, Col_LightGray, 1.0f);
	nY += nYst + 5;
	nX -= 5;

	fWaitForGPU = (m_fTimeWaitForGPU[m_RP.m_nFillThreadID] + fWaitForGPU * fSmooth) / (fSmooth + 1.0f);
	Draw2dLabel(nX, nY, fFSize, &colF.r, false, "Wait for GPU: %.3fms", fWaitForGPU * 1000.0f);
	CTexture::s_ptexWhite->Apply(0);
	DrawQuad(nX + fOffs, nY + 4, nX + fOffs + fWaitForGPU / fFrameTime * fMaxBar, nY + 12, Col_Blue, 1.0f);
	nY += nYst;

	Set2DMode(false, m_width, m_height);
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

		if (pTexResource->GetFlags() & FT_USAGE_RENDERTARGET)
		{
			pRenderTargets.push_back(pTexResource);
			nSizeTotalRT += pTexResource->GetDeviceDataSize();
		}
		else if (pTexResource->GetFlags() & FT_USAGE_DYNAMIC)
		{
			pDynTextures.push_back(pTexResource);
			nSizeTotalDynTex += pTexResource->GetDeviceDataSize();
		}
		else
		{
			pTextures.push_back(pTexResource);
			nSizeTotalTex += pTexResource->GetDeviceDataSize();
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
		fprintf(fp, "\nRender targets (%d): %.1f kb, Dynamic textures (%d): %.1f kb, Textures (%d): %.1f kb", pRenderTargets.size(), BYTES_TO_KB(nSizeTotalRT), pDynTextures.size(), BYTES_TO_KB(nSizeTotalDynTex), pTextures.size(), BYTES_TO_KB(nSizeTotalTex));

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

	char str[512];
	pSH->m_pCurInst = pInst;
	cry_strcpy(str, pSH->m_EntryFunc.c_str());
	uint32 nSize = strlen(str);
	pSH->mfGenName(pInst, &str[nSize], 512 - nSize, 1);

	ColorF col = Col_Green;
	Draw2dLabel(nX, nY, 1.6f, &col.r, false, "Shader: %s (%d instructions)", str, pInst->m_nInstructions);
	nX += 10;
	nY += 25;

	SD3DShader* pHWS = pInst->m_Handle.m_pShader;
	if (!pHWS || !pHWS->m_pHandle)
		return;
	if (!pInst->m_pShaderData)
		return;
	byte* pData = NULL;
	ID3D10Blob* pAsm = NULL;
	D3DDisassemble((UINT*)pInst->m_pShaderData, pInst->m_nDataSize, 0, NULL, &pAsm);
	if (!pAsm)
		return;
	char* szAsm = (char*)pAsm->GetBufferPointer();
	int nMaxY = m_height;
	int nM = 0;
	while (szAsm[0])
	{
		fxFillCR(&szAsm, str);
		Draw2dLabel(nX, nY, 1.2f, &colSH.r, false, "%s", str);
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

void CD3D9Renderer::DebugDrawStats8()
{
#if !defined(_RELEASE) && defined(ENABLE_PROFILING_CODE)
	ColorF col = Col_White;
	Draw2dLabel(30, 50, 1.2f, &col.r, false, "%d total instanced DIPs in %d batches", m_RP.m_PS[m_RP.m_nProcessThreadID].m_nInsts, m_RP.m_PS[m_RP.m_nProcessThreadID].m_nInstCalls);
#endif
}

void CD3D9Renderer::DebugDrawStats2()
{
#if !defined(EXCLUDE_RARELY_USED_R_STATS)
	const int nYstep = 10;
	int nY = 30; // initial Y pos
	int nX = 20; // initial X pos
	static int snTech = 0;

	if (!g_SelectedTechs.size())
		return;

	if (CryGetAsyncKeyState('0') & 0x1)
		snTech = 0;
	if (CryGetAsyncKeyState('1') & 0x1)
		snTech = 1;
	if (CryGetAsyncKeyState('2') & 0x1)
		snTech = 2;
	if (CryGetAsyncKeyState('3') & 0x1)
		snTech = 3;
	if (CryGetAsyncKeyState('4') & 0x1)
		snTech = 4;
	if (CryGetAsyncKeyState('5') & 0x1)
		snTech = 5;
	if (CryGetAsyncKeyState('6') & 0x1)
		snTech = 6;
	if (CryGetAsyncKeyState('7') & 0x1)
		snTech = 7;
	if (CryGetAsyncKeyState('8') & 0x1)
		snTech = 8;
	if (CryGetAsyncKeyState('9') & 0x1)
		snTech = 9;

	TArray<SShaderTechniqueStat*> Techs;
	uint32 i;
	int j;
	for (i = 0; i < g_SelectedTechs.size(); i++)
	{
		SShaderTechniqueStat* pTech = &g_SelectedTechs[i];
		for (j = 0; j < (int)Techs.Num(); j++)
		{
			if (Techs[j]->pVSInst == pTech->pVSInst && Techs[j]->pPSInst == pTech->pPSInst)
				break;
		}
		if (j == Techs.Num())
			Techs.AddElem(pTech);
	}

	if (snTech >= (int)Techs.Num())
		snTech = Techs.Num() - 1;

	if (!(snTech >= 0 && snTech < (int)Techs.Num()))
		return;

	SShaderTechniqueStat* pTech = Techs[snTech];

	ColorF col = Col_Yellow;
	Draw2dLabel(nX, nY, 2.0f, &col.r, false, "FX Shader: %s, Technique: %s (%d out of %d), Pass: %d", pTech->pShader->GetName(), pTech->pTech->m_NameStr.c_str(), snTech, Techs.Num(), 0);
	nY += 25;

	CHWShader_D3D* pVS = pTech->pVS;
	CHWShader_D3D* pPS = pTech->pPS;
	DebugPrintShader(pTech->pVS, pTech->pVSInst, nX - 10, nY, Col_White);
	DebugPrintShader(pPS, pTech->pPSInst, nX + 450, nY, Col_Cyan);
#endif
}

void CD3D9Renderer::DebugDrawStats20()
{
#if !defined(_RELEASE) && defined(ENABLE_PROFILING_CODE)
	ColorF col = Col_Yellow;
	Draw2dLabel(30, 50, 1.5f, &col.r, false, "Compiled Render Objects");
	Draw2dLabel(30, 80, 1.5f, &col.r, false, "Objects: Modified: %-5d  Temporary: %-5d  Incomplete: %-5d",
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nModifiedCompiledObjects,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nTempCompiledObjects,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nIncompleteCompiledObjects);
	Draw2dLabel(30, 110, 1.5f, &col.r, false, "State Changes: PSO [%d] PT [%d] L [%d] I [%d] RS [%d]",
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumPSOSwitches,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumTopologySets,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumLayoutSwitches,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumInlineSets,
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumResourceSetSwitches);
	Draw2dLabel(30, 140, 1.5f, &col.r, false, "Resource bindings Mem[GPU,CPU]: VB [%d,%d] IB [%d,%d] CB [%d,%d] / [%d,%d] UB [%d,%d] TEX [%d,%d]",
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundVertexBuffers[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundVertexBuffers[1],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundIndexBuffers[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundIndexBuffers[1],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundConstBuffers[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundConstBuffers[1],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundInlineBuffers[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundInlineBuffers[1],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundUniformBuffers[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundUniformBuffers[1],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundUniformTextures[0],
	            m_RP.m_PS[m_RP.m_nProcessThreadID].m_nNumBoundUniformTextures[1]);
#endif
}

void CD3D9Renderer::DebugDrawStats()
{
#ifndef _RELEASE
	if (CV_r_stats)
	{
		CRenderer* crend = gRenDev;

		CCryNameTSCRC Name;
		switch (CV_r_stats)
		{
		case 1:
			DebugDrawStats1();
			break;
		case 2:
			DebugDrawStats2();
			break;
		case 3:
			DebugPerfBars(40, 50);
			DebugVidResourcesBars(450, 80);
			break;
		case 4:
			DebugPerfBars(40, 50);
			break;
		case 8:
			DebugDrawStats8();
			break;
		case 13:
			EF_PrintRTStats("Cleared Render Targets:");
			break;
		case 20:
			DebugDrawStats20();
			break;
		case 5:
			{
				const int nYstep = 30;
				int nYpos = 270; // initial Y pos
				crend->WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery stats:%d",
				               CREOcclusionQuery::m_nQueriesPerFrameCounter);
				crend->WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nQueriesPerFrameCounter=%d",
				               CREOcclusionQuery::m_nQueriesPerFrameCounter);
				crend->WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nReadResultNowCounter=%d",
				               CREOcclusionQuery::m_nReadResultNowCounter);
				crend->WriteXY(10, nYpos += nYstep, 2, 2, 1, 1, 1, 1, "CREOcclusionQuery::m_nReadResultTryCounter=%d",
				               CREOcclusionQuery::m_nReadResultTryCounter);
			}
			break;

		case 6:
			{
				ColorF clrDPBlue = ColorF(0, 1, 1, 1);
				ColorF clrDPRed = ColorF(1, 0, 0, 1);
				ColorF clrDPInterp = ColorF(1, 0, 0, 1);
				;
				ColorF clrInfo = ColorF(1, 1, 0, 1);

				IRenderer::RNDrawcallsMapNodeItor pEnd = m_RP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID].end();
				IRenderer::RNDrawcallsMapNodeItor pItor = m_RP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID].begin();
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

					DrawLabelEx(pInfo.pPos, 1.3f, pfColor, true, true, "DP: %d (%d/%d/%d/%d/%d)",
					            nDrawcalls, pInfo.nZpass, pInfo.nGeneral, pInfo.nTransparent, pInfo.nShadows, pInfo.nMisc);
				}

				Draw2dLabel(40.f, 40.f, 1.3f, &clrInfo.r, false, "Instance drawcall count (zpass/general/transparent/shadows/misc)");
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

			IRenderer::RNDrawcallsMapNodeItor pEnd = m_RP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID].end();
			IRenderer::RNDrawcallsMapNodeItor pItor = m_RP.m_pRNDrawCallsInfoPerNode[m_RP.m_nProcessThreadID].begin();
			for (; pItor != pEnd; ++pItor)
			{
				IRenderNode* pRenderNode = pItor->first;

				//display info for render node under debug gun
				if (pRenderNode == m_pDebugRenderNode)
				{
					SDrawCallCountInfo& pInfo = pItor->second;

					uint32 nDrawcalls = pInfo.nShadows + pInfo.nZpass + pInfo.nGeneral + pInfo.nTransparent + pInfo.nMisc;

					Draw2dLabel(970.f, 65.f, 1.5f, yellow, false, "Draw calls: %d \n  zpass: %d\n  general: %d\n  transparent: %d\n  shadows: %d\n  misc: %d\n",
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
	m_pRT->RC_RenderDebug(bRenderStats);
}

void CD3D9Renderer::RT_RenderDebug(bool bRenderStats)
{
	if (gEnv->IsEditor() && !m_CurrContext->m_bMainViewport)
		return;

	if (m_bDeviceLost)
		return;
#if !defined(_RELEASE)
	if (CV_r_showbufferusage)
	{
		const uint32 xStartCoord = 695;
		int YStep = 12;
	}

	#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
	if (CV_r_RefractionPartialResolvesDebug)
	{
		SPipeStat* pRP = &m_RP.m_PS[m_RP.m_nFillThreadID];
		const float xPos = 0.0f;
		float yPos = 90.0f;
		const float textYSpacing = 18.0f;
		const float size = 1.5f;
		const ColorF titleColor = Col_SpringGreen;
		const ColorF textColor = Col_Yellow;
		const bool bCentre = false;

		float fInvScreenArea = 1.0f / ((float)GetWidth() * (float)GetHeight());

		Draw2dLabel(xPos, yPos, size, &titleColor.r, bCentre, "Partial Resolves");
		yPos += textYSpacing;
		Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Count: %d", pRP->m_refractionPartialResolveCount);
		yPos += textYSpacing;
		Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Pixels: %d", pRP->m_refractionPartialResolvePixelCount);
		yPos += textYSpacing;
		Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Percentage of Screen area: %d", (int) (pRP->m_refractionPartialResolvePixelCount * fInvScreenArea * 100.0f));
		yPos += textYSpacing;
		Draw2dLabel(xPos, yPos, size, &textColor.r, bCentre, "Estimated cost: %.2fms", pRP->m_fRefractionPartialResolveEstimatedCost);
	}
	#endif

	#ifndef EXCLUDE_DOCUMENTATION_PURPOSE
	if (CV_r_DebugFontRendering)
	{
		const float fPixelPerfectScale = 16.0f / UIDRAW_TEXTSIZEFACTOR * 2.0f;    // we have to compensate  vSize.x/16.0f; and pFont->SetCharWidthScale(0.5f);
		const int line = 10;

		float y = 0;
		SDrawTextInfo ti;
		ti.flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;

		ti.color[2] = 0.0f;
		DrawTextQueued(Vec3(0, y += line, 0), ti, "Colors (black,white,blue,..): { $00$11$22$33$44$55$66$77$88$99$$$o } ()_!+*/# ?");
		ti.color[2] = 1.0f;
		DrawTextQueued(Vec3(0, y += line, 0), ti, "Colors (black,white,blue,..): { $00$11$22$33$44$55$66$77$88$99$$$o } ()_!+*/# ?");

		for (int iPass = 0; iPass < 3; ++iPass)      // left, center, right
		{
			ti.xscale = ti.yscale = fPixelPerfectScale * 0.5f;
			float x = 0;

			y = 3 * line;

			int passflags = eDrawText_2D;

			if (iPass == 1)
			{
				passflags |= eDrawText_Center;
				x = (float)GetWidth() * 0.5f;
			}
			else if (iPass == 2)
			{
				x = (float)GetWidth();
				passflags |= eDrawText_Right;
			}

			ti.color[3] = 0.5f;
			ti.flags = passflags | eDrawText_FixedSize | eDrawText_Monospace;
			DrawTextQueued(Vec3(x, y += line, 0), ti, "0");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !MMMMM!");

			ti.color[0] = 0.0f;
			ti.color[3] = 1.0f;
			ti.flags = passflags | eDrawText_FixedSize;
			DrawTextQueued(Vec3(x, y += line, 0), ti, "1");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !MMMMM!");
			/*
			      ti.flags = passflags | eDrawText_Monospace;
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"2");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !.....!");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !MMMMM!");

			      ti.flags = passflags;
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"3");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !.....!");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !MMMMM!");
			 */
			ti.color[1] = 0.0f;
			ti.flags = passflags | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace;
			DrawTextQueued(Vec3(x, y += line, 0), ti, "4");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !MMMMM!");

			ti.color[0] = 1.0f;
			ti.color[1] = 1.0f;
			ti.flags = passflags | eDrawText_800x600 | eDrawText_FixedSize;
			DrawTextQueued(Vec3(x, y += line, 0), ti, "5");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line, 0), ti, "AbcW !MMMMM!");

			/*
			      ti.flags = passflags | eDrawText_800x600 | eDrawText_Monospace;
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"6");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"Halfsize");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !MMMMM!");

			      ti.flags = passflags | eDrawText_800x600;
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"7");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !.....!");
			      DrawTextQueued(Vec3(x,y+=line,0),ti,"AbcW !MMMMM!");
			 */
			// Pixel Perfect (1:1 mapping of the texels to the pixels on the screeen)

			ti.flags = passflags | eDrawText_FixedSize | eDrawText_Monospace;
			ti.xscale = ti.yscale = fPixelPerfectScale;
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "8");
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "AbcW !MMMMM!");

			ti.flags = passflags | eDrawText_FixedSize;
			ti.xscale = ti.yscale = fPixelPerfectScale;
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "9");
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "AbcW !.....!");
			DrawTextQueued(Vec3(x, y += line * 2, 0), ti, "AbcW !MMMMM!");
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
		int wdt = m_width;
		int hgt = m_height;

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

		int wdt = m_width;
		int hgt = m_height;
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

		Set2DMode(true, m_width, m_height);
		ColorF col = Col_White;
		int num = CTexture::s_ptexWhite->GetID();
		DrawImage((float)nC, (float)(hgt - 280), 1, 256, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
		Set2DMode(false, m_width, m_height);

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
			WriteXY(4, hgt - 280, 1, 1, col.r, col.g, col.b, 1, "UploadMB (%d-%d)", (int)(BYTES_TO_MB(CTexture::s_nTexturesDataBytesUploaded)), (int)fScaleUpl);
		}

		if (sMask & 2)
		{
			f = m_RP.m_PS[m_RP.m_nProcessThreadID].m_fTexUploadTime / fScaleTimeUpl;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgTimeUpl[nC] = (byte)f;
			Graph(fgTimeUpl, 0, hgt - 280, wdt, 256, nC, type, NULL, ColTimeUpl, fScaleTimeUpl);
			col = ColTimeUpl;
			WriteXY(4, hgt - 280 + 16, 1, 1, col.r, col.g, col.b, 1, "Upload Time (%.3fMs - %.3fMs)", m_RP.m_PS[m_RP.m_nProcessThreadID].m_fTexUploadTime, fScaleTimeUpl);
		}

		if (sMask & 4)
		{
			f = BYTES_TO_MB(CTexture::s_nTexturesDataBytesLoaded) / fScaleStreamSync;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgStreamSync[nC] = (byte)f;
			Graph(fgStreamSync, 0, hgt - 280, wdt, 256, nC, type, NULL, ColStreamSync, fScaleStreamSync);
			col = ColStreamSync;
			WriteXY(4, hgt - 280 + 16 * 2, 1, 1, col.r, col.g, col.b, 1, "StreamMB (%d-%d)", (int)BYTES_TO_MB(CTexture::s_nTexturesDataBytesLoaded), (int)fScaleStreamSync);
		}

		if (sMask & 32)
		{
			size_t pool = CTexture::s_nStatsStreamPoolInUseMem;
			f = BYTES_TO_MB(pool) / fScaleTotalMem;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgTotalMem[nC] = (byte)f;
			Graph(fgTotalMem, 0, hgt - 280, wdt, 256, nC, type, NULL, ColTotalMem, fScaleTotalMem);
			col = ColTotalMem;
			WriteXY(4, hgt - 280 + 16 * 5, 1, 1, col.r, col.g, col.b, 1, "Streaming textures pool used (Mb) (%d of %d)", (int)BYTES_TO_MB(pool), (int)fScaleTotalMem);
		}
		if (sMask & 64)
		{
			f = (BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize + m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize + m_RP.m_PS[m_RP.m_nProcessThreadID].m_DynTexturesSize)) / fScaleCurMem;
			f = 255.0f - CLAMP(f * 255.0f, 0, 255.0f);
			fgCurMem[nC] = (byte)f;
			Graph(fgCurMem, 0, hgt - 280, wdt, 256, nC, type, NULL, ColCurMem, fScaleCurMem);
			col = ColCurMem;
			WriteXY(4, hgt - 280 + 16 * 6, 1, 1, col.r, col.g, col.b, 1, "Cur Scene Size: Dyn. + Stat. (Mb) (%d-%d)", (int)(BYTES_TO_MB(m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesSysMemSize + m_RP.m_PS[m_RP.m_nProcessThreadID].m_ManagedTexturesStreamSysSize + m_RP.m_PS[m_RP.m_nProcessThreadID].m_DynTexturesSize)), (int)fScaleCurMem);
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
			WriteXY(4, hgt - 280 + 14 * 7, 1, 1, col.r, col.g, col.b, 1, "Streaming throughput (Kb/s) (%d of %d)", (int)thp, (int)fScaleStreaming * 1024);

			// output assets
			if (!vecStreamingProblematicAssets.empty())
			{
				int top = vecStreamingProblematicAssets.size() * nLineStep + 320;
				WriteXY(4, hgt - top - nLineStep, 1, 1, col.r, col.g, col.b, 1, "Problematic assets:");
				for (int i = vecStreamingProblematicAssets.size() - 1; i >= 0; --i)
					WriteXY(4, hgt - top + nLineStep * i, 1, 1, col.r, col.g, col.b, 1, "[%.1fKB] '%s'", BYTES_TO_KB(vecStreamingProblematicAssets[i].m_nSize), vecStreamingProblematicAssets[i].m_sName.c_str());
			}
		}
		nC++;
		if (nC == wdt)
			nC = 0;
	}
	else
		vecStreamingProblematicAssets.clear();

	PostMeasureOverdraw();

	DrawTexelsPerMeterInfo();

	if (m_pColorGradingControllerD3D)
		m_pColorGradingControllerD3D->DrawDebugInfo();

	double time = 0;
	ticks(time);

	if (bRenderStats)
		DebugDrawStats();

	VidMemLog();

	{
		// print shadow maps on the screen
		static ICVar* pVar = iConsole->GetCVar("e_ShadowsDebug");
		if (pVar && pVar->GetIVal() >= 1 && pVar->GetIVal() <= 2)
			DrawAllShadowsOnTheScreen();
	}
	#endif

	if (CV_r_DeferredShadingDebug == 1)
	{
		// Draw a custom RT list for deferred rendering
		m_showRenderTargetInfo.rtList.clear();
		m_showRenderTargetInfo.bShowList = false;

		SShowRenderTargetInfo::RT rt;
		rt.bFiltered = false;
		rt.bRGBKEncoded = false;
		rt.bAliased = false;
		rt.channelWeight = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		rt.pTexture = CTexture::s_ptexZTarget;
		rt.channelWeight = Vec4(10.0f, 10.0f, 10.0f, 1.0f);
		m_showRenderTargetInfo.rtList.push_back(rt);
		rt.channelWeight = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
		rt.pTexture = CTexture::s_ptexSceneNormalsMap;
		m_showRenderTargetInfo.rtList.push_back(rt);

		rt.pTexture = CTexture::s_ptexCurrentSceneDiffuseAccMap;
		m_showRenderTargetInfo.rtList.push_back(rt);

		rt.pTexture = CTexture::s_ptexSceneSpecularAccMap;
		m_showRenderTargetInfo.rtList.push_back(rt);

		DebugShowRenderTarget();
		m_showRenderTargetInfo.rtList.clear();
	}

	if (CV_r_DeferredShadingDebugGBuffer >= 1 && CV_r_DeferredShadingDebugGBuffer <= 9)
	{
		m_showRenderTargetInfo.rtList.clear();
		m_showRenderTargetInfo.bShowList = false;
		m_showRenderTargetInfo.bDisplayTransparent = true;
		m_showRenderTargetInfo.col = 1;

		SShowRenderTargetInfo::RT rt;
		rt.bFiltered = false;
		rt.bRGBKEncoded = false;
		rt.bAliased = false;
		rt.channelWeight = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		rt.pTexture = CTexture::s_ptexStereoR;
		m_showRenderTargetInfo.rtList.push_back(rt);

		DebugShowRenderTarget();
		m_showRenderTargetInfo.rtList.clear();

		char* nameList[9] = { "Normals", "Smoothness", "Reflectance", "Albedo", "Lighting model", "Translucency", "Sun self-shadowing", "Subsurface Scattering", "Specular Validation" };
		char* descList[9] = {
			"", "", "", "", "gray: standard -- yellow: transmittance -- blue: pom self-shadowing", "", "", "",
			"blue: too low -- orange: too high and not yet metal -- pink: just valid for oxidized metal/rust"
		};
		WriteXY(10, 10, 1.0f, 1.0f, 0, 1, 0, 1, "%s", nameList[clamp_tpl(CV_r_DeferredShadingDebugGBuffer - 1, 0, 8)]);
		WriteXY(10, 30, 0.85f, 0.85f, 0, 1, 0, 1, "%s", descList[clamp_tpl(CV_r_DeferredShadingDebugGBuffer - 1, 0, 8)]);
	}

	if (m_showRenderTargetInfo.bShowList)
	{
		iLog->Log("RenderTargets:\n");
		for (size_t i = 0; i < m_showRenderTargetInfo.rtList.size(); ++i)
		{
			CTexture* pTex = m_showRenderTargetInfo.rtList[i].pTexture;
			if (pTex)
				iLog->Log("\t%" PRISIZE_T "  %s\t--------------%s  %d x %d\n", i, pTex->GetName(), pTex->GetFormatName(), pTex->GetWidth(), pTex->GetHeight());
			else
				iLog->Log("\t%" PRISIZE_T "  %s\t--------------(NOT INITIALIZED)\n", i, pTex->GetName());
		}

		m_showRenderTargetInfo.Reset();
	}
	else if (m_showRenderTargetInfo.rtList.empty() == false)
	{
		DebugShowRenderTarget();
	}

	static char r_showTexture_prevString[256] = "";  // a wrokaround to reset the "??" command
	// show custom texture
	if (CV_r_ShowTexture)
	{
		const char* arg = CV_r_ShowTexture->GetString();

		SetState(GS_NODEPTHTEST);
		int iTmpX, iTmpY, iTempWidth, iTempHeight;
		GetViewport(&iTmpX, &iTmpY, &iTempWidth, &iTempHeight);
		// SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
		Set2DMode(true, 1, 1);

		char* endp;
		int texId = strtol(arg, &endp, 10);

		if (endp != arg)
		{
			CTexture* pTexToShow = CTexture::GetByID(texId);

			if (pTexToShow)
			{
				RT_SetViewport(m_width - m_width / 3 - 10, m_height - m_height / 3 - 10, m_width / 3, m_height / 3);
				DrawImage(0, 0, 1, 1, pTexToShow->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
				WriteXY(10, 10, 1, 1, 1, 1, 1, 1, "Name: %s", pTexToShow->GetSourceName());
				WriteXY(10, 25, 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", pTexToShow->GetFormatName(), CTexture::NameForTextureType(pTexToShow->GetTextureType()));
				WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight(), pTexToShow->GetDepth());
				WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight());
				WriteXY(10, 55, 1, 1, 1, 1, 1, 1, "Mips: %d", pTexToShow->GetNumMips());
			}
		}
		else if (strlen(arg) == 2)
		{
			// Special cmd
			if (strcmp(arg, "??") == 0)
			{
				// print all entries in the index book
				iLog->Log("All entries:\n");

				AUTO_LOCK(CBaseResource::s_cResLock);

				const CCryNameTSCRC& resClass = CTexture::mfGetClassName();
				const SResourceContainer* pRes = CBaseResource::GetResourcesForClass(resClass);
				if (pRes)
				{
					const size_t size = pRes->m_RList.size();
					for (size_t i = 0; i < size; ++i)
					{
						CTexture* pTex = (CTexture*) pRes->m_RList[i];
						if (pTex)
						{
							const char* pName = pTex->GetName();
							if (pName && !strchr(pName, '/'))
							{
								iLog->Log("\t%" PRISIZE_T " %s -- fmt: %s, dim: %d x %d\n", i, pName, pTex->GetFormatName(), pTex->GetWidth(), pTex->GetHeight());
							}
						}
					}
				}

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
				RT_SetViewport(m_width - m_width / 3 - 10, m_height - m_height / 3 - 10, m_width / 3, m_height / 3);
				DrawImage(0, 0, 1, 1, pTexToShow->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
				WriteXY(10, 10, 1, 1, 1, 1, 1, 1, "Name: %s", pTexToShow->GetSourceName());
				WriteXY(10, 25, 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", pTexToShow->GetFormatName(), CTexture::NameForTextureType(pTexToShow->GetTextureType()));
				WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight(), pTexToShow->GetDepth());
				WriteXY(10, 40, 1, 1, 1, 1, 1, 1, "Size: %dx%d", pTexToShow->GetWidth(), pTexToShow->GetHeight());
				WriteXY(10, 55, 1, 1, 1, 1, 1, 1, "Mips: %d", pTexToShow->GetNumMips());
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
				RT_SetViewport(0, 0, m_width, m_height);
				const float tileW = 0.24f;
				const float tileH = 0.24f;
				const float tileGapW = 5.f / m_width;
				const float tileGapH = 25.f / m_height;

				const int maxTilesInRow = (int) ((1 - tileGapW) / (tileW + tileGapW));
				for (size_t i = 0; i < nameList.size(); i++)
				{
					CTexture* tex = CTexture::GetByName(nameList[i].c_str());
					if (!tex)  continue;

					int row = i / maxTilesInRow;
					int col = i - row * maxTilesInRow;
					float curX = tileGapW + col * (tileW + tileGapW);
					float curY = tileGapH + row * (tileH + tileGapH);
					gcpRendD3D->FX_SetState(GS_NODEPTHTEST);  // fix the state change by using WriteXY
					DrawImage(curX, curY, tileW, tileH, tex->GetID(), 0, 1, 1, 0, 1, 1, 1, 1, true);
					WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 - 15), 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", tex->GetFormatName(), CTexture::NameForTextureType(tex->GetTextureType()));
					WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 + 1), 1, 1, 1, 1, 1, 1, "%s   %d x %d", nameList[i].c_str(), tex->GetWidth(), tex->GetHeight());
				}
			}
		}

		RT_SetViewport(iTmpX, iTmpY, iTempWidth, iTempHeight);
		Set2DMode(false, 1, 1);
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
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	//////////////////////////////////////////////////////////////////////
	// End the scene and update
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	m_pRT->RC_TryFlush();
}

void CD3D9Renderer::EndFrame()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

	//////////////////////////////////////////////////////////////////////
	// End the scene and update
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

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

void CD3D9Renderer::IssueFrameFences()
{
	STATIC_ASSERT(CRY_ARRAY_COUNT(m_frameFences) == MAX_FRAMES_IN_FLIGHT, "Unexpected size for m_frameFences");

	if (!m_frameFences[0])
	{
		m_frameFenceCounter = MAX_FRAMES_IN_FLIGHT;
		for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		{
			HRESULT hr = m_DevMan.CreateFence(m_frameFences[i]);
			assert(hr == S_OK);
		}
		return;
	}

	HRESULT hr = gRenDev->m_DevMan.IssueFence(m_frameFences[m_frameFenceCounter % MAX_FRAMES_IN_FLIGHT]);
	assert(hr == S_OK);

	if (CV_r_SyncToFrameFence)
	{
		// Stall render thread until GPU has finished processing previous frame (in case max frame latency is 1)
		PROFILE_FRAME("WAIT FOR GPU");
		HRESULT hr = gRenDev->m_DevMan.SyncFence(m_frameFences[(m_frameFenceCounter - (MAX_FRAMES_IN_FLIGHT - 1)) % MAX_FRAMES_IN_FLIGHT], true, true);
		assert(hr == S_OK);
	}

	m_completedFrameFenceCounter = m_frameFenceCounter - (MAX_FRAMES_IN_FLIGHT - 1);
	m_frameFenceCounter += 1;
}

void CD3D9Renderer::RT_EndFrame()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

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

	CTexture::Update();
	CFlashTextureSourceSharedRT::TickRT();

	if (m_CVDisplayInfo && m_CVDisplayInfo->GetIVal() && iSystem && iSystem->IsDevMode())
	{
		static const int nIconSize = 32;
		int nIconIndex = 0;

		FX_SetState(GS_NODEPTHTEST);

		const Vec2 overscanOffset = Vec2(s_overscanBorders.x * VIRTUAL_SCREEN_WIDTH, s_overscanBorders.y * VIRTUAL_SCREEN_HEIGHT);

		if (SShaderAsyncInfo::s_nPendingAsyncShaders > 0 && CV_r_shadersasynccompiling > 0 && CTexture::s_ptexIconShaderCompiling)
			Push2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CTexture::s_ptexIconShaderCompiling->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		if (CTexture::IsStreamingInProgress() && CTexture::s_ptexIconStreaming)
			Push2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CTexture::s_ptexIconStreaming->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		// indicate terrain texture streaming activity
		if (gEnv->p3DEngine && CTexture::s_ptexIconStreamingTerrainTexture && gEnv->p3DEngine->IsTerrainTextureStreamingInProgress())
			Push2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CTexture::s_ptexIconStreamingTerrainTexture->GetID(), 0, 1, 1, 0);

		++nIconIndex;

		if (IAISystem* pAISystem = gEnv->pAISystem)
		{
			if (INavigationSystem* pAINavigation = pAISystem->GetNavigationSystem())
			{
				if (pAINavigation->GetState() == INavigationSystem::Working)
				{
					Push2dImage(nIconSize * nIconIndex + overscanOffset.x, overscanOffset.y, nIconSize, nIconSize, CTexture::s_ptexIconNavigationProcessing->GetID(), 0, 1, 1, 0);
				}
			}
		}

		++nIconIndex;

		if (!gEnv->pMonoRuntime)
			Draw2dImageList();
	}

	if (gEnv->pMonoRuntime)
		Draw2dImageList();

	m_prevCamera = m_RP.m_TI[m_RP.m_nProcessThreadID].m_cam;

	m_nDisableTemporalEffects = max(0, m_nDisableTemporalEffects - 1);

	//////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
	{
		assert(1 == m_SceneRecurseCount);
		// in case of MT rendering flush render thread CB is flashed (main thread CB gets flushed in SRenderThread::FlushFrame()),
		// otherwise unified "main-render" tread CB is flashed

		if (CAuxGeomCB* pAuxGeomCB = m_pRenderAuxGeomD3D->GetRenderAuxGeom())
		{
			pAuxGeomCB->Commit();
		}

		m_pRenderAuxGeomD3D->Process();
	}
#endif

	EF_RemoveParticlesFromScene();

	FX_SetState(GS_NODEPTHTEST);

	//char str[1024];
	//cry_sprintf(str, "Frame: %d", m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID);
	//PrintToScreen(5,50, 16, str);

	if (!IsRecursiveRenderView())
	{
		if (gEnv->pConsole)
		{
			//PROFILE_LABEL_PUSH_SKIP_GPU("Text");
			RT_FlushTextMessages();
			//PROFILE_LABEL_POP_SKIP_GPU("Text");
		}
	}

	//PROFILE_LABEL_POP_SKIP_GPU("Frame");

	if (gEnv && gEnv->pHardwareMouse)
		gEnv->pHardwareMouse->Render();

	GetS3DRend().DisplayStereo();

	CTimeValue timePresentBegin = iTimer->GetAsyncTime();
	GetS3DRend().SubmitFrameToHMD();

	if (m_pPipelineProfiler)
	{
		m_pPipelineProfiler->EndFrame();
		m_pPipelineProfiler->DisplayUI();
	}

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
	if (!m_bDeviceLost)
		FX_Commit();

	// Flip the back buffer to the front
	bool bSetActive = false;
	if (m_bSwapBuffers)
	{
		if (IsEditorMode())
		{
			ResolveSupersampledBackbuffer();
		}

		CaptureFrameBuffer();

		CaptureFrameBufferCallBack();

		if (!IsEditorMode())
		{
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
#if CRY_PLATFORM_ORBIS
			hReturn = m_pSwapChain->Present(m_VSync ? 1 : 0, 0);
			gRenDev->RT_UnbindTMUs();

			m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_pSwapChain);
#elif CRY_PLATFORM_DURANGO
	#if DURANGO_ENABLE_ASYNC_DIPS
			WaitForAsynchronousDevice();
	#endif
			hReturn = m_pSwapChain->Present(m_VSync ? 1 : 0, 0);
			gRenDev->RT_UnbindTMUs();

			m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_pSwapChain);
	#if DURANGO_ENABLE_ASYNC_DIPS
			WaitForAsynchronousDevice();
	#endif
#elif defined(SUPPORT_DEVICE_INFO)
	#if CRY_PLATFORM_WINDOWS
			m_devInfo.EnforceFullscreenPreemption();
	#endif
			DWORD syncInterval = ComputePresentInterval(m_devInfo.SyncInterval() != 0, m_devInfo.RefreshRate().Numerator, m_devInfo.RefreshRate().Denominator);
			DWORD presentFlags = m_devInfo.PresentFlags();
			hReturn = m_pSwapChain->Present(syncInterval, presentFlags);
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
	#if 0
		#ifdef DEBUG
			// CAPTURING NOTE: frame-capture exchanges the swap-chain on the next present the capture
			// is started, we can't cache the RTV in that case, and have to recreate it
			unsigned int b = GetCurrentBackBufferIndex(m_pSwapChain);
			{
				ID3D11Texture2D* pBackBuffer = 0;
				HRESULT hr = m_pSwapChain->GetBuffer(b, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

				SAFE_RELEASE(m_pBackBuffers[b]);
				if (SUCCEEDED(hr) && pBackBuffer)
				{
					{
						assert(!m_pBackBuffers[b]);
						hr = GetDevice().CreateRenderTargetView(pBackBuffer, 0, &m_pBackBuffers[b]);
					}

			#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
					char str[32] = "";
					sprintf(str, "Swap-Chain back buffer %d", b);
					pBackBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(str), str);
			#elif !defined(RELEASE) && CRY_PLATFORM_ORBIS
					char str[32] = "";
					sprintf(str, "Swap-Chain back buffer %d", b);
					pBackBuffer->DebugSetName(str);
			#endif
				}

				SAFE_RELEASE(pBackBuffer);
			}
		#endif
	#endif

			m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_pSwapChain);
			m_pBackBuffer = m_pBackBuffers[m_pCurrentBackBufferIndex];

			assert(m_nRTStackLevel[0] == 0);

			// TODO
			bool bIsVRActive = false;
			IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
			if (pHmdManager && pHmdManager->GetHmdDevice())
				bIsVRActive = true;

			bSetActive = FX_SetRenderTarget(0, m_pBackBuffer, bIsVRActive ? NULL : &m_DepthBufferNative, 0);
#endif
		}
		else
		{
			ScaleBackbufferToViewport();

#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
			CRY_ASSERT_MESSAGE(0, "Case in EndFrame() not implemented yet");
#elif defined(SUPPORT_DEVICE_INFO)
			DWORD dwFlags = 0;
			if (m_dwPresentStatus & (epsOccluded | epsNonExclusive))
				dwFlags = DXGI_PRESENT_TEST;
			else
				dwFlags = m_devInfo.PresentFlags();
			RECT ClientRect;
			ClientRect.top = 0;
			ClientRect.left = 0;
			ClientRect.right = m_CurrContext->m_Width;
			ClientRect.bottom = m_CurrContext->m_Height;
			//hReturn = m_pSwapChain->Present(0, dwFlags);
			if (m_CurrContext->m_pSwapChain)
			{
				hReturn = m_CurrContext->m_pSwapChain->Present(0, dwFlags);
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

				m_CurrContext->m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_CurrContext->m_pSwapChain);
				m_CurrContext->m_pBackBuffer = m_CurrContext->m_pBackBuffers[m_CurrContext->m_pCurrentBackBufferIndex];

				assert(m_nRTStackLevel[0] == 0);

				bSetActive = FX_SetRenderTarget(0, m_CurrContext->m_pBackBuffer, &m_DepthBufferNative, 0);
			}
#endif
		}

		m_nFrameSwapID++;
	}

	IssueFrameFences();

	m_fTimeWaitForGPU[m_RP.m_nProcessThreadID] += iTimer->GetAsyncTime().GetDifferenceInSeconds(timePresentBegin);

#ifdef ENABLE_BENCHMARK_SENSOR
	m_benchmarkRendererSensor->afterSwapBuffers(GetDevice(), GetDeviceContext());
#endif
	CheckDeviceLost();

	GetS3DRend().NotifyFrameFinished();

	// Disable screenshot code path for pure release builds on consoles
#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS || defined(ENABLE_LW_PROFILERS)
	if (CV_r_GetScreenShot)
	{
		ScreenShot();
		CV_r_GetScreenShot = 0;
	}
#endif
#ifndef CONSOLE_CONST_CVAR_MODE
	if (m_wireframe_mode != m_wireframe_mode_prev)
	{
		if (m_wireframe_mode > R_SOLID_MODE)  // disable zpass in wireframe mode
		{
			if (m_wireframe_mode_prev == R_SOLID_MODE)
			{
				m_nUseZpass = CV_r_usezpass;
				CV_r_usezpass = 0;
			}
		}
		else
		{
			CV_r_usezpass = m_nUseZpass;
		}
	}
#endif
	if (CRenderer::CV_r_logTexStreaming)
	{
		LogStrv(0, "******************************* EndFrame ********************************\n");
		LogStrv(0, "Loaded: %.3f Kb, UpLoaded: %.3f Kb, UploadTime: %.3fMs\n\n", BYTES_TO_KB(CTexture::s_nTexturesDataBytesLoaded), BYTES_TO_KB(CTexture::s_nTexturesDataBytesUploaded), m_RP.m_PS[m_RP.m_nProcessThreadID].m_fTexUploadTime);
	}

	if (!m_2dImages.empty())
	{
		m_2dImages.resize(0);
		assert(0);
	}

	/*if (CTexture::s_nTexturesDataBytesLoaded > 3.0f*1024.0f*1024.0f)
	   CTexture::m_fStreamDistFactor = min(2048.0f, CTexture::m_fStreamDistFactor*1.2f);
	   else
	   CTexture::m_fStreamDistFactor = max(1.0f, CTexture::m_fStreamDistFactor/1.2f);*/
	CTexture::s_nTexturesDataBytesUploaded = 0;
	CTexture::s_nTexturesDataBytesLoaded = 0;

	m_wireframe_mode_prev = m_wireframe_mode;

	m_SceneRecurseCount++;

	// we render directly to a video memory buffer
	// we need to unlock it here in case we renderered a frame without any particles
	// lock the VMEM buffer for the next frame here (to prevent a lock in the mainthread)
	// NOTE: main thread is already working on buffer+1 and we want to prepare the next one => hence buffer+2
	gRenDev->LockParticleVideoMemory();

#if CRY_PLATFORM_ORBIS
	m_fTimeGPUIdlePercent[m_RP.m_nProcessThreadID] = DXOrbis::Device()->GetGPUIdlePercentage();
	m_fTimeWaitForGPU[m_RP.m_nProcessThreadID] = DXOrbis::Device()->GetCPUWaitOnGPUTime();
	m_fTimeProcessedGPU[m_RP.m_nProcessThreadID] = DXOrbis::Device()->GetCPUFrameTime();
#else
	m_fTimeGPUIdlePercent[m_RP.m_nProcessThreadID] = 0;

	#if !defined(ENABLE_SIMPLE_GPU_TIMERS)
	// BK: We need a way of getting gpu frame time in release, without gpu timers
	// for now we just use overall frame time
	m_fTimeProcessedGPU[m_RP.m_nProcessThreadID] = m_fTimeProcessedRT[m_RP.m_nProcessThreadID];
	#else
	RPProfilerStats profileStats = m_pPipelineProfiler->GetBasicStats(eRPPSTATS_OverallFrame, m_RP.m_nProcessThreadID);
	m_fTimeProcessedGPU[m_RP.m_nProcessThreadID] = profileStats.gpuTime / 1000.0f;
	#endif
#endif

#if defined(USE_GEOM_CACHES)
	if (m_SceneRecurseCount == 1)
	{
		CREGeomCache::UpdateModified();
	}
#endif

#if defined(OPENGL)
	DXGLIssueFrameFences(GetDevice().GetRealDevice());
#endif //defined(OPENGL)

	// Note: Please be aware that the below has to be called AFTER the xenon texturemanager performs
	// Note: Please be aware that the below has to be called AFTER the texturemanager performs
	// it's garbage collection because a scheduled gpu copy might be pending
	// touching the memory that will be reclaimed below.
	m_DevBufMan.ReleaseEmptyBanks(m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID);

	if (m_bStopRendererAtFrameEnd)
	{
		m_mtxStopAtRenderFrameEnd.Lock();
		m_condStopAtRenderFrameEnd.Notify();
		m_condStopAtRenderFrameEnd.Wait(m_mtxStopAtRenderFrameEnd);
		m_mtxStopAtRenderFrameEnd.Unlock();
	}

	if (bSetActive)
	{
		FX_SetActiveRenderTargets();
	}
}

void CD3D9Renderer::RT_PresentFast()
{
	HRESULT hReturn = S_OK;
#if CRY_PLATFORM_DURANGO
	#if DURANGO_ENABLE_ASYNC_DIPS
	WaitForAsynchronousDevice();
	#endif
	hReturn = m_pSwapChain->Present(m_VSync ? 1 : 0, 0);
#elif CRY_PLATFORM_ORBIS
	hReturn = m_pSwapChain->Present(m_VSync ? 1 : 0, 0);
	gRenDev->RT_UnbindTMUs();
#elif defined(SUPPORT_DEVICE_INFO)

	GetS3DRend().DisplayStereo();
	GetS3DRend().NotifyFrameFinished();

	#if CRY_PLATFORM_WINDOWS
	m_devInfo.EnforceFullscreenPreemption();
	#endif
	DWORD syncInterval = m_devInfo.SyncInterval();
	DWORD presentFlags = m_devInfo.PresentFlags();
	hReturn = m_pSwapChain->Present(syncInterval, presentFlags);
#endif
	assert(hReturn == S_OK);

	m_pCurrentBackBufferIndex = GetCurrentBackBufferIndex(m_pSwapChain);
	m_pBackBuffer = m_pBackBuffers[m_pCurrentBackBufferIndex];

	assert(m_nRTStackLevel[0] == 0);

	FX_ClearTarget(m_pBackBuffer, Clr_Transparent, 0, nullptr);
	FX_SetRenderTarget(0, m_pBackBuffer, &m_DepthBufferNative, 0);
	//FX_SetActiveRenderTargets();

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_nFrameUpdateID++;
}

void CD3D9Renderer::FX_DrawRE(CShader* sh, SShaderPass* sl)
{
	int bFogOverrided = 0;
	// Unlock all VB (if needed) and set current streams
	FX_CommitStreams(sl);

	if (ShouldApplyFogCorrection())
		FX_FogCorrection();

	if (m_RP.m_pRE)
		m_RP.m_pRE->mfDraw(sh, sl);
	else
		FX_DrawIndexedMesh(eptTriangleList);
}

HRESULT CD3D9Renderer::FX_SetVStream(int nID, const void* pB, uint32 nOffs, uint32 nStride)
{
	FUNCTION_PROFILER_RENDER_FLAT
	D3DVertexBuffer* pVB = (D3DVertexBuffer*)pB;
	HRESULT h = S_OK;
	SRenderPipeline::SPipelineStreamInfo& sinfo = m_RP.m_VertexStreams[nID];
	if (sinfo.pStream != pVB || sinfo.nOffset != nOffs || sinfo.nStride != nStride)
	{
		sinfo.pStream = pVB;
		sinfo.nOffset = nOffs;
		sinfo.nStride = nStride;
		m_DevMan.BindVB(nID, 1, &pVB, &nOffs, &nStride);
	}

	//assert(h == S_OK);
	return h;
}

HRESULT CD3D9Renderer::FX_SetIStream(const void* pB, uint32 nOffs, RenderIndexType idxType)
{
#if !defined(_RELEASE) && !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	IF (idxType == Index32 || idxType == Index16 && (nOffs & 1), 0) __debugbreak();
#endif

	D3DIndexBuffer* pIB = (D3DIndexBuffer*)pB;
	HRESULT h = S_OK;
#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	if (m_RP.m_pIndexStream != pIB)
	{
		m_RP.m_pIndexStream = pIB;
		m_DevMan.BindIB(pIB, 0, DXGI_FORMAT_R16_UINT);
	}
	m_RP.m_IndexStreamOffset = nOffs;
	m_RP.m_IndexStreamType = idxType;
#else
	if (m_RP.m_pIndexStream != pIB || m_RP.m_IndexStreamOffset != nOffs || m_RP.m_IndexStreamType != idxType)
	{
		m_RP.m_pIndexStream = pIB;
		m_RP.m_IndexStreamOffset = nOffs;
		m_RP.m_IndexStreamType = idxType;
		m_DevMan.BindIB(pIB, nOffs, (DXGI_FORMAT)idxType);
	}
#endif

	assert(h == S_OK);
	return h;
}

HRESULT CD3D9Renderer::FX_ResetVertexDeclaration()
{
	GetDeviceContext().IASetInputLayout(NULL);
	m_pLastVDeclaration = NULL;
	return S_OK;
}

void CD3D9Renderer::FX_DrawIndexedPrimitive(const ERenderPrimitiveType eType, const int nVBOffset, const int nMinVertexIndex, const int nVerticesCount, const int nStartIndex, const int nNumIndices, bool bInstanced /*= false*/)
{
	int nPrimitives = 0;

	if (eType == eptTriangleList)
	{
		nPrimitives = nNumIndices / 3;
		assert(nNumIndices % 3 == 0);
	}
	else
	{
		switch (eType)
		{
		case ept4ControlPointPatchList:
			nPrimitives = nNumIndices >> 2;
			assert(nNumIndices % 4 == 0);
			break;
		case ept3ControlPointPatchList:
			nPrimitives = nNumIndices / 3;
			assert(nNumIndices % 3 == 0);
			break;
		case eptTriangleStrip:
			nPrimitives = nNumIndices - 2;
			assert(nNumIndices > 2);
			break;
		case eptLineList:
			nPrimitives = nNumIndices >> 1;
			assert(nNumIndices % 2 == 0);
			break;
#ifdef _DEBUG
		default:
			assert(0);    // not supported
			return;
#endif
		}
	}

	const D3DPrimitiveType eNativePType = FX_ConvertPrimitiveType(eType);

	SetPrimitiveTopology(eNativePType);
	if (bInstanced)
	{
		m_DevMan.DrawIndexedInstanced(nNumIndices, nVerticesCount, ApplyIndexBufferBindOffset(nStartIndex), nVBOffset, nVBOffset);
#if defined(ENABLE_PROFILING_CODE)
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[m_RP.m_nPassGroupDIP] += nPrimitives * nVerticesCount;
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[m_RP.m_nPassGroupDIP]++;
#endif
	}
	else
	{
		m_DevMan.DrawIndexed(nNumIndices, ApplyIndexBufferBindOffset(nStartIndex), nVBOffset);
#if defined(ENABLE_PROFILING_CODE)
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[m_RP.m_nPassGroupDIP] += nPrimitives;
		m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[m_RP.m_nPassGroupDIP]++;
#endif
	}
}

void CD3D9Renderer::FX_DrawPrimitive(const ERenderPrimitiveType eType, const int nStartVertex, const int nVerticesCount, const int nInstanceVertices /*= 0*/)
{
	int nPrimitives;
	if (nInstanceVertices)
		nPrimitives = nVerticesCount;
	else
		switch (eType)
		{
		case eptTriangleList:
			nPrimitives = nVerticesCount / 3;
			assert(nVerticesCount % 3 == 0);
			break;
		case eptTriangleStrip:
			nPrimitives = nVerticesCount - 2;
			assert(nVerticesCount > 2);
			break;
		case eptLineList:
			nPrimitives = nVerticesCount / 2;
			assert(nVerticesCount % 2 == 0);
			break;
		case eptLineStrip:
			nPrimitives = nVerticesCount - 1;
			assert(nVerticesCount > 1);
			break;
		case eptPointList:
		case ept1ControlPointPatchList:
			nPrimitives = nVerticesCount;
			assert(nVerticesCount > 0);
			break;
#ifndef _RELEASE
		default:
			assert(0);  // not supported
			return;
#endif
		}

	const D3DPrimitiveType eNativePType = FX_ConvertPrimitiveType(eType);

	SetPrimitiveTopology(eNativePType);
	if (nInstanceVertices)
		m_DevMan.DrawInstanced(nInstanceVertices, nVerticesCount, 0, nStartVertex);
	else
		m_DevMan.Draw(nVerticesCount, nStartVertex);

#if defined(ENABLE_PROFILING_CODE)
	m_RP.m_PS[m_RP.m_nProcessThreadID].m_nPolygons[m_RP.m_nPassGroupDIP] += nPrimitives;
	m_RP.m_PS[m_RP.m_nProcessThreadID].m_nDIPs[m_RP.m_nPassGroupDIP]++;
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CD3D9Renderer::ScreenShotInternal(const char* filename, int width, EScreenShotMode eScreenShotMode)
{
	// ignore invalid file access for screenshots
	CDebugAllowFileAccess ignoreInvalidFileAccess;

	bool bRet = true;
#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS || defined(ENABLE_LW_PROFILERS)
	if (m_pRT && !m_pRT->IsRenderThread() && eScreenShotMode == eScreenShotMode_Normal)
		m_pRT->FlushAndWait();

	if (!gEnv || !gEnv->pSystem || gEnv->pSystem->IsQuitting() || gEnv->bIsOutOfMemory)
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

	iLog->LogWithType(ILog::eInputResponse, " ");
	iLog->LogWithType(ILog::eInputResponse, "$5Drawcalls: %d", gEnv->pRenderer->GetCurrentNumberOfDrawCalls());
	iLog->LogWithType(ILog::eInputResponse, "$5FPS: %.1f (%.1f ms)", gEnv->pTimer->GetFrameRate(), gEnv->pTimer->GetFrameTime() * 1000.0f);

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
		GetS3DRend().TakeScreenshot(path);
		return true;
	}

	return CaptureFrameBufferToFile(path);

#endif//_RELEASE
	return bRet;
}

bool CD3D9Renderer::FX_ShouldTrackStats()
{
	bool bShouldTrackStats = (CV_r_stats == 6 || m_pDebugRenderNode || m_bCollectDrawCallsInfo || m_bCollectDrawCallsInfoPerNode) &&
	                         !(m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_MAKESPRITE);
	return bShouldTrackStats;
}

bool CD3D9Renderer::ScreenShot(const char* filename, int iPreWidth, EScreenShotMode eScreenShotMode)
{
	return ScreenShotInternal(filename, iPreWidth, eScreenShotMode);
}

int CD3D9Renderer::CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF)
{
	// check if parameters are valid
	if (!nWidth || !nHeight)
		return -1;

	if (!m_RTargets.Num())
	{
		m_RTargets.AddIndex(1);
	}

	int n = m_RTargets.Num();
	for (uint32 i = 1; i < m_RTargets.Num(); i++)
	{
		if (!m_RTargets[i])
		{
			n = i;
			break;
		}
	}

	if (n == m_RTargets.Num())
	{
		m_RTargets.AddIndex(1);
	}

	char pName[128];
	cry_sprintf(pName, "$RenderTarget_%d", n);
	m_RTargets[n] = CTexture::CreateRenderTarget(pName, nWidth, nHeight, cClear, eTT_2D, FT_NOMIPS, eTF);

	return m_RTargets[n]->GetID();
}

bool CD3D9Renderer::DestroyRenderTarget(int nHandle)
{
	CTexture* pTex = CTexture::GetByID(nHandle);
	SAFE_RELEASE(pTex);
	return true;
}

bool CD3D9Renderer::SetRenderTarget(int nHandle, int nFlags)
{
	if (nHandle == 0)
	{
		// Check: Restore not working
		FX_PopRenderTarget(0);
		return true;
	}

	CTexture* pTex = CTexture::GetByID(nHandle);
	gTexture2 = pTex;
	if (!pTex)
	{
		return false;
	}
	bool bScreenVP = (nFlags & SRF_SCREENTARGET) != 0;
	SDepthTexture* pDepthSurf = NULL;
	if (nFlags & SRF_USE_ORIG_DEPTHBUF)
		pDepthSurf = &m_DepthBufferOrig;
	else if (nFlags & SRF_USE_ORIG_DEPTHBUF_MSAA)
		pDepthSurf = &m_DepthBufferOrigMSAA;
	else
		pDepthSurf = FX_GetDepthSurface(pTex->GetWidth(), pTex->GetHeight(), false);

	bool bRes = FX_PushRenderTarget(0, pTex, pDepthSurf, -1, bScreenVP);
	FX_SetActiveRenderTargets();

	return bRes;
}

void CD3D9Renderer::ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY)
{
	m_pRT->RC_ReadFrameBuffer(pRGB, nImageX, nSizeX, nSizeY, eRBType, bRGBA, nScaledX, nScaledY);
}

void CD3D9Renderer::RT_ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX, int nScaledY)
{

#if defined(USE_D3DX) && CRY_PLATFORM_WINDOWS && !defined(OPENGL)
	if (!pRGB || nImageX <= 0 || nSizeX <= 0 || nSizeY <= 0 || eRBType != eRB_BackBuffer)
		return;

	assert(!IsEditorMode() || (m_CurrContext && m_CurrContext->m_pBackBuffers == m_pBackBuffers));

	// get last buffer used for Present()
	D3DSurface* pBackBuffer;
	pBackBuffer = m_pBackBuffers[(m_pCurrentBackBufferIndex + m_pBackBuffers.size() - 1) % m_pBackBuffers.size()];

	assert(pBackBuffer);

	D3DTexture* pBackBufferTex(0);

	D3D11_RENDER_TARGET_VIEW_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
	{
		// TODO: resolve
	}

	pBackBuffer->GetResource((ID3D11Resource**) &pBackBufferTex);
	if (pBackBufferTex)
	{
		D3D11_TEXTURE2D_DESC dstDesc;
		dstDesc.Width = nScaledX <= 0 ? nSizeX : nScaledX;
		dstDesc.Height = nScaledY <= 0 ? nSizeY : nScaledY;
		dstDesc.MipLevels = 1;
		dstDesc.ArraySize = 1;
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.SampleDesc.Count = 1;
		dstDesc.SampleDesc.Quality = 0;
		dstDesc.Usage = D3D11_USAGE_STAGING;
		dstDesc.BindFlags = 0;
		dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ /* | D3D11_CPU_ACCESS_WRITE*/;
		dstDesc.MiscFlags = 0;

		D3DTexture* pDstTex(0);
		if (SUCCEEDED(GetDevice().CreateTexture2D(&dstDesc, 0, &pDstTex)))
		{
			D3D11_BOX srcBox;
			srcBox.left = 0;
			srcBox.right = nSizeX;
			srcBox.top = 0;
			srcBox.bottom = nSizeY;
			srcBox.front = 0;
			srcBox.back = 1;

			D3D11_BOX dstBox;
			dstBox.left = 0;
			dstBox.right = dstDesc.Width;
			dstBox.top = 0;
			dstBox.bottom = dstDesc.Height;
			dstBox.front = 0;
			dstBox.back = 1;

			D3DX11_TEXTURE_LOAD_INFO loadInfo;
			loadInfo.pSrcBox = &srcBox;
			loadInfo.pDstBox = &dstBox;
			loadInfo.SrcFirstMip = 0;
			loadInfo.DstFirstMip = 0;
			loadInfo.NumMips = 1;
			loadInfo.SrcFirstElement = D3D11CalcSubresource(0, 0, 1);
			loadInfo.DstFirstElement = D3D11CalcSubresource(0, 0, 1);
			loadInfo.NumElements = 0;
			loadInfo.Filter = D3DX11_FILTER_LINEAR;
			loadInfo.MipFilter = D3DX11_FILTER_LINEAR;

			if (SUCCEEDED(D3DX11LoadTextureFromTexture(GetDeviceContext().GetRealDeviceContext(), pBackBufferTex, &loadInfo, pDstTex)))
			{
				D3D11_MAPPED_SUBRESOURCE mappedTex;
				STALL_PROFILER("lock/read texture")
				if (SUCCEEDED(GetDeviceContext().Map(pDstTex, 0, D3D11_MAP_READ, 0, &mappedTex)))
				{
					if (bRGBA)
					{
						for (unsigned int i = 0; i < dstDesc.Height; ++i)
						{
							uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
							uint8* pDst((uint8*) pRGB + (dstDesc.Height - 1 - i) * nImageX * 4);
							for (unsigned int j = 0; j < dstDesc.Width; ++j, pSrc += 4, pDst += 4)
							{
								pDst[0] = pSrc[2];
								pDst[1] = pSrc[1];
								pDst[2] = pSrc[0];
								pDst[3] = 255;
							}
						}
					}
					else
					{
						for (unsigned int i = 0; i < dstDesc.Height; ++i)
						{
							uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
							uint8* pDst((uint8*) pRGB + (dstDesc.Height - 1 - i) * nImageX * 3);
							for (unsigned int j = 0; j < dstDesc.Width; ++j, pSrc += 4, pDst += 3)
							{
								pDst[0] = pSrc[2];
								pDst[1] = pSrc[1];
								pDst[2] = pSrc[0];
							}
						}
					}
					GetDeviceContext().Unmap(pDstTex, 0);
				}
			}
		}
		SAFE_RELEASE(pDstTex);
	}
	SAFE_RELEASE(pBackBufferTex);
#endif
}

void CD3D9Renderer::ReadFrameBufferFast(uint32* pDstARGBA8, int dstWidth, int dstHeight)
{
	if (!pDstARGBA8 || dstWidth <= 0 || dstHeight <= 0)
		return;

#if CRY_PLATFORM_WINDOWS

	assert(!IsEditorMode() || (m_CurrContext && m_CurrContext->m_pBackBuffers == m_pBackBuffers));

	// get last buffer used for Present()
	D3DSurface* pBackBuffer;
	pBackBuffer = m_pBackBuffers[(m_pCurrentBackBufferIndex + m_pBackBuffers.size() - 1) % m_pBackBuffers.size()];

	assert(pBackBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
		return;

	D3DTexture* pBackBufferTex(0);
	pBackBuffer->GetResource((D3DResource**)&pBackBufferTex);
	if (pBackBufferTex)
	{
		D3D11_TEXTURE2D_DESC dstDesc;
		dstDesc.Width = dstWidth > GetWidth() ? GetWidth() : dstWidth;
		dstDesc.Height = dstHeight > GetHeight() ? GetHeight() : dstHeight;
		dstDesc.MipLevels = 1;
		dstDesc.ArraySize = 1;
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.SampleDesc.Count = 1;
		dstDesc.SampleDesc.Quality = 0;
		dstDesc.Usage = D3D11_USAGE_STAGING;
		dstDesc.BindFlags = 0;
		dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ /* | D3D11_CPU_ACCESS_WRITE*/;
		dstDesc.MiscFlags = 0;

		ID3D11Texture2D* pDstTex(0);
		if (SUCCEEDED(GetDevice().CreateTexture2D(&dstDesc, 0, &pDstTex)))
		{
			D3D11_TEXTURE2D_DESC srcDesc;
			pBackBufferTex->GetDesc(&srcDesc);

			D3D11_BOX srcBox;
			srcBox.left = 0;
			srcBox.right = dstDesc.Width;
			srcBox.top = 0;
			srcBox.bottom = dstDesc.Height;
			srcBox.front = 0;
			srcBox.back = 1;
			GetDeviceContext().CopySubresourceRegion(pDstTex, 0, 0, 0, 0, pBackBufferTex, 0, &srcBox);

			D3D11_MAPPED_SUBRESOURCE mappedTex;
			STALL_PROFILER("lock/read texture")
			if (SUCCEEDED(GetDeviceContext().Map(pDstTex, 0, D3D11_MAP_READ, 0, &mappedTex)))
			{
				for (unsigned int i = 0; i < dstDesc.Height; ++i)
				{
					uint8* pSrc((uint8*) mappedTex.pData + i * mappedTex.RowPitch);
					uint8* pDst((uint8*) pDstARGBA8 + i * dstWidth * 4);
					for (unsigned int j = 0; j < dstDesc.Width; ++j, pSrc += 4, pDst += 4)
					{
						pDst[0] = pSrc[2];
						pDst[1] = pSrc[1];
						pDst[2] = pSrc[0];
						pDst[3] = 255;
					}
				}
				GetDeviceContext().Unmap(pDstTex, 0);
			}
		}
		SAFE_RELEASE(pDstTex);
	}
	SAFE_RELEASE(pBackBufferTex);
#endif
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

	assert(!IsEditorMode() || (m_CurrContext && m_CurrContext->m_pBackBuffers == m_pBackBuffers));

	// get last buffer used for Present()
	D3DSurface* pBackBuffer;
	pBackBuffer = m_pBackBuffers[(m_pCurrentBackBufferIndex + m_pBackBuffers.size() - 1) % m_pBackBuffers.size()];

	assert(pBackBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
		return(false);

	m_captureFlipFlop = 0;
	D3DTexture* pSourceTexture(0);
	pBackBuffer->GetResource((D3DResource**) &pSourceTexture);
	if (!pSourceTexture)
		return(false);

	D3D11_TEXTURE2D_DESC sourceDesc;
	pSourceTexture->GetDesc(&sourceDesc);

	if (bufferWidth == 0)
	{
		bufferWidth = sourceDesc.Width;
	}
	if (bufferHeight == 0)
	{
		bufferHeight = sourceDesc.Height;
	}

	m_pSaveTexture[0] = CTexture::Create2DTexture("$SaveTexture0", bufferWidth, bufferHeight, 1, FT_STAGE_READBACK, nullptr, eTF_R8G8B8A8, eTF_R8G8B8A8);
	m_pSaveTexture[1] = CTexture::Create2DTexture("$SaveTexture1", bufferWidth, bufferHeight, 1, FT_STAGE_READBACK, nullptr, eTF_R8G8B8A8, eTF_R8G8B8A8);

	// Initialize one of the buffers by capturing the current backbuffer
	// If we allow this call on the MT we cannot interact with the device
	// The first screen grab will perform the copy anyway
	/*D3D11_BOX srcBox;
	   srcBox.left = 0; srcBox.right = dstDesc.Width;
	   srcBox.top = 0; srcBox.bottom = dstDesc.Height;
	   srcBox.front = 0; srcBox.back = 1;
	   GetDeviceContext().CopySubresourceRegion(m_pSaveTexture[0], 0, 0, 0, 0, pSourceTexture, 0, &srcBox);*/

	SAFE_RELEASE(pSourceTexture);
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
	D3DTexture* pSourceTexture(0);

	//In case this routine is called without the init function being called
	if (m_pSaveTexture[0] == NULL || m_pSaveTexture[1] == NULL || !GetDevice().IsValid()) return bStatus;

	assert(!IsEditorMode() || (m_CurrContext && m_CurrContext->m_pBackBuffers == m_pBackBuffers));

	// get last buffer used for Present()
	D3DSurface* pBackBuffer;
	pBackBuffer = m_pBackBuffers[(m_pCurrentBackBufferIndex + m_pBackBuffers.size() - 1) % m_pBackBuffers.size()];

	assert(pBackBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);
	if (bbDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
		return bStatus;

	pBackBuffer->GetResource((D3DResource**)&pSourceTexture);
	if (pSourceTexture)
	{
		D3D11_TEXTURE2D_DESC sourceDesc;
		pSourceTexture->GetDesc(&sourceDesc);

		if (sourceDesc.SampleDesc.Count == 1)
		{
			D3DTexture* pCopySourceTexture;
			CTexture* pTargetTexture, * pCopyTexture;
			unsigned int width(destinationWidth > GetWidth() ? GetWidth() : destinationWidth);
			unsigned int height(destinationHeight > GetHeight() ? GetHeight() : destinationHeight);

			pTargetTexture = m_captureFlipFlop ? m_pSaveTexture[1] : m_pSaveTexture[0];
			pCopyTexture = m_captureFlipFlop ? m_pSaveTexture[0] : m_pSaveTexture[1];
			m_captureFlipFlop = (m_captureFlipFlop + 1) % 2;

			if (width != GetWidth() || height != GetHeight())
			{
				// reuse stereo left and right RTs to downscale
				GetDeviceContext().CopyResource(CTexture::s_ptexStereoL->GetDevTexture()->Get2DTexture(), pSourceTexture);
				GetUtils().Downsample(CTexture::s_ptexStereoL, CTexture::s_ptexStereoR, GetWidth(), GetHeight(), width, height);
				pCopySourceTexture = CTexture::s_ptexStereoR->GetDevTexture()->Get2DTexture();
			}
			else
			{
				pCopySourceTexture = pSourceTexture;
			}

			// Setup the copy of the captured frame to our local surface in the background
			D3D11_BOX srcBox;
			srcBox.left = 0;
			srcBox.right = width;
			srcBox.top = 0;
			srcBox.bottom = height;
			srcBox.front = 0;
			srcBox.back = 1;
			GetDeviceContext().CopySubresourceRegion(pTargetTexture->GetDevTexture()->Get2DTexture(), 0, 0, 0, 0, pCopySourceTexture, 0, &srcBox);

			// Copy the previous frame from our local surface to the requested buffer location
			pCopyTexture->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
			{
				for (unsigned int i = 0; i < height; ++i)
				{
				  uint8* pSrc((uint8*) pData + i * rowPitch);
				  uint8* pDst((uint8*) pDstRGBA8 + i * width * 4);
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

	SAFE_RELEASE(pSourceTexture);
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
	if (m_pSaveTexture[0] == NULL || m_pSaveTexture[1] == NULL || !GetDevice().IsValid()) return bStatus;

	CTexture* pCopyTexture = m_captureFlipFlop ? m_pSaveTexture[0] : m_pSaveTexture[1];
	unsigned int width(destinationWidth > GetWidth() ? GetWidth() : destinationWidth);
	unsigned int height(destinationHeight > GetHeight() ? GetHeight() : destinationHeight);

	// Copy the previous frame from our local surface to the requested buffer location
	pCopyTexture->GetDevTexture()->DownloadToStagingResource(0, [&](void* pData, uint32 rowPitch, uint32 slicePitch)
	{
		for (unsigned int i = 0; i < height; ++i)
		{
		  uint8* pSrc((uint8*) pData + i * rowPitch);
		  uint8* pDst((uint8*) pDstRGBA8 + i * width * 4);
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
				int currentFrame = GetFrameID(false);

				//Currently we only support one screen capture request per frame
				if (m_nScreenCaptureRequestFrame[m_RP.m_nFillThreadID] != currentFrame)
				{
					m_screenCapTexHandle[m_RP.m_nFillThreadID] = texHandle;
					m_nScreenCaptureRequestFrame[m_RP.m_nFillThreadID] = currentFrame;
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

int CD3D9Renderer::ScreenToTexture(int nTexID)
{
	CTexture* pTex = CTexture::GetByID(nTexID);
	if (!pTex)
		return -1;

	int iTempX, iTempY, iWidth, iHeight;
	GetViewport(&iTempX, &iTempY, &iWidth, &iHeight);

	return 0;
}

void CD3D9Renderer::Draw2dImage(float xpos, float ypos, float w, float h, int textureid, float s0, float t0, float s1, float t1, float angle, const ColorF& col, float z)
{
	Draw2dImage(xpos, ypos, w, h, textureid, s0, t0, s1, t1, angle, col.r, col.g, col.b, col.a, z);
}

void CD3D9Renderer::Draw2dImageStretchMode(bool bStretch)
{
	if (m_bDeviceLost)
		return;

	m_pRT->RC_Draw2dImageStretchMode(bStretch);
}

void CD3D9Renderer::Draw2dImage(float xpos, float ypos, float w, float h, int textureid, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z)
{
	if (m_bDeviceLost)
		return;

	//////////////////////////////////////////////////////////////////////
	// Draw a textured quad, texture is assumed to be in video memory
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	PROFILE_FRAME(Draw_2DImage);

	CTexture* pTexture = textureid >= 0 ? CTexture::GetByID(textureid) : NULL;

	m_pRT->RC_Draw2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, r, g, b, a, z);
}

void CD3D9Renderer::Push2dImage(float xpos, float ypos, float w, float h, int textureid, float s0, float t0, float s1, float t1, float angle, float r, float g, float b, float a, float z, float stereoDepth)
{
	if (m_bDeviceLost)
		return;

	//////////////////////////////////////////////////////////////////////
	// Draw a textured quad, texture is assumed to be in video memory
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	PROFILE_FRAME(Push_2DImage);

	CTexture* pTexture = textureid >= 0 ? CTexture::GetByID(textureid) : NULL;

	m_pRT->RC_Push2dImage(xpos, ypos, w, h, pTexture, s0, t0, s1, t1, angle, r, g, b, a, z, stereoDepth);
}

void CD3D9Renderer::PushUITexture(int srcId, int dstId, float x0, float y0, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b, float a)
{
	CTexture* pSrc = srcId >= 0 ? CTexture::GetByID(srcId) : NULL;
	CTexture* pDst = dstId >= 0 ? CTexture::GetByID(dstId) : NULL;
	if (!pSrc || !pDst)
		return;
	m_pRT->RC_PushUITexture(x0, y0, w, h, pSrc, u0, v0, u1, v1, pDst, r, g, b, a);
}

bool CD3D9Renderer::RayIntersectMesh(IRenderMesh* pMesh, const Ray& ray, Vec3& hitpos, Vec3& p0, Vec3& p1, Vec3& p2, Vec2& uv0, Vec2& uv1, Vec2& uv2)
{
	bool hasHit = false;
	// get triangle that was hit
	pMesh->LockForThreadAccess();
	const int numIndices = pMesh->GetIndicesCount();
	const int numVertices = pMesh->GetVerticesCount();
	if (numIndices || numVertices)
	{
		// TODO: this could be optimized, e.g cache triangles and uv's and use octree to find triangles
		strided_pointer<Vec3> pPos;
		strided_pointer<Vec2> pUV;
		pPos.data = (Vec3*)pMesh->GetPosPtr(pPos.iStride, FSL_READ);
		pUV.data = (Vec2*)pMesh->GetUVPtr(pUV.iStride, FSL_READ);
		const vtx_idx* pIndices = pMesh->GetIndexPtr(FSL_READ);

		const TRenderChunkArray& Chunks = pMesh->GetChunks();
		const int nChunkCount = Chunks.size();
		for (int nChunkId = 0; nChunkId < nChunkCount; nChunkId++)
		{
			const CRenderChunk* pChunk = &Chunks[nChunkId];
			if ((pChunk->m_nMatFlags & MTL_FLAG_NODRAW))
				continue;

			int lastIndex = pChunk->nFirstIndexId + pChunk->nNumIndices;
			for (int i = pChunk->nFirstIndexId; i < lastIndex; i += 3)
			{
				const Vec3& v0 = pPos[pIndices[i]];
				const Vec3& v1 = pPos[pIndices[i + 1]];
				const Vec3& v2 = pPos[pIndices[i + 2]];

				if (Intersect::Ray_Triangle(ray, v0, v2, v1, hitpos))  // only front face
				{
					uv0 = pUV[pIndices[i]];
					uv1 = pUV[pIndices[i + 1]];
					uv2 = pUV[pIndices[i + 2]];
					p0 = v0;
					p1 = v1;
					p2 = v2;
					hasHit = true;
					nChunkId = nChunkCount; // break outer loop
					break;
				}
			}
		}
	}

	pMesh->UnlockStream(VSF_GENERAL);
	pMesh->UnlockIndexStream();
	pMesh->UnLockForThreadAccess();

	return hasHit;
}

int CD3D9Renderer::RayToUV(const Vec3& vOrigin, const Vec3& vDirection, float* pUOut, float* pVOut)
{
	IGameFramework* pGameFramework = gEnv->pGame->GetIGameFramework();

	// Setup ray + optionally skip 1 entity
	ray_hit rayHit;
	static const float maxRayDist = 100.f;
	const unsigned int flags = rwi_stop_at_pierceable | rwi_colltype_any;
	IPhysicalEntity* skipList[1];
	int skipCount = 0;

	// Check if the ray hits an entity
	if (gEnv->pSystem->GetIPhysicalWorld()->RayWorldIntersection(vOrigin, vDirection * maxRayDist, ent_all, flags, &rayHit, 1, skipList, skipCount))
	{
		int type = rayHit.pCollider->GetiForeignData();

		if (type == PHYS_FOREIGN_ID_ENTITY)
		{
			IEntity* pEntity = (IEntity*)rayHit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
			IEntityRenderProxy* pRenderProxy = pEntity ? (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER) : 0;

			// Get the renderproxy, and use it to check if the material is a DynTex, and get the UIElement if so
			if (pRenderProxy)
			{
				IRenderNode* pRenderNode = pRenderProxy->GetRenderNode();
				if (pRenderNode)
				{
					int m_dynTexGeomSlot = 0;
					IStatObj* pObj = pRenderNode->GetEntityStatObj(m_dynTexGeomSlot);

					// result
					bool hasHit = false;
					Vec2 uv0, uv1, uv2;
					Vec3 p0, p1, p2;
					Vec3 hitpos;

					// calculate ray dir
					CCamera cam = gEnv->pRenderer->GetCamera();
					if (pEntity->GetSlotFlags(m_dynTexGeomSlot) & ENTITY_SLOT_RENDER_NEAREST)
					{
						ICVar* r_drawnearfov = gEnv->pConsole->GetCVar("r_DrawNearFoV");
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
							hasHit = RayIntersectMesh(pMesh, ray, hitpos, p0, p1, p2, uv0, uv1, uv2);
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
			}
		}
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

void CD3D9Renderer::Draw2dImageList()
{
	m_pRT->RC_Draw2dImageList();
}

void CD3D9Renderer::DrawImage(float xpos, float ypos, float w, float h, int texture_id, float s0, float t0, float s1, float t1, float r, float g, float b, float a, bool filtered)
{
	float s[4], t[4];

	s[0] = s0;
	t[0] = 1.0f - t0;
	s[1] = s1;
	t[1] = 1.0f - t0;
	s[2] = s1;
	t[2] = 1.0f - t1;
	s[3] = s0;
	t[3] = 1.0f - t1;

	DrawImageWithUV(xpos, ypos, 0, w, h, texture_id, s, t, r, g, b, a, filtered);
}

void CD3D9Renderer::DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float* s, float* t, float r, float g, float b, float a, bool filtered)
{
	assert(s);
	assert(t);

	if (m_bDeviceLost)
		return;

	m_pRT->RC_DrawImageWithUV(xpos, ypos, z, w, h, texture_id, s, t, r, g, b, a, filtered);
}

void CD3D9Renderer::RT_DrawImageWithUV(float xpos, float ypos, float z, float w, float h, int texture_id, float s[4], float t[4], DWORD col, bool filtered)
{
	if (GetS3DRend().IsStereoEnabled() && !GetS3DRend().DisplayStereoDone())
	{
		GetS3DRend().BeginRenderingTo(LEFT_EYE);
		RT_DrawImageWithUVInternal(xpos, ypos, z, w, h, texture_id, s, t, col, filtered);
		GetS3DRend().EndRenderingTo(LEFT_EYE);

		if (GetS3DRend().RequiresSequentialSubmission())
		{
			GetS3DRend().BeginRenderingTo(RIGHT_EYE);
			RT_DrawImageWithUVInternal(xpos, ypos, z, w, h, texture_id, s, t, col, filtered);
			GetS3DRend().EndRenderingTo(RIGHT_EYE);
		}
	}
	else
	{
		RT_DrawImageWithUVInternal(xpos, ypos, z, w, h, texture_id, s, t, col, filtered);
	}
}

void CD3D9Renderer::RT_DrawImageWithUVInternal(float xpos, float ypos, float z, float w, float h, int texture_id, float s[4], float t[4], DWORD col, bool filtered)
{
	//////////////////////////////////////////////////////////////////////
	// Draw a textured quad, texture is assumed to be in video memory
	//////////////////////////////////////////////////////////////////////

	// Check for the presence of a D3D device
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	PROFILE_FRAME(Draw_2DImage);

	HRESULT hReturn = S_OK;

	float fx = xpos;
	float fy = ypos;
	float fw = w;
	float fh = h;

	SetCullMode(R_CULL_DISABLE);
	EF_SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);

	TempDynVB<SVF_P3F_C4B_T2F> vb;
	vb.Allocate(4);
	SVF_P3F_C4B_T2F* vQuad = vb.Lock();

	vQuad[0].xyz.x = xpos;
	vQuad[0].xyz.y = ypos;
	vQuad[0].xyz.z = z;
	vQuad[0].st = Vec2(s[0], t[0]);
	vQuad[0].color.dcolor = col;

	vQuad[1].xyz.x = xpos + w;
	vQuad[1].xyz.y = ypos;
	vQuad[1].xyz.z = z;
	vQuad[1].st = Vec2(s[1], t[1]);
	vQuad[1].color.dcolor = col;

	vQuad[2].xyz.x = xpos;
	vQuad[2].xyz.y = ypos + h;
	vQuad[2].xyz.z = z;
	vQuad[2].st = Vec2(s[3], t[3]);
	vQuad[2].color.dcolor = col;

	vQuad[3].xyz.x = xpos + w;
	vQuad[3].xyz.y = ypos + h;
	vQuad[3].xyz.z = z;
	vQuad[3].st = Vec2(s[2], t[2]);
	vQuad[3].color.dcolor = col;

	vb.Unlock();

	STexState TS;
	TS.SetFilterMode(filtered ? FILTER_BILINEAR : FILTER_POINT);
	TS.SetClampMode(1, 1, 1);
	CTexture::ApplyForID(0, texture_id, CTexture::GetTexState(TS), -1);

	FX_SetFPMode();

	vb.Bind(0);
	vb.Release();

	if (FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		return;

	FX_DrawPrimitive(eptTriangleStrip, 0, 4);
}

void CD3D9Renderer::DrawLines(Vec3 v[], int nump, ColorF& col, int flags, float fGround)
{
	if (nump > 1) m_pRT->RC_DrawLines(v, nump, col, flags, fGround);
}

void CD3D9Renderer::Graph(byte* g, int x, int y, int wdt, int hgt, int nC, int type, char* text, ColorF& color, float fScale)
{
	ColorF col;
	PREFAST_SUPPRESS_WARNING(6255)
	Vec3 * vp = (Vec3*) alloca(wdt * sizeof(Vec3));
	int i;

	Set2DMode(true, m_width, m_height);
	SetState(GS_NODEPTHTEST);
	col = Col_Blue;
	int num = CTexture::s_ptexWhite->GetTextureID();

	float fy = (float)y;
	float fx = (float)x;
	float fwdt = (float)wdt;
	float fhgt = (float)hgt;

	DrawImage(fx, fy, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	DrawImage(fx, fy + fhgt, fwdt, 2, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	DrawImage(fx, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);
	DrawImage(fx + fwdt - 2, fy, 2, fhgt, num, 0, 0, 1, 1, col.r, col.g, col.b, col.a);

	float fGround = CV_r_graphstyle ? fy + fhgt : -1;

	for (i = 0; i < wdt; i++)
	{
		vp[i][0] = (float)i + fx;
		vp[i][1] = fy + (float)(g[i]) * fhgt / 255.0f;
		vp[i][2] = 0;
	}
	if (type == 1)
	{
		col = color;
		DrawLines(&vp[0], nC, col, 3, fGround);
		col = ColorF(1.0f) - col;
		col[3] = 1;
		DrawLines(&vp[nC], wdt - nC, col, 3, fGround);
	}
	else if (type == 2)
	{
		col = color;
		DrawLines(&vp[0], wdt, col, 3, fGround);
	}

	if (text)
	{
		WriteXY(4, y - 18, 0.5f, 1, 1, 1, 1, 1, "%s", text);
		WriteXY(wdt - 260, y - 18, 0.5f, 1, 1, 1, 1, 1, "%d ms", (int)(1000.0f * fScale));
	}

	Set2DMode(false, 0, 0);
}

void CD3D9Renderer::GetModelViewMatrix(float* mat)
{
	int nThreadID = m_pRT->GetThreadList();
	*(Matrix44*)mat = *m_RP.m_TI[nThreadID].m_matView->GetTop();
}

void CD3D9Renderer::GetProjectionMatrix(float* mat)
{
	int nThreadID = m_pRT->GetThreadList();
	*(Matrix44*)mat = *m_RP.m_TI[nThreadID].m_matProj->GetTop();
}

void CD3D9Renderer::SetMatrices(float* pProjMat, float* pViewMat)
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_matProj->LoadMatrix((Matrix44*)pProjMat);
	m_RP.m_TI[nThreadID].m_matView->LoadMatrix((Matrix44*)pViewMat);
}

///////////////////////////////////////////
void CD3D9Renderer::PushMatrix()
{
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	EF_PushMatrix();
}

///////////////////////////////////////////
void CD3D9Renderer::PopMatrix()
{
	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	EF_PopMatrix();
}

void CD3D9Renderer::SetPerspective(const CCamera& cam)
{
	int nThreadID = m_pRT->GetThreadList();
	Matrix44A* m = m_RP.m_TI[nThreadID].m_matProj->GetTop();
	mathMatrixPerspectiveFov(m, cam.GetFov(), cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
	EF_DirtyMatrix();
}

//-----------------------------------------------------------------------------
// coded by ivo:
// calculate parameter for an off-center projection matrix.
// the projection matrix itself is calculated by D3D9.
//-----------------------------------------------------------------------------
Matrix44A OffCenterProjection(const CCamera& cam, const Vec3& nv, unsigned short max, unsigned short win_width, unsigned short win_height)
{

	int nThreadID = gRenDev->m_pRT->GetThreadList();

	//get the size of near plane
	float l = +nv.x;
	float r = -nv.x;
	float b = -nv.z;
	float t = +nv.z;

	//---------------------------------------------------

	float max_x = (float)max;
	float max_z = (float)max;

	float win_x = (float)win_width;
	float win_z = (float)win_height;

	if ((win_x < max_x) && (win_z < max_z))
	{
		//calculate parameters for off-center projection-matrix
		float ext_x = -nv.x * 2;
		float ext_z = +nv.z * 2;
		l = +nv.x + (ext_x / max_x) * win_x;
		r = +nv.x + (ext_x / max_x) * (win_x + 1);
		t = +nv.z - (ext_z / max_z) * win_z;
		b = +nv.z - (ext_z / max_z) * (win_z + 1);
	}

	Matrix44A m;
	mathMatrixPerspectiveOffCenter(&m, l, r, b, t, cam.GetNearPlane(), cam.GetFarPlane());

	return m;
}

void CD3D9Renderer::SetRCamera(const CRenderCamera& cam)
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_rcam = cam;
	Matrix44A* m = m_RP.m_TI[nThreadID].m_matView->GetTop();
	cam.GetModelviewMatrix((float*)m);
	if (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_MIRRORCAMERA)
		m_RP.m_TI[nThreadID].m_matView->Scale(1, -1, 1);
	m = (Matrix44A*)m_RP.m_TI[nThreadID].m_matProj->GetTop();
	//cam.GetProjectionMatrix(*m);
	mathMatrixPerspectiveOffCenter((Matrix44A*)m, cam.fWL, cam.fWR, cam.fWB, cam.fWT, cam.fNear, cam.fFar);

	const bool bReverseDepth = (CV_r_ReverseDepth != 0 && (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_SHADOWGEN) == 0) ? 1 : 0;
	const bool bWasReverseDepth = (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0 ? 1 : 0;

	m_RP.m_TI[nThreadID].m_PersFlags &= ~RBPF_REVERSE_DEPTH;
	if (bReverseDepth)
	{
		mathMatrixPerspectiveOffCenterReverseDepth((Matrix44A*)m, cam.fWL, cam.fWR, cam.fWB, cam.fWT, cam.fNear, cam.fFar);
		m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_REVERSE_DEPTH;
	}

	// directly reset affected states
	if ((bReverseDepth ^ bWasReverseDepth) && m_pRT->IsRenderThread())
	{
		uint32 depthState = ReverseDepthHelper::ConvertDepthFunc(m_RP.m_CurState);
		FX_SetState(m_RP.m_CurState, m_RP.m_CurAlphaRef, depthState);
	}

	EF_DirtyMatrix();
}

void CD3D9Renderer::SetCamera(const CCamera& cam)
{
	int nThreadID = m_pRT->GetThreadList();

	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	// Ortho-normalize camera matrix in double precision to minimize numerical errors and improve precision when inverting matrix
	Matrix34_tpl<f64> mCam34 = cam.GetMatrix();
	mCam34.OrthonormalizeFast();

	CRenderCamera rcam;

	// Asymmetric frustum
	float Near = cam.GetNearPlane(), Far = cam.GetFarPlane();

	float wT = tanf(cam.GetFov() * 0.5f) * Near, wB = -wT;
	float wR = wT * cam.GetProjRatio(), wL = -wR;
	rcam.Frustum(wL + cam.GetAsymL(), wR + cam.GetAsymR(), wB + cam.GetAsymB(), wT + cam.GetAsymT(), Near, Far);

	Vec3 vEye = cam.GetPosition();
	Vec3 vAt = vEye + Vec3((f32)mCam34(0, 1), (f32)mCam34(1, 1), (f32)mCam34(2, 1));
	Vec3 vUp = Vec3((f32)mCam34(0, 2), (f32)mCam34(1, 2), (f32)mCam34(2, 2));
	rcam.LookAt(vEye, vAt, vUp);
	SetRCamera(rcam);

	Matrix44A* m = m_RP.m_TI[nThreadID].m_matProj->GetTop();
	//mathMatrixPerspectiveFov(m, fov, cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());
	//D3DXMatrixPerspectiveFovRH(m, fov, cam.GetProjRatio(), cam.GetNearPlane(), cam.GetFarPlane());

	if (!IsRecursiveRenderView() && (m_RenderTileInfo.nGridSizeX > 1.f || m_RenderTileInfo.nGridSizeY > 1.f))
	{
		// shift and scale viewport
		(*m).m00 *= m_RenderTileInfo.nGridSizeX;
		(*m).m11 *= m_RenderTileInfo.nGridSizeY;
		(*m).m20 = (m_RenderTileInfo.nGridSizeX - 1) - m_RenderTileInfo.nPosX * 2.0f;
		(*m).m21 = -((m_RenderTileInfo.nGridSizeY - 1) - m_RenderTileInfo.nPosY * 2.0f);
	}

	Matrix44_tpl<f64> mCam44T = mCam34.GetTransposed();
	Matrix44_tpl<f64> mView64;
	mathMatrixLookAtInverse(&mView64, &mCam44T);

	Matrix44 mView = (Matrix44_tpl<f32> )mView64;

	// Rotate around x-axis by -PI/2
	Matrix44 mViewFinal = mView;
	mViewFinal.m01 = mView.m02;
	mViewFinal.m02 = -mView.m01;
	mViewFinal.m11 = mView.m12;
	mViewFinal.m12 = -mView.m11;
	mViewFinal.m21 = mView.m22;
	mViewFinal.m22 = -mView.m21;
	mViewFinal.m31 = mView.m32;
	mViewFinal.m32 = -mView.m31;

	m = m_RP.m_TI[nThreadID].m_matView->GetTop();
	m_RP.m_TI[nThreadID].m_matView->LoadMatrix(&mViewFinal);

	mViewFinal.m30 = 0;
	mViewFinal.m31 = 0;
	mViewFinal.m32 = 0;
	m_CameraZeroMatrix[nThreadID] = mViewFinal;

	if (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_MIRRORCAMERA)
		m_RP.m_TI[nThreadID].m_matView->Scale(1, -1, 1);

	m_RP.m_TI[nThreadID].m_PersFlags |= RBPF_FP_MATRIXDIRTY;

	m_RP.m_TI[nThreadID].m_cam = cam;

	EF_SetCameraInfo();
}

void CD3D9Renderer::SetRenderTile(f32 nTilesPosX, f32 nTilesPosY, f32 nTilesGridSizeX, f32 nTilesGridSizeY)
{
	m_RenderTileInfo.nPosX = nTilesPosX;
	m_RenderTileInfo.nPosY = nTilesPosY;
	m_RenderTileInfo.nGridSizeX = nTilesGridSizeX;
	m_RenderTileInfo.nGridSizeY = nTilesGridSizeY;
}

void CD3D9Renderer::SetViewport(int x, int y, int width, int height, int id)
{
	if (m_pRT->IsRenderThread())
	{
		RT_SetViewport(x, y, width, height, id);
	}
	else
	{
		ASSERT_IS_MAIN_THREAD(m_pRT)
		m_MainRTViewport.nX = x;
		m_MainRTViewport.nY = y;
		m_MainRTViewport.nWidth = width;
		m_MainRTViewport.nHeight = height;
		m_pRT->RC_SetViewport(x, y, width, height, id);
	}
}

void CD3D9Renderer::RT_SetViewport(int x, int y, int width, int height, int id)
{
	assert(m_pRT->IsRenderThread());

	if (x == 0 && y == 0 && width == GetWidth() && height == GetHeight())
	{
		width = m_FullResRect.right;
		height = m_FullResRect.bottom;
	}

	m_NewViewport.nX = x;
	m_NewViewport.nY = y;
	m_NewViewport.nWidth = width;
	m_NewViewport.nHeight = height;
	m_RP.m_PersFlags2 |= RBPF2_COMMIT_PF;
	m_RP.m_nCommitFlags |= FC_GLOBAL_PARAMS;
	m_bViewportDirty = true;
	if (-1 != id)
		m_CurViewportID = clamp_tpl(id, 0, MAX_NUM_VIEWPORTS);
}

void CD3D9Renderer::GetViewport(int* x, int* y, int* width, int* height)
{
	SViewport& vp = m_pRT->IsRenderThread() ? m_NewViewport : m_MainRTViewport;
	*x = vp.nX;
	*y = vp.nY;
	*width = vp.nWidth;
	*height = vp.nHeight;
}

void CD3D9Renderer::SetScissor(int x, int y, int width, int height)
{
	if (!x && !y && !width && !height)
		EF_Scissor(false, x, y, width, height);
	else
		EF_Scissor(true, x, y, width, height);
}

void CD3D9Renderer::SelectGPU(int nGPU)
{
	m_pRT->RC_SelectGPU(nGPU);
}

void CD3D9Renderer::RT_SelectGPU(int nGPU)
{
	m_nGPU = nGPU;
}

void CD3D9Renderer::SetCullMode(int mode)
{
	m_pRT->RC_SetCull(mode);
}

void CD3D9Renderer::RT_SetCull(int mode)
{
	//////////////////////////////////////////////////////////////////////
	// Change the culling mode
	//////////////////////////////////////////////////////////////////////

	CRY_ASSERT(static_cast<const CD3D9Renderer*>(this)->GetDevice().IsValid());

	switch (mode)
	{
	case R_CULL_DISABLE:
		D3DSetCull(eCULL_None);
		break;
	case R_CULL_BACK:
		D3DSetCull(eCULL_Back);
		break;
	case R_CULL_FRONT:
		D3DSetCull(eCULL_Front);
		break;
	}
}

void CD3D9Renderer::PushProfileMarker(char* label)
{
	m_pRT->RC_PushProfileMarker(label);
}

void CD3D9Renderer::PopProfileMarker(char* label)
{
	m_pRT->RC_PopProfileMarker(label);
}

void CD3D9Renderer::SetFogColor(const ColorF& color)
{
	int nThreadID = m_pRT->GetThreadList();
	m_RP.m_TI[nThreadID].m_FS.m_FogColor = color;

	// Fog color
	EF_SetFogColor(m_RP.m_TI[nThreadID].m_FS.m_FogColor);
}

bool CD3D9Renderer::EnableFog(bool enable)
{
	int nThreadID = m_pRT->GetThreadList();

	bool bPrevFog = m_RP.m_TI[nThreadID].m_FS.m_bEnable; // remember fog value

	m_RP.m_TI[nThreadID].m_FS.m_bEnable = enable;

	return bPrevFog;
}

///////////////////////////////////////////
void CD3D9Renderer::FX_PushWireframeMode(int mode)
{
	IF (m_nWireFrameStack >= MAX_WIREFRAME_STACK, 0)
	{
		CryFatalError("Pushing more than %d different WireFrame Modes onto stack", MAX_WIREFRAME_STACK);
	}
	assert(m_nWireFrameStack >= 0 && m_nWireFrameStack < MAX_WIREFRAME_STACK);
	PREFAST_ASSUME(m_nWireFrameStack >= 0 && m_nWireFrameStack < MAX_WIREFRAME_STACK);
	m_arrWireFrameStack[m_nWireFrameStack] = m_wireframe_mode;
	m_nWireFrameStack += 1;
	FX_SetWireframeMode(mode);
}

void CD3D9Renderer::FX_PopWireframeMode()
{
	IF (m_nWireFrameStack == 0, 0)
	{
		CryFatalError("WireFrame Mode more often popped than pushed");
	}

	m_nWireFrameStack -= 1;
	assert(m_nWireFrameStack >= 0 && m_nWireFrameStack < MAX_WIREFRAME_STACK);
	PREFAST_ASSUME(m_nWireFrameStack >= 0 && m_nWireFrameStack < MAX_WIREFRAME_STACK);
	int nMode = m_arrWireFrameStack[m_nWireFrameStack];
	FX_SetWireframeMode(nMode);
}

void CD3D9Renderer::FX_SetWireframeMode(int mode)
{
	assert(mode == R_WIREFRAME_MODE || mode == R_SOLID_MODE || mode == R_POINT_MODE);

	if (m_wireframe_mode == mode)
		return;

	int prev_mode = m_wireframe_mode;
	m_wireframe_mode = mode;

	m_RP.m_StateOr &= ~(GS_WIREFRAME | GS_POINTRENDERING);
	if (m_wireframe_mode == R_POINT_MODE)
		m_RP.m_StateOr |= GS_POINTRENDERING;
	else if (m_wireframe_mode == R_WIREFRAME_MODE)
		m_RP.m_StateOr |= GS_WIREFRAME;

	int nState = m_RP.m_CurState;

	SetState(nState);
}

///////////////////////////////////////////
void CD3D9Renderer::EnableVSync(bool enable)
{
	// TODO: remove
}

void CD3D9Renderer::DrawQuad(float x0, float y0, float x1, float y1, const ColorF& color, float z, float s0, float t0, float s1, float t1)
{
	PROFILE_FRAME(Draw_2DImage);

	ColorF c = color;
	c.NormalizeCol(c);
	DWORD col = c.pack_argb8888();

	TempDynVB<SVF_P3F_C4B_T2F> vb;
	vb.Allocate(4);
	SVF_P3F_C4B_T2F* vQuad = vb.Lock();

	float ftx0 = s0;
	float fty0 = t0;
	float ftx1 = s1;
	float fty1 = t1;

	// Define the quad
	vQuad[0].xyz = Vec3(x0, y0, z);
	vQuad[0].color.dcolor = col;
	vQuad[0].st = Vec2(ftx0, fty0);

	vQuad[1].xyz = Vec3(x1, y0, z);
	vQuad[1].color.dcolor = col;
	vQuad[1].st = Vec2(ftx1, fty0);

	vQuad[3].xyz = Vec3(x1, y1, z);
	vQuad[3].color.dcolor = col;
	vQuad[3].st = Vec2(ftx1, fty1);

	vQuad[2].xyz = Vec3(x0, y1, z);
	vQuad[2].color.dcolor = col;
	vQuad[2].st = Vec2(ftx0, fty1);

	vb.Unlock();
	vb.Bind(0);
	vb.Release();

	FX_Commit();

	if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		FX_DrawPrimitive(eptTriangleStrip, 0, 4);
}

void CD3D9Renderer::DrawFullScreenQuad(CShader* pSH, const CCryNameTSCRC& TechName, float s0, float t0, float s1, float t1, uint32 nState)
{
	uint32 nPasses;
	pSH->FXSetTechnique(TechName);
	pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	pSH->FXBeginPass(0);

	float fWidth5 = (float)m_NewViewport.nWidth - 0.5f;
	float fHeight5 = (float)m_NewViewport.nHeight - 0.5f;

	TempDynVB<SVF_TP3F_C4B_T2F> vb;
	vb.Allocate(4);
	SVF_TP3F_C4B_T2F* Verts = vb.Lock();

	std::swap(t0, t1);
	Verts->pos = Vec4(-0.5f, -0.5f, 0.0f, 1.0f);
	Verts->st = Vec2(s0, t0);
	Verts->color.dcolor = (uint32) - 1;
	Verts++;

	Verts->pos = Vec4(fWidth5, -0.5f, 0.0f, 1.0f);
	Verts->st = Vec2(s1, t0);
	Verts->color.dcolor = (uint32) - 1;
	Verts++;

	Verts->pos = Vec4(-0.5f, fHeight5, 0.0f, 1.0f);
	Verts->st = Vec2(s0, t1);
	Verts->color.dcolor = (uint32) - 1;
	Verts++;

	Verts->pos = Vec4(fWidth5, fHeight5, 0.0f, 1.0f);
	Verts->st = Vec2(s1, t1);
	Verts->color.dcolor = (uint32) - 1;

	vb.Unlock();
	vb.Bind(0);
	vb.Release();

	FX_Commit();

	// Draw a fullscreen quad to sample the RT
	FX_SetState(nState);
	if (!FAILED(FX_SetVertexDeclaration(0, eVF_TP3F_C4B_T2F)))
		FX_DrawPrimitive(eptTriangleStrip, 0, 4);

	pSH->FXEndPass();
}

void CD3D9Renderer::DrawQuad3D(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
                               const ColorF& color, float ftx0, float fty0, float ftx1, float fty1)
{
	assert(color.r >= 0.0f && color.r <= 1.0f);
	assert(color.g >= 0.0f && color.g <= 1.0f);
	assert(color.b >= 0.0f && color.b <= 1.0f);
	assert(color.a >= 0.0f && color.a <= 1.0f);

	DWORD col = D3DRGBA(color.r, color.g, color.b, color.a);

	TempDynVB<SVF_P3F_C4B_T2F> vb;
	vb.Allocate(4);
	SVF_P3F_C4B_T2F* vQuad = vb.Lock();

	// Define the quad
	vQuad[0].xyz = v0;
	vQuad[0].color.dcolor = col;
	vQuad[0].st = Vec2(ftx0, fty0);

	vQuad[1].xyz = v1;
	vQuad[1].color.dcolor = col;
	vQuad[1].st = Vec2(ftx1, fty0);

	vQuad[3].xyz = v2;
	vQuad[3].color.dcolor = col;
	vQuad[3].st = Vec2(ftx1, fty1);

	vQuad[2].xyz = v3;
	vQuad[2].color.dcolor = col;
	vQuad[2].st = Vec2(ftx0, fty1);

	vb.Unlock();
	vb.Bind(0);
	vb.Release();

	FX_Commit();
	if (!FAILED(FX_SetVertexDeclaration(0, eVF_P3F_C4B_T2F)))
		FX_DrawPrimitive(eptTriangleStrip, 0, 4);
}

///////////////////////////////////////////

void CD3D9Renderer::DrawPrimitivesInternal(CVertexBuffer* src, int vert_num, const ERenderPrimitiveType prim_type)
{
	size_t stride = 0;
	switch (src->m_eVF)
	{
	case eVF_P3F_C4B_T2F:
		stride = sizeof(SVF_P3F_C4B_T2F);
		break;
	case eVF_TP3F_C4B_T2F:
		stride = sizeof(SVF_TP3F_C4B_T2F);
		break;
	case eVF_P3F_T3F:
		stride = sizeof(SVF_P3F_T3F);
		break;
	case eVF_P3F_T2F_T3F:
		stride = sizeof(SVF_P3F_T2F_T3F);
		break;
	default:
		assert(0);
		return;
	}

	FX_Commit();

	if (FAILED(FX_SetVertexDeclaration(0, src->m_eVF)))
		return;

	FX_SetVStream(3, NULL, 0, 0);

	TempDynVBAny::CreateFillAndBind(src->m_VS.m_pLocalData, vert_num, 0, stride);

	FX_DrawPrimitive(prim_type, 0, vert_num);
}

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
void CD3D9Renderer::ResetToDefault()
{
	assert(m_pRT->IsRenderThread());

	if (m_LogFile)
		Logv(".... ResetToDefault ....\n");

	m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags |= RBPF_FP_DIRTY;
	const bool bReverseDepth = (m_RP.m_TI[m_RP.m_nProcessThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) != 0;

	EF_Scissor(false, 0, 0, 0, 0);

	SStateDepth DS;
	SStateBlend BS;
	SStateRaster RS;
	DS.Desc.DepthEnable = TRUE;
	DS.Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DS.Desc.DepthFunc = bReverseDepth ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL;
	DS.Desc.StencilEnable = FALSE;
	SetDepthState(&DS, 0);

	RS.Desc.CullMode = (m_nCurStateRS != (uint32) - 1) ? m_StatesRS[m_nCurStateRS].Desc.CullMode : D3D11_CULL_BACK;
	switch (RS.Desc.CullMode)
	{
	case D3D11_CULL_BACK:
		m_RP.m_eCull = eCULL_Back;
		break;
	case D3D11_CULL_NONE:
		m_RP.m_eCull = eCULL_None;
		break;
	case D3D11_CULL_FRONT:
		m_RP.m_eCull = eCULL_Front;
		break;
	}
	;
	RS.Desc.FillMode = D3D11_FILL_SOLID;
	SetRasterState(&RS);

	BS.Desc.RenderTarget[0].BlendEnable = FALSE;
	BS.Desc.RenderTarget[1].BlendEnable = FALSE;
	BS.Desc.RenderTarget[2].BlendEnable = FALSE;
	BS.Desc.RenderTarget[3].BlendEnable = FALSE;
	BS.Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	BS.Desc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	BS.Desc.RenderTarget[2].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	BS.Desc.RenderTarget[3].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	SetBlendState(&BS);
	GetDeviceContext().GSSetShader(NULL, NULL, 0);

	m_RP.m_CurState = GS_DEPTHWRITE;

	FX_ResetPipe();

	RT_UnbindTMUs();

	FX_ResetVertexDeclaration();

	m_RP.m_ForceStateOr &= ~GS_STENCIL;

#ifdef DO_RENDERLOG
	if (m_LogFile && CV_r_log == 3)
		Logv(".... End ResetToDefault ....\n");
#endif
}

///////////////////////////////////////////
void CD3D9Renderer::SetMaterialColor(float r, float g, float b, float a)
{
	EF_SetGlobalColor(r, g, b, a);
}

///////////////////////////////////////////
bool CD3D9Renderer::ProjectToScreen(float ptx, float pty, float ptz, float* sx, float* sy, float* sz)
{
	int nThreadID = m_pRT->GetThreadList();
	SViewport& vp = m_pRT->IsRenderThread() ? m_NewViewport : m_MainRTViewport;

	Vec3 vOut, vIn;
	vIn.x = ptx;
	vIn.y = pty;
	vIn.z = ptz;

	int32 v[4];
	v[0] = vp.nX;
	v[1] = vp.nY;
	v[2] = vp.nWidth;
	v[3] = vp.nHeight;

	Matrix44A mIdent;
	mIdent.SetIdentity();
	if (mathVec3Project(
	      &vOut,
	      &vIn,
	      v,
	      m_RP.m_TI[nThreadID].m_matProj->GetTop(),
	      m_RP.m_TI[nThreadID].m_matView->GetTop(),
	      &mIdent))
	{
		*sx = vOut.x * 100 / vp.nWidth;
		*sy = vOut.y * 100 / vp.nHeight;
		*sz = (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH) ? 1.0f - vOut.z : vOut.z;

		return true;
	}

	return false;
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
	float modelMatrix[16];
	float projMatrix[16];
	int viewport[4];

	const int nThreadID = m_pRT->GetThreadList();
	if (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
		sz = 1.0f - sz;

	GetModelViewMatrix(modelMatrix);
	GetProjectionMatrix(projMatrix);
	GetViewport(&viewport[0], &viewport[1], &viewport[2], &viewport[3]);
	return sUnProject(sx, sy, sz, modelMatrix, projMatrix, viewport, px, py, pz);
}

//////////////////////////////////////////////////////////////////////

void CD3D9Renderer::ClearTargetsImmediately(uint32 nFlags)
{
	m_pRT->RC_ClearTargetsImmediately(0, nFlags, Clr_Transparent, Clr_FarPlane.r);
}

void CD3D9Renderer::ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth)
{
	m_pRT->RC_ClearTargetsImmediately(1, nFlags, Colors, fDepth);
}

void CD3D9Renderer::ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors)
{
	m_pRT->RC_ClearTargetsImmediately(2, nFlags, Colors, Clr_FarPlane.r);
}

void CD3D9Renderer::ClearTargetsImmediately(uint32 nFlags, float fDepth)
{
	m_pRT->RC_ClearTargetsImmediately(3, nFlags, Clr_Transparent, fDepth);
}

void CD3D9Renderer::ClearTargetsLater(uint32 nFlags)
{
	EF_ClearTargetsLater(nFlags);
}

void CD3D9Renderer::ClearTargetsLater(uint32 nFlags, const ColorF& Colors, float fDepth)
{
	EF_ClearTargetsLater(nFlags, Colors, fDepth, 0);
}

void CD3D9Renderer::ClearTargetsLater(uint32 nFlags, const ColorF& Colors)
{
	EF_ClearTargetsLater(nFlags, Colors);
}

void CD3D9Renderer::ClearTargetsLater(uint32 nFlags, float fDepth)
{
	EF_ClearTargetsLater(nFlags, fDepth, 0);
}

#ifdef _DEBUG

int s_In2DMode[RT_COMMAND_BUF_COUNT];

#endif

void CD3D9Renderer::Set2DMode(bool enable, int ortox, int ortoy, float znear, float zfar)
{
	Matrix44A* m;
	int nThreadID = m_pRT->GetThreadList();
	if (enable)
	{
#ifdef _DEBUG
		assert(s_In2DMode[nThreadID]++ >= 0);
#endif

		m_RP.m_TI[nThreadID].m_matProj->Push();
		m = m_RP.m_TI[nThreadID].m_matProj->GetTop();
		if (ortox && ortoy && znear != zfar)
			mathMatrixOrthoOffCenterLH(m, 0.0, (float)ortox, (float)ortoy, 0.0, znear, zfar);

		if (m_RP.m_TI[nThreadID].m_PersFlags & RBPF_REVERSE_DEPTH)
			(*m) = ReverseDepthHelper::Convert(*m);

#if defined(ENABLE_RENDER_AUX_GEOM)
		if (m_pRenderAuxGeomD3D)
			m_pRenderAuxGeomD3D->SetOrthoMode(true, m);
#endif
		EF_PushMatrix();
		m = m_RP.m_TI[nThreadID].m_matView->GetTop();
		m_RP.m_TI[nThreadID].m_matView->LoadIdentity();
	}
	else
	{
#ifdef _DEBUG
		assert(s_In2DMode[nThreadID]-- > 0);
#endif

#if defined(ENABLE_RENDER_AUX_GEOM)
		if (m_pRenderAuxGeomD3D)
			m_pRenderAuxGeomD3D->SetOrthoMode(false);
#endif
		m_RP.m_TI[nThreadID].m_matProj->Pop();
		EF_PopMatrix();
	}
	EF_SetCameraInfo();
}

void CD3D9Renderer::RemoveTexture(unsigned int TextureId)
{
	if (!TextureId)
		return;

	CTexture* tp = CTexture::GetByID(TextureId);
	if (!tp)
		return;

	bool bShadow = false;

	//if(!tp->IsAsyncDevTexCreation())
	//{
	//  SDynTexture_Shadow *pTX, *pNext;
	//  for (pTX=SDynTexture_Shadow::s_RootShadow.m_NextShadow; pTX != &SDynTexture_Shadow::s_RootShadow; pTX=pNext)
	//  {
	//    pNext = pTX->m_NextShadow;
	//    if (pTX->m_pTexture && pTX->m_pTexture->GetID() == TextureId)
	//    {
	//      delete pTX;
	//      bShadow = true;
	//    }
	//  }
	//}

	if (!bShadow)
	{
		if (tp->IsAsyncDevTexCreation())
		{
			SResourceAsync* pInfo = new SResourceAsync();
			pInfo->eClassName = eRCN_Texture;
			pInfo->pResource = tp;
			gRenDev->ReleaseResourceAsync(pInfo);
		}
		else
			SAFE_RELEASE(tp);
	}
}

void CD3D9Renderer::UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTFSrc, int posz, int sizez)
{
	if (m_bDeviceLost)
		return;

	CTexture* pTex = CTexture::GetByID(tnum);
	pTex->UpdateTextureRegion(newdata, posx, posy, posz, w, h, sizez, eTFSrc);
}

bool CD3D9Renderer::EF_PrecacheResource(SShaderItem* pSI, float fMipFactorSI, float fTimeToReady, int Flags, int nUpdateId, int nCounter)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

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
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

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

ITexture* CD3D9Renderer::EF_CreateCompositeTexture(int type, const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, ETEX_Format eTF, const STexComposition* pCompositions, size_t nCompositions, int8 nPriority)
{
	ITexture* pTex = NULL;

	switch (type)
	{
	case eTT_2DArray:
		pTex = CTexture::Create2DCompositeTexture(szName, nWidth, nHeight, nMips, nFlags, eTF, pCompositions, nCompositions);
		break;

	default:
		CRY_ASSERT_MESSAGE(0, "Not implemented texture format");
		pTex = CTexture::s_ptexNoTexture;
	}

	return pTex;
}

void CD3D9Renderer::CreateResourceAsync(SResourceAsync* pResource)
{
	m_pRT->RC_CreateResource(pResource);
}
void CD3D9Renderer::ReleaseResourceAsync(SResourceAsync* pResource)
{
	m_pRT->RC_ReleaseResource(pResource);
}

unsigned int CD3D9Renderer::DownLoadToVideoMemory(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, ETEX_Type eTT, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_RENDERER);

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

	if (!repeat)
		flags |= FT_STATE_CLAMP;
	if (!szCacheName)
		flags |= FT_DONT_STREAM;

	if (Id > 0)
	{
		CTexture* pTex = CTexture::GetByID(Id);
		if (pTex)
		{
			if (pRegion)
				pTex->UpdateTextureRegion(data, pRegion->x, pRegion->y, 0, pRegion->w, pRegion->h, 1, eTFSrc);
			else
				pTex->UpdateTextureRegion(data, 0, 0, 0, w, h, 1, eTFSrc);

			return Id;
		}
		else
			return 0;
	}
	else
	{
		CTexture* tp = 0;

		bool bIsMultiThreadedRenderer = false;
		gRenDev->EF_Query(EFQ_RenderMultithreaded, bIsMultiThreadedRenderer);
		if (bAsyncDevTexCreation && bIsMultiThreadedRenderer)
		{
			// make texture without device texture and request it async creation
			SResourceAsync* pInfo = new SResourceAsync();
			int nImgSize = CTexture::TextureDataSize(w, h, 1, nummipmap, 1, eTFSrc);
			pInfo->pData = new byte[nImgSize];
			memcpy(pInfo->pData, data, nImgSize);
			pInfo->eClassName = eRCN_Texture;
			pInfo->nWidth = w;
			pInfo->nHeight = h;
			pInfo->nMips = nummipmap;
			pInfo->nTexFlags = flags;
			pInfo->nFormat = eTFDst;

			tp = CTexture::CreateTextureObject(name, w, h, 1, eTT_2D, flags, eTFDst);
			tp->m_bAsyncDevTexCreation = bAsyncDevTexCreation;
			tp->m_eTFSrc = eTFSrc;
			tp->m_nMips = nummipmap;

			pInfo->nTexId = tp->GetID();
			gRenDev->CreateResourceAsync(pInfo);
		}
		else if (eTT == eTT_Cube)
			assert(!"Not supported");   // tp = CTexture::CreateCubeTexture(name, w, h, nummipmap, flags, data, eTFSrc, eTFDst);
		else if (eTT == eTT_3D)
			tp = CTexture::Create3DTexture(name, w, h, d, nummipmap, flags, data, eTFSrc, eTFDst);
		else
			tp = CTexture::Create2DTexture(name, w, h, nummipmap, flags, data, eTFSrc, eTFDst);

		return tp->GetID();
	}
}

unsigned int CD3D9Renderer::DownLoadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return DownLoadToVideoMemory(data, w, h, 1, eTFSrc, eTFDst, nummipmap, eTT_2D, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
}

unsigned int CD3D9Renderer::DownLoadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return DownLoadToVideoMemory(data, w, h, 1, eTFSrc, eTFDst, nummipmap, eTT_Cube, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
}

unsigned int CD3D9Renderer::DownLoadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat, int filter, int Id, const char* szCacheName, int flags, EEndian eEndian, RectI* pRegion, bool bAsyncDevTexCreation)
{
	return DownLoadToVideoMemory(data, w, h, d, eTFSrc, eTFDst, nummipmap, eTT_3D, repeat, filter, Id, szCacheName, flags, eEndian, pRegion, bAsyncDevTexCreation);
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
	for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
	{
		int nTotal = 0;
		string final;
		char tmp[128];

		COMPILE_TIME_ASSERT(CRY_ARRAY_COUNT(sStreamNames) == VSF_NUM);
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
			SIZER_COMPONENT_NAME(Sizer, "ShaderCache");
			Sizer->AddObject(CHWShader::m_ShaderCache);
		}
		{
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
			SIZER_COMPONENT_NAME(Sizer, "SResourceContainer");
			Name = CShader::mfGetClassName();
			pRL = CBaseResource::GetResourcesForClass(Name);
			Sizer->AddObject(pRL);
		}
	}
	{
		SIZER_COMPONENT_NAME(Sizer, "Mesh");
		AUTO_LOCK(CRenderMesh::m_sLinkLock);
		for (util::list<CRenderMesh>* iter = CRenderMesh::m_MeshList.next; iter != &CRenderMesh::m_MeshList; iter = iter->next)
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

		AUTO_LOCK(m_sREResLock);
		CRendElement* pRE = CRendElement::m_RootGlobal.m_NextGlobal;
		while (pRE != &CRendElement::m_RootGlobal)
		{
			Sizer->AddObject(pRE);
			pRE = pRE->m_NextGlobal;
		}
	}
	{
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

IRenderAuxGeom* CD3D9Renderer::GetIRenderAuxGeom(void* jobID /*= 0*/)
{
#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
		return m_pRenderAuxGeomD3D->GetRenderAuxGeom(jobID);
#endif
	return &m_renderAuxGeomNull;
}

IColorGradingController* CD3D9Renderer::GetIColorGradingController()
{
	return m_pColorGradingControllerD3D;
}

IStereoRenderer* CD3D9Renderer::GetIStereoRenderer()
{
	return m_pStereoRenderer;
}

void CD3D9Renderer::PushFogVolume(class CREFogVolume* pFogVolume, const SRenderingPassInfo& passInfo)
{
	GetVolumetricFog().PushFogVolume(pFogVolume, passInfo);
}

bool CD3D9Renderer::IsStereoEnabled() const
{
	return GetS3DRend().IsStereoEnabled();
}

void CD3D9Renderer::EnableGPUTimers2(bool bEnabled)
{
	if (bEnabled)
		CSimpleGPUTimer::EnableTiming();
	else
		CSimpleGPUTimer::DisableTiming();
}

void CD3D9Renderer::AllowGPUTimers2(bool bAllow)
{
	if (bAllow)
		CSimpleGPUTimer::AllowTiming();
	else
		CSimpleGPUTimer::DisallowTiming();
}

const RPProfilerStats* CD3D9Renderer::GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread /*= true */)
{
	return m_pPipelineProfiler ? &m_pPipelineProfiler->GetBasicStats(eStat, bCalledFromMainThread ? m_RP.m_nFillThreadID : m_RP.m_nProcessThreadID) : NULL;
}

const RPProfilerStats* CD3D9Renderer::GetRPPStatsArray(bool bCalledFromMainThread /*= true */)
{
	return m_pPipelineProfiler ? m_pPipelineProfiler->GetBasicStatsArray(bCalledFromMainThread ? m_RP.m_nFillThreadID : m_RP.m_nProcessThreadID) : NULL;
}

int CD3D9Renderer::GetPolygonCountByType(uint32 EFSList, EVertexCostTypes vct, uint32 z, bool bCalledFromMainThread /*= true*/)
{
#if defined(ENABLE_PROFILING_CODE)
	return m_RP.m_PS[bCalledFromMainThread ? m_RP.m_nFillThreadID : m_RP.m_nProcessThreadID].m_nPolygonsByTypes[EFSList][vct][z];
#else
	return 0;
#endif
}

void CD3D9Renderer::PostLevelLoading()
{
	CRenderer::PostLevelLoading();
	if (gRenDev)
	{
		m_bStartLevelLoading = false;
		if (m_pRT->IsMultithreaded())
		{
			iLog->Log("-- Render thread was idle during level loading: %.3f secs", gRenDev->m_pRT->m_fTimeIdleDuringLoading);
			iLog->Log("-- Render thread was busy during level loading: %.3f secs", gRenDev->m_pRT->m_fTimeBusyDuringLoading);
		}

		m_pRT->RC_PostLoadLevel();
		m_cEF.mfSortResources();
	}

	{
		LOADING_TIME_PROFILE_SECTION(iSystem);
		CTexture::Precache();
	}
}

//====================================================================

void CD3D9Renderer::PostLevelUnload()
{
	if (m_pRT)
	{
		m_pRT->RC_FlushTextureStreaming(true);
		m_pRT->FlushAndWait();

		StaticCleanup();
		if (m_pColorGradingControllerD3D)
		{
			m_pColorGradingControllerD3D->ReleaseTextures();
		}
		if (CTexture::IsTextureExist(CTexture::s_ptexWaterVolumeTemp[0]))
		{
			CTexture::s_ptexWaterVolumeTemp[0]->ReleaseDeviceTexture(false);
		}
		if (CTexture::IsTextureExist(CTexture::s_ptexWaterVolumeTemp[1]))
		{
			CTexture::s_ptexWaterVolumeTemp[1]->ReleaseDeviceTexture(false);
		}
		for (unsigned int i = 0; i < m_TempDepths.Num(); i++)
		{
			SDepthTexture* pS = m_TempDepths[i];
			if (pS)
			{
				m_pRT->RC_ReleaseSurfaceResource(pS);
			}
		}

		PostProcessUtils().m_pCurDepthSurface = NULL;
		m_pRT->FlushAndWait();

		for (unsigned int i = 0; i < m_TempDepths.Num(); i++)
			SAFE_DELETE(m_TempDepths[i])
			m_TempDepths.Free();

		EF_ResetPostEffects(); // reset post effects
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
	if (!m_showRenderTargetInfo.bDisplayTransparent)
		SetState(GS_NODEPTHTEST);
	else
		SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	int x0, y0, w0, h0;
	GetViewport(&x0, &y0, &w0, &h0);
	SetColorOp(eCO_MODULATE, eCO_MODULATE, DEF_TEXARG0, DEF_TEXARG0);
	Set2DMode(true, 1, 1);
	m_cEF.mfRefreshSystemShader("Debug", CShaderMan::s_ShaderDebug);
	CShader* pSH = CShaderMan::s_ShaderDebug;

	RT_SetViewport(0, 0, m_width, m_height);
	float tileW = 1.f / static_cast<float>(m_showRenderTargetInfo.col);
	float tileH = 1.f / static_cast<float>(m_showRenderTargetInfo.col);
	const float tileGapW = tileW * 0.05f;
	const float tileGapH = tileH * 0.05f;
	if (m_showRenderTargetInfo.col != 1) // If not a fullsceen(= 1x1 table),
	{
		tileW -= tileGapW;
		tileH -= tileGapH;
	}

	// Draw render targets with a special debug shader.
	uint32 nPasses = 0;
	pSH->FXSetTechnique("Debug_RenderTarget");
	pSH->FXBegin(&nPasses, FEF_DONTSETTEXTURES | FEF_DONTSETSTATES);
	pSH->FXBeginPass(0);

	static CCryNameR colorMultiplierName("colorMultiplier");
	static CCryNameR showRTFlagsName("showRTFlags");
	int nTexStateLinear = CTexture::GetTexState(STexState(FILTER_LINEAR, true));
	int nTexStatePoint = CTexture::GetTexState(STexState(FILTER_POINT, true));

	size_t col2 = m_showRenderTargetInfo.col * m_showRenderTargetInfo.col;
	for (size_t i = 0; i < min(m_showRenderTargetInfo.rtList.size(), col2); ++i)
	{
		CTexture* pTex = m_showRenderTargetInfo.rtList[i].pTexture;
		if (NULL == pTex)
			continue;

		int row = i / m_showRenderTargetInfo.col;
		int col = i - row * m_showRenderTargetInfo.col;
		float curX = col * (tileW + tileGapW);
		float curY = row * (tileH + tileGapH);
		pTex->Apply(0, m_showRenderTargetInfo.rtList[i].bFiltered ? nTexStateLinear : nTexStatePoint);

		Vec4 channelWeight = m_showRenderTargetInfo.rtList[i].channelWeight;
		pSH->FXSetPSFloat(colorMultiplierName, &channelWeight, 1);

		Vec4 showRTFlags(0, 0, 0, 0); // onlyAlpha, RGBKEncoded, aliased, 0
		if (channelWeight.x == 0 && channelWeight.y == 0 && channelWeight.z == 0 && channelWeight.w > 0.5f)
		{
			showRTFlags.x = 1.0f;
		}
		showRTFlags.y = m_showRenderTargetInfo.rtList[i].bRGBKEncoded ? 1.0f : 0.0f;
		showRTFlags.z = m_showRenderTargetInfo.rtList[i].bAliased ? 1.0f : 0.0f;
		pSH->FXSetPSFloat(showRTFlagsName, &showRTFlags, 1);

		PostProcessUtils().DrawScreenQuad(pTex->GetWidth(), pTex->GetHeight(), curX, curY, curX + tileW, curY + tileH);
	}

	pSH->FXEndPass();
	pSH->FXEnd();

	// Draw overlay texts.
	for (size_t i = 0; i < min(m_showRenderTargetInfo.rtList.size(), col2); ++i)
	{
		CTexture* pTex = m_showRenderTargetInfo.rtList[i].pTexture;
		if (NULL == pTex)
			continue;

		int row = i / m_showRenderTargetInfo.col;
		int col = i - row * m_showRenderTargetInfo.col;
		float curX = col * (tileW + tileGapW);
		float curY = row * (tileH + tileGapH);
		gcpRendD3D->FX_SetState(GS_NODEPTHTEST);  // fix the state change by using WriteXY
		WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 - 15), 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", pTex->GetFormatName(), CTexture::NameForTextureType(pTex->GetTextureType()));
		WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 + 1), 1, 1, 1, 1, 1, 1, "%s   %d x %d", pTex->GetName(), pTex->GetWidth(), pTex->GetHeight());
	}

	RT_SetViewport(x0, y0, w0, h0);
	Set2DMode(false, 1, 1);
}

//====================================================================

ILog* iLog;
IConsole* iConsole;
ITimer* iTimer;
ISystem* iSystem;

//////////////////////////////////////////////////////////////////////////
struct CSystemEventListner_Render : public ISystemEventListener
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
			{
				if (gRenDev)
				{
					if (gRenDev->m_pRT)
					{
						gRenDev->m_pRT->RC_PrepareLevelTexStreaming();
						gRenDev->m_pRT->FlushAndWait();
					}
				}
			}
			break;

		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				if (gRenDev)
				{
					gRenDev->m_cEF.m_bActivated = false;
					gRenDev->m_bEndLevelLoading = false;
					gRenDev->m_bStartLevelLoading = true;
					gRenDev->m_bInLevel = true;
					if (gRenDev->m_pRT)
					{
						gRenDev->m_pRT->m_fTimeIdleDuringLoading = 0;
						gRenDev->m_pRT->m_fTimeBusyDuringLoading = 0;
					}
				}
				if (CRenderer::CV_r_texpostponeloading)
					CTexture::s_bPrecachePhase = true;

				CTexture::s_bInLevelPhase = true;

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
				CTexture::s_bInLevelPhase = false;
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

#if CRY_PLATFORM_WINDOWS
		case ESYSTEM_EVENT_MOVE:
			{
				// When moving the window, update the preferred monitor dimensions so full-screen will pick the closest monitor
				HWND hWnd = (HWND) gcpRendD3D->GetHWND();
				HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

				MONITORINFO monitorInfo;
				monitorInfo.cbSize = sizeof(MONITORINFO);
				GetMonitorInfo(hMonitor, &monitorInfo);

				gcpRendD3D->m_prefMonX = monitorInfo.rcMonitor.left;
				gcpRendD3D->m_prefMonY = monitorInfo.rcMonitor.top;
				gcpRendD3D->m_prefMonWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
				gcpRendD3D->m_prefMonHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
			}
			break;
#endif
		}
		bInside = false;
	}
};

static CSystemEventListner_Render g_system_event_listener_render;

extern "C" DLL_EXPORT IRenderer * CreateCryRenderInterface(ISystem * pSystem)
{
	ModuleInitISystem(pSystem, "CryRenderer");

	gbRgb = false;

	iConsole = gEnv->pConsole;
	iLog = gEnv->pLog;
	iTimer = gEnv->pTimer;
	iSystem = gEnv->pSystem;

	gcpRendD3D->InitRenderer();

	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	srand((uint32) li.QuadPart);

	g_CpuFlags = iSystem->GetCPUFlags();

	iSystem->GetISystemEventDispatcher()->RegisterListener(&g_system_event_listener_render);
	return gRenDev;
}

class CEngineModule_CryRenderer : public IEngineModule
{
	CRYINTERFACE_SIMPLE(IEngineModule)
	CRYGENERATE_SINGLETONCLASS(CEngineModule_CryRenderer, "EngineModule_CryRenderer", 0x540c91a7338e41d3, 0xaceeac9d55614450)

	virtual const char* GetName() override { return "CryRenderer"; }
	virtual const char* GetCategory() override { return "CryEngine"; }

	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override
	{
		ISystem* pSystem = env.pSystem;
		env.pRenderer = CreateCryRenderInterface(pSystem);
		return env.pRenderer != 0;
	}
};

CRYREGISTER_SINGLETON_CLASS(CEngineModule_CryRenderer)

CEngineModule_CryRenderer::CEngineModule_CryRenderer()
{
};

CEngineModule_CryRenderer::~CEngineModule_CryRenderer()
{
};

//=========================================================================================
void CD3D9Renderer::LockParticleVideoMemory()
{
	FRAME_PROFILER("LockParticleVideoMemory", gEnv->pSystem, PROFILE_RENDERER);
	SRenderPipeline& rp = gRenDev->m_RP;
	rp.m_particleBuffer.Lock();
}

void CD3D9Renderer::UnLockParticleVideoMemory()
{
	SRenderPipeline& rp = gRenDev->m_RP;
	rp.m_particleBuffer.Unlock();
}

void CD3D9Renderer::InsertParticleVideoDataFence()
{
	SRenderPipeline& rp = gRenDev->m_RP;

	rp.m_particleBuffer.SetFence();
}
//================================================================================================================================

void CD3D9Renderer::ActivateLayer(const char* pLayerName, bool activate)
{
	CDynTextureSourceLayerActivator::ActivateLayer(pLayerName, activate);
}

SDepthTexture CD3D9Renderer::FX_ReplaceMSAADepthBuffer(CTexture* pDepthBufferRT, bool bMSAA, D3DShaderResource*& pZTargetOrigSRV)
{
	SDepthTexture sBackup = m_DepthBufferOrigMSAA;

	m_DepthBufferOrigMSAA.pTexture = m_pZTexture;
	m_DepthBufferOrigMSAA.pTarget = m_pZTexture->GetDevTexture()->Get2DTexture();
	m_DepthBufferOrigMSAA.pSurface = m_pZTexture->GetDeviceDepthStencilView(0, -1, bMSAA, true);
	m_DepthBufferOrigMSAA.pTexture->AddRef();

	pZTargetOrigSRV = pDepthBufferRT->GetShaderResourceView(bMSAA ? SResourceView::DefaultViewMS : SResourceView::DefaultView);

	return sBackup;
}

void CD3D9Renderer::FX_RestoreMSAADepthBuffer(SDepthTexture sBackup, CTexture* pDepthBufferRT, bool bMSAA, D3DShaderResource*& pZTargetOrigSRV)
{
	// Restore original DSV/SRV
	m_DepthBufferOrigMSAA.pTexture->Release();
	m_DepthBufferOrigMSAA = sBackup;

	pDepthBufferRT->SetShaderResourceView(pZTargetOrigSRV, bMSAA);
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
	return m_pSwapChain ? m_pSwapChain->GetIHWMouseCursor() : NULL;
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
			DXGI_SWAP_CHAIN_DESC desc = { 0 };
			gcpRendD3D->m_pSwapChain->GetDesc(&desc);
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
			DXGI_SWAP_CHAIN_DESC desc = { 0 };
			gcpRendD3D->m_pSwapChain->GetDesc(&desc);
			fpEndFrameCap(desc.OutputWindow);
			CryLogAlways("Finished RenderDoc capture");
		}
	}
#endif
}

void CD3D9Renderer::PushVolumetricCloudBlocker(const Vec3& pos, const Vec3& param, int flag)
{
	if (m_pVolumetricCloudMan)
	{
		m_pVolumetricCloudMan->PushCloudBlocker(pos, param, flag);
	}
}

const std::vector<SSkinningData*>& CD3D9Renderer::GetComputeSkinningDataListRT() const
{
	unsigned poolId = (m_nPoolIndexRT) % 3;
	return m_computeSkinningData[poolId];
}
