// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

class CSwapChain
{
private:
	DXGISwapChain*           m_pSwapChain = nullptr;
	DXGI_SURFACE_DESC        m_swapChainDesc;

#if defined(SUPPORT_DEVICE_INFO)
	uint32                   m_refreshRateNumerator = 0;
	uint32                   m_refreshRateDenominator = 0;
#endif

	CSwapChain(DXGISwapChain* pSwapChain) : m_pSwapChain(pSwapChain) { ReadSwapChainSurfaceDesc(); }

	void ReadSwapChainSurfaceDesc();

public:
	CSwapChain() = default;
	~CSwapChain()
	{
		if (m_pSwapChain)
			SAFE_RELEASE(m_pSwapChain);
	}

	CSwapChain(CSwapChain &&o) noexcept
		: m_pSwapChain(o.m_pSwapChain)
		, m_swapChainDesc(std::move(o.m_swapChainDesc))
#if defined(SUPPORT_DEVICE_INFO)
		, m_refreshRateNumerator(o.m_refreshRateNumerator)
		, m_refreshRateDenominator(o.m_refreshRateDenominator)
#endif
	{
		o.m_pSwapChain = nullptr;
	}
	CSwapChain &operator=(CSwapChain &&o) noexcept
	{
		m_pSwapChain = o.m_pSwapChain;
		m_swapChainDesc = std::move(o.m_swapChainDesc);
#if defined(SUPPORT_DEVICE_INFO)
		m_refreshRateNumerator = o.m_refreshRateNumerator;
		m_refreshRateDenominator = o.m_refreshRateDenominator;
#endif

		o.m_pSwapChain = nullptr;

		return *this;
	}

	void                     ResizeSwapChain(uint32_t width, uint32_t height, bool isMainContext, bool bResizeTarget = false);
#if !CRY_PLATFORM_ORBIS && !CRY_PLATFORM_DURANGO
	void                     SetFullscreenState(bool isFullscreen, IDXGIOutput *output = nullptr)
	{
		m_pSwapChain->SetFullscreenState(isFullscreen, output);
	}
#endif

#ifdef CRY_RENDERER_GNM
	HRESULT                  Present(CGnmGraphicsCommandList* pCommandList, CGnmSwapChain::EFlipMode flipMode);
#else
	HRESULT                  Present(unsigned int syncInterval, unsigned int flags);
#endif

	DXGISwapChain*           GetSwapChain() const     { return m_pSwapChain; }
	const DXGI_SURFACE_DESC& GetSwapChainDesc() const { return m_swapChainDesc; }
#if defined(SUPPORT_DEVICE_INFO)
	uint32                   GetRefreshRateNumerator() const { return m_refreshRateNumerator; }
	uint32                   GetRefreshRateDemoninator() const { return m_refreshRateDenominator; }
#endif

public:
#if CRY_PLATFORM_DURANGO
#if (CRY_RENDERER_DIRECT3D >= 120)
	static CSwapChain        CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D12Device* pD3D12Device, CCryDX12Device* pDX12Device, uint32_t width, uint32_t height);
#else
	static CSwapChain        CreateXboxSwapChain(IDXGIFactory2ToCall* pDXGIFactory, ID3D11Device* pD3D11Device, uint32_t width, uint32_t height);
#endif
#elif CRY_RENDERER_GNM
	static CSwapChain        CreateGNMSwapChain(uint32_t width, uint32_t height);
#else
	static CSwapChain        CreateSwapChain(HWND hWnd, DXGIOutput* pOutput, uint32_t width, uint32_t height, bool isMainContext, bool isFullscreen);
#endif

#if defined(SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES)
	static void UserOverrideDisplayProperties(DXGI_MODE_DESC& desc);
#endif
};
