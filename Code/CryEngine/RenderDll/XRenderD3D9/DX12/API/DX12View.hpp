// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12Base.hpp"

namespace NCryDX12
{

class CSwapChain;

enum EViewType
{
	EVT_Unknown = 0,
	EVT_VertexBufferView,
	EVT_IndexBufferView,
	EVT_ConstantBufferView,
	EVT_ShaderResourceView,
	EVT_UnorderedAccessView,
	EVT_DepthStencilView,
	EVT_RenderTargetView
};

class CView : public CRefCounted
{
public:
	CView();
	virtual ~CView();

	CView(CView&& r);
	CView& operator=(CView&& r);

	bool   Init(CResource& resource, EViewType type, UINT64 size = 0);
	void   Invalidate();

	//template<class T = ID3D12Resource>
	ID3D12Resource*  GetD3D12Resource() const;

	ILINE CResource& GetDX12Resource() const
	{
		return *m_pResource;
	}

	ILINE void SetType(EViewType evt)
	{
		m_Type = evt;
	}
	ILINE EViewType GetType() const
	{
		return m_Type;
	}

	ILINE void HasDesc(bool has)
	{
		m_HasDesc = has;
	}
	ILINE bool HasDesc() const
	{
		return m_HasDesc;
	}

	ILINE bool MapsFullResource() const
	{
		return m_bMapsFullResource;
	}

	ILINE void SetMapsFullResource(bool bMapsFullResource)
	{
		m_bMapsFullResource = bMapsFullResource;
	}

	ILINE void SetMips(TRange<uint16>& mips)
	{
		m_mips = mips;
	}

	ILINE const TRange<uint16>& GetMips() const
	{
		return m_mips;
	}

	ILINE void SetSlices(TRange<uint16>& slices)
	{
		m_slices = slices;
	}

	ILINE const TRange<uint16>& GetSlices() const
	{
		return m_slices;
	}

	ILINE void SetSize(UINT64 size)
	{
		m_Size = size;
	}
	ILINE UINT64 GetSize() const
	{
		return m_Size;
	}

	ILINE D3D12_DEPTH_STENCIL_VIEW_DESC& GetDSVDesc()
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
		return m_unDSVDesc;
	}
	ILINE const D3D12_DEPTH_STENCIL_VIEW_DESC& GetDSVDesc() const
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
		return m_unDSVDesc;
	}

	ILINE D3D12_RENDER_TARGET_VIEW_DESC& GetRTVDesc()
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
		return m_unRTVDesc;
	}
	ILINE const D3D12_RENDER_TARGET_VIEW_DESC& GetRTVDesc() const
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET));
		return m_unRTVDesc;
	}

	ILINE D3D12_VERTEX_BUFFER_VIEW& GetVBVDesc()
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unVBVDesc;
	}
	ILINE const D3D12_VERTEX_BUFFER_VIEW& GetVBVDesc() const
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unVBVDesc;
	}

	ILINE D3D12_INDEX_BUFFER_VIEW& GetIBVDesc()
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unIBVDesc;
	}
	ILINE const D3D12_INDEX_BUFFER_VIEW& GetIBVDesc() const
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unIBVDesc;
	}

	ILINE D3D12_CONSTANT_BUFFER_VIEW_DESC& GetCBVDesc()
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unCBVDesc;
	}
	ILINE const D3D12_CONSTANT_BUFFER_VIEW_DESC& GetCBVDesc() const
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unCBVDesc;
	}

	ILINE D3D12_SHADER_RESOURCE_VIEW_DESC& GetSRVDesc()
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unSRVDesc;
	}
	ILINE const D3D12_SHADER_RESOURCE_VIEW_DESC& GetSRVDesc() const
	{
		//		DX12_ASSERT(m_pResource && true);
		return m_unSRVDesc;
	}

	ILINE D3D12_UNORDERED_ACCESS_VIEW_DESC& GetUAVDesc()
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
		return m_unUAVDesc;
	}
	ILINE const D3D12_UNORDERED_ACCESS_VIEW_DESC& GetUAVDesc() const
	{
		//		DX12_ASSERT(m_pResource && (m_pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
		return m_unUAVDesc;
	}

	ILINE void SetDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& descriptorHandle)
	{
		m_DescriptorHandle = descriptorHandle;
	}
	ILINE D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const
	{
		return m_DescriptorHandle;
	}

private:
	CResource*                  m_pResource;
	EViewType                   m_Type;
	D3D12_CPU_DESCRIPTOR_HANDLE m_DescriptorHandle;

	union
	{
		D3D12_VERTEX_BUFFER_VIEW         m_unVBVDesc;
		D3D12_INDEX_BUFFER_VIEW          m_unIBVDesc;
		D3D12_CONSTANT_BUFFER_VIEW_DESC  m_unCBVDesc;
		D3D12_SHADER_RESOURCE_VIEW_DESC  m_unSRVDesc;
		D3D12_UNORDERED_ACCESS_VIEW_DESC m_unUAVDesc;
		D3D12_DEPTH_STENCIL_VIEW_DESC    m_unDSVDesc;
		D3D12_RENDER_TARGET_VIEW_DESC    m_unRTVDesc;
	};

	// Some views can be created without descriptor (DSV)
	bool           m_HasDesc;
	bool           m_bMapsFullResource;

	TRange<uint16> m_slices;
	TRange<uint16> m_mips;

	// View size in bytes
	UINT64 m_Size;
};

}
