// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/CCryDX12Object.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"

#include "DX12/API/DX12SwapChain.hpp"

class CCryDX12SwapChain : public CCryDX12GIObject<IDXGISwapChain3ToImplement>
{
public:
	DX12_OBJECT(CCryDX12SwapChain, CCryDX12GIObject<IDXGISwapChain3ToImplement>);

	static CCryDX12SwapChain* Create(CCryDX12Device* pDevice, IDXGIFactory4ToCall* factory, DXGI_SWAP_CHAIN_DESC* pDesc);
	static CCryDX12SwapChain* Create(CCryDX12Device* pDevice, IDXGISwapChain1ToCall* swapchain, DXGI_SWAP_CHAIN_DESC1* pDesc);

	virtual ~CCryDX12SwapChain();

	ILINE CCryDX12Device* GetDevice() const
	{
		return m_pDevice;
	}

	ILINE IDXGISwapChain3ToCall* GetDXGISwapChain() const
	{
		return m_pDX12SwapChain->GetDXGISwapChain();
	}

	ILINE NCryDX12::CSwapChain* GetDX12SwapChain()
	{
		return m_pDX12SwapChain;
	}

	#pragma region /* IDXGIDeviceSubObject implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDevice(
		_In_ REFIID riid,
		_Out_ void** ppDevice) FINALGFX { abort(); return S_OK; }

	#pragma endregion

	#pragma region /* IDXGISwapChain implementation */

	VIRTUALGFX HRESULT STDMETHODCALLTYPE Present(
	  UINT SyncInterval,
	  UINT Flags) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetBuffer(
	  UINT Buffer,
	  _In_ REFIID riid,
	  _Out_ void** ppSurface) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetFullscreenState(
		BOOL Fullscreen,
		_In_opt_ IDXGIOutput* pTarget) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->SetFullscreenState(Fullscreen, pTarget ? reinterpret_cast<CCryDX12GIOutput*>(pTarget)->GetDXGIOutput() : nullptr);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetFullscreenState(
		_Out_opt_ BOOL* pFullscreen,
		_Out_opt_ IDXGIOutput** ppTarget) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDesc(
		_Out_ DXGI_SWAP_CHAIN_DESC* pDesc) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetDesc(pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE ResizeBuffers(
	  UINT BufferCount,
	  UINT Width,
	  UINT Height,
	  DXGI_FORMAT NewFormat,
	  UINT SwapChainFlags) FINALGFX;

	VIRTUALGFX HRESULT STDMETHODCALLTYPE ResizeTarget(
		_In_ const DXGI_MODE_DESC* pNewTargetParameters) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->ResizeTarget(pNewTargetParameters);
	}


	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetContainingOutput(
		_Out_ IDXGIOutput** ppOutput) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetFrameStatistics(
		_Out_ DXGI_FRAME_STATISTICS* pStats) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetFrameStatistics(pStats);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetLastPresentCount(
		_Out_ UINT* pLastPresentCount) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetLastPresentCount(pLastPresentCount);
	}

	#pragma endregion

	#pragma region /* IDXGISwapChain1 implementation */
	#if defined(__dxgi1_2_h__) || defined(__d3d11_x_h__)

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetDesc1(
	  /* [annotation][out] */
	  _Out_ DXGI_SWAP_CHAIN_DESC1* pDesc) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetDesc1(pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
	  /* [annotation][out] */
	  _Out_ DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetFullscreenDesc(pDesc);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetHwnd(
	  /* [annotation][out] */
	  _Out_ HWND* pHwnd) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetHwnd(pHwnd);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetCoreWindow(
	  /* [annotation][in] */
	  _In_ REFIID refiid,
	  /* [annotation][out] */
	  _COM_Outptr_ void** ppUnk) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetCoreWindow(refiid, ppUnk);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE Present1(
	  /* [in] */ UINT SyncInterval,
	  /* [in] */ UINT PresentFlags,
	  /* [annotation][in] */
	  _In_ const DXGI_PRESENT_PARAMETERS* pPresentParameters) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->Present1(SyncInterval, PresentFlags, pPresentParameters);
	}

	VIRTUALGFX BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->IsTemporaryMonoSupported();
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
	  /* [annotation][out] */
	  _Out_ IDXGIOutput** ppRestrictToOutput) FINALGFX { abort(); return S_OK; }

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetBackgroundColor(
	  /* [annotation][in] */
	  _In_ const DXGI_RGBA* pColor) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetBackgroundColor(pColor);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetBackgroundColor(
	  /* [annotation][out] */
	  _Out_ DXGI_RGBA* pColor) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetBackgroundColor(pColor);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetRotation(
	  /* [annotation][in] */
	  _In_ DXGI_MODE_ROTATION Rotation) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetRotation(Rotation);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetRotation(
	  /* [annotation][out] */
	  _Out_ DXGI_MODE_ROTATION* pRotation) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetRotation(pRotation);
	}

	#endif
	#pragma endregion

	#pragma region /* IDXGISwapChain2 implementation */
	#ifdef __dxgi1_3_h__

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetSourceSize(
	  UINT Width,
	  UINT Height) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetSourceSize(Width, Height);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetSourceSize(
	  /* [annotation][out] */
	  _Out_ UINT* pWidth,
	  /* [annotation][out] */
	  _Out_ UINT* pHeight) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetSourceSize(pWidth, pHeight);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
	  UINT MaxLatency) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetMaximumFrameLatency(MaxLatency);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
	  /* [annotation][out] */
	  _Out_ UINT* pMaxLatency) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetMaximumFrameLatency(pMaxLatency);
	}

	VIRTUALGFX HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetFrameLatencyWaitableObject();
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetMatrixTransform(
	  const DXGI_MATRIX_3X2_F* pMatrix) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetMatrixTransform(pMatrix);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetMatrixTransform(
	  /* [annotation][out] */
	  _Out_ DXGI_MATRIX_3X2_F* pMatrix) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->GetMatrixTransform(pMatrix);
	}

	#endif
	#pragma endregion

	#pragma region /* IDXGISwapChain3 implementation */
	#ifdef __dxgi1_4_h__

	VIRTUALGFX UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex(
		void) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetCurrentBackbufferIndex();
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
	  /* [annotation][in] */
	  _In_ DXGI_COLOR_SPACE_TYPE ColorSpace,
	  /* [annotation][out] */
	  _Out_ UINT* pColorSpaceSupport) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetColorSpace1(
	  /* [annotation][in] */
	  _In_ DXGI_COLOR_SPACE_TYPE ColorSpace) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->SetColorSpace1(ColorSpace);
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE ResizeBuffers1(
	  /* [annotation][in] */
	  _In_ UINT BufferCount,
	  /* [annotation][in] */
	  _In_ UINT Width,
	  /* [annotation][in] */
	  _In_ UINT Height,
	  /* [annotation][in] */
	  _In_ DXGI_FORMAT Format,
	  /* [annotation][in] */
	  _In_ UINT SwapChainFlags,
	  /* [annotation][in] */
	  _In_reads_(BufferCount)  const UINT* pCreationNodeMask,
	  /* [annotation][in] */
	  _In_reads_(BufferCount)  IUnknown* const* ppPresentQueue) FINALGFX
	{
		DX12_FUNC_LOG
		return m_pDX12SwapChain->GetDXGISwapChain()->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
	}

	#endif
	#pragma endregion

protected:
	CCryDX12SwapChain(CCryDX12Device* pDevice, NCryDX12::CSwapChain* pSwapChain);

private:
	CCryDX12Device*       m_pDevice;

	NCryDX12::CSwapChain* m_pDX12SwapChain;

	std::vector<CCryDX12Texture2D*> m_pBuffers;
};
