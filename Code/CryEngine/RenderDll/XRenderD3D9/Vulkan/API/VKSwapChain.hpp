// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"
#include "VKCommandList.hpp"
#include "VKAsyncCommandQueue.hpp"
#include "VKDevice.hpp"
#include "VKImageResource.hpp"

namespace NCryVulkan
{

class CSwapChain : public CRefCounted
{
public:
	static bool GetSupportedSurfaceFormats(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, std::vector<VkSurfaceFormatKHR>& outFormatsSupported);
	static std::vector<VkPresentModeKHR> GetSupportedPresentModes(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface);

public:
	static _smart_ptr<CSwapChain> Create(CCommandListPool& commandQueue, VkSwapchainKHR KHRSwapChain, uint32_t numberOfBuffers, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkFormat format, VkPresentModeKHR presentMode, VkImageUsageFlags imageUsage);
	static _smart_ptr<CSwapChain> Create(CCommandListPool& commandQueue, VkSwapchainKHR KHRSwapChain, uint32_t numberOfBuffers, uint32_t width, uint32_t height, VkSurfaceKHR surface, VkFormat format, VkPresentModeKHR presentMode, VkImageUsageFlags imageUsage, VkSurfaceTransformFlagBitsKHR transform);
	static _smart_ptr<CSwapChain> Create(CCommandListPool& commandQueue, VkSwapchainCreateInfoKHR* pInfo);

protected:
	CSwapChain(CCommandListPool& commandQueue, VkSurfaceKHR KHRSurface, VkSwapchainKHR KHRSwapChain, VkSwapchainCreateInfoKHR* pInfo);

	virtual ~CSwapChain();

public:
	ILINE VkSwapchainKHR GetKHRSwapChain() const
	{
		return m_KHRSwapChain;
	}

	ILINE VkSurfaceKHR GetKHRSurface() const
	{
		return m_KHRSurface;
	}

	ILINE const VkSwapchainCreateInfoKHR& GetKHRSwapChainInfo() const
	{
		return m_Info;
	}

	ILINE UINT GetBackBufferCount() const
	{
		return m_NumBackbuffers;
	}

	// Get the index of the back-buffer which is to be used for the next Present().
	// Cache the value for multiple calls between Present()s, as vkAcquireNextImageKHR() will always increment the counter regardless if flips occured.
	ILINE UINT GetCurrentBackbufferIndex() const
	{
		if (!m_bChangedBackBufferIndex)
			return m_nCurrentBackBufferIndex;

		// Pick a semaphore from the ring-buffer ...
		auto* const pFence = m_presentFence.GetFence();
		VkSemaphore Semaphore = pFence->second[m_semaphoreIndex++ % pFence->second.size()];
		{
			m_asyncQueue.FlushNextPresent(); VK_ASSERT(!IsPresentScheduled(), "Flush didn't dry out all outstanding Present() calls!");
			VkResult result = vkAcquireNextImageKHR(m_pDevice->GetVkDevice(), m_KHRSwapChain, ~0ULL, Semaphore, VK_NULL_HANDLE, &m_nCurrentBackBufferIndex);
			VK_ASSERT(result == VK_SUCCESS);
			m_bChangedBackBufferIndex = false;
		}
		// ... and inject it as sync-primitive into the core-command-list - which is the one active at the moment of this call (which can be mid-frame)
		// Adding the semaphore will make the command-list submit even if there are no commands in it, because semaphores can not set/reset from CPU side.
		m_pDevice->GetScheduler().GetCommandList(CMDQUEUE_GRAPHICS)->WaitForFinishOnGPU(Semaphore);

		return m_nCurrentBackBufferIndex;
	}
	ILINE CImageResource& GetBackBuffer(UINT index)
	{
		return m_BackBuffers[index];
	}
	ILINE CImageResource& GetCurrentBackBuffer()
	{
		return m_BackBuffers[GetCurrentBackbufferIndex()];
	}

	ILINE bool IsPresentScheduled() const
	{
		return m_asyncQueue.GetQueuedFramesCount() > 0;
	}

	// Run Present() asynchronuously, which means the next back-buffer index is not queryable within this function.
	// Instead defer acquiring the next index to the next call of GetCurrentBackbufferIndex().
	void Present(UINT SyncInterval, UINT Flags, VkSemaphore sem)
	{
		m_asyncQueue.Present(m_KHRSwapChain, GetCurrentBackbufferIndex(), 1, &sem, &m_presentResult);
		m_bChangedBackBufferIndex = true;
	}

	VkResult GetLastPresentReturnValue() { return m_presentResult; }

//	VkResult ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters);
//	VkResult ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

	void    AcquireBuffers();
	void    UnblockBuffers(CCommandList* pCommandList);
	void    VerifyBufferCounters();
	void    ForfeitBuffers();

private:
	CDevice*                 m_pDevice;
	CAsyncCommandQueue&      m_asyncQueue;
	CCommandListPool&        m_pCommandQueue;
	CCommandListFence        m_presentFence;

	VkSwapchainCreateInfoKHR m_Info;
	uint32_t                 m_NumBackbuffers;
	mutable bool             m_bChangedBackBufferIndex;
	mutable uint32_t         m_nCurrentBackBufferIndex;
	mutable uint32_t         m_semaphoreIndex = 0;

	VkResult                 m_presentResult;
	VkSurfaceKHR             m_KHRSurface;
	VkSwapchainKHR           m_KHRSwapChain;

	std::vector<CImageResource> m_BackBuffers;
};

}
