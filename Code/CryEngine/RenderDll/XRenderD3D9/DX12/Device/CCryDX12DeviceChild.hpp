// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/CCryDX12Object.hpp"
#include "DX12/Device/CCryDX12Device.hpp"

DEFINE_GUID(WKPDID_D3DDebugObjectName, 0x429b8c22, 0x9188, 0x4b0c, 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00);
DEFINE_GUID(WKPDID_D3DDebugObjectNameW, 0x4cca5fd8, 0x921f, 0x42c8, 0x85, 0x66, 0x70, 0xca, 0xf2, 0xa9, 0xb7, 0x41);
DEFINE_GUID(WKPDID_D3DDebugClearValue, 0x01234567, 0x0123, 0x0123, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01);

// Specialize std::hash
namespace std {
template<> struct hash<GUID>
{
	size_t operator()(const GUID& guid) const   /*noexcept*/
	{
		//	RPC_STATUS status;
		//	return UuidHash((UUID*)&guid, &status);

		const uint64_t* p = reinterpret_cast<const uint64_t*>(&guid);
		std::hash<uint64_t> hash;
		return hash(p[0]) ^ hash(p[1]);
	}
};
}

template<typename T>
class CCryDX12DeviceChild : public CCryDX12Object<T>
{
public:
	DX12_OBJECT(CCryDX12DeviceChild, CCryDX12Object<T> );

	#if !defined(RELEASE)
	std::string GetName()
	{
		UINT len = 512;
		char str[512] = "-";

		GetPrivateData(WKPDID_D3DDebugObjectName, &len, str);

		return str;
	}

	HRESULT STDMETHODCALLTYPE GetPrivateCustomData(
	  _In_ REFGUID guid,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_opt_(*pDataSize)  void* pData)
	{
		TDatas::iterator elm = m_Data.find(guid);
		if (elm != m_Data.end())
		{
			void* Blob = (*elm).second.second;
			UINT nCopied = std::min(*pDataSize, (*elm).second.first);

			*pDataSize = nCopied;
			if (pData)
			{
				memcpy(pData, Blob, nCopied);
			}

			return S_OK;
		}

		return E_FAIL;
	}

	HRESULT STDMETHODCALLTYPE SetPrivateCustomData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData)
	{
		TDatas::iterator elm = m_Data.find(guid);
		if (elm != m_Data.end())
		{
			free((*elm).second.second);
		}

		if (pData)
		{
			void* Blob = malloc(DataSize);
			m_Data[guid] = std::make_pair(DataSize, Blob);
			memcpy(Blob, pData, DataSize);
		}
		else
		{
			m_Data.erase(elm);
		}

		return S_OK;
	}
	#endif

	#pragma region /* ID3D11DeviceChild implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDevice(
	  _Out_ ID3D11Device** ppDevice) FINALGFX
	{
		*ppDevice = nullptr;
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE GetPrivateData(
	  _In_ REFGUID guid,
	  _Inout_ UINT* pDataSize,
	  _Out_writes_bytes_opt_(*pDataSize)  void* pData) FINALGFX
	{
	#if !defined(RELEASE)
		if (m_pChild)
		{
			return m_pChild->GetPrivateData(guid, pDataSize, pData);
		}

		return GetPrivateCustomData(guid, pDataSize, pData);
	#else
		return E_FAIL;
	#endif
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateData(
	  _In_ REFGUID guid,
	  _In_ UINT DataSize,
	  _In_reads_bytes_opt_(DataSize)  const void* pData) FINALGFX
	{
	#if !defined(RELEASE)
		if (m_pChild)
		{
			if (pData)
			{
				if (guid == WKPDID_D3DDebugObjectName)
				{
					wchar_t objectname[4096] = { 0 };
					size_t len = strlen((char*)pData);
					MultiByteToWideChar(0, 0, (char*)pData, len, objectname, len);
					m_pChild->SetName((LPCWSTR)objectname);
				}

				m_pChild->SetPrivateData(guid, 0, nullptr);
			}

			return m_pChild->SetPrivateData(guid, DataSize, pData);
		}

		return SetPrivateCustomData(guid, DataSize, pData);
	#else
		return E_FAIL;
	#endif
	}

	VIRTUALGFX HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
	  _In_ REFGUID guid,
	  _In_opt_ const IUnknown* pData) FINALGFX
	{
	#if !defined(RELEASE)
		if (m_pChild)
		{
			return m_pChild->SetPrivateDataInterface(guid, pData);
		}
	#endif

		return E_FAIL;
	}

	#pragma endregion

protected:
	CCryDX12DeviceChild(CCryDX12Device* pDevice, ID3D12DeviceChild* pChild)
	{
	#if !defined(RELEASE)
		m_pChild = pChild;
	#endif
	}

private:
	#if !defined(RELEASE)
	typedef std::unordered_map<GUID, std::pair<UINT, void*>> TDatas;
	TDatas             m_Data;

	ID3D12DeviceChild* m_pChild;
	#endif
};
