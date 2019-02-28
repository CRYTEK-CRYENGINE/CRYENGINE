// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

// ==============================================================================
// Always give out unique handles
#include <concqueue/concqueue.hpp>

#define DX12_MULTIGPU_VAR_HANDLE          true
#define DX12_MULTIGPU_NUM_DESCRIPTORHEAPS (1 << 10)
#define DX12_MULTIGPU_NUM_RESOURCES       (1 << 17)

template<class T, const int numTargets>
struct Handable
{
	UINT32 m_Handle;

	Handable(T pInstance)
	{
		RegisterInstanceAsHandle(pInstance);
	}

	~Handable()
	{
		UnregisterHandle(m_Handle);
	}

	//	typedef UINT64 D3D12_CPU_DELTA_ADDRESS[numTargets];
	//	typedef UINT64 D3D12_GPU_DELTA_ADDRESS[numTargets];

	typedef struct
	{
		UINT64  m_Deltas[numTargets];
		UINT64& operator[](int i)
		{
			return m_Deltas[i];
		}
	} D3D12_GPU_DELTA_ADDRESS;

	typedef struct
	{
		UINT64  m_Deltas[numTargets];
		UINT64& operator[](int i)
		{
			return m_Deltas[i];
		}
	} D3D12_CPU_DELTA_ADDRESS;

	static UINT32                               m_HighHandle;
	static std::vector<T>                       m_AddressTableLookUp;
	static std::vector<D3D12_CPU_DELTA_ADDRESS> m_DeltaCPUAddressTableLookUp;
	static std::vector<D3D12_GPU_DELTA_ADDRESS> m_DeltaGPUAddressTableLookUp;
	static ConcQueue<BoundMPMC, UINT32>         m_FreeHandles;

	// Handles ---------------------
	static UINT32 RequestHandle() threadsafe
	{
		UINT32 handle;
		if (!m_FreeHandles.dequeue(handle))
			handle = CryInterlockedIncrement((int*)&m_HighHandle);

#if DX12_MULTIGPU_VAR_HANDLE
		if (handle >= m_AddressTableLookUp.size())
		{
			m_AddressTableLookUp        .resize(m_AddressTableLookUp        .size() << 1);
			m_DeltaCPUAddressTableLookUp.resize(m_DeltaCPUAddressTableLookUp.size() << 1);
			m_DeltaGPUAddressTableLookUp.resize(m_DeltaGPUAddressTableLookUp.size() << 1);
		}
#endif
		return handle;
	}

	static void Releasehandle(UINT32 handle) threadsafe
	{
		m_FreeHandles.enqueue(handle);
	}

	static void UnregisterHandle(UINT32 handle) threadsafe
	{
		Releasehandle(handle);
	}

	// Handles / Instances ---------
	static UINT32 RegisterInstanceAsHandle(T pInstance) threadsafe
	{
		UINT handle = RequestHandle();
		m_AddressTableLookUp[handle] = pInstance;
		return pInstance->m_Handle = handle;
	}

	static void AssignInstanceToHandle(UINT32 handle, T pInstance) threadsafe
	{
		m_AddressTableLookUp[handle] = pInstance;
	}

	static UINT32 GetHandleFromInstance(T pInstance) threadsafe
	{
		return pInstance->m_Handle;
	}

	static T GetInstancesFromHandle(UINT32 handle) threadsafe
	{
		return m_AddressTableLookUp[handle];
	}

	// Handles / Deltas ------------
	static UINT32 RegisterCPUDeltasAsHandle(D3D12_CPU_DELTA_ADDRESS& pDeltas) threadsafe
	{
		UINT handle = RequestHandle();
		m_DeltaCPUAddressTableLookUp[handle] = pDeltas;
		return handle;
	}

	static const D3D12_CPU_DELTA_ADDRESS& GetCPUDeltasFromHandle(UINT32 handle) threadsafe
	{
		return m_DeltaCPUAddressTableLookUp[handle];
	}

	static void AssignCPUDeltasToHandle(UINT32 handle, D3D12_CPU_DELTA_ADDRESS& pDeltas) threadsafe
	{
		memcpy(&m_DeltaCPUAddressTableLookUp[handle], &pDeltas, sizeof(pDeltas));
	}

	static UINT32 RegisterGPUDeltasAsHandle(D3D12_GPU_DELTA_ADDRESS& pDeltas) threadsafe
	{
		UINT handle = RequestHandle();
		m_DeltaGPUAddressTableLookUp[handle] = pDeltas;
		return handle;
	}

	static const D3D12_GPU_DELTA_ADDRESS& GetGPUDeltasFromHandle(UINT32 handle) threadsafe
	{
		return m_DeltaGPUAddressTableLookUp[handle];
	}

	static void AssignGPUDeltasToHandle(UINT32 handle, D3D12_GPU_DELTA_ADDRESS& pDeltas) threadsafe
	{
		memcpy(&m_DeltaGPUAddressTableLookUp[handle], &pDeltas, sizeof(pDeltas));
	}
};

// ==============================================================================
#include "D3D12CommandAllocator.inl"
#include "D3D12CommandQueue.inl"
#include "D3D12GraphicsCommandList.inl"

#include "D3D12DescriptorHeap.inl"
#include "D3D12QueryHeap.inl"
#include "D3D12Heap.inl"

#include "D3D12Fence.inl"
#include "D3D12Resource.inl"

// ==============================================================================
template<const int numTargets>
class BroadcastableD3D12Device : public ID3D12Device
{
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;
	friend class NCryDX12::CDevice;

	int           m_RefCount;
	ID3D12Device* m_Target;

	typedef BroadcastableD3D12Device<numTargets> self;
public:
	BroadcastableD3D12Device(
	  _In_ ID3D12Device* pDevice,
	  REFIID riid) : m_RefCount(1), m_Target(pDevice)
	{
	}

#define Redirect(func) \
  m_Target->func;

	static void DuplicateMetaData(
		_In_ ID3D12Resource* pInputResource,
		_In_ ID3D12Resource* pOutputResource)
	{
		return BroadcastableD3D12Resource<numTargets>::DuplicateMetaData(pInputResource, pOutputResource);
	}

	#pragma region /* IUnknown implementation */

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
	  /* [in] */ REFIID riid,
	  /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) final
	{
		HRESULT ret = S_OK;
		if (riid == IID_ID3D12Device)
		{
			*ppvObject = this;
		}
		else
		{
			// Internal Interfaces are requested by the DXGI methods (DXGIOutput etc.)
			ret = Redirect(QueryInterface(riid, ppvObject));
		}

		return ret;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) final
	{
		Redirect(AddRef());
		return CryInterlockedIncrement(&m_RefCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void) final
	{
		Redirect(Release());
		ULONG count = CryInterlockedDecrement(&m_RefCount);
		if (!count) delete this; return count;
	}

	#pragma endregion

	#pragma region /* ID3D12Object implementation */

	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID guid,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_opt_(*pDataSize)  void* pData) final
	{
		return Redirect(GetPrivateData(guid, pDataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData) final
	{
		return Redirect(SetPrivateData(guid, DataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) final
	{
		return Redirect(SetPrivateDataInterface(guid, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetName(
	  _In_z_ LPCWSTR Name) final
	{
		return Redirect(SetName(Name));
	}

	#pragma endregion

	#pragma region /* ID3D12Device implementation */

	virtual UINT STDMETHODCALLTYPE GetNodeCount(void) final
	{
		return Redirect(GetNodeCount());
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCommandQueue(
	  _In_ const D3D12_COMMAND_QUEUE_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandQueue) final
	{
		return BroadcastableD3D12CommandQueue<numTargets>::CreateCommandQueue(m_Target, pDesc, riid, ppCommandQueue);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCommandAllocator(
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandAllocator) final
	{
		return BroadcastableD3D12CommandAllocator<numTargets>::CreateCommandAllocator(m_Target, type, riid, ppCommandAllocator);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(
	  _In_ const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppPipelineState) final
	{
		return Redirect(CreateGraphicsPipelineState(pDesc, riid, ppPipelineState));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateComputePipelineState(
	  _In_ const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppPipelineState) final
	{
		return Redirect(CreateComputePipelineState(pDesc, riid, ppPipelineState));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCommandList(
	  _In_ UINT nodeMask,
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  _In_ ID3D12CommandAllocator* pCommandAllocator,
	  _In_opt_ ID3D12PipelineState* pInitialState,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandList) final
	{
		BroadcastableD3D12CommandAllocator<numTargets>* Allocator = (BroadcastableD3D12CommandAllocator<numTargets>*)pCommandAllocator;

		return BroadcastableD3D12GraphicsCommandList<numTargets>::CreateCommandList(m_Target, nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
	}

	virtual HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
	  D3D12_FEATURE Feature,
	  _Inout_updates_bytes_(FeatureSupportDataSize)  void* pFeatureSupportData,
	  UINT FeatureSupportDataSize) final
	{
		return Redirect(CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(
	  _In_ const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppvHeap) final
	{
		return BroadcastableD3D12DescriptorHeap<numTargets>::CreateDescriptorHeap(m_Target, pDescriptorHeapDesc, riid, ppvHeap);
	}

	virtual UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(
	  _In_ D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) final
	{
		return Redirect(GetDescriptorHandleIncrementSize(DescriptorHeapType));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateRootSignature(
	  _In_ UINT nodeMask,
	  _In_reads_(blobLengthInBytes)  const void* pBlobWithRootSignature,
	  _In_ SIZE_T blobLengthInBytes,
	  REFIID riid,
	  _COM_Outptr_ void** ppvRootSignature) final
	{
#if DX12_LINKEDADAPTER_SIMULATION
		// Always create on the first GPU, if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0)
			nodeMask = 1;
#endif

		return Redirect(CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature));
	}

	virtual void STDMETHODCALLTYPE CreateConstantBufferView(
	  _In_opt_ const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		// optional
		if (!pDesc)
		{
			auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

			D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
			for (int i = 0; i < numTargets; ++i)
			{
				Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

				Redirect(CreateConstantBufferView(nullptr, Descriptor));
			}

			return;
		}

		auto DeltaLocations = BroadcastableD3D12Resource<numTargets>::GetGPUDeltasFromHandle(UINT32(pDesc->BufferLocation >> 32));
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		D3D12_CONSTANT_BUFFER_VIEW_DESC Desc = *pDesc;
		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Desc.BufferLocation = pDesc->BufferLocation + DeltaLocations[i];
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			Redirect(CreateConstantBufferView(&Desc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CreateShaderResourceView(
	  _In_opt_ ID3D12Resource* pResource,
	  _In_opt_ const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		DX12_ASSERT(!pBroadcastableResource || !pBroadcastableResource->IsShared(), "CreateView is not valid on a shared resource");

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			// optional
			pResource = nullptr;
			if (pBroadcastableResource)
				pResource = *(*pBroadcastableResource)[i];

			Redirect(CreateShaderResourceView(pResource, pDesc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CreateUnorderedAccessView(
	  _In_opt_ ID3D12Resource* pResource,
	  _In_opt_ ID3D12Resource* pCounterResource,
	  _In_opt_ const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		BroadcastableD3D12Resource<numTargets>* pBroadcastableCounterResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pCounterResource);
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		DX12_ASSERT(!pBroadcastableResource || !pBroadcastableResource->IsShared(), "CreateView is not valid on a shared resource");
		DX12_ASSERT(!pBroadcastableCounterResource || !pBroadcastableCounterResource->IsShared(), "CreateView is not valid on a shared resource");

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			// optional
			pResource = nullptr;
			if (pBroadcastableResource)
				pResource = *(*pBroadcastableResource)[i];

			// optional
			pCounterResource = nullptr;
			if (pBroadcastableCounterResource)
				pCounterResource = *(*pBroadcastableCounterResource)[i];

			Redirect(CreateUnorderedAccessView(pResource, pCounterResource, pDesc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CreateRenderTargetView(
	  _In_opt_ ID3D12Resource* pResource,
	  _In_opt_ const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		DX12_ASSERT(!pBroadcastableResource || !pBroadcastableResource->IsShared(), "CreateView is not valid on a shared resource");

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			// optional
			pResource = nullptr;
			if (pBroadcastableResource)
				pResource = *(*pBroadcastableResource)[i];

			Redirect(CreateRenderTargetView(pResource, pDesc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CreateDepthStencilView(
	  _In_opt_ ID3D12Resource* pResource,
	  _In_opt_ const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		BroadcastableD3D12Resource<numTargets>* pBroadcastableResource = reinterpret_cast<BroadcastableD3D12Resource<numTargets>*>(pResource);
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		DX12_ASSERT(!pBroadcastableResource || !pBroadcastableResource->IsShared(), "CreateView is not valid on a shared resource");

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			// optional
			pResource = nullptr;
			if (pBroadcastableResource)
				pResource = *(*pBroadcastableResource)[i];

			Redirect(CreateDepthStencilView(pResource, pDesc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CreateSampler(
	  _In_ const D3D12_SAMPLER_DESC* pDesc,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final
	{
		auto DeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptor.ptr >> 32));

		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = DestDescriptor;
		for (int i = 0; i < numTargets; ++i)
		{
			Descriptor.ptr = DestDescriptor.ptr + DeltaDescriptors[i];

			Redirect(CreateSampler(pDesc, Descriptor));
		}
	}

	virtual void STDMETHODCALLTYPE CopyDescriptors(
	  _In_ UINT NumDestDescriptorRanges,
	  _In_reads_(NumDestDescriptorRanges)  const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
	  _In_reads_opt_(NumDestDescriptorRanges)  const UINT* pDestDescriptorRangeSizes,
	  _In_ UINT NumSrcDescriptorRanges,
	  _In_reads_(NumSrcDescriptorRanges)  const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
	  _In_reads_opt_(NumSrcDescriptorRanges)  const UINT* pSrcDescriptorRangeSizes,
	  _In_ D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) final
	{
		D3D12_CPU_DESCRIPTOR_HANDLE* DstDescriptorRangeStarts = (D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * NumDestDescriptorRanges);
		D3D12_CPU_DESCRIPTOR_HANDLE* SrcDescriptorRangeStarts = (D3D12_CPU_DESCRIPTOR_HANDLE*)alloca(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE) * NumSrcDescriptorRanges);

		// TODO: switch inner and outer loop to reduce GetDeltasFromHandle() calls
		for (int i = 0; i < numTargets; ++i)
		{
			for (int j = 0; j < NumDestDescriptorRanges; ++j)
			{
				auto StartDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(pDestDescriptorRangeStarts[j].ptr >> 32));

				DstDescriptorRangeStarts[j] = pDestDescriptorRangeStarts[j];
				DstDescriptorRangeStarts[j].ptr = pDestDescriptorRangeStarts[j].ptr + StartDeltaDescriptors[i];
			}

			for (int j = 0; j < NumSrcDescriptorRanges; ++j)
			{
				auto StartDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(pSrcDescriptorRangeStarts[j].ptr >> 32));

				SrcDescriptorRangeStarts[j] = pSrcDescriptorRangeStarts[j];
				SrcDescriptorRangeStarts[j].ptr = pSrcDescriptorRangeStarts[j].ptr + StartDeltaDescriptors[i];
			}

			Redirect(CopyDescriptors(NumDestDescriptorRanges, DstDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, SrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType));
		}
	}

	virtual void STDMETHODCALLTYPE CopyDescriptorsSimple(
	  _In_ UINT NumDescriptors,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
	  _In_ D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
	  _In_ D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) final
	{
		auto DstDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(DestDescriptorRangeStart.ptr >> 32));
		auto SrcDeltaDescriptors = BroadcastableD3D12DescriptorHeap<numTargets>::GetCPUDeltasFromHandle(UINT32(SrcDescriptorRangeStart.ptr >> 32));

		D3D12_CPU_DESCRIPTOR_HANDLE DstDescriptor = DestDescriptorRangeStart;
		D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor = SrcDescriptorRangeStart;
		for (int i = 0; i < numTargets; ++i)
		{
			DstDescriptor.ptr = DestDescriptorRangeStart.ptr + DstDeltaDescriptors[i];
			SrcDescriptor.ptr = SrcDescriptorRangeStart.ptr + SrcDeltaDescriptors[i];

			Redirect(CopyDescriptorsSimple(NumDescriptors, DstDescriptor, SrcDescriptor, DescriptorHeapsType));
		}
	}

	virtual D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE GetResourceAllocationInfo(
	  _In_ UINT visibleMask,
	  _In_ UINT numResourceDescs,
	  _In_reads_(numResourceDescs)  const D3D12_RESOURCE_DESC* pResourceDescs) final
	{
		return Redirect(GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs));
	}

	virtual D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE GetCustomHeapProperties(
	  _In_ UINT nodeMask,
	  D3D12_HEAP_TYPE heapType) final
	{
#if DX12_LINKEDADAPTER_SIMULATION
		// Always create on the first GPU, if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0)
			nodeMask = 1;
#endif

		// Q: Could the be different for each node?
		return Redirect(GetCustomHeapProperties(blsi(nodeMask), heapType));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCommittedResource(
	  _In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
	  D3D12_HEAP_FLAGS HeapFlags,
	  _In_ const D3D12_RESOURCE_DESC* pResourceDesc,
	  D3D12_RESOURCE_STATES InitialResourceState,
	  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	  REFIID riidResource,
	  _COM_Outptr_opt_ void** ppvResource) final
	{
		return BroadcastableD3D12Resource<numTargets>::CreateResource(m_Target, pHeapProperties, HeapFlags, pResourceDesc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateHeap(
	  _In_ const D3D12_HEAP_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvHeap) final
	{
		return BroadcastableD3D12Heap<numTargets>::CreateHeap(m_Target, pDesc, riid, ppvHeap);
	}

	virtual HRESULT STDMETHODCALLTYPE CreatePlacedResource(
	  _In_ ID3D12Heap* pHeap,
	  UINT64 HeapOffset,
	  _In_ const D3D12_RESOURCE_DESC* pDesc,
	  D3D12_RESOURCE_STATES InitialState,
	  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvResource) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateReservedResource(
	  _In_ const D3D12_RESOURCE_DESC* pDesc,
	  D3D12_RESOURCE_STATES InitialState,
	  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvResource) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateSharedHandle(
	  _In_ ID3D12DeviceChild* pObject,
	  _In_opt_ const SECURITY_ATTRIBUTES* pAttributes,
	  DWORD Access,
	  _In_opt_ LPCWSTR Name,
	  _Out_ HANDLE* pHandle) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedHandle(
	  _In_ HANDLE NTHandle,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvObj) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(
	  _In_ LPCWSTR Name,
	  DWORD Access,
	  /* [annotation][out] */
	  _Out_ HANDLE* pNTHandle) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE MakeResident(
	  UINT NumObjects,
	  _In_reads_(NumObjects)  ID3D12Pageable* const* ppObjects) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE Evict(
	  UINT NumObjects,
	  _In_reads_(NumObjects)  ID3D12Pageable* const* ppObjects) final
	{
		__debugbreak();
		return E_FAIL;
	}

	virtual HRESULT STDMETHODCALLTYPE CreateFence(
	  UINT64 InitialValue,
	  D3D12_FENCE_FLAGS Flags,
	  REFIID riid,
	  _COM_Outptr_ void** ppFence) final
	{
		return BroadcastableD3D12Fence<numTargets>::CreateFence(m_Target, InitialValue, Flags, riid, ppFence);
	}

	virtual HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(void) final
	{
		return Redirect(GetDeviceRemovedReason());
	}

	virtual void STDMETHODCALLTYPE GetCopyableFootprints(
	  _In_ const D3D12_RESOURCE_DESC* pResourceDesc,
	  _In_range_(0, D3D12_REQ_SUBRESOURCES)  UINT FirstSubresource,
	  _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource)  UINT NumSubresources,
	  UINT64 BaseOffset,
	  _Out_writes_opt_(NumSubresources)  D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
	  _Out_writes_opt_(NumSubresources)  UINT* pNumRows,
	  _Out_writes_opt_(NumSubresources)  UINT64* pRowSizeInBytes,
	  _Out_opt_ UINT64* pTotalBytes) final
	{
		return Redirect(GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateQueryHeap(
	  _In_ const D3D12_QUERY_HEAP_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvHeap) final
	{
		return BroadcastableD3D12QueryHeap<numTargets>::CreateQueryHeap(m_Target, pDesc, riid, ppvHeap);
	}

	virtual HRESULT STDMETHODCALLTYPE SetStablePowerState(
	  BOOL Enable) final
	{
		return Redirect(SetStablePowerState(Enable));
	}

	virtual HRESULT STDMETHODCALLTYPE CreateCommandSignature(
	  _In_ const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
	  _In_opt_ ID3D12RootSignature* pRootSignature,
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvCommandSignature) final
	{
		// TODO: implement BroadcastableD3D12CommandSignature
		__debugbreak();
		return Redirect(CreateCommandSignature(pDesc, pRootSignature, riid, ppvCommandSignature));
	}

	virtual void STDMETHODCALLTYPE GetResourceTiling(
	  _In_ ID3D12Resource* pTiledResource,
	  _Out_opt_ UINT* pNumTilesForEntireResource,
	  _Out_opt_ D3D12_PACKED_MIP_INFO* pPackedMipDesc,
	  _Out_opt_ D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
	  _Inout_opt_ UINT* pNumSubresourceTilings,
	  _In_ UINT FirstSubresourceTilingToGet,
	  _Out_writes_(*pNumSubresourceTilings)  D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) final
	{
		abort();
	}

	virtual LUID STDMETHODCALLTYPE GetAdapterLuid(void) final
	{
		return Redirect(GetAdapterLuid());
	}

	#pragma endregion
};
