// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <array>
#include <d3d12.h>

template<const int numTargets>
class BroadcastableD3D12CommandAllocator : public ID3D12CommandAllocator
{
	template<const int numTargets> friend class BroadcastableD3D12CommandQueue;
	template<const int numTargets> friend class BroadcastableD3D12GraphicsCommandList;

	int m_RefCount;
	std::array<ID3D12CommandAllocator*, numTargets> m_Targets;
	ID3D12CommandAllocator* const* operator[](int i) const { return &m_Targets[i]; }

	typedef BroadcastableD3D12CommandAllocator<numTargets> self;
public:
	BroadcastableD3D12CommandAllocator(
	  _In_ ID3D12Device* pDevice,
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  REFIID riid) : m_RefCount(1)
	{
		DX12_ASSERT(~0, "0 is not allowed in the broadcaster!");
		for (int i = 0; i < numTargets; ++i)
		{
			HRESULT ret = pDevice->CreateCommandAllocator(
			  type, riid, (void**)&m_Targets[i]);
			DX12_ASSERT(ret == S_OK, "Failed to create fence!");
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

	static HRESULT STDMETHODCALLTYPE CreateCommandAllocator(
	  _In_ ID3D12Device* pDevice,
	  _In_ D3D12_COMMAND_LIST_TYPE type,
	  REFIID riid,
	  _COM_Outptr_ void** ppCommandAllocator)
	{
		return (*ppCommandAllocator = new self(pDevice, type, riid)) ? S_OK : E_FAIL;
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

	#pragma region /* ID3D12CommandAllocator implementation */

	virtual HRESULT STDMETHODCALLTYPE Reset(void)
	{
		BroadcastWithFail(Reset());
	}

	#pragma endregion
};
