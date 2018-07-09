// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12Fence : public ID3D12Fence
{
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;

	int                                  m_RefCount;
	std::array<ID3D12Fence*, numTargets> m_Targets;
	std::array<HANDLE, numTargets>       m_Events;
	std::array<UINT64, numTargets>       m_CompletedValues;
	ID3D12Fence* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12Fence<numTargets> self;
public:
	BroadcastableD3D12Fence(
	  _In_ ID3D12Device* pDevice,
	  _In_ UINT64 InitialValue,
	  _In_ D3D12_FENCE_FLAGS Flags,
	  REFIID riid) : m_RefCount(1)
	{
		DX12_ASSERT(~0, "0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			HRESULT ret = pDevice->CreateFence(
			  InitialValue, Flags, riid, (void**)&m_Targets[i]);
			DX12_ASSERT(ret == S_OK, "Failed to create fence!");

			m_Events[i] = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);
			m_CompletedValues[i] = 0ULL;
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

	static HRESULT STDMETHODCALLTYPE CreateFence(
	  _In_ ID3D12Device* pDevice,
	  _In_ UINT64 InitialValue,
	  _In_ D3D12_FENCE_FLAGS Flags,
	  REFIID riid,
	  _COM_Outptr_ void** ppvFence)
	{
		return (*ppvFence = new self(pDevice, InitialValue, Flags, riid)) ? S_OK : E_FAIL;
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
		Broadcast(SetPrivateData(guid, DataSize, pData));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) final
	{
		Broadcast(SetPrivateDataInterface(guid, pData));
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetName(
	  _In_z_ LPCWSTR Name) final
	{
		Broadcast(SetName(Name));
		return S_OK;
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

	#pragma region /* ID3D12Fence implementation */

	virtual UINT64 STDMETHODCALLTYPE GetCompletedValue(void) final
	{
		UINT64 fenceValue = ~0ULL;
		for (int i = 0; i < numTargets; ++i)
		{
			m_CompletedValues[i] = std::max(m_CompletedValues[i], m_Targets[i]->GetCompletedValue());
			fenceValue = std::min(fenceValue, m_CompletedValues[i]);
		}

		return fenceValue;
	}

	virtual HRESULT STDMETHODCALLTYPE SetEventOnCompletion(
	  UINT64 Value,
	  HANDLE hEvent) final
	{
		abort();
	}

	virtual HRESULT STDMETHODCALLTYPE Signal(
	  UINT64 Value) final
	{
		BroadcastWithFail(Signal(Value));
	}

	#pragma endregion

	HRESULT STDMETHODCALLTYPE WaitForCompletion(
	  UINT64 Value)
	{
		// NOTE: event does ONLY trigger when the value has been set (it'll lock when trying with 0)
		int numEvents = 0;
		for (int i = 0; i < numTargets; ++i)
		{
			if (Value && (m_CompletedValues[i] < Value))
			{
				m_Targets[i]->SetEventOnCompletion(Value, m_Events[numEvents++]);
			}
		}

		if (numEvents)
			WaitForMultipleObjects(numEvents, &m_Events[0], TRUE, INFINITE);

		return S_OK;
	}
};
