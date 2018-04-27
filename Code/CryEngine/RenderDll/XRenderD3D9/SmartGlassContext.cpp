// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:ISmartGlassManager.h
//
//	History:
//	-Jan 16,2013:Originally Created by Steve Barnett
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(SUPPORT_SMARTGLASS)

	#include "../Common/SmartGlassContext.h"

	#include "driverD3D.h"
	#include "../Common/Textures/Texture.h"
	#include "D3DPostProcess.h"

	#include "inspectable.h"
	#include <wrl/client.h>

	#include <CrySystem/Scaleform/IFlashPlayer.h>

using Windows::Foundation::TypedEventHandler;

// WinRT wrapper class required to receive callbacks from SmartGlass
ref class CSmartGlassInputListenerWrapper sealed
{
public:
	void Shutdown()
	{
		m_ctx = NULL;
	}

	void OnPointerPressed(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
	{
		if (m_ctx)
		{
			m_ctx->OnPointerPressed(sender, args);
		}
	}

	void OnPointerMoved(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
	{
		if (m_ctx)
		{
			m_ctx->OnPointerMoved(sender, args);
		}
	}

	void OnPointerReleased(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
	{
		if (m_ctx)
		{
			m_ctx->OnPointerReleased(sender, args);
		}
	}

	// Cannot have a standard type in the interface of a WinRT type, so constructor must be private and CSmartGlassContext declared as a friend
	friend class CSmartGlassContext;

private:
	CSmartGlassInputListenerWrapper(class CSmartGlassContext* ctx)
		: m_ctx(ctx)
	{
	}

	CSmartGlassContext* m_ctx;
};

CSmartGlassContext::CSmartGlassContext()
	: m_pCryTexture(NULL)
	, m_pFlashPlayer(NULL)
{
	m_eventQueue.reserve(EVENT_RESERVE_COUNT);
}

CSmartGlassContext::~CSmartGlassContext()
{
	m_pInputWrapper->Shutdown();
	m_pInputWrapper = nullptr;

	SAFE_RELEASE(m_pInputListener);

	if (m_pCryTexture)
	{
		delete m_pCryTexture->m_pDevTexture;
		m_pCryTexture->m_pDevTexture = m_pPrevDeviceTexture;
		m_pPrevDeviceTexture = NULL;

		m_pCryTexture->m_pDeviceRTV = m_pPrevRenderTargetView;
		m_pPrevRenderTargetView = NULL;

		SAFE_RELEASE(m_pCryTexture);
	}

	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pRenderTargetView);

	m_pSmartGlassDirectSurfaceNative = NULL;
	m_pDevice = NULL;
	m_pDeviceContext = NULL;
	if (m_pSmartGlassDevice)
	{
		SmartGlassDirectSurface^ pDirectSurface = m_pSmartGlassDevice->DirectSurface;
		pDirectSurface->PointerPressed -= m_pressedEventCookie;
		pDirectSurface->PointerMoved -= m_movedEventCookie;
		pDirectSurface->PointerReleased -= m_releasedEventCookie;
		m_pSmartGlassDevice = nullptr;
	}

	SAFE_RELEASE(m_pFlashPlayer);
}

void CSmartGlassContext::OnPointerPressed(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
{
	SInputEvent evt = { EIET_PRESSED, args->CurrentPoint->PointerId, args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y };
	m_eventQueueLock.Lock();
	m_eventQueue.push_back(evt);
	m_eventQueueLock.Unlock();
}

void CSmartGlassContext::OnPointerMoved(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
{
	SInputEvent evt = { EIET_MOVED, args->CurrentPoint->PointerId, args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y };
	m_eventQueueLock.Lock();
	m_eventQueue.push_back(evt);
	m_eventQueueLock.Unlock();
}

void CSmartGlassContext::OnPointerReleased(SmartGlassDirectSurface^ sender, PointerEventArgs^ args)
{
	SInputEvent evt = { EIET_RELEASED, args->CurrentPoint->PointerId, args->CurrentPoint->Position.X, args->CurrentPoint->Position.Y };
	m_eventQueueLock.Lock();
	m_eventQueue.push_back(evt);
	m_eventQueueLock.Unlock();
}

void CSmartGlassContext::SetDevice(SmartGlassDevice^ pDevice)
{
	m_pSmartGlassDevice = pDevice;
}

void CSmartGlassContext::SetFlashPlayer(IFlashPlayer* pFlashPlayer)
{
	if (m_pFlashPlayer != pFlashPlayer)
	{
		if (m_pFlashPlayer)
		{
			m_pFlashPlayer->Release();
		}
		pFlashPlayer->AddRef();
		pFlashPlayer->SetViewport(0, 0, m_screenWidth, m_screenHeight);
		pFlashPlayer->SetBackgroundAlpha(0.0f);
		m_pFlashPlayer = pFlashPlayer;
	}
}

void CSmartGlassContext::InitializeDevice(D3DDevice* pD3DDevice, D3DDeviceContext* pD3DDeviceContext)
{
	assert(m_pSmartGlassDevice);
	SmartGlassDirectSurface^ pDirectSurface = m_pSmartGlassDevice->DirectSurface;
	m_pSmartGlassDevice->SetActiveSurfaceAsync(pDirectSurface);

	IInspectable* pIns = reinterpret_cast<IInspectable*>(pDirectSurface);
	ISmartGlassDirectSurfaceNative* pDirectSurfaceNative;

	pIns->QueryInterface(IID_ISmartGlassDirectSurfaceNative, (void**)&pDirectSurfaceNative);

	m_pSmartGlassDirectSurfaceNative = pDirectSurfaceNative;
	m_screenHeight = (int)pDirectSurface->NativeBounds.Height;
	m_screenWidth = (int)pDirectSurface->NativeBounds.Width;
	m_pDevice = pD3DDevice;
	m_pDeviceContext = pD3DDeviceContext;

	m_pInputWrapper = ref new CSmartGlassInputListenerWrapper(this);
	// The syntax below stores a registration token, this is required to remove the event handler when cleaning up
	m_pressedEventCookie = pDirectSurface->PointerPressed += ref new TypedEventHandler<SmartGlassDirectSurface^, PointerEventArgs^>(m_pInputWrapper, &CSmartGlassInputListenerWrapper::OnPointerPressed);
	m_movedEventCookie = pDirectSurface->PointerMoved += ref new TypedEventHandler<SmartGlassDirectSurface^, PointerEventArgs^>(m_pInputWrapper, &CSmartGlassInputListenerWrapper::OnPointerMoved);
	m_releasedEventCookie = pDirectSurface->PointerReleased += ref new TypedEventHandler<SmartGlassDirectSurface^, PointerEventArgs^>(m_pInputWrapper, &CSmartGlassInputListenerWrapper::OnPointerReleased);
}

void CSmartGlassContext::SetSwapChain(IDXGISwapChain* pSwapChain)
{
	m_pSmartGlassDirectSurfaceNative->SetSwapChain(pSwapChain);
	Update();
}

void CD3D9Renderer::InitializeSmartGlassContext(class ISmartGlassContext* pContext)
{
	pContext->AddRef();
	CSmartGlassContext* pConcreteContext = reinterpret_cast<CSmartGlassContext*>(pContext);
	pConcreteContext->InitializeDevice(GetDevice(), GetDeviceContext());
	m_pRT->RC_CreateSmartGlassSwapChain(pConcreteContext);
}

void CD3D9Renderer::UpdateSmartGlassDevice(class ISmartGlassContext* pContext)
{
	pContext->AddRef();
	m_pRT->RC_UpdateSmartGlassDevice(pContext);
}

void CD3D9Renderer::RT_RequestRenderSmartGlassContext(class ISmartGlassContext* pContext)
{
	if (m_pRT->m_eVideoThreadMode == SRenderThread::eVTM_Disabled)
	{
		pContext->AddRef();
		m_renderSmartGlassContexts.push_back(pContext);
	}
}

void CD3D9Renderer::RT_RenderForSmartGlass(CTexture* pTexBackbuffer)
{
	std::vector<ISmartGlassContext*>::iterator end = m_renderSmartGlassContexts.end();
	for (std::vector<ISmartGlassContext*>::iterator it = m_renderSmartGlassContexts.begin(); it != end; ++it)
	{
		(*it)->RT_Render(pTexBackbuffer);
		(*it)->Release();
	}
	m_renderSmartGlassContexts.clear();
}

void CSmartGlassContext::CreateSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	swapChainDesc.BufferDesc.Width = m_screenWidth;
	swapChainDesc.BufferDesc.Height = m_screenHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // this is the most common swapchain format
	swapChainDesc.SampleDesc.Count = 1;                           // don't use multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;                       // use two buffers to enable flip effect
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // we recommend using this swap effect for all applications
	swapChainDesc.Flags = 0;
	swapChainDesc.Windowed = TRUE;

	// Once the desired swap chain pDescription is configured, it must be created on the same adapter as our D3D Device

	// First, retrieve the underlying DXGI Device from the D3D Device
	Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
	Microsoft::WRL::ComPtr<D3DDevice> pComDevice = m_pDevice;
	pComDevice.As(&dxgiDevice);

	// Ensure that DXGI does not queue more than one frame at a time. This both reduces
	// latency and ensures that the application will only render after each VSync, minimizing
	// power consumption.
	dxgiDevice->SetMaximumFrameLatency(1);

	CreateSwapChainForSmartGlassDevice(m_pDevice, &swapChainDesc, &m_pSwapChain);

	// Obtain the backbuffer for this window which will be the final 3D rendertarget.
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);

	// Create a view interface on the rendertarget to use on bind.
	m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_pRenderTargetView);

	// Cache the rendertarget dimensions in our helper class for convenient use.
	D3D11_TEXTURE2D_DESC backBufferDesc = { 0 };
	backBuffer->GetDesc(&backBufferDesc);

	// initialize the viewport descriptor of the companion size.
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Height = static_cast<float>(backBufferDesc.Height);
	m_viewport.Width = static_cast<float>(backBufferDesc.Width);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_pCryTexture = CTexture::GetOrCreateTextureObject("$SmartGlass", (uint32)m_viewport.Width, (uint32)m_viewport.Height, 1, eTT_2D, FT_DONT_RELEASE | FT_DONT_STREAM | FT_USAGE_RENDERTARGET, eTF_Unknown, TO_SMARTGLASS);
	bool bRes = m_pCryTexture->CreateRenderTarget(eTF_R8G8B8A8, Clr_Empty); //
	m_pPrevRenderTargetView = reinterpret_cast<D3DSurface*>(m_pCryTexture->m_pDeviceRTV);
	m_pPrevDeviceTexture = m_pCryTexture->m_pDevTexture;
	m_pCryTexture->m_pDeviceRTV = m_pRenderTargetView;
	m_pCryTexture->m_pDevTexture = new CDeviceTexture(backBuffer.Get());
	SetSwapChain(m_pSwapChain);
}

void CSmartGlassContext::Update()
{
	gcpRendD3D->RT_RequestRenderSmartGlassContext(this);
}

void CSmartGlassContext::RT_Render(CTexture* pTexBackbuffer)
{
	gcpRendD3D->FX_PushRenderTarget(0, m_pCryTexture, NULL);
	if (pTexBackbuffer)
	{
		PostProcessUtils().CopyTextureToScreen(pTexBackbuffer);
	}
	if (m_pFlashPlayer)
	{
		m_pFlashPlayer->Render(false);
	}
	gcpRendD3D->FX_PopRenderTarget(0);
	m_pSmartGlassDirectSurfaceNative->CaptureFrame();
}

void CSmartGlassContext::SendInputEvents()
{
	m_eventQueueLock.Lock();
	InputEventVec events;
	events.swap(m_eventQueue);
	m_eventQueueLock.Unlock();

	InputEventVec::iterator end = events.end();
	for (InputEventVec::iterator it = events.begin(); it != end; ++it)
	{
		switch (it->type)
		{
		case EIET_PRESSED:
			{
				m_pInputListener->OnPressed(it->pointerId, it->x, it->y);
			} break;
		case EIET_MOVED:
			{
				m_pInputListener->OnMoved(it->pointerId, it->x, it->y);
			} break;
		case EIET_RELEASED:
			{
				m_pInputListener->OnReleased(it->pointerId, it->x, it->y);
			} break;
		default:
			{
				CryFatalError("Unexpected SmartGlass input event type.");
			} break;
		}
	}

	if (m_pFlashPlayer)
	{
		m_pFlashPlayer->Advance(gEnv->pTimer->GetFrameTime());
	}
}

#endif
