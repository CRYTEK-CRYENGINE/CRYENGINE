// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Resource/CCryDX12Resource.hpp"
#include "DX12/API/DX12View.hpp"

class CCryDX12Buffer : public CCryDX12Resource<ID3D11BufferToImplement>
{
public:
	DX12_OBJECT(CCryDX12Buffer, CCryDX12Resource<ID3D11BufferToImplement> );

	static CCryDX12Buffer* Create(CCryDX12Device* pDevice);
	static CCryDX12Buffer* Create(CCryDX12Device* pDevice, ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState);
	static CCryDX12Buffer* Create(CCryDX12Device* pDevice, CCryDX12SwapChain* pSwapChain, ID3D12Resource* pResource);
	static CCryDX12Buffer* Create(CCryDX12Device* pDevice, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);

	ILINE UINT GetStructureByteStride() const
	{
		// NOTE: masking by (m_Desc11.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) was done in the constructor, no need to check here
		return m_Desc11.StructureByteStride;
	}

	NCryDX12::CView& GetDX12View()
	{
		return m_DX12View;
	}

	const NCryDX12::CView& GetDX12View() const
	{
		return m_DX12View;
	}

	#pragma region /* ICryDX12Resource implementation */

	VIRTUALGFX EDX12ResourceType GetDX12ResourceType() const FINALGFX
	{
		return eDX12RT_Buffer;
	}

	VIRTUALGFX void STDMETHODCALLTYPE GetType(_Out_ D3D11_RESOURCE_DIMENSION* pResourceDimension) FINALGFX
	{
		if (pResourceDimension)
		{
			*pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
		}
	}

	VIRTUALGFX bool SubstituteUsed() FINALGFX;

	#pragma endregion

	#pragma region /* ID3D11Buffer implementation */

	VIRTUALGFX void STDMETHODCALLTYPE GetDesc(
	  _Out_ D3D11_BUFFER_DESC* pDesc) FINALGFX;

	#pragma endregion

protected:
	CCryDX12Buffer(CCryDX12Device* pDevice, const D3D11_BUFFER_DESC& desc11, ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, const D3D12_RESOURCE_DESC& desc12, const D3D11_SUBRESOURCE_DATA* pInitialData = NULL, size_t numInitialData = 0);

private:
	D3D11_BUFFER_DESC m_Desc11;

	NCryDX12::CView   m_DX12View;
};
