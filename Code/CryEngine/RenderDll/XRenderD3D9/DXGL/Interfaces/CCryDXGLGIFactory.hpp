// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIFactory.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for IDXGIFactory
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLGIFACTORY__
#define __CRYDXGLGIFACTORY__

#include "CCryDXGLGIObject.hpp"

class CCryDXGLGIAdapter;

class CCryDXGLGIFactory : public CCryDXGLGIObject
{
public:
#if DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIFactory, DXGIFactory)
#endif //DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIFactory, DXGIFactory1)

	CCryDXGLGIFactory();
	~CCryDXGLGIFactory();
	bool Initialize();

	// IDXGIFactory implementation
	HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter** ppAdapter);
	HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags);
	HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* pWindowHandle);
	HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);
	HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter** ppAdapter);

	// IDXGIFactory1 implementation
	HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT Adapter, IDXGIAdapter1** ppAdapter);
	BOOL STDMETHODCALLTYPE    IsCurrent();
protected:
	typedef std::vector<_smart_ptr<CCryDXGLGIAdapter>> Adapters;
protected:
	// The adapters available on this system
	Adapters m_kAdapters;
	HWND     m_hWindowHandle;
};

#endif //__CRYDXGLGIFACTORY__
