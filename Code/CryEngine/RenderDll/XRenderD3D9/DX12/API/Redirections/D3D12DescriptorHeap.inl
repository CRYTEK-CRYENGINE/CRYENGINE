// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12DescriptorHeap : public ID3D12DescriptorHeap, public Handable<BroadcastableD3D12DescriptorHeap<numTargets>*, numTargets>
{
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;
	friend Handable;

	int m_RefCount;
	std::array<ID3D12DescriptorHeap*, numTargets> m_Targets;
	ID3D12DescriptorHeap* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12DescriptorHeap<numTargets> self;
public:
	BroadcastableD3D12DescriptorHeap(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc,
	  REFIID riid) : m_RefCount(1), Handable(this)
	{
		D3D12_CPU_DELTA_ADDRESS deltaCPUAddresses = { 0ULL, 0ULL };
		D3D12_GPU_DELTA_ADDRESS deltaGPUAddresses = { 0ULL, 0ULL };
		D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc = *pDescriptorHeapDesc;

		DX12_ASSERT(m_Handle < DX12_MULTIGPU_NUM_DESCRIPTORHEAPS, "Too many descriptor heaps allocated, adjust the vector-size!");
		DX12_ASSERT(pDescriptorHeapDesc->NodeMask != 0, "0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			m_Targets[i] = nullptr;

			if (DescriptorHeapDesc.NodeMask = (pDescriptorHeapDesc->NodeMask & (1 << i)))
			{
#if DX12_LINKEDADAPTER_SIMULATION
				// Always create on the first GPU, if running simulation
				if (CRenderer::CV_r_StereoEnableMgpu < 0)
					DescriptorHeapDesc.NodeMask = 1;
#endif

				HRESULT ret = pDevice->CreateDescriptorHeap(
				  &DescriptorHeapDesc, riid, (void**)&m_Targets[i]);
				DX12_ASSERT(ret == S_OK, "Failed to create descriptor heap!");
			}

			if (m_Targets[i])
			{
				deltaCPUAddresses[i] = m_Targets[i]->GetCPUDescriptorHandleForHeapStart().ptr - (SIZE_T(m_Handle) << 32);
				deltaGPUAddresses[i] = m_Targets[i]->GetGPUDescriptorHandleForHeapStart().ptr - (SIZE_T(m_Handle) << 32);
			}
		}

		AssignCPUDeltasToHandle(m_Handle, deltaCPUAddresses);
		AssignGPUDeltasToHandle(m_Handle, deltaGPUAddresses);
	}

#define Pick(i, func) \
  m_Targets[i]->func;

#define Broadcast(func)                  \
  if (numTargets > 2)                    \
  {                                      \
    for (int i = 0; i < numTargets; ++i) \
      if (m_Targets[i])                  \
        m_Targets[i]->func;              \
  }                                      \
  else                                   \
  {                                      \
    m_Targets[0]->func;                  \
    m_Targets[1]->func;                  \
  }

	static HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppvHeap)
	{
		return (*ppvHeap = new self(pDevice, pDescriptorHeapDesc, riid)) ? S_OK : E_FAIL;
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

	#pragma region /* ID3D12DescriptorHeap implementation */

	virtual D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE GetDesc(void) final
	{
		D3D12_DESCRIPTOR_HEAP_DESC Desc = Pick(0, GetDesc());
		for (int i = 1; i < numTargets; ++i)
			Desc.NodeMask += UINT(!!m_Targets[i]) << i;
		return Desc;
	}

	virtual D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart(void) final
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = SIZE_T(m_Handle) << 32;
		return handle;
	}

	virtual D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart(void) final
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = SIZE_T(m_Handle) << 32;
		return handle;
	}

	#pragma endregion

};

#ifdef INCLUDE_STATICS
// For all utilized templates
static const Handable<BroadcastableD3D12DescriptorHeap<2>*, 2>::D3D12_CPU_DELTA_ADDRESS emptyCPUDeltaDH = { 0ULL, 0ULL };
static const Handable<BroadcastableD3D12DescriptorHeap<2>*, 2>::D3D12_GPU_DELTA_ADDRESS emptyGPUDeltaDH = { 0ULL, 0ULL };
UINT32 Handable<BroadcastableD3D12DescriptorHeap<2>*, 2 >::m_HighHandle = 0UL;
std::vector<BroadcastableD3D12DescriptorHeap<2>*>                                       Handable<BroadcastableD3D12DescriptorHeap<2>*, 2 >::m_AddressTableLookUp(DX12_MULTIGPU_NUM_DESCRIPTORHEAPS, 0ULL);
std::vector<Handable<BroadcastableD3D12DescriptorHeap<2>*, 2>::D3D12_CPU_DELTA_ADDRESS> Handable<BroadcastableD3D12DescriptorHeap<2>*, 2 >::m_DeltaCPUAddressTableLookUp(DX12_MULTIGPU_NUM_DESCRIPTORHEAPS, emptyCPUDeltaDH);
std::vector<Handable<BroadcastableD3D12DescriptorHeap<2>*, 2>::D3D12_GPU_DELTA_ADDRESS> Handable<BroadcastableD3D12DescriptorHeap<2>*, 2 >::m_DeltaGPUAddressTableLookUp(DX12_MULTIGPU_NUM_DESCRIPTORHEAPS, emptyGPUDeltaDH);
ConcQueue<BoundMPMC, UINT32>                                                            Handable<BroadcastableD3D12DescriptorHeap<2>*, 2 >::m_FreeHandles;
#endif
