// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryVKSwapChain.hpp"
#include "../../API/VKSwapChain.hpp"

_smart_ptr<CCryVKSwapChain> CCryVKSwapChain::Create(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC* pDesc)
{
	// We might not be able to create the swapchain with the desired values. Copy desired desc and update to actual values.
	DXGI_SWAP_CHAIN_DESC correctedDesc = *pDesc;

	if (!pDesc->Windowed)
	{
		if (ApplyFullscreenState(true, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height) == false)
			correctedDesc.Windowed = true;
	}

	const bool bVsync = CRenderer::CV_r_vsync != 0;
	VkPresentModeKHR presentMode = GetPresentMode(correctedDesc.SwapEffect, bVsync);

	_smart_ptr<NCryVulkan::CSwapChain> pVKSwapChain = NCryVulkan::CSwapChain::Create(pDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS), VK_NULL_HANDLE,
		pDesc->BufferCount, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, NCryVulkan::DXGIFormatToVKFormat(pDesc->BufferDesc.Format),
		presentMode, NCryVulkan::DXGIUsagetoVkUsage((DXGI_USAGE)pDesc->BufferUsage));

	if (pVKSwapChain)
	{
		const VkSwapchainCreateInfoKHR& vkDesc = pVKSwapChain->GetKHRSwapChainInfo();

		correctedDesc.BufferDesc.Width = vkDesc.imageExtent.width;
		correctedDesc.BufferDesc.Height = vkDesc.imageExtent.height;
		correctedDesc.BufferDesc.Format = NCryVulkan::VKFormatToDXGIFormat(vkDesc.imageFormat);
		correctedDesc.BufferCount = pVKSwapChain->GetBackBufferCount();

		return new CCryVKSwapChain(std::move(pDevice), &correctedDesc, std::move(pVKSwapChain), bVsync);
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKSwapChain::CCryVKSwapChain(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC* pDesc, _smart_ptr<NCryVulkan::CSwapChain> pSwapChain, bool bVSync)
	: m_Desc(*pDesc)
	, m_pDevice(std::move(pDevice))
	, m_pVKSwapChain(std::move(pSwapChain))
	, m_bFullscreen(!pDesc->Windowed)
	, m_bVSync(bVSync)
{
	VK_FUNC_LOG();
}

CCryVKSwapChain::~CCryVKSwapChain()
{
	VK_FUNC_LOG();
}

/* IDXGIDeviceSubObject implementation */

/* IDXGISwapChain implementation */

HRESULT STDMETHODCALLTYPE CCryVKSwapChain::Present(
  UINT SyncInterval,
  UINT Flags)
{
	VK_FUNC_LOG();

	NCryVulkan::CDevice* pVKDevice = m_pDevice.get();
	NCryVulkan::CCommandList* const pCommandList = pVKDevice->GetScheduler().GetCommandList(CMDQUEUE_GRAPHICS);
	NCryVulkan::CCommandListPool* const pCommandListPool = &pVKDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS);

	pCommandList->PresentSwapChain(m_pVKSwapChain);
	pVKDevice->GetScheduler().EndOfFrame(false);

	VK_LOG(false, "------------------------------------------------ PRESENT ------------------------------------------------");
	m_pVKSwapChain->Present(SyncInterval, Flags, pCommandListPool->AcquireLastSignalledSemaphore());

	return m_pVKSwapChain->GetLastPresentReturnValue();
}

NCryVulkan::CImageResource* CCryVKSwapChain::GetVKBuffer(UINT Buffer) const
{
	NCryVulkan::CImageResource* pResource = &m_pVKSwapChain->GetBackBuffer(Buffer);
	pResource->AddRef();
	return pResource;
}

HRESULT STDMETHODCALLTYPE CCryVKSwapChain::ResizeBuffers(
  UINT BufferCount,
  UINT Width,
  UINT Height,
  DXGI_FORMAT NewFormat,
  UINT SwapChainFlags)
{
	DXGI_SWAP_CHAIN_DESC Desc = m_Desc;

	Desc.BufferCount = BufferCount == 0 ? Desc.BufferCount : BufferCount;
	Desc.BufferDesc.Width = Width;
	Desc.BufferDesc.Height = Height;
	Desc.BufferDesc.Format = NewFormat;

	const bool bHaveFullscreen = m_Desc.Windowed == 0;
	const bool bWantFullscreen = m_bFullscreen;

	if (bHaveFullscreen != bWantFullscreen)
	{
		if (ApplyFullscreenState(bWantFullscreen, Width, Height))
			Desc.Windowed = !bWantFullscreen;
	}

	VkPresentModeKHR presentMode = GetPresentMode(Desc.SwapEffect, m_bVSync);
	NCryVulkan::CDevice* pVKDevice = m_pDevice.get();

	_smart_ptr<NCryVulkan::CSwapChain> pVKSwapChain = NCryVulkan::CSwapChain::Create(pVKDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS), m_pVKSwapChain->GetKHRSwapChain(),
		Desc.BufferCount, Desc.BufferDesc.Width, Desc.BufferDesc.Height, NCryVulkan::DXGIFormatToVKFormat(Desc.BufferDesc.Format),
		presentMode, NCryVulkan::DXGIUsagetoVkUsage((DXGI_USAGE)Desc.BufferUsage));

	m_pDevice->FlushAndWaitForGPU();

	if (pVKSwapChain)
	{
		// The swap-chain might not have supported the values we requested. Ask the actual values and update the desc
		const VkSwapchainCreateInfoKHR& vkDesc = pVKSwapChain->GetKHRSwapChainInfo();

		Desc.BufferDesc.Width = vkDesc.imageExtent.width;
		Desc.BufferDesc.Height = vkDesc.imageExtent.height;
		Desc.BufferDesc.Format = NCryVulkan::VKFormatToDXGIFormat(vkDesc.imageFormat);
		Desc.BufferCount = pVKSwapChain->GetBackBufferCount();

		m_pVKSwapChain = std::move(pVKSwapChain);
		m_Desc = Desc;

		return S_OK;
	}

	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CCryVKSwapChain::SetFullscreenState(
	BOOL Fullscreen,
	_In_opt_ IDXGIOutput* pTarget)
{
	m_bFullscreen = Fullscreen != 0;
	return S_OK;
}

VkPresentModeKHR CCryVKSwapChain::GetPresentMode(DXGI_SWAP_EFFECT swapEffect, bool bEnableVSync)
{
	return bEnableVSync ? VK_PRESENT_MODE_FIFO_KHR : NCryVulkan::s_presentModes[swapEffect];
}

bool CCryVKSwapChain::ApplyFullscreenState(bool bFullscreen, uint32_t width, uint32_t height)
{
#if CRY_PLATFORM_WINDOWS
	int result = DISP_CHANGE_FAILED;

	if (bFullscreen)
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = width;
		dmScreenSettings.dmPelsHeight = height;
		dmScreenSettings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

		result = ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
	}
	else
	{
		result = ChangeDisplaySettings(nullptr, 0);
	}

	if (result != DISP_CHANGE_SUCCESSFUL)
	{
		VK_WARNING("Failed to switch to '%s' mode", bFullscreen ? "fullscreen" : "windowed");
	}

	return result == DISP_CHANGE_SUCCESSFUL;
#elif CRY_PLATFORM_LINUX
	CryWarning(EValidatorModule::VALIDATOR_MODULE_RENDERER, EValidatorSeverity::VALIDATOR_WARNING,
			   "CCryVKSwapChain::ApplyFullscreenState not implemented on Linux.");
	return false;
#else
	#error "Unknown platform in CCryVKSwapChain::ApplyFullscreenState."
#endif
}

/* IDXGISwapChain1 implementation */

/* IDXGISwapChain2 implementation */

/* IDXGISwapChain3 implementation */
