// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if VK_USE_DXGI

#include "..\CCryVKGIObject.hpp"

class CCryVKGIAdapter_DXGI;

class CCryVKGIOutput_DXGI : public CCryVKGIOutput
{
public:
	static CCryVKGIOutput_DXGI* Create(CCryVKGIAdapter_DXGI* Adapter, _smart_ptr<IDXGIOutputToCall> pNativeDxgiOutput, UINT Output);

	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList(
		/* [in] */ DXGI_FORMAT EnumFormat,
		/* [in] */ UINT Flags,
		/* [annotation][out][in] */
		_Inout_ UINT* pNumModes,
		/* [annotation][out] */
		_Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC* pDesc) final;

	virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
		/* [annotation][in] */
		_In_ const DXGI_MODE_DESC* pModeToMatch,
		/* [annotation][out] */
		_Out_ DXGI_MODE_DESC* pClosestMatch,
		/* [annotation][in] */
		_In_opt_ IUnknown* pConcernedDevice) final;

	virtual HRESULT STDMETHODCALLTYPE GetDesc(
		/* [annotation][out] */
		_Out_ DXGI_OUTPUT_DESC* pDesc);

protected:
	CCryVKGIOutput_DXGI(CCryVKGIAdapter_DXGI* pAdapter, _smart_ptr<IDXGIOutputToCall> pNativeDxgiOutput, UINT Output);

private:
	_smart_ptr<IDXGIOutputToCall> m_pNativeDxgiOutput;
};

#endif
