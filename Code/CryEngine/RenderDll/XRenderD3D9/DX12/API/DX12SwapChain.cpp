// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12SwapChain.hpp"
//#define I_WONT_USE_THE_FRAME_DEBUGGER // MSVC frame debugger has different internal ref-counts

namespace NCryDX12
{

CSwapChain* CSwapChain::Create(CCommandListPool& commandQueue, IDXGIFactory4ToCall* pFactory, DXGI_SWAP_CHAIN_DESC* pDesc)
{
	IDXGISwapChainToCall* pDXGISwapChain = nullptr;
	IDXGISwapChain3ToCall* pDXGISwapChain3 = nullptr;
	ID3D12CommandQueue* pQueue12 = commandQueue.GetD3D12CommandQueue();

	// If discard isn't implemented/supported/fails, try the newer swap-types
#if defined(__dxgi1_4_h__) || defined(__d3d11_x_h__)
	if (pDesc->SwapEffect == DXGI_SWAP_EFFECT_SEQUENTIAL)
	{
		// - flip_sequentially is win 8
		pDesc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		pDesc->BufferCount = std::max(2U, pDesc->BufferCount);
	}
#ifdef __dxgi1_4_h__
	else if (pDesc->SwapEffect == DXGI_SWAP_EFFECT_DISCARD)
	{
		// - flip_discard is win 10
		pDesc->SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		pDesc->BufferCount = std::max(2U, pDesc->BufferCount);
	}
#endif
#endif

#ifdef DX12_LINKEDADAPTER
	// Put swap-chain on node "0"
	if (commandQueue.GetDevice()->IsMultiAdapter())
	{
		pQueue12 = commandQueue.GetDevice()->GetNativeObject(pQueue12, 0);
	}
#endif

#ifdef __dxgi1_3_h__
	{
		BOOL bAllowTearing = FALSE;
		IDXGIFactory5ToCall* pFactory5 = nullptr;
		if (S_OK == pFactory->QueryInterface(IID_GFX_ARGS(&pFactory5)) &&
			S_OK == pFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bAllowTearing, sizeof(bAllowTearing)))
		{
		}

		if (bAllowTearing)
		{
			pDesc->Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		}
	}
#endif

	HRESULT hr = pFactory->CreateSwapChain(pQueue12, pDesc, &pDXGISwapChain);

	if (hr == S_OK && pDXGISwapChain)
	{
		hr = pDXGISwapChain->QueryInterface(IID_GFX_ARGS(&pDXGISwapChain3));
		pDXGISwapChain->Release();

		if (hr == S_OK && pDXGISwapChain3)
		{
			auto result = new CSwapChain(commandQueue, pDXGISwapChain3, pDesc);
			return result;
		}
	}

	return nullptr;
}

CSwapChain* CSwapChain::Create(CCommandListPool& commandQueue, IDXGISwapChain1ToCall* swapchain, DXGI_SWAP_CHAIN_DESC1* pDesc)
{
	IDXGISwapChainToCall* pDXGISwapChain = swapchain;
	IDXGISwapChain3ToCall* pDXGISwapChain3 = nullptr;
	ID3D12CommandQueue* pQueue12 = commandQueue.GetD3D12CommandQueue();

#ifdef DX12_LINKEDADAPTER
	// Put swap-chain on node "0"
	if (commandQueue.GetDevice()->IsMultiAdapter())
	{
		pQueue12 = commandQueue.GetDevice()->GetNativeObject(pQueue12, 0);
	}
#endif

	HRESULT hr = S_OK;

	if (hr == S_OK && pDXGISwapChain)
	{
		hr = pDXGISwapChain->QueryInterface(IID_GFX_ARGS(&pDXGISwapChain3));
		pDXGISwapChain->Release();

		if (hr == S_OK && pDXGISwapChain3)
		{
			auto result = new CSwapChain(commandQueue, pDXGISwapChain3, nullptr);
			return result;
		}
	}

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CSwapChain::CSwapChain(CCommandListPool& commandQueue, IDXGISwapChain3ToCall* dxgiSwapChain, DXGI_SWAP_CHAIN_DESC* pDesc)
	: CRefCounted()
	, m_bChangedBackBufferIndex(true)
	, m_nCurrentBackBufferIndex(0)
	, m_pCommandQueue(commandQueue)
	, m_asyncQueue(commandQueue.GetAsyncCommandQueue())
{
	if (pDesc)
	{
		m_Desc = *pDesc;
	}
	else
	{
		dxgiSwapChain->GetDesc(&m_Desc);
	}

	m_PresentResult = S_OK;
	m_pDXGISwapChain = dxgiSwapChain;
	dxgiSwapChain->Release();

	AcquireBuffers();
}

//---------------------------------------------------------------------------------------------------------------------
CSwapChain::~CSwapChain()
{
	ForfeitBuffers();

	m_pDXGISwapChain->Release();
}

HRESULT CSwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
{
	m_bChangedBackBufferIndex = true;

	return m_pDXGISwapChain->ResizeTarget(
	  pNewTargetParameters);
}

HRESULT CSwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	m_bChangedBackBufferIndex = true;

	m_pDXGISwapChain->GetDesc(&m_Desc);

	// DXGI ERROR: IDXGISwapChain::ResizeBuffers: Cannot add or remove the DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING flag using ResizeBuffers. 
	m_Desc.BufferCount = BufferCount ? BufferCount : m_Desc.BufferCount;
	m_Desc.BufferDesc.Width = Width ? Width : m_Desc.BufferDesc.Width;
	m_Desc.BufferDesc.Height = Height ? Height : m_Desc.BufferDesc.Height;
	m_Desc.BufferDesc.Format = NewFormat != DXGI_FORMAT_UNKNOWN ? NewFormat : m_Desc.BufferDesc.Format;
#ifdef __dxgi1_3_h__
	m_Desc.Flags = (m_Desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) | (SwapChainFlags & ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
#endif

	return m_pDXGISwapChain->ResizeBuffers(
	  m_Desc.BufferCount,
	  m_Desc.BufferDesc.Width,
	  m_Desc.BufferDesc.Height,
	  m_Desc.BufferDesc.Format,
	  m_Desc.Flags);
}

void CSwapChain::AcquireBuffers()
{
	CDevice* pDevice = m_pCommandQueue.GetDevice();

	m_BackBuffers.reserve(m_Desc.BufferCount);

#ifdef DX12_LINKEDADAPTER
	if (pDevice->IsMultiAdapter())
	{
		// Buffers created using ResizeBuffers1 with a non-null pCreationNodeMask array are visible to all nodes.
		std::vector<UINT> createNodeMasks(m_Desc.BufferCount, 0x1);
		std::vector<IUnknown*> presentCommandQueues(m_Desc.BufferCount, pDevice->GetNativeObject(m_pCommandQueue.GetD3D12CommandQueue(), 0));

		HRESULT ret = m_pDXGISwapChain->ResizeBuffers1(
		  m_Desc.BufferCount,
		  m_Desc.BufferDesc.Width,
		  m_Desc.BufferDesc.Height,
		  m_Desc.BufferDesc.Format,
		  m_Desc.Flags,
		  &createNodeMasks[0],
		  &presentCommandQueues[0]);

		DX12_ASSERT(ret == S_OK, "Failed to re-locate the swap-chain's backbuffer(s) to all device nodes!");
	}
#endif

	for (int i = 0; i < m_Desc.BufferCount; ++i)
	{
		ID3D12Resource* pResource12 = nullptr;
		if (S_OK == m_pDXGISwapChain->GetBuffer(i, IID_GFX_ARGS(&pResource12)))
		{
#ifdef DX12_LINKEDADAPTER
			if (pDevice->IsMultiAdapter())
			{
				ID3D12Resource* pResource12s[2] = { pResource12, nullptr };

				HRESULT ret = pDevice->DuplicateNativeCommittedResource(
					1U << 1U,
					pDevice->GetNodeMask(),
					pResource12s[0],
					&pResource12s[1]);

	#if !defined(RELEASE) && defined(I_WONT_USE_THE_FRAME_DEBUGGER)
				ULONG refCount0 = pResource12s[0]->AddRef() - 1;
				pResource12s[0]->Release();
				ULONG refCount1 = pResource12s[1]->AddRef() - 1;
				pResource12s[1]->Release();
				DX12_ASSERT(refCount0 == 1 + i, "RefCount corruption!");
				DX12_ASSERT(refCount1 == 1 + 0, "RefCount corruption!");
	#endif  // !RELEASE

				// Get broadcast resource from node "0" valid for all nodes
				pResource12 = pDevice->CreateBroadcastObject(pResource12s);

				ULONG refCount = pResource12s[0]->Release();

	#if !defined(RELEASE) && defined(I_WONT_USE_THE_FRAME_DEBUGGER)
				DX12_ASSERT(refCount == 1 + i, "RefCount corruption!");
	#endif  // !RELEASE
			}
#endif

			m_BackBuffers.emplace_back(pDevice);
			m_BackBuffers[i].Init(pResource12, D3D12_RESOURCE_STATE_PRESENT);
			m_BackBuffers[i].SetDX12SwapChain(this);

			ULONG refCount = pResource12->Release();

#if !defined(RELEASE) && defined(I_WONT_USE_THE_FRAME_DEBUGGER)
			if (pDevice->IsMultiAdapter())
			{
				DX12_ASSERT(refCount == 1 + 0, "RefCount corruption!");
			}
			else
			{
				DX12_ASSERT(refCount == 1 + i, "RefCount corruption!");
			}
#endif  // !RELEASE
		}
	}
}

void CSwapChain::UnblockBuffers(CCommandList* pCommandList)
{
	for (int i = 0; i < m_Desc.BufferCount; ++i)
	{
		D3D12_RESOURCE_STATES resourceState = m_BackBuffers[i].GetAnnouncedState();
		if (resourceState != (D3D12_RESOURCE_STATES)-1)
		{
			m_BackBuffers[i].EndTransitionBarrier(pCommandList, resourceState);
		}
	}
}

void CSwapChain::VerifyBufferCounters()
{
	CDevice* pDevice = m_pCommandQueue.GetDevice();

#if !defined(RELEASE) && defined(I_WONT_USE_THE_FRAME_DEBUGGER)
	for (int i = 0; i < m_Desc.BufferCount; ++i)
	{
		ID3D12Resource* pResource12 = m_BackBuffers[i].GetD3D12Resource();

		ULONG refCount = pResource12->AddRef() - 1;
		pResource12->Release();
		if (pDevice->IsMultiAdapter())
		{
			// 1x for m_BackBuffers
			DX12_ASSERT(refCount == 1, "RefCount corruption!");
		}
		else
		{
			// 1x for m_BackBuffers + counter is shared
			DX12_ASSERT(refCount == 1 * m_Desc.BufferCount, "RefCount corruption!");
		}
	}
#endif // !RELEASE
}

void CSwapChain::ForfeitBuffers()
{
	VerifyBufferCounters();

	m_BackBuffers.clear();
}

}
