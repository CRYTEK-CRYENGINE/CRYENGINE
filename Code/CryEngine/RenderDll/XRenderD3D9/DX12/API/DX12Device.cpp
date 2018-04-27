// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12Device.hpp"
#include "DX12CommandList.hpp"
#include "DX12CommandScheduler.hpp"
#include "DX12Resource.hpp"

#define DX12_GLOBALHEAP_RESOURCES (1 << D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
#define DX12_GLOBALHEAP_SAMPLERS  (1 << D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
#define DX12_GLOBALHEAP_TYPES     DX12_GLOBALHEAP_RESOURCES

#ifdef  DX12_LINKEDADAPTER
	#define INCLUDE_STATICS
	#include "Redirections/D3D12Device.inl"
#endif

#include <atlbase.h>

namespace NCryDX12 {

//---------------------------------------------------------------------------------------------------------------------
CDevice* CDevice::Create(CCryDX12GIAdapter* pAdapter, D3D_FEATURE_LEVEL* pFeatureLevel)
{
	ID3D12Device* pDevice12 = NULL;

	if (CRenderer::CV_r_EnableDebugLayer)
	{
		ID3D12Debug* debugInterface = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_GFX_ARGS(&debugInterface))))
		{
			debugInterface->EnableDebugLayer();

			if (CRenderer::CV_r_EnableDebugLayer == 2)
			{
				// Enable DX12 GBV as well
				CComPtr<ID3D12Debug1> spDebugController1;
				if (SUCCEEDED(debugInterface->QueryInterface(IID_PPV_ARGS(&spDebugController1))))
					spDebugController1->SetEnableGPUBasedValidation(true);
			}
		}
	}

	IGfxUnknown* pDXGIAdapter = pAdapter ? pAdapter->GetDXGIAdapter() : nullptr;

#ifndef CRY_PLATFORM_CONSOLE
	// On console there is only one adapter (the default one which can be reached using nullptr)
	if (pDXGIAdapter == nullptr)
	{
		DX12_ASSERT(0, "No adapter available to create D3D12 Device!");
		return NULL;
	}
#endif // !CRY_PLATFORM_CONSOLE

	D3D_FEATURE_LEVEL level;
	HRESULT hr =
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_12_1, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_12_0, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
//	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_11_3, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
//	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_11_2, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_11_1, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_11_0, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_10_1, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_10_0, IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_9_3 , IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_9_2 , IID_GFX_ARGS(&pDevice12)) == S_OK) ||
	  (D3D12CreateDevice(pDXGIAdapter, level = D3D_FEATURE_LEVEL_9_1 , IID_GFX_ARGS(&pDevice12)) == S_OK) ? S_OK : S_FALSE;

	if (hr != S_OK)
	{
		DX12_ASSERT(0, "Failed to create D3D12 Device!");
		return NULL;
	}

	UINT nodeMask = 0;
	UINT nodeCount = 1;

#ifdef DX12_LINKEDADAPTER
	nodeCount = pDevice12->GetNodeCount();
	if (gcpRendD3D->GetS3DRend().GetDevice() != EStereoDevice::STEREO_DEVICE_NONE || !CRenderer::CV_r_StereoEnableMgpu)
		nodeCount = 1;

	if (CRenderer::CV_r_StereoEnableMgpu && int(nodeCount) > 1)
	{
		nodeMask = (1UL << 2) - 1UL;
		switch (nodeCount)
		{
		case  2:
			pDevice12 = new BroadcastableD3D12Device<2>(pDevice12, __uuidof(*pDevice12));
			break;
		case  3:
			pDevice12 = new BroadcastableD3D12Device<2>(pDevice12, __uuidof(*pDevice12));
			break;
		case  4:
			pDevice12 = new BroadcastableD3D12Device<2>(pDevice12, __uuidof(*pDevice12));
			break;
		default:
			pDevice12 = new BroadcastableD3D12Device<2>(pDevice12, __uuidof(*pDevice12));
			break;
		}
	}
#endif

	if (pFeatureLevel)
	{
		*pFeatureLevel = level;
	}

	CDevice* result = new CDevice(pDevice12, level, nodeCount, nodeMask);

	pDevice12->Release();

	return result;
}

#ifdef DX12_LINKEDADAPTER
bool CDevice::IsMultiAdapter() const
{
	return CRenderer::CV_r_StereoEnableMgpu && (int(m_nodeCount) > 1);
}

ID3D12CommandQueue* CDevice::GetNativeObject(ID3D12CommandQueue* pQueue, UINT node) const
{
	if (IsMultiAdapter())
	{
		return *(*((BroadcastableD3D12CommandQueue<2>*)pQueue))[node];
	}

	return pQueue;
}

ID3D12Resource* CDevice::CreateBroadcastObject(ID3D12Resource** pResources) const
{
	if (IsMultiAdapter())
	{
		return new BroadcastableD3D12Resource<2>(GetD3D12Device(), pResources, __uuidof(**pResources));
	}

	return pResources[0];
}

bool CDevice::WaitForCompletion(ID3D12Fence* pFence, UINT64 fenceValue) const
{
	if (IsMultiAdapter())
	{
		((BroadcastableD3D12Fence<2>*)pFence)->WaitForCompletion(fenceValue);
		return true;
	}

	return false;
}

//---------------------------------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CDevice::DuplicateNativeCommittedResource(
  _In_ UINT creationMask,
  _In_ UINT visibilityMask,
  _In_ ID3D12Resource* pInputResource,
  _Out_ ID3D12Resource** ppOutputResource)
{
	D3D12_HEAP_PROPERTIES sHeap;
	D3D12_RESOURCE_DESC resourceDesc = pInputResource->GetDesc();

	pInputResource->GetHeapProperties(&sHeap, nullptr);

	D3D12_RESOURCE_STATES initialState =
	  (sHeap.Type == D3D12_HEAP_TYPE_DEFAULT ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE :
	   (sHeap.Type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST :
	    (sHeap.Type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ :
	     D3D12_RESOURCE_STATE_GENERIC_READ)));

	if (sHeap.Type == D3D12_HEAP_TYPE_DEFAULT)
	{
		if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		{
			initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		}

		if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
	}

	sHeap.CreationNodeMask = creationMask;
	sHeap.VisibleNodeMask = visibilityMask;

	// Can't use recycle-heap because it's a native resource (the recycle-heap consists of broadcast-resources)
	ID3D12Device* realDevice = ((BroadcastableD3D12Device<2>*)m_pDevice.get())->m_Target;
	ID3D12Resource* outputResource = nullptr;
	HRESULT result = realDevice->CreateCommittedResource(
		&sHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_GFX_ARGS(&outputResource));

	if (result == S_OK && outputResource != nullptr)
	{
		*ppOutputResource = outputResource;

		return S_OK;
	}

	return result;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
CDevice::CDevice(ID3D12Device* d3d12Device, D3D_FEATURE_LEVEL featureLevel, UINT nodeCount, UINT nodeMask)
	: m_pDevice(d3d12Device)
	, m_featureLevel(featureLevel)
	, m_nodeCount(nodeCount)
	, m_nodeMask(nodeMask)
	, m_SamplerCache(this)
	, m_ShaderResourceDescriptorCache(this)
	, m_UnorderedAccessDescriptorCache(this)
	, m_DepthStencilDescriptorCache(this)
	, m_RenderTargetDescriptorCache(this)
	, m_ResourceDescriptorScratchSpace(this)
#if defined(_ALLOW_INITIALIZER_LISTS)
	// *INDENT-OFF*
	, m_GlobalDescriptorHeaps
	{
#ifdef __d3d12_x_h__
		{ this },
#endif
		{ this },
		{ this },
		{ this },
		{ this }
	}
	// *INDENT-ON*
#endif
	// Must be constructed last as it relies on functionality from the heaps
	, m_Scheduler(this, nodeMask)
{
#if !defined(_ALLOW_INITIALIZER_LISTS)
#ifdef __d3d12_x_h__
	m_GlobalDescriptorHeaps.emplace_back(this);
#endif
	m_GlobalDescriptorHeaps.emplace_back(this);
	m_GlobalDescriptorHeaps.emplace_back(this);
	m_GlobalDescriptorHeaps.emplace_back(this);
	m_GlobalDescriptorHeaps.emplace_back(this);
#endif

	m_PSOCache.Init(this);
	m_RootSignatureCache.Init(this);

	// init sampler cache
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_SamplerCache.Init(desc);
	}

	// init shader resource descriptor cache
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65535, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_ShaderResourceDescriptorCache.Init(desc);
	}

	// init unordered access descriptor cache
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_UnorderedAccessDescriptorCache.Init(desc);
	}

	// init depth stencil descriptor cache
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_DepthStencilDescriptorCache.Init(desc);
	}

	// init render target descriptor cache
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_RenderTargetDescriptorCache.Init(desc);
	}

	// init cpu accessible descriptor scratch space
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, m_nodeMask };

		m_ResourceDescriptorScratchSpace.Init(desc);
		m_ResourceDescriptorScratchSpace.AddRef(); // Refcount 1 to make sure CDescriptorBlock won't think it's the only owner
	}

	// init global descriptor heaps

	static uint32 globalHeapSizes[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
	{
		1000000,  // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
		1024,     // D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
		256,      // D3D12_DESCRIPTOR_HEAP_TYPE_RTV
		256       // D3D12_DESCRIPTOR_HEAP_TYPE_DSV
	};

	for (D3D12_DESCRIPTOR_HEAP_TYPE eType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; eType < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; eType = D3D12_DESCRIPTOR_HEAP_TYPE(eType + 1))
	{
		if (DX12_GLOBALHEAP_TYPES & (1 << eType))
		{
			D3D12_DESCRIPTOR_HEAP_DESC desc =
			{
				eType,
				globalHeapSizes[eType],
				(eType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || eType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE),
				m_nodeMask
			};

			m_GlobalDescriptorHeaps[eType].Init(desc);
		}
	}

	m_Scheduler.BeginScheduling();
}

//---------------------------------------------------------------------------------------------------------------------
CDevice::~CDevice()
{
	UINT64 clearFences[CMDQUEUE_NUM] = { 0ULL, 0ULL, 0ULL };

	// Kill all entries in the allocation cache
	FlushReleaseHeap(clearFences, clearFences);
}

//---------------------------------------------------------------------------------------------------------------------
void CDevice::RequestUploadHeapMemory(UINT64 size, DX12_PTR(ID3D12Resource)& result)
{
	ID3D12Resource* resource = NULL;

	if (S_OK != CreateOrReuseCommittedResource(
	      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
	      D3D12_HEAP_FLAG_NONE,
	      &CD3DX12_RESOURCE_DESC::Buffer(size),
	      D3D12_RESOURCE_STATE_GENERIC_READ,
	      nullptr,
	      IID_GFX_ARGS(&resource)) || !resource)
	{
		DX12_ASSERT(0, "Could not create upload heap memory buffer!");
		return;
	}

	result = resource;
	resource->Release();
}

//---------------------------------------------------------------------------------------------------------------------
CDescriptorBlock CDevice::GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE eType, UINT size)
{
	CryAutoLock<CryCriticalSectionNonRecursive> m_DescriptorAllocatorTheadSafeScope(m_DescriptorAllocatorTheadSafeScope);

	if (DX12_GLOBALHEAP_TYPES & (1 << eType))
	{
		DX12_ASSERT(int64(m_GlobalDescriptorHeaps[eType].GetCapacity()) - int64(m_GlobalDescriptorHeaps[eType].GetCursor()) >= int64(size));
		CDescriptorBlock result(&m_GlobalDescriptorHeaps[eType], m_GlobalDescriptorHeaps[eType].GetCursor(), size);
		m_GlobalDescriptorHeaps[eType].IncrementCursor(size);
		return result;
	}

	CDescriptorHeap* pResourceHeap = DX12_NEW_RAW(CDescriptorHeap(this));

	D3D12_DESCRIPTOR_HEAP_DESC desc = {
		eType,
		size,
		(eType == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || eType == D3D12_DESCRIPTOR_HEAP_TYPE_DSV ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE),
		m_nodeMask
	};

	pResourceHeap->Init(desc);

	CDescriptorBlock result(pResourceHeap, pResourceHeap->GetCursor(), size);
	pResourceHeap->Release();
	return result;
}

//---------------------------------------------------------------------------------------------------------------------
THash CDevice::GetSamplerHash(const D3D12_SAMPLER_DESC* pDesc)
{
	return ComputeSmallHash<sizeof(D3D12_SAMPLER_DESC)>(pDesc);
}

THash CDevice::GetShaderResourceViewHash(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, const ID3D12Resource* pResource)
{
	return ComputeSmallHash<sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC)>(pDesc, (UINT32)((UINT64)pResource));
}

THash CDevice::GetUnorderedAccessViewHash(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, const ID3D12Resource* pResource)
{
	return ComputeSmallHash<sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC)>(pDesc, (UINT32)((UINT64)pResource));
}

THash CDevice::GetDepthStencilViewHash(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, const ID3D12Resource* pResource)
{
	return ComputeSmallHash<sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC)>(pDesc, (UINT32)((UINT64)pResource));
}

THash CDevice::GetRenderTargetViewHash(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, const ID3D12Resource* pResource)
{
	return ComputeSmallHash<sizeof(D3D12_RENDER_TARGET_VIEW_DESC)>(pDesc, (UINT32)((UINT64)pResource));
}

//---------------------------------------------------------------------------------------------------------------------
CryCriticalSectionNonRecursive CDevice::m_SamplerThreadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_ShaderResourceThreadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_UnorderedAccessThreadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_DepthStencilThreadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_RenderTargetThreadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_DescriptorAllocatorTheadSafeScope;

CryCriticalSectionNonRecursive CDevice::m_ReleaseHeapTheadSafeScope;
CryCriticalSectionNonRecursive CDevice::m_RecycleHeapTheadSafeScope;

//---------------------------------------------------------------------------------------------------------------------
void CDevice::InvalidateSampler(const D3D12_SAMPLER_DESC* pDesc)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_SamplerThreadSafeScope);

	auto itCachedSampler = m_SamplerCacheLookupTable.find(GetSamplerHash(pDesc));
	if (itCachedSampler != m_SamplerCacheLookupTable.end())
	{
		m_SamplerCacheFreeTable.push_back(itCachedSampler->second);
		m_SamplerCacheLookupTable.erase(itCachedSampler);
	}
}

void CDevice::InvalidateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_ShaderResourceThreadSafeScope);

	auto itCachedSRV = m_ShaderResourceDescriptorLookupTable.find(GetShaderResourceViewHash(pDesc, pResource));
	if (itCachedSRV != m_ShaderResourceDescriptorLookupTable.end())
	{
		m_ShaderResourceDescriptorFreeTable.push_back(itCachedSRV->second);
		m_ShaderResourceDescriptorLookupTable.erase(itCachedSRV);
	}
}

void CDevice::InvalidateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_UnorderedAccessThreadSafeScope);

	auto itCachedUAV = m_UnorderedAccessDescriptorLookupTable.find(GetUnorderedAccessViewHash(pDesc, pResource));
	if (itCachedUAV != m_UnorderedAccessDescriptorLookupTable.end())
	{
		m_UnorderedAccessDescriptorFreeTable.push_back(itCachedUAV->second);
		m_UnorderedAccessDescriptorLookupTable.erase(itCachedUAV);
	}
}

void CDevice::InvalidateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_DepthStencilThreadSafeScope);

	auto itCachedDSV = m_DepthStencilDescriptorLookupTable.find(GetDepthStencilViewHash(pDesc, pResource));
	if (itCachedDSV != m_DepthStencilDescriptorLookupTable.end())
	{
		m_DepthStencilDescriptorFreeTable.push_back(itCachedDSV->second);
		m_DepthStencilDescriptorLookupTable.erase(itCachedDSV);
	}
}

void CDevice::InvalidateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RenderTargetThreadSafeScope);

	auto itCachedRTV = m_RenderTargetDescriptorLookupTable.find(GetRenderTargetViewHash(pDesc, pResource));
	if (itCachedRTV != m_RenderTargetDescriptorLookupTable.end())
	{
		m_RenderTargetDescriptorFreeTable.push_back(itCachedRTV->second);
		m_RenderTargetDescriptorLookupTable.erase(itCachedRTV);
	}
}

//---------------------------------------------------------------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE CDevice::CacheSampler(const D3D12_SAMPLER_DESC* pDesc)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_SamplerThreadSafeScope);

	THash hHash = GetSamplerHash(pDesc);

	auto itCachedSampler = m_SamplerCacheLookupTable.find(hHash);
	if (itCachedSampler == m_SamplerCacheLookupTable.end())
	{
		if (!m_SamplerCacheFreeTable.size() && (m_SamplerCache.GetCursor() >= m_SamplerCache.GetCapacity()))
		{
			DX12_ASSERT(false, "Sampler heap is too small!");
			return INVALID_CPU_DESCRIPTOR_HANDLE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;
		if (m_SamplerCacheFreeTable.size())
		{
			dstHandle = m_SamplerCacheFreeTable.front();
			m_SamplerCacheFreeTable.pop_front();
		}
		else
		{
			dstHandle = m_SamplerCache.GetHandleOffsetCPU(0);
			m_SamplerCache.IncrementCursor();
		}

		GetD3D12Device()->CreateSampler(pDesc, dstHandle);

		auto insertResult = m_SamplerCacheLookupTable.emplace(hHash, dstHandle);
		DX12_ASSERT(insertResult.second);
		itCachedSampler = insertResult.first;
	}

	return itCachedSampler->second;
}

D3D12_CPU_DESCRIPTOR_HANDLE CDevice::CacheShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_ShaderResourceThreadSafeScope);

	// In case of null resource: make sure we have a valid format. TODO: never feed the function with UNKNOWN
	D3D12_SHADER_RESOURCE_VIEW_DESC Desc = *pDesc;
	if (!pResource)
	{
		Desc.Format = DXGI_FORMAT_R32_UINT;
	}

	THash hHash = GetShaderResourceViewHash(&Desc, pResource);

	auto itCachedSRV = m_ShaderResourceDescriptorLookupTable.find(hHash);
	if (itCachedSRV == m_ShaderResourceDescriptorLookupTable.end())
	{
		if (!m_ShaderResourceDescriptorFreeTable.size() && (m_ShaderResourceDescriptorCache.GetCursor() >= m_ShaderResourceDescriptorCache.GetCapacity()))
		{
			DX12_ASSERT(false, "ShaderResourceView heap is too small!");
			return INVALID_CPU_DESCRIPTOR_HANDLE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;
		if (m_ShaderResourceDescriptorFreeTable.size())
		{
			dstHandle = m_ShaderResourceDescriptorFreeTable.front();
			m_ShaderResourceDescriptorFreeTable.pop_front();
		}
		else
		{
			dstHandle = m_ShaderResourceDescriptorCache.GetHandleOffsetCPU(0);
			m_ShaderResourceDescriptorCache.IncrementCursor();
		}

		GetD3D12Device()->CreateShaderResourceView(pResource, &Desc, dstHandle);

		auto insertResult = m_ShaderResourceDescriptorLookupTable.emplace(hHash, dstHandle);
		DX12_ASSERT(insertResult.second);
		itCachedSRV = insertResult.first;
	}

	return itCachedSRV->second;
}

D3D12_CPU_DESCRIPTOR_HANDLE CDevice::CacheUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_UnorderedAccessThreadSafeScope);

	THash hHash = GetUnorderedAccessViewHash(pDesc, pResource);

	auto itCachedUAV = m_UnorderedAccessDescriptorLookupTable.find(hHash);
	if (itCachedUAV == m_UnorderedAccessDescriptorLookupTable.end())
	{
		if (!m_UnorderedAccessDescriptorFreeTable.size() && (m_UnorderedAccessDescriptorCache.GetCursor() >= m_UnorderedAccessDescriptorCache.GetCapacity()))
		{
			DX12_ASSERT(false, "UnorderedAccessView heap is too small!");
			return INVALID_CPU_DESCRIPTOR_HANDLE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;
		if (m_UnorderedAccessDescriptorFreeTable.size())
		{
			dstHandle = m_UnorderedAccessDescriptorFreeTable.front();
			m_UnorderedAccessDescriptorFreeTable.pop_front();
		}
		else
		{
			dstHandle = m_UnorderedAccessDescriptorCache.GetHandleOffsetCPU(0);
			m_UnorderedAccessDescriptorCache.IncrementCursor();
		}

		GetD3D12Device()->CreateUnorderedAccessView(pResource, nullptr, pDesc, dstHandle);

		auto insertResult = m_UnorderedAccessDescriptorLookupTable.emplace(hHash, dstHandle);
		DX12_ASSERT(insertResult.second);
		itCachedUAV = insertResult.first;
	}

	return itCachedUAV->second;
}

D3D12_CPU_DESCRIPTOR_HANDLE CDevice::CacheDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_DepthStencilThreadSafeScope);

	THash hHash = GetDepthStencilViewHash(pDesc, pResource);

	auto itCachedDSV = m_DepthStencilDescriptorLookupTable.find(hHash);
	if (itCachedDSV == m_DepthStencilDescriptorLookupTable.end())
	{
		if (!m_DepthStencilDescriptorFreeTable.size() && (m_DepthStencilDescriptorCache.GetCursor() >= m_DepthStencilDescriptorCache.GetCapacity()))
		{
			DX12_ASSERT(false, "DepthStencilView heap is too small!");
			return INVALID_CPU_DESCRIPTOR_HANDLE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;
		if (m_DepthStencilDescriptorFreeTable.size())
		{
			dstHandle = m_DepthStencilDescriptorFreeTable.front();
			m_DepthStencilDescriptorFreeTable.pop_front();
		}
		else
		{
			dstHandle = m_DepthStencilDescriptorCache.GetHandleOffsetCPU(0);
			m_DepthStencilDescriptorCache.IncrementCursor();
		}

		GetD3D12Device()->CreateDepthStencilView(pResource, pDesc, dstHandle);

		auto insertResult = m_DepthStencilDescriptorLookupTable.emplace(hHash, dstHandle);
		DX12_ASSERT(insertResult.second);
		itCachedDSV = insertResult.first;
	}

	return itCachedDSV->second;
}

D3D12_CPU_DESCRIPTOR_HANDLE CDevice::CacheRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, ID3D12Resource* pResource)
{
	CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RenderTargetThreadSafeScope);

	THash hHash = GetRenderTargetViewHash(pDesc, pResource);

	auto itCachedRTV = m_RenderTargetDescriptorLookupTable.find(hHash);
	if (itCachedRTV == m_RenderTargetDescriptorLookupTable.end())
	{
		if (!m_RenderTargetDescriptorFreeTable.size() && (m_RenderTargetDescriptorCache.GetCursor() >= m_RenderTargetDescriptorCache.GetCapacity()))
		{
			DX12_ASSERT(false, "DepthStencilView heap is too small!");
			return INVALID_CPU_DESCRIPTOR_HANDLE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;
		if (m_RenderTargetDescriptorFreeTable.size())
		{
			dstHandle = m_RenderTargetDescriptorFreeTable.front();
			m_RenderTargetDescriptorFreeTable.pop_front();
		}
		else
		{
			dstHandle = m_RenderTargetDescriptorCache.GetHandleOffsetCPU(0);
			m_RenderTargetDescriptorCache.IncrementCursor();
		}

		GetD3D12Device()->CreateRenderTargetView(pResource, pDesc, dstHandle);

		auto insertResult = m_RenderTargetDescriptorLookupTable.emplace(hHash, dstHandle);
		DX12_ASSERT(insertResult.second);
		itCachedRTV = insertResult.first;
	}

	return itCachedRTV->second;
}

//---------------------------------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CDevice::CreateOrReuseStagingResource(
  _In_ ID3D12Resource* pInputResource,
  _Out_ ID3D12Resource** ppStagingResource,
  _In_ BOOL Upload) threadsafe
{
	D3D12_HEAP_PROPERTIES sHeap;
	D3D12_RESOURCE_DESC resourceDesc = pInputResource->GetDesc();
	UINT64 requiredSize, rowPitch;
	UINT rowCount;
	UINT numSubResources = 1;// resourceDesc.MipLevels * (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : resourceDesc.DepthOrArraySize);
	GetD3D12Device()->GetCopyableFootprints(&resourceDesc, 0, numSubResources, 0, nullptr, &rowCount, &rowPitch, &requiredSize);

	D3D12_RESOURCE_STATES initialState = Upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
	D3D12_HEAP_TYPE heapType = Upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_READBACK;

	pInputResource->GetHeapProperties(&sHeap, nullptr);

	// NOTE: this is a staging resource, no flags permitting views are allowed
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	ID3D12Resource* stagingResource = NULL;
	HRESULT result = CreateOrReuseCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType, blsi(sHeap.CreationNodeMask), sHeap.CreationNodeMask),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(requiredSize),
		initialState,
		nullptr,
		IID_GFX_ARGS(&stagingResource));

	if (result == S_OK && stagingResource != nullptr)
	{
		*ppStagingResource = stagingResource;

		return S_OK;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CDevice::DuplicateCommittedResource(
	_In_ ID3D12Resource* pInputResource,
	_In_ D3D12_RESOURCE_STATES OutputState,
	_Out_ ID3D12Resource** ppOutputResource)
{
	D3D12_HEAP_PROPERTIES sHeap;
	D3D12_RESOURCE_DESC resourceDesc = pInputResource->GetDesc();

	pInputResource->GetHeapProperties(&sHeap, nullptr);

	ID3D12Resource* outputResource = nullptr;
	HRESULT result = CreateOrReuseCommittedResource(
		&sHeap,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		OutputState,
		nullptr,
		IID_GFX_ARGS(&outputResource));

	if (result == S_OK && outputResource != nullptr)
	{
#ifdef DX12_LINKEDADAPTER
		if (IsMultiAdapter())
		{
			((BroadcastableD3D12Device<2>*)m_pDevice.get())->DuplicateMetaData(
				pInputResource,
				outputResource
			);
		}
#endif

		*ppOutputResource = outputResource;

		return S_OK;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CDevice::SubstituteUsedCommittedResource(
	_In_ const FVAL64 (&fenceValues)[CMDQUEUE_NUM],
	_In_ D3D12_RESOURCE_STATES OutputState,
	_Inout_ ID3D12Resource** ppSubstituteResource) threadsafe
{
	const auto& fenceManager = GetScheduler().GetFenceManager();
	if (fenceManager.IsCompleted(fenceValues[CMDQUEUE_GRAPHICS], CMDQUEUE_GRAPHICS) &&
		fenceManager.IsCompleted(fenceValues[CMDQUEUE_COMPUTE], CMDQUEUE_COMPUTE) &&
		fenceManager.IsCompleted(fenceValues[CMDQUEUE_COPY], CMDQUEUE_COPY))
	{
		// Can continued to be used without substitution
		return S_FALSE;
	}

	ID3D12Resource* pDisposableResource = *ppSubstituteResource;
	HRESULT result = DuplicateCommittedResource(pDisposableResource, OutputState, ppSubstituteResource);

	if (result == S_OK && *ppSubstituteResource != nullptr)
	{
		ReleaseLater(fenceValues, pDisposableResource, true);

		return S_OK;
	}

	return result;
}

//---------------------------------------------------------------------------------------------------------------------
HRESULT STDMETHODCALLTYPE CDevice::CreateOrReuseCommittedResource(
	_In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
	_In_ D3D12_HEAP_FLAGS HeapFlags,
	_In_ const D3D12_RESOURCE_DESC* pResourceDesc,
	_In_ D3D12_RESOURCE_STATES InitialResourceState,
	_In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	_In_ REFIID riidResource,
	_COM_Outptr_opt_ void** ppvResource) threadsafe
{
	struct
	{
		D3D12_HEAP_FLAGS      sHeapFlags;
		D3D12_HEAP_PROPERTIES sHeapProperties;
		D3D12_RESOURCE_DESC   sResourceDesc;
	}
	hashableBlob;

	hashableBlob.sHeapProperties = *pHeapProperties;
	hashableBlob.sResourceDesc = *pResourceDesc;
	hashableBlob.sHeapFlags = HeapFlags;
	hashableBlob.sResourceDesc.Alignment = 0; // alignment is intrinsic

	// Clear spaces from alignment of members
	void* ptr1 = ((char*)&hashableBlob.sResourceDesc.Dimension) + sizeof(hashableBlob.sResourceDesc.Dimension);
	ZeroMemory(ptr1, offsetof(D3D12_RESOURCE_DESC, Alignment) - sizeof(hashableBlob.sResourceDesc.Dimension));
	void* ptr2 = ((char*)&hashableBlob.sResourceDesc.Flags) + sizeof(hashableBlob.sResourceDesc.Flags);
	ZeroMemory(ptr2, sizeof(hashableBlob.sResourceDesc) - offsetof(D3D12_RESOURCE_DESC, Flags) - sizeof(hashableBlob.sResourceDesc.Flags));

	THash hHash = ComputeSmallHash<sizeof(hashableBlob)>(&hashableBlob);

	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RecycleHeapTheadSafeScope);

		// Best-case O(1) lookup
		TRecycleHeap::iterator result = m_RecycleHeap.find(hHash);
		if (result != m_RecycleHeap.end())
		{
			if (ppvResource)
			{
				*ppvResource = result->second.pObject;

				m_RecycleHeap.erase(result);
			}

			return S_OK;
		}
	}

	return GetD3D12Device()->CreateCommittedResource(
	  pHeapProperties, HeapFlags, pResourceDesc, InitialResourceState,
	  pOptimizedClearValue, riidResource, ppvResource);
}

//---------------------------------------------------------------------------------------------------------------------
void CDevice::FlushReleaseHeap(const UINT64 (&completedFenceValues)[CMDQUEUE_NUM], const UINT64(&pruneFenceValues)[CMDQUEUE_NUM]) threadsafe
{
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_ReleaseHeapTheadSafeScope);

		TReleaseHeap::iterator it, nx;
		for (it = m_ReleaseHeap.begin(); it != m_ReleaseHeap.end(); it = nx)
		{
			nx = it;
			++nx;

			if (((it->second.fenceValues[CMDQUEUE_GRAPHICS]) <= completedFenceValues[CMDQUEUE_GRAPHICS]) &&
			    ((it->second.fenceValues[CMDQUEUE_COMPUTE]) <= completedFenceValues[CMDQUEUE_COMPUTE]) &&
			    ((it->second.fenceValues[CMDQUEUE_COPY]) <= completedFenceValues[CMDQUEUE_COPY]))
			{
				if (it->second.bFlags & 1)
				{
					CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RecycleHeapTheadSafeScope);

					RecycleInfo recycleInfo;

					recycleInfo.pObject = it->first;
					recycleInfo.fenceValues[CMDQUEUE_GRAPHICS] = completedFenceValues[CMDQUEUE_GRAPHICS];
					recycleInfo.fenceValues[CMDQUEUE_COMPUTE] = completedFenceValues[CMDQUEUE_COMPUTE];
					recycleInfo.fenceValues[CMDQUEUE_COPY] = completedFenceValues[CMDQUEUE_COPY];

					// TODO: could be accumulated to a local list and batch-merged in the next code-block to prevent the locking
					m_RecycleHeap.emplace(it->second.hHash, std::move(recycleInfo));
				}
				else
				{
					it->first->Release();
				}

				m_ReleaseHeap.erase(it);
			}
		}
	}

	//	if (0)
	{
		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RecycleHeapTheadSafeScope);

		TRecycleHeap::iterator it, nx;
		for (it = m_RecycleHeap.begin(); it != m_RecycleHeap.end(); it = nx)
		{
			nx = it;
			++nx;

			if (((it->second.fenceValues[CMDQUEUE_GRAPHICS]) <= pruneFenceValues[CMDQUEUE_GRAPHICS]) &&
			    ((it->second.fenceValues[CMDQUEUE_COMPUTE]) <= pruneFenceValues[CMDQUEUE_COMPUTE]) &&
			    ((it->second.fenceValues[CMDQUEUE_COPY]) <= pruneFenceValues[CMDQUEUE_COPY]))
			{
				// NOTE: some of the objects are referenced by multiple classes,
				// so even when the CResource is destructed and the d3d resource
				// given up for release, they can continue being in use
				// This means the ref-count here doesn't necessarily need to be 0
				ULONG counter = it->second.pObject->Release();
				DX12_ASSERT(counter == 0, "Ref-Counter of D3D12 resource is not 0, memory will leak!");
				m_RecycleHeap.erase(it);
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CDevice::ReleaseLater(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], ID3D12Resource* pObject, bool bReusable) threadsafe
{
	if (pObject)
	{
		struct
		{
			D3D12_HEAP_FLAGS      sHeapFlags;
			D3D12_HEAP_PROPERTIES sHeapProperties;
			D3D12_RESOURCE_DESC   sResourceDesc;
		}
		hashableBlob;

		pObject->GetHeapProperties(&hashableBlob.sHeapProperties, &hashableBlob.sHeapFlags);
		hashableBlob.sResourceDesc = pObject->GetDesc();
		// When creating a committed resource, D3D12_HEAP_FLAGS must not have either D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES,
		// D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES, nor D3D12_HEAP_FLAG_DENY_BUFFERS set. These flags will be set automatically
		// to correspond with the committed resource type.
		hashableBlob.sHeapFlags = D3D12_HEAP_FLAGS(hashableBlob.sHeapFlags & ~(D3D12_HEAP_FLAG_DENY_BUFFERS + D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES + D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES));
		hashableBlob.sResourceDesc.Alignment = 0; // alignment is intrinsic

		// Clear spaces from alignment of members
		void* ptr1 = ((char*)&hashableBlob.sResourceDesc.Dimension) + sizeof(hashableBlob.sResourceDesc.Dimension);
		ZeroMemory(ptr1, offsetof(D3D12_RESOURCE_DESC, Alignment) - sizeof(hashableBlob.sResourceDesc.Dimension));
		void* ptr2 = ((char*)&hashableBlob.sResourceDesc.Flags) + sizeof(hashableBlob.sResourceDesc.Flags);
		ZeroMemory(ptr2, sizeof(hashableBlob.sResourceDesc) - offsetof(D3D12_RESOURCE_DESC, Flags) - sizeof(hashableBlob.sResourceDesc.Flags));

		ReleaseInfo releaseInfo;

		releaseInfo.hHash = ComputeSmallHash<sizeof(hashableBlob)>(&hashableBlob);
		releaseInfo.bFlags = bReusable ? 1 : 0;
		releaseInfo.fenceValues[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
		releaseInfo.fenceValues[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
		releaseInfo.fenceValues[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];

		{
			CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_ReleaseHeapTheadSafeScope);

			std::pair<TReleaseHeap::iterator, bool> result = m_ReleaseHeap.emplace(pObject, std::move(releaseInfo));

			// Only count if insertion happened
			if (result.second)
			{
				pObject->AddRef();
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
void CDevice::FlushAndWaitForGPU()
{
	// Submit pending command-lists in case there are left-overs, make sure it's flushed to and executed on the hardware
	m_Scheduler.Flush(false);
	m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).GetAsyncCommandQueue().Flush();
	m_Scheduler.GetCommandListPool(CMDQUEUE_GRAPHICS).WaitForFenceOnCPU();
}

}
