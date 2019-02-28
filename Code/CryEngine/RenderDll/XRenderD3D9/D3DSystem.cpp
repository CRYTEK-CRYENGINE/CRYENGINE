// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryString/UnicodeFunctions.h>
#include <CryCore/Platform/WindowsUtils.h>
#include <CrySystem/IProjectManager.h>

#include "D3DStereo.h"
#include "D3DPostProcess.h"
#include "D3DREBreakableGlassBuffer.h"
#include "NullD3D11Device.h"
#include "PipelineProfiler.h"
#include <CryInput/IHardwareMouse.h>
#include <Common/RenderDisplayContext.h>

#if CRY_PLATFORM_WINDOWS
	#if defined(USE_AMD_API)
		#include AMD_API_HEADER
LINK_THIRD_PARTY_LIBRARY(AMD_API_LIB)

		AGSContext* g_pAGSContext = nullptr;
		AGSConfiguration g_pAGSContextConfiguration = {};
		AGSGPUInfo g_gpuInfo;
		unsigned int g_extensionsSupported = 0;
	#endif

	#if defined(USE_NV_API)
		#include NV_API_HEADER
LINK_THIRD_PARTY_LIBRARY(NV_API_LIB)
	#endif

	#if defined(USE_AMD_EXT)
		#include <AMD/AMD_Extensions/AmdDxExtDepthBoundsApi.h>

		bool g_bDepthBoundsTest = true;
		IAmdDxExt* g_pExtension = NULL;
		IAmdDxExtDepthBounds* g_pDepthBoundsTest = NULL;
	#endif
#endif

// Only needed to load AMD_AGS
#undef LoadLibrary

#if CRY_PLATFORM_WINDOWS
// Count monitors helper
static BOOL CALLBACK CountConnectedMonitors(HMONITOR hMonitor, HDC hDC, LPRECT pRect, LPARAM opaque)
{
	uint* count = reinterpret_cast<uint*>(opaque);
	(*count)++;
	return TRUE;
}

#endif

void CD3D9Renderer::DisplaySplash()
{
#if CRY_PLATFORM_WINDOWS
	if (IsEditorMode())
		return;

	HBITMAP hImage = (HBITMAP)LoadImage(CryGetCurrentModule(), "splash.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

	if (hImage != INVALID_HANDLE_VALUE)
	{
		RECT rect;
		HDC hDC = GetDC(m_hWnd);
		HDC hDCBitmap = CreateCompatibleDC(hDC);
		BITMAP bm;

		GetClientRect(m_hWnd, &rect);
		GetObjectA(hImage, sizeof(bm), &bm);
		SelectObject(hDCBitmap, hImage);

		DWORD x = rect.left + (((rect.right - rect.left) - bm.bmWidth) >> 1);
		DWORD y = rect.top + (((rect.bottom - rect.top) - bm.bmHeight) >> 1);

		//    BitBlt(hDC, x, y, bm.bmWidth, bm.bmHeight, hDCBitmap, 0, 0, SRCCOPY);

		RECT Rect;
		GetWindowRect(m_hWnd, &Rect);
		StretchBlt(hDC, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top, hDCBitmap, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

		DeleteObject(hImage);
		DeleteDC(hDCBitmap);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

static CryCriticalSection gs_contextLock;

CRenderDisplayContext* CD3D9Renderer::FindDisplayContext(const SDisplayContextKey& key) threadsafe const
{
	if (key == SDisplayContextKey{})
		return GetBaseDisplayContext();

	AUTO_LOCK(gs_contextLock); // Not thread safe without this

	auto it = m_displayContexts.find(key);
	if (it != m_displayContexts.end())
		return it->second.get();

	return nullptr;
}

std::pair<bool, SDisplayContextKey> CD3D9Renderer::SetCurrentContext(const SDisplayContextKey &key) threadsafe
{
	const auto prevKey = m_activeContextKey;

	if (key == SDisplayContextKey{})
	{
		UpdateActiveContext(m_pBaseDisplayContext, {});
		return { true, prevKey };
	}

	auto it = m_displayContexts.find(key);
	if (it == m_displayContexts.end())
		return { false, prevKey };
	UpdateActiveContext(it->second, key);
	return { true, prevKey };
}

SDisplayContextKey CD3D9Renderer::AddCustomContext(const CRenderDisplayContextPtr &context) threadsafe
{
	SDisplayContextKey key;
	key.key.emplace<uint32_t>(context->m_uniqueId);

	{
		AUTO_LOCK(gs_contextLock); // Not thread safe without this
		m_displayContexts.emplace(std::make_pair(key, context));
	}

	return key;
}

SDisplayContextKey CD3D9Renderer::CreateSwapChainBackedContext(const SDisplayContextDescription& desc) threadsafe
{
	LOADING_TIME_PROFILE_SECTION;

	const int width = desc.screenResolution.x;
	const int height = desc.screenResolution.y;
	const std::string name = desc.type == IRenderer::eViewportType_Default ? "MainView-SwapChain" : "SecView-SwapChain";

	auto pDC = std::make_shared<CSwapChainBackedRenderDisplayContext>(desc, name, m_uniqueDisplayContextId++);

	pDC->m_bMainViewport = desc.type == IRenderer::eViewportType_Default;
	pDC->m_desc.renderFlags |= pDC->m_bMainViewport ? FRT_OVERLAY_DEPTH | FRT_OVERLAY_STENCIL : 0;
	pDC->m_desc.superSamplingFactor.x = std::max<int>(1, desc.superSamplingFactor.x);
	pDC->m_desc.superSamplingFactor.y = std::max<int>(1, desc.superSamplingFactor.y);
	pDC->m_desc.screenResolution.x = 0;
	pDC->m_desc.screenResolution.y = 0;

	pDC->m_nSSSamplesX = pDC->m_desc.superSamplingFactor.x;
	pDC->m_nSSSamplesY = pDC->m_desc.superSamplingFactor.y;

	pDC->CreateSwapChain(desc.handle, desc.vsync);

	SDisplayContextKey key;
	if (desc.handle)
		key.key.emplace<HWND>(desc.handle);
	else
		key.key.emplace<uint32_t>(pDC->m_uniqueId);

	if (width * height)
	{
		pDC->ChangeDisplayResolution(width, height);
	}

	{
		AUTO_LOCK(gs_contextLock); // Not thread safe without this
		m_displayContexts.emplace(std::make_pair(key, std::move(pDC)));
	}

	return key;
}

void CD3D9Renderer::ResizeContext(CRenderDisplayContext *pDC, int width, int height) threadsafe
{
	if (pDC->m_desc.screenResolution.x != width ||
		pDC->m_desc.screenResolution.y != height)
	{
		pDC->m_desc.screenResolution.x = width;
		pDC->m_desc.screenResolution.y = height;

		ChangeViewport(pDC, 0, 0, width, height);

		// Must be deferred to Render Thread
		gRenDev->ExecuteRenderThreadCommand([=] { EF_DisableTemporalEffects(); }, 
			ERenderCommandFlags::None
		);
	}
}

void CD3D9Renderer::ResizeContext(const SDisplayContextKey& key, int width, int height) threadsafe
{
	CRenderDisplayContext* pDC = FindDisplayContext(key);
	if (pDC)
		ResizeContext(pDC, width, height);
}

bool CD3D9Renderer::DeleteContext(const SDisplayContextKey& key) threadsafe
{
	// Make sure there are no outstanding render commands which use the current viewport
	FlushRTCommands(true, true, true);

	AUTO_LOCK(gs_contextLock); // Not thread safe without this

	auto it = m_displayContexts.find(key);
	if (it == m_displayContexts.end())
		return false;

	auto deletedContext = std::move(it->second);
	m_displayContexts.erase(it);

	// If deleting active context, set active context to some other context
	if (m_pActiveContext == deletedContext)
	{
		const auto *pair = m_displayContexts.size() ? &*m_displayContexts.begin() : nullptr;
		UpdateActiveContext(pair->second, pair->first);
	}

	return true;
}

void CD3D9Renderer::UpdateActiveContext(const std::shared_ptr<CRenderDisplayContext> &ctx, const SDisplayContextKey &key)
{
	m_pActiveContext = ctx;
	m_activeContextKey = key;
}

CRenderDisplayContext* CD3D9Renderer::GetActiveDisplayContext() const
{
	return m_pActiveContext ? m_pActiveContext.get() : m_pBaseDisplayContext.get();
}

CSwapChainBackedRenderDisplayContext* CD3D9Renderer::GetBaseDisplayContext() const
{
	return m_pBaseDisplayContext.get();
}

WIN_HWND CD3D9Renderer::GetCurrentContextHWND()
{
	return m_pActiveContext && m_pActiveContext->IsSwapChainBacked() ? 
		static_cast<CSwapChainBackedRenderDisplayContext*>(m_pActiveContext.get())->GetWindowHandle() : 
		m_hWnd;
}

#ifdef CRY_PLATFORM_WINDOWS
RectI CD3D9Renderer::GetDefaultContextWindowCoordinates()
{
	auto hWnd = m_pBaseDisplayContext->GetWindowHandle();
	{
		AUTO_LOCK(gs_contextLock); // Not thread safe without this

		for (const auto& pair : m_displayContexts)
		{
			if (pair.second->IsMainViewport() && stl::holds_alternative<HWND>(pair.first.key))
				hWnd = stl::get<HWND>(pair.first.key);
		}
	}

	RECT rcClient;
	::GetClientRect(hWnd, &rcClient);
	::ClientToScreen(hWnd, (LPPOINT)&rcClient.left);
	::ClientToScreen(hWnd, (LPPOINT)&rcClient.right);

	return RectI
	{
		rcClient.left,
		rcClient.top,
		static_cast<int>(m_pBaseDisplayContext->GetDisplayResolution().x),
		static_cast<int>(m_pBaseDisplayContext->GetDisplayResolution().y)
	};
}
#endif

bool CD3D9Renderer::IsCurrentContextMainVP()
{
	return m_pActiveContext ? m_pActiveContext->IsMainViewport() : true;
}

bool CD3D9Renderer::ChangeRenderResolution(int nNewRenderWidth, int nNewRenderHeight, CRenderView* pRenderView)
{
	if (m_bDeviceLost)
		return true;

#if !defined(_RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	if (m_pRT && !m_pRT->IsRenderThread()) __debugbreak();
#endif

	pRenderView->ChangeRenderResolution(nNewRenderWidth, nNewRenderHeight, false);
	pRenderView->SetViewport(SRenderViewport(0, 0, nNewRenderWidth, nNewRenderHeight));

	return true;
}

bool CD3D9Renderer::ChangeOutputResolution(int nNewOutputWidth, int nNewOutputHeight, CRenderOutput* pRenderOutput)
{
	if (m_bDeviceLost)
		return true;

#if !defined(_RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	if (m_pRT && !m_pRT->IsRenderThread()) __debugbreak();
#endif

	pRenderOutput->ChangeOutputResolution(nNewOutputWidth, nNewOutputHeight);
	pRenderOutput->SetViewport(SRenderViewport(0, 0, nNewOutputWidth, nNewOutputHeight));

	return true;
}

bool CD3D9Renderer::ChangeDisplayResolution(int nNewDisplayWidth, int nNewDisplayHeight, int nNewColDepth, int nNewRefreshHZ, EWindowState previousWindowState, bool bForceReset, CRenderDisplayContext* pDC)
{
	CSwapChainBackedRenderDisplayContext* pBC = GetBaseDisplayContext();

	iLog->Log("Changing resolution...");

	m_isChangingResolution = true;

	const int  nPrevColorDepth = m_cbpp;
	if (nNewColDepth < 24)
		nNewColDepth = 16;
	else
		nNewColDepth = 32;

	bool wasFullscreen = previousWindowState == EWindowState::Fullscreen;

	// Save the new dimensions
	m_cbpp = nNewColDepth;
#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	m_overrideRefreshRate = CV_r_overrideRefreshRate;
	m_overrideScanlineOrder = CV_r_overrideScanlineOrder;
#endif

	m_VSync = !IsEditorMode() ? CV_r_vsync : 0;

	if (IsFullscreen() && nNewColDepth == 16)
	{
		m_zbpp = 16;
		m_sbpp = 0;
	}

	RestoreGamma();

	CRY_ASSERT(!IsEditorMode() || bForceReset);

	HRESULT hr = S_OK;
	if (!IsEditorMode() && (pDC == pBC))
	{
		// This is only allowed for the main viewport
		CRY_ASSERT(pBC->IsMainViewport());

		GetS3DRend().ReleaseBuffers();

#if defined(SUPPORT_DEVICE_INFO)
#if CRY_PLATFORM_WINDOWS
		// disable floating point exceptions due to driver bug when switching to fullscreen
		SCOPED_DISABLE_FLOAT_EXCEPTIONS();
#endif
		if (m_CVWidth)
			m_CVWidth->Set(nNewDisplayWidth);
		if (m_CVHeight)
			m_CVHeight->Set(nNewDisplayHeight);
		if (m_CVColorBits)
			m_CVColorBits->Set(nNewColDepth);

		AdjustWindowForChange(nNewDisplayWidth, nNewDisplayHeight, previousWindowState);

		pBC->ChangeOutputIfNecessary(IsFullscreen(), m_VSync != 0);

#if DURANGO_ENABLE_ASYNC_DIPS
		WaitForAsynchronousDevice();
#endif

// 		OnD3D11PostCreateDevice(m_devInfo.Device());

		pBC->SetFullscreenState(IsFullscreen());
		pBC->ChangeDisplayResolution(nNewDisplayWidth, nNewDisplayHeight);

		if (gEnv->pHardwareMouse)
			gEnv->pHardwareMouse->GetSystemEventListener()->OnSystemEvent(ESYSTEM_EVENT_TOGGLE_FULLSCREEN, IsFullscreen() ? 1 : 0, 0);
#endif
	}
	else
	{
		pDC->ChangeDisplayResolution(nNewDisplayWidth, nNewDisplayHeight);
		if (pDC->IsSwapChainBacked())
			static_cast<CSwapChainBackedRenderDisplayContext*>(pDC)->SetFullscreenState(IsFullscreen());
	}

	CRendererResources::OnDisplayResolutionChanged(nNewDisplayWidth, nNewDisplayHeight);

	ICryFont* pCryFont = gEnv->pCryFont;
	if (pCryFont)
	{
		IFFont* pFont = pCryFont->GetFont("default");
	}

	PostDeviceReset();

	m_isChangingResolution = false;

	return true;
}

bool CD3D9Renderer::ChangeDisplayResolution(int nNewDisplayWidth, int nNewDisplayHeight, int nNewColDepth, int nNewRefreshHZ, EWindowState previousWindowState, bool bForceReset, const SDisplayContextKey& displayContextKey)
{
	if (m_bDeviceLost)
		return true;

#if !defined(_RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	if (m_pRT && !m_pRT->IsRenderThread()) __debugbreak();
#endif

	CRenderDisplayContext* pBC = GetBaseDisplayContext();
	CRenderDisplayContext* pDC = pBC;
	if (displayContextKey != SDisplayContextKey{})
		pDC = FindDisplayContext(displayContextKey);

	return ChangeDisplayResolution(nNewDisplayWidth, nNewDisplayHeight, nNewColDepth, nNewRefreshHZ, previousWindowState, bForceReset, pDC);
}

void CD3D9Renderer::PostDeviceReset()
{
	m_bDeviceLost = 0;
	if (IsFullscreen())
		SetGamma(CV_r_gamma + m_fDeltaGamma, CV_r_brightness, CV_r_contrast, true);

	m_nFrameReset++;
}

//-----------------------------------------------------------------------------
// Name: CD3D9Renderer::AdjustWindowForChange()
// Desc: Prepare the window for a possible change between windowed mode and
//       fullscreen mode.  This function is virtual and thus can be overridden
//       to provide different behavior, such as switching to an entirely
//       different window for fullscreen mode (as in the MFC sample apps).
//-----------------------------------------------------------------------------
HRESULT CD3D9Renderer::AdjustWindowForChange(const int displayWidth, const int displayHeight, EWindowState previousWindowState)
{
	CSwapChainBackedRenderDisplayContext* pDC = GetBaseDisplayContext();

#if CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL
	if (IsEditorMode())
		return S_OK;
#endif

#if CRY_RENDERER_OPENGL
	const DXGI_SWAP_CHAIN_DESC& swapChainDesc(m_devInfo.SwapChainDesc());

	DXGI_MODE_DESC modeDesc;
	modeDesc.Width  = displayWidth;
	modeDesc.Height = displayHeight;
	modeDesc.RefreshRate = swapChainDesc.BufferDesc.RefreshRate;
	modeDesc.Format = swapChainDesc.BufferDesc.Format;
	modeDesc.ScanlineOrdering = swapChainDesc.BufferDesc.ScanlineOrdering;
	modeDesc.Scaling = swapChainDesc.BufferDesc.Scaling;

	HRESULT result = pDC->GetSwapChain()->ResizeTarget(&modeDesc);
	if (FAILED(result))
		return result;
#elif CRY_PLATFORM_WINDOWS
	RectI monitorBounds = pDC->GetCurrentMonitorBounds();

	if (IsFullscreen())
	{
		if (previousWindowState != EWindowState::Fullscreen)
		{
			constexpr auto fullscreenStyle = WS_POPUP | WS_VISIBLE;
			SetWindowLongPtrW(m_hWnd, GWL_STYLE, fullscreenStyle);
		}
			
		SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, displayWidth, displayHeight, SWP_SHOWWINDOW);
	}
	else if (m_windowState == EWindowState::BorderlessWindow || m_windowState == EWindowState::BorderlessFullscreen)
	{
		if (previousWindowState != EWindowState::BorderlessWindow)
		{
			// Set fullscreen-mode style
			constexpr auto fullscreenWindowStyle = WS_POPUP | WS_VISIBLE;
			SetWindowLongPtrW(m_hWnd, GWL_STYLE, fullscreenWindowStyle);
		}

		const int x = monitorBounds.x + (monitorBounds.w - displayWidth) / 2;
		const int y = monitorBounds.y + (monitorBounds.h - displayHeight) / 2;
		SetWindowPos(m_hWnd, HWND_NOTOPMOST, x, y, displayWidth, displayHeight, SWP_SHOWWINDOW);
	}
	else
	{
		const ICVar* pResizeableWindow = gEnv->pConsole->GetCVar("r_resizableWindow");
		auto windowedStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		if (pResizeableWindow && pResizeableWindow->GetIVal() == 0)
		{
			windowedStyle &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
		}
		if (previousWindowState != EWindowState::Windowed)
		{
			SetWindowLongPtrW(m_hWnd, GWL_STYLE, windowedStyle);
		}

		RECT windowRect;
		GetWindowRect(m_hWnd, &windowRect);

		const int x = windowRect.left;
		const int y = windowRect.top;

		windowRect.right = windowRect.left + displayWidth;
		windowRect.bottom = windowRect.top + displayHeight;
		AdjustWindowRectEx(&windowRect, windowedStyle, FALSE, WS_EX_APPWINDOW);

		const int width = windowRect.right - windowRect.left;
		const int height = windowRect.bottom - windowRect.top;
		SetWindowPos(m_hWnd, HWND_NOTOPMOST, x, y, width, height, SWP_SHOWWINDOW);
	}
#endif

	return S_OK;
}

#if defined(SUPPORT_DEVICE_INFO)
bool compareDXGIMODEDESC(const DXGI_MODE_DESC& lhs, const DXGI_MODE_DESC& rhs)
{
	if (lhs.Width != rhs.Width)
		return lhs.Width < rhs.Width;
	return lhs.Height < rhs.Height;
}
#endif

int CD3D9Renderer::EnumDisplayFormats(SDispFormat* formats)
{
#if CRY_PLATFORM_WINDOWS || CRY_RENDERER_OPENGL

	#if defined(SUPPORT_DEVICE_INFO)

	auto* pDC = gcpRendD3D->GetActiveDisplayContext();
	auto* swapDC = pDC->IsSwapChainBacked() ? static_cast<CSwapChainBackedRenderDisplayContext*>(pDC) : gcpRendD3D->GetBaseDisplayContext();
	const DXGI_SURFACE_DESC& swapChainDesc = swapDC->GetSwapChain().GetSurfaceDesc();

	unsigned int numModes = 0;
	if (SUCCEEDED(swapDC->m_pOutput->GetDisplayModeList(swapChainDesc.Format, 0, &numModes, 0)) && numModes)
	{
		std::vector<DXGI_MODE_DESC> dispModes(numModes);
		if (SUCCEEDED(swapDC->m_pOutput->GetDisplayModeList(swapChainDesc.Format, 0, &numModes, &dispModes[0])) && numModes)
		{

			std::sort(dispModes.begin(), dispModes.end(), compareDXGIMODEDESC);

			unsigned int numUniqueModes = 0;
			unsigned int prevWidth = 0;
			unsigned int prevHeight = 0;
			for (unsigned int i = 0; i < numModes; ++i)
			{
				if (prevWidth != dispModes[i].Width || prevHeight != dispModes[i].Height)
				{
					if (formats)
					{
						formats[numUniqueModes].m_Width = dispModes[i].Width;
						formats[numUniqueModes].m_Height = dispModes[i].Height;
						formats[numUniqueModes].m_BPP = CTexture::BitsPerPixel(DeviceFormats::ConvertToTexFormat(dispModes[i].Format));
					}

					prevWidth = dispModes[i].Width;
					prevHeight = dispModes[i].Height;
					++numUniqueModes;
				}
			}

			numModes = numUniqueModes;
		}
	}

	return numModes;

	#endif

#else
	return 0;
#endif
}

void CD3D9Renderer::UnSetRes()
{
	m_Features |= RFT_SUPPORTZBIAS;

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	m_d3dDebug.Release();
#endif
}

void CD3D9Renderer::DestroyWindow(void)
{
#if defined(USE_SDL2_VIDEO) && (CRY_RENDERER_OPENGL || CRY_RENDERER_OPENGLES)
	DXGLDestroySDLWindow(m_hWnd);
#elif defined(USE_SDL2_VIDEO) && (CRY_RENDERER_VULKAN)
	VKDestroySDLWindow(m_hWnd);
#elif CRY_PLATFORM_WINDOWS
	if (gEnv && gEnv->pSystem && !m_bShaderCacheGen)
	{
		gEnv->pSystem->UnregisterWindowMessageHandler(this);
	}
	if (m_hWnd)
	{
		::DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}
	if (m_hIconBig)
	{
		::DestroyIcon(m_hIconBig);
		m_hIconBig = NULL;
	}
	if (m_hIconSmall)
	{
		::DestroyIcon(m_hIconSmall);
		m_hIconSmall = NULL;
	}
#endif
}

static CD3D9Renderer::SGammaRamp orgGamma;

static BOOL g_doGamma = false;

void CD3D9Renderer::RestoreGamma(void)
{
	//if (!m_bFullScreen)
	//	return;

	if (!(GetFeatures() & RFT_HWGAMMA))
		return;

	if (CV_r_nohwgamma)
		return;

	m_fLastGamma = 1.0f;
	m_fLastBrightness = 0.5f;
	m_fLastContrast = 0.5f;

	//iLog->Log("...RestoreGamma");

#if CRY_PLATFORM_WINDOWS
	if (!g_doGamma)
		return;

	g_doGamma = false;

	m_hWndDesktop = GetDesktopWindow();

	if (HDC dc = GetDC(m_hWndDesktop))
	{
		SetDeviceGammaRamp(dc, &orgGamma);
		ReleaseDC(m_hWndDesktop, dc);
	}
#endif
}

void CD3D9Renderer::GetDeviceGamma()
{
#if CRY_PLATFORM_WINDOWS
	if (g_doGamma)
	{
		return;
	}

	m_hWndDesktop = GetDesktopWindow();

	if (HDC dc = GetDC(m_hWndDesktop))
	{
		g_doGamma = true;

		if (!GetDeviceGammaRamp(dc, &orgGamma))
		{
			for (uint16 i = 0; i < 256; i++)
			{
				orgGamma.red[i] = i * 0x101;
				orgGamma.green[i] = i * 0x101;
				orgGamma.blue[i] = i * 0x101;
			}
		}

		ReleaseDC(m_hWndDesktop, dc);
	}
#endif
}

void CD3D9Renderer::SetDeviceGamma(SGammaRamp* gamma)
{
	if (!(GetFeatures() & RFT_HWGAMMA))
		return;

	if (CV_r_nohwgamma)
		return;

#if CRY_PLATFORM_WINDOWS
	if (!g_doGamma)
		return;

	m_hWndDesktop = GetDesktopWindow();  // TODO: DesktopWindow - does not represent actual output window thus gamma affects all desktop monitors !!!

	if (HDC dc = GetDC(m_hWndDesktop))
	{
		g_doGamma = true;
		// INFO!!! - very strange: in the same time
		// GetDeviceGammaRamp -> TRUE
		// SetDeviceGammaRamp -> FALSE but WORKS!!!
		// at least for desktop window DC... be careful
		SetDeviceGammaRamp(dc, gamma);
		ReleaseDC(m_hWndDesktop, dc);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

void CD3D9Renderer::SetGamma(float fGamma, float fBrightness, float fContrast, bool bForce)
{
	if (m_pStereoRenderer)  // Function can be called on shutdown
		fGamma += GetS3DRend().GetGammaAdjustment();

	fGamma = CLAMP(fGamma, 0.4f, 1.6f);

	if (!bForce && m_fLastGamma == fGamma && m_fLastBrightness == fBrightness && m_fLastContrast == fContrast)
		return;

	GetDeviceGamma();

	SGammaRamp gamma;

	float fInvGamma = 1.f / fGamma;

	float fAdd = (fBrightness - 0.5f) * 0.5f - fContrast * 0.5f + 0.25f;
	float fMul = fContrast + 0.5f;

	for (int i = 0; i < 256; i++)
	{
		float pfInput[3];

		pfInput[0] = (float)(orgGamma.red[i] >> 8) / 255.f;
		pfInput[1] = (float)(orgGamma.green[i] >> 8) / 255.f;
		pfInput[2] = (float)(orgGamma.blue[i] >> 8) / 255.f;

		pfInput[0] = pow_tpl(pfInput[0], fInvGamma) * fMul + fAdd;
		pfInput[1] = pow_tpl(pfInput[1], fInvGamma) * fMul + fAdd;
		pfInput[2] = pow_tpl(pfInput[2], fInvGamma) * fMul + fAdd;

		gamma.red[i] = CLAMP(int_round(pfInput[0] * 65535.f), 0, 65535);
		gamma.green[i] = CLAMP(int_round(pfInput[1] * 65535.f), 0, 65535);
		gamma.blue[i] = CLAMP(int_round(pfInput[2] * 65535.f), 0, 65535);
	}

	SetDeviceGamma(&gamma);

	m_fLastGamma = fGamma;
	m_fLastBrightness = fBrightness;
	m_fLastContrast = fContrast;
}

bool CD3D9Renderer::SetGammaDelta(const float fGamma)
{
	m_fDeltaGamma = fGamma;
	SetGamma(CV_r_gamma + fGamma, CV_r_brightness, CV_r_contrast, false);
	return true;
}

void CD3D9Renderer::ShutDownFast()
{
	CVrProjectionManager::Reset();

	// Flush RT command buffer
	ForceFlushRTCommands();
	CHWShader::mfFlushPendedShadersWait(-1);
	EF_Exit(true);
	//CBaseResource::ShutDown();

	SAFE_DELETE(m_pRT);

#if CRY_RENDERER_OPENGL
	#if !DXGL_FULL_EMULATION && !OGL_SINGLE_CONTEXT
	if (CV_r_multithreaded)
		DXGLReleaseContext(m_devInfo.Device());
	#endif
	m_devInfo.Release();
#endif
}

void CD3D9Renderer::RT_ShutDown(uint32 nFlags)
{
	CVrProjectionManager::Reset();

	CREBreakableGlassBuffer::RT_ReleaseInstance();
	SAFE_DELETE(m_pColorGradingControllerD3D);
	SAFE_DELETE(m_pPostProcessMgr);
	SAFE_DELETE(m_pWaterSimMgr);
	SAFE_DELETE(m_pStereoRenderer);
	SAFE_DELETE(m_pPipelineProfiler);
#if defined(ENABLE_RENDER_AUX_GEOM)
	SAFE_DELETE(m_pRenderAuxGeomD3D);
#endif

	for (size_t i = 0; i < 3; ++i)
		while (m_CharCBActiveList[i].next != &m_CharCBActiveList[i])
			delete m_CharCBActiveList[i].next->item<& SCharacterInstanceCB::list>();
	while (m_CharCBFreeList.next != &m_CharCBFreeList)
		delete m_CharCBFreeList.next->item<& SCharacterInstanceCB::list>();

	CHWShader::mfFlushPendedShadersWait(-1);
	if (!IsShaderCacheGenMode())
	{
		if (nFlags == FRR_SHUTDOWN)
		{
			FreeSystemResources(nFlags);
		}
	}
	else
	{
		RT_GraphicsPipelineShutdown();
		EF_Exit();

		ForceFlushRTCommands();
	}

	GetDeviceObjectFactory().ReleaseInputLayouts();
	GetDeviceObjectFactory().ReleaseSamplerStates();
	GetDeviceObjectFactory().ReleaseNullResources();

#if defined(SUPPORT_DEVICE_INFO)
	//m_devInfo.Release();
	#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
		#if OGL_SINGLE_CONTEXT
	m_pRT->m_kDXGLDeviceContextHandle.Set(NULL);
		#else
	m_pRT->m_kDXGLDeviceContextHandle.Set(NULL, !CV_r_multithreaded);
	m_pRT->m_kDXGLContextHandle.Set(NULL);
		#endif
	#endif
#endif

	// Shut Down all contexts.
	{
		AUTO_LOCK(gs_contextLock); // Not thread safe without this
		for (auto &pDisplayContext : m_displayContexts)
			pDisplayContext.second->ReleaseResources();
	}

#if !CRY_PLATFORM_ORBIS && !CRY_RENDERER_OPENGL
	GetDeviceContext().ReleaseDeviceContext();
#endif

#if defined(ENABLE_NULL_D3D11DEVICE)
	if (m_bShaderCacheGen)
		GetDevice().ReleaseDevice();
#endif
}

void CD3D9Renderer::ShutDown(bool bReInit)
{
	m_bInShutdown = true;

	// Force Flush RT command buffer
	ForceFlushRTCommands();	
	PreShutDown();

	DeleteAuxGeomCollectors();
	DeleteAuxGeomCBs();

	if (m_pRT)
	{
		ExecuteRenderThreadCommand( [=]{ this->RT_ShutDown(bReInit ? 0 : FRR_SHUTDOWN); }, ERenderCommandFlags::FlushAndWait);
	}

	//CBaseResource::ShutDown();
	ForceFlushRTCommands();

#ifdef USE_PIX_DURANGO
	SAFE_RELEASE(m_pPixPerf);
#endif
	//////////////////////////////////////////////////////////////////////////
	// Clear globals.
	//////////////////////////////////////////////////////////////////////////


	// Release Display Contexts, freeing Swap Channels.
	m_pBaseDisplayContext = nullptr;
	UpdateActiveContext(nullptr, {});

	{
		AUTO_LOCK(gs_contextLock); // Not thread safe without this
		m_displayContexts.clear();
	}

	SAFE_DELETE(m_pRT);

#if CRY_RENDERER_OPENGL
	#if !DXGL_FULL_EMULATION && !OGL_SINGLE_CONTEXT
	if (CV_r_multithreaded)
		DXGLReleaseContext(GetDevice().GetRealDevice());
	#endif
	m_devInfo.Release();
#endif

	if (!bReInit)
	{
		iLog = NULL;
		//iConsole = NULL;
		iTimer = NULL;
		iSystem = NULL;
	}
	
	PostShutDown();
}

#if CRY_PLATFORM_WINDOWS
LRESULT CALLBACK LowLevelKeyboardProc(INT nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*) lParam;
	BOOL bControlKeyDown = 0;
	switch (nCode)
	{
	case HC_ACTION:
		{
			if (pkbhs->vkCode == VK_TAB && pkbhs->flags & LLKHF_ALTDOWN)
				return 1;            // Disable ALT+ESC
		}
	default:
		break;
	}
	return CallNextHookEx(0, nCode, wParam, lParam);
}
#endif

#if defined(SUPPORT_DEVICE_INFO)
HWND CD3D9Renderer::CreateWindowCallback()
{
	const CRenderDisplayContext* pDC = gcpRendD3D->GetBaseDisplayContext();

	const int displayWidth  = pDC->GetDisplayResolution()[0];
	const int displayHeight = pDC->GetDisplayResolution()[1];

	return gcpRendD3D->SetWindow(displayWidth, displayHeight) ? gcpRendD3D->m_hWnd : 0;
}
#endif

bool CD3D9Renderer::SetWindow(int width, int height)
{
	LOADING_TIME_PROFILE_SECTION;

	iSystem->RegisterWindowMessageHandler(this);

#if USE_SDL2_VIDEO && (CRY_RENDERER_OPENGL || CRY_RENDERER_OPENGLES)
	DXGLCreateSDLWindow(m_WinTitle, width, height, IsFullscreen(), &m_hWnd);
#elif USE_SDL2_VIDEO && (CRY_RENDERER_VULKAN)
	VKCreateSDLWindow(m_WinTitle, width, height, IsFullscreen(), &m_hWnd);
#elif CRY_PLATFORM_WINDOWS
	DWORD style, exstyle;

	if (width < 640)
		width = 640;
	if (height < 480)
		height = 480;

	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
	int x = monitorInfo.rcMonitor.left;
	int y = monitorInfo.rcMonitor.top;
	const int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
	const int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

	bool isFullscreenWindow = m_windowState == EWindowState::BorderlessWindow;

	if (IsFullscreen() || isFullscreenWindow)
	{
		exstyle = isFullscreenWindow ? WS_EX_APPWINDOW : WS_EX_TOPMOST;
		style = WS_POPUP | WS_VISIBLE;
		x += (monitorWidth - width) / 2;
		y += (monitorHeight - height) / 2;
	}
	else
	{
		exstyle = WS_EX_APPWINDOW;
		style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
		const ICVar* pResizeableWindow = gEnv->pConsole->GetCVar("r_resizableWindow");
		if (pResizeableWindow && pResizeableWindow->GetIVal() == 0)
		{
			style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
		}

		RECT wndrect;
		SetRect(&wndrect, 0, 0, width, height);
		AdjustWindowRectEx(&wndrect, style, FALSE, exstyle);

		width = wndrect.right - wndrect.left;
		height = wndrect.bottom - wndrect.top;

		x += (monitorWidth - width) / 2;
		y += (monitorHeight - height) / 2;
	}

	if (IsEditorMode())
	{
	#if defined(UNICODE) || defined(_UNICODE)
		#error Review this, probably should be wide if Editor also has UNICODE support (or maybe moved into Editor)
	#endif
		style = WS_OVERLAPPED;
		exstyle = 0;
		x = y = 0;
		width = 100;
		height = 100;

		WNDCLASSA wc;
		memset(&wc, 0, sizeof(WNDCLASSA));
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = DefWindowProcA;
		wc.hInstance = CryGetCurrentModule();
		wc.lpszClassName = "D3DDeviceWindowClassForSandbox";
		if (!RegisterClassA(&wc))
		{
			CryFatalError("Cannot Register Window Class %s", wc.lpszClassName);
			return false;
		}
		m_hWnd = CreateWindowExA(exstyle, wc.lpszClassName, m_WinTitle, style, x, y, width, height, NULL, NULL, wc.hInstance, NULL);
		ShowWindow(m_hWnd, SW_HIDE);
	}
	else
	{
		if (!m_hWnd)
		{
			LPCWSTR pClassName = L"CryENGINE";

			// Load default icon if we have nothing yet
			if (m_hIconBig == NULL)
			{
				SetWindowIcon(gEnv->pConsole->GetCVar("r_WindowIconTexture")->GetString());
			}

			// Moved from Game DLL
			WNDCLASSEXW wc;
			memset(&wc, 0, sizeof(WNDCLASSEXW));
			wc.cbSize = sizeof(WNDCLASSEXW);
			wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
			wc.lpfnWndProc = (WNDPROC)GetISystem()->GetRootWindowMessageHandler();
			wc.hInstance = CryGetCurrentModule();
			wc.hIcon = m_hIconBig;
			wc.hIconSm = m_hIconSmall;
			wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
			wc.lpszClassName = pClassName;
			if (!RegisterClassExW(&wc))
			{
				CryFatalError("Cannot Register Launcher Window Class");
				return false;
			}

			wstring wideTitle = Unicode::Convert<wstring>(m_WinTitle);

			m_hWnd = CreateWindowExW(exstyle, pClassName, wideTitle.c_str(), style, x, y, width, height, NULL, NULL, wc.hInstance, NULL);
			if (m_hWnd && !IsWindowUnicode(m_hWnd))
			{
				CryFatalError("Expected an UNICODE window for launcher");
				return false;
			}

			EnableCloseButton(m_hWnd, false);

			if (IsFullscreen() && (!gEnv->pSystem->IsDevMode() && CV_r_enableAltTab == 0))
				SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
		}

		if (m_hWnd)
		{
			ShowWindow(m_hWnd, SW_SHOWNORMAL);
			const bool wasFocusSet = SetFocus(m_hWnd) != nullptr;
			CRY_ASSERT(wasFocusSet);
			// Attempt to move the window to the foreground and activate it
			// Note that this will fail if the user alt-tabbed away to another application while the engine was starting
			// See https://msdn.microsoft.com/en-us/library/windows/desktop/ms633539(v=vs.85).aspx
			SetForegroundWindow(m_hWnd);
		}
	}
#endif

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ANDROID || CRY_PLATFORM_LINUX
	m_VSync = !IsEditorMode() ? CV_r_vsync : 0;

	// Update base context hWnd and key
	SDisplayContextKey baseContextKey;
	baseContextKey.key.emplace<HWND>(m_pBaseDisplayContext->GetWindowHandle());
	m_pBaseDisplayContext->CreateSwapChain(m_hWnd, m_VSync != 0);
	{
		AUTO_LOCK(gs_contextLock);
		m_displayContexts.erase(baseContextKey);

		baseContextKey.key.emplace<HWND>(m_hWnd);
		m_displayContexts.emplace(std::make_pair(std::move(baseContextKey), m_pBaseDisplayContext));
	}

	if (!m_hWnd)
		iConsole->Exit("Couldn't create window\n");
#else
	return false;
#endif
	return true;
}

bool CD3D9Renderer::SetWindowIcon(const char* path)
{
#if CRY_PLATFORM_WINDOWS
	if (IsEditorMode())
	{
		return false;
	}

	if (stricmp(path, m_iconPath.c_str()) == 0)
	{
		return true;
	}

	HICON hIconBig = CreateResourceFromTexture(this, path, eResourceType_IconBig);
	if (hIconBig)
	{
		if (m_hWnd)
		{
			SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
		}
		if (m_hIconBig)
		{
			::DestroyIcon(m_hIconBig);
		}
		m_hIconBig = hIconBig;
		m_iconPath = path;
	}

	// Note: Also set the small icon manually.
	// Even though the big icon will also affect the small icon, the re-scaling done by GDI has aliasing problems.
	// Just grabbing a smaller MIP from the texture (if possible) will solve this.
	HICON hIconSmall = CreateResourceFromTexture(this, path, eResourceType_IconSmall);
	if (hIconSmall)
	{
		if (m_hWnd)
		{
			SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
		}
		if (m_hWnd)
		{
			SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
		}
		if (m_hIconSmall)
		{
			::DestroyIcon(m_hIconSmall);
		}
		m_hIconSmall = hIconSmall;
	}

	// Update the CVar value to match, in case this API was not called through CVar-change-callback
	gEnv->pConsole->GetCVar("r_WindowIconTexture")->Set(m_iconPath.c_str());

	return hIconBig != NULL;
#else
	return false;
#endif
}

#define QUALITY_VAR(name)                                                 \
  static void OnQShaderChange_Shader ## name(ICVar * pVar)                \
  {                                                                       \
    int iQuality = eSQ_Low;                                               \
    if (gRenDev->GetFeatures() & (RFT_HW_SM2X | RFT_HW_SM30))             \
      iQuality = CLAMP(pVar->GetIVal(), 0, eSQ_Max);                      \
    gRenDev->EF_SetShaderQuality(eST_ ## name, (EShaderQuality)iQuality); \
  }

QUALITY_VAR(General)
QUALITY_VAR(Metal)
QUALITY_VAR(Glass)
QUALITY_VAR(Vegetation)
QUALITY_VAR(Ice)
QUALITY_VAR(Terrain)
QUALITY_VAR(Shadow)
QUALITY_VAR(Water)
QUALITY_VAR(FX)
QUALITY_VAR(PostProcess)
QUALITY_VAR(HDR)
QUALITY_VAR(Sky)

#undef QUALITY_VAR

static void OnQShaderChange_Renderer(ICVar* pVar)
{
	int iQuality = eRQ_Low;

	if (gRenDev->GetFeatures() & (RFT_HW_SM2X | RFT_HW_SM30))
		iQuality = CLAMP(pVar->GetIVal(), 0, eSQ_Max);
	else
		pVar->ForceSet("0");

	auto rq = gRenDev->GetRenderQuality();
	rq.renderQuality = (ERenderQuality)iQuality;
	gRenDev->SetRenderQuality(rq);
}

static void Command_Quality(IConsoleCmdArgs* Cmd)
{
	bool bLog = false;
	bool bSet = false;

	int iQuality = -1;

	if (Cmd->GetArgCount() == 2)
	{
		iQuality = CLAMP(atoi(Cmd->GetArg(1)), 0, eSQ_Max);
		bSet = true;
	}
	else bLog = true;

	if (bLog) iLog->LogWithType(IMiniLog::eInputResponse, " ");
	if (bLog) iLog->LogWithType(IMiniLog::eInputResponse, "Current quality settings (0=low/1=med/2=high/3=very high):");

#define QUALITY_VAR(name) if (bLog) iLog->LogWithType(IMiniLog::eInputResponse, "  $3q_" # name " = $6%d", gEnv->pConsole->GetCVar("q_" # name)->GetIVal()); \
  if (bSet) gEnv->pConsole->GetCVar("q_" # name)->Set(iQuality);

	QUALITY_VAR(ShaderGeneral)
	QUALITY_VAR(ShaderMetal)
	QUALITY_VAR(ShaderGlass)
	QUALITY_VAR(ShaderVegetation)
	QUALITY_VAR(ShaderIce)
	QUALITY_VAR(ShaderTerrain)
	QUALITY_VAR(ShaderShadow)
	QUALITY_VAR(ShaderWater)
	QUALITY_VAR(ShaderFX)
	QUALITY_VAR(ShaderPostProcess)
	QUALITY_VAR(ShaderHDR)
	QUALITY_VAR(ShaderSky)
	QUALITY_VAR(Renderer)

#undef QUALITY_VAR

	if (bSet) iLog->LogWithType(IMiniLog::eInputResponse, "Set quality to %d", iQuality);
}

const char* sGetSQuality(const char* szName)
{
	ICVar* pVar = iConsole->GetCVar(szName);
	assert(pVar);
	if (!pVar)
		return "Unknown";
	int iQ = pVar->GetIVal();
	switch (iQ)
	{
	case eSQ_Low:
		return "Low";
	case eSQ_Medium:
		return "Medium";
	case eSQ_High:
		return "High";
	case eSQ_VeryHigh:
		return "VeryHigh";
	default:
		return "Unknown";
	}
}

static void Command_ColorGradingChartImage(IConsoleCmdArgs* pCmd)
{
	CColorGradingController* pCtrl = gcpRendD3D->m_pColorGradingControllerD3D;
	if (pCmd && pCtrl)
	{
		const int numArgs = pCmd->GetArgCount();
		if (numArgs == 1)
		{
			const CTexture* pChart = pCtrl->GetStaticColorChart();
			if (pChart)
				iLog->Log("current static chart is \"%s\"", pChart->GetName());
			else
				iLog->Log("no static chart loaded");
		}
		else if (numArgs == 2)
		{
			const char* pArg = pCmd->GetArg(1);
			if (pArg && pArg[0])
			{
				if (pArg[0] == '0' && !pArg[1])
				{
					pCtrl->LoadStaticColorChart(0);
					iLog->Log("static chart reset");
				}
				else
				{
					if (pCtrl->LoadStaticColorChart(pArg))
						iLog->Log("\"%s\" loaded successfully", pArg);
					else
						iLog->Log("failed to load \"%s\"", pArg);
				}
			}
		}
	}
}

WIN_HWND CD3D9Renderer::Init(int x, int y, int width, int height, unsigned int colorBits, int depthBits, int stencilBits, WIN_HWND Glhwnd, bool bReInit, bool bShaderCacheGen)
{
	LOADING_TIME_PROFILE_SECTION;

	// Create and set the current aux collector to capture any aux commands
	SetCurrentAuxGeomCollector(GetOrCreateAuxGeomCollector(gEnv->pSystem->GetViewCamera()));

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Renderer initialisation");

	if (!iSystem || !iLog)
		return 0;

	iLog->Log("Initializing Direct3D and creating game window:");
	INDENT_LOG_DURING_SCOPE();

	m_CVWidth = iConsole->GetCVar("r_Width");
	m_CVHeight = iConsole->GetCVar("r_Height");
	m_CVWindowType = iConsole->GetCVar("r_WindowType");
	m_CVDisplayInfo = iConsole->GetCVar("r_DisplayInfo");
	m_CVColorBits = iConsole->GetCVar("r_ColorBits");

#if CRY_PLATFORM_WINDOWS
	CV_r_FullscreenNativeRes = iConsole->GetCVar("r_FullscreenNativeRes");
#endif

	m_windowState = CalculateWindowState();

	bool bNativeResolution;
#if CRY_PLATFORM_CONSOLE || CRY_PLATFORM_MOBILE
	bNativeResolution = true;
#elif CRY_PLATFORM_WINDOWS
	bNativeResolution = CV_r_FullscreenNativeRes && CV_r_FullscreenNativeRes->GetIVal() != 0 && (m_windowState == EWindowState::Fullscreen || m_windowState == EWindowState::BorderlessWindow);

	REGISTER_STRING_CB("r_WindowIconTexture", "%ENGINE%/EngineAssets/Textures/default_icon.dds", VF_CHEAT | VF_CHEAT_NOCHECK,
	                   "Sets the image (dds file) to be displayed as the window and taskbar icon",
	                   SetWindowIconCVar);
#else
	bNativeResolution = false;
#endif

#if CRY_PLATFORM_DESKTOP
	REGISTER_STRING_CB("r_MouseCursorTexture", "%ENGINE%/EngineAssets/Textures/Cursor_Green.dds", VF_NULL,
	                   "Sets the image (dds file) to be displayed as the mouse cursor",
	                   SetMouseCursorIconCVar);
#endif // CRY_PLATFORM_DESKTOP

	REGISTER_INT("r_resizableWindow", 1, VF_NULL, "Turn on resizable window borders. Changes are only applied after changing the window style once.");

#if (CRY_RENDERER_OPENGL || CRY_RENDERER_OPENGLES) && !DXGL_FULL_EMULATION
	#if OGL_SINGLE_CONTEXT
	DXGLInitialize(0);
	#else
	DXGLInitialize(CV_r_multithreaded ? 4 : 0);
	#endif
#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION

	iLog->Log("Direct3D driver is creating...");
	iLog->Log("Crytek Direct3D driver version %4.2f (%s <%s>)", VERSION_D3D, __DATE__, __TIME__);

	const char* sGameName = gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName();
	cry_strcpy(m_WinTitle, sGameName);

	iLog->Log("Creating window called '%s' (%dx%d)", m_WinTitle, width, height);

	if (Glhwnd == (WIN_HWND)1)
	{
		Glhwnd = 0;
		m_bEditor = true;

		if (m_windowState == EWindowState::Fullscreen)
			m_windowState = EWindowState::Windowed;
	}

	m_bShaderCacheGen = bShaderCacheGen;

	// RenderPipeline parameters
	int renderWidth, renderHeight;
	int outputWidth, outputHeight;
	int displayWidth, displayHeight;

	m_cbpp = colorBits;
	m_zbpp = depthBits;
	m_sbpp = stencilBits;

	CRenderDisplayContext* pDC = GetBaseDisplayContext();
	CalculateResolutions(width, height, bNativeResolution, IsFullscreen(), &renderWidth, &renderHeight, &outputWidth, &outputHeight, &displayWidth, &displayHeight);

	pDC->m_DisplayWidth =  displayWidth;
	pDC->m_DisplayHeight = displayHeight;

	// only create device if we are not in shader cache generation mode
	if (!m_bShaderCacheGen)
	{
		// call init stereo before device is created!
		m_pStereoRenderer->InitDeviceBeforeD3D();

		while (true)
		{
			m_hWnd = (HWND)Glhwnd;

			// Creates Device here.
			bool bRes = m_pRT->RC_CreateDevice();
			if (!bRes)
			{
				CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "Rendering device creation failed!");
				ShutDown(true);
				return 0;
			}

			break;
		}

#if defined(SUPPORT_DEVICE_INFO)
		iLog->Log(" ****** CryRender InfoDump ******");
		iLog->Log(" Driver description: %S", m_devInfo.AdapterDesc().Description);
#endif

#if CRY_RENDERER_DIRECT3D
		iLog->Log(" API Version: Direct3D %d.%1d", (CRY_RENDERER_DIRECT3D / 10), (CRY_RENDERER_DIRECT3D % 10));
#endif

#if defined(SUPPORT_DEVICE_INFO)
		switch (m_devInfo.FeatureLevel())
		{
		case D3D_FEATURE_LEVEL_9_1:
			iLog->Log(" Feature level: D3D 9_1");
			iLog->Log(" Vertex Shaders version %d.%d", 2, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 2, 0);
			break;
		case D3D_FEATURE_LEVEL_9_2:
			iLog->Log(" Feature level: D3D 9_2");
			iLog->Log(" Vertex Shaders version %d.%d", 2, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 2, 0);
			break;
		case D3D_FEATURE_LEVEL_9_3:
			iLog->Log(" Feature level: D3D 9_3");
			iLog->Log(" Vertex Shaders version %d.%d", 3, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 3, 0);
			break;
		case D3D_FEATURE_LEVEL_10_0:
			iLog->Log(" Feature level: D3D 10_0");
			iLog->Log(" Vertex Shaders version %d.%d", 4, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 4, 0);
			break;
		case D3D_FEATURE_LEVEL_10_1:
			iLog->Log(" Feature level: D3D 10_1");
			iLog->Log(" Vertex Shaders version %d.%d", 4, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 4, 0);
			break;
		case D3D_FEATURE_LEVEL_11_0:
			iLog->Log(" Feature level: D3D 11_0");
			iLog->Log(" Vertex Shaders version %d.%d", 5, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 5, 0);
			break;
		case D3D_FEATURE_LEVEL_11_1:
			iLog->Log(" Feature level: D3D 11_1");
			iLog->Log(" Vertex Shaders version %d.%d", 5, 0);
			iLog->Log(" Pixel Shaders version %d.%d", 5, 0);
			break;
		case D3D_FEATURE_LEVEL_12_0:
			iLog->Log(" Feature level: D3D 12_0");
			iLog->Log(" Vertex Shaders version %d.%d", 5, 1);
			iLog->Log(" Pixel Shaders version %d.%d", 5, 1);
			break;
		case D3D_FEATURE_LEVEL_12_1:
			iLog->Log(" Feature level: D3D 12_1");
			iLog->Log(" Vertex Shaders version %d.%d", 5, 1);
			iLog->Log(" Pixel Shaders version %d.%d", 5, 1);
			break;
		default:
			iLog->Log(" Feature level: Unknown");
			break;
		}

		iLog->Log(" Full stats: %s", m_strDeviceStats);
		if (m_devInfo.DriverType() == D3D_DRIVER_TYPE_HARDWARE)
			iLog->Log(" Rasterizer: Hardware");
		else if (m_devInfo.DriverType() == D3D_DRIVER_TYPE_REFERENCE)
			iLog->Log(" Rasterizer: Reference");
		else if (m_devInfo.DriverType() == D3D_DRIVER_TYPE_SOFTWARE)
			iLog->Log(" Rasterizer: Software");
#elif CRY_PLATFORM_DURANGO
		iLog->Log(" Feature level: D3D 11_1");
		iLog->Log(" Vertex Shaders version %d.%d", 5, 0);
		iLog->Log(" Pixel Shaders version %d.%d", 5, 0);
#endif

		iLog->Log(" Current Resolution: %dx%dx%d %s", CRendererResources::s_renderWidth, CRendererResources::s_renderHeight, CRenderer::m_cbpp, GetWindowStateName());
		iLog->Log(" Occlusion queries: %s", (m_Features & RFT_OCCLUSIONTEST) ? "Supported" : "Not supported");
		iLog->Log(" Geometry instancing: %s", (m_bDeviceSupportsInstancing) ? "Supported" : "Not supported");
		iLog->Log(" NormalMaps compression : %s", CRendererResources::s_hwTexFormatSupport.m_FormatBC5U.IsValid() ? "Supported" : "Not supported");
		iLog->Log(" Gamma control: %s", (m_Features & RFT_HWGAMMA) ? "Hardware" : "Software");

		CRenderer::OnChange_GeomInstancingThreshold(0);   // to get log printout and to set the internal value (vendor dependent)

		m_Features |= RFT_HW_SM20 | RFT_HW_SM2X | RFT_HW_SM30;

		if (!m_bDeviceSupportsInstancing)
			_SetVar("r_GeomInstancing", 0);

		const char* str = NULL;
		if (m_Features & RFT_HW_SM50)
			str = "SM.5.0";
		else if (m_Features & RFT_HW_SM40)
			str = "SM.4.0";
		else
			assert(0);
		iLog->Log(" Shader model usage: '%s'", str);

		// Needs to happen post D3D device creation
		CVrProjectionManager::Instance()->Init(this);

		CRendererResources::OnDisplayResolutionChanged(displayWidth, displayHeight);
		CRendererResources::OnOutputResolutionChanged(outputWidth, outputHeight);
		CRendererResources::OnRenderResolutionChanged(renderWidth, renderHeight);
	}
	else
	{

		// force certain features during shader cache gen mode
		m_Features |= RFT_HW_SM20 | RFT_HW_SM2X | RFT_HW_SM30;

#if defined(ENABLE_NULL_D3D11DEVICE)
		m_DeviceWrapper.AssignDevice(new NullD3D11Device);
		D3DDeviceContext* pContext = NULL;
	#if (CRY_RENDERER_DIRECT3D >= 113)
		GetDevice().GetImmediateContext3(&pContext);
	#elif (CRY_RENDERER_DIRECT3D >= 112)
		GetDevice().GetImmediateContext2(&pContext);
	#elif (CRY_RENDERER_DIRECT3D >= 111)
		GetDevice().GetImmediateContext1(&pContext);
	#else
		GetDevice().GetImmediateContext(&pContext);
	#endif
		m_DeviceContextWrapper.AssignDeviceContext(pContext);
#endif
	}

	iLog->Log(" *****************************************");
	iLog->Log(" ");

	iLog->Log("Init Shaders");

	//  if (!(GetFeatures() & (RFT_HW_PS2X | RFT_HW_PS30)))
	//    SetShaderQuality(eST_All, eSQ_Low);

	// Quality console variables --------------------------------------

#define QUALITY_VAR(name) { ICVar* pVar = ConsoleRegistrationHelper::Register("q_Shader" # name, &m_cEF.m_ShaderProfiles[(int)eST_ ## name].m_iShaderProfileQuality, 1, \
  0, CVARHELP("Defines the shader quality of " # name "\nUsage: q_Shader" # name " 0=low/1=med/2=high/3=very high (default)"), OnQShaderChange_Shader ## name);         \
OnQShaderChange_Shader## name(pVar);                                                                                                                                                                                   \
iLog->Log(" %s shader quality: %s", # name, sGetSQuality("q_Shader" # name)); } // clamp for lowspec

	QUALITY_VAR(General);
	QUALITY_VAR(Metal);
	QUALITY_VAR(Glass);
	QUALITY_VAR(Vegetation);
	QUALITY_VAR(Ice);
	QUALITY_VAR(Terrain);
	QUALITY_VAR(Shadow);
	QUALITY_VAR(Water);
	QUALITY_VAR(FX);
	QUALITY_VAR(PostProcess);
	QUALITY_VAR(HDR);
	QUALITY_VAR(Sky);

#undef QUALITY_VAR

	ICVar* pVar = REGISTER_INT_CB("q_Renderer", 3, 0, "Defines the quality of Renderer\nUsage: q_Renderer 0=low/1=med/2=high/3=very high (default)", OnQShaderChange_Renderer);
	OnQShaderChange_Renderer(pVar);   // clamp for lowspec, report renderer current value
	iLog->Log("Render quality: %s", sGetSQuality("q_Renderer"));

	REGISTER_COMMAND("q_Quality", &Command_Quality, 0,
	                 "If called with a parameter it sets the quality of all q_.. variables\n"
	                 "otherwise it prints their current state\n"
	                 "Usage: q_Quality [0=low/1=med/2=high/3=very high]");

	REGISTER_COMMAND("r_ColorGradingChartImage", &Command_ColorGradingChartImage, 0,
	                 "If called with a parameter it loads a color chart image. This image will overwrite\n"
	                 " the dynamic color chart blending result and be used during post processing instead.\n"
	                 "If called with no parameter it displays the name of the previously loaded chart.\n"
	                 "To reset a previously loaded chart call r_ColorGradingChartImage 0.\n"
	                 "Usage: r_ColorGradingChartImage [path of color chart image/reset]");

#if defined(DURANGO_VSGD_CAP)
	REGISTER_COMMAND("GPUCapture", &GPUCapture, VF_NULL,
	                 "Usage: GPUCapture name"
	                 "Takes a PIX GPU capture with the specified name\n");
#endif

#if CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION
	#if OGL_SINGLE_CONTEXT
	if (!m_pRT->IsRenderThread())
		DXGLUnbindDeviceContext(GetDeviceContext().GetRealDeviceContext());
	#else
	if (!m_pRT->IsRenderThread())
		DXGLUnbindDeviceContext(GetDeviceContext().GetRealDeviceContext(), !CV_r_multithreaded);
	#endif
#endif //CRY_RENDERER_OPENGL && !DXGL_FULL_EMULATION

	if (!bShaderCacheGen)
	{
		ExecuteRenderThreadCommand([=] { this->RT_Init(); }, ERenderCommandFlags::FlushAndWait);
	}

	if (!g_shaderGeneralHeap)
		g_shaderGeneralHeap = CryGetIMemoryManager()->CreateGeneralExpandingMemoryHeap(4 * 1024 * 1024, 0, "Shader General");

	m_cEF.mfInit();

#if CRY_PLATFORM_WINDOWS
	// Initialize the set of connected monitors
	HandleMessage(0, WM_DEVICECHANGE, 0, 0, 0);
	m_bDisplayChanged = false;
	m_bWindowRestored = false;
#endif

#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	if (m_pPipelineProfiler && !IsShaderCacheGenMode())
	{
		m_pPipelineProfiler->Init();
	}
#endif
	m_bInitialized = true;

	//  Cry_memcheck();

	// Success, return the window handle
	return (m_hWnd);
}

//==========================================================================
void CD3D9Renderer::InitAMDAPI()
{
#if USE_AMD_API
	do
	{
		AGSReturnCode status = g_pAGSContext ? AGS_SUCCESS : agsInit(&g_pAGSContext, &g_pAGSContextConfiguration, &g_gpuInfo);
		iLog->Log("AGS: AMD GPU Services API init %s (%d)", status == AGS_SUCCESS ? "ok" : "failed", status);
		m_bVendorLibInitialized = status == AGS_SUCCESS;
		if (!m_bVendorLibInitialized)
			break;
		
		iLog->Log("AGS: Catalyst Version: %s  Driver Version: %s", g_gpuInfo.radeonSoftwareVersion, g_gpuInfo.driverVersion);

		m_nGPUs = 1;
		int numGPUs = 1;
		status = agsGetCrossfireGPUCount(g_pAGSContext, &numGPUs);

		if (status != AGS_SUCCESS)
		{
			iLog->LogError("AGS: Unable to get crossfire info (%d)", status);
		}
		else
		{
			m_nGPUs = numGPUs;
			iLog->Log("AGS: Multi GPU count = %d", numGPUs);
		}
		
		m_bDeviceSupports_AMDExt = g_extensionsSupported;
	}
	while (0);
#endif   // USE_AMD_API

#if defined(USE_AMD_EXT)
	do
	{
		PFNAmdDxExtCreate11 AmdDxExtCreate = NULL;
		HMODULE hDLL;
		HRESULT hr = S_OK;
		D3DDevice* device = NULL;
		device = GetDevice().GetRealDevice();

	#if defined _WIN64
		hDLL = GetModuleHandle("atidxx64.dll");
	#else
		hDLL = GetModuleHandle("atidxx32.dll");
	#endif

		g_pDepthBoundsTest = NULL;

		// Find the DLL entry point
		if (hDLL)
		{
			AmdDxExtCreate = reinterpret_cast<PFNAmdDxExtCreate11>(GetProcAddress(hDLL, "AmdDxExtCreate11"));
		}
		if (AmdDxExtCreate == NULL)
		{
			g_bDepthBoundsTest = false;
			break;
		}

		// Create the extension object
		hr = AmdDxExtCreate(device, &g_pExtension);

		// Get the Extension Interfaces
		if (SUCCEEDED(hr))
		{
			g_pDepthBoundsTest = static_cast<IAmdDxExtDepthBounds*>(g_pExtension->GetExtInterface(AmdDxExtDepthBoundsID));
		}

		g_bDepthBoundsTest = g_pDepthBoundsTest != NULL;
	}
	while (0);

#endif // USE_AMD_EXT
}

void CD3D9Renderer::InitNVAPI()
{
#if defined(USE_NV_API)
	NvAPI_Status stat = NvAPI_Initialize();
	iLog->Log("NVAPI: API init %s (%d)", stat ? "failed" : "ok", stat);
	m_bVendorLibInitialized = stat == 0;

	if (!m_bVendorLibInitialized)
		return;

	NvU32 version;
	NvAPI_ShortString branch;
	NvAPI_Status status = NvAPI_SYS_GetDriverAndBranchVersion(&version, branch);
	if (status != NVAPI_OK)
		iLog->LogError("NVAPI: Unable to get driver version (%d)", status);

	// enumerate displays
	for (int i = 0; i < NVAPI_MAX_DISPLAYS; ++i)
	{
		NvDisplayHandle displayHandle;
		status = NvAPI_EnumNvidiaDisplayHandle(i, &displayHandle);
		if (status != NVAPI_OK)
			break;
	}

	m_nGPUs = 1;

	// check SLI state to get number of GPUs available for rendering
	NV_GET_CURRENT_SLI_STATE sliState;
	sliState.version = NV_GET_CURRENT_SLI_STATE_VER;

	#if (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120)
	// TODO: add DX12/Vulkan support
	D3DDevice* device = NULL;
	device = GetDevice().GetRealDevice();
	status = NvAPI_D3D_GetCurrentSLIState(device, &sliState);

	if (status != NVAPI_OK)
	{
		iLog->LogError("NVAPI: Unable to get SLI state (%d)", status);
	}
	else
	{
		m_nGPUs = sliState.numAFRGroups;
		if (m_nGPUs < 2)
		{
			iLog->Log("NVAPI: Single GPU system");
		}
		else
		{
			m_nGPUs = min((int)m_nGPUs, 31);
			iLog->Log("NVAPI: System configured as SLI: %d GPU(s) for rendering", m_nGPUs);
		}
	}
	#endif

	m_bDeviceSupports_NVDBT = 1;
	iLog->Log("NVDBT supported");
#endif // USE_NV_API
}
#if CRY_PLATFORM_DURANGO
bool CD3D9Renderer::CreateDeviceDurango()
{
	auto* pDC = GetBaseDisplayContext();

	HRESULT hr = S_OK;

	ID3D11Device* pD3D11Device = nullptr;
	ID3D11DeviceContext* pD3D11Context = nullptr;
#if (CRY_RENDERER_DIRECT3D >= 120)
	CCryDX12Device* pDX12Device = nullptr;
	ID3D12Device* pD3D12Device = nullptr;
#endif

	typedef HRESULT(WINAPI * FP_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

	FP_D3D11CreateDevice pD3D11CD =
#if (CRY_RENDERER_DIRECT3D >= 120)
		(FP_D3D11CreateDevice)DX12CreateDevice;
#else
		(FP_D3D11CreateDevice)D3D11CreateDevice;
#endif

	if (pD3D11CD)
	{
		// On Durango we use an internal resolution of 720p but create a 1080p swap chain and do custom upscaling.
		// This is required since the swap chain scaling does just point filtering and since the engine requires the
		// backbuffer to be RGBA and not BGRA as required by the swap chain on Durango.

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
		const unsigned int debugRTFlag = gcpRendD3D->CV_r_EnableDebugLayer ? D3D11_CREATE_DEVICE_VALIDATED | D3D11_CREATE_DEVICE_DEBUG : 0;
#else
		const unsigned int debugRTFlag = 0;
#endif

		D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
		uint32 creationFlags = debugRTFlag
			| D3D11_CREATE_DEVICE_BGRA_SUPPORT
#ifdef USE_PIX_DURANGO
			| D3D11_CREATE_DEVICE_INSTRUMENTED
#endif
			| D3D11_CREATE_DEVICE_FAST_KICKOFFS; // AprilXDK QFE 4

		D3D_FEATURE_LEVEL arrFeatureLevels[] = { CRY_RENDERER_DIRECT3D_FL };
		D3D_FEATURE_LEVEL* pFeatureLevels(arrFeatureLevels);
		unsigned int uNumFeatureLevels(sizeof(arrFeatureLevels) / sizeof(D3D_FEATURE_LEVEL));

		D3D_FEATURE_LEVEL featureLevel;

		HRESULT hr = pD3D11CD(nullptr, driverType, 0, creationFlags, pFeatureLevels, uNumFeatureLevels, D3D11_SDK_VERSION, &pD3D11Device, &featureLevel, &pD3D11Context);
		if (SUCCEEDED(hr) && pD3D11Device && pD3D11Context)
		{
#if (CRY_RENDERER_DIRECT3D >= 120)
			pD3D12Device = (pDX12Device = reinterpret_cast<CCryDX12Device*>(pD3D11Device))->GetD3D12Device();
#endif
		}
	}

	IDXGIDevice1* pDXGIDevice = nullptr;
	IDXGIAdapterToCall* pDXGIAdapter = nullptr;
	IDXGIFactory2ToCall* pDXGIFactory = nullptr;

#if (CRY_RENDERER_DIRECT3D >= 120)
	hr |= pD3D12Device->QueryInterface(IID_GFX_ARGS(&pDXGIDevice));
	hr |= pDXGIDevice->GetAdapter(&pDXGIAdapter);
	hr |= pDXGIAdapter->GetParent(IID_GFX_ARGS(&pDXGIFactory));

	if (pDXGIFactory != nullptr)
	{
		pDC->CreateSwapChain(pDXGIFactory, pD3D12Device, pDX12Device, pDC->GetDisplayResolution().x, pDC->GetDisplayResolution().y);
	}
#else
	hr |= pD3D11Device->QueryInterface(IID_GFX_ARGS(&pDXGIDevice));
	hr |= pDXGIDevice->GetAdapter(&pDXGIAdapter);
	hr |= pDXGIAdapter->GetParent(IID_GFX_ARGS(&pDXGIFactory));

	if (pDXGIFactory != nullptr)
	{
		pDC->CreateSwapChain(pDXGIFactory, pD3D11Device, pDC->GetDisplayResolution().x, pDC->GetDisplayResolution().y);
	}
#endif

	if (!pDXGIFactory || !pDXGIAdapter || !pD3D11Device || !pD3D11Context)
	{
		CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "XBOX::CreateDevice() failed");
		return false;
	}

	{
#if (CRY_RENDERER_DIRECT3D >= 111)
		// TODOL check documentation if it is ok to ignore the context from create device? (both pointers are the same), some refcounting to look out for?\	
		ID3D11Device1* pDevice1 = static_cast<ID3D11Device1*>(pD3D11Device);
		m_DeviceWrapper.AssignDevice(pDevice1);

		D3DDeviceContext* pDeviceContext1 = nullptr;
		GetDevice().GetImmediateContext1(&pDeviceContext1);
		m_DeviceContextWrapper.AssignDeviceContext(pDeviceContext1);
#else
		m_DeviceWrapper.AssignDevice(pD3D11Device);
		m_DeviceContextWrapper.AssignDeviceContext(pDeviceContext);
#endif
	}

#if defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE)
	{
		// create performance context	and context
		ID3DXboxPerformanceDevice* pPerformanceDevice = nullptr;
		ID3DXboxPerformanceContext* pPerformanceDeviceContext = nullptr;

		hr = GetDevice().QueryInterface(__uuidof(ID3DXboxPerformanceDevice), (void**)&pPerformanceDevice);
		assert(hr == S_OK);

		hr = GetDeviceContext().QueryInterface(__uuidof(ID3DXboxPerformanceContext), (void**)&pPerformanceDeviceContext);
		assert(hr == S_OK);

		m_PerformanceDeviceWrapper.AssignPerformanceDevice(m_pPerformanceDevice = pPerformanceDevice);
		m_PerformanceDeviceContextWrapper.AssignPerformanceDeviceContext(m_pPerformanceDeviceContext = pPerformanceDeviceContext);
	}

	hr = GetPerformanceDevice().SetDriverHint(XBOX_DRIVER_HINT_CONSTANT_BUFFER_OFFSETS_IN_CONSTANTS, 1);
	assert(hr == S_OK);
#endif

#ifdef USE_PIX_DURANGO
	HRESULT res = GetDeviceContext().QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&m_pPixPerf));
	assert(SUCCEEDED(res));
#endif

	m_DevMan.Init();

	// Post device creation callbacks
	OnD3D11CreateDevice(GetDevice().GetRealDevice());
	OnD3D11PostCreateDevice(GetDevice().GetRealDevice());

	return true;
}
#elif CRY_PLATFORM_IOS || CRY_PLATFORM_ANDROID
bool CD3D9Renderer::CreateDeviceMobile()
{
	auto* pDC = GetBaseDisplayContext();

	if (!m_devInfo.CreateDevice(m_zbpp, OnD3D11CreateDevice, CreateWindowCallback))
		return false;

	OnD3D11PostCreateDevice(m_devInfo.Device());

	AdjustWindowForChange(pDC->GetDisplayResolution().x, pDC->GetDisplayResolution().y, EWindowState::Fullscreen);

	pDC->ChangeOutputIfNecessary(true, m_VSync != 0);

	return true;
}
#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX
bool CD3D9Renderer::CreateDeviceDesktop()
{
	auto* pDC = GetBaseDisplayContext();

	UnSetRes();

	// DirectX9 and DirectX10 device creating
#if defined(SUPPORT_DEVICE_INFO)
	if (!m_devInfo.CreateDevice(m_zbpp, OnD3D11CreateDevice, CreateWindowCallback))
		return false;

	//query adapter name
	const DXGI_ADAPTER_DESC1& desc = m_devInfo.AdapterDesc();
	m_adapterInfo.name = CryStringUtils::WStrToUTF8(desc.Description).c_str();
	m_adapterInfo.VendorId = desc.VendorId;
	m_adapterInfo.DeviceId = desc.DeviceId;
	m_adapterInfo.SubSysId = desc.SubSysId;
	m_adapterInfo.Revision = desc.Revision;

	OnD3D11PostCreateDevice(m_devInfo.Device());
#endif

	AdjustWindowForChange(pDC->GetDisplayResolution().x, pDC->GetDisplayResolution().y, EWindowState::Fullscreen);

	pDC->ChangeOutputIfNecessary(true, m_VSync != 0);

	return true;
}
#elif CRY_RENDERER_GNM
bool CD3D9Renderer::CreateDeviceGNM()
{
	auto* pDC = GetBaseDisplayContext();

	CGnmDevice::Create();

	pDC->CreateSwapChain(pDC->GetDisplayResolution().x, pDC->GetDisplayResolution().y);

	GetDevice().AssignDevice(gGnmDevice);

	BindContextToThread(CryGetCurrentThreadId());
	m_DeviceContextWrapper.AssignDeviceContext(gGnmDevice);

	OnD3D11CreateDevice(gGnmDevice);
	OnD3D11PostCreateDevice(gGnmDevice);

	return true;
}
#elif CRY_PLATFORM_ORBIS
bool CD3D9Renderer::CreateDeviceOrbis()
{
	auto* pDC = GetBaseDisplayContext();

	DXOrbis::CreateCCryDXOrbisRenderDevice();
	DXOrbis::CreateCCryDXOrbisSwapChain();
	DXOrbis::Device()->RegisterDeviceThread();

	GetDevice().AssignDevice(nullptr);
	GetDeviceContext().AssignDeviceContext(nullptr);

	OnD3D11CreateDevice(GetDevice().GetRealDevice());
	OnD3D11PostCreateDevice(GetDevice().GetRealDevice());

	return true;
}
#endif

bool CD3D9Renderer::CreateDevice()
{
	LOADING_TIME_PROFILE_SECTION;
	ChangeLog();

	auto* pDC = GetBaseDisplayContext();

	m_pixelAspectRatio = 1.0f;
	m_dwCreateFlags = 0;

	///////////////////////////////////////////////////////////////////
#if CRY_PLATFORM_DURANGO
	if (!CreateDeviceDurango())
		return false;

#elif CRY_PLATFORM_IOS || CRY_PLATFORM_ANDROID
	if (!CreateDeviceMobile())
		return false;

#elif CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX
	if (!CreateDeviceDesktop())
		return false;

#elif CRY_RENDERER_GNM
	if (!CreateDeviceGNM())
		return false;

#elif CRY_PLATFORM_ORBIS
	if (!CreateDeviceOrbis())
		return false;

#else
	#error UNKNOWN RENDER DEVICE PLATFORM
#endif

	m_DevBufMan.Init();

#if defined(ENABLE_RENDER_AUX_GEOM)
	if (m_pRenderAuxGeomD3D)
	{
		m_pRenderAuxGeomD3D->RestoreDeviceObjects();
	}
#endif

	m_pStereoRenderer->InitDeviceAfterD3D();

	UpdateActiveContext(m_pBaseDisplayContext, {});

	return true;
}

#if (CRY_RENDERER_DIRECT3D >= 110) && NTDDI_WIN10 && (WDK_NTDDI_VERSION >= NTDDI_WIN10) && defined(SUPPORT_DEVICE_INFO)
	#include "dxgi1_4.h"
#endif

void CD3D9Renderer::GetVideoMemoryUsageStats(size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently, bool bGetPoolsSizes)
{
	if (bGetPoolsSizes)
	{
		vidMemUsedThisFrame = vidMemUsedRecently = (GetTexturesStreamPoolSize() + CV_r_rendertargetpoolsize) * 1024 * 1024;
	}
	else
	{
#if (CRY_RENDERER_DIRECT3D >= 110) && NTDDI_WIN10 && (WDK_NTDDI_VERSION >= NTDDI_WIN10) && defined(SUPPORT_DEVICE_INFO)
		CD3D9Renderer* rd = gcpRendD3D;
		IDXGIAdapter3* pAdapter = nullptr;

	#if (CRY_RENDERER_DIRECT3D >= 120)
		pAdapter = rd->m_devInfo.Adapter();
		pAdapter->AddRef();
	#else
		rd->m_devInfo.Adapter()->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter);
	#endif

		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfoA = {};
		DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfoB = {};

		if (pAdapter)
		{
			pAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfoA);
			pAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &videoMemoryInfoB);
			pAdapter->Release();
		}
#else
		struct { SIZE_T CurrentUsage; } videoMemoryInfoA = {};
		struct { SIZE_T CurrentUsage; } videoMemoryInfoB = {};

	#if CRY_RENDERER_GNM
		gGnmDevice->GetMemoryUsageStats(videoMemoryInfoA.CurrentUsage, videoMemoryInfoB.CurrentUsage);
	#endif

		vidMemUsedThisFrame = size_t(videoMemoryInfoA.CurrentUsage);
		vidMemUsedRecently = 0;
#endif
	}
}

//===========================================================================================

HRESULT CALLBACK CD3D9Renderer::OnD3D11CreateDevice(D3DDevice* pd3dDevice)
{
	LOADING_TIME_PROFILE_SECTION;
	CD3D9Renderer* rd = gcpRendD3D;
	rd->m_DeviceWrapper.AssignDevice(pd3dDevice);
	GetDeviceObjectFactory().AssignDevice(pd3dDevice);

#if defined(SUPPORT_DEVICE_INFO)
	rd->m_DeviceContextWrapper.AssignDeviceContext(rd->m_devInfo.Context());
#endif
	rd->m_Features |= RFT_OCCLUSIONQUERY | RFT_ALLOWANISOTROPIC | RFT_HW_SM20 | RFT_HW_SM2X | RFT_HW_SM30 | RFT_HW_SM40 | RFT_HW_SM50;

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
	rd->m_d3dDebug.Init(pd3dDevice);
	rd->m_d3dDebug.Update(ESeverityCombination(CV_d3d11_debugMuteSeverity->GetIVal()), CV_d3d11_debugMuteMsgID->GetString(), CV_d3d11_debugBreakOnMsgID->GetString());
	rd->m_bUpdateD3DDebug = false;
#endif

	GetDeviceObjectFactory().AllocateNullResources();
	GetDeviceObjectFactory().AllocatePredefinedSamplerStates();
	GetDeviceObjectFactory().AllocatePredefinedInputLayouts();

#if defined(SUPPORT_DEVICE_INFO)
	rd->BindContextToThread(CryGetCurrentThreadId());

	LARGE_INTEGER driverVersion;
	driverVersion.LowPart = 0;
	driverVersion.HighPart = 0;
	#if CRY_RENDERER_VULKAN >= 10
	auto version = VK_VERSION_1_0;
	driverVersion.HighPart = (VK_VERSION_MAJOR(version) << 16) | VK_VERSION_MINOR(version);
	driverVersion.LowPart = (VK_VERSION_PATCH(version) << 16) | VK_HEADER_VERSION;
	#else
	rd->m_devInfo.Adapter()->CheckInterfaceSupport(__uuidof(ID3D11Device), &driverVersion);
	#endif
	iLog->Log("D3D Adapter: Description: %ls", rd->m_devInfo.AdapterDesc().Description);
	iLog->Log("D3D Adapter: Driver version (UMD): %d.%02d.%02d.%04d", HIWORD(driverVersion.u.HighPart), LOWORD(driverVersion.u.HighPart), HIWORD(driverVersion.u.LowPart), LOWORD(driverVersion.u.LowPart));
	iLog->Log("D3D Adapter: VendorId = 0x%.4X", rd->m_devInfo.AdapterDesc().VendorId);
	iLog->Log("D3D Adapter: DeviceId = 0x%.4X", rd->m_devInfo.AdapterDesc().DeviceId);
	iLog->Log("D3D Adapter: SubSysId = 0x%.8X", rd->m_devInfo.AdapterDesc().SubSysId);
	iLog->Log("D3D Adapter: Revision = %i", rd->m_devInfo.AdapterDesc().Revision);

	// Vendor-specific initializations and workarounds for driver bugs.
	{
		if (rd->m_devInfo.AdapterDesc().VendorId == 4098)
		{
			rd->m_Features |= RFT_HW_ATI;
			rd->InitAMDAPI();
			iLog->Log("D3D Detected: AMD video card");
		}
		else if (rd->m_devInfo.AdapterDesc().VendorId == 4318)
		{
			rd->m_Features |= RFT_HW_NVIDIA;
			rd->InitNVAPI();
			iLog->Log("D3D Detected: NVIDIA video card");
		}
		else if (rd->m_devInfo.AdapterDesc().VendorId == 8086)
		{
			rd->m_Features |= RFT_HW_INTEL;
			iLog->Log("D3D Detected: intel video card");
		}
	}

	rd->m_nGPUs = min(rd->m_nGPUs, (uint32)MAX_GPU_NUM);

#endif
	rd->m_adapterInfo.nNodeCount = max(rd->m_nGPUs, rd->m_adapterInfo.nNodeCount);
	CryLogAlways("Active GPUs: %i", rd->m_nGPUs);

#if CRY_PLATFORM_ORBIS
	rd->m_NumResourceSlots = 128;
	rd->m_NumSamplerSlots = 16;
	rd->m_MaxAnisotropyLevel = min(16, CRenderer::CV_r_texmaxanisotropy);
#else
	rd->m_NumResourceSlots = D3D11_COMMONSHADER_INPUT_RESOURCE_REGISTER_COUNT;
	rd->m_NumSamplerSlots = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
	rd->m_MaxAnisotropyLevel = min(D3D11_REQ_MAXANISOTROPY, CRenderer::CV_r_texmaxanisotropy);
#endif

#if CRY_PLATFORM_WINDOWS
	HWND hWndDesktop = GetDesktopWindow();
	HDC dc = GetDC(hWndDesktop);
	uint16 gamma[3][256];
	if (GetDeviceGammaRamp(dc, gamma))
		rd->m_Features |= RFT_HWGAMMA;
	ReleaseDC(hWndDesktop, dc);
#endif

	// For safety, lots of drivers don't handle tiny texture sizes too tell.
#if defined(SUPPORT_DEVICE_INFO)
	rd->m_MaxTextureMemory = rd->m_devInfo.AdapterDesc().DedicatedVideoMemory;
#else
	rd->m_MaxTextureMemory = 256 * 1024 * 1024;

	#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	rd->m_MaxTextureMemory = 1024 * 1024 * 1024;
	#endif
#endif
	if (CRenderer::CV_r_TexturesStreamPoolSize <= 0)
		CRenderer::CV_r_TexturesStreamPoolSize = (int)(rd->m_MaxTextureMemory / 1024.0f / 1024.0f * 0.75f);

#if CRY_PLATFORM_ORBIS
	rd->m_MaxTextureSize = 8192;
	rd->m_bDeviceSupportsInstancing = true;
#else
	rd->m_MaxTextureSize = D3D11_REQ_FILTERING_HW_ADDRESSABLE_RESOURCE_DIMENSION;
	rd->m_bDeviceSupportsInstancing = true;
#endif

#if !CRY_RENDERER_OPENGLES
	rd->m_bDeviceSupportsGeometryShaders = (rd->m_Features & RFT_HW_SM40) != 0;
#else
	rd->m_bDeviceSupportsGeometryShaders = false;
#endif

#if !CRY_RENDERER_OPENGL && !CRY_RENDERER_VULKAN
	rd->m_bDeviceSupportsTessellation = (rd->m_Features & RFT_HW_SM50) != 0;
#else
	rd->m_bDeviceSupportsTessellation = false;
#endif

	rd->m_Features |= RFT_OCCLUSIONTEST;

	rd->m_bUseWaterTessHW = CV_r_WaterTessellationHW != 0 && rd->m_bDeviceSupportsTessellation;

	PREFAST_SUPPRESS_WARNING(6326); //not a constant vs constant comparison on Win32/Win64
	rd->m_bUseSilhouettePOM = CV_r_SilhouettePOM != 0;

	// Handle the texture formats we need.
	{
		// Find the needed texture formats.
		CRendererResources::s_hwTexFormatSupport.CheckFormatSupport();

		if (CRendererResources::s_hwTexFormatSupport.m_FormatBC1.IsValid() ||
			CRendererResources::s_hwTexFormatSupport.m_FormatBC2.IsValid() ||
			CRendererResources::s_hwTexFormatSupport.m_FormatBC3.IsValid())
		{
			rd->m_Features |= RFT_COMPRESSTEXTURE;
		}

		// One of the two needs to be supported
		if ((rd->m_zbpp == 32) && !CRendererResources::s_hwTexFormatSupport.m_FormatD32FS8.IsValid())
			rd->m_zbpp = 24;
		if ((rd->m_zbpp == 24) && !CRendererResources::s_hwTexFormatSupport.m_FormatD24S8.IsValid())
			rd->m_zbpp = 32;

		iLog->Log(" Renderer DepthBits: %d", rd->m_zbpp);
	}

	rd->m_Features |= RFT_HW_HDR;

	return S_OK;
}

HRESULT CALLBACK CD3D9Renderer::OnD3D11PostCreateDevice(D3DDevice* pd3dDevice)
{
	LOADING_TIME_PROFILE_SECTION;
	CD3D9Renderer* rd = gcpRendD3D;
	auto* pDC = rd->GetBaseDisplayContext();

	pDC->m_nSSSamplesX = CV_r_Supersampling;
	pDC->m_nSSSamplesY = CV_r_Supersampling;
	pDC->m_bMainViewport = true;

#if DX11_WRAPPABLE_INTERFACE && CAPTURE_REPLAY_LOG
	rd->MemReplayWrapD3DDevice();
#endif

	// Force resize
	const auto displayWidth = pDC->GetDisplayResolution().x;
	const auto displayHeight = pDC->GetDisplayResolution().y;
	pDC->m_DisplayWidth = 0;
	pDC->m_DisplayHeight = 0;
	pDC->ChangeDisplayResolution(displayWidth, displayHeight);

	// Copy swap chain surface desc back to global var
	rd->m_d3dsdBackBuffer = pDC->GetSwapChain().GetSurfaceDesc();

	rd->ReleaseAuxiliaryMeshes();
	rd->CreateAuxiliaryMeshes();

	rd->m_bDeviceLost = 0;

	//rd->ResetToDefault();

	return S_OK;
}

#if CRY_PLATFORM_WINDOWS
// Renderer looks for multi-monitor setup changes and fullscreen key combination
bool CD3D9Renderer::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	switch (message)
	{
	case WM_DISPLAYCHANGE:
	case WM_DEVICECHANGE:
		{
			// Count the number of connected display devices
			bool bHaveMonitorsChanged = true;
			uint connectedMonitors = 0;
			EnumDisplayMonitors(NULL, NULL, CountConnectedMonitors, reinterpret_cast<LPARAM>(&connectedMonitors));

			// Check for changes
			if (connectedMonitors > m_nConnectedMonitors)
			{
				iSystem->GetILog()->LogAlways("[Renderer] A display device has been connected to the system");
			}
			else if (connectedMonitors < m_nConnectedMonitors)
			{
				iSystem->GetILog()->LogAlways("[Renderer] A display device has been disconnected from the system");
			}
			else
			{
				bHaveMonitorsChanged = false;
			}

			// Update state
			m_nConnectedMonitors = connectedMonitors;
			m_bDisplayChanged = bHaveMonitorsChanged;
		}
		break;

	case WM_SYSKEYDOWN:
		{
			const bool bAlt = (lParam & (1 << 29)) != 0;
			if (bAlt)
			{
				if (wParam == VK_RETURN) // ALT+ENTER
				{
					if (m_CVWindowType != nullptr)
					{
						if (m_CVWindowType->GetIVal() == static_cast<int>(EWindowState::Fullscreen))
						{
							m_CVWindowType->Set(static_cast<int>(EWindowState::Windowed));
						}
						else
						{
							m_CVWindowType->Set(static_cast<int>(EWindowState::Fullscreen));
						}

						// Indicate that we have handled the event
						*pResult = 0;
						return true;
					}
				}
				else if (wParam == VK_MENU)
				{
					// Windows tries to focus the menu when pressing Alt,
					// so we need to tell it that we already handled the event
					*pResult = 0;
					return true;
				}
			}
		}
		break;

	case WM_SIZE:
		{
			if (wParam == SIZE_RESTORED)
			{
				m_bWindowRestored = true;
			}
			if (m_CVWidth != nullptr && !m_isChangingResolution && !IsFullscreen())
			{
				m_CVWidth->Set(LOWORD(lParam));
				m_CVHeight->Set(HIWORD(lParam));
			}
		}
		break;

	case WM_ACTIVATE:
		{
			// Toggle DXGI fullscreen state when user alt-tabs out
			// This is required since we explicitly set the DXGI_MWA_NO_WINDOW_CHANGES, forbidding DXGI from handling this itself
			if (IsFullscreen())
			{
				gcpRendD3D->ExecuteRenderThreadCommand([wParam]()
				{
					gcpRendD3D->GetBaseDisplayContext()->SetFullscreenState(wParam != 0);
				}, ERenderCommandFlags::None);
			}
		}
		break;
	}
	return false;
}
#endif // CRY_PLATFORM_WINDOWS

#include "DeviceInfo.inl"

void EnableCloseButton(void* hWnd, bool enabled)
{
#if CRY_PLATFORM_WINDOWS
	if (hWnd)
	{
		HMENU hMenu = GetSystemMenu((HWND) hWnd, FALSE);
		if (hMenu)
		{
			const unsigned int flags = enabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | flags);
		}
	}
#endif
}

#if defined(DX11_ALLOW_D3D_DEBUG_RUNTIME)
string D3DDebug_GetLastMessage()
{
	return gcpRendD3D->m_d3dDebug.GetLastMessage();
}
#endif
