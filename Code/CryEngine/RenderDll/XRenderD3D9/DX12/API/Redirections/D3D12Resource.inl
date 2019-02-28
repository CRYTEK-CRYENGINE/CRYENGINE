// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12Resource : public ID3D12Resource, public Handable<BroadcastableD3D12Resource<numTargets>*, numTargets>
{
	template<const int numTargets> friend class BroadcastableD3D12Device;
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;
	friend Handable;

public:
	int m_RefCount;
	std::array<ID3D12Resource*, numTargets> m_Targets;
	ID3D12Resource* const* operator[](int i) const { return &m_Targets[i]; }

#define Share(i, resource)                          \
	if (!m_Targets[i] && (m_Targets[i] = resource)) \
		m_Targets[i]->AddRef();                     \

	// Debug only, because it's really slow
	bool IsShared(UINT pos) const
	{
		for (int i = 0; i < pos; ++i)
		{
			if (m_Targets[i] == m_Targets[pos])
			{
				return true;
			}
		}

		return false;
	}

	bool IsShared() const
	{
		for (int pos = 0; pos < numTargets; ++pos)
		{
			for (int i = pos + 1; i < numTargets; ++i)
			{
				if (m_Targets[i] == m_Targets[pos])
				{
					return true;
				}
			}
		}

		return false;
	}

	UINT GetSharedOrigin(UINT pos) const
	{
		for (int i = 0; i < pos; ++i)
		{
			if (m_Targets[i] == m_Targets[pos])
			{
				return i;
			}
		}

		return pos;
	}

	typedef BroadcastableD3D12Resource<numTargets> self;
public:
	BroadcastableD3D12Resource(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
	  D3D12_HEAP_FLAGS HeapFlags,
	  _In_ const D3D12_RESOURCE_DESC* pResourceDesc,
	  D3D12_RESOURCE_STATES InitialResourceState,
	  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	  REFIID riid) : m_RefCount(1), Handable(this)
	{
		D3D12_GPU_DELTA_ADDRESS deltaGPUAddresses = { 0ULL, 0ULL };
		D3D12_HEAP_PROPERTIES Properties = *pHeapProperties;
		D3D12_RESOURCE_DESC Desc = *pResourceDesc;

#if DX12_MAP_STAGE_MULTIGPU
		m_Size = 0;

		if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			Desc.Width = Align(Desc.Width, std::min<UINT64>(CRY_PLATFORM_ALIGNMENT, std::max<UINT64>(CRY_PLATFORM_ALIGNMENT, Desc.Alignment)));

			// NOTE: Buffers have 256 byte alignment requirement and Textures 64k, we only need SSE alignment for uploads
			m_Size = Desc.Width;
		}

		m_MapCount = 0;
		m_Buffer = nullptr;
#endif

		DX12_ASSERT(m_Handle < DX12_MULTIGPU_NUM_RESOURCES, "Too many resources allocated, adjust the vector-size!");
		DX12_ASSERT(pHeapProperties->CreationNodeMask != 0, "Creation   of 0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;

			if (Properties.CreationNodeMask = (pHeapProperties->CreationNodeMask & (1 << i)))
			{
				Properties.VisibleNodeMask = pHeapProperties->CreationNodeMask | pHeapProperties->VisibleNodeMask;

#if DX12_LINKEDADAPTER_SIMULATION
				// Always create on the first GPU, if running simulation
				if (CRenderer::CV_r_StereoEnableMgpu < 0)
					Properties.CreationNodeMask = Properties.VisibleNodeMask = 1;
#endif

				HRESULT ret = pDevice->CreateCommittedResource(
				  &Properties, HeapFlags, &Desc, InitialResourceState, pOptimizedClearValue, riid, (void**)&m_Targets[i]);
				DX12_ASSERT(ret == S_OK, "Failed to create comitted resource!");
			}
		}

		for (int i = 0; i < numTargets; ++i)
		{
			if (!m_Targets[i] && (pHeapProperties->VisibleNodeMask & (1 << i)))
			{
				const UINT src = countTrailingZeros32(pHeapProperties->CreationNodeMask);
				Share(i, m_Targets[src]);
#if DX12_LINKEDADAPTER_SIMULATION
				deltaGPUAddresses[i] = deltaGPUAddresses[src];
#endif
			}

			// ERROR #745: GetGPUVirtualAddress returns NULL for non-buffer resources.
			else if (m_Targets[i] && (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER))
			{
				DX12_ASSERT(!IsShared(i), "GetGPUVirtualAddress is not valid on a shared resource");
				deltaGPUAddresses[i] = m_Targets[i]->GetGPUVirtualAddress() - (UINT64(m_Handle) << 32);
			}
		}

		if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			AssignGPUDeltasToHandle(m_Handle, deltaGPUAddresses);
		}
	}

	BroadcastableD3D12Resource(
		_In_ ID3D12Device* pDevice,
		_In_ ID3D12Resource* pResource,
		REFIID riid) : m_RefCount(1), Handable(this)
	{
		D3D12_GPU_DELTA_ADDRESS deltaGPUAddresses = { 0ULL, 0ULL };
		D3D12_HEAP_PROPERTIES Properties;
		pResource->GetHeapProperties(&Properties, nullptr);
		D3D12_RESOURCE_DESC Desc = pResource->GetDesc();

#if DX12_MAP_STAGE_MULTIGPU
		m_Size = 0;

		if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			// NOTE: Buffers have 256 byte alignment requirement and Textures 64k, we only need SSE alignment for uploads
			m_Size = Desc.Width;
		}

		m_MapCount = 0;
		m_Buffer = nullptr;
#endif

		DX12_ASSERT(this->m_Handle < DX12_MULTIGPU_NUM_RESOURCES, "Too many resources allocated, adjust the vector-size!");
		DX12_ASSERT(Properties.VisibleNodeMask  != 0, "Visibility of 0 is not allowed in the broadcaster!");
		DX12_ASSERT(Properties.CreationNodeMask != 0, "Creation   of 0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;

			// There is only one bit set and only one target assigned here
			if (Properties.CreationNodeMask & (1 << i))
			{
				Share(i, pResource);
			}
		}

		for (int i = 0; i < numTargets; ++i)
		{
			if (!m_Targets[i] && (Properties.VisibleNodeMask & (1 << i)))
			{
				const UINT src = countTrailingZeros32(Properties.CreationNodeMask);
				Share(i, m_Targets[src]);
#if DX12_LINKEDADAPTER_SIMULATION
				deltaGPUAddresses[i] = deltaGPUAddresses[src];
#endif
			}

			// ERROR #745: GetGPUVirtualAddress returns NULL for non-buffer resources.
			else if (m_Targets[i] && (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER))
			{
				DX12_ASSERT(!IsShared(i), "GetGPUVirtualAddress is not valid on a shared resource");
				deltaGPUAddresses[i] = m_Targets[i]->GetGPUVirtualAddress() - (UINT64(this->m_Handle) << 32);
			}
		}

		if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			AssignGPUDeltasToHandle(this->m_Handle, deltaGPUAddresses);
		}
	}

	BroadcastableD3D12Resource(
	  _In_ ID3D12Device* pDevice,
	  _In_ ID3D12Resource** pResources,
	  REFIID riid) : m_RefCount(1), Handable(this)
	{
		D3D12_GPU_DELTA_ADDRESS deltaGPUAddresses = { 0ULL, 0ULL };
		D3D12_RESOURCE_DESC Desc;

		DX12_ASSERT(m_Handle < DX12_MULTIGPU_NUM_RESOURCES, "Too many resources allocated, adjust the vector-size!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;

			Share(i, pResources[i]);
		}

		for (int i = 0; i < numTargets; ++i)
		{
			Desc = m_Targets[i]->GetDesc();	

			// ERROR #745: GetGPUVirtualAddress returns NULL for non-buffer resources.
			if (m_Targets[i] && (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER))
			{
				DX12_ASSERT(!IsShared(i), "GetGPUVirtualAddress is not valid on a shared resource");
				deltaGPUAddresses[i] = m_Targets[i]->GetGPUVirtualAddress() - (UINT64(m_Handle) << 32);
			}
		}

		if (Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			AssignGPUDeltasToHandle(m_Handle, deltaGPUAddresses);
		}

#if DX12_MAP_STAGE_MULTIGPU
		// NOTE: Buffers have 256 byte alignment requirement and Textures 64k, we only need SSE alignment for uploads
		m_Size = 0;
		m_MapCount = 0;
		m_Buffer = nullptr;
#endif
	}

#if DX12_MAP_STAGE_MULTIGPU
	UINT64 m_Size;
	int m_MapCount;
	void* m_Buffer;
	void* m_RealBuffer;

	~BroadcastableD3D12Resource()
	{
	//	assert(m_MapCount == 0);
		if (m_MapCount > 0)
		{
#if 0
			void* pInData = m_Buffer, *pOutData;
			const D3D12_RANGE sNoRead = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin
			const D3D12_RANGE FullWrite = { 0, m_Size };

			for (int i = 0; i < numTargets; ++i)
			{
				HRESULT hr = Pick(i, Map(0, &sNoRead, &pOutData));
				cryMemcpy((char*)pOutData + pWrittenRange->Begin, (char*)pInData + pWrittenRange->Begin, pWrittenRange->End - pWrittenRange->Begin, MC_CPU_TO_GPU);
				Pick(i, Unmap(0, &FullWrite));
			}
#endif
		}

		delete[] m_Buffer;
	}
#endif

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

	static HRESULT STDMETHODCALLTYPE CreateResource(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_HEAP_PROPERTIES* pHeapProperties,
	  D3D12_HEAP_FLAGS HeapFlags,
	  _In_ const D3D12_RESOURCE_DESC* pResourceDesc,
	  D3D12_RESOURCE_STATES InitialResourceState,
	  _In_opt_ const D3D12_CLEAR_VALUE* pOptimizedClearValue,
	  REFIID riid,
	  _COM_Outptr_ void** ppvResource)
	{
		return (*ppvResource = new self(pDevice, pHeapProperties, HeapFlags, pResourceDesc, InitialResourceState, pOptimizedClearValue, riid)) ? S_OK : E_FAIL;
	}

	static void DuplicateMetaData(
		_In_ ID3D12Resource* pInputResource,
		_In_ ID3D12Resource* pOutputResource)
	{
		BroadcastableD3D12Resource<2>* pIn  = (BroadcastableD3D12Resource<2>*)pInputResource;
		BroadcastableD3D12Resource<2>* pOut = (BroadcastableD3D12Resource<2>*)pOutputResource;

#if DX12_MAP_STAGE_MULTIGPU
		pOut->m_Size = pIn->m_Size;
		pOut->m_MapCount = pIn->m_MapCount;
		if (pIn->m_Buffer)
			pOut->m_Buffer = new unsigned char[pIn->m_Size];

		const D3D12_RANGE sNoRead = { 0, 0 };
		for (int nm = 0; nm < pOut->m_MapCount; ++nm)
			pOut->Pick(0, Map(0, &sNoRead, &pOut->m_RealBuffer));
#endif
	}

	#pragma region /* IUnknown implementation */

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(
	  /* [in] */ REFIID riid,
	  /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) final
	{
		abort();
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

	#pragma region /* ID3D12Resource implementation */

	virtual HRESULT STDMETHODCALLTYPE Map(
	  UINT Subresource,
	  _In_opt_ const D3D12_RANGE* pReadRange,
	  _Outptr_opt_result_bytebuffer_(_Inexpressible_("Dependent on resource"))  void** ppData) final
	{
#if DX12_LINKEDADAPTER_SIMULATION
		// Always create on the first GPU, if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0)
			return Pick(0, Map(Subresource, pReadRange, ppData));
#endif

#if DX12_MAP_STAGE_MULTIGPU
		HRESULT hr = Pick(0, Map(Subresource, pReadRange, &m_RealBuffer));

		int count = CryInterlockedIncrement(&m_MapCount);
		if (count == 1)
		{
			m_Buffer = new unsigned char[m_Size];
		}

		if (pReadRange->End > pReadRange->Begin)
		{
			DX12_ASSERT(pReadRange->End <= m_Size, "Out-of-bounds access!");

			FetchBufferData((unsigned char*)m_Buffer + pReadRange->Begin, (unsigned char*)m_RealBuffer + pReadRange->Begin, pReadRange->End - pReadRange->Begin);
		}

		if (ppData)
			*ppData = m_Buffer;

		return hr;
#else
		return Pick(0, Map(Subresource, pReadRange, ppData));
#endif
	}

	virtual void STDMETHODCALLTYPE Unmap(
	  UINT Subresource,
	  _In_opt_ const D3D12_RANGE* pWrittenRange) final
	{
#if DX12_LINKEDADAPTER_SIMULATION
		// Always create on the first GPU, if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0)
			return Pick(0, Unmap(Subresource, pWrittenRange));
#endif

		const D3D12_RANGE sNoRead = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin

#if DX12_MAP_STAGE_MULTIGPU
		if (pWrittenRange->End > pWrittenRange->Begin)
		{
			DX12_ASSERT(pWrittenRange->End <= m_Size, "Out-of-bounds access!");

			void* pInData = m_Buffer, *pOutData;

			for (int i = 0; i < numTargets; ++i)
			{
				Pick(i, Map(Subresource, &sNoRead, &pOutData));
				StreamBufferData((char*)pOutData + pWrittenRange->Begin, (char*)pInData + pWrittenRange->Begin, pWrittenRange->End - pWrittenRange->Begin);
				Pick(i, Unmap(Subresource, pWrittenRange));
			}
		}

		int count = CryInterlockedDecrement(&m_MapCount);
		if (count == 0)
		{
//			__debugbreak();
			delete [] m_Buffer;
			m_Buffer = nullptr;
			m_RealBuffer = nullptr;
		}

		if (count >= 0)
		{
			Pick(0, Unmap(Subresource, pWrittenRange));
		}

		if (count < 0)
		{
			__debugbreak();
		}
#else
		// Nest the broadcasted Map()/Unmap() to prevent getting the VirtualAddressTranslationTable-hit
		if (pWrittenRange->End > pWrittenRange->Begin)
		{
			D3D12_RESOURCE_DESC Desc = m_Targets[0]->GetDesc();

			const D3D12_RANGE NoRead = { 0, 0 };
			void* pInData, *pOutData;
			Pick(0, Map(Subresource, pWrittenRange, &pInData));

			for (int i = 1; i < numTargets; ++i)
			{
				if (!IsShared(i))
				{
					Pick(i, Map(Subresource, sNoRead, &pOutData));
					cryMemcpy((char*)pOutData + pWrittenRange->Begin, (char*)pInData + pWrittenRange->Begin, pWrittenRange->End - pWrittenRange->Begin, MC_CPU_TO_GPU);
					Pick(i, Unmap(Subresource, pWrittenRange));
				}
			}

			Pick(0, Unmap(Subresource, &NoRead));
		}

		Pick(0, Unmap(Subresource, pWrittenRange));
#endif
	}

	virtual D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc(void) final
	{
		return Pick(0, GetDesc());
	}

	virtual D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress(void) final
	{
		// Each target needs unique GPUVirtualAddress to be used by all participating GPUs
		// for anything else but CopyResource/Copy...Region/UpdateSubresource
		DX12_ASSERT(!IsShared(), "GetGPUVirtualAddress is not valid on a shared resource");

		return D3D12_GPU_VIRTUAL_ADDRESS(m_Handle) << 32;
	}

	virtual HRESULT STDMETHODCALLTYPE WriteToSubresource(
	  UINT DstSubresource,
	  _In_opt_ const D3D12_BOX* pDstBox,
	  _In_ const void* pSrcData,
	  UINT SrcRowPitch,
	  UINT SrcDepthPitch) final
	{
		BroadcastWithFail(WriteToSubresource(DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch));
	}

	virtual HRESULT STDMETHODCALLTYPE ReadFromSubresource(
	  _Out_ void* pDstData,
	  UINT DstRowPitch,
	  UINT DstDepthPitch,
	  UINT SrcSubresource,
	  _In_opt_ const D3D12_BOX* pSrcBox) final
	{
		BroadcastWithFail(ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox));
	}

	virtual HRESULT STDMETHODCALLTYPE GetHeapProperties(
	  _Out_opt_ D3D12_HEAP_PROPERTIES* pHeapProperties,
	  _Out_opt_ D3D12_HEAP_FLAGS* pHeapFlags) final
	{
		D3D12_HEAP_PROPERTIES Properties;
		D3D12_HEAP_FLAGS Flags;

		HRESULT res = Pick(0, GetHeapProperties(&Properties, &Flags));
		if (res == S_OK)
		{
			for (int i = 1; i < numTargets; ++i)
			{
				//  CreationNodeMask := bits of first found unique resources
				//  VisibleNodeMask  := bits of resources referencing the unique ones
				Properties.CreationNodeMask += UINT( ! IsShared(i)) << i;
				Properties.VisibleNodeMask  |= UINT(!!m_Targets[i]) << i;
			}

			if (pHeapProperties)
				*pHeapProperties = Properties;
			if (pHeapFlags)
				*pHeapFlags = Flags;
		}

		return res;
	}

	#pragma endregion

	HRESULT STDMETHODCALLTYPE MappedWriteToSubresource(
		_In_ UINT Subresource,
		_In_ const D3D12_RANGE* Range,
		_In_reads_bytes_(Range->End) const void* pInData,
		_In_ const UINT numDataBlocks)
	{
		DX12_ASSERT(Range->End <= m_Size, "Out-of-bounds access!");
		const D3D12_RANGE sNoRead = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin

		void* pOutData;
		for (int i = 0; i < numTargets; ++i)
		{
			const uint8* pInDataBlock = reinterpret_cast<const uint8*>(pInData) + ((Range->End - Range->Begin) * (i % numDataBlocks));

			Pick(i, Map(Subresource, &sNoRead, &pOutData));
			StreamBufferData(reinterpret_cast<uint8*>(pOutData) + Range->Begin, pInDataBlock, Range->End - Range->Begin);
			Pick(i, Unmap(Subresource, Range));
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE MappedReadFromSubresource(
		_In_ UINT Subresource,
		_In_ const D3D12_RANGE* Range,
		_Out_writes_bytes_(Range->End) void* pOutData,
		_In_ const UINT numDataBlocks)
	{
		DX12_ASSERT(Range->End <= m_Size, "Out-of-bounds access!");
		const D3D12_RANGE sNoWrite = { 0, 0 }; // It is valid to specify the CPU won't read any data by passing a range where End is less than or equal to Begin

		void* pInData;
		for (int i = 0; i < numDataBlocks; ++i)
		{
			uint8* pOutDataBlock = reinterpret_cast<uint8*>(pOutData) + ((Range->End - Range->Begin) * (i));

			Pick(0, Map(Subresource, Range, &pInData));
			FetchBufferData(pOutDataBlock, reinterpret_cast<uint8*>(pInData) + Range->Begin, Range->End - Range->Begin);
			Pick(0, Unmap(Subresource, &sNoWrite));
		}

		return S_OK;
	}
};

#ifdef INCLUDE_STATICS
// For all utilized templates
static const Handable<BroadcastableD3D12Resource<2>*, 2>::D3D12_CPU_DELTA_ADDRESS emptyCPUDeltaR = { 0ULL, 0ULL };
static const Handable<BroadcastableD3D12Resource<2>*, 2>::D3D12_GPU_DELTA_ADDRESS emptyGPUDeltaR = { 0ULL, 0ULL };
UINT32 Handable<BroadcastableD3D12Resource<2>*, 2 >::m_HighHandle = 0UL;
std::vector<BroadcastableD3D12Resource<2>*> Handable<BroadcastableD3D12Resource<2>*, 2 >::m_AddressTableLookUp(DX12_MULTIGPU_NUM_RESOURCES, 0ULL);
std::vector<Handable<BroadcastableD3D12Resource<2>*, 2>::D3D12_CPU_DELTA_ADDRESS> Handable<BroadcastableD3D12Resource<2>*, 2 >::m_DeltaCPUAddressTableLookUp(DX12_MULTIGPU_NUM_RESOURCES, emptyCPUDeltaR);
std::vector<Handable<BroadcastableD3D12Resource<2>*, 2>::D3D12_GPU_DELTA_ADDRESS> Handable<BroadcastableD3D12Resource<2>*, 2 >::m_DeltaGPUAddressTableLookUp(DX12_MULTIGPU_NUM_RESOURCES, emptyGPUDeltaR);
ConcQueue<BoundMPMC, UINT32> Handable<BroadcastableD3D12Resource<2>*, 2 >::m_FreeHandles;
#endif
