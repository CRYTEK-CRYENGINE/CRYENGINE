// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryVKGIFactory.hpp"
#include "CCryVKGIAdapter.hpp"
#include "CCryVKSwapChain.hpp"
#include "../../API/VKInstance.hpp"


CCryVKGIFactory* CCryVKGIFactory::Create()
{
	return new CCryVKGIFactory;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKGIFactory::CCryVKGIFactory()
{
	VK_FUNC_LOG();
	auto pInstance = stl::make_unique<NCryVulkan::CInstance>();
	if (pInstance->Initialize("appName", 1, "CryEngine", 1))
	{
		m_pInstance = std::move(pInstance);
	}
}

CCryVKGIFactory::~CCryVKGIFactory()
{
	VK_FUNC_LOG();
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::EnumAdapters(UINT Adapter, _Out_ IDXGIAdapter** ppAdapter)
{
	VK_FUNC_LOG();
	return EnumAdapters1(Adapter, ppAdapter);
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags)
{
	VK_FUNC_LOG();
	return S_OK;  //m_pDXGIFactory4->MakeWindowAssociation(WindowHandle, Flags);
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::GetWindowAssociation(_Out_ HWND* pWindowHandle)
{
	VK_FUNC_LOG();
	return S_OK;  //m_pDXGIFactory4->GetWindowAssociation(pWindowHandle);
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::CreateSwapChain(_In_ IUnknown* pDevice, _In_ DXGI_SWAP_CHAIN_DESC* pDesc, _Out_ IDXGISwapChain** ppSwapChain)
{
	VK_FUNC_LOG();

	*ppSwapChain = CCryVKSwapChain::Create(static_cast<D3DDevice*>(pDevice), pDesc);
	return *ppSwapChain ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::CreateSoftwareAdapter(HMODULE Module, _Out_ IDXGIAdapter** ppAdapter)
{
	VK_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT STDMETHODCALLTYPE CCryVKGIFactory::EnumAdapters1(UINT Adapter, _Out_ IDXGIAdapter1** ppAdapter)
{
	VK_FUNC_LOG();

	if (Adapter < m_pInstance->GetPhysicalDeviceCount())
	{
		*ppAdapter = CCryVKGIAdapter::Create(this, Adapter);
		return S_OK;
	}
	else
	{
		return DXGI_ERROR_NOT_FOUND;
	}
}

BOOL STDMETHODCALLTYPE CCryVKGIFactory::IsCurrent()
{
	VK_FUNC_LOG();
	return 1;  //m_pDXGIFactory4->IsCurrent();
}
