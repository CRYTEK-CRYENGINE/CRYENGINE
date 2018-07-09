// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12GIAdapter.hpp"

#include "CCryDX12GIFactory.hpp"
#include "CCryDX12GIOutput.hpp"

CCryDX12GIAdapter* CCryDX12GIAdapter::Create(CCryDX12GIFactory* pFactory, UINT Adapter)
{
	IDXGIAdapter1ToCall* pAdapter1;
	pFactory->GetDXGIFactory()->EnumAdapters1(Adapter, &pAdapter1);
	IDXGIAdapter3ToCall* pAdapter3;
	pAdapter1->QueryInterface(__uuidof(IDXGIAdapter3ToCall), (void**)&pAdapter3);

	return pAdapter3 ? DX12_NEW_RAW(CCryDX12GIAdapter(pFactory, pAdapter3)) : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12GIAdapter::CCryDX12GIAdapter(CCryDX12GIFactory* pFactory, IDXGIAdapter3ToCall* pAdapter)
	: Super()
	, m_pFactory(pFactory)
	, m_pDXGIAdapter3(pAdapter)
{
	DX12_FUNC_LOG
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma region /* IDXGIAdapter implementation */

HRESULT STDMETHODCALLTYPE CCryDX12GIAdapter::EnumOutputs(UINT Output, _COM_Outptr_ IDXGIOutput** ppOutput)
{
	DX12_FUNC_LOG
	* ppOutput = CCryDX12GIOutput::Create(this, Output);
	return *ppOutput ? S_OK : E_FAIL;
}

#pragma endregion
