// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DX12View.hpp"
#include "DX12Resource.hpp"
#include "DX12SwapChain.hpp"

namespace NCryDX12
{

//---------------------------------------------------------------------------------------------------------------------
CView::CView()
	: CRefCounted()
	, m_pResource(nullptr)
	, m_DescriptorHandle(INVALID_CPU_DESCRIPTOR_HANDLE)
	, m_Type(EVT_Unknown)
	, m_HasDesc(true)
	, m_Size(0)
	, m_bMapsFullResource(true)
{
	// clear before use
	memset(&m_unDSVDesc, 0, sizeof(m_unDSVDesc));
	memset(&m_unRTVDesc, 0, sizeof(m_unRTVDesc));
	memset(&m_unVBVDesc, 0, sizeof(m_unVBVDesc));
	memset(&m_unCBVDesc, 0, sizeof(m_unCBVDesc));
	memset(&m_unSRVDesc, 0, sizeof(m_unSRVDesc));
	memset(&m_unUAVDesc, 0, sizeof(m_unUAVDesc));
}

CView::CView(CView&& v)
	: m_pResource(std::move(v.m_pResource))
	, m_DescriptorHandle(std::move(v.m_DescriptorHandle))
	, m_Type(std::move(v.m_Type))
	, m_HasDesc(std::move(v.m_HasDesc))
	, m_Size(std::move(v.m_Size))
	, m_bMapsFullResource(std::move(v.m_bMapsFullResource))
{
	m_unDSVDesc = std::move(v.m_unDSVDesc);
	m_unRTVDesc = std::move(v.m_unRTVDesc);
	m_unVBVDesc = std::move(v.m_unVBVDesc);
	m_unCBVDesc = std::move(v.m_unCBVDesc);
	m_unSRVDesc = std::move(v.m_unSRVDesc);
	m_unUAVDesc = std::move(v.m_unUAVDesc);

	v.m_pResource = nullptr;
	v.m_DescriptorHandle = INVALID_CPU_DESCRIPTOR_HANDLE;
}

CView& CView::operator=(CView&& v)
{
	m_pResource = std::move(v.m_pResource);
	m_DescriptorHandle = std::move(v.m_DescriptorHandle);
	m_Type = std::move(v.m_Type);
	m_HasDesc = std::move(v.m_HasDesc);
	m_Size = std::move(v.m_Size);
	m_bMapsFullResource = std::move(m_bMapsFullResource);

	m_unDSVDesc = std::move(v.m_unDSVDesc);
	m_unRTVDesc = std::move(v.m_unRTVDesc);
	m_unVBVDesc = std::move(v.m_unVBVDesc);
	m_unCBVDesc = std::move(v.m_unCBVDesc);
	m_unSRVDesc = std::move(v.m_unSRVDesc);
	m_unUAVDesc = std::move(v.m_unUAVDesc);

	v.m_pResource = nullptr;
	v.m_DescriptorHandle = INVALID_CPU_DESCRIPTOR_HANDLE;

	return *this;
}

//---------------------------------------------------------------------------------------------------------------------
CView::~CView()
{
	Invalidate();
}

void CView::Invalidate()
{
	if (INVALID_CPU_DESCRIPTOR_HANDLE != m_DescriptorHandle)
	{
		switch (m_Type)
		{
		case EVT_ShaderResourceView:
			m_pResource->GetDevice()->InvalidateShaderResourceView(&GetSRVDesc(), GetD3D12Resource());
			break;
		case EVT_UnorderedAccessView:
			m_pResource->GetDevice()->InvalidateUnorderedAccessView(&GetUAVDesc(), GetD3D12Resource());
			break;
		case EVT_DepthStencilView:
			m_pResource->GetDevice()->InvalidateDepthStencilView(&GetDSVDesc(), GetD3D12Resource());
			break;
		case EVT_RenderTargetView:
			m_pResource->GetDevice()->InvalidateRenderTargetView(&GetRTVDesc(), GetD3D12Resource());
			break;
		}
	}

	m_pResource = nullptr;
	m_DescriptorHandle = INVALID_CPU_DESCRIPTOR_HANDLE;
}

//---------------------------------------------------------------------------------------------------------------------
//template<class T = ID3D12Resource>
ID3D12Resource* CView::GetD3D12Resource() const
{
	return m_pResource->GetD3D12Resource();
}

//---------------------------------------------------------------------------------------------------------------------
bool CView::Init(CResource& resource, EViewType type, UINT64 size)
{
	DX12_ASSERT(type != EVT_Unknown);
	const D3D12_RESOURCE_DESC& desc = resource.GetDesc();
	const CD3DX12_RESOURCE_DESC descX(desc);

	m_pResource = &resource;
	m_Type = type;
	m_Size = size;
	m_bMapsFullResource = true;

	switch (type)
	{
	case EVT_VertexBufferView:
		m_unVBVDesc.BufferLocation = resource.GetGPUVirtualAddress();
		m_unVBVDesc.SizeInBytes = static_cast<UINT>(m_Size);
		break;

	case EVT_IndexBufferView:
		m_unIBVDesc.BufferLocation = resource.GetGPUVirtualAddress();
		m_unIBVDesc.SizeInBytes = static_cast<UINT>(m_Size);
		break;

	case EVT_ConstantBufferView:
		// Align to 256 bytes as required by DX12
		m_Size = (m_Size + 255) & ~255;

		m_unCBVDesc.BufferLocation = resource.GetGPUVirtualAddress();
		m_unCBVDesc.SizeInBytes = static_cast<UINT>(m_Size);
		break;

	// The *View descriptions have all the same structure and identical enums
	case EVT_ShaderResourceView:
		m_unSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
			m_unSRVDesc.Texture1D.MipLevels = -1;

	case EVT_UnorderedAccessView:
	case EVT_DepthStencilView:
	case EVT_RenderTargetView:
		m_unSRVDesc.Format = desc.Format;

		switch (desc.Dimension)
		{
		case D3D12_RESOURCE_DIMENSION_BUFFER:
			m_unSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
			m_unSRVDesc.ViewDimension = desc.DepthOrArraySize <= 1 ? D3D12_SRV_DIMENSION_TEXTURE1D : D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			m_unSRVDesc.ViewDimension = desc.DepthOrArraySize <= 1 ? (desc.SampleDesc.Count <= 1 ? D3D12_SRV_DIMENSION_TEXTURE2D : D3D12_SRV_DIMENSION_TEXTURE2DMS) : (desc.SampleDesc.Count <= 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY);
			break;
		case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
			m_unSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			break;

		default:
			DX12_NOT_IMPLEMENTED;
			break;
		}
		break;
	}

	return true;
}

}
