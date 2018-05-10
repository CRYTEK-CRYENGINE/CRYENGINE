// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12DepthStencilView.hpp"

#include "DX12/Device/CCryDX12Device.hpp"

#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"

#include "DX12/Resource/Texture/CCryDX12Texture1D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"

CCryDX12DepthStencilView* CCryDX12DepthStencilView::Create(CCryDX12Device* pDevice, ID3D11Resource* pResource11, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc)
{
	ID3D12Resource* pResource12 = DX12_EXTRACT_D3D12RESOURCE(pResource11);

	// Special case: NullResource is valid when only the D3D12 resource is null
	if (!pResource11 && !pResource12)
	{
		DX12_ASSERT(0, "Unknown resource type!");
		return NULL;
	}

	TRange<uint16> slices(0, 1);
	uint16 mip = 0;

	D3D11_DEPTH_STENCIL_VIEW_DESC desc11 = pDesc ? *pDesc : D3D11_DEPTH_STENCIL_VIEW_DESC();
	D3D12_DEPTH_STENCIL_VIEW_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));

	if (pDesc)
	{
		desc12.Format = pDesc->Format;
		desc12.ViewDimension = static_cast<D3D12_DSV_DIMENSION>(pDesc->ViewDimension);

		desc12.Flags |= (desc11.Flags & D3D11_DSV_READ_ONLY_DEPTH ? D3D12_DSV_FLAG_READ_ONLY_DEPTH : D3D12_DSV_FLAG_NONE);
		desc12.Flags |= (desc11.Flags & D3D11_DSV_READ_ONLY_STENCIL ? D3D12_DSV_FLAG_READ_ONLY_STENCIL : D3D12_DSV_FLAG_NONE);

		switch (desc12.ViewDimension)
		{
		case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
			desc12.Texture1DArray.ArraySize = pDesc->Texture1DArray.ArraySize;
			desc12.Texture1DArray.FirstArraySlice = pDesc->Texture1DArray.FirstArraySlice;
			desc12.Texture1DArray.MipSlice = pDesc->Texture1DArray.MipSlice;
			mip = pDesc->Texture1DArray.MipSlice;
			slices.start = pDesc->Texture1DArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture1DArray.ArraySize;
			break;
		case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
			desc12.Texture2DArray.ArraySize = pDesc->Texture2DArray.ArraySize;
			desc12.Texture2DArray.FirstArraySlice = pDesc->Texture2DArray.FirstArraySlice;
			desc12.Texture2DArray.MipSlice = pDesc->Texture2DArray.MipSlice;
			mip = pDesc->Texture2DArray.MipSlice;
			slices.start = pDesc->Texture2DArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture2DArray.ArraySize;
			break;
		case D3D12_DSV_DIMENSION_TEXTURE2DMS:
			break;
		case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
			desc12.Texture2DMSArray.FirstArraySlice = pDesc->Texture2DMSArray.FirstArraySlice;
			desc12.Texture2DMSArray.ArraySize = pDesc->Texture2DMSArray.ArraySize;
			slices.start = pDesc->Texture2DMSArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture2DMSArray.ArraySize;
			break;
		case D3D12_DSV_DIMENSION_TEXTURE1D:
			desc12.Texture1D.MipSlice = pDesc->Texture1D.MipSlice;
			mip = pDesc->Texture1D.MipSlice;
			break;
		case D3D12_DSV_DIMENSION_TEXTURE2D:
			desc12.Texture2D.MipSlice = pDesc->Texture2D.MipSlice;
			mip = pDesc->Texture2D.MipSlice;
			break;
		}
	}
	else
	{
		EDX12ResourceType type = DX12_EXTRACT_RESOURCE_TYPE(pResource11);

		switch (type)
		{
		case eDX12RT_Texture1D:
			{
				D3D11_TEXTURE1D_DESC desc;
				(reinterpret_cast<CCryDX12Texture1D*>(pResource11))->GetDesc(&desc);

				desc11.Format = desc.Format;
				desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;

				desc12.Format = desc.Format;
				desc12.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			}
			break;

		case eDX12RT_Texture2D:
			{
				D3D11_TEXTURE2D_DESC desc;
				(reinterpret_cast<CCryDX12Texture2D*>(pResource11))->GetDesc(&desc);

				desc11.Format = desc.Format;
				desc11.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

				desc12.Format = desc.Format;
				desc12.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			}
			break;

		default:
			DX12_NOT_IMPLEMENTED;
			return NULL;
		}
	}

	CCryDX12DepthStencilView* result = NULL;

	if (pDesc)
	{
		result = DX12_NEW_RAW(CCryDX12DepthStencilView(pResource11, desc11, pResource12, desc12));
	}
	else
	{
		result = DX12_NEW_RAW(CCryDX12DepthStencilView(pResource11, desc11, pResource12));
	}

	auto& resource = DX12_EXTRACT_RESOURCE(pResource11)->GetDX12Resource();
	CD3DX12_RESOURCE_DESC descX(resource.GetDesc());

	const int planeCount = resource.GetPlaneCount();
	uint32 subresourceCount = descX.Subresources(planeCount);
	uint32 subresourcesInUse = D3D12CalcSubresourceCount(1, std::min(slices.Length(), descX.ArraySize()), planeCount);

	result->GetDX12View().SetMapsFullResource(subresourcesInUse == subresourceCount);
	result->GetDX12View().SetSlices(slices);
	result->GetDX12View().SetMips(TRange<uint16>(mip, mip + 1));
	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12DepthStencilView::CCryDX12DepthStencilView(ID3D11Resource* pResource11, const D3D11_DEPTH_STENCIL_VIEW_DESC& rDesc11, ID3D12Resource* pResource12, const D3D12_DEPTH_STENCIL_VIEW_DESC& rDesc12)
	: Super(pResource11, NCryDX12::EVT_DepthStencilView)
	, m_Desc11(rDesc11)
{
	m_DX12View.GetDSVDesc() = rDesc12;
}

CCryDX12DepthStencilView::CCryDX12DepthStencilView(ID3D11Resource* pResource11, const D3D11_DEPTH_STENCIL_VIEW_DESC& rDesc11, ID3D12Resource* pResource12)
	: Super(pResource11, NCryDX12::EVT_DepthStencilView)
	, m_Desc11(rDesc11)
{
	m_DX12View.HasDesc(false);
}

#pragma region /* ID3D11DepthStencilView implementation */

void STDMETHODCALLTYPE CCryDX12DepthStencilView::GetDesc(
  _Out_ D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}

#pragma endregion
