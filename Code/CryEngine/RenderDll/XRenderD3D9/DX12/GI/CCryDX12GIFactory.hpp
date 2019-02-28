// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/CCryDX12Object.hpp"

class CCryDX12GIFactory : public CCryDX12GIObject<IDXGIFactory4ToImplement>
{
public:
	DX12_OBJECT(CCryDX12GIFactory, CCryDX12GIObject<IDXGIFactory4ToImplement> );

	static CCryDX12GIFactory* Create();

	IDXGIFactory4ToCall* GetDXGIFactory() const { return m_pDXGIFactory4; }

	#pragma region /* IDXGIFactory implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE EnumAdapters(
	  UINT Adapter,
	  _Out_ IDXGIAdapter** ppAdapter) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE MakeWindowAssociation(
	  HWND WindowHandle,
	  UINT Flags) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetWindowAssociation(
	  _Out_ HWND* pWindowHandle) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSwapChain(
	  _In_ IGfxUnknown* pDevice,
	  _In_ DXGI_SWAP_CHAIN_DESC* pDesc,
	  _Out_ IDXGISwapChain** ppSwapChain) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(
	  HMODULE Module,
	  _Out_ IDXGIAdapter** ppAdapter) FINALGFX;

	#pragma endregion

	#pragma region /* IDXGIFactory1 implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE EnumAdapters1(
	  UINT Adapter,
	  _Out_ IDXGIAdapter1** ppAdapter) FINALGFX;

	VIRTUALGFX BOOL STDMETHODCALLTYPE IsCurrent() FINALGFX;

	#pragma endregion

	#pragma region /* IDXGIFactory2 implementation */
	#if defined(__dxgi1_2_h__) || defined(__d3d11_x_h__)

	VIRTUALGFX BOOL STDMETHODCALLTYPE    IsWindowedStereoEnabled() FINALGFX { abort(); return FALSE; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(
	  /* [annotation][in] */
	  _In_ IGfxUnknown* pDevice,
	  /* [annotation][in] */
	  _In_ HWND hWnd,
	  /* [annotation][in] */
	  _In_ const DXGI_SWAP_CHAIN_DESC1* pDesc,
	  /* [annotation][in] */
	  _In_opt_ const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
	  /* [annotation][in] */
	  _In_opt_ IDXGIOutput* pRestrictToOutput,
	  /* [annotation][out] */
	  _COM_Outptr_ IDXGISwapChain1** ppSwapChain) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(
	  /* [annotation][in] */
	  _In_ IGfxUnknown* pDevice,
	  /* [annotation][in] */
	  _In_ IUnknown* pWindow,
	  /* [annotation][in] */
	  _In_ const DXGI_SWAP_CHAIN_DESC1* pDesc,
	  /* [annotation][in] */
	  _In_opt_ IDXGIOutput* pRestrictToOutput,
	  /* [annotation][out] */
	  _COM_Outptr_ IDXGISwapChain1** ppSwapChain) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(
	  /* [annotation] */
	  _In_ HANDLE hResource,
	  /* [annotation] */
	  _Out_ LUID* pLuid) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(
	  /* [annotation][in] */
	  _In_ HWND WindowHandle,
	  /* [annotation][in] */
	  _In_ UINT wMsg,
	  /* [annotation][out] */
	  _Out_ DWORD* pdwCookie) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(
	  /* [annotation][in] */
	  _In_ HANDLE hEvent,
	  /* [annotation][out] */
	  _Out_ DWORD* pdwCookie) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX void STDMETHODCALLTYPE UnregisterStereoStatus(
	  /* [annotation][in] */
	  _In_ DWORD dwCookie) FINALGFX { abort(); }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(
	  /* [annotation][in] */
	  _In_ HWND WindowHandle,
	  /* [annotation][in] */
	  _In_ UINT wMsg,
	  /* [annotation][out] */
	  _Out_ DWORD* pdwCookie) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(
	  /* [annotation][in] */
	  _In_ HANDLE hEvent,
	  /* [annotation][out] */
	  _Out_ DWORD* pdwCookie) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX void STDMETHODCALLTYPE UnregisterOcclusionStatus(
	  /* [annotation][in] */
	  _In_ DWORD dwCookie) FINALGFX { abort(); }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(
	  /* [annotation][in] */
	  _In_ IGfxUnknown* pDevice,
	  /* [annotation][in] */
	  _In_ const DXGI_SWAP_CHAIN_DESC1* pDesc,
	  /* [annotation][in] */
	  _In_opt_ IDXGIOutput* pRestrictToOutput,
	  /* [annotation][out] */
	  _COM_Outptr_ IDXGISwapChain1** ppSwapChain) FINALGFX { abort(); return S_OK; }

	#endif
	#pragma endregion

	#pragma region /* IDXGIFactory3 implementation */
	#ifdef __dxgi1_3_h__

	VIRTUALGFX UINT STDMETHODCALLTYPE GetCreationFlags(void) FINALGFX { abort(); return 0; }

	#endif
	#pragma endregion

	#pragma region /* IDXGIFactory4 implementation */
	#ifdef __dxgi1_4_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(
	  /* [annotation] */
	  _In_ LUID AdapterLuid,
	  /* [annotation] */
	  _In_ REFIID riid,
	  /* [annotation] */
	  _COM_Outptr_ void** ppvAdapter) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE EnumWarpAdapter(
	  /* [annotation] */
	  _In_ REFIID riid,
	  /* [annotation] */
	  _COM_Outptr_ void** ppvAdapter) FINALGFX { abort(); return S_OK; }

	#endif
	#pragma endregion

protected:
	CCryDX12GIFactory(IDXGIFactory4ToCall* pDXGIFactory4);

private:
	DX12_PTR(IDXGIFactory4ToCall) m_pDXGIFactory4;
};
