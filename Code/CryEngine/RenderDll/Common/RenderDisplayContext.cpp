// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RenderDisplayContext.h"
#include "RenderOutput.h"
#include "DriverD3D.h"

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
	SetHWND(0);
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetHWND(CryDisplayContextHandle hWnd)
{
	if (hWnd == m_hWnd)
		return;

	int w = m_DisplayWidth, h = m_DisplayHeight;

#if CRY_PLATFORM_WINDOWS
	if (TRUE == ::IsWindow((HWND)hWnd))
	{
		RECT rc;
		if (TRUE == GetWindowRect((HWND)hWnd, &rc))
		{
			// On Windows force screen resolution to be a real pixel size of the display context window
			w = rc.right - rc.left;
			h = rc.bottom - rc.top;
		}
	}
#endif

	m_hWnd = hWnd;
	m_pRenderOutput = std::make_shared<CRenderOutput>(this);

	InitializeDisplayResolution(w, h);
}

//////////////////////////////////////////////////////////////////////////
void CRenderDisplayContext::SetViewport(const SRenderViewport& vp)
{
	m_viewport = vp;
}

void CRenderDisplayContext::ToggleHDRRendering(bool bHDRRendering)
{
	// Toggle HDR/LDR output selection
	if (m_bHDRRendering != bHDRRendering)
	{
		m_bHDRRendering = bHDRRendering;
		m_pRenderOutput->ChangeOutputResolution(
			m_pRenderOutput->GetOutputResolution()[0],
			m_pRenderOutput->GetOutputResolution()[1]
		);
	}
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

	SAFE_RELEASE(m_pSwapChain);
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

void CRenderDisplayContext::ChangeDisplayResolution(int displayWidth, int displayHeight, bool bResizeTarget, bool bForce)
{
	// No changes do not need to resize
	if (m_DisplayWidth == displayWidth && m_DisplayHeight == displayHeight && m_pSwapChain && !bForce)
		return;

	ReleaseBackBuffers();

	InitializeDisplayResolution(displayWidth, displayHeight);

	if (!m_pSwapChain)
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
void CRenderDisplayContext::AssignSwapChain(DXGISwapChain* pSwapChain)
{
	const bool bDifferentSwapChain = m_pSwapChain != pSwapChain;

	if (bDifferentSwapChain)
	{
		if (m_pSwapChain)
		{
			ShutDown();
		}

		m_pSwapChain = pSwapChain;
	}

	ReadSwapChainSurfaceDesc();

	const int displayWidth  = m_swapChainDesc.Width;
	const int displayHeight = m_swapChainDesc.Height;

	ChangeDisplayResolution(displayWidth, displayHeight, false, bDifferentSwapChain);
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
	if (m_pSwapChain)
		return;

	DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM;

	const int backbufferWidth  = m_DisplayWidth;
	const int backbufferHeight = m_DisplayHeight;

	if (!m_pSwapChain)
	{
#if CRY_RENDERER_GNM
		CGnmSwapChain::SDesc desc;
		desc.width = backbufferWidth;
		desc.height = backbufferHeight;
		desc.format = fmt;

		const HRESULT hr = GnmCreateSwapChain(desc, &m_pSwapChain) ? S_OK : DXGI_ERROR_DRIVER_INTERNAL_ERROR;
		CRY_ASSERT(SUCCEEDED(hr) && "Cannot create swapchain");
#else
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
		scDesc.BufferCount = 1;
		scDesc.OutputWindow = (HWND)m_hWnd;

		scDesc.Windowed = TRUE;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scDesc.Flags = 0;

#if defined(SUPPORT_DEVICE_INFO)
		HRESULT hr = gcpRendD3D.DevInfo().Factory()->CreateSwapChain(gcpRendD3D.GetDevice().GetRealDevice(), &scDesc, &pSwapChain);
#elif CRY_PLATFORM_ORBIS
		HRESULT hr = CCryDXOrbisGIFactory().CreateSwapChain(gcpRendD3D.GetDevice().GetRealDevice(), &scDesc, &pSwapChain);
#elif CRY_PLATFORM_DURANGO
		// create the D3D11 swapchain here
		HRESULT hr = NULL;
		__debugbreak();
#else
#error UNKNOWN PLATFORM TRYING TO CREATE SWAP CHAIN
#endif
		hr = pSwapChain->QueryInterface(__uuidof(DXGISwapChain), (void**)&m_pSwapChain);
		assert(SUCCEEDED(hr) && m_pSwapChain != 0);

		// Decrement Create() increment
		SAFE_RELEASE(pSwapChain);
#endif

		ReadSwapChainSurfaceDesc();
	}
}

void CRenderDisplayContext::ResizeSwapChain(bool bResizeTarget)
{
	// Wait for GPU to finish occupying the resources
	GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->ClearState(true);

	const int backbufferWidth  = m_DisplayWidth;
	const int backbufferHeight = m_DisplayHeight;

	m_swapChainDesc.Width  = backbufferWidth;
	m_swapChainDesc.Height = backbufferHeight;

#if CRY_RENDERER_GNM
	CGnmSwapChain::SDesc desc = m_pSwapChain->GnmGetDesc();
	desc.width  = backbufferWidth;
	desc.height = backbufferHeight;
	HRESULT hr = m_pSwapChain->GnmSetDesc(desc) ? S_OK : E_FAIL;
	CRY_ASSERT(SUCCEEDED(hr));
#elif CRY_PLATFORM_DURANGO
	// Durango Swap-Chain is unresizable
	DXGI_SWAP_CHAIN_DESC scDesc;
	m_pSwapChain->GetDesc(&scDesc);

	CRY_ASSERT(scDesc.BufferDesc.Width  == backbufferWidth);
	CRY_ASSERT(scDesc.BufferDesc.Height == backbufferHeight);
#elif (CRY_RENDERER_DIRECT3D >= 110) || (CRY_RENDERER_VULKAN >= 10)
	DXGI_SWAP_CHAIN_DESC scDesc;
	m_pSwapChain->GetDesc(&scDesc);

	scDesc.BufferDesc.Width  = backbufferWidth;
	scDesc.BufferDesc.Height = backbufferHeight;

	// Resize the hWnd's dimensions associated with the SwapChain (triggers EVENT)
	if (bResizeTarget)
	{
		HRESULT rr = m_pSwapChain->ResizeTarget(&scDesc.BufferDesc);
		CRY_ASSERT(SUCCEEDED(rr));
	}

	// Resize the Resources associated with the SwapChain
	{
		HRESULT hr = m_pSwapChain->ResizeBuffers(0, scDesc.BufferDesc.Width, scDesc.BufferDesc.Height, DXGI_FORMAT_UNKNOWN, scDesc.Flags);
		CRY_ASSERT(SUCCEEDED(hr));
	}
#endif

	ReadSwapChainSurfaceDesc();
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
	ETEX_Format eHDRFormat =
		CRenderer::CV_r_HDRTexFormat == 0 ? eTF_R11G11B10F : eTF_R16G16B16A16F;

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
