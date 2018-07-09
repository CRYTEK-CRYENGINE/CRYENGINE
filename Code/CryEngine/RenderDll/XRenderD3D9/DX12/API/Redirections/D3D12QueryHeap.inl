// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12QueryHeap : public ID3D12QueryHeap
{
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;

	int m_RefCount;
	std::array<ID3D12QueryHeap*, numTargets> m_Targets;
	ID3D12QueryHeap* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12QueryHeap<numTargets> self;
public:
	BroadcastableD3D12QueryHeap(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_QUERY_HEAP_DESC* pDesc,
	  REFIID riid) : m_RefCount(1)
	{
		D3D12_QUERY_HEAP_DESC Desc = *pDesc;

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

				HRESULT ret = pDevice->CreateQueryHeap(
				  &Desc, riid, (void**)&m_Targets[i]);
				DX12_ASSERT(ret == S_OK, "Failed to create query heap!");
			}
		}
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

	static HRESULT STDMETHODCALLTYPE CreateQueryHeap(
	  _In_ ID3D12Device* pDevice,
	  _In_ const D3D12_QUERY_HEAP_DESC* pDesc,
	  REFIID riid,
	  _COM_Outptr_ void** ppvHeap)
	{
		return (*ppvHeap = new self(pDevice, pDesc, riid)) ? S_OK : E_FAIL;
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
};
