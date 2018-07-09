// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKSwapChain.hpp"

#include "VKInstance.hpp"
#include "VKDevice.hpp"

namespace NCryVulkan
{
	
bool CSwapChain::GetSupportedSurfaceFormats(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, std::vector<VkSurfaceFormatKHR>& outFormatsSupported)
{
	uint32_t formatsCount;

	VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatsCount, NULL);
	if (result != VK_SUCCESS)
		return false;

	outFormatsSupported.resize(formatsCount);

	result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatsCount, outFormatsSupported.data());
	if (result != VK_SUCCESS)
		return false;

	return true;
}

std::vector<VkPresentModeKHR> CSwapChain::GetSupportedPresentModes(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface)
{
	std::vector<VkPresentModeKHR> presentModes;

	uint32_t presentModeCount;
	VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (result != VK_SUCCESS)
		return presentModes;

	presentModes.resize(presentModeCount);
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
	if (result != VK_SUCCESS)
		presentModes.clear();

	return presentModes;
}

_smart_ptr<CSwapChain> CSwapChain::Create(CCommandListPool& commandQueue, VkSwapchainKHR KHRSwapChain, uint32_t numberOfBuffers, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkFormat format, VkPresentModeKHR presentMode, VkImageUsageFlags imageUsage)
{
	std::vector<VkSurfaceFormatKHR> supportedFormats;
	VkSurfaceCapabilitiesKHR surfaceCapabilites;

	VkPhysicalDevice physicalDevice = commandQueue.GetDevice()->GetPhysicalDeviceInfo()->device;
	VkBool32 bSupported = false;

	{
		const auto a = GetSupportedSurfaceFormats(physicalDevice, surface, supportedFormats); VK_ASSERT(a);
		const auto b = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilites); VK_ASSERT(b == VK_SUCCESS);
		const auto c = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, commandQueue.GetVkQueueFamily(), surface, &bSupported); VK_ASSERT(c == VK_SUCCESS);
	}

	// validate surface dimensions
	if (surfaceCapabilites.currentExtent.width != (uint32_t) ~0)
	{
		if (surfaceCapabilites.currentExtent.width != width || surfaceCapabilites.currentExtent.height != height)
		{
			// the surface and the swap-chain dimensions won't match, should we fall back to current surface size here?
			VK_WARNING("The surface and swapchain dimensions won't match");
		}

	}

	uint32_t w = min(max(width, surfaceCapabilites.minImageExtent.width), surfaceCapabilites.maxImageExtent.width);
	uint32_t h = min(max(height, surfaceCapabilites.minImageExtent.height), surfaceCapabilites.maxImageExtent.height);

	if (w != width || h != height)
	{
		VK_WARNING("Requested swapchain dimensions out of bounds, min(%d, %d), max(%d,%d), requested(%d, %d). Clamping to fit the bounds.",
		           surfaceCapabilites.minImageExtent.width, surfaceCapabilites.minImageExtent.height,
		           surfaceCapabilites.maxImageExtent.width, surfaceCapabilites.maxImageExtent.height,
		           width, height);
	}

	uint32_t numberOfImagesToRequest = numberOfBuffers;
	if ((surfaceCapabilites.minImageCount > 0) && (numberOfImagesToRequest < surfaceCapabilites.minImageCount))
	{
		VK_WARNING("Cannot obtain requested amount of swapchain images (%d), taking the supported minimum (%d)", numberOfImagesToRequest, surfaceCapabilites.minImageCount);
		numberOfImagesToRequest = surfaceCapabilites.minImageCount;
	}
	if ((surfaceCapabilites.maxImageCount > 0) && (numberOfImagesToRequest > surfaceCapabilites.maxImageCount))
	{
		VK_WARNING("Cannot obtain requested amount of swapchain images (%d), taking the supported maximum (%d)", numberOfImagesToRequest, surfaceCapabilites.maxImageCount);
		numberOfImagesToRequest = surfaceCapabilites.maxImageCount;
	}

	// validate surface format
	VkFormat formatToRequest = VK_FORMAT_UNDEFINED;
	if (supportedFormats.size() == 1 && supportedFormats[0].format == VK_FORMAT_UNDEFINED)   //if only one format is supported and it's undefined, there is no preferred format
	{
		formatToRequest = format;
	}
	else
	{
		for (size_t i = 0; i < supportedFormats.size(); ++i)
		{
			if (supportedFormats[i].format == format)
			{
				formatToRequest = format;
				break;
			}
		}
	}

	// if requested format not supported, fall back to something supported
	if (formatToRequest == VK_FORMAT_UNDEFINED)
	{
		VK_WARNING("Requested image format (%d) not supported, falling back to %d", format, supportedFormats[0].format);
		formatToRequest = supportedFormats[0].format;
	}

	// validate swap-chain transform
	VkSurfaceTransformFlagBitsKHR preTransform;
	if (surfaceCapabilites.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfaceCapabilites.currentTransform;
	}

	return Create(commandQueue, KHRSwapChain, numberOfImagesToRequest, w, h, surface, formatToRequest, presentMode, imageUsage, preTransform);
}

_smart_ptr<CSwapChain> CSwapChain::Create(CCommandListPool& commandQueue, VkSwapchainKHR KHRSwapChain, uint32_t numberOfBuffers, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkFormat format, VkPresentModeKHR presentMode, VkImageUsageFlags imageUsage, VkSurfaceTransformFlagBitsKHR transform)
{
	VkSwapchainCreateInfoKHR Info;
	ZeroStruct(Info);

	Info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	Info.pNext = NULL;
	Info.surface = surface;
	Info.minImageCount = numberOfBuffers;
	Info.imageFormat = format;
	Info.imageExtent.width = width;
	Info.imageExtent.height = height;
	Info.imageArrayLayers = 1;
	Info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	Info.imageUsage = imageUsage;
	Info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	Info.preTransform = transform;
	Info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	Info.presentMode = presentMode;
	Info.oldSwapchain = KHRSwapChain;
	Info.clipped = true;
	Info.queueFamilyIndexCount = 0;
	Info.pQueueFamilyIndices = NULL;

	return Create(commandQueue, &Info);
}

_smart_ptr<CSwapChain> CSwapChain::Create(CCommandListPool& commandQueue, VkSwapchainCreateInfoKHR* pInfo)
{
	VkSwapchainKHR KHRSwapChain = VK_NULL_HANDLE;
	VkQueue QueueVk = commandQueue.GetVkCommandQueue();
	VkDevice DeviceVk = commandQueue.GetDevice()->GetVkDevice();

	VkResult result = vkCreateSwapchainKHR(DeviceVk, pInfo, nullptr, &KHRSwapChain);

	if (result == VK_SUCCESS && KHRSwapChain)
	{
		CSwapChain* pRaw = new CSwapChain(commandQueue, pInfo->surface, KHRSwapChain, pInfo); // Has ref-count 1
		_smart_ptr<CSwapChain> smart;
		smart.Assign_NoAddRef(pRaw);
		return smart;
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CSwapChain::CSwapChain(CCommandListPool& commandQueue, VkSurfaceKHR KHRSurface, VkSwapchainKHR KHRSwapChain, VkSwapchainCreateInfoKHR* pInfo)
	: CRefCounted()
	, m_pDevice(commandQueue.GetDevice())
	, m_NumBackbuffers(0)
	, m_bChangedBackBufferIndex(true)
	, m_nCurrentBackBufferIndex(0)
	, m_pCommandQueue(commandQueue)
	, m_presentFence(commandQueue.GetDevice())
	, m_asyncQueue(commandQueue.GetAsyncCommandQueue())
{
	VK_ASSERT(pInfo);
	{
		m_Info = *pInfo;
	}

	m_presentFence.Init();
	m_presentResult = VK_SUCCESS;
	m_KHRSurface = KHRSurface;
	m_KHRSwapChain = KHRSwapChain;

	AcquireBuffers();
}

//---------------------------------------------------------------------------------------------------------------------
CSwapChain::~CSwapChain()
{
	ForfeitBuffers();

	vkDestroySwapchainKHR(m_pDevice->GetVkDevice(), m_KHRSwapChain, nullptr);
}

//HRESULT CSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
//{
//	return m_KHRSwapChain->ResizeTarget(
//	  pNewTargetParameters);
//}
//
//HRESULT CSwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
//{
//	m_NumBackbuffers = BufferCount ? BufferCount : m_NumBackbuffers;
//	m_Info.BufferDesc.Width = Width ? Width : m_Info.BufferDesc.Width;
//	m_Info.BufferDesc.Height = Height ? Height : m_Info.BufferDesc.Height;
//	m_Info.BufferDesc.Format = NewFormat != DXGI_FORMAT_UNKNOWN ? NewFormat : m_Info.BufferDesc.Format;
//	m_Info.Flags = SwapChainFlags;
//
//	return m_KHRSwapChain->ResizeBuffers(
//	  m_NumBackbuffers,
//	  m_Info.BufferDesc.Width,
//	  m_Info.BufferDesc.Height,
//	  m_Info.BufferDesc.Format,
//	  m_Info.Flags);
//}

void CSwapChain::AcquireBuffers()
{
	VkDevice DeviceVk = m_pDevice->GetVkDevice();
	VkResult result = vkGetSwapchainImagesKHR(DeviceVk, m_KHRSwapChain, &m_NumBackbuffers, NULL);
	VK_ASSERT(result == VK_SUCCESS);

	m_BackBuffers.reserve(m_NumBackbuffers);

	std::vector<VkImage> images(m_NumBackbuffers);
	result = vkGetSwapchainImagesKHR(DeviceVk, m_KHRSwapChain, &m_NumBackbuffers, images.data());
	VK_ASSERT(result == VK_SUCCESS);

	for (int i = 0; i < m_NumBackbuffers; ++i)
	{
		m_BackBuffers.emplace_back(m_pDevice);
		m_BackBuffers[i].InitFromSwapchain(images[i], VK_IMAGE_LAYOUT_UNDEFINED, m_Info.imageExtent.width, m_Info.imageExtent.height, m_Info.imageFormat);
	}
}

void CSwapChain::UnblockBuffers(CCommandList* pCommandList)
{
	for (int i = 0; i < m_NumBackbuffers; ++i)
	{
#if 0
		D3D12_RESOURCE_STATES resourceState = m_BackBuffers[i].GetAnnouncedState();
		if (resourceState != (D3D12_RESOURCE_STATES)-1)
		{
			m_BackBuffers[i].EndTransitionBarrier(pCommandList, resourceState);
		}
#endif
	}
}

void CSwapChain::VerifyBufferCounters()
{
}

void CSwapChain::ForfeitBuffers()
{
	VerifyBufferCounters();

	m_BackBuffers.clear();
}

}