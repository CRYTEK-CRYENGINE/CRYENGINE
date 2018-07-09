// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12GIFactory.hpp"
#include "CCryDX12GIAdapter.hpp"
#include "CCryDX12SwapChain.hpp"

#include "DX12/Device/CCryDX12Device.hpp"
#include "DX12/Device/CCryDX12DeviceContext.hpp"

#include "DX12/API/DX12SwapChain.hpp"

CCryDX12GIFactory* CCryDX12GIFactory::Create()
{
	IDXGIFactory4ToCall* pDXGIFactory4 = nullptr;

#if CRY_PLATFORM_DESKTOP
	if (S_OK != CreateDXGIFactory1(IID_GFX_ARGS(&pDXGIFactory4)))
#endif
	{
		DX12_ASSERT("Failed to create underlying DXGI factory!");
		return nullptr;
	}

	return DX12_NEW_RAW(CCryDX12GIFactory(pDXGIFactory4));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12GIFactory::CCryDX12GIFactory(IDXGIFactory4ToCall* pDXGIFactory4)
	: Super()
	, m_pDXGIFactory4(pDXGIFactory4)
{
	DX12_FUNC_LOG

}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::EnumAdapters(UINT Adapter, _Out_ IDXGIAdapter** ppAdapter)
{
	DX12_FUNC_LOG
	* ppAdapter = CCryDX12GIAdapter::Create(this, Adapter);
	return *ppAdapter ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags)
{
	DX12_FUNC_LOG
	return m_pDXGIFactory4->MakeWindowAssociation(WindowHandle, Flags);
}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::GetWindowAssociation(_Out_ HWND* pWindowHandle)
{
	DX12_FUNC_LOG
	return m_pDXGIFactory4->GetWindowAssociation(pWindowHandle);
}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::CreateSwapChain(_In_ IGfxUnknown* pDevice, _In_ DXGI_SWAP_CHAIN_DESC* pDesc, _Out_ IDXGISwapChain** ppSwapChain)
{
	DX12_FUNC_LOG
	* ppSwapChain = CCryDX12SwapChain::Create(static_cast<CCryDX12Device*>(pDevice), m_pDXGIFactory4, pDesc);
	return *ppSwapChain ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::CreateSoftwareAdapter(HMODULE Module, _Out_ IDXGIAdapter** ppAdapter)
{
	DX12_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryDX12GIFactory::EnumAdapters1(UINT Adapter, _Out_ IDXGIAdapter1** ppAdapter)
{
	DX12_FUNC_LOG
	* ppAdapter = CCryDX12GIAdapter::Create(this, Adapter);
	return *ppAdapter ? S_OK : E_FAIL;
}

BOOL STDMETHODCALLTYPE CCryDX12GIFactory::IsCurrent()
{
	DX12_FUNC_LOG
	return m_pDXGIFactory4->IsCurrent();
}
