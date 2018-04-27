// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12UnorderedAccessView.hpp"

CCryDX12UnorderedAccessView* CCryDX12UnorderedAccessView::Create(CCryDX12Device* pDevice, ID3D11Resource* pResource11, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc11)
{
	ID3D12Resource* pResource12 = DX12_EXTRACT_D3D12RESOURCE(pResource11);

	// Special case: NullResource is valid when only the D3D12 resource is null
	if (!pResource11 && !pResource12)
	{
		DX12_ASSERT(0, "Unknown resource type!");
		return NULL;
	}

	DX12_ASSERT((pDesc11->ViewDimension != D3D11_UAV_DIMENSION_BUFFER) || !(pDesc11->Buffer.Flags & D3D11_BUFFER_UAV_FLAG_APPEND), "No append/consume supported under DX12!");
	DX12_ASSERT((pDesc11->ViewDimension != D3D11_UAV_DIMENSION_BUFFER) || !(pDesc11->Buffer.Flags & D3D11_BUFFER_UAV_FLAG_COUNTER), "Counters have significantly changed under DX12! Please rewrite the algorithm for DX12 on a higher level.");

	TRange<uint16> slices(0, 1);
	uint16 mip = 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC desc12;
	ZeroMemory(&desc12, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));

	desc12.Format = pDesc11->Format;
	desc12.ViewDimension = static_cast<D3D12_UAV_DIMENSION>(pDesc11->ViewDimension);
	switch (desc12.ViewDimension)
	{
	case D3D12_UAV_DIMENSION_BUFFER:
		desc12.Buffer.FirstElement = static_cast<UINT64>(pDesc11->Buffer.FirstElement);
		desc12.Buffer.NumElements = pDesc11->Buffer.NumElements;
		desc12.Buffer.Flags = (pDesc11->Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE);
		desc12.Buffer.CounterOffsetInBytes = 0;      // NOTE: not yet implemented/supported
		desc12.Buffer.StructureByteStride = DX12_EXTRACT_BUFFER(pResource11)->GetStructureByteStride();
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
		desc12.Texture1DArray.MipSlice = pDesc11->Texture1DArray.MipSlice;
		desc12.Texture1DArray.FirstArraySlice = pDesc11->Texture1DArray.FirstArraySlice;
		desc12.Texture1DArray.ArraySize = pDesc11->Texture1DArray.ArraySize;
		mip = pDesc11->Texture1DArray.MipSlice;
		slices.start = pDesc11->Texture1DArray.FirstArraySlice;
		slices.end = slices.start + pDesc11->Texture1DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		desc12.Texture2DArray.MipSlice = pDesc11->Texture2DArray.MipSlice;
		desc12.Texture2DArray.FirstArraySlice = pDesc11->Texture2DArray.FirstArraySlice;
		desc12.Texture2DArray.ArraySize = pDesc11->Texture2DArray.ArraySize;
		desc12.Texture2DArray.PlaneSlice = 0;        // NOTE: not yet implemented/supported
		mip = pDesc11->Texture2DArray.MipSlice;
		slices.start = pDesc11->Texture2DArray.FirstArraySlice;
		slices.end = slices.start + pDesc11->Texture2DArray.ArraySize;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE1D:
		desc12.Texture1D.MipSlice = pDesc11->Texture1D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2D:
		desc12.Texture2D.MipSlice = pDesc11->Texture2D.MipSlice;
		desc12.Texture2D.PlaneSlice = 0;             // NOTE: not yet implemented/supported
		mip = pDesc11->Texture2D.MipSlice;
		break;
	case D3D12_UAV_DIMENSION_TEXTURE3D:
		desc12.Texture3D.MipSlice = pDesc11->Texture3D.MipSlice;
		desc12.Texture3D.FirstWSlice = pDesc11->Texture3D.FirstWSlice;
		desc12.Texture3D.WSize = pDesc11->Texture3D.WSize;
		mip = pDesc11->Texture3D.MipSlice;
		break;
	}

	auto& resource = DX12_EXTRACT_RESOURCE(pResource11)->GetDX12Resource();
	CD3DX12_RESOURCE_DESC descX(resource.GetDesc());

	const int planeCount = resource.GetPlaneCount();
	uint32 subresourceCount = descX.Subresources(planeCount);
	uint32 subresourcesInUse = D3D12CalcSubresourceCount(1, std::min(slices.Length(), descX.ArraySize()), planeCount);

	auto pUav = DX12_NEW_RAW(CCryDX12UnorderedAccessView(pResource11, *pDesc11, pResource12, desc12));
	pUav->GetDX12View().SetMapsFullResource(subresourcesInUse == subresourceCount);
	pUav->GetDX12View().SetSlices(slices);
	pUav->GetDX12View().SetMips(TRange<uint16>(mip, mip + 1));
	return pUav;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12UnorderedAccessView::CCryDX12UnorderedAccessView(ID3D11Resource* pResource11, const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc11, ID3D12Resource* pResource12, const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc12)
	: Super(pResource11, NCryDX12::EVT_UnorderedAccessView)
	, m_Desc11(desc11)
{
	m_DX12View.GetUAVDesc() = desc12;
}

#pragma region /* ID3D11UnorderedAccessView implementation */

void STDMETHODCALLTYPE CCryDX12UnorderedAccessView::GetDesc(
  _Out_ D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc)
{
	if (pDesc)
	{
		*pDesc = m_Desc11;
	}
}

#pragma endregion
