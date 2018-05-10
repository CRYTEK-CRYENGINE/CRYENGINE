// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/CCryDX12Object.hpp"
#include "CCryDX12GIAdapter.hpp"

class CCryDX12GIOutput : public CCryDX12GIObject<IDXGIOutput4ToImplement>
{
public:
	DX12_OBJECT(CCryDX12GIOutput, CCryDX12GIObject<IDXGIOutput4ToImplement>);

	static CCryDX12GIOutput* Create(CCryDX12GIAdapter* Adapter, UINT Output);

	ILINE CCryDX12GIAdapter*  GetAdapter   () const { return m_pAdapter; }
	ILINE IDXGIOutput4ToCall* GetDXGIOutput() const { return m_pDXGIOutput4; }

	#pragma region /* IDXGIOutput implementation */
	#ifndef __d3d11_x_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDesc(
	  /* [annotation][out] */
	  _Out_ DXGI_OUTPUT_DESC* pDesc) FINALGFX
	{
		return m_pDXGIOutput4->GetDesc(pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDisplayModeList(
	  /* [in] */ DXGI_FORMAT EnumFormat,
	  /* [in] */ UINT Flags,
	  /* [annotation][out][in] */
	  _Inout_ UINT* pNumModes,
	  /* [annotation][out] */
	  _Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC* pDesc) FINALGFX
	{
		return m_pDXGIOutput4->GetDisplayModeList(EnumFormat, Flags, pNumModes, pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(
	  /* [annotation][in] */
	  _In_ const DXGI_MODE_DESC* pModeToMatch,
	  /* [annotation][out] */
	  _Out_ DXGI_MODE_DESC* pClosestMatch,
	  /* [annotation][in] */
	  _In_opt_ IUnknown* pConcernedDevice) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE WaitForVBlank(void) FINALGFX
	{
		return m_pDXGIOutput4->WaitForVBlank();
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE TakeOwnership(
	  /* [annotation][in] */
	  _In_ IUnknown* pDevice,
	  BOOL Exclusive) FINALGFX;

	VIRTUALGFX void STDMETHODCALLTYPE ReleaseOwnership(void) FINALGFX
	{
		return m_pDXGIOutput4->ReleaseOwnership();
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps) FINALGFX
	{
		return m_pDXGIOutput4->GetGammaControlCapabilities(pGammaCaps);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetGammaControl(
	  /* [annotation][in] */
	  _In_ const DXGI_GAMMA_CONTROL* pArray) FINALGFX
	{
		return m_pDXGIOutput4->SetGammaControl(pArray);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetGammaControl(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL* pArray) FINALGFX
	{
		return m_pDXGIOutput4->GetGammaControl(pArray);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetDisplaySurface(
	  /* [annotation][in] */
	  _In_ IDXGISurface* pScanoutSurface) FINALGFX
	{
		return m_pDXGIOutput4->SetDisplaySurface(pScanoutSurface);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(
	  /* [annotation][in] */
	  _In_ IDXGISurface* pDestination) FINALGFX
	{
		return m_pDXGIOutput4->GetDisplaySurfaceData(pDestination);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetFrameStatistics(
	  /* [annotation][out] */
	  _Out_ DXGI_FRAME_STATISTICS* pStats) FINALGFX
	{
		return m_pDXGIOutput4->GetFrameStatistics(pStats);
	}

	#else
	
	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps) FINALGFX
	{
		return m_pDXGIOutput4->GetGammaControlCapabilities(pGammaCaps);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetGammaControl(
	  /* [annotation][in] */
	  _In_ const DXGI_GAMMA_CONTROL* pArray) FINALGFX
	{
		return m_pDXGIOutput4->SetGammaControl(pArray);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetGammaControl(
	  /* [annotation][out] */
	  _Out_ DXGI_GAMMA_CONTROL* pArray) FINALGFX
	{
		return m_pDXGIOutput4->GetGammaControl(pArray);
	}

	#endif
	#pragma endregion

	#pragma region /* IDXGIOutput1 implementation */
	#ifdef __dxgi1_2_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDisplayModeList1(
	  /* [in] */ DXGI_FORMAT EnumFormat,
	  /* [in] */ UINT Flags,
	  /* [annotation][out][in] */
	  _Inout_ UINT* pNumModes,
	  /* [annotation][out] */
	  _Out_writes_to_opt_(*pNumModes, *pNumModes)  DXGI_MODE_DESC1* pDesc) FINALGFX
	{
		return m_pDXGIOutput4->GetDisplayModeList1(EnumFormat, Flags, pNumModes, pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE FindClosestMatchingMode1(
	  /* [annotation][in] */
	  _In_ const DXGI_MODE_DESC1* pModeToMatch,
	  /* [annotation][out] */
	  _Out_ DXGI_MODE_DESC1* pClosestMatch,
	  /* [annotation][in] */
	  _In_opt_ IUnknown* pConcernedDevice) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData1(
	  /* [annotation][in] */
	  _In_ IDXGIResource* pDestination) FINALGFX
	{
		return m_pDXGIOutput4->GetDisplaySurfaceData1(pDestination);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE DuplicateOutput(
	  /* [annotation][in] */
	  _In_ IUnknown* pDevice,
	  /* [annotation][out] */
	  _COM_Outptr_ IDXGIOutputDuplication** ppOutputDuplication) FINALGFX
	{
		return m_pDXGIOutput4->DuplicateOutput(pDevice, ppOutputDuplication);
	}

	#endif
	#pragma endregion

	#pragma region /* IDXGIOutput2 implementation */
	#ifdef __dxgi1_3_h__

	VIRTUALGFX BOOL STDMETHODCALLTYPE SupportsOverlays(void) FINALGFX
	{
		return m_pDXGIOutput4->SupportsOverlays();
	}

	#endif
	#pragma endregion

	#pragma region /* IDXGIOutput3 implementation */
	#ifdef __dxgi1_3_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckOverlaySupport(
	  /* [annotation][in] */
	  _In_ DXGI_FORMAT EnumFormat,
	  /* [annotation][out] */
	  _In_ IUnknown* pConcernedDevice,
	  /* [annotation][out] */
	  _Out_ UINT* pFlags) FINALGFX;

	#endif
	#pragma endregion

	#pragma region /* IDXGIOutput4 implementation */
	#ifdef __dxgi1_4_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckOverlayColorSpaceSupport(
	  /* [annotation][in] */
	  _In_ DXGI_FORMAT Format,
	  /* [annotation][in] */
	  _In_ DXGI_COLOR_SPACE_TYPE ColorSpace,
	  /* [annotation][in] */
	  _In_ IUnknown* pConcernedDevice,
	  /* [annotation][out] */
	  _Out_ UINT* pFlags) FINALGFX;

	#endif
	#pragma endregion

protected:
	CCryDX12GIOutput(CCryDX12GIAdapter* pAdapter, IDXGIOutput4ToCall* pOutput);

private:
	CCryDX12GIAdapter*  m_pAdapter;
	IDXGIOutput4ToCall* m_pDXGIOutput4;
};
