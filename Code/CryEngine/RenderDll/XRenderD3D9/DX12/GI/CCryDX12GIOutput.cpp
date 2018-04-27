// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12GIOutput.hpp"

#include "CCryDX12GIAdapter.hpp"

CCryDX12GIOutput* CCryDX12GIOutput::Create(CCryDX12GIAdapter* pAdapter, UINT Output)
{
	IDXGIOutputToCall* pOutput;
	if (!SUCCEEDED(pAdapter->GetDXGIAdapter()->EnumOutputs(Output, &pOutput)))
		return nullptr;

	IDXGIOutput4ToCall* pOutput4;
	pOutput->QueryInterface(__uuidof(IDXGIOutput4ToCall), (void**)&pOutput4);

	return pOutput4 ? DX12_NEW_RAW(CCryDX12GIOutput(pAdapter, pOutput4)) : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12GIOutput::CCryDX12GIOutput(CCryDX12GIAdapter* pAdapter, IDXGIOutput4ToCall* pOutput)
	: Super()
	, m_pAdapter(pAdapter)
	, m_pDXGIOutput4(pOutput)
{
	DX12_FUNC_LOG
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __d3d11_x_h__
HRESULT STDMETHODCALLTYPE CCryDX12GIOutput::FindClosestMatchingMode(_In_ const DXGI_MODE_DESC* pModeToMatch, _Out_ DXGI_MODE_DESC* pClosestMatch, _In_opt_ IUnknown* pConcernedDevice)
{
	return m_pDXGIOutput4->FindClosestMatchingMode(pModeToMatch, pClosestMatch, pConcernedDevice ? reinterpret_cast<IUnknown*>(static_cast<CCryDX12Device*>(pConcernedDevice)->GetD3D12Device()) : nullptr);
}

HRESULT STDMETHODCALLTYPE CCryDX12GIOutput::TakeOwnership(_In_ IUnknown* pDevice, BOOL Exclusive)
{
	return m_pDXGIOutput4->TakeOwnership(pDevice ? reinterpret_cast<IUnknown*>(static_cast<CCryDX12Device*>(pDevice)->GetD3D12Device()) : nullptr, Exclusive);
}
#endif

#ifdef __dxgi1_2_h__
HRESULT STDMETHODCALLTYPE CCryDX12GIOutput::FindClosestMatchingMode1(_In_ const DXGI_MODE_DESC1* pModeToMatch, _Out_ DXGI_MODE_DESC1* pClosestMatch, _In_opt_ IUnknown* pConcernedDevice)
{
	return m_pDXGIOutput4->FindClosestMatchingMode1(pModeToMatch, pClosestMatch, pConcernedDevice ? reinterpret_cast<IUnknown*>(static_cast<CCryDX12Device*>(pConcernedDevice)->GetD3D12Device()) : nullptr);
}
#endif

#ifdef __dxgi1_3_h__
HRESULT STDMETHODCALLTYPE CCryDX12GIOutput::CheckOverlaySupport(_In_ DXGI_FORMAT EnumFormat, _In_ IUnknown* pConcernedDevice, _Out_ UINT* pFlags)
{
	return m_pDXGIOutput4->CheckOverlaySupport(EnumFormat, reinterpret_cast<IUnknown*>(static_cast<CCryDX12Device*>(pConcernedDevice)->GetD3D12Device()), pFlags);
}
#endif

#ifdef __dxgi1_4_h__
HRESULT STDMETHODCALLTYPE CCryDX12GIOutput::CheckOverlayColorSpaceSupport(_In_ DXGI_FORMAT Format, _In_ DXGI_COLOR_SPACE_TYPE ColorSpace, _In_ IUnknown* pConcernedDevice, _Out_ UINT* pFlags)
{
	return m_pDXGIOutput4->CheckOverlayColorSpaceSupport(Format, ColorSpace, reinterpret_cast<IUnknown*>(static_cast<CCryDX12Device*>(pConcernedDevice)->GetD3D12Device()), pFlags);
}
#endif
