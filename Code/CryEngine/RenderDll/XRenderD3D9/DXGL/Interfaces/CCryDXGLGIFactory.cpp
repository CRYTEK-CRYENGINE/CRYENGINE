// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIFactory.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for IDXGIFactory
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLGIAdapter.hpp"
#include "CCryDXGLGIFactory.hpp"
#include "CCryDXGLSwapChain.hpp"
#include "../Interfaces/CCryDXGLDevice.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLGIFactory::CCryDXGLGIFactory()
{
	DXGL_INITIALIZE_INTERFACE(DXGIFactory)
	DXGL_INITIALIZE_INTERFACE(DXGIFactory1)
}

CCryDXGLGIFactory::~CCryDXGLGIFactory()
{
}

bool CCryDXGLGIFactory::Initialize()
{
	std::vector<NCryOpenGL::SAdapterPtr> kAdapters;
	if (!NCryOpenGL::DetectAdapters(kAdapters))
		return false;

	uint32 uAdapter;
	for (uAdapter = 0; uAdapter < kAdapters.size(); ++uAdapter)
	{
		_smart_ptr<CCryDXGLGIAdapter> spAdapter(new CCryDXGLGIAdapter(this, kAdapters.at(uAdapter).get()));
		if (!spAdapter->Initialize())
			return false;
		m_kAdapters.push_back(spAdapter);
	}

	return true;
}

template<typename AdapterInterface>
HRESULT EnumAdaptersInternal(UINT Adapter, AdapterInterface** ppAdapter, const std::vector<_smart_ptr<CCryDXGLGIAdapter>>& kAdapters)
{
	if (Adapter < kAdapters.size())
	{
		CCryDXGLGIAdapter::ToInterface(ppAdapter, kAdapters.at(Adapter));
		return S_OK;
	}
	*ppAdapter = NULL;
	return DXGI_ERROR_NOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////
// IDXGIFactory implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLGIFactory::EnumAdapters(UINT Adapter, IDXGIAdapter** ppAdapter)
{
	return EnumAdaptersInternal(Adapter, ppAdapter, m_kAdapters);
}

HRESULT CCryDXGLGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags)
{
	DXGL_TODO("Implement ALT+ENTER handling in OpenGL if required")
	Flags;

	m_hWindowHandle = WindowHandle;
	return S_OK;
}

HRESULT CCryDXGLGIFactory::GetWindowAssociation(HWND* pWindowHandle)
{
	*pWindowHandle = m_hWindowHandle;
	return S_OK;
}

HRESULT CCryDXGLGIFactory::CreateSwapChain(IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
#if DXGL_FULL_EMULATION
	void* pvD3D11Device;
	if (FAILED(pDevice->QueryInterface(__uuidof(ID3D11Device), &pvD3D11Device)) || pvD3D11Device == NULL)
	{
		DXGL_ERROR("CCryDXGLGIFactory::CreateSwapChain - device type is not compatible with swap chain creation");
		return E_FAIL;
	}
	CCryDXGLDevice* pDXGLDevice(CCryDXGLDevice::FromInterface(static_cast<ID3D11Device*>(pvD3D11Device)));

	_smart_ptr<CCryDXGLSwapChain> spSwapChain(new CCryDXGLSwapChain(pDXGLDevice, *pDesc));
	if (!spSwapChain->Initialize())
		return false;
	CCryDXGLSwapChain::ToInterface(ppSwapChain, spSwapChain);
	spSwapChain->AddRef();

	return S_OK;
#else
	DXGL_ERROR("CCryDXGLGIFactory::CreateSwapChain is not supported, use D3D11CreateDeviceAndSwapChain");
	return E_FAIL;
#endif
}

HRESULT CCryDXGLGIFactory::CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter** ppAdapter)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////
// IDXGIFactory1 implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLGIFactory::EnumAdapters1(UINT Adapter, IDXGIAdapter1** ppAdapter)
{
	return EnumAdaptersInternal(Adapter, ppAdapter, m_kAdapters);
}

BOOL CCryDXGLGIFactory::IsCurrent()
{
	DXGL_NOT_IMPLEMENTED
	return false;
}
