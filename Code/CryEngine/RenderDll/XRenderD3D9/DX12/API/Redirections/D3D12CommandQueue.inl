// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12CommandQueue : public ID3D12CommandQueue
{
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;
	friend class NCryDX12::CDevice;

public:
	int m_RefCount;
	std::array<ID3D12CommandQueue*, numTargets> m_Targets;
	ID3D12CommandQueue* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12CommandQueue<numTargets> self;
public:
	BroadcastableD3D12CommandQueue(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_COMMAND_QUEUE_DESC* pDesc,
	  REFIID riid) : m_RefCount(1)
	{
		D3D12_COMMAND_QUEUE_DESC Desc = *pDesc;

		DX12_ASSERT(pDesc->NodeMask != 0, "0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;
			if (Desc.NodeMask = (pDesc->NodeMask & (1 << i)))
			{
#if DX12_LINKEDADAPTER_SIMULATION
				// Always create on the first GPU, if running simulation
				if (CRenderer::CV_r_StereoEnableMgpu < 0)
					Desc.NodeMask = 1;
#endif

				HRESULT ret = pDevice->CreateCommandQueue(
				  &Desc, riid, (void**)&m_Targets[i]);
				DX12_ASSERT(ret == S_OK, "Failed to create command queue!");
			}
		}
	}

#define Pick(i, func)                           \
  m_Targets[i]->func;

#define Broadcast(func)                         \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
      if (m_Targets[i])                         \
        m_Targets[i]->func;                     \
  }                                             \
  else                                          \
  {                                             \
    m_Targets[0]->func;                         \
    m_Targets[1]->func;                         \
  }

#define BroadcastWithFail(func)                 \
  HRESULT ret;                                  \
                                                \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
    {                                           \
      if (m_Targets[i])                         \
      {                                         \
        if ((ret = m_Targets[i]->func) != S_OK) \
          return ret;                           \
      }                                         \
    }                                           \
  }                                             \
  else                                          \
  {                                             \
    if ((ret = m_Targets[0]->func) != S_OK)     \
      return ret;                               \
    if ((ret = m_Targets[1]->func) != S_OK)     \
      return ret;                               \
  }                                             \
                                                \
  return S_OK;

#define ParallelizeWithFail(func)               \
  if (numTargets > 2)                           \
  {                                             \
    for (int i = 0; i < numTargets; ++i)        \
    {                                           \
      if (m_Targets[i])                         \
      {                                         \
        HRESULT ret;                            \
        if ((ret = m_Targets[i]->func) != S_OK) \
          return ret;                           \
      }                                         \
    }                                           \
  }                                             \
  else                                          \
  {                                             \
    { int i = 0; HRESULT ret;                   \
      if ((ret = m_Targets[i]->func) != S_OK)   \
        return ret; }                           \
    { int i = 1; HRESULT ret;                   \
      if ((ret = m_Targets[i]->func) != S_OK)   \
        return ret; }                           \
  }                                             \
                                                \
  return S_OK;

	static HRESULT STDMETHODCALLTYPE CreateCommandQueue(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_COMMAND_QUEUE_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandQueue)
	{
		return (*ppCommandQueue = new self(pDevice, pDesc, riid)) ? S_OK : E_FAIL;
	}

	#pragma region /* IUnknown implementation */

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
	  /* [in] */ REFIID riid,
	  /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) final
	{
		HRESULT ret = S_OK;
		if (riid == IID_ID3D12CommandQueue)
		{
			*ppvObject = this;
		}
		else
		{
			// Internal Interfaces are requested by the DXGI methods (DXGISwapChain etc.)
			ret = Pick(0, QueryInterface(riid, ppvObject));
		}

		return ret;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef(void) final
	{
		Broadcast(AddRef());
		return CryInterlockedIncrement(&m_RefCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release(void) final
	{
		Broadcast(Release());
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
		return Pick(0, GetPrivateData(guid, pDataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData) final
	{
		BroadcastWithFail(SetPrivateData(guid, DataSize, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) final
	{
		BroadcastWithFail(SetPrivateDataInterface(guid, pData));
	}

	virtual HRESULT STDMETHODCALLTYPE SetName(
	  _In_z_ LPCWSTR Name) final
	{
		BroadcastWithFail(SetName(Name));
	}

	#pragma endregion

	#pragma region /* ID3D12DeviceChild implementation */

	virtual HRESULT STDMETHODCALLTYPE GetDevice(
	  REFIID riid,
	  _COM_Outptr_opt_ void** ppvDevice) final
	{
		return Pick(0, GetDevice(riid, ppvDevice));
	}

	#pragma endregion

	#pragma region /* ID3D12CommandQueue implementation */

	virtual void STDMETHODCALLTYPE UpdateTileMappings(
	  _In_ ID3D12Resource* pResource,
	  UINT NumResourceRegions,
	  _In_reads_opt_(NumResourceRegions)  const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
	  _In_reads_opt_(NumResourceRegions)  const D3D12_TILE_REGION_SIZE* pResourceRegionSizes,
	  _In_opt_ ID3D12Heap* pHeap,
	  UINT NumRanges,
	  _In_reads_opt_(NumRanges)  const D3D12_TILE_RANGE_FLAGS* pRangeFlags,
	  _In_reads_opt_(NumRanges)  const UINT* pHeapRangeStartOffsets,
	  _In_reads_opt_(NumRanges)  const UINT* pRangeTileCounts,
	  D3D12_TILE_MAPPING_FLAGS Flags) final
	{
		// TODO: IsShared() possible?

		Broadcast(UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, pHeap, NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags));
	}

	virtual void STDMETHODCALLTYPE CopyTileMappings(
	  _In_ ID3D12Resource* pDstResource,
	  _In_ const D3D12_TILED_RESOURCE_COORDINATE* pDstRegionStartCoordinate,
	  _In_ ID3D12Resource* pSrcResource,
	  _In_ const D3D12_TILED_RESOURCE_COORDINATE* pSrcRegionStartCoordinate,
	  _In_ const D3D12_TILE_REGION_SIZE* pRegionSize,
	  D3D12_TILE_MAPPING_FLAGS Flags) final
	{
		// TODO: IsShared() possible?

		Broadcast(CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags));
	}

	virtual void STDMETHODCALLTYPE ExecuteCommandLists(
	  _In_ UINT NumCommandLists,
	  _In_reads_(NumCommandLists)  ID3D12CommandList* const* ppCommandLists) final
	{
		ID3D12CommandList** CommandLists = (ID3D12CommandList**)alloca(sizeof(ID3D12CommandList*) * NumCommandLists);

		for (int i = 0; i < numTargets; ++i)
		{
			for (int n = 0; n < NumCommandLists; ++n)
			{
				BroadcastableD3D12GraphicsCommandList<numTargets>* pCommandLists = (BroadcastableD3D12GraphicsCommandList<numTargets>*)ppCommandLists[n];

				CommandLists[n] = (ID3D12GraphicsCommandList*)*(*pCommandLists)[i];
			}

			Pick(i, ExecuteCommandLists(NumCommandLists, CommandLists));
		}
	}

	virtual void STDMETHODCALLTYPE SetMarker(
	  UINT Metadata,
	  _In_reads_bytes_opt_(Size)  const void* pData,
	  UINT Size) final
	{
		Broadcast(SetMarker(Metadata, pData, Size));
	}

	virtual void STDMETHODCALLTYPE BeginEvent(
	  UINT Metadata,
	  _In_reads_bytes_opt_(Size)  const void* pData,
	  UINT Size) final
	{
		Broadcast(BeginEvent(Metadata, pData, Size));
	}

	virtual void STDMETHODCALLTYPE EndEvent(void) final
	{
		Broadcast(EndEvent());
	}

	virtual HRESULT STDMETHODCALLTYPE Signal(
	  ID3D12Fence* pFence,
	  UINT64 Value) final
	{
		BroadcastableD3D12Fence<numTargets>* Fence = (BroadcastableD3D12Fence<numTargets>*)pFence;

		ParallelizeWithFail(Signal(*(*Fence)[i], Value));
	}

	virtual HRESULT STDMETHODCALLTYPE Wait(
	  ID3D12Fence* pFence,
	  UINT64 Value) final
	{
		BroadcastableD3D12Fence<numTargets>* Fence = (BroadcastableD3D12Fence<numTargets>*)pFence;

		ParallelizeWithFail(Wait(*(*Fence)[i], Value));
	}

	virtual HRESULT STDMETHODCALLTYPE GetTimestampFrequency(
	  _Out_ UINT64* pFrequency) final
	{
		// Q: Could the be different for each node?
		return Pick(0, GetTimestampFrequency(pFrequency));
	}

	virtual HRESULT STDMETHODCALLTYPE GetClockCalibration(
	  _Out_ UINT64* pGpuTimestamp,
	  _Out_ UINT64* pCpuTimestamp) final
	{
		// Q: Could the be different for each node?
		return Pick(0, GetClockCalibration(pGpuTimestamp, pCpuTimestamp));
	}

	virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc(void) final
	{
		D3D12_COMMAND_QUEUE_DESC Desc = Pick(0, GetDesc());
		for (int i = 1; i < numTargets; ++i)
			Desc.NodeMask += UINT(!!m_Targets[i]) << i;
		return Desc;
	}

	#pragma endregion

	inline void SyncAdapters(
		ID3D12Fence* pFence,
		UINT64 fencevalue
	)
	{
		BroadcastableD3D12Fence<numTargets>* pFences = (BroadcastableD3D12Fence<numTargets>*)pFence;

		// Let every CL wait for every other command-lists to finish
		m_Targets[0]->Wait(*(*pFences)[1], fencevalue);
		m_Targets[1]->Wait(*(*pFences)[0], fencevalue);
	}

};
