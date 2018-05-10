// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RenderDisplayContext.h"
#include "RenderOutput.h"

static inline string GenerateUniqueTextureName(const string &prefix, uint32 id, const string &name)
{
	string uniqueTexName = prefix + string().Format("-%d", id);
	if (!name.empty())
	{
		uniqueTexName += '-';
		uniqueTexName += name;
	}
	return uniqueTexName;
}

//////////////////////////////////////////////////////////////////////////
CRenderDisplayContext::CRenderDisplayContext()
{
	m_hWnd = 0;
	m_pRenderOutput = std::make_shared<CRenderOutput>(this);

	m_lastRenderCamera[CCamera::eEye_Left ].SetMatrix(Matrix34(type_identity::IDENTITY));
	m_lastRenderCamera[CCamera::eEye_Right].SetMatrix(Matrix34(type_identity::IDENTITY));
}

//////////////////////////////////////////////////////////////////////////
CRenderDisplayContext::~CRenderDisplayContext()
{
	ShutDown();
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetHWND(HWND hWnd)
{
	if (hWnd == m_hWnd)
		return;

	int w = m_DisplayWidth, h = m_DisplayHeight;

#if CRY_PLATFORM_WINDOWS
	if (TRUE == ::IsWindow((HWND)hWnd))
	{
		RECT rc;
		if (TRUE == GetClientRect((HWND)hWnd, &rc))
		{
			// On Windows force screen resolution to be a real pixel size of the client rect of the real window
			w = rc.right - rc.left;
			h = rc.bottom - rc.top;
		}
	}
#endif

	m_hWnd = hWnd;

	InitializeDisplayResolution(w, h);

	// Create the output
	ChangeOutputIfNecessary(m_fullscreen);

	m_pRenderOutput = std::make_shared<CRenderOutput>(this);
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetViewport(const SRenderViewport& vp)
{
	m_viewport = vp;
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::BeginRendering(bool isHighDynamicRangeEnabled)
{
	// Create the swap chain if not already present
	if (m_pSwapChain == nullptr)
	{
		ChangeDisplayResolution(m_DisplayWidth, m_DisplayHeight, m_fullscreen);
	}

	// Toggle HDR/LDR output selection
	if (m_bHDRRendering != isHighDynamicRangeEnabled)
	{
		m_bHDRRendering = isHighDynamicRangeEnabled;
		m_pRenderOutput->ChangeOutputResolution(
			m_pRenderOutput->GetOutputResolution()[0],
			m_pRenderOutput->GetOutputResolution()[1]
		);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::EndRendering()
{
	m_bHDRRendering = false;
}

//////////////////////////////////////////////////////////////////////////
CTexture* CRenderDisplayContext::GetCurrentBackBuffer()
{
	uint32 index = 0;

#if (CRY_RENDERER_DIRECT3D >= 120) && defined(__dxgi1_4_h__) // Otherwise index front-buffer only
	IDXGISwapChain3* pSwapChain3;
	m_pSwapChain->QueryInterface(IID_GFX_ARGS(&pSwapChain3));
	if (pSwapChain3)
	{
		index = pSwapChain3->GetCurrentBackBufferIndex();
		pSwapChain3->Release();
	}
#endif
#if (CRY_RENDERER_VULKAN >= 10)
	index = m_pSwapChain->GetCurrentBackBufferIndex();
#endif
#if (CRY_RENDERER_GNM)
	// The GNM renderer uses the GPU to drive scan-out, so a command-list is required here.
	auto* pCommandList = GnmCommandList(GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterfaceImpl());
	index = m_pSwapChain->GnmGetCurrentBackBufferIndex(pCommandList);
#endif

	assert(index < m_backBuffersArray.size());

	return m_backBuffersArray[index];
}

CTexture* CRenderDisplayContext::GetPresentedBackBuffer()
{
	return m_pBackBufferPresented;
}

CTexture* CRenderDisplayContext::GetCurrentColorOutput()
{
	if (IsHighDynamicRange())
	{
		CRY_ASSERT(m_pColorTarget);
		return m_pColorTarget.get();
	}
	
	return GetCurrentBackBuffer();
}

CTexture* CRenderDisplayContext::GetCurrentDepthOutput()
{
	CRY_ASSERT(m_pDepthTarget || !NeedsDepthStencil());
	return m_pDepthTarget.get();
}

CTexture* CRenderDisplayContext::GetStorableColorOutput()
{
	if (IsHighDynamicRange())
	{
		CRY_ASSERT(m_pColorTarget);
		return m_pColorTarget.get();
	}

	// Toggle current back-buffer if transitioning into LDR output
	PostPresent();
	return m_pBackBufferProxy;
}

CTexture* CRenderDisplayContext::GetStorableDepthOutput()
{
	CRY_ASSERT(m_pDepthTarget || !NeedsDepthStencil());
	return m_pDepthTarget.get();
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::ShutDown()
{
	ReleaseResources();
	ReleaseSwapChain();
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::ReleaseResources()
{
	if (gcpRendD3D.GetDeviceContext().IsValid())
	{
		GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);
	}

	m_pRenderOutput->ReleaseResources();

	m_pColorTarget = nullptr;
	m_pDepthTarget = nullptr;

	m_pBackBufferPresented = nullptr;

	if (m_pBackBufferProxy)
	{
		m_pBackBufferProxy->RefDevTexture(nullptr);
		m_pBackBufferProxy.reset();
	}

	uint32 indices = m_backBuffersArray.size();
	for (int i = 0, n = indices; i < n; ++i)
	{
		if (m_backBuffersArray[i])
		{
			m_backBuffersArray[i]->SetDevTexture(nullptr);
			m_backBuffersArray[i].reset();
		}
	}

	m_bSwapProxy = true;
}

void CRenderDisplayContext::ReleaseSwapChain()
{
	if (m_pSwapChain != nullptr)
	{
#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
		m_pSwapChain->SetFullscreenState(FALSE, 0); // swapchain cannot be released in fullscreen state (DXGI restriction)
#endif

		unsigned long numRemainingReferences = m_pSwapChain->Release();
		CRY_ASSERT(numRemainingReferences == 0);
		m_pSwapChain = nullptr;
	}

#if CRY_PLATFORM_WINDOWS
	if (m_pOutput != nullptr)
	{
		unsigned long numRemainingReferences = m_pOutput->Release();
		CRY_ASSERT(numRemainingReferences == 0);
		m_pOutput = nullptr;
	}
#endif
}

RectI CRenderDisplayContext::GetCurrentMonitorBounds() const
{
#ifdef CRY_PLATFORM_WINDOWS
	// When moving the window, update the preferred monitor dimensions so full-screen will pick the closest monitor
	HMONITOR hMonitor = MonitorFromWindow(reinterpret_cast<HWND>(m_hWnd), MONITOR_DEFAULTTONEAREST);

	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &monitorInfo);

	return RectI{ monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top };
#else
	CRY_ASSERT_MESSAGE(false, "Not implemented for platform!");
	return RectI();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::InitializeDisplayResolution(int displayWidth, int displayHeight)
{
	m_DisplayWidth  = displayWidth;
	m_DisplayHeight = displayHeight;

	if (m_DisplayWidth && m_DisplayHeight)
		m_aspectRatio = (float)m_DisplayWidth / (float)m_DisplayHeight;

	SetViewport(SRenderViewport(0, 0, displayWidth, displayHeight));

	m_pRenderOutput->InitializeDisplayContext();
}

void CRenderDisplayContext::ChangeDisplayResolution(int displayWidth, int displayHeight, bool fullscreen, bool bResizeTarget, bool bForce)
{
#if !defined(CRY_PLATFORM_ORBIS)
	// No changes do not need to resize
	DXGI_SWAP_CHAIN_DESC desc;
	if (m_pSwapChain != nullptr && SUCCEEDED(m_pSwapChain->GetDesc(&desc)) && !bForce)
	{
		if (desc.BufferDesc.Width == displayWidth && desc.BufferDesc.Height == displayHeight && desc.BufferCount == GetBackBufferCount())
			return;
	}
#else
	if (m_DisplayWidth == displayWidth && m_DisplayHeight == displayHeight && m_pSwapChain && !bForce)
		return;
#endif

	ReleaseBackBuffers();

	InitializeDisplayResolution(displayWidth, displayHeight);

	m_fullscreen = fullscreen;

	if (m_pSwapChain == nullptr)
		AllocateSwapChain();
	else
		ResizeSwapChain(bResizeTarget);

	AllocateBackBuffers();
	AllocateColorTarget();
	AllocateDepthTarget();

	m_pRenderOutput->ReinspectDisplayContext();

	if (!IsEditorDisplay())
	{
		// When Base context is resized update system camera dimensions.
		CCamera camera = GetISystem()->GetViewCamera();
		camera.SetFrustum(displayWidth, displayHeight, camera.GetFov(), camera.GetNearPlane(), camera.GetFarPlane());
		GetISystem()->SetViewCamera(camera);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::PrePresent()
{
	m_pBackBufferPresented = GetCurrentBackBuffer();
	m_bSwapProxy = true;
}

void CRenderDisplayContext::PostPresent()
{
	if (!m_bSwapProxy)
		return;

	// Substitute current swap-chain back-buffer
	CDeviceTexture* pNewDeviceTex = GetCurrentBackBuffer()->GetDevTexture();
	CDeviceTexture* pOldDeviceTex =     m_pBackBufferProxy->GetDevTexture();

	// Trigger dirty-flag handling
	if (pNewDeviceTex != pOldDeviceTex)
		m_pBackBufferProxy->RefDevTexture(pNewDeviceTex);

	m_bSwapProxy = false;
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetLastCamera( CCamera::EEye eye,const CCamera& camera )
{
	m_lastRenderCamera[eye] = camera;
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateSwapChain()
{
#if !CRY_PLATFORM_DURANGO && !CRY_RENDERER_GNM
	if (m_pSwapChain != nullptr)
		return;

#if defined(USE_SDL2_VIDEO)
	DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8X8_UNORM;
#else
	DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif

	const int backbufferWidth  = m_DisplayWidth;
	const int backbufferHeight = m_DisplayHeight;
	const int backbufferCount  = m_backbufferCount;

	const bool windowed = !gcpRendD3D->IsFullscreen();
	const bool isMainContext = IsMainContext();

	IDXGISwapChain* pSwapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC scDesc;

	scDesc.BufferDesc.Width = backbufferWidth;
	scDesc.BufferDesc.Height = backbufferHeight;
	scDesc.BufferDesc.RefreshRate.Numerator = 0;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = fmt;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;

	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	const bool bWaitable = (isMainContext && CRenderer::CV_r_D3D12WaitableSwapChain && windowed);
	scDesc.BufferCount = backbufferCount;
	scDesc.OutputWindow = (HWND)m_hWnd;

	// Always create a swapchain for windowed mode as per Microsoft recommendations here: https://msdn.microsoft.com/en-us/library/windows/desktop/bb174579(v=vs.85).aspx
	// Specifically: "We recommend that you create a windowed swap chain and allow the end user to change the swap chain to full screen through SetFullscreenState; that is, do not set the Windowed member of DXGI_SWAP_CHAIN_DESC to FALSE to force the swap chain to be full screen."
	scDesc.Windowed = 1;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

#if (CRY_RENDERER_DIRECT3D >= 120)
	if (bWaitable)
	{
		// Set this flag to create a waitable object you can use to ensure rendering does not begin while a
		// frame is still being presented. When this flag is used, the swapchain's latency must be set with
		// the IDXGISwapChain2::SetMaximumFrameLatency API instead of IDXGIDevice1::SetMaximumFrameLatency.
		scDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
	}
#endif

	DXGI_RATIONAL desktopRefreshRate = scDesc.BufferDesc.RefreshRate;

#if CRY_PLATFORM_DESKTOP
	if (!windowed)
	{
		DXGI_MODE_DESC match;
		if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&scDesc.BufferDesc, &match, gcpRendD3D.DevInfo().Device())))
		{
			scDesc.BufferDesc = match;
			desktopRefreshRate = match.RefreshRate;
		}
	}
#endif

#if defined(SUPPORT_DEVICE_INFO)
	auto refreshRate = !windowed ? scDesc.BufferDesc.RefreshRate : desktopRefreshRate;
	m_refreshRateNumerator = refreshRate.Numerator;
	m_refreshRateDenominator = refreshRate.Denominator;
#endif

#if defined(SUPPORT_DEVICE_INFO)
	HRESULT hr = gcpRendD3D.DevInfo().Factory()->CreateSwapChain(gcpRendD3D.DevInfo().Device(), &scDesc, &pSwapChain);
#elif CRY_PLATFORM_ORBIS
	HRESULT hr = CCryDXOrbisGIFactory().CreateSwapChain(gcpRendD3D.GetDevice().GetRealDevice(), &scDesc, &pSwapChain);
#else
#error UNKNOWN PLATFORM TRYING TO CREATE SWAP CHAIN
#endif

	CRY_ASSERT(SUCCEEDED(hr) && pSwapChain != nullptr);
	hr = pSwapChain->QueryInterface(__uuidof(DXGISwapChain), (void**)&m_pSwapChain);
	CRY_ASSERT(SUCCEEDED(hr) && m_pSwapChain != 0);

	pSwapChain->Release();

#if (CRY_RENDERER_DIRECT3D >= 120)
	if (scDesc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)
	{
		CRY_ASSERT(scDesc.BufferCount > CRendererCVars::CV_r_MaxFrameLatency);
		pSwapChain->SetMaximumFrameLatency(CRendererCVars::CV_r_MaxFrameLatency);
	}
#endif

	ReadSwapChainSurfaceDesc();

#ifdef SUPPORT_DEVICE_INFO
	// Disable automatic DXGI alt + enter behavior
	gcpRendD3D->DevInfo().Factory()->MakeWindowAssociation(reinterpret_cast<HWND>(m_hWnd), DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
#endif

	if (m_fullscreen)
	{
		SetFullscreenState(m_fullscreen);
	}
#endif
}

#if CRY_PLATFORM_DURANGO
#if (CRY_RENDERER_DIRECT3D >= 120)
void CRenderDisplayContext::CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D12Device* pD3D12Device, CCryDX12Device* pDX12Device)
#else
void CRenderDisplayContext::CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D11Device* pD3D11Device)
#endif
{
	// Create full HD swap chain with backbuffer
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
	swapChainDesc.Width = m_DisplayWidth;
	swapChainDesc.Height = m_DisplayHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGIX_SWAP_CHAIN_MATCH_XBOX360_AND_PC;

	LOADING_TIME_PROFILE_SECTION_NAMED("CRenderDisplayContext::CreateXboxSwapChain: CreateSwapChainForCoreWindow()");
#if (CRY_RENDERER_DIRECT3D >= 120)
	IDXGISwapChain1ToCall* pDXGISwapChain = nullptr;
	HRESULT hr = pDXGIFactory->CreateSwapChainForCoreWindow(pD3D12Device, (IUnknown*)gEnv->pWindow, &swapChainDesc, nullptr, &pDXGISwapChain);
	if (hr == S_OK && pDXGISwapChain)
	{
		m_pSwapChain = CCryDX12SwapChain::Create(pDX12Device, pDXGISwapChain, &swapChainDesc);
	}
#else
	HRESULT hr = pDXGIFactory->CreateSwapChainForCoreWindow(pD3D11Device, (IUnknown*)gEnv->pWindow, &swapChainDesc, nullptr, &m_pSwapChain);
#endif

	CRY_ASSERT(SUCCEEDED(hr) && m_pSwapChain != nullptr);

	ReadSwapChainSurfaceDesc();
}
#endif

#if CRY_RENDERER_GNM
void CRenderDisplayContext::CreateGNMSwapChain()
{
	CGnmSwapChain::SDesc desc;
	desc.type = CGnmSwapChain::kTypeTelevision;
	desc.width = m_DisplayWidth;
	desc.height = m_DisplayHeight;
	desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.numBuffers = 2;
	HRESULT hr = GnmCreateSwapChain(desc, &m_pSwapChain) ? S_OK : E_FAIL;
	CRY_ASSERT(SUCCEEDED(hr) && m_pSwapChain != nullptr);

#if defined(SUPPORT_DEVICE_INFO)
	if (m_pSwapChain != nullptr)
	{
		DXGI_SWAP_CHAIN_DESC scDesc;
		m_pSwapChain->GetDesc(&scDesc);

		m_refreshRateNumerator = scDesc.BufferDesc.RefreshRate.Numerator;
		m_refreshRateDenominator = scDesc.BufferDesc.RefreshRate.Denominator;
	}
#endif

	ReadSwapChainSurfaceDesc();
}
#endif

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
static const char* GetScanlineOrderNaming(DXGI_MODE_SCANLINE_ORDER v)
{
	switch (v)
	{
		case DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE:
			return "progressive";
		case DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST:
			return "interlaced (upper field first)";
		case DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST:
			return "interlaced (lower field first)";
		default:
			return "unspecified";
	}
}
void UserOverrideDisplayProperties(DXGI_MODE_DESC& desc)
{
	if (gcpRendD3D->IsFullscreen())
	{
		if (CRenderer::CV_r_overrideRefreshRate > 0)
		{
			DXGI_RATIONAL& refreshRate = desc.RefreshRate;
			if (refreshRate.Denominator)
				gEnv->pLog->Log("Overriding refresh rate to %.2f Hz (was %.2f Hz).", (float)CRenderer::CV_r_overrideRefreshRate, (float)refreshRate.Numerator / (float)refreshRate.Denominator);
			else
				gEnv->pLog->Log("Overriding refresh rate to %.2f Hz (was undefined).", (float)CRenderer::CV_r_overrideRefreshRate);
			refreshRate.Numerator = (unsigned int)(CRenderer::CV_r_overrideRefreshRate * 1000.0f);
			refreshRate.Denominator = 1000;
		}

		if (CRenderer::CV_r_overrideScanlineOrder > 0)
		{
			DXGI_MODE_SCANLINE_ORDER old = desc.ScanlineOrdering;
			DXGI_MODE_SCANLINE_ORDER& so = desc.ScanlineOrdering;
			switch (CRenderer::CV_r_overrideScanlineOrder)
			{
				case 2:
					so = DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST;
					break;
				case 3:
					so = DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST;
					break;
				default:
					so = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
					break;
			}
			gEnv->pLog->Log("Overriding scanline order to %s (was %s).", GetScanlineOrderNaming(so), GetScanlineOrderNaming(old));
		}
	}
}
#endif

void CRenderDisplayContext::ResizeSwapChain(bool bResizeTarget)
{
#if !CRY_PLATFORM_DURANGO && !CRY_RENDERER_GNM
	// Wait for GPU to finish occupying the resources
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	const int backbufferWidth  = m_DisplayWidth;
	const int backbufferHeight = m_DisplayHeight;

	m_swapChainDesc.Width  = backbufferWidth;
	m_swapChainDesc.Height = backbufferHeight;

#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
	DXGI_SWAP_CHAIN_DESC scDesc;
	m_pSwapChain->GetDesc(&scDesc);

	scDesc.BufferDesc.Width  = backbufferWidth;
	scDesc.BufferDesc.Height = backbufferHeight;
	scDesc.BufferCount       = GetBackBufferCount();

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	if (IsMainContext())
	{
		UserOverrideDisplayProperties(scDesc.BufferDesc);
	}
#endif

	//	TODO: Activate once sRGB encoding is gone from the shader
	//	m_devInfo.SwapChainDesc().BufferDesc.Format =
	//		((nNewColDepth / 4) <=  8 ? DXGI_FORMAT_R8G8B8A8_UNORM :
	//		((nNewColDepth / 4) <= 10 ? DXGI_FORMAT_R10G10B10A2_UNORM :
	//		                            DXGI_FORMAT_R16G16B16A16_FLOAT));

	// NOTE: Going fullscreen doesn't require freeing the back-buffers
	SetFullscreenState(m_fullscreen);

	// Resize the hWnd's dimensions associated with the SwapChain (triggers EVENT)
	if (bResizeTarget)
	{
		HRESULT rr = m_pSwapChain->ResizeTarget(&scDesc.BufferDesc);
		CRY_ASSERT(SUCCEEDED(rr));
	}

	// Configure maximum frame latency
#if CRY_PLATFORM_WINDOWS && CRY_RENDERER_DIRECT3D
	{
		CRY_ASSERT(scDesc.BufferCount > CRendererCVars::CV_r_MaxFrameLatency);

		DXGIDevice* pDXGIDevice = nullptr;
		if (SUCCEEDED(gcpRendD3D->GetDevice().GetRealDevice()->QueryInterface(__uuidof(DXGIDevice), (void**)&pDXGIDevice)) && pDXGIDevice)
			pDXGIDevice->SetMaximumFrameLatency(CRendererCVars::CV_r_MaxFrameLatency);
		SAFE_RELEASE(pDXGIDevice);
	}
#endif

	// Resize the Resources associated with the SwapChain
	{
		HRESULT hr = m_pSwapChain->ResizeBuffers(scDesc.BufferCount, scDesc.BufferDesc.Width, scDesc.BufferDesc.Height, DXGI_FORMAT_UNKNOWN, scDesc.Flags);
		CRY_ASSERT(SUCCEEDED(hr));
	}
#endif

	ReadSwapChainSurfaceDesc();
#endif
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetFullscreenState(bool isFullscreen)
{
	if (m_pSwapChain == nullptr)
		return;

#if CRY_PLATFORM_WINDOWS
	if (isFullscreen)
	{
		// Attempt to move the window to the foreground and activate it
		// If this failed, chances are that the user alt-tabbed to another window and started working there
		// In that case it is not possible to activate the window, so we opt to minimize and let the user return to fullscreen when they want
		// See https://msdn.microsoft.com/en-us/library/windows/desktop/ms633539(v=vs.85).aspx
		const bool wasActivated = SetForegroundWindow(reinterpret_cast<HWND>(m_hWnd)) != 0;
		if (!wasActivated)
		{
			isFullscreen = false;
			ShowWindow(reinterpret_cast<HWND>(m_hWnd), SW_MINIMIZE);
		}
	}
#endif

#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
	if (isFullscreen)
	{
		const int backbufferWidth = m_DisplayWidth;
		const int backbufferHeight = m_DisplayHeight;

		DXGI_SWAP_CHAIN_DESC scDesc;
		m_pSwapChain->GetDesc(&scDesc);

#if (CRY_RENDERER_DIRECT3D >= 120)
		CRY_ASSERT_MESSAGE((scDesc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) == 0, "Fullscreen does not work with Waitable SwapChain");
#endif

		scDesc.BufferDesc.Width = backbufferWidth;
		scDesc.BufferDesc.Height = backbufferHeight;

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
		UserOverrideDisplayProperties(scDesc.BufferDesc);
#endif

		HRESULT rr = m_pSwapChain->ResizeTarget(&scDesc.BufferDesc);
		CRY_ASSERT(SUCCEEDED(rr));
	}

#if !CRY_PLATFORM_DURANGO
	m_pSwapChain->SetFullscreenState(isFullscreen, isFullscreen ? m_pOutput : nullptr);
#else
	m_pSwapChain->SetFullscreenState(isFullscreen, nullptr);
#endif
#endif
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::EnableVerticalSync(bool enable)
{
#if defined(SUPPORT_DEVICE_INFO)
	m_syncInterval = enable ? 1 : 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateBackBuffers()
{
	unsigned indices = 1;
#if CRY_RENDERER_GNM
	CGnmSwapChain::SDesc desc = m_pSwapChain->GnmGetDesc();
	indices = desc.numBuffers;
#elif (CRY_RENDERER_DIRECT3D >= 120) || (CRY_RENDERER_VULKAN >= 10)
	DXGI_SWAP_CHAIN_DESC scDesc;
	m_pSwapChain->GetDesc(&scDesc);
	indices = scDesc.BufferCount;
#endif

	m_backBuffersArray.resize(indices, nullptr);
	m_pBackBufferPresented = nullptr;

	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const uint32 displayWidth  = m_DisplayWidth;
	const uint32 displayHeight = m_DisplayHeight;
	const ETEX_Format displayFormat = DeviceFormats::ConvertToTexFormat(m_swapChainDesc.Format);

	char str[40];
	
	// ------------------------------------------------------------------------------
	if (!m_pBackBufferProxy)
	{
		sprintf(str, "$SwapChainBackBuffer %d:Current", m_uniqueId);

		m_pBackBufferProxy = CTexture::GetOrCreateTextureObject(str, displayWidth, displayHeight, 1, eTT_2D, renderTargetFlags, displayFormat);
		m_pBackBufferProxy->SRGBRead(DeviceFormats::ConvertToSRGB(DeviceFormats::ConvertFromTexFormat(displayFormat)) == m_swapChainDesc.Format);
	}

	if (!m_pBackBufferProxy->GetDevTexture())
	{
		m_pBackBufferProxy->Invalidate(displayWidth, displayHeight, displayFormat);
	}

	CRY_ASSERT(m_pBackBufferProxy->GetWidth    () == displayWidth );
	CRY_ASSERT(m_pBackBufferProxy->GetHeight   () == displayHeight);
	CRY_ASSERT(m_pBackBufferProxy->GetSrcFormat() == displayFormat);

	// ------------------------------------------------------------------------------
	for (int i = 0, n = indices; i < n; ++i)
	{
		// Prepare back-buffer texture(s)
		if (!m_backBuffersArray[i])
		{
			sprintf(str, "$SwapChainBackBuffer %d:Buffer %d", m_uniqueId, i);

			m_backBuffersArray[i] = CTexture::GetOrCreateTextureObject(str, displayWidth, displayHeight, 1, eTT_2D, renderTargetFlags, displayFormat);
			m_backBuffersArray[i]->SRGBRead(DeviceFormats::ConvertToSRGB(DeviceFormats::ConvertFromTexFormat(displayFormat)) == m_swapChainDesc.Format);
		}

		if (!m_backBuffersArray[i]->GetDevTexture())
		{
#if   CRY_RENDERER_GNM
			CGnmTexture* pBackBuffer = m_pSwapChain->GnmGetBuffer(i);
#elif CRY_RENDERER_VULKAN
			D3DTexture* pBackBuffer = m_pSwapChain->GetVKBuffer(i);
			assert(pBackBuffer != nullptr);
#else
			D3DTexture* pBackBuffer = nullptr;
			HRESULT hr = m_pSwapChain->GetBuffer(i, __uuidof(D3DTexture), (void**)&pBackBuffer);
			assert(SUCCEEDED(hr) && pBackBuffer != nullptr);
#endif

			m_backBuffersArray[i]->Invalidate(displayWidth, displayHeight, displayFormat);
			m_backBuffersArray[i]->SetDevTexture(CDeviceTexture::Associate(m_backBuffersArray[i]->GetLayout(), pBackBuffer));
			
			// Guarantee that the back-buffers are cleared on first use (e.g. Flash alpha-blends onto the back-buffer)
			// NOTE: GNM requires shaders to be initialized before issuing any draws/clears/copies/resolves. This is not yet the case here.
			// NOTE: Can only access current back-buffer on DX12
#if !(CRY_RENDERER_GNM || CRY_RENDERER_DIRECT3D >= 120)
			CClearSurfacePass::Execute(m_backBuffersArray[i], Clr_Transparent);
#endif
		}

		CRY_ASSERT(m_backBuffersArray[i]->GetWidth    () == displayWidth );
		CRY_ASSERT(m_backBuffersArray[i]->GetHeight   () == displayHeight);
		CRY_ASSERT(m_backBuffersArray[i]->GetSrcFormat() == displayFormat);
	}
	
	// Assign first back-buffer on initialization
	m_pBackBufferProxy->RefDevTexture(GetCurrentBackBuffer()->GetDevTexture());
	m_bSwapProxy = false;
}

void CRenderDisplayContext::ReleaseBackBuffers()
{
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	m_pBackBufferPresented = nullptr;

	if (m_pBackBufferProxy)
		m_pBackBufferProxy->RefDevTexture(nullptr);

	// Keep the CTextures for reuse, remove the device-resources and trigger dirty-handling
	for (int i = 0, n = m_backBuffersArray.size(); i < n; ++i)
	{
		if (m_backBuffersArray[i])
			m_backBuffersArray[i]->SetDevTexture(nullptr);
	}

	m_bSwapProxy = true;
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateColorTarget()
{
	const ETEX_Format eHDRFormat = CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;
	const ColorF clearValue = Clr_Empty;

	// NOTE: Actual device texture allocation happens just before rendering.
	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const string uniqueTexName = GenerateUniqueTextureName("$HDR-Overlay", m_uniqueId, m_name);

	m_pColorTarget = CTexture::GetOrCreateRenderTarget(uniqueTexName.c_str(), m_DisplayWidth, m_DisplayHeight, clearValue, eTT_2D, renderTargetFlags, eHDRFormat);
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateDepthTarget()
{
	m_pDepthTarget = nullptr;
	if (!NeedsDepthStencil())
		return;

	const ETEX_Format eZFormat =
		gRenDev->GetDepthBpp() == 32 ? eTF_D32FS8 :
		gRenDev->GetDepthBpp() == 24 ? eTF_D24S8  :
		gRenDev->GetDepthBpp() ==  8 ? eTF_D16S8  : eTF_D16;

	const float  clearDepth   = CRenderer::CV_r_ReverseDepth ? 0.f : 1.f;
	const uint   clearStencil = 1;
	const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

	// Create the native resolution depth stencil buffer for overlay rendering if needed
	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL;
	const string uniqueOverlayTexName = GenerateUniqueTextureName("$Z-Overlay", m_uniqueId, m_name);

	m_pDepthTarget = CTexture::GetOrCreateDepthStencil(uniqueOverlayTexName.c_str(), m_DisplayWidth, m_DisplayHeight, clearValues, eTT_2D, renderTargetFlags, eZFormat);
}

//////////////////////////////////////////////////////////////////////////
bool CRenderDisplayContext::ReadSwapChainSurfaceDesc()
{
	HRESULT hr = S_OK;
	ZeroMemory(&m_swapChainDesc, sizeof(DXGI_SURFACE_DESC));

	//////////////////////////////////////////////////////////////////////////
	// Read Back Buffers Surface description from the Swap Chain
#if CRY_RENDERER_GNM
	CGnmSwapChain::SDesc desc = m_pSwapChain->GnmGetDesc();
	m_swapChainDesc.Width = desc.width;
	m_swapChainDesc.Height = desc.height;
	m_swapChainDesc.Format = desc.format;
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.SampleDesc.Quality = 0;
	hr = m_pSwapChain->GnmIsValid() ? S_OK : E_FAIL;
#else
	DXGI_SWAP_CHAIN_DESC backBufferSurfaceDesc;
	hr = m_pSwapChain->GetDesc(&backBufferSurfaceDesc);

	m_swapChainDesc.Width = (UINT)backBufferSurfaceDesc.BufferDesc.Width;
	m_swapChainDesc.Height = (UINT)backBufferSurfaceDesc.BufferDesc.Height;
#if defined(SUPPORT_DEVICE_INFO)
	m_swapChainDesc.Format = backBufferSurfaceDesc.BufferDesc.Format;
	m_swapChainDesc.SampleDesc = backBufferSurfaceDesc.SampleDesc;
#elif CRY_RENDERER_VULKAN
	m_swapChainDesc.Format = backBufferSurfaceDesc.BufferDesc.Format;
	m_swapChainDesc.SampleDesc = backBufferSurfaceDesc.SampleDesc;
#elif CRY_PLATFORM_DURANGO
	m_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.SampleDesc.Quality = 0;
#endif
#endif
	//////////////////////////////////////////////////////////////////////////
	assert(!FAILED(hr));
	return !FAILED(hr);
}

void CRenderDisplayContext::ChangeOutputIfNecessary(bool isFullscreen)
{
#if CRY_PLATFORM_WINDOWS
	HMONITOR hMonitor = MonitorFromWindow(reinterpret_cast<HWND>(m_hWnd), MONITOR_DEFAULTTONEAREST);

	bool isWindowOnExistingOutputMonitor = false;

	DXGI_OUTPUT_DESC outputDesc;
	if (m_pOutput != nullptr && SUCCEEDED(m_pOutput->GetDesc(&outputDesc)))
	{
		isWindowOnExistingOutputMonitor = outputDesc.Monitor == hMonitor;
	}

	// Output will need to be recreated if we switched to fullscreen on a different monitor
	if(!isWindowOnExistingOutputMonitor && isFullscreen)
	{
		ReleaseBackBuffers();
		ReleaseSwapChain();
	}

	if (m_pOutput == nullptr)
	{
		// Find the output that matches the monitor our window is currently on
		IDXGIAdapter1* pAdapter = gcpRendD3D->DevInfo().Adapter();
		IDXGIOutput* pOutput = nullptr;
		unsigned int i = 0;
		while (pAdapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND)
		{
			DXGI_OUTPUT_DESC outputDesc;
			if (SUCCEEDED(pOutput->GetDesc(&outputDesc)))
			{
				if (outputDesc.Monitor == hMonitor)
				{
					// Promote interfaces to the required level
					pOutput->QueryInterface(__uuidof(DXGIOutput), (void**)&m_pOutput);
					break;
				}
				else
				{
					++i;
				}
			}
			else
			{
				++i;
			}
		}

		if (m_pOutput == nullptr)
		{
			if (SUCCEEDED(pAdapter->EnumOutputs(0, &pOutput)))
			{
				// Promote interfaces to the required level
				pOutput->QueryInterface(__uuidof(DXGIOutput), (void**)&m_pOutput);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "No display connected for rendering!");
				CRY_ASSERT(false);
			}
		}

		// Release the reference added by QueryInterface
		const unsigned long numRemainingReferences = m_pOutput->Release();
		CRY_ASSERT(numRemainingReferences == 1);
	}
#endif
}

#if CRY_PLATFORM_WINDOWS
void CRenderDisplayContext::EnforceFullscreenPreemption()
{
	if (IsMainContext() && CRenderer::CV_r_FullscreenPreemption && gcpRendD3D->IsFullscreen())
	{
		HRESULT hr = m_pSwapChain->Present(0, DXGI_PRESENT_TEST);
		if (hr == DXGI_STATUS_OCCLUDED)
		{
			if (::GetFocus() == reinterpret_cast<HWND>(m_hWnd))
				::BringWindowToTop(reinterpret_cast<HWND>(m_hWnd));
		}
	}
}
#endif // #if CRY_PLATFORM_WINDOWS
