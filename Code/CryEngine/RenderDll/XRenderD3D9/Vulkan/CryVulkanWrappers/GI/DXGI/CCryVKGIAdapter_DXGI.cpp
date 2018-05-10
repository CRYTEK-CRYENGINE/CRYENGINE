// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if VK_USE_DXGI

#include "CCryVKGIAdapter_DXGI.hpp"
#include "CCryVKGIFactory_DXGI.hpp"
#include "CCryVKGIOutput_DXGI.hpp"

CCryVKGIAdapter_DXGI* CCryVKGIAdapter_DXGI::Create(CCryVKGIFactory* factory, _smart_ptr<IDXGIAdapterToCall> pNativeDxgiAdapter, uint index)
{
	if (pNativeDxgiAdapter)
	{
		return new CCryVKGIAdapter_DXGI(factory, pNativeDxgiAdapter, index);
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKGIAdapter_DXGI::CCryVKGIAdapter_DXGI(CCryVKGIFactory* pFactory, _smart_ptr<IDXGIAdapterToCall> pNativeDxgiAdapter, uint index)
	: CCryVKGIAdapter(pFactory, index)
	, m_pNativeDxgiAdapter(std::move(pNativeDxgiAdapter))
{}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT CCryVKGIAdapter_DXGI::EnumOutputs(UINT Output, _COM_Outptr_ IDXGIOutput** ppOutput)
{
	VK_FUNC_LOG();

	IDXGIOutputToCall* pNativeOutputRaw;
	HRESULT hr = m_pNativeDxgiAdapter->EnumOutputs(Output, &pNativeOutputRaw);

	if (hr== S_OK)
	{
		_smart_ptr<IDXGIOutputToCall> pNativeOutput;
		pNativeOutput.Assign_NoAddRef(pNativeOutputRaw);

		*ppOutput = CCryVKGIOutput_DXGI::Create(this, pNativeOutput, Output);
	}

	return hr;
}

HRESULT CCryVKGIAdapter_DXGI::GetDesc1(
	_Out_ DXGI_ADAPTER_DESC1* pDesc)
{
	return m_pNativeDxgiAdapter->GetDesc1(pDesc);
}

#endif