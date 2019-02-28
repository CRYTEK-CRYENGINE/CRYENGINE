// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12CommandList.hpp"
#include "DX12CommandScheduler.hpp"

namespace NCryDX12 {

class CDevice : public CRefCounted
{
public:
	static CDevice*                  Create(CCryDX12GIAdapter* adapter, D3D_FEATURE_LEVEL* pFeatureLevel);

	ILINE ID3D12Device*              GetD3D12Device() const        { return /*PassAddRef*/ (m_pDevice ); } // RTM
#if NTDDI_WIN10_RS1 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS1)
	ILINE ID3D12Device1*             GetD3D12Device1() const       { return /*PassAddRef*/ (m_pDevice1); } // Anniversary Update
#endif
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
	ILINE ID3D12Device2*             GetD3D12Device2() const       { return /*PassAddRef*/ (m_pDevice2); } // Creator's Update
#endif

	ILINE CPSOCache&                 GetPSOCache()                 { return m_PSOCache; }
	ILINE const CPSOCache&           GetPSOCache() const           { return m_PSOCache; }

	ILINE CRootSignatureCache&       GetRootSignatureCache()       { return m_RootSignatureCache; }
	ILINE const CRootSignatureCache& GetRootSignatureCache() const { return m_RootSignatureCache; }

	#ifdef DX12_LINKEDADAPTER
	bool                      IsMultiAdapter() const;

	ID3D12CommandQueue*       GetNativeObject(ID3D12CommandQueue* pQueue, UINT node) const;
	ID3D12Resource*           CreateBroadcastObject(ID3D12Resource** pResources) const;

	bool                      WaitForCompletion(ID3D12Fence* pFence, UINT64 fenceValue) const;
	#endif

	CDescriptorBlock GetGlobalDescriptorBlock(D3D12_DESCRIPTOR_HEAP_TYPE eType, UINT size);

protected:
	CDevice(ID3D12Device* d3d12Device, D3D_FEATURE_LEVEL featureLevel, UINT nodeCount, UINT nodeMask);

	virtual ~CDevice();

private:
	DX12_PTR(ID3D12Device)  m_pDevice;  // RTM
#if NTDDI_WIN10_RS1 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS1)
	DX12_PTR(ID3D12Device1) m_pDevice1; // Anniversary Update
#endif
#if NTDDI_WIN10_RS2 && (WDK_NTDDI_VERSION >= NTDDI_WIN10_RS2)
	DX12_PTR(ID3D12Device2) m_pDevice2; // Creator's Update
#endif
	D3D_FEATURE_LEVEL      m_featureLevel;
	UINT                   m_nodeCount;
	UINT                   m_nodeMask;
	
	CPSOCache              m_PSOCache;
	CRootSignatureCache    m_RootSignatureCache;

public:
	HRESULT                     CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) { return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize); }
	D3D_FEATURE_LEVEL           GetFeatureLevel() const { return m_featureLevel; }
	UINT                        GetNodeCount() const    { return m_nodeCount; }
	UINT                        GetNodeMask() const     { return m_nodeMask; }

	CCommandScheduler&          GetScheduler() { return m_Scheduler; }
	const CCommandScheduler&    GetScheduler() const { return m_Scheduler; }

	D3D12_CPU_DESCRIPTOR_HANDLE CacheSampler(const D3D12_SAMPLER_DESC* pDesc) threadsafe;
	D3D12_CPU_DESCRIPTOR_HANDLE CacheShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;
	D3D12_CPU_DESCRIPTOR_HANDLE CacheUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;
	D3D12_CPU_DESCRIPTOR_HANDLE CacheDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D12Resource* pResource threadsafe);
	D3D12_CPU_DESCRIPTOR_HANDLE CacheRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;

	static THash                GetSamplerHash(const D3D12_SAMPLER_DESC* pDesc) threadsafe;
	static THash                GetShaderResourceViewHash(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, const ID3D12Resource* pResource) threadsafe;
	static THash                GetUnorderedAccessViewHash(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, const ID3D12Resource* pResource) threadsafe;
	static THash                GetDepthStencilViewHash(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, const ID3D12Resource* pResource) threadsafe;
	static THash                GetRenderTargetViewHash(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, const ID3D12Resource* pResource) threadsafe;

	void                        InvalidateSampler(const D3D12_SAMPLER_DESC* pDesc) threadsafe;
	void                        InvalidateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;
	void                        InvalidateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;
	void                        InvalidateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;
	void                        InvalidateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, ID3D12Resource* pResource) threadsafe;

	const CDescriptorHeap&      GetSamplerCacheHeap() threadsafe_const         { return m_SamplerCache; }
	const CDescriptorHeap&      GetShaderResourceCacheHeap() threadsafe_const  { return m_ShaderResourceDescriptorCache; }
	const CDescriptorHeap&      GetUnorderedAccessCacheHeap() threadsafe_const { return m_UnorderedAccessDescriptorCache; }
	const CDescriptorHeap&      GetDepthStencilCacheHeap() threadsafe_const    { return m_DepthStencilDescriptorCache; }
	const CDescriptorHeap&      GetRenderTargetCacheHeap() threadsafe_const    { return m_RenderTargetDescriptorCache; }

	CDescriptorBlock            GetResourceDescriptorScratchSpace()            { return CDescriptorBlock(&m_ResourceDescriptorScratchSpace, 0, m_ResourceDescriptorScratchSpace.GetCapacity()); }

#ifdef DX12_LINKEDADAPTER
	HRESULT STDMETHODCALLTYPE DuplicateNativeCommittedResource(
		_In_ UINT creationMask,
		_In_ UINT visibilityMask,
		_In_ ID3D12Resource* pInputResource,
		_Out_ ID3D12Resource** ppOutputResource);
#endif

	HRESULT STDMETHODCALLTYPE DuplicateCommittedResource(
		_In_ ID3D12Resource* pInputResource,
		_In_ D3D12_RESOURCE_STATES OutputState,
		_Out_ ID3D12Resource** ppOutputResource) threadsafe;

	HRESULT STDMETHODCALLTYPE SubstituteUsedCommittedResource(
		_In_ const FVAL64(&fenceValues)[CMDQUEUE_NUM],
		_In_ D3D12_RESOURCE_STATES OutputState,
		_Inout_ ID3D12Resource** ppSubstituteResource) threadsafe;

	HRESULT STDMETHODCALLTYPE CreateOrReuseStagingResource(
		_In_ ID3D12Resource* pInputResource,
		_Out_ ID3D12Resource** ppStagingResource,
		_In_ BOOL Upload) threadsafe;

	HRESULT STDMETHODCALLTYPE CreateOrReuseCommittedResource(
		_In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
		_In_ D3D12_HEAP_FLAGS HeapFlags,
		_In_ const D3D12_RESOURCE_DESC* pResourceDesc,
		_In_ D3D12_RESOURCE_STATES InitialResourceState,
		_In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
		_In_ REFIID riidResource,
		_COM_Outptr_opt_ void** ppvResource) threadsafe;

	void FlushReleaseHeap(const UINT64 (&completedFenceValues)[CMDQUEUE_NUM], const UINT64 (&pruneFenceValues)[CMDQUEUE_NUM]) threadsafe;
	void ReleaseLater(const FVAL64 (&fenceValues)[CMDQUEUE_NUM], ID3D12Resource* pObject, bool bReusable = true) threadsafe;

	void FlushAndWaitForGPU();

private:
	CDescriptorHeap m_SamplerCache;
	CDescriptorHeap m_ShaderResourceDescriptorCache;
	CDescriptorHeap m_UnorderedAccessDescriptorCache;
	CDescriptorHeap m_DepthStencilDescriptorCache;
	CDescriptorHeap m_RenderTargetDescriptorCache;
	CDescriptorHeap m_ResourceDescriptorScratchSpace;

	#if defined(_ALLOW_INITIALIZER_LISTS)
	CDescriptorHeap              m_GlobalDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	#else
	std::vector<CDescriptorHeap> m_GlobalDescriptorHeaps;
	#endif

	std::unordered_map<THash, D3D12_CPU_DESCRIPTOR_HANDLE> m_SamplerCacheLookupTable;
	std::unordered_map<THash, D3D12_CPU_DESCRIPTOR_HANDLE> m_ShaderResourceDescriptorLookupTable;
	std::unordered_map<THash, D3D12_CPU_DESCRIPTOR_HANDLE> m_UnorderedAccessDescriptorLookupTable;
	std::unordered_map<THash, D3D12_CPU_DESCRIPTOR_HANDLE> m_DepthStencilDescriptorLookupTable;
	std::unordered_map<THash, D3D12_CPU_DESCRIPTOR_HANDLE> m_RenderTargetDescriptorLookupTable;

	std::list<D3D12_CPU_DESCRIPTOR_HANDLE>                 m_SamplerCacheFreeTable;
	std::list<D3D12_CPU_DESCRIPTOR_HANDLE>                 m_ShaderResourceDescriptorFreeTable;
	std::list<D3D12_CPU_DESCRIPTOR_HANDLE>                 m_UnorderedAccessDescriptorFreeTable;
	std::list<D3D12_CPU_DESCRIPTOR_HANDLE>                 m_DepthStencilDescriptorFreeTable;
	std::list<D3D12_CPU_DESCRIPTOR_HANDLE>                 m_RenderTargetDescriptorFreeTable;

	static CryCriticalSectionNonRecursive                  m_SamplerThreadSafeScope;
	static CryCriticalSectionNonRecursive                  m_ShaderResourceThreadSafeScope;
	static CryCriticalSectionNonRecursive                  m_UnorderedAccessThreadSafeScope;
	static CryCriticalSectionNonRecursive                  m_DepthStencilThreadSafeScope;
	static CryCriticalSectionNonRecursive                  m_RenderTargetThreadSafeScope;
	static CryCriticalSectionNonRecursive                  m_DescriptorAllocatorTheadSafeScope;

	// Objects that should be released when they are not in use anymore
	static CryCriticalSectionNonRecursive                  m_ReleaseHeapTheadSafeScope;
	static CryCriticalSectionNonRecursive                  m_RecycleHeapTheadSafeScope;

	struct ReleaseInfo
	{
		THash  hHash;
		UINT32 bFlags;
		UINT64 fenceValues[CMDQUEUE_NUM];
	};

	struct RecycleInfo
	{
		ID3D12Resource* pObject;
		UINT64          fenceValues[CMDQUEUE_NUM];
	};

	typedef std::unordered_map<ID3D12Resource*, ReleaseInfo>   TReleaseHeap;
	typedef std::unordered_map<THash, std::deque<RecycleInfo>> TRecycleHeap;

	TReleaseHeap m_ReleaseHeap;
	TRecycleHeap m_RecycleHeap;

	CCommandScheduler m_Scheduler;
};

}
