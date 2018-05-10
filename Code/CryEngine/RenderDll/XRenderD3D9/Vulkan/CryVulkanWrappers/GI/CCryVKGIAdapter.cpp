// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryVKGIAdapter.hpp"

#include "CCryVKGIFactory.hpp"
#include "CCryVKGIOutput.hpp"

CCryVKGIAdapter* CCryVKGIAdapter::Create(CCryVKGIFactory* pFactory, UINT Adapter)
{
	return new CCryVKGIAdapter(pFactory, Adapter);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKGIAdapter::CCryVKGIAdapter(CCryVKGIFactory* pFactory, UINT index)
	: m_pFactory(pFactory),
	m_deviceIndex(index)
{
	VK_FUNC_LOG();
}

CCryVKGIAdapter::~CCryVKGIAdapter()
{
	VK_FUNC_LOG();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CCryVKGIAdapter::EnumOutputs(UINT Output, _COM_Outptr_ IDXGIOutput** ppOutput)
{
	VK_FUNC_LOG();
	* ppOutput = CCryVKGIOutput::Create(this, Output);
	return *ppOutput ? S_OK : E_FAIL;
}
