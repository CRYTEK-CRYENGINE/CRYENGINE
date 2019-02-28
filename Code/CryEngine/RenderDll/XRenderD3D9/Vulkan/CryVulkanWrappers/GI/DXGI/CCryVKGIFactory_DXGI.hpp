// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if VK_USE_DXGI

#include "..\CCryVKGIFactory.hpp"

class CCryVKGIFactory_DXGI : public CCryVKGIFactory
{
public:
	CCryVKGIFactory_DXGI(_smart_ptr<IDXGIFactoryToCall> pNativeDxgiFactory);
	static CCryVKGIFactory_DXGI* Create();

	virtual HRESULT STDMETHODCALLTYPE EnumAdapters1(
		UINT Adapter,
		_Out_ IDXGIAdapter1** ppAdapter);

protected:
	_smart_ptr<IDXGIFactoryToCall> m_pNativeDxgiFactory;
};

#endif