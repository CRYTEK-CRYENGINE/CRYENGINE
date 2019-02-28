// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryVKGIOutput.hpp"

#include "CCryVKGIAdapter.hpp"

CCryVKGIOutput* CCryVKGIOutput::Create(CCryVKGIAdapter* pAdapter, UINT Output)
{
	return new CCryVKGIOutput(pAdapter);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKGIOutput::CCryVKGIOutput(CCryVKGIAdapter* pAdapter)
	: m_pAdapter(pAdapter)
{
	VK_FUNC_LOG();
}

CCryVKGIOutput::~CCryVKGIOutput()
{
	VK_FUNC_LOG();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CCryVKGIOutput::FindClosestMatchingMode(_In_ const DXGI_MODE_DESC* pModeToMatch, _Out_ DXGI_MODE_DESC* pClosestMatch, _In_opt_ IUnknown* pConcernedDevice)
{
	*pClosestMatch = *pModeToMatch;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CCryVKGIOutput::TakeOwnership(_In_ IUnknown* pDevice, BOOL Exclusive)
{
	return S_OK;
}
