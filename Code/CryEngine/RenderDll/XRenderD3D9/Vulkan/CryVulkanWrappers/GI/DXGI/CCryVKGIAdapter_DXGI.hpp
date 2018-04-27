// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if VK_USE_DXGI

#include "..\CCryVKGIAdapter.hpp"

class CCryVKGIAdapter_DXGI : public CCryVKGIAdapter
{
public:
	static CCryVKGIAdapter_DXGI* Create(CCryVKGIFactory* factory, _smart_ptr<IDXGIAdapterToCall> pNativeDxgiAdapter, uint index);

	virtual HRESULT STDMETHODCALLTYPE EnumOutputs(
		/* [in] */ UINT Output,
		/* [annotation][out][in] */
		_COM_Outptr_ IDXGIOutput** ppOutput);

	virtual HRESULT STDMETHODCALLTYPE GetDesc1(
		_Out_ DXGI_ADAPTER_DESC1* pDesc);

protected:
	CCryVKGIAdapter_DXGI(CCryVKGIFactory* pFactory, _smart_ptr<IDXGIAdapterToCall> pNativeDxgiAdapter, UINT index);

private:
	_smart_ptr<IDXGIAdapterToCall> m_pNativeDxgiAdapter;
};

#endif