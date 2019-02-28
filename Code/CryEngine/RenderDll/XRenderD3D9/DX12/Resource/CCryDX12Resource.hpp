// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

#include "DX12/API/DX12Resource.hpp"

// "res" must be ID3D11Resource
// This is potentialy dangerous, but easy & fast...
#define DX12_EXTRACT_RESOURCE(res) \
	(reinterpret_cast<CCryDX12Resource<ID3D11ResourceToImplement>*>(res))
#define DX12_EXTRACT_RESOURCE_TYPE(res) \
	(reinterpret_cast<CCryDX12Resource<ID3D11ResourceToImplement>*>(res))->GetDX12ResourceType()

#define DX12_EXTRACT_ICRYDX12RESOURCE(res) \
	((res) ? (static_cast<ICryDX12Resource*>(reinterpret_cast<CCryDX12Resource<ID3D11ResourceToImplement>*>(res))) : NULL)
#define DX12_EXTRACT_D3D12RESOURCE(res) \
	((res) ? (static_cast<ICryDX12Resource*>(reinterpret_cast<CCryDX12Resource<ID3D11ResourceToImplement>*>(res)))->GetD3D12Resource() : NULL)

#define DX12_EXTRACT_BUFFER(res) \
	(reinterpret_cast<CCryDX12Buffer*>(res))
#define DX12_EXTRACT_TEXTURE1D(res) \
	(reinterpret_cast<CCryDX12Texture1D*>(res))
#define DX12_EXTRACT_TEXTURE2D(res) \
	(reinterpret_cast<CCryDX12Texture2D*>(res))
#define DX12_EXTRACT_TEXTURE3D(res) \
	(reinterpret_cast<CCryDX12Texture3D*>(res))

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum EDX12ResourceType
{
	eDX12RT_Unknown = 0,
	eDX12RT_Buffer,
	eDX12RT_Texture1D,
	eDX12RT_Texture2D,
	eDX12RT_Texture3D
};

struct ICryDX12Resource
{
	virtual EDX12ResourceType          GetDX12ResourceType() const = 0;
	virtual ID3D12Resource*            GetD3D12Resource() const = 0;
	virtual NCryDX12::CResource&       GetDX12Resource() = 0;
	virtual const NCryDX12::CResource& GetDX12Resource() const = 0;

	virtual bool                       SubstituteUsed() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class CCryDX12Resource : public CCryDX12DeviceChild<T>, public ICryDX12Resource
{
public:
	DX12_OBJECT(CCryDX12Resource, CCryDX12DeviceChild<T> );

	#pragma region /* ICryDX12Resource implementation */

	virtual EDX12ResourceType GetDX12ResourceType() const
	{
		return eDX12RT_Unknown;
	}

	virtual ID3D12Resource* GetD3D12Resource() const final
	{
		return m_DX12Resource.GetD3D12Resource();
	}

	virtual NCryDX12::CResource& GetDX12Resource() final
	{
		return m_DX12Resource;
	}

	virtual const NCryDX12::CResource& GetDX12Resource() const final
	{
		return m_DX12Resource;
	}

	virtual bool SubstituteUsed()
	{
		return m_DX12Resource.SubstituteUsed();
	}

	#pragma endregion

	#pragma region /* ID3D11Resource implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetType(
	  _Out_ D3D11_RESOURCE_DIMENSION* pResourceDimension)
	{
		if (pResourceDimension)
		{
			*pResourceDimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
		}
	}

	VIRTUALGFX void STDMETHODCALLTYPE SetEvictionPriority(
	  _In_ UINT EvictionPriority) FINALGFX
	{

	}

	VIRTUALGFX UINT STDMETHODCALLTYPE GetEvictionPriority() FINALGFX
	{
		return 0;
	}

	#pragma endregion

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
	{
		return m_DX12Resource.GetGPUVirtualAddress();
	}

	template<class T>
	void BeginResourceStateTransition(T* pCmdList, D3D12_RESOURCE_STATES desiredState)
	{
		pCmdList->BeginResourceStateTransition(m_DX12Resource, desiredState);
	}

	template<class T>
	void EndResourceStateTransition(T* pCmdList, D3D12_RESOURCE_STATES desiredState)
	{
		pCmdList->EndResourceStateTransition(m_DX12Resource, desiredState);
	}

protected:
	CCryDX12Resource(CCryDX12Device* pDevice, ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, const D3D12_RESOURCE_DESC& desc, const D3D11_SUBRESOURCE_DATA* pInitialData = NULL, size_t numInitialData = 0)
		: Super(pDevice, pResource)
		, m_DX12Resource(pDevice->GetDX12Device())
	{
		m_DX12Resource.Init(pResource, eInitialState, desc);

		if (pInitialData && numInitialData)
		{
			pDevice->GetDeviceContext()->UploadResource(this, numInitialData, pInitialData);
		}
	}

	NCryDX12::CResource m_DX12Resource;
};
