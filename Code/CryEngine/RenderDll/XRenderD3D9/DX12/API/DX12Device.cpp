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

namespace NCryDX12 {

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
auto GetDebugName = [](ID3D12Resource* pO) -> const char* { UINT len = 511; static char name[512] = "unknown"; pO->GetPrivateData(WKPDID_D3DDebugObjectName, &len, name); name[len] = '\0'; return name; };
#else
auto GetDebugName = [](ID3D12Resource* pO) -> const char* { return "unknown"; };
#endif

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

#ifndef CRY_PLATFORM_CONSOLE
			if (CRenderer::CV_r_EnableDebugLayer == 2)
			{
				// Enable DX12 GBV as well
				ID3D12Debug1* spDebugController1;
				if (SUCCEEDED(debugInterface->QueryInterface(IID_PPV_ARGS(&spDebugController1))))
				{
					spDebugController1->SetEnableGPUBasedValidation(true);
					spDebugController1->Release();
				}
			}
#endif
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
	// Anniversary Update
#if NTDDI_WIN10_RS1 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS1)
	ID3D12Device1* pDevice1 = nullptr;
	m_pDevice->QueryInterface(__uuidof(ID3D12Device1), (void**)&pDevice1);
	if (m_pDevice1 = pDevice1)
		pDevice1->Release();
#endif
	// Creator's Update
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
	ID3D12Device2* pDevice2 = nullptr;
	m_pDevice->QueryInterface(__uuidof(ID3D12Device2), (void**)&pDevice2);
	if (m_pDevice2 = pDevice2)
		pDevice2->Release();
#endif

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
	//	pDisposableResource->Release();

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

	ZeroMemory(&hashableBlob, sizeof(hashableBlob));

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
				// Guaranteed O(1) lookup
				*ppvResource = result->second.front().pObject;

				result->second.pop_front();
				if (!result->second.size())
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
	uint32 releases = 0;
	uint32 recyclations = 0;
	uint32 evictions = 0;

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
					recycleInfo.fenceValues[CMDQUEUE_GRAPHICS] = it->second.fenceValues[CMDQUEUE_GRAPHICS];
					recycleInfo.fenceValues[CMDQUEUE_COMPUTE] = it->second.fenceValues[CMDQUEUE_COMPUTE];
					recycleInfo.fenceValues[CMDQUEUE_COPY] = it->second.fenceValues[CMDQUEUE_COPY];

					// TODO: could be accumulated to a local list and batch-merged in the next code-block to prevent the locking
					auto& sorted = m_RecycleHeap[it->second.hHash];
#if OUT_OF_ODER_RELEASE_LATER
					if (sorted.size())
					{
						auto insertionpoint = sorted.begin();
						while (insertionpoint->fenceValue > recycleInfo.fenceValue)
							++insertionpoint;
						sorted.insert(insertionpoint, std::move(recycleInfo));
					}
					else
#endif
						sorted.push_front(std::move(recycleInfo));

					recyclations++;
				}
				else
				{
					ULONG counter = it->first->Release();
					DX12_ASSERT(counter == 0, "Ref-Counter of D3D12 resource %s is not 0, memory will leak!", GetDebugName(it->first));

					releases++;
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

			while (((it->second.back().fenceValues[CMDQUEUE_GRAPHICS]) <= pruneFenceValues[CMDQUEUE_GRAPHICS]) &&
			       ((it->second.back().fenceValues[CMDQUEUE_COMPUTE]) <= pruneFenceValues[CMDQUEUE_COMPUTE]) &&
			       ((it->second.back().fenceValues[CMDQUEUE_COPY]) <= pruneFenceValues[CMDQUEUE_COPY]))
			{
				// NOTE: some of the objects are referenced by multiple classes,
				// so even when the CResource is destructed and the d3d resource
				// given up for release, they can continue being in use
				// This means the ref-count here doesn't necessarily need to be 0
				ULONG counter = it->second.back().pObject->Release();
				DX12_ASSERT(counter == 0, "Ref-Counter of D3D12 resource %s is not 0, memory will leak!", GetDebugName(it->second.back().pObject));

				it->second.pop_back();
				evictions++;

				if (!it->second.size())
				{
					m_RecycleHeap.erase(it);
					break;
				}
			}
		}
	}

	evictions = evictions - m_RecycleHeap.size();

	// Output debug information
	if (CRendererCVars::CV_r_ShowVideoMemoryStats)
	{
		float fontSize = 1.7f;
		float fontSizeSmall = 1.2f;
		int rowHeight = 35;
		int rowHeightSmall = 13;
		int rowPos = 25;
		int rowMax = 25;
		float colPos = 20;
		size_t releaseSize = 0;
		size_t recycleSize = 0;
		size_t releaseNums = 0;
		size_t recycleNums = 0; 

		const auto& fenceManager = GetScheduler().GetFenceManager();

		IRenderAuxText::Draw2dLabel(colPos, float(rowPos += rowHeight), 2.0f, Col_Blue, false, "GPU Resource Heap Debug");
		IRenderAuxText::Draw2dLabel(colPos, float(rowPos += rowHeight), fontSize, Col_Blue, false, "Fences: Current %lli, Submitted %lli, Completed %lli",
			fenceManager.GetCurrentValue(CMDQUEUE_GRAPHICS), fenceManager.GetSubmittedValue(CMDQUEUE_GRAPHICS), fenceManager.GetLastCompletedFenceValue(CMDQUEUE_GRAPHICS));
		IRenderAuxText::Draw2dLabel(colPos, float(rowPos += rowHeight), fontSize, Col_Blue, false, "Movements: Releases %lli, Recyclations %lli, Evictions %lli",
			releases, recyclations, evictions);

		IRenderAuxText::Draw2dLabel(colPos, float(rowPos += rowHeight), 2.0f, Col_Yellow, false, "Recycle-heap:");

		rowPos += (rowHeight - rowHeightSmall);
		int rowReset = rowPos;
		float colReset = colPos;

		CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RecycleHeapTheadSafeScope);
		TRecycleHeap::iterator it, nx;
		for (it = m_RecycleHeap.begin(); it != m_RecycleHeap.end(); it = nx)
		{
			nx = it;
			++nx;

			int counter = it->second.size();
			ID3D12Resource* pInputResource = it->second.front().pObject;

			D3D12_RESOURCE_DESC   sDesc;
			D3D12_HEAP_FLAGS      sHeapFlags;
			D3D12_HEAP_PROPERTIES sHeapProperties;

			sDesc = pInputResource->GetDesc();
			pInputResource->GetHeapProperties(&sHeapProperties, &sHeapFlags);

			if (sDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{
				uint32 size = uint32(sDesc.Width);
				recycleSize += counter * size;
				recycleNums += counter;

				IRenderAuxText::Draw2dLabel /* Ex */(colPos, float(rowPos += rowHeightSmall), fontSizeSmall, Col_Yellow, false /* eDrawText_Monospace */,
					"%2x: BUF %8d bytes [%2x %2x %1x %1x]", counter, size,
					sHeapFlags, sDesc.Flags, sHeapProperties.CPUPageProperty, sHeapProperties.Type);
			}
			else if (sDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
			{
				uint32 size = CTexture::TextureDataSize(uint32(sDesc.Width), 1, 1, sDesc.MipLevels, sDesc.DepthOrArraySize, DeviceFormats::ConvertToTexFormat(sDesc.Format));
				recycleSize += counter * size;
				recycleNums += counter;

				IRenderAuxText::Draw2dLabel /* Ex */(colPos, float(rowPos += rowHeightSmall), fontSizeSmall, Col_Yellow, false /* eDrawText_Monospace */,
					"%2x: T1D %8d bytes [%2x %2x %1x %1x], %d[%d]*%d", counter, size,
					sHeapFlags, sDesc.Flags, sHeapProperties.CPUPageProperty, sHeapProperties.Type,
					uint32(sDesc.Width), sDesc.DepthOrArraySize, sDesc.MipLevels);
			}
			else if (sDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			{
				uint32 size = CTexture::TextureDataSize(uint32(sDesc.Width), sDesc.Height, 1, sDesc.MipLevels, sDesc.DepthOrArraySize, DeviceFormats::ConvertToTexFormat(sDesc.Format));
				recycleSize += counter * size;
				recycleNums += counter;

				IRenderAuxText::Draw2dLabel /* Ex */(colPos, float(rowPos += rowHeightSmall), fontSizeSmall, Col_Yellow, false /* eDrawText_Monospace */,
					"%2x: T2D %8d bytes [%2x %2x %1x %1x], %dx%d[%d]*%d", counter, size,
					sHeapFlags, sDesc.Flags, sHeapProperties.CPUPageProperty, sHeapProperties.Type,
					uint32(sDesc.Width), sDesc.Height, sDesc.DepthOrArraySize, sDesc.MipLevels);
			}
			else if (sDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
			{
				uint32 size = CTexture::TextureDataSize(uint32(sDesc.Width), sDesc.Height, sDesc.DepthOrArraySize, sDesc.MipLevels, 1, DeviceFormats::ConvertToTexFormat(sDesc.Format));
				recycleSize += counter * size;
				recycleNums += counter;

				IRenderAuxText::Draw2dLabel /* Ex */(colPos, float(rowPos += rowHeightSmall), fontSizeSmall, Col_Yellow, false /* eDrawText_Monospace */,
					"%2x: T3D %8d bytes [%2x %2x %1x %1x], %dx%dx%d*%d", counter, size,
					sHeapFlags, sDesc.Flags, sHeapProperties.CPUPageProperty, sHeapProperties.Type,
					uint32(sDesc.Width), sDesc.Height, sDesc.DepthOrArraySize, sDesc.MipLevels);
			}

			if (rowPos >= (400 - rowHeightSmall))
			{
				rowMax = std::max(rowMax, rowPos);
				rowPos = rowReset;
				colPos += 300;
			}
		}

		rowPos = std::max(rowMax, rowPos);
		colPos = colReset;

		IRenderAuxText::Draw2dLabel(colPos + 150, float(rowReset - (rowHeight - rowHeightSmall)), 2.0f, Col_Orange, false, "%d bytes, %d elements", recycleSize, recycleNums);
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

		THash hHash = ComputeSmallHash<sizeof(hashableBlob)>(&hashableBlob);
		const bool isGPUOnly =
			hashableBlob.sHeapProperties.Type == D3D11_USAGE_DEFAULT;

		// GPU-only resources can't race each other when they are managed by ref-counts/pools
		if (isGPUOnly && bReusable)
		{
			CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_RecycleHeapTheadSafeScope);

			RecycleInfo recycleInfo;

			recycleInfo.pObject = pObject;
			recycleInfo.fenceValues[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
			recycleInfo.fenceValues[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
			recycleInfo.fenceValues[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];

			auto& sorted = m_RecycleHeap[hHash];
#if OUT_OF_ODER_RELEASE_LATER
			if (sorted.size())
			{
				auto insertionpoint = sorted.begin();
				while (insertionpoint->fenceValue > recycleInfo.fenceValue)
					++insertionpoint;
				sorted.insert(insertionpoint, std::move(recycleInfo));
			}
			else
#endif
				sorted.push_front(std::move(recycleInfo));

			// Only count if insertion happened
			{
				pObject->AddRef();
			}
		}
		else
		{
			CryAutoLock<CryCriticalSectionNonRecursive> lThreadSafeScope(m_ReleaseHeapTheadSafeScope);

			ReleaseInfo releaseInfo;

			releaseInfo.hHash = ComputeSmallHash<sizeof(hashableBlob)>(&hashableBlob);
			releaseInfo.bFlags = bReusable ? 1 : 0;
			releaseInfo.fenceValues[CMDQUEUE_GRAPHICS] = fenceValues[CMDQUEUE_GRAPHICS];
			releaseInfo.fenceValues[CMDQUEUE_COMPUTE] = fenceValues[CMDQUEUE_COMPUTE];
			releaseInfo.fenceValues[CMDQUEUE_COPY] = fenceValues[CMDQUEUE_COPY];

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
