// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSwapChain.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for IDXGISwapChain
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLDevice.hpp"
#include "CCryDXGLGIOutput.hpp"
#include "CCryDXGLSwapChain.hpp"
#include "CCryDXGLTexture2D.hpp"
#include "../Implementation/GLDevice.hpp"
#include "../Implementation/GLContext.hpp"
#include "../Implementation/GLResource.hpp"

CCryDXGLSwapChain::CCryDXGLSwapChain(CCryDXGLDevice* pDevice, const DXGI_SWAP_CHAIN_DESC& kDesc)
	: m_spDevice(pDevice)
{
	DXGL_INITIALIZE_INTERFACE(DXGIDeviceSubObject)
	DXGL_INITIALIZE_INTERFACE(DXGISwapChain)

	m_kDesc = kDesc;
}

CCryDXGLSwapChain::~CCryDXGLSwapChain()
{
}

bool CCryDXGLSwapChain::Initialize()
{
	if (!m_kDesc.Windowed &&
	    FAILED(SetFullscreenState(TRUE, NULL)))
		return false;

	return UpdateTexture(true);
}

bool CCryDXGLSwapChain::UpdateTexture(bool bSetPixelFormat)
{
	// Create a dummy texture that represents the default back buffer
	D3D11_TEXTURE2D_DESC kBackBufferDesc;
	kBackBufferDesc.Width = m_kDesc.BufferDesc.Width;
	kBackBufferDesc.Height = m_kDesc.BufferDesc.Height;
	kBackBufferDesc.MipLevels = 1;
	kBackBufferDesc.ArraySize = 1;
	kBackBufferDesc.Format = m_kDesc.BufferDesc.Format;
	kBackBufferDesc.SampleDesc = m_kDesc.SampleDesc;
	kBackBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	kBackBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET; // Default back buffer can only be bound as render target
	kBackBufferDesc.CPUAccessFlags = 0;
	kBackBufferDesc.MiscFlags = 0;

	NCryOpenGL::SDefaultFrameBufferTexturePtr spBackBufferTex(NCryOpenGL::CreateBackBufferTexture(kBackBufferDesc));
	m_spBackBufferTexture = new CCryDXGLTexture2D(kBackBufferDesc, spBackBufferTex, m_spDevice);

#if DXGL_FULL_EMULATION
	if (bSetPixelFormat)
	{
		NCryOpenGL::CDevice* pDevice(m_spDevice->GetGLDevice());
		NCryOpenGL::TNativeDisplay kNativeDisplay(NULL);
		NCryOpenGL::TWindowContext kCustomWindowContext(NULL);
		if (!NCryOpenGL::GetNativeDisplay(kNativeDisplay, m_kDesc.OutputWindow) ||
		    !NCryOpenGL::CreateWindowContext(kCustomWindowContext, pDevice->GetFeatureSpec(), pDevice->GetPixelFormatSpec(), kNativeDisplay))
			return false;

		spBackBufferTex->SetCustomWindowContext(kCustomWindowContext);
	}
#endif //DXGL_FULL_EMULATION

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// IDXGISwapChain implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLSwapChain::Present(UINT SyncInterval, UINT Flags)
{
	NCryOpenGL::CDevice* pDevice(m_spDevice->GetGLDevice());
#if OGL_SINGLE_CONTEXT
	NCryOpenGL::CContext* pContext(pDevice->GetCurrentContext());
#else
	NCryOpenGL::CContext* pContext(pDevice->ReserveContext());
#endif
	if (pContext == NULL)
		return E_FAIL;

	NCryOpenGL::SDefaultFrameBufferTexture* pGLBackBufferTexture(static_cast<NCryOpenGL::SDefaultFrameBufferTexture*>(m_spBackBufferTexture->GetGLTexture()));

#if DXGL_FULL_EMULATION
	const NCryOpenGL::TWindowContext& kWindowContext(
	  pGLBackBufferTexture->m_kCustomWindowContext != NULL ?
	  pGLBackBufferTexture->m_kCustomWindowContext :
	  pDevice->GetDefaultWindowContext());
	pContext->SetWindowContext(kWindowContext);
#else
	const NCryOpenGL::TWindowContext& kWindowContext(pDevice->GetDefaultWindowContext());
#endif

	pGLBackBufferTexture->Flush(pContext);
	HRESULT kResult(pDevice->Present(kWindowContext) ? S_OK : E_FAIL);

#if !OGL_SINGLE_CONTEXT
	pDevice->ReleaseContext();
#endif
	return kResult;
}

HRESULT CCryDXGLSwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface)
{
	if (Buffer == 0 && riid == __uuidof(ID3D11Texture2D))
	{
		m_spBackBufferTexture->AddRef();
		CCryDXGLTexture2D::ToInterface(reinterpret_cast<ID3D11Texture2D**>(ppSurface), m_spBackBufferTexture.get());
		return S_OK;
	}
	DXGL_TODO("Support more than one swap chain buffer if required");
	return E_FAIL;
}

HRESULT CCryDXGLSwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget)
{
	NCryOpenGL::SFrameBufferSpec kFrameBufferSpec;
	if (!SwapChainDescToFrameBufferSpec(kFrameBufferSpec, m_kDesc))
		return E_FAIL;

	NCryOpenGL::SOutput* pGLOutput(pTarget == NULL ? NULL : CCryDXGLGIOutput::FromInterface(pTarget)->GetGLOutput());
	return m_spDevice->GetGLDevice()->SetFullScreenState(kFrameBufferSpec, Fullscreen == TRUE, pGLOutput) ? S_OK : E_FAIL;
}

HRESULT CCryDXGLSwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget)
{
	DXGL_NOT_IMPLEMENTED;
	return E_FAIL;
}

HRESULT CCryDXGLSwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
	return S_OK;
}

HRESULT CCryDXGLSwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags)
{
	if (
	  Format == m_kDesc.BufferDesc.Format &&
	  Width == m_kDesc.BufferDesc.Width &&
	  Height == m_kDesc.BufferDesc.Height &&
	  BufferCount == m_kDesc.BufferCount &&
	  SwapChainFlags == m_kDesc.Flags)
		return S_OK; // Nothing to do

	if (BufferCount == m_kDesc.BufferCount)
	{
		m_kDesc.BufferDesc.Format = Format;
		m_kDesc.BufferDesc.Width = Width;
		m_kDesc.BufferDesc.Height = Height;
		m_kDesc.Flags = SwapChainFlags;

		if (UpdateTexture(false))
			return S_OK;
	}

	return E_FAIL;
}

HRESULT CCryDXGLSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
{
	NCryOpenGL::SDisplayMode kDisplayMode;
	if (!NCryOpenGL::GetDisplayMode(&kDisplayMode, *pNewTargetParameters) ||
	    m_spDevice->GetGLDevice()->ResizeTarget(kDisplayMode))
		return E_FAIL;
	return S_OK;
}

HRESULT CCryDXGLSwapChain::GetContainingOutput(IDXGIOutput** ppOutput)
{
	DXGL_NOT_IMPLEMENTED;
	return E_FAIL;
}

HRESULT CCryDXGLSwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)
{
	DXGL_NOT_IMPLEMENTED;
	return E_FAIL;
}

HRESULT CCryDXGLSwapChain::GetLastPresentCount(UINT* pLastPresentCount)
{
	DXGL_NOT_IMPLEMENTED;
	return E_FAIL;
}
