// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <string>

#include "RenderDisplayContext.h"
#include "RenderOutput.h"

std::string GenerateUniqueTextureName(const std::string& prefix, uint32 id, const std::string& name)
{
	return prefix + "-" + std::to_string(id) + (name.empty() ? "" : "-" + name);
}

void CRenderDisplayContext::BeginRendering()
{
	// Toggle HDR/LDR output selection
	if (false)
	{
		AllocateColorTarget();
		AllocateDepthTarget();

		m_pRenderOutput->ChangeOutputResolution(
			m_pRenderOutput->GetOutputResolution()[0],
			m_pRenderOutput->GetOutputResolution()[1]
		);
	}
}

void CRenderDisplayContext::EndRendering()
{
}

bool CRenderDisplayContext::IsEditorDisplay() const 
{ 
	return m_uniqueId != 0 || gRenDev->IsEditorMode(); 
}

bool CRenderDisplayContext::IsNativeScalingEnabled() const 
{ 
	return GetRenderOutput() ? 
		GetDisplayResolution() != GetRenderOutput()->GetOutputResolution() : 
		false; 
}

void CRenderDisplayContext::SetDisplayResolutionAndRecreateTargets(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp)
{
	CRY_ASSERT(displayWidth > 0 && displayHeight > 0);

	m_viewport = vp;

	if (m_DisplayWidth  == displayWidth &&
	    m_DisplayHeight == displayHeight)
		return;

	m_DisplayWidth  = displayWidth;
	m_DisplayHeight = displayHeight;

	if (m_DisplayHeight)
		m_aspectRatio = (float)m_DisplayWidth / (float)m_DisplayHeight;

	AllocateColorTarget();
	AllocateDepthTarget();
}

void CRenderDisplayContext::ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp)
{
	SetDisplayResolutionAndRecreateTargets(displayWidth, displayHeight, vp);

	m_pRenderOutput->InitializeDisplayContext();
	m_pRenderOutput->ReinspectDisplayContext();
	m_pRenderOutput->m_hasBeenCleared = 0;
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateColorTarget()
{
	m_pColorTarget = nullptr;
	if (!NeedsTempColor())
		return;

	const ETEX_Format eCFormat = GetColorFormat();
	const ColorF clearValue = Clr_Empty;

	// NOTE: Actual device texture allocation happens just before rendering.
	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const std::string uniqueTexName = GenerateUniqueTextureName(CImageExtensionHelper::IsDynamicRange(eCFormat) ? "$HDR-Overlay" : "$LDR-Overlay", m_uniqueId, m_name);

	m_pColorTarget = nullptr;
	m_pColorTarget.Assign_NoAddRef(CTexture::GetOrCreateRenderTarget(uniqueTexName.c_str(), m_DisplayWidth, m_DisplayHeight, clearValue, eTT_2D, renderTargetFlags, eCFormat));
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::AllocateDepthTarget()
{
	m_pDepthTarget = nullptr;
	if (!NeedsDepthStencil())
		return;

	const ETEX_Format eZFormat = GetDepthFormat();
	const float  clearDepth   = 0.f;
	const uint   clearStencil = 0;
	const ColorF clearValues  = ColorF(clearDepth, FLOAT(clearStencil), 0.f, 0.f);

	// Create the native resolution depth stencil buffer for overlay rendering if needed
	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_DEPTHSTENCIL;
	const std::string uniqueOverlayTexName = GenerateUniqueTextureName("$Z-Overlay", m_uniqueId, m_name);

	m_pDepthTarget = nullptr;
	m_pDepthTarget.Assign_NoAddRef(CTexture::GetOrCreateDepthStencil(uniqueOverlayTexName.c_str(), m_DisplayWidth, m_DisplayHeight, clearValues, eTT_2D, renderTargetFlags, eZFormat));
}

void CRenderDisplayContext::ReleaseResources()
{
	m_pRenderOutput->ReleaseResources();
	m_pDepthTarget = nullptr;
	m_pColorTarget = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void CSwapChainBackedRenderDisplayContext::CreateSwapChain(CRY_HWND hWnd, bool isFullscreen, bool vsync)
{
	CRY_ASSERT(hWnd);

	m_hWnd = hWnd;
	m_fullscreen = isFullscreen;

	// TODO: make m_desc a function parameter
	m_nSSSamplesX = m_desc.superSamplingFactor.x;
	m_nSSSamplesY = m_desc.superSamplingFactor.y;

#if !CRY_PLATFORM_CONSOLE
	auto w = m_desc.screenResolution.x,
	     h = m_desc.screenResolution.y;

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

	CreateOutput();
	CreateSwapChain(GetWindowHandle(),
		m_pOutput,
		w,
		h,
		IsMainContext(),
		IsFullscreen(),
		vsync);

#if (CRY_RENDERER_VULKAN >= 10)
	m_bVSync = vsync;
#endif
#endif

	// Create the output
	ChangeOutputIfNecessary(IsFullscreen(), vsync);
}

void CSwapChainBackedRenderDisplayContext::ShutDown()
{
	ReleaseResources();

#if (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
	if (m_swapChain.GetSwapChain())
		m_swapChain.GetSwapChain()->SetFullscreenState(FALSE, 0);
#endif

#if CRY_PLATFORM_WINDOWS
	SAFE_RELEASE(m_pOutput);
#endif
}

void CSwapChainBackedRenderDisplayContext::ReleaseResources()
{
	if (gcpRendD3D.GetDeviceContext().IsValid())
		GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	CRenderDisplayContext::ReleaseResources();

	m_pBackBufferPresented = nullptr;

	if (m_pBackBufferProxy)
	{
		m_pBackBufferProxy->RefDevTexture(nullptr);
		m_pBackBufferProxy = nullptr;
	}

	for (auto &t : m_backBuffersArray)
	{
		if (!t) 
			continue;
		t->SetDevTexture(nullptr);
		t = nullptr;
	}

	m_bSwapProxy = true;
}

uint32 CSwapChainBackedRenderDisplayContext::ComputePresentInterval(uint32 presentInterval)
{
	if (presentInterval && GetRefreshRateNumerator() != 0 && GetRefreshRateDenominator() != 0)
	{
		static ICVar* pSysMaxFPS = gEnv && gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_MaxFPS") : 0;
		if (pSysMaxFPS)
		{
			const int32 maxFPS = pSysMaxFPS->GetIVal();
			if (maxFPS > 0)
			{
				const float refreshRate = float(GetRefreshRateNumerator()) / GetRefreshRateDenominator();
				const float lockedFPS = float(maxFPS);

				presentInterval = (uint32)clamp_tpl((int)floorf(refreshRate / lockedFPS + 0.1f), 1, 4);
			}
		}
	}

	return presentInterval;
};

ETEX_Format CSwapChainBackedRenderDisplayContext::GetBackBufferFormat() const
{
	return DeviceFormats::ConvertToTexFormat(m_swapChain.GetSurfaceDesc().Format);
}

void CSwapChainBackedRenderDisplayContext::ReleaseBackBuffers()
{
	// NOTE: Because we want to delete objects referencing swap-chain
	// resources immediately we have to wait for the GPU to finish using it.
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	m_pBackBufferPresented = nullptr;

	if (m_pBackBufferProxy)
		m_pBackBufferProxy->RefDevTexture(nullptr);

	// Keep the CTextures for reuse, remove the device-resources and trigger dirty-handling
	for (auto &t : m_backBuffersArray)
	{
		if (t)
			t->SetDevTexture(nullptr);
	}

	GetDeviceObjectFactory().FlushToGPU(true, true);

	m_bSwapProxy = true;
}

void CSwapChainBackedRenderDisplayContext::AllocateBackBuffers()
{
	unsigned indices = 1;

#if CRY_RENDERER_GNM
	CGnmSwapChain::SDesc desc = m_swapChain.GetSwapChain()->GnmGetDesc();
	indices = desc.numBuffers;
#elif (CRY_RENDERER_DIRECT3D >= 120) || (CRY_RENDERER_VULKAN >= 10)
	DXGI_SWAP_CHAIN_DESC scDesc = m_swapChain.GetDesc();
	indices = scDesc.BufferCount;
#endif

	m_backBuffersArray.resize(indices, nullptr);
	m_pBackBufferPresented = nullptr;

	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const uint32 displayWidth = m_DisplayWidth;
	const uint32 displayHeight = m_DisplayHeight;
	const ETEX_Format displayFormat = GetBackBufferFormat();

	char str[40];

	// ------------------------------------------------------------------------------
	if (!m_pBackBufferProxy)
	{
		sprintf(str, "$SwapChainBackBuffer %d:Current", m_uniqueId);

		m_pBackBufferProxy = nullptr;
		m_pBackBufferProxy.Assign_NoAddRef(CTexture::GetOrCreateTextureObject(str, displayWidth, displayHeight, 1, eTT_2D, renderTargetFlags, displayFormat));
		m_pBackBufferProxy->SRGBRead(DeviceFormats::ConvertToSRGB(DeviceFormats::ConvertFromTexFormat(displayFormat)) == m_swapChain.GetSurfaceDesc().Format);
	}

	if (!m_pBackBufferProxy->GetDevTexture())
	{
		m_pBackBufferProxy->Invalidate(displayWidth, displayHeight, displayFormat);
	}

	CRY_ASSERT(m_pBackBufferProxy->GetWidth() == displayWidth);
	CRY_ASSERT(m_pBackBufferProxy->GetHeight() == displayHeight);
	CRY_ASSERT(m_pBackBufferProxy->GetSrcFormat() == displayFormat);

	// ------------------------------------------------------------------------------
	for (int i = 0, n = indices; i < n; ++i)
	{
		sprintf(str, "$SwapChainBackBuffer %d:Buffer %d", m_uniqueId, i);

		// Prepare back-buffer texture(s)
		if (!m_backBuffersArray[i])
		{
			m_backBuffersArray[i] = nullptr;
			m_backBuffersArray[i].Assign_NoAddRef(CTexture::GetOrCreateTextureObject(str, displayWidth, displayHeight, 1, eTT_2D, renderTargetFlags, displayFormat));
			m_backBuffersArray[i]->SRGBRead(DeviceFormats::ConvertToSRGB(DeviceFormats::ConvertFromTexFormat(displayFormat)) == m_swapChain.GetSurfaceDesc().Format);
		}

		if (!m_backBuffersArray[i]->GetDevTexture())
		{
#if   CRY_RENDERER_GNM
			CGnmTexture* pBackBuffer = m_swapChain.GetSwapChain()->GnmGetBuffer(i);
#elif CRY_RENDERER_VULKAN
			D3DTexture* pBackBuffer = m_swapChain.GetSwapChain()->GetVKBuffer(i);
			CRY_ASSERT(pBackBuffer != nullptr);
#else
			D3DTexture* pBackBuffer = nullptr;
			HRESULT hr = m_swapChain.GetSwapChain()->GetBuffer(i, IID_GFX_ARGS(&pBackBuffer));
			CRY_VERIFY(SUCCEEDED(hr) && pBackBuffer != nullptr);
#endif

			const auto &layout = m_backBuffersArray[i]->GetLayout();
			m_backBuffersArray[i]->Invalidate(displayWidth, displayHeight, displayFormat);
			m_backBuffersArray[i]->SetDevTexture(CDeviceTexture::Associate(layout, pBackBuffer));

			SetDebugName(pBackBuffer, str);

			// Guarantee that the back-buffers are cleared on first use (e.g. Flash alpha-blends onto the back-buffer)
			// NOTE: GNM requires shaders to be initialized before issuing any draws/clears/copies/resolves. This is not yet the case here.
			// NOTE: Can only access current back-buffer on DX12
#if !(CRY_RENDERER_GNM || CRY_RENDERER_DIRECT3D >= 120)
			CClearSurfacePass::Execute(m_backBuffersArray[i], Clr_Transparent);
#endif
		}

		CRY_ASSERT(m_backBuffersArray[i]->GetWidth() == displayWidth);
		CRY_ASSERT(m_backBuffersArray[i]->GetHeight() == displayHeight);
		CRY_ASSERT(m_backBuffersArray[i]->GetSrcFormat() == displayFormat);
	}

	// Assign first back-buffer on initialization
	m_pBackBufferProxy->RefDevTexture(GetCurrentBackBuffer()->GetDevTexture());
	m_bSwapProxy = false;
}

void CSwapChainBackedRenderDisplayContext::CreateOutput()
{
#if CRY_PLATFORM_WINDOWS
	CRY_ASSERT(m_hWnd);
	HMONITOR hMonitor = MonitorFromWindow((HWND)m_hWnd, MONITOR_DEFAULTTONEAREST);

	if (m_pOutput != nullptr)
	{
		CRY_VERIFY(m_pOutput->Release() == 0);
		m_pOutput = nullptr;
	}

	// Find the output that matches the monitor our window is currently on
	IDXGIAdapter1* pAdapter = gcpRendD3D->DevInfo().Adapter();
	IDXGIOutput* pOutput = nullptr;
	
	for (unsigned int i = 0; pAdapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND && pOutput; ++i)
	{
		DXGI_OUTPUT_DESC outputDesc;
		if (SUCCEEDED(pOutput->GetDesc(&outputDesc)) && outputDesc.Monitor == hMonitor)
		{
			// Promote interfaces to the required level
			pOutput->QueryInterface(IID_GFX_ARGS(&m_pOutput));
			break;
		}
	}

	if (m_pOutput == nullptr)
	{
		if (SUCCEEDED(pAdapter->EnumOutputs(0, &pOutput)))
		{
			// Promote interfaces to the required level
			pOutput->QueryInterface(IID_GFX_ARGS(&m_pOutput));
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
#elif CRY_PLATFORM_ANDROID || CRY_PLATFORM_LINUX
	m_pOutput = CCryVKGIOutput::Create(gcpRendD3D->DevInfo().Adapter(), 0);
#endif
}

void CSwapChainBackedRenderDisplayContext::ChangeOutputIfNecessary(bool isFullscreen, bool vsync)
{
	bool isWindowOnExistingOutputMonitor = false;
#if CRY_PLATFORM_WINDOWS
	HMONITOR hMonitor = MonitorFromWindow((HWND)m_hWnd, MONITOR_DEFAULTTONEAREST);

	DXGI_OUTPUT_DESC outputDesc;
	if (m_pOutput != nullptr && SUCCEEDED(m_pOutput->GetDesc(&outputDesc)))
		isWindowOnExistingOutputMonitor = outputDesc.Monitor == hMonitor;
#endif

#if CRY_PLATFORM_ANDROID
	isWindowOnExistingOutputMonitor = true;
#endif

	// Recreate output only if the window has been moved to another monitor
	if (!isWindowOnExistingOutputMonitor)
		CreateOutput();

#if !CRY_PLATFORM_CONSOLE
	bool recreateSwapChain = (!isWindowOnExistingOutputMonitor && isFullscreen);

#if (CRY_RENDERER_DIRECT3D >= 120)
	recreateSwapChain |= ((m_swapChain.GetDesc().Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) && isFullscreen);
#elif CRY_RENDERER_VULKAN
	recreateSwapChain |= (m_bVSync != vsync);
#endif

	// Output will need to be recreated if we switched to fullscreen on a different monitor
	if (recreateSwapChain)
	{
		auto w = m_swapChain.GetSurfaceDesc().Width,
		     h = m_swapChain.GetSurfaceDesc().Height;

		ReleaseBackBuffers();

		// Swap chain needs to be recreated with the new output in mind
		CreateSwapChain(GetWindowHandle(), m_pOutput, w, h, isFullscreen, IsMainContext(), vsync);

		if (m_pOutput != nullptr)
		{
			CRY_VERIFY(m_pOutput->Release() == 0);
			m_pOutput = nullptr;
		}
	}

#if (CRY_RENDERER_VULKAN >= 10)
	m_bVSync = vsync;
#endif
#endif

	SetFullscreenState(isFullscreen);
}

void CSwapChainBackedRenderDisplayContext::ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp)
{
	SetDisplayResolutionAndRecreateTargets(displayWidth, displayHeight, vp);

	if (m_swapChain.GetSwapChain())
	{
		ReleaseBackBuffers();

		m_swapChain.ResizeSwapChain(CRendererCVars::CV_r_MaxFrameLatency + 1, m_DisplayWidth, m_DisplayHeight, m_fullscreen, IsMainContext());

		AllocateBackBuffers();

		// Update swapchain dimensions
#if CRY_RENDERER_GNM
		CGnmSwapChain::SDesc desc = GetSwapChain().GetSwapChain()->GnmGetDesc();
		desc.width = displayWidth;
		desc.height = displayHeight;
		GetSwapChain().GetSwapChain()->GnmSetDesc(desc);
#elif CRY_PLATFORM_ORBIS
		GetSwapChain().GetSwapChain()->UpdateBackbufferDimensions(displayWidth, displayHeight);
#endif

		m_pRenderOutput->InitializeDisplayContext();
		m_pRenderOutput->ReinspectDisplayContext();
		m_pRenderOutput->m_hasBeenCleared = 0;

		// Configure maximum frame latency
#if CRY_PLATFORM_WINDOWS && CRY_RENDERER_DIRECT3D
		{
			DXGIDevice* pDXGIDevice = nullptr;
			if (SUCCEEDED(gcpRendD3D->GetDevice().GetRealDevice()->QueryInterface(IID_GFX_ARGS(&pDXGIDevice))) && pDXGIDevice)
				pDXGIDevice->SetMaximumFrameLatency(CRendererCVars::CV_r_MaxFrameLatency);
			SAFE_RELEASE(pDXGIDevice);
		}
#endif
	}
}

void CSwapChainBackedRenderDisplayContext::SetFullscreenState(bool isFullscreen)
{
	if (m_fullscreen != isFullscreen)
	{
		m_fullscreen = isFullscreen;

		if (m_swapChain.GetSwapChain())
		{
			ReleaseBackBuffers();

			m_swapChain.ResizeSwapChain(CRendererCVars::CV_r_MaxFrameLatency + 1, m_DisplayWidth, m_DisplayHeight, m_fullscreen, IsMainContext(), true);

			AllocateBackBuffers();
		}
	}
}

Vec2_tpl<uint32_t> CSwapChainBackedRenderDisplayContext::FindClosestMatchingScreenResolution(const Vec2_tpl<uint32_t> &resolution) const
{
#if CRY_PLATFORM_WINDOWS
	if (m_swapChain.GetSwapChain() && m_pOutput)
	{
		DXGI_SWAP_CHAIN_DESC scDesc = m_swapChain.GetDesc();

		scDesc.BufferDesc.Width = resolution.x;
		scDesc.BufferDesc.Height = resolution.y;

		DXGI_MODE_DESC match;
		if (SUCCEEDED(m_pOutput->FindClosestMatchingMode(&scDesc.BufferDesc, &match, gcpRendD3D.DevInfo().Device())))
		{
			return Vec2_tpl<uint32_t>{ match.Width, match.Height };
		}
	}
#endif

	return resolution;
}

RectI CSwapChainBackedRenderDisplayContext::GetCurrentMonitorBounds() const
{
#ifdef CRY_PLATFORM_WINDOWS
	// When moving the window, update the preferred monitor dimensions so full-screen will pick the closest monitor
	HMONITOR hMonitor = MonitorFromWindow((HWND)m_hWnd, MONITOR_DEFAULTTONEAREST);

	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(hMonitor, &monitorInfo);

	return RectI{ monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top };
#else
	CRY_ASSERT_MESSAGE(false, "Not implemented for platform!");
	return RectI();
#endif
}

CTexture* CSwapChainBackedRenderDisplayContext::GetCurrentBackBuffer() const
{
	uint32 index = 0;

#if (CRY_RENDERER_DIRECT3D >= 120) && defined(__dxgi1_4_h__) // Otherwise index front-buffer only
	index = m_swapChain.GetSwapChain()->GetCurrentBackBufferIndex();
#endif
#if (CRY_RENDERER_VULKAN >= 10)
	index = m_swapChain.GetSwapChain()->GetCurrentBackBufferIndex();
#endif
#if (CRY_RENDERER_GNM)
	// The GNM renderer uses the GPU to drive scan-out, so a command-list is required here.
	auto* pCommandList = GnmCommandList(GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterfaceImpl());
	index = m_swapChain.GetSwapChain()->GnmGetCurrentBackBufferIndex(pCommandList);
#endif

	CRY_ASSERT(index < m_backBuffersArray.size());
	if (index >= m_backBuffersArray.size())
		return nullptr;

	return m_backBuffersArray[index];
}

CTexture* CSwapChainBackedRenderDisplayContext::GetStorableColorOutput()
{
	if (NeedsTempColor())
	{
		CRY_ASSERT(m_pColorTarget);
		return m_pColorTarget.get();
	}

	// Toggle current back-buffer if transitioning into LDR output
	PostPresent();
	return m_pBackBufferProxy;
}

void CSwapChainBackedRenderDisplayContext::PrePresent()
{
	m_pBackBufferPresented = GetCurrentBackBuffer();
	m_bSwapProxy = true;
}

void CSwapChainBackedRenderDisplayContext::PostPresent()
{
	if (!m_bSwapProxy || !m_backBuffersArray.size())
		return;

	// Substitute current swap-chain back-buffer
	CDeviceTexture* pNewDeviceTex = GetCurrentBackBuffer()->GetDevTexture();
	CDeviceTexture* pOldDeviceTex = m_pBackBufferProxy->GetDevTexture();

	// Trigger dirty-flag handling
	if (pNewDeviceTex != pOldDeviceTex)
		m_pBackBufferProxy->RefDevTexture(pNewDeviceTex);

	m_bSwapProxy = false;
	m_pRenderOutput->m_hasBeenCleared = 0;
}

//////////////////////////////////////////////////////////////////////////

CCustomRenderDisplayContext::CCustomRenderDisplayContext(IRenderer::SDisplayContextDescription desc, std::string name, uint32 uniqueId, std::vector<TexSmartPtr> &&backBuffersArray, uint32_t initialSwapChainIndex)
	: CRenderDisplayContext(desc, "CustomDisplay", uniqueId)
	, m_swapChainIndex(initialSwapChainIndex)
{
	CRY_ASSERT(backBuffersArray.size());

	const uint32 renderTargetFlags = FT_NOMIPS | FT_DONT_STREAM | FT_USAGE_RENDERTARGET;
	const uint32 displayWidth = backBuffersArray[initialSwapChainIndex]->GetWidth();
	const uint32 displayHeight = backBuffersArray[initialSwapChainIndex]->GetHeight();
	const ETEX_Format displayFormat = backBuffersArray[initialSwapChainIndex]->GetDstFormat();

	char str[40];

	// ------------------------------------------------------------------------------
	if (!m_pBackBufferProxy)
	{
		sprintf(str, "$SwapChainBackBuffer %d:Current", m_uniqueId);

		m_pBackBufferProxy = nullptr;
		m_pBackBufferProxy.Assign_NoAddRef(CTexture::GetOrCreateTextureObject(str, displayWidth, displayHeight, 1, eTT_2D, renderTargetFlags, displayFormat));
		m_pBackBufferProxy->SRGBRead(false);
	}

	if (!m_pBackBufferProxy->GetDevTexture())
	{
		m_pBackBufferProxy->Invalidate(displayWidth, displayHeight, displayFormat);
	}

	CRY_ASSERT(m_pBackBufferProxy->GetWidth() == displayWidth);
	CRY_ASSERT(m_pBackBufferProxy->GetHeight() == displayHeight);
	CRY_ASSERT(m_pBackBufferProxy->GetSrcFormat() == displayFormat);
	// ------------------------------------------------------------------------------

	m_backBuffersArray = std::move(backBuffersArray);

	// Assign first back-buffer on initialization
	m_pBackBufferProxy->RefDevTexture(GetCurrentBackBuffer()->GetDevTexture());
	m_bSwapProxy = false;

	// Create the targets
	ChangeDisplayResolution(displayWidth, displayHeight);
}

void CCustomRenderDisplayContext::ShutDown()
{
	ReleaseResources();
}

void CCustomRenderDisplayContext::ReleaseResources()
{
	if (gcpRendD3D.GetDeviceContext().IsValid())
		GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	CRenderDisplayContext::ReleaseResources();

	m_pBackBufferPresented = nullptr;

	if (m_pBackBufferProxy)
	{
		m_pBackBufferProxy->RefDevTexture(nullptr);
		m_pBackBufferProxy = nullptr;
	}

	m_bSwapProxy = true;
}

void CCustomRenderDisplayContext::SetSwapChainIndex(uint32_t index)
{
	CRY_ASSERT(index < m_backBuffersArray.size());

	m_swapChainIndex = index;
	m_bSwapProxy = true;
}

CTexture* CCustomRenderDisplayContext::GetStorableColorOutput()
{
	if (NeedsTempColor())
	{
		CRY_ASSERT(m_pColorTarget);
		return m_pColorTarget.get();
	}

	// Toggle current back-buffer if transitioning into LDR output
	PostPresent();
	return m_pBackBufferProxy;
}

void CCustomRenderDisplayContext::PrePresent()
{
	m_pBackBufferPresented = GetCurrentBackBuffer();
}

void CCustomRenderDisplayContext::PostPresent()
{
	if (!m_bSwapProxy || !m_backBuffersArray.size())
		return;

	// Substitute current swap-chain back-buffer
	CDeviceTexture* pNewDeviceTex = GetCurrentBackBuffer()->GetDevTexture();
	CDeviceTexture* pOldDeviceTex = m_pBackBufferProxy->GetDevTexture();

	// Trigger dirty-flag handling
	if (pNewDeviceTex != pOldDeviceTex)
		m_pBackBufferProxy->RefDevTexture(pNewDeviceTex);

	m_bSwapProxy = false;
	m_pRenderOutput->m_hasBeenCleared = 0;
}
