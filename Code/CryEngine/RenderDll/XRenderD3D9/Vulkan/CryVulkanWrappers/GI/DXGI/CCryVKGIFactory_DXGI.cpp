// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if VK_USE_DXGI

#include "CCryVKGIFactory_DXGI.hpp"
#include "CCryVKGIAdapter_DXGI.hpp"

CCryVKGIFactory_DXGI* CCryVKGIFactory_DXGI::Create()
{
	typedef HRESULT(WINAPI * FP_CreateDXGIFactory1)(REFIID, void**);
	FP_CreateDXGIFactory1 pCreateDXGIFactory1 = (FP_CreateDXGIFactory1)GetProcAddress(LoadLibraryA("dxgi.dll"), "CreateDXGIFactory1");

	IDXGIFactoryToCall* pNativeFactoryRaw;
	if (pCreateDXGIFactory1(__uuidof(IDXGIFactoryToCall), (void**)&pNativeFactoryRaw) == S_OK)
	{
		_smart_ptr<IDXGIFactoryToCall> pNativeFactory;
		pNativeFactory.Assign_NoAddRef(pNativeFactoryRaw);

		auto factory = new CCryVKGIFactory_DXGI(pNativeFactory);
		if (factory->GetVkInstance() == VK_NULL_HANDLE) {
			delete factory;
			factory = nullptr;
		}
		return factory;
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKGIFactory_DXGI::CCryVKGIFactory_DXGI(_smart_ptr<IDXGIFactoryToCall> pNativeDxgiFactory)
	: m_pNativeDxgiFactory(std::move(pNativeDxgiFactory))
{
}

HRESULT CCryVKGIFactory_DXGI::EnumAdapters1(
	UINT Adapter,
	_Out_ IDXGIAdapter1** ppAdapter)
{

	IDXGIAdapterToCall* pNativeAdapterRaw;
	HRESULT hr = m_pNativeDxgiFactory->EnumAdapters1(Adapter, &pNativeAdapterRaw);

	if (hr == S_OK)
	{
		_smart_ptr<IDXGIAdapterToCall> pNativedapter;
		pNativedapter.Assign_NoAddRef(pNativeAdapterRaw);

		*ppAdapter = CCryVKGIAdapter_DXGI::Create(this, pNativedapter, Adapter);
	}

	return hr;
}

#endif