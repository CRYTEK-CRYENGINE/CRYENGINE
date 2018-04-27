// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12RenderTargetView.hpp"

#include "DX12/Resource/Misc/CCryDX12Buffer.hpp"

#include "DX12/Resource/Texture/CCryDX12Texture1D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture2D.hpp"
#include "DX12/Resource/Texture/CCryDX12Texture3D.hpp"

CCryDX12RenderTargetView* CCryDX12RenderTargetView::Create(CCryDX12Device* pDevice, ID3D11Resource* pResource11, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc)
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

	D3D11_RENDER_TARGET_VIEW_DESC desc11 = pDesc ? *pDesc : D3D11_RENDER_TARGET_VIEW_DESC();
	D3D12_RENDER_TARGET_VIEW_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D12_RENDER_TARGET_VIEW_DESC));

	if (pDesc)
	{
		desc12.Format = pDesc->Format;
		desc12.ViewDimension = static_cast<D3D12_RTV_DIMENSION>(pDesc->ViewDimension);
		switch (desc12.ViewDimension)
		{
		case D3D11_RTV_DIMENSION_BUFFER:
			desc12.Buffer.FirstElement = pDesc->Buffer.FirstElement;
			desc12.Buffer.NumElements = pDesc->Buffer.NumElements;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
			desc12.Texture1DArray.ArraySize = pDesc->Texture1DArray.ArraySize;
			desc12.Texture1DArray.FirstArraySlice = pDesc->Texture1DArray.FirstArraySlice;
			desc12.Texture1DArray.MipSlice = pDesc->Texture1DArray.MipSlice;
			mip = pDesc->Texture1DArray.MipSlice;
			slices.start = pDesc->Texture1DArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture1DArray.ArraySize;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
			desc12.Texture2DArray.ArraySize = pDesc->Texture2DArray.ArraySize;
			desc12.Texture2DArray.FirstArraySlice = pDesc->Texture2DArray.FirstArraySlice;
			desc12.Texture2DArray.MipSlice = pDesc->Texture2DArray.MipSlice;
			mip = pDesc->Texture2DArray.MipSlice;
			slices.start = pDesc->Texture2DArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture2DArray.ArraySize;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DMS:
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
			desc12.Texture2DMSArray.FirstArraySlice = pDesc->Texture2DMSArray.FirstArraySlice;
			desc12.Texture2DMSArray.ArraySize = pDesc->Texture2DMSArray.ArraySize;
			slices.start = pDesc->Texture2DMSArray.FirstArraySlice;
			slices.end = slices.start + pDesc->Texture2DMSArray.ArraySize;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE1D:
			desc12.Texture1D.MipSlice = pDesc->Texture1D.MipSlice;
			mip = pDesc->Texture1D.MipSlice;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE2D:
			desc12.Texture2D.MipSlice = pDesc->Texture2D.MipSlice;
			mip = pDesc->Texture2D.MipSlice;
			break;
		case D3D12_RTV_DIMENSION_TEXTURE3D:
			desc12.Texture3D.FirstWSlice = pDesc->Texture3D.FirstWSlice;
			desc12.Texture3D.MipSlice = pDesc->Texture3D.MipSlice;
			desc12.Texture3D.WSize = pDesc->Texture3D.WSize;
			mip = pDesc->Texture3D.MipSlice;
			break;
		}
	}
	else
	{
		EDX12ResourceType type = DX12_EXTRACT_RESOURCE_TYPE(pResource11);

		switch (type)
		{
		case eDX12RT_Buffer:
			{
				D3D11_BUFFER_DESC desc;
				(reinterpret_cast<CCryDX12Buffer*>(pResource11))->GetDesc(&desc);

				desc11.Format = DXGI_FORMAT_UNKNOWN;
				desc11.ViewDimension = D3D11_RTV_DIMENSION_BUFFER;

				desc12.Format = DXGI_FORMAT_UNKNOWN;
				desc12.ViewDimension = D3D12_RTV_DIMENSION_BUFFER;
			}
			break;

		case eDX12RT_Texture1D:
			{
				D3D11_TEXTURE1D_DESC desc;
				(reinterpret_cast<CCryDX12Texture1D*>(pResource11))->GetDesc(&desc);

				desc11.Format = desc.Format;
				desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;

				desc12.Format = desc.Format;
				desc12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;

				slices.end = desc.ArraySize;
			}
			break;

		case eDX12RT_Texture2D:
			{
				D3D11_TEXTURE2D_DESC desc;
				(reinterpret_cast<CCryDX12Texture2D*>(pResource11))->GetDesc(&desc);

				desc11.Format = desc.Format;
				desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

				desc12.Format = desc.Format;
				desc12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

				slices.end = desc.ArraySize;
			}
			break;

		case eDX12RT_Texture3D:
			{
				D3D11_TEXTURE3D_DESC desc;
				(reinterpret_cast<CCryDX12Texture3D*>(pResource11))->GetDesc(&desc);

				desc11.Format = desc.Format;
				desc11.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;

				desc12.Format = desc.Format;
				desc12.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			}
			break;

		default:
			DX12_NOT_IMPLEMENTED;
			return NULL;
		}
	}

	CCryDX12RenderTargetView* result;

	if (pDesc)
	{
		result = DX12_NEW_RAW(CCryDX12RenderTargetView(pResource11, desc11, pResource12, desc12));
	}
	else
	{
		result = DX12_NEW_RAW(CCryDX12RenderTargetView(pResource11, desc11, pResource12));
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

CCryDX12RenderTargetView::CCryDX12RenderTargetView(ID3D11Resource* pResource11, const D3D11_RENDER_TARGET_VIEW_DESC& rDesc11, ID3D12Resource* pResource12, const D3D12_RENDER_TARGET_VIEW_DESC& rDesc12)
	: Super(pResource11, NCryDX12::EVT_RenderTargetView)
	, m_Desc11(rDesc11)
{
	m_DX12View.GetRTVDesc() = rDesc12;
}

CCryDX12RenderTargetView::CCryDX12RenderTargetView(ID3D11Resource* pResource11, const D3D11_RENDER_TARGET_VIEW_DESC& rDesc11, ID3D12Resource* pResource12)
	: Super(pResource11, NCryDX12::EVT_RenderTargetView)
	, m_Desc11(rDesc11)
{
	m_DX12View.HasDesc(false);
}

/* ID3D11RenderTargetView implementation */
