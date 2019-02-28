// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CCryVKGIObject.hpp"
#include "CCryVKGIOutput.hpp"

class CCryVKGIFactory;
class CCryVKGIAdapter : public CCryVKGIObject
{
public:
	IMPLEMENT_INTERFACES(CCryVKGIAdapter)
	static CCryVKGIAdapter* Create(CCryVKGIFactory* factory, UINT Adapter);

	virtual ~CCryVKGIAdapter();

	ILINE CCryVKGIFactory* GetFactory() const { return m_pFactory; }

	ILINE UINT GetDeviceIndex()   { return m_deviceIndex; }

	#pragma region /* IDXGIAdapter implementation */

	virtual HRESULT STDMETHODCALLTYPE EnumOutputs(
	  /* [in] */ UINT Output,
	  /* [annotation][out][in] */
	  _COM_Outptr_ IDXGIOutput** ppOutput);

	virtual HRESULT STDMETHODCALLTYPE GetDesc(
	  /* [annotation][out] */
	  _Out_ DXGI_ADAPTER_DESC* pDesc)
	{
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(
	  /* [annotation][in] */
	  _In_ REFGUID InterfaceName,
	  /* [annotation][out] */
	  _Out_ LARGE_INTEGER* pUMDVersion)
	{
		return E_FAIL;
	}

	#pragma endregion

	#pragma region /* IDXGIAdapter1 implementation */

	virtual HRESULT STDMETHODCALLTYPE GetDesc1(
	  _Out_ DXGI_ADAPTER_DESC1* pDesc)
	{
		return S_FALSE;
	}

	#pragma endregion

	#pragma endregion

protected:
	CCryVKGIAdapter(CCryVKGIFactory* pFactory, UINT index);

private:
	_smart_ptr<CCryVKGIFactory> m_pFactory;
	UINT m_deviceIndex;
};
