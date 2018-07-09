// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CCryVKGIObject.hpp"

#include "Vulkan/API/VKSwapChain.hpp"

#include <functional>

namespace NCryVulkan
{
class CSwapChain;
}

class CCryVKGIFactory;
class CCryVKSwapChain : public CCryVKGIObject
{
public:
	IMPLEMENT_INTERFACES(CCryVKSwapChain);

	static _smart_ptr<CCryVKSwapChain> Create(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC * pDesc);

	virtual ~CCryVKSwapChain();

	ILINE NCryVulkan::CDevice* GetDevice() const
	{
		m_pDevice->AddRef();
		return m_pDevice.get();
	}

	ILINE VkSwapchainKHR GetKHRSwapChain() const
	{
		return m_pVKSwapChain->GetKHRSwapChain();
	}

	ILINE const NCryVulkan::CSwapChain* GetVKSwapchain()
	{
		return m_pVKSwapChain;
	}

	void SetVSyncEnabled(bool bEnable)
	{
		m_bVSync = bEnable;
	}

	#pragma region /* IDXGIDeviceSubObject implementation */
	
	virtual HRESULT STDMETHODCALLTYPE GetDevice(
		_In_ REFIID riid,
		_Out_ void** ppDevice) final { abort(); return S_OK; }

	#pragma endregion

	#pragma region /* IDXGISwapChain implementation */

	virtual HRESULT STDMETHODCALLTYPE Present(
	  UINT SyncInterval,
	  UINT Flags) final;

	NCryVulkan::CImageResource* GetVKBuffer(UINT Buffer) const;
	/* This cannot be called, as CImageResource doesn't have a COM UUID. Use GetVkBuffer instead!
	virtual HRESULT STDMETHODCALLTYPE GetBuffer(
	  UINT Buffer,
	  _In_ REFIID riid,
	  _Out_ void** ppSurface) final; */

	HRESULT STDMETHODCALLTYPE SetFullscreenState(
		BOOL Fullscreen,
		_In_opt_ IDXGIOutput* pTarget);

	HRESULT STDMETHODCALLTYPE GetFullscreenState(
	  _Out_opt_ BOOL* pFullscreen,
	  _Out_opt_ IDXGIOutput** ppTarget)
	{
		VK_FUNC_LOG();
		*pFullscreen = m_bFullscreen;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDesc(
	  _Out_ DXGI_SWAP_CHAIN_DESC* pDesc)
	{
		*pDesc = m_Desc;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE ResizeBuffers(
	  UINT BufferCount,
	  UINT Width,
	  UINT Height,
	  DXGI_FORMAT NewFormat,
	  UINT SwapChainFlags) final;

	HRESULT STDMETHODCALLTYPE ResizeTarget(
	  _In_ const DXGI_MODE_DESC* pNewTargetParameters)
	{
		m_Desc.BufferDesc = *pNewTargetParameters;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetContainingOutput(
	  _Out_ IDXGIOutput** ppOutput)
	{
		VK_FUNC_LOG();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetFrameStatistics(
	  _Out_ DXGI_FRAME_STATISTICS* pStats)
	{
		VK_FUNC_LOG();
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetLastPresentCount(
	  _Out_ UINT* pLastPresentCount)
	{
		VK_FUNC_LOG();
		return S_OK;
	}

	#pragma endregion
	
	#pragma region /* IDXGISwapChain3 implementation */

	virtual UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex(
		void) final
	{
		VK_FUNC_LOG();
		return m_pVKSwapChain->GetCurrentBackbufferIndex();
	}
	
	#pragma endregion
	
protected:
	CCryVKSwapChain(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC* pDesc, _smart_ptr<NCryVulkan::CSwapChain> swapchain, VkSurfaceKHR surface, bool bVSync);

	static VkPresentModeKHR GetPresentMode(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, DXGI_SWAP_EFFECT swapEffect, bool bEnableVSync);
	static bool ApplyFullscreenState(bool bFullscreen, uint32_t width, uint32_t height);

private:
	DXGI_SWAP_CHAIN_DESC               m_Desc;
	_smart_ptr<NCryVulkan::CDevice>    m_pDevice;
	VkSurfaceKHR                       m_Surface;
	_smart_ptr<NCryVulkan::CSwapChain> m_pVKSwapChain;
	bool                               m_bFullscreen;
	bool                               m_bVSync;
};
