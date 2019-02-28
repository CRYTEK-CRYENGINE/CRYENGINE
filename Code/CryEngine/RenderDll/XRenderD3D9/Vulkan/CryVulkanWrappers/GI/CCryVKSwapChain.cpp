// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryVKSwapChain.hpp"
#include "../../API/VKSwapChain.hpp"

#include "../../API/VKInstance.hpp"
#include "../../API/VKDevice.hpp"

#if USE_SDL2 && (CRY_PLATFORM_ANDROID || CRY_PLATFORM_IOS || CRY_PLATFORM_LINUX)
#include <SDL_syswm.h>
#endif

_smart_ptr<CCryVKSwapChain> CCryVKSwapChain::Create(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC* pDesc)
{
	NCryVulkan::SSurfaceCreationInfo surfaceInfo;
#if CRY_PLATFORM_WINDOWS
	surfaceInfo.windowHandle = pDesc->OutputWindow;
	surfaceInfo.appHandle = (HINSTANCE)GetWindowLongPtr(surfaceInfo.windowHandle, GWLP_HINSTANCE);
#else
	SDL_Window* pWindowContext = reinterpret_cast<SDL_Window*>(pDesc->OutputWindow);
	struct SDL_SysWMinfo info;
	ZeroStruct(info);
	info.version.major = SDL_MAJOR_VERSION;
	info.version.minor = SDL_MINOR_VERSION;
	if (!SDL_GetWindowWMInfo(pWindowContext, &info))
	{
		return nullptr;
	}

	surfaceInfo.pWindow = pWindowContext;
#if CRY_PLATFORM_ANDROID
	surfaceInfo.pNativeWindow = info.info.android.window;
	//	surfaceInfo.hNativeSurface = info.info.android.surface;
#elif CRY_PLATFORM_LINUX
	surfaceInfo.window = static_cast<xcb_window_t>(info.info.x11.window);
#endif  // CRY_PLATFORM_ANDROID

#endif

	VkSurfaceKHR surface;
	if (gcpRendD3D.DevInfo().Factory()->GetVkInstance()->CreateSurface(surfaceInfo, &surface) != VK_SUCCESS)
	{
		VK_ERROR("Failed to create KHR Surface");
		return nullptr;
	}

	// We might not be able to create the swapchain with the desired values. Copy desired desc and update to actual values.
	DXGI_SWAP_CHAIN_DESC correctedDesc = *pDesc;

	if (!pDesc->Windowed)
	{
		if (ApplyFullscreenState(true, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height) == false)
			correctedDesc.Windowed = true;
	}

	auto& commandQueue = pDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS);
	VkPhysicalDevice physicalDevice = commandQueue.GetDevice()->GetPhysicalDeviceInfo()->device;

	// We use the DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING DXGI flag as a vsync toggle for Vulkan
	const bool bVsync = !(pDesc->Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
	VkPresentModeKHR presentMode = GetPresentMode(physicalDevice, surface, correctedDesc.SwapEffect, bVsync);

	_smart_ptr<NCryVulkan::CSwapChain> pVKSwapChain = NCryVulkan::CSwapChain::Create(commandQueue, VK_NULL_HANDLE,
		pDesc->BufferCount, pDesc->BufferDesc.Width, pDesc->BufferDesc.Height, surface, NCryVulkan::DXGIFormatToVKFormat(pDesc->BufferDesc.Format),
		presentMode, NCryVulkan::DXGIUsagetoVkUsage((DXGI_USAGE)pDesc->BufferUsage));

	if (pVKSwapChain)
	{
		const VkSwapchainCreateInfoKHR& vkDesc = pVKSwapChain->GetKHRSwapChainInfo();

		correctedDesc.BufferDesc.Width = vkDesc.imageExtent.width;
		correctedDesc.BufferDesc.Height = vkDesc.imageExtent.height;
		correctedDesc.BufferDesc.Format = NCryVulkan::VKFormatToDXGIFormat(vkDesc.imageFormat);
		correctedDesc.BufferCount = pVKSwapChain->GetBackBufferCount();

		return new CCryVKSwapChain(std::move(pDevice), &correctedDesc, std::move(pVKSwapChain), surface, bVsync);
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryVKSwapChain::CCryVKSwapChain(_smart_ptr<NCryVulkan::CDevice> pDevice, CONST DXGI_SWAP_CHAIN_DESC* pDesc, _smart_ptr<NCryVulkan::CSwapChain> pSwapChain, VkSurfaceKHR surface, bool bVSync)
	: m_Desc(*pDesc)
	, m_pDevice(std::move(pDevice))
	, m_Surface(surface)
	, m_pVKSwapChain(std::move(pSwapChain))
	, m_bFullscreen(!pDesc->Windowed)
	, m_bVSync(bVSync)
{
	VK_FUNC_LOG();
}

CCryVKSwapChain::~CCryVKSwapChain()
{
	VK_FUNC_LOG();

	gcpRendD3D.DevInfo().Factory()->GetVkInstance()->DestroySurface(m_Surface);
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

	NCryVulkan::CDevice* pVKDevice = m_pDevice.get();
	auto& commandQueue = pVKDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS);
	VkPhysicalDevice physicalDevice = commandQueue.GetDevice()->GetPhysicalDeviceInfo()->device;

	VkPresentModeKHR presentMode = GetPresentMode(physicalDevice, m_Surface, Desc.SwapEffect, m_bVSync);

	_smart_ptr<NCryVulkan::CSwapChain> pVKSwapChain = NCryVulkan::CSwapChain::Create(pVKDevice->GetScheduler().GetCommandListPool(CMDQUEUE_GRAPHICS), m_pVKSwapChain->GetKHRSwapChain(),
		Desc.BufferCount, Desc.BufferDesc.Width, Desc.BufferDesc.Height, m_pVKSwapChain->GetKHRSurface(), NCryVulkan::DXGIFormatToVKFormat(Desc.BufferDesc.Format),
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

VkPresentModeKHR CCryVKSwapChain::GetPresentMode(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, DXGI_SWAP_EFFECT, bool bEnableVSync)
{
	const std::vector<VkPresentModeKHR> supportedPresentModes = NCryVulkan::CSwapChain::GetSupportedPresentModes(physicalDevice, surface);

	const VkPresentModeKHR presentPriorityListWithVsync[] = { VK_PRESENT_MODE_MAILBOX_KHR };
	const VkPresentModeKHR presentPriorityListWithoutVsync[] = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR };

	if (bEnableVSync)
	{
		for (const auto &p : presentPriorityListWithVsync)
			if (std::find(supportedPresentModes.begin(), supportedPresentModes.end(), p) != supportedPresentModes.end())
				return p;
	}
	else
	{
		for (const auto &p : presentPriorityListWithoutVsync)
			if (std::find(supportedPresentModes.begin(), supportedPresentModes.end(), p) != supportedPresentModes.end())
				return p;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

bool CCryVKSwapChain::ApplyFullscreenState(bool bFullscreen, uint32_t width, uint32_t height)
{
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
}

/* IDXGISwapChain1 implementation */

/* IDXGISwapChain2 implementation */

/* IDXGISwapChain3 implementation */
