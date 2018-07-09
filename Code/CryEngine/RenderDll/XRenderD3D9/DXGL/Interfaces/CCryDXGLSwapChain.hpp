// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSwapChain.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for IDXGISwapChain
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLSWAPCHAIN__
#define __CRYDXGLSWAPCHAIN__

#include "CCryDXGLBase.hpp"
#include "CCryDXGLGIObject.hpp"

namespace NCryOpenGL
{
class CDeviceContextProxy;
}

class CCryDXGLDevice;
class CCryDXGLTexture2D;

class CCryDXGLSwapChain : public CCryDXGLGIObject
{
public:
#if DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLSwapChain, DXGIDeviceSubObject)
#endif //DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLSwapChain, DXGISwapChain)

	CCryDXGLSwapChain(CCryDXGLDevice* pDevice, const DXGI_SWAP_CHAIN_DESC& kDesc);
	~CCryDXGLSwapChain();

	bool Initialize();

	// IDXGISwapChain implementation
	HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags);
	HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface);
	HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget);
	HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget);
	HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc);
	HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
	HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters);
	HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput);
	HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats);
	HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount);

	// IDXGIDeviceSubObject implementation
	HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) { return E_NOTIMPL; }
protected:
	bool                      UpdateTexture(bool bSetPixelFormat);
protected:
	_smart_ptr<CCryDXGLDevice>    m_spDevice;
	_smart_ptr<CCryDXGLTexture2D> m_spBackBufferTexture;
	DXGI_SWAP_CHAIN_DESC          m_kDesc;
};

#endif //__CRYDXGLSWAPCHAIN__
