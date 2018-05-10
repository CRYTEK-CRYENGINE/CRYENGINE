// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Buffer.hpp"
#include "../GI/CCryDX12SwapChain.hpp"

CCryDX12Buffer* CCryDX12Buffer::Create(CCryDX12Device* pDevice)
{
	D3D12_RESOURCE_DESC desc12;
	D3D11_BUFFER_DESC desc11;

	ZeroStruct(desc12);
	ZeroStruct(desc11);

	desc12.Format = DXGI_FORMAT_UNKNOWN;
	desc12.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc11.Usage = D3D11_USAGE_DEFAULT;

	CCryDX12Buffer* result = DX12_NEW_RAW(CCryDX12Buffer(pDevice, desc11, nullptr, D3D12_RESOURCE_STATE_COMMON, CD3DX12_RESOURCE_DESC(desc12), NULL));

	return result;
}

CCryDX12Buffer* CCryDX12Buffer::Create(CCryDX12Device* pDevice, ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState)
{
	DX12_ASSERT(pResource);

	D3D12_RESOURCE_DESC desc12 = pResource->GetDesc();

	D3D11_BUFFER_DESC desc11;
	desc11.ByteWidth = 0;           // static_cast<UINT>(desc12.ByteWidth);
	desc11.StructureByteStride = 0; // static_cast<UINT>(desc12.StructureByteStride);
	desc11.Usage = D3D11_USAGE_DEFAULT;
	desc11.CPUAccessFlags = 0;
	desc11.BindFlags = 0;
	desc11.MiscFlags = 0;

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		desc11.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		desc11.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		desc11.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	if (!(desc12.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
	{
		desc11.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	CCryDX12Buffer* result = DX12_NEW_RAW(CCryDX12Buffer(pDevice, desc11, pResource, initialState, CD3DX12_RESOURCE_DESC(desc12), NULL));

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Buffer* CCryDX12Buffer::Create(CCryDX12Device* pDevice, CCryDX12SwapChain* pSwapChain, ID3D12Resource* pResource)
{
	CCryDX12Buffer* result = Create(pDevice, pResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
	result->GetDX12Resource().SetDX12SwapChain(pSwapChain->GetDX12SwapChain());

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Buffer* CCryDX12Buffer::Create(CCryDX12Device* pDevice, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DX12_ASSERT(pDesc, "CreateBuffer() called without description!");

	D3D12_RESOURCE_DESC desc12 = CD3DX12_RESOURCE_DESC::Buffer((pDesc->ByteWidth + 255) & ~255);

	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pDevice->GetCreationMask(false), pDevice->GetVisibilityMask(false));
	D3D12_RESOURCE_STATES resourceUsage = D3D12_RESOURCE_STATE_COMMON;

	if (pDesc->Usage == D3D11_USAGE_IMMUTABLE)
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pDevice->GetCreationMask(false), pDevice->GetVisibilityMask(false));
	else if (pDesc->Usage == D3D11_USAGE_STAGING)
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
	else if (pDesc->Usage == D3D11_USAGE_DYNAMIC)
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));

	if (pDesc->CPUAccessFlags != 0)
	{
		// TODO: Check feasibility and performance of allocating staging textures as row major buffers
		// and using CopyTextureRegion instead of ReadFromSubresource/WriteToSubresource

		if (pDesc->CPUAccessFlags == D3D11_CPU_ACCESS_WRITE)
			heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
		else if (pDesc->CPUAccessFlags == D3D11_CPU_ACCESS_READ)
			heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
		else
		{
			DX12_NOT_IMPLEMENTED;
			return nullptr;
		}
	}

	if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE)
		resourceUsage = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (pDesc->BindFlags & (D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER))
		resourceUsage = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER)
		resourceUsage = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS)
		resourceUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL)
		resourceUsage = D3D12_RESOURCE_STATE_DEPTH_WRITE, desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (pDesc->BindFlags & D3D11_BIND_RENDER_TARGET)
		resourceUsage = D3D12_RESOURCE_STATE_RENDER_TARGET, desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	if (!(pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE))
		desc12.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	// Certain heaps are restricted to certain D3D12_RESOURCE_STATES states, and cannot be changed.
	// D3D12_HEAP_TYPE_UPLOAD requires D3D12_RESOURCE_STATE_GENERIC_READ.
	if (heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD)
		resourceUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (heapProperties.Type == D3D12_HEAP_TYPE_READBACK)
		resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;

	// Any of these flags require a view with a GPUVirtualAddress, in which case the resource needs to be duplicated for all GPUs
	if (pDesc->BindFlags & (
		D3D11_BIND_VERTEX_BUFFER   | // vertex buffer    view
		D3D11_BIND_INDEX_BUFFER    | // index buffer     view
		D3D11_BIND_CONSTANT_BUFFER | // constant buffer  view
		D3D11_BIND_SHADER_RESOURCE | // shader resource  view
		D3D11_BIND_RENDER_TARGET   | // render target    view
		D3D11_BIND_DEPTH_STENCIL   | // depth stencil    view
		D3D11_BIND_UNORDERED_ACCESS  // unordered access view
	))
	{
		heapProperties.CreationNodeMask = pDevice->GetCreationMask(false) | pDevice->GetVisibilityMask(false);
		heapProperties.VisibleNodeMask = pDevice->GetVisibilityMask(false);

#if DX12_LINKEDADAPTER_SIMULATION
		// Always allow getting GPUAddress (CreationMask == VisibilityMask), if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0 && (heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || heapProperties.Type == D3D12_HEAP_TYPE_READBACK))
		{
			heapProperties.CreationNodeMask = 1U;
			heapProperties.VisibleNodeMask = pDevice->GetCreationMask(false) | pDevice->GetVisibilityMask(false);
		}
#endif
	}

	ID3D12Resource* resource = NULL;
	D3D11_SUBRESOURCE_DATA sInitialData;
	if (pInitialData)
	{
		sInitialData = *pInitialData;
		sInitialData.SysMemPitch = pDesc->ByteWidth;
		sInitialData.SysMemSlicePitch = pDesc->ByteWidth;
		pInitialData = &sInitialData;

		// Anticipate deferred initial upload
		resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;
	}

	HRESULT hresult = S_OK;
	if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP)
	{
		hresult = pDevice->GetDX12Device()->CreateOrReuseCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc12,
			resourceUsage,
			NULL,
			IID_GFX_ARGS(&resource)
		);
	}
	else
	{
		hresult = pDevice->GetD3D12Device()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc12,
			resourceUsage,
			NULL,
			IID_GFX_ARGS(&resource)
		);
	}

	if ((hresult != S_OK) || !resource)
	{
		DX12_ASSERT(0, "Could not create buffer resource!");
		return NULL;
	}

	auto result = DX12_NEW_RAW(CCryDX12Buffer(pDevice, *pDesc, resource, resourceUsage, CD3DX12_RESOURCE_DESC(resource->GetDesc()), pInitialData, 1));
	resource->Release();

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Buffer::CCryDX12Buffer(CCryDX12Device* pDevice, const D3D11_BUFFER_DESC& desc11, ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, const D3D12_RESOURCE_DESC& desc12, const D3D11_SUBRESOURCE_DATA* pInitialData, size_t numInitialData)
	: Super(pDevice, pResource, eInitialState, desc12, pInitialData, numInitialData)
	, m_Desc11(desc11)
{
	m_Desc11.StructureByteStride = (m_Desc11.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) ? m_Desc11.StructureByteStride : 0U;
	m_DX12Resource.MakeConcurrentWritable(m_Desc11.MiscFlags & D3D11_RESOURCE_MISC_UAV_OVERLAP ? true : false);
	m_DX12Resource.MakeReusableResource(m_Desc11.MiscFlags & D3D11_RESOURCE_MISC_HIFREQ_HEAP ? true : false);
	m_DX12View.Init(m_DX12Resource, NCryDX12::EVT_ConstantBufferView, m_Desc11.ByteWidth);
}

bool CCryDX12Buffer::SubstituteUsed()
{
	if (!Super::SubstituteUsed())
	{
		return false;
	}

	m_DX12View.Init(m_DX12Resource, NCryDX12::EVT_ConstantBufferView, m_Desc11.ByteWidth);
	return true;
}

#pragma region /* ID3D11Buffer implementation */

void STDMETHODCALLTYPE CCryDX12Buffer::GetDesc(
  _Out_ D3D11_BUFFER_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}

#pragma endregion
