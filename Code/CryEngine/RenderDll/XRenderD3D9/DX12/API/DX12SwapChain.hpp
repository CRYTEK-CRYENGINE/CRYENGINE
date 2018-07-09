// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"
#include "DX12CommandList.hpp"
#include "DX12AsyncCommandQueue.hpp"

namespace NCryDX12
{

class CSwapChain : public CRefCounted
{
public:
	static CSwapChain* Create(CCommandListPool& commandQueue, IDXGIFactory4ToCall* factory, DXGI_SWAP_CHAIN_DESC* pDesc);
	static CSwapChain* Create(CCommandListPool& commandQueue, IDXGISwapChain1ToCall* swapchain, DXGI_SWAP_CHAIN_DESC1* pDesc);

protected:
	CSwapChain(CCommandListPool& commandQueue, IDXGISwapChain3ToCall* dxgiSwapChain, DXGI_SWAP_CHAIN_DESC* pDesc);

	virtual ~CSwapChain();

public:
	ILINE IDXGISwapChain3ToCall* GetDXGISwapChain() const
	{
		return m_pDXGISwapChain;
	}

	ILINE UINT GetBackBufferCount()
	{
		return m_Desc.BufferCount;
	}

	// Get the index of the back-buffer which is to be used for the next Present().
	// Cache the value for multiple calls between Present()s, as the cost of GetCurrentBackBufferIndex() could be high.
	ILINE UINT GetCurrentBackbufferIndex() const
	{
		if (!m_bChangedBackBufferIndex)
			return m_nCurrentBackBufferIndex;

		m_asyncQueue.FlushNextPresent(); DX12_ASSERT(!IsPresentScheduled(), "Flush didn't dry out all outstanding Present() calls!");
#if CRY_PLATFORM_DURANGO
		m_nCurrentBackBufferIndex = 0;
#else
		m_nCurrentBackBufferIndex = m_pDXGISwapChain->GetCurrentBackBufferIndex();
#endif
		m_bChangedBackBufferIndex = false;

		return m_nCurrentBackBufferIndex;
	}
	ILINE CResource& GetBackBuffer(UINT index)
	{
		return m_BackBuffers[index];
	}
	ILINE CResource& GetCurrentBackBuffer()
	{
		return m_BackBuffers[GetCurrentBackbufferIndex()];
	}

	ILINE bool IsPresentScheduled() const
	{
		return m_asyncQueue.GetQueuedFramesCount() > 0;
	}

	// Run Present() asynchronuously, which means the next back-buffer index is not queryable within this function.
	// Instead defer acquiring the next index to the next call of GetCurrentBackbufferIndex().
	void Present(UINT SyncInterval, UINT Flags)
	{
		m_asyncQueue.Present(m_pDXGISwapChain, &m_PresentResult, SyncInterval, Flags, m_Desc, GetCurrentBackbufferIndex());
		m_bChangedBackBufferIndex = true;
	}
	
	ILINE HRESULT SetFullscreenState(BOOL Fullscreen, IDXGIOutput4ToCall* pTarget)
	{
		m_Desc.Windowed = !Fullscreen;
		return m_pDXGISwapChain->SetFullscreenState(Fullscreen, pTarget);
	}

	HRESULT GetLastPresentReturnValue() { return m_PresentResult; }

	HRESULT ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters);
	HRESULT ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

	void    AcquireBuffers();
	void    UnblockBuffers(NCryDX12::CCommandList* pCommandList);
	void    VerifyBufferCounters();
	void    ForfeitBuffers();

private:
	CAsyncCommandQueue&  m_asyncQueue;
	CCommandListPool&    m_pCommandQueue;

	DXGI_SWAP_CHAIN_DESC m_Desc;
	mutable bool         m_bChangedBackBufferIndex;
	mutable UINT         m_nCurrentBackBufferIndex;

	HRESULT              m_PresentResult;
	DX12_PTR(IDXGISwapChain3ToCall) m_pDXGISwapChain;

	std::vector<CResource> m_BackBuffers;
};

}
