// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if VK_USE_DXGI

#include "CCryVKGIOutput_DXGI.hpp"
#include "CCryVKGIAdapter_DXGI.hpp"

CCryVKGIOutput_DXGI* CCryVKGIOutput_DXGI::Create(CCryVKGIAdapter_DXGI* Adapter, _smart_ptr<IDXGIOutputToCall> pNativeDxgiOutput, UINT Output)
{
	if (pNativeDxgiOutput)
	{
		return new CCryVKGIOutput_DXGI(Adapter, pNativeDxgiOutput, Output);
	}

	return nullptr;
}

CCryVKGIOutput_DXGI::CCryVKGIOutput_DXGI(CCryVKGIAdapter_DXGI* pAdapter, _smart_ptr<IDXGIOutputToCall> pNativeDxgiOutput, UINT Output)
	: CCryVKGIOutput(pAdapter)
	, m_pNativeDxgiOutput(std::move(pNativeDxgiOutput))
{}

HRESULT CCryVKGIOutput_DXGI::GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, _Inout_ UINT* pNumModes, _Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC* pDesc)
{
	return m_pNativeDxgiOutput->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
}

HRESULT CCryVKGIOutput_DXGI::FindClosestMatchingMode(_In_ const DXGI_MODE_DESC* pModeToMatch, _Out_ DXGI_MODE_DESC* pClosestMatch, _In_opt_ IUnknown* pConcernedDevice)
{
	return m_pNativeDxgiOutput->FindClosestMatchingMode(pModeToMatch, pClosestMatch, nullptr);
}

HRESULT CCryVKGIOutput_DXGI::GetDesc(DXGI_OUTPUT_DESC* pDesc)
{
	return m_pNativeDxgiOutput->GetDesc(pDesc);
}

#endif