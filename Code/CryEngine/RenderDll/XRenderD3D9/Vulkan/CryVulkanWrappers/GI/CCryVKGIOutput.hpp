// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CCryVKGIObject.hpp"

class CCryVKGIAdapter;
class CCryVKGIOutput : public CCryVKGIObject
{
public:
	IMPLEMENT_INTERFACES(CCryVKGIOutput)
	static CCryVKGIOutput* Create(CCryVKGIAdapter* Adapter, UINT Output);

	virtual ~CCryVKGIOutput();

	#pragma region /* IDXGIOutput implementation */

	virtual HRESULT STDMETHODCALLTYPE GetDesc(
	  /* [annotation][out] */
	  _Out_ DXGI_OUTPUT_DESC* pDesc)
	{
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList(
	  /* [in] */ DXGI_FORMAT EnumFormat,
	  /* [in] */ UINT Flags,
	  /* [annotation][out][in] */
	  _Inout_ UINT* pNumModes,
	  /* [annotation][out] */
	  _Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC* pDesc)
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
	  /* [annotation][in] */
	  _In_ const DXGI_MODE_DESC* pModeToMatch,
	  /* [annotation][out] */
	  _Out_ DXGI_MODE_DESC* pClosestMatch,
	  /* [annotation][in] */
	  _In_opt_ IUnknown* pConcernedDevice);

	virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(void) final
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE TakeOwnership(
	  /* [annotation][in] */
	  _In_ IUnknown* pDevice,
	  BOOL Exclusive) final;

	virtual void STDMETHODCALLTYPE ReleaseOwnership(void) final
	{
		
	}

	virtual HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps) final
	{
		return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE SetGammaControl(
	  /* [annotation][in] */
	  _In_ const DXGI_GAMMA_CONTROL* pArray) final
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE GetGammaControl(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL* pArray) final
	{
		return S_FALSE; 
	}

	virtual HRESULT STDMETHODCALLTYPE SetDisplaySurface(
	  /* [annotation][in] */
	  _In_ IDXGISurface* pScanoutSurface) final
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
	  /* [annotation][in] */
	  _In_ IDXGISurface* pDestination) final
	{
		return S_FALSE;
	}

	virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(
	  /* [annotation][out] */
	  _Out_ DXGI_FRAME_STATISTICS* pStats) final
	{
		return S_FALSE;
	}

	#pragma endregion

protected:
	CCryVKGIOutput(CCryVKGIAdapter* pAdapter);

private:
	_smart_ptr<CCryVKGIAdapter> m_pAdapter;

};
